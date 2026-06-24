#include "studio.h"
#include <math.h>
#include <stdio.h>

// SKEWLAB — clean 1px road edges at any SKEW or CURVE (petri-dish for streetlab's edge issues).
//
// A road is a UNION of straight capsule segments — 2 long crossing arms (skew mode) or many
// short chords along a wiggly centreline (curve/Perlin mode). Two ways to draw the kerb:
//
//   PER-PIECE casing (what streetlab does): each segment draws a 1px-proud BROWNISH_BLACK
//     casing then DARK_GREY asphalt, piece by piece. Where pieces OVERLAP (a crossing, or every
//     bend of a curve) one piece's casing lands inside another's asphalt → interior dark
//     STRAYS + overpainted (missing) edges. Gets worse with skew, and far worse on a curve
//     (every joint overlaps).
//
//   UNION 1px outline (the fix): fill the whole union once, then ONE border pass = "an asphalt
//     pixel with a grass 4-neighbour" (the engine's own circ/poly coverage-outline rule). A
//     clean 1px kerb tracing the union boundary at ANY angle or curvature — generator-agnostic
//     (it reads coverage, not geometry), so a Perlin road is no harder than a straight one.
//
// Readouts: strays = interior dark pixels (should be 0); edge = sampled kerb thickness (1).
//
//   Z   method:  PER-PIECE casing  <->  UNION 1px outline (fix)
//   C   geometry: SKEW crossing     <->  CURVE (Perlin-ish) road
//   <>  skew angle (skew mode) / wiggle amplitude (curve mode)
//   ^v  road half-width
//   T   kerb thickness 1->2->3px (union method) — uniform at any angle via the coverage-diff
//   K   full CROSS-SECTION: asphalt -> kerb -> SIDEWALK -> grass, all nested coverage bands —
//       a "curb-like thing" that stays uniform-width + gap-free at any skew or curve
//   R   reset

static int   method = 1;     // 0 = per-piece casing ; 1 = union outline
static int   mode   = 0;     // 0 = skew crossing ; 1 = curvy road
static float skew   = 35.f;  // crossing-arm angle off 90 (skew mode)
static float amp    = 22.f;  // centreline wiggle amplitude (curve mode)
static int   HW     = 13;    // road half-width
static int   kerbW  = 1;      // kerb (outline) thickness in px — uniform at ANY angle via coverage-diff
static int   xsec   = 0;      // K: full cross-section (asphalt -> kerb -> SIDEWALK -> grass), nested bands

static float CX, CY;
static int   strays, edgeMax;

// ── geometry: a list of straight segments; the road is the capsule union (half-width HW) ──
enum { MAXSEG = 64 };
static float Ax[MAXSEG],Ay[MAXSEG],Bx[MAXSEG],By[MAXSEG];
static int   nSeg;

static void build(void){
    nSeg = 0;
    if (mode == 0){                                   // two long crossing arms through centre
        float far = (float)(SCREEN_W+SCREEN_H);
        Ax[0]=CX-far; Ay[0]=CY;        Bx[0]=CX+far; By[0]=CY;        // arm A: horizontal
        float r=(90.f+skew)*3.14159265f/180.f, dx=cosf(r), dy=sinf(r);
        Ax[1]=CX-far*dx; Ay[1]=CY-far*dy; Bx[1]=CX+far*dx; By[1]=CY+far*dy;  // arm B: skewed
        nSeg = 2;
    } else {                                          // wiggly road: chords of a multi-sine line
        int N = 26; float x0 = -10, x1 = SCREEN_W+10;
        float px=0,py=0;
        for (int i=0;i<=N;i++){
            float x = x0 + (x1-x0)*i/N;
            float y = CY + amp*(sinf(x*0.045f) + 0.45f*sinf(x*0.11f + 1.3f));  // fake-Perlin sum-of-sines
            if (i>0 && nSeg<MAXSEG){ Ax[nSeg]=px; Ay[nSeg]=py; Bx[nSeg]=x; By[nSeg]=y; nSeg++; }
            px=x; py=y;
        }
    }
}

// distance from a pixel CENTRE to segment i (clamped → capsule)
static float dseg(int xx,int yy,int i){
    float px=xx+0.5f, py=yy+0.5f, dx=Bx[i]-Ax[i], dy=By[i]-Ay[i], L2=dx*dx+dy*dy;
    float t = L2>0 ? ((px-Ax[i])*dx+(py-Ay[i])*dy)/L2 : 0; if(t<0)t=0; if(t>1)t=1;
    float qx=Ax[i]+t*dx, qy=Ay[i]+t*dy; return hypotf(px-qx, py-qy);
}
static int inRoad(int xx,int yy,float hw){
    for (int i=0;i<nSeg;i++) if (dseg(xx,yy,i) <= hw) return 1;
    return 0;
}

