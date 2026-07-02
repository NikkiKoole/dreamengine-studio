// net.h — lockstep netplay, rung 1: two native builds exchange one byte of
// btn() bits per frame over UDP (localhost or LAN by IP). Design + rung ladder:
// docs/design/multiplayer-research.md.
//
// Like sound.h this file is only compiled inside studio.c, and only for the
// native Raylib build (studio.c gates the include on !PLATFORM_WEB &&
// !DE_NO_RAYLIB). Activated at runtime by CLI flags — a normal run touches
// none of this:
//
//   --net-host            host a 2-player session (waits for a joiner, then runs)
//   --net-join <ip>       join a host by IP, e.g. --net-join 127.0.0.1
//   --net-port <n>        UDP port (default 33445, both sides must match)
//
// How it works (input lockstep — see the design doc §4):
//   - Both sides run the SAME deterministic simulation (net implies --det:
//     fixed timestep + seeded rnd; the host's seed is sent in the handshake).
//   - Each frame, each side samples its LOCAL input (either keymap + touch —
//     all local input methods are "me"), packs it into one byte (bit = BTN_*),
//     and schedules it NET_DELAY frames in the future.
//   - The frame barrier (net_frame_sync) sends my byte and blocks until the
//     peer's byte for the current frame has arrived; then btn(0)/btn(1) read
//     from the resolved lockstep bytes — local or remote, the cart can't tell.
//   - Packets carry the last NET_REDUN frames of input (GGPO-style redundancy)
//     so a dropped datagram never stalls anything: the next packet refills it.
//   - Host is player 0, joiner is player 1. v1 syncs btn() ONLY — a cart that
//     reads key()/mouse_*() in update() will desync under netplay.
//
// Handshake (before the window opens; blocking, console-prompted):
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ifaddrs.h>
#include <net/if.h>

#define NET_PORT_DEFAULT 33445
#define NET_DELAY        3     // input latency in frames (~50 ms at 60 fps)
#define NET_RING         128   // per-player ring of input bytes (≫ max peer drift)
#define NET_REDUN        8     // input frames per packet (drop-proofing redundancy)
#define NET_TIMEOUT_MS   10000 // barrier gives up after this long without the peer

// packet types (byte 2, after the "DN" magic)
#define NET_PKT_HELLO   1      // joiner → host: "I'm here" (from-address = peer)
#define NET_PKT_WELCOME 2      // host → joiner: + u32 seed (LE)
#define NET_PKT_INPUT   3      // + u32 first-frame (LE), u8 count, count input bytes
#define NET_PKT_BYE     4      // sender is quitting

static bool net_requested = false;   // a --net-* flag was passed (main() checks this)
static bool net_active    = false;   // handshake done, lockstep running
static bool net_is_host   = false;
static int  net_me        = 0;       // my player index: 0 = host, 1 = joiner
static const char *net_join_ip = NULL;
static int  net_port      = NET_PORT_DEFAULT;

static int                net_sock = -1;
static struct sockaddr_in net_peer;
static bool               net_peer_set   = false;
static bool               net_got_welcome = false;
static bool               net_peer_bye    = false;
static unsigned           net_seed_v      = 1;   // host: seed to send; joiner: seed received

static uint8_t net_ring_bits[2][NET_RING];    // [player][frame % NET_RING] input byte
static int     net_ring_frame[2][NET_RING];   // which frame the slot holds (-1 = empty)
static uint8_t net_bits[2];                   // resolved lockstep inputs for the current frame
static int     net_frame = 0;                 // lockstep sim frame counter

// btn()'s pre-net body — reads THIS machine's keymaps + touch (defined in studio.c)
static bool btn_local(int player, int button);

static void net_sendto(const void *buf, int len) {
    if (net_peer_set)
        sendto(net_sock, buf, (size_t)len, 0, (struct sockaddr *)&net_peer, sizeof net_peer);
}

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

