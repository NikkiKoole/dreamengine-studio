// net.h — lockstep netplay. ONE lockstep core, per-platform transports
// (docs/design/multiplayer-research.md §"one lockstep core, two transports").
//
// Like sound.h this file is only compiled inside studio.c. Two build tiers
// (gated there, both excluded under DE_NO_RAYLIB):
//
//   DE_NET_CORE   the transport-agnostic lockstep core — input rings, packet
//                 format, frame barrier, echo loopback. Compiles for native
//                 AND web (no sockets anywhere in it).
//   DE_NET_BUILD  full native netplay on top of the core: UDP transport,
//                 --net-* flags, handshake, LAN discovery, boot lobby.
//                 Native Raylib build only (browsers have no UDP).
//
// Native runtime flags — a normal run touches none of this:
//
//   --net-host            host a 2-player session (waits for a joiner, then runs)
//   --net-join <ip>       join a host by IP, e.g. --net-join 127.0.0.1
//   --net-port <n>        UDP port (default 33445, both sides must match)
//   --net-echo            loopback fake peer: P2 mirrors P1 through the real
//                         ring/barrier path, no sockets (injection test/demo)
//
// How it works (input lockstep — see the design doc §4):
//   - Both sides run the SAME deterministic simulation (net implies --det:
//     fixed timestep + seeded rnd; the host's seed is sent in the handshake).
//   - Each frame, each side samples its LOCAL input (either keymap + touch —
//     all local input methods are "me"), packs it into one byte (bit = BTN_*),
//     and schedules it NET_DELAY frames in the future.
//   - The frame barrier (net_frame_try_sync / net_frame_sync) sends my byte and
//     waits until the peer's byte for the current frame has arrived; then
//     btn(0)/btn(1) read from the resolved lockstep bytes — local or remote,
//     the cart can't tell. Native BLOCKS in the barrier; web can't block (the
//     JS event loop must run to deliver messages), so loop_step retries
//     try_sync each tick and STALLS the whole tick (no sim, no sound) on false.
//   - Packets carry the last NET_REDUN frames of input (GGPO-style redundancy)
//     so a dropped datagram never stalls anything: the next packet refills it.
//   - Host is player 0, joiner is player 1. v1 syncs btn() ONLY — a cart that
//     reads key()/mouse_*() in update() will desync under netplay.
//
// Handshake (UDP; before the window opens; blocking, console-prompted):
//   joiner --HELLO--> host   (repeats every ~200 ms until answered)
//   host   --WELCOME{seed}--> joiner
//   then both enter the lockstep loop; frames 0..NET_DELAY-1 are pre-seeded
//   as "no buttons held" on both sides, so nobody waits until frame NET_DELAY.
//
// Exit: a closing side sends BYE (best-effort ×3); the peer prints "peer left"
// and exits cleanly. If the peer just vanishes (crash, unplugged), the barrier
// times out after NET_TIMEOUT_MS and exits nonzero.
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#ifdef DE_NET_BUILD
#ifdef _WIN32
  // winsock2.h / ws2tcpip.h are included at the TOP of studio.c (before raylib.h,
  // which pulls windows.h) — they MUST precede windows.h or the old winsock v1 in
  // windows.h collides. Here we only need the compat shims.
  #define NET_CLOSE(s)     closesocket(s)
  #define NET_SLEEP_MS(ms) Sleep(ms)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <ifaddrs.h>
  #include <net/if.h>
  #define NET_CLOSE(s)     close(s)
  #define NET_SLEEP_MS(ms) usleep((ms) * 1000)
#endif
#endif // DE_NET_BUILD

#define NET_PORT_DEFAULT 33445
#define NET_DISCOVERY_PORT 33446 // LAN broadcast port for "Open to LAN" auto-discovery (rung 3)
#define NET_DELAY        3     // input latency in frames (~50 ms at 60 fps)
#define NET_RING         128   // per-player ring of input bytes (≫ max peer drift)
#define NET_REDUN        8     // input frames per packet (drop-proofing redundancy)
#define NET_TIMEOUT_MS   10000 // barrier gives up after this long without the peer

