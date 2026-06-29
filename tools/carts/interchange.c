/* de:meta
{
  "title": "interchange",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "procedural-mesh"
  ],
  "lineage": "Geometry sandbox grown out of the roadnet2 project (docs/design/roadnet2-plan.md); novel in building a declarative junction matrix (topology × road-class × ramp-style) where every interchange type is assembled from cubic Bézier ramp atoms and ribbon polylines rather than hand-placed coordinates.",
  "description": "A geometry SANDBOX for roadnet2's road interchanges — how a crossing road meets a highway, in fake-3D (the crossing road is drawn OVER the highway, reading as an overpass — grade separation without real z-levels) plus a library of connection types built from ramp curves. A horizontal highway and a hand-placed crossing road, with the ramps grown and eyeballed in isolation before porting the drawer into roadnet2. The eventual interface: given two crossing roads + a type, draw the ramps. Controls: T cycles the type (OVERPASS / DIAMOND / CLOVERLEAF / TRUMPET), LEFT/RIGHT rotate the crossing road, UP/DOWN ramp size, H hides the HUD. Diamond + overpass are solid; cloverleaf/trumpet are first-draft. See docs/design/roadnet2-plan.md."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <stdio.h>

// ============================================================================
// INTERCHANGE — a geometry SANDBOX for roadnet2's part-B (how roads cross/connect a
// highway). Screen-space, hand-placed: a horizontal HIGHWAY and a crossing road, with
// FAKE-3D grade separation (the crossing road is drawn OVER the highway → reads as an
// overpass) and a library of connection types built from RAMP curves. Grow the shapes
// here, eyeball them, then port the drawer into roadnet2. The eventual interface is:
// "given two crossing roads (+ a type) → draw the ramps."  See docs/design/roadnet2-plan.md.
//
// Junction = TOPOLOGY (cross / T-split / Y-split) × CLASS-PAIR (highway/arterial each). The rule:
// any highway → grade-separated (overpass + ramps); else at-grade. Toggle via on-screen buttons
// or keys. See docs/design/roadnet2-plan.md → "The junction matrix".
//   click buttons  topology / flip / A class / B class / ramp type      (or the keys:)
//   Y  topology    1/2  road A/B class    F  flip 3rd leg    T  ramp type
//   L  lanes       ◄ ►  angle             P  panel           H  hud
//   sliders        reach / gore / taper / run-on (ramp shape)
// ============================================================================

#define LANE_W   6          // one lane width (px)
#define HW_AR    8          // arterial half-width
static int   lanes = 2;     // highway lanes PER DIRECTION (variable) → half-width = lanes·LANE_W
enum { C_HIGHWAY, C_ARTERIAL };          // the only two road classes this cart handles
static int   clsA = C_HIGHWAY;           // road A = the horizontal road
static int   clsB = C_ARTERIAL;          // road B = the crossing road
static int   rwidth(int c){ return c==C_HIGHWAY ? lanes*LANE_W : HW_AR; }   // half-width by class
static float fsq(float x){ if(x<=0)return 0; float g=x>1?x:1; for(int i=0;i<6;i++) g=0.5f*(g+x/g); return g; }
static float c_deg(float d){ return dx(1.0f, d); }   // engine dx/dy = cos/sin in degrees (0=right,90=down)
static float s_deg(float d){ return dy(1.0f, d); }

enum { T_OVERPASS, T_DIAMOND, T_CLOVERLEAF, T_TRUMPET, T_COUNT };   // ramp STYLE (grade-separated only)
static const char *TNAME[T_COUNT] = { "OVERPASS", "DIAMOND", "CLOVERLEAF", "TRUMPET" };
static int   itype = T_DIAMOND;
enum { TOPO_CROSS, TOPO_TEE, TOPO_WYE, TOPO_COUNT };    // junction TOPOLOGY (the leg layout)
static const char *PNAME[TOPO_COUNT] = { "CROSS", "T-SPLIT", "Y-SPLIT" };
// THE junction matrix: every supported junction is one button. A click sets the WHOLE config
// (topology + both classes + ramp-style). Laid out as a ragged grid — row = topology, columns =
// that topology's variants. `built` 0 = stub/draft (marked + flagged in the tooltip).
typedef struct { const char *name; int topo, ca, cb, it, built; const char *tip; } Junc;
static const Junc JUNCS[] = {
    // CROSS
    { "cross",  TOPO_CROSS, C_ARTERIAL, C_ARTERIAL, T_OVERPASS,   1, "CROSS ARxAR: crossroads (at-grade 4-way)" },
    { "diamond",TOPO_CROSS, C_HIGHWAY,  C_ARTERIAL, T_DIAMOND,    1, "CROSS HWxAR: diamond - full, both carriageways" },
    { "clover", TOPO_CROSS, C_HIGHWAY,  C_HIGHWAY,  T_CLOVERLEAF, 0, "CROSS HWxHW: cloverleaf - DRAFT (4 loops, tangled)" },
    { "stack",  TOPO_CROSS, C_HIGHWAY,  C_HIGHWAY,  T_OVERPASS,   0, "CROSS HWxHW: stack - NOT BUILT (overpass+ramps stub)" },
    // T-SPLIT
    { "tee",    TOPO_TEE,   C_ARTERIAL, C_ARTERIAL, T_OVERPASS,   1, "T-SPLIT ARxAR: T-intersection (at-grade)" },
    { "half",   TOPO_TEE,   C_HIGHWAY,  C_ARTERIAL, T_DIAMOND,    1, "T-SPLIT HWxAR: half-diamond - PARTIAL, near carriageway only" },
    { "trumpet",TOPO_TEE,   C_HIGHWAY,  C_HIGHWAY,  T_TRUMPET,    1, "T-SPLIT HWxHW: trumpet - loop + flyover, both carriageways" },
    // Y-SPLIT
    { "fork",   TOPO_WYE,   C_ARTERIAL, C_ARTERIAL, T_OVERPASS,   1, "Y-SPLIT ARxAR: fork (at-grade diverge)" },
    { "wye",    TOPO_WYE,   C_HIGHWAY,  C_ARTERIAL, T_OVERPASS,   1, "Y-SPLIT HWxAR: wye - tangent diverge (at-grade)" },
};
#define NJUNCS ((int)(sizeof JUNCS / sizeof JUNCS[0]))
static int   topo = TOPO_CROSS;
static int   flip = 0;          // 3-way junctions: which side the 3rd leg is on
static float ang   = 90.0f;     // crossing road angle (deg from highway)
static int   show_hud = 1, show_panel = 1;
static int   show_near = 1, show_loop = 1, show_fly = 1;  // atom-isolation toggles (composed junctions)
// tunable ramp shape — slider-normalised 0..1, mapped to real values in draw() (tune by feel,
// then bake the chosen values as constants when porting the drawer into roadnet2):
static float s_reach = 0.33f;   //  R       reach  24..90 px
static float s_gore  = 0.40f;   //  gore    divergence point along highway, ×R 1.0..2.0
static float s_taper = 0.46f;   //  kA      off-ramp taper run, ×R 0.5..1.8
static float s_runon = 0.45f;   //  kB      run-on along the overpass, ×R 0.4..1.4
static float s_loopend = 0.5f;  //  endk    trumpet LOOP exit: how long the merge runs along the lane

// ── ribbon: stroke a polyline as a clean filled road (centre-line + polyfill quads + joint
//    circles), the same technique roadnet2 uses. casing = a darker outline drawn first/wider.
// stroke a polyline as ONE offset polygon: each vertex pushed ±hw along its averaged normal,
// then a single polyfill → smooth mitred edges, no per-joint blobs.
static void ribbon(const float *xs, const float *ys, int n, int hw, int col) {
    if (n < 2) return;
    if (hw < 2) { for (int i=0;i+1<n;i++) line((int)xs[i],(int)ys[i],(int)xs[i+1],(int)ys[i+1], col); return; }
    if (n > 40) n = 40;
    float nx[40], ny[40];
    for (int i=0;i<n;i++) {
        float tx, ty;                                    // averaged tangent at this vertex
        if      (i==0)   { tx=xs[1]-xs[0];     ty=ys[1]-ys[0];     }
        else if (i==n-1) { tx=xs[i]-xs[i-1];   ty=ys[i]-ys[i-1];   }
        else             { tx=xs[i+1]-xs[i-1]; ty=ys[i+1]-ys[i-1]; }
        float L=fsq(tx*tx+ty*ty); if (L<0.001f) L=1;
        nx[i]=-ty/L*hw; ny[i]=tx/L*hw;                   // outward normal × half-width
    }
    int poly[160], m=0;
    for (int i=0;i<n;i++)   { poly[m++]=(int)(xs[i]+nx[i]); poly[m++]=(int)(ys[i]+ny[i]); }  // left edge fwd
    for (int i=n-1;i>=0;i--){ poly[m++]=(int)(xs[i]-nx[i]); poly[m++]=(int)(ys[i]-ny[i]); }  // right edge back
    polyfill(poly, m/2, col);
}
// stroke with a casing (wider darker band under a narrower lighter centre) — a "real road" look
static void road(const float *xs, const float *ys, int n, int hw, int casing, int centre) {
    ribbon(xs, ys, n, hw,   casing);
    ribbon(xs, ys, n, hw-3, centre);
}

// ── cubic Bézier ramp from A (heading dir aA) to B (heading dir aB), sampled into xs/ys.
//    Control handles along the two tangents give a smooth merge curve.
#define RN 14
// kA = handle at A (highway) — long = a parallel off-ramp TAPER before peeling off;
// kB = handle at B (overpass) — long = the ramp runs along the crossing road before merging.
static void ramp_pts(float ax, float ay, float aA, float bx, float by, float aB,
                     float kA, float kB, float *xs, float *ys) {
    float c1x=ax+c_deg(aA)*kA, c1y=ay+s_deg(aA)*kA;
    float c2x=bx-c_deg(aB)*kB, c2y=by-s_deg(aB)*kB;
    for (int i=0;i<=RN;i++){
        float t=(float)i/RN, u=1-t;
        float w0=u*u*u, w1=3*u*u*t, w2=3*u*t*t, w3=t*t*t;
        xs[i]=w0*ax + w1*c1x + w2*c2x + w3*bx;
        ys[i]=w0*ay + w1*c1y + w2*c2y + w3*by;
    }
}
static void draw_ramp(float ax,float ay,float aA, float bx,float by,float aB, float kA, float kB) {
    float xs[RN+1], ys[RN+1];
    ramp_pts(ax,ay,aA, bx,by,aB, kA,kB, xs,ys);
    ribbon(xs, ys, RN+1, 5, CLR_DARKER_GREY);    // ~1-lane ramp (was 2 lanes wide → looked detached)
    ribbon(xs, ys, RN+1, 3, CLR_DARK_GREY);
}
// a teardrop LOOP ramp: a near-full circular arc around (cx,cy) of radius `rad`, `sweep`° from `a0`°.
// (cloverleaf loops + the trumpet loop are this; ~1-lane ribbon like draw_ramp.)
static void draw_loop(float cx, float cy, float rad, float a0, float sweep, int hw) {
    float xs[40], ys[40]; int n=0, steps=34;
    for (int i=0;i<=steps;i++){ float a=a0+sweep*(float)i/steps;
        xs[n]=cx+c_deg(a)*rad; ys[n]=cy+s_deg(a)*rad; n++; }
    ribbon(xs, ys, n, hw+2, CLR_DARKER_GREY);
    ribbon(xs, ys, n, hw,   CLR_DARK_GREY);
}

// ── junction geometry context — the derived values every ramp ATOM reads, so atoms take no long
//    param list and the same `near_pair`/`loop`/`flyover` compose into every junction type.
typedef struct {
    int   cx, cy;            // junction centre (highway × trunk)
    float ux, uy;            // trunk/crossing-road unit dir (the `ang` direction)
    int   hw, bw;            // road A half-width (highway), road B half-width (trunk)
    float reach, spread, gore, taper, runon;   // mapped slider values (px / ×reach)
    float endk;              // trumpet loop-exit tuning (merge run length, ×reach)
    float ang;               // trunk angle (deg)
} Geo;

static void straight(float ax,float ay,float bx,float by,int hw,int casing,int centre);  // fwd (defined below)

// ATOM — the half-diamond fan: two ramps tying the trunk to ONE carriageway (sy = which side,
// ±1). Highway-ends sit ±spread along the highway; trunk-ends sit ±gore off the trunk axis (gore
// 0 = a sharp single-point nose). This is the shared base of half-diamond / diamond / trumpet.
static void near_pair(const Geo *g, int sy) {
    float perpx = -g->uy, perpy = g->ux;             // perpendicular to the trunk
    float cd = g->hw + g->reach;                     // trunk-end distance out along the trunk axis
    for (int sx=-1; sx<=1; sx+=2) {                  // the two ramps (left / right along the highway)
        float hx = g->cx + sx*g->spread,  hy = g->cy + sy*(g->hw - LANE_W*0.5f);
        float ax = g->cx + g->ux*sy*cd - perpx*sx*g->gore;
        float ay = g->cy + g->uy*sy*cd - perpy*sx*g->gore;
        draw_ramp(hx,hy, (sx>0?180.f:0.f), ax,ay, (sy>0?g->ang:g->ang+180.f),
                  g->reach*g->taper, g->reach*g->runon);
    }
}

// ATOM — the teardrop LOOP: trunk → FAR carriageway via a ~300° loop on the +pp side. This is the
// move the half-diamond can't do (it reaches the OTHER carriageway). A short tangent stub at the
// loop's exit kisses the far carriageway instead of just touching it.
static void loop_ramp(const Geo *g, int ss) {
    float aux = ss*g->ux, auy = ss*g->uy;             // trunk axis toward the approach side
    float fx = -aux, fy = -auy;                       // toward the FAR side (trunk pokes over to here)
    float ppx = -auy, ppy = aux;                      // along the highway = the bell's side
    float rl   = g->reach*0.8f;
    float moff = g->hw - LANE_W*0.5f;                 // far-carriageway lane offset from the centreline
    // The loop bends ONE way (over the top, ~180°), then the EXIT REVERSES the bend (the other way) to
    // flatten out ALONG the horizontal carriageway — a reverse curve that aligns with the road and merges
    // with no cusp. (A single 270° circle can't do this; that was the cusp.) Entry E sits at the top of
    // the trunk poke so the trunk forms the loop's straight side; the bell bulges to the +pp side.
    int   lw = g->bw/2; if (lw<3) lw=3;                     // loop width = the whole NORTHBOUND carriageway,
                                                            // so every trunk lane can get onto it
    float cwoff = g->bw*0.5f;                               // entry centred on the northbound (right) carriageway
    float Ex = g->cx - ppx*cwoff, Ey = g->cy + fy*(moff + rl);  // entry — northbound carriageway, not the centreline
    float Cx = Ex + ppx*rl,  Cy = Ey + ppy*rl;              // loop centre, rl to the +pp (bell) side
    float a0 = angle_to((int)Cx,(int)Cy,(int)Ex,(int)Ey);   // start angle (centre → entry)
    float af = angle_to(0,0,(int)(fx*100),(int)(fy*100));    // far-side bearing — the bell bulges this way
    float dd = af - a0; while (dd>180) dd-=360; while (dd<-180) dd+=360;
    float sweep = (dd>0) ? 180.f : -180.f;                  // 180° over the top, bulging to the far side
    draw_loop(Cx, Cy, rl, a0, sweep, lw);
    // exit reverse-curve: loop ends heading back toward the highway (aux); bend the OTHER way round to
    // heading pp (downstream along the far carriageway) and merge onto the lane. `endk` (slider) sets how
    // long the merge runs along the carriageway.
    float ae = a0 + sweep;
    float Wx = Cx + c_deg(ae)*rl, Wy = Cy + s_deg(ae)*rl;   // loop end (the bell-side point)
    float aAUX = angle_to(0,0,(int)(aux*100),(int)(auy*100)); // heading down toward the highway
    float aPP  = angle_to(0,0,(int)(ppx*100),(int)(ppy*100)); // heading downstream along the carriageway
    float Mx = Wx + ppx*rl*g->endk, My = g->cy + fy*(moff - LANE_W*0.5f);  // merge, on the far carriageway lane
    float xs[RN+1], ys[RN+1];
    ramp_pts(Wx,Wy, aAUX, Mx,My, aPP, rl*0.45f, rl*0.7f*g->endk, xs,ys);   // reverse curve, width = the loop
    ribbon(xs,ys, RN+1, lw+2, CLR_DARKER_GREY);
    ribbon(xs,ys, RN+1, lw,   CLR_DARK_GREY);
    straight(Mx,My, Mx + ppx*rl*0.6f*g->endk, My + ppy*rl*0.6f*g->endk, lw, CLR_DARKER_GREY, CLR_DARK_GREY);  // run on
}

// SECOND loop (the "pink" one) — nests INSIDE loop_ramp so the two read as ONE big circular interchange.
// Tighter radius, tucked toward the trunk, bell bulging to the far side. FIRST PASS: getting the FIT next
// to the outer loop right (the stated priority) — entry/exit lanes + direction to refine from annotations.
static void loop_inner(const Geo *g, int ss) {
    float aux = ss*g->ux, auy = ss*g->uy;             // trunk axis toward the approach side
    float fx = -aux, fy = -auy;                       // toward the far side
    float ppx = -auy, ppy = aux;                      // along the highway = the bell's side
    float ri   = g->reach*0.5f;                       // tighter than the outer loop → nests inside it
    int   lw = g->bw/2; if (lw<3) lw=3;
    float moff = g->hw - LANE_W*0.5f;
    // highway-westbound → trunk-southbound (a left turn → 270° loop, bell to the far side). Entry is
    // tangent to the carriageway (heading +pp = west); exit is tangent to the trunk's SOUTHBOUND (+pp)
    // lane (heading aux = down). Centre sits just inside the outer loop's centre → the two read as one.
    float Cx = g->cx + ppx*(g->bw*0.5f + ri), Cy = g->cy + fy*(moff + ri);   // centre — nests inside the outer
    float Epx = Cx,           Epy = Cy - fy*ri;               // entry, on the far carriageway
    float Xpx = Cx - ppx*ri,  Xpy = Cy;                       // exit, on the trunk's southbound lane
    float a0 = angle_to((int)Cx,(int)Cy,(int)Epx,(int)Epy);
    float sweep = (ss>0) ? 270.f : -270.f;                    // round the long way, bell to the far side
    draw_loop(Cx, Cy, ri, a0, sweep, lw);
    // entry tail along the carriageway (upstream/east, where the westbound car comes from)
    straight(Epx,Epy, Epx - ppx*ri*0.8f, Epy - ppy*ri*0.8f, lw, CLR_DARKER_GREY, CLR_DARK_GREY);
    // exit tail down the southbound trunk lane
    straight(Xpx,Xpy, Xpx + aux*ri*0.8f, Xpy + auy*ri*0.8f, lw, CLR_DARKER_GREY, CLR_DARK_GREY);
}

// ATOM — the semi-direct FLYOVER: trunk → far carriageway, climbing straight across the highway
// (drawn over it), merging tangent to the far carriageway from the −pp side.
static void flyover(const Geo *g, int ss) {
    float aux = ss*g->ux, auy = ss*g->uy;
    float ppx = -auy;
    float thx = g->cx + aux*(g->hw+6), thy = g->cy + auy*(g->hw+6);   // throat
    float mx = g->cx - ppx*g->spread, my = g->cy - auy*(g->hw - LANE_W*0.5f);   // far carriageway merge (−pp)
    draw_ramp(thx,thy, angle_to((int)thx,(int)thy,(int)mx,(int)my),
              mx,my, (ppx>0?180.f:0.f), g->reach*0.45f, g->reach*0.5f);
}

// a STRAIGHT road A→B, subdivided into short segments (polyfill fills short quads reliably; one
// screen-long quad came out empty). Optional `casing<0` → no casing (shadow pass uses one colour).
static void straight(float ax,float ay,float bx,float by,int hw,int casing,int centre){
    float xs[25], ys[25];
    for (int i=0;i<=24;i++){ float t=(float)i/24; xs[i]=ax+(bx-ax)*t; ys[i]=ay+(by-ay)*t; }
    if (casing >= 0) road(xs, ys, 25, hw, casing, centre);
    else             ribbon(xs, ys, 25, hw, centre);
}

// dashed line along a horizontal scan-line y (the sandbox highway is horizontal)
static void hdash(int y, int col) {
    for (int x = 4; x < SCREEN_W; x += 12)
        for (int d = 0; d < 6; d++) pset(x + d, y, col);
}

// ── direction arrows: a tiny FILLED triangle centred at (x,y), tip pointing along `dir`°.
static void arrowhead(int x, int y, float dir, int sz, int col) {
    int tx = x+(int)(c_deg(dir)*sz),        ty = y+(int)(s_deg(dir)*sz);          // tip
    int lx = x+(int)(c_deg(dir+140)*sz),    ly = y+(int)(s_deg(dir+140)*sz);      // back-left
    int rx = x+(int)(c_deg(dir-140)*sz),    ry = y+(int)(s_deg(dir-140)*sz);      // back-right
    trifill(tx,ty, lx,ly, rx,ry, col);
}
// travel-direction chevrons down each lane of a STRAIGHT two-way road A→B with `nl` lanes per
// direction. Drive-on-right: the carriageway on the right of A→B (the +perp side) flows A→B, the
// other flows B→A. Lets you read which way each lane goes (so trumpet ramp merges make sense).
static void seg_arrows(float ax, float ay, float bx, float by, int nl, int col) {
    float fdir = angle_to((int)ax,(int)ay,(int)bx,(int)by);
    float dxu=c_deg(fdir), dyu=s_deg(fdir), perpx=-dyu, perpy=dxu;
    float len = fsq((bx-ax)*(bx-ax)+(by-ay)*(by-ay));
    for (int side=-1; side<=1; side+=2) {
        float td = side>0 ? fdir : fdir+180;              // +perp (right of A→B) flows A→B, −perp flows B→A
        for (int j=0; j<nl; j++) {
            float off = side*(j+0.5f)*LANE_W;
            for (float d=12; d<len-6; d+=28) {
                float x=ax+dxu*d+perpx*off, y=ay+dyu*d+perpy*off;
                if (x<3||x>SCREEN_W-3||y<3||y>SCREEN_H-3) continue;
                arrowhead((int)x,(int)y, td, 5, col);
            }
        }
    }
}
// highway markings along a STRAIGHT road A→B: a dark centre median + dashed white lane lines at
// each ±j·LANE_W offset (nl lanes per direction). The angled-road version of hdash+median.
static void lane_marks(float ax, float ay, float bx, float by, int nl) {
    float fdir=angle_to((int)ax,(int)ay,(int)bx,(int)by);
    float dxu=c_deg(fdir), dyu=s_deg(fdir), perpx=-dyu, perpy=dxu;
    float len=fsq((bx-ax)*(bx-ax)+(by-ay)*(by-ay));
    for (int j=1; j<nl; j++)                              // dashed lane separators
        for (int s=-1; s<=1; s+=2) {
            float off=s*j*LANE_W;
            for (float d=4; d<len-4; d+=12) {
                float x0=ax+dxu*d+perpx*off, y0=ay+dyu*d+perpy*off;
                float x1=ax+dxu*(d+6)+perpx*off, y1=ay+dyu*(d+6)+perpy*off;
                line((int)x0,(int)y0,(int)x1,(int)y1, CLR_WHITE);
            }
        }
    straight(ax,ay, bx,by, 1, -1, CLR_DARKER_GREY);       // centre median (thin dark ribbon)
}

void init(void) {}

void update(void) {
    // the whole junction (topology + classes + ramp-style) is picked from the on-screen matrix (one
    // click). Keys are just the modifiers that don't fit a grid: F flip, ◄► angle, L lanes, P/H toggles.
    if (keyp('F')) flip = !flip;                 // mirror the 3rd leg (3-way only)
    if (keyp('L')) lanes = lanes % 4 + 1;        // cycle highway lanes 1..4
    if (key(KEY_LEFT))  ang  -= 1.5f;
    if (key(KEY_RIGHT)) ang  += 1.5f;
    if (ang < 25) ang = 25; if (ang > 155) ang = 155;
    if (keyp('H')) show_hud = !show_hud;
    if (keyp('P')) show_panel = !show_panel;
}

void draw(void) {
    cls(CLR_DARK_GREEN);
    int CX = SCREEN_W/2, CY = SCREEN_H/2;
    float ux = c_deg(ang), uy = s_deg(ang);            // crossing-road unit dir
    float R  = 24 + s_reach*66;                        // ramp reach (px), from the slider
    float goreM = 1.0f + s_gore*1.0f, taperM = 0.5f + s_taper*1.3f, runonM = 0.4f + s_runon*1.0f;
    int HW = rwidth(clsA);                             // road A (the through road) half-width
    int BW = rwidth(clsB);                             // road B (crossing / branch) half-width
    int graded = (clsA==C_HIGHWAY || clsB==C_HIGHWAY); // any highway → grade-separated; else at-grade
    int tee = (topo==TOPO_TEE), wye = (topo==TOPO_WYE);
    int ss  = flip ? 1 : -1;                           // 3-way: which side the 3rd leg sits on
    float far = (float)(SCREEN_W + SCREEN_H);
    // atom context — `spread` (highway-end ramp offset) = R·goreM, `gore` (trunk-end offset) = BW·0.35,
    // i.e. the `gore` slider drives the side-spacing as before (the spread/gore split was reverted).
    Geo g = { CX, CY, ux, uy, HW, BW, R, R*goreM, BW*0.35f, taperM, runonM, 0.4f + s_loopend*1.4f, ang };

    // 1. ROAD A (horizontal through road), under everything; highway gets median + lane lines
    straight(0, CY, SCREEN_W, CY, HW+2, CLR_DARKER_GREY, clsA==C_HIGHWAY?CLR_LIGHT_GREY:CLR_MEDIUM_GREY);
    if (clsA == C_HIGHWAY) lane_marks(0, CY, SCREEN_W, CY, lanes);
    seg_arrows(0, CY, SCREEN_W, CY, clsA==C_HIGHWAY?lanes:1, CLR_YELLOW);   // travel direction

    // 2. RAMPS — composed from atoms. `near_pair` = the half-diamond fan; the diamond is just it on
    //    both carriageways. (AR×AR crosses at grade; WYE has none yet.)
    if (graded && !wye && (itype == T_DIAMOND || itype == T_CLOVERLEAF) && show_near) {
        if (tee) near_pair(&g, ss);                          // half-diamond — near carriageway only
        else { near_pair(&g, -1); near_pair(&g, +1); }       // full diamond — both carriageways
    }
    if (graded && !wye && itype == T_CLOVERLEAF && show_loop) {
        for (int sx=-1; sx<=1; sx+=2) for (int sy=-1; sy<=1; sy+=2) {
            if (tee && sy != ss) continue;
            float cx = CX + sx*R*0.6f, cy = CY + sy*R*0.6f;
            float lp[28], lq[28]; int n=0;
            for (int i=0;i<=26;i++){ float a = (float)i/26*300 + (sx*sy>0?40:220);
                lp[n]=cx + c_deg(a)*R*0.55f; lq[n]=cy + s_deg(a)*R*0.55f; n++; }
            road(lp, lq, n, 5, CLR_DARKER_GREY, CLR_DARK_GREY);
        }
    }
    // (TRUMPET ramps now drawn AFTER road B — see below — so the loop/fan read on top of the trunk
    //  bridge instead of being buried under it.)

    // 3. ROAD B / the 3rd leg, drawn OVER A (overpass shadow if grade-separated; at-grade = no shadow)
    int dcol = clsB==C_HIGHWAY ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY;
    if (wye) {                                           // Y-SPLIT: road A forks — a branch peels off.
        // The branch DIVERGES from the highway: it starts tangent on the road surface, curves out to
        // the fork angle, then runs straight. A fork is not a crossing, so it stays at-grade (no
        // overpass) even for HW×AR — and it must never spear the centreline (the old bug). The grass
        // wedge left between the highway and the branch reads as the gore.
        float bd  = ss * ang * 0.45f;                    // fork angle on the ss side — shallow (a fork,
                                                         // not a T); the shared angle slider remapped
        float fux = c_deg(bd), fuy = s_deg(bd);
        float ax  = CX - R*goreM*0.5f, ay = CY + ss*(HW*0.4f);   // split point, on the road surface
        float bx  = CX + fux*R,        by = CY + fuy*R;          // where the curve reaches full fork dir
        float xs[RN+1], ys[RN+1];
        ramp_pts(ax,ay,0, bx,by,bd, R*runonM, R*taperM, xs,ys);  // tangent-to-highway → fork direction
        float ex = CX + fux*far, ey = CY + fuy*far;              // straight continuation out
        road(xs, ys, RN+1, BW, CLR_DARKER_GREY, dcol);
        straight(bx,by, ex,ey, BW, CLR_DARKER_GREY, dcol);
    } else {                                             // CROSS (full) or T-SPLIT (stub on ss side)
        // graded T-split: the trunk BEGINS where the ramps merge into it (cd = HW+R, with a small
        // overlap so the join is seamless) — it must NOT punch down to the highway centreline. The
        // at-grade T-intersection (AR×AR) still meets the centreline, where a T should.
        // trumpet: the trunk runs up to the LOOP ENTRY so it forms the loop's straight side (candy-cane).
        // Must match loop_ramp's entry height: moff + rl = (HW - LANE_W/2) + reach·0.8, on the far side.
        float n0 = (tee && graded) ? (itype==T_TRUMPET ? -((HW - LANE_W*0.5f) + R*0.8f) : (HW + R - BW)) : 0;
        float b0x = tee ? CX + ss*ux*n0 : CX - ux*far;
        float b0y = tee ? CY + ss*uy*n0 : CY - uy*far;
        float b1x = CX + (tee?ss:1)*ux*far, b1y = CY + (tee?ss:1)*uy*far;
        if (graded) straight(b0x,b0y, b1x,b1y, BW+3, -1, CLR_BROWNISH_BLACK);
        straight(b0x,b0y, b1x,b1y, BW, CLR_DARKER_GREY, dcol);
        if (clsB == C_HIGHWAY) lane_marks(b0x,b0y, b1x,b1y, lanes);   // crossing highway gets lanes too
        seg_arrows(b0x,b0y, b1x,b1y, clsB==C_HIGHWAY?lanes:1, CLR_YELLOW);
    }

    // TRUMPET ramps — drawn LAST so the loop/fan sit ON TOP of the trunk bridge (legible). Composed
    // from the atoms: half-diamond base + teardrop LOOP + semi-direct FLYOVER reaching the far carriageway.
    if (graded && tee && itype == T_TRUMPET) {
        if (show_near) near_pair(&g, ss);                  // base: trunk ↔ near carriageway
        if (show_fly)  flyover(&g, ss);                    // semi-direct: trunk → far carriageway
        if (show_loop) loop_ramp(&g, ss);                  // OUTER loop: trunk → far carriageway (west)
        if (show_loop) loop_inner(&g, ss);                 // INNER loop: far carriageway (west) → trunk (south)
    }

    {                                                  // live tuning: ramp sliders + the junction matrix
        const int MBW = 30, BH = 11, MX = 3, MY = 89;  // matrix button width/height/origin (below the sliders)
        const char *tip = NULL;
        int abx = -99, aby = -99;
        font(FONT_SMALL);
        if (show_panel) { fillp(FILL_CHECKER, -1); rectfill(0, 12, 130, 118, CLR_BLACK); fillp_reset(); }
        ui_begin();
        // always-visible toggle (left, in the panel area): hide the panel for a clean screenshot, click to restore
        if (ui_button(3, 14, 28, 10, show_panel ? "UI -" : "UI +")) show_panel = !show_panel;
        if (show_panel) {
            // atom-isolation toggles — show only part of a composed junction (green frame = on)
            int *av[3] = { &show_near, &show_loop, &show_fly };
            const char *al[3] = { "near", "loop", "fly" };
            for (int k=0; k<3; k++) { int bx = 34 + k*32;
                if (ui_button(bx, 14, 30, 10, al[k])) *av[k] = !*av[k];
                if (*av[k]) rect(bx-1, 13, 32, 12, CLR_GREEN);
            }
            ui_slider(&s_reach,   3, 27, 122, "reach");
            ui_slider(&s_gore,    3, 39, 122, "gore");
            ui_slider(&s_taper,   3, 51, 122, "taper");
            ui_slider(&s_runon,   3, 63, 122, "run-on");
            ui_slider(&s_loopend, 3, 75, 122, "loop end");
            // the matrix — EVERY supported junction is one button. Row = topology, columns = that row's
            // variants (ragged). One click sets topology + both classes + ramp-style.
            int col[TOPO_COUNT] = {0};
            for (int i = 0; i < NJUNCS; i++) {
                const Junc *j = &JUNCS[i];
                int bx = MX + col[j->topo]++ * (MBW+1), by = MY + j->topo*(BH+1);
                if (ui_button(bx, by, MBW, BH, j->name)) {
                    topo = j->topo; clsA = j->ca; clsB = j->cb; itype = j->it;
                }
                if (ui_hover(bx, by, MBW, BH)) tip = j->tip;
                if (!j->built) pset(bx+MBW-2, by+1, CLR_RED);   // stub/draft marker
                if (topo==j->topo && clsA==j->ca && clsB==j->cb && itype==j->it) { abx=bx; aby=by; }
            }
        }
        ui_end();
        if (show_panel) {
            if (abx > -99) rect(abx-1, aby-1, MBW+2, BH+2, CLR_YELLOW);   // frame the active junction
            if (tip) {                                            // hover tooltip — bottom strip
                rectfill(0, SCREEN_H-18, SCREEN_W, 8, CLR_BLACK);
                print_centered(tip, SCREEN_W/2, SCREEN_H-17, CLR_WHITE);
            }
        }
        font(FONT_NORMAL);
    }
    if (show_hud) {
        char buf[56]; const char *cn[2] = { "HW", "AR" };
        rectfill(0,0,SCREEN_W,11,CLR_BLACK);
        const char *jname = (topo==TOPO_WYE) ? "WYE (fork)"
                          : !graded ? "at-grade"
                          : (tee && itype==T_TRUMPET) ? "TRUMPET"
                          : tee     ? "HALF-DIAMOND"   // the partial T ramp-style
                          : TNAME[itype];
        snprintf(buf,sizeof buf,"%s  %sx%s  %s", PNAME[topo], cn[clsA], cn[clsB], jname);
        print(buf, 4, 2, CLR_LIGHT_GREY);
        font(FONT_SMALL);
        print_centered("click a junction  \x1a\x1b angle  F flip  L lanes  P panel", SCREEN_W/2, SCREEN_H-8, CLR_DARK_GREY);
        font(FONT_NORMAL);
    }
}
