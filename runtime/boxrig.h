#ifndef BOXRIG_H
#define BOXRIG_H
// boxrig.h — the shared "sprite region -> <=8-vert Box2D polygon + texture" toolkit.
//
// The puppetmaker representation (a body part = one convex poly of <=8 (x,y) points +
// a texture) factored out of puppet.c / boxlab.c / boxjelly.c so they stop drifting. Jobs:
//   1. boxrig_hull()          — sprite alpha -> a <=8-point b2Hull (Box2D's B2_MAX_POLYGON_VERTICES)
//   2. boxrig_draw()          — texture a b2Polygon with that sprite via tritex, drawn straight
//                               from the polygon's OWN vertices so the paint EXACTLY covers the hull.
//   3. boxrig_point_in_body() — is a world point inside a part's polygon? (click-to-grab)
//   4. boxrig_resolve_box/poly() — verlet(physics.h) <-> Box2D coupling: push a verlet point out
//                               of a rigid shape + return the contact (only if physics.h is included).
// The CART still owns body/shape/joint policy (density, friction, static vs dynamic, what joins
// to what) and the reaction-impulse policy — this header owns the pixels<->geometry math + the skin.
//
// Requires (include FIRST, in the cart): "studio.h" then "box2d/box2d.h".
// Box2D is METRES + y-UP; studio.h is PIXELS + y-DOWN — every fn takes `ppm` (pixels per
// metre) and `screenH` so it needs no global from the cart. See docs/design/box2d-integration.md.
//
// Typical use:
//   float pcx, pcy;
//   b2Hull h = boxrig_hull(ox,oy,w,h, PPM, &pcx,&pcy);   // sprite rect -> hull
//   b2Polygon poly = b2MakePolygon(&h, 0.0f);            // cart makes the shape + body
//   ... b2CreatePolygonShape(body, &sd, &poly) ...
//   boxrig_draw(body, poly, pcx, pcy, PPM, SCREEN_H);    // in draw()

#include <stdlib.h>   // qsort, labs
#include <math.h>

typedef struct { int x, y; } BrPt;

static int br_cmp(const void *a, const void *b) {
    const BrPt *p = a, *q = b;
    return p->x != q->x ? p->x - q->x : p->y - q->y;
}
static long br_cross(BrPt o, BrPt a, BrPt b) {
    return (long)(a.x - o.x) * (b.y - o.y) - (long)(a.y - o.y) * (b.x - o.x);
}
// Andrew's monotone chain — convex hull of the pixel cloud (CCW), no repeated end point.
static int br_chain(BrPt *pts, int n, BrPt *out) {
    if (n < 3) { for (int i = 0; i < n; i++) out[i] = pts[i]; return n; }
    qsort(pts, n, sizeof(BrPt), br_cmp);
    int k = 0;
    for (int i = 0; i < n; i++) { while (k >= 2 && br_cross(out[k-2], out[k-1], pts[i]) <= 0) k--; out[k++] = pts[i]; }
    int lo = k + 1;
    for (int i = n - 2; i >= 0; i--) { while (k >= lo && br_cross(out[k-2], out[k-1], pts[i]) <= 0) k--; out[k++] = pts[i]; }
    return k - 1;
}
// Drop the least-area vertex until <=8 remain (Box2D's polygon budget).
static int br_reduce8(BrPt *h, int n) {
    while (n > 8) {
        long best = -1; int bi = 0;
        for (int i = 0; i < n; i++) {
            long a = labs(br_cross(h[(i+n-1)%n], h[i], h[(i+1)%n]));
            if (best < 0 || a < best) { best = a; bi = i; }
        }
        for (int i = bi; i < n - 1; i++) h[i] = h[i + 1];
        n--;
    }
    return n;
}

// Sprite rect (ox,oy,w,h) on the sheet -> a <=8-point convex hull in LOCAL metres
// (centred on the opaque-pixel centroid, y-up). Writes that centroid (sheet px) to
// *pcx,*pcy — it's the local/UV origin boxrig_draw() needs. Index 0 == transparent.
static b2Hull boxrig_hull(int ox, int oy, int w, int h, float ppm, float *pcx, float *pcy) {
    static BrPt cloud[8192], hull[8192];
    int nc = 0; double sx = 0, sy = 0;
    for (int y = oy; y < oy + h; y++)
        for (int x = ox; x < ox + w; x++)
            if (sget(x, y) != 0 && nc < 8192) { cloud[nc] = (BrPt){x, y}; sx += x; sy += y; nc++; }
    float cx = nc ? (float)(sx / nc) : ox, cy = nc ? (float)(sy / nc) : oy;
    *pcx = cx; *pcy = cy;

    int nh = br_reduce8(hull, br_chain(cloud, nc, hull));
    b2Vec2 local[8];
    for (int i = 0; i < nh; i++)
        local[i] = (b2Vec2){ (hull[i].x - cx) / ppm, -(hull[i].y - cy) / ppm };
    return b2ComputeHull(local, nh);
}

