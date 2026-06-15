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

enum { T_OVERPASS, T_DIAMOND, T_CLOVERLEAF, T_COUNT };   // ramp STYLE (grade-separated only)
static const char *TNAME[T_COUNT] = { "OVERPASS", "DIAMOND", "CLOVERLEAF" };
static int   itype = T_DIAMOND;
enum { TOPO_CROSS, TOPO_TEE, TOPO_WYE, TOPO_COUNT };    // junction TOPOLOGY (the leg layout)
static const char *PNAME[TOPO_COUNT] = { "CROSS", "T-SPLIT", "Y-SPLIT" };
static int   topo = TOPO_CROSS;
static int   flip = 0;          // 3-way junctions: which side the 3rd leg is on
static float ang   = 90.0f;     // crossing road angle (deg from highway)
static int   show_hud = 1, show_panel = 1;
// tunable ramp shape — slider-normalised 0..1, mapped to real values in draw() (tune by feel,
// then bake the chosen values as constants when porting the drawer into roadnet2):
static float s_reach = 0.33f;   //  R       reach  24..90 px
static float s_gore  = 0.40f;   //  gore    divergence point along highway, ×R 1.0..2.0
static float s_taper = 0.46f;   //  kA      off-ramp taper run, ×R 0.5..1.8
static float s_runon = 0.45f;   //  kB      run-on along the overpass, ×R 0.4..1.4

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

void init(void) {}

void update(void) {
    if (keyp('T') || keyp(KEY_TAB)) itype = (itype + 1) % T_COUNT;
    if (keyp('Y')) topo = (topo + 1) % TOPO_COUNT;   // cross / T-split / Y-split
    if (keyp('F')) flip = !flip;                 // mirror the 3rd leg (3-way only)
    if (keyp('L')) lanes = lanes % 4 + 1;        // cycle highway lanes 1..4
    if (keyp('1')) clsA = !clsA;                 // toggle road A class (highway/arterial)
    if (keyp('2')) clsB = !clsB;                 // toggle road B class
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

    // 1. ROAD A (horizontal through road), under everything; highway gets median + lane lines
    straight(0, CY, SCREEN_W, CY, HW+2, CLR_DARKER_GREY, clsA==C_HIGHWAY?CLR_LIGHT_GREY:CLR_MEDIUM_GREY);
    if (clsA == C_HIGHWAY) {
        for (int j = 1; j < lanes; j++) { hdash(CY - j*LANE_W, CLR_WHITE); hdash(CY + j*LANE_W, CLR_WHITE); }
        rectfill(0, CY-1, SCREEN_W, 2, CLR_DARKER_GREY);   // centre median
    }

    // 2. RAMPS — grade-separated cross/T only (AR×AR crosses at grade; WYE has none yet)
    if (graded && !wye && (itype == T_DIAMOND || itype == T_CLOVERLEAF)) {
        float px=-uy, py=ux;
        for (int sx=-1; sx<=1; sx+=2) for (int sy=-1; sy<=1; sy+=2) {
            if (tee && sy != ss) continue;                   // T-split: ramps only on the 3rd-leg side
            float hx = CX + sx*R*goreM, hy = CY + sy*(HW - LANE_W*0.5f);
            float cd = HW + R;
            float ax = CX + ux*sy*cd - px*sx*(BW*0.35f);
            float ay = CY + uy*sy*cd - py*sx*(BW*0.35f);
            draw_ramp(hx,hy, (sx>0?180:0), ax,ay, (sy>0?ang:ang+180), R*taperM, R*runonM);
        }
    }
    if (graded && !wye && itype == T_CLOVERLEAF) {
        for (int sx=-1; sx<=1; sx+=2) for (int sy=-1; sy<=1; sy+=2) {
            if (tee && sy != ss) continue;
            float cx = CX + sx*R*0.6f, cy = CY + sy*R*0.6f;
            float lp[28], lq[28]; int n=0;
            for (int i=0;i<=26;i++){ float a = (float)i/26*300 + (sx*sy>0?40:220);
                lp[n]=cx + c_deg(a)*R*0.55f; lq[n]=cy + s_deg(a)*R*0.55f; n++; }
            road(lp, lq, n, 5, CLR_DARKER_GREY, CLR_DARK_GREY);
        }
    }

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
        float n0 = (tee && graded) ? (HW + R - BW) : 0;
        float b0x = tee ? CX + ss*ux*n0 : CX - ux*far;
        float b0y = tee ? CY + ss*uy*n0 : CY - uy*far;
        float b1x = CX + (tee?ss:1)*ux*far, b1y = CY + (tee?ss:1)*uy*far;
        if (graded) straight(b0x,b0y, b1x,b1y, BW+3, -1, CLR_BROWNISH_BLACK);
        straight(b0x,b0y, b1x,b1y, BW, CLR_DARKER_GREY, dcol);
    }

    if (show_panel) {                                  // live tuning: sliders + toggle buttons
        font(FONT_SMALL);
        fillp(FILL_CHECKER, -1); rectfill(0, 12, 66, 102, CLR_BLACK); fillp_reset();
        ui_begin();
        ui_slider(&s_reach, 3, 16, 58, "reach");
        ui_slider(&s_gore,  3, 28, 58, "gore");
        ui_slider(&s_taper, 3, 40, 58, "taper");
        ui_slider(&s_runon, 3, 52, 58, "run-on");
        if (ui_button(3, 66, 58, 10, PNAME[topo]))            topo = (topo+1) % TOPO_COUNT;
        if (ui_button(3, 78, 58, 10, flip ? "flip \x1a" : "flip \x1b")) flip = !flip;
        if (ui_button(3, 90, 27, 10, clsA==C_HIGHWAY?"A:HW":"A:AR")) clsA = !clsA;
        if (ui_button(34,90, 27, 10, clsB==C_HIGHWAY?"B:HW":"B:AR")) clsB = !clsB;
        if (ui_button(3,102, 58, 10, TNAME[itype]))           itype = (itype+1) % T_COUNT;
        ui_end();
        font(FONT_NORMAL);
    }
    if (show_hud) {
        char buf[56]; const char *cn[2] = { "HW", "AR" };
        rectfill(0,0,SCREEN_W,11,CLR_BLACK);
        const char *jname = (topo==TOPO_WYE) ? "WYE (fork)" : (graded ? TNAME[itype] : "at-grade");
        snprintf(buf,sizeof buf,"%s  %sx%s  %s", PNAME[topo], cn[clsA], cn[clsB], jname);
        print(buf, 4, 2, CLR_LIGHT_GREY);
        print_centered("click toggles \x10 or: 1/2 Y F T L  \x1a\x1b angle  P panel", SCREEN_W/2, SCREEN_H-9, CLR_DARK_GREY);
    }
}
