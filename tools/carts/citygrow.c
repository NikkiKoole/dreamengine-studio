/* de:meta
{
  "slug": "citygrow",
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
  "description": "The DENSITY-FIELD bench (worldgen-plan rungs 2-5) - the master input both pillar papers (Parish-Muller, Chen) drive everything from, grown here as a knobbed instrument. RUNG 2, the field: a deterministic population field D(x,y) says WHERE PEOPLE LIVE - regional noise shaped by terrain (people live LOW and FLAT; hills and rock drain it), pulled toward water (coasts and rivers attract harbours and river towns - the same water that is itself uninhabitable), fading out past the ~500 km world rim (one characterful place, not an endless smear). Settlement candidates stay on worldnet.h's suppression lattice (blue-noise spacing survives), but PRESENCE, RANK and SIZE come from the field - cities cluster along coasts and valley floors, hamlets thin toward the wilderness, and a city's EXTENT is a density threshold contour, not a radius. RUNG 3, the arterials: press T over any city and it becomes a CITY VIEW - a per-city TENSOR FIELD (Chen 2008: radial-around-the-core + grid-aligned-to-the-ENTERING-HIGHWAY read live from the worldnet graph + terrain-contour alignment + noise, weights hashed per city) traced as hyperstreamlines of both perpendicular eigen-families with Jobard-Lefer-style separation, wholesale and cached because a city is BOUNDED. The arterials visibly shear along coastlines, wrap around lakes, and orient to their highway gate - every city its own anatomy, same seed same city, always. X exports the traced graph as sndi-check JSON (crossings split, nodes welded) - the first generated network the rung-0 oracle can hold against a real city (first measurement: mean degree 2.65 vs Amersfoort's 2.71; the dead-end rim stubs and missing T-share are rung-5 calibration targets, now measurable). RUNG 4, the districts: the arterial graph's planar FACES are the districts - press K in the city view to cycle off/faces/minor-fill/both. Each district hashes to one of streetlab's five street patterns as a density-biased preset (dense core to grid, mid to organic/superblock, fringe to cul-de-sac dead-ends), fills with a block-scale local lattice clipped inside the face, and STITCHES onto the bounding arterials (locals T onto collectors T onto arterials - the continuity tenet). V exports the WHOLE city (arterials + minors + stitches) for sndi-check: the missing T-junction share climbs from the arterials-only 1.1% to 64.6% - real Dutch-city territory (Amersfoort 63%, Rotterdam 60%). This is where 'procedural grid' dies. Sliders re-roll everything live; F heat, E extent contours, O the field-vs-hash A/B, B the world rim, SPACE hub-scale hops, R reseed.",
  "todo": [
    "rung 5 CALIBRATION (rung 4 shipped): whole-city T-share now 64.6% (real NL 60-63%), but two gaps measured: dead-ends TOO FEW (5.1% vs real ~20% - raise the cul-de-sac fringe band in ms_pattern / sparser grid interiors) and mean degree a touch high (3.20 vs ~2.8 - the full-lattice grid/organic over-connect). Plus a live SNDi panel + an OSM target beside it for the A/B.",
    "rung 4 followups: district fill uses straight-edge faces (curved arterials approximated); superblock/cul-de-sac need bigger polygons to read; radial pattern not used (whole-city, not district-scale). Stitch dedups one stub per arterial vertex - could space by class.",
    "true Jobard-Lefer distance separation (the id-grid is coarse; spacing jitters between 0.7x and 1.4x d_sep)",
    "rung 5.5 WIRING (citygen.h extracted 2026-07-10, gate-preserved): the payoff is still open. citygen grew its OWN world model (density-field settlements + tensor arterials) not yet reconciled with worldnet.h's beta-skeleton lattice/hubs. Two spine edits (shared with roadnet2+sloop, do gated): (a) point worldnet get_node/get_hub presence/rank at citygen density -> organic settlements in the infinite world; (b) wn_road_at falls through to a cached citygen_road_at near a city -> sloop drives the generated minor streets (the deferred drive-it gate).",
    "bridgehead attractors: crossings concentrate traffic -> density (needs rivers as network edges)",
    "PERF: draw() recomputes the terrain/density noise per-pixel every frame - attach-profiler shows ~89% of cart CPU in noise3 (63.7%) + noise2 (16.4%) + height_at (8.8%), all via draw>density_at>height_at>noise3 and draw>draw_terrain>height_at. Cache height_at/density_at into a buffer that only recomputes on map/seed/pan/zoom change instead of every frame."
  ]
}
de:meta */
#include "studio.h"
#include "worldnet.h"   // the shared spine: terrain, lattice, hash — the field shapes ITS lattice
#include "ui.h"
#include <stdio.h>
#include <math.h>       // powf — the contrast gamma
#include "citygen.h"   // the extracted city grammar (rungs 2-5) — worldgen-plan rung 5.5

