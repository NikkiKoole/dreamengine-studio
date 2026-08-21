/* de:meta
{
  "slug": "obplate",
  "title": "plate probe",
  "status": "active",
  "created": "2026-08-19",
  "kind": [
    "probe"
  ],
  "teaches": [
    "positional-audio"
  ],
  "lineage": "docs/design/analog-outboard-chain.md §6 named the stereo plate as the ONE place in the analog output chain where new DSP is justified, because width is the product and 'lush' is not reachable from a mono tank. This probe is the bench that reverb_plate() was voiced on: a single mallet hit every two seconds into a hard reverb send, so the file is almost all TAIL, plus a real goniometer so the width is something you SEE and not only something a gate reports.",
  "description": {
    "summary": "One mallet hit every two seconds, straight into a plate reverb, drawn on a vectorscope. A mono tank draws a vertical line down the middle of the screen. Switch the plate in and that line opens into a cloud, which is the whole point of a plate.",
    "detail": "The scope is a goniometer, the display a mastering engineer uses to look at stereo: mid (L+R) runs up the screen, side (L-R) runs across it. One signal in the middle of the mix has no side content at all, so it draws a vertical line, and that is exactly what the engine's reverb tank drew before reverb_plate() existed: the tank is mono, so the wet copy of every voice came back dead centre. With the plate in, the tank's two pickups read the same steel sheet at different distances and their DIFFERENCE becomes the side channel, so the cloud opens sideways as the WIDTH knob comes up. Only the difference is spread, so the mono fold is unchanged and nothing cancels on a phone speaker. The other three parts of the plate voicing are audible rather than visible here: a low cut (a steel sheet has no sub), a bright top (steel rings on), and dense diffusion in front of the tank so the hit arrives already smeared instead of as a discrete early echo, which is what makes a plate sit in a mix where a room fights it. Levels are deliberately low: the master soft-clip absorbs everything a mastering stage does if you feed it a mix that already peaks at 0 dBFS.",
    "controls": "SPACE switches the plate in and out, LEFT/RIGHT set the pickup spread, 1 fires a hit by hand, 2 swaps the route between the in-line insert and the master send; it plays itself otherwise"
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// PLATE PROBE — the bench reverb_plate() was voiced on, and a picture of what it buys.
//
// A plate reverb is a sheet of steel under tension: a driver pushes one point, and TWO PICKUPS
// somewhere else read what comes back. That is where the width comes from, and width is the whole
// reason a plate is the studio reverb. The engine's tank was mono, so every wet tail arrived dead
// centre; reverb_plate() gives the tank the two pickups (plus the low cut, the bright top and the
// dense diffusion that go with steel), and this cart draws the result on a goniometer so the
// difference is visible in one glance rather than argued about.
//
// MEASURE IT, don't just look:
//   node tools/ab-render.js obplate --set plate_amt=0.0f,1.0f --frames 480 --keep
//   node tools/stereo-check.js <the two wavs>            # corr / width / mono-fold, side by side
//   node tools/stereo-check.js <the plate wav> --expect decorrelated

#define SL 5                      // the one voice: a mallet, so every hit is a clean transient

static float plate_amt = 1.0f;    // ab-render flips THIS: 0 = the mono Schroeder tank, 1 = full plate
static float width     = 0.55f;   // pickup spread (reverb_plate_width)
static int   last_beat = -1;

// THE BYPASS CONTROL. 0 = never call reverb_plate() AT ALL, which is the only way to prove the claim
// that amount 0 is byte-identical to a build with no plate in it: flip this to 0, render, flip it
// back with plate_amt = 0.0f, render, and diff the two files sample by sample. A sha alone cannot
// answer the question, because it only ever says "differs".
static int plate_wired = 1;

// ROUTE. 0 = the ordinary master SEND, which is what a plate IS and what runtime/outboard.h's PLATE
// stage uses. 1 = the plate as an in-line INSERT at 85% wet, which is how you look at a reverb on a
// BENCH: with the dry out of the way the dish shows the tank's own output and nothing else. The two
// are DIFFERENT call sites inside the engine (the master send return, and the FX_REVERB insert), so
// the probe covers both.
static int route = 0;

// SET-AND-HOLD: the reverb voicing is reconfigured only by the code below, never per frame.
static void apply_voicing(void) {
    if (!plate_wired) return;
    reverb_plate(plate_amt);
    reverb_plate_width(width);
}

static void apply_route(void) {
    // fx_order is set UNCONDITIONALLY with the same chain either way — the trap recorded in
    // analog-outboard-chain.md §4: a chain assembled from only the enabled stages stops a bypass
    // being bit-exact.
    static const int chain[1] = { FX_REVERB };
    fx_order(0, chain, 1);
    reverb(0.80f, 0.25f);                          // long, and low damp: a plate's tail stays bright
    reverb_insert(0.80f, 0.25f, route == 1 ? 0.85f : 0.0f);
    instrument_reverb(SL, route == 1 ? 0.0f : 1.0f);
}

void init(void) {
    bpm(60);                                  // one hit every two beats = a two-second tail to look at
    instrument(SL, INSTR_MALLET, 1, 600, 0, 400);
    // HEADROOM (analog-outboard-chain.md §2c): the master soft-clip absorbs everything an output
    // stage does if the mix already peaks at 0 dBFS. A mastering bench on a clipped mix measures
    // nothing, so this probe deliberately runs quiet.
    instrument_level(SL, 1.0f);
    apply_route();
    apply_voicing();
    note(60, SL, 7);                           // strike on boot, so the dish has a tail in it from
                                               // frame one (the bake screenshots frame 3)
}

void update(void) {
    int b = beat();
    if (b != last_beat) {
        last_beat = b;
        if (b % 2 == 0) note(60 + (b % 8), SL, 7);
    }
    if (keyp('1')) note(67, SL, 7);
    if (keyp(' ')) { plate_amt = (plate_amt > 0.0f) ? 0.0f : 1.0f; apply_voicing(); }
    if (keyp(KEY_LEFT))  { width -= 0.1f; if (width < 0.0f) width = 0.0f; apply_voicing(); }
    if (keyp(KEY_RIGHT)) { width += 0.1f; if (width > 1.0f) width = 1.0f; apply_voicing(); }
    if (keyp('2')) { route = !route; apply_route(); }
#ifdef DE_TRACE
    watch("plate", "%.2f", plate_amt);
    watch("width", "%.2f", width);
    watch("route", "%d", route);
#endif
}

#define NS 2048
static float sl[NS], sr[NS];

// The goniometer PERSISTS, like a real one: the phosphor holds a fraction of a second, which is what
// turns 23 ms of samples into a readable SHAPE. Without it the dish shows three dots and a still
// frame says nothing at all.
#define GW 160
static unsigned char glow[GW * GW];
static float agc = 0.0f;   // the dish's auto gain (see draw)

void draw(void) {
    cls(CLR_BLACK);
    scope_read2(sl, sr, NS);

    // ── the goniometer: mid (L+R) up the screen, side (L-R) across it ──
    int cx = 100, cy = SCREEN_H / 2, rad = 78;
    int ox = cx - GW / 2, oy = cy - GW / 2;
    for (int i = 0; i < GW * GW; i++) glow[i] = (unsigned char)(glow[i] * 15 / 16);   // phosphor decay
    // AUTO GAIN, like the gain knob on a real goniometer. Not a nicety: the DRY hit is 20-30 dB louder
    // than the tail it leaves behind, so at any fixed scale you get a full-height line for 200 ms and
    // a single dot for the two seconds that actually carry the plate.
    float mx = 0.0001f;
    for (int i = 0; i < NS; i++) { float m = fabsf(sl[i] + sr[i]); if (m > mx) mx = m; }
    if (mx > agc) agc = mx; else agc += (mx - agc) * 0.12f;
    float g = (float)(rad - 4) * 0.85f / (agc > 0.0002f ? agc : 0.0002f);
    for (int i = 0; i < NS; i++) {
        int x = cx + (int)((sl[i] - sr[i]) * g * 2.2f);      // side is the small axis; open it out
        int y = cy - (int)((sl[i] + sr[i]) * g);
        // CLAMPED into the dish rather than dropped: a loud transient runs off the top, and dropping
        // those samples leaves the mono case looking like two dots instead of the vertical LINE it is.
        int gx = x - ox, gy = y - oy;
        if (gx < 1) gx = 1; if (gx > GW - 2) gx = GW - 2;
        if (gy < 1) gy = 1; if (gy > GW - 2) gy = GW - 2;
        int v = glow[gy * GW + gx] + 42; glow[gy * GW + gx] = (unsigned char)(v > 255 ? 255 : v);
    }
    circ(cx, cy, rad, CLR_DARKER_GREY);
    line(cx, cy - rad, cx, cy + rad, CLR_DARK_GREY);        // the mono axis: a centred mix sits here
    line(cx - rad, cy, cx + rad, cy, CLR_DARK_GREY);
    for (int gy = 0; gy < GW; gy++) for (int gx = 0; gx < GW; gx++) {
        int v = glow[gy * GW + gx];
        if (v < 24) continue;
        int dx = gx + ox - cx, dy = gy + oy - cy;
        if (dx * dx + dy * dy > rad * rad) continue;
        pset(gx + ox, gy + oy, v > 205 ? CLR_WHITE : (v > 85 ? CLR_LIME_GREEN : CLR_MEDIUM_GREEN));
    }
    print("MID", cx - 10, cy - rad - 10, CLR_DARK_GREY);
    print("SIDE", cx + rad - 22, cy + 4, CLR_DARK_GREY);

    // ── the readout ──
    int px = 196, py = 24;
    print("PLATE PROBE", px, py, CLR_WHITE); py += 14;
    char s[64];
    snprintf(s, sizeof s, "plate  %s", plate_amt > 0.0f ? "IN " : "OUT");
    print(s, px, py, plate_amt > 0.0f ? CLR_LIME_GREEN : CLR_MEDIUM_GREY); py += 10;
    snprintf(s, sizeof s, "width  %.2f", width);
    print(s, px, py, plate_amt > 0.0f ? CLR_WHITE : CLR_DARK_GREY); py += 18;

    font(FONT_SMALL);
    print(plate_amt > 0.0f ? "steel: two pickups," : "one mono tank:", px, py, CLR_LIGHT_GREY); py += 8;
    print(plate_amt > 0.0f ? "so the tail opens out" : "every tail dead centre", px, py, CLR_LIGHT_GREY); py += 8;
    print(plate_amt > 0.0f ? "sideways into a cloud" : "-- a vertical line", px, py, CLR_LIGHT_GREY); py += 14;
    print("SPACE plate  <> width", px, py, CLR_MEDIUM_GREY); py += 8;
    print("1 hit   2 route", px, py, CLR_MEDIUM_GREY); py += 8;
    print(route == 1 ? "route: INSERT 85% wet" : "route: master SEND", px, py, CLR_INDIGO);
    font(FONT_NORMAL);

    // the width knob, as a bar you can see move
    int bx = px, by = SCREEN_H - 30, bw = 108;
    rect(bx, by, bw, 7, CLR_DARK_GREY);
    if (plate_amt > 0.0f) rectfill(bx + 1, by + 1, (int)((bw - 2) * width), 5, CLR_ORANGE);
}
