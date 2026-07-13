/* de:meta
{
  "slug": "floorplan",
  "title": "floorplan",
  "status": "active",
  "created": "2026-06-26",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": "One cart, many floorplans: a real Floorplanner project loaded at RUNTIME (not baked) and walked top-down. Fetch any project with data-tools/fmltools/floorplanner.js -pid=<id>, then run with --data/$DE_DATA or drag the .json onto the window. Press TAB for a start-screen picker of the plans you've already fetched (or type an id). Same look as floorwalker/seinelaan. WASD/arrows move, T toggles sprites vs boxes.",
  "todo": [
    "object HEIGHTS for collision: rugs/mats (flat, low z-height) should be walkable, not solid boxes — read each item's height/z and skip collision below a threshold.",
    "true 32-bit RGB: fall back to engine RGB instead of quantising sprites/textures to the 32-palette — natural furniture + rich floor textures.",
    "z-ordering: sort draws by (z + z_height) so taller objects layer over shorter and the player occludes / is occluded correctly (vs the current fixed paint order)."
  ]
}
de:meta */
#include "studio.h"
#include "json.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

// floorplan — ONE cart, MANY floorplans. The runtime-loaded twin of floorwalker:
// the level (walls, doorways, room floors, furniture sprites) is NOT baked into the
// source — it is loaded at startup from a JSON data file via --data <file> / $DE_DATA
// (drag a file onto the window to swap it live). The renderer is identical to
// floorwalker, so a fetched project looks exactly like the baked carts.
//
//   node data-tools/fmltools/floorplanner.js -pid=187256440 --play   # fetch + build + launch, one command
//   DE_DATA=data/floorplan/187256440.json node tools/play.js floorplan run   # run a built one
//
// Data schema (fml2cart.js --json + fml-sprites.js --json + fml-textures.js --json):
//   { name, scale, w, h, spawn:[x,y],
//     walls:[ax,ay,bx,by,thick, ...], windows:[...same...],
//     doors:[cx,cy,w,rot, ...], furn:[cx,cy,w,h,rot,ref, ...],
//     areas:[{c, poly:[x,y,...]}, ...],                              // c = real room floor colour
//     surfaces:[{c, tex, tile, poly:[x,y,...]}, ...],                // floor coverings; tex<0 = flat
//     sprites:[{w,h,px:[palette idx, 255=transparent]}, ...],        // furn.ref indexes sprites[]
//     textures:[{w,h,px:[palette idx]}, ...] }                       // surfaces[].tex indexes textures[]
// Design: docs/design/external-data-carts.md → "Floorplanner — implemented".
// Furniture + floor IMAGES are baked from Floorplanner CDN renders by
// data-tools/fmltools/fml-assets.js (furniture sprites) + fml-textures.js (floor
// textures); the pipeline status + the open "do this better" work (object-height
// collision, true-RGB colour vs the muddy 32-palette quantise, z-ordering) is
// tracked in data-tools/fmltools/TODO.md — the same three items mirrored in this
// cart's de:meta.todo[].
//
// FINDINGS (from the 2026-06 build-out — read before changing the pipeline):
//   - Furniture refids are 40 chars of [0-9a-f] OR 'x' — the 'x' is part of the id, NOT a
//     placeholder. The old /^[0-9a-f]{40}$/ filter silently dropped every x-item to a box.
//   - Coordinates/sizes are in CENTIMETRES; scale is cm/px (default 8). Item 75cm -> 9px. The
//     scaling IS correct — small items just look small. Don't "fix" it.
//   - DON'T drop furniture by size (fml2cart --maxfurn 0): real floor coverings are surfaces[]
//     (rs-#### Roomstyler materials), so items[] are genuine objects; the old 280cm cap dropped
//     normal sofas/beds and left only tiny decor.
//   - The 32-colour palette is the real visual limit: low-saturation floor materials (grey tile,
//     concrete) quantise to ~1 entry (flat); heavy --saturate turns small furniture into rainbow
//     confetti. Dynamic bake uses gentle --saturate 1.4; textures get a luma-contrast stretch.
//
// NEXT / IDEAS: now tracked in this cart's de:meta.todo[] (node tools/cart-todos.js floorplan);
// the data-pipeline backlog stays in data-tools/fmltools/TODO.md.

// ---- runtime pools (sized for the largest real plans; floorwalker is ~1k furniture) ----
#define MAXSEG   4096
#define MAXDOOR  1024
#define MAXAREA  1024
#define MAXAPT  32768          // floats (= 16k polygon vertices)
#define MAXFURN  8192
#define MAXSPR   1024
#define MAXPOOL (1 << 20)      // furniture sprite pixels (palette indices)

