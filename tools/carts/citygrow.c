/* de:meta
{
  "title": "city grow",
  "status": "active",
  "created": "2026-07-06",
  "kind": [
    "tech-demo",
    "generative"
  ],
  "teaches": [
    "noise-terrain",
    "road-network"
  ],
  "lineage": "The worldgen-plan rung 2-5 BENCH (docs/design/worldgen-plan.md §Where the code lives): the settlement/street generation grammar grown in its own knobbed sandbox, over the same worldnet.h spine roadnet2 and sloop share, then extracted (citygen.h) once calibrated. Rung 2 first: the population-density field.",
  "description": "The DENSITY-FIELD bench (worldgen-plan rung 2) - the master input both pillar papers (Parish-Muller, Chen) drive everything from, grown here as a knobbed instrument. A deterministic population field D(x,y) says WHERE PEOPLE LIVE: regional noise shaped by terrain (people live LOW and FLAT - hills and rock drain it), pulled toward water (coasts and rivers attract harbours and river towns - the same water that is itself uninhabitable), fading out past the ~500 km world rim (one characterful place, not an endless smear - the float-precision bound roadnet2-plan called). Settlement candidates stay on worldnet.h's suppression lattice (blue-noise spacing survives), but PRESENCE, RANK and SIZE now come from the field instead of pure hash - so cities cluster along coasts and valley floors, hamlets thin out toward the wilderness, and a city's EXTENT is a density threshold contour, not a radius. Drag the sliders and watch the civilisation redistribute live: region = the size of the population blobs, water/flat = how hard the geography pulls, density = the presence threshold, extent = the city-footprint contour, contrast = field gamma. F toggles the heat overlay (the raw field), E the extent contours, O flips settlements between FIELD (rung 2) and the old pure-HASH lattice - the A/B that shows exactly what this rung buys: hash sprinkles towns uniformly over any passable pixel; the field GROWS them where a geographer would expect. B shows the world rim, R rerolls the seed (same seed = same world, always), SPACE jumps to fresh scenery, wheel zooms continental to town. The graph tiers (arterials, districts, street fill - rungs 3-5) build on this field in this same bench.",
  "todo": [
    "rung 3: per-city tensor field + traced arterial streamlines (bounded, cached, seeded per city)",
    "rung 4: district polygons + per-district minor fill via the morph knobs",
    "rung 5: the live SNDi panel + an OSM target loaded beside it for A/B calibration",
    "wire the calibrated field into worldnet.h (get_node/get_hub presence/rank from D) so roadnet2 + sloop's world grows organic settlements - the rung-5.5 extraction",
    "bridgehead attractors: crossings concentrate traffic -> density (needs rung-3 river/crossing sites)"
  ]
}
de:meta */
#include "studio.h"
#include "worldnet.h"   // the shared spine: terrain, lattice, hash — the field shapes ITS lattice
#include "ui.h"
#include <stdio.h>
#include <math.h>       // powf — the contrast gamma

// ============================================================================
// CITY GROW — the worldgen rung-2..5 BENCH. This file is where the generation
// grammar gets grown and calibrated, per docs/design/worldgen-plan.md §Where
// the code lives: knobbed, seed-rerollable, instant visual feedback, and NEVER
// a second home for world state — everything here is a pure function over the
// same worldnet.h spine the real carts drive. Extraction target: citygen.h
// (rung 5.5), plus the density field wiring INTO worldnet.h's get_node/get_hub.
//
// RUNG 2 (this build): THE POPULATION-DENSITY FIELD — the master input.
//   D(x,y) = regional noise · terrain shaping (low+flat) · water pull · rim fade
// Settlements keep the suppression lattice (blue-noise spacing) but presence,
// rank and size come from D. City extent = the D > extent contour.
//
//   drag sliders   field re-rolls live      ROLL / EXPLORE
//   ◄▲▼► / WASD    pan     wheel zoom       SPACE jump   R new seed
//   F density heat overlay    E extent contours    N settlement dots
//   O settlements: FIELD (rung 2) vs pure-HASH (the old lattice) — the A/B
//   B world-rim ring    G lattice grid    M setup panel    H hide hud
// ============================================================================

#define TILE 4
#define ZMIN 0.0008f
#define ZMAX 0.5f
#define OVERVIEW_ZOOM 0.0012f         // settlement-map zoom (~65 km view) — the rung-2 money view
#define SPAWN_X 40000.0f
#define SPAWN_Y 30000.0f
#define PANEL_W 96

