/* de:meta
{
  "slug": "palettelab",
  "title": "palette lab",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [
    "dithering-gradient",
    "palette-cycling"
  ],
  "lineage": "An extension of the sibling blendlab cart: ports its scene-function architecture to run against live swappable candidate palettes (11 candidates: 32-, 42- and 64-slot, incl. DB32 = Aseprite's default), so AVG/ADD/MUL blend tables and dither scenes are re-evaluated against each candidate — the probe that drives the palette-expansion design decision.",
  "description": "Try on a new default palette and judge it where it matters most: BLENDING. This is blendlab x palettes — the Layer-1/1b probe from palette-and-color.md with blendlab's scene-function architecture ported in, so every blend runs against the LIVE candidate. Keys 1-9, 0 and A swap the palette (or click the top HUD bar to cycle - left-click next, right-click back): shipped PICO-8 / ENDESGA 32 / full RESURRECT 64 / E32 + 32 derived in-betweens (sRGB ramp midpoints + hue bridges, computed in-cart) / ENDESGA 64 (the same author's own curated expansion — candidate 4 vs 5 is generate-vs-curate head to head) / AAP-64 (the other canonical long-ramp 64) / FAMICUBE (the opposite philosophy: a fictional console's identity palette, distinct hues over dense ramps — watch its glows ring in the wrong family) / JOURNEY (the painterly school: violet-cast darks, ramps that bend through hue — even its black is a midnight blue) / JEHKOBA64 (the lattice school: hue-family ramps of five, every hue given the same value treatment — does a systematic grid blend more predictably than hand curation?) / ROSY 42 (key 0, PineappleOnPizza's 42-colour painterly set - 32 role-mapped + 10 leftovers, pal_n=42, no neutral grey) / DB32 (key A, DawnBringer 32 = Aseprite's default new-sprite palette and the de-facto pixel-art standard - the most important baseline to beat, and the first candidate with a real neutral-grey ramp). LEFT/RIGHT walk six scenes: the 64-swatch grid + five dithered corpus ramps, a sunset, a portrait (skin tones are where palettes get cruel), the NIGHT GLOW street with real additive streetlights (D toggles today's fillp fake next to it; carry a glow on the mouse), GLASS+FOG (translucent pane on the mouse, C cycles its color, drifting MUL cloud shadows, an AVG fog band), and the raw BLEND TABLE grid (T cycles AVG/ADD/MUL). The AVG/ADD/MUL tables rebuild from the candidate's RGB on every switch — more/denser colors visibly band less. Note the dither scenes only reference slots 0-31, so candidates 2 and 4 are identical there by construction; the blend scenes are where 32 vs 64 separates. Built on the EXPERIMENTAL palette_hex() + the 64-slot palette.",
  "todo": [
    "ui-audit?: the bottom control-hint line runs past the right edge (clipped) — low-confidence, may be intentional; see action-plan \"control-hint overflow\"."
  ]
}
de:meta */
#include "studio.h"

// PALETTE LAB — try on a new default palette without touching anything else,
// and judge it where it matters most: BLENDING. This is blendlab x palettes —
// the Layer-1/1b probe from docs/design/palette-and-color.md, with blendlab's
// scene-function architecture ported in so every blend runs against the LIVE
// candidate. Two probes, one verdict machine: a palette is only as good as
// what `nearest(mix(a,b))` can land on.
//
// Built on the EXPERIMENTAL palette_hex(i, 0xRRGGBB) and the 64-slot palette
// (slots 32-63 mirror 0-31 by default, so nothing else changed). Scenes draw
// with normal CLR_* indices and re-skin instantly on palette switch; the blend
// tables (AVG/ADD/MUL, blendlab's trio) are rebuilt from the candidate's RGB
// on every switch — candidates with more/denser colors visibly band less.
//
// Eleven candidates (keys 1-9, 0, A — or click the top HUD bar to cycle:
// left-click next, right-click back):
//   1  PICO-8 (shipped)     — the baseline we're trying to replace
//   2  ENDESGA 32           — 32 role-mapped, upper half mirrors
//   3  RESURRECT 64 (full)  — 32 role-mapped + the other 32 in slots 32-63
//   4  E32 + 32 DERIVED     — the "blended in-betweens" idea: slots 32-63 are
//      sRGB midpoints of ramp neighbours + hue bridges, computed here
//      (OKLab mixing would be kinder — judging that muddiness is the point)
//   5  ENDESGA 64           — E32's big sibling by the same author: candidate 4
//      vs 5 is GENERATE vs CURATE with the same taste, head to head
//   6  AAP-64               — the other canonical long-ramp 64 (vs Resurrect)
//   7  FAMICUBE             — the opposite philosophy: a fictional CONSOLE's
//      identity palette — distinct hues over dense ramps. Tests whether dense
//      ramps are even the right goal for a fantasy console.
//   8  JOURNEY              — the painterly school: everything violet-tinted,
//      ramps that bend through hue instead of just lightness. Tests whether a
//      strong unified CAST (vs neutral coverage) suits a whole console.
//   9  JEHKOBA64            — the lattice school: hue-family ramps of five,
//      every hue given the same value treatment. Tests whether a SYSTEMATIC
//      grid blends more predictably than hand-curated ramps.
//   0  ROSY 42              — PineappleOnPizza's 42-colour painterly set (same
//      author as JOURNEY). The odd-count case: 32 role-mapped + 10 leftovers in
//      32-41, pal_n=42. No neutral grey (slots 5/6 borrow mauve/blue-grey).
//   A  DB32 (DawnBringer 32) — ASEPRITE'S DEFAULT + the de-facto pixel-art
//      standard, so the most important baseline to beat. Clean 32 role-map with
//      a real grey ramp (where Rosy/Journey had none).
//
// NOTE the derived/upper colors only show where something SAMPLES them — the
// dither scenes (sunset/portrait) reference indices 0-31 and are identical for
// candidates 2 and 4 by construction. The blend scenes are where 32 vs 64
// separates; the D toggle puts today's fillp fake next to the real thing.
//
// Role-mapping notes: slot 8 must stay "the red", ramps must still ramp, or
// every existing cart scrambles. Weak fits are flagged in the tables (E32 has
// no lime, no second dark plum, no near-black navy pair — three dup slots);
// the Resurrect cut filled every role. That asymmetry is already a finding.
//
// CAVEAT while a custom palette is active: pal(c0,c1) would inject the SHIPPED
// c1 color (base_palette stays the texel-matching key) — scenes avoid pal().
//
// CONTROLS: 1-9 palette · LEFT/RIGHT scene · D dither-fake vs real blend ·
// C glass presets, MOUSE WHEEL scrubs the pane through every live slot (incl.
// the upper 32) · T cycle AVG/ADD/MUL in the table view · mouse = glow/pane.