// packet types (byte 2, after the "DN" magic)
#define NET_PKT_HELLO    1     // joiner → host: "I'm here" (from-address = peer)
#define NET_PKT_WELCOME  2     // host → joiner: + u32 seed (LE)
#define NET_PKT_INPUT    3     // + u32 first-frame (LE), u8 count, count input bytes
#define NET_PKT_BYE      4     // sender is quitting
#define NET_PKT_ANNOUNCE 5     // host → LAN broadcast: "a game is here" + u16 game-port (LE)

// ── lockstep core state (transport-agnostic — native AND web) ────────────────
static bool net_active    = false;   // handshake done, lockstep running
static bool net_is_host   = false;
static bool net_echo      = false;   // loopback fake-peer transport (see net_echo_start)
static int  net_me        = 0;       // my player index: 0 = host, 1 = joiner
static bool net_peer_bye  = false;
static unsigned net_seed_v = 1;      // host: seed to send; joiner: seed received

static uint8_t net_ring_bits[2][NET_RING];    // [player][frame % NET_RING] input byte
static int     net_ring_frame[2][NET_RING];   // which frame the slot holds (-1 = empty)
static uint8_t net_bits[2];                   // resolved lockstep inputs for the current frame
static int     net_frame = 0;                 // lockstep sim frame counter
static int     net_sampled_frame = -1;        // sim frame whose local input is already sampled+sent
static int     net_try_spins = 0;             // barrier attempts for the current frame (resend cadence)

// btn()'s pre-net body — reads THIS machine's keymaps + touch (defined in studio.c)
static bool btn_local(int player, int button);

// the ONE seam that differs per platform (design doc §5a): UDP (native),
// loopback echo, and — rung 5a step 2 — WebSocket relay (web). Dispatchers
// are defined at the bottom of this file, after every transport arm.
static void net_transport_send(const void *buf, int len);
static void net_transport_pump(void);

static void net_store(int player, int frame, uint8_t bits) {
    int s = frame % NET_RING;
    if (net_ring_frame[player][s] < frame) {  // never regress a slot (stale resends)
        net_ring_frame[player][s] = frame;
        net_ring_bits[player][s]  = bits;
    }
}

static bool net_have(int player, int frame) {
    return net_ring_frame[player][frame % NET_RING] == frame;
}

static uint8_t net_get(int player, int frame) {
    return net_ring_bits[player][frame % NET_RING];
}

// send my most recent inputs up to and including `latest` (NET_REDUN-frame window)
static void net_send_inputs(int latest) {
    int n = NET_REDUN, f0 = latest - NET_REDUN + 1;
    if (f0 < 0) { f0 = 0; n = latest + 1; }
    uint8_t pkt[8 + NET_REDUN] = { 'D', 'N', NET_PKT_INPUT,
                                   (uint8_t)f0, (uint8_t)(f0 >> 8),
                                   (uint8_t)(f0 >> 16), (uint8_t)(f0 >> 24), (uint8_t)n };
    for (int i = 0; i < n; i++) pkt[8 + i] = net_get(net_me, f0 + i);
    net_transport_send(pkt, 8 + n);
}

// reset the lockstep state for a fresh session: empty rings, frames
// 0..NET_DELAY-1 pre-seeded as "no buttons held" on both sides (so nobody
// waits until frame NET_DELAY), frame counter back to 0.
static void net_core_reset(void) {
    memset(net_ring_frame, -1, sizeof net_ring_frame);
    for (int p = 0; p < 2; p++)
        for (int f = 0; f < NET_DELAY; f++)
            net_store(p, f, 0);
    net_frame = 0;
    net_sampled_frame = -1;
    net_try_spins = 0;
    net_bits[0] = net_bits[1] = 0;
    net_peer_bye = false;
}