void update(void){
    if (keyp('Z')) method ^= 1;
    if (keyp('C')) mode ^= 1;
    if (keyp(KEY_LEFT))  { if(mode==0){skew-=5; if(skew<-80)skew=-80;} else {amp-=3; if(amp<0)amp=0;} }
    if (keyp(KEY_RIGHT)) { if(mode==0){skew+=5; if(skew> 80)skew= 80;} else {amp+=3; if(amp>40)amp=40;} }
    if (keyp(KEY_UP))    { if(HW<30)HW++; }
    if (keyp(KEY_DOWN))  { if(HW> 4)HW--; }
    if (keyp('T')) kerbW = kerbW%3 + 1;     // cycle kerb thickness 1->2->3px (union method)
    if (keyp('K')) xsec ^= 1;               // toggle the kerb+sidewalk cross-section (union method)
    if (keyp('R')) { method=1; mode=0; skew=35; amp=22; HW=13; kerbW=1; xsec=0; }
}

void draw(void){
    cls(CLR_DARK_GREEN);
    CX = SCREEN_W/2.f; CY = 24 + (SCREEN_H-24)/2.f;
    build();
    strays = 0; edgeMax = 0;

    if (method == 0){
        // PER-PIECE: each segment lays casing (HW+1) then asphalt (HW), in turn. Later pieces'
        // casing overwrites earlier pieces' asphalt at overlaps → interior strays.
        for (int i=0;i<nSeg;i++){
            for (int y=24;y<SCREEN_H;y++) for (int x=0;x<SCREEN_W;x++)
                if (dseg(x,y,i) <= HW+1) pset(x,y,CLR_BROWNISH_BLACK);
            for (int y=24;y<SCREEN_H;y++) for (int x=0;x<SCREEN_W;x++)
                if (dseg(x,y,i) <= HW)   pset(x,y,CLR_DARK_GREY);
        }
    } else if (!xsec) {
        // UNION OUTLINE: a pixel inside HW is kerb if it's NOT inside (HW-kerbW) — i.e. the
        // perpendicular band [HW-kerbW, HW]. A difference of two coverage tests ⇒ a UNIFORM
        // kerbW-px border at ANY angle or curvature (set kerbW=2 for a 2px kerb). Per-arm casing
        // could never keep that uniform at a skew — this is the payoff of coverage-based edges.
        for (int y=24;y<SCREEN_H;y++) for (int x=0;x<SCREEN_W;x++)
            if (inRoad(x,y,HW))
                pset(x,y, inRoad(x,y,(float)(HW-kerbW)) ? CLR_DARK_GREY : CLR_BROWNISH_BLACK);
    } else {
        // FULL CROSS-SECTION from nested coverage tests (asphalt -> kerb -> sidewalk -> grass).
        // Each band is just inRoad() at a different half-width — so a kerb AND a sidewalk both stay
        // uniform-width and gap-free at any skew or curve, the same way the 1px kerb did.
        const int SW = 6;                               // sidewalk width (outside the kerb)
        for (int y=24;y<SCREEN_H;y++) for (int x=0;x<SCREEN_W;x++){
            if (!inRoad(x,y,(float)(HW+SW))) continue;          // grass beyond
            int col;
            if      (!inRoad(x,y,(float)HW))          col = CLR_LIGHT_GREY;       // [HW, HW+SW] sidewalk
            else if (!inRoad(x,y,(float)(HW-kerbW)))  col = CLR_BROWNISH_BLACK;   // [HW-kerbW, HW] kerb
            else                                      col = CLR_DARK_GREY;        // [0, HW-kerbW] asphalt
            pset(x,y,col);
        }
    }

    // readouts (read the framebuffer): interior dark = a dark pixel with NO grass 4-neighbour
    // (a stray, not a real edge); edge thickness = longest vertical dark run sampled near a
    // road edge column.
    for (int y=25;y<SCREEN_H-1;y++) for (int x=1;x<SCREEN_W-1;x++){
        if (pget(x,y)!=CLR_BROWNISH_BLACK) continue;
        int touchesGrass = (pget(x-1,y)==CLR_DARK_GREEN)||(pget(x+1,y)==CLR_DARK_GREEN)
                         ||(pget(x,y-1)==CLR_DARK_GREEN)||(pget(x,y+1)==CLR_DARK_GREEN);
        if (!touchesGrass) strays++;
    }
    for (int x=20;x<SCREEN_W;x+=23){ int run=0;
        for (int y=25;y<SCREEN_H;y++){ if(pget(x,y)==CLR_BROWNISH_BLACK){run++; if(run>edgeMax)edgeMax=run;} else run=0; } }

    // HUD
    char buf[96];
    print("SKEWLAB - 1px road edges at skew/curve", 4, 4, CLR_WHITE);
    snprintf(buf,sizeof buf,"%s  |  %s",
        method? (xsec? "UNION x-section (kerb+walk)":"UNION outline (fix)") : "PER-PIECE casing",
        mode? "CURVE":"SKEW");
    print(buf, 4, 14, method? CLR_GREEN : CLR_ORANGE);
    snprintf(buf,sizeof buf, mode? "amp %d  HW %d  segs %d":"skew %+d  HW %d  segs %d",
        mode?(int)amp:(int)skew, HW, nSeg);
    print(buf, 210, 4, CLR_LIGHT_GREY);
    snprintf(buf,sizeof buf,"strays:%d  edge:%dpx  kerb:%dpx", strays, edgeMax, kerbW);
    print(buf, 210, 14, strays? CLR_RED : CLR_GREEN);
    print("Z method  C skew/curve  <>ang/amp  ^v width  T kerb1-3  K kerb+walk  R reset", 4, SCREEN_H-8, CLR_DARK_GREY);
}
