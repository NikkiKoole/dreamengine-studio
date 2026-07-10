# Multiplayer — research & proposed direction

> **Status: building — rungs 1 & 2 SHIPPED (2026-07-02).** Started as a research
> pass (session, 2026-06-04) on "could dreamengine offer a multiplayer API, for
> both the native C build and the wasm build?" **Rungs 1, 2 and 2.5 are now
> built**: `runtime/net.h` + `--net-host`/`--net-join <ip>` on any native build
> (rung 1), a **🌐 multiplayer button** in the editor + `getifaddrs()` host-IP
> display (rung 2, LAN by IP), and an **engine-owned Host/Join/Solo boot lobby**
> (`--net-lobby`, rung 2.5) so a *standalone build with no editor* can start
> netplay with no flags — the send-a-friend-an-exe case. Demo + desync gate via
> `play.js <cart> netdemo`; see the rung ladder below and the ledger entry in
> [`STATUS.md`](../STATUS.md). **Debugging a hang/desync in a live game:** the
> `F2` net-health overlay (peer-buffer runway + stalls + tx/rx), a console stall
> log, and a per-frame `net` field in `--trace` — see
> [`../guides/debug-harness.md`](../guides/debug-harness.md) §"Netplay diagnostics". **Rung 5a (WebSocket relay) LIVE-VERIFIED
> 2026-07-06** (two browsers played on the home wifi); **rung 5b (WebRTC P2P)
> BUILT + play-tested 2026-07-07** — the WebRTC DataChannel is now the WEB game
> transport (the relay reused unchanged as signaling only), play-tested
> Mac↔iPhone over wifi at LAN speed; steps 1–4 shipped, `NET_DELAY` bumped to a
> fixed 10-frame cushion for wifi jitter (adaptive sizing = step 5, still open;
> TURN = step 7). See §"rung 5b". Rung 4 remains a proposal. See also
> [`cart-as-script.md`](cart-as-script.md) (the `STATE`/`de_state()` block this
> doc leans on), [`headless-autoplay.md`](headless-autoplay.md), and
> [`../guides/debug-harness.md`](../guides/debug-harness.md) (the determinism
> harness that makes the whole approach plausible).
>
> Sources were web-researched and adversarially fact-checked (21 sources, 25
> claims verified 3-vote); citations at the bottom. iOS/Safari specifics are the
> least-verified section — treat those as "reported", not "confirmed".

---

## The question

The dream UX, in the user's words: *click a button on my machine and get an
address/code another player can type in* — or Minecraft-style "open to LAN".
Eventually it should also work on iPads. And the cart-facing API must stay
**very friendly** — beginners write these games.

Three sub-problems, which this doc keeps separate because they have different
answers:

| Sub-problem | What it's about |
|---|---|
| **Transport** | how bytes physically travel between two machines (UDP? WebSocket? WebRTC?) |
| **Matchmaking / UX** | how two players *find* each other (join code, LAN discovery) |
| **Sync model** | what the bytes *contain* (inputs? state?) and how both games stay identical |

The headline findings:

1. **WebRTC DataChannels are the only transport that works for both native C
   and the browser.** Browsers have no raw UDP at all, and emscripten's
   TCP-over-WebSocket emulation is explicitly incomplete ([emscripten docs]).
   One library — **libdatachannel** (C++ with C bindings) + its sibling
   **datachannel-wasm** (same API, compiled for the web) — covers both targets
   with one codebase, including STUN/TURN/ICE NAT traversal. Native↔browser
   cross-play falls out for free, since libdatachannel speaks the same protocol
   browsers do.
2. **Input-lockstep is the right sync model**, and dreamengine is unusually
   well-positioned for it: the deterministic replay harness (`--det`, input
   recording, frame scripts) *is already* a single-machine lockstep engine.
   Netplay = the same input pipeline, where some players' inputs arrive over a
   socket instead of a `.rec` file. Cart code stays 100% network-unaware:
   `btn(1, BTN_A)` simply *is* the remote player.
3. **The join code needs a tiny signaling service; LAN discovery is native-only.**
   A signaling server is ~100 lines of relay logic, and free hosted options
   exist for both signaling (Cloudflare Worker via the p2pcf pattern) and the
   TURN fallback (Cloudflare TURN, Metered Open Relay).

## 1 · Transport options (native C **and** wasm)

| Transport | Native (clang/raylib) | Browser (emcc) | Verdict |
|---|---|---|---|
| **Raw UDP** | ✅ trivial (BSD sockets) | ❌ does not exist in browsers | LAN-only stage |
| **TCP** | ✅ | ⚠️ emscripten emulates over WebSocket, needs a proxy, "not complete" ([emscripten docs]) | avoid |
| **WebSocket** | ✅ (any small C lib, or libdatachannel includes one) | ✅ native browser API | good for *relay-server* topology |
| **WebRTC DataChannel** | ✅ **libdatachannel** — standalone C/C++, C bindings, ICE+STUN+TURN built in | ✅ **datachannel-wasm** — same API wrapping the browser's RTCDataChannel; Firefox/Chromium/Safari | **the unifying answer** |
| **ENet** | ✅ classic UDP game lib | ❌ no real wasm story (enet.js experiments are unmaintained) | skip |
| **WebTransport / QUIC** | immature C story | Chrome/Firefox yes, Safari late | revisit in a few years |

Key facts, verified:

- Emscripten has **no built-in C/C++ WebRTC API** — you bring a wrapper, which
  is exactly what datachannel-wasm is ([emscripten docs], [datachannel-wasm]).
- **libdatachannel** implements ICE with STUN and TURN support, has official C
  bindings, and the **same application code compiles to both targets** — the
  wasm wrapper exposes a (subset of the) same API ([libdatachannel],
  [datachannel-wasm]). DataChannels can run unordered/unreliable
  (UDP-like), which is what game traffic wants.
- DataChannels work in Safari, including iOS ([datachannel-wasm], [webrtcHacks]).

So the engine-level network code can be written **once, in C, against the
libdatachannel C API**, and `#ifdef __EMSCRIPTEN__` swaps in datachannel-wasm.
Raylib itself has no networking (fine — this layer is transport-only and lives
in `studio.c` like everything else).

### The Tsoding data point — WebSocket relay topology

[Tsoding's multiplayer prototype][tsoding] (Node.js server + browser clients
over plain WebSockets, server-authoritative state sync) is worth studying as
the *simplicity baseline*:

- **What's great:** zero NAT traversal. No STUN, no ICE, no signaling dance —
  everyone dials one server. Works from every browser ever, including iPad
  Safari, today.
- **What doesn't fit:** someone must run a public server; and *state* sync
  means the server (or host) must understand game state — which for dreamengine
  would leak networking into beginner cart code.

But the topology and the sync model are separable. A **WebSocket relay that
forwards lockstep *inputs*** (not state) keeps the cart API clean, costs ~1–2
bytes/player/frame, and entirely sidesteps NAT traversal at the price of one
hop through a server (~+20–50 ms vs P2P). That makes it a legitimate fallback
*and* a low-effort first internet-play milestone — see the staged plan.

## 2 · Matchmaking UX: join codes & LAN discovery

### "Click host → get a code" (the wanted UX)

WebRTC peers must exchange a session description (SDP) + ICE candidates before
connecting. Strictly speaking this does **not** require a server — the research
pass killed the claim that it does — any out-of-band channel works, including
copy-paste. But the SDP blob is hundreds of characters and the exchange is
*two-way* (offer out, answer back), so "paste this wall of text, then paste
theirs back" is a miserable join-code. Every friendly system therefore uses a
small **signaling service** so the code can be 4 letters:

1. Host's engine → signaling server: "create room" → gets `KKGF`
2. Joiner types `KKGF` → server relays the SDP/ICE blobs both ways
3. WebRTC connects peer-to-peer; the server's job is over (verified: after
   matchmaking, game data flows directly between peers — [Trystero])

Signaling options, cheapest first:

| Option | Cost | Notes |
|---|---|---|
| **Tiny self-hosted WebSocket server** | ~$0–5/mo | ~100 lines of Node; full control; the boring, robust choice |
| **Cloudflare Worker** (p2pcf pattern) | free tier | [p2pcf] does WebRTC signaling entirely on Workers, with tricks to minimize messages; JS, but the *pattern* ports |
| **Trystero-style serverless** | $0 | signaling over public BitTorrent/Nostr/MQTT infrastructure — genuinely no server ([Trystero]). JS-only; usable by the wasm build via JS interop, not by native. Clever, but couples you to public infra |
| **PeerJS cloud** | free | hosted signaling, JS-only, availability not guaranteed |

