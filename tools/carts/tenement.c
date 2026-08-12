/* de:meta
{
  "title": "tenement",
  "slug": "tenement",
  "kind": ["probe", "tech-demo"],
  "teaches": ["isometric-projection", "finite-state-ai"],
  "created": "2026-08-12",
  "lineage": "Thin vertical slice of the tenement sim: proves ONE claim, that a single argmax over every (object, need) pair produces different, better behaviour than the urgency-sort every other needs sim here uses. sims sorts needs then finds an object; this lets an adjacent nearly-free toilet outbid a distant fridge at higher hunger, which is what people recognise as Sims-like. Built against the frozen contract runtime/tenement/model.h before any fan-out, because that contract's centrepiece had never been exercised by a line of code.",
  "todo": [
    "SLICE, NOT THE GAME. Present: the offer index, needs decay, contention via queue penalty, and the spec that pins the argmax. Absent: households, money, work orders, storage, rent, building, pathfinding. Those are the fan-out.",
    "Distance is straight-line, not a path. Enough to prove the DECISION mechanism; the contention claim in design section 1 (corridors jam) needs real pathfinding before it can be judged.",
    "One file, so the contract's per-module owner comments are unfulfilled. The module split (world/offer/agents/work/econ/art/hud) comes with the fan-out.",
    "The iso projection is COPIED from isoroom. Extract runtime/isoview.h when the second consumer proves the shape, per ADR-0006 (a library header wants real consumers first, not speculation).",
    "Objects are placeholders and the tag vocabulary is a first guess. Both should be revised by what the fan-out learns, not defended."
  ],
  "description": {
    "summary": "A few residents, some furniture, and one question: does the best offer on the table beat picking your worst need first? Watch the scores and see.",
    "detail": "The thin vertical slice of a bigger sim about several households sharing one building. Every object advertises what it offers, how strongly, for how long, and for how many people at once. Every resident does no thinking at all beyond taking the single best offer available, where a need's deficit is one term in the score rather than a filter applied first. That distinction is the whole point: the usual way to write this is to sort needs by urgency and then look for an object, which sends a hungry person past an empty toilet to a fridge on the far side of the building. Here the near thing can win. The HUD shows each resident's current winning bid and its score, because the interesting part of this simulation is invisible otherwise.",
    "controls": "Q/E turn the building. TAB shows every bid the winning resident considered, not just the winner. SPACE pauses. 1/2/4 set speed."
  }
}
de:meta */

// tenement — the thin vertical slice.
//
// Design: docs/design/tenement.md   Contract: runtime/tenement/model.h
//
// WHAT THIS EXISTS TO PROVE, and nothing else: that the contract's tn_best_action() can express
// smart-object advertisement, and that advertisement is observably different from the urgency-sort
// that `sims` uses. spec() case 1 is the whole argument in one assertion.
//
// It is a slice, so it is one file. The module split comes with the fan-out; see de:meta.todo.

#include "studio.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "spec.h"
#include "tenement/model.h"
#include "tenement/atlas.h"

// ── the slice's building: a corner of the contract's grid ────────────────────
// TN_MW/TN_MH in the contract are ARRAY BOUNDS. This is the building actually used, sized so its
// diamond fits 320px: the span is (W+H) * TN_TILE_PX/2, so 13+9 is 264 of 320.
#define B_W 13
#define B_H  9

// ── contract globals: defined here for the slice (the fan-out will re-home them) ──
TnObject    tn_obj[TN_MAX_OBJECTS];       int tn_obj_n;
TnItem      tn_item[TN_MAX_ITEMS];        int tn_item_n;
TnAgent     tn_agent[TN_MAX_AGENTS];      int tn_agent_n;
TnHousehold tn_house[TN_MAX_HOUSEHOLDS];  int tn_house_n;
TnOrder     tn_order[TN_MAX_ORDERS];      int tn_order_n;
TnClock     tn_clock;
int         tn_rot;