static int   mode = 0;                // 0 = setup panel, 1 = explore
static float camX, camY, jumpN;
static float zoom = OVERVIEW_ZOOM;
static float P = TILE;
static int   vcols, vrows;
static int   dragging = 0, drag_px, drag_py;
static int   show_heat = 1, show_extent = 1, show_dots = 1, show_bound = 0;
static int   show_grid = 0, show_hud = 1;
static int   use_field = 1;           // 1 = rung-2 field settlements · 0 = old pure-hash (the A/B)

static void view_metrics(void) {
    P = TILE * zoom;
    vcols = (int)(SCREEN_W / P) + 3;
    vrows = (int)(SCREEN_H / P) + 3;
}
static int sxp(float wx) { return (int)((wx - camX) * P + P * 0.5f); }
static int syp(float wy) { return (int)((wy - camY) * P + P * 0.5f); }

// ═════════════════════════════════════════════════════════════════════════════
// THE DENSITY FIELD — rung 2's deliverable, FENCED FOR EXTRACTION into
// worldnet.h once calibrated (it becomes the presence/rank/size input of
// get_node/get_hub — the one-data-model guard says the bench only PROTOTYPES it).
//
// D(x,y) ∈ [0,1], a pure function of (x, y, seedZ, knobs) — locally evaluable
// anywhere, no global pass, seam-true like everything else on the spine.

static float K_region = 0.25f;  // regional blob size          → 20..120 km
static float K_water  = 0.60f;  // water-adjacency pull        → ×1 .. ×2.5 near coast/river
static float K_flat   = 0.65f;  // terrain shaping weight (people live LOW + FLAT)
static float K_dens   = 0.32f;  // presence threshold (left = crowded, right = sparse)
static float K_extent = 0.62f;  // city-extent contour level
static float K_pow    = 0.35f;  // contrast gamma → D^(0.5..3)

// the world bound — worldgen-plan says LOCK AT RUNG 2, leaning ~500 km (the
// float-precision note in roadnet2-plan). Implemented here as a density RIM:
// civilisation fades to wilderness over BOUND_FADE, no wall. Confirm by feel.
#define BOUND_CX 0.0f
#define BOUND_CY 0.0f
#define BOUND_R    250000.0f    // 500 km across
#define BOUND_FADE  60000.0f

static float density_at(float x, float y) {
    float h = height_at(x, y);
    if (h < 0.0f) return 0;                          // water: a constraint (density 0)...
    // regional population noise — an independent z-slice so re-rolling terrain
    // and re-rolling "where the people are" stay one seed but distinct layers
    float sc = 20000.0f + K_region * 100000.0f;      // 20..120 km blobs
    float n = 0.65f * noise3(x / sc, y / sc, seedZ + 7.7f)
            + 0.35f * noise3(x / (sc * 0.27f), y / (sc * 0.27f), seedZ + 7.7f);
    // terrain shaping: LOW is livable (1 at sea level → 0 at the rock line),
    // FLAT is livable (finite-difference slope over a 400 m probe, LAND-clamped so
    // a river channel next door doesn't read as a cliff and speckle the field)
    float lowl = 1.0f - clamp(h / 0.40f, 0, 1);
    float d = 1500.0f;      // baseline chosen to NULL the 3 km roughness octave (sin(kd)=0)
    float he = clamp(height_at(x + d, y), 0, 0.4f), hw2 = clamp(height_at(x - d, y), 0, 0.4f);
    float hs = clamp(height_at(x, y + d), 0, 0.4f), hn = clamp(height_at(x, y - d), 0, 0.4f);
    float gx = (he - hw2) / (2 * d), gy = (hs - hn) / (2 * d);
    float steep = clamp(fsqrt(gx * gx + gy * gy) * 4000.0f, 0, 1);      // ~hill flank → 1
    float shape = lowl * (1.0f - steep);
    float D = n * (1.0f - K_flat + K_flat * shape);
    // ...but water ADJACENCY attracts: real cities sit at coasts, mouths, fords
    // (worldgen-plan §Beyond roads — the harbour attractor; bridgeheads join at rung 3).
    // GRADED by how much water the probe ring sees: a big shore (coast/lake/mouth)
    // pulls harder than a thin river thread — and no binary speckle.
    int wat = 0;
    for (int i = 0; i < 8; i++)
        if (height_at(x + dx(1500.0f, i * 45.0f), y + dy(1500.0f, i * 45.0f)) < 0.0f) wat++;
    D *= 1.0f + K_water * 1.5f * clamp(wat / 4.0f, 0, 1);
    // the rim: one characterful place, thinning to wilderness past ~250 km out
    float bx = x - BOUND_CX, by = y - BOUND_CY;
    float br = fsqrt(bx * bx + by * by);
    if (br > BOUND_R) D *= clamp(1.0f - (br - BOUND_R) / BOUND_FADE, 0, 1);
    return powf(clamp(D, 0, 1), 0.5f + K_pow * 2.5f);
}

