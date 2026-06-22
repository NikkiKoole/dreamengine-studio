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
// Next milestones (mirror roadlab's arc): M2 skew + the T (3-corner) family · M3 turn lanes / pockets +
//                channelizing islands · M4 the street WEB (network topology — grid/cul-de-sac/radial, the
//                Parish-Müller / tensor-field generators from §8). Then a seed-driven world emits it.
//   Controls: on-screen ui.h toolbar (clickable; keyboard too). [ ] curb radius · -/+ lanes.

#define LANEW    8     // one lane width (px)
#define TOOLBAR  30    // bottom control strip height
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

// ── state ──
static float cornerR = 8.f;   // curb-return radius (the headline at-grade knob)
static int   lanesPer = 2;     // lanes PER DIRECTION (street width = 2 * lanesPer)

void update(void){
    if (keyp('['))  cornerR -= 2;
    if (keyp(']'))  cornerR += 2;
    if (keyp('-'))  lanesPer--;
    if (keyp('='))  lanesPer++;
    if (cornerR<0) cornerR=0;  if (cornerR>28) cornerR=28;
    if (lanesPer<1) lanesPer=1; if (lanesPer>3) lanesPer=3;
#ifdef DE_TRACE
    { CurbReturn c=curb_return(100,100,270,0,cornerR);     // NE-shaped corner, for the trace/spec
      watch("cornerR", "%.1f", cornerR);
      watch("fillet_ox", "%.1f", c.ox); watch("fillet_oy", "%.1f", c.oy); }
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
    int   top=(int)(SCREEN_H-TOOLBAR);

    // casing (kerb base): strips 1px proud of the asphalt, drawn first so a 1px curb edge shows
    rectfill(0,(int)(cy-HW-1), SCREEN_W,(int)(2*HW+2), CLR_BROWNISH_BLACK);
    rectfill((int)(cx-HW-1),0, (int)(2*HW+2), top,     CLR_BROWNISH_BLACK);
    // asphalt strips (the cross)
    rectfill(0,(int)(cy-HW), SCREEN_W,(int)(2*HW), CLR_DARK_GREY);
    rectfill((int)(cx-HW),0, (int)(2*HW), top,     CLR_DARK_GREY);

    // the 4 curb returns. K = a box corner; the two edges leave K up/down the two streets.
    CurbReturn ne=curb_return(cx+HW,cy-HW,270,  0,cornerR);   // up + east
    CurbReturn se=curb_return(cx+HW,cy+HW, 90,  0,cornerR);   // down + east
    CurbReturn sw=curb_return(cx-HW,cy+HW, 90,180,cornerR);   // down + west
    CurbReturn nw=curb_return(cx-HW,cy-HW,270,180,cornerR);   // up + west
    fill_corner(cx+HW,cy-HW, ne,cornerR,CLR_DARK_GREY); fill_corner(cx+HW,cy+HW, se,cornerR,CLR_DARK_GREY);
    fill_corner(cx-HW,cy+HW, sw,cornerR,CLR_DARK_GREY); fill_corner(cx-HW,cy-HW, nw,cornerR,CLR_DARK_GREY);
    stroke_corner(ne,cornerR,CLR_BROWNISH_BLACK); stroke_corner(se,cornerR,CLR_BROWNISH_BLACK);
    stroke_corner(sw,cornerR,CLR_BROWNISH_BLACK); stroke_corner(nw,cornerR,CLR_BROWNISH_BLACK);

    // ── road markings (only OUTSIDE the intersection box) ──
    // yellow centre line separates opposing directions; white dashed dividers separate same-direction lanes,
    // so `lanes/dir` reads as actual lanes (the road also widens per lane, like a real road).
    for (int s=0;s<2;s++){                                 // horizontal street, the two arms (W then E)
        float x0=s?cx+HW:0, x1=s?SCREEN_W:cx-HW;
        dashed(x0,cy, x1,cy, CLR_YELLOW);
        for (int k=1;k<lanesPer;k++){ dashed(x0,cy-k*LANEW,x1,cy-k*LANEW,CLR_WHITE);
                                      dashed(x0,cy+k*LANEW,x1,cy+k*LANEW,CLR_WHITE); }
    }
    for (int s=0;s<2;s++){                                 // vertical street, the two arms (N then S)
        float y0=s?cy+HW:0, y1=s?top:cy-HW;
        dashed(cx,y0, cx,y1, CLR_YELLOW);
        for (int k=1;k<lanesPer;k++){ dashed(cx-k*LANEW,y0,cx-k*LANEW,y1,CLR_WHITE);
                                      dashed(cx+k*LANEW,y0,cx+k*LANEW,y1,CLR_WHITE); }
    }
    // white stop bars — at each box face, across the inbound (right-hand) lanes only (drive-on-right)
    line((int)(cx-HW),(int)(cy-HW),(int)(cx),   (int)(cy-HW),CLR_WHITE);  // N face (S-bound stops, west half)
    line((int)(cx),   (int)(cy+HW),(int)(cx+HW),(int)(cy+HW),CLR_WHITE);  // S face (N-bound stops, east half)
    line((int)(cx-HW),(int)(cy),   (int)(cx-HW),(int)(cy+HW),CLR_WHITE);  // W face (E-bound stops, south half)
    line((int)(cx+HW),(int)(cy-HW),(int)(cx+HW),(int)(cy),   CLR_WHITE);  // E face (W-bound stops, north half)

    // ── HUD ──
    font(FONT_SMALL);
    char b[64];
    print("streetlab - at-grade crossing (M1: curb returns)", 4,5, CLR_WHITE);
    snprintf(b,sizeof b,"curb radius %dpx   -   small = tight turn, short ped crossing",(int)cornerR);
    print(b, 4,13, CLR_ORANGE);

    // ── toolbar ──
    rectfill(0, SCREEN_H-TOOLBAR, SCREEN_W, TOOLBAR, CLR_BLACK);
    int d;
    print("curb radius", 4, SCREEN_H-22, CLR_ORANGE);
    d=step_btn(64, SCREEN_H-25, 14, "-", "+"); if (d) cornerR+=2*d;
    snprintf(b,sizeof b,"%d",(int)cornerR); print(b, 98, SCREEN_H-22, CLR_LIGHT_GREY);
    print("lanes/dir", 124, SCREEN_H-22, CLR_WHITE);
    d=step_btn(180, SCREEN_H-25, 14, "-", "+"); if (d) lanesPer+=d;
    snprintf(b,sizeof b,"%d",lanesPer); print(b, 214, SCREEN_H-22, CLR_LIGHT_GREY);
    if (cornerR<0) cornerR=0;  if (cornerR>28) cornerR=28;
    if (lanesPer<1) lanesPer=1; if (lanesPer>3) lanesPer=3;

    ui_end();
    font(FONT_NORMAL);
}