**NAT traversal reality check:** STUN-based hole punching fails for some
peer pairs (symmetric NATs, CGNAT); a recent large-scale libp2p study measured
~70% success for *decentralized* hole punching ([libp2p study]) — production
WebRTC with good STUN does better, but **a TURN relay fallback is what makes
"it just works" true** for the last 10–20%. Free/cheap TURN exists: Cloudflare
TURN (managed; generous free allowance) and Metered's Open Relay Project
(free TURN) — both verified ([Cloudflare TURN], [Open Relay]).

### Minecraft-style LAN discovery

Minecraft's "Open to LAN" works by **UDP multicast announcement**: the host
broadcasts a small "here I am, port N" packet to the local network every ~1.5 s;
clients listen and show the session in a list. The native build can do exactly
this (a UDP multicast socket is ~30 lines of C) and it's a *fantastic* fit for
the classroom / same-couch / sibling-iPads scenario — zero internet, zero
accounts, zero servers.

**Browsers cannot do this.** No UDP, no multicast, no mDNS listening from a web
page. LAN discovery is native-only (and on iOS, even native apps trigger the
local-network permission prompt since iOS 14 — see §5). For browser-to-browser
on the same LAN, the join-code flow still works and the WebRTC connection will
be direct (local ICE candidates), so it's fast — you just still need the
signaling hop to get connected.

## 3 · Prior art — what others do

| Project | Approach | Lesson for dreamengine |
|---|---|---|
| **PICO-8** | No built-in transport (verified). Community netplay (e.g. [pico8-net]) bridges via GPIO pins + a JS WebRTC shim, and sends **only 5 bytes of button state per player** with a wrapped `btn()` that returns local or remote bits | The headline lesson: **multiplayer = `btn()` for more players.** PICO-8 proves the API can be *invisible* |
| **Tsoding prototype** | Node WebSocket server, state sync, browser clients ([tsoding]) | Relay topology = zero NAT pain; keep it in the toolbox, but relay *inputs*, not state |
| **TIC-80 / Picotron** | No built-in netplay | The field is wide open — a fantasy console with first-class netplay is a differentiator |
| **Godot** | High-level multiplayer API over swappable transports; WebRTC classes are **built-in only for the web export — native needs an extension** (verified, [Godot docs]) | Even Godot treats native WebRTC as an add-on; bundling libdatachannel ourselves is the price of the unified story |
| **GGRS / GGPO** | Rollback netcode; GGRS runs in browsers via WASM, paired with **Matchbox** for signaling ([GGRS]) | Rollback is the *eventual* upgrade path, not the starting point; Matchbox = the "tiny signaling server" pattern, in Rust |
| **Trystero** | `joinRoom(config, code)` + send/receive actions; serverless signaling ([Trystero]) | The API bar for friendliness: *two calls and you're connected* |
| **Rune/Dusk, PlayroomKit** | Hosted platforms for casual web multiplayer; SDK hides everything | Confirms the market: beginners adopt multiplayer only when the API is ~3 functions |

## 4 · Sync model: lockstep vs rollback vs state sync

For 2–4 player beginner carts on a fixed-timestep 60 fps loop:

| Model | What travels | Cart author must… | Engine must… |
|---|---|---|---|
| **State sync** (Tsoding) | game state, continuously | decide what state to send — **leaks networking into beginner code** | nothing special |
| **Deterministic lockstep** | inputs only (~1–2 B/player/frame) | nothing — carts stay as-is | guarantee determinism; buffer inputs with a small delay |
| **Rollback (GGPO)** | inputs only | nothing | determinism **plus** save/load state + re-simulate; much more engine work ([GGPO]) |

**Lockstep wins for stage 1**, for a reason specific to this repo: the
deterministic harness already exists and is exercised by `play.js`
record/replay. Lockstep is *literally* "everyone replays the same recording,
live". The known costs:

- **Input delay** — each player's input takes effect N frames later (N≈2–4 at
  60 fps ≈ 33–66 ms). For the kinds of games carts are (arcade, co-op,
  turn-based), this is fine. Fighting-game players invented rollback because
  they can't tolerate it; that's the later upgrade.
- **One stall stalls everyone** — if a packet is late, the game waits. Mitigate
  with a small jitter buffer; acceptable at 2–4 players.

**Rollback later, cheaply:** the `STATE { ... }` / `de_state()` sugar from
[`cart-as-script.md`](cart-as-script.md) puts cart state in **one flat block**.
That makes save-state = `memcpy`, which is 90% of what rollback needs — and
makes **desync detection** trivial: CRC32 the state block every N frames,
exchange checksums, mismatch ⇒ surface "desync at frame F" (exactly what GGPO's
sync-test mode does ([GGPO])). The same checksum doubles as a *test*: record a
session natively, replay it in the wasm build, compare checksums per frame —
the existing harness can verify cross-compiler determinism **before any
networking is built**.

### The determinism risk (clang-arm64 vs emcc-wasm32)

The one genuinely hard problem. IEEE-754 basic ops (+ − × ÷ √) are bit-exact
everywhere *if compiled consistently*, but determinism breaks via: libm
transcendentals (`sinf` differs per platform), FMA contraction, `-ffast-math`,
x87 excess precision (not a concern on arm64/wasm), and uninitialized memory
([Gaffer], [randomascii]). Box2D v3 ships cross-platform/cross-compiler
determinism by avoiding exactly these ([Box2D]). dreamengine's mitigations are
unusually good:

- Carts use **engine math**, not libm: degree-based trig
  ([decision 0004](../decisions/0004-degree-based-trig.md)), engine `rnd()`
  (seedable PRNG, already used by `--det`). Implement engine trig as
  table-lookup or own-polynomial → bit-identical on both targets.
- Compile both targets **without fast-math and with FMA contraction off**
  (`-ffp-contract=off`) for the cart + sim-relevant engine code.
- The replay-checksum test above turns "hope it's deterministic" into CI.

### Player-count ceiling — "20 players" is a different subsystem *(someday)*

Everything above assumes **2–4 players** (a couch/classroom stretch to ~8).
That is not an accident of scope — it is what the *lockstep model itself*
allows. A cart with 20 players is **off-axis from this whole plan**, not rung 6
of it. Two walls, one trivial and one fundamental:

1. **The API cap is trivial.** `btn(player, …)` returns false for `player > 1`
   today (`studio.c`), and `net.h`'s arrays are `[2]` (host = 0, joiner = 1).
   Widening those to `MAX_PLAYERS` and fattening the input packet is an
   afternoon. This is *not* the blocker.
2. **Lockstep has a hard ceiling that has nothing to do with dreamengine.**
   The per-frame barrier (`net_frame_sync`) means *the game runs at the speed of
   the slowest peer* — every frame waits until **everyone's** input has arrived.
   At 2–4 players on wifi that's fine; at 20, the odds that all 20 datagrams land
   inside one 16 ms frame collapse, and one player on bad wifi stalls all 20.
   This is exactly why lockstep RTS games cap around 8 and bolt on
   lag-adaptation, and why no 20-player game ships on lockstep. Lockstep's other
   property — **every machine simulates the entire world** — is its strength at
   small N and its doom at large N.

Big-player-count games (io-games, battle-royale, party games) don't use lockstep
at all: they use **server-authoritative state sync** (the "Tsoding topology",
§1) — one server owns the truth, each client sends its input up and renders a
*view* of what the server sends down. No shared deterministic sim, no barrier,
tolerates per-player lag. §4's own table already flagged the catch: state sync
**leaks networking into cart code** (the author must decide what state to send),
which breaks the "beginners write these, `btn()` is invisible" promise that
makes rung 1 beautiful. So many-player support isn't a bigger version of what we
built — it's a **second, parallel subsystem** with a different API philosophy,
and it would have to answer "how does this stay friendly for beginners?" from
scratch.

| | Players | Model | Cart API | Status |
|---|---|---|---|---|
| **A · shared sim** | 2–4 (≤~8 stretch) | lockstep, `btn()` invisible | zero cart changes | rung 1 shipped |
| **B · many players** | 10–20+ | server-authoritative state sync | cart must model net state | someday; not designed, not on the ladder |

Honest take: given the lo-fi / beginner ethos, the `btn()`-lockstep sweet spot
(2–4, maybe a couch-multiplayer 8) is probably the *right* scope. Product B is a
fork in the road to revisit only if a genuinely many-player cart becomes a goal —
at which point it's its own design pass, not an extension of this one.