// ─────────────────────────────────────────────────────────────────────────────
// THE OFFER TABLE — the data that replaces code (contract rule 3).
//
// Every row here is a thing an object says about itself. Nothing anywhere asks "is this a fridge".
// Note the fridge declaring TWO offers, one of which is storage: that is the whole reason needs,
// capabilities and storage classes share one tag namespace.
// ─────────────────────────────────────────────────────────────────────────────
const TnOffer TN_OFFERS[TN_OBJ_KIND_COUNT][TN_MAX_OFFERS] = {
    [TN_OBJ_BED]      = { { TN_SERVE_REST,    120, 480, 1 } },   // 8 hours; short, not char
    [TN_OBJ_FRIDGE]   = { { TN_SERVE_HUNGER,  100,  30, 1 },
                          { TN_STORE_FOOD,     -1,   0, 8 } },
    [TN_OBJ_COUNTER]  = { { TN_SERVE_HUNGER,   40,  20, 1 },     // a snack, worse than the fridge
                          { TN_CAP_WORK,       -1,   0, 1 } },
    [TN_OBJ_TOILET]   = { { TN_SERVE_BLADDER, 110,  10, 1 } },
    [TN_OBJ_SOFA]     = { { TN_SERVE_FUN,      90,  60, 2 } },   // the only shareable thing here
    [TN_OBJ_LOOM]     = { { TN_CAP_WORK,       -1, 480, 1 } },   // the dumb machine, design §4
    [TN_OBJ_WARDROBE] = { { TN_STORE_CLOTHES,  -1,   0, 6 } },
};
const unsigned char TN_OFFER_N[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED] = 1, [TN_OBJ_FRIDGE] = 2, [TN_OBJ_COUNTER] = 2, [TN_OBJ_TOILET] = 1,
    [TN_OBJ_SOFA] = 1, [TN_OBJ_LOOM] = 1, [TN_OBJ_WARDROBE] = 1,
};
const short TN_OBJ_PRICE[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED] = 120, [TN_OBJ_FRIDGE] = 200, [TN_OBJ_COUNTER] = 60, [TN_OBJ_TOILET] = 90,
    [TN_OBJ_SOFA] = 140, [TN_OBJ_LOOM] = 300, [TN_OBJ_WARDROBE] = 80,
};

// v1 recipes: a dumb machine turns TIME into a GOOD. in_n == 0 is the marked open loop (design §5).
const TnRecipe TN_RECIPES[] = {
    { TN_CAP_WORK, 0, 0, TN_STORE_GOODS, 12, 480 },
};
const int TN_RECIPE_N = 1;

// which sprite each object kind draws with. ART ONLY — never branched on for behaviour.
static const int OBJ_CELL[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED] = ISO_BED, [TN_OBJ_FRIDGE] = ISO_FRIDGE, [TN_OBJ_COUNTER] = ISO_COUNTER,
    [TN_OBJ_TOILET] = ISO_TOILET, [TN_OBJ_SOFA] = ISO_SOFA, [TN_OBJ_LOOM] = ISO_LOOM,
    [TN_OBJ_WARDROBE] = ISO_WARDROBE,
};
static const char *TAG_NAME[TN_TAG_COUNT] = {
    [TN_SERVE_HUNGER]="HUNGER", [TN_SERVE_REST]="REST", [TN_SERVE_HYGIENE]="HYGIENE",
    [TN_SERVE_BLADDER]="BLADDER", [TN_SERVE_FUN]="FUN", [TN_SERVE_COUNT]="-",
    [TN_CAP_WORK]="WORK", [TN_CAP_HEAT]="HEAT", [TN_CAP_CUT]="CUT", [TN_CAP_POWER]="POWER",
    [TN_STORE_FOOD]="ST_FOOD", [TN_STORE_GOODS]="ST_GOODS", [TN_STORE_CLOTHES]="ST_CLOTHES",
};

// ── slice state ─────────────────────────────────────────────────────────────
static int   sim_paused = 0, speed = 1, show_bids = 0;
static TnTag last_tag[TN_MAX_AGENTS];
static int   last_score[TN_MAX_AGENTS];

// ─────────────────────────────────────────────────────────────────────────────
// THE OFFER INDEX
// ─────────────────────────────────────────────────────────────────────────────
bool tn_offers(int obj, TnTag tag, int *strength) {
    const int kind = tn_obj[obj].kind;
    for (int i = 0; i < TN_OFFER_N[kind]; i++) {
        if (TN_OFFERS[kind][i].tag == (unsigned char)tag) {
            if (strength) *strength = TN_OFFERS[kind][i].strength;
            return true;
        }
    }
    return false;
}

static const TnOffer *offer_of(int obj, TnTag tag) {
    const int kind = tn_obj[obj].kind;
    for (int i = 0; i < TN_OFFER_N[kind]; i++)
        if (TN_OFFERS[kind][i].tag == (unsigned char)tag) return &TN_OFFERS[kind][i];
    return NULL;
}

// Straight-line tile distance. A PATH would be correct; see de:meta.todo. It is enough to prove the
// decision mechanism, and swapping it for a path changes only this function.
static int travel_cost(int agent, int obj) {
    const int dx = tn_agent[agent].tx - tn_obj[obj].tx;
    const int dy = tn_agent[agent].ty - tn_obj[obj].ty;
    return (int)(sqrtf((float)(dx * dx + dy * dy)) + 0.5f);
}

