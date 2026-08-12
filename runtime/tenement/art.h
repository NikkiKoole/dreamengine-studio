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

typedef struct { float depth; int cell, rot; float vx, vy, vz; int fp0, fp1; int shadow; } Draw;
// + 2*TN_N because every tile can carry a north AND a west edge wall.
static Draw tnr_dl[TN_MAX_OBJECTS + TN_MAX_AGENTS + 2 * TN_N];
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
                             (float)tn_obj[o].ty*TN_TILE_VOX, 0.0f, fp[0], fp[1], 1 };
    }
    for (int i = 0; i < tn_agent_n; i++) {
        // The cell comes from the agent's POSE, which came from the offer it is using. No branch on
        // object kind anywhere: a bed does not tell the renderer it is a bed, it tells the sim that
        // using it means lying down (contract rule 2). Standing on a mattress was the symptom of
        // there being no notion of posture at all.
        const int pcell = (tn_agent[i].pose == TN_POSE_LIE) ? ISO_PERSON_LIE : ISO_PERSON;
        const short *fp = ISO_FOOTPRINT[pcell];
        // A resident who is not standing is ON the thing it is using, not on the floor beside it.
        // Position comes from the OBJECT and height from that object's own voxel depth (ISO_FOOTPRINT
        // carries nx,ny,NZ), so a taller bed lifts the sleeper without anyone hardcoding a number.
        // Lying beside the bed was the second half of the standing-on-the-bed bug: posture without
        // placement just moves the wrongness.
        float ax = (float)tn_agent[i].tx * TN_TILE_VOX, ay = (float)tn_agent[i].ty * TN_TILE_VOX;
        float az = 0.0f;
        int   arot = (tn_rot + tn_agent[i].facing) & 3;
        if (tn_agent[i].pose != TN_POSE_STAND && tn_agent[i].target_obj >= 0) {
            const TnObject *ob = &tn_obj[tn_agent[i].target_obj];
            const short *ofp = ISO_FOOTPRINT[OBJ_CELL[ob->kind]];
            // CENTRED on the furniture, not parked at its corner: a bed is 6x12 voxels and a lying
            // figure is 3x8, so using the object's origin puts the sleeper half off the mattress.
            // WHOLE voxels. `* 0.5f` gave +1.5 for a bed, and iso-rooms.md §7 measured exactly this:
            // a fractional offset puts corners between lattice points and the two rasterizers then
            // disagree everywhere (+0.25 was forty times worse than 0). A critic traced the remaining
            // stray pixels on occupied beds back to this line.
            ax = (float)(ob->tx * TN_TILE_VOX + (ofp[0] - fp[0]) / 2);
            ay = (float)(ob->ty * TN_TILE_VOX + (ofp[1] - fp[1]) / 2);
            az = (float)ofp[2];                       // stand ON its surface, whatever height it is
            arot = tn_rot;                            // align with the furniture, not with the walk
        }
        tnr_dl[tnr_dl_n++] = (Draw){ tnr_iso_depth(tn_rot, ax + fp[0]*0.5f, ay + fp[1]*0.5f) + 0.5f,
                             pcell, arot, ax, ay, az, fp[0], fp[1], az == 0.0f };
    }
    // ── EDGE WALLS ──────────────────────────────────────────────────────────
    // world.h stores walls on tile EDGES, not tiles, and only each tile's north and west edge (the
    // south edge IS the next tile's north). So iterating N and W per tile visits every wall exactly
    // once, with no double-draw to guard against.
    //
    // Orientation needs no `facing` field and cannot be got wrong: the DIRECTION is the orientation,
    // so a north/south edge takes the *_NS model and an east/west edge the *_EW one. That was the
    // world agent's reason for choosing edges, and it makes iso-rooms §8's turned-footprint bug
    // unrepresentable here rather than merely avoided.
    //
    // A wall model is 6 voxels long and 2 thick, so it straddles the boundary: offset by half its
    // thickness so it sits ON the edge rather than inside one of the two tiles.
    // NEAR WALLS ARE CUT, and the far shell is drawn. Both were missing and a critic measured the
    // cost: across 64 object-by-rotation samples, mean visibility 52%, twenty-one under 25%, and two
    // beds at exactly 0% — with 89% of all occluded furniture pixels covered by a wall colour. The
    // one shared toilet, which design §1's soul is literally about, showed ZERO of its 136 bright
    // pixels at three of the four rotations.
    //
    // This is not a new idea: iso-rooms.md §8 settled it ("FULL height with the near side cut away,
    // which beat Theme Hospital's low stubs plainly"). It simply was never implemented here, because
    // the first pass drew every edge unconditionally.
    //
    // Nearness is DERIVED from the projection, not hardcoded per rotation, so it survives a change to
    // the turn: step along the edge's outward normal and see whether depth rises. And the loops run
    // to <= so the far shell exists at all — tn_edge_at already reports the outside ring SOLID, so
    // this needs no new data.
    const float d0 = tnr_iso_depth(tn_rot, 0.0f, 0.0f);
    const int near_n = tnr_iso_depth(tn_rot, 0.0f, -1.0f) > d0;   // a north edge faces the camera
    const int near_w = tnr_iso_depth(tn_rot, -1.0f, 0.0f) > d0;   // a west edge faces the camera
    for (int ty = 0; ty <= tn_bh; ty++) {
        for (int tx = 0; tx <= tn_bw; tx++) {
            const float vx = tx * TN_TILE_VOX, vy = ty * TN_TILE_VOX;
            if (tx < tn_bw) {
                const int en = tn_edge_at(tx, ty, TN_DIR_N);
                if (en != TN_WALL_NONE) {
                    // A door already draws low. A NEAR wall draws low for the same reason: you have to
                    // see past it. Costs no atlas and no new art, because both models exist.
                    const int cell = (en == TN_WALL_DOOR || near_n) ? ISO_WALL_LOW_NS : ISO_WALL_FULL_NS;
                    const short *fp = ISO_FOOTPRINT[cell];
                    tnr_dl[tnr_dl_n++] = (Draw){ tnr_iso_depth(tn_rot, vx + fp[0] * 0.5f, vy),
                                                 cell, tn_rot, vx, vy - 1.0f, 0.0f, fp[0], fp[1], 0 };
                }
            }
            if (ty < tn_bh) {
                const int ew = tn_edge_at(tx, ty, TN_DIR_W);
                if (ew != TN_WALL_NONE) {
                    const int cell = (ew == TN_WALL_DOOR || near_w) ? ISO_WALL_LOW_EW : ISO_WALL_FULL_EW;
                    const short *fp = ISO_FOOTPRINT[cell];
                    tnr_dl[tnr_dl_n++] = (Draw){ tnr_iso_depth(tn_rot, vx, vy + fp[1] * 0.5f),
                                                 cell, tn_rot, vx - 1.0f, vy, 0.0f, fp[0], fp[1], 0 };
                }
            }
        }
    }

    qsort(tnr_dl, tnr_dl_n, sizeof tnr_dl[0], tnr_cmp_draw);
    for (int i = 0; i < tnr_dl_n; i++) {
        const IsoCell *c = &ISO_CELLS[tnr_dl[i].cell][tnr_dl[i].rot];
        float sx, sy; tnr_iso_project(tn_rot, tnr_dl[i].vx, tnr_dl[i].vy, tnr_dl[i].vz, &sx, &sy);
        // A contact shadow, but only for things that stand on a TILE. A wall stands on an EDGE, so
        // its footprint quad would smear a dark stripe along the boundary between two rooms.
        if (tnr_dl[i].shadow) {
            // one-voxel-INSET: an integer inset, because a fractional pad puts the quad between
            // lattice points and strays pixels everywhere (iso-rooms.md §7's measured table)
            float q[4][2]; const float pad = 1.0f;
            const float x0 = tnr_dl[i].vx+pad, y0 = tnr_dl[i].vy+pad;
            const float x1 = tnr_dl[i].vx+tnr_dl[i].fp0-pad, y1 = tnr_dl[i].vy+tnr_dl[i].fp1-pad;
            tnr_iso_project(tn_rot, x0,y0,0,&q[0][0],&q[0][1]); tnr_iso_project(tn_rot, x1,y0,0,&q[1][0],&q[1][1]);
            tnr_iso_project(tn_rot, x1,y1,0,&q[2][0],&q[2][1]); tnr_iso_project(tn_rot, x0,y1,0,&q[3][0],&q[3][1]);
            quadfill((int)(q[0][0]+cam_x),(int)(q[0][1]+cam_y),(int)(q[1][0]+cam_x),(int)(q[1][1]+cam_y),
                     (int)(q[2][0]+cam_x),(int)(q[2][1]+cam_y),(int)(q[3][0]+cam_x),(int)(q[3][1]+cam_y),
                     CLR_BROWNISH_BLACK);
        }
        sspr(c->x, c->y, c->w, c->h, (int)(sx+cam_x)-c->ox, (int)(sy+cam_y)-c->oy, c->w, c->h);
    }
}

#endif // TENEMENT_ART_H
