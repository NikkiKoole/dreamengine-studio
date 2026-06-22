#include "studio.h"
#include "ui.h"        // on-screen buttons (per-finger capture, fat-finger hit pads) — never hand-roll
#include <stdio.h>
#include <math.h>

// streetlab — the FOUNDATION cart for the AT-GRADE world, one tier BELOW roadlab. roadlab draws the
// grade-separated INTERCHANGE family (loop/flyover ramps that ELIMINATE the crossing); streetlab draws
// the streets that cross AT GRADE — a fundamentally different geometry grammar (curb returns, small radii,
// turn lanes, channelization) flagged as unmodeled in docs/design/road-hierarchy-notes.md §2. The seam
// between the two tiers is "access control" (§1): freeway = ramp-only = roadlab; everything below = at
// grade = streetlab. The sibling framing is exact — both are sandboxes that prove a grammar before a
// seed-driven world consumes them.
//   MILESTONE 1: a perpendicular 4-way at-grade crossing with CURB-RETURN corner fillets. The headline
//                at-grade primitive: where two carriageways meet, the curb is rounded by an arc TANGENT to
//                both curb lines (radius R kept SMALL — it slows turns + shortens the pedestrian crossing,
//                the opposite goal of a high-speed ramp). curb_return() is the closed-form tangent-arc
//                construction (pure fn of K, the two edge directions, R) — and the first spec() target.
//   MILESTONE 2: SKEW + the T (3-leg) family. Like roadlab's leg layer: a junction is a list of LEGS, each
//                an arm at a `bearing`; the arms are laid out from the legs, NOTHING hardcoded to 90°. The
//                curb-return grammar was already angle-agnostic (it takes arbitrary edge directions), so it
//                re-solves for free — only the STAGE generalised (rotated bands; corners = where adjacent
//                inner edges cross). `skew` tilts the crossing street; `T` drops the north arm → 3 legs, 2
//                corners (the straight "back" of the through road has no corner). count_corners() is pure.
//   MILESTONE 3: TURN LANES + channelizing islands. A left-turn lane is marked INSIDE the carriageway
//                (no outer widening — that detached into tabs under skew): a raised MEDIAN splitter on the
//                centreline + a solid delineation of the inner inbound lane + a left-turn arrow. All in the
//                arm's local frame ⇒ skew-safe like the curb returns.
//   MILESTONE 4: the street WEB (network topology — road-hierarchy-notes §8). A second VIEW ('v'): a GRAPH
//                of intersections (nodes) + street segments (edges) generated deterministically from a seed.
//                gen_network() is a PURE fn of (pattern, seed) — the spec asserts on the graph (node/edge
//                counts, mean degree, dead-ends = the SNDi measures §8.2). Four patterns, and the metrics
//                SEPARATE them (the §8.2 point): grid/organic (degree ~3.4, 0 dead-ends) · radial (a hub +
//                rings, degree ~3.9) · cul-de-sac (a random spanning tree + sparse loops ⇒ dendritic, degree
//                ~2.2, many dead-ends). Next: the §8.4 two-tier major→minor generator, then a seed-driven
//                WORLD that emits (pattern, region) per place.
//   MILESTONE 5: the PEDESTRIAN layer (§2 — the reason at-grade exists: small radii are "to shorten the ped
//                crossing"). 'k' toggles SIDEWALKS (the whole junction footprint inflated by SW, drawn under
//                the road ⇒ an SW-wide kerbside ring that wraps the corners for free) + ZEBRA CROSSWALKS at
//                each mouth (stripes along travel, stop bar set back behind them). Composes with skew/T.
//   MILESTONE 6: the MINI-ROUNDABOUT (§2) — the at-grade counterpart of roadlab's grade-separated ring. 'r'
//                lays a circulatory disc over the centre with a MINI + TRAVERSABLE (mountable/domed) central
//                island, flush SPLITTER islands on each approach, GIVE-WAY (yield) entry lines (not stop
//                bars) + CCW circulating arrows. Built on the same leg model ⇒ any leg count / skew for free;
//                mutually exclusive with turn lanes (both are "treatments" of the crossing). island radius is
//                the headline knob — it reuses the [ ] slot in this mode. round_icr() (island + circulatory)
//                is pure ⇒ spec'd: the island sits strictly inside the inscribed circle and stays "mini".
//   MILESTONE 7: the typed CROSS-SECTION (OpenDRIVE lane types, §5). The carriageway stops being uniform
//                asphalt: a lane-section from the centre out — [median] · driving×N · [bike] · [parking] —
//                toggled per element (m/b/;). The MEDIAN is itself a lane type (the same island primitive as
//                M3's splitter / M6's central island, now run continuously down the centre). The key trick:
//                the half-width HW = cross_hw() = the SUM of the lanes present, and EVERY junction primitive
//                (curb returns, the mouth datum, the roundabout, sidewalks) keys off HW ⇒ widening the
//                section re-solves the whole junction for free. cross_hw() is the pure, spec'd datum.
//   Controls: ui.h toolbar (clickable; keyboard too). v = junction↔network view. JUNCTION: [ ] curb radius
//             (island R in roundabout mode) · -/= lanes · ,/. skew · t = T · p = turn lanes · r = roundabout ·
//             k = sidewalks+crosswalks · m = median · b = bike lane · ; = parking lane. NETWORK: [ ] seed ·
//             b = pattern · c = curve (M4c: the §8.5 curvature knob bows each edge ⇒ sinuosity goes live).

#define LANEW    8     // one lane width (px)
#define TOOLBAR  62    // bottom control strip height (three rows; M7 added the cross-section row)
#define POCKET   22    // M3: turn-bay length at the stop bar
#define PTAPER   14    // M3: upstream taper of the turn bay
#define MEDW     2     // M3: raised median splitter half-width
#define SW       4     // M5: sidewalk width (the strip outside each kerb)
#define CWDEP    6     // M5: crosswalk depth (how far the zebra band reaches out from the box)
#define NETTOP   28    // M4: network-view top edge (leaves room for 3 header lines: title/metrics/degree mix)
#define BIKEW    5     // M7: bike-lane width (a tinted lane outside the driving lanes)
#define PARKW    7     // M7: parking-lane width (at the kerb, marked with bay ticks)
#define MEDHW    4     // M7: continuous centre-median half-width (the median IS a cross-section lane type)
#define DEG2RAD  0.01745329252f
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float ux(float d){ return cos_deg(d); }
static float uy(float d){ return sin_deg(d); }