typedef struct { float ax, ay, bx, by, thick; } Seg;
typedef struct { float cx, cy, w, rot; } Door;
typedef struct { float cx, cy, w, h, rot; int ref; } Furn;
typedef struct { int color, n, off; } AreaP;   // off/n index into apts[] (vertices)
typedef struct { short w, h; int off; } Spr;    // off indexes pool[]

static Seg   walls[MAXSEG];   static int n_walls;
static Seg   windows[MAXSEG]; static int n_windows;
static Door  doors[MAXDOOR];  static int n_doors;
static AreaP areas[MAXAREA];  static int n_areas;
static float apts[MAXAPT];    static int n_apts;     // count of floats (pairs)
static Furn  furn[MAXFURN];   static int n_furn;
static Spr   sprs[MAXSPR];    static int n_spr;   // 'spr' is a studio.h built-in — don't shadow it
static unsigned char pool[MAXPOOL]; static int n_pool;

// floor coverings: polygons tiled with a real Roomstyler material texture (or a flat colour)
static struct { int tex, c, tile, off, n; } surfs[MAXAREA]; static int n_surfs;
static float spts[MAXAPT];          static int n_spts;       // surface polygon vertices (pairs)
static Spr   texs[MAXSPR];          static int n_texs;       // floor textures {w,h,off}
static unsigned char tpool[MAXPOOL]; static int n_tpool;     // floor-texture pixels

static int   world_w = 320, world_h = 200;
static float spawn_x = 160, spawn_y = 100;
static char  dname[64] = "";
static char  err[160] = "";
static int   loaded_ok = 0, tried = 0, truncated = 0;

static struct { float x, y, aim; } pl;
static bool use_sprites = true;   // T toggles sprites vs placeholder boxes
static bool outline_on = true;    // O toggles the render-time furniture outline

// ---- start-screen plan picker (stage 1 of the loader; docs/design/external-data-carts.md → "Loader") ----
// Lists the plans already fetched into the data folder; pick one or type an id. TAB summons it while
// walking; ESC backs out. Fetching a NEW id (the --serve bridge) is stage 2 — not built yet.
#define MAXPLANS  256
#define PICK_Y0   34              // y of the first list row
#define PICK_RH   11              // row height
#define C_SEL     CLR_DARK_BLUE   // selected-row highlight bar
typedef struct { char file[64]; char name[48]; } PlanEntry;   // file = filename stem, name = plan's "name"
static PlanEntry plans[MAXPLANS]; static int n_plans = 0;
static int  sel = 0, scroll = 0;      // highlighted row / first visible row
static bool picking = false;          // showing the picker instead of the plan
static char plans_dir[256] = "";      // folder scanned for *.json
static char idbuf[16] = "";           // id typed into the entry field
static char pickmsg[128] = "";        // transient status under the entry field

#define PLAYER_R 4.0f
#define CAM_ZOOM 1.0f
#define C_BG     0
#define C_WALL   6
#define C_GLASS  12
#define C_FURN   22
#define C_FURN_O 16
#define C_PLAYER 8
#define C_DOOR   9
#define DATA_DIR "../data/floorplan"   // cwd is build/, so ../data/floorplan == <project>/data/floorplan

// ---- load ----
static void reset_pools(void) {
    n_walls = n_windows = n_doors = n_areas = n_apts = n_furn = n_spr = n_pool = 0;
    n_surfs = n_spts = n_texs = n_tpool = 0;
    loaded_ok = truncated = 0; dname[0] = 0; err[0] = 0;
}

// fill an array of fixed-stride records from a flat JSON number array; returns record count
static int load_flat(const char *js, jsmntok_t *tok, int obj, const char *key, float *out, int maxFloats, int stride) {
    int a = json_get(js, tok, obj, key);
    if (a < 0 || tok[a].type != JSMN_ARRAY) return 0;
    int n = tok[a].size; if (n > maxFloats) { n = (maxFloats / stride) * stride; truncated = 1; }
    for (int i = 0; i < n; i++) out[i] = (float)json_num(js, &tok[a + 1 + i]);
    return n / stride;
}

