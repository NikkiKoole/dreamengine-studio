/* de:meta
{
  "title": "isoroom",
  "slug": "isoroom",
  "kind": ["probe", "tech-demo"],
  "teaches": ["isometric-projection"],
  "created": "2026-08-12",
  "lineage": "A renderer probe, not a game: gates whether a Sims-style life sim is buildable here by testing whether ONE voxel model can supply every view rotation without paying triangles at runtime. The pipeline (author once, pre-render each rotation to sprites at build time) is the one RollerCoaster Tycoon and The Sims 1 both shipped; the novelty is doing it at 320x200 in 32 colours, with a 16px figure, and with the whole object set inside a single 128x128 sheet.",
  "todo": [
    "The on-device frame number is still unmeasured (ios/measure-device.sh). On a Mac the full render is ~0.68ms, about 4% of a 60fps frame, but iOS is the number that decides anything.",
    "Objects turned by a FACING that span more than one tile do not move their footprint with them: cell = (r + facing) & 3 picks the right ART, but a 2-tile sofa turned 90 degrees still claims its original two tiles. Fine for a shape test, wrong for a game.",
    "No multi-storey. Deferred on purpose: a second floor changes both the depth sort and the wall-cut rules.",
    "The character walks a fixed patrol, not a path. Occlusion is what is being tested, not navigation.",
    "At 4 voxels per tile the objects are very coarse and several read only by silhouette. That is forced, not lazy: a crisp 2:1 diamond needs ISO_TW to be a multiple of 4, so a 4x2px voxel is the floor and a smaller picture means fewer voxels, not smaller ones."
  ],
  "description": {
    "summary": "A rotating isometric room, drawn entirely from sprite cells pre-rendered from voxel models at build time. Turn it four ways and nothing is a triangle.",
    "detail": "The probe behind a possible Sims-style sim. A rotating isometric view normally costs one drawing per object per rotation, which is where such a project dies; rendering the voxels live instead costs ~89ms/frame on an iPhone (ADR-0024), which is where the other version dies. So every object here is authored ONCE as ASCII voxel layers and baked into all four rotations at build time, and the runtime is nothing but sspr() and a painter's sort. Light is fixed in SCREEN space rather than world space, so it does not swing around as you turn the room, and every object carries a contact shadow so it stands on the floor instead of being pasted onto it. Walls are full height with the near side cut away, or low stubs that never occlude. An earlier cut also carried the four CARDINAL views, for eight in total; they were removed because with no X/Y mixing there is no depth cue left, so objects flattened into slabs and edge-on walls survived as 1-2px bars.",
    "controls": "Q/E turn the room one step (four in total). W switches walls between FULL+cutaway and LOW. Move the mouse to pick a floor tile; the readout names it. TAB shows the depth-sort order. SPACE pauses the walker."
  }
}
de:meta */

// isoroom — can a rotating isometric room run at 2D-only cost?
//
// Design, oracles and the go/no-go: docs/design/iso-rooms.md
// The art pipeline: tools/voxel-bake.js + tools/voxel-models/room.js
// The generated cell table: runtime/isoroom/atlas.h (do not hand-edit)
//
// THE ONE THING TO KNOW IF YOU EDIT THIS: iso_project() below must stay bit-for-bit in
// step with projector() in tools/voxel-bake.js. The baker decided where each object's
// pixels sit inside its cell; this decides where that cell goes on screen. Let them drift
// and furniture floats off the floor with nothing reporting an error. spec() pins the pair
// by round-tripping every tile at every rotation, so run `node tools/spec.js isoroom`
// after touching either.

#include "studio.h"
#include <stdio.h>              // snprintf, for the HUD + spec messages
#include <stdlib.h>             // qsort, the painter's-order sort
#include <math.h>               // floorf/fabsf
#include "spec.h"
#include "isoroom/atlas.h"

