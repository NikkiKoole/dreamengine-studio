// linesym — micro-probe: is the ENGINE's line()/polyfill() reflection-symmetric when fed
// EXACTLY mirrored integer coords? (double-rounding removed by construction: we mirror the
// already-integer endpoints, so any mismatch is the rasteriser's own handedness, not ri()).
// Axis = x=160 on a 320-wide screen (even width, mirror maps pixel x -> 319-x).
// Run: node tools/mirror-diff.js linesym   (0 = symmetric; >0 = the rasteriser is handed).
#include "studio.h"
#include <math.h>

#define MIR(x) (319 - (x))

// Plot the minor coordinate `v` (rounded) at major position `maj`, where `horiz` says
// which is x. Ties (exact .5) break TOWARD `mid` — reflection-invariant: under c↦K-c the
// value, mid and comparison all flip together, so rounding flips to match (any parity).
// The ONE degenerate is v == mid exactly (tie AT the midpoint): "toward mid" has nowhere
// to point, so plot BOTH candidates — {k,k+1} mirrors to {K-k-1,K-k}, symmetric by
// construction (a ≤1px nub at the dead centre of a segment whose centre is half-integer).
static void plot_minor(int maj, float v, float mid, int horiz, int c){
    float f = floorf(v); int fi = (int)f; float r = v - f; int m;
    if (r != 0.5f) { m = (r < 0.5f) ? fi : fi + 1; }
    else if (v == mid) { if (horiz) { pset(maj,fi,c); pset(maj,fi+1,c); } else { pset(fi,maj,c); pset(fi+1,maj,c); } return; }
    else m = (mid > v) ? fi + 1 : fi;
    if (horiz) pset(maj, m, c); else pset(m, maj, c);
}
// reflection-symmetric software line: iterate the MAJOR axis over its integer range,
// round the minor coordinate (ties → minor-axis midpoint). Per-column / per-row sampling,
// no direction-dependent error term ⇒ a line and its mirror rasterise to mirror-identical
// pixel sets, about EITHER axis, regardless of grid parity.
static void sline(int x0,int y0,int x1,int y1,int c){
    int dx = x1 - x0, dy = y1 - y0;
    int adx = dx<0?-dx:dx, ady = dy<0?-dy:dy;
    if (adx == 0 && ady == 0){ pset(x0,y0,c); return; }
    if (adx >= ady){                                   // x-major: per column, round y
        int lo = x0<x1?x0:x1, hi = x0<x1?x1:x0; float ymid = (y0 + y1) * 0.5f;
        for (int x = lo; x <= hi; x++)
            plot_minor(x, y0 + (float)(x - x0) * dy / (float)dx, ymid, 1, c);
    } else {                                           // y-major: per row, round x
        int lo = y0<y1?y0:y1, hi = y0<y1?y1:y0; float xmid = (x0 + x1) * 0.5f;
        for (int y = lo; y <= hi; y++)
            plot_minor(y, x0 + (float)(y - y0) * dx / (float)dy, xmid, 0, c);
    }
}

static void seg(int x0,int y0,int x1,int y1,int c){
    sline(x0,y0,x1,y1,c);                      // left copy  (was line() = GPU DrawLine)
    sline(MIR(x0),y0,MIR(x1),y1,c);            // exact integer mirror (same vertex order)
}

// reflection-symmetric even-odd scanline fill — same coverage as poly_fill_cov EXCEPT the
// right span edge is CLOSED (b = floor(cR-0.5), not ceil(cR-0.5)-1). Identical everywhere
// but exact-centre crossings, where it keeps the boundary pixel ⇒ symmetric, errs to 1px
// overlap (hidden) not a gap (visible). The whole asymmetry was that one right-open edge.
static void sfill(int *xy, int n, int color){
    int y0=99999, y1=-99999;
    for (int i=0;i<n;i++){ int y=xy[i*2+1]; if(y<y0)y0=y; if(y>y1)y1=y; }
    for (int y=y0; y<=y1; y++){
        float yc=y+0.5f, cr[64]; int m=0;
        for (int i=0,j=n-1; i<n; j=i++){
            float yi=xy[i*2+1], yj=xy[j*2+1];
            if ((yi>yc)!=(yj>yc)){ float xi=xy[i*2], xj=xy[j*2];
                cr[m++]=(xj-xi)*(yc-yi)/(yj-yi)+xi; }
        }
        for (int a=1;a<m;a++){ float v=cr[a]; int b=a-1; while(b>=0&&cr[b]>v){cr[b+1]=cr[b];b--;} cr[b+1]=v; }
        for (int t=0;t+1<m;t+=2){
            int a=(int)ceilf(cr[t]-0.5f);          // first centre >= cL   (unchanged)
            int b=(int)ceilf(cr[t+1]-0.5f)-1;      // TEST: original half-open right edge
            for (int x=a;x<=b;x++) pset(x,y,color);
        }
    }
}

void draw(void){
    cls(CLR_BLACK);
    // a fan of slopes through line() — the kerb stroke is short diagonals like these
    seg(40,40, 70,55,  CLR_WHITE);
    seg(30,80, 75,92,  CLR_YELLOW);
    seg(45,120,60,150, CLR_GREEN);
    seg(50,160,90,168, CLR_BROWNISH_BLACK);
    seg(20,30, 78,34,  CLR_BROWN);

    // a filled polygon through polyfill() + its exact integer mirror (a quarter-arc wedge)
    int wx[8]={100,118,124,128,130,131,131,100};
    int wy[8]={100,101,104,109,116,124,140,140};
    int L[16], Rr[16];
    // fill vertices are CONTINUOUS points → reflect about the true centre 160 (320-x),
    // NOT the cell mirror 319-x that lines use. (mirror-diff pairs cells x<->319-x, whose
    // CENTRES average to 160 — so symmetric geometry must reflect about 160.)
    for (int i=0;i<8;i++){ L[i*2]=wx[i]; L[i*2+1]=wy[i]; Rr[i*2]=320-wx[i]; Rr[i*2+1]=wy[i]; }
    sfill(L,8,CLR_DARK_GREY);
    sfill(Rr,8,CLR_DARK_GREY);
}