static void load_from(const char *path) {
    reset_pools();
    long len; char *js = json_slurp(path, &len);
    if (!js) { snprintf(err, sizeof err, "cannot read %s", path); return; }
    jsmntok_t *tok; int nt = json_parse(js, &tok);
    if (nt < 1 || tok[0].type != JSMN_OBJECT) { snprintf(err, sizeof err, "bad json in %s (err %d)", path, nt); free(js); free(tok); return; }

    int ni = json_get(js, tok, 0, "name");
    if (ni >= 0) json_str(dname, sizeof dname, js, &tok[ni]);
    int wi = json_get(js, tok, 0, "w"); if (wi >= 0) world_w = (int)json_num(js, &tok[wi]);
    int hi = json_get(js, tok, 0, "h"); if (hi >= 0) world_h = (int)json_num(js, &tok[hi]);
    int si = json_get(js, tok, 0, "spawn");
    if (si >= 0 && tok[si].type == JSMN_ARRAY && tok[si].size >= 2) {
        spawn_x = (float)json_num(js, &tok[si + 1]); spawn_y = (float)json_num(js, &tok[si + 2]);
    }

    n_walls   = load_flat(js, tok, 0, "walls",   (float *)walls,   MAXSEG  * 5, 5);
    n_windows = load_flat(js, tok, 0, "windows", (float *)windows, MAXSEG  * 5, 5);
    n_doors   = load_flat(js, tok, 0, "doors",   (float *)doors,   MAXDOOR * 4, 4);

    // furniture: [cx,cy,w,h,rot,ref] — ref is an int but rides in the float array, round it back
    int fa = json_get(js, tok, 0, "furn");
    if (fa >= 0 && tok[fa].type == JSMN_ARRAY) {
        int cnt = tok[fa].size / 6;
        for (int i = 0; i < cnt && n_furn < MAXFURN; i++) {
            int b = fa + 1 + i * 6;
            furn[n_furn].cx  = (float)json_num(js, &tok[b]);
            furn[n_furn].cy  = (float)json_num(js, &tok[b + 1]);
            furn[n_furn].w   = (float)json_num(js, &tok[b + 2]);
            furn[n_furn].h   = (float)json_num(js, &tok[b + 3]);
            furn[n_furn].rot = (float)json_num(js, &tok[b + 4]);
            furn[n_furn].ref = (int)json_num(js, &tok[b + 5]);
            n_furn++;
        }
        if (cnt > MAXFURN) truncated = 1;
    }

    // areas: [{c, poly:[x,y,...]}] — flatten poly into the shared apts[] pool
    int aa = json_get(js, tok, 0, "areas");
    if (aa >= 0 && tok[aa].type == JSMN_ARRAY) {
        int fi = aa + 1;
        for (int a = 0; a < tok[aa].size && n_areas < MAXAREA; a++) {
            int ci = json_get(js, tok, fi, "c");
            int pi = json_get(js, tok, fi, "poly");
            int color = (ci >= 0) ? (int)json_num(js, &tok[ci]) : 16;
            if (pi >= 0 && tok[pi].type == JSMN_ARRAY && tok[pi].size >= 6) {
                int nv = tok[pi].size / 2, off = n_apts / 2;
                for (int k = 0; k < nv * 2 && n_apts < MAXAPT; k++) apts[n_apts++] = (float)json_num(js, &tok[pi + 1 + k]);
                areas[n_areas].color = color; areas[n_areas].n = nv; areas[n_areas].off = off;
                n_areas++;
            }
            fi += json_span(tok, fi);
        }
    }

    // floor coverings: [{c, tex, tile, poly:[x,y,...]}] — tex indexes textures[], -1 = flat colour
    int ua = json_get(js, tok, 0, "surfaces");
    if (ua >= 0 && tok[ua].type == JSMN_ARRAY) {
        int fi = ua + 1;
        for (int a = 0; a < tok[ua].size && n_surfs < MAXAREA; a++) {
            int ci = json_get(js, tok, fi, "c"), ti = json_get(js, tok, fi, "tex"),
                tli = json_get(js, tok, fi, "tile"), pi = json_get(js, tok, fi, "poly");
            int color = (ci >= 0) ? (int)json_num(js, &tok[ci]) : 16;
            int tex = (ti >= 0) ? (int)json_num(js, &tok[ti]) : -1;
            int tile = (tli >= 0) ? (int)json_num(js, &tok[tli]) : 0;
            if (pi >= 0 && tok[pi].type == JSMN_ARRAY && tok[pi].size >= 6) {
                int nv = tok[pi].size / 2, off = n_spts / 2;
                for (int k = 0; k < nv * 2 && n_spts < MAXAPT; k++) spts[n_spts++] = (float)json_num(js, &tok[pi + 1 + k]);
                surfs[n_surfs].tex = tex; surfs[n_surfs].c = color;
                surfs[n_surfs].tile = tile < 1 ? 1 : tile; surfs[n_surfs].off = off; surfs[n_surfs].n = nv;
                n_surfs++;
            }
            fi += json_span(tok, fi);
        }
    }

    // textures: [{w,h,px:[...]}] — surfaces[].tex indexes this; px packs into tpool
    int xa = json_get(js, tok, 0, "textures");
    if (xa >= 0 && tok[xa].type == JSMN_ARRAY) {
        int fi = xa + 1;
        for (int s = 0; s < tok[xa].size && n_texs < MAXSPR; s++) {
            int twi = json_get(js, tok, fi, "w"), thi = json_get(js, tok, fi, "h"), pxi = json_get(js, tok, fi, "px");
            int w = (twi >= 0) ? (int)json_num(js, &tok[twi]) : 0;
            int h = (thi >= 0) ? (int)json_num(js, &tok[thi]) : 0;
            int off = n_tpool;
            if (w > 0 && h > 0 && pxi >= 0 && tok[pxi].type == JSMN_ARRAY)
                for (int k = 0; k < tok[pxi].size && n_tpool < MAXPOOL; k++)
                    tpool[n_tpool++] = (unsigned char)json_num(js, &tok[pxi + 1 + k]);
            texs[n_texs].w = (short)w; texs[n_texs].h = (short)h; texs[n_texs].off = off;
            n_texs++;
        }
    }

    // sprites: [{w,h,px:[...]}] — furn.ref indexes this; px packs into the byte pool
    int sa = json_get(js, tok, 0, "sprites");
    if (sa >= 0 && tok[sa].type == JSMN_ARRAY) {
        int fi = sa + 1;
        for (int s = 0; s < tok[sa].size && n_spr < MAXSPR; s++) {
            int swi = json_get(js, tok, fi, "w"), shi = json_get(js, tok, fi, "h"), pxi = json_get(js, tok, fi, "px");
            int w = (swi >= 0) ? (int)json_num(js, &tok[swi]) : 0;
            int h = (shi >= 0) ? (int)json_num(js, &tok[shi]) : 0;
            int off = n_pool;
            if (w > 0 && h > 0 && pxi >= 0 && tok[pxi].type == JSMN_ARRAY)
                for (int k = 0; k < tok[pxi].size && n_pool < MAXPOOL; k++)
                    pool[n_pool++] = (unsigned char)json_num(js, &tok[pxi + 1 + k]);
            sprs[n_spr].w = (short)w; sprs[n_spr].h = (short)h; sprs[n_spr].off = off;
            n_spr++;
        }
    }

    free(tok); free(js);
    if (!n_walls && !n_areas) { snprintf(err, sizeof err, "no floor in %s", path); return; }
    pl.x = spawn_x; pl.y = spawn_y; pl.aim = 0;
    loaded_ok = 1;
}