// ── spec() — the cart-logic safety net (docs/design/spec-harness.md). streetlab is the FIRST cart to
//    carry one: curb_return() is a pure, deterministic geometry fn (clean seam), so it pins exactly the
//    M1 primitive. Run: `node tools/spec.js streetlab` (or all carts: `node tools/spec.js`). ──
#ifdef DE_SPEC
#include "spec.h"
static float sp_dist(float ax,float ay,float bx,float by){ return sqrtf((ax-bx)*(ax-bx)+(ay-by)*(ay-by)); }
static float sp_dline(float kx,float ky,float dir,float px,float py){     // ⊥ distance from (px,py) to the line through K along dir
    return fabsf((px-kx)*uy(dir) - (py-ky)*ux(dir));                       // dir is a unit vector (ux,uy)
}
static int   sp_near(float a,float b){ float d=a-b; return (d<0?-d:d) < 0.2f; }
static void  sp_tap(int k){ key_down(k); step(1); key_up(k); step(1); }    // one clean press→release edge

void spec(void){
    // ── curb_return() geometry — the M1 primitive, pinned ──
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

    // ── update() loop — the radius knob clamps (proves step() + key injection drive the cart) ──
    for (int i=0;i<40;i++) sp_tap(']');                                    // hammer the + key past the cap
    expect(cornerR <= 28.f, "curb radius caps at 28");
    for (int i=0;i<40;i++) sp_tap('[');                                    // hammer the - key past the floor
    expect(cornerR >= 0.f,  "curb radius floors at 0");
}
#endif
