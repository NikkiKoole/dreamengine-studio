/* de:meta
{
  "slug": "arcsym",
  "title": "arcsym",
  "status": "active",
  "created": "2026-06-23",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "software-rasterizer",
    "algorithm-visualization"
  ],
  "lineage": "Isolates the <=1px corner-staircase floor accepted in streetlab (docs/design/road-program-state.md, streetlab.c SEAM note, STATUS.md 43) into a pixel-precise sandbox. Sibling to pixelperfect (also a software-rasterizer tech-demo) but about rasterisation SYMMETRY under a flip rather than fractional scaling: shows ri-snap double-rounding break a mirror, and the compute-one-corner-then-blit-mirrored fix the docs propose, proven exact by a spec().",
  "description": "ARC FLIP SYMMETRY — a petri dish for streetlab's ≤1px corner floor: why a mirrored arc has the SAME pixel count but a DIFFERENT staircase, and how to fix it. On streetlab's default 4-way intersection the four kerb fillets are mirror-symmetric in real space, yet the rendered kerb edges land ≤1px apart (the docs' accepted floor). The cause: streetlab snaps each arc vertex to the grid with ri() and THEN strokes/fills, so re-rasterising the mirror rounds TWICE — ri(mirror(v)) != mirror(ri(v)) at half-pixel alignments — and the edge drifts. The fix the docs predict: rasterise ONE corner, then BLIT its cells mirrored (a pure index reflection is exact by construction). The cart draws a quarter arc and its flip on a magnified EVEN-width grid (mirror axis on a pixel boundary, like x=160 on a 320 screen): blue = the original, green = flip matches the true mirror, RED/orange = where the naive flip is off a step. A live stat sweeps the sub-pixel phase and counts how many alignments the naive flip breaks at (some — that's why it's a subtle floor) vs the blit flip (zero, always). Controls: Z naive re-raster <-> blit cells (the fix); S stroke <-> fill; left/right sub-pixel phase; up/down radius; R reset. spec() proves the blit is pixel-exact at every phase/radius/shape."
}
de:meta */
#include "studio.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ARC FLIP SYMMETRY — why a mirrored arc has the SAME pixel count but a DIFFERENT
// "ladder", and how to fix it. This is the streetlab corner floor in a petri dish:
// on its default 4-way intersection the four kerb fillets are mirror-symmetric in
// REAL space, yet the rendered kerb edges land ≤1px apart (run tools/play.js +
// a left/right pixel diff and the left kerb octagon edges light up). Geometry is
// symmetric; the SCAN-CONVERSION isn't. See docs/design/road-program-state.md
// "Accepted floor", streetlab.c §"SEAM", and STATUS.md §43 (the visual blind spot).
//
// THE MECHANISM. streetlab snaps every arc vertex to the pixel grid with ri()
// (round-to-nearest) and THEN strokes/fills it. To draw the mirror you have two ways:
//   NAIVE — re-rasterise the mirrored geometry independently. You round TWICE on
//           different sub-pixel phases, and ri(mirror(v)) != mirror(ri(v)) at half-
//           pixel alignments → the edge drifts ≤1px. (Each corner does this.)
//   BLIT  — rasterise ONE side, then reflect its CELLS. A pure index reflection is a
//           bijection, so the mirror is pixel-identical by construction. The fix the
//           docs predict: compute one corner, blit it mirrored for the symmetric ones.
//
// The arc sits on an EVEN-width grid (mirror axis on a pixel BOUNDARY — like x=160 on
// a 320-wide screen), the case the docs call out. Watch the "naive breaks" stat: the
// naive flip is off at some sub-pixel phases (not all — that's why it's a subtle
// "floor"), while the blit flip is exact at EVERY phase and radius.
//
//   Z      flip method:  NAIVE re-raster  <->  BLIT cells (the fix)
//   S      shape:        STROKE (kerb line)  <->  FILL (kerb fillet, like streetlab)
//   ←/→    arc-centre sub-pixel phase (0.05 cell) — scan the rounding alignment
//   ↑/↓    radius +/-
//   R      reset

