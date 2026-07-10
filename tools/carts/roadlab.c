/* de:meta
{
  "slug": "roadlab",
  "title": "roadlab",
  "status": "active",
  "created": "2026-06-16",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "procedural-mesh"
  ],
  "lineage": "Successor to `rampkit` (which proved the port-to-ramp abstraction on Beziers); roadlab replaces free Beziers with arc-spline reference lines (LINE→ARC→LINE, the OpenDRIVE model) so that offset lane polygons nest concentrically without a solver.",
  "description": "The FOUNDATION sandbox for the interchange geometry DSL, built on the road-geometry research (docs/design/road-geometry-refs.md). Lane-accurate roads (drive-on-right via a DRIVE constant) with per-lane direction arrows; ports anchored to REAL lanes (a port = a lane + its travel direction); and ramps built as ARC-SPLINES (LINE->ARC->LINE — the OpenDRIVE reference-line model, minus the XML) instead of free Beziers, so they offset and nest cleanly. Supersedes rampkit (which proved the port->ramp abstraction on Beziers). Milestone 1: pick two ports, watch the arc-spline ramp round the corner between their lanes. Controls: LEFT/RIGHT cycle port A, UP/DOWN cycle port B, [ / ] arc radius, F flips DRIVE. Next: clothoid joints (drivable feel) and concentric-arc nesting. See docs/design/interchange-dsl.md."
}
de:meta */
#include "studio.h"
#include "roadkit.h"   // the shared junction grammar: at-grade field (B2/B3) + interchanges (B4)
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
//                flyovers).
//   RING (spec §9): ROUNDABOUT is the PEER construct — not a ramp per movement but ONE circulating
//                carriageway (a closed-circle ref-line through draw_multilane — concentric lanes nest on a
//                circle) the legs tap on/off via merge + diverge slips. draw_roundabout(), separate from the
//                ramp generator. `g` now cycles all 7 — the WHOLE roads.org.uk catalogue. See junction-lanelink §9.
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