// ── CURB RETURN = the at-grade corner primitive. Two curb edges leave the corner K in directions e1,e2
//    (degrees, pointing AWAY from K along each curb). The return is the arc of radius R TANGENT to both
//    edges; its centre O sits on the bisector of (e1,e2) on the SIDEWALK side (the curbs point outward up
//    the two streets, so their sum points away from the intersection = toward the block corner). Closed
//    form: tangent points at distance R/tan(half) along each edge, centre at R/sin(half) along the
//    bisector. PURE fn — the geometry the spec pins (tangency: dist(O, each edge line) == R). ──
typedef struct { float ox, oy, t1x, t1y, t2x, t2y; } CurbReturn;
static CurbReturn curb_return(float kx, float ky, float e1, float e2, float R){
    float d1x=ux(e1), d1y=uy(e1), d2x=ux(e2), d2y=uy(e2);
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

// fill the fillet: the curvy triangle bounded by the two curb edges (meeting at the SHARP corner K) and
// the arc. PAVEMENT sits on the K side of the arc (rounding the re-entrant corner inward); the block keeps
// its square outer shape. Apex = K (NOT the arc centre O — that fills the block side, the inverted bug).
static void fill_corner(float kx, float ky, CurbReturn c, float R, int col){
    float a0 = atan2f(c.t1y-c.oy, c.t1x-c.ox);
    float a1 = atan2f(c.t2y-c.oy, c.t2x-c.ox);
    float d  = a1-a0; while(d> M_PI)d-=2*M_PI; while(d<-M_PI)d+=2*M_PI;   // shorter sweep (the ≤90° arc)
    enum { N=10 };
    int xy[2*(N+2)]; int k=0;
    xy[k++]=(int)kx; xy[k++]=(int)ky;                      // apex = the sharp corner (pavement side)
    for (int i=0;i<=N;i++){ float a=a0+d*i/N; xy[k++]=(int)(c.ox+cosf(a)*R); xy[k++]=(int)(c.oy+sinf(a)*R); }
    polyfill(xy, N+2, col);
}
// stroke just the curb arc (the rounded kerb line)
static void stroke_corner(CurbReturn c, float R, int col){
    float a0 = atan2f(c.t1y-c.oy, c.t1x-c.ox);
    float a1 = atan2f(c.t2y-c.oy, c.t2x-c.ox);
    float d  = a1-a0; while(d> M_PI)d-=2*M_PI; while(d<-M_PI)d+=2*M_PI;
    enum { N=10 }; float px=c.t1x, py=c.t1y;
    for (int i=1;i<=N;i++){ float a=a0+d*i/N, x=c.ox+cosf(a)*R, y=c.oy+sinf(a)*R;
        line((int)px,(int)py,(int)x,(int)y,col); px=x; py=y; }
}
static void dashed(float x0,float y0,float x1,float y1,int col){       // a dashed lane/centre line
    float dx=x1-x0, dy=y1-y0, L=sqrtf(dx*dx+dy*dy); if(L<1)return; dx/=L; dy/=L;
    for (float t=0;t<L;t+=8){ float a=t, b=t+4>L?L:t+4;
        line((int)(x0+dx*a),(int)(y0+dy*a),(int)(x0+dx*b),(int)(y0+dy*b),col); }
}

// ── LEG model (mirrors roadlab): a junction is a list of arms, each at a bearing. The two arms of a street
//    are collinear (b and b+180). `crossing` marks the street that SKEWS; T drops the north arm. ──
#define NLEG 4
typedef struct { float base; int crossing; } Leg;          // base bearing (deg); crossing = part of skewable street
static Leg legs[NLEG] = {
    {   0, 0 },   // 0 East  (through street)
    {  90, 1 },   // 1 South (crossing street — skews)
    { 180, 0 },   // 2 West  (through street)
    { 270, 1 },   // 3 North (crossing street — skews)  ← dropped for a T
};
static float cornerR = 8.f;   // curb-return radius (the headline at-grade knob)
static int   lanesPer = 2;    // lanes PER DIRECTION (street width = 2 * lanesPer)
static int   skew     = 0;    // degrees added to the crossing street (the N–S pair)
static int   isT      = 0;    // drop the north arm → a T-junction
static int   turnLanes= 0;    // M3: per-approach left-turn bay + raised median splitter
static int   peds     = 0;    // M5: sidewalks (a strip outside each kerb) + zebra crosswalks at the mouths
static int   roundabout=0;    // M6: a MINI-ROUNDABOUT (traversable island + give-way entries) — excl. turnLanes
static float islandR  = 8.f;  // M6: central-island radius (the headline roundabout knob; reuses [ ] in this mode)
static int   medOn    = 0;    // M7: continuous raised centre median (a cross-section lane type, not the turn splitter)
static int   bikeOn   = 0;    // M7: a bike lane outside the driving lanes
static int   parkOn   = 0;    // M7: a parking lane at the kerb
enum { PAT_GRID, PAT_ORGANIC, PAT_RADIAL, PAT_CULDESAC, NPAT };   // M4: street-web patterns
static int   netview  = 0;    // M4: 0 = junction detail (M1–M3), 1 = the street-web network
static int   pattern  = PAT_GRID;
static int   netseed  = 1;
static int   curveAmt = 0;    // M4c: 0 = straight chords … 3 = winding (the §8.5 curvature knob; bows each edge)

static float leg_bearing(int i){ float b=legs[i].base + (legs[i].crossing?(float)skew:0.f); return fmodf(b+3600,360); }
static int   leg_present(int i){ return !(isT && i==3); }              // T drops North (leg 3)

// present legs, sorted by bearing 0..360; fills idx[]/brg[], returns n
static int present_legs(int *idx, float *brg){
    int n=0;
    for (int i=0;i<NLEG;i++) if (leg_present(i)){ idx[n]=i; brg[n]=leg_bearing(i); n++; }
    for (int a=0;a<n;a++) for (int b=a+1;b<n;b++) if (brg[b]<brg[a]){
        float tb=brg[a]; brg[a]=brg[b]; brg[b]=tb; int ti=idx[a]; idx[a]=idx[b]; idx[b]=ti; }
    return n;
}
// number of CONVEX corners = adjacent gaps strictly between 0 and 180 (a 180° gap is the straight T back). PURE.
static int count_corners(void){
    int idx[NLEG]; float brg[NLEG]; int n=present_legs(idx,brg);
    if (n<2) return 0;
    int c=0;
    for (int i=0;i<n;i++){ float g=fmodf(brg[(i+1)%n]-brg[i]+3600,360); if (g>0.5f && g<179.5f) c++; }
    return c;
}
// corner K = where the two band edges FACING the gap between arms A,B cross (each offset HW toward bisector bm)
static void edge_corner(float cx,float cy,float HW,float bA,float bB,float bm,float*kx,float*ky){
    float dAx=ux(bA),dAy=uy(bA), nAx=ux(bA+90),nAy=uy(bA+90);
    float sA = (nAx*ux(bm)+nAy*uy(bm))>0 ? 1.f : -1.f;     // pick the edge on the gap (grass) side
    float PAx=cx+sA*HW*nAx, PAy=cy+sA*HW*nAy;
    float dBx=ux(bB),dBy=uy(bB), nBx=ux(bB+90),nBy=uy(bB+90);
    float sB = (nBx*ux(bm)+nBy*uy(bm))>0 ? 1.f : -1.f;
    float PBx=cx+sB*HW*nBx, PBy=cy+sB*HW*nBy;
    float det = dAx*(-dBy) - (-dBx)*dAy;                   // solve PA + t·DA = PB + u·DB
    if (fabsf(det)<1e-4f){ *kx=(PAx+PBx)*0.5f; *ky=(PAy+PBy)*0.5f; return; }
    float t = ((PBx-PAx)*(-dBy) - (-dBx)*(PBy-PAy)) / det;
    *kx=PAx+dAx*t; *ky=PAy+dAy*t;
}
// ── M7: the typed CROSS-SECTION (OpenDRIVE lane types §5). A lane-section, centre→out: [median] · driving×N ·
//    [bike] · [parking] · (sidewalk = the M5 peds layer). The carriageway half-width is the SUM of the lanes
//    present — and because every junction primitive (curb returns, the mouth datum, the roundabout, sidewalks)
//    keys off HW, widening the section re-solves the whole junction for free. cross_hw() is the pure datum. ──
static float cross_hw(void){
    return (medOn?MEDHW:0) + lanesPer*LANEW + (bikeOn?BIKEW:0) + (parkOn?PARKW:0);
}
// a solid lane line along the arm (offset o from centre, on side s = +1/-1), from startd to reach
static void edge_line(float cx,float cy,float b,float o,int s,float startd,float reach,int col){
    float dx=ux(b),dy=uy(b), nx=ux(b+90)*s*o, ny=uy(b+90)*s*o;
    line((int)(cx+dx*startd+nx),(int)(cy+dy*startd+ny),(int)(cx+dx*reach+nx),(int)(cy+dy*reach+ny),col);
}
// fill the band [a,c] from centre on side s along the arm (a tinted lane surface)
static void lane_band(float cx,float cy,float b,float a,float c,int s,float startd,float reach,int col){
    float dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90);
    float x0=cx+dx*startd, y0=cy+dy*startd, x1=cx+dx*reach, y1=cy+dy*reach;
    int q[8]={(int)(x0+nx*s*a),(int)(y0+ny*s*a),(int)(x0+nx*s*c),(int)(y0+ny*s*c),
              (int)(x1+nx*s*c),(int)(y1+ny*s*c),(int)(x1+nx*s*a),(int)(y1+ny*s*a)};
    polyfill(q,4,col);
}
// markings along one arm: the typed cross-section (centre line / median, lane dividers, bike + parking lanes)
static void cross_markings(float cx,float cy,float b,float startd,float reach){
    float dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90);
    float x0=cx+dx*startd, y0=cy+dy*startd, x1=cx+dx*reach, y1=cy+dy*reach;
    float inner = medOn?MEDHW:0;                            // driving lanes start here (outside the median)
    if (medOn){                                             // a raised centre median (light-grey + kerbs)
        lane_band(cx,cy,b, 0, MEDHW, +1, startd,reach, CLR_LIGHT_GREY);
        lane_band(cx,cy,b, 0, MEDHW, -1, startd,reach, CLR_LIGHT_GREY);
        edge_line(cx,cy,b, MEDHW,+1, startd,reach, CLR_BROWNISH_BLACK);
        edge_line(cx,cy,b, MEDHW,-1, startd,reach, CLR_BROWNISH_BLACK);
    } else {
        dashed(x0,y0,x1,y1,CLR_YELLOW);                     // the centreline (no median)
    }
    for (int k=1;k<lanesPer;k++){                           // dashed driving-lane dividers
        float o=inner+k*LANEW;
        dashed(x0+nx*o,y0+ny*o, x1+nx*o,y1+ny*o, CLR_WHITE);
        dashed(x0-nx*o,y0-ny*o, x1-nx*o,y1-ny*o, CLR_WHITE);
    }
    float edge = inner + lanesPer*LANEW;
    if (bikeOn){                                            // bike lane: tinted surface + a solid inner line
        lane_band(cx,cy,b, edge, edge+BIKEW, +1, startd,reach, CLR_BROWN);
        lane_band(cx,cy,b, edge, edge+BIKEW, -1, startd,reach, CLR_BROWN);
        edge_line(cx,cy,b, edge,+1, startd,reach, CLR_WHITE);
        edge_line(cx,cy,b, edge,-1, startd,reach, CLR_WHITE);
        edge += BIKEW;
    }
    if (parkOn){                                            // parking lane: a solid inner line + bay ticks
        edge_line(cx,cy,b, edge,+1, startd,reach, CLR_WHITE);
        edge_line(cx,cy,b, edge,-1, startd,reach, CLR_WHITE);
        float L=sqrtf((x1-x0)*(x1-x0)+(y1-y0)*(y1-y0));
        for (float t=14; t<L; t+=16)                        // bay-delimiter ticks across the parking lane
            for (int s=-1;s<=1;s+=2){
                float px=cx+dx*(startd+t), py=cy+dy*(startd+t);
                line((int)(px+nx*s*edge),(int)(py+ny*s*edge),
                     (int)(px+nx*s*(edge+PARKW)),(int)(py+ny*s*(edge+PARKW)),CLR_WHITE);
            }
    }
}