// ============================================================================
// CITY GROW — the worldgen rung-2..5 BENCH. This file is where the generation
// grammar gets grown and calibrated, per docs/design/worldgen-plan.md §Where
// the code lives: knobbed, seed-rerollable, instant visual feedback, and NEVER
// a second home for world state — everything here is a pure function over the
// same worldnet.h spine the real carts drive. Extraction target: citygen.h
// (rung 5.5), plus the density field wiring INTO worldnet.h's get_node/get_hub.
//
// RUNG 2: THE POPULATION-DENSITY FIELD — the master input.
//   D(x,y) = regional noise · terrain shaping (low+flat) · water pull · rim fade
// Settlements keep the suppression lattice (blue-noise spacing) but presence,
// rank and size come from D. City extent = the D > extent contour.
// RUNG 3 (T): per-city TENSOR-FIELD arterials, traced as two eigen-families.
// RUNG 4 (K): the arterials' planar faces = DISTRICTS; each fills with a
//   density-biased streetlab pattern (grid/organic/cul-de-sac/superblock) and
//   is STITCHED onto the bounding arterials. Whole-city T-share → 64.6% (real).
//
//   drag sliders   field re-rolls live      ROLL / EXPLORE
//   ◄▲▼► / WASD    pan     wheel zoom       SPACE jump   R new seed
//   F density heat   E extent contours   N settlement dots   B rim   G grid
//   O settlements: FIELD (rung 2) vs pure-HASH — the A/B     M panel   H hud
//   MAP:  T enter the strongest city →
//   CITY: K districts (off/faces/minors/both)   X arterials.json   V city.json
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
static int   show_dist = 0;                // rung 4: district faces overlay (K)
static int   show_junc = 0;                // rung 6: junction descriptors overlay (J)
static int   use_field = 1;           // 1 = rung-2 field settlements · 0 = old pure-hash (the A/B)

static void view_metrics(void) {
    P = TILE * zoom;
    vcols = (int)(SCREEN_W / P) + 3;
    vrows = (int)(SCREEN_H / P) + 3;
}
static int sxp(float wx) { return (int)((wx - camX) * P + P * 0.5f); }
static int syp(float wy) { return (int)((wy - camY) * P + P * 0.5f); }


static float map_camX, map_camY, map_zoom; // where T left the map
// enter the city view: the strongest field-hub near the view centre
static int ar_pick_city(void) {
    float vx0 = camX + SCREEN_W * 0.5f / P, vy0 = camY + SCREEN_H * 0.5f / P;
    int hc = wn_ifloor(vx0 / HUB_CS), hr = wn_ifloor(vy0 / HUB_CS);
    float bestD = -1;
    for (int a = hc - 2; a <= hc + 2; a++)
        for (int b = hr - 2; b <= hr + 2; b++) {
            float wx, wy, D; int rk;
            if (!fsettle_hub(a, b, &wx, &wy, &rk, &D)) continue;
            float dd = (wx - vx0) * (wx - vx0) + (wy - vy0) * (wy - vy0);
            float sc = D - fsqrt(dd) * 6e-6f;            // strongest, biased toward the view
            if (sc > bestD) {
                bestD = sc;
                ar_ccx = a; ar_ccy = b; ar_cx = wx; ar_cy = wy; ar_D = D;
            }
        }
    if (bestD < 0) return 0;
    ar_R = 2200.0f + 3200.0f * ar_D;                     // SIZE from the field (rung 2)
    ar_valid = 0;
    return 1;
}


