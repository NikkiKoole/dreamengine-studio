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
//   HARD-TURN drawers (spec §8.1): RP_LOOP — loop_spline() goes the LONG way (≈270°) so it never crosses
//                opposing traffic (the cloverleaf trick): LINE→ARC→LINE, the two line lengths solved so a
//                fixed-R loop lands on B. RP_FLYOVER — scurve_spline() is a SEMI-DIRECT reverse curve (an
//                equal-tangent BIARC: two tangent arcs joined at a midpoint) that crosses the conflict on a
//                raised deck. `l` cycles the sandbox ramp primitive (direct/loop/flyover) for any port pair.
//   GENERATOR (spec §8.2): make_junction(type) builds the connection TABLE itself — enumerate every
//                movement, classify the turn (through/right/left via relative bearing, DRIVE-folded), assign
//                the primitive from a per-type POLICY. diamond/cloverleaf/stack differ ONLY in how the hard
//                LEFT turn is served (direct / loop / flyover). Pure fn of (ports, type) ⇒ deterministic. The
//                junction view now shows this GENERATED junction; `g` cycles the type. (Was the hand DEMO.)
//   LEG LAYER (skew): a junction is a list of LEGS, each a road arm at a `bearing`; rebuild_ports() lays the
//                ports out from the legs, so NOTHING is hardcoded to 90° — the ramp drawers were already
//                angle-agnostic, only the stage was perpendicular. The `skew` control angles the crossing
//                road (the trunk) and the ramps + generated junction re-solve.
//   TOPOLOGY: a junction recipe = topology × policy. Per-leg `present` gives the 3-leg family on a T (drop
//                the north arm): TRUMPET (1 loop + 1 flyover — asymmetric), FORK (all direct), TRIANGLE (3
//                flyovers). `g` now cycles all 6 (diamond/cloverleaf/stack + trumpet/fork/triangle). The
//                ring/roundabout family is the remaining peer construct. See docs/design/junction-lanelink.md §9.
//   Controls: an on-screen ui.h toolbar — every control a clickable button. Keyboard: ←/→ A · ↑/↓ B ·
//             [ ] radius · ,/. Ls · C clothoid · 1-4 lanes · t taper · e lift · j sandbox/junction · l primitive · g type.
//             (skew has a toolbar stepper.)
// Next: port into a world that EMITS (legs,type) per crossing from the seed and calls make_junction() +
//       draw_junction() as the drawer — a new junction-first world, or roadnet2 (see junction-lanelink §7).

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

// ── LEG LAYER: a junction = a list of LEGS, each a road arm meeting the hub at a `bearing` (outward, deg)
//    with two carriageways (inbound + outbound). The ports are LAID OUT from the legs (rebuild_ports), so
//    nothing is hardcoded to 90° — vary a bearing → SKEW; vary `present` → topology (trumpet/fork, later).
//    The 4 default legs are two collinear pairs (W–E highway, N–S trunk) = the perpendicular crossroads. ──
#define NLEG   4
#define LEGLEN 44
typedef struct { float bearing; int present; const char* in; const char* out; } Leg;
static Leg legs[NLEG] = {
    { 180, 1, "W-in", "W-out" },   // 0  highway west arm
    {   0, 1, "E-in", "E-out" },   // 1  highway east arm
    { 270, 1, "N-in", "N-out" },   // 2  trunk north arm  (the trunk — legs 2,3 — skews to angle the crossing)
    {  90, 1, "S-in", "S-out" },   // 3  trunk south arm
};
static int skew=0;                 // degrees added to the TRUNK legs (2,3): the crossing road tilts but stays straight
static float leg_bearing(int L){ return legs[L].bearing + (L>=2 ? (float)skew : 0.f); }

static void draw_port(Port p,int col){
    circfill((int)p.x,(int)p.y,2,col);
    float tx=p.x+ux(p.dir)*12, ty=p.y+uy(p.dir)*12;
    line((int)p.x,(int)p.y,(int)tx,(int)ty,col);
    line((int)tx,(int)ty,(int)(tx+ux(p.dir+150)*5),(int)(ty+uy(p.dir+150)*5),col);
    line((int)tx,(int)ty,(int)(tx+ux(p.dir-150)*5),(int)(ty+uy(p.dir-150)*5),col);
}