// ── the room ──────────────────────────────────────────────────
#define TILE_VOX  6                  // voxels per floor tile — the scale the models assume
// Sized to the canvas: the room's diamond spans (ROOM_W + ROOM_H) * TILE_PX/2 across and half
// that down, plus the wall height. At 24px tiles and 14+10 that is 288 of the 320 available.
#define ROOM_W    14
#define ROOM_H    10
#define TILE_PX   (TILE_VOX * ISO_TW / 2)   // a tile's screen width: 24px

typedef struct { unsigned char model, tx, ty, facing; } Placed;

// A one-room flat. Facing is in quarter turns; see the note in de:meta.todo about
// multi-tile objects and facing.
static const Placed PLACED[] = {
    // kitchen run along the north wall
    { ISO_FRIDGE,   0,  0, 0 },
    { ISO_COUNTER,  1,  0, 0 },
    { ISO_COUNTER,  2,  0, 0 },
    { ISO_COUNTER,  3,  0, 0 },
    // living end
    { ISO_SOFA,     6,  1, 0 },
    { ISO_SOFA,     9,  5, 0 },
    { ISO_COUNTER,  7,  4, 0 },
    // bedroom, south-west
    { ISO_BED,      1,  4, 0 },
    { ISO_BED,      3,  4, 0 },
    { ISO_COUNTER,  0,  7, 0 },
    // bathroom, south-east
    { ISO_TOILET,  12,  8, 0 },
    { ISO_TOILET,  11,  8, 0 },
    { ISO_COUNTER, 13,  5, 1 },
    { ISO_FRIDGE,  13,  0, 0 },
    { ISO_BED,      6,  8, 0 },
};
#define N_PLACED ((int)(sizeof PLACED / sizeof PLACED[0]))

// ── state ─────────────────────────────────────────────────────
static int   rot       = 0;           // 0..3 — FOUR views, all of them 45-degree diagonals
// FULL is the default because it plainly won the comparison: low stubs read as a picture frame
// around a floor, while full walls with the near side cut away read as an interior. W toggles.
static int   full_wall = 1;           // 0 = low stubs (no cutaway), 1 = full + cutaway
static int   show_order = 0;
static int   walk_paused = 0;         // NOT `paused` — studio.h already has a paused() built-in
static float walk_t    = 0;
static int   pick_tx = -1, pick_ty = -1;
static int   drawn_cells = 0;         // per-frame sspr count, for the budget question

// ── projection ────────────────────────────────────────────────
// Mirrors tools/voxel-bake.js projector() for its DIAGONAL family. A rotation is a quarter turn
// of the world plane, then the fixed 2:1 diamond map.
static void iso_turn(int q, float x, float y, float *X, float *Y) {
    switch (q & 3) {
        case 0: *X =  x; *Y =  y; break;
        case 1: *X = -y; *Y =  x; break;
        case 2: *X = -x; *Y = -y; break;
        default:*X =  y; *Y = -x; break;
    }
}

static void iso_unturn(int q, float X, float Y, float *x, float *y) {
    switch (q & 3) {
        case 0: *x =  X; *y =  Y; break;
        case 1: *x =  Y; *y = -X; break;
        case 2: *x = -X; *y = -Y; break;
        default:*x = -Y; *y =  X; break;
    }
}

// Voxel-space point -> screen offset (before the camera shift). Always the 2:1 diamond: the
// cardinal family was cut (iso-rooms.md §7), so a rotation is just a quarter turn of the world.
static void iso_project(int r, float vx, float vy, float vz, float *sx, float *sy) {
    float X, Y; iso_turn(r, vx, vy, &X, &Y);
    *sx = (X - Y) * (ISO_TW * 0.5f);
    *sy = (X + Y) * (ISO_TH * 0.5f) - vz * ISO_ZH;
}

// Screen offset -> voxel-space point on the FLOOR (z = 0). The exact inverse of the above,
// which is what makes picking land on the tile you are pointing at.
static void iso_unproject(int r, float sx, float sy, float *vx, float *vy) {
    float d = sx / (ISO_TW * 0.5f);                // X - Y
    float s = sy / (ISO_TH * 0.5f);                // X + Y
    float X = (s + d) * 0.5f, Y = (s - d) * 0.5f;
    iso_unturn(r, X, Y, vx, vy);
}