// export the traced arterial graph as sndi-check's generated-graph JSON:
// nodes = streamline endpoints + crossings (5 m weld), edges carry their pts.
// Written to the cwd (build/ under the harness): citygrow-graph.json.
static void ar_export(void) {
    ar_graph_ensure();
    FILE *f = fopen("citygrow-graph.json", "w");
    if (!f) return;
    fprintf(f, "{\"nodes\":[");
    for (int i = 0; i < g_nn; i++) fprintf(f, "%s[%.1f,%.1f]", i ? "," : "", gnx[i], gny[i]);
    fprintf(f, "],\"edges\":[");
    for (int e = 0; e < g_ne; e++) {
        int a = gea[e], b = geb[e], l = ge_line[e];
        // pts BEGIN and END at the welded node coords — sndi-check rebuilds the
        // graph from pts via 1 m vertex welding, so crossing edges must SHARE
        // their endpoint vertex exactly or the crossing doesn't exist to it
        fprintf(f, "%s{\"a\":%d,\"b\":%d,\"pts\":[", e ? "," : "", a, b);
        fprintf(f, "[%.1f,%.1f]", gnx[a], gny[a]);
        int i0 = (int)floorf(ge_t0[e]) + 1, i1 = (int)ceilf(ge_t1[e]) - 1;
        for (int i = i0; i <= i1 && i < ar_np[l]; i++)
            if ((float)i > ge_t0[e] + 0.05f && (float)i < ge_t1[e] - 0.05f)
                fprintf(f, ",[%.1f,%.1f]", ar_px[l][i], ar_py[l][i]);
        fprintf(f, ",[%.1f,%.1f]]}", gnx[b], gny[b]);
    }
    fprintf(f, "]}\n");
    fclose(f);
}

// rung 4.4 gate: the WHOLE city — arterials + minor streets + stitches — as one
// sndi-check graph (all edges carry pts; the oracle welds them at 1 m, so a stub
// snapped onto an arterial vertex reads as a real T). Written: citygrow-city.json.
// This is the A/B against the arterials-only export: T-share should climb.
static void ar_export_full(void) {
    ar_graph_ensure();
    FILE *f = fopen("citygrow-city.json", "w");
    if (!f) return;
    fprintf(f, "{\"nodes\":[],\"edges\":[");
    int first = 1;
    for (int e = 0; e < g_ne; e++) {                     // arterials (their full polyline)
        int a = gea[e], b = geb[e], l = ge_line[e];
        fprintf(f, "%s{\"pts\":[[%.1f,%.1f]", first ? "" : ",", gnx[a], gny[a]); first = 0;
        int i0 = (int)floorf(ge_t0[e]) + 1, i1 = (int)ceilf(ge_t1[e]) - 1;
        for (int i = i0; i <= i1 && i < ar_np[l]; i++)
            if ((float)i > ge_t0[e] + 0.05f && (float)i < ge_t1[e] - 0.05f)
                fprintf(f, ",[%.1f,%.1f]", ar_px[l][i], ar_py[l][i]);
        fprintf(f, ",[%.1f,%.1f]]}", gnx[b], gny[b]);
    }
    for (int e = 0; e < me_n; e++) {                     // minors + stitches (straight)
        int a = mea[e], b = meb[e];
        fprintf(f, ",{\"pts\":[[%.1f,%.1f],[%.1f,%.1f]]}", msx[a], msy[a], msx[b], msy[b]);
    }
    fprintf(f, "]}\n");
    fclose(f);
}
// ═══ end of the fenced rung-3 block ══════════════════════════════════════════

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