// echo transport — a loopback fake peer: every INPUT packet I send is stored
// straight back as the OTHER player's input, so P2 mirrors P1 through the real
// pack → send → store → barrier → btn() path with no sockets anywhere. This is
// the transport-agnosticism proof for rung 5a step 1, and the cheapest
// remote-input-injection oracle on both targets (native --net-echo, web
// -DDE_NET_ECHO_DEFAULT). No handshake: the seed stays local.
static void net_echo_start(void) {
    net_echo    = true;
    net_is_host = true;
    net_me      = 0;
    net_core_reset();
    net_active  = true;
}

// Non-blocking barrier attempt. First call for the current sim frame samples
// local input (both keymaps + touch are "me") and sends it NET_DELAY frames
// ahead; every call pumps the transport and re-sends on a ~30-attempt cadence
// (our packet may have dropped). Returns false while the peer's byte for this
// frame is missing — the caller must NOT advance the sim (web loop_step stalls
// the whole tick; native wraps this in the blocking loop below). On true,
// net_bits[] holds both players' resolved bytes and net_frame has advanced.
static bool net_frame_try_sync(void) {
#ifdef PLATFORM_WEB
    if (net_peer_bye) {   // web can't exit() a tab: drop to solo (btn() falls back to btn_local)
        printf("net: peer left — continuing solo\n");
        net_active = false;
        return true;
    }
#endif
    int f = net_frame;
    if (net_sampled_frame != f) {
        uint8_t mine = 0;
        for (int b = 0; b < 8; b++)                       // every local input method is "me":
            if (btn_local(0, b) || btn_local(1, b))       // either keymap, or the touch overlay
                mine |= (uint8_t)(1u << b);
        net_store(net_me, f + NET_DELAY, mine);
        net_sampled_frame = f;
        net_try_spins = 0;
        net_send_inputs(f + NET_DELAY);
    } else if (++net_try_spins % 30 == 0) {
        net_send_inputs(f + NET_DELAY);                   // our packet may have dropped
    }

    net_transport_pump();
    int peer = net_me ^ 1;
    if (!net_have(peer, f)) return false;

    net_transport_pump();   // opportunistic drain (future frames land now, not at the next barrier)
    net_bits[0] = net_get(0, f);
    net_bits[1] = net_get(1, f);
    net_frame   = f + 1;
    net_try_spins = 0;
    return true;
}

// ── UDP transport + native-only netplay (rungs 1–3) ──────────────────────────
#ifdef DE_NET_BUILD

static bool net_requested = false;   // a --net-host/--net-join flag was passed (main() checks this)
static bool net_lobby_requested = false;  // --net-lobby (or DE_NET_LOBBY_DEFAULT): show the boot menu
static const char *net_join_ip = NULL;
static int  net_port      = NET_PORT_DEFAULT;

static int                net_sock = -1;
static int                net_disco_sock = -1;   // rung 3: joiner's LAN-discovery listen socket
static struct sockaddr_in net_peer;
static bool               net_peer_set   = false;
static bool               net_got_welcome = false;

static void net_sendto(const void *buf, int len) {
    if (net_peer_set)
        sendto(net_sock, buf, (size_t)len, 0, (struct sockaddr *)&net_peer, sizeof net_peer);
}

