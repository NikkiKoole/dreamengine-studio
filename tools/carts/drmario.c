/* de:meta
{
  "title": "Dr. Mario",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "screen-shake-juice"
  ],
  "lineage": "Dr. Mario (1990) faithful port — falling two-color pill mechanic with a multi-phase resolve loop (control → clear-blink → gravity settle → chain) that is the conceptual heart; chain combos and the rising-arpeggio feedback are its juice layer.",
  "genre": "puzzle",
  "homage": "Dr. Mario (1990)",
  "description": "A virus-busting pill puzzler. The doctor drops two-color capsules into a bottle crawling with red, blue, and yellow viruses — steer them down, twist them around, and line up four or more of one color across or down to wipe them out. Every clear flashes white and kicks the screen; loose capsule-halves rain down afterward and can chain into bigger combos with a rising arpeggio. Empty the bottle of every virus to beat the level and seed a tougher one; choke the neck and you top out. Best score is saved. Left/Right move, Up rotates, Down soft-drops, Z/X start the next level or restart.",
  "todo": [
    "The win panel needs more space — text overlaps."
  ]
}
de:meta */
#include "studio.h"
#include <string.h>

// DR. MARIO
// Drop two-color pills into a virus-infested bottle. Line up 4+ of one color
// (across or down) to clear them. Clear EVERY virus to beat the level; let the
// pills choke the neck and you top out. After a clear, loose halves fall — chains!
//
// Left/Right: move    Up: rotate    Down: soft drop    Z/X: next level / restart

// ---- bottle grid -------------------------------------------------------------
#define BW    8           // bottle width  (cells)
#define BH   16           // bottle height
#define CELL 10
#define BX  136           // bottle pixel origin (x)
#define BY   28           // bottle pixel origin (y)
#define NCOL 3            // red / blue / yellow

// a cell is 0 = empty, else encodes color + kind.
// low 2 bits = color (1..3), high bits = kind tag.
#define COLOR(v)   ((v) & 3)
#define KIND(v)    ((v) & 0x1C)
#define K_VIRUS    0x04   // a virus
#define K_SINGLE   0x08   // a lone capsule half (no partner)
#define K_LEFT     0x0C   // half whose partner is to the RIGHT
#define K_RIGHT    0x10   // half whose partner is to the LEFT
#define K_UP       0x14   // half whose partner is ABOVE
#define K_DOWN     0x18   // half whose partner is BELOW
#define MK(kind,color) ((kind) | (color))

static const int CCOL[4]  = { CLR_BLACK, CLR_RED, CLR_BLUE, CLR_YELLOW };
static const int CCOL2[4] = { CLR_BLACK, CLR_DARK_RED, CLR_TRUE_BLUE, CLR_ORANGE }; // shade

static int grid[BH][BW];        // settled cells
static int viruses;             // remaining virus count
static int score, hiscore, level;
static int state;               // 0 play, 1 cleared(win), 2 over

// falling pill: two halves a (pivot) + b
static int pa_c, pb_c;          // colors 1..3
static int rot;                 // 0 horiz(b right), 1 vert(b up), 2 horiz(b left), 3 vert(b down)
static int px, py;              // pivot cell
static int fall_count;
static int das_dir, das_t;

// resolve / juice timing
static int flash_t;             // white flash after a clear
static int shk_t;
static int hitstop;
static int chain;               // current chain depth while resolving
static int phase;               // 0 = pill control, 1 = clearing (blink), 2 = gravity settle
static int phase_t;
static bool marked[BH][BW];     // cells flagged for clearing

// ------------------------------------------------------------------------------
static int rndcol(void) { return 1 + rnd(NCOL); }

// compute b's cell offset from pivot for a rotation
static void boff(int r, int *dx, int *dy) {
    switch (r) {
        case 0: *dx =  1; *dy =  0; break;  // b to the right
        case 1: *dx =  0; *dy = -1; break;  // b above
        case 2: *dx = -1; *dy =  0; break;  // b to the left
        default:*dx =  0; *dy =  1; break;  // b below
    }
}

