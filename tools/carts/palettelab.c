#include "studio.h"

// PALETTE LAB — try on a new default palette without touching anything else.
// The Layer-1 probe from docs/design/palette-and-color.md: our 32 colors are
// lifted verbatim from PICO-8, and before blend tables (STATUS #18) bake that
// in deeper, candidates need to be SEEN against the corpus's worst cases.
//
// Built on the EXPERIMENTAL palette_hex(i, 0xRRGGBB) — it writes the live
// palette slot, so every scene below is drawn with normal CLR_* indices and
// re-skins instantly when the palette changes. Sprite editor, cart format,
// docs: all untouched. That's the point of the experiment tier.
//
// The hard part this cart makes visible: a candidate isn't 32 pretty hexes,
// it's a ROLE MAPPING — slot 8 must still be "the red", slot 1 "the dark
// blue", ramps must still ramp, or every existing cart scrambles. Slots where
// the mapping is weak (E32 has no lime, no warm dark-grey family — see the
// dup/approx comments in the tables) are exactly the evidence being gathered.
//
// CAVEAT while a custom palette is active: pal(c0,c1) would inject the
// SHIPPED c1 color (base_palette is the matching key) — scenes here avoid pal().
//
// CONTROLS: 1 PICO-8 (shipped) · 2 ENDESGA 32 · 3 RESURRECT-32 cut ·
// LEFT/RIGHT switch scene (swatches+ramps / sunset / night glow / portrait).

// ---- candidate palettes, ROLE-MAPPED to the 32 slots -------------------
// ENDESGA 32 (by ENDESGA, lospec.com/palette-list/endesga-32), reordered to
// pico roles. Weak fits flagged: 18 dups 16 (no second dark plum), 17 dups 1
// (no near-black navy pair), 26 dups 11 (E32 has no lime).
static const int PAL_E32[32] = {
    0x181425, 0x262b44, 0x68386c, 0x265c42, 0xbe4a2f, 0x3a4466, 0xc0cbdc, 0xffffff,
    0xff0044, 0xf77622, 0xfee761, 0x63c74d, 0x0099db, 0x8b9bb4, 0xf6757a, 0xe8b796,
    0x3e2731, 0x262b44, 0x3e2731, 0x193c3e, 0x733e39, 0x5a6988, 0xc28569, 0xead4aa,
    0xa22633, 0xd77643, 0x63c74d, 0x3e8948, 0x124e89, 0xb55088, 0xe43b44, 0xe4a672,
};

// RESURRECT 64 (by Kerrie Lake, lospec.com/palette-list/resurrect-64), a
// 32-color cut onto pico roles — long ramps mean almost every role has a
// genuine fit; that's the curated-64 advantage made visible.
static const int PAL_R32[32] = {
    0x2e222f, 0x484a77, 0x6b3e75, 0x165a4c, 0x9e4539, 0x3e3546, 0x9babb2, 0xffffff,
    0xe83b3b, 0xfb6b1d, 0xf9c22b, 0x1ebc73, 0x4d9be6, 0x7f708a, 0xed8099, 0xfdcbb0,
    0x45293f, 0x323353, 0x753c54, 0x0b5e65, 0x6e2727, 0x625565, 0xab947a, 0xfbff86,
    0xae2334, 0xcd683d, 0xd5e04b, 0x239063, 0x4d65b4, 0xa24b6f, 0xf68181, 0xfca790,
};

static int cur_pal;     // 0 = shipped PICO-8, 1 = E32, 2 = R32
static int scene;       // 0..3
#define NSCENES 4

static void apply_palette(int which) {
    cur_pal = which;
    if (which == 0) { pal_reset(); return; }
    const int *p = which == 1 ? PAL_E32 : PAL_R32;
    for (int i = 0; i < 32; i++) palette_hex(i, p[i]);
}

void init(void) {
    apply_palette(1);    // boot on a candidate so the experiment is visible
    scene = 1;
}

void update(void) {
    if (keyp('1')) apply_palette(0);
    if (keyp('2')) apply_palette(1);
    if (keyp('3')) apply_palette(2);
    if (keyp(KEY_RIGHT)) scene = (scene + 1) % NSCENES;
    if (keyp(KEY_LEFT))  scene = (scene + NSCENES - 1) % NSCENES;
}