static void load_data(void) {
    const char *path = de_data_path();
    load_from(path ? path : DATA_DIR "/demo.json");
    if (!loaded_ok && !path)
        snprintf(err, sizeof err, "no data -- run: node data-tools/fmltools/floorplanner.js -pid=<id>  (then drop the .json here)");
}

// ---- plan picker ----
// The folder to scan: the dirname of the launch --data file (so --play's folder is listed),
// else DATA_DIR. cwd is build/, so DATA_DIR == <project>/data/floorplan.
static void resolve_plans_dir(void) {
    const char *dp = de_data_path();
    if (dp && *dp) {
        strncpy(plans_dir, dp, sizeof plans_dir - 1);
        plans_dir[sizeof plans_dir - 1] = 0;
        char *slash = strrchr(plans_dir, '/');
        if (slash && slash != plans_dir) *slash = 0; else strcpy(plans_dir, DATA_DIR);
    } else {
        strcpy(plans_dir, DATA_DIR);
    }
}
// pull "name":"..." from the first bytes of a plan file (cheap — no full JSON parse; name is key 1)
static void peek_name(const char *path, char *out, int cap) {
    out[0] = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char head[512]; size_t got = fread(head, 1, sizeof head - 1, f); head[got] = 0; fclose(f);
    char *k = strstr(head, "\"name\"");
    if (!k) return;
    k = strchr(k + 6, '"'); if (!k) return;              // opening quote of the value
    k++; char *end = strchr(k, '"'); if (!end) return;
    int n = (int)(end - k); if (n > cap - 1) n = cap - 1;
    memcpy(out, k, n); out[n] = 0;
}
static int plan_cmp(const void *a, const void *b) {
    return strcmp(((const PlanEntry *)a)->file, ((const PlanEntry *)b)->file);
}
static void scan_plans(void) {
    n_plans = 0;
    DIR *d = opendir(plans_dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && n_plans < MAXPLANS) {
        const char *nm = e->d_name;
        int L = (int)strlen(nm);
        if (L < 6 || strcmp(nm + L - 5, ".json")) continue;      // *.json only
        PlanEntry *p = &plans[n_plans];
        int stem = L - 5; if (stem > (int)sizeof p->file - 1) stem = (int)sizeof p->file - 1;
        memcpy(p->file, nm, stem); p->file[stem] = 0;
        char path[400]; snprintf(path, sizeof path, "%s/%s", plans_dir, nm);
        peek_name(path, p->name, sizeof p->name);
        n_plans++;
    }
    closedir(d);
    qsort(plans, n_plans, sizeof plans[0], plan_cmp);
}
static void open_picker(void) {
    resolve_plans_dir();
    scan_plans();
    picking = true; idbuf[0] = 0; pickmsg[0] = 0;
    if (sel >= n_plans) sel = n_plans ? n_plans - 1 : 0;
}
static void load_plan(int i) {
    if (i < 0 || i >= n_plans) return;
    char path[400]; snprintf(path, sizeof path, "%s/%s.json", plans_dir, plans[i].file);
    load_from(path);
    if (loaded_ok) picking = false;
    else snprintf(pickmsg, sizeof pickmsg, "%s", err[0] ? err : "load failed");
}
// load by the typed id — stage 1: local files only. Stage 2's --serve bridge fetches unknown ids.
static void load_id(void) {
    for (int i = 0; i < n_plans; i++)
        if (!strcmp(plans[i].file, idbuf)) { load_plan(i); return; }
    snprintf(pickmsg, sizeof pickmsg, "%s not fetched -- run: floorplanner.js %s --play", idbuf, idbuf);
}
static void boot_once(void) {
    if (tried) return;
    tried = 1;
    load_data();
    if (!loaded_ok) open_picker();   // no --data given → offer the picker instead of the bare error
}
static void update_picker(void) {
    int maxrows = (SCREEN_H - PICK_Y0 - 20) / PICK_RH; if (maxrows < 1) maxrows = 1;
    if ((keyp(KEY_DOWN) || keyp('S')) && n_plans) sel = (sel + 1) % n_plans;
    if ((keyp(KEY_UP)   || keyp('W')) && n_plans) sel = (sel + n_plans - 1) % n_plans;

    const char *t = text_input();               // accept typed digits into the id field
    for (const char *p = t; *p; p++)
        if (*p >= '0' && *p <= '9' && (int)strlen(idbuf) < (int)sizeof idbuf - 1) {
            char c[2] = { *p, 0 }; strcat(idbuf, c); pickmsg[0] = 0;
        }
    if (keyp(KEY_BACKSPACE) && idbuf[0]) { idbuf[strlen(idbuf) - 1] = 0; pickmsg[0] = 0; }
    if (idbuf[0])                               // a typed id that matches a local plan highlights it
        for (int i = 0; i < n_plans; i++)
            if (!strcmp(plans[i].file, idbuf)) { sel = i; break; }

    // keep the selection on screen
    if (sel < scroll) scroll = sel;
    if (sel >= scroll + maxrows) scroll = sel - maxrows + 1;
    if (scroll < 0) scroll = 0;

    if (mouse_pressed(0)) {                      // single click on a row = select + load
        int my = mouse_y();
        if (my >= PICK_Y0) {
            int row = scroll + (my - PICK_Y0) / PICK_RH;
            if (row >= 0 && row < n_plans) { sel = row; load_plan(row); }
        }
    }
    if (keyp(KEY_ENTER)) { if (idbuf[0]) load_id(); else load_plan(sel); }
    if (keyp(KEY_ESCAPE) && loaded_ok) picking = false;   // back out only if a plan is loaded
}
static void draw_picker(void) {
    camera(0, 0);
    cls(C_BG);
    print("FLOORPLAN", 8, 6, CLR_LIGHT_GREY);
    font(FONT_SMALL);
    print("[up/down] pick   [enter] load   type an id   drag a .json in", 8, 17, CLR_DARK_GREY);
    font(FONT_NORMAL);

    int maxrows = (SCREEN_H - PICK_Y0 - 20) / PICK_RH; if (maxrows < 1) maxrows = 1;
    if (n_plans == 0) {
        font(FONT_SMALL);
        print("no plans in this folder yet -- fetch one:", 8, PICK_Y0, CLR_ORANGE);
        print("node data-tools/fmltools/floorplanner.js <id> --play", 8, PICK_Y0 + 11, CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    } else {
        for (int i = scroll; i < n_plans && i < scroll + maxrows; i++) {
            int ry = PICK_Y0 + (i - scroll) * PICK_RH;
            bool on = (i == sel);
            if (on) rectfill(4, ry - 1, SCREEN_W - 8, PICK_RH, C_SEL);
            print(plans[i].file, 8, ry + 1, on ? CLR_WHITE : CLR_LIGHT_GREY);
            if (plans[i].name[0]) {
                font(FONT_SMALL);
                print(plans[i].name, 104, ry + 2, on ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY);
                font(FONT_NORMAL);
            }
        }
        if (scroll + maxrows < n_plans) {
            font(FONT_SMALL); print("...more", 8, PICK_Y0 + maxrows * PICK_RH, CLR_DARK_GREY); font(FONT_NORMAL);
        }
    }
    if (pickmsg[0]) { font(FONT_SMALL); print(pickmsg, 8, SCREEN_H - 22, CLR_ORANGE); font(FONT_NORMAL); }
    char entry[64]; snprintf(entry, sizeof entry, "id: %s_", idbuf);
    print(entry, 8, SCREEN_H - 12, CLR_WHITE);
}

// ---- geometry helpers (identical to floorwalker) ----
static bool seg_block(float px, float py, const Seg *s) {
    float vx = s->bx - s->ax, vy = s->by - s->ay;
    float L = sqrtf(vx * vx + vy * vy);
    if (L < 0.001f) return false;
    vx /= L; vy /= L;
    float wx = px - s->ax, wy = py - s->ay;
    float along = wx * vx + wy * vy;
    float perp  = wx * -vy + wy * vx;
    return along >= 0 && along <= L && fabsf(perp) <= s->thick * 0.5f + PLAYER_R;
}
static bool in_box(float px, float py, const Furn *f, float r) {
    float rad = -f->rot * (float)M_PI / 180.0f;
    float dxp = px - f->cx, dyp = py - f->cy;
    float lx = dxp * cosf(rad) - dyp * sinf(rad);
    float ly = dxp * sinf(rad) + dyp * cosf(rad);
    return fabsf(lx) <= f->w * 0.5f + r && fabsf(ly) <= f->h * 0.5f + r;
}
static bool passable(float x, float y) {
    for (int i = 0; i < n_walls; i++)   if (seg_block(x, y, &walls[i]))   return false;
    for (int i = 0; i < n_windows; i++) if (seg_block(x, y, &windows[i])) return false;
    for (int i = 0; i < n_furn; i++)    if (in_box(x, y, &furn[i], PLAYER_R)) return false;
    return true;
}
static void move_axis(float nx, float ny) {
    if (passable(nx, pl.y)) pl.x = nx;
    if (passable(pl.x, ny)) pl.y = ny;
}

// ---- drawing (identical to floorwalker) ----
static void quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, int col) {
    trifill((int)x0, (int)y0, (int)x1, (int)y1, (int)x2, (int)y2, col);
    trifill((int)x0, (int)y0, (int)x2, (int)y2, (int)x3, (int)y3, col);
}
static void thick_seg(const Seg *s, int col) {
    float dx2 = s->bx - s->ax, dy2 = s->by - s->ay;
    float len = sqrtf(dx2 * dx2 + dy2 * dy2);
    if (len < 0.001f) { circfill((int)s->ax, (int)s->ay, (int)(s->thick * 0.5f), col); return; }
    float nx = -dy2 / len * s->thick * 0.5f, ny = dx2 / len * s->thick * 0.5f;
    quad(s->ax + nx, s->ay + ny, s->bx + nx, s->by + ny,
         s->bx - nx, s->by - ny, s->ax - nx, s->ay - ny, col);
}
static void poly_fill(int off, int n, int col) {
    float ymin = 1e9f, ymax = -1e9f;
    for (int k = 0; k < n; k++) { float y = apts[(off + k) * 2 + 1]; if (y < ymin) ymin = y; if (y > ymax) ymax = y; }
    for (int y = (int)ymin; y <= (int)ymax; y++) {
        float xs[16]; int nx = 0;
        for (int k = 0; k < n && nx < 16; k++) {
            float x0 = apts[(off + k) * 2], y0 = apts[(off + k) * 2 + 1];
            int j = (k + 1) % n;
            float x1 = apts[(off + j) * 2], y1 = apts[(off + j) * 2 + 1];
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y))
                xs[nx++] = x0 + (y - y0) / (y1 - y0) * (x1 - x0);
        }
        for (int a = 0; a < nx - 1; a++)
            for (int b = a + 1; b < nx; b++)
                if (xs[b] < xs[a]) { float t = xs[a]; xs[a] = xs[b]; xs[b] = t; }
        for (int p = 0; p + 1 < nx; p += 2)
            line((int)xs[p], y, (int)xs[p + 1], y, col);
    }
}
// scanline-fill a floor-covering polygon (spts[]), tiling its material texture in WORLD
// space (so the pattern is camera-stable). tex < 0 -> flat colour.
static void surf_fill(int si) {
    int off = surfs[si].off, n = surfs[si].n, tex = surfs[si].tex, tile = surfs[si].tile;
    if (tile < 1) tile = 1;
    int textured = (tex >= 0 && tex < n_texs && texs[tex].w > 0);
    Spr t = textured ? texs[tex] : (Spr){ 0, 0, 0 };
    float ymin = 1e9f, ymax = -1e9f;
    for (int k = 0; k < n; k++) { float y = spts[(off + k) * 2 + 1]; if (y < ymin) ymin = y; if (y > ymax) ymax = y; }
    for (int y = (int)ymin; y <= (int)ymax; y++) {
        float xs[16]; int nx = 0;
        for (int k = 0; k < n && nx < 16; k++) {
            float x0 = spts[(off + k) * 2], y0 = spts[(off + k) * 2 + 1];
            int j = (k + 1) % n;
            float x1 = spts[(off + j) * 2], y1 = spts[(off + j) * 2 + 1];
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y))
                xs[nx++] = x0 + (y - y0) / (y1 - y0) * (x1 - x0);
        }
        for (int a = 0; a < nx - 1; a++)
            for (int b = a + 1; b < nx; b++)
                if (xs[b] < xs[a]) { float tt = xs[a]; xs[a] = xs[b]; xs[b] = tt; }
        for (int p = 0; p + 1 < nx; p += 2) {
            int xa = (int)xs[p], xb = (int)xs[p + 1];
            if (!textured) { line(xa, y, xb, y, surfs[si].c); continue; }
            int v = (((y % tile) + tile) % tile) * t.h / tile;
            for (int x = xa; x <= xb; x++) {
                int u = (((x % tile) + tile) % tile) * t.w / tile;
                pset(x, y, tpool[t.off + v * t.w + u]);
            }
        }
    }
}