// Larger = nearer the camera. Mirrors the baker's depth().
static float iso_depth(int r, float vx, float vy, float vz) {
    float X, Y; iso_turn(r, vx, vy, &X, &Y);
    return X + Y + vz * 0.001f;
}

// ── camera ────────────────────────────────────────────────────
// Centre the room's screen bounding box. Recomputed per frame because it changes with the
// rotation; cheap, and it means no per-rotation offset table to get wrong.
static float cam_x, cam_y;
static void iso_camera(int r) {
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    const float W = ROOM_W * TILE_VOX, H = ROOM_H * TILE_VOX;
    const float TOP = full_wall ? 12.0f : 4.0f;    // tallest thing that needs to fit, in voxels
    for (int c = 0; c < 8; c++) {
        float vx = (c & 1) ? W : 0, vy = (c & 2) ? H : 0, vz = (c & 4) ? TOP : 0;
        float sx, sy; iso_project(r, vx, vy, vz, &sx, &sy);
        if (sx < minx) minx = sx; if (sx > maxx) maxx = sx;
        if (sy < miny) miny = sy; if (sy > maxy) maxy = sy;
    }
    // FLOORED TO WHOLE PIXELS, and this is load-bearing rather than tidiness. Centring produces a
    // .5 offset whenever the room's bounding box is an odd number of pixels wide, and every cell is
    // then placed with (int)(sx + cam_x), which truncates. Two adjacent objects whose sx differ by
    // an odd amount round OPPOSITE ways, and a one-pixel seam of floor opens along the tile edge
    // between them — visible at some rotations and not others, because the bounding box's parity
    // changes with the rotation. An integer camera puts every sprite on the same subpixel phase and
    // removes the whole class.
    cam_x = floorf((SCREEN_W - (maxx - minx)) * 0.5f - minx);
    cam_y = floorf((SCREEN_H - 14 - (maxy - miny)) * 0.5f - miny + 10);
}

// ── the draw list ─────────────────────────────────────────────
typedef struct { float depth; short cell_r; unsigned char model; float vx, vy; } Item;
static Item items[128];
static int  n_items;

static void push_item(int model, float vx, float vy, int cell_r) {
    if (n_items >= (int)(sizeof items / sizeof items[0])) return;
    const short *fp = ISO_FOOTPRINT[model];
    // Depth is taken at the footprint's CENTRE, not its origin: a 2-tile sofa sorted by its
    // corner slips behind things it should be in front of.
    float cx = vx + fp[0] * 0.5f, cy = vy + fp[1] * 0.5f;
    items[n_items++] = (Item){ iso_depth(rot, cx, cy, 0), (short)cell_r, (unsigned char)model, vx, vy };
}

// Is this perimeter wall between the camera and the room? Same trick the baker uses for
// face culling: step along the wall's outward normal and see whether you got nearer.
static int wall_hidden(int nx, int ny) {
    float here  = iso_depth(rot, 0, 0, 0);
    float ahead = iso_depth(rot, (float)nx, (float)ny, 0);
    return ahead > here + 1e-6f;
}

