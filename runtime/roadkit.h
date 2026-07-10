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

// ── GRADE-SEPARATED INTERCHANGES (B4) — roadlab's ramp grammar, extracted so the generated world
// (citygen's grade-2 junctions) and roadlab share ONE implementation. The topology (make_junction:
// which movement uses which ramp primitive, per interchange TYPE) + the ramp SPLINES (arc / clothoid /
// loop / flyover, port→port centreline). PURE over Ports (x,y,dir) + a present[] mask — no cart globals.
// NB the ramps use the NON-snapping rk_ux_raw/rk_uy_raw (roadlab's convention — its spec pins this
// geometry; the at-grade field above uses the SNAPPING rk_ux/rk_uy, streetlab's convention).
#ifndef RK_R2D
#define RK_R2D 57.29578f
#endif
#define RK_DRIVE 1                       // +1 = drive on the right (roadlab's handedness)
static float rk_ux_raw(float d){ return cos_deg(d); }
static float rk_uy_raw(float d){ return sin_deg(d); }
static float rk_tan(float d){ return sin_deg(d)/cos_deg(d); }
typedef struct { float x,y,dir; const char*name; } Port;   // a lane endpoint = a point + its travel direction

// ARC-SPLINE ramp: LINE → ARC → LINE (round the corner where A's heading-line meets B's with radius R).
static int arc_spline(Port a, Port b, float R, float *xs, float *ys){
    float ad=a.dir;
    float uax=rk_ux_raw(ad),uay=rk_uy_raw(ad), ubx=rk_ux_raw(b.dir),uby=rk_uy_raw(b.dir);
    float den=uax*uby-uay*ubx;
    float dA=b.dir-ad; while(dA>180)dA-=360; while(dA<-180)dA+=360;
    float fa=dA<0?-dA:dA;
    int n=0;
    if (fa<1.f || (den<0.02f&&den>-0.02f)){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
    float s=((b.x-a.x)*uby-(b.y-a.y)*ubx)/den;
    float Px=a.x+uax*s, Py=a.y+uay*s;
    float distB=(b.x-Px)*ubx+(b.y-Py)*uby;
    float avail=(s<distB?s:distB);
    if (avail<2.f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
    float tanH=sin_deg(fa*0.5f)/cos_deg(fa*0.5f);
    if (R>avail*0.95f/tanH) R=avail*0.95f/tanH;
    float T=R*tanH;
    float t1x=Px-uax*T,t1y=Py-uay*T, t2x=Px+ubx*T,t2y=Py+uby*T;
    float nx=-uay,ny=uax;
    float caX=t1x+nx*R,caY=t1y+ny*R, cbX=t1x-nx*R,cbY=t1y-ny*R, cx,cy;
    if (distance((int)caX,(int)caY,(int)t2x,(int)t2y) < distance((int)cbX,(int)cbY,(int)t2x,(int)t2y)){cx=caX;cy=caY;}
    else {cx=cbX;cy=cbY;}
    xs[n]=a.x; ys[n++]=a.y;
    float a0=angle_to((int)cx,(int)cy,(int)t1x,(int)t1y);
    float a1=angle_to((int)cx,(int)cy,(int)t2x,(int)t2y);
    float sw=a1-a0; while(sw>180)sw-=360; while(sw<-180)sw+=360;
    for (int i=0;i<=16;i++){ float ang=a0+sw*(float)i/16; xs[n]=cx+rk_ux_raw(ang)*R; ys[n++]=cy+rk_uy_raw(ang)*R; }
    xs[n]=b.x; ys[n++]=b.y;
    return n;
}
// CLOTHOID-JOINTED ramp: LINE → CLOTHOID → ARC → CLOTHOID → LINE (G2-continuous). Reduces to arc as Ls→0.
static int clothoid_spline(Port a, Port b, float R, float Ls, float *xs, float *ys){
    if (Ls < 0.5f) return arc_spline(a,b,R,xs,ys);
    float ad=a.dir;
    float uax=rk_ux_raw(ad),uay=rk_uy_raw(ad), ubx=rk_ux_raw(b.dir),uby=rk_uy_raw(b.dir);
    float den=uax*uby-uay*ubx;
    float dA=b.dir-ad; while(dA>180)dA-=360; while(dA<-180)dA+=360;
    float fa=dA<0?-dA:dA, turn=dA<0?-1.f:1.f;
    int n=0;
    if (fa<1.f || (den<0.02f&&den>-0.02f)){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
    float P_s=((b.x-a.x)*uby-(b.y-a.y)*ubx)/den;
    float Px=a.x+uax*P_s, Py=a.y+uay*P_s;
    float distB=(b.x-Px)*ubx+(b.y-Py)*uby;
    float avail=(P_s<distB?P_s:distB);
    if (avail<2.f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
    float tanH=rk_tan(fa*0.5f);
    float T=R*tanH;
    if (T>avail*0.95f){ R=avail*0.95f/tanH; T=avail*0.95f; }
    float room=avail-T;
    if (Ls>room*1.8f) Ls=room*1.8f;
    if (Ls<0.5f) return arc_spline(a,b,R,xs,ys);
    if (R<2) R=2;
    float thS=(Ls/(2*R))*RK_R2D;
    if (thS > fa*0.5f){ thS=fa*0.5f; Ls=2*R*(thS/RK_R2D); }
    int NC=10; float ds=Ls/NC, sig=1.f/(R*Ls);
    float lx=0,ly=0,th=0,k=0;
    for(int i=0;i<NC;i++){ float tm=th+(k*ds*0.5f)*RK_R2D; lx+=rk_ux_raw(tm)*ds; ly+=rk_uy_raw(tm)*ds; th+=(k+sig*ds*0.5f)*ds*RK_R2D; k+=sig*ds; }
    float p =ly - R*(1-cos_deg(thS));
    float kk=lx - R*sin_deg(thS);
    float Ts=(R+p)*rk_tan(fa*0.5f) + kk;
    float TSx=Px-uax*Ts, TSy=Py-uay*Ts;
    xs[n]=a.x; ys[n++]=a.y; xs[n]=TSx; ys[n++]=TSy;
    float x=TSx,y=TSy,hd=ad,kc=0,sg=turn/(R*Ls);
    for(int i=0;i<NC;i++){ float tm=hd+(kc*ds*0.5f)*RK_R2D; x+=rk_ux_raw(tm)*ds; y+=rk_uy_raw(tm)*ds; hd+=(kc+sg*ds*0.5f)*ds*RK_R2D; kc+=sg*ds; xs[n]=x;ys[n++]=y; }
    float arcDeg=fa-2*thS;
    if (arcDeg>0.5f){ int NA=(int)(arcDeg/6)+1; float ads=(R*arcDeg/RK_R2D)/NA;
        for(int i=0;i<NA;i++){ float tm=hd+(kc*ads*0.5f)*RK_R2D; x+=rk_ux_raw(tm)*ads; y+=rk_uy_raw(tm)*ads; hd+=kc*ads*RK_R2D; xs[n]=x;ys[n++]=y; } }
    sg=-turn/(R*Ls);
    for(int i=0;i<NC;i++){ float tm=hd+(kc*ds*0.5f)*RK_R2D; x+=rk_ux_raw(tm)*ds; y+=rk_uy_raw(tm)*ds; hd+=(kc+sg*ds*0.5f)*ds*RK_R2D; kc+=sg*ds; xs[n]=x;ys[n++]=y; }
    xs[n]=b.x; ys[n++]=b.y;
    return n;
}
// LOOP ramp: the ≈270° hard turn (the cloverleaf trick) — LINE → ARC(long way) → LINE, solved to land on B.
static int loop_spline(Port a, Port b, float R, float *xs, float *ys){
    float adA=a.dir;
    float uax=rk_ux_raw(adA),uay=rk_uy_raw(adA), ubx=rk_ux_raw(b.dir),uby=rk_uy_raw(b.dir);
    float den=uax*uby-uay*ubx;
    int n=0;
    if (den<0.02f&&den>-0.02f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
    float dA=b.dir-adA; while(dA>180)dA-=360; while(dA<-180)dA+=360;
    float sweep=(dA>=0)?dA-360.f:dA+360.f;
    float st=(sweep>=0)?1.f:-1.f;
    float lnx=-uay,lny=uax;
    float cx=a.x+st*lnx*R, cy=a.y+st*lny*R;
    float a0=angle_to((int)cx,(int)cy,(int)a.x,(int)a.y);
    float a1=a0+sweep;
    float e0x=cx+rk_ux_raw(a1)*R, e0y=cy+rk_uy_raw(a1)*R;
    float rx=b.x-e0x, ry=b.y-e0y;
    float stub =( rx*uby-ry*ubx)/den;
    float merge=( uax*ry-uay*rx)/den;
    if (stub<-2.f||merge<-2.f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
    float dx=uax*stub, dy=uay*stub;
    float t1x=a.x+dx,t1y=a.y+dy; cx+=dx; cy+=dy;
    xs[n]=a.x; ys[n++]=a.y; xs[n]=t1x; ys[n++]=t1y;
    int NS=32; for(int i=0;i<=NS;i++){ float ang=a0+sweep*(float)i/NS; xs[n]=cx+rk_ux_raw(ang)*R; ys[n++]=cy+rk_uy_raw(ang)*R; }
    xs[n]=b.x; ys[n++]=b.y;
    return n;
}
// append a circular ARC from P (tangent T) to endpoint Q — the biarc primitive the flyover S-curve uses.
static int emit_arc_to(float px,float py,float tx,float ty,float qx,float qy,float *xs,float *ys,int n,int steps){
    float tl=sqrtf(tx*tx+ty*ty); if(tl<0.0001f)tl=1; tx/=tl; ty/=tl;
    float nx=-ty, ny=tx;
    float wx=px-qx, wy=py-qy, wn=wx*nx+wy*ny;
    if (wn>-0.01f && wn<0.01f){ xs[n]=qx; ys[n++]=qy; return n; }
    float s=-(wx*wx+wy*wy)/(2*wn);
    float cx=px+nx*s, cy=py+ny*s, R=(s<0?-s:s);
    float a0=angle_to((int)cx,(int)cy,(int)px,(int)py);
    float a1=angle_to((int)cx,(int)cy,(int)qx,(int)qy);
    float rpx=px-cx, rpy=py-cy;
    int ccw = (-rpy*tx + rpx*ty) >= 0;
    float sw=a1-a0;
    if (ccw){ while(sw<=0)sw+=360; while(sw>360)sw-=360; }
    else    { while(sw>=0)sw-=360; while(sw<-360)sw+=360; }
    for(int i=1;i<=steps;i++){ float ang=a0+sw*(float)i/steps; xs[n]=cx+rk_ux_raw(ang)*R; ys[n++]=cy+rk_uy_raw(ang)*R; }
    return n;
}
// FLYOVER ramp: the semi-direct reverse-curve (S), an equal-tangent BIARC A→J→B (grade-separated).
static int scurve_spline(Port a, Port b, float *xs, float *ys){
    float t0x=rk_ux_raw(a.dir), t0y=rk_uy_raw(a.dir);
    float t1x=rk_ux_raw(b.dir), t1y=rk_uy_raw(b.dir);
    float vx=b.x-a.x, vy=b.y-a.y, vv=vx*vx+vy*vy;
    float tx=t0x+t1x, ty=t0y+t1y, vt=vx*tx+vy*ty;
    float dot=t0x*t1x+t0y*t1y, A=2*(dot-1);
    int n=0;
    float d;
    if (A>-0.0008f){
        if (vt>-0.5f&&vt<0.5f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
        d=vv/(2*vt);
    } else {
        float disc=vt*vt-A*vv; if(disc<0)disc=0; disc=sqrtf(disc);
        float r1=(vt+disc)/A, r2=(vt-disc)/A;
        d = (r1>0)?r1:r2; if(d<=0)d=(r1>r2?r1:r2);
    }
    if (d<1.f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
    float q1x=a.x+d*t0x, q1y=a.y+d*t0y;
    float q2x=b.x-d*t1x, q2y=b.y-d*t1y;
    float jx=(q1x+q2x)*0.5f, jy=(q1y+q2y)*0.5f;
    xs[n]=a.x; ys[n++]=a.y;
    n=emit_arc_to(a.x,a.y, t0x,t0y, jx,jy, xs,ys,n,16);
    n=emit_arc_to(jx,jy, q2x-q1x,q2y-q1y, b.x,b.y, xs,ys,n,16);
    return n;
}

// the TOPOLOGY layer: a junction = a table of connections; each is one movement drawn by a ramp PRIMITIVE.
typedef enum { RP_DIRECT, RP_LOOP, RP_FLYOVER, RP_THROUGH } RampPrim;
static const char* RP_NAME[] = { "direct", "loop", "flyover", "through" };
typedef struct { int from, to; } LaneLink;
typedef struct {
    int      inPort, outPort;
    RampPrim prim;
    LaneLink links[6]; int nLinks;
    float    R; int lift;
} Connection;
typedef struct { const char* name; Connection conns[16]; int nConns; } Junction;
typedef enum { JT_DIAMOND, JT_CLOVERLEAF, JT_STACK, JT_TRUMPET, JT_FORK, JT_TRIANGLE, JT_ROUNDABOUT } JuncType;
#define NJTYPE 7
static const char* JT_NAME[NJTYPE] = { "diamond", "cloverleaf", "stack", "trumpet", "fork", "triangle", "roundabout" };
static int is_ring(JuncType t){ return t==JT_ROUNDABOUT; }
typedef enum { T_THROUGH, T_RIGHT, T_LEFT, T_UTURN } Turn;
typedef struct { RampPrim through, right, left; int serveUturn; } JuncPolicy;
static const JuncPolicy POLICY[NJTYPE] = {
    /* diamond    */ { RP_THROUGH, RP_DIRECT, RP_DIRECT,  0 },
    /* cloverleaf */ { RP_THROUGH, RP_DIRECT, RP_LOOP,    0 },
    /* stack      */ { RP_THROUGH, RP_DIRECT, RP_FLYOVER, 0 },
    /* trumpet    */ { RP_THROUGH, RP_DIRECT, RP_LOOP,    0 },
    /* fork       */ { RP_THROUGH, RP_DIRECT, RP_DIRECT,  0 },
    /* triangle   */ { RP_THROUGH, RP_DIRECT, RP_FLYOVER, 0 },
    /* roundabout */ { RP_THROUGH, RP_DIRECT, RP_DIRECT,  0 },
};
static int topo_present(JuncType t, int L){ return !((t==JT_TRUMPET||t==JT_FORK||t==JT_TRIANGLE) && L==2); }
static int rk_leg_in (int L){ return L*2;   }
static int rk_leg_out(int L){ return L*2+1; }
static float rk_norm180(float a){ while(a>180)a-=360; while(a<-180)a+=360; return a; }
static Turn classify_turn(float inDir, float outDir){
    float rel=rk_norm180(outDir-inDir);
    if (rel>-30 && rel<30)   return T_THROUGH;
    if (rel>150 || rel<-150) return T_UTURN;
    return (rel*RK_DRIVE > 0) ? T_RIGHT : T_LEFT;
}
// GENERATE the connection table from a junction TYPE. Pure over (present[] mask, ports[], type, lanes).
static int rk_make_junction(int nleg, JuncType type, int lanes, const unsigned char *present,
                            const Port *ports, Junction *out){
    if (lanes<1) lanes=1;
    JuncPolicy p = POLICY[type]; out->name = JT_NAME[type]; out->nConns = 0;
    int leftSeen = 0;
    for (int o=0;o<nleg;o++) for (int d=0;d<nleg;d++){
        if (o==d || !present[o] || !present[d]) continue;
        Turn t = classify_turn(ports[rk_leg_in(o)].dir, ports[rk_leg_out(d)].dir);
        if (t==T_UTURN && !p.serveUturn) continue;
        RampPrim prim = (t==T_THROUGH)?p.through : (t==T_RIGHT)?p.right : p.left;
        if (type==JT_TRUMPET && t==T_LEFT) prim = (leftSeen++ == 0) ? RP_LOOP : RP_FLYOVER;
        float r = (prim==RP_LOOP)?(lanes*4.f>12.f?lanes*4.f:12.f):0.f;
        out->conns[out->nConns++] = (Connection){ rk_leg_in(o), rk_leg_out(d), prim, {{-1,-1},{-2,-2}}, lanes, r, 0 };
    }
    return out->nConns;
}

#endif // ROADKIT_H