static void draw_furn(const Furn *f) {
    float rad = f->rot * (float)M_PI / 180.0f;
    float c = cosf(rad), s = sinf(rad);
    float hw = f->w * 0.5f, hh = f->h * 0.5f;
    float cx[4], cy[4];
    float ox[4] = { -hw, hw, hw, -hw }, oy[4] = { -hh, -hh, hh, hh };
    for (int k = 0; k < 4; k++) {
        cx[k] = f->cx + ox[k] * c - oy[k] * s;
        cy[k] = f->cy + ox[k] * s + oy[k] * c;
    }
    quad(cx[0], cy[0], cx[1], cy[1], cx[2], cy[2], cx[3], cy[3], C_FURN);
    for (int k = 0; k < 4; k++)
        line((int)cx[k], (int)cy[k], (int)cx[(k + 1) % 4], (int)cy[(k + 1) % 4], C_FURN_O);
}
static unsigned char spr_at(const Furn *f, Spr s, float c, float sn, int dx, int dy) {
    float lx =  dx * c + dy * sn;
    float ly = -dx * sn + dy * c;
    float hw = f->w * 0.5f, hh = f->h * 0.5f;
    if (lx < -hw || lx >= hw || ly < -hh || ly >= hh) return 255;
    int u = (int)((lx + hw) / f->w * s.w);
    int v = (int)((ly + hh) / f->h * s.h);
    if (u < 0 || u >= s.w || v < 0 || v >= s.h) return 255;
    return pool[s.off + v * s.w + u];
}
static bool blit_spr(const Furn *f) {
    if (f->ref < 0 || f->ref >= n_spr || sprs[f->ref].w == 0) return false;
    Spr s = sprs[f->ref];
    float rad = f->rot * (float)M_PI / 180.0f, c = cosf(rad), sn = sinf(rad);
    float hw = f->w * 0.5f, hh = f->h * 0.5f;
    int ex = (int)(fabsf(hw * c) + fabsf(hh * sn)) + 2;
    int ey = (int)(fabsf(hw * sn) + fabsf(hh * c)) + 2;
    int cx = (int)f->cx, cy = (int)f->cy;
    if (outline_on)
        for (int dy = -ey; dy <= ey; dy++) for (int dx = -ex; dx <= ex; dx++)
            if (spr_at(f, s, c, sn, dx, dy) == 255 &&
                (spr_at(f, s, c, sn, dx - 1, dy) != 255 || spr_at(f, s, c, sn, dx + 1, dy) != 255 ||
                 spr_at(f, s, c, sn, dx, dy - 1) != 255 || spr_at(f, s, c, sn, dx, dy + 1) != 255))
                pset(cx + dx, cy + dy, C_FURN_O);
    for (int dy = -ey; dy <= ey; dy++) for (int dx = -ex; dx <= ex; dx++) {
        unsigned char idx = spr_at(f, s, c, sn, dx, dy);
        if (idx != 255) pset(cx + dx, cy + dy, idx);
    }
    return true;
}
static void blit_furn(const Furn *f) { if (!blit_spr(f)) draw_furn(f); }
static void draw_door(const Door *d) {
    float rad = d->rot * (float)M_PI / 180.0f;
    float ax = cosf(rad), ay = sinf(rad);
    float px = -ay, py = ax;
    float hw = d->w * 0.5f;
    int hx = (int)(d->cx - ax * hw), hy = (int)(d->cy - ay * hw);
    int ox = (int)(d->cx + ax * hw), oy = (int)(d->cy + ay * hw);
    line(hx, hy, (int)(hx + px * d->w), (int)(hy + py * d->w), C_DOOR);
    circfill(hx, hy, 1, C_DOOR);
    circfill(ox, oy, 1, C_DOOR);
}
static int cam_axis(float p, int world, int screen) {
    if (world <= screen) return -(screen - world) / 2;
    return (int)clamp(p - screen / 2.0f, 0, world - screen);
}

