#!/usr/bin/env node
// net-check.js — the one-liner LOCKSTEP NETPLAY gate (the netplay twin of
// tune-check): run after touching runtime/net.h, the studio.c net/loop seams,
// or tools/net-relay.js. Three deterministic legs, PASS/FAIL verdict:
//
//   1. echo     — pong under --net-echo (the loopback fake peer): scripts P1,
//                 asserts P2 mirrors it EXACTLY (p2y == p1y on every traced
//                 frame). Proves the remote-input injection path — pack → send
//                 → ring → barrier → btn(1) — with no sockets.
//   2. netdemo  — play.js pong netdemo: a REAL host + joiner pair over UDP
//                 loopback, different per-side scripts (the committed netcheck
//                 clips), per-frame trace diff → LOCKSTEP OK / DESYNC.
//   3. relay    — tools/net-relay.js as a lib: spins a relay on an ephemeral
//                 port and runs TWO SIMULATED CARTS through it, speaking the
//                 exact web-cart handshake (ROLE → HELLO → WELCOME{seed} →
//                 INPUT frames → BYE). Verifies the wire protocol the wasm
//                 build speaks, headless — the browser-free half of the
//                 two-tab test.
//
//   node tools/net-check.js            all three legs, chatty
//   node tools/net-check.js --quiet    verdict only; exit 0 pass / 1 fail (CI)
//   node tools/net-check.js --frames N echo/netdemo length (default 300)
//
// Reference cart: pong (the multiplayer reference — watches p1y/p2y), driven by
// the committed clips at tools/clips/pong/. Design: docs/design/
// multiplayer-research.md; the blessed-check row lives in
// docs/guides/checks-and-oracles.md.
'use strict'

const { spawnSync } = require('child_process')
const fs   = require('fs')
const path = require('path')

const ROOT   = path.join(__dirname, '..')
const argv   = process.argv.slice(2)
const quiet  = argv.includes('--quiet')
const opt    = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] ? argv[i + 1] : d }
const FRAMES = parseInt(opt('--frames', '300'), 10)

const say  = (s) => { if (!quiet) console.log(s) }
let fails = 0
const verdict = (ok, what) => { say(`  ${ok ? '✓' : '✗ FAIL:'} ${what}`); if (!ok) fails++ }

// ── leg 1: echo — P2 mirrors P1 through the real lockstep path ────────────────
function checkEcho() {
  say('echo: pong vs the loopback fake peer')
  const trace = path.join(ROOT, 'build', 'netcheck-echo.jsonl')
  const r = spawnSync('node', ['tools/play.js', 'pong', 'script', 'tools/clips/pong/01-netcheck-host.script',
                               '--net-echo', '--headless', '--frames', String(FRAMES), '--trace', trace],
                      { cwd: ROOT, encoding: 'utf8', timeout: 120000 })
  if (r.status !== 0) { verdict(false, `echo run exited ${r.status}\n${(r.stderr || '').trim()}`); return }
  const lines = fs.readFileSync(trace, 'utf8').trim().split('\n').map(l => JSON.parse(l))
  const rows = lines.filter(l => l.w && l.w.p1y !== undefined)  // frames carrying the p1y/p2y watch (some emit no w block)
  const bad = rows.filter(l => l.w.p1y !== l.w.p2y)
  const moved = rows.some(l => l.w.p1y !== rows[0].w.p1y)
  verdict(rows.length > 0 && moved, `P1 moved under the script (${rows.length} traced frames)`)
  verdict(rows.length > 0 && bad.length === 0, bad.length === 0
    ? `P2 mirrors P1 on all ${rows.length} traced frames (p2y == p1y)`
    : `P2 diverged on ${bad.length} frames — first at f=${bad[0].f}: p1y=${bad[0].w.p1y} p2y=${bad[0].w.p2y}`)
}

