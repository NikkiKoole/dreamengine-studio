#!/usr/bin/env node
// net-relay.js — the lockstep-netplay RELAY for web/wasm carts (rung 5a step 3,
// docs/design/multiplayer-research.md §"Scoped plan — rung 5a"). Browsers have
// no UDP, so two wasm carts meet HERE instead: each opens a WebSocket to
// ws://<this box>/room/<code> and the relay BLINDLY forwards every binary
// message to the other member of the room. It never parses game packets — the
// same 1-byte-per-frame lockstep DN packets net.h speaks over UDP ride through
// verbatim, so carts stay 100% network-unaware and one relay serves every cart
// forever.
//
//   node tools/net-relay.js                      relay only, port 33447
//   node tools/net-relay.js --port <n>           pick the port
//   node tools/net-relay.js --serve <dir>        ALSO serve <dir> as static files
//                                                (the one-wifi-box setup: cart +
//                                                relay from one process — build a
//                                                cart with build-site.js, serve
//                                                its folder, open http://<lan-ip>
//                                                :<port>/?room=X on two devices)
//   node tools/net-relay.js --check              headless self-test (spins a
//                                                relay + two in-process WS
//                                                clients, asserts rooms/roles/
//                                                forwarding/BYE), exits nonzero
//                                                on failure — the CI gate
//
// Beyond the LAN: the PUBLIC site repo (NikkiKoole/dreamengine) self-deploys
// this relay — publish-cart.sh syncs a copy to site/net-relay.js, and
// site/render.yaml is the Render blueprint (free tier, wss:// handed out; the
// service also --serves the gallery, so games run straight off the Render
// domain with no ?relay= param). The private code repo is never shared with
// Render. Walkthrough: multiplayer-research.md §"Hosting beyond the LAN".
//
// Protocol (all binary WS frames):
//   - WS path /room/<code> (or ?room=<code>) joins that room. Rooms hold 2.
//     Web carts prepend their cart name to the code ("pong-play") so ONE shared
//     relay never cross-pairs two different games — the relay itself stays
//     blind; codes are opaque here.
//   - On join the relay sends the ONE packet it originates:
//     ROLE = [ 'D','N', 6, seat ] — seat 0 = host (first in), 1 = joiner.
//     (packet type 6 = NET_PKT_ROLE in runtime/net.h; 'DN' is the magic.)
//   - A third client gets close code 4001 ("room full").
//   - Every binary frame from one member goes to the other, unparsed.
//   - When a member disconnects the relay synthesizes BYE = ['D','N',4] to the
//     peer and closes it too — a room is ONE match, no re-pairing ambiguity.
//
// Zero dependencies: the WebSocket server (and the self-test's client) are
// hand-rolled RFC 6455 — the frames here are tiny and the subset is small
// (binary + close + ping/pong, no extensions, no fragmentation of our sends).
'use strict'

const http   = require('http')
const crypto = require('crypto')
const fs     = require('fs')
const path   = require('path')
const net    = require('net')

const WS_GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'
const PKT_ROLE = Buffer.from(['D'.charCodeAt(0), 'N'.charCodeAt(0), 6, 0])
const PKT_BYE  = Buffer.from(['D'.charCodeAt(0), 'N'.charCodeAt(0), 4])

// ── RFC 6455 framing (server side: receive masked, send unmasked) ────────────

// encode one server→client frame. opcode 2 = binary, 8 = close, 10 = pong.
function wsEncode(payload, opcode = 2) {
  const n = payload.length
  let head
  if (n < 126)        { head = Buffer.from([0x80 | opcode, n]) }
  else if (n < 65536) { head = Buffer.alloc(4); head[0] = 0x80 | opcode; head[1] = 126; head.writeUInt16BE(n, 2) }
  else                { head = Buffer.alloc(10); head[0] = 0x80 | opcode; head[1] = 127; head.writeBigUInt64BE(BigInt(n), 2) }
  return Buffer.concat([head, payload])
}