// drain every pending datagram; handles handshake + input + bye
static void net_pump(void) {
    uint8_t buf[64];
    struct sockaddr_in from;
    for (;;) {
        socklen_t fl = sizeof from;
        int n = (int)recvfrom(net_sock, (char *)buf, (int)sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (n < 3 || buf[0] != 'D' || buf[1] != 'N') { if (n < 0) break; else continue; }
        switch (buf[2]) {
            case NET_PKT_HELLO:                    // host: learn the peer, answer (idempotent —
                if (net_is_host) {                 // a lost WELCOME just gets re-asked)
                    net_peer = from; net_peer_set = true;
                    uint8_t w[7] = { 'D', 'N', NET_PKT_WELCOME,
                                     (uint8_t)net_seed_v, (uint8_t)(net_seed_v >> 8),
                                     (uint8_t)(net_seed_v >> 16), (uint8_t)(net_seed_v >> 24) };
                    net_sendto(w, sizeof w);
                }
                break;
            case NET_PKT_WELCOME:
                if (!net_is_host && n >= 7) {
                    net_seed_v = (unsigned)buf[3] | (unsigned)buf[4] << 8
                               | (unsigned)buf[5] << 16 | (unsigned)buf[6] << 24;
                    net_got_welcome = true;
                }
                break;
            case NET_PKT_INPUT:
                if (n >= 8) {
                    int f0  = buf[3] | buf[4] << 8 | buf[5] << 16 | buf[6] << 24;
                    int cnt = buf[7];
                    if (f0 >= 0 && 8 + cnt <= n)
                        for (int i = 0; i < cnt; i++) net_store(net_me ^ 1, f0 + i, buf[8 + i]);
                }
                break;
            case NET_PKT_BYE:
                net_peer_bye = true;
                break;
        }
    }
}

static void net_shutdown(void) {
    if (net_sock < 0) return;
    uint8_t bye[3] = { 'D', 'N', NET_PKT_BYE };
    for (int i = 0; i < 3; i++) net_sendto(bye, sizeof bye);  // best-effort (UDP)
    NET_CLOSE(net_sock);
    net_sock = -1;
    net_active = false;
}

// Windows needs Winsock started before any socket call; no-op everywhere else.
static void net_platform_init(void) {
#ifdef _WIN32
    static bool started = false;
    if (!started) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); started = true; }
#endif
}

// Best-guess LAN IPv4 for THIS machine, so the host can tell the joiner what to
// type (--net-join <ip>). Prefers a private-range address (192.168.x / 10.x),
// skips loopback + link-local (169.254); falls back to the first other
// non-loopback IPv4, else "127.0.0.1" (localhost play still works). Writes into
// out (size n). rung 2 — docs/design/multiplayer-research.md.
static void net_local_ipv4(char *out, size_t n) {
    net_platform_init();
    snprintf(out, n, "127.0.0.1");
#ifdef _WIN32
    // No getifaddrs on Windows. The UDP "connect" trick: connecting a datagram
    // socket sends NOTHING, but getsockname() then reports the local address the
    // OS would route out — i.e. the active LAN IP — with no GetAdaptersAddresses.
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return;
    struct sockaddr_in a = { 0 };
    a.sin_family = AF_INET;
    a.sin_port   = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &a.sin_addr);           // a routable target; no packet is sent
    if (connect(s, (struct sockaddr *)&a, sizeof a) == 0) {
        struct sockaddr_in me; socklen_t ml = sizeof me;
        if (getsockname(s, (struct sockaddr *)&me, &ml) == 0)
            inet_ntop(AF_INET, &me.sin_addr, out, n);
    }
    closesocket(s);
#else
    struct ifaddrs *ifs = NULL;
    if (getifaddrs(&ifs) != 0) return;
    char first[INET_ADDRSTRLEN] = { 0 };
    for (struct ifaddrs *a = ifs; a; a = a->ifa_next) {
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
        if (!(a->ifa_flags & IFF_UP) || (a->ifa_flags & IFF_LOOPBACK)) continue;
        char ip[INET_ADDRSTRLEN];
        struct sockaddr_in *sin = (struct sockaddr_in *)a->ifa_addr;
        inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof ip);
        if (strncmp(ip, "169.254.", 8) == 0) continue;      // link-local (no DHCP) — useless to a joiner
        if (!first[0]) snprintf(first, sizeof first, "%s", ip);
        if (strncmp(ip, "192.168.", 8) == 0 || strncmp(ip, "10.", 3) == 0) {  // the home/classroom LAN
            snprintf(out, n, "%s", ip); freeifaddrs(ifs); return;
        }
    }
    if (first[0]) snprintf(out, n, "%s", first);            // e.g. a 172.16/12 net, or a lone public IP
    freeifaddrs(ifs);
#endif
}

