#include "studio.h"
#include "ui.h"        // on-screen buttons (per-finger capture, fat-finger hit pads) — never hand-roll
#include <stdio.h>
#include <math.h>

// roadlab — the FOUNDATION cart for the interchange geometry DSL, built on the research findings
// (docs/design/road-geometry-refs.md): LANE-ACCURATE roads (drive-on-right via DRIVE) + ports anchored
// to REAL lanes (a port = a lane + its travel direction) + ARC-SPLINE ramps (LINE→ARC→LINE; the
// OpenDRIVE model minus XML). Clothoid joints + nesting are later milestones. Supersedes rampkit
// (which proved the abstraction on Béziers); kept separate so rampkit stays as the proof reference.
//   MILESTONE 1: roads w/ lanes+arrows, ports on lanes, one arc-spline ramp between two ports.
//   MILESTONE 2: clothoid joints (LINE→CLOTHOID→ARC→CLOTHOID→LINE) — κ ramps 0→1/R→0 so steering is
//                continuous (G2); the §2 forward-integration loop, no Fresnel. C toggles it for A/B.
//   MILESTONE 3: MULTI-LANE ramps via lateral offsets of the one reference line (OpenDRIVE s/t shift).
//                Lanes = concentric offset polylines ⇒ constant gap ⇒ they NEST FOR FREE on arc ref-lines
//                — the empirical test of "do arcs nest without a solver?" (answer: yes). 1-4 set lanes.
//   MILESTONE 4: lane ADD/DROP via width(s) (OpenDRIVE lane-width model) — outer lanes taper full→0 over
//                the ramp's tail, staggered, so a fat ramp FANS DOWN to one lane (a real diverge + gore)
//                instead of a peeling slab. `taper` 0-100% scrubs it; t cycles. taper=0 ⇒ M3 parallel lanes.
//   MILESTONE 5: ELEVATION z(s) (OpenDRIVE elevation profile) — a ramp FLIES OVER the junction: z rises,
//                holds a deck height, falls (hump). Top-down depiction: deck in plan + drop-shadow ∝ z +
//                draws on top = grade separation. `lift` px scrubs it; e cycles. lift=0 ⇒ at grade.
//   MILESTONE 6: JUNCTION SCHEMA (OpenDRIVE junction/laneLink — docs/design/junction-lanelink.md). A
//                junction is a TABLE of connections; each connection = one movement (entry port → exit
//                port) + a ramp PRIMITIVE + its laneLinks. draw_junction() iterates the table, drawing
//                each connection with the M1–M5 splines. The single ramp becomes one Connection; `j`
//                toggles the sandbox ↔ the whole-junction view. This is the topology layer (the SHAPE
//                still lives in M1–M5) — the step that makes roadlab the table-driven roadnet2 drawer.
//   Controls: an on-screen ui.h toolbar — every control a clickable button. Keyboard: ←/→ A · ↑/↓ B ·
//             [ ] radius · ,/. Ls · C clothoid · 1-4 lanes · t taper · e lift · j sandbox/junction.
// Next: port roadlab into roadnet2 — the Junction[] table is the junction representation worldgen emits
//       (deterministically, from the seed) and this cart's draw_junction() is the drawer it calls.

#define DRIVE  1     // +1 = drive on the right — the single source of truth for handedness
#define LANEW  7     // one lane width (px)

static float ux(float d){ return cos_deg(d); }
static float uy(float d){ return sin_deg(d); }