// ── FIELD-DRIVEN SETTLEMENTS — the rung-2 swap. Same lattice cell + jitter as
// worldnet's get_node/get_hub (positions identical when wired), same R=1
// priority suppression (blue-noise spacing survives) — but presence, rank and
// size come from D. A hash dither softens the threshold so the presence edge
// sprinkles instead of cutting a hard contour line of towns.

// D at a town cell's jittered spot; -1 = no candidate (water/rock)
static float town_D(int cx, int cy, float *wx, float *wy) {
    unsigned h = wn_hash2(cx * 2654435761u + 17, cy * 40503u + 91);
    int j = jit(NODE_CS);
    float x = cx * NODE_CS + NODE_CS * 0.5f + (float)((int)((h >> 7)  % (2 * j + 1)) - j);
    float y = cy * NODE_CS + NODE_CS * 0.5f + (float)((int)((h >> 15) % (2 * j + 1)) - j);
    if (!passable(height_at(x, y))) return -1;
    *wx = x; *wy = y;
    return density_at(x, y);
}
// presence-eligible strength for suppression: D dithered by the cell hash
static float town_str(int cx, int cy) {
    float x, y, D = town_D(cx, cy, &x, &y);
    if (D < 0) return -1;
    unsigned h = wn_hash2(cx * 741103597u + 43, cy * 1597334677u + 11);
    return D * (0.70f + 0.60f * (float)(h % 1000u) / 1000.0f);   // ±30% soft edge
}
static int fsettle_town(int cx, int cy, float *wx, float *wy, int *rank, float *Dout) {
    float s = town_str(cx, cy);
    if (s < K_dens) return 0;                              // presence FROM the field
    for (int a = cx - 1; a <= cx + 1; a++)                 // R=1 suppression: strongest D wins
        for (int b = cy - 1; b <= cy + 1; b++) {
            if (a == cx && b == cy) continue;
            float t = town_str(a, b);
            if (t > s || (t == s && (a < cx || (a == cx && b < cy)))) {
                if (t >= K_dens) return 0;
            }
        }
    float D = town_D(cx, cy, wx, wy);
    *rank = (D > K_dens * 1.8f) ? RK_TOWN : RK_HAMLET;     // rank FROM the field
    *Dout = D;
    return 1;
}
// hubs (cities/metros) — the coarse lattice, higher bar, de-clustered
static float hub_D(int cx, int cy, float *wx, float *wy) {
    unsigned h = wn_hash2(cx * 374761393u + 5, cy * 668265263u + 3);
    int j = jit(HUB_CS);
    float x = cx * HUB_CS + HUB_CS * 0.5f + (float)((int)((h >> 6)  % (2 * j + 1)) - j);
    float y = cy * HUB_CS + HUB_CS * 0.5f + (float)((int)((h >> 14) % (2 * j + 1)) - j);
    if (!passable(height_at(x, y))) return -1;
    *wx = x; *wy = y;
    return density_at(x, y);
}
static int fsettle_hub(int cx, int cy, float *wx, float *wy, int *rank, float *Dout) {
    float D = hub_D(cx, cy, wx, wy);
    if (D < K_dens * 1.3f) return 0;                       // cities need real density
    for (int a = cx - 1; a <= cx + 1; a++)                 // de-cluster: strongest metro wins
        for (int b = cy - 1; b <= cy + 1; b++) {
            if (a == cx && b == cy) continue;
            float xx, yy, t = hub_D(a, b, &xx, &yy);
            if (t > D) { *rank = RK_CITY; *Dout = D; return 1; }   // demote, keep the node
        }
    *rank = (D > K_dens * 2.4f) ? RK_METRO : RK_CITY;      // rank FROM the field
    *Dout = D;
    return 1;
}
// ═══ end of the fenced field block ═══════════════════════════════════════════