static bool free_cell(int x, int y) {
    if (x < 0 || x >= BW || y >= BH) return false;
    if (y < 0) return true;                 // above the neck is OK while falling in
    return grid[y][x] == 0;
}

static bool pill_fits(int r, int x, int y) {
    int dx, dy; boff(r, &dx, &dy);
    return free_cell(x, y) && free_cell(x + dx, y + dy);
}

static void spawn_pill(void) {
    pa_c = rndcol(); pb_c = rndcol();
    rot = 0; px = 3; py = 0;
    fall_count = 0;
    phase = 0;
    if (!pill_fits(rot, px, py)) {
        state = 2;
        if (score > hiscore) { hiscore = score; save(0, hiscore); }
        note(36, INSTR_NOISE, 5);
    }
}

static void seed_level(int lv) {
    memset(grid, 0, sizeof(grid));
    int want = 4 + lv * 4;
    if (want > 56) want = 56;            // leave breathing room in an 8x16 bottle
    // viruses live in the lower 11 rows (leave the neck clear)
    int placed = 0, guard = 0;
    while (placed < want && guard < 4000) {
        guard++;
        int x = rnd(BW);
        int y = 5 + rnd(BH - 5);
        if (grid[y][x]) continue;
        grid[y][x] = MK(K_VIRUS, 1 + rnd(NCOL));
        placed++;
    }
    viruses = placed;
}

static void start_level(int lv) {
    level = lv;
    state = 0; phase = 0; chain = 0;
    flash_t = shk_t = hitstop = 0;
    das_dir = 0;
    seed_level(lv);
    spawn_pill();
}

void init(void) {
    hiscore = load(0);
    score = 0;
    start_level(1);
    touch_layout(TOUCH_DPAD4, 1);
}

// ---- pill control -------------------------------------------------------------
static void rotate(void) {
    int nr = (rot + 1) & 3;
    if (pill_fits(nr, px, py)) { rot = nr; note(67, INSTR_SQUARE, 2); return; }
    // wall kick: nudge left one
    if (pill_fits(nr, px - 1, py)) { rot = nr; px--; note(67, INSTR_SQUARE, 2); return; }
}

static bool move_pill(int dx, int dy) {
    if (pill_fits(rot, px + dx, py + dy)) { px += dx; py += dy; return true; }
    return false;
}

static void lock_pill(void) {
    int dx, dy; boff(rot, &dx, &dy);
    int ax = px, ay = py, bx = px + dx, by = py + dy;
    // kinds based on relative position
    int ka, kb;
    if (dy == 0) {                       // horizontal: a-b side by side
        if (dx == 1) { ka = K_LEFT;  kb = K_RIGHT; }
        else         { ka = K_RIGHT; kb = K_LEFT;  }
    } else {                             // vertical
        if (dy == -1) { ka = K_DOWN; kb = K_UP;   } // a below, b above
        else          { ka = K_UP;   kb = K_DOWN; } // a above, b below
    }
    if (ay >= 0) grid[ay][ax] = MK(ka, pa_c);
    if (by >= 0) grid[by][bx] = MK(kb, pb_c);
    hit(40, INSTR_SQUARE, 3, 70);
}

// ---- match scan ---------------------------------------------------------------
// when a half is cleared, its partner becomes a lone single (so gravity works)
static void orphan_partner(int x, int y) {
    int v = grid[y][x];
    if (!v) return;
    int k = KIND(v), nx = x, ny = y;
    switch (k) {
        case K_LEFT:  nx = x + 1; break;
        case K_RIGHT: nx = x - 1; break;
        case K_UP:    ny = y + 1; break;
        case K_DOWN:  ny = y - 1; break;
        default: return;
    }
    if (nx >= 0 && nx < BW && ny >= 0 && ny < BH && grid[ny][nx])
        grid[ny][nx] = MK(K_SINGLE, COLOR(grid[ny][nx]));
}