// classify a port GEOMETRICALLY (no extra fields): INBOUND = its travel dir points toward the junction
// centre (entries live here); leg = which side it sits on. Used only to FLAG nonsense sandbox movements —
// the picker stays free (handy for probing odd pairs), it just marks what isn't a real movement.
static int port_inbound(Port p){ float cx=SCREEN_W/2.0f, cy=SCREEN_H/2.0f;
    return (cx-p.x)*ux(p.dir) + (cy-p.y)*uy(p.dir) > 0; }
// why a sandbox A→B pair (by port index) isn't a real movement (NULL = valid): entries must be inbound,
// exits outbound, and the two must be on different legs (port i belongs to leg i/2 — two ports per leg).
static const char* movement_problem(int ai, int bi){
    if (!port_inbound(ports[ai]))  return "Port A (EXIT lane) - entry against traffic";
    if ( port_inbound(ports[bi]))  return "Port B (ENTRY lane) - merge against traffic";
    if (ai/2 == bi/2)              return "same leg - a U-turn";
    return 0;
}

#define R2D 57.29578f                                  // radians → degrees
static float tan_deg(float d){ return sin_deg(d)/cos_deg(d); }

// ── ARC-SPLINE ramp: LINE → ARC → LINE between two ports (a "simple curve": round the corner where
//    A's heading-line meets B's heading-line with an arc of radius R). Returns the polyline length. ──
static int arc_spline(Port a, Port b, float R, float *xs, float *ys){
    float ad=a.dir;                                         // A is the ENTRY: leave along the inbound lane's travel dir
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
    float ad=a.dir;                                         // A is the ENTRY: leave along the inbound lane's travel dir
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

// ── LOOP ramp (spec §8.1): the HARD (left-equivalent) turn — go the LONG way (≈270°) so the ramp never
//    crosses opposing traffic (the cloverleaf trick). LINE(diverge) → ARC(loop, |sweep|>180) → LINE(merge).
//    The arc is tangent to A's heading at the start and to B's heading at the end; the two line lengths
//    (diverge stub + merge) are SOLVED (a 2×2, exactly like arc_spline's apex) so a FIXED-R loop still lands
//    on B. Unique given (A,B,R): the long-way sweep Δ∓360 fixes both the angle AND the turn side. Two loops
//    nest as concentric offsets (M3) at different R — no solver. ──
static int loop_spline(Port a, Port b, float R, float *xs, float *ys){
    float adA=a.dir;                                        // A is the ENTRY: leave along the inbound lane's travel dir
    float uax=ux(adA),uay=uy(adA), ubx=ux(b.dir),uby=uy(b.dir);
    float den=uax*uby-uay*ubx;                              // cross(uA,uB); ~0 ⇒ parallel ⇒ no loop solution
    int n=0;
    if (den<0.02f&&den>-0.02f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
    float dA=b.dir-adA; while(dA>180)dA-=360; while(dA<-180)dA+=360;        // Δ (short deflection)
    float sweep=(dA>=0)?dA-360.f:dA+360.f;                  // the LONG way round (|sweep|∈(180,360))
    float st=(sweep>=0)?1.f:-1.f;                           // turn side = sign of the sweep
    float lnx=-uay,lny=uax;                                 // left normal of uA
    float cx=a.x+st*lnx*R, cy=a.y+st*lny*R;                 // loop centre (stub=0 baseline: tangent at A)
    float a0=angle_to((int)cx,(int)cy,(int)a.x,(int)a.y);   // arc start angle (C→A)
    float a1=a0+sweep;
    float e0x=cx+ux(a1)*R, e0y=cy+uy(a1)*R;                 // arc end at stub=0
    float rx=b.x-e0x, ry=b.y-e0y;                           // solve  uA·stub + uB·merge = B − E0  (Cramer)
    float stub =( rx*uby-ry*ubx)/den;
    float merge=( uax*ry-uay*rx)/den;
    if (stub<-2.f||merge<-2.f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }  // no clean loop ⇒ stand-in
    float dx=uax*stub, dy=uay*stub;                         // translate the whole circle along uA by stub
    float t1x=a.x+dx,t1y=a.y+dy; cx+=dx; cy+=dy;
    xs[n]=a.x; ys[n++]=a.y; xs[n]=t1x; ys[n++]=t1y;         // LINE diverge: A → T1
    int NS=32; for(int i=0;i<=NS;i++){ float ang=a0+sweep*(float)i/NS; xs[n]=cx+ux(ang)*R; ys[n++]=cy+uy(ang)*R; }  // ARC
    xs[n]=b.x; ys[n++]=b.y;                                 // LINE merge: E → B
    return n;
}

// append a circular ARC from P (unit-ish tangent T) to endpoint Q. The centre is the point on P's normal
// equidistant from P and Q; the sweep direction is whichever matches T. (The biarc primitive below.)
static int emit_arc_to(float px,float py,float tx,float ty,float qx,float qy,float *xs,float *ys,int n,int steps){
    float tl=sqrtf(tx*tx+ty*ty); if(tl<0.0001f)tl=1; tx/=tl; ty/=tl;     // unit tangent
    float nx=-ty, ny=tx;                                                 // left normal
    float wx=px-qx, wy=py-qy, wn=wx*nx+wy*ny;
    if (wn>-0.01f && wn<0.01f){ xs[n]=qx; ys[n++]=qy; return n; }         // P,Q,T ~collinear ⇒ straight
    float s=-(wx*wx+wy*wy)/(2*wn);                                        // signed dist along normal → centre
    float cx=px+nx*s, cy=py+ny*s, R=(s<0?-s:s);
    float a0=angle_to((int)cx,(int)cy,(int)px,(int)py);
    float a1=angle_to((int)cx,(int)cy,(int)qx,(int)qy);
    float rpx=px-cx, rpy=py-cy;                                          // radial at P; CCW tangent = (−rpy,rpx)
    int ccw = (-rpy*tx + rpx*ty) >= 0;
    float sw=a1-a0;
    if (ccw){ while(sw<=0)sw+=360; while(sw>360)sw-=360; }                // sweep the way T points
    else    { while(sw>=0)sw-=360; while(sw<-360)sw+=360; }
    for(int i=1;i<=steps;i++){ float ang=a0+sw*(float)i/steps; xs[n]=cx+ux(ang)*R; ys[n++]=cy+uy(ang)*R; }
    return n;
}

// ── FLYOVER ramp (spec §8.1): a SEMI-DIRECT turn that's ALLOWED to cross the conflict point because it's
//    grade-separated — the characteristic reverse-curve (S) shape (diverge one way, arc back the other),
//    NOT the 270° loop and NOT the corner-rounder. Built as an equal-tangent BIARC: two circular arcs that
//    join with a continuous tangent at a midpoint J, connecting A(tangent uA) → B(tangent uB) exactly. The
//    joint distance d solves a quadratic (equal tangent lengths); the deck (lift) is forced by the caller. ──
static int scurve_spline(Port a, Port b, float *xs, float *ys){
    float t0x=ux(a.dir), t0y=uy(a.dir);                     // leave A along the inbound lane's travel dir
    float t1x=ux(b.dir), t1y=uy(b.dir);                     // arrive B along the outbound lane's travel dir
    float vx=b.x-a.x, vy=b.y-a.y, vv=vx*vx+vy*vy;
    float tx=t0x+t1x, ty=t0y+t1y, vt=vx*tx+vy*ty;
    float dot=t0x*t1x+t0y*t1y, A=2*(dot-1);                 // A→0 as the tangents become parallel
    int n=0;
    float d;
    if (A>-0.0008f){                                        // parallel tangents ⇒ linear (or degenerate)
        if (vt>-0.5f&&vt<0.5f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
        d=vv/(2*vt);
    } else {
        float disc=vt*vt-A*vv; if(disc<0)disc=0; disc=sqrtf(disc);
        float r1=(vt+disc)/A, r2=(vt-disc)/A;
        d = (r1>0)?r1:r2; if(d<=0)d=(r1>r2?r1:r2);
    }
    if (d<1.f){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; } // degenerate ⇒ stand-in
    float q1x=a.x+d*t0x, q1y=a.y+d*t0y;                     // ends of the two equal tangents
    float q2x=b.x-d*t1x, q2y=b.y-d*t1y;
    float jx=(q1x+q2x)*0.5f, jy=(q1y+q2y)*0.5f;             // joint J (shared tangent = q1→q2)
    xs[n]=a.x; ys[n++]=a.y;
    n=emit_arc_to(a.x,a.y, t0x,t0y, jx,jy, xs,ys,n,16);     // arc 1: A → J  (tangent uA at A)
    n=emit_arc_to(jx,jy, q2x-q1x,q2y-q1y, b.x,b.y, xs,ys,n,16);  // arc 2: J → B  (tangent uB at B)
    return n;
}

// ── M6: JUNCTION SCHEMA (OpenDRIVE junction/laneLink, baked to C — docs/design/junction-lanelink.md) ──
//    The TOPOLOGY layer (interchange-dsl Layer 1): a junction is a TABLE of connections; each connection
//    is one movement (entry port → exit port) drawn by a ramp PRIMITIVE. The SHAPE lives in the spline
//    drawers above (M1–M5), NOT here. A roadlab PORT already IS a laneLink endpoint ("a lane + its travel
//    direction"), so a connection's lanes ARE its laneLinks (here 1:1 parallel ⇒ no lane change). ──
typedef enum { RP_DIRECT, RP_LOOP, RP_FLYOVER, RP_THROUGH } RampPrim;  // generative: how to draw the
                                                            // connecting road. OpenDRIVE INFERS shape from
                                                            // geometry; we generate FROM this. THROUGH = the
                                                            // road continues straight across (the mainline);
                                                            // LOOP/FLYOVER are the hard (left-equiv) turns.
static const char* RP_NAME[] = { "direct", "loop", "flyover", "through" };
typedef struct { int from, to; } LaneLink;                  // signed lane ids: incoming lane → connecting lane
typedef struct {
    int      inPort, outPort;        // entry port (A — the ENTRY, tangent points IN) → exit port (B)
    RampPrim prim;                   // OUR field (OpenDRIVE drops it; it's the lowered/baked form)
    LaneLink links[6]; int nLinks;   // lanes carried; nLinks>1 ⇒ lane change allowed (here all parallel)
    float    R; int lift;            // per-connection radius / flyover deck (0 ⇒ use the global default)
} Connection;
typedef struct { const char* name; Connection conns[16]; int nConns; } Junction;  // a 4-way serves 12 movements

// ── THE GENERATOR (spec §8.2): build the connection TABLE from a junction TYPE — interchange-dsl Layer 1
//    in code. A type is a POLICY (which primitive serves each turn class), NOT geometry. We enumerate every
//    movement (entry leg → exit leg), classify the turn by relative bearing (DRIVE folds handedness so
//    "right" is always the easy side), and assign the primitive. PURE function of (legs, type) ⇒ the same
//    seed grows the same junction. The SHAPE is still the M1–M5 splines; this only decides who connects to
//    whom, with which primitive. (Supersedes the old hand-authored DEMO table.) ──
// JUNCTION TYPE = topology (how many legs) × policy (how the hard turn is served). The 4-leg family sits on
// the full CROSS; the 3-leg family (trumpet/fork/triangle) on a T — see topo_present. (roads.org.uk taxonomy,
// junction-lanelink §9: a recipe is `topology × policy`.)
typedef enum { JT_DIAMOND, JT_CLOVERLEAF, JT_STACK, JT_TRUMPET, JT_FORK, JT_TRIANGLE } JuncType;
#define NJTYPE 6
static const char* JT_NAME[NJTYPE] = { "diamond", "cloverleaf", "stack", "trumpet", "fork", "triangle" };
typedef enum { T_THROUGH, T_RIGHT, T_LEFT, T_UTURN } Turn;
typedef struct { RampPrim through, right, left; int serveUturn; } JuncPolicy;
static const JuncPolicy POLICY[NJTYPE] = {                  // 4-leg types differ ONLY in the LEFT column
    /* diamond    */ { RP_THROUGH, RP_DIRECT, RP_DIRECT,  0 },   // 4-leg: hard left = at-grade direct (signalised)
    /* cloverleaf */ { RP_THROUGH, RP_DIRECT, RP_LOOP,    0 },   // 4-leg: hard left = loop (≈270°, no crossing)
    /* stack      */ { RP_THROUGH, RP_DIRECT, RP_FLYOVER, 0 },   // 4-leg: hard left = flyover (semi-direct, over)
    /* trumpet    */ { RP_THROUGH, RP_DIRECT, RP_LOOP,    0 },   // 3-leg T: lefts ASYMMETRIC — 1 loop + 1 flyover (below)
    /* fork       */ { RP_THROUGH, RP_DIRECT, RP_DIRECT,  0 },   // 3-leg T: all at-grade direct (a simple Y)
    /* triangle   */ { RP_THROUGH, RP_DIRECT, RP_FLYOVER, 0 },   // 3-leg T: 3 flyovers (the loopless free-flow cousin)
};
// topology — which legs are present for a type. 4-leg = full cross; 3-leg (trumpet/fork/triangle) = a T with
// the NORTH trunk arm (leg 2) dropped, so the trunk terminates at the highway from the south.
static int topo_present(JuncType t, int L){ return !(t>=JT_TRUMPET && L==2); }
// the 4 legs are encoded in the port layout (setup order = in,out per leg) — leg L: 0=W 1=E 2=N 3=S
static int leg_in (int L){ return L*2;   }                  // inbound (entry) port of leg L
static int leg_out(int L){ return L*2+1; }                  // outbound (exit) port of leg L
static float norm180(float a){ while(a>180)a-=360; while(a<-180)a+=360; return a; }
static Turn classify_turn(float inDir, float outDir){       // by the change in travel heading through the hub
    float rel=norm180(outDir-inDir);
    if (rel>-30 && rel<30)   return T_THROUGH;               // heading ~unchanged ⇒ straight across
    if (rel>150 || rel<-150) return T_UTURN;                 // ~reversed ⇒ U-turn
    return (rel*DRIVE > 0) ? T_RIGHT : T_LEFT;               // else easy (right) vs hard (left), DRIVE-folded
}
// GENERATE: enumerate every movement over `nleg` legs → one Connection per served movement, per the policy.
// `lanes` = how many lanes each ramp carries (from the lanes control). Loops get a radius scaled to the lane
// count so the inner edge can't pinch past the centre (a tight loop can't fit a fat ribbon).
static int make_junction(int nleg, JuncType type, int lanes, Junction *out){
    if (lanes<1) lanes=1;
    JuncPolicy p = POLICY[type]; out->name = JT_NAME[type]; out->nConns = 0;
    int leftSeen = 0;                                       // for the trumpet's ASYMMETRIC hard turns
    for (int o=0;o<nleg;o++) for (int d=0;d<nleg;d++){
        if (o==d || !legs[o].present || !legs[d].present) continue;   // only served between PRESENT legs (topology)
        Turn t = classify_turn(ports[leg_in(o)].dir, ports[leg_out(d)].dir);
        if (t==T_UTURN && !p.serveUturn) continue;
        RampPrim prim = (t==T_THROUGH)?p.through : (t==T_RIGHT)?p.right : p.left;
        if (type==JT_TRUMPET && t==T_LEFT) prim = (leftSeen++ == 0) ? RP_LOOP : RP_FLYOVER;  // trumpet: 1 loop + 1 flyover
        float r = (prim==RP_LOOP)?(lanes*4.f>12.f?lanes*4.f:12.f):0.f;   // loops: R scales with lanes; direct uses global
        out->conns[out->nConns++] = (Connection){ leg_in(o), leg_out(d), prim, {{-1,-1},{-2,-2}}, lanes, r, 0 };
    }
    return out->nConns;
}

// draw ONE connection: pick the spline by the PRIMITIVE, stroke its laneLink count as a multilane ribbon
static void draw_connection(Connection c, int useCloth, float R, float Ls, float taperFrac, float liftPx, int warn){
    float xs[128], ys[128];
    float r    = c.R > 0.5f   ? c.R         : R;            // per-connection overrides, else the global
    float lift = c.lift > 0   ? (float)c.lift : liftPx;
    int n;
    if      (c.prim == RP_LOOP)    n = loop_spline    (ports[c.inPort], ports[c.outPort], r, xs, ys);
    else if (c.prim == RP_FLYOVER) n = scurve_spline  (ports[c.inPort], ports[c.outPort],    xs, ys);
    else if (useCloth)             n = clothoid_spline(ports[c.inPort], ports[c.outPort], r, Ls, xs, ys);
    else                           n = arc_spline     (ports[c.inPort], ports[c.outPort], r, xs, ys);
    if (c.prim == RP_FLYOVER && lift < 6) lift = 12;        // a flyover is a semi-direct S-curve on a raised deck
    draw_multilane(xs, ys, n, c.nLinks < 1 ? 1 : c.nLinks, taperFrac, lift);     // lanes = laneLink count
    if (warn) stroke_poly(xs, ys, n, CLR_RED);              // FLAG: not a real movement — red spine on the ramp
}
static void draw_junction(const Junction* j, int useCloth, float R, float Ls, float taperFrac, float liftPx){
    for (int i = 0; i < j->nConns; i++)
        if (j->conns[i].prim != RP_THROUGH)                 // the through movement IS the road itself, not a ramp
            draw_connection(j->conns[i], useCloth, R, Ls, taperFrac, liftPx, 0);
}

// ── state ──
static int selA=4, selB=3, view=1, sandPrim=1; static float radius=30.f, spiral=14.f; static int use_cloth=1, nlanes=3, taperPct=60, lift=0;
static int juncType=1; static Junction gen_junc;           // junction view: type (default cloverleaf) + the generated table

// a "−/+" (or "</>") stepper: two ui buttons; returns -1, 0 or +1
static int step_btn(int x,int y,int w,const char*lm,const char*rm){
    int d=0;
    if (ui_button(x,       y, w, 13, lm)) d=-1;
    if (ui_button(x+w+1,   y, w, 13, rm)) d=+1;
    return d;
}

// lay the 8 ports out from the legs: per leg, inbound (toward centre) + outbound (away), one each side of the
// arm centreline (drive-on-right: outbound on the RIGHT of the outward direction). Port i belongs to leg i/2.
static void rebuild_ports(void){
    nport=0;
    float cx=SCREEN_W/2.0f, cy=SCREEN_H/2.0f;
    for (int L=0; L<NLEG; L++){
        float b=leg_bearing(L), ax=ux(b), ay=uy(b), rx=-ay, ry=ax;     // outward dir + its right normal
        float tx=cx+ax*LEGLEN, ty=cy+ay*LEGLEN;                        // arm tip
        addport(tx - rx*(LANEW/2.0f), ty - ry*(LANEW/2.0f), b+180, legs[L].in);   // inbound  = port L*2
        addport(tx + rx*(LANEW/2.0f), ty + ry*(LANEW/2.0f), b,      legs[L].out);  // outbound = port L*2+1
    }
}
// draw one road ARM from the centre outward along its bearing: asphalt slab + yellow centreline + lane arrows
static void draw_arm(int L){
    float cx=SCREEN_W/2.0f, cy=SCREEN_H/2.0f;
    float b=leg_bearing(L), ax=ux(b), ay=uy(b), rx=-ay, ry=ax, W=LANEW;
    float ex=cx+ax*(SCREEN_W+SCREEN_H), ey=cy+ay*(SCREEN_W+SCREEN_H);  // run well past the screen edge
    int q[8]={(int)(cx+rx*W),(int)(cy+ry*W),(int)(cx-rx*W),(int)(cy-ry*W),
              (int)(ex-rx*W),(int)(ey-ry*W),(int)(ex+rx*W),(int)(ey+ry*W)};
    polyfill(q,4,CLR_DARKER_GREY);
    line((int)cx,(int)cy,(int)ex,(int)ey,CLR_YELLOW);
    for (float t=12;t<420;t+=40){ float px=cx+ax*t, py=cy+ay*t;
        arrow(px+rx*(W*0.5f),py+ry*(W*0.5f), b,     CLR_YELLOW);       // outbound lane (right side)
        arrow(px-rx*(W*0.5f),py-ry*(W*0.5f), b+180, CLR_YELLOW); }     // inbound lane
}

void update(void){
    rebuild_ports();
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
    if (keyp('l')||keyp('L')) sandPrim=(sandPrim+1)%3;                         // sandbox ramp primitive: direct/loop/flyover
    if (keyp('g')||keyp('G')) juncType=(juncType+1)%NJTYPE;                    // junction type (diamond…triangle)
}

void draw(void){
    rebuild_ports();
    ui_begin();
    cls(CLR_DARK_GREEN);
    // topology: in the JUNCTION view the type decides which legs exist (4-leg cross vs 3-leg T); the sandbox
    // is always the full cross (a freeform bench). Set presence before drawing roads / generating.
    for (int L=0; L<NLEG; L++) legs[L].present = view ? topo_present((JuncType)juncType, L) : 1;
    // lane-accurate roads, drawn from the LEGS (so a skewed leg draws tilted)
    for (int L=0; L<NLEG; L++) if (legs[L].present) draw_arm(L);
    const char* sand_problem = (selA!=selB) ? movement_problem(selA, selB) : 0;
    make_junction(4, (JuncType)juncType, nlanes, &gen_junc);  // GENERATE the table from the type (pure fn of ports+type+lanes)
    if (view){
        // JUNCTION view (M6 + §8.2) — the WHOLE junction GENERATED from the type, then drawn from the
        // connection table. This is the table-driven drawer roadnet2 will call (worldgen picks the type).
        draw_junction(&gen_junc, use_cloth, radius, spiral, taperPct/100.f, (float)lift);
        for (int i=0;i<nport;i++) if (legs[i/2].present) draw_port(ports[i], CLR_MEDIUM_GREY);
    } else {
        // RAMP sandbox — one ramp between the two selected ports, as a transient Connection so it runs the
        // SAME drawer as the junction: direct = arc-spline (M1) + clothoid joints (M2), loop = loop_spline
        // (§8.1), flyover = direct + deck. Drawn as an nl-lane ribbon (M3 lateral offsets / nesting via arcs).
        if (selA!=selB){
            Connection c = { selA, selB, (RampPrim)sandPrim, {{-1,-1},{-2,-2}}, nlanes, 0, 0 };
            draw_connection(c, use_cloth, radius, spiral, taperPct/100.f, (float)lift, sand_problem!=0);
        }
        for (int i=0;i<nport;i++) if (i!=selA&&i!=selB) draw_port(ports[i], CLR_MEDIUM_GREY);
        draw_port(ports[selA], CLR_GREEN);  draw_port(ports[selB], CLR_RED);
    }

    // ── HUD — small 4×6 font so labels can be spelled out in full and still fit 320px ──
    font(FONT_SMALL);
    char b[64];
    if (view){
        snprintf(b,sizeof b,"Junction: %s  -  %d movements (g=type)", gen_junc.name, gen_junc.nConns);
        print(b,4,5,CLR_WHITE);
        int yy=13;
        for (int i=0;i<gen_junc.nConns;i++){ Connection c=gen_junc.conns[i];
            snprintf(b,sizeof b,"%s -> %s : %s", ports[c.inPort].name, ports[c.outPort].name, RP_NAME[c.prim]);
            print(b,4,yy, c.prim==RP_THROUGH?CLR_DARKER_GREY:CLR_LIGHT_GREY); yy+=7; }
    } else {
        snprintf(b,sizeof b,"Port A: %s   to   Port B: %s    [%s]", ports[selA].name, ports[selB].name, RP_NAME[sandPrim]);
        print(b,4,5,CLR_WHITE);
        const char* note = sandPrim==RP_LOOP    ? "loop  -  the hard turn, ~270 the long way (no crossing)"
                         : sandPrim==RP_FLYOVER ? "flyover  -  semi-direct S-curve on a raised deck"
                         : use_cloth            ? "arc-spline + clothoid joints  -  continuous curvature (G2)"
                                                : "arc-spline only  -  curvature steps at the corner (G1)";
        print(note,4,13, sandPrim?CLR_GREEN:(use_cloth?CLR_ORANGE:CLR_MEDIUM_GREY));
        if (sand_problem){ snprintf(b,sizeof b,"x not a real movement: %s", sand_problem); print(b,4,21,CLR_RED); }
    }

    // ── on-screen control toolbar (clickable; keyboard still works) — 3 rows ──
    rectfill(0, SCREEN_H-52, SCREEN_W, 52, CLR_BLACK);
    int d;
    // row 1 — port pickers (green A / red B): prev/next + the live lane name
    print("Port A", 4, SCREEN_H-47, CLR_GREEN);
    d=step_btn(36, SCREEN_H-50, 14, "<", ">"); if (d) selA=(selA+nport+d)%nport;
    print(ports[selA].name, 70, SCREEN_H-47, CLR_GREEN);
    { int ok=(selA!=selB)&&!sand_problem;                  // between A and B: the live movement-validity flag
      print(ok?"-->":"-x>", 102, SCREEN_H-47, ok?CLR_GREEN:CLR_RED); }
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
    snprintf(b,sizeof b,"%d",nlanes); print(b, 228, SCREEN_H-31, CLR_LIGHT_GREY);
    print("skew", 242, SCREEN_H-31, CLR_PINK);                 // angle the crossing road (the trunk)
    d=step_btn(270, SCREEN_H-34, 14, "-", "+"); if (d) skew+=5*d;
    snprintf(b,sizeof b,"%d",skew); print(b, 302, SCREEN_H-31, CLR_LIGHT_GREY);
    // row 3 — cross-section: clothoid toggle, lane taper, flyover elevation
    if (ui_button(4, SCREEN_H-18, 66, 13, use_cloth?"clothoid: on":"clothoid: off")) use_cloth=!use_cloth;
    print("taper", 80, SCREEN_H-15, CLR_BLUE);
    d=step_btn(112, SCREEN_H-18, 14, "-", "+"); if (d) taperPct+=10*d;
    snprintf(b,sizeof b,"%d%%",taperPct); print(b, 144, SCREEN_H-15, CLR_LIGHT_GREY);
    print("lift", 176, SCREEN_H-15, CLR_INDIGO);
    d=step_btn(204, SCREEN_H-18, 14, "-", "+"); if (d) lift+=3*d;
    snprintf(b,sizeof b,"%dpx",lift); print(b, 236, SCREEN_H-15, CLR_LIGHT_GREY);
    // junction view: cycle the junction TYPE (regenerates the table) · sandbox: cycle the ramp primitive
    if (view){ if (ui_button(250, SCREEN_H-18, 66, 13, JT_NAME[juncType])) juncType=(juncType+1)%NJTYPE; }
    else { char pb[16]; snprintf(pb,sizeof pb,"ramp:%s",RP_NAME[sandPrim]);
        if (ui_button(264, SCREEN_H-18, 52, 13, pb)) sandPrim=(sandPrim+1)%3; }
    // clamps (apply to both button + keyboard edits)
    if (radius<6) radius=6;  if (spiral<0) spiral=0;
    if (nlanes<1) nlanes=1;  if (nlanes>6) nlanes=6;
    if (taperPct<0) taperPct=0;  if (taperPct>100) taperPct=100;
    if (lift<0) lift=0;  if (lift>24) lift=24;
    if (skew<-45) skew=-45;  if (skew>45) skew=45;

    ui_end();
    font(FONT_NORMAL);
}