// ── LANES = lateral offsets of ONE reference line (OpenDRIVE's "same s, shifted in t", §1/§5). We never
//    offset per-curve; we shift each sample ⊥ to the local tangent. On ARC ref-lines the offsets are
//    concentric arcs ⇒ constant gap ⇒ they NEST for free (the whole reason we chose arcs over Béziers). ──
// offset a polyline by d px along the local LEFT normal (averaged tangent; exact gap for dense samples)
static void offset_poly(const float *xs,const float *ys,int n,float d,float *ox,float *oy){
    for (int i=0;i<n;i++){
        float ax,ay;
        if      (i==0)   { ax=xs[1]-xs[0];       ay=ys[1]-ys[0]; }
        else if (i==n-1) { ax=xs[n-1]-xs[n-2];   ay=ys[n-1]-ys[n-2]; }
        else             { ax=xs[i+1]-xs[i-1];   ay=ys[i+1]-ys[i-1]; }   // averaged tangent at the vertex
        float L=sqrtf(ax*ax+ay*ay); if(L<0.001f)L=1;
        float nx=-ay/L, ny=ax/L;                                         // ⊥ (left) unit normal
        ox[i]=xs[i]+nx*d; oy[i]=ys[i]+ny*d;
    }
}
static void fill_strip(const float*lx,const float*ly,const float*rx,const float*ry,int n,int col){
    for (int i=0;i<n-1;i++){
        int xy[8]={(int)lx[i],(int)ly[i],(int)rx[i],(int)ry[i],(int)rx[i+1],(int)ry[i+1],(int)lx[i+1],(int)ly[i+1]};
        polyfill(xy,4,col);
    }
}
static void stroke_poly(const float*xs,const float*ys,int n,int col){
    for (int i=0;i<n-1;i++) line((int)xs[i],(int)ys[i],(int)xs[i+1],(int)ys[i+1],col);
}
// offset where the lateral distance VARIES per sample — d[i] px along the left normal (for width tapers)
static void offset_var(const float *xs,const float *ys,int n,const float *d,float *ox,float *oy){
    for (int i=0;i<n;i++){
        float ax,ay;
        if      (i==0)   { ax=xs[1]-xs[0];       ay=ys[1]-ys[0]; }
        else if (i==n-1) { ax=xs[n-1]-xs[n-2];   ay=ys[n-1]-ys[n-2]; }
        else             { ax=xs[i+1]-xs[i-1];   ay=ys[i+1]-ys[i-1]; }
        float L=sqrtf(ax*ax+ay*ay); if(L<0.001f)L=1;
        float nx=-ay/L, ny=ax/L;
        ox[i]=xs[i]+nx*d[i]; oy[i]=ys[i]+ny*d[i];
    }
}
static float smoothstep01(float t){ if(t<0)t=0; if(t>1)t=1; return t*t*(3-2*t); }

// ── M4: LANE ADD/DROP via width(s) (OpenDRIVE lane-width model). Each lane has a width that varies along
//    the ramp. Outer lanes DROP full→0 over the last `taperFrac` of the ramp, STAGGERED (outermost first)
//    so a fat ramp FANS DOWN to one lane — a real off-ramp diverge with its gore — instead of a slab. The
//    width profile is a smooth cubic (smoothstep). taperFrac=0 ⇒ constant width (the M3 parallel lanes). ──
// ── M5: ELEVATION z(s) (OpenDRIVE elevation profile). A ramp can FLY OVER the junction: z rises from 0
//    at port A, holds a deck height across the middle, falls back to 0 at port B (rise/hold/fall hump).
//    Depicted top-down (option a): the deck stays in PLAN position and draws on top; a drop-shadow,
//    offset ∝ z, is cast onto the ground first — so the higher road reads as grade-separated. liftPx=0
//    ⇒ at grade. (Draw-order by z is what resolves over/under; here: ground roads → shadow → deck.) ──
static void draw_multilane(const float*xs,const float*ys,int n,int nl,float taperFrac,float liftPx){
    int keep=1, nd=nl-keep;                              // 1 lane survives; outer nd lanes taper away
    float HW=nl*LANEW*0.5f;
    static float bnd[8][128], z[128];                    // per-sample lane-boundary offsets + height z(s)
    for (int i=0;i<n;i++){
        float f=(n>1)?(float)i/(n-1):0;
        float u=(f-(1.f-taperFrac))/(taperFrac>0.001f?taperFrac:1.f);   // 0 at taper start … 1 at ramp end
        float off=-HW; bnd[0][i]=off;
        for (int k=0;k<nl;k++){
            float w=LANEW;
            if (k>=keep && nd>0 && taperFrac>0.001f){
                int r=(nl-1)-k;                          // 0 = outermost (drops earliest)
                w=LANEW*(1.f - smoothstep01((u-(float)r/nd)*nd));        // staggered window of width 1/nd
            }
            off+=w; bnd[k+1][i]=off;
        }
        float zt = f<0.3f? smoothstep01(f/0.3f) : f>0.7f? smoothstep01((1.f-f)/0.3f) : 1.f;  // rise/hold/fall
        z[i]=liftPx*zt;
    }
    float ix[128],iy[128],ox[128],oy[128],e[128];
    if (liftPx>0.5f){                                    // SHADOW pass — deck footprint dropped onto ground
        for(int i=0;i<n;i++) e[i]=bnd[0][i]-1;  offset_var(xs,ys,n,e,ix,iy);
        for(int i=0;i<n;i++) e[i]=bnd[nl][i]+1; offset_var(xs,ys,n,e,ox,oy);
        for(int i=0;i<n;i++){ ix[i]+=z[i]*0.45f; iy[i]+=z[i]*0.85f; ox[i]+=z[i]*0.45f; oy[i]+=z[i]*0.85f; }
        fill_strip(ix,iy,ox,oy,n,CLR_BLACK);
    }
    for(int i=0;i<n;i++) e[i]=bnd[0][i]-1;   offset_var(xs,ys,n,e,ix,iy);   // casing inner edge
    for(int i=0;i<n;i++) e[i]=bnd[nl][i]+1;  offset_var(xs,ys,n,e,ox,oy);   // casing outer edge
    fill_strip(ix,iy,ox,oy,n,CLR_BROWNISH_BLACK);
    offset_var(xs,ys,n,bnd[0],ix,iy);  offset_var(xs,ys,n,bnd[nl],ox,oy);   // asphalt surface
    fill_strip(ix,iy,ox,oy,n,CLR_DARK_GREY);
    for (int b=1;b<nl;b++){ offset_var(xs,ys,n,bnd[b],ix,iy); stroke_poly(ix,iy,n,CLR_WHITE); }  // dividers (fan)
}

