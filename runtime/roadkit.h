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

#endif // ROADKIT_H