void init(void) {
    pl.x = spawn_x; pl.y = spawn_y; pl.aim = 0;
}

void update(void) {
    boot_once();
    const char *dropped = de_dropped_file();   // drag a .json onto the window to swap plans
    if (dropped) { load_from(dropped); if (loaded_ok) picking = false; }
    if (keyp(KEY_TAB)) { if (!picking) open_picker(); else if (loaded_ok) picking = false; }
    if (picking) { update_picker(); return; }  // the picker eats input while it's open
    if (!loaded_ok) return;

    float d = dt();
    int mvx = (key('D') || key(KEY_RIGHT) ? 1 : 0) - (key('A') || key(KEY_LEFT) ? 1 : 0);
    int mvy = (key('S') || key(KEY_DOWN) ? 1 : 0) - (key('W') || key(KEY_UP) ? 1 : 0);
    if (mvx || mvy) {
        float a = angle_to(0, 0, mvx * 100, mvy * 100);
        float step = 70.0f * d;
        move_axis(pl.x + dx(step, a), pl.y + dy(step, a));
        pl.aim = a;
    }
    if (keyp('T')) use_sprites = !use_sprites;
    if (keyp('O')) outline_on = !outline_on;
    int cx = cam_axis(pl.x, world_w, SCREEN_W);
    int cy = cam_axis(pl.y, world_h, SCREEN_H);
    camera_ex(cx, cy, CAM_ZOOM, 0);
    pl.aim = angle_to((int)pl.x, (int)pl.y, mouse_world_x(), mouse_world_y());
}