// ---- scene 0: swatches + dithered ramps --------------------------------
// the ramps are the load-bearing test: gradient()/vgradient() and the whole
// dither-fake school assume perceptually adjacent steps exist.
static void scene_swatches(void) {
    cls(CLR_BLACK);
    for (int i = 0; i < 32; i++) {
        int x = 8 + (i % 16) * 19, y = 22 + (i / 16) * 26;
        rectfill(x, y, 17, 17, i);
        font(FONT_TINY); print(str("%d", i), x + 1, y + 19, CLR_LIGHT_GREY); font(FONT_NORMAL);
    }
    // corpus ramps, drawn as continuous dithered runs (hgradient pair by pair)
    static const int ramps[5][6] = {
        { 0, 1, 12, 6, 7, -1 },          // night sky -> day
        { 16, 4, 9, 10, 23, -1 },        // embers / warm
        { 19, 3, 27, 11, 26, -1 },       // greens
        { 0, 21, 5, 22, 15, -1 },        // greys -> warm light
        { 16, 20, 30, 15, 31, -1 },      // skin / peach
    };
    static const char *names[5] = { "SKY", "WARM", "GREEN", "GREY", "SKIN" };
    for (int r = 0; r < 5; r++) {
        int y = 86 + r * 20;
        font(FONT_SMALL); print(names[r], 8, y + 3, CLR_LIGHT_GREY); font(FONT_NORMAL);
        int x = 44;
        for (int i = 0; i < 5 && ramps[r][i + 1] >= 0; i++) {
            hgradient(x, y, 53, 12, ramps[r][i], ramps[r][i + 1]);
            x += 53;
        }
    }
}

// ---- scene 1: sunset — gradients + dither glow + fog -------------------
static void scene_sunset(void) {
    vgradient(0, 0, SCREEN_W, 60, CLR_DARK_BLUE, CLR_DARK_PURPLE);     // high sky
    vgradient(0, 60, SCREEN_W, 50, CLR_DARK_PURPLE, CLR_ORANGE);       // sunset band
    vgradient(0, 110, SCREEN_W, 20, CLR_ORANGE, CLR_YELLOW);           // horizon glow
    // sun + dither halo (the additive-glow fake)
    fillp(FILL_DOTS, -1);    circfill(240, 112, 26, CLR_YELLOW);    fillp_reset();
    fillp(FILL_CHECKER, -1); circfill(240, 112, 18, CLR_LIGHT_YELLOW); fillp_reset();
    circfill(240, 112, 11, CLR_WHITE);
    // sea
    vgradient(0, 130, SCREEN_W, 50, CLR_BLUE, CLR_DARK_BLUE);
    fillp(FILL_HLINES, -1); rectfill(0, 134, SCREEN_W, 30, CLR_LIGHT_YELLOW); fillp_reset();
    // far hills + fog band over them
    for (int x = 0; x < SCREEN_W; x += 4) {
        int h = 18 + (int)(noise(x * 0.02f) * 22);
        rectfill(x, 130 - h, 4, h, CLR_DARK_GREEN);
    }
    fillp(FILL_CHECKER, -1); rectfill(0, 104, SCREEN_W, 26, CLR_LIGHT_GREY); fillp_reset();
    // beach
    vgradient(0, 180, SCREEN_W, 20, CLR_PEACH, CLR_BROWN);
}