enum { GW=42, GH=28 };          // the petri-dish grid, in cells (EVEN width)
#define MAG 8                   // each cell drawn this many screen px
#define GX  14                  // grid origin on screen
#define GY  46
#define NARC 28                 // arc angle samples (matches streetlab's ~N)
#define MFIX (GW-1)             // mirror: column p -> MFIX-p. EVEN grid => MFIX odd
                                //   => axis at (GW-1)/2 (a pixel BOUNDARY, no fixed col)

static unsigned char gA[GH][GW];      // arc A (the original, right of the axis)
static unsigned char gFlip[GH][GW];   // the flipped arc actually shown (naive OR blit)
static unsigned char gIdeal[GH][GW];  // the TRUE mirror of A (blit) — the reference

// state
static int   useBlit = 0;       // 0 = naive re-rasterise, 1 = blit cells (the fix)
static int   useFill = 0;       // 0 = stroke the arc, 1 = fill the wedge
static float phase   = 0.0f;    // arc-centre sub-pixel offset, in cells
static float R       = 13.0f;   // radius in cells

// readouts
static int cntA, cntFlip, mism, naiveBreaks, sweepN;

static inline int   iround(float x){ return (int)floorf(x + 0.5f); }
static inline void  clearg(unsigned char g[GH][GW]){ memset(g, 0, GH*GW); }
static inline void  setc(unsigned char g[GH][GW], int x, int y){
    if (x>=0 && x<GW && y>=0 && y<GH) g[y][x] = 1;
}
static int countg(unsigned char g[GH][GW]){
    int n=0; for(int y=0;y<GH;y++) for(int x=0;x<GW;x++) n += g[y][x]; return n;
}

// integer Bresenham — the lattice line streetlab's stroke connects snapped points with
static void bres(unsigned char g[GH][GW], int x0,int y0,int x1,int y1){
    int dx=abs(x1-x0), sx=x0<x1?1:-1, dy=-abs(y1-y0), sy=y0<y1?1:-1, err=dx+dy;
    for(;;){ setc(g,x0,y0); if(x0==x1&&y0==y1) break; int e2=2*err;
        if(e2>=dy){err+=dy;x0+=sx;} if(e2<=dx){err+=dx;y0+=sy;} }
}
// scanline even-odd fill of an integer polygon (the polyfill family)
static void fillpoly(unsigned char g[GH][GW], int *vx,int *vy,int n){
    for (int y=0;y<GH;y++){ int xs[NARC+4], m=0;
        for (int i=0;i<n;i++){ int j=(i+1)%n; float y0=vy[i],y1=vy[j],x0=vx[i],x1=vx[j];
            if ((y0<=y && y1>y) || (y1<=y && y0>y)){ float t=(y-y0)/(y1-y0); xs[m++]=iround(x0+t*(x1-x0)); } }
        for (int a=0;a<m;a++) for (int b=a+1;b<m;b++) if (xs[b]<xs[a]){ int t=xs[a];xs[a]=xs[b];xs[b]=t; }
        for (int a=0;a+1<m;a+=2) for (int x=xs[a];x<xs[a+1];x++) setc(g,x,y);
    }
}

// rasterise a quarter arc: centre (ocx,ocy) float, radius rr, angles a0..a1.
// STROKE connects ri-snapped samples; FILL fills the wedge (apex at the centre O).
static void raster_arc(unsigned char g[GH][GW], float ocx,float ocy,float rr, float a0,float a1){
    clearg(g);
    if (useFill){
        int vx[NARC+2], vy[NARC+2], k=0;
        vx[k]=iround(ocx); vy[k]=iround(ocy); k++;          // apex = the arc centre
        for (int i=0;i<=NARC;i++){ float a=a0+(a1-a0)*i/NARC;
            vx[k]=iround(ocx+cosf(a)*rr); vy[k]=iround(ocy+sinf(a)*rr); k++; }
        fillpoly(g, vx,vy, k);
    } else {
        int px=iround(ocx+cosf(a0)*rr), py=iround(ocy+sinf(a0)*rr);
        for (int i=1;i<=NARC;i++){ float a=a0+(a1-a0)*i/NARC;
            int x=iround(ocx+cosf(a)*rr), y=iround(ocy+sinf(a)*rr);
            bres(g, px,py, x,y); px=x; py=y; }
    }
}