static void build_list(void) {
    n_items = 0;
    // Each edge uses a model already ORIENTED for it rather than one model turned 90 degrees. See
    // the note in room.js: a turned non-square object's world footprint swaps x/y, but push_item
    // takes the depth centre from the UNROTATED footprint, so a turned wall sorted wrong and drew
    // over furniture in front of it.
    const int wall_ns = full_wall ? ISO_WALL_FULL_NS : ISO_WALL_LOW_NS;
    const int wall_ew = full_wall ? ISO_WALL_FULL_EW : ISO_WALL_LOW_EW;
    // Walls sit ENTIRELY OUTSIDE the floor. The offset is the wall's own thickness, read from the
    // baked footprint so it cannot drift from the model: placing them one voxel out instead of two
    // left them straddling the first row of tiles, which put their depth centre inside the room.
    const float th_ns = (float)ISO_FOOTPRINT[wall_ns][1];
    const float th_ew = (float)ISO_FOOTPRINT[wall_ew][0];

    for (int t = 0; t < ROOM_W; t++) {
        if (!(full_wall && wall_hidden(0, -1)))                    // north edge
            push_item(wall_ns, t * TILE_VOX, -th_ns, rot);
        if (!(full_wall && wall_hidden(0, 1)))                     // south edge
            push_item(wall_ns, t * TILE_VOX, ROOM_H * TILE_VOX, rot);
    }
    for (int t = 0; t < ROOM_H; t++) {
        if (!(full_wall && wall_hidden(-1, 0)))                    // west edge
            push_item(wall_ew, -th_ew, t * TILE_VOX, rot);
        if (!(full_wall && wall_hidden(1, 0)))                     // east edge
            push_item(wall_ew, ROOM_W * TILE_VOX, t * TILE_VOX, rot);
    }

    for (int i = 0; i < N_PLACED; i++) {
        const Placed *p = &PLACED[i];
        push_item(p->model, p->tx * TILE_VOX, p->ty * TILE_VOX, (rot + p->facing) & 3);
    }

    // The walker: a fixed patrol of the room's long axis. Its FACING re-indexes the same
    // ring of 8 cells that the view rotation does, which is the collapse the probe wanted
    // to confirm: 8 views x 8 facings is one ring, not 64 bakes.
    float px = 1.0f + walk_t, py = 3.0f;
    int   face = (walk_t < (ROOM_W - 2)) ? 1 : 3;
    push_item(ISO_PERSON, px * TILE_VOX, py * TILE_VOX, (rot + face) & 3);
}

static int is_wall(int model) {
    return model == ISO_WALL_LOW_NS || model == ISO_WALL_FULL_NS ||
           model == ISO_WALL_LOW_EW || model == ISO_WALL_FULL_EW;
}

// Where one item's cell lands on screen. Shared by the drawing and by spec's overlap check, so
// the assertion is about the SAME rect that actually gets blitted.
static void item_rect(const Item *it, int *x, int *y, int *w, int *h) {
    const IsoCell *c = &ISO_CELLS[it->model][it->cell_r];
    float sx, sy; iso_project(rot, it->vx, it->vy, 0, &sx, &sy);
    *x = (int)(sx + cam_x) - c->ox;
    *y = (int)(sy + cam_y) - c->oy;
    *w = c->w; *h = c->h;
}

static int cmp_depth(const void *a, const void *b) {
    float d = ((const Item *)a)->depth - ((const Item *)b)->depth;
    return d < 0 ? -1 : (d > 0 ? 1 : 0);
}

// ── drawing ───────────────────────────────────────────────────
static void draw_floor(void) {
    for (int ty = 0; ty < ROOM_H; ty++) {
        for (int tx = 0; tx < ROOM_W; tx++) {
            float c[4][2];
            const float vx = tx * TILE_VOX, vy = ty * TILE_VOX;
            iso_project(rot, vx,            vy,            0, &c[0][0], &c[0][1]);
            iso_project(rot, vx + TILE_VOX, vy,            0, &c[1][0], &c[1][1]);
            iso_project(rot, vx + TILE_VOX, vy + TILE_VOX, 0, &c[2][0], &c[2][1]);
            iso_project(rot, vx,            vy + TILE_VOX, 0, &c[3][0], &c[3][1]);
            int picked = (tx == pick_tx && ty == pick_ty);
            // Two floor tones in a slow check, so the grid reads without a drawn line: the
            // reference does its floor with noise, and lockup's rejected fleck was about
            // CONTRAST, not about having any at all.
            int col = picked ? CLR_YELLOW : (((tx + ty) & 1) ? CLR_BROWN : CLR_DARK_BROWN);
            quadfill((int)(c[0][0] + cam_x), (int)(c[0][1] + cam_y),
                     (int)(c[1][0] + cam_x), (int)(c[1][1] + cam_y),
                     (int)(c[2][0] + cam_x), (int)(c[2][1] + cam_y),
                     (int)(c[3][0] + cam_x), (int)(c[3][1] + cam_y), col);
        }
    }
}