// ---- scene 2: night glow — the galerijflat "lights gap" ----------------
static void scene_night(void) {
    vgradient(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK, CLR_DARKER_BLUE);
    // building
    rectfill(20, 60, 120, 130, CLR_DARKER_GREY);
    for (int wy = 0; wy < 4; wy++)
        for (int wx = 0; wx < 5; wx++) {
            int lit = (wx * 7 + wy * 3) % 3 != 0;
            rectfill(30 + wx * 22, 70 + wy * 30, 12, 16, lit ? CLR_LIGHT_YELLOW : CLR_DARKER_BLUE);
            if (lit) { fillp(FILL_CHECKER, -1); rect(29 + wx * 22, 69 + wy * 30, 14, 18, CLR_YELLOW); fillp_reset(); }
        }
    // streetlamps: concentric dither halos — what blend tables would do for real
    for (int i = 0; i < 3; i++) {
        int lx = 180 + i * 55;
        line(lx, 130, lx, 188, CLR_DARK_GREY);
        fillp(FILL_DOTS, -1);    circfill(lx, 128, 22, CLR_ORANGE);       fillp_reset();
        fillp(FILL_CHECKER, -1); circfill(lx, 128, 13, CLR_LIGHT_YELLOW); fillp_reset();
        circfill(lx, 128, 4, CLR_WHITE);
    }
    rectfill(0, 188, SCREEN_W, 12, CLR_DARKER_GREY);
    // neon sign
    print_outline("NEON", 226, 60, CLR_PINK, CLR_DARK_PURPLE);
    print_outline("BAR",  238, 72, CLR_GREEN, CLR_BLUE_GREEN);
}

// ---- scene 3: portrait — skin tones are where palettes get cruel -------
static void scene_portrait(void) {
    vgradient(0, 0, SCREEN_W, SCREEN_H, CLR_INDIGO, CLR_DARKER_PURPLE);
    int cx = 160, cy = 96;
    ovalfill(cx, cy + 78, 52, 36, CLR_TRUE_BLUE);                 // shirt
    rectfill(cx - 9, cy + 34, 18, 16, CLR_LIGHT_PEACH);           // neck
    ovalfill(cx, cy, 40, 46, CLR_LIGHT_PEACH);                    // head
    ovalfill(cx, cy - 28, 42, 24, CLR_BROWN);                     // hair
    ovalfill(cx - 30, cy - 10, 12, 22, CLR_BROWN);
    ovalfill(cx + 30, cy - 10, 12, 22, CLR_BROWN);
    fillp(FILL_CHECKER, -1); ovalfill(cx - 14, cy + 18, 9, 6, CLR_DARK_PEACH); fillp_reset();  // blush
    fillp(FILL_CHECKER, -1); ovalfill(cx + 14, cy + 18, 9, 6, CLR_DARK_PEACH); fillp_reset();
    ovalfill(cx - 14, cy - 2, 6, 4, CLR_WHITE);  pset(cx - 13, cy - 2, CLR_BROWNISH_BLACK);   // eyes
    ovalfill(cx + 14, cy - 2, 6, 4, CLR_WHITE);  pset(cx + 15, cy - 2, CLR_BROWNISH_BLACK);
    ovalfill(cx, cy + 26, 8, 3, CLR_DARK_PEACH);                  // mouth
    // shading: the dithered cheek/jaw falloff every face cart fakes
    fillp(FILL_CHECKER, -1); ovalfill(cx + 18, cy + 8, 18, 30, CLR_PEACH); fillp_reset();
    // swatch strip of the skin family for direct judgement
    static const int skin[6] = { 16, 20, 4, 30, 15, 31 };
    for (int i = 0; i < 6; i++) rectfill(8 + i * 16, 168, 14, 14, skin[i]);
    font(FONT_SMALL); print("SKIN FAMILY 16 20 4 30 15 31", 8, 184, CLR_LIGHT_GREY); font(FONT_NORMAL);
}

void draw(void) {
    switch (scene) {
        case 0: scene_swatches(); break;
        case 1: scene_sunset();   break;
        case 2: scene_night();    break;
        case 3: scene_portrait(); break;
    }
    // HUD
    static const char *pnames[3] = { "1 PICO-8 (shipped)", "2 ENDESGA 32", "3 RESURRECT-32 CUT" };
    static const char *snames[NSCENES] = { "SWATCHES+RAMPS", "SUNSET", "NIGHT GLOW", "PORTRAIT" };
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(pnames[cur_pal], 4, 2, CLR_WHITE);
    print_right(str("%s  %d/%d", snames[scene], scene + 1, NSCENES), 316, 2, CLR_LIGHT_GREY);
    font(FONT_SMALL);
    rectfill(0, 193, SCREEN_W, 7, CLR_BLACK);
    print("1/2/3 palette   LEFT/RIGHT scene   same indices, new clothes", 4, 194, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