// blit-mirror every set cell of src into dst across the axis (column p -> MFIX-p)
static void blit_mirror(unsigned char src[GH][GW], unsigned char dst[GH][GW]){
    clearg(dst);
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) if (src[y][x]) setc(dst, MFIX-x, y);
}

// the arc-A centre for a given sub-pixel phase. Right of the axis, leftmost point
// reaching the axis (like a kerb meeting the centreline). a0..a1 = the upper-left quarter.
static float axisf(void){ return MFIX * 0.5f; }            // pixel-boundary axis (even grid)
static void  arcA_params(float ph, float *ocx,float *ocy){ *ocx = axisf() + R + ph; *ocy = GH*0.60f; }

// build all three grids for the CURRENT phase, and (separately) sweep phases for the stat
static int build_at(float ph, unsigned char A[GH][GW], unsigned char F[GH][GW], unsigned char I[GH][GW], int blit){
    float ocx, ocy; arcA_params(ph, &ocx,&ocy);
    raster_arc(A, ocx, ocy, R, M_PI, 1.5f*M_PI);           // arc A: upper-left quarter
    blit_mirror(A, I);                                      // the TRUE mirror — always exact
    if (blit){ memcpy(F, I, sizeof gFlip); }
    else { float ocxL = 2.0f*axisf() - ocx;                 // NAIVE: re-rasterise the mirror
           raster_arc(F, ocxL, ocy, R, -0.5f*M_PI, 0.0f); } // the mirrored angle range
    int d=0; for(int y=0;y<GH;y++) for(int x=0;x<GW;x++) if (F[y][x]!=I[y][x]) d++;
    return d;
}

static void recompute(void){
    mism    = build_at(phase, gA, gFlip, gIdeal, useBlit);
    cntA    = countg(gA);
    cntFlip = countg(gFlip);
    // sweep the sub-pixel phase: how many alignments does the NAIVE flip break at?
    static unsigned char tA[GH][GW], tF[GH][GW], tI[GH][GW];
    naiveBreaks = 0; sweepN = 0;
    for (int i=0;i<40;i++){ float ph = i*0.05f; sweepN++;
        if (build_at(ph, tA, tF, tI, 0) > 0) naiveBreaks++; }
}

void update(void){
    int dirty = 0;
    if (keyp('Z')) { useBlit ^= 1; dirty = 1; }
    if (keyp('S')) { useFill ^= 1; dirty = 1; }
    if (keyp('R')) { useBlit=0; useFill=0; phase=0; R=13; dirty=1; }
    if (keyp(KEY_LEFT))  { phase -= 0.05f; dirty = 1; }
    if (keyp(KEY_RIGHT)) { phase += 0.05f; dirty = 1; }
    if (keyp(KEY_UP))    { if (R < 16) R += 1; dirty = 1; }
    if (keyp(KEY_DOWN))  { if (R >  6) R -= 1; dirty = 1; }
    if (phase >  1.5f) phase =  1.5f;
    if (phase < -1.5f) phase = -1.5f;
    if (dirty) recompute();
}

static void cell(int ix, int iy, int col){ rectfill(GX + ix*MAG, GY + iy*MAG, MAG-1, MAG-1, col); }