// the 4 legs are encoded in the port layout (setup order = in,out per leg) — leg L: 0=W 1=E 2=N 3=S
static int leg_in (int L){ return L*2;   }                  // inbound (entry) port of leg L
static int leg_out(int L){ return L*2+1; }                  // outbound (exit) port of leg L
// GENERATE: enumerate every movement over `nleg` legs → one Connection per served movement, per the policy.
// `lanes` = how many lanes each ramp carries (from the lanes control). Loops get a radius scaled to the lane
// count so the inner edge can't pinch past the centre (a tight loop can't fit a fat ribbon).
// SEAM (Phase 2 → docs/design/road-program-state.md): this pure fn is the interchange drawer a seed-driven
// WORLD calls per grade-separated crossing — (legs, type) in, a drawable connection table out, unchanged.
// make_junction: roadkit (rk_make_junction) owns the topology now — this thin wrapper adapts roadlab's
// leg-present mask + ports so every call site + the spec stay unchanged. (Types/POLICY/splines: roadkit.h.)
static int make_junction(int nleg, JuncType type, int lanes, Junction *out){
    unsigned char pres[NLEG]; for (int L=0;L<NLEG;L++) pres[L]=(unsigned char)legs[L].present;
    return rk_make_junction(nleg, type, lanes, pres, ports, out);
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

// ── RING / ROUNDABOUT (spec §9, the PEER construct): not a ramp per movement but ONE circulating carriageway
//    the legs tap on/off. The ring is a closed-circle reference line fed to draw_multilane (concentric lanes
//    nest for free — the M3 insight on a circle); each present leg gets a MERGE slip (inbound lane → ring,
//    tangential, just before its angle in flow order) and a DIVERGE slip (ring → outbound lane, just after).
//    Circulation sense follows DRIVE. This is OpenDRIVE's junctionGroup type=roundabout, drawn directly. ──
static void draw_roundabout(int lanes){
    if (lanes<1) lanes=1;
    float cx=SCREEN_W/2.0f, cy=SCREEN_H/2.0f;
    float Rring = 22.f + lanes*2.f;                         // grows a little with lane count (island stays open)
    int   circ  = -DRIVE;                                   // circulation sense (drive-on-right); flip if backwards
    float xs[130], ys[130]; int n=0;
    for (int i=0;i<=48;i++){ float a=360.f*i/48.0f; xs[n]=cx+ux(a)*Rring; ys[n++]=cy+uy(a)*Rring; }  // closed circle
    draw_multilane(xs,ys,n, lanes, 0.f, 0.f);               // the circulating carriageway (open centre = the island)
    for (int L=0; L<NLEG; L++){ if(!legs[L].present) continue;
        float b=leg_bearing(L);
        float ea=b - circ*24.f, xa=b + circ*24.f;           // entry just BEFORE the leg angle, exit just AFTER (flow order)
        Port ein  = { cx+ux(ea)*Rring, cy+uy(ea)*Rring, ea + circ*90.f, "ring" };   // dir = ring tangent in flow
        Port eout = { cx+ux(xa)*Rring, cy+uy(xa)*Rring, xa + circ*90.f, "ring" };
        int m = arc_spline(ports[leg_in(L)], ein, 10.f, xs, ys);  draw_multilane(xs,ys,m, 1, 0.f, 0.f);  // MERGE slip
        m     = arc_spline(eout, ports[leg_out(L)], 10.f, xs, ys); draw_multilane(xs,ys,m, 1, 0.f, 0.f);  // DIVERGE slip
    }
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
    if (!is_ring((JuncType)juncType)) make_junction(4, (JuncType)juncType, nlanes, &gen_junc);  // ramp family: GENERATE the table
    if (view){
        // JUNCTION view — the WHOLE junction from the type. RAMP family (M6 + §8.2): the generated connection
        // table. RING family (§9): a circulating carriageway the legs tap on/off (the peer construct).
        if (is_ring((JuncType)juncType)) draw_roundabout(nlanes);
        else draw_junction(&gen_junc, use_cloth, radius, spiral, taperPct/100.f, (float)lift);
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
    if (view && is_ring((JuncType)juncType)){
        snprintf(b,sizeof b,"Junction: %s  (g=type)", JT_NAME[juncType]);
        print(b,4,5,CLR_WHITE);
        print("a PEER construct: legs tap one circulating carriageway", 4,13, CLR_PINK);
        print("(merge on + diverge off) - not a ramp per movement", 4,20, CLR_MEDIUM_GREY);
    } else if (view){
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

// ── spec() — the cart-logic safety net (docs/design/spec-harness.md). roadlab is FEATURE-COMPLETE, so
//    this is a regression lock + a GOLDEN REFERENCE for the Phase-2 port into a seed-driven world: it pins
//    the pure invariants that bakes used to check by eye (and that bit us — the DRIVE chirality, the
//    port-A-entry tangent, the splines landing on their ports). NOT the aesthetics ("drives easily" / "no
//    cusp" / "nests clean") — those stay with the annotated-screenshot method. Run: node tools/spec.js roadlab ──
#ifdef DE_SPEC
#include "spec.h"
static int  sp_count(const Junction*j, RampPrim p){ int c=0; for(int i=0;i<j->nConns;i++) if(j->conns[i].prim==p) c++; return c; }
// sp geometry/edge helpers are cart-specific; the generic spec_near/spec_tap come from spec.h.

void spec(void){
    // ── classify_turn: the DRIVE-folded handedness (the chirality that kept biting) ──
    expect(classify_turn(0,  0) == T_THROUGH, "straight-through: heading unchanged");
    expect(classify_turn(0,180) == T_UTURN,   "reversed heading is a U-turn");
    expect(classify_turn(0, 90) == T_RIGHT,   "drive-on-right: +90 heading change = the easy RIGHT turn");
    expect(classify_turn(0,-90) == T_LEFT,    "drive-on-right: -90 heading change = the hard LEFT turn");

    // ── make_junction: pure generator over (legs, type). 4-leg cross, lay ports, generate. ──
    for (int L=0;L<NLEG;L++) legs[L].present=1;  skew=0;  rebuild_ports();
    Junction j;  make_junction(4, JT_DIAMOND, 2, &j);
    expect_eq(j.nConns, 12, "a 4-way serves 12 movements");
    expect_eq(sp_count(&j,RP_THROUGH), 4, "4-way: 4 through movements (the two mainlines)");
    expect_eq(sp_count(&j,RP_LOOP),    0, "diamond: hard lefts are at-grade direct (no loops)");
    Junction jc; make_junction(4, JT_CLOVERLEAF, 2, &jc);
    expect_eq(jc.nConns, 12, "cloverleaf has the same 12 movements as the diamond...");
    expect_eq(sp_count(&jc,RP_LOOP), 4, "...differing ONLY in the LEFT column: the 4 hard lefts become loops");
    Junction js; make_junction(4, JT_STACK, 2, &js);
    expect_eq(sp_count(&js,RP_FLYOVER), 4, "stack: the 4 hard lefts become flyovers");

    // ── topology: the 3-leg family drops the north arm (leg 2) → a T ──
    expect(!topo_present(JT_TRUMPET,2), "trumpet (3-leg) drops the north arm");
    expect( topo_present(JT_DIAMOND,2), "diamond (4-leg) keeps all arms");
    for (int L=0;L<NLEG;L++) legs[L].present = topo_present(JT_TRUMPET,L);  rebuild_ports();
    Junction jt; make_junction(4, JT_TRUMPET, 2, &jt);
    expect_eq(jt.nConns, 6, "trumpet T: 6 movements among 3 legs");
    expect_eq(sp_count(&jt,RP_LOOP),    1, "trumpet: asymmetric hard lefts — exactly one loop...");
    expect_eq(sp_count(&jt,RP_FLYOVER), 1, "...and one flyover");

    // ── arc_spline (M1): lands on both ports + leaves A along A's travel dir (the port-A-entry fix) ──
    float xs[200], ys[200];
    Port a={100,100,0,"a"}, b={150,150,90,"b"};            // east → south, a right-angle turn
    int n = arc_spline(a,b,20.f,xs,ys);
    expect(n>2, "arc_spline builds a real curve (line-arc-line)");
    expect(spec_near(xs[0],100)&&spec_near(ys[0],100),         "arc_spline starts exactly on port A");
    expect(spec_near(xs[n-1],150)&&spec_near(ys[n-1],150),     "arc_spline ends exactly on port B");
    expect(spec_near(ys[1],100), "arc_spline leaves A along A's travel dir (east) — lead-in points INTO the junction");

    // ── clothoid_spline reduces EXACTLY to arc_spline as Ls→0 ──
    float cxs[200], cys[200];
    int cn = clothoid_spline(a,b,20.f,0.f,cxs,cys);
    expect_eq(cn, n, "clothoid_spline(Ls=0) returns the same sample count as arc_spline");
    expect(spec_near(cxs[cn-1],xs[n-1])&&spec_near(cys[cn-1],ys[n-1]), "clothoid(Ls=0) == arc_spline (reduces exactly)");

    // ── loop_spline lands on B (the cloverleaf hard turn) — using the generated junction's real ports ──
    for (int L=0;L<NLEG;L++) legs[L].present=1;  rebuild_ports();
    make_junction(4, JT_CLOVERLEAF, 2, &jc);
    int li=-1; for(int i=0;i<jc.nConns;i++) if(jc.conns[i].prim==RP_LOOP){ li=i; break; }
    expect(li>=0, "the cloverleaf has a loop connection to test");
    if (li>=0){ Connection c=jc.conns[li];
        int ln = loop_spline(ports[c.inPort], ports[c.outPort], c.R, xs, ys);
        expect(ln>2, "loop_spline builds a real ~270deg loop (not the degenerate stand-in)");
        expect(spec_near(xs[ln-1],ports[c.outPort].x)&&spec_near(ys[ln-1],ports[c.outPort].y), "loop_spline lands on port B"); }

    // ── the harness drives roadlab too: 'g' cycles the junction type (proves step() + key injection) ──
    int g0=juncType; spec_tap('g'); expect(juncType!=g0, "the 'g' key cycles the junction type");
}
#endif