// THE SCORE, in one place so no module invents its own (contract, offer-index block).
//     deficit * strength / (travel + queue)
// Every term a number, which is what makes the choice oracle-able.
#define QUEUE_FULL 1000                   // an occupied capacity-1 object is not worth waiting for
static int score_offer(int agent, int obj, const TnOffer *of, TnTag tag) {
    if (of->strength < 0) return -1;                       // capability/storage: not a need bid
    const int deficit = 255 - tn_agent[agent].need[tag];
    if (deficit <= 0) return -1;
    int queue = 0;
    if (tn_obj[obj].users >= of->capacity) queue = QUEUE_FULL;
    else queue = tn_obj[obj].users * 4;                    // sharing is worse than being alone
    return deficit * of->strength / (travel_cost(agent, obj) + 1 + queue);
}

// ONE argmax over EVERY (object, need) pair. Not "most urgent need, then an object for it".
// See the contract's offer-index block for why that distinction is the entire design.
int tn_best_action(int agent, TnTag *out_tag, int *out_score) {
    int best = -1, best_score = 0; TnTag best_tag = TN_SERVE_COUNT;
    for (int o = 0; o < tn_obj_n; o++) {
        const int kind = tn_obj[o].kind;
        for (int i = 0; i < TN_OFFER_N[kind]; i++) {
            const TnOffer *of = &TN_OFFERS[kind][i];
            if (of->tag >= TN_SERVE_COUNT) continue;        // only needs bid for attention
            const int s = score_offer(agent, o, of, (TnTag)of->tag);
            if (s > best_score) { best_score = s; best = o; best_tag = (TnTag)of->tag; }
        }
    }
    if (out_tag)   *out_tag   = best_tag;
    if (out_score) *out_score = best_score;
    return best;
}

int tn_best_offer(int agent, TnTag tag, int *out_score) {
    int best = -1, best_score = 0;
    for (int o = 0; o < tn_obj_n; o++) {
        const TnOffer *of = offer_of(o, tag);
        if (!of) continue;
        const int s = (of->strength < 0) ? (1000 / (travel_cost(agent, o) + 1))
                                        : score_offer(agent, o, of, tag);
        if (s > best_score) { best_score = s; best = o; }
    }
    if (out_score) *out_score = best_score;
    return best;
}

int tn_find_workspot(int agent, TnTag cap) { return tn_best_offer(agent, cap, NULL); }

int tn_find_store(int agent, int item) {
    return tn_best_offer(agent, (TnTag)tn_item[item].store_tag, NULL);
}

void tn_sell(int household, int item) {                    // TN_SEAM_EXTERNAL (design §5)
    tn_house[household].money += tn_item[item].value;
}

// ─────────────────────────────────────────────────────────────────────────────
// SIM
// ─────────────────────────────────────────────────────────────────────────────
static void add_obj(int kind, int tx, int ty, int household) {
    if (tn_obj_n >= TN_MAX_OBJECTS) return;
    tn_obj[tn_obj_n++] = (TnObject){ (unsigned char)kind, (unsigned char)tx, (unsigned char)ty,
                                     0, (signed char)household, 0 };
}
static void add_agent(int household, int tx, int ty) {
    if (tn_agent_n >= TN_MAX_AGENTS) return;
    TnAgent *a = &tn_agent[tn_agent_n++];
    *a = (TnAgent){0};
    a->species = TN_SPECIES_ADULT; a->household = (unsigned char)household;
    a->tx = (short)tx; a->ty = (short)ty; a->target_obj = -1; a->carrying = -1;
    for (int n = 0; n < TN_NEED_COUNT; n++) a->need[n] = (unsigned char)(150 + 20 * n);
}

void tn_world_init(void) {
    tn_obj_n = tn_agent_n = tn_item_n = tn_order_n = 0;
    tn_house_n = 2;
    for (int h = 0; h < tn_house_n; h++) tn_house[h] = (TnHousehold){ 200, {0}, 0, 20, (unsigned char)h };
    // Household 0, left half. Household 1, right half. One communal loom in the middle: the
    // contention that design §1 is about starts the moment two households want one machine.
    add_obj(TN_OBJ_BED,      1, 1, 0);  add_obj(TN_OBJ_FRIDGE,  1, 3, 0);
    add_obj(TN_OBJ_TOILET,   3, 1, 0);  add_obj(TN_OBJ_SOFA,    1, 6, 0);
    add_obj(TN_OBJ_WARDROBE, 3, 3, 0);
    add_obj(TN_OBJ_BED,     11, 1, 1);  add_obj(TN_OBJ_FRIDGE, 11, 3, 1);
    add_obj(TN_OBJ_TOILET,   9, 1, 1);  add_obj(TN_OBJ_COUNTER,11, 6, 1);
    add_obj(TN_OBJ_LOOM,     6, 4, -1);
    add_agent(0, 2, 2); add_agent(0, 2, 5);
    add_agent(1, 10, 2); add_agent(1, 10, 5);
    tn_clock = (TnClock){ 8 * 60, 1 };
    for (int i = 0; i < TN_MAX_AGENTS; i++) { last_tag[i] = TN_SERVE_COUNT; last_score[i] = 0; }
}

