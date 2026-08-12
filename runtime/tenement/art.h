// ─────────────────────────────────────────────────────────────────────────────
// tenement/art.h — the isometric view: projection, camera, painter's sort, contact shadows.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tnr_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_ART_H
#define TENEMENT_ART_H

#include "tenement/atlas.h"   // the generated cell table. ONLY the art module includes this.

// which sprite each object kind draws with. ART ONLY — never branched on for behaviour.
static const int OBJ_CELL[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED] = ISO_BED, [TN_OBJ_FRIDGE] = ISO_FRIDGE, [TN_OBJ_COUNTER] = ISO_COUNTER,
    [TN_OBJ_TOILET] = ISO_TOILET, [TN_OBJ_SOFA] = ISO_SOFA, [TN_OBJ_LOOM] = ISO_LOOM,
    [TN_OBJ_WARDROBE] = ISO_WARDROBE,
};
static float cam_x, cam_y;   // owner: art. The HUD does not need these.
static void tnr_iso_turn(int q, float x, float y, float *X, float *Y) {
    switch (q & 3) { case 0: *X= x; *Y= y; break; case 1: *X=-y; *Y= x; break;
                     case 2: *X=-x; *Y=-y; break; default: *X= y; *Y=-x; break; }
}
static void tnr_iso_project(int r, float vx, float vy, float vz, float *sx, float *sy) {
    float X, Y; tnr_iso_turn(r, vx, vy, &X, &Y);
    *sx = (X - Y) * (ISO_TW * 0.5f);
    *sy = (X + Y) * (ISO_TH * 0.5f) - vz * ISO_ZH;
}
static float tnr_iso_depth(int r, float vx, float vy) {
    float X, Y; tnr_iso_turn(r, vx, vy, &X, &Y); return X + Y;
}
void tn_camera(void) {
    float minx=1e9f, maxx=-1e9f, miny=1e9f, maxy=-1e9f;
    const float W = tn_bw * TN_TILE_VOX, H = tn_bh * TN_TILE_VOX;
    for (int c = 0; c < 8; c++) {
        float sx, sy;
        tnr_iso_project(tn_rot, (c&1)?W:0, (c&2)?H:0, (c&4)?12.0f:0, &sx, &sy);
        if (sx<minx) minx=sx; if (sx>maxx) maxx=sx; if (sy<miny) miny=sy; if (sy>maxy) maxy=sy;
    }
    // Floored: a half-pixel camera rounds adjacent sprites opposite ways and opens 1px seams
    // (iso-rooms.md §7). Learned there, not rediscovered here.
    cam_x = floorf((SCREEN_W - (maxx - minx)) * 0.5f - minx);
    cam_y = floorf((SCREEN_H - 26 - (maxy - miny)) * 0.5f - miny + 12);
}

typedef struct { float depth; int cell, rot; float vx, vy; int fp0, fp1; } Draw;
static Draw tnr_dl[TN_MAX_OBJECTS + TN_MAX_AGENTS];
static int tnr_dl_n;
static int tnr_cmp_draw(const void *a, const void *b) {
    const float d = ((const Draw*)a)->depth - ((const Draw*)b)->depth;
    return d < 0 ? -1 : (d > 0 ? 1 : 0);
}