// ── the CITY VIEW (rung 3) — the traced arterials over the terrain ───────────
static void ar_draw(void) {
    ar_ensure();
    for (int l = 0; l < ar_nl; l++)                      // casing pass, then centre pass
        for (int i = 0; i + 1 < ar_np[l]; i++) {
            int x0 = sxp(ar_px[l][i]), y0 = syp(ar_py[l][i]);
            int x1 = sxp(ar_px[l][i + 1]), y1 = syp(ar_py[l][i + 1]);
            line(x0, y0 + 1, x1, y1 + 1, CLR_DARKER_GREY);
            line(x0 + 1, y0, x1 + 1, y1, CLR_DARKER_GREY);
        }
    for (int l = 0; l < ar_nl; l++)
        for (int i = 0; i + 1 < ar_np[l]; i++)
            line(sxp(ar_px[l][i]), syp(ar_py[l][i]),
                 sxp(ar_px[l][i + 1]), syp(ar_py[l][i + 1]),
                 ar_lfam[l] ? CLR_LIGHT_GREY : CLR_WHITE);   // the two eigen-families
    int cx = sxp(ar_cx), cy = syp(ar_cy);
    circfill(cx, cy, 3, CLR_BLACK); circfill(cx, cy, 2, CLR_GREEN);   // the core
    if (ar_has_hw) {                                     // the highway-gate bearing
        int gx = sxp(ar_cx + cosf(ar_hw_ang) * ar_R * 0.9f);
        int gy = syp(ar_cy + sinf(ar_hw_ang) * ar_R * 0.9f);
        line(cx, cy, gx, gy, CLR_DARK_RED);
        circfill(gx, gy, 2, CLR_RED);
    }
    circ(cx, cy, (int)(ar_R * P), CLR_DARK_PURPLE);      // the extent cap
    // rung 5.5b: the QUERY-SEAM proof — a crosshair at the view centre, coloured
    // by citygen_road_at (green = on an arterial, yellow = on a minor street, red
    // = off-road). Pan it over the city: it flips exactly at the drawn streets —
    // screen == query, the seam sloop will drive.
    float qwx = camX + SCREEN_W * 0.5f / P, qwy = camY + SCREEN_H * 0.5f / P;
    CityHit qh = citygen_road_at(qwx, qwy);
    int qc = qh.on_road ? (qh.minor ? CLR_YELLOW : CLR_GREEN) : CLR_RED;
    int qx = SCREEN_W / 2, qy = SCREEN_H / 2;
    line(qx - 5, qy, qx + 5, qy, qc); line(qx, qy - 5, qx, qy + 5, qc);
    circ(qx, qy, 3, qc);
    print(qh.on_road ? (qh.minor ? "street" : "ARTERIAL") : "off-road", qx + 8, qy - 3, qc);
}

// ── Rung 4 (K): the DISTRICTS — the planar faces the arterials enclose ───────
// Each face is one district: a coloured dither wash (hashed per district so
// neighbours differ) + a bright outline + a centroid dot. This is the partition
// the per-district minor fill (rung 4.2–4.4) is generated INTO.
static void ar_dist_draw(void) {
    ar_graph_ensure();
    static const int pal[6] = { CLR_RED, CLR_ORANGE, CLR_YELLOW,
                                CLR_GREEN, CLR_BLUE, CLR_PINK };
    for (int fi = 0; fi < fc_n; fi++) {
        int nv = fc_nv[fi]; if (nv < 3 || nv > FACEV_MAX) continue;
        int xy[2 * FACEV_MAX];
        for (int i = 0; i < nv; i++) {
            xy[2 * i]     = sxp(gnx[fc_v[fi][i]]);
            xy[2 * i + 1] = syp(gny[fc_v[fi][i]]);
        }
        unsigned h = wn_hash2((int)(fc_cx[fi] * 0.02f), (int)(fc_cy[fi] * 0.02f));
        int col = pal[h % 6];
        fillp(0x5A5A, -1);                               // ~50% dither: terrain shows through
        polyfill(xy, nv, col);
        fillp_reset();
        poly(xy, nv, col);
        int cx = sxp(fc_cx[fi]), cy = syp(fc_cy[fi]);
        circfill(cx, cy, 1, CLR_WHITE);
    }
}