// incremental decoder: feed bytes, get complete frames out. Handles masked and
// unmasked payloads (server receives masked, the self-test client unmasked).
function wsDecoder(onFrame) {
  let buf = Buffer.alloc(0)
  return (chunk) => {
    buf = Buffer.concat([buf, chunk])
    for (;;) {
      if (buf.length < 2) return
      const opcode = buf[0] & 0x0f
      const masked = (buf[1] & 0x80) !== 0
      let len = buf[1] & 0x7f, off = 2
      if (len === 126) { if (buf.length < 4) return; len = buf.readUInt16BE(2); off = 4 }
      else if (len === 127) { if (buf.length < 10) return; len = Number(buf.readBigUInt64BE(2)); off = 10 }
      const maskOff = off
      if (masked) off += 4
      if (buf.length < off + len) return
      let payload = buf.subarray(off, off + len)
      if (masked) {
        const mask = buf.subarray(maskOff, maskOff + 4)
        const un = Buffer.alloc(len)
        for (let i = 0; i < len; i++) un[i] = payload[i] ^ mask[i & 3]
        payload = un
      }
      buf = buf.subarray(off + len)
      onFrame(opcode, payload)
    }
  }
}

// ── rooms ─────────────────────────────────────────────────────────────────────

const rooms = new Map()   // code → [socketA, socketB?]

function roomJoin(code, sock, log) {
  let members = rooms.get(code)
  if (!members) { members = []; rooms.set(code, members) }
  if (members.length >= 2) {
    sock.write(wsEncode(Buffer.alloc(0), 8))   // close: room full (code in app-land)
    sock.end()
    log(`room ${code}: full, third client refused`)
    return
  }
  const seat = members.length
  members.push(sock)
  sock.deRoom = code
  sock.deSeat = seat
  const role = Buffer.from(PKT_ROLE); role[3] = seat
  sock.write(wsEncode(role))
  log(`room ${code}: seat ${seat} joined (${seat === 0 ? 'host — waiting for a joiner' : 'joiner — match on'})`)
}

function roomPeer(sock) {
  const members = rooms.get(sock.deRoom)
  if (!members) return null
  return members.find(s => s !== sock) || null
}

function roomLeave(sock, log) {
  const code = sock.deRoom
  if (code === undefined) return
  const peer = roomPeer(sock)
  rooms.delete(code)                            // a room is ONE match
  sock.deRoom = undefined
  if (peer) {
    peer.write(wsEncode(PKT_BYE))               // tell the survivor immediately
    peer.write(wsEncode(Buffer.alloc(0), 8))
    peer.end()
    peer.deRoom = undefined
    log(`room ${code}: seat ${sock.deSeat} left — BYE sent to peer, room closed`)
  } else {
    log(`room ${code}: seat ${sock.deSeat} left — room closed`)
  }
}

// ── the server: HTTP (optional static) + WS upgrade ──────────────────────────

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm',
               '.png': 'image/png', '.css': 'text/css', '.json': 'application/json',
               '.data': 'application/octet-stream' }

