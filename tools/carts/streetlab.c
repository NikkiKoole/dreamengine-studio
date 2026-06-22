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
// Next milestones (mirror roadlab's arc): M3 turn lanes / pockets + channelizing islands · M4 the street
//                WEB (network topology — grid/cul-de-sac/radial, the Parish-Müller / tensor-field generators
//                from §8). Then a seed-driven world emits it.
//   Controls: on-screen ui.h toolbar (clickable; keyboard too). [ ] curb radius · -/= lanes · ,/. skew · t = T.

#define LANEW    8     // one lane width (px)
#define TOOLBAR  44    // bottom control strip height (two rows)
#define POCKET   22    // M3: turn-bay length at the stop bar
#define PTAPER   14    // M3: upstream taper of the turn bay
#define MEDW     2     // M3: raised median splitter half-width
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
// markings along one arm (centre line + lane dividers), from startd out to the screen
static void arm_markings(float cx,float cy,float b,float HW,float startd,float reach){
    float dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90);
    float x0=cx+dx*startd, y0=cy+dy*startd, x1=cx+dx*reach, y1=cy+dy*reach;
    dashed(x0,y0,x1,y1,CLR_YELLOW);
    for (int k=1;k<lanesPer;k++){
        dashed(x0+nx*k*LANEW,y0+ny*k*LANEW, x1+nx*k*LANEW,y1+ny*k*LANEW, CLR_WHITE);
        dashed(x0-nx*k*LANEW,y0-ny*k*LANEW, x1-nx*k*LANEW,y1-ny*k*LANEW, CLR_WHITE);
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
static void chevron(float x,float y,float dir,float L,int col){       // a small arrowhead pointing `dir`
    float tx=x+ux(dir)*L, ty=y+uy(dir)*L;
    line((int)x,(int)y,(int)tx,(int)ty,col);
    line((int)tx,(int)ty,(int)(tx+ux(dir+150)*3),(int)(ty+uy(dir+150)*3),col);
    line((int)tx,(int)ty,(int)(tx+ux(dir-150)*3),(int)(ty+uy(dir-150)*3),col);
}
// LEFT-TURN BAY: widen the inbound side (drive-on-right ⇒ the b-90 side) by one lane over [df,df+POCKET],
// tapering back over PTAPER. The turn lane is the inbound lane next to the centreline; the widening makes
// room for it. Drawn as asphalt + a kerb on the new outer edge.
static void draw_turnbay(float cx,float cy,float b,float HW,float df){
    float ax=ux(b),ay=uy(b), ix=ux(b-90),iy=uy(b-90);     // axis + inbound-side normal
    float s0=df, s1=df+POCKET, s2=df+POCKET+PTAPER;
    int q[8]={ (int)(cx+ax*s0+ix*HW),         (int)(cy+ay*s0+iy*HW),
               (int)(cx+ax*s0+ix*(HW+LANEW)), (int)(cy+ay*s0+iy*(HW+LANEW)),
               (int)(cx+ax*s1+ix*(HW+LANEW)), (int)(cy+ay*s1+iy*(HW+LANEW)),
               (int)(cx+ax*s2+ix*HW),         (int)(cy+ay*s2+iy*HW) };
    polyfill(q,4,CLR_DARK_GREY);
    line(q[2],q[3],q[4],q[5],CLR_BROWNISH_BLACK);          // kerb: pocket outer edge
    line(q[4],q[5],q[6],q[7],CLR_BROWNISH_BLACK);          // kerb: taper back to the carriageway
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

void update(void){
    if (keyp('['))  cornerR -= 2;
    if (keyp(']'))  cornerR += 2;
    if (keyp('-'))  lanesPer--;
    if (keyp('='))  lanesPer++;
    if (keyp(','))  skew -= 5;
    if (keyp('.'))  skew += 5;
    if (keyp('t')||keyp('T')) isT = !isT;
    if (keyp('p')||keyp('P')) turnLanes = !turnLanes;
    if (cornerR<0) cornerR=0;  if (cornerR>28) cornerR=28;
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

void draw(void){
    ui_begin();
    cls(CLR_DARK_GREEN);
    float cx=SCREEN_W/2.f, cy=(SCREEN_H-TOOLBAR)/2.f;
    float HW=lanesPer*LANEW;                               // carriageway half-width (both directions)
    float REACH=SCREEN_W+SCREEN_H;                         // run each arm well past the screen edge
    int idx[NLEG]; float brg[NLEG]; int n=present_legs(idx,brg);

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

    // M3: turn-bay asphalt (widen the inbound side near each mouth) — laid before the curb returns so the
    // corner fillets/kerbs draw on top where they meet.
    if (turnLanes) for (int i=0;i<n;i++) draw_turnbay(cx,cy, brg[i], HW, arm_face(brg,n,i,HW));

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
        float startd = df + cornerR + 3;
        if (turnLanes && df+POCKET+PTAPER+3 > startd) startd = df+POCKET+PTAPER+3;
        arm_markings(cx,cy,b,HW,startd,REACH);
        float dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90), ix=ux(b-90),iy=uy(b-90);
        if (turnLanes){
            draw_median(cx,cy,b,df);                       // the channelizing island on the centreline
            // left-turn arrowhead in the bay (inbound lane next to the median), pointing toward the hub
            float qx=cx+dx*(df+6)+ix*(LANEW*0.5f), qy=cy+dy*(df+6)+iy*(LANEW*0.5f);
            chevron(qx,qy, b+180, 6, CLR_WHITE);
        }
        // stop bar across the inbound half (drive-on-right: inbound lanes sit on the b-90 side); the turn
        // bay adds a lane, so the bar spans HW+LANEW there.
        float bar = turnLanes ? HW+LANEW : HW;
        float mx=cx+dx*(df+1), my=cy+dy*(df+1);
        line((int)mx,(int)my,(int)(mx+ix*bar),(int)(my+iy*bar),CLR_WHITE);
    }

    // ── HUD ──
    font(FONT_SMALL);
    char bb[64];
    print("streetlab - at-grade junction (M3: turn lanes + islands)", 4,5, CLR_WHITE);
    snprintf(bb,sizeof bb,"%s   -   %d corners%s", isT?"T-junction":"4-way crossing",
             count_corners(), turnLanes?"   -   left-turn bays + raised median":"");
    print(bb, 4,13, CLR_ORANGE);

    // ── toolbar (two rows) ──
    rectfill(0, SCREEN_H-TOOLBAR, SCREEN_W, TOOLBAR, CLR_BLACK);
    int d;
    print("curb radius", 4, SCREEN_H-37, CLR_ORANGE);
    d=step_btn(64, SCREEN_H-40, 14, "-", "+"); if (d) cornerR+=2*d;
    snprintf(bb,sizeof bb,"%d",(int)cornerR); print(bb, 98, SCREEN_H-37, CLR_LIGHT_GREY);
    print("lanes/dir", 124, SCREEN_H-37, CLR_WHITE);
    d=step_btn(180, SCREEN_H-40, 14, "-", "+"); if (d) lanesPer+=d;
    snprintf(bb,sizeof bb,"%d",lanesPer); print(bb, 214, SCREEN_H-37, CLR_LIGHT_GREY);
    print("skew", 4, SCREEN_H-19, CLR_PINK);
    d=step_btn(64, SCREEN_H-22, 14, "-", "+"); if (d) skew+=5*d;
    snprintf(bb,sizeof bb,"%d",skew); print(bb, 98, SCREEN_H-19, CLR_LIGHT_GREY);
    if (ui_button(124, SCREEN_H-22, 90, 13, isT?"topology: T":"topology: 4-way")) isT=!isT;
    if (ui_button(220, SCREEN_H-22, 96, 13, turnLanes?"turn lanes: on":"turn lanes: off")) turnLanes=!turnLanes;
    if (cornerR<0) cornerR=0;  if (cornerR>28) cornerR=28;
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
static int   sp_near(float a,float b){ float d=a-b; return (d<0?-d:d) < 0.2f; }
static void  sp_tap(int k){ key_down(k); step(1); key_up(k); step(1); }    // one clean press→release edge

void spec(void){
    // ── curb_return() geometry — the M1 primitive, pinned (perpendicular) ──
    CurbReturn c = curb_return(100,100, 270, 0, 10.f);                     // perpendicular corner, edges up + east, R=10
    expect(sp_near(sp_dline(100,100,270, c.ox,c.oy), 10.f), "fillet centre is R from curb edge 1 (tangent)");
    expect(sp_near(sp_dline(100,100,  0, c.ox,c.oy), 10.f), "fillet centre is R from curb edge 2 (tangent)");
    expect(sp_near(sp_dist(c.ox,c.oy,c.t1x,c.t1y), 10.f),   "tangent point 1 sits R from the centre");
    expect(sp_near(sp_dist(c.ox,c.oy,c.t2x,c.t2y), 10.f),   "tangent point 2 sits R from the centre");
    expect(c.ox > 100 && c.oy < 100,                        "centre is on the sidewalk side (NE for up+east)");

    CurbReturn small = curb_return(100,100,270,0, 5.f);
    CurbReturn big   = curb_return(100,100,270,0,20.f);
    expect(sp_dist(100,100,big.ox,big.oy) > sp_dist(100,100,small.ox,small.oy),
           "bigger radius pushes the centre further from the corner");

    CurbReturn zero = curb_return(100,100,270,0, 0.f);
    expect(sp_near(zero.ox,100) && sp_near(zero.oy,100),    "R=0 collapses to a sharp corner (no fillet)");

    // ── M2: tangency holds at a SKEWED (non-90°) corner — proves the grammar is angle-agnostic ──
    CurbReturn sk = curb_return(100,100, 270, 40, 10.f);                   // a 70° corner
    expect(sp_near(sp_dline(100,100,270, sk.ox,sk.oy), 10.f), "skew: centre is R from edge 1 (tangent at 70deg)");
    expect(sp_near(sp_dline(100,100, 40, sk.ox,sk.oy), 10.f), "skew: centre is R from edge 2 (tangent at 70deg)");

    // ── M2: topology — count_corners() is pure over (skew, isT) ──
    skew=0;  isT=0;  expect_eq(count_corners(), 4, "4-way crossing has 4 corners");
    skew=20; isT=0;  expect_eq(count_corners(), 4, "a skewed 4-way still has 4 corners");
    skew=0;  isT=1;  expect_eq(count_corners(), 2, "a T-junction has 2 corners (straight back, no corner)");
    skew=0;  isT=0;                                                        // restore for the loop test

    // ── M3: arm_face() — the mouth distance the turn bay + median key off. On a perpendicular 4-way the
    //    corner projects to HW on the arm axis, so the face = HW. PURE (geometry of the leg layout). ──
    { int ix[NLEG]; float br[NLEG]; int m=present_legs(ix,br);
      expect(sp_near(arm_face(br,m,0,16.f), 16.f), "4-way arm face = HW (corner projects to HW on the axis)"); }

    // ── update() loop — the radius knob clamps + the turn-lanes toggle (proves step() + key injection) ──
    for (int i=0;i<40;i++) sp_tap(']');                                    // hammer the + key past the cap
    expect(cornerR <= 28.f, "curb radius caps at 28");
    for (int i=0;i<40;i++) sp_tap('[');                                    // hammer the - key past the floor
    expect(cornerR >= 0.f,  "curb radius floors at 0");
    int t0=turnLanes; sp_tap('p'); expect(turnLanes!=t0, "the 'p' key toggles turn lanes");
}
#endif