// ── Rung 4.2/4.3 (K): the MINOR STREETS — coloured by their district's pattern
// so a stranger can SEE neighbouring districts differ (grid vs organic vs
// dead-end vs superblock). This is the "procedural grid dies" view.
static void ms_draw(void) {
    ar_graph_ensure();
    for (int b = 0; b < cgb_n; b++) {           // rung 7: the buildings lining the streets (dark blocks)
        int cx = sxp(cgb_mx[b]), cy = syp(cgb_my[b]);
        rectfill_rot(cx, cy, (int)(cgb_w[b] * P), (int)(cgb_h[b] * P), cgb_ang[b], CLR_DARK_GREY);
    }
    static const int pcol[MP_N + 1] = { CLR_WHITE, CLR_YELLOW, CLR_ORANGE, CLR_PINK, CLR_RED };
    for (int e = 0; e < me_n; e++) {
        int a = mea[e], b = meb[e];
        line(sxp(msx[a]), syp(msy[a]), sxp(msx[b]), syp(msy[b]), pcol[me_pat[e]]);
    }
}

// ── Rung 6 (J): the emitted JUNCTION descriptors — every arterial node with its arms drawn by TIER
// (highway = white, arterial = grey), and the node dot GREEN at-grade / RED grade-separated (a highway
// flying over). This is the (legs, bearings, class, grade) B4's roadkit dispatcher routes.
static void cg_junc_draw(void) {
    ar_graph_ensure();
    for (int i = 0; i < cgj_n; i++) {
        CgJunc *j = &cgj[i];
        int cx = sxp(j->x), cy = syp(j->y);
        for (int a = 0; a < j->narm; a++) {                // an arm tick out each bearing, coloured by tier
            float b = j->brg[a] * 0.0174533f;
            int ex = sxp(j->x + cosf(b) * 120.0f), ey = syp(j->y + sinf(b) * 120.0f);
            line(cx, cy, ex, ey, j->cls[a] == CG_HWY ? CLR_WHITE : CLR_LIGHT_GREY);
        }
        int gc = j->grade == 2 ? CLR_RED : j->grade == 1 ? CLR_ORANGE : CLR_GREEN;   // interchange / overpass / at-grade
        circfill(cx, cy, j->grade == 2 ? 4 : j->grade == 1 ? 3 : 2, gc);
    }
}