// ── M3: turn lanes + channelizing islands ──
// arm_face = axial distance from the hub to where this arm's curb returns sit (its "mouth"). It's the
// largest corner distance over the arm's two convex gaps (an acute gap reaches further down the arm). PURE.
static float arm_face(const float *brg,int n,int i,float HW){
    float gP=fmodf(brg[i]-brg[(i-1+n)%n]+3600,360), gN=fmodf(brg[(i+1)%n]-brg[i]+3600,360);
    float m=0;
    if (gP>0.5f&&gP<179.5f){ float d=HW/tanf(gP*0.5f*DEG2RAD); if(d>m)m=d; }
    if (gN>0.5f&&gN<179.5f){ float d=HW/tanf(gN*0.5f*DEG2RAD); if(d>m)m=d; }
    return m;
}
// LEFT-TURN ARROW: shaft pointing into the junction (inbound) with a head hooking LEFT (across the
// centreline) — the glyph that marks a lane as turn-only. Lives in the arm's local frame ⇒ skew-safe.
static void turn_arrow(float cx,float cy,float b,float df,int col){
    float ax=ux(b),ay=uy(b), ix=ux(b-90),iy=uy(b-90);     // axis + inbound-side normal
    float bx=cx+ax*(df+9)+ix*(LANEW*0.5f), by=cy+ay*(df+9)+iy*(LANEW*0.5f);   // base, in the inner inbound lane
    float tx=cx+ax*(df+2)+ix*(LANEW*0.5f), ty=cy+ay*(df+2)+iy*(LANEW*0.5f);   // tip, nearer the stop bar
    line((int)bx,(int)by,(int)tx,(int)ty,col);                                // shaft (inbound)
    float hx=tx+ux(b+90)*3, hy=ty+uy(b+90)*3;                                 // hook LEFT across the centreline
    line((int)tx,(int)ty,(int)hx,(int)hy,col);
    line((int)hx,(int)hy,(int)(hx+ux(b-30)*3),(int)(hy+uy(b-30)*3),col);      // arrowhead on the hook
    line((int)hx,(int)hy,(int)(hx+ux(b+150)*3),(int)(hy+uy(b+150)*3),col);
}
// RAISED MEDIAN SPLITTER (the channelizing island): a thin island on the centreline from df out to
// df+POCKET+PTAPER with a rounded nose upstream — separates opposing traffic and frames the left-turn bay.
static void draw_median(float cx,float cy,float b,float df){
    float ax=ux(b),ay=uy(b), nx=ux(b+90),ny=uy(b+90);
    float s0=df+2, s1=df+POCKET+PTAPER;
    int q[8]={ (int)(cx+ax*s0+nx*MEDW),(int)(cy+ay*s0+ny*MEDW),
               (int)(cx+ax*s0-nx*MEDW),(int)(cy+ay*s0-ny*MEDW),
               (int)(cx+ax*s1-nx*MEDW),(int)(cy+ay*s1-ny*MEDW),
               (int)(cx+ax*s1+nx*MEDW),(int)(cy+ay*s1+ny*MEDW) };
    polyfill(q,4,CLR_LIGHT_GREY);
    circfill((int)(cx+ax*s1),(int)(cy+ay*s1),MEDW,CLR_LIGHT_GREY);   // rounded nose
    line(q[0],q[1],q[6],q[7],CLR_BROWNISH_BLACK);          // kerb edges
    line(q[2],q[3],q[4],q[5],CLR_BROWNISH_BLACK);
}
// ── M5: a ZEBRA CROSSWALK across one approach at the junction mouth (df). Stripes run ALONG the travel
//    direction (cars drive over them lengthwise), spaced across the full carriageway width. The whole-street
//    sidewalks are drawn as the footprint inflated by SW (in draw()); this is the per-approach crossing. ──
static void draw_crosswalk(float cx,float cy,float b,float HW,float df){
    float ax=ux(b),ay=uy(b), nx=ux(b+90),ny=uy(b+90);
    float s0=df+1, s1=df+1+CWDEP;
    for (float o=-HW+1.5f; o<HW; o+=3.f)                    // a stripe per ~3px across the width
        line((int)(cx+ax*s0+nx*o),(int)(cy+ay*s0+ny*o),(int)(cx+ax*s1+nx*o),(int)(cy+ay*s1+ny*o),CLR_WHITE);
}

// ── M6: the MINI-ROUNDABOUT (at-grade, §2) — distinct from roadlab's GRADE-SEPARATED ring. The at-grade
//    tells: the central island is MINI + TRAVERSABLE (mountable/domed — long vehicles drive straight OVER
//    it), the splitters are flush, and entries GIVE WAY (yield), they don't stop. Built on the same leg
//    model ⇒ any leg count / skew for free. Circulatory width = one direction's HW; the inscribed circle =
//    island + circulatory. island radius is the headline knob (reuses the [ ] slot in this mode). PURE: ──
static float round_circ_w(void){ return lanesPer*LANEW; }        // circulatory carriageway width (one way round)
static float round_icr(void){ return islandR + round_circ_w(); } // inscribed-circle radius (island + ring)