function makeServer({ serveDir, log }) {
  const server = http.createServer((req, res) => {
    // no --serve: answer 200 with a liveness note — a browser hit on the bare
    // URL wakes a slept free-tier host (the "warm it up" trick in render.yaml)
    // and doubles as the health check
    if (!serveDir) { res.writeHead(200, { 'Content-Type': 'text/plain' }); res.end('dreamengine net-relay: up — carts connect via ws(s)://…/room/<code>\n'); return }
    const urlPath = decodeURIComponent((req.url || '/').split('?')[0])
    let file = path.join(serveDir, urlPath === '/' ? 'index.html' : urlPath)
    if (!path.resolve(file).startsWith(path.resolve(serveDir))) { res.writeHead(403); res.end(); return }
    fs.readFile(file, (err, data) => {
      if (err) { res.writeHead(404); res.end('not found\n'); return }
      res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' })
      res.end(data)
    })
  })

  server.on('upgrade', (req, sock) => {
    const key = req.headers['sec-websocket-key']
    if (!key) { sock.end('HTTP/1.1 400 Bad Request\r\n\r\n'); return }
    const accept = crypto.createHash('sha1').update(key + WS_GUID).digest('base64')
    sock.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n' +
               'Connection: Upgrade\r\nSec-WebSocket-Accept: ' + accept + '\r\n\r\n')
    sock.setNoDelay(true)                       // lockstep = tiny per-frame packets; Nagle hurts

    // room code: /room/<code> path, or ?room=<code> on any path
    const u = new URL(req.url || '/', 'http://x')
    const m = u.pathname.match(/^\/room\/([A-Za-z0-9_%.-]{1,64})$/)   // 64: web carts prepend "<cart>-" to the code
    const code = m ? m[1] : (u.searchParams.get('room') || '')
    if (!code) { sock.end(); return }

    sock.on('data', wsDecoder((opcode, payload) => {
      if (opcode === 8) { roomLeave(sock, log); sock.end(); return }          // close
      if (opcode === 9) { sock.write(wsEncode(payload, 10)); return }         // ping → pong
      if (opcode !== 2 && opcode !== 1) return
      const peer = roomPeer(sock)
      if (peer) peer.write(wsEncode(payload))   // the whole relay: blind forwarding
    }))
    // http-upgrade sockets are allowHalfOpen: a client FIN fires 'end', not
    // 'close' — without this the relay never notices a vanished tab
    sock.on('end',   () => { roomLeave(sock, log); sock.end() })
    sock.on('close', () => roomLeave(sock, log))
    sock.on('error', () => roomLeave(sock, log))

    roomJoin(code, sock, log)
  })

  return server
}

// ── a minimal WS client (self-test only: send masked, receive unmasked) ──────

function wsClient(port, roomPath, onPacket) {
  const sock = net.connect(port, '127.0.0.1')
  const key = crypto.randomBytes(16).toString('base64')
  const c = { sock, open: false, packets: [], closed: false }
  sock.setNoDelay(true)
  sock.on('connect', () => {
    sock.write(`GET ${roomPath} HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n` +
               `Connection: Upgrade\r\nSec-WebSocket-Key: ${key}\r\nSec-WebSocket-Version: 13\r\n\r\n`)
  })
  let handshook = false
  const decode = wsDecoder((opcode, payload) => {
    if (opcode === 8) { c.closed = true; return }
    c.packets.push(Buffer.from(payload))
    if (onPacket) onPacket(Buffer.from(payload))
  })
  sock.on('data', (chunk) => {
    if (!handshook) {
      const i = chunk.indexOf('\r\n\r\n')
      if (i < 0) return
      handshook = true; c.open = true
      chunk = chunk.subarray(i + 4)
      if (!chunk.length) return
    }
    decode(chunk)
  })
  sock.on('close', () => { c.closed = true })
  c.send = (payload) => {                       // client→server frames MUST be masked
    const mask = crypto.randomBytes(4)
    const n = payload.length
    let head
    if (n < 126) head = Buffer.from([0x82, 0x80 | n])
    else { head = Buffer.alloc(4); head[0] = 0x82; head[1] = 0x80 | 126; head.writeUInt16BE(n, 2) }
    const masked = Buffer.alloc(n)
    for (let i = 0; i < n; i++) masked[i] = payload[i] ^ mask[i & 3]
    sock.write(Buffer.concat([head, mask, masked]))
  }
  c.close = () => sock.end()
  return c
}

// ── self-test (--check) ───────────────────────────────────────────────────────

