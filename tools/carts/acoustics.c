#include "studio.h"
#include <math.h>
#include <stdio.h>

// ACOUSTICS PROBE (v3) — a little guy walking rooms, to HEAR where the API hurts.
// Hand-wired against EXISTING knobs only (no new engine API) — the point is to find
// what "drops out": see docs/design/spatial.md → "v3 — acoustic zones".
//
// Walk (arrows) across floors + through a wall/doorway and listen for:
//   • MATERIAL — your FOOTSTEPS change tone by ground (tile click / carpet thud / wood
//     knock / grass rustle) AND carry the room's reverb (absorption → bright vs dead).
//   • ZONES    — each room sets the master reverb (tile = long+bright, carpet = short+dark,
//     wood = mid, grass = nearly dry). Crossing rooms re-applies it — listen for the
//     ABRUPT switch (no crossfade): that's the "rideable reverb" gap.
//   • OCCLUSION — a RADIO (room A) + a MACHINE (room B) emit continuously; a wall between
//     you and one muffles it (low-pass + quieter), the doorway opens it back up. Thicker
//     wall = more muffled (transmission).
//
// FINDINGS (from building + walking this, 2026-06-15):
//   1. Core works with EXISTING knobs — occlusion = note_cutoff (Hz, fine + slewed: great) +
//      attenuation; zones = reverb(size,damp). v3's mechanic is genuinely cart-side. ✓
//   2. **note_vol(0..7) is too COARSE for occlusion attenuation** — mapping occ→vol gives ~4
//      audible steps. The thing that'd "drop out": a float `note_gain(handle, 0..1)`. note_cutoff
//      is fine (Hz). [the muffle reads smooth; the level steps]
//   3. **Zone reverb switches ABRUPTLY** — set-and-hold means you can't ride reverb() per frame,
//      so crossing rooms is an instant jump, not a crossfade. THE #1 engine gap: a rideable /
//      crossfadable zone reverb (slewed target, or crossfade two tanks).
//   4. The listener→source RAYCAST is ~12 lines (occlusion() below) and GENERALIZES (enemy
//      vision / stealth / lighting). Candidate `line_of_sight`/`raycast_map` primitive — useful,
//      not required.
//   5. Reverb is LISTENER-centric here (an emitter carries YOUR room, not its own) — the
//      rooms+portals refinement (hear a source's room through the doorway) is deferred.
// Verdict: ship a cart-land `acoustics.h` helper over existing knobs; the only real ENGINE
// asks are #2 (note_gain float) and #3 (rideable reverb). See STATUS open item + spatial.md v3.

#define CELL 16
#define COLS 20
#define ROWS 12
#define OY   4                       // map y-offset (thin HUD on top)

enum { MAT_TILE, MAT_CARPET, MAT_WOOD, MAT_GRASS };
static const char *MNAME[] = { "TILE", "CARPET", "WOOD", "GRASS" };

static const char *MAP[ROWS] = {
    "####################",
    "#TTTTTTT#CCCCCCCCCC#",
    "#TTTTTTT#CCCCCCCCCC#",
    "#TTTRTTT#CCCCMCCCCC#",
    "#TTTTTTT#CCCCCCCCCC#",
    "#TTTTTTTDCCCCCCCCCC#",   // D = doorway in the dividing wall
    "#TTTTTTT#CCCCCCCCCC#",
    "#WWWWWWWWWWWWWWWWWW#",   // wood corridor (connects both rooms)
    "#WWWWWWWWWWWWWWWWWW#",
    "#GGGGGGGGGGGGGGGGGG#",   // grass strip (nearly dry / outdoors)
    "#GGGGGGGGGGGGGGGGGG#",
    "####################",
};

static float px, py;                 // player == listener (pixel center)
static int   radio_h, machine_h;
static float rx, ry, mx, my;         // emitter pixel centers
static int   cur_mat = -1;           // last-applied zone material (-1 = none yet)
static int   step_t;
static float occ_r = -1, occ_m = -1; // last-applied occlusion per emitter (for change-gating)