// ---- the shipped palette, for the blend tables + completeness -----------
static const int PAL_PICO[32] = {
    0x000000, 0x1d2b53, 0x7e2553, 0x008751, 0xab5236, 0x5f574f, 0xc2c3c7, 0xfff1e8,
    0xff004d, 0xffa300, 0xffec27, 0x00e436, 0x29adff, 0x83769c, 0xff77a8, 0xffccaa,
    0x291814, 0x111d35, 0x422136, 0x125359, 0x742f29, 0x49333b, 0xa28879, 0xf3ef7d,
    0xbe1250, 0xff6c24, 0xa8e72e, 0x00b543, 0x065ab5, 0x754665, 0xff6e59, 0xff9d81,
};

// ENDESGA 32 (by ENDESGA, lospec.com/palette-list/endesga-32), pico-role order.
// Weak fits: 17 dups 1, 18 dups 16, 26 dups 11.
static const int PAL_E32[32] = {
    0x181425, 0x262b44, 0x68386c, 0x265c42, 0xbe4a2f, 0x3a4466, 0xc0cbdc, 0xffffff,
    0xff0044, 0xf77622, 0xfee761, 0x63c74d, 0x0099db, 0x8b9bb4, 0xf6757a, 0xe8b796,
    0x3e2731, 0x262b44, 0x3e2731, 0x193c3e, 0x733e39, 0x5a6988, 0xc28569, 0xead4aa,
    0xa22633, 0xd77643, 0x63c74d, 0x3e8948, 0x124e89, 0xb55088, 0xe43b44, 0xe4a672,
};

// RESURRECT 64 (by Kerrie Lake, lospec.com/palette-list/resurrect-64):
// 32 role-mapped to the pico slots + the remaining 32 in the upper half.
static const int PAL_R32[32] = {
    0x2e222f, 0x484a77, 0x6b3e75, 0x165a4c, 0x9e4539, 0x3e3546, 0x9babb2, 0xffffff,
    0xe83b3b, 0xfb6b1d, 0xf9c22b, 0x1ebc73, 0x4d9be6, 0x7f708a, 0xed8099, 0xfdcbb0,
    0x45293f, 0x323353, 0x753c54, 0x0b5e65, 0x6e2727, 0x625565, 0xab947a, 0xfbff86,
    0xae2334, 0xcd683d, 0xd5e04b, 0x239063, 0x4d65b4, 0xa24b6f, 0xf68181, 0xfca790,
};
static const int PAL_R64X[32] = {   // the other 32 of Resurrect 64
    0x966c6c, 0x694f62, 0xc7dcd0, 0xb33831, 0xea4f36, 0xf57d4a, 0xf79617, 0x7a3045,
    0xe6904e, 0xfbb954, 0x4c3e24, 0x676633, 0xa2a947, 0x91db69, 0xcddf6c, 0x313638,
    0x374e4a, 0x547e64, 0x92a984, 0xb2ba90, 0x0b8a8f, 0x0eaf9b, 0x30e1b9, 0x8ff8e2,
    0x8fd3ff, 0x905ea9, 0xa884f3, 0xeaaded, 0xcf657f, 0x831c5d, 0xc32454, 0xf04f78,
};

// ENDESGA 64 (by ENDESGA, lospec.com/palette-list/endesga-64), pico-role order
// + the other 32 upper. The same author's own curated expansion of E32.
static const int PAL_E64[32] = {
    0x131313, 0x2a2f4e, 0x622461, 0x1e6f50, 0x8a4836, 0x5d5d5d, 0xb4b4b4, 0xffffff,
    0xff0040, 0xffa214, 0xffeb57, 0x5ac54f, 0x0098dc, 0x657392, 0xf389f5, 0xf6ca9f,
    0x1c121c, 0x1a1932, 0x3b1443, 0x134c4c, 0x5d2c28, 0x3d3d3d, 0x858585, 0xf9e6cf,
    0xc42430, 0xed7614, 0x99e65f, 0x33984b, 0x0069aa, 0x93388f, 0xf5555d, 0xf68187,
};
static const int PAL_E64X[32] = {
    0x1b1b1b, 0x272727, 0xc7cfdd, 0x92a1b9, 0x424c6e, 0x0e071b, 0x391f21, 0xbf6f4a,
    0xe69c69, 0xedab50, 0xe07438, 0xc64524, 0x8e251d, 0xff5000, 0xffc825, 0xd3fc7e,
    0x0c2e44, 0x00396d, 0x00cdf9, 0x0cf1ff, 0x94fdff, 0xfdd2ed, 0xdb3ffd, 0x7a09fa,
    0x3003d9, 0x0c0293, 0x03193f, 0xca52c9, 0xc85086, 0xea323c, 0x891e2b, 0x571c27,
};