// ── overlays ─────────────────────────────────────────────────────────────────
// With the heat on, terrain drops to a silhouette (dark land / blue water) so the
// field ramp POPS — the bench's money view. Heat off = the normal biome map.
static void draw_terrain(void) {
    int step = 4;
    for (int sy = 0; sy < SCREEN_H; sy += step)
        for (int sx = 0; sx < SCREEN_W; sx += step) {
            float h = height_at(camX + sx / P, camY + sy / P);
            int c = show_heat ? (h < 0.0f ? CLR_DARK_BLUE : CLR_DARKER_GREY) : biome_col(h);
            rectfill(sx, sy, step, step, c);
        }
}

// heat + extent share ONE field-sampling pass (D is ~13 height_at per sample)
static const int HEAT[6] = { CLR_DARK_PURPLE, CLR_DARK_RED, CLR_RED,
                             CLR_ORANGE, CLR_YELLOW, CLR_WHITE };
static void draw_field(void) {
    if (!show_heat && !show_extent) return;
    int step = 8;
    for (int sy = 0; sy < SCREEN_H; sy += step)
        for (int sx = 0; sx < SCREEN_W; sx += step) {
            float D = density_at(camX + sx / P, camY + sy / P);
            if (show_extent && D >= K_extent) {  // the CITY FOOTPRINT: a contour, not a radius
                rectfill(sx, sy, step, step, CLR_WHITE);
                fillp(FILL_CHECKER, -1);
                rectfill(sx, sy, step, step, CLR_ORANGE);
                fillp_reset();
            } else if (show_heat && D > 0.05f) {
                int i = (int)(D * 5.999f);
                rectfill(sx, sy, step, step, HEAT[i]);
            }
        }
}

static void draw_settlements(void) {
    if (!show_dots) return;
    int detail = (zoom >= 0.0009f);   // towns visible at the settlement-map overview
                                      // (continental zoom = hubs only; the field is heavy)
    if (detail) {                                // towns/hamlets (fine lattice)
        int c0 = wn_ifloor(camX / NODE_CS) - 2, c1 = wn_ifloor((camX + vcols) / NODE_CS) + 2;
        int r0 = wn_ifloor(camY / NODE_CS) - 2, r1 = wn_ifloor((camY + vrows) / NODE_CS) + 2;
        for (int cx = c0; cx <= c1; cx++)
            for (int cy = r0; cy <= r1; cy++) {
                float wx, wy, D; int rk;
                if (use_field) { if (!fsettle_town(cx, cy, &wx, &wy, &rk, &D)) continue; }
                else           { if (!get_node(cx, cy, &wx, &wy)) continue;
                                 rk = town_rank(cx, cy); D = 0.5f; }
                int px = sxp(wx), py = syp(wy);
                int town = rk == RK_TOWN;
                int r = town ? 2 : 1;
                if (use_field) r += (int)(D * 1.8f);       // SIZE from the field
                circfill(px, py, r + 2, CLR_BLACK);        // heavy ring: reads on the heat
                circfill(px, py, r, town ? CLR_GREEN : CLR_LIGHT_GREY);
            }
    }
    int h0 = wn_ifloor(camX / HUB_CS) - 2, h1 = wn_ifloor((camX + vcols) / HUB_CS) + 2;
    int v0 = wn_ifloor(camY / HUB_CS) - 2, v1 = wn_ifloor((camY + vrows) / HUB_CS) + 2;
    for (int cx = h0; cx <= h1; cx++)            // cities/metros (coarse lattice)
        for (int cy = v0; cy <= v1; cy++) {
            float wx, wy, D; int rk;
            if (use_field) { if (!fsettle_hub(cx, cy, &wx, &wy, &rk, &D)) continue; }
            else           { if (!get_hub(cx, cy, &wx, &wy)) continue;
                             rk = hub_rank(cx, cy); D = 0.5f; }
            int metro = rk == RK_METRO;
            int rr = metro ? 4 : 3;
            if (use_field) rr += (int)(D * 2.5f);          // SIZE from the field
            if (zoom < 0.0022f) rr = (rr + 1) / 2;         // LOD far out
            int px = sxp(wx), py = syp(wy);
            circfill(px, py, rr + 2, CLR_BLACK);           // heavy ring: reads on the heat
            circfill(px, py, rr, CLR_WHITE);
            if (metro && rr >= 3) circfill(px, py, 2, CLR_DARK_BLUE);
        }
}