static void arrow(float x,float y,float dir,int col){      // a small travel-direction chevron
    float tx=x+ux(dir)*4, ty=y+uy(dir)*4;
    line((int)x,(int)y,(int)tx,(int)ty,col);
    line((int)tx,(int)ty,(int)(tx+ux(dir+150)*3),(int)(ty+uy(dir+150)*3),col);
    line((int)tx,(int)ty,(int)(tx+ux(dir-150)*3),(int)(ty+uy(dir-150)*3),col);
}

// ── PORT = a point on a lane + the lane's travel direction (degrees) ──
typedef struct { float x,y,dir; const char*name; } Port;
static Port ports[16]; static int nport=0;
static void addport(float x,float y,float dir,const char*nm){ ports[nport++]=(Port){x,y,dir,nm}; }

static void draw_port(Port p,int col){
    circfill((int)p.x,(int)p.y,2,col);
    float tx=p.x+ux(p.dir)*12, ty=p.y+uy(p.dir)*12;
    line((int)p.x,(int)p.y,(int)tx,(int)ty,col);
    line((int)tx,(int)ty,(int)(tx+ux(p.dir+150)*5),(int)(ty+uy(p.dir+150)*5),col);
    line((int)tx,(int)ty,(int)(tx+ux(p.dir-150)*5),(int)(ty+uy(p.dir-150)*5),col);
}

#define R2D 57.29578f                                  // radians → degrees
static float tan_deg(float d){ return sin_deg(d)/cos_deg(d); }