// AAP-64 (by Adigun A. Polack, lospec.com/palette-list/aap-64), pico-role order
// + the other 32 upper. The other canonical long-ramp 64.
static const int PAL_A64[32] = {
    0x060608, 0x143464, 0x73172d, 0x1a7a3e, 0xbb7547, 0x5a4e44, 0xb3b9d1, 0xffffff,
    0xb4202a, 0xfa6a0a, 0xffd541, 0x14a02e, 0x249fde, 0x8b93af, 0xbc4a9b, 0xfad6b8,
    0x141013, 0x242234, 0x422433, 0x23674e, 0x71413b, 0x333941, 0xa08662, 0xfef3c0,
    0x3b1725, 0xf9a31b, 0x9cdb43, 0x328464, 0x285cc4, 0x8e5252, 0xe86a73, 0xf5a097,
};
static const int PAL_A64X[32] = {
    0xdf3e23, 0xfffc40, 0xd6f264, 0x59c135, 0x24523b, 0x122020, 0x20d6c7, 0xa6fcdb,
    0x793a80, 0x403353, 0x221c1a, 0x322b28, 0xdba463, 0xf4d29c, 0xdae0ea, 0x6d758d,
    0x4a5462, 0x5b3138, 0xba756a, 0xe9b5a3, 0xe3e6ff, 0xb9bffb, 0x849be4, 0x588dbe,
    0x477d85, 0x5daf8d, 0x92dcba, 0xcdf7e2, 0xe4d2aa, 0xc7b08b, 0x796755, 0x423934,
};

// FAMICUBE (by Arne Niklas Jansson), pico-role order + the other 32 upper.
// Designed as a fictional console's identity palette — distinct hues over ramps.
static const int PAL_FC[32] = {
    0x000000, 0x00177d, 0x871646, 0x004e00, 0xae6c37, 0x343434, 0xa8a8a8, 0xffffff,
    0xe03c28, 0xf68f37, 0xffe737, 0x58d332, 0x5ba8ff, 0x9ba0ef, 0xff82ce, 0xf5b784,
    0x231712, 0x0d2030, 0x211640, 0x00604b, 0x5c3c0d, 0x151515, 0xc59782, 0xeeffa9,
    0xcf3c71, 0xad4e1a, 0x8cd612, 0x20b562, 0x024aca, 0x823c3d, 0xda655e, 0xe18289,
};
static const int PAL_FCX[32] = {
    0xd7d7d7, 0x7b7b7b, 0x415d66, 0x71a6a1, 0xbdffca, 0x25e2cd, 0x0a98ac, 0x005280,
    0x139d08, 0x172808, 0x376d03, 0x6ab417, 0xbeeb71, 0xb6c121, 0x939717, 0xcc8f15,
    0xffbb31, 0xe2d7b5, 0x4f1507, 0xffe9c5, 0xa328b3, 0xcc69e4, 0xd59cfc, 0xfec9ed,
    0xe2c9ff, 0xa675fe, 0x6a31ca, 0x5a1991, 0x3d34a5, 0x6264dc, 0x98dcff, 0x0a89ff,
};

// JOURNEY (by PineappleOnPizza, lospec.com/palette-list/journey), pico-role
// order + the other 32 upper. The painterly school: violet-cast darks, ramps
// that bend through hue — note even "black" (slot 0) is a midnight blue.
static const int PAL_J64[32] = {
    0x050914, 0x132243, 0x691749, 0x429058, 0xbd4035, 0x5b537d, 0xc7d4e1, 0xfaffff,
    0xd41e3c, 0xed7b39, 0xfff540, 0x3dff6e, 0x488bd4, 0x928fb8, 0xff417d, 0xffcf8e,
    0x24142c, 0x0e0f2c, 0x3d1132, 0x153c4a, 0x691b22, 0x392946, 0xcf968c, 0xf8ffb8,
    0x9b0e3e, 0xed4c40, 0xc6d831, 0x28c074, 0x144491, 0x8f5765, 0xd46453, 0xf5a15d,
};
static const int PAL_J64X[32] = {
    0x110524, 0x3b063a, 0x9c3247, 0xff7a7d, 0xd61a88, 0x94007a, 0x42004e, 0x220029,
    0x100726, 0x25082c, 0x73263d, 0xffb84a, 0x77b02a, 0x2c645e, 0x052137, 0x0e0421,
    0x0c0b42, 0x032769, 0x78d7ff, 0xb0fff1, 0x1a466b, 0x10908e, 0xf0c297, 0x52294b,
    0x0f022e, 0x35003b, 0x64004c, 0xff9757, 0xd4662f, 0x9c341a, 0x450c28, 0x2d002e,
};