void tn_draw_world(void) {
    for (int ty = 0; ty < tn_bh; ty++) for (int tx = 0; tx < tn_bw; tx++) {
        float c[4][2]; const float vx = tx*TN_TILE_VOX, vy = ty*TN_TILE_VOX;
        tnr_iso_project(tn_rot, vx,             vy,             0, &c[0][0], &c[0][1]);
        tnr_iso_project(tn_rot, vx+TN_TILE_VOX, vy,             0, &c[1][0], &c[1][1]);
        tnr_iso_project(tn_rot, vx+TN_TILE_VOX, vy+TN_TILE_VOX, 0, &c[2][0], &c[2][1]);
        tnr_iso_project(tn_rot, vx,             vy+TN_TILE_VOX, 0, &c[3][0], &c[3][1]);
        quadfill((int)(c[0][0]+cam_x),(int)(c[0][1]+cam_y), (int)(c[1][0]+cam_x),(int)(c[1][1]+cam_y),
                 (int)(c[2][0]+cam_x),(int)(c[2][1]+cam_y), (int)(c[3][0]+cam_x),(int)(c[3][1]+cam_y),
                 ((tx+ty)&1) ? CLR_BROWN : CLR_DARK_BROWN);
    }
    tnr_dl_n = 0;
    for (int o = 0; o < tn_obj_n; o++) {
        const int cell = OBJ_CELL[tn_obj[o].kind];
        const short *fp = ISO_FOOTPRINT[cell];
        tnr_dl[tnr_dl_n++] = (Draw){ tnr_iso_depth(tn_rot, tn_obj[o].tx*TN_TILE_VOX + fp[0]*0.5f,
                                               tn_obj[o].ty*TN_TILE_VOX + fp[1]*0.5f),
                             cell, tn_rot, (float)tn_obj[o].tx*TN_TILE_VOX,
                             (float)tn_obj[o].ty*TN_TILE_VOX, fp[0], fp[1] };
    }
    for (int i = 0; i < tn_agent_n; i++) {
        const short *fp = ISO_FOOTPRINT[ISO_PERSON];
        tnr_dl[tnr_dl_n++] = (Draw){ tnr_iso_depth(tn_rot, tn_agent[i].tx*TN_TILE_VOX + fp[0]*0.5f,
                                               tn_agent[i].ty*TN_TILE_VOX + fp[1]*0.5f),
                             ISO_PERSON, (tn_rot + tn_agent[i].facing) & 3,
                             (float)tn_agent[i].tx*TN_TILE_VOX,
                             (float)tn_agent[i].ty*TN_TILE_VOX, fp[0], fp[1] };
    }
    qsort(tnr_dl, tnr_dl_n, sizeof tnr_dl[0], tnr_cmp_draw);
    for (int i = 0; i < tnr_dl_n; i++) {
        const IsoCell *c = &ISO_CELLS[tnr_dl[i].cell][tnr_dl[i].rot];
        float sx, sy; tnr_iso_project(tn_rot, tnr_dl[i].vx, tnr_dl[i].vy, 0, &sx, &sy);
        // one-voxel-INSET contact shadow: an integer inset, because a fractional pad puts the quad
        // between lattice points and strays everywhere (iso-rooms.md §7's measured table)
        float q[4][2]; const float pad = 1.0f;
        const float x0 = tnr_dl[i].vx+pad, y0 = tnr_dl[i].vy+pad;
        const float x1 = tnr_dl[i].vx+tnr_dl[i].fp0-pad, y1 = tnr_dl[i].vy+tnr_dl[i].fp1-pad;
        tnr_iso_project(tn_rot, x0,y0,0,&q[0][0],&q[0][1]); tnr_iso_project(tn_rot, x1,y0,0,&q[1][0],&q[1][1]);
        tnr_iso_project(tn_rot, x1,y1,0,&q[2][0],&q[2][1]); tnr_iso_project(tn_rot, x0,y1,0,&q[3][0],&q[3][1]);
        quadfill((int)(q[0][0]+cam_x),(int)(q[0][1]+cam_y),(int)(q[1][0]+cam_x),(int)(q[1][1]+cam_y),
                 (int)(q[2][0]+cam_x),(int)(q[2][1]+cam_y),(int)(q[3][0]+cam_x),(int)(q[3][1]+cam_y),
                 CLR_BROWNISH_BLACK);
        sspr(c->x, c->y, c->w, c->h, (int)(sx+cam_x)-c->ox, (int)(sy+cam_y)-c->oy, c->w, c->h);
    }
}

#endif // TENEMENT_ART_H