// ── ARC-SPLINE ramp: LINE → ARC → LINE between two ports (a "simple curve": round the corner where
//    A's heading-line meets B's heading-line with an arc of radius R). Returns the polyline length. ──
static int arc_spline(Port a, Port b, float R, float *xs, float *ys){
    float ad=a.dir+180;                                     // A is the ENTRY: ramp leaves A heading INTO the junction
    float uax=ux(ad),uay=uy(ad), ubx=ux(b.dir),uby=uy(b.dir);
    float den=uax*uby-uay*ubx;                              // cross(uA,uB); ~0 ⇒ parallel
    float dA=b.dir-ad; while(dA>180)dA-=360; while(dA<-180)dA+=360;
    float fa=dA<0?-dA:dA;
    int n=0;
    if (fa<1.f || (den<0.02f&&den>-0.02f)){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }  // ~straight
    float s=((b.x-a.x)*uby-(b.y-a.y)*ubx)/den;              // dist A→P along uA (the tangent room on A's side)
    float Px=a.x+uax*s, Py=a.y+uay*s;
    float distB=(b.x-Px)*ubx+(b.y-Py)*uby;                  // dist P→B along uB (tangent room on B's side)
    float avail=(s<distB?s:distB);                          // tightest tangent room
    if (avail<2.f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }  // corner behind a port ⇒ needs a LOOP; straight stand-in
    float tanH=sin_deg(fa*0.5f)/cos_deg(fa*0.5f);
    if (R>avail*0.95f/tanH) R=avail*0.95f/tanH;             // clamp R so the curve FITS the corner (no overshoot/cusp)
    float T=R*tanH;                                         // tangent length = R·tan(Δ/2)
    float t1x=Px-uax*T,t1y=Py-uay*T, t2x=Px+ubx*T,t2y=Py+uby*T;
    float nx=-uay,ny=uax;                                   // ⊥ to uA; pick the centre that's also R from T2
    float caX=t1x+nx*R,caY=t1y+ny*R, cbX=t1x-nx*R,cbY=t1y-ny*R, cx,cy;
    if (distance((int)caX,(int)caY,(int)t2x,(int)t2y) < distance((int)cbX,(int)cbY,(int)t2x,(int)t2y)){cx=caX;cy=caY;}
    else {cx=cbX;cy=cbY;}
    xs[n]=a.x; ys[n++]=a.y;                                 // LINE a → T1
    float a0=angle_to((int)cx,(int)cy,(int)t1x,(int)t1y);
    float a1=angle_to((int)cx,(int)cy,(int)t2x,(int)t2y);
    float sw=a1-a0; while(sw>180)sw-=360; while(sw<-180)sw+=360;
    for (int i=0;i<=16;i++){ float ang=a0+sw*(float)i/16; xs[n]=cx+ux(ang)*R; ys[n++]=cy+uy(ang)*R; }  // ARC
    xs[n]=b.x; ys[n++]=b.y;                                 // LINE T2 → b
    return n;
}