// ROSY 42 (by PineappleOnPizza, lospec.com/palette-list/rosy-42): a 42-COLOUR
// palette — odd fit for the 32/64 model, so it's role-mapped to the pico slots
// (0-31) with the 10 leftover colours parked in 32-41 and pal_n = 42 (the cart
// already runs the blend tables at any pal_n). Same painterly author as JOURNEY;
// warm earth + pink/wine ramps and NO neutral grey — so slots 5/6 borrow a
// mauve / blue-grey (weak fit, flagged), and the dither scenes (which only touch
// 0-31) won't show the upper 10. PAL_ROSY is laid out in final slot order 0-41.
static const int PAL_ROSY[42] = {
    0x14182e, 0x2c354d, 0x4b1d52, 0x3b7d4f, 0xab5130, 0x52333f, 0xa3a7c2, 0xf5ffe8,  //  0-7  black,dkblue,dkpurple,dkgreen,brown,dkgrey~,ltgrey~,white
    0xff5277, 0xff8933, 0xffee83, 0x63ab3f, 0x4fa4b8, 0x686f99, 0xcc2f7b, 0xffc2a1,  //  8-15 red,orange,yellow,green,blue,lavender,pink,skin
    0x21181b, 0x283540, 0x4f1d4c, 0x2f5753, 0x7d3833, 0x3d2936, 0xbd6a62, 0xf0b541,  // 16-23 nearblk,dknavy,dkplum,dkteal,brick,dkmauve,clay,ltyellow
    0xad2f45, 0xcf752b, 0xc8d45d, 0x92e8c0, 0x4c6885, 0x8f4d57, 0xe64539, 0xffae70,  // 24-31 dkred,burntorange,lime,mint,medblue,mauve,coral,ltskin
    0x3b2027, 0x1b1f21, 0x2b2b45, 0x3a3f5e, 0xdfe0e8, 0x404973, 0x692464, 0x9c2a70,  // 32-39 the 10 leftovers
    0x781d4f, 0x291d2b,                                                              // 40-41
};

// JEHKOBA64 (by Jehkoba, lospec.com/palette-list/jehkoba64), pico-role order
// + the other 32 upper. The lattice school: hue-family ramps of five, every
// hue given the same five-step value treatment — a systematic grid.
static const int PAL_K64[32] = {
    0x000000, 0x243966, 0x852264, 0x068051, 0x9e4c4c, 0x696570, 0xc4bbb3, 0xf2f2da,
    0xf53141, 0xfaa032, 0xfad937, 0x55b33b, 0x25acf5, 0x807980, 0xeb758f, 0xfabbaf,
    0x472e3e, 0x0d2140, 0x4e278c, 0x116061, 0x875d58, 0x495169, 0xb58c7f, 0xccc73d,
    0xc40c2e, 0xf2621f, 0x94bf30, 0x179c43, 0x195ba6, 0x6e4250, 0xff7070, 0xfa9891,
};
static const int PAL_K64X[32] = {
    0xd94c8e, 0xb32d7d, 0xf58122, 0xdb4b16, 0xffb938, 0xe69b22, 0xcc8029, 0xad6a45,
    0xb3b02d, 0x989c27, 0x8c8024, 0x7a5e37, 0xa0eba8, 0x7ccf9a, 0x5cb888, 0x3da17e,
    0x20806c, 0x49c2f2, 0x1793e6, 0x1c75bd, 0xae88e3, 0x7e7ef2, 0x586ac4, 0x3553a6,
    0xe29bfa, 0xca7ef2, 0xa35dd9, 0x773bbf, 0x9e7767, 0xa69a9c, 0x050e1a, 0xd9a798,
};

// DB32 / DAWNBRINGER 32 (by DawnBringer, lospec.com/palette-list/dawnbringer-32)
// — ASEPRITE'S DEFAULT new-sprite palette and the de-facto pixel-art standard,
// so the single most important baseline to test PICO-8 against. A clean 32-into-32
// role-map (no dups): DB32 has a real neutral GREY ramp (slots 5/6/21 land
// properly, unlike Rosy/Journey) + full hue ramps. Weak fits in the upper half:
// no teal (19 borrows a blue), no light-coral/extra-mauve (29/30/31 borrow
// olive/grey/light-blue) — DB32 is leaner in the warm-light corner than pico's
// upper ramp. pal_n = 32 (upper half mirrors, like ENDESGA 32).
static const int PAL_DB32[32] = {
    0x000000, 0x222034, 0x45283c, 0x37946e, 0x8f563b, 0x595652, 0x9badb7, 0xffffff,  //  0-7  black,dkblue,dkpurple,dkgreen,brown,GREY,LTGREY,white
    0xac3232, 0xdf7126, 0xfbf236, 0x6abe30, 0x639bff, 0x5b6ee1, 0xd77bba, 0xeec39a,  //  8-15 red,orange,yellow,green,blue,indigo,pink,skin
    0x323c39, 0x3f3f74, 0x76428a, 0x306082, 0x663931, 0x696a6a, 0xd9a066, 0x8f974a,  // 16-23 dkslate,dknavy,purple,blue~teal,brick,grey,tan,olive~ltyellow
    0xd95763, 0x8a6f30, 0x99e550, 0x4b692f, 0x5fcde4, 0x847e87, 0xcbdbfc, 0x524b24,  // 24-31 dkred,gold,lime,medgreen,cyan,grey,ltblue,olive
};

// candidate 4: E32 + derived in-betweens — midpoint pairs (ramp neighbours +
// hue bridges so cross-ramp blends have somewhere to land)
static const int MIX_PAIRS[32][2] = {
    {0,1},{1,12},{12,6},{6,7},        // sky
    {16,4},{4,9},{9,10},{10,23},      // warm
    {19,3},{3,27},{27,11},{11,26},    // green
    {0,21},{21,5},{5,22},{22,15},     // grey -> warm light
    {16,20},{20,30},{30,15},{15,31},  // skin
    {8,24},{24,2},{2,29},{29,14},     // reds/purples
    {1,28},{28,12},{13,5},{14,15},    // blues + misc
    {8,12},{9,12},{11,12},{10,7},     // hue bridges (glow-over-water class)
};