void draw(void) {
    boot_once();
    camera(0, 0);
    cls(C_BG);

    if (picking) { draw_picker(); return; }

    if (!loaded_ok) {
        print_centered("FLOORPLAN", SCREEN_W / 2, SCREEN_H / 2 - 12, CLR_LIGHT_GREY);
        font(FONT_SMALL);
        print_centered(err, SCREEN_W / 2, SCREEN_H / 2 + 2, CLR_ORANGE);
        print_centered("drag a floorplan .json onto this window", SCREEN_W / 2, SCREEN_H / 2 + 14, CLR_DARK_GREY);
        font(FONT_NORMAL);
        return;
    }

    int cx = cam_axis(pl.x, world_w, SCREEN_W);
    int cy = cam_axis(pl.y, world_h, SCREEN_H);
    camera_ex(cx, cy, CAM_ZOOM, 0);

    for (int i = 0; i < n_areas; i++)
        poly_fill(areas[i].off, areas[i].n, areas[i].color);
    int vx0 = cx - 40, vy0 = cy - 40, vx1 = cx + SCREEN_W + 40, vy1 = cy + SCREEN_H + 40;
    // floor coverings (textured/flat) on top of the room base colours, beneath furniture
    for (int i = 0; i < n_surfs; i++) {
        int off = surfs[i].off, n = surfs[i].n;
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        for (int k = 0; k < n; k++) { float X = spts[(off + k) * 2], Y = spts[(off + k) * 2 + 1]; if (X < minx) minx = X; if (X > maxx) maxx = X; if (Y < miny) miny = Y; if (Y > maxy) maxy = Y; }
        if (maxx < vx0 || minx > vx1 || maxy < vy0 || miny > vy1) continue;   // cull off-screen
        surf_fill(i);
    }
    for (int i = 0; i < n_furn; i++) {
        const Furn *f = &furn[i];
        if (f->cx < vx0 || f->cx > vx1 || f->cy < vy0 || f->cy > vy1) continue;
        if (use_sprites) blit_furn(f); else draw_furn(f);
    }
    for (int i = 0; i < n_walls; i++) thick_seg(&walls[i], C_WALL);
    for (int i = 0; i < n_windows; i++) thick_seg(&windows[i], C_GLASS);
    for (int i = 0; i < n_doors; i++) {
        const Door *o = &doors[i];
        if (o->cx < vx0 || o->cx > vx1 || o->cy < vy0 || o->cy > vy1) continue;
        draw_door(o);
    }
    circfill((int)pl.x, (int)pl.y, (int)PLAYER_R, C_PLAYER);
    line((int)pl.x, (int)pl.y, (int)(pl.x + dx(7, pl.aim)), (int)(pl.y + dy(7, pl.aim)), CLR_WHITE);

    // HUD
    camera(0, 0);
    char buf[96];
    snprintf(buf, sizeof buf, "%s  furn:%d  [T] %s  [TAB] plans%s", dname[0] ? dname : "FLOORPLAN", n_furn,
             use_sprites ? "sprites" : "boxes", truncated ? "  (truncated)" : "");
    print(buf, 4, 4, CLR_WHITE);
}