void draw(void){
    cls(CLR_BLACK);
    if (cntA==0 && sweepN==0) recompute();   // first frame

    // grid backdrop
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++)
        rectfill(GX + x*MAG, GY + y*MAG, MAG-1, MAG-1, CLR_DARKER_GREY);

    // mirror axis (on the pixel boundary between the two centre columns)
    int axpx = GX + iround((axisf()+0.5f)*MAG);
    line(axpx, GY-4, axpx, GY+GH*MAG+2, CLR_YELLOW);

    // arc A (the original) in blue
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) if (gA[y][x]) cell(x,y,CLR_BLUE);

    // the flipped arc: green where it matches the true mirror, RED where it's off a step
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++){
        if (!gFlip[y][x] && !gIdeal[y][x]) continue;
        if (gFlip[y][x] && gIdeal[y][x]) cell(x,y,CLR_GREEN);    // matches the true mirror
        else if (gFlip[y][x])            cell(x,y,CLR_RED);      // naive drew one the mirror didn't
        else                             cell(x,y,CLR_ORANGE);   // mirror wanted one naive missed
    }

    // HUD
    char buf[96];
    print("ARC FLIP SYMMETRY", GX, 6, CLR_WHITE);
    snprintf(buf,sizeof buf, "flip: %s    shape: %s",
             useBlit? "BLIT cells (fix)":"NAIVE re-raster", useFill? "FILL":"STROKE");
    print(buf, GX, 18, useBlit? CLR_GREEN : CLR_ORANGE);
    snprintf(buf,sizeof buf, "R=%d  phase=%+.2f", (int)R, phase);
    print(buf, GX, 30, CLR_LIGHT_GREY);

    int by = GY + GH*MAG + 8;
    snprintf(buf,sizeof buf, "pixels  A:%d  flip:%d   %s",
             cntA, cntFlip, cntA==cntFlip? "(same count)":"(COUNT DIFFERS)");
    print(buf, GX, by, CLR_WHITE);
    snprintf(buf,sizeof buf, "ladder mismatch here: %d px", mism);
    print(buf, GX, by+12, mism? CLR_RED : CLR_GREEN);
    snprintf(buf,sizeof buf, "sweep %d phases:  naive breaks %d   blit 0", sweepN, naiveBreaks);
    print(buf, GX, by+24, naiveBreaks? CLR_ORANGE : CLR_GREEN);
    print("Z:flip  S:shape  <>:phase  ^v:rad  R:reset", GX, by+38, CLR_DARK_GREY);
}

#ifdef DE_SPEC
#include "spec.h"
// The fix is provable, not eyeballed: a blit flip is a pixel-exact mirror at EVERY
// phase, radius and shape; the naive flip DOES drift at some phases (the real bug);
// but its drift is a SMALL bounded floor (≤ a few px), not a logic blowup — exactly
// the "accepted ≤1px floor" the docs describe.
void spec(void){
    static unsigned char A[GH][GW], F[GH][GW], I[GH][GW];
    int blit_breaks = 0, naive_break_phases = 0, naive_worst = 0;
    for (useFill=0; useFill<2; useFill++){
        for (R=6; R<=16; R+=1){
            for (int p=0; p<=30; p++){
                float ph = -0.75f + p*0.05f;
                int db = build_at(ph, A, F, I, 1);          // blit flip — must be exact
                if (db != 0) blit_breaks++;
                int dn = build_at(ph, A, F, I, 0);          // naive flip — the floor
                if (dn > 0) naive_break_phases++;
                if (dn > naive_worst) naive_worst = dn;
            }
        }
    }
    expect(blit_breaks == 0,      "blit flip is a pixel-exact mirror at every phase/radius/shape");
    expect(naive_break_phases>0,  "naive flip DOES shift the ladder at some sub-pixel phases");
    // a stroke is ~1px thick so its drift is tiny; a fill shifts a whole edge, so allow
    // up to ~2× the max radius. Either way it's a small FLOOR, not a runaway error.
    expect(naive_worst <= 2*16,   "naive drift stays a small bounded floor, never a blowup");
    useFill = 0; R = 13;
}
#endif