// the traversable central ISLAND: a low domed/painted disc — the mini tell. Apron (mountable) + white dome.
static void draw_island(float cx,float cy){
    int r=(int)islandR;
    circfill((int)cx,(int)cy,r+1,CLR_BROWNISH_BLACK);            // thin kerb ring (low — you can mount it)
    circfill((int)cx,(int)cy,r,  CLR_LIGHT_GREY);               // mountable apron
    circfill((int)cx,(int)cy,(int)(islandR*0.55f),CLR_WHITE);    // painted dome
}
// circulating ARROWS on the ring midline. Drive-on-right ⇒ keep the island on the LEFT ⇒ travel visually
// COUNTERCLOCKWISE; with screen y pointing down that's the a-90 tangent. Each = a little shaft + arrowhead.
static void draw_circulation(float cx,float cy){
    float rm = islandR + round_circ_w()*0.5f;
    for (int k=0;k<3;k++){
        float a = 60 + k*120;                                    // 3 arrows, between the arms
        float px=cx+ux(a)*rm, py=cy+uy(a)*rm, t=a-90;            // CCW tangent (visual, y-down)
        float bx=px-ux(t)*2, by=py-uy(t)*2, hx=px+ux(t)*3, hy=py+uy(t)*3;   // base → head
        line((int)bx,(int)by,(int)hx,(int)hy,CLR_YELLOW);                                   // shaft
        line((int)hx,(int)hy,(int)(hx+ux(t+145)*3),(int)(hy+uy(t+145)*3),CLR_YELLOW);        // barb
        line((int)hx,(int)hy,(int)(hx+ux(t-145)*3),(int)(hy+uy(t-145)*3),CLR_YELLOW);        // barb
    }
}
// a GIVE-WAY (yield) line across the inbound half of one approach at `base` — dashes, NOT a solid stop bar.
static void give_way(float cx,float cy,float b,float base,float hw){
    float dx=ux(b),dy=uy(b), ix=ux(b-90),iy=uy(b-90);
    float gx=cx+dx*base, gy=cy+dy*base;
    for (float o=1.5f; o<hw; o+=4.f)                            // transverse dashes (the give-way look)
        line((int)(gx+ix*o),(int)(gy+iy*o),(int)(gx+ix*(o+2)),(int)(gy+iy*(o+2)),CLR_WHITE);
}
// a SPLITTER island: a teardrop on the approach centreline — widest at the circulatory (`inner`), tapering
// to a point `len` upstream. Deflects entering traffic + separates entry from exit; doubles as a ped refuge.
static void draw_splitter(float cx,float cy,float b,float inner,float len){
    float ax=ux(b),ay=uy(b), nx=ux(b+90),ny=uy(b+90); float W=3.f;
    float s0=inner, s1=inner+len;
    int t[6]={ (int)(cx+ax*s0+nx*W),(int)(cy+ay*s0+ny*W),
               (int)(cx+ax*s0-nx*W),(int)(cy+ay*s0-ny*W),
               (int)(cx+ax*s1),     (int)(cy+ay*s1)     };     // point, upstream
    polyfill(t,3,CLR_LIGHT_GREY);
    line(t[0],t[1],t[4],t[5],CLR_BROWNISH_BLACK);              // kerb edges
    line(t[2],t[3],t[4],t[5],CLR_BROWNISH_BLACK);
}

void update(void){
    if (keyp('v')||keyp('V')) netview = !netview;            // M4: junction-detail ↔ street-web view
    if (netview){
        if (keyp('[')) { netseed--; if(netseed<0) netseed=0; }
        if (keyp(']')) netseed++;
        if (keyp('b')||keyp('B')) pattern=(pattern+1)%NPAT;
        if (keyp('c')||keyp('C')) curveAmt=(curveAmt+1)%4;   // M4c: cycle the winding knob 0..3
        return;
    }
    if (keyp('['))  { if (roundabout) islandR -= 2; else cornerR -= 2; }   // M6: [ ] = island R in roundabout mode
    if (keyp(']'))  { if (roundabout) islandR += 2; else cornerR += 2; }
    if (keyp('-'))  lanesPer--;
    if (keyp('='))  lanesPer++;
    if (keyp(','))  skew -= 5;
    if (keyp('.'))  skew += 5;
    if (keyp('t')||keyp('T')) isT = !isT;
    if (keyp('p')||keyp('P')) { turnLanes = !turnLanes; if (turnLanes) roundabout=0; }   // excl. roundabout
    if (keyp('r')||keyp('R')) { roundabout = !roundabout; if (roundabout) turnLanes=0; } // M6: excl. turn lanes
    if (keyp('k')||keyp('K')) peds = !peds;                  // M5: sidewalks + crosswalks
    if (keyp('m')||keyp('M')) medOn  = !medOn;               // M7: cross-section — centre median
    if (keyp('b')||keyp('B')) bikeOn = !bikeOn;              // M7: bike lane
    if (keyp(';'))            parkOn = !parkOn;              // M7: parking lane
    if (cornerR<0) cornerR=0;  if (cornerR>28) cornerR=28;
    if (islandR<3) islandR=3;  if (islandR>20) islandR=20;   // M6: stays MINI (small, traversable)
    if (lanesPer<1) lanesPer=1; if (lanesPer>3) lanesPer=3;
    if (skew<-60) skew=-60;    if (skew>60) skew=60;
#ifdef DE_TRACE
    watch("cornerR","%.1f",cornerR); watch("skew","%d",skew); watch("corners","%d",count_corners());
#endif
}

// step_btn: a "-/+" stepper as two ui buttons; returns -1, 0 or +1 (borrowed from roadlab's chassis)
static int step_btn(int x,int y,int w,const char*lm,const char*rm){
    int d=0;
    if (ui_button(x,     y, w, 13, lm)) d=-1;
    if (ui_button(x+w+1, y, w, 13, rm)) d=+1;
    return d;
}

// ── M4: the street WEB (network topology) ── one tier ABOVE the single junction: a GRAPH of intersections
//    (nodes) + street segments (edges), generated deterministically from a seed. The canonical planning
//    patterns (grid / organic / radial / cul-de-sac — road-hierarchy-notes §8) are real archetypes; here
//    they're a morph over one generator. gen_network() is a PURE fn of (pattern, seed) — the spec asserts
//    on the graph directly (node/edge counts, mean degree, dead-ends — the SNDi measures, §8.2). M4a does
//    grid + organic (a jitter morph on one lattice); radial + cul-de-sac (real topology changes) are M4b.
#define GW 9                       // node columns
#define GH 6                       // node rows
#define R_RINGS   5                // radial: concentric rings
#define R_SECTORS 8                // radial: spokes
#define MAXNODE (GW*GH)
#define MAXEDGE (MAXNODE*3)
static const char* PAT_NAME[NPAT] = { "grid", "organic", "radial", "cul-de-sac" };
typedef struct { float x,y; } NetNode;
typedef struct { int a,b; } NetEdge;
static NetNode nnodes[MAXNODE]; static NetEdge nedges[MAXEDGE];
static int nn=0, ne=0;

// a hash → [0,1): deterministic noise from integer coords + seed (NOT engine rnd, so it's frame-independent
// and spec-stable — the network is a pure function of (pattern, seed), reproducible anywhere).
static float hash01(int x,int y,int s){
    unsigned int h=(unsigned)(x*374761393 + y*668265263 + s*1274126177 + 0x9e3779b9);
    h=(h^(h>>15))*2246822519u; h=(h^(h>>13))*3266489917u; h^=h>>16;
    return (h & 0xffffff) / (float)0x1000000;
}
static int uf[MAXNODE];
static int ufind(int x){ while(uf[x]!=x){ uf[x]=uf[uf[x]]; x=uf[x]; } return x; }