async function selfTest() {
  const sleep = (ms) => new Promise(r => setTimeout(r, ms))
  const until = async (fn, ms = 2000) => { const t0 = Date.now(); while (!fn()) { if (Date.now() - t0 > ms) return false; await sleep(10) } return true }
  let fails = 0
  const assert = (ok, what) => { console.log(`  ${ok ? '✓' : '✗ FAIL:'} ${what}`); if (!ok) fails++ }

  const verbose = argv.includes('--verbose')
  const server = makeServer({ serveDir: null, log: verbose ? (s) => console.log(`   [srv] ${s}`) : () => {} })
  await new Promise(r => server.listen(0, '127.0.0.1', r))
  const port = server.address().port
  console.log(`net-relay --check (ephemeral port ${port})`)

  // 1. roles: first in = seat 0, second = seat 1
  const a = wsClient(port, '/room/CHECK')
  await until(() => a.packets.length >= 1)
  assert(a.packets[0] && a.packets[0][2] === 6 && a.packets[0][3] === 0, 'first client gets ROLE seat 0 (host)')
  const b = wsClient(port, '/room/CHECK')
  await until(() => b.packets.length >= 1)
  assert(b.packets[0] && b.packets[0][2] === 6 && b.packets[0][3] === 1, 'second client gets ROLE seat 1 (joiner)')

  // 2. blind forwarding, both directions, bytes verbatim (a fake INPUT packet)
  const input = Buffer.from([0x44, 0x4e, 3, 7, 0, 0, 0, 2, 0xaa, 0xbb])
  b.send(input)
  await until(() => a.packets.length >= 2)
  assert(a.packets[1] && a.packets[1].equals(input), 'joiner→host packet forwarded verbatim')
  a.send(input)
  await until(() => b.packets.length >= 2)
  assert(b.packets[1] && b.packets[1].equals(input), 'host→joiner packet forwarded verbatim')

  // 3. a third client is refused
  const c3 = wsClient(port, '/room/CHECK')
  const refused = await until(() => c3.closed, 2000)
  assert(refused, 'third client into a full room is closed')

  // 4. disconnect → peer gets BYE and the room dies
  a.close()
  await until(() => b.packets.some(p => p.length >= 3 && p[2] === 4))
  assert(b.packets.some(p => p[2] === 4), 'peer receives synthesized BYE on disconnect')
  await until(() => b.closed)
  assert(b.closed, 'peer is closed with the room')

  // 5. the code is reusable after the room dies (fresh match)
  const d = wsClient(port, '/room/CHECK')
  await until(() => d.packets.length >= 1)
  assert(d.packets[0] && d.packets[0][3] === 0, 'room code reusable after close (fresh host seat)')
  d.close()

  server.close()
  console.log(fails ? `net-relay --check: ${fails} FAILED` : 'net-relay --check: PASS')
  process.exit(fails ? 1 : 0)
}

// ── main ──────────────────────────────────────────────────────────────────────

module.exports = { makeServer, wsClient }   // tools/net-check.js simulates two carts through a real relay

const argv = process.argv.slice(2)
const opt = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] ? argv[i + 1] : d }

if (require.main !== module) {
  // required as a lib — don't run the CLI (no server, no self-test)
} else if (argv.includes('--check')) {
  selfTest()
} else {
  const port = parseInt(opt('--port', '33447'), 10)
  const serveDir = opt('--serve', null)
  const log = (s) => console.log(`relay: ${s}`)
  const server = makeServer({ serveDir: serveDir ? path.resolve(serveDir) : null, log })
  server.listen(port, () => {
    const ips = []
    for (const ifs of Object.values(require('os').networkInterfaces()))
      for (const i of ifs || []) if (i.family === 'IPv4' && !i.internal) ips.push(i.address)
    const ip = ips.find(x => x.startsWith('192.168.') || x.startsWith('10.')) || ips[0] || '127.0.0.1'
    console.log(`relay: listening on port ${port}${serveDir ? ` — serving ${serveDir}` : ''}`)
    if (serveDir) console.log(`relay: open  http://${ip}:${port}/?room=play  on TWO devices (same room = same match)`)
    else console.log(`relay: carts connect to  ws://${ip}:${port}/room/<code>`)
  })
}
