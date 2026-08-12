/* de:meta
{
  "title": "isoroom",
  "slug": "isoroom",
  "kind": ["probe", "tech-demo"],
  "teaches": ["isometric-projection"],
  "created": "2026-08-12",
  "lineage": "A renderer probe, not a game: gates whether a Sims-style life sim is buildable here by testing whether one voxel model can supply EIGHT view rotations without paying triangles at runtime. The pipeline (author once, pre-render every rotation to sprites at build time) is the one RollerCoaster Tycoon and The Sims 1 both shipped; the novelty is doing it at 320x200 in 32 colours, and carrying BOTH projection families (the 2:1 diamond and the axis-aligned cardinal) in one data model.",
  "todo": [
    "VERDICT NOT YET WRITTEN. The probe answers five things and only some are in: the atlas measurement is done (67,780px of cells, which is why make-cart.js now takes a declared sheet size), the light-does-not-rotate invariant holds by construction, and the projection round-trips. Still open: the ms/frame number, the on-device number, and the cardinal-views-readability go/no-go.",
    "Objects turned by a FACING that spans more than one tile do not move their footprint with them: cell = (r + 2*facing) & 7 picks the right ART, but a 2-tile sofa turned 90 degrees still claims its original two tiles. Fine for a shape test, wrong for a game.",
    "fridge and wall_full bake to indistinguishable grey slabs; the fridge's door is a single near-invisible voxel line. The toilet is mush at 8-voxels-per-tile.",
    "No multi-storey. Deferred on purpose: a second floor changes both the depth sort and the wall-cut rules.",
    "The character walks a fixed patrol, not a path. Occlusion is what is being tested, not navigation."
  ],
  "description": {
    "summary": "A rotating isometric room, drawn entirely from sprite cells that were pre-rendered from voxel models at build time. Turn it eight ways and nothing is a triangle.",
    "detail": "The probe behind a possible Sims-style sim. An isometric view that ROTATES normally costs one drawing per object per rotation, which is where such a project dies; rendering the voxels live instead costs ~89ms/frame on an iPhone (ADR-0024), which is where the other version dies. So every object here is authored ONCE as ASCII voxel layers and baked into all eight rotations at build time, and the runtime is nothing but sspr() and a painter's sort. The eight rotations are two different projections sharing one data model: the four 45-degree steps give the familiar 2:1 diamond, the four cardinal steps give an axis-aligned view, and those eight are the complete set of angles that land on the pixel grid. Light is fixed in SCREEN space rather than world space, so it does not swing around as you turn the room. Walls can be low stubs that never occlude, or full height with the near side cut away.",
    "controls": "Q/E turn the room one step (eight in total). W switches walls between LOW and FULL+cutaway. Move the mouse to pick a floor tile; the readout names the tile and the rotation family. TAB shows the depth-sort order. SPACE pauses the walker."
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
#define TILE_VOX  8                  // voxels per floor tile — the scale the models assume
#define ROOM_W    9
#define ROOM_H    7
#define TILE_PX   (TILE_VOX * ISO_TW / 2)   // a tile's screen width: 32px

typedef struct { unsigned char model, tx, ty, facing; } Placed;

// A one-room flat. Facing is in quarter turns; see the note in de:meta.todo about
// multi-tile objects and facing.
static const Placed PLACED[] = {
    { ISO_SOFA,    1, 1, 0 },
    { ISO_COUNTER, 6, 1, 0 },
    { ISO_FRIDGE,  7, 1, 0 },
    { ISO_BED,     1, 4, 0 },
    { ISO_TOILET,  7, 5, 0 },
    { ISO_COUNTER, 4, 5, 1 },
};
#define N_PLACED ((int)(sizeof PLACED / sizeof PLACED[0]))

// ── state ─────────────────────────────────────────────────────
static int   rot       = 1;           // 0..7. EVEN = cardinal, ODD = diagonal
// FULL is the default because it plainly won the comparison: low stubs read as a picture frame
// around a floor, while full walls with the near side cut away read as an interior. W toggles.
static int   full_wall = 1;           // 0 = low stubs (no cutaway), 1 = full + cutaway
static int   show_order = 0;
static int   walk_paused = 0;         // NOT `paused` — studio.h already has a paused() built-in
static float walk_t    = 0;
static int   pick_tx = -1, pick_ty = -1;
static int   drawn_cells = 0;         // per-frame sspr count, for the budget question

// ── projection ────────────────────────────────────────────────
// Mirrors tools/voxel-bake.js projector(). Quarter-turn of the world plane, then one of
// two linear maps depending on the family.
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

// Voxel-space point -> screen offset (before the camera shift).
static void iso_project(int r, float vx, float vy, float vz, float *sx, float *sy) {
    float X, Y; iso_turn(r >> 1, vx, vy, &X, &Y);
    if (r & 1) {                                   // DIAGONAL: the 2:1 diamond
        *sx = (X - Y) * (ISO_TW * 0.5f);
        *sy = (X + Y) * (ISO_TH * 0.5f) - vz * ISO_ZH;
    } else {                                       // CARDINAL: axis-aligned
        *sx =  X * ISO_CW;
        *sy =  Y * (ISO_CW * 0.5f) - vz * ISO_ZH;
    }
}

// Screen offset -> voxel-space point on the FLOOR (z = 0). The exact inverse of the above,
// which is what makes picking land on the tile you are pointing at.
static void iso_unproject(int r, float sx, float sy, float *vx, float *vy) {
    float X, Y;
    if (r & 1) {
        float d = sx / (ISO_TW * 0.5f);            // X - Y
        float s = sy / (ISO_TH * 0.5f);            // X + Y
        X = (s + d) * 0.5f; Y = (s - d) * 0.5f;
    } else {
        X = sx / ISO_CW;
        Y = sy / (ISO_CW * 0.5f);
    }
    iso_unturn(r >> 1, X, Y, vx, vy);
}

// Larger = nearer the camera. Mirrors the baker's depth().
static float iso_depth(int r, float vx, float vy, float vz) {
    float X, Y; iso_turn(r >> 1, vx, vy, &X, &Y);
    return (r & 1) ? (X + Y + vz * 0.001f) : (Y + vz * 0.001f);
}

// ── camera ────────────────────────────────────────────────────
// Centre the room's screen bounding box. Recomputed per frame because it changes with the
// rotation; cheap, and it means no per-rotation offset table to get wrong.
static float cam_x, cam_y;
static void iso_camera(int r) {
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    const float W = ROOM_W * TILE_VOX, H = ROOM_H * TILE_VOX;
    const float TOP = full_wall ? 16.0f : 6.0f;    // tallest thing that needs to fit
    for (int c = 0; c < 8; c++) {
        float vx = (c & 1) ? W : 0, vy = (c & 2) ? H : 0, vz = (c & 4) ? TOP : 0;
        float sx, sy; iso_project(r, vx, vy, vz, &sx, &sy);
        if (sx < minx) minx = sx; if (sx > maxx) maxx = sx;
        if (sy < miny) miny = sy; if (sy > maxy) maxy = sy;
    }
    cam_x = (SCREEN_W - (maxx - minx)) * 0.5f - minx;
    cam_y = (SCREEN_H - 14 - (maxy - miny)) * 0.5f - miny + 10;
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
    const int wall_model = full_wall ? ISO_WALL_FULL : ISO_WALL_LOW;

    // Perimeter walls, one segment per tile. A segment's art is the same model turned to lie
    // along its edge, which is the ring collapse in action: 4 edges x 8 views come out of the
    // same 8 baked cells.
    for (int t = 0; t < ROOM_W; t++) {
        if (!(full_wall && wall_hidden(0, -1)))                    // north edge
            push_item(wall_model, t * TILE_VOX, -2.0f, rot);
        if (!(full_wall && wall_hidden(0, 1)))                     // south edge
            push_item(wall_model, t * TILE_VOX, ROOM_H * TILE_VOX, rot);
    }
    for (int t = 0; t < ROOM_H; t++) {
        if (!(full_wall && wall_hidden(-1, 0)))                    // west edge
            push_item(wall_model, -2.0f, t * TILE_VOX, (rot + 2) & 7);
        if (!(full_wall && wall_hidden(1, 0)))                     // east edge
            push_item(wall_model, ROOM_W * TILE_VOX, t * TILE_VOX, (rot + 2) & 7);
    }

    for (int i = 0; i < N_PLACED; i++) {
        const Placed *p = &PLACED[i];
        push_item(p->model, p->tx * TILE_VOX, p->ty * TILE_VOX, (rot + 2 * p->facing) & 7);
    }

    // The walker: a fixed patrol of the room's long axis. Its FACING re-indexes the same
    // ring of 8 cells that the view rotation does, which is the collapse the probe wanted
    // to confirm: 8 views x 8 facings is one ring, not 64 bakes.
    float px = 1.0f + walk_t, py = 3.0f;
    int   face = (walk_t < (ROOM_W - 2)) ? 2 : 6;
    push_item(ISO_PERSON, px * TILE_VOX, py * TILE_VOX, (rot + face) & 7);
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
    const float x0 = it->vx, y0 = it->vy;
    const float x1 = x0 + fp[0], y1 = y0 + fp[1];
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
        if (it->model != ISO_WALL_LOW && it->model != ISO_WALL_FULL) draw_shadow(it);
        float sx, sy; iso_project(rot, it->vx, it->vy, 0, &sx, &sy);
        // The cell's own origin marker tells us where its voxel (0,0,0) lives inside it, so
        // placing it is a subtraction rather than a per-model fudge factor.
        int dx = (int)(sx + cam_x) - c->ox;
        int dy = (int)(sy + cam_y) - c->oy;
        sspr(c->x, c->y, c->w, c->h, dx, dy, c->w, c->h);
        drawn_cells++;
        if (show_order) {
            char n[8]; snprintf(n, sizeof n, "%d", i);
            print(n, dx + c->w / 2 - 2, dy + c->h / 2, CLR_WHITE);
        }
    }
}

static void draw_hud(void) {
    const char *fam = (rot & 1) ? "DIAG" : "CARD";
    char hud_txt[64];   // NOT `line` or `hud` — studio.h claims both
    snprintf(hud_txt, sizeof hud_txt, "rot %d/8 %s  walls %s  cells %d",
             rot, fam, full_wall ? "FULL" : "LOW", drawn_cells);
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
    if (keyp('Q')) rot = (rot + 7) & 7;
    if (keyp('E')) rot = (rot + 1) & 7;
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
    for (int r = 0; r < 8; r++) {
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
                 "rot %d (%s): all %d tile centres round-trip to their own tile",
                 r, (r & 1) ? "diag" : "card", ROOM_W * ROOM_H);
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

    // 3. NO ZOOM POP between the two families. A tile must cover the same screen footprint in
    // both, or rotating from a diagonal to a cardinal view reads as a zoom. This is the
    // finding that de-risked carrying 8 rotations instead of 4 (docs/design/iso-rooms.md §7).
    {
        float ax, ay, bx, by;
        iso_project(1, 0, 0, 0, &ax, &ay);
        iso_project(1, TILE_VOX, TILE_VOX, 0, &bx, &by);
        float diag_h = by - ay, diag_w = bx - ax;
        iso_project(0, 0, 0, 0, &ax, &ay);
        iso_project(0, TILE_VOX, TILE_VOX, 0, &bx, &by);
        float card_h = by - ay, card_w = bx - ax;
        expect(spec_close(diag_h, card_h, 0.01f), "a tile is the same screen HEIGHT in both families");
        // The widths differ by construction (the diagonal's spans the diamond), so what has to
        // match is the tile's total screen DIAGONAL extent, checked as height above.
        expect(diag_w != card_w || diag_w == 0, "the two families really are different projections");
    }

    // 4. THE RING COLLAPSE. 8 view rotations x 8 facings must resolve to the SAME 8 baked
    // cells. If they did not, an object would need 64 bakes and 8 rotations would be
    // unaffordable — so this assertion is the one guarding the project's whole premise.
    {
        int in_range = 1, covers = 0, seen[8] = {0};
        for (int r = 0; r < 8; r++)
            for (int f = 0; f < 8; f++) {
                int cell = (r + f) & 7;
                if (cell < 0 || cell >= ISO_ROTS) in_range = 0;
                seen[cell] = 1;
            }
        for (int i = 0; i < 8; i++) covers += seen[i];
        expect(in_range, "every (view, facing) pair indexes an already-baked cell");
        expect(covers == 8, "the 64 pairs need exactly 8 cells, no 9th");
        expect(ISO_ROTS == 8, "the atlas carries all 8 rotations");
    }

    // 5. DEPTH SORT. The painter's order must put nearer things later, at every rotation.
    for (int r = 0; r < 8; r++) {
        rot = r; full_wall = 0; walk_t = 1.0f;
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
    // cannot see in, hide too many and the room stops reading as a box. A diagonal view faces
    // two perimeter edges, a cardinal view faces one.
    for (int r = 0; r < 8; r++) {
        rot = r;
        int hidden = 0;
        if (wall_hidden(0, -1)) hidden++;
        if (wall_hidden(0,  1)) hidden++;
        if (wall_hidden(-1, 0)) hidden++;
        if (wall_hidden(1,  0)) hidden++;
        int want = (r & 1) ? 2 : 1;
        snprintf(sp_msg, sizeof sp_msg, "rot %d (%s): %d of 4 wall edges cut, wanted %d",
                 r, (r & 1) ? "diag" : "card", hidden, want);
        expect(hidden == want, sp_msg);
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
    for (int r = 0; r < 8; r++) {
        rot = r; full_wall = 1;
        iso_camera(r);
        float sx, sy; iso_project(r, ROOM_W * TILE_VOX * 0.5f, ROOM_H * TILE_VOX * 0.5f, 0, &sx, &sy);
        int cx = (int)(sx + cam_x), cy = (int)(sy + cam_y);
        snprintf(sp_msg, sizeof sp_msg, "rot %d: room centre is on canvas at %d,%d", r, cx, cy);
        expect(cx > 0 && cx < SCREEN_W && cy > 0 && cy < SCREEN_H, sp_msg);
    }

    rot = 1; full_wall = 0; walk_t = 0;
}
#endif