// RADIAL: a hub + concentric rings of sectors, joined by spokes (hub→ring0, then ring→ring) and ring
// cycles. The hub is the high-degree node (degree = R_SECTORS) — a radial-concentric signature.
static void gen_radial(int seed){
    float cx=SCREEN_W/2.f, cy=(NETTOP + (SCREEN_H-TOOLBAR-10))/2.f;
    float maxR=((SCREEN_H-TOOLBAR-10)-NETTOP)/2.f - 2;
    nnodes[nn++]=(NetNode){ cx, cy };                                  // node 0 = hub
    int ring0=1;
    for (int r=0;r<R_RINGS;r++){ float rad=maxR*(r+1)/R_RINGS;
        for (int s=0;s<R_SECTORS;s++){ float a=360.f*s/R_SECTORS + (hash01(r,s,seed)-0.5f)*8.f;
            nnodes[nn++]=(NetNode){ cx+ux(a)*rad, cy+uy(a)*rad }; }
    }
    for (int s=0;s<R_SECTORS;s++) nedges[ne++]=(NetEdge){ 0, ring0+s };                       // hub spokes
    for (int r=0;r<R_RINGS-1;r++) for (int s=0;s<R_SECTORS;s++)                                // inter-ring spokes
        nedges[ne++]=(NetEdge){ ring0+r*R_SECTORS+s, ring0+(r+1)*R_SECTORS+s };
    for (int r=0;r<R_RINGS;r++) for (int s=0;s<R_SECTORS;s++)                                  // ring cycles
        nedges[ne++]=(NetEdge){ ring0+r*R_SECTORS+s, ring0+r*R_SECTORS+(s+1)%R_SECTORS };
}
// CUL-DE-SAC / dendritic: a random SPANNING TREE over the grid (connected + tree-like ⇒ many dead-ends)
// plus a sparse fraction of extra edges (loops) — the loops-and-lollipops pattern. Kruskal over hash weights.
static void gen_culdesac(int id[GH][GW], int seed){
    NetEdge cand[MAXEDGE]; float wt[MAXEDGE]; int nc=0;
    for (int r=0;r<GH;r++) for (int c=0;c<GW;c++){
        if (c+1<GW){ cand[nc]=(NetEdge){id[r][c],id[r][c+1]}; wt[nc]=hash01(c,r,seed);     nc++; }
        if (r+1<GH){ cand[nc]=(NetEdge){id[r][c],id[r+1][c]}; wt[nc]=hash01(c,r,seed+555); nc++; }
    }
    int ord[MAXEDGE]; for(int i=0;i<nc;i++) ord[i]=i;                  // order candidates by weight (selection sort, small n)
    for(int i=0;i<nc;i++){ int m=i; for(int j=i+1;j<nc;j++) if(wt[ord[j]]<wt[ord[m]]) m=j;
        int t=ord[i]; ord[i]=ord[m]; ord[m]=t; }
    for(int i=0;i<nn;i++) uf[i]=i;
    for(int i=0;i<nc;i++){ NetEdge e=cand[ord[i]]; int ra=ufind(e.a), rb=ufind(e.b);
        if (ra!=rb){ uf[ra]=rb; nedges[ne++]=e; }                                            // tree edge (connectivity)
        else if (hash01(e.a,e.b,seed+999) < 0.12f) nedges[ne++]=e;                           // a sparse loop
    }
}
// SEAM (Phase 2 → docs/design/road-program-state.md): gen_network() is the PER-REGION generator a two-tier
// (major→minor, §8.4) world calls — it fills one region's local streets given a (pattern, seed). The world
// owns the major arterials + the region partition and stitches these graphs at the boundaries. Each node is
// a crossing the JUNCTION layer (M1–M3) can render in detail — the network→junction zoom.
static void gen_network(int pat,int seed){
    nn=0; ne=0;
    if (pat==PAT_RADIAL){ gen_radial(seed); return; }
    float x0=18, y0=NETTOP, x1=SCREEN_W-18, y1=SCREEN_H-TOOLBAR-10;
    float cw=(x1-x0)/(GW-1), ch=(y1-y0)/(GH-1);
    float jit = (pat==PAT_ORGANIC) ? 0.42f : (pat==PAT_CULDESAC ? 0.22f : 0.f);
    int id[GH][GW];
    for (int r=0;r<GH;r++) for (int c=0;c<GW;c++){
        float jx=(hash01(c,r,seed)    -0.5f)*cw*jit;
        float jy=(hash01(c,r,seed+97) -0.5f)*ch*jit;
        id[r][c]=nn; nnodes[nn++]=(NetNode){ x0+c*cw+jx, y0+r*ch+jy };
    }
    if (pat==PAT_CULDESAC){ gen_culdesac(id, seed); return; }
    for (int r=0;r<GH;r++) for (int c=0;c<GW;c++){          // grid / organic: full grid edges
        if (c+1<GW) nedges[ne++]=(NetEdge){ id[r][c], id[r][c+1] };
        if (r+1<GH) nedges[ne++]=(NetEdge){ id[r][c], id[r+1][c] };
    }
}
static int   node_degree(int i){ int d=0; for(int e=0;e<ne;e++) if(nedges[e].a==i||nedges[e].b==i) d++; return d; }
static float mean_degree(void){ if(!nn)return 0; int s=0; for(int i=0;i<nn;i++) s+=node_degree(i); return (float)s/nn; }
static int   dead_ends(void){ int c=0; for(int i=0;i<nn;i++) if(node_degree(i)==1) c++; return c; }
// DEGREE DISTRIBUTION — §8.2's real discriminator: "mean degree ALONE is not enough" (a sparse grid and a
// dense dendritic tree can read the same mean). The SHARE of nodes by degree separates them — cul-de-sac(1),
// T/Y(3), X-crossroads(4+) in Marshall's node taxonomy. PURE over the graph; the network-view readout + spec.
static int   deg_count(int d){    int c=0; for(int i=0;i<nn;i++) if(node_degree(i)==d) c++; return c; }
static int   deg_count_ge(int d){ int c=0; for(int i=0;i<nn;i++) if(node_degree(i)>=d) c++; return c; }
static int   deg_pct(int c){ return nn ? (100*c + nn/2)/nn : 0; }   // rounded share of all nodes (%)

// draw one street segment as a road quad (perpendicular extrusion of the centreline)
static void road_segment(float ax,float ay,float bx,float by,float hw,int col){
    float dx=bx-ax, dy=by-ay, L=sqrtf(dx*dx+dy*dy); if(L<0.5f) return; dx/=L; dy/=L;
    float nx=-dy*hw, ny=dx*hw;
    int q[8]={ (int)(ax+nx),(int)(ay+ny),(int)(ax-nx),(int)(ay-ny),
               (int)(bx-nx),(int)(by-ny),(int)(bx+nx),(int)(by+ny) };
    polyfill(q,4,col);
}

// ── M4c: the curvature knob (§8.5 #4 — warped vs straight). Each EDGE bows into a quadratic curve; the
//    graph/topology is untouched (degree, dead-ends unchanged) but the drawn roads WIND — and sinuosity
//    (edge arc-length ÷ chord) becomes a live SNDi measure (it's pinned at 1 by straight chords otherwise). ──
static float edge_bow(int i){            // signed ⊥ bow (px) for edge i — seeded per edge, scaled by the knob
    if (curveAmt<=0) return 0;
    NetNode a=nnodes[nedges[i].a], b=nnodes[nedges[i].b];
    float L=sqrtf((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y));
    return (hash01(nedges[i].a, nedges[i].b, netseed+313)-0.5f) * 2.f * (curveAmt/3.f) * 0.40f * L;
}
// sample edge i's curve into `out` (pairs); returns point count. bow=0 ⇒ the straight chord (2 points).
static int edge_curve(int i,float *out){
    NetNode a=nnodes[nedges[i].a], b=nnodes[nedges[i].b];
    float bow=edge_bow(i);
    if (bow>-0.5f && bow<0.5f){ out[0]=a.x;out[1]=a.y;out[2]=b.x;out[3]=b.y; return 2; }
    float dx=b.x-a.x, dy=b.y-a.y, L=sqrtf(dx*dx+dy*dy);
    float nx=-dy/L, ny=dx/L;
    float cx=(a.x+b.x)*0.5f+nx*bow, cy=(a.y+b.y)*0.5f+ny*bow;     // quadratic control = bowed midpoint
    int k=0; for (int s=0;s<=6;s++){ float t=s/6.f, it=1-t;
        out[k++]=it*it*a.x + 2*it*t*cx + t*t*b.x;
        out[k++]=it*it*a.y + 2*it*t*cy + t*t*b.y; }
    return 7;
}
static void draw_edge(int i,float hw,int col){
    float p[16]; int m=edge_curve(i,p);
    for (int s=0;s<m-1;s++) road_segment(p[s*2],p[s*2+1],p[s*2+2],p[s*2+3],hw,col);
}
static float mean_sinuosity(void){       // arc-length ÷ chord, averaged over edges (1.0 = straight)
    if (!ne) return 1;
    float tot=0; int cnt=0; float p[16];
    for (int i=0;i<ne;i++){
        NetNode a=nnodes[nedges[i].a], b=nnodes[nedges[i].b];
        float chord=sqrtf((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y)); if(chord<0.5f) continue;
        int m=edge_curve(i,p); float arc=0;
        for (int s=0;s<m-1;s++) arc+=sqrtf((p[s*2+2]-p[s*2])*(p[s*2+2]-p[s*2])+(p[s*2+3]-p[s*2+1])*(p[s*2+3]-p[s*2+1]));
        tot+=arc/chord; cnt++;
    }
    return cnt ? tot/cnt : 1;
}