static int scan_matches(void) {
    memset(marked, 0, sizeof(marked));
    int found = 0;
    // horizontal runs
    for (int y = 0; y < BH; y++) {
        int run = 1;
        for (int x = 1; x <= BW; x++) {
            bool same = x < BW && grid[y][x] && COLOR(grid[y][x]) == COLOR(grid[y][x-1]) && grid[y][x-1];
            if (same) run++;
            else {
                if (run >= 4) for (int k = x - run; k < x; k++) marked[y][k] = true;
                run = 1;
            }
        }
    }
    // vertical runs
    for (int x = 0; x < BW; x++) {
        int run = 1;
        for (int y = 1; y <= BH; y++) {
            bool same = y < BH && grid[y][x] && COLOR(grid[y][x]) == COLOR(grid[y-1][x]) && grid[y-1][x];
            if (same) run++;
            else {
                if (run >= 4) for (int k = y - run; k < y; k++) marked[k][x] = true;
                run = 1;
            }
        }
    }
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            if (marked[y][x]) found++;
    return found;
}

static void apply_clear(void) {
    int cleared_viruses = 0;
    // first orphan all partners that survive, then erase marked cells
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            if (marked[y][x]) orphan_partner(x, y);
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            if (marked[y][x]) {
                if (KIND(grid[y][x]) == K_VIRUS) cleared_viruses++;
                grid[y][x] = 0;
            }
    viruses -= cleared_viruses;
    if (viruses < 0) viruses = 0;
    int gain = 0;
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            if (marked[y][x]) gain += 10 * (chain + 1);
    score += gain;
}

// move loose singles down one step; return true if anything moved.
// connected pairs fall together only if BOTH have space below.
static bool gravity_step(void) {
    bool moved = false;
    for (int y = BH - 2; y >= 0; y--) {
        for (int x = 0; x < BW; x++) {
            int v = grid[y][x];
            if (!v || KIND(v) == K_VIRUS) continue;
            int k = KIND(v);
            if (k == K_SINGLE) {
                if (grid[y+1][x] == 0) { grid[y+1][x] = v; grid[y][x] = 0; moved = true; }
            } else if (k == K_UP) {
                // partner is below at y+1; we are the top. handled when we reach the bottom half.
                // skip: bottom half (K_DOWN) drives the pair.
            } else if (k == K_DOWN) {
                // we are bottom half, partner above (K_UP) at y-1
                if (y + 1 < BH && grid[y+1][x] == 0) {
                    grid[y+1][x] = v; grid[y][x] = grid[y-1][x]; grid[y-1][x] = 0; moved = true;
                }
            } else if (k == K_LEFT) {
                // partner to the right (K_RIGHT). both drop if both have floor.
                if (x + 1 < BW && grid[y+1][x] == 0 && grid[y+1][x+1] == 0) {
                    grid[y+1][x] = v; grid[y][x] = 0;
                    grid[y+1][x+1] = grid[y][x+1]; grid[y][x+1] = 0; moved = true;
                }
            } else if (k == K_RIGHT) {
                // driven by its K_LEFT partner above; nothing here
            }
        }
    }
    return moved;
}