// ── rung 3: LAN auto-discovery (UDP broadcast) — docs/design/multiplayer-research.md
// Host shouts "a game is here on port N" to the whole subnet so a joiner can find
// it without typing an IP (Minecraft-style "Open to LAN"). Sent on the game
// socket with SO_BROADCAST enabled (see net_handshake).
static void net_announce(void) {
    struct sockaddr_in b;
    memset(&b, 0, sizeof b);
    b.sin_family      = AF_INET;
    b.sin_port        = htons(NET_DISCOVERY_PORT);
    b.sin_addr.s_addr = htonl(INADDR_BROADCAST);          // 255.255.255.255
    uint8_t pkt[5] = { 'D', 'N', NET_PKT_ANNOUNCE, (uint8_t)net_port, (uint8_t)(net_port >> 8) };
    sendto(net_sock, (const char *)pkt, sizeof pkt, 0, (struct sockaddr *)&b, sizeof b);
}

// Joiner: open a socket listening for host announces. SO_REUSEADDR so it can
// coexist with a host bound to the same box (localhost testing).
static void net_discover_begin(void) {
    net_platform_init();
    net_disco_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (net_disco_sock < 0) return;
    int on = 1;
    setsockopt(net_disco_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof on);
    struct sockaddr_in me;
    memset(&me, 0, sizeof me);
    me.sin_family      = AF_INET;
    me.sin_addr.s_addr = htonl(INADDR_ANY);
    me.sin_port        = htons(NET_DISCOVERY_PORT);
    if (bind(net_disco_sock, (struct sockaddr *)&me, sizeof me) < 0) { NET_CLOSE(net_disco_sock); net_disco_sock = -1; return; }
#ifdef _WIN32
    { u_long nb = 1; ioctlsocket(net_disco_sock, FIONBIO, &nb); }
#else
    fcntl(net_disco_sock, F_SETFL, O_NONBLOCK);
#endif
}