static void draw_network_view(void){
    gen_network(pattern, netseed);
    for (int i=0;i<ne;i++) draw_edge(i, 2.5f, CLR_BROWNISH_BLACK);    // casing
    for (int i=0;i<ne;i++) draw_edge(i, 1.5f, CLR_DARK_GREY);         // asphalt
    for (int i=0;i<nn;i++) circfill((int)nnodes[i].x,(int)nnodes[i].y,1,CLR_DARK_GREY);

    font(FONT_SMALL);
    char bb[80];
    print("streetlab - the street WEB (M4: network topology)", 4,5, CLR_WHITE);
    snprintf(bb,sizeof bb,"%s  -  %d nodes  -  deg %.1f  -  %d dead-ends  -  sinuosity %.2f",
             PAT_NAME[pattern], nn, mean_degree(), dead_ends(), mean_sinuosity());
    print(bb, 4,13, CLR_ORANGE);
    // §8.2 degree distribution — the SNDi discriminator mean degree hides (Marshall node taxonomy)
    snprintf(bb,sizeof bb,"node mix:  cul(1) %d%%   T(3) %d%%   X(4+) %d%%",
             deg_pct(deg_count(1)), deg_pct(deg_count(3)), deg_pct(deg_count_ge(4)));
    print(bb, 4,21, CLR_LIGHT_GREY);

    rectfill(0, SCREEN_H-TOOLBAR, SCREEN_W, TOOLBAR, CLR_BLACK);
    int d;
    print("seed", 4, SCREEN_H-37, CLR_WHITE);
    d=step_btn(36, SCREEN_H-40, 14, "-", "+"); if (d){ netseed+=d; if(netseed<0)netseed=0; }
    snprintf(bb,sizeof bb,"%d",netseed); print(bb, 70, SCREEN_H-37, CLR_LIGHT_GREY);
    if (ui_button(92, SCREEN_H-40, 90, 13, PAT_NAME[pattern])) pattern=(pattern+1)%NPAT;
    print("curve", 192, SCREEN_H-37, CLR_BLUE);                       // M4c: winding knob
    d=step_btn(228, SCREEN_H-40, 14, "-", "+"); if (d){ curveAmt+=d; if(curveAmt<0)curveAmt=0; if(curveAmt>3)curveAmt=3; }
    snprintf(bb,sizeof bb,"%d",curveAmt); print(bb, 262, SCREEN_H-37, CLR_LIGHT_GREY);
    if (ui_button(220, SCREEN_H-22, 96, 13, "view: junction")) netview=0;
    font(FONT_NORMAL);
}