static char cell_at(float x, float y) {
    int cx = (int)(x / CELL), cy = (int)((y - OY) / CELL);
    if (cx < 0 || cx >= COLS || cy < 0 || cy >= ROWS) return '#';
    return MAP[cy][cx];
}
static int is_wall(float x, float y) { return cell_at(x, y) == '#'; }
static int mat_at(float x, float y) {
    switch (cell_at(x, y)) {
        case 'C': case 'M': return MAT_CARPET;
        case 'W': case 'D': return MAT_WOOD;
        case 'G':           return MAT_GRASS;
        default:            return MAT_TILE;   // T / R / # → tile-ish
    }
}
// count distinct wall runs crossed listener→source = an occlusion/"thickness" proxy 0..1
static float occlusion(float ax, float ay, float bx, float by) {
    int walls = 0, prev = 0;
    for (int i = 1; i < 28; i++) {
        float t = (float)i / 28.0f;
        int w = is_wall(ax + (bx - ax) * t, ay + (by - ay) * t);
        if (w && !prev) walls++;
        prev = w;
    }
    float o = walls * 0.6f;
    return o > 1.0f ? 1.0f : o;
}

// apply a material's reverb as the zone (called only on room change — set-and-hold)
static void apply_zone(int mat) {
    switch (mat) {
        case MAT_TILE:   reverb(0.80f, 0.18f); break;   // long, bright
        case MAT_CARPET: reverb(0.30f, 0.88f); break;   // short, dark (high HF absorption)
        case MAT_WOOD:   reverb(0.52f, 0.45f); break;   // mid
        default:         reverb(0.08f, 0.50f); break;   // grass ≈ dry/outdoors
    }
}
// a footstep on the current ground (timbre by material; the room reverb colors it)
static void footstep(int mat) {
    switch (mat) {
        case MAT_TILE:   hit_at(84, INSTR_NOISE, 4, 28, px, py); break;  // sharp click
        case MAT_CARPET: hit_at(46, INSTR_NOISE, 3, 75, px, py); break;  // soft thud
        case MAT_WOOD:   hit_at(64, INSTR_NOISE, 4, 45, px, py); break;  // knock
        default:         hit_at(72, INSTR_NOISE, 2, 22, px, py); break;  // grass rustle
    }
}

void init(void) {
    px = 3 * CELL + 8;  py = OY + 4 * CELL + 8;          // start in room A (tile)
    rx = 4 * CELL + 8;  ry = OY + 3 * CELL + 8;          // radio
    mx = 12 * CELL + 8; my = OY + 3 * CELL + 8;          // machine

    instrument(5, INSTR_NOISE, 1, 0, 0, 30);  instrument_reverb(5, 0.4f);   // footsteps → room verb
    instrument(6, INSTR_TRI,  30, 0, 6, 200); instrument_filter(6, FILTER_LOW, 6000, 2); instrument_reverb(6, 0.35f);
    instrument(7, INSTR_SAW,  30, 0, 6, 200); instrument_filter(7, FILTER_LOW, 6000, 2); instrument_reverb(7, 0.30f);

    spatial_model(14.0f, 220.0f, 1.2f);
    spatial_speed(0.0f);                       // Doppler off — this probe is about rooms, not motion
    listener(px, py);
    apply_zone(mat_at(px, py)); cur_mat = mat_at(px, py);

    radio_h   = note_on(64, 6, 5); note_pos(radio_h,   rx, ry);   // a tone in room A
    machine_h = note_on(33, 7, 5); note_pos(machine_h, mx, my);   // a low hum in room B
}