void update(void) {
    if (flash_t > 0) flash_t--;
    if (shk_t > 0) shk_t--;

    if (state != 0) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B)) {
            if (state == 1) start_level(level + 1);
            else            { score = 0; start_level(1); }
        }
        return;
    }

    // hit-stop: freeze briefly after a big clear
    if (hitstop > 0) { hitstop--; return; }

    // ---- resolve phase (clear + gravity chain) -------------------------------
    if (phase == 1) {                    // showing the blink, then erase
        if (--phase_t <= 0) {
            apply_clear();
            if (viruses == 0) {
                state = 1;
                strum(60, CHORD_MAJ, INSTR_SINE, 5, 60);
                if (score > hiscore) { hiscore = score; save(0, hiscore); }
                return;
            }
            phase = 2; phase_t = 4;      // settle gravity
        }
        return;
    }
    if (phase == 2) {
        if (--phase_t <= 0) {
            if (gravity_step()) { phase_t = 4; }   // keep settling
            else {
                int n = scan_matches();            // chain re-scan
                if (n > 0) {
                    chain++;
                    hit(72 + chain * 3, INSTR_SINE, 5, 120);
                    flash_t = 6; shk_t = 6;
                    phase = 1; phase_t = 8;
                } else {
                    chain = 0;
                    spawn_pill();                  // back to control
                }
            }
        }
        return;
    }

    // ---- pill control --------------------------------------------------------
    int dir = (btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0);
    if (dir != 0) {
        if (dir != das_dir) { move_pill(dir, 0); das_dir = dir; das_t = 10; }
        else if (--das_t <= 0) { move_pill(dir, 0); das_t = 3; }
    } else das_dir = 0;

    if (btnp(0, BTN_UP) || btnp(0, BTN_A)) rotate();

    bool soft = btn(0, BTN_DOWN);
    int delay = max(4, 40 - (level - 1) * 3);
    if (soft) delay = 2;
    if (++fall_count >= delay) {
        fall_count = 0;
        if (!move_pill(0, 1)) {
            lock_pill();
            int n = scan_matches();
            if (n > 0) {
                chain = 0;
                if (n >= 4) { hitstop = 5; shk_t = 8; }
                flash_t = 6; shk_t = max(shk_t, 6);
                hit(72, INSTR_SINE, 5, 120);
                phase = 1; phase_t = 8;
            } else {
                spawn_pill();
            }
        } else if (soft) score += 1;
    }
}

// ---- drawing ------------------------------------------------------------------
static void draw_half(int sx, int sy, int color, int kind) {
    int c = CCOL[color], c2 = CCOL2[color];
    // rounded capsule cell
    rectfill(sx + 1, sy + 1, CELL - 2, CELL - 2, c);
    // flatten the joint side so a pair reads as one capsule
    rectfill(sx, sy + 2, CELL, CELL - 4, c);
    rectfill(sx + 2, sy, CELL - 4, CELL, c);
    // shading + highlight
    rectfill(sx + 2, sy + CELL - 3, CELL - 4, 1, c2);
    pset(sx + 2, sy + 2, CLR_WHITE);
    (void)kind;
}