void draw(void){
    ui_begin();
    cls(CLR_DARK_GREEN);
    if (netview){ draw_network_view(); ui_end(); return; }     // M4: the street-web view
    float cx=SCREEN_W/2.f, cy=(SCREEN_H-TOOLBAR)/2.f;
    float HW=cross_hw();                                   // M7: half-width = the sum of the typed lanes present
    float REACH=SCREEN_W+SCREEN_H;                         // run each arm well past the screen edge
    int idx[NLEG]; float brg[NLEG]; int n=present_legs(idx,brg);

    // M5: SIDEWALK footprint = the whole junction inflated by SW, drawn FIRST (under the road). The asphalt
    // then covers the inner part, leaving an SW-wide sidewalk ring around everything — wrapping the corners
    // for free (same fillet geometry, wider). Sidewalks line the full length of each arm.
    if (peds){
        for (int i=0;i<n;i++){ float b=brg[i],dx=ux(b),dy=uy(b),nx=ux(b+90),ny=uy(b+90); float w=HW+SW;
            float ox=cx+dx*REACH, oy=cy+dy*REACH;
            int q[8]={(int)(cx+nx*w),(int)(cy+ny*w),(int)(cx-nx*w),(int)(cy-ny*w),
                      (int)(ox-nx*w),(int)(oy-ny*w),(int)(ox+nx*w),(int)(oy+ny*w)};
            polyfill(q,4,CLR_LIGHT_GREY); }
        for (int i=0;i<n;i++){ float bA=brg[i], bB=brg[(i+1)%n]; float gap=fmodf(bB-bA+3600,360);
            if (gap<=0.5f || gap>=179.5f) continue;
            float bm=bA+gap*0.5f, kx,ky; edge_corner(cx,cy,HW+SW, bA,bB,bm, &kx,&ky);
            CurbReturn c=curb_return(kx,ky, bA, bB, cornerR); fill_corner(kx,ky, c, cornerR, CLR_LIGHT_GREY); }
    }

    // asphalt = the union of arm bands (collinear arm pairs ⇒ a continuous strip ⇒ the centre is always
    // covered, no gap). casing pass (1px proud) first, asphalt pass second, so a kerb edge shows at the
    // outer band edges while the central overlap stays clean asphalt.
    for (int pass=0; pass<2; pass++){
        float w = HW + (pass?0:1);
        int col = pass ? CLR_DARK_GREY : CLR_BROWNISH_BLACK;
        for (int i=0;i<n;i++){
            float b=brg[i], dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90);
            float ox=cx+dx*REACH, oy=cy+dy*REACH;
            int q[8]={(int)(cx+nx*w),(int)(cy+ny*w),(int)(cx-nx*w),(int)(cy-ny*w),
                      (int)(ox-nx*w),(int)(oy-ny*w),(int)(ox+nx*w),(int)(oy+ny*w)};
            polyfill(q,4,col);
        }
    }

  if (roundabout){
    // ── M6: lay a clean CIRCULATORY disc over the messy central overlap (it connects every arm), then the
    //    traversable island. The disc's outer kerb is a full ring; we re-lay each arm's asphalt over it to
    //    REOPEN the mouths, so the kerb survives only on the grass wedges between arms. ──
    float ICR = round_icr();
    if (peds) circfill((int)cx,(int)cy,(int)(ICR+SW),CLR_LIGHT_GREY);   // sidewalk ring around the circulatory
    circfill((int)cx,(int)cy,(int)ICR+1,CLR_BROWNISH_BLACK);            // circulatory outer kerb (trimmed next)
    circfill((int)cx,(int)cy,(int)ICR,  CLR_DARK_GREY);                 // circulatory asphalt
    for (int i=0;i<n;i++){                                              // reopen each arm mouth through the kerb
        float b=brg[i], dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90);
        float ox=cx+dx*REACH, oy=cy+dy*REACH;
        int q[8]={(int)(cx+nx*HW),(int)(cy+ny*HW),(int)(cx-nx*HW),(int)(cy-ny*HW),
                  (int)(ox-nx*HW),(int)(oy-ny*HW),(int)(ox+nx*HW),(int)(oy+ny*HW)};
        polyfill(q,4,CLR_DARK_GREY);
    }
    draw_island(cx,cy);
    draw_circulation(cx,cy);
    for (int i=0;i<n;i++){                                              // per approach: splitter, give-way, crossing
        float b=brg[i];
        draw_splitter(cx,cy,b, ICR, 16);                               // teardrop splitter (deflect + ped refuge)
        give_way(cx,cy,b, ICR+1.5f, HW);                               // yield line at the circulatory edge
        if (peds) draw_crosswalk(cx,cy,b,HW, ICR+3);                    // crossing set back behind the give-way
    }
  } else {
    // curb returns: one per CONVEX gap between adjacent arms (skip the 180° straight back of a T)
    for (int i=0;i<n;i++){
        float bA=brg[i], bB=brg[(i+1)%n];
        float gap=fmodf(bB-bA+3600,360);
        if (gap<=0.5f || gap>=179.5f) continue;
        float bm=bA+gap*0.5f, kx,ky;
        edge_corner(cx,cy,HW, bA,bB,bm, &kx,&ky);
        CurbReturn c=curb_return(kx,ky, bA, bB, cornerR);
        fill_corner(kx,ky, c, cornerR, CLR_DARK_GREY);
        stroke_corner(c, cornerR, CLR_BROWNISH_BLACK);
    }

    // markings, median islands + stop bar per arm. startd clears the arm's corners (acute corners reach
    // further down the arm) — and, with turn lanes, the median nose too (no centreline over the island).
    for (int i=0;i<n;i++){
        float b=brg[i], df=arm_face(brg,n,i,HW);
        // the cleared THROAT of the arm is at the curb-return TANGENT, ~cornerR beyond the bare corner K
        // (arm_face). Every mouth marking — stop bar, crosswalk, median, turn bay, arrow — keys off this
        // `mouth` datum, NOT df, so they sit at the rounded mouth instead of poking back into the fillet.
        float mouth = df + cornerR;
        float startd = mouth + 3;
        if (turnLanes && mouth+POCKET+PTAPER+3 > startd) startd = mouth+POCKET+PTAPER+3;
        cross_markings(cx,cy,b,startd,REACH);
        float dx=ux(b),dy=uy(b), ix=ux(b-90),iy=uy(b-90);
        if (turnLanes){
            draw_median(cx,cy,b,mouth);                    // the channelizing island on the centreline
            // mark the inner inbound lane (next to the median) as the LEFT-TURN lane: a SOLID white
            // delineation from the stop bar back over the bay, where the dashed dividers leave off. No
            // carriageway widening — the lane lives inside the existing road, so it's skew-safe.
            if (lanesPer>=2)
                line((int)(cx+dx*mouth+ix*LANEW),(int)(cy+dy*mouth+iy*LANEW),
                     (int)(cx+dx*(mouth+POCKET)+ix*LANEW),(int)(cy+dy*(mouth+POCKET)+iy*LANEW),CLR_WHITE);
            turn_arrow(cx,cy,b,mouth,CLR_WHITE);
        }
        // M5: zebra crosswalk at the mouth (peds cross here); the stop bar sits BEHIND it (cars stop short).
        if (peds) draw_crosswalk(cx,cy,b,HW,mouth);
        // stop bar across the inbound lanes (drive-on-right: inbound sits on the b-90 side)
        float sb = peds ? mouth+1+CWDEP+1 : mouth+1;
        float mx=cx+dx*sb, my=cy+dy*sb;
        line((int)mx,(int)my,(int)(mx+ix*HW),(int)(my+iy*HW),CLR_WHITE);
    }
  }

    // ── HUD ──
    font(FONT_SMALL);
    char bb[64];
    int xs = medOn||bikeOn||parkOn;                        // M7: any typed cross-section element active?
    print(xs        ? "streetlab - at-grade junction (M7: typed cross-section)"
         : roundabout ? "streetlab - at-grade junction (M6: mini-roundabout)"
                      : "streetlab - at-grade junction (M5: sidewalks + crosswalks)", 4,5, CLR_WHITE);
    if (roundabout)
        snprintf(bb,sizeof bb,"mini-roundabout (traversable island)  -  %d arms  -  give-way entries%s",
                 n, peds?"  -  sidewalks + crossings":"");
    else
        snprintf(bb,sizeof bb,"%s  -  %d corners%s%s", isT?"T-junction":"4-way crossing", count_corners(),
                 turnLanes?"  -  turn bays":"", peds?"  -  sidewalks + crosswalks":"");
    print(bb, 4,13, CLR_ORANGE);

    // ── toolbar (three rows) ── row1 = M7 cross-section · row2 = curb/lanes/view · row3 = skew/topology/treatment
    rectfill(0, SCREEN_H-TOOLBAR, SCREEN_W, TOOLBAR, CLR_BLACK);
    int d; char wbuf[12];
    if (ui_button(4,   SCREEN_H-58, 78, 13, medOn ?"median: on" :"median: off")) medOn =!medOn;
    if (ui_button(86,  SCREEN_H-58, 62, 13, bikeOn?"bike: on"   :"bike: off"))    bikeOn=!bikeOn;
    if (ui_button(152, SCREEN_H-58, 82, 13, parkOn?"parking: on":"parking: off")) parkOn=!parkOn;
    snprintf(wbuf,sizeof wbuf,"HW %d",(int)cross_hw()); print(wbuf, 244, SCREEN_H-55, CLR_LIGHT_GREY);
    print(roundabout?"island R":"curb radius", 4, SCREEN_H-37, roundabout?CLR_BLUE:CLR_ORANGE);
    d=step_btn(64, SCREEN_H-40, 14, "-", "+"); if (d){ if(roundabout) islandR+=2*d; else cornerR+=2*d; }
    snprintf(bb,sizeof bb,"%d",(int)(roundabout?islandR:cornerR)); print(bb, 98, SCREEN_H-37, CLR_LIGHT_GREY);
    print("lanes/dir", 124, SCREEN_H-37, CLR_WHITE);
    d=step_btn(180, SCREEN_H-40, 14, "-", "+"); if (d) lanesPer+=d;
    snprintf(bb,sizeof bb,"%d",lanesPer); print(bb, 214, SCREEN_H-37, CLR_LIGHT_GREY);
    if (ui_button(232, SCREEN_H-40, 84, 13, "view: network")) netview=1;
    print("skew", 4, SCREEN_H-19, CLR_PINK);
    d=step_btn(64, SCREEN_H-22, 14, "-", "+"); if (d) skew+=5*d;
    snprintf(bb,sizeof bb,"%d",skew); print(bb, 98, SCREEN_H-19, CLR_LIGHT_GREY);
    if (ui_button(124, SCREEN_H-22, 90, 13, isT?"topology: T":"topology: 4-way")) isT=!isT;
    // one button cycles the junction TREATMENT: plain → turn lanes → roundabout (the last two are exclusive)
    const char *trt = roundabout?"junction: roundabout" : turnLanes?"junction: turn lanes":"junction: plain";
    if (ui_button(220, SCREEN_H-22, 96, 13, trt)){
        if (!turnLanes && !roundabout) turnLanes=1;
        else if (turnLanes){ turnLanes=0; roundabout=1; }
        else roundabout=0;
    }
    if (cornerR<0) cornerR=0;  if (cornerR>28) cornerR=28;
    if (islandR<3) islandR=3;  if (islandR>20) islandR=20;
    if (lanesPer<1) lanesPer=1; if (lanesPer>3) lanesPer=3;
    if (skew<-60) skew=-60;    if (skew>60) skew=60;

    ui_end();
    font(FONT_NORMAL);
}

// ── spec() — the cart-logic safety net (docs/design/spec-harness.md). streetlab is the FIRST cart to
//    carry one: curb_return() + count_corners() are pure, deterministic fns (clean seam), so they pin the
//    M1 primitive AND the M2 topology. Run: `node tools/spec.js streetlab` (or all: `node tools/spec.js`). ──
#ifdef DE_SPEC
#include "spec.h"
static float sp_dist(float ax,float ay,float bx,float by){ return sqrtf((ax-bx)*(ax-bx)+(ay-by)*(ay-by)); }
static float sp_dline(float kx,float ky,float dir,float px,float py){     // ⊥ distance from (px,py) to the line through K along dir
    return fabsf((px-kx)*uy(dir) - (py-ky)*ux(dir));                       // dir is a unit vector (ux,uy)
}
// sp_dist/sp_dline are cart-specific; the generic spec_near/spec_tap come from spec.h.