// Poll for a host announce; on one, write its IP (the datagram's source) to out
// and return true. Non-blocking — returns false when nothing's waiting.
static bool net_discover_poll(char *out, size_t n) {
    if (net_disco_sock < 0) return false;
    uint8_t buf[16];
    struct sockaddr_in from;
    for (;;) {
        socklen_t fl = sizeof from;
        int r = (int)recvfrom(net_disco_sock, (char *)buf, (int)sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (r < 5) { if (r < 0) return false; else continue; }
        if (buf[0] == 'D' && buf[1] == 'N' && buf[2] == NET_PKT_ANNOUNCE) {
            inet_ntop(AF_INET, &from.sin_addr, out, (socklen_t)n);
            return true;
        }
    }
}

static void net_discover_end(void) {
    if (net_disco_sock >= 0) { NET_CLOSE(net_disco_sock); net_disco_sock = -1; }
}

// blocking handshake, called from main() BEFORE the window opens. On the
// joiner, *seed is overwritten with the host's seed (both sims must roll the
// same rnd() stream).
static void net_handshake(unsigned *seed) {
    net_platform_init();
    net_me = net_is_host ? 0 : 1;
    net_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (net_sock < 0) { fprintf(stderr, "net: socket() failed\n"); exit(1); }
    struct sockaddr_in me = { 0 };
    me.sin_family      = AF_INET;
    me.sin_addr.s_addr = htonl(INADDR_ANY);
    me.sin_port        = htons(net_is_host ? (uint16_t)net_port : 0);
    if (bind(net_sock, (struct sockaddr *)&me, sizeof me) < 0) {
        fprintf(stderr, "net: bind failed — port %d already in use? (another host running?)\n", net_port);
        exit(1);
    }
#ifdef _WIN32
    { u_long nb = 1; ioctlsocket(net_sock, FIONBIO, &nb); }   // non-blocking
#else
    fcntl(net_sock, F_SETFL, O_NONBLOCK);
#endif
    net_core_reset();
    net_seed_v = *seed;

    if (net_is_host) {
        char ip[INET_ADDRSTRLEN];
        net_local_ipv4(ip, sizeof ip);
        printf("net: HOSTING on %s:%d — the joiner runs: --net-join %s\n", ip, net_port, ip);
        printf("net: waiting for a joiner...\n");
        fflush(stdout);
        int bcast = 1;                                          // enable LAN-discovery broadcast (rung 3)
        setsockopt(net_sock, SOL_SOCKET, SO_BROADCAST, (const char *)&bcast, sizeof bcast);
        int spins = 0;
        while (!net_peer_set) {                                 // HELLO answered inside net_pump
            net_pump();
            if (spins % 100 == 0) net_announce();               // shout "here I am" ~1×/s while waiting
            spins++;
            NET_SLEEP_MS(10);
        }
        printf("net: joiner connected — you are P1 (host)\n");
    } else {
        memset(&net_peer, 0, sizeof net_peer);
        net_peer.sin_family = AF_INET;
        net_peer.sin_port   = htons((uint16_t)net_port);
        if (inet_pton(AF_INET, net_join_ip, &net_peer.sin_addr) != 1) {
            fprintf(stderr, "net: bad ip '%s' (numeric IPv4 only, e.g. 127.0.0.1)\n", net_join_ip);
            exit(1);
        }
        net_peer_set = true;
        printf("net: joining %s:%d ...\n", net_join_ip, net_port);
        fflush(stdout);
        uint8_t hello[3] = { 'D', 'N', NET_PKT_HELLO };
        int waited_ms = 0;
        while (!net_got_welcome) {
            net_sendto(hello, sizeof hello);
            for (int i = 0; i < 20 && !net_got_welcome; i++) { net_pump(); NET_SLEEP_MS(10); }
            waited_ms += 200;
            if (waited_ms >= 30000) { fprintf(stderr, "net: no answer from host after 30s\n"); exit(1); }
        }
        *seed = net_seed_v;
        printf("net: connected — you are P2 (seed %u from host)\n", *seed);
    }
    fflush(stdout);
    net_active = true;
}

// the per-frame lockstep barrier — called from loop_step() right before the
// btn_curr edge snapshot. The blocking native wrapper around net_frame_try_sync:
// spins the transport until the peer's byte arrives (echo resolves on the first
// try), with the rung-1 timeout + "peer left" exit semantics.
static void net_frame_sync(void) {
    int f = net_frame, spins = 0;
    while (!net_frame_try_sync()) {
        if (net_peer_bye) break;
        NET_SLEEP_MS(1);
        if (++spins == 2000) { printf("net: waiting for peer (frame %d)...\n", f); fflush(stdout); }
        if (spins >= NET_TIMEOUT_MS) {
            fprintf(stderr, "net: peer timed out at frame %d\n", f);
            net_shutdown();
            exit(1);
        }
    }
    if (net_peer_bye) { printf("net: peer left — exiting\n"); net_shutdown(); exit(0); }
}

#endif // DE_NET_BUILD

// ── the transport dispatchers (the seam itself) ──────────────────────────────
// Echo short-circuits with no I/O; native falls through to UDP. Rung 5a step 2
// adds the web WebSocket arm here (and nowhere else).
static void net_transport_send(const void *buf, int len) {
    if (net_echo) {
        const uint8_t *b = (const uint8_t *)buf;
        if (len >= 8 && b[2] == NET_PKT_INPUT) {          // reflect my inputs as the peer's
            int f0  = b[3] | b[4] << 8 | b[5] << 16 | b[6] << 24;
            int cnt = b[7];
            if (f0 >= 0 && 8 + cnt <= len)
                for (int i = 0; i < cnt; i++) net_store(net_me ^ 1, f0 + i, b[8 + i]);
        }
        return;                                           // BYE/handshake: nothing to tell ourselves
    }
#ifdef DE_NET_BUILD
    net_sendto(buf, len);
#else
    (void)buf; (void)len;
#endif
}

static void net_transport_pump(void) {
    if (net_echo) return;                                 // echo "delivers" inside send
#ifdef DE_NET_BUILD
    net_pump();
#endif
}
