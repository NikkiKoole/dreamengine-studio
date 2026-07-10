// roadkit.h — the shared AT-GRADE JUNCTION GEOMETRY, cart-land (ADR-0006).
//
// Extracted from streetlab (the spec-locked SOURCE) at worldgen Track-B B2 — the
// roadkit trigger discipline: a second consumer (citydrive's real-OSM render, the
// generated city's junctions) needs the grammar as a callable library, so the PURE
// geometry primitives live here and everyone solves a junction through ONE impl:
//   · curb_return()          — the tangent-arc curb fillet (streetlab's M1 headline)
//   · edge_corner()          — the corner K where two arms' gap-facing band edges cross
//   · rk_count_corners(brg,n)— convex-corner count from a bearings array
//   · rk_cross_hw(...)       — the typed cross-section half-width (Σ lanes present)
//
// A junction is N ARMS AT BEARINGS — no 90°/collinear assumption. The CALLER
// supplies the bearing array; streetlab's leg model, OSM (osm-junction), and the
// generated city (citygen) all produce one. These fns take pure geometry params,
// read no cart globals — so streetlab, roadlab, citydrive and citygen share them.
//
// CONTRACT: include AFTER studio.h (uses cos_deg/sin_deg). Bearings in DEGREES,
// world units = pixels here (the ground-plane datum; a consumer scales as it likes).
//
// ── ux/uy SNAP (streetlab's, kept verbatim so its spec stays 104/0) ──────────
// At cardinal angles cos/sin return ~1e-7, not 0, and over a long arm that
// sub-pixel error tips an (int) truncation by 1px (the two halves of a through
// road mismatch by 1px at the hub). Snap near-zero to exactly 0. NB roadlab's own
// ux/uy do NOT snap — when it adopts roadkit (B4) its spec guards the sub-pixel shift.
#ifndef ROADKIT_H
#define ROADKIT_H
#include "studio.h"
#include <math.h>

static float rk_ux(float d){ float c=cos_deg(d); return (c>-1e-4f&&c<1e-4f)?0.f:c; }
static float rk_uy(float d){ float s=sin_deg(d); return (s>-1e-4f&&s<1e-4f)?0.f:s; }
static int   rk_ri(float v){ return (int)roundf(v); }   // round-to-nearest, symmetric about 0 (mirror-safe)

// the curb-return fillet: an arc tangent to both curb lines meeting at the sharp
// corner K. Closed form — tangent points at R/tan(half) along each edge, centre at
// R/sin(half) along the bisector (which points toward the block corner). PURE; the
// geometry streetlab's spec pins (tangency: dist(O, each edge line) == R).
typedef struct { float ox, oy, t1x, t1y, t2x, t2y; } CurbReturn;
static CurbReturn curb_return(float kx, float ky, float e1, float e2, float R){
    float d1x=rk_ux(e1), d1y=rk_uy(e1), d2x=rk_ux(e2), d2y=rk_uy(e2);
    float bx=d1x+d2x, by=d1y+d2y;                          // bisector (toward the block corner)
    float bl=sqrtf(bx*bx+by*by); if(bl<1e-4f){ bx=-d1y; by=d1x; bl=1; } bx/=bl; by/=bl;
    float coshalf = bx*d1x + by*d1y;                       // cos(half-angle) = bisector · edge
    if (coshalf>1) coshalf=1; if (coshalf<-1) coshalf=-1;
    float half = acosf(coshalf);
    float sinhalf = sinf(half); if (sinhalf<1e-4f) sinhalf=1e-4f;
    float tanhalf = sinhalf/coshalf; if (tanhalf<1e-4f) tanhalf=1e-4f;
    float tdist = R / tanhalf;                             // K → tangent point, along each edge
    float cdist = R / sinhalf;                             // K → centre, along the bisector
    CurbReturn c;
    c.ox  = kx + bx*cdist;   c.oy  = ky + by*cdist;
    c.t1x = kx + d1x*tdist;  c.t1y = ky + d1y*tdist;
    c.t2x = kx + d2x*tdist;  c.t2y = ky + d2y*tdist;
    return c;
}