void spec(void){
    // ── curb_return() geometry — the M1 primitive, pinned (perpendicular) ──
    CurbReturn c = curb_return(100,100, 270, 0, 10.f);                     // perpendicular corner, edges up + east, R=10
    expect(spec_near(sp_dline(100,100,270, c.ox,c.oy), 10.f), "fillet centre is R from curb edge 1 (tangent)");
    expect(spec_near(sp_dline(100,100,  0, c.ox,c.oy), 10.f), "fillet centre is R from curb edge 2 (tangent)");
    expect(spec_near(sp_dist(c.ox,c.oy,c.t1x,c.t1y), 10.f),   "tangent point 1 sits R from the centre");
    expect(spec_near(sp_dist(c.ox,c.oy,c.t2x,c.t2y), 10.f),   "tangent point 2 sits R from the centre");
    expect(c.ox > 100 && c.oy < 100,                        "centre is on the sidewalk side (NE for up+east)");

    CurbReturn small = curb_return(100,100,270,0, 5.f);
    CurbReturn big   = curb_return(100,100,270,0,20.f);
    expect(sp_dist(100,100,big.ox,big.oy) > sp_dist(100,100,small.ox,small.oy),
           "bigger radius pushes the centre further from the corner");

    CurbReturn zero = curb_return(100,100,270,0, 0.f);
    expect(spec_near(zero.ox,100) && spec_near(zero.oy,100),    "R=0 collapses to a sharp corner (no fillet)");

    // ── M2: tangency holds at a SKEWED (non-90°) corner — proves the grammar is angle-agnostic ──
    CurbReturn sk = curb_return(100,100, 270, 40, 10.f);                   // a 70° corner
    expect(spec_near(sp_dline(100,100,270, sk.ox,sk.oy), 10.f), "skew: centre is R from edge 1 (tangent at 70deg)");
    expect(spec_near(sp_dline(100,100, 40, sk.ox,sk.oy), 10.f), "skew: centre is R from edge 2 (tangent at 70deg)");

    // ── M2: topology — count_corners() is pure over (skew, isT) ──
    skew=0;  isT=0;  expect_eq(count_corners(), 4, "4-way crossing has 4 corners");
    skew=20; isT=0;  expect_eq(count_corners(), 4, "a skewed 4-way still has 4 corners");
    skew=0;  isT=1;  expect_eq(count_corners(), 2, "a T-junction has 2 corners (straight back, no corner)");
    skew=0;  isT=0;                                                        // restore for the loop test

    // ── M3: arm_face() — the mouth distance the turn bay + median key off. On a perpendicular 4-way the
    //    corner projects to HW on the arm axis, so the face = HW. PURE (geometry of the leg layout). ──
    { int ix[NLEG]; float br[NLEG]; int m=present_legs(ix,br);
      expect(spec_near(arm_face(br,m,0,16.f), 16.f), "4-way arm face = HW (corner projects to HW on the axis)"); }

    // ── M4: the street web — gen_network() is pure over (pattern, seed); assert on the GRAPH (SNDi §8.2) ──
    gen_network(PAT_GRID, 1);
    expect_eq(nn, GW*GH, "grid network: GW*GH nodes");
    expect_eq(ne, GW*(GH-1)+GH*(GW-1), "grid network: edges = horizontals + verticals");
    expect_eq(dead_ends(), 0, "a full grid has no dead-ends");
    expect(mean_degree() > 2.5f && mean_degree() < 4.0f, "grid mean degree in (2.5, 4)");
    gen_network(PAT_ORGANIC, 7); float gx=nnodes[5].x;
    gen_network(PAT_ORGANIC, 7); expect(spec_near(nnodes[5].x, gx), "gen_network is deterministic for a fixed seed");
    gen_network(PAT_ORGANIC, 8); expect(!spec_near(nnodes[5].x, gx), "a different seed moves the nodes");
    // M4b topology changes — the patterns now differ structurally (the SNDi point: metrics separate them)
    gen_network(PAT_RADIAL, 1);
    expect_eq(nn, 1+R_RINGS*R_SECTORS, "radial: hub + rings*sectors nodes");
    expect_eq(node_degree(0), R_SECTORS, "radial: the hub is the high-degree node (degree = spokes)");
    gen_network(PAT_CULDESAC, 3);
    expect(ne >= nn-1, "cul-de-sac is connected (>= n-1 edges — a spanning tree + sparse loops)");
    expect(dead_ends() > 0, "cul-de-sac has dead-ends (grid has none — the topology differs)");
    expect(mean_degree() < 3.0f, "cul-de-sac mean degree < a full grid's (dendritic, not gridded)");
    // §8.2: the DEGREE DISTRIBUTION is the real discriminator (mean degree alone can blur two networks).
    // grid = zero cul-de-sacs + a big X-crossroads share; cul-de-sac = real degree-1 share + far fewer Xs.
    gen_network(PAT_GRID, 1);
    expect_eq(deg_count(1), 0,             "grid node mix: zero cul-de-sac (degree-1) nodes");
    expect(deg_count_ge(4) > 0,            "grid node mix: has X-crossroads (degree-4+) nodes");
    int grid_xshare = deg_pct(deg_count_ge(4));
    gen_network(PAT_CULDESAC, 3);
    expect(deg_count(1) > 0,               "cul-de-sac node mix: a real degree-1 (dead-end) share");
    expect(deg_pct(deg_count_ge(4)) < grid_xshare,
           "node mix separates the patterns: cul-de-sac's X(4+) share < grid's");
    // M4c: the curvature knob — straight chords pin sinuosity at 1; winding pushes it above 1 (a live SNDi measure)
    gen_network(PAT_GRID, 1);
    curveAmt=0; expect(spec_near(mean_sinuosity(), 1.0f), "curve=0: straight chords, sinuosity == 1");
    curveAmt=3; expect(mean_sinuosity() > 1.0f,         "curve>0: roads wind, sinuosity > 1");
    curveAmt=0;                                                            // restore

    // ── M6: mini-roundabout — round_icr() is pure over (islandR, lanesPer); the island sits strictly inside
    //    the inscribed circle and stays MINI (traversable: its radius <= the circulatory width). ──
    islandR=8; lanesPer=2;
    expect(round_icr() > islandR,                            "roundabout: inscribed circle is bigger than the island");
    expect(spec_near(round_icr(), islandR + lanesPer*LANEW), "roundabout: ICR = island radius + circulatory width");
    expect(islandR <= round_circ_w(),                        "mini-roundabout: island is traversable (R <= circulatory width)");
    float icr2 = round_icr(); lanesPer=3;
    expect(round_icr() > icr2,                               "more lanes widen the circulatory => a bigger inscribed circle");
    lanesPer=2;
    // 'r' toggles the roundabout and clears turn lanes (the two crossing treatments are mutually exclusive)
    turnLanes=1; roundabout=0; spec_tap('r');
    expect(roundabout==1 && turnLanes==0, "the 'r' key toggles the roundabout and clears turn lanes");
    spec_tap('r'); expect(roundabout==0,  "'r' again clears the roundabout");
    // in roundabout mode the [ ] knob drives the island radius (not curb radius); it stays mini-clamped
    roundabout=1; islandR=8;
    for (int i=0;i<40;i++) spec_tap(']'); expect(islandR <= 20.f, "roundabout: island radius caps at 20 (stays mini)");
    for (int i=0;i<40;i++) spec_tap('['); expect(islandR >= 3.f,  "roundabout: island radius floors at 3");
    roundabout=0;                                                          // restore before the curb-radius clamps

    // ── M7: typed cross-section — cross_hw() is pure over the lane toggles; each element adds its width, and
    //    every junction primitive keys off the sum. The median is the same island primitive as M3/M6. ──
    medOn=0; bikeOn=0; parkOn=0; lanesPer=2;
    expect(spec_near(cross_hw(), 2*LANEW),                 "cross-section: plain = lanesPer driving lanes wide");
    bikeOn=1; expect(spec_near(cross_hw(), 2*LANEW+BIKEW), "a bike lane adds its width to the carriageway");
    parkOn=1; expect(spec_near(cross_hw(), 2*LANEW+BIKEW+PARKW), "parking adds its width too");
    medOn=1;  expect(spec_near(cross_hw(), MEDHW+2*LANEW+BIKEW+PARKW), "HW = median + driving + bike + parking");
    { float fb[4]={0,90,180,270};                                          // the junction geometry keys off HW:
      expect(arm_face(fb,4,0, cross_hw()) > arm_face(fb,4,0, 2*LANEW),
             "a wider cross-section pushes the mouth further out (arm_face scales with HW)"); }
    medOn=0; bikeOn=0; parkOn=0;
    int m0=medOn;  spec_tap('m'); expect(medOn!=m0,  "the 'm' key toggles the centre median");
    int bk0=bikeOn; spec_tap('b'); expect(bikeOn!=bk0,"the 'b' key toggles the bike lane");
    int pk0=parkOn; spec_tap(';'); expect(parkOn!=pk0,"the ';' key toggles the parking lane");
    medOn=0; bikeOn=0; parkOn=0;                                            // restore

    // ── update() loop — the radius knob clamps + the turn-lanes toggle (proves step() + key injection) ──
    for (int i=0;i<40;i++) spec_tap(']');                                    // hammer the + key past the cap
    expect(cornerR <= 28.f, "curb radius caps at 28");
    for (int i=0;i<40;i++) spec_tap('[');                                    // hammer the - key past the floor
    expect(cornerR >= 0.f,  "curb radius floors at 0");
    int t0=turnLanes; spec_tap('p'); expect(turnLanes!=t0, "the 'p' key toggles turn lanes");
    int k0=peds;      spec_tap('k'); expect(peds!=k0,      "the 'k' key toggles sidewalks + crosswalks");
}
#endif