// Decay rates per need, per hour. DATA, per seam 1: a new need is a row here, not a code path.
static const unsigned char DECAY[TN_NEED_COUNT] = { 8, 6, 5, 10, 4 };

void tn_agents_tick(void) {
    tn_clock.minute += 1;
    if (tn_clock.minute >= 1440) { tn_clock.minute = 0; tn_clock.day++; }

    for (int i = 0; i < tn_agent_n; i++) {
        TnAgent *a = &tn_agent[i];
        if (tn_clock.minute % 60 == 0)
            for (int n = 0; n < TN_NEED_COUNT; n++)
                a->need[n] = (unsigned char)(a->need[n] > DECAY[n] ? a->need[n] - DECAY[n] : 0);

        switch (a->activity) {
        case TN_ACT_USE:
            if (tn_clock.minute >= a->until || a->until < 0) {
                if (a->target_obj >= 0) {
                    const TnOffer *of = offer_of(a->target_obj, last_tag[i]);
                    if (of && of->strength > 0) {
                        const int v = a->need[last_tag[i]] + of->strength;
                        a->need[last_tag[i]] = (unsigned char)(v > 255 ? 255 : v);
                    }
                    if (tn_obj[a->target_obj].users > 0) tn_obj[a->target_obj].users--;
                }
                a->target_obj = -1; a->activity = TN_ACT_IDLE;
            }
            break;
        case TN_ACT_WALK:
            if (a->target_obj < 0) { a->activity = TN_ACT_IDLE; break; }
            {
                const int ox = tn_obj[a->target_obj].tx, oy = tn_obj[a->target_obj].ty;
                if (a->tx != ox) a->tx += (a->tx < ox) ? 1 : -1;
                else if (a->ty != oy) a->ty += (a->ty < oy) ? 1 : -1;
                a->facing = (unsigned char)((a->tx < ox) ? 1 : (a->tx > ox) ? 3 : (a->ty < oy) ? 2 : 0);
                if (abs(a->tx - ox) + abs(a->ty - oy) <= 1) {
                    const TnOffer *of = offer_of(a->target_obj, last_tag[i]);
                    if (of && tn_obj[a->target_obj].users < of->capacity) {
                        tn_obj[a->target_obj].users++;
                        a->activity = TN_ACT_USE;
                        a->until = (short)((tn_clock.minute + of->minutes) % 1440);
                    } else { a->target_obj = -1; a->activity = TN_ACT_IDLE; }
                }
            }
            break;
        default: {                                          // IDLE: take the best offer going
            TnTag tag; int score;
            const int o = tn_best_action(i, &tag, &score);
            last_tag[i] = tag; last_score[i] = score;
            if (o >= 0) { a->target_obj = (signed char)o; a->activity = TN_ACT_WALK; }
            break;
        }
        }
    }
}

void tn_work_tick(void) {}                                  // fan-out
void tn_econ_tick(void) {}                                  // fan-out

// ─────────────────────────────────────────────────────────────────────────────
// ART — the iso projection, COPIED from isoroom (see de:meta.todo about isoview.h)
// ─────────────────────────────────────────────────────────────────────────────
static float cam_x, cam_y;
static void iso_turn(int q, float x, float y, float *X, float *Y) {
    switch (q & 3) { case 0: *X= x; *Y= y; break; case 1: *X=-y; *Y= x; break;
                     case 2: *X=-x; *Y=-y; break; default: *X= y; *Y=-x; break; }
}
static void iso_project(int r, float vx, float vy, float vz, float *sx, float *sy) {
    float X, Y; iso_turn(r, vx, vy, &X, &Y);
    *sx = (X - Y) * (ISO_TW * 0.5f);
    *sy = (X + Y) * (ISO_TH * 0.5f) - vz * ISO_ZH;
}
static float iso_depth(int r, float vx, float vy) {
    float X, Y; iso_turn(r, vx, vy, &X, &Y); return X + Y;
}
static void iso_camera(void) {
    float minx=1e9f, maxx=-1e9f, miny=1e9f, maxy=-1e9f;
    const float W = B_W * TN_TILE_VOX, H = B_H * TN_TILE_VOX;
    for (int c = 0; c < 8; c++) {
        float sx, sy;
        iso_project(tn_rot, (c&1)?W:0, (c&2)?H:0, (c&4)?12.0f:0, &sx, &sy);
        if (sx<minx) minx=sx; if (sx>maxx) maxx=sx; if (sy<miny) miny=sy; if (sy>maxy) maxy=sy;
    }
    // Floored: a half-pixel camera rounds adjacent sprites opposite ways and opens 1px seams
    // (iso-rooms.md §7). Learned there, not rediscovered here.
    cam_x = floorf((SCREEN_W - (maxx - minx)) * 0.5f - minx);
    cam_y = floorf((SCREEN_H - 26 - (maxy - miny)) * 0.5f - miny + 12);
}