## 5 · iOS / iPad path *(least-verified section)*

Likely ship vehicle: the **wasm web player in Safari** (no App Store, no
per-device builds — fits the learning-environment ethos).

- **WebRTC DataChannels work in Safari/iOS** — datachannel-wasm lists Safari
  compatibility; Safari has shipped WebRTC since 11 ([webrtcHacks]). The
  join-code flow should work unchanged on an iPad in Safari. *(Reported;
  needs a hands-on test.)*
- **WKWebView** (if a native shell app ever happens): WebRTC support arrived
  ~iOS 14.3+; older shells had to fall back to WebSockets. *(Unverified.)*
- **LAN discovery on iOS:** a *native* app doing UDP multicast triggers the
  iOS 14+ **local-network permission prompt**; Safari pages can't do LAN
  discovery at all. So on iPad, "join code" is the path even at home —
  the WebRTC connection itself will still be direct/local and fast.
- **Multipeer Connectivity / Game Center:** native-app-only, Apple-only;
  not worth designing around unless a native iPad app becomes a goal.

Practical consequence: **nothing about the recommended design changes for
iPad** — the wasm + DataChannel + join-code path *is* the iPad path. That's the
strongest argument for it.

### What about a *native* iPad app? ("it's just C, right?")

The C is indeed the easy part — BSD sockets and libdatachannel compile fine for
iOS. The nuances are everything *around* the C, and none of them are
networking:

- **Raylib has no official iOS platform.** iOS isn't in raylib's supported
  target list; the community route is a maintained fork
  ([ghera/raylib-iOS](https://github.com/ghera/raylib-iOS)) running OpenGL ES
  via **ANGLE** (GL-on-Metal), and an upstream port attempt stalled
  ([PR #3880](https://github.com/raysan5/raylib/pull/3880)) partly because iOS
  inverts raylib's architecture: Apple's `UIApplicationMain` owns the event
  loop, while raylib (and `studio.c`) expect to own `main()` and the loop. The
  SDL3 backend (raylib 5.5+) is the most plausible official-ish path since SDL
  supports iOS properly.
- **No JIT, ever.** iOS enforces W^X — no writable+executable memory for
  third-party apps. The **libtcc live backend is dead on arrival** on iOS;
  carts cannot be compiled or hot-reloaded on-device as native code. A native
  iPad app must either bundle precompiled carts or embed an *interpreter*
  (e.g. a wasm interpreter like wasm3 running emcc-built carts — interpretation
  is allowed, JIT is not).
- **App Store rule 2.5.2** forbids downloading code that adds functionality —
  which is what "browse the cart shelf and play" *is*. But there's a carve-out
  that fits dreamengine almost word-for-word: **educational apps designed to
  teach or test executable code may download code, provided the source is
  completely viewable and editable by the user** (the Swift Playgrounds
  precedent, added 2017). A dreamengine player that always shows the cart's C
  source plausibly qualifies — but "plausibly qualifies" means App Review
  roulette, not a guarantee.
- **LAN discovery needs Apple's permission, literally.** UDP multicast/broadcast
  on physical iOS hardware requires the restricted
  `com.apple.developer.networking.multicast` entitlement — you apply to Apple
  per-account and wait (~2 weeks, reportedly) — *on top of* the iOS 14
  local-network user prompt. Unicast UDP/WebRTC needs no entitlement, so
  join-codes work without asking Apple for anything.
- Plus the standard friction: $99/yr developer account, signing, review cycles
  (cf. [`packaging-distribution.md`](packaging-distribution.md) — the same
  forces that make native desktop packaging the harder path).

Net: a native iPad app is an "App Store policy + raylib-iOS port + cart
interpreter" project in which networking is the *easiest* component. The wasm
Safari player sidesteps every bullet above, which is why it stays the
recommended vehicle; revisit native only if Safari performance or touch-input
limits demand it.

## 6 · The cart-facing API (the point of all this)

Survey says friendliness ≈ 3 calls (Trystero: `joinRoom` + actions; pico8-net:
just `btn()`). Proposal — keep carts network-unaware, hide everything in the
engine:

```c
// netplay — lockstep multiplayer (engine hides signaling/ICE/buffering)
void net_host(void);              // host a session; engine overlays the join code
void net_join(const char *code);  // join by code, e.g. net_join("KKGF");
int  net_players(void);           // players in session (1 = solo / not connected)
int  net_me(void);                // my player index, 0 = host
bool net_ready(void);             // true once all players are connected + synced
```

And that's the whole API, because **`btn(player, button)` already takes a
player index** — under netplay it simply returns the lockstep-buffered input
of that player, local or remote. A two-player cart written for one keyboard
(player 0 = arrows, player 1 = WASD) becomes an internet game by adding *one
line* and using `net_me()` to pick a side:

```c
void update(void) {
    if (btn(0, BTN_LEFT))  p[0].x--;   // host's input — local or remote
    if (btn(1, BTN_LEFT))  p[1].x--;   // joiner's input — local or remote
}
```

Even `net_host()`/`net_join()` could be optional: the **shell** (editor / cart
player) can own the host/join UI — a "🌐 host" button next to ▶, a code entry
field — so *zero* cart code changes are needed for any existing 2-player cart.
The C calls exist for carts that want in-game lobbies.

What the engine hides: signaling, ICE/STUN/TURN, the input-delay ring buffer,
frame-sync barrier, state checksums, disconnect/timeout handling ("PLAYER 2
LEFT" overlay), and pause-on-stall. What the engine *requires* (documented, and
mostly already true): use engine `rnd()`/trig, don't read wall-clock time in
`update()`, fixed timestep — i.e. exactly the rules `--det` replay already
imposes.

## 7 · Recommendation — staged plan

| Stage | What | Effort | Risk |
|---|---|---|---|
| **0. Determinism proof** | Cross-target replay test: record native → replay wasm → compare per-frame state checksums (CRC of `de_state()` block). Fix what diverges (libm, fp flags). No networking. | days | low — and it de-risks everything after |
| **1. LAN lockstep, native** | UDP sockets + multicast discovery ("open to LAN" in the shell). Input-delay lockstep over the existing input pipeline. Two Macs on one network. | ~1 week | low; pure BSD sockets |
| **2. Join codes, everywhere** | libdatachannel (native) + datachannel-wasm (web); tiny signaling server (~100-line Node/Worker) minting 4-letter codes; free TURN fallback (Cloudflare/Metered). Native↔browser cross-play. | ~2–3 weeks | medium — ICE edge cases, TURN config, build integration |
| **2b. (fallback/shortcut)** | WebSocket *input relay* server (Tsoding topology, lockstep payload). If stage 2 stalls, this gets internet play working everywhere — incl. iPad Safari — for ~3 days' work, at +1 hop latency. | days | low |
| **3. iPad** | Test the stage-2 wasm player in Safari on iPad; fix touch input → `btn()` mapping (on-screen buttons). | days | Safari quirks (unverified territory) |
| **4. (someday) Rollback** | GGPO-style, enabled by the flat `de_state()` block (save = memcpy). Only if input delay ever actually bothers anyone. | weeks | high; probably never needed |

**Main risks, honestly:** (1) cross-compiler float determinism — mitigated by
stage 0 being *first*; (2) NAT traversal failures for ~10–20% of peer pairs
without TURN — mitigated by free TURN tiers; (3) operating a signaling server
forever — mitigated by it being ~100 lines, stateless, and free-tier-sized;
(4) iOS claims in §5 are the least verified — stage 3 starts with a spike, not
a plan.

### The rung ladder — lowest-hanging fruit first (code-level)

A follow-up pass against the actual `studio.c` sharpened the staged plan into
smaller rungs, each independently shippable. Two findings move work *off* the
critical path:

1. **The input seam already exists.** Every `btn()`/`key()`/`mouse_*()` read
   funnels through the `inp_*()` indirection layer (`studio.c`, "input
   indirection" comment) so the replay harness can inject state. Lockstep is
   just a *third* input source besides keyboard and replay-file — the
   architecture was already paid for by the debug harness.
2. **Native↔native needs no determinism work.** The same binary on two Macs is
   bit-identical by construction. The clang-vs-emcc float problem (stage 0
   above) only gates *wasm cross-play*, not the first playable rungs.

| Rung | What | Effort | Needs |
|---|---|---|---|
| **1. Localhost lockstep** — **SHIPPED 2026-07-02** | `--net-host` / `--net-join <ip>` / `--net-port <n>` flags on the native binary; UDP; per frame each side sends **1 byte** of packed `btn()` bits (with an 8-frame redundancy window), waits for the peer's byte; host sends the seed in the handshake. Two windows, one Mac, pong. Landed as `runtime/net.h` (~250 lines), gated by runtime flags rather than a `-DDE_NET` define (mirroring how `--det`/`--replay` work — always compiled native, zero footprint unless flagged). `play.js <cart> netdemo` spawns both windows AND doubles as the desync gate: per-side scripts + trace diff → `LOCKSTEP OK`/`DESYNC` ([checks-and-oracles](../guides/checks-and-oracles.md)). Note: since it takes any IP, **this already covers rung 2's transport** — what rung 2 adds is the shell UX (show the host's IP, a join screen). | ~1–2 days *(actual: 1 session)* | nothing |
| **2. LAN by IP** — **SHIPPED 2026-07-02** | Same UDP code across two machines. Host resolves its LAN IPv4 with `getifaddrs()` (`net_local_ipv4()` in `net.h` — prefers a 192.168/10 private address) and prints `HOSTING on <ip>:<port> — the joiner runs: --net-join <ip>`; the editor drives it with a **🌐 multiplayer button** next to ▶ (host / join-by-IP popover in `shell.js`; `main.cjs` adds the `--net-*` flags and shows the host's IP via `os.networkInterfaces()`). **This is the wished-for "click host → get an address" UX** for the home/classroom case — no NAT, no servers. **Deviation from the original plan:** the address surfaces in the editor UI + console, *not* an in-window overlay — `net_handshake()` blocks *before* `InitWindow`, so during the host's wait there is no window to draw on; surfacing it in the shell (where the Host button is) is both lower-risk and closer to where the user is looking. | ~1 day *(actual: 1 session)* | rung 1 |
| **2.5 In-game lobby + pause entry + Windows** — **SHIPPED 2026-07-02** | An engine-owned **Host / Join / Solo boot menu** (`net_lobby_menu()` in `studio.c`, gated by `--net-lobby` or the compile-time `DE_NET_LOBBY_DEFAULT`), so a **standalone build with no editor** can start netplay with no CLI flags — the son-runs-the-exe case. Plus a **`MULTIPLAYER` item in the pause menu** that **self-restarts** the binary with `--net-lobby` (`net_restart_into_lobby()` — reusing the RESTART item's `execv(restart_argv)`), because netplay needs its startup order (seed before `init()`), so re-entering net mid-game is a clean relaunch. A player double-clicks the exe → plays solo → pause → MULTIPLAYER → lands in the lobby; two launches on one machine each do this (one Hosts, one Joins `127.0.0.1`). And **`net.h` is now ported to Winsock** so netplay carts cross-compile to a real Windows `.exe` (winsock2-before-windows.h ordering handled; `getifaddrs`→UDP-connect trick; `-lws2_32`) — compile-verified, Windows runtime test pending a real box. Required reordering the net startup: the lobby draws after fonts load but before the cart's `init()` (the host's seed must reach the joiner before any `rnd()`), so the handshake now happens *with the window open* (which also fixes rung 2's "host waits with no window" rough edge — a `HOSTING at <ip>` status frame is drawn). Join screen has an in-window IP text-entry (digits + dots). This is §6's "the shell owns the host/join UI", except **for a standalone the *engine* is the shell**. The direct `--net-host`/`--net-join` path (editor/CLI/netdemo) is unchanged (still handshakes before the window). | ~1 session | rung 2 |
| **3. "Open to LAN"** — **SHIPPED 2026-07-02** | The host broadcasts a small ANNOUNCE datagram to the subnet (`255.255.255.255:33446`) every ~1 s while waiting (`net_announce()`, game socket + `SO_BROADCAST`); the joiner's lobby Join screen listens (`net_discover_begin/poll/end`, a `SO_REUSEADDR` socket on the discovery port) and **auto-fills the discovered host IP — no typing** ("found a game at 192.168.x.x — ENTER to join"), with manual entry kept as a fallback. Cross-platform (Winsock too). Minecraft-style. Verified: the announce is received on-box (sniffed: `DN` + type 5 + port 33445) and cross-compiles for Windows; the full two-machine pick is a live test. | ~1 session | rung 2 |
| **4. Determinism proof** | Per-frame CRC of the `de_state()` block in the existing trace; record native → replay under wasm → diff. Note the web build currently compiles the harness out (`#else // web build: harness is a no-op`), so enabling replay under emcc is part of this rung. Gates rung 5, **not** rungs 1–3; can run in parallel. | days | nothing |
| **5. Internet + browser** | Stage 2 / 2b above (join codes + signaling + libdatachannel, or the WebSocket input-relay shortcut). The first rung needing infrastructure. Decide after rungs 1–3 have proven the model and the input-delay feel is tuned. | weeks | rungs 1 + 4 |

The one real refactor lives in rung 1: today `btn(player, …)` means "which
*keymap* on this keyboard" (`P0_*`/`P1_*` defines + touch UI). Under netplay it
must mean "which *machine*" — all local input methods (any keymap, touch)
produce *my* bits; remote bits come off the wire. The existing per-frame
`btn_curr`/`btn_prev` snapshot (which powers `btnp()`) already happens at the
frame boundary, so edge detection works unchanged once `btn()` reads from the
per-player frame buffers. Scope note: v1 lockstep syncs `btn()` only — carts
using `mouse()`/`key()` stay single-machine until those are added to the input
packet.

Net: rungs 1–3 need **no servers, no NAT traversal, no determinism work, no
wasm** — BSD sockets plus a seam the debug harness already built. Two Macs
playing pong over wifi is roughly a week of part-time work from a standing
start.

### Distribution: shipping the standalone build — code signing is the open TODO

Rung 2.5 made a cart runnable with no editor (boot lobby / pause → MULTIPLAYER),
and the editor's **export .exe** / **export mac** buttons (`studio:export-win` /
`studio:export-mac` in `main.cjs`) emit a self-contained single file with the
lobby baked in (`DE_NET_LOBBY_DEFAULT`). What's shipped works, but every exported
binary is **unsigned**, so the OS scares the recipient once:

| Platform | Today (unsigned) | TODO for a clean, warning-free open |
|---|---|---|
| **Windows** | SmartScreen "Windows protected your PC / unknown publisher" → *More info → Run anyway* | An **Authenticode** code-signing certificate (OV/EV from a CA, ~$100–400/yr); EV clears SmartScreen reputation immediately, OV warms up over installs. |
| **macOS** | Gatekeeper "unidentified developer / can't be opened" → **right-click → Open** once (and a bare exported binary fails outright on another Mac — confirmed 2026-07-02: a colleague couldn't open it) | A **`.app` bundle** + **Developer ID Application** cert + **notarization** (`notarytool`) + **staple**. **Pipeline is built:** [`tools/mac-app.sh`](../../tools/mac-app.sh) (bundle → codesign → notarize → staple; header has the one-time setup). Blocked only on prerequisites: the maker has only an *Apple Development* cert, **not** a Developer ID one — mint it in Xcode (Accounts → Manage Certificates → + → Developer ID Application; paid account, Team ID `L4S453HYLF`), plus an app-specific password + `notarytool store-credentials`. **Do this on the home Mac** (where iOS signing is already set up); the Developer-ID private key is machine-bound (export a `.p12` to sign elsewhere). |

**Two separate walls (learned 2026-07-02, when a colleague couldn't open a
Slack'd binary).** The "couldn't open it" failure is easy to misdiagnose as a
signing problem when the *first* wall is actually plumbing:

1. **Transport strips the executable bit.** Slack / email / most transfers don't
   preserve POSIX permissions, so a bare Mach-O exported `chmod +x` arrives on the
   other Mac as a non-executable `0644` file — Finder won't run it, which reads as
   "can't open it," *before Gatekeeper is even reached.* **Fix: always send it
   zipped** — a `.zip` (or `ditto`) preserves the `+x` bit, and a `.app` (a folder)
   must be zipped to send anyway.
2. **Gatekeeper (unsigned + quarantine).** Even with `+x` restored, an unsigned
   download triggers "unidentified developer / cannot be verified" (or "damaged").
   **Fix: Developer ID sign + notarize** (`tools/mac-app.sh`).

Isolate which wall a recipient hit, no signing needed: send the **zip**, have them
unzip → **right-click → Open**. Runs (maybe after one "open anyway") ⇒ it was only
the `+x` bit. "Unidentified developer / damaged" ⇒ Gatekeeper, signing required.
Only a **notarized `.app`** double-clicks with *zero* friction (no zip dance, no
right-click, no warning) — that's the end state.

**Decision on file (2026-07-02):** ship **unsigned binaries for now** — fine for
"send it to your kid, click through the one warning" and for the maker's own two
Macs (right-click → Open). Signing/notarization is deferred, not cancelled;
revisit when handing builds to strangers is a real need. The Mac path also wants
a real `.app` bundle (double-click without Terminal) — see
[`packaging-distribution.md`](packaging-distribution.md) for the broader packaging
history and [ADR-0023](../decisions/0023-ship-carts-as-apps-not-the-editor.md)
(ship precompiled results, never the editor).

### Scenario: a few machines on one wifi, all playing the *wasm* build

A tempting-but-wrong mapping: "they're all on my LAN, so that's rung 2/3 (LAN
by IP / Open to LAN)." It isn't. **Rungs 1–3 are native-only** — they lean on
raw UDP + multicast, which **browsers do not have** (§2, "Browsers cannot do
this"). A wasm tab can't broadcast "here I am" or discover a peer, even on the
same wifi. So the friendly LAN-discovery magic is off the table the moment the
clients are browsers.

For an all-wasm LAN, the surprise is that the *easy* environment (one LAN, no
NAT between machines) and the *hard* client (browser, no UDP) pull opposite
ways. The fit is a trimmed-down **rung 5** — specifically the **2b WebSocket
input-relay**, not the full WebRTC path:

| Option | Fit for all-wasm-on-one-LAN | Why |
|---|---|---|
| **Rungs 1–3** (UDP/multicast) | ❌ doesn't apply | native-only; browsers have no UDP/multicast |
| **Stage 2** (datachannel-wasm + signaling + STUN/TURN) | ⚠️ works, but overkill | STUN/TURN exist to punch through NAT *between* peers; on one LAN there's no NAT to punch. You'd pay the whole ICE/signaling apparatus to solve a problem you don't have |
| **Stage 2b** (WebSocket input-relay) | ✅ **the fit** | run the ~100-line relay on one box on the wifi; every browser opens `ws://<that-box-LAN-IP>:port`; it forwards lockstep `btn()` packets. No STUN, no TURN, no ICE, no signaling dance |

The doc's one knock on 2b — "+1 hop latency vs P2P (~+20–50 ms)" — **evaporates
on a LAN**, where that hop is sub-millisecond. So the only downside 2b carries
on the open internet simply isn't present here.

Two things ride along:

1. **Determinism is the *cheap* case, not the scary one.** The clang-arm64 vs
   emcc-wasm32 float-divergence risk (§4, rung 4) is a *cross-target* problem —
   native record → wasm replay. If **every** machine runs the **same wasm
   build**, it's wasm↔wasm, bit-identical by construction — exactly like the
   "two Macs, same binary" native↔native case that needs zero determinism work.
   The all-wasm topology dodges the one genuinely hard problem.
2. **The web build currently compiles the harness out** (`#else // web build:
   harness is a no-op`, rung 4). The lockstep input plumbing — the `inp_*()`
   indirection seam — is stubbed under emcc today, so *enabling* it for the web
   target is real work regardless of transport. This is the actual gating task
   for an all-wasm setup, not NAT/determinism.

Bottom line: a tiny WebSocket input-relay on one wifi box, browsers dialing its
LAN IP, forwarding lockstep inputs. It sidesteps NAT traversal (none exists),
sidesteps cross-compiler determinism (all same wasm), and the latency cost the
plan holds against it doesn't apply on a LAN.

## Scoped plan — rung 5a: web/wasm multiplayer via a WebSocket input-relay

> **Status: shipped — steps 1–3 LIVE-VERIFIED (2026-07-06): two real browsers
> played a relay match on the home wifi.** Steps 2 (WebSocket transport) + 3
> (`tools/net-relay.js`) landed the day after step 1; `node tools/net-check.js`
> is the one-command gate (echo + netdemo + relay wire-protocol), and
> `node tools/net-relay.js --serve site/<cart>` is the two-devices-on-one-wifi
> setup. A leaving peer now shows an on-screen banner on the survivor's side
> ("PLAYER 2 LEFT — PLAYING SOLO") — the console is invisible in a browser.
> Remaining in 5a: step 4's real lobby (in-cart room-code entry) and step 5's
> CRC tripwire; see also §"Hosting beyond the LAN" below for the github.io
> story. The pragmatic first step to get *browsers*
> (and iPads) playing, chosen over full WebRTC because it needs no NAT traversal,
> no STUN/TURN, no signaling dance — just a tiny relay. Reuses the entire native
> lockstep model (same barrier, same 1-byte packet, same lobby concept); what's
> new is a WebSocket transport for the web target and the relay itself. WebRTC +
> join-codes (play with *anyone, anywhere*) stays the later rung 5b upgrade.
>
> **Ground truth, corrected in the step-1 build (2026-07-05):** the plan's
> "un-stub the web `inp_*()` seam" framing was wrong — lockstep never went
> through the harness's `inp_*()` injection. The real injection point is
> `btn()` + `net_bits[]` (net.h samples local hardware via `btn_local()`, the
> barrier resolves both players' bytes, `btn()` answers from them). So step 1
> became: compile the lockstep CORE under `PLATFORM_WEB` (the `DE_NET_CORE` /
> `DE_NET_BUILD` split below), give the web loop a non-blocking barrier, and
> unify the deterministic clock. Shipped as exactly that; see the step table.

### The architecture in one picture: ONE lockstep core, TWO transports

Web multiplayer is **not a second multiplayer system bolted next to the native
one.** It is the *same* lockstep engine with a swapped bottom layer. Everything
above the transport line is built once and shared; only the socket send/receive
changes per platform. This is the whole mental model — keep it in view:

```
              cart code  —  btn(0,…) / btn(1,…)      ← 100% network-unaware, IDENTICAL on both
                                  │
        ┌─────────────────────────┴─────────────────────────┐
        │              LOCKSTEP CORE   (shared)              │
        │  frame barrier · input ring · redundancy window ·  │   ← built once, used by native AND web
        │  seed handshake · lobby (Host/Join/Solo) ·         │
        │  de_players() gate · NET_PKT_* 1-byte packets      │
        └─────────────────────────┬─────────────────────────┘
                     net_sendto() / net_pump()                 ← the ONE seam that differs
        ┌─────────────────────────┴─────────────────────────┐
        ▼                                                     ▼
   UDP sockets  (native)                          WebSocket → relay  (web)
   localhost · LAN-by-IP · broadcast discovery    room code (?room= in the URL)
   ✅ SHIPPED (rungs 1–3)                          ✅ BUILT 2026-07-05 (steps 2–3;
        (+ a third arm: ✅ echo loopback             live two-tab pick pending)
           — the fake peer, both targets)
```

Concretely — **this split SHIPPED as step 1 (2026-07-05)**: the lockstep core is
transport-agnostic (`DE_NET_CORE`, compiled for both targets) and every send/
receive goes through the `net_transport_send`/`net_transport_pump` seam (UDP ↔
echo today; step 2 adds the WebSocket arm there and nowhere else). The lobby,
`de_players()`, packet format, barrier, and `btn()` semantics are *not*
re-implemented; they're the shared core.

**Interop caveat (the only place the two aren't interchangeable):** two natives
play each other and two wasms play each other, each **bit-identical by
construction**. A *native ↔ browser* match (Mac vs a tab) is a further step —
that's the one needing the clang-vs-emcc float-determinism proof (rung 4).
Same-platform play needs none of it; it's the normal case.

| Step | What | Effort | Risk / notes |
|---|---|---|---|
| **1. Un-stub the web input seam** — **SHIPPED 2026-07-05** | Landed as the "one lockstep core, two transports" split, not an `inp_*()` change (see the corrected ground truth above). `net.h` is now two tiers: **`DE_NET_CORE`** (rings, `NET_PKT_*`, barrier — no sockets, compiles native AND web; `studio.c` gates it on `!DE_NO_RAYLIB`) and **`DE_NET_BUILD`** (UDP transport + `--net-*` flags + lobby, native-only as before). Sends/receives go through the **`net_transport_send`/`net_transport_pump` seam**. New **`net_frame_try_sync()`** non-blocking barrier: web `loop_step` calls it at the top of the tick and **stalls the whole tick** (no `sound_tick`, no update/draw, no clock) while the peer's byte is missing — the browser main thread can't block or WS messages never arrive; native `net_frame_sync()` is now a blocking wrapper around it (netdemo-verified identical). `det_mode`/`det_clock`/`clk()` moved out of the native-only block so web runs the fixed-step deterministic sim under net. Proven by the new **echo transport** (loopback fake peer, zero sockets): `--net-echo` native / `-DDE_NET_ECHO_DEFAULT` web — P2 mirrors P1 through the real pack→send→ring→barrier→`btn(1)` path. Gates run: netdemo `LOCKSTEP OK` (300 frames, per-side scripts), echo trace `p2y==p1y` ×300, `build-all` 466/466, emcc compile of the core both bare and echo-enabled. | ~2–3 days *(actual: 1 session)* | low-ish; mechanical, but touches the hot `inp_*()` layer — guard with the existing input/replay tests. |
| **2. WebSocket transport for the web target** — **BUILT 2026-07-05** (live two-tab pick pending, like rung 3's two-machine test) | Landed as the `EM_JS` shim (thin WS layer, as planned — no BSD-socket emulation): `de_ws_begin/state/send/recv` in `net.h`'s web arm. **No strings cross the C/JS boundary** — JS reads `?room=`/`?relay=` from `location.search` itself (no `UTF8ToString` runtime-export dependency); C sees stage codes + raw packet bytes. The handshake is **async** (`net_web_poll`, the non-blocking twin of `net_handshake`): driven by the pre-init "click to start" screen in studio.c, ROLE → HELLO → WELCOME{seed} through the relay, seed applied before `init()` — same ordering the native lobby enforces. Same `NET_PKT_*` frames; `net_ws_pump` handles INPUT/BYE mid-game; a dead socket counts as BYE (web drops to solo instead of exiting the tab). New `NET_PKT_ROLE` (6) — the one relay-originated packet (seat 0 = host). Verified headless by `net-check.js`'s relay leg (two simulated carts through a real relay, full protocol) + emcc compile. | ~2–3 days *(actual: 1 session, with step 3)* | medium; emscripten's socket story is the fiddly part — a thin WS layer is more predictable than its BSD-socket emulation. |
| **3. The relay server** — **SHIPPED 2026-07-05** | `tools/net-relay.js` — ~250 lines of **zero-dependency** Node (hand-rolled RFC 6455, no `ws` package): a client joins `/room/<code>` (rooms of 2), the relay sends ROLE on seating, then **blindly forwards every binary frame** to the other member — it never parses game packets, so one relay serves every cart forever. Disconnect → synthesized BYE to the peer + room closes (a room is one match; codes are reusable after). Extras beyond the plan: **`--serve <dir>`** makes it the one-wifi-box setup (static-serves a `build-site.js` cart folder AND relays from one process — it prints the exact `http://<lan-ip>:<port>/?room=play` URL to open on two devices), and **`--check`** is a headless self-test (8 asserts: roles, verbatim forwarding both ways, room-full refusal, BYE-on-disconnect, code reuse). | ~1 day *(actual: same session)* | low; stateless, free-tier-sized. Run it on one LAN box (sub-ms) or a $5 VPS (internet, +1 hop). |
| **4. Web lobby / room UX** — **v0.5 SHIPPED 2026-07-06 (site chrome)** | **The URL is the lobby, and the site now mints + shares it.** Gallery: 2-player carts (derived at build time from the SOURCE's `de_players()` — `cartPlayers()` in build-site.js, no hand-maintained metadata) get a **👥 play together** button that mints a room code and opens the cart in it, `&relay=` appended automatically off the Render domain. Cart page: a **room bar** (both shells) — with `?room=` it shows the code + a **copy invite link** button (canvas text can't be copied; the page URL *is* the invite); without one it offers *play together* when the cart's `manifest.json` carries `players ≥ 2` (stamped by build-site.js; absent → hidden, so editor previews are unaffected). Still open for a real v1 *in-cart* lobby: room-code entry in the Host/Join/Solo shape, for the player who can't receive a link. | ~1–2 days | low; mostly the lobby draw, already exists for native. |
| **5. (optional) Determinism gate** | wasm↔wasm is bit-identical (same build) so no cross-compiler work is needed — but add the per-frame CRC of the `de_state()` block (rung 4) as a cheap desync tripwire, reused from the native netdemo gate. | ~1 day | low; turns "hope" into a test. Only *native↔wasm* cross-play would need the real determinism proof — out of scope here. |

**Scope boundary.** This rung is **2 players through one relay** (LAN box or a
single shared server) — the all-wasm-on-one-wifi and "share a URL + room code"
cases. It deliberately does **not** do WebRTC P2P, NAT traversal, TURN, or
join-code signaling — that's **rung 5b** (§1–§2 above, WebRTC + `datachannel-wasm`
+ a signaling worker + free TURN), the "play with anyone, anywhere, direct P2P"
upgrade to add once the relay proves the model and the input-delay feel is tuned.

**Why this order.** Step 1 (input seam) is the true blocker and is reusable by
*any* web transport, so it de-risks 5b too. Steps 2–4 are small and independent.
The whole rung 5a is ~1–1.5 weeks part-time and gets browsers **and iPad Safari**
playing — which also unlocks the tinyjam **co-op jamming** spark
([`tinyjam-racks.md`](tinyjam-racks.md) → "Spark: co-op jamming"), since a shared
rack in two browser tabs is exactly this.

### Hosting beyond the LAN — github.io + a wss relay

The cart half is already solved: the gallery publishes to
<https://nikkikoole.github.io/dreamengine/> (`tools/publish-cart.sh`), and a
republished multiplayer cart there reads `?room=`/`?relay=` like any other web
build. **What github.io can NOT do is run the relay** (Pages is static-only) —
and it forces the second constraint: Pages is **https**, and a browser blocks
plain `ws://` from an https page (mixed content), so the relay must be
reachable over **`wss://`** (TLS). The relay itself already works behind any
TLS terminator — it speaks plain WS and lets the host do the certificates.
Options, cheapest-first:

| Option | What it is | Cost / catch |
|---|---|---|
| **Free-tier PaaS** (Render, Fly.io, Railway) — **CHOSEN (2026-07-06), blueprint committed** | `node tools/net-relay.js` as a tiny web service — they hand out an https/wss domain automatically | free tier idles/sleeps on some plans (first joiner waits a few seconds); ~10 min setup |
| **Cloudflare Tunnel** from any always-on box at home | free wss-capable hostname pointed at a local relay | the box must stay on; tunnel setup once |
| **$5 VPS + Caddy** | Caddy auto-TLS proxying to the relay | the classic; zero surprises, small bill. The natural upgrade when 5b needs signaling+TURN anyway |

**The blessed path (picked: Render free tier — nothing at home to babysit,
reversible for the price of editing `?relay=`).** Since the repo split
(2026-07-06, [`sharing-channels.md`](sharing-channels.md)) the **public site
repo self-deploys the relay**: `site/render.yaml` is the blueprint and
`site/net-relay.js` is a copy publish-cart.sh keeps in sync — **Render
connects to the public `NikkiKoole/dreamengine` repo only**; the (soon
private) code repo is never shared with any third-party service.

1. Create a Render account → **New → Blueprint** → point it at
   **`NikkiKoole/dreamengine`** (the public site repo). It finds
   `render.yaml` and deploys `dreamengine-relay` (Frankfurt, free plan,
   zero-dep).
2. The service **also serves the gallery** (`--serve .`), so games run
   straight off the Render domain with **no `?relay=` param at all**:
   `https://dreamengine-relay.onrender.com/<cart>/?room=<code>`. The
   github.io links work too, with the explicit param:
   `https://nikkikoole.github.io/dreamengine/<cart>/?room=<code>&relay=wss://dreamengine-relay.onrender.com`.
3. **Cold starts:** the free tier sleeps after ~15 idle min; the first
   connection waits ~30–60 s. Warm it before a session by opening the bare
   `https://…onrender.com/` URL (it answers with the gallery).
4. Redeploys are manual (`autoDeploy: false`) — a publish shouldn't bounce a
   live match; click deploy on Render when the relay code itself changes.

**One relay, every cart, no collisions:** rooms are **cart-scoped by
construction** — the web transport prepends the cart's URL path segment to the
code (`?room=play` in pong becomes room `pong-play` on the wire), so a shared
relay can host pong and blackjack matches with the same human-friendly code
simultaneously and never cross-pair them. The relay stays blind; the cart name
exists only in the room string.

Then the shareable link is
`https://nikkikoole.github.io/dreamengine/<cart>/?room=<code>&relay=wss://<relay-host>`
— note `?relay=` is **required** there (the default is the page's own host,
which on github.io has no WS endpoint; without it the cart shows "relay
unreachable"). LAN play (`net-relay.js --serve`) is unaffected — same code,
`ws://` is fine on a plain-http LAN page. One more Pages caveat, pre-existing:
Pages can't set COOP/COEP headers, so the AudioWorklet build's
SharedArrayBuffer path isn't available there — the gallery's ScriptProcessor
fallback already covers it (sound works on the published gallery today).

---

## Scoped plan — rung 5b: WebRTC peer-to-peer (play anywhere, LAN-fast)

> **Status: BUILT + play-tested (2026-07-07).** Steps 1–4 shipped
> (commit `05a5dc76`): the WebRTC DataChannel is the web game transport
> (`de_rtc_*` EM_JS shim in `runtime/net.h`), the relay reused unchanged as
> signaling only; play-tested Mac↔iPhone over wifi at LAN speed. The spike's two
> handshake potholes (below) are baked in. The measured ~70 ms wifi radio-sleep
> jitter exceeded the old 3-frame/50 ms cushion → visible 1-frame stalls, so
> `NET_DELAY` is bumped to a **fixed 10-frame (~165 ms)** cushion — playable, but
> a blunt instrument: **step 5 (adaptive `NET_DELAY`)** is the open refinement
> that keeps latency low on clean links — and a **native LAN session (2026-07-09,
> see §"Field evidence")** showed the fixed cushion is not just latency-suboptimal
> but genuinely inadequate on bad wifi, AND that step 5 needs a second half
> (re-centering, not only RTT-sizing). **Step 7 (TURN)** for the un-punchable
> ~10–20% is also still open (today they hit "connection failed - reload").
> Pairing is a Host/Join split (`build-site.js` gallery + `web_shell.html` bar),
> Join via native `prompt()` (an inline `<input>` is blocked by the running
> cart's key handlers on iOS). The `webrtc-spike/` page stays as a probe.

**Why 5b at all.** Rung 5a (WebSocket relay) *structurally cannot* be LAN-fast
from a **github.io** page: Pages is static (can't host a relay) and https (a
browser blocks plain `ws://`, so the relay must be a public `wss://` box — i.e.
internet-latency, always). See §"Hosting beyond the LAN". WebRTC removes the
relay from the *data* path entirely: peers connect **directly** — same wifi →
LAN (~12 ms), different networks → STUN hole-punch (direct across NAT), and only
the un-punchable minority (~10–20%, symmetric/CGNAT) falls back to a TURN relay.
So github.io stays the host, and couch co-op **and** play-across-town both work.

**The architecture: WebRTC is just a THIRD transport arm.** Everything above the
`net_transport_send`/`net_transport_pump` seam (lockstep core, barrier, input
ring, `NET_PKT_*`, seed handshake, `btn()` semantics) is reused verbatim — same
as the UDP↔WS↔echo split. **Crucially, the browser↔browser case needs NO
`libdatachannel`**: `RTCPeerConnection`/`RTCDataChannel` are built into every
browser, so the web arm is a thin `EM_JS` shim (`de_rtc_begin/state/send/recv`)
exactly parallel to the shipped `de_ws_*` shim. `libdatachannel` is only for a
*native* binary to speak WebRTC — out of scope here (see boundary).

**Signaling reuses the relay for free.** WebRTC peers must swap an SDP
offer/answer + ICE candidates before connecting. `net-relay.js` already
blind-forwards bytes within a room — so it carries the handshake untouched, then
the DataChannel opens and game traffic leaves it. **No new signaling server.**
Render's cold-start stops mattering (it only relays the ~1 s setup).

**Two potholes the spike surfaced (both fixes carry into the real design):**
1. **Offer-before-peer race.** The host fired its offer on being seated, but the
   joiner hadn't arrived, so the relay (no peer yet) dropped it. Fix: **joiner
   announces itself first** (a `ready` message), host offers only on hearing it —
   the same shape as the WS handshake's joiner-`HELLO` → host-`WELCOME`.
2. **The relay normalizes everything to binary** (`wsEncode` defaults to opcode
   2). Text signaling arrived as bytes and got dropped. Fix: **send signaling as
   binary**, tell it apart from the relay's `ROLE` packet by the `DN` magic (our
   JSON starts with `{`=123, never `D`=68). The real C shim is binary anyway.

**Bonus wins proven:** (a) `RTCDataChannel` configured `{ordered:false,
maxRetransmits:0}` (UDP-like) — a dropped packet no longer stalls for a TCP
retransmit, killing rung 5a's 0.3 fps head-of-line-blocking freeze mode; the
redundancy window refills it like native UDP. (b) **Safari-on-iOS connected over
plain http** — iOS is not the blocker feared in §5; https (github.io) is even
safer.

### The step table

| Step | What | Effort | Risk |
|---|---|---|---|
| **1. `de_rtc_*` EM_JS shim** ✅ DONE | Browser WebRTC as the 3rd transport arm: `RTCPeerConnection` + unreliable DataChannel, `begin/state/seat/send/recv` mirroring the WS shim; dispatch from the `net_transport_*` seam | ~2–3 days | low — spike proved the browser side |
| **2. Signaling over the existing relay** ✅ DONE | joiner-`ready` → host offer → answer → trickle ICE, all as binary through the room; roles from the existing `NET_PKT_ROLE` (seat 0 = host, no offer glare). Seed handshake (HELLO/WELCOME) rides the opened channel; self-heals over the unreliable channel | ~2 days | medium — trickle-ICE timing is the fiddly bit; spike de-risked it |
| **3. STUN config** ✅ DONE | one free public STUN URL (`stun:stun.l.google.com:19302`) in the peer config → cross-network hole-punch. Same-wifi doesn't need it | ~½ day | low |
| **4. Connection-state UX** ✅ DONE | wait screen shows connecting → waiting/joining → connected; Host/Join split (gallery + in-cart bar) fixes the 5a "both-clicked / wrong room" confusion (host shares link, joiner uses it) | ~1–2 days | low |
| **5. Adaptive + re-centering `NET_DELAY`** ⬅ NEXT | TWO halves, per the 2026-07-09 field evidence. **(a) Size** the input-delay buffer to cover live jitter (its input — the in-band PING/PONG RTT probe — shipped 2026-07-10 in `net.h`; `net_stat_rtt_ms`, ~2 Hz) — currently a **fixed 10-frame (~165 ms)** cushion covers ~70 ms spikes but adds latency on clean links; adaptive claws that back ("feels laggy vs. hitches" dial). **(b) Re-center:** after a burst drains the shared cushion, lockstep never rebuilds it — all slack ends up on one peer (buffer ~2×NET_DELAY) while the other is pinned at **buffer 0**, micro-stalling on every frame's jitter (measured: one peer stalled on **62 % of frames**). Sizing alone doesn't fix this; the fed peer must give a frame back so the starved peer rebuilds. This is the actual cure for the constant stutter. | ~1–2 days | low–med |
| **6. Desync tripwire** (opt) | per-frame CRC of the `de_state()` block; wasm↔wasm is bit-identical so it's insurance, reused from the netdemo gate | ~1 day | low |
| **7. TURN fallback** (opt) | free TURN (Cloudflare / Metered Open Relay) for the ~10–20% of pairs STUN can't punch — makes across-town play reliable for *everyone*. Same-wifi never touches it. Today those pairs hit "connection failed - reload" | ~½ day + a free account | low |

**Ballpark: ~1.5–2 weeks part-time** for browser↔browser; steps 5–7 are optional
polish/reach.

### Field evidence — native LAN session, 2026-07-09

First real two-machine play-test of the native path: Mac (editor host, P0) ↔ Windows
`.exe` (joiner, P1), pong, ~58 s (3504 frames), on office wifi. Captured with the new
net diagnostics (F2 overlay + `net-debug-P<seat>.log` + `--trace` `net` field — see
[`../guides/debug-harness.md`](../guides/debug-harness.md) §"Netplay diagnostics").
Findings — all on the **shared lockstep core**, so they apply to the WebRTC path too:

- **Not CPU-bound.** Host `perf.json`: 0.36 ms cart work / 16.67 ms frame, solid 60 fps.
  Every stall was a *network wait*, not compute. Rules out the machines.
- **Two distinct failures, both from `NET_DELAY` being a fixed, non-rebalancing cushion:**
  1. **Constant micro-stutter (the hidden one).** After the first burst drained the
     joiner's cushion, it sat at **buffer 0 for the rest of the match** — stalling on
     **2190 / 3514 frames (62 %)**, 1–8 ms each — while the host sat at buffer ~20.
     Lockstep never re-centered the slack. → motivates step 5's **(b) re-center** half.
  2. **Periodic 1–2 s freezes, ~every 18 s** (host frames 165/1296/2388/3453), each
     **correlated on both peers 11 frames apart** (= NET_DELAY+1 → the *same* event
     through the lockstep offset). Bidirectional → a shared-channel event; a follow-up
     `ping` + link-quality check (see §"the wifi-burst question") pinned it to **shared-AP
     airtime congestion** on a busy office network (pristine −39 dBm link, 300 ms+ RTT
     with zero loss), NOT a radio scan and NOT our code. **Un-bufferable:** a 60–120-frame
     gap can't be hidden by any cushion without adding seconds of input lag. Environmental
     ceiling both transports share.
- **Tooling gap → CLOSED (2026-07-10).** The stall recorder + log file used to live only
  in the native barrier (`net_frame_sync`, `DE_NET_BUILD`); the web tick now books its
  stalled ticks into the same counters (a run of stalled ticks = one stall with its real
  duration), so the F2 overlay reads the same on the WebRTC build — the prerequisite for
  watching step 5 work there. Same pass added the **wire-side instruments** this field
  session was missing: an in-band **RTT probe** (`NET_PKT_PING`/`PONG`, ~2 Hz — also
  step 5a's adaptive-`NET_DELAY` input), an **rx inter-arrival gap** counter (splits
  "wire went silent" from "cushion starvation" per stall), and **wall-clock stamps on
  every log line** (correlate a freeze with a concurrent `ping` by time of day — the
  §wifi-burst verdict below no longer has to be inferred from a separate-time ping).
  Formats: [`../guides/debug-harness.md`](../guides/debug-harness.md) §"Netplay diagnostics".

### The wifi-burst question — how to find the cause

The ~18 s / 1–2 s freezes are the part no netcode can hide, so they're worth
diagnosing directly. What the logs pin down: **bidirectional** loss (both peers waited
for each other at the same wall-clock moment), **regular** (~18 s, low variance), and
**seconds-long**. That combination rules out most things and points at a *shared
radio/channel* event, not congestion or a slow machine.

**Measured verdict (2026-07-09, office wifi "Floorplanner"): shared-AP airtime
congestion — NOT a radio scan, NOT our code.** The initial guess was a periodic
off-channel roaming scan, but the tests below refuted it:

- **The host's link is pristine**, so roaming/scan is off the table: 5 GHz **channel 100
  (40 MHz)**, **−39 dBm / −95 dBm** (SNR 56 dB — excellent), 360 Mbps 802.11ac. A radio
  this well-connected has no reason to hunt for a better AP.
- **A 3-min `ping` to the router with NO game running** was clean for ~100 s, then hit a
  **~40 s window of 100–374 ms RTT with ZERO packet loss**, then cleared. High-RTT +
  no-loss is the signature of **congestion / bufferbloat** (packets queuing, not
  dropping) — not a scan (which shows brief *losses*/gaps) and not our traffic (none was
  running). So the channel/AP was simply loaded for ~40 s.
- **The game-search code was also ruled out by reading it:** the host only broadcasts
  `net_announce` in the pre-game wait loop (`net.h`, `spins % 100`), which exits the
  instant a joiner connects; the joiner closes its discovery socket on join
  (`net_discover_end`, `studio.c`). No discovery/broadcast traffic runs during a match.

Conclusion: on a busy **shared office AP**, many clients contend for the same airtime, so
latency intermittently balloons to 300–400 ms even with a perfect signal. The game — LAN
lockstep over *two* wireless hops (host→AP→joiner) — freezes whenever a congestion window
lands. This is environmental and largely outside a corporate-AP user's control.

Diagnostic path used (built-in tools, no code — reusable next time):

- **Isolate game vs. environment:** `ping -i 0.2 <router-ip>` for a few minutes with **no
  game running**. Sustained high RTT with ~0 loss ⇒ congestion; brief losses/timeouts on
  a regular interval ⇒ a radio scan. (Ours was the former.)
- **Read the link quality:** `system_profiler SPAirPortDataType` (or ⌥-click the wifi
  menu → Wireless Diagnostics → Scan) for RSSI/channel/band. A strong RSSI (better than
  ~−60 dBm) rules out roaming scans; a crowded channel points at contention.
- **Find a local hog (if any):** `nettop -P` during a bad window — is *this* machine
  transferring, or is the congestion external (other clients / the AP)?

Remedies (all environmental — the engine can't hide 300 ms+ spikes): **wire the host via
ethernet** (removes one wireless hop and its contention), check/ improve the *joiner's*
link too (it's the second hop), move to a **less-loaded AP or channel**, or accept that a
busy shared AP will hitch. The engine-side lever (step 5 adaptive + re-centering
`NET_DELAY`) smooths small jitter and cures the constant stutter, but not congestion
spikes this large.

**Scope boundary.** This rung is **2 players, both in browsers/wasm**, direct P2P
with the relay as signaling only. It deliberately does **not** do: **native**
(clang binary) WebRTC (needs `libdatachannel` bundled — a separate rung);
**native↔browser** cross-play (needs the §"determinism risk" clang-vs-emcc proof);
or **>2 players** (§"player-count ceiling"). TURN (step 7) is the reach case, not
required for same-wifi. Verify each step with `tools/net-check.js`.

---

## Sources

Fact-checked (3-vote adversarial verification) unless marked otherwise.

- [emscripten docs]: https://emscripten.org/docs/porting/networking.html — no UDP in browsers; TCP emulation incomplete; no built-in WebRTC API
- [libdatachannel]: https://github.com/paullouisageneau/libdatachannel — C/C++ DataChannels, C bindings, ICE+STUN+TURN
- [datachannel-wasm]: https://github.com/paullouisageneau/datachannel-wasm — same API for wasm; Firefox/Chromium/Safari
- [Trystero]: https://github.com/dmotz/trystero — serverless signaling; P2P data after matchmaking
- [p2pcf]: https://github.com/gfodor/p2pcf — WebRTC signaling on Cloudflare Workers, minimal-message design
- [Cloudflare TURN]: https://developers.cloudflare.com/realtime/turn/ — managed TURN
- [Open Relay]: https://www.metered.ca/tools/openrelay/ — free TURN
- [libp2p study]: https://arxiv.org/pdf/2510.27500 — ~70% decentralized hole-punch success (Oct 2025 preprint)
- [pico8-net]: https://github.com/freds72/pico8-net — 5-byte button-state netplay; wrapped btn()
- [GGRS]: https://github.com/gschup/ggrs — GGPO reimplementation, wasm-capable
- [GGPO]: https://github.com/pond3r/ggpo/blob/master/doc/README.md — rollback + sync-test/checksum pattern
- [Godot docs]: https://docs.godotengine.org/en/stable/tutorials/networking/webrtc.html — WebRTC built-in for web export only
- [Gaffer]: https://gafferongames.com/post/floating_point_determinism/ — float determinism survey *(blog)*
- [randomascii]: https://randomascii.wordpress.com/2013/07/16/floating-point-determinism/ — what actually breaks determinism *(blog)*
- [Box2D]: https://box2d.org/posts/2024/08/determinism/ — shipping cross-platform determinism
- [yal.cc]: https://yal.cc/preparing-your-game-for-deterministic-netcode/ — practical determinism prep *(blog)*
- [snapnet]: https://www.snapnet.dev/blog/netcode-architectures-part-2-rollback/ — netcode architecture comparison *(blog)*
- [webrtcHacks]: https://webrtchacks.com/guide-to-safari-webrtc/ — Safari WebRTC status *(blog)*
- [tsoding]: https://github.com/tsoding/multiplayer-game-prototype — WebSocket client-server state-sync prototype (+ [video](https://www.youtube.com/watch?v=Ih9OkNeg7v8)) *(read directly, not fact-checked)*

Killed in verification: "WebRTC peers **require** an external signaling server"
— refuted (1-2): any out-of-band channel works, including manual copy-paste of
SDP; a server is a *UX* requirement for short join codes, not a protocol one.