// corner K = where the two band edges FACING the gap between arms A,B cross (each
// offset HW toward the gap-side bisector bm). PURE.
static void edge_corner(float cx,float cy,float HW,float bA,float bB,float bm,float*kx,float*ky){
    float dAx=rk_ux(bA),dAy=rk_uy(bA), nAx=rk_ux(bA+90),nAy=rk_uy(bA+90);
    float sA = (nAx*rk_ux(bm)+nAy*rk_uy(bm))>0 ? 1.f : -1.f;   // pick the edge on the gap (grass) side
    float PAx=cx+sA*HW*nAx, PAy=cy+sA*HW*nAy;
    float dBx=rk_ux(bB),dBy=rk_uy(bB), nBx=rk_ux(bB+90),nBy=rk_uy(bB+90);
    float sB = (nBx*rk_ux(bm)+nBy*rk_uy(bm))>0 ? 1.f : -1.f;
    float PBx=cx+sB*HW*nBx, PBy=cy+sB*HW*nBy;
    float det = dAx*(-dBy) - (-dBx)*dAy;                   // solve PA + t·DA = PB + u·DB
    if (fabsf(det)<1e-4f){ *kx=(PAx+PBx)*0.5f; *ky=(PAy+PBy)*0.5f; return; }
    float t = ((PBx-PAx)*(-dBy) - (-dBx)*(PBy-PAy)) / det;
    *kx=PAx+dAx*t; *ky=PAy+dAy*t;
}

// number of CONVEX corners = adjacent gaps strictly between 0 and 180 (a 180° gap
// is a straight through-arm, no corner). brg[] sorted 0..360, n arms. PURE.
static int rk_count_corners(const float *brg, int n){
    if (n<2) return 0;
    int c=0;
    for (int i=0;i<n;i++){ float g=fmodf(brg[(i+1)%n]-brg[i]+3600,360); if (g>0.5f && g<179.5f) c++; }
    return c;
}

// the typed cross-section half-width: [centre median/TWLTL] + driving×lanesPer +
// [bike] + [parking], centre→kerb. Every junction primitive keys off this datum,
// so widening the section re-solves the whole junction for free.
static float rk_cross_hw(float centreW, int lanesPer, float laneW, float bikeW, float parkW){
    return centreW + lanesPer*laneW + bikeW + parkW;
}

// ── the JUNCTION FIELD (B3) — the N-arm-native road surface as ONE float predicate ──
// A junction's asphalt = the union of arm capsules (a ray HW-wide out each arm from the hub) ∪ the
// curb-return fillets between adjacent arms ∪ an optional circulatory disc (roundabout). Snap-free
// floats → symmetric at any angle, N arms with no 90°/collinear assumption. This is streetlab's
// DE_FIELD_ROADS renderer, promoted to the shared home: SPACE-AGNOSTIC (px/py in whatever units cx/cy/
// HW are), so streetlab scans it in SCREEN pixels and citydrive can evaluate it in GROUND METRES and
// project. The caller owns the scan + how it paints; roadkit owns the geometry predicate.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define RK_MAXARM 8            // arms per junction (≥ streetlab NLEG 6, ≥ citydrive MAXARM 8)
#define RK_FR_NP  12           // fillet polygon verts (apex K + 11 arc samples)
typedef struct {
    float cx, cy, HW2, rad2, disc2;                 // hub, carriageway HW², fillet-cutoff², disc² (0=none)
    int   n; float dx[RK_MAXARM], dy[RK_MAXARM];     // arm unit directions
    int   nfil; float fil[RK_MAXARM][2*RK_FR_NP];    // cached curb-return fillet polygons (float, snap-free)
} RkField;