typedef struct { float depth; int cell, rot; float vx, vy; int fp0, fp1; } Draw;
static Draw dl[TN_MAX_OBJECTS + TN_MAX_AGENTS];
static int dl_n;
static int cmp_draw(const void *a, const void *b) {
    const float d = ((const Draw*)a)->depth - ((const Draw*)b)->depth;
    return d < 0 ? -1 : (d > 0 ? 1 : 0);
}

void tn_draw_world(void) {
    for (int ty = 0; ty < B_H; ty++) for (int tx = 0; tx < B_W; tx++) {
        float c[4][2]; const float vx = tx*TN_TILE_VOX, vy = ty*TN_TILE_VOX;
        iso_project(tn_rot, vx,             vy,             0, &c[0][0], &c[0][1]);
        iso_project(tn_rot, vx+TN_TILE_VOX, vy,             0, &c[1][0], &c[1][1]);
        iso_project(tn_rot, vx+TN_TILE_VOX, vy+TN_TILE_VOX, 0, &c[2][0], &c[2][1]);
        iso_project(tn_rot, vx,             vy+TN_TILE_VOX, 0, &c[3][0], &c[3][1]);
        quadfill((int)(c[0][0]+cam_x),(int)(c[0][1]+cam_y), (int)(c[1][0]+cam_x),(int)(c[1][1]+cam_y),
                 (int)(c[2][0]+cam_x),(int)(c[2][1]+cam_y), (int)(c[3][0]+cam_x),(int)(c[3][1]+cam_y),
                 ((tx+ty)&1) ? CLR_BROWN : CLR_DARK_BROWN);
    }
    dl_n = 0;
    for (int o = 0; o < tn_obj_n; o++) {
        const int cell = OBJ_CELL[tn_obj[o].kind];
        const short *fp = ISO_FOOTPRINT[cell];
        dl[dl_n++] = (Draw){ iso_depth(tn_rot, tn_obj[o].tx*TN_TILE_VOX + fp[0]*0.5f,
                                               tn_obj[o].ty*TN_TILE_VOX + fp[1]*0.5f),
                             cell, tn_rot, (float)tn_obj[o].tx*TN_TILE_VOX,
                             (float)tn_obj[o].ty*TN_TILE_VOX, fp[0], fp[1] };
    }
    for (int i = 0; i < tn_agent_n; i++) {
        const short *fp = ISO_FOOTPRINT[ISO_PERSON];
        dl[dl_n++] = (Draw){ iso_depth(tn_rot, tn_agent[i].tx*TN_TILE_VOX + fp[0]*0.5f,
                                               tn_agent[i].ty*TN_TILE_VOX + fp[1]*0.5f),
                             ISO_PERSON, (tn_rot + tn_agent[i].facing) & 3,
                             (float)tn_agent[i].tx*TN_TILE_VOX,
                             (float)tn_agent[i].ty*TN_TILE_VOX, fp[0], fp[1] };
    }
    qsort(dl, dl_n, sizeof dl[0], cmp_draw);
    for (int i = 0; i < dl_n; i++) {
        const IsoCell *c = &ISO_CELLS[dl[i].cell][dl[i].rot];
        float sx, sy; iso_project(tn_rot, dl[i].vx, dl[i].vy, 0, &sx, &sy);
        // one-voxel-INSET contact shadow: an integer inset, because a fractional pad puts the quad
        // between lattice points and strays everywhere (iso-rooms.md §7's measured table)
        float q[4][2]; const float pad = 1.0f;
        const float x0 = dl[i].vx+pad, y0 = dl[i].vy+pad;
        const float x1 = dl[i].vx+dl[i].fp0-pad, y1 = dl[i].vy+dl[i].fp1-pad;
        iso_project(tn_rot, x0,y0,0,&q[0][0],&q[0][1]); iso_project(tn_rot, x1,y0,0,&q[1][0],&q[1][1]);
        iso_project(tn_rot, x1,y1,0,&q[2][0],&q[2][1]); iso_project(tn_rot, x0,y1,0,&q[3][0],&q[3][1]);
        quadfill((int)(q[0][0]+cam_x),(int)(q[0][1]+cam_y),(int)(q[1][0]+cam_x),(int)(q[1][1]+cam_y),
                 (int)(q[2][0]+cam_x),(int)(q[2][1]+cam_y),(int)(q[3][0]+cam_x),(int)(q[3][1]+cam_y),
                 CLR_BROWNISH_BLACK);
        sspr(c->x, c->y, c->w, c->h, (int)(sx+cam_x)-c->ox, (int)(sy+cam_y)-c->oy, c->w, c->h);
    }
}