static void draw_virus(int sx, int sy, int color) {
    int c = CCOL[color];
    int wob = (anim(2, 3, (float)(sx + sy) / 40.0f)) ? 0 : 1;
    int cx = sx + CELL / 2, cy = sy + CELL / 2 + wob;
    circfill(cx, cy, CELL / 2 - 1, c);
    // little antennae + eyes
    pset(cx - 2, cy - 3, c);
    pset(cx + 2, cy - 3, c);
    pset(cx - 2, cy - 1, CLR_WHITE);
    pset(cx + 2, cy - 1, CLR_WHITE);
    pset(cx - 2, cy - 1, CLR_BLACK);
    pset(cx + 2, cy - 1, CLR_BLACK);
    line(cx - 2, cy + 2, cx + 2, cy + 2, CLR_BLACK);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    int ox = 0, oy = 0;
    if (shk_t > 0) { ox = rnd_between(-2, 3); oy = rnd_between(-2, 3); }
    camera(ox, oy);

    // bottle well
    rectfill(BX - 3, BY - 3, BW * CELL + 6, BH * CELL + 6, CLR_BLACK);
    rect(BX - 3, BY - 3, BW * CELL + 6, BH * CELL + 6, CLR_LIGHT_GREY);
    rect(BX - 2, BY - 2, BW * CELL + 4, BH * CELL + 4, CLR_INDIGO);
    // neck opening
    rectfill(BX + 3 * CELL - 1, BY - 7, 2 * CELL + 2, 5, CLR_BLACK);
    rect(BX + 3 * CELL - 1, BY - 7, 2 * CELL + 2, 5, CLR_LIGHT_GREY);

    // settled cells
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
            int v = grid[y][x];
            if (!v) continue;
            int sx = BX + x * CELL, sy = BY + y * CELL;
            bool blink_off = (phase == 1 && marked[y][x] && (frame() / 2) % 2);
            if (blink_off) { rectfill(sx + 2, sy + 2, CELL - 4, CELL - 4, CLR_WHITE); continue; }
            if (KIND(v) == K_VIRUS) draw_virus(sx, sy, COLOR(v));
            else                    draw_half(sx, sy, COLOR(v), KIND(v));
        }

    // active falling pill
    if (state == 0 && phase == 0) {
        int dx, dy; boff(rot, &dx, &dy);
        if (py >= 0) draw_half(BX + px * CELL, BY + py * CELL, pa_c, K_SINGLE);
        if (py + dy >= 0) draw_half(BX + (px + dx) * CELL, BY + (py + dy) * CELL, pb_c, K_SINGLE);
    }

    camera(0, 0);

    // clear flash
    if (flash_t > 0 && (flash_t / 2) % 2)
        rectfill(BX, BY, BW * CELL, BH * CELL, CLR_WHITE);

    // ---- HUD -----------------------------------------------------------------
    print("DR. MARIO", 12, 14, CLR_GREEN);
    print("LEVEL", 16, 40, CLR_LIGHT_GREY);
    print(str("%d", level), 16, 50, CLR_WHITE);
    print("VIRUS", 16, 70, CLR_LIGHT_GREY);
    print(str("%d", viruses), 16, 80, CLR_LIME_GREEN);
    print("SCORE", 16, 100, CLR_LIGHT_GREY);
    print(str("%d", score), 16, 110, CLR_WHITE);
    print("BEST", 16, 130, CLR_LIGHT_GREY);
    print(str("%d", hiscore), 16, 140, CLR_YELLOW);

    // next pill preview
    print("NEXT", 234, 40, CLR_LIGHT_GREY);
    draw_half(234, 52, pa_c, K_SINGLE);
    draw_half(234 + CELL, 52, pb_c, K_SINGLE);

    // legend
    for (int i = 0; i < NCOL; i++) {
        int ly = 110 + i * 12;
        circfill(248, ly + 4, 4, CCOL[i + 1]);
    }

    if (state == 1) {
        rectfill(BX - 6, SCREEN_H / 2 - 26, BW * CELL + 12, 54, CLR_BLACK);
        rect    (BX - 6, SCREEN_H / 2 - 26, BW * CELL + 12, 54, CLR_GREEN);
        print_centered("LEVEL CLEAR!", SCREEN_W/2, SCREEN_H / 2 - 16, CLR_GREEN);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H / 2 - 2, CLR_YELLOW);
        print_centered("Z: next level", SCREEN_W/2, SCREEN_H / 2 + 12, CLR_LIGHT_GREY);
    } else if (state == 2) {
        rectfill(BX - 6, SCREEN_H / 2 - 26, BW * CELL + 12, 54, CLR_BLACK);
        rect    (BX - 6, SCREEN_H / 2 - 26, BW * CELL + 12, 54, CLR_RED);
        print_centered("GAME OVER", SCREEN_W/2, SCREEN_H / 2 - 16, CLR_RED);
        print_centered(str("SCORE %d", score), SCREEN_W/2, SCREEN_H / 2 - 2, CLR_YELLOW);
        print_centered("Z: restart", SCREEN_W/2, SCREEN_H / 2 + 12, CLR_LIGHT_GREY);
    }
}