#define NPAL 11
static int cur_pal;        // 0 pico, 1 e32, 2 r64, 3 e32+derived, 4 e64, 5 aap64, 6 famicube, 7 journey, 8 jehkoba, 9 rosy42, 10 db32
static int pal_n;          // how many DISTINCT colors the candidate brings (32 or 64)
static int cur_hex[64];    // active palette as hexes — feeds the blend tables
static int scene;
#define NSCENES 6
static int dither_fake;    // D — draw the blend shapes the way carts fake them today
static int glass_i;        // C — jump between glass color presets
static int glass_c = CLR_WHITE;  // the pane's actual color — WHEEL scrubs it through every live slot
static int table_i;        // T — which table the grid shows

static const int  GLASS_CLR[6] = { CLR_RED, CLR_TRUE_BLUE, CLR_YELLOW, CLR_MEDIUM_GREEN, CLR_WHITE, CLR_BLACK };
static const char *GLASS_NM[6] = { "red", "blue", "yellow", "green", "white", "black" };

// ---- blend tables, built from the LIVE candidate (blendlab's trio) ------
static unsigned char t_avg[64][64];   // (s+d)/2       — translucency / glass / shadow
static unsigned char t_add[64][64];   // min(s+d,255)  — additive glow / light
static unsigned char t_mul[64][64];   // s*d/255       — darken / fog / tint

static int mix_hex(int a, int b) {   // plain sRGB midpoint (OKLab would be kinder)
    return ((((a >> 16 & 255) + (b >> 16 & 255)) / 2) << 16) |
           ((((a >> 8  & 255) + (b >> 8  & 255)) / 2) << 8)  |
            (((a       & 255) + (b       & 255)) / 2);
}