// A CONTACT SHADOW under one object's footprint, drawn just before the object itself so it
// cannot paint over anything nearer. Without it every piece of furniture looks pasted onto
// the floor rather than standing on it — the first build had none, and the whole room read
// as a collage. The reference does this too (Sims 1 puts a soft shadow under everything);
// one flat dark quad is the lo-fi version of the same cue.
static void draw_shadow(const Item *it) {
    const short *fp = ISO_FOOTPRINT[it->model];
    // INSET by exactly ONE VOXEL, and both the direction and the integer-ness were measured rather
    // than guessed.
    //
    // The problem: a shadow's edge is rasterized by the engine's quadfill, while the object's edge
    // came out of the BAKER's fillPoly. Along a 2:1 diagonal the two disagree about which pixel owns
    // the boundary, so a shadow ending flush against neighbouring art opens a 1px staircase of floor
    // between them — which reads as a rendering glitch.
    //
    // Sweeping the pad and counting stray pixels over all four rotations:
    //     -1.0  ->   1        0.0  ->   4        +1.0 ->   3
    //     -0.5  ->  18       +0.25 -> 176       +0.5  ->   8
    // The lesson is not "inset a bit", it is that the pad must be a WHOLE number of voxels. A
    // fractional pad puts the quad's corners between lattice points, where the two rasterizers
    // disagree *everywhere* rather than occasionally — hence +0.25 being forty times worse than 0.
    // Of the integer options a one-voxel inset wins: the shadow stops short of the boundary, so
    // there is no shared edge left to disagree about.
    const float pad = -1.0f;
    const float x0 = it->vx - pad, y0 = it->vy - pad;
    const float x1 = it->vx + fp[0] + pad, y1 = it->vy + fp[1] + pad;
    float c[4][2];
    iso_project(rot, x0, y0, 0, &c[0][0], &c[0][1]);
    iso_project(rot, x1, y0, 0, &c[1][0], &c[1][1]);
    iso_project(rot, x1, y1, 0, &c[2][0], &c[2][1]);
    iso_project(rot, x0, y1, 0, &c[3][0], &c[3][1]);
    quadfill((int)(c[0][0] + cam_x), (int)(c[0][1] + cam_y),
             (int)(c[1][0] + cam_x), (int)(c[1][1] + cam_y),
             (int)(c[2][0] + cam_x), (int)(c[2][1] + cam_y),
             (int)(c[3][0] + cam_x), (int)(c[3][1] + cam_y), CLR_BROWNISH_BLACK);
}

static void draw_items(void) {
    drawn_cells = 0;
    for (int i = 0; i < n_items; i++) {
        const Item *it = &items[i];
        const IsoCell *c = &ISO_CELLS[it->model][it->cell_r];
        // Walls sit ON the perimeter rather than on the floor, so a footprint shadow under one
        // would just smear along the room's edge.
        if (!is_wall(it->model)) draw_shadow(it);
        // The cell's own origin marker tells us where its voxel (0,0,0) lives inside it, so
        // placing it is a subtraction rather than a per-model fudge factor.
        int dx, dy, dw, dh; item_rect(it, &dx, &dy, &dw, &dh);
        sspr(c->x, c->y, c->w, c->h, dx, dy, dw, dh);
        drawn_cells++;
        if (show_order) {
            char n[8]; snprintf(n, sizeof n, "%d", i);
            print(n, dx + c->w / 2 - 2, dy + c->h / 2, CLR_WHITE);
        }
    }
}

static void draw_hud(void) {
    char hud_txt[64];   // NOT `line` or `hud` — studio.h claims both
    snprintf(hud_txt, sizeof hud_txt, "rot %d/4  walls %s  cells %d",
             rot + 1, full_wall ? "FULL" : "LOW", drawn_cells);
    rectfill(0, SCREEN_H - 14, SCREEN_W, 14, CLR_BLACK);
    print(hud_txt, 3, SCREEN_H - 11, CLR_LIGHT_GREY);
    if (pick_tx >= 0) {
        snprintf(hud_txt, sizeof hud_txt, "tile %d,%d", pick_tx, pick_ty);
        print(hud_txt, SCREEN_W - 56, SCREEN_H - 11, CLR_YELLOW);
    } else {
        print("off floor", SCREEN_W - 56, SCREEN_H - 11, CLR_DARK_GREY);
    }
    print("Q/E turn  W walls  TAB order", 3, 3, CLR_DARK_GREY);
}

