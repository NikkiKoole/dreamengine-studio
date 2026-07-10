/* de:meta
{
  "slug": "gnomeseek",
  "title": "Gnome Hide & Eek",
  "status": "active",
  "created": "2026-06-10",
  "kind": [
    "game",
    "tech-demo"
  ],
  "teaches": [
    "particle-system"
  ],
  "lineage": "Original cart built as the showcase for ui.h's loupe/magnifier primitive; the hidden-object genre is the framing device, the magnifier routing (ui_loupe_over/ui_loupe_map) is the point.",
  "genre": "puzzle",
  "description": "A forest hidden-object game that IS the ui.h loupe showcase. Little gnomes (red hat, white beard) are tucked among the trees — you can see them, but they're 3-4px, far too small to tap on a phone. Drag the corner box to summon a 3x magnifier lens, sweep it over the forest, and tap a gnome INSIDE the lens to catch it (it throws its arms up - eek!). A tap only counts through the lens, so the magnifier isn't an aid here, it's the whole game. Catch all 6; R = new forest. The gnomes are cart-drawn game objects (not widgets), so it routes taps via ui_loupe_over/ui_loupe_map while the lens visual comes free from the zoom_rect engine primitive. See docs/design/loupe-notes.md."
}
de:meta */
// gnomeseek.c — "Gnome Hide & Eek": a showcase for the ui.h loupe.
//
// A forest of tiny pixels. Hidden in it are little gnomes (red hat, white
// beard) — you can SEE them, but they're 3–4 px, far too small to tap on a
// phone. Drag the corner ⊕ to summon the 3× lens, sweep it over the trees,
// and tap a gnome INSIDE the magnified view to catch it (it throws its arms up
// — eek!). A tap only counts through the lens, so the magnifier isn't an aid
// here, it's the whole game. R = new forest.
//
// How it works with the loupe: gnomes are cart-drawn game objects (not ui.h
// widgets), so the cart routes taps itself — `ui_loupe_over()` tells it a tap
// landed in the lens and `ui_loupe_map()` converts it to board space. The lens
// VISUAL is free (ui.h magnifies a copy of the whole canvas via zoom_rect), so
// the gnomes, trees and sparkles all show magnified with zero extra work.

#include "studio.h"
#include "ui.h"

#define GROUND_Y 128
#define NTREE  8
#define NBUSH  13
#define NGRASS 48
#define NGNOME 6

// ── deterministic forest (own LCG so R rerolls cleanly) ───────────────────
static unsigned int g_seed = 7;
static unsigned int rng;
static int rr(int lo, int hi) { rng = rng * 1103515245u + 12345u; return lo + (int)((rng >> 16) % (unsigned)(hi - lo + 1)); }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static struct { int x, y, r; } tree[NTREE];
static struct { int x, y, r; } bush[NBUSH];
static struct { int x, y; }    grass[NGRASS];
static struct { int x, y; unsigned char found; float pop; } gnome[NGNOME];
static int found_n = 0, won_glow = 0;

#define NP 60
static struct { float x, y, vx, vy; int life, col; } pcl[NP];

static void spawn_sparkle(int px, int py) {
    for (int k = 0; k < 12; k++)
        for (int i = 0; i < NP; i++)
            if (pcl[i].life <= 0) {
                pcl[i].x = px; pcl[i].y = py;
                pcl[i].vx = rr(-15, 15) / 10.0f;
                pcl[i].vy = rr(-22, -4) / 10.0f;
                pcl[i].life = rr(14, 26);
                pcl[i].col = (k & 1) ? CLR_YELLOW : CLR_WHITE;
                break;
            }
}

static void gen(void) {
    rng = g_seed * 2654435761u + 101;
    for (int i = 0; i < NTREE; i++)  { tree[i].x = rr(16, SCREEN_W - 16); tree[i].y = rr(46, GROUND_Y - 8); tree[i].r = rr(11, 18); }
    for (int i = 0; i < NBUSH; i++)  { bush[i].x = rr(6, SCREEN_W - 6);   bush[i].y = rr(GROUND_Y - 6, SCREEN_H - 6); bush[i].r = rr(5, 9); }
    for (int i = 0; i < NGRASS; i++) { grass[i].x = rr(0, SCREEN_W);      grass[i].y = rr(GROUND_Y, SCREEN_H - 2); }
    // tuck each gnome against a tree base or bush so it blends into foliage
    for (int i = 0; i < NGNOME; i++) {
        int useTree = rr(0, 1);
        int ax, ay, ar;
        if (useTree) { int t = rr(0, NTREE - 1); ax = tree[t].x; ay = tree[t].y + tree[t].r - 2; ar = tree[t].r; }
        else         { int b = rr(0, NBUSH - 1); ax = bush[b].x; ay = bush[b].y;                 ar = bush[b].r; }
        gnome[i].x = clampi(ax + rr(-ar, ar), 10, SCREEN_W - 10);
        gnome[i].y = clampi(ay + rr(-ar / 2, ar), 56, SCREEN_H - 8);
        gnome[i].found = 0; gnome[i].pop = 0;
    }
    for (int i = 0; i < NP; i++) pcl[i].life = 0;
    found_n = 0; won_glow = 0;
}