void update(void) {
    float ox = px, oy = py, sp = 1.6f;
    float nx = px, ny = py;
    if (btn(0, BTN_LEFT))  nx -= sp;
    if (btn(0, BTN_RIGHT)) nx += sp;
    if (btn(0, BTN_UP))    ny -= sp;
    if (btn(0, BTN_DOWN))  ny += sp;
    if (!is_wall(nx, py)) px = nx;             // axis-separated wall collision
    if (!is_wall(px, ny)) py = ny;
    int moving = (px != ox || py != oy);

    listener(px, py);

    // ZONE: re-apply the room reverb only when the floor material changes (set-and-hold)
    int mat = mat_at(px, py);
    if (mat != cur_mat) { apply_zone(mat); cur_mat = mat; }

    // footsteps while walking, timbre by ground
    if (moving && ++step_t >= 15) { step_t = 0; footstep(mat); }
    if (!moving) step_t = 14;                  // first step on move is near-immediate

    // OCCLUSION: raycast player→emitter; muffle + quieten through walls. Push the live
    // knobs only when the value actually moved (the "don't reconfigure every frame" rule).
    float orr = occlusion(px, py, rx, ry);
    if (fabsf(orr - occ_r) > 0.02f) {
        note_cutoff(radio_h, (int)(6000.0f - orr * 5400.0f));   // 6000 open → 600 muffled
        note_vol(radio_h, (int)(5.0f - orr * 3.0f + 0.5f));     // 5 → 2 (note: 0..7 is COARSE — a finding)
        occ_r = orr;
    }
    float omm = occlusion(px, py, mx, my);
    if (fabsf(omm - occ_m) > 0.02f) {
        note_cutoff(machine_h, (int)(6000.0f - omm * 5400.0f));
        note_vol(machine_h, (int)(5.0f - omm * 3.0f + 0.5f));
        occ_m = omm;
    }

#ifdef DE_TRACE
    watch("zone",  "%s", MNAME[mat]);
    watch("occ_R", "%.2f", orr);
    watch("occ_M", "%.2f", omm);
#endif
}

static int matcol(int mat) {
    switch (mat) { case MAT_TILE: return CLR_LIGHT_GREY; case MAT_CARPET: return CLR_DARK_PURPLE;
                   case MAT_WOOD: return CLR_DARK_PEACH;  default: return CLR_GREEN; }
}

void draw(void) {
    cls(CLR_BLACK);
    // floor + walls
    for (int cy = 0; cy < ROWS; cy++) for (int cx = 0; cx < COLS; cx++) {
        char c = MAP[cy][cx];
        int x = cx * CELL, y = OY + cy * CELL;
        if (c == '#') { rectfill(x, y, CELL, CELL, CLR_DARKER_GREY); rect(x, y, CELL, CELL, CLR_DARK_GREY); }
        else {
            int m = mat_at(x + 8, y + 8);
            rectfill(x, y, CELL, CELL, matcol(m));
            if (m == MAT_TILE) rect(x, y, CELL, CELL, CLR_WHITE);   // grout lines
        }
    }

    // emitters with expanding rings (brighter = louder/clearer to the listener)
    float t = now() * 30.0f;
    for (int r = 0; r < 3; r++) { int rr = (((int)t + r * 8) % 24); if (rr > 3) circ((int)rx, (int)ry, rr, CLR_YELLOW); }
    rectfill((int)rx - 4, (int)ry - 5, 8, 10, CLR_YELLOW); print("R", (int)rx - 2, (int)ry - 3, CLR_BLACK);
    for (int r = 0; r < 3; r++) { int rr = (((int)t + 4 + r * 8) % 24); if (rr > 3) circ((int)mx, (int)my, rr, CLR_ORANGE); }
    rectfill((int)mx - 4, (int)my - 5, 8, 10, CLR_ORANGE); print("M", (int)mx - 2, (int)my - 3, CLR_BLACK);

    // the little guy (head + body)
    circfill((int)px, (int)py - 2, 3, CLR_PEACH);
    rectfill((int)px - 2, (int)py + 1, 4, 4, CLR_BLUE);

    // HUD
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    char buf[48]; snprintf(buf, sizeof buf, "ACOUSTICS  floor:%s", MNAME[cur_mat < 0 ? 0 : cur_mat]);
    print(buf, 3, 2, CLR_WHITE);
}