static int nearest_idx(int r, int g, int b) {
    int best = 0; long bd = 0x7fffffff;
    for (int i = 0; i < pal_n; i++) {
        int dr = r - (cur_hex[i] >> 16 & 255), dg = g - (cur_hex[i] >> 8 & 255), db = b - (cur_hex[i] & 255);
        long d = 2L*dr*dr + 4L*dg*dg + 3L*db*db;   // rough luma weighting
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void build_tables(void) {
    for (int s = 0; s < pal_n; s++)
        for (int d = 0; d < pal_n; d++) {
            int sr = cur_hex[s] >> 16 & 255, sg = cur_hex[s] >> 8 & 255, sb = cur_hex[s] & 255;
            int dr = cur_hex[d] >> 16 & 255, dg = cur_hex[d] >> 8 & 255, db = cur_hex[d] & 255;
            t_avg[s][d] = (unsigned char)nearest_idx((sr+dr)/2, (sg+dg)/2, (sb+db)/2);
            t_add[s][d] = (unsigned char)nearest_idx(min(sr+dr,255), min(sg+dg,255), min(sb+db,255));
            t_mul[s][d] = (unsigned char)nearest_idx(sr*dr/255, sg*dg/255, sb*db/255);
        }
}

static void apply_palette(int which) {
    cur_pal = which;
    static const int *LO[NPAL] = { PAL_PICO, PAL_E32, PAL_R32, PAL_E32, PAL_E64, PAL_A64, PAL_FC, PAL_J64, PAL_K64, PAL_ROSY, PAL_DB32 };
    static const int *HI[NPAL] = { 0, 0, PAL_R64X, 0 /*derived*/, PAL_E64X, PAL_A64X, PAL_FCX, PAL_J64X, PAL_K64X, 0 /*rosy=42*/, 0 /*db32=32*/ };
    for (int i = 0; i < 32; i++) cur_hex[i] = LO[which][i];
    if (HI[which])       { for (int i = 0; i < 32; i++) cur_hex[32 + i] = HI[which][i]; pal_n = 64; }
    else if (which == 3) { for (int i = 0; i < 32; i++) cur_hex[32 + i] = mix_hex(PAL_E32[MIX_PAIRS[i][0]], PAL_E32[MIX_PAIRS[i][1]]); pal_n = 64; }
    else if (which == 9) { for (int i = 32; i < 42; i++) cur_hex[i] = PAL_ROSY[i];   // the 10 leftovers
                           for (int i = 42; i < 64; i++) cur_hex[i] = cur_hex[i - 42]; pal_n = 42; }  // mirror the tail so swatches aren't black
    else                 { for (int i = 0; i < 32; i++) cur_hex[32 + i] = LO[which][i]; pal_n = 32; }
    for (int i = 0; i < 64; i++) palette_hex(i, cur_hex[i]);
    build_tables();
}

// ---- scenes as pure (x,y) -> index functions (blendlab's architecture):
// one definition paints the screen AND answers "what's under this pixel?",
// so the blend reads dst with no pget feedback.
static int night_at(int x, int y) {
    if (x >= 36 && x < 148 && y >= 64 && y < 172) {            // gallery flat
        int wx = (x-36)/14, wy = (y-64)/18, ix = (x-36)%14, iy = (y-64)%18;
        if (ix >= 4 && ix < 11 && iy >= 5 && iy < 13) {
            int m = (wx*7 + wy*13) % 7;
            return (m < 2) ? CLR_LIGHT_YELLOW : (m == 2) ? CLR_ORANGE : CLR_DARKER_PURPLE;
        }
        return CLR_DARKER_GREY;
    }
    if (x >= 176 && x < 292 && y >= 88 && y < 172) {           // far tower, dimmer
        int wx = (x-176)/16, wy = (y-88)/16, ix = (x-176)%16, iy = (y-88)%16;
        if (ix >= 5 && ix < 12 && iy >= 4 && iy < 11)
            return ((wx*3 + wy*11) % 4 == 0) ? CLR_LIGHT_YELLOW : CLR_DARKER_BLUE;
        return CLR_BROWNISH_BLACK;
    }
    if (y >= 172) return CLR_BROWNISH_BLACK;                   // street
    if (y < 56 && ((x*31 + y*17) % 331) == 0) return CLR_LIGHT_GREY;  // stars
    if (y < 40) return CLR_BLACK;
    if (y < 90) return CLR_DARKER_BLUE;
    return CLR_DARK_BLUE;
}

static int day_at(int x, int y) {
    if (y >= 60 && y < 92) return (x/10) % 32;                 // all-32 swatch band
    { int ddx = x-70,  ddy = y-142; if (ddx*ddx + ddy*ddy <= 24*24) return CLR_MEDIUM_GREEN; }
    { int ddx = x-160, ddy = y-152; if (ddx*ddx + ddy*ddy <= 18*18) return CLR_ORANGE; }
    { int ddx = x-252, ddy = y-138; if (ddx*ddx + ddy*ddy <= 27*27) return CLR_TRUE_BLUE; }
    return (((x/16) + (y/16)) & 1) ? CLR_LIGHT_GREY : CLR_WHITE;   // tiled wall
}

static void draw_scene(int (*at)(int, int)) {
    for (int y = 0; y < SCREEN_H; y++) {
        int x = 0;
        while (x < SCREEN_W) {
            int c = at(x, y), x2 = x + 1;
            while (x2 < SCREEN_W && at(x2, y) == c) x2++;
            rectfill(x, y, x2 - x, 1, c);
            x = x2;
        }
    }
}

// ---- blended primitives (ported from blendlab, 64-wide tables) ----------
static void brect(int x, int y, int w, int h, int c, unsigned char tbl[64][64], int (*at)(int, int)) {
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
            pset(px, py, tbl[c][at(px, py)]);
        }
}
static void bcirc(int cx, int cy, int r, int c, unsigned char tbl[64][64], int (*at)(int, int)) {
    for (int yy = -r; yy <= r; yy++)
        for (int xx = -r; xx <= r; xx++) {
            if (xx*xx + yy*yy > r*r) continue;
            int px = cx + xx, py = cy + yy;
            if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
            pset(px, py, tbl[c][at(px, py)]);
        }
}
static void glow(int cx, int cy, int r, int c_core, int c_mid, int c_rim, int (*at)(int, int)) {
    int r2 = r * r;
    for (int yy = -r; yy <= r; yy++)
        for (int xx = -r; xx <= r; xx++) {
            int d2 = xx*xx + yy*yy;
            if (d2 > r2) continue;
            int px = cx + xx, py = cy + yy;
            if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
            int c = (d2 * 9 < r2) ? c_core : (d2 * 9 < r2 * 4) ? c_mid : c_rim;
            pset(px, py, t_add[c][at(px, py)]);
        }
}
static void glow_fake(int cx, int cy, int r, int c_core, int c_mid, int c_rim) {
    fillp(FILL_DOTS, -1);    circfill(cx, cy, r,     c_rim);
    fillp(FILL_CHECKER, -1); circfill(cx, cy, r*2/3, c_mid);
    fillp_reset();           circfill(cx, cy, r/3,   c_core);
}

// ---- scene 0: swatches + dithered ramps ---------------------------------
static void scene_swatches(void) {
    cls(CLR_BLACK);
    for (int i = 0; i < 64; i++) {
        int x = 8 + (i % 16) * 19, y = 20 + (i / 16) * 15;
        rectfill(x, y, 17, 12, i);
        if (i % 16 == 0) { font(FONT_TINY); print(str("%d", i), x - 7, y + 3, CLR_MEDIUM_GREY); font(FONT_NORMAL); }
    }
    static const int ramps[5][6] = {
        { 0, 1, 12, 6, 7, -1 }, { 16, 4, 9, 10, 23, -1 }, { 19, 3, 27, 11, 26, -1 },
        { 0, 21, 5, 22, 15, -1 }, { 16, 20, 30, 15, 31, -1 },
    };
    static const char *names[5] = { "SKY", "WARM", "GREEN", "GREY", "SKIN" };
    for (int r = 0; r < 5; r++) {
        int y = 88 + r * 20;
        font(FONT_SMALL); print(names[r], 8, y + 3, CLR_LIGHT_GREY); font(FONT_NORMAL);
        int x = 44;
        for (int i = 0; i < 5 && ramps[r][i + 1] >= 0; i++) { hgradient(x, y, 53, 12, ramps[r][i], ramps[r][i + 1]); x += 53; }
    }
}

// ---- scene 1: sunset — the dither-school corpus stress ------------------
static void scene_sunset(void) {
    vgradient(0, 0, SCREEN_W, 60, CLR_DARK_BLUE, CLR_DARK_PURPLE);
    vgradient(0, 60, SCREEN_W, 50, CLR_DARK_PURPLE, CLR_ORANGE);
    vgradient(0, 110, SCREEN_W, 20, CLR_ORANGE, CLR_YELLOW);
    fillp(FILL_DOTS, -1);    circfill(240, 112, 26, CLR_YELLOW);       fillp_reset();
    fillp(FILL_CHECKER, -1); circfill(240, 112, 18, CLR_LIGHT_YELLOW); fillp_reset();
    circfill(240, 112, 11, CLR_WHITE);
    vgradient(0, 130, SCREEN_W, 50, CLR_BLUE, CLR_DARK_BLUE);
    fillp(FILL_HLINES, -1); rectfill(0, 134, SCREEN_W, 30, CLR_LIGHT_YELLOW); fillp_reset();
    for (int x = 0; x < SCREEN_W; x += 4) {
        int h = 18 + (int)(noise(x * 0.02f) * 22);
        rectfill(x, 130 - h, 4, h, CLR_DARK_GREEN);
    }
    fillp(FILL_CHECKER, -1); rectfill(0, 104, SCREEN_W, 26, CLR_LIGHT_GREY); fillp_reset();
    vgradient(0, 180, SCREEN_W, 20, CLR_PEACH, CLR_BROWN);
}

// ---- scene 2: portrait — skin tones are where palettes get cruel --------
static void scene_portrait(void) {
    vgradient(0, 0, SCREEN_W, SCREEN_H, CLR_INDIGO, CLR_DARKER_PURPLE);
    int cx = 160, cy = 96;
    ovalfill(cx, cy + 78, 52, 36, CLR_TRUE_BLUE);
    rectfill(cx - 9, cy + 34, 18, 16, CLR_LIGHT_PEACH);
    ovalfill(cx, cy, 40, 46, CLR_LIGHT_PEACH);
    ovalfill(cx, cy - 28, 42, 24, CLR_BROWN);
    ovalfill(cx - 30, cy - 10, 12, 22, CLR_BROWN);
    ovalfill(cx + 30, cy - 10, 12, 22, CLR_BROWN);
    fillp(FILL_CHECKER, -1); ovalfill(cx - 14, cy + 18, 9, 6, CLR_DARK_PEACH); fillp_reset();
    fillp(FILL_CHECKER, -1); ovalfill(cx + 14, cy + 18, 9, 6, CLR_DARK_PEACH); fillp_reset();
    ovalfill(cx - 14, cy - 2, 6, 4, CLR_WHITE);  pset(cx - 13, cy - 2, CLR_BROWNISH_BLACK);
    ovalfill(cx + 14, cy - 2, 6, 4, CLR_WHITE);  pset(cx + 15, cy - 2, CLR_BROWNISH_BLACK);
    ovalfill(cx, cy + 26, 8, 3, CLR_DARK_PEACH);
    fillp(FILL_CHECKER, -1); ovalfill(cx + 18, cy + 8, 18, 30, CLR_PEACH); fillp_reset();
    static const int skin[6] = { 16, 20, 4, 30, 15, 31 };
    for (int i = 0; i < 6; i++) rectfill(8 + i * 16, 168, 14, 14, skin[i]);
    font(FONT_SMALL); print("SKIN FAMILY 16 20 4 30 15 31", 8, 184, CLR_LIGHT_GREY); font(FONT_NORMAL);
}

// ---- scene 3: night glow (blendlab) — D toggles fake vs real ------------
static void scene_night(void) {
    draw_scene(night_at);
    static const int lamp_x[3] = { 52, 162, 268 };
    for (int i = 0; i < 3; i++) {
        if (dither_fake) glow_fake(lamp_x[i], 146, 26, CLR_LIGHT_YELLOW, CLR_ORANGE, CLR_DARK_BROWN);
        else             glow(lamp_x[i], 146, 26, CLR_LIGHT_YELLOW, CLR_ORANGE, CLR_DARK_BROWN, night_at);
        rectfill(lamp_x[i] - 1, 148, 2, 24, CLR_DARK_GREY);
        circfill(lamp_x[i], 146, 2, CLR_WHITE);
    }
    int mx = mouse_x(), my = mouse_y();                       // a glow you carry
    if (mx > 2 && my > 20 && mx < SCREEN_W - 2 && my < SCREEN_H - 26) {
        if (dither_fake) glow_fake(mx, my, 30, CLR_YELLOW, CLR_DARK_ORANGE, CLR_DARK_BROWN);
        else             glow(mx, my, 30, CLR_YELLOW, CLR_DARK_ORANGE, CLR_DARK_BROWN, night_at);
    }
}

// ---- scene 4: glass + fog (blendlab) — D toggle, C glass color ----------
static void scene_glass(void) {
    draw_scene(day_at);
    int cx = (int)(now() * 14) % (SCREEN_W + 140) - 70;       // drifting cloud shadows (MUL)
    if (dither_fake) {
        fillp(FILL_CHECKER, -1);
        circfill(cx, 36, 24, CLR_DARK_GREY); circfill(cx + 30, 44, 18, CLR_DARK_GREY);
        fillp_reset();
    } else {
        bcirc(cx, 36, 24, CLR_MEDIUM_GREY, t_mul, day_at);
        bcirc(cx + 30, 44, 18, CLR_MEDIUM_GREY, t_mul, day_at);
    }
    if (dither_fake) { fillp(FILL_CHECKER, -1); rectfill(0, 150, SCREEN_W, 36, CLR_LIGHT_GREY); fillp_reset(); }
    else             brect(0, 150, SCREEN_W, 36, CLR_LIGHT_GREY, t_avg, day_at);   // fog band
    int mx = clamp(mouse_x(), 40, SCREEN_W - 40), my = clamp(mouse_y(), 50, SCREEN_H - 50);
    int gc = glass_c;                                         // the pane on your mouse
    if (dither_fake) { fillp(FILL_CHECKER, -1); rectfill(mx - 38, my - 28, 76, 56, gc); fillp_reset(); }
    else             brect(mx - 38, my - 28, 76, 56, gc, t_avg, day_at);
    rect(mx - 38, my - 28, 76, 56, gc);
    // label: slot number (+ preset name when it matches one); swatch beside it
    const char *nm = "";
    for (int i = 0; i < 6; i++) if (GLASS_CLR[i] == gc) nm = GLASS_NM[i];
    print(str("%d %s", gc, nm), mx - 34, my - 24, gc == CLR_BLACK ? CLR_WHITE : gc);
}

// ---- scene 5: the raw table — eyeball where the snap bands --------------
static void scene_table(void) {
    cls(CLR_BROWNISH_BLACK);
    unsigned char (*tbl)[64] = (table_i == 0) ? t_avg : (table_i == 1) ? t_add : t_mul;
    const char *nm = (table_i == 0) ? "AVG  (s+d)/2" : (table_i == 1) ? "ADD  min(s+d,255)" : "MUL  s*d/255";
    int cell = pal_n == 64 ? 2 : 4, ox = 96, oy = 40;
    print_centered(str("%s   %dx%d", nm, pal_n, pal_n), SCREEN_W / 2, 16, CLR_LIGHT_YELLOW);
    print("src", ox - 36, oy + 58, CLR_LIGHT_GREY);
    print("dst", ox + 140, oy - 16, CLR_LIGHT_GREY);
    for (int s = 0; s < pal_n; s++) rectfill(ox - 2 - cell, oy + s*cell, cell, cell, s);
    for (int d = 0; d < pal_n; d++) rectfill(ox + d*cell, oy - 2 - cell, cell, cell, d);
    for (int s = 0; s < pal_n; s++)
        for (int d = 0; d < pal_n; d++)
            rectfill(ox + d*cell, oy + s*cell, cell, cell, tbl[s][d]);
    font(FONT_SMALL);
    print_centered("cell = table[src][dst]   T cycles AVG/ADD/MUL", SCREEN_W / 2, oy + pal_n*cell + 6, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}

void init(void) {
    enable_pget(true);
    apply_palette(2);    // boot on full Resurrect 64...
    scene = 3;           // ...over the blended night — the verdict view
}

void update(void) {
    for (int i = 0; i < 9; i++) if (keyp('1' + i)) apply_palette(i);   // keys 1-9 → 0-8
    if (keyp('0')) apply_palette(9);                                   // key 0 → the 10th (rosy42)
    if (keyp('A')) apply_palette(10);                                  // key A → DB32 (Aseprite default)
    // click the top HUD bar to cycle palettes (LMB next / RMB back) — scales past
    // the keys as more candidates land. (We're out of digits at 10; A is the 11th.)
    if (mouse_y() < 12) {
        mouse_cursor(CURSOR_HAND);
        if (mouse_pressed(0)) apply_palette((cur_pal + 1) % NPAL);
        if (mouse_pressed(1)) apply_palette((cur_pal + NPAL - 1) % NPAL);
    } else mouse_cursor(CURSOR_DEFAULT);
    if (keyp(KEY_RIGHT)) scene = (scene + 1) % NSCENES;
    if (keyp(KEY_LEFT))  scene = (scene + NSCENES - 1) % NSCENES;
    if (keyp('D')) dither_fake = !dither_fake;
    if (keyp('C')) { glass_i = (glass_i + 1) % 6; glass_c = GLASS_CLR[glass_i]; }
    if (keyp('T')) table_i = (table_i + 1) % 3;
    // mouse wheel scrubs the pane through EVERY live slot (incl. the upper 32) —
    // the quick "what does each palette color do as glass?" tour
    float w = mouse_wheel();
    if (w > 0.01f)  glass_c = (glass_c + 1) % pal_n;
    if (w < -0.01f) glass_c = (glass_c + pal_n - 1) % pal_n;
    if (glass_c >= pal_n) glass_c = pal_n - 1;   // palette switch 64 -> 32
}

void draw(void) {
    switch (scene) {
        case 0: scene_swatches(); break;
        case 1: scene_sunset();   break;
        case 2: scene_portrait(); break;
        case 3: scene_night();    break;
        case 4: scene_glass();    break;
        case 5: scene_table();    break;
    }
    static const char *pnames[NPAL] = { "1 PICO-8 (shipped)", "2 ENDESGA 32", "3 RESURRECT 64", "4 E32+32 DERIVED",
                                        "5 ENDESGA 64", "6 AAP-64", "7 FAMICUBE", "8 JOURNEY", "9 JEHKOBA64", "0 ROSY 42", "A DB32 (aseprite)" };
    static const char *snames[NSCENES] = { "SWATCHES+RAMPS", "SUNSET", "PORTRAIT", "NIGHT GLOW", "GLASS+FOG", "BLEND TABLE" };
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(pnames[cur_pal], 4, 2, CLR_WHITE);
    print_right(str("%s%s  %d/%d", scene >= 3 && scene <= 4 ? (dither_fake ? "FAKE " : "REAL ") : "",
                    snames[scene], scene + 1, NSCENES), 316, 2, CLR_LIGHT_GREY);
    font(FONT_SMALL);
    rectfill(0, 193, SCREEN_W, 7, CLR_BLACK);
    print("1-0,A palette (click HUD: L/R-click cycle)  LEFT/RIGHT scene  D fake  C glass  T table", 4, 194, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