// THE POINT OF THE HUD: the interesting part of this sim is invisible. Show the winning bid.
void tn_draw_hud(void) {
    char t[80];
    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    snprintf(t, sizeof t, "day %d  %02d:%02d   Q/E turn  TAB bids", tn_clock.day,
             tn_clock.minute / 60, tn_clock.minute % 60);
    print(t, 3, 1, CLR_LIGHT_GREY);

    rectfill(0, SCREEN_H - 26, SCREEN_W, 26, CLR_BLACK);
    // Two columns of 14 chars. The 8x8 font gives 40 columns across 320px, so a 14-char cell at
    // x=3 and x=160 fits with room to spare. The first cut used a 21-char format in a 20-char
    // column and the score ran into the next agent's id.
    for (int i = 0; i < tn_agent_n && i < 4; i++) {
        const char mark = tn_agent[i].activity == TN_ACT_USE ? '*'
                        : tn_agent[i].activity == TN_ACT_WALK ? '>' : '.';
        snprintf(t, sizeof t, "%d%d%c%-7s%4d", i, tn_agent[i].household, mark,
                 TAG_NAME[last_tag[i]] ? TAG_NAME[last_tag[i]] : "?", last_score[i]);
        print(t, 3 + (i % 2) * 160, SCREEN_H - 23 + (i / 2) * 9, CLR_WHITE);
    }
    if (show_bids) {                                  // every bid agent 0 considered, not just the winner
        int y = 12;
        for (int o = 0; o < tn_obj_n && y < SCREEN_H - 30; o++) {
            const int kind = tn_obj[o].kind;
            for (int k = 0; k < TN_OFFER_N[kind]; k++) {
                const TnOffer *of = &TN_OFFERS[kind][k];
                if (of->tag >= TN_SERVE_COUNT) continue;
                const int s = score_offer(0, o, of, (TnTag)of->tag);
                if (s <= 0) continue;
                snprintf(t, sizeof t, "%-8s %5d", TAG_NAME[of->tag], s);
                print(t, 3, y, (o == tn_agent[0].target_obj) ? CLR_YELLOW : CLR_DARK_GREY);
                y += 8;
            }
        }
    }
}

// ── entry points ────────────────────────────────────────────────────────────
void init(void) { colorkey(-1); tn_world_init(); }

void update(void) {
    if (keyp('Q')) tn_rot = (tn_rot + 3) & 3;
    if (keyp('E')) tn_rot = (tn_rot + 1) & 3;
    if (keyp(KEY_TAB)) show_bids = !show_bids;
    if (keyp(KEY_SPACE)) sim_paused = !sim_paused;
    if (keyp('1')) speed = 1;
    if (keyp('2')) speed = 2;
    if (keyp('4')) speed = 4;
    if (!sim_paused) for (int s = 0; s < speed * 4; s++) { tn_agents_tick(); tn_work_tick(); tn_econ_tick(); }
    iso_camera();
#ifdef DE_TRACE
    watch("hunger0", "%d", tn_agent[0].need[TN_SERVE_HUNGER]);
    watch("act0",    "%d", tn_agent[0].activity);
    watch("bid0",    "%d", last_score[0]);
#endif
}

void draw(void) { cls(CLR_DARK_BLUE); tn_draw_world(); tn_draw_hud(); }

// ─────────────────────────────────────────────────────────────────────────────
// spec — case 1 IS the argument for this whole design. Read it before the others.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef DE_SPEC
static char sp[160];