// ── gnome sprite (tiny; found ones bounce + raise arms) ───────────────────
static void draw_gnome(int x, int y, int found, float pop) {
    int dy = found ? -(int)(pop * 5.0f) : 0;
    int gy = y + dy;
    if (found) {
        trifill(x - 1, gy - 1, x + 1, gy - 1, x, gy - 3, CLR_RED);  // pointy hat
        pset(x - 2, gy, CLR_PEACH); pset(x + 2, gy, CLR_PEACH);     // arms up — eek!
    } else {
        pset(x, gy - 2, CLR_RED);                                   // hat tip
        pset(x - 1, gy - 1, CLR_RED); pset(x, gy - 1, CLR_RED); pset(x + 1, gy - 1, CLR_RED);  // brim
    }
    pset(x, gy, CLR_PEACH);       // face
    pset(x, gy + 1, CLR_WHITE);   // beard
}

// ── input: catch gnomes tapped INSIDE the lens ────────────────────────────
static int prevId[16], prevN;
static int was_prev(int id) { for (int i = 0; i < prevN; i++) if (prevId[i] == id) return 1; return 0; }

static void catch_taps(void) {
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        if (was_prev(id)) continue;              // only a fresh press
        int tx = touch_x(i), ty = touch_y(i);
        if (!ui_loupe_over(tx, ty)) continue;    // must be tapped through the lens
        int bx = tx, by = ty; ui_loupe_map(&bx, &by);
        for (int g = 0; g < NGNOME; g++) {
            if (gnome[g].found) continue;
            int dx = bx - gnome[g].x, dy = by - gnome[g].y;
            if (dx * dx + dy * dy <= 16) {       // within ~4 px of the gnome
                gnome[g].found = 1; gnome[g].pop = 1.0f; found_n++;
                spawn_sparkle(gnome[g].x, gnome[g].y);
                hit(69 + found_n * 3, INSTR_MALLET, 5, 220);   // rising celesta "eek!"
                if (found_n == NGNOME) {                       // fanfare
                    won_glow = 120;
                    hit(84, INSTR_MALLET, 5, 320); hit(88, INSTR_MALLET, 4, 360); hit(91, INSTR_MALLET, 4, 460);
                }
                break;
            }
        }
    }
    prevN = 0;
    for (int i = 0; i < touch_count() && prevN < 16; i++) prevId[prevN++] = touch_id(i);
}

void update(void) {
    static int started = 0;
    if (!started) { started = 1; gen(); }
    if (keyp('R')) { g_seed++; gen(); }
    for (int g = 0; g < NGNOME; g++) gnome[g].pop = lerp(gnome[g].pop, 0, 0.18f);
    for (int i = 0; i < NP; i++)
        if (pcl[i].life > 0) { pcl[i].x += pcl[i].vx; pcl[i].y += pcl[i].vy; pcl[i].vy += 0.12f; pcl[i].life--; }
    if (won_glow > 0) won_glow--;
}

void draw(void) {
    ui_begin();
    ui_loupe(1);                                 // <-- the magnifier (handle + lens)
    static int seeded = 0;
    if (!seeded) { seeded = 1; ui_loupe_at(gnome[0].x, gnome[0].y); }  // park over a gnome for the thumbnail
    catch_taps();

    // ── forest ──
    cls(CLR_DARK_GREEN);
    rectfill(0, 0, SCREEN_W, 34, CLR_DARK_BLUE);                 // shaded canopy depth
    rectfill(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, CLR_DARK_GREEN);
    for (int i = 0; i < SCREEN_W; i += 3) pset(i, GROUND_Y, CLR_DARK_GREY); // forest floor line
    for (int i = 0; i < NTREE; i++) {                            // trunks + foliage
        rectfill(tree[i].x - 2, tree[i].y, 4, GROUND_Y - tree[i].y, CLR_BROWN);
        circfill(tree[i].x, tree[i].y, tree[i].r, CLR_DARK_GREEN);
        circfill(tree[i].x - tree[i].r / 2, tree[i].y - 2, tree[i].r * 2 / 3, CLR_GREEN);
        circfill(tree[i].x + tree[i].r / 2, tree[i].y + 2, tree[i].r / 2, CLR_DARK_GREEN);
    }
    for (int i = 0; i < NBUSH; i++) { circfill(bush[i].x, bush[i].y, bush[i].r, CLR_GREEN); circfill(bush[i].x - 1, bush[i].y - 1, bush[i].r / 2, CLR_DARK_GREEN); }
    for (int i = 0; i < NGRASS; i++) line(grass[i].x, grass[i].y, grass[i].x, grass[i].y - 3, CLR_GREEN);

    for (int g = 0; g < NGNOME; g++) draw_gnome(gnome[g].x, gnome[g].y, gnome[g].found, gnome[g].pop);
    for (int i = 0; i < NP; i++) if (pcl[i].life > 0) pset((int)pcl[i].x, (int)pcl[i].y, pcl[i].col);

    // ── HUD ──
    print("GNOME HIDE & EEK", 4, 4, CLR_WHITE);
    print(str("caught %d/%d", found_n, NGNOME), 4, 14, CLR_YELLOW);
    if (found_n == NGNOME) {
        if ((won_glow / 4) % 2 == 0) rect(1, 1, SCREEN_W - 2, SCREEN_H - 2, CLR_YELLOW);
        print_centered("ALL GNOMES FOUND!", SCREEN_W / 2, SCREEN_H / 2 - 6, CLR_WHITE);
        print_centered("R = new forest", SCREEN_W / 2, SCREEN_H / 2 + 4, CLR_YELLOW);
    } else {
        print("drag the lens, tap gnomes in it", 4, SCREEN_H - 10, CLR_LIGHT_GREY);
    }

#ifdef DE_TRACE
    watch("found", "%d", found_n);
    watch("g0", "%d,%d", gnome[0].x, gnome[0].y);
#endif
    ui_end();                                    // draws the lens (magnified copy of the forest)
}