// ── leg 2: netdemo — a real host+joiner pair over UDP loopback ────────────────
function checkNetdemo() {
  say('netdemo: host + joiner over 127.0.0.1, per-side scripts')
  const r = spawnSync('node', ['tools/play.js', 'pong', 'netdemo',
                               '--host-script', 'tools/clips/pong/01-netcheck-host.script',
                               '--join-script', 'tools/clips/pong/02-netcheck-join.script',
                               '--frames', String(FRAMES), '--headless'],
                      { cwd: ROOT, encoding: 'utf8', timeout: 180000 })
  const out = (r.stdout || '') + (r.stderr || '')
  const ok = out.includes('LOCKSTEP OK')
  verdict(ok, ok ? out.match(/LOCKSTEP OK[^\n]*/)[0]
                 : `no LOCKSTEP OK — tail:\n${out.split('\n').slice(-6).join('\n')}`)
}

// ── leg 3: relay — two simulated carts speak the web protocol through it ─────
async function checkRelay() {
  say('relay: two simulated carts through a real net-relay (ROLE→HELLO→WELCOME→INPUT→BYE)')
  const { makeServer, wsClient } = require('./net-relay.js')
  const sleep = (ms) => new Promise(res => setTimeout(res, ms))
  const until = async (fn, ms = 3000) => { const t0 = Date.now(); while (!fn()) { if (Date.now() - t0 > ms) return false; await sleep(10) } return true }
  const DN = (...b) => Buffer.from([0x44, 0x4e, ...b])
  const SEED = 0xdeadbeef

  const server = makeServer({ serveDir: null, log: () => {} })
  await new Promise(res => server.listen(0, '127.0.0.1', res))
  const port = server.address().port

  // the host cart: seat 0, answers HELLO with WELCOME{seed}, then streams inputs
  const host = wsClient(port, '/room/NETCHECK')
  await until(() => host.packets.length >= 1)
  verdict(host.packets[0]?.[2] === 6 && host.packets[0]?.[3] === 0, 'host seated (ROLE 0)')

  const join = wsClient(port, '/room/NETCHECK')
  await until(() => join.packets.length >= 1)
  verdict(join.packets[0]?.[2] === 6 && join.packets[0]?.[3] === 1, 'joiner seated (ROLE 1)')

  join.send(DN(1))                                             // HELLO
  await until(() => host.packets.some(p => p[2] === 1))
  verdict(host.packets.some(p => p[2] === 1), 'host received HELLO through the relay')
  host.send(DN(2, SEED & 0xff, (SEED >> 8) & 0xff, (SEED >> 16) & 0xff, (SEED >>> 24) & 0xff))  // WELCOME
  await until(() => join.packets.some(p => p[2] === 2))
  const w = join.packets.find(p => p[2] === 2)
  const seed = w ? (w[3] | w[4] << 8 | w[5] << 16 | w[6] << 24) >>> 0 : 0
  verdict(seed === SEED, `joiner adopted the host seed (0x${seed.toString(16)})`)

  // lockstep inputs both ways: INPUT f0=3, count=2, bytes per side
  host.send(DN(3, 3, 0, 0, 0, 2, 0x11, 0x22))
  join.send(DN(3, 3, 0, 0, 0, 2, 0x33, 0x44))
  await until(() => join.packets.some(p => p[2] === 3) && host.packets.some(p => p[2] === 3))
  const hIn = host.packets.find(p => p[2] === 3), jIn = join.packets.find(p => p[2] === 3)
  verdict(!!jIn && jIn[8] === 0x11 && jIn[9] === 0x22, 'host inputs reached the joiner verbatim')
  verdict(!!hIn && hIn[8] === 0x33 && hIn[9] === 0x44, 'joiner inputs reached the host verbatim')

  // one side leaves → the other hears BYE (relay-synthesized or forwarded)
  host.close()
  await until(() => join.packets.some(p => p[2] === 4))
  verdict(join.packets.some(p => p[2] === 4), 'peer got BYE when the other tab vanished')

  join.close()
  server.close()
}

// ── main ──────────────────────────────────────────────────────────────────────
;(async () => {
  checkEcho()
  checkNetdemo()
  await checkRelay()
  console.log(fails ? `net-check: ${fails} FAILED` : `net-check: PASS (echo + netdemo + relay)`)
  process.exit(fails ? 1 : 0)
})()