// ── picking ───────────────────────────────────────────────────
static void update_pick(void) {
    float vx, vy;
    iso_unproject(rot, mouse_x() - cam_x, mouse_y() - cam_y, &vx, &vy);
    int tx = (int)floorf(vx / TILE_VOX), ty = (int)floorf(vy / TILE_VOX);
    if (tx >= 0 && tx < ROOM_W && ty >= 0 && ty < ROOM_H) { pick_tx = tx; pick_ty = ty; }
    else { pick_tx = pick_ty = -1; }
}

// ── entry points ──────────────────────────────────────────────
void init(void) {
    colorkey(-1);                     // the sheet carries real alpha; no colour is a hole
}

void update(void) {
    if (keyp('Q')) rot = (rot + 3) & 3;
    if (keyp('E')) rot = (rot + 1) & 3;
    if (keyp('W')) full_wall = !full_wall;
    if (keyp(KEY_TAB)) show_order = !show_order;
    if (keyp(KEY_SPACE)) walk_paused = !walk_paused;

    if (!walk_paused) {
        walk_t += 0.03f;
        if (walk_t > 2 * (ROOM_W - 2)) walk_t = 0;
    }
    iso_camera(rot);
    update_pick();
    build_list();
    qsort(items, n_items, sizeof items[0], cmp_depth);

#ifdef DE_TRACE
    watch("rot",   "%d", rot);
    watch("items", "%d", n_items);
    watch("pick",  "%d,%d", pick_tx, pick_ty);
#endif
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    draw_floor();
    draw_items();
    draw_hud();
}

// ── spec: the oracles that make this probe converge ───────────
// ADR-0034's finding is that a dimension with a deterministic oracle converges and one
// judged by prose does not. Everything checkable about this renderer is checked here.
#ifdef DE_SPEC
static char sp_msg[128];