static void hud_city(void) {
    char buf[96];
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("CITYGROW", 4, 2, CLR_LIGHT_GREY);
    static const char *dn[4] = { "off", "faces", "minors", "both" };
    snprintf(buf, sizeof buf, "CITY %d,%d  D %.2f  lines %d  districts %d  minor %d/%d  [%s]",
             ar_ccx, ar_ccy, ar_D, ar_nl, show_dist ? fc_n : 0,
             show_dist >= 2 ? ms_n : 0, show_dist >= 2 ? me_n : 0, dn[show_dist]);
    print(buf, 84, 2, CLR_GREEN);
    print_centered("T map   K districts   X arterials.json  V whole-city.json   R reroll",
                   SCREEN_W / 2, SCREEN_H - 9, CLR_DARK_GREY);
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

    if (keyp(KEY_SPACE) && mode == 1) {          // hub-scale hops — a fresh NEIGHBOURHOOD each press,
        jumpN += 1.0f;                           // clamped inside the world rim (there's no one out there)
        unsigned h = wn_hash2((int)(jumpN * 911), 7);
        camX += (float)((int)(h % 300000u) - 150000) + 60000.0f;
        camY += (float)((int)((h >> 11) % 300000u) - 150000) + 60000.0f;
        float ccx = camX + SCREEN_W * 0.5f / pp, ccy = camY + SCREEN_H * 0.5f / pp;
        ccx = clamp(ccx, BOUND_CX - BOUND_R * 0.85f, BOUND_CX + BOUND_R * 0.85f);
        ccy = clamp(ccy, BOUND_CY - BOUND_R * 0.85f, BOUND_CY + BOUND_R * 0.85f);
        camX = ccx - SCREEN_W * 0.5f / pp; camY = ccy - SCREEN_H * 0.5f / pp;
    }
    if (keyp('R')) { zoom = OVERVIEW_ZOOM; view_metrics();
                     camX = SPAWN_X - vcols / 2.0f; camY = SPAWN_Y - vrows / 2.0f;
                     seedZ += 0.37f; jumpN = 0; ar_on = 0; }
    if (keyp('T') && mode == 1) {                // rung 3: the CITY VIEW ↔ the map
        if (ar_on) { ar_on = 0; camX = map_camX; camY = map_camY; zoom = map_zoom; view_metrics(); }
        else if (ar_pick_city()) {
            map_camX = camX; map_camY = camY; map_zoom = zoom;
            ar_on = 1;
            zoom = (SCREEN_H * 0.42f) / (TILE * ar_R);   // the city fills the view
            view_metrics();
            camX = ar_cx - SCREEN_W * 0.5f / (TILE * zoom);
            camY = ar_cy - SCREEN_H * 0.5f / (TILE * zoom);
        }
    }
    if (keyp('X') && ar_on) { ar_ensure(); ar_export(); }       // arterials-only (rung 3)
    if (keyp('V') && ar_on) { ar_graph_ensure(); ar_export_full(); }  // whole city (rung 4 gate)
    if (keyp('F')) show_heat   = !show_heat;
    if (keyp('E')) show_extent = !show_extent;
    if (keyp('N')) show_dots   = !show_dots;
    if (keyp('O')) use_field   = !use_field;
    if (keyp('B')) show_bound  = !show_bound;
    if (keyp('G')) show_grid   = !show_grid;
    if (keyp('K')) show_dist   = (show_dist + 1) % 4;  // rung 4: off→faces→minors→both
    if (keyp('J')) show_junc   = !show_junc;           // rung 6: junction descriptors
    if (keyp('H')) show_hud    = !show_hud;
    if (keyp('M')) mode = 0;

#ifdef DE_TRACE
    if (ar_on) {   // rung-4 gate: district count + the per-district pattern mix + minor-street size
        ar_graph_ensure();
        int pc[MP_N] = { 0, 0, 0, 0 };
        for (int i = 0; i < fc_n; i++) pc[fc_pat[i]]++;
        watch("districts", "%d", fc_n);
        watch("pat_grid",  "%d", pc[MP_GRID]);
        watch("pat_org",   "%d", pc[MP_ORGANIC]);
        watch("pat_cul",   "%d", pc[MP_CULDESAC]);
        watch("pat_super", "%d", pc[MP_SUPERBLOCK]);
        watch("minor_n",   "%d", ms_n);
        watch("minor_e",   "%d", me_n);
        // rung 6: junction emission — total + overpasses (grade 1) + interchanges (grade 2 = B4's roadlab families)
        { int ov = 0, ix = 0; for (int i = 0; i < cgj_n; i++){ if (cgj[i].grade==1) ov++; else if (cgj[i].grade==2) ix++; }
          watch("junc", "%d", cgj_n); watch("junc_over", "%d", ov); watch("junc_ixc", "%d", ix); }
        // rung 5.5b: the query seam — on_road over a grid, + at the city centre.
        // Deterministic + agrees with the rendered streets (screen == query).
        int on = 0, onm = 0, ns = 0;
        for (int gy = 0; gy < 24; gy++)
            for (int gx = 0; gx < 24; gx++) {
                float qx = ar_cx + (gx - 11.5f) * (ar_R / 12.0f);
                float qy = ar_cy + (gy - 11.5f) * (ar_R / 12.0f);
                CityHit q = citygen_road_at(qx, qy);
                ns++; if (q.on_road) { on++; if (q.minor) onm++; }
            }
        watch("q_center", "%d", citygen_road_at(ar_cx, ar_cy).on_road);
        watch("q_onroad", "%d", on);       // of 576 grid samples over the extent
        watch("q_minor",  "%d", onm);
    }
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
    if (ar_on) {                                 // rung 3: one city, its arterials
        int heat = show_heat; show_heat = 0;     // biome terrain under the streets
        draw_terrain(); show_heat = heat;
        ar_draw();
        if (show_junc) cg_junc_draw();                          // rung 6: emitted junctions
        if (show_dist == 1 || show_dist == 3) ar_dist_draw();   // the partition
        if (show_dist >= 2) ms_draw();                          // the minor fill
        if (show_hud) hud_city();
        return;
    }
    draw_terrain();
    draw_field();
    if (show_grid) draw_grid_overlay();
    draw_settlements();
    draw_bound();
    if (mode == 0) draw_setup_panel();
    else if (show_hud) hud();
}