static void draw_bound(void) {                   // the world rim (B) — ~500 km across
    if (!show_bound) return;
    int cx = sxp(BOUND_CX), cy = syp(BOUND_CY);
    int r = (int)(BOUND_R * P);
    circ(cx, cy, r, CLR_RED);
    circ(cx, cy, r + (int)(BOUND_FADE * P), CLR_DARK_RED);
}

static void draw_grid_overlay(void) {
    for (int cx = wn_ifloor(camX / NODE_CS); cx * NODE_CS < camX + vcols; cx++) {
        int x = (int)((cx * NODE_CS - camX) * P);
        line(x, 0, x, SCREEN_H, CLR_DARK_PURPLE);
    }
    for (int cy = wn_ifloor(camY / NODE_CS); cy * NODE_CS < camY + vrows; cy++) {
        int y = (int)((cy * NODE_CS - camY) * P);
        line(0, y, SCREEN_W, y, CLR_DARK_PURPLE);
    }
}

static void hud(void) {
    char buf[80];
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("CITYGROW", 4, 2, CLR_LIGHT_GREY);
    // D under the mouse — the bench's probe readout
    float mwx = camX + mouse_x() / P, mwy = camY + mouse_y() / P;
    snprintf(buf, sizeof buf, "D %.2f  %s", density_at(mwx, mwy),
             use_field ? "FIELD" : "HASH(old)");
    print(buf, 84, 2, use_field ? CLR_GREEN : CLR_ORANGE);
    print_centered("F heat  E extent  N dots  O field/hash  B rim  \x18\x19\x1a\x1b pan  wheel  SPACE  R  M",
                   SCREEN_W / 2, SCREEN_H - 9, CLR_DARK_GREY);
}

static void draw_setup_panel(void) {
    char buf[48];
    fillp(FILL_CHECKER, -1); rectfill(0, 0, PANEL_W, SCREEN_H, CLR_BLACK); fillp_reset();
    line(PANEL_W, 0, PANEL_W, SCREEN_H, CLR_DARK_GREY);
    print("CITYGROW", 4, 4, CLR_LIGHT_GREY);

    font(FONT_SMALL);
    ui_begin();
    static const char *L[7] = { "water", "region", "waterpull", "flat", "density",
                                "extent", "contrast" };
    float *pp[7] = { &P_sea, &K_region, &K_water, &K_flat, &K_dens, &K_extent, &K_pow };
    for (int i = 0; i < 7; i++) ui_slider(pp[i], 4, 16 + i * 12, 88, L[i]);
    int roll = ui_button(4, 104, 88, 14, "ROLL");
    int go   = ui_button(4, 122, 88, 16, "EXPLORE \x10");
    ui_end();

    snprintf(buf, sizeof buf, "seed #%u", (unsigned)(seedZ * 1000) % 100000u);
    print(buf, 4, 144, CLR_MEDIUM_GREY);
    print("drag = re-roll live", 4, SCREEN_H - 9, CLR_DARKER_GREY);
    font(FONT_NORMAL);

    if (roll) seedZ += 0.37f;
    if (go || keyp(KEY_ENTER)) mode = 1;
}

void init(void) {
    view_metrics();
    camX = SPAWN_X - vcols / 2.0f; camY = SPAWN_Y - vrows / 2.0f;
}