void spec(void) {
    // ── CASE 1: ADVERTISEMENT IS NOT URGENCY-SORT ───────────────────────────
    // Build the scenario the two models disagree about. Hunger is the WORSE need, but the fridge
    // is across the building while a toilet is right here. Urgency-sort must pick the fridge
    // (biggest deficit first). Advertisement must pick the toilet (best offer on the table).
    // If this assertion ever flips, the design has silently become `sims`.
    tn_world_init();
    tn_obj_n = 0; tn_agent_n = 0;
    add_obj(TN_OBJ_TOILET, 1, 1, 0);          // adjacent
    add_obj(TN_OBJ_FRIDGE, 12, 8, 0);         // far corner
    add_agent(0, 1, 2);
    tn_agent[0].need[TN_SERVE_HUNGER]  = 40;   // deficit 215  ← the MORE urgent need
    tn_agent[0].need[TN_SERVE_BLADDER] = 120;  // deficit 135
    for (int n = 0; n < TN_NEED_COUNT; n++)
        if (n != TN_SERVE_HUNGER && n != TN_SERVE_BLADDER) tn_agent[0].need[n] = 255;

    expect(255 - tn_agent[0].need[TN_SERVE_HUNGER] > 255 - tn_agent[0].need[TN_SERVE_BLADDER],
           "case 1 setup: hunger really is the more urgent need (so urgency-sort would pick it)");
    {
        TnTag tag; int score;
        const int o = tn_best_action(0, &tag, &score);
        snprintf(sp, sizeof sp, "the NEAR toilet outbids the FAR fridge despite lower urgency "
                                "(picked %s, score %d)", TAG_NAME[tag], score);
        expect(o == 0 && tag == TN_SERVE_BLADDER, sp);
    }
    // And the converse, so the test cannot pass by always preferring the toilet: move the fridge
    // next door and it must win, because now its bigger deficit is not paying a travel penalty.
    tn_obj[1].tx = 1; tn_obj[1].ty = 3;
    {
        TnTag tag; int score;
        const int o = tn_best_action(0, &tag, &score);
        snprintf(sp, sizeof sp, "with travel equalised the hungrier need wins (picked %s)", TAG_NAME[tag]);
        expect(o == 1 && tag == TN_SERVE_HUNGER, sp);
    }

    // ── CASE 2: CONTENTION IS IN THE SCORE ──────────────────────────────────
    // A capacity-1 object already in use must stop attracting anyone. This is what makes queues
    // and traffic emerge from the score instead of from queue-handling code.
    tn_obj_n = 0; tn_agent_n = 0;
    add_obj(TN_OBJ_TOILET, 1, 1, 0);
    add_obj(TN_OBJ_SOFA,   3, 1, 0);
    add_agent(0, 1, 2);
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 255;
    tn_agent[0].need[TN_SERVE_BLADDER] = 60;
    tn_agent[0].need[TN_SERVE_FUN]     = 90;
    {
        TnTag tag; int free_score, busy_score;
        int o = tn_best_action(0, &tag, &free_score);
        expect(o == 0 && tag == TN_SERVE_BLADDER, "case 2: a free toilet wins while it is free");
        tn_obj[0].users = 1;                              // somebody is in there
        o = tn_best_action(0, &tag, &busy_score);
        snprintf(sp, sizeof sp, "an OCCUPIED capacity-1 object stops winning (was %d, now bids lower)",
                 free_score);
        expect(o == 1 && tag == TN_SERVE_FUN, sp);
        expect(busy_score < free_score, "case 2: the occupied object's own bid actually fell");
    }
    // A capacity-2 object is still usable by a second person. Sharing costs, but does not block.
    tn_obj[1].users = 1;                                   // sofa capacity 2
    {
        int strength = 0;
        expect(tn_offers(1, TN_SERVE_FUN, &strength) && strength == 90,
               "case 2: tn_offers reports the sofa's strength without anyone naming a sofa");
        TnTag tag; int score;
        expect(tn_best_action(0, &tag, &score) == 1 && tag == TN_SERVE_FUN,
               "case 2: a half-full capacity-2 object is still a valid offer");
    }

    // ── CASE 3: A SATED NEED MAKES NO BID ───────────────────────────────────
    tn_obj_n = 0; tn_agent_n = 0;
    add_obj(TN_OBJ_TOILET, 1, 1, 0);
    add_agent(0, 1, 2);
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 255;
    expect(tn_best_action(0, NULL, NULL) == -1,
           "case 3: a fully sated agent has nothing to do, and -1 says so honestly");

    // ── CASE 4: CAPABILITIES AND STORAGE NEVER BID FOR ATTENTION ────────────
    // The loom offers TN_CAP_WORK and the wardrobe TN_STORE_CLOTHES. Neither is a need, so neither
    // may ever be chosen by tn_best_action, however desperate the agent is. They are reachable
    // ONLY through the tag-specific lookups. This is the boundary that keeps one index serving
    // three consumers without them bleeding into each other.
    tn_obj_n = 0; tn_agent_n = 0;
    add_obj(TN_OBJ_LOOM,     2, 2, -1);
    add_obj(TN_OBJ_WARDROBE, 3, 2,  0);
    add_agent(0, 1, 2);
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 0;   // as desperate as possible
    expect(tn_best_action(0, NULL, NULL) == -1,
           "case 4: a loom and a wardrobe never bid for attention, even at zero needs");
    expect(tn_find_workspot(0, TN_CAP_WORK) == 0,
           "case 4: but the loom IS findable as a workspot, by capability");
    expect(tn_best_offer(0, TN_STORE_CLOTHES, NULL) == 1,
           "case 4: and the wardrobe IS findable as storage, by tag");
    expect(tn_find_workspot(0, TN_CAP_POWER) == -1,
           "case 4: a capability nothing provides resolves to -1, not to something nearby");

    // ── CASE 5: THE CONTRACT'S DATA TABLES AGREE WITH THEMSELVES ────────────
    {
        int bad = 0, needs = 0, caps = 0;
        for (int k = 0; k < TN_OBJ_KIND_COUNT; k++) {
            if (TN_OFFER_N[k] == 0 || TN_OFFER_N[k] > TN_MAX_OFFERS) bad++;
            for (int i = 0; i < TN_OFFER_N[k]; i++) {
                const TnOffer *of = &TN_OFFERS[k][i];
                if (of->tag >= TN_TAG_COUNT) bad++;
                if (of->capacity == 0) bad++;             // a capacity-0 object is unusable
                if (of->tag < TN_SERVE_COUNT) { needs++; if (of->strength <= 0) bad++; }
                else caps++;
            }
        }
        snprintf(sp, sizeof sp, "every object declares at least one valid offer (%d need bids, %d capabilities)",
                 needs, caps);
        expect(bad == 0, sp);
        expect(needs > 0 && caps > 0, "case 5: the one tag namespace really is carrying both kinds");
    }

    // ── CASE 6: THE 8-HOUR SHIFT SURVIVES THE TYPE ──────────────────────────
    // The contract originally had `minutes` as unsigned char and 480 wrapped to 224. Pin it.
    expect(TN_OFFERS[TN_OBJ_BED][0].minutes == 480, "case 6: a night's sleep is 480 minutes, not 224");
    expect(TN_RECIPES[0].minutes == 480,             "case 6: an 8-hour shift is 480 minutes, not 224");
    expect(TN_RECIPES[0].in_n == 0,
           "case 6: v1's recipe is the MARKED open loop (time -> good), per design section 5");

    // ── CASE 7: THE SIM RUNS AND NEEDS ACTUALLY MOVE ────────────────────────
    tn_world_init();
    {
        const int before = tn_agent[0].need[TN_SERVE_BLADDER];
        for (int i = 0; i < 600; i++) tn_agents_tick();
        int any_used = 0;
        for (int i = 0; i < tn_agent_n; i++) if (tn_agent[i].activity != TN_ACT_IDLE) any_used = 1;
        snprintf(sp, sizeof sp, "case 7: after 600 minutes somebody is doing something (bladder %d -> %d)",
                 before, tn_agent[0].need[TN_SERVE_BLADDER]);
        expect(any_used, sp);
        expect(tn_clock.minute != 8 * 60 || tn_clock.day != 1, "case 7: the calendar advanced");
    }

    // ── CASE 8: OWNERSHIP IS NOT ENFORCED, AND THAT IS A FINDING ────────────
    // The slice found this by running: residents from household 1 walk across the building and eat
    // out of household 0's fridge, because nothing in the score knows about `household`. The design
    // (§6) says storage is owned and treats "whose fridge is it" as a FEATURE, the source of the
    // comedy the game is supposed to generate. So this is a real gap in the model, not in the code.
    //
    // It is asserted here deliberately, as CURRENT behaviour rather than as desired behaviour, so
    // that whoever wires ownership has to come here and consciously flip it. A gap that a test
    // describes is a work item; a gap in a comment is folklore.
    tn_obj_n = 0; tn_agent_n = 0;
    add_obj(TN_OBJ_FRIDGE, 1, 1, 0);          // belongs to household 0
    add_agent(1, 1, 2);                        // a resident of household 1, standing next to it
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 255;
    tn_agent[0].need[TN_SERVE_HUNGER] = 30;
    {
        TnTag tag; int score;
        const int o = tn_best_action(0, &tag, &score);
        expect(o == 0 && tag == TN_SERVE_HUNGER && tn_obj[0].household == 0 &&
               tn_agent[0].household == 1,
               "case 8 (KNOWN GAP): a household-1 resident helps itself to household-0's fridge, "
               "because ownership is not yet a term in the score. Flip this when §6 lands.");
    }

    tn_world_init();
}
#endif