void spec(void) {
    // 1. PROJECTION ROUND-TRIP, the load-bearing one. The whole cart rests on iso_project and
    // iso_unproject being exact inverses at every rotation. If they are not, picking lands on
    // the wrong tile and furniture floats off the floor, and BOTH failures are silent.
    for (int r = 0; r < ISO_ROTS; r++) {
        int bad = 0;
        for (int ty = 0; ty < ROOM_H; ty++) {
            for (int tx = 0; tx < ROOM_W; tx++) {
                float vx = tx * TILE_VOX + TILE_VOX * 0.5f;
                float vy = ty * TILE_VOX + TILE_VOX * 0.5f;
                float sx, sy, bx, by;
                iso_project(r, vx, vy, 0, &sx, &sy);
                iso_unproject(r, sx, sy, &bx, &by);
                if ((int)floorf(bx / TILE_VOX) != tx || (int)floorf(by / TILE_VOX) != ty) bad++;
            }
        }
        snprintf(sp_msg, sizeof sp_msg,
                 "rot %d: all %d tile centres round-trip to their own tile",
                 r, ROOM_W * ROOM_H);
        expect(bad == 0, sp_msg);
    }

    // 2. THE QUARTER-TURN IS INVERTIBLE. If turn/unturn disagree, picking is wrong on exactly
    // three of the four quadrants, which looks like a mysterious "only some angles work" bug.
    for (int q = 0; q < 4; q++) {
        float X, Y, x, y;
        iso_turn(q, 3.0f, 5.0f, &X, &Y);
        iso_unturn(q, X, Y, &x, &y);
        snprintf(sp_msg, sizeof sp_msg, "quarter-turn %d then unturn is the identity", q);
        expect(fabsf(x - 3.0f) < 1e-4f && fabsf(y - 5.0f) < 1e-4f, sp_msg);
    }

    // 3. THE VOXEL GRID IS INTEGER, which is the reason the scale cannot simply be halved. A
    // crisp 2:1 diamond needs one voxel step to be a whole number of pixels both across and
    // down; ISO_TW must be a multiple of 4 for that (sx steps TW/2, sy steps TW/4). At TW=2 the
    // rows land on half-pixels and voxels start vanishing, which is why going to a 16px figure
    // meant re-authoring the models coarser rather than turning a knob (iso-rooms.md §7).
    {
        float ax, ay, bx, by, cx, cy;
        iso_project(0, 0, 0, 0, &ax, &ay);
        iso_project(0, 1, 0, 0, &bx, &by);
        iso_project(0, 0, 1, 0, &cx, &cy);
        float stepx = bx - ax, stepy = cy - ay;
        expect(stepx == (float)(int)stepx && stepx != 0, "one voxel step across is a whole number of pixels");
        expect(stepy == (float)(int)stepy && stepy != 0, "one voxel step down is a whole number of pixels");
        expect(ISO_TW % 4 == 0, "ISO_TW is a multiple of 4, so the 2:1 diamond stays on the grid");
        // And the diamond really is 2:1, the shape the whole look depends on.
        expect(spec_close(stepx / stepy, 2.0f, 0.001f), "the tile diamond is exactly 2:1");
    }

    // 4. THE RING COLLAPSE. Every (view rotation, object facing) pair must resolve to one of the
    // SAME four baked cells. If it did not, an object would need a bake per pair and rotation
    // would be unaffordable — so this is the assertion guarding the project's whole premise.
    {
        int in_range = 1, covers = 0, seen[4] = {0};
        for (int r = 0; r < ISO_ROTS; r++)
            for (int f = 0; f < 4; f++) {
                int cell = (r + f) & 3;
                if (cell < 0 || cell >= ISO_ROTS) in_range = 0;
                seen[cell] = 1;
            }
        for (int i = 0; i < 4; i++) covers += seen[i];
        expect(in_range, "every (view, facing) pair indexes an already-baked cell");
        expect(covers == 4, "the 16 pairs need exactly 4 cells, no 5th");
        expect(ISO_ROTS == 4, "the atlas carries the four diagonal rotations and nothing else");
    }

    // 5. DEPTH SORT. The painter's order must put nearer things later, at every rotation.
    for (int r = 0; r < ISO_ROTS; r++) {
        rot = r; full_wall = 1; walk_t = 1.0f;
        iso_camera(r);
        build_list();
        qsort(items, n_items, sizeof items[0], cmp_depth);
        int monotone = 1;
        for (int i = 1; i < n_items; i++)
            if (items[i].depth < items[i - 1].depth - 1e-6f) monotone = 0;
        snprintf(sp_msg, sizeof sp_msg, "rot %d: %d items sort back-to-front", r, n_items);
        expect(monotone && n_items > 0, sp_msg);
    }

    // 6. WALL CUTAWAY. In full-wall mode exactly the NEAR walls must drop: hide none and you
    // cannot see in, hide too many and the room stops reading as a box. Every view here is a
    // diagonal, and a diagonal faces exactly TWO of the four perimeter edges.
    for (int r = 0; r < ISO_ROTS; r++) {
        rot = r;
        int hidden = 0;
        if (wall_hidden(0, -1)) hidden++;
        if (wall_hidden(0,  1)) hidden++;
        if (wall_hidden(-1, 0)) hidden++;
        if (wall_hidden(1,  0)) hidden++;
        snprintf(sp_msg, sizeof sp_msg, "rot %d: %d of 4 wall edges cut, wanted 2", r, hidden);
        expect(hidden == 2, sp_msg);
    }

    // 6b. NO FAR WALL MAY BE DRAWN OVER FURNITURE IT OVERLAPS ON SCREEN.
    //
    // This assertion exists because the other 29 did not catch the bug it guards. In FULL-wall mode
    // every wall still drawn is a FAR wall, so it must sort BEFORE anything standing on the floor
    // in front of it. Two of them did not: the east/west walls took their depth centre from the
    // model's UNROTATED footprint while their art was turned 90 degrees, so they sorted too near
    // and painted over a counter and a fridge, leaving a triangular sliver of each visible.
    //
    // The depth-sort check above could never see it: the list WAS monotone in depth. The depths
    // were simply wrong, and a sort is happy to order wrong numbers correctly. Catching it needs a
    // claim about SCREEN OVERLAP, which is what this makes.
    for (int r = 0; r < ISO_ROTS; r++) {
        rot = r; full_wall = 1; walk_t = 1.0f;
        iso_camera(r);
        build_list();
        qsort(items, n_items, sizeof items[0], cmp_depth);
        int offenders = 0;
        for (int i = 0; i < n_items; i++) {
            if (is_wall(items[i].model)) continue;                 // furniture, drawn at i
            for (int j = i + 1; j < n_items; j++) {
                if (!is_wall(items[j].model)) continue;            // a wall drawn LATER
                int ax, ay, aw, ah, bx, by, bw, bh;
                item_rect(&items[i], &ax, &ay, &aw, &ah);
                item_rect(&items[j], &bx, &by, &bw, &bh);
                int ox = (ax + aw < bx + bw ? ax + aw : bx + bw) - (ax > bx ? ax : bx);
                int oy = (ay + ah < by + bh ? ay + ah : by + bh) - (ay > by ? ay : by);
                if (ox > 2 && oy > 2) offenders++;
            }
        }
        snprintf(sp_msg, sizeof sp_msg,
                 "rot %d: no far wall is drawn over furniture it overlaps (%d offenders)", r, offenders);
        expect(offenders == 0, sp_msg);
    }

    // 7. EVERY BAKED CELL IS INSIDE THE SHEET, with its origin inside itself. A packing bug
    // draws a neighbouring object's pixels, which reads as corruption rather than an error.
    {
        int oob = 0, bad_origin = 0;
        for (int m = 0; m < ISO_MODEL_COUNT; m++)
            for (int r = 0; r < ISO_ROTS; r++) {
                const IsoCell *c = &ISO_CELLS[m][r];
                if (c->x < 0 || c->y < 0 || c->x + c->w > ISO_ATLAS_W || c->y + c->h > ISO_ATLAS_H) oob++;
                if (c->w <= 0 || c->h <= 0) oob++;
                // The floor origin may sit slightly OUTSIDE the tight crop, and legitimately so:
                // it is voxel (0,0,0)'s projected corner, and for a model whose (0,0,0) voxel is
                // empty (the toilet, the person) that corner is not part of the silhouette. The
                // placement arithmetic (dx = sx - ox) does not care about the sign. So the real
                // invariant is that it stays NEAR the cell — a wild value means a crop or packing
                // bug, which would draw a neighbour's pixels and read as corruption, not an error.
                int slack = TILE_PX;
                if (c->ox < -slack || c->ox > c->w + slack ||
                    c->oy < -slack || c->oy > c->h + slack) bad_origin++;
            }
        snprintf(sp_msg, sizeof sp_msg, "all %d baked cells lie inside the %dx%d atlas",
                 ISO_MODEL_COUNT * ISO_ROTS, ISO_ATLAS_W, ISO_ATLAS_H);
        expect(oob == 0, sp_msg);
        expect(bad_origin == 0, "every cell's floor origin sits within a tile of its cell");
    }

    // 8. THE ROOM IS ON SCREEN at every rotation. The camera is recomputed per rotation, and
    // a sign slip there parks the room off-canvas, which the other checks would not notice.
    for (int r = 0; r < ISO_ROTS; r++) {
        rot = r; full_wall = 1;
        iso_camera(r);
        float sx, sy; iso_project(r, ROOM_W * TILE_VOX * 0.5f, ROOM_H * TILE_VOX * 0.5f, 0, &sx, &sy);
        int cx = (int)(sx + cam_x), cy = (int)(sy + cam_y);
        snprintf(sp_msg, sizeof sp_msg, "rot %d: room centre is on canvas at %d,%d", r, cx, cy);
        expect(cx > 0 && cx < SCREEN_W && cy > 0 && cy < SCREEN_H, sp_msg);
    }

    rot = 0; full_wall = 1; walk_t = 0;
}
#endif