static int rk_pip(const float *xy, int n, float px, float py){   // even-odd point-in-polygon (float)
    int c=0; for (int i=0,j=n-1;i<n;j=i++){
        float yi=xy[2*i+1], yj=xy[2*j+1];
        if ((yi>py)!=(yj>py)){
            float xc=xy[2*i]+(xy[2*j]-xy[2*i])*(py-yi)/(yj-yi);
            if (px<xc) c^=1;
        }
    } return c;
}
// the arm-capsule union: rays from the hub (the opposite arm covers behind). Squared distance, no sqrt.
static int rk_field_arm(const RkField*f, float px, float py){
    for (int i=0;i<f->n;i++){
        float t=(px-f->cx)*f->dx[i]+(py-f->cy)*f->dy[i]; if(t<0)t=0;
        float ex=px-(f->cx+f->dx[i]*t), ey=py-(f->cy+f->dy[i]*t);
        if (ex*ex+ey*ey <= f->HW2) return 1;
    }
    return 0;
}
static int rk_field_fillet(const RkField*f, float px, float py){   // inside a curb-return fillet? (near the hub)
    float hx=px-f->cx, hy=py-f->cy;
    if (hx*hx+hy*hy > f->rad2) return 0;
    for (int k=0;k<f->nfil;k++) if (rk_pip(f->fil[k], RK_FR_NP, px, py)) return 1;
    return 0;
}
// asphalt = arm-ray union ∪ curb-return fillets ∪ (roundabout) circulatory disc.
static int rk_field_road(const RkField*f, float px, float py){
    if (f->disc2>0){ float hx=px-f->cx, hy=py-f->cy; if (hx*hx+hy*hy <= f->disc2) return 1; }
    return rk_field_arm(f,px,py) || rk_field_fillet(f,px,py);
}
// build the field: arms at bearings, carriageway half-width HW, per-corner radius cornerR[i] (the fillet
// for the gap between arm i and i+1; a straight-through/reflex gap is skipped regardless), optional
// circulatory disc (>0 ⇒ roundabout, no corner fillets). curb_return/edge_corner do the fillet geometry.
static void rk_field_build(RkField*f, float cx, float cy, float HW, const float*brg, int n,
                           const float*cornerR, float disc){
    f->cx=cx; f->cy=cy; f->HW2=HW*HW; f->n=n; f->disc2=disc*disc; f->nfil=0;
    for (int i=0;i<n;i++){ f->dx[i]=rk_ux(brg[i]); f->dy[i]=rk_uy(brg[i]); }
    float maxR=0;
    for (int i=0; i<n && disc<=0; i++){                       // roundabout disc replaces the corner fillets
        float bA=brg[i], bB=brg[(i+1)%n], gap=fmodf(bB-bA+3600,360);
        if (gap<=0.5f || gap>=179.5f) continue;
        float R=cornerR[i]; if (R<=0) continue;
        if (R>maxR) maxR=R;
        float bm=bA+gap*0.5f, kx,ky; edge_corner(cx,cy,HW, bA,bB,bm, &kx,&ky);
        CurbReturn c=curb_return(kx,ky, bA,bB, R);
        float a0=atan2f(c.t1y-c.oy,c.t1x-c.ox), a1=atan2f(c.t2y-c.oy,c.t2x-c.ox);
        float d=a1-a0; while(d>M_PI)d-=2*M_PI; while(d<-M_PI)d+=2*M_PI;
        float *xy=f->fil[f->nfil]; int k=0;
        xy[k++]=kx; xy[k++]=ky;
        for (int s=0;s<=10;s++){ float a=a0+d*s/10; xy[k++]=c.ox+cosf(a)*R; xy[k++]=c.oy+sinf(a)*R; }
        f->nfil++;
    }
    // fillet-cutoff radius: acute skew corners reach far past a fixed radius — take the furthest vertex.
    f->rad2 = (HW+maxR)*(HW+maxR);
    for (int k=0;k<f->nfil;k++) for (int v=0;v<RK_FR_NP;v++){
        float vx=f->fil[k][2*v]-cx, vy=f->fil[k][2*v+1]-cy, d2=vx*vx+vy*vy;
        if (d2>f->rad2) f->rad2=d2;
    }
    f->rad2 += 4;                                            // small margin
}

#endif // ROADKIT_H