// ── CLOTHOID-JOINTED ramp (M2): LINE → CLOTHOID → ARC → CLOTHOID → LINE. The spirals ramp curvature
//    0 → 1/R → 0 (κ linear in arc length) so steering rate is continuous (G2) — "drives easily".
//    Construction: the standard spiral SHIFT — measure the spiral's lateral shift p and back-distance k
//    by integrating ONE local clothoid arm (the §2 forward-integration loop; exact, no Fresnel), shift
//    the equivalent circle inward by p, take tangent dist Ts=(R+p)·tan(Δ/2)+k, then integrate the real
//    curve forward. Reduces EXACTLY to arc_spline as Ls→0. ──
static int clothoid_spline(Port a, Port b, float R, float Ls, float *xs, float *ys){
    if (Ls < 0.5f) return arc_spline(a,b,R,xs,ys);          // Ls→0 IS the plain arc (avoids 1/(R·Ls) blow-up)
    float ad=a.dir+180;                                     // A is the ENTRY: ramp leaves A heading INTO the junction
    float uax=ux(ad),uay=uy(ad), ubx=ux(b.dir),uby=uy(b.dir);
    float den=uax*uby-uay*ubx;
    float dA=b.dir-ad; while(dA>180)dA-=360; while(dA<-180)dA+=360;
    float fa=dA<0?-dA:dA, turn=dA<0?-1.f:1.f;               // |Δ| and turn sign
    int n=0;
    if (fa<1.f || (den<0.02f&&den>-0.02f)){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
    float P_s=((b.x-a.x)*uby-(b.y-a.y)*ubx)/den;            // dist A→P along uA (tangent room on A's side)
    float Px=a.x+uax*P_s, Py=a.y+uay*P_s;
    float distB=(b.x-Px)*ubx+(b.y-Py)*uby;                  // dist P→B along uB (tangent room on B's side)
    float avail=(P_s<distB?P_s:distB);                      // tightest tangent room
    if (avail<2.f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }  // corner behind a port ⇒ needs a LOOP; straight stand-in
    float tanH=tan_deg(fa*0.5f);
    float T=R*tanH;                                         // bare-arc tangent — KEEP R (and the corner) if it fits
    if (T>avail*0.95f){ R=avail*0.95f/tanH; T=avail*0.95f; }//   only shrink R if even the plain arc overshoots (rare)
    float room=avail-T;                                     // leftover tangent room for the spiral back-distance k≈Ls/2
    if (Ls>room*1.8f) Ls=room*1.8f;                         // trim ONLY the spiral so the lead-in can't overshoot/cusp
    if (Ls<0.5f) return arc_spline(a,b,R,xs,ys);            //   no spiral room ⇒ the fitted plain arc keeps the corner
    if (R<2) R=2;
    // spiral angle θs = Ls/2R; clamp so two spirals fit the deflection (spiral-spiral limit → arc=0)
    float thS=(Ls/(2*R))*R2D;                               // spiral angle (deg)
    if (thS > fa*0.5f){ thS=fa*0.5f; Ls=2*R*(thS/R2D); }
    int NC=10; float ds=Ls/NC, sig=1.f/(R*Ls);              // dκ/ds; integrate one arm with +curvature
    float lx=0,ly=0,th=0,k=0;                               // local frame: x along tangent, y lateral
    for(int i=0;i<NC;i++){ float tm=th+(k*ds*0.5f)*R2D; lx+=ux(tm)*ds; ly+=uy(tm)*ds; th+=(k+sig*ds*0.5f)*ds*R2D; k+=sig*ds; }
    float p =ly - R*(1-cos_deg(thS));                       // lateral shift of the equivalent circle
    float kk=lx - R*sin_deg(thS);                           // back-distance (TS → equiv-circle PC)
    float Ts=(R+p)*tan_deg(fa*0.5f) + kk;                   // tangent dist from apex P to the spiral start
    float TSx=Px-uax*Ts, TSy=Py-uay*Ts;                     // start of entry spiral (after the lead-in line)
    // emit: LINE a → TS, then forward-integrate spiral / arc / spiral; append exact port b for the run-out
    xs[n]=a.x; ys[n++]=a.y; xs[n]=TSx; ys[n++]=TSy;
    float x=TSx,y=TSy,hd=ad,kc=0,sg=turn/(R*Ls);            // entry CLOTHOID: κ 0 → turn/R
    for(int i=0;i<NC;i++){ float tm=hd+(kc*ds*0.5f)*R2D; x+=ux(tm)*ds; y+=uy(tm)*ds; hd+=(kc+sg*ds*0.5f)*ds*R2D; kc+=sg*ds; xs[n]=x;ys[n++]=y; }
    float arcDeg=fa-2*thS;                                  // ARC: constant κ over the residual deflection
    if (arcDeg>0.5f){ int NA=(int)(arcDeg/6)+1; float ads=(R*arcDeg/R2D)/NA;
        for(int i=0;i<NA;i++){ float tm=hd+(kc*ads*0.5f)*R2D; x+=ux(tm)*ads; y+=uy(tm)*ads; hd+=kc*ads*R2D; xs[n]=x;ys[n++]=y; } }
    sg=-turn/(R*Ls);                                        // exit CLOTHOID: κ turn/R → 0
    for(int i=0;i<NC;i++){ float tm=hd+(kc*ds*0.5f)*R2D; x+=ux(tm)*ds; y+=uy(tm)*ds; hd+=(kc+sg*ds*0.5f)*ds*R2D; kc+=sg*ds; xs[n]=x;ys[n++]=y; }
    xs[n]=b.x; ys[n++]=b.y;                                 // LINE (run-out) → b
    return n;
}

// ── M6: JUNCTION SCHEMA (OpenDRIVE junction/laneLink, baked to C — docs/design/junction-lanelink.md) ──
//    The TOPOLOGY layer (interchange-dsl Layer 1): a junction is a TABLE of connections; each connection
//    is one movement (entry port → exit port) drawn by a ramp PRIMITIVE. The SHAPE lives in the spline
//    drawers above (M1–M5), NOT here. A roadlab PORT already IS a laneLink endpoint ("a lane + its travel
//    direction"), so a connection's lanes ARE its laneLinks (here 1:1 parallel ⇒ no lane change). ──
typedef enum { RP_DIRECT, RP_LOOP, RP_FLYOVER } RampPrim;   // generative: how to draw the connecting road.
                                                            // OpenDRIVE INFERS shape from geometry; we
                                                            // generate FROM this. Only RP_DIRECT has a real
                                                            // drawer yet (the arc/clothoid spline above);
                                                            // LOOP/FLYOVER fall back to it for now.
static const char* RP_NAME[] = { "direct", "loop", "flyover" };
typedef struct { int from, to; } LaneLink;                  // signed lane ids: incoming lane → connecting lane
typedef struct {
    int      inPort, outPort;        // entry port (A — the ENTRY, tangent points IN) → exit port (B)
    RampPrim prim;                   // OUR field (OpenDRIVE drops it; it's the lowered/baked form)
    LaneLink links[6]; int nLinks;   // lanes carried; nLinks>1 ⇒ lane change allowed (here all parallel)
} Connection;
typedef struct { const char* name; Connection conns[8]; int nConns; } Junction;

// A demo junction declared as a connection TABLE over the 4 ports (0 hw-W, 1 hw-E, 2 tr-N, 3 tr-S):
// a 4-way pinwheel of free-flow slip turns, each a DIRECT curve carrying 2 lanes. The point is the
// SCHEMA driving the drawer — not a tuned interchange (real left turns would be RP_LOOP / RP_FLYOVER).
static const Junction DEMO = { "4-way slip turns", {
    { 2, 0, RP_DIRECT, {{-1,-1},{-2,-2}}, 2 },   // tr-N → hw-W
    { 0, 3, RP_DIRECT, {{-1,-1},{-2,-2}}, 2 },   // hw-W → tr-S
    { 3, 1, RP_DIRECT, {{-1,-1},{-2,-2}}, 2 },   // tr-S → hw-E
    { 1, 2, RP_DIRECT, {{-1,-1},{-2,-2}}, 2 },   // hw-E → tr-N
}, 4 };

// draw ONE connection: pick the spline by the primitive, stroke its laneLink count as a multilane ribbon
static void draw_connection(Connection c, int useCloth, float R, float Ls, float taperFrac, float liftPx){
    float xs[128], ys[128];
    int n = useCloth ? clothoid_spline(ports[c.inPort], ports[c.outPort], R, Ls, xs, ys)
                     : arc_spline   (ports[c.inPort], ports[c.outPort], R,     xs, ys);
    draw_multilane(xs, ys, n, c.nLinks < 1 ? 1 : c.nLinks, taperFrac, liftPx);   // lanes = laneLink count
}
static void draw_junction(const Junction* j, int useCloth, float R, float Ls, float taperFrac, float liftPx){
    for (int i = 0; i < j->nConns; i++) draw_connection(j->conns[i], useCloth, R, Ls, taperFrac, liftPx);
}

// ── state ──
static int selA=2, selB=0, view=1; static float radius=30.f, spiral=14.f; static int use_cloth=1, nlanes=3, taperPct=60, lift=0;

// a "−/+" (or "</>") stepper: two ui buttons; returns -1, 0 or +1
static int step_btn(int x,int y,int w,const char*lm,const char*rm){
    int d=0;
    if (ui_button(x,       y, w, 13, lm)) d=-1;
    if (ui_button(x+w+1,   y, w, 13, rm)) d=+1;
    return d;
}

static void setup(void){
    if (nport) return;
    int CX=SCREEN_W/2, CY=SCREEN_H/2;
    // drive-on-right lanes: highway N lane = westbound, S lane = eastbound; trunk E lane = northbound, W = southbound
    addport(CX-44, CY-LANEW/2.0f, 180, "hw-W");    // 0  on the westbound (north) lane, west of the junction
    addport(CX+44, CY+LANEW/2.0f,   0, "hw-E");    // 1  on the eastbound (south) lane, east of the junction
    addport(CX+LANEW/2.0f, CY-44, 270, "tr-N");    // 2  on the northbound (east) lane, north of the junction
    addport(CX-LANEW/2.0f, CY+44,  90, "tr-S");    // 3  on the southbound (west) lane, south of the junction
}

void update(void){
    setup();
    if (keyp(KEY_LEFT))  selA=(selA+nport-1)%nport;
    if (keyp(KEY_RIGHT)) selA=(selA+1)%nport;
    if (keyp(KEY_DOWN))  selB=(selB+nport-1)%nport;
    if (keyp(KEY_UP))    selB=(selB+1)%nport;
    if (keyp('['))       radius-=2;
    if (keyp(']'))       radius+=2;
    if (radius<6) radius=6;
    if (keyp(','))       spiral-=2;
    if (keyp('.'))       spiral+=2;
    if (spiral<0) spiral=0;
    if (keyp('c')||keyp('C')) use_cloth=!use_cloth;
    if (keyp('1')) nlanes=1; if (keyp('2')) nlanes=2; if (keyp('3')) nlanes=3; if (keyp('4')) nlanes=4;
    if (keyp('t')||keyp('T')){ taperPct+=20; if(taperPct>100)taperPct=0; }   // cycle lane-drop taper
    if (keyp('e')||keyp('E')){ lift+=6; if(lift>18)lift=0; }                  // cycle flyover deck height
    if (keyp('j')||keyp('J')) view=!view;                                     // sandbox ↔ junction view
}

void draw(void){
    setup();
    ui_begin();
    cls(CLR_DARK_GREEN);
    int CX=SCREEN_W/2, CY=SCREEN_H/2;
    // lane-accurate roads: slabs + yellow median + per-lane direction arrows
    rectfill(0, CY-LANEW, SCREEN_W, 2*LANEW, CLR_DARKER_GREY);     // highway (E-W)
    rectfill(CX-LANEW, 0, 2*LANEW, SCREEN_H, CLR_DARKER_GREY);     // trunk   (N-S)
    line(0,CY,SCREEN_W,CY,CLR_YELLOW);  line(CX,0,CX,SCREEN_H,CLR_YELLOW);
    for (int x=12;x<SCREEN_W;x+=40){ arrow(x, CY-LANEW/2.0f, 180, CLR_YELLOW); arrow(x, CY+LANEW/2.0f, 0, CLR_YELLOW); }
    for (int y=12;y<SCREEN_H;y+=40){ arrow(CX+LANEW/2.0f, y, 270, CLR_YELLOW); arrow(CX-LANEW/2.0f, y, 90, CLR_YELLOW); }
    if (view){
        // JUNCTION view (M6) — the WHOLE junction drawn from the connection TABLE, each connection an
        // arc/clothoid spline ribbon. This is the table-driven drawer roadnet2 will call.
        draw_junction(&DEMO, use_cloth, radius, spiral, taperPct/100.f, (float)lift);
        for (int i=0;i<nport;i++) draw_port(ports[i], CLR_MEDIUM_GREY);
    } else {
        // RAMP sandbox — one ramp between the two selected ports: arc-spline (M1) + clothoid joints (M2),
        // drawn as an nl-lane ribbon via lateral offsets of the one reference line (M3: nesting via arcs)
        if (selA!=selB){
            float xs[128],ys[128];
            int n = use_cloth ? clothoid_spline(ports[selA],ports[selB],radius,spiral,xs,ys)
                              : arc_spline(ports[selA],ports[selB],radius,xs,ys);
            draw_multilane(xs,ys,n,nlanes, taperPct/100.f, (float)lift);
        }
        for (int i=0;i<nport;i++) if (i!=selA&&i!=selB) draw_port(ports[i], CLR_MEDIUM_GREY);
        draw_port(ports[selA], CLR_GREEN);  draw_port(ports[selB], CLR_RED);
    }

    // ── HUD — small 4×6 font so labels can be spelled out in full and still fit 320px ──
    font(FONT_SMALL);
    char b[64];
    if (view){
        snprintf(b,sizeof b,"Junction: %s   -   %d connections (table-driven)", DEMO.name, DEMO.nConns);
        print(b,4,5,CLR_WHITE);
        int yy=13;
        for (int i=0;i<DEMO.nConns;i++){ Connection c=DEMO.conns[i];
            snprintf(b,sizeof b,"%s -> %s : %s  x%d", ports[c.inPort].name, ports[c.outPort].name, RP_NAME[c.prim], c.nLinks);
            print(b,4,yy,CLR_LIGHT_GREY); yy+=7; }
    } else {
        snprintf(b,sizeof b,"Port A: %s    to    Port B: %s", ports[selA].name, ports[selB].name);
        print(b,4,5,CLR_WHITE);
        print(use_cloth ? "arc-spline + clothoid joints  -  continuous curvature (G2)"
                        : "arc-spline only  -  curvature steps at the corner (G1)",
              4,13, use_cloth?CLR_ORANGE:CLR_MEDIUM_GREY);
    }

    // ── on-screen control toolbar (clickable; keyboard still works) — 3 rows ──
    rectfill(0, SCREEN_H-52, SCREEN_W, 52, CLR_BLACK);
    int d;
    // row 1 — port pickers (green A / red B): prev/next + the live lane name
    print("Port A", 4, SCREEN_H-47, CLR_GREEN);
    d=step_btn(36, SCREEN_H-50, 14, "<", ">"); if (d) selA=(selA+nport+d)%nport;
    print(ports[selA].name, 70, SCREEN_H-47, CLR_GREEN);
    print("Port B", 128, SCREEN_H-47, CLR_RED);
    d=step_btn(160, SCREEN_H-50, 14, "<", ">"); if (d) selB=(selB+nport+d)%nport;
    print(ports[selB].name, 194, SCREEN_H-47, CLR_RED);
    if (ui_button(250, SCREEN_H-50, 66, 13, view?"view: junc":"view: ramp")) view=!view;
    // row 2 — geometry params (− / +) with live values
    print("radius", 4, SCREEN_H-31, CLR_WHITE);
    d=step_btn(36, SCREEN_H-34, 14, "-", "+"); if (d) radius+=2*d;
    snprintf(b,sizeof b,"%d",(int)radius); print(b, 70, SCREEN_H-31, CLR_LIGHT_GREY);
    print("spiral", 88, SCREEN_H-31, CLR_ORANGE);
    d=step_btn(120, SCREEN_H-34, 14, "-", "+"); if (d) spiral+=2*d;
    snprintf(b,sizeof b,"%d",(int)spiral); print(b, 152, SCREEN_H-31, CLR_LIGHT_GREY);
    print("lanes", 170, SCREEN_H-31, CLR_WHITE);
    d=step_btn(198, SCREEN_H-34, 14, "-", "+"); if (d) nlanes+=d;
    snprintf(b,sizeof b,"%d",nlanes); print(b, 230, SCREEN_H-31, CLR_LIGHT_GREY);
    // row 3 — cross-section: clothoid toggle, lane taper, flyover elevation
    if (ui_button(4, SCREEN_H-18, 66, 13, use_cloth?"clothoid: on":"clothoid: off")) use_cloth=!use_cloth;
    print("taper", 80, SCREEN_H-15, CLR_BLUE);
    d=step_btn(112, SCREEN_H-18, 14, "-", "+"); if (d) taperPct+=10*d;
    snprintf(b,sizeof b,"%d%%",taperPct); print(b, 144, SCREEN_H-15, CLR_LIGHT_GREY);
    print("lift", 176, SCREEN_H-15, CLR_INDIGO);
    d=step_btn(204, SCREEN_H-18, 14, "-", "+"); if (d) lift+=3*d;
    snprintf(b,sizeof b,"%dpx",lift); print(b, 236, SCREEN_H-15, CLR_LIGHT_GREY);
    // clamps (apply to both button + keyboard edits)
    if (radius<6) radius=6;  if (spiral<0) spiral=0;
    if (nlanes<1) nlanes=1;  if (nlanes>6) nlanes=6;
    if (taperPct<0) taperPct=0;  if (taperPct>100) taperPct=100;
    if (lift<0) lift=0;  if (lift>24) lift=24;

    ui_end();
    font(FONT_NORMAL);
}