// drain every pending datagram; handles handshake + input + bye
static void net_pump(void) {
    uint8_t buf[64];
    struct sockaddr_in from;
    for (;;) {
        socklen_t fl = sizeof from;
        ssize_t n = recvfrom(net_sock, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
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

// send my most recent inputs up to and including `latest` (NET_REDUN-frame window)
static void net_send_inputs(int latest) {
    int n = NET_REDUN, f0 = latest - NET_REDUN + 1;
    if (f0 < 0) { f0 = 0; n = latest + 1; }
    uint8_t pkt[8 + NET_REDUN] = { 'D', 'N', NET_PKT_INPUT,
                                   (uint8_t)f0, (uint8_t)(f0 >> 8),
                                   (uint8_t)(f0 >> 16), (uint8_t)(f0 >> 24), (uint8_t)n };
    for (int i = 0; i < n; i++) pkt[8 + i] = net_get(net_me, f0 + i);
    net_sendto(pkt, 8 + n);
}

static void net_shutdown(void) {
    if (net_sock < 0) return;
    uint8_t bye[3] = { 'D', 'N', NET_PKT_BYE };
    for (int i = 0; i < 3; i++) net_sendto(bye, sizeof bye);  // best-effort (UDP)
    close(net_sock);
    net_sock = -1;
    net_active = false;
}

// Best-guess LAN IPv4 for THIS machine, so the host can tell the joiner what to
// type (--net-join <ip>). Prefers a private-range address (192.168.x / 10.x),
// skips loopback + link-local (169.254); falls back to the first other
// non-loopback IPv4, else "127.0.0.1" (localhost play still works). Writes into
// out (size n). rung 2 — docs/design/multiplayer-research.md.
static void net_local_ipv4(char *out, size_t n) {
    snprintf(out, n, "127.0.0.1");
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
}

// blocking handshake, called from main() BEFORE the window opens. On the
// joiner, *seed is overwritten with the host's seed (both sims must roll the
// same rnd() stream).
static void net_handshake(unsigned *seed) {
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
    fcntl(net_sock, F_SETFL, O_NONBLOCK);
    memset(net_ring_frame, -1, sizeof net_ring_frame);
    for (int p = 0; p < 2; p++)                 // nobody holds a button before frame 0's
        for (int f = 0; f < NET_DELAY; f++)     // input can arrive — pre-seed the gap
            net_store(p, f, 0);
    net_seed_v = *seed;

    if (net_is_host) {
        char ip[INET_ADDRSTRLEN];
        net_local_ipv4(ip, sizeof ip);
        printf("net: HOSTING on %s:%d — the joiner runs: --net-join %s\n", ip, net_port, ip);
        printf("net: waiting for a joiner...\n");
        fflush(stdout);
        while (!net_peer_set) { net_pump(); usleep(10000); }  // HELLO answered inside net_pump
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
            for (int i = 0; i < 20 && !net_got_welcome; i++) { net_pump(); usleep(10000); }
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
// btn_curr edge snapshot. Samples local input, exchanges it with the peer, and
// resolves both players' bytes for this frame (btn() reads net_bits under net).
static void net_frame_sync(void) {
    int f = net_frame++;
    uint8_t mine = 0;
    for (int b = 0; b < 8; b++)                       // every local input method is "me":
        if (btn_local(0, b) || btn_local(1, b))       // either keymap, or the touch overlay
            mine |= (uint8_t)(1u << b);
    net_store(net_me, f + NET_DELAY, mine);
    net_send_inputs(f + NET_DELAY);

    int peer = net_me ^ 1, spins = 0;
    while (!net_have(peer, f)) {                      // barrier: wait for the peer's byte
        net_pump();
        if (net_peer_bye) {
            printf("net: peer left — exiting\n");
            net_shutdown();
            exit(0);
        }
        if (net_have(peer, f)) break;
        usleep(1000);
        if (++spins % 30 == 0) net_send_inputs(f + NET_DELAY);   // our packet may have dropped
        if (spins == 2000) { printf("net: waiting for peer (frame %d)...\n", f); fflush(stdout); }
        if (spins >= NET_TIMEOUT_MS) {
            fprintf(stderr, "net: peer timed out at frame %d\n", f);
            net_shutdown();
            exit(1);
        }
    }
    net_pump();   // opportunistic drain (future frames land now, not at the next barrier)
    net_bits[0] = net_get(0, f);
    net_bits[1] = net_get(1, f);
    if (net_peer_bye) { printf("net: peer left — exiting\n"); net_shutdown(); exit(0); }
}