void update(void) {
    float w = mouse_wheel();
    if (w != 0) {
        float oldp = TILE * zoom;
        float ccx = camX + SCREEN_W * 0.5f / oldp, ccy = camY + SCREEN_H * 0.5f / oldp;
        zoom += (w > 0 ? 1 : -1) * 0.15f * zoom;
        if (zoom < ZMIN) zoom = ZMIN;
        if (zoom > ZMAX) zoom = ZMAX;
        float np = TILE * zoom;
        camX = ccx - SCREEN_W * 0.5f / np; camY = ccy - SCREEN_H * 0.5f / np;
    }
    float pp = TILE * zoom;
    float pan = 1.3f / zoom;
    if (btn(0, BTN_LEFT)  || key('A')) camX -= pan;
    if (btn(0, BTN_RIGHT) || key('D')) camX += pan;
    if (btn(0, BTN_UP)    || key('W')) camY -= pan;
    if (btn(0, BTN_DOWN)  || key('S')) camY += pan;
    int pan_ok = (mode == 1) || (mouse_x() > PANEL_W);
    if (mouse_pressed(MOUSE_LEFT) && pan_ok) { dragging = 1; drag_px = mouse_x(); drag_py = mouse_y(); }
    if (mouse_down(MOUSE_LEFT)) {
        if (dragging) { camX -= (mouse_x() - drag_px) / pp; camY -= (mouse_y() - drag_py) / pp;
                        drag_px = mouse_x(); drag_py = mouse_y(); }
    } else dragging = 0;

    if (keyp(KEY_SPACE) && mode == 1) {
        jumpN += 1.0f;
        unsigned h = wn_hash2((int)(jumpN * 911), 7);
        camX += (float)((int)(h % 60000u) - 30000) + 12000.0f;
        camY += (float)((int)((h >> 11) % 60000u) - 30000) + 12000.0f;
    }
    if (keyp('R')) { zoom = OVERVIEW_ZOOM; view_metrics();
                     camX = SPAWN_X - vcols / 2.0f; camY = SPAWN_Y - vrows / 2.0f;
                     seedZ += 0.37f; jumpN = 0; }
    if (keyp('F')) show_heat   = !show_heat;
    if (keyp('E')) show_extent = !show_extent;
    if (keyp('N')) show_dots   = !show_dots;
    if (keyp('O')) use_field   = !use_field;
    if (keyp('B')) show_bound  = !show_bound;
    if (keyp('G')) show_grid   = !show_grid;
    if (keyp('H')) show_hud    = !show_hud;
    if (keyp('M')) mode = 0;

#ifdef DE_TRACE
    // deterministic probes — the rung-2 gate reads these (same seed → same field)
    watch("d_spawn", "%.4f", density_at(SPAWN_X, SPAWN_Y));
    watch("d_far",   "%.4f", density_at(SPAWN_X + 87000.0f, SPAWN_Y - 41000.0f));
    watch("field",   "%d", use_field);
    {   // field statistics over the LAND in view (calibration eyes)
        float dmin = 1, dmax = 0, dsum = 0; int nland = 0;
        for (int gy = 0; gy < 10; gy++)
            for (int gx = 0; gx < 16; gx++) {
                float x = camX + (gx + 0.5f) * vcols / 16.0f;
                float y = camY + (gy + 0.5f) * vrows / 10.0f;
                if (height_at(x, y) < 0) continue;
                float D = density_at(x, y);
                nland++; dsum += D;
                if (D < dmin) dmin = D;
                if (D > dmax) dmax = D;
            }
        watch("d_min",  "%.3f", nland ? dmin : -1.0f);
        watch("d_mean", "%.3f", nland ? dsum / nland : -1.0f);
        watch("d_max",  "%.3f", nland ? dmax : -1.0f);
        watch("nland",  "%d", nland);
    }
    {   // settlement counts over the view — the organic-map gate reads these
        int nt = 0, nh = 0, nhub = 0;
        int c0 = wn_ifloor(camX / NODE_CS), c1 = wn_ifloor((camX + vcols) / NODE_CS);
        for (int cx = c0; cx <= c1; cx++)
            for (int cy = wn_ifloor(camY / NODE_CS); cy <= wn_ifloor((camY + vrows) / NODE_CS); cy++) {
                float wx, wy, D; int rk;
                if (fsettle_town(cx, cy, &wx, &wy, &rk, &D)) { if (rk == RK_TOWN) nt++; else nh++; }
            }
        for (int cx = wn_ifloor(camX / HUB_CS); cx <= wn_ifloor((camX + vcols) / HUB_CS); cx++)
            for (int cy = wn_ifloor(camY / HUB_CS); cy <= wn_ifloor((camY + vrows) / HUB_CS); cy++) {
                float wx, wy, D; int rk;
                if (fsettle_hub(cx, cy, &wx, &wy, &rk, &D)) nhub++;
            }
        watch("towns", "%d", nt); watch("hamlets", "%d", nh); watch("hubs", "%d", nhub);
    }
#endif
}

void draw(void) {
    view_metrics();
    draw_terrain();
    draw_field();
    if (show_grid) draw_grid_overlay();
    draw_settlements();
    draw_bound();
    if (mode == 0) draw_setup_panel();
    else if (show_hud) hud();
}