// Texture a body's collision polygon with its sprite: a triangle fan from the centroid
// (UV = each metric vertex mapped back to its sheet pixel), so drawn == collides.
static void boxrig_draw(b2BodyId body, b2Polygon poly, float pcx, float pcy, float ppm, int screenH) {
    b2Vec2 p = b2Body_GetPosition(body);
    b2Rot  r = b2Body_GetRotation(body);
    int n = poly.count;
    int cxs = (int)(p.x * ppm), cys = (int)(screenH - p.y * ppm);
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        b2Vec2 a = poly.vertices[i], b = poly.vertices[j];
        float axw = p.x + (r.c*a.x - r.s*a.y), ayw = p.y + (r.s*a.x + r.c*a.y);
        float bxw = p.x + (r.c*b.x - r.s*b.y), byw = p.y + (r.s*b.x + r.c*b.y);
        tritex(cxs, cys, pcx, pcy,
               (int)(axw*ppm), (int)(screenH - ayw*ppm), pcx + a.x*ppm, pcy - a.y*ppm,
               (int)(bxw*ppm), (int)(screenH - byw*ppm), pcx + b.x*ppm, pcy - b.y*ppm);
    }
}

// Is a world point (METRES) inside a body's convex polygon? For click-to-grab: loop your
// parts and grab the first that contains the cursor (much kinder than nearest-centre).
static bool boxrig_point_in_body(b2BodyId body, const b2Polygon *poly, float mxm, float mym) {
    b2Vec2 c = b2Body_GetPosition(body); b2Rot r = b2Body_GetRotation(body);
    float dx = mxm-c.x, dy = mym-c.y, lx = dx*r.c+dy*r.s, ly = -dx*r.s+dy*r.c;   // into body-local
    for (int k = 0; k < poly->count; k++)
        if ((lx-poly->vertices[k].x)*poly->normals[k].x + (ly-poly->vertices[k].y)*poly->normals[k].y > 0.0f) return false;
    return true;
}

// ── verlet <-> Box2D coupling (needs physics.h — include it BEFORE boxrig.h) ───
// Two independent solvers (runtime/physics.h verlet points in PIXELS, Box2D in METRES) can
// share a world: resolve each verlet point against Box2D shapes. These do the HARD push-out
// (point can't sink into the rigid body) + tangential friction on the point, and RETURN the
// contact so the CALLER decides the reaction impulse policy (accumulate+cap vs immediate).
#ifdef PHYSICS_H
typedef struct { bool hit; float nx, ny, pen, cx, cy; } BrHit;   // normal + penetration + contact, all METRES

// shared tail: push point out along world normal n (metres), damp inward + tangential velocity (px).
static BrHit br__resolve(PhysPt *P, float ppm, int screenH, float nx, float ny, float pen, float pmx, float pmy) {
    P->x = pmx*ppm; P->y = screenH - pmy*ppm;
    float npx = nx, npy = -ny;                         // px-space normal (screen y is flipped)
    float vx = P->x-P->px, vy = P->y-P->py, vn = vx*npx + vy*npy;
    if (vn < 0) { P->px += npx*vn; P->py += npy*vn; }
    float tpx = -npy, tpy = npx, vt = vx*tpx + vy*tpy; P->px += tpx*vt*0.3f; P->py += tpy*vt*0.3f;
    return (BrHit){ true, nx, ny, pen, pmx, pmy };
}
// verlet point vs an oriented Box2D box (half-extents metres).
static BrHit boxrig_resolve_box(PhysPt *P, b2BodyId body, float hw, float hh, float ppm, int screenH) {
    b2Vec2 c = b2Body_GetPosition(body); b2Rot r = b2Body_GetRotation(body);
    float rm = P->r/ppm, Pmx = P->x/ppm, Pmy = (screenH-P->y)/ppm;
    float dx = Pmx-c.x, dy = Pmy-c.y, lx = dx*r.c+dy*r.s, ly = -dx*r.s+dy*r.c;
    float ex = (hw+rm)-fabsf(lx), ey = (hh+rm)-fabsf(ly);
    if (ex<=0 || ey<=0) return (BrHit){0};
    float nlx, nly, pen;
    if (ex < ey){ nlx = lx<0?-1:1; nly=0; pen=ex; } else { nlx=0; nly=ly<0?-1:1; pen=ey; }
    float nx = r.c*nlx - r.s*nly, ny = r.s*nlx + r.c*nly;
    return br__resolve(P, ppm, screenH, nx, ny, pen, Pmx+nx*pen, Pmy+ny*pen);
}
// verlet point vs a body's convex polygon (uses the polygon's own normals).
static BrHit boxrig_resolve_poly(PhysPt *P, b2BodyId body, const b2Polygon *poly, float ppm, int screenH) {
    b2Vec2 c = b2Body_GetPosition(body); b2Rot r = b2Body_GetRotation(body);
    float rm = P->r/ppm, Pmx = P->x/ppm, Pmy = (screenH-P->y)/ppm;
    float dx = Pmx-c.x, dy = Pmy-c.y, lx = dx*r.c+dy*r.s, ly = -dx*r.s+dy*r.c;
    float maxSD = -1e9f; int edge = -1;
    for (int i = 0; i < poly->count; i++) {
        float sd = (lx-poly->vertices[i].x)*poly->normals[i].x + (ly-poly->vertices[i].y)*poly->normals[i].y;
        if (sd > maxSD) { maxSD = sd; edge = i; }
    }
    if (edge < 0 || maxSD >= rm) return (BrHit){0};
    float pen = rm-maxSD, nlx = poly->normals[edge].x, nly = poly->normals[edge].y;
    float nx = r.c*nlx - r.s*nly, ny = r.s*nlx + r.c*nly;
    return br__resolve(P, ppm, screenH, nx, ny, pen, Pmx+nx*pen, Pmy+ny*pen);
}
#endif // PHYSICS_H

#endif // BOXRIG_H
