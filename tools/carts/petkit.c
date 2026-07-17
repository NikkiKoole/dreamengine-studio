/* de:meta
{
  "slug": "petkit",
  "collection": ["device-face"],
  "title": "petkit (shape lab)",
  "status": "active",
  "created": "2026-07-17",
  "kind": [
    "tool"
  ],
  "teaches": [],
  "description": "The foundation for the acid-pets mascots (docs/design/acid-pets.md): ONE parametric, reactive SHAPE primitive — circle / oval / rect / rounded-rect / polygon — where every property is a dial: position, width, height, rotation, polygon sides, corner round, fill colour, edge colour, BOIL (hand-drawn wobble that ticks on a beat) and BEVEL (candy rim). A Mr-Men character is just a shape whose look reads its state; get the atom right here, then compose. A reference row shows all five kinds; the big hero shape below is fully tweakable by key so we can confirm each property behaves before building creatures from it."
}
de:meta */
#include "studio.h"
// tiny-slider theme: make ui.h widgets readable on the dark lab background
#define UI_COL_BG       CLR_INDIGO
#define UI_COL_FILL     CLR_MEDIUM_GREY
#define UI_COL_FILL_HOT CLR_LIGHT_GREY
#define UI_COL_TEXT     CLR_WHITE
#define UI_COL_TEXT_HOT CLR_WHITE
#include "ui.h"
#include <math.h>
#include <stdio.h>

// ── PETKIT ────────────────────────────────────────────────────────────────────
// One reactive SHAPE, drawn in the squishy house look (boiled outline + candy
// bevel). Every kind is rendered as a POLYGON of points, so boil/bevel/fill/edge
// are ONE code path for all shapes. This is the atom acid-pets characters compose.

#define TAU 6.2831853f
#define MAXP 72
#define DEG (TAU/360.0f)

// ── the squishy-lite kit (shared with acidpets) ──────────────────────────────
typedef struct { float x, y; } P;
static unsigned g_fseed = 0;    // boil frame seed — advances on the beat

static unsigned hashu(unsigned x){ x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; x^=x>>16; return x; }
static float    hashf(unsigned x){ return (hashu(x) & 0xFFFFFF) / (float)0x1000000; }
static float bo(unsigned seed, int i, unsigned salt, float jit){
  return (hashf(seed ^ (unsigned)(i*2654435761u) ^ g_fseed ^ salt) * 2 - 1) * jit;
}
#define SUNX (-0.6f)
#define SUNY (-0.7f)

static void fill_poly(const P *p, int n, float ox, float oy, unsigned seed, float jit, int color){
  P q[MAXP]; if (n>MAXP) n=MAXP;
  float miny=1e9f, maxy=-1e9f;
  for (int i=0;i<n;i++){ q[i].x=p[i].x+ox+bo(seed,i,0x11,jit); q[i].y=p[i].y+oy+bo(seed,i,0x22,jit);
    if (q[i].y<miny) miny=q[i].y; if (q[i].y>maxy) maxy=q[i].y; }
  int y0=(int)miny, y1=(int)maxy; if (y0<0) y0=0; if (y1>SCREEN_H-1) y1=SCREEN_H-1;
  for (int y=y0;y<=y1;y++){
    float yc=y+0.5f, xs[MAXP]; int cnt=0;
    for (int i=0;i<n;i++){ int j=(i+1)%n; float ay=q[i].y, by=q[j].y;
      if ((ay<=yc)==(by<=yc)) continue;
      float t=(yc-ay)/(by-ay); if (cnt<MAXP) xs[cnt++]=q[i].x+(q[j].x-q[i].x)*t; }
    for (int a=1;a<cnt;a++){ float v=xs[a]; int b=a-1; while(b>=0&&xs[b]>v){xs[b+1]=xs[b];b--;} xs[b+1]=v; }
    for (int k=0;k+1<cnt;k+=2){ int xa=(int)ceilf(xs[k]), xb=(int)floorf(xs[k+1]); if (xb>=xa) rectfill(xa,y,xb-xa+1,1,color); }
  }
}
// 4x4 ordered (Bayer) matrix for the gradient FILL (from squishy)
static const int GBAYER[4][4] = { {0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5} };
// GRADIENT fill (PROCEDURAL, dpaint-style): dither c0 → c1 across the shape along
// `ang`. The blend is centred at `center` (0..1 along the axis — slide it to move the
// transition "from here to there") with width `band` (small = sharp split, 1 = full
// ramp). All scalars, so it hooks to state exactly like boil/bevel — no drawing.
static void fill_poly_grad(const P *p, int n, unsigned seed, float jit,
                           int c0, int c1, float ang, float center, float band){
  P q[MAXP]; if (n>MAXP) n=MAXP;
  float miny=1e9f, maxy=-1e9f, ca=cosf(ang*DEG), sa=sinf(ang*DEG), pmin=1e9f, pmax=-1e9f;
  for (int i=0;i<n;i++){ q[i].x=p[i].x+bo(seed,i,0x11,jit); q[i].y=p[i].y+bo(seed,i,0x22,jit);
    if (q[i].y<miny) miny=q[i].y; if (q[i].y>maxy) maxy=q[i].y;
    float pr=q[i].x*ca+q[i].y*sa; if (pr<pmin) pmin=pr; if (pr>pmax) pmax=pr; }
  if (pmax-pmin<0.001f) pmax=pmin+1;
  float bw=band; if (bw<0.02f) bw=0.02f;
  float lo=center-bw*0.5f, inv=1.0f/(pmax-pmin);
  int y0=(int)miny, y1=(int)maxy; if (y0<0) y0=0; if (y1>SCREEN_H-1) y1=SCREEN_H-1;
  for (int y=y0;y<=y1;y++){
    float yc=y+0.5f, xs[MAXP]; int cnt=0;
    for (int i=0;i<n;i++){ int j=(i+1)%n; float ya=q[i].y, yb=q[j].y;
      if ((ya<=yc)==(yb<=yc)) continue;
      float t=(yc-ya)/(yb-ya); if (cnt<MAXP) xs[cnt++]=q[i].x+(q[j].x-q[i].x)*t; }
    for (int a=1;a<cnt;a++){ float v=xs[a]; int b=a-1; while(b>=0&&xs[b]>v){xs[b+1]=xs[b];b--;} xs[b+1]=v; }
    for (int k=0;k+1<cnt;k+=2){ int xa=(int)ceilf(xs[k]), xb=(int)floorf(xs[k+1]);
      if (xa<0) xa=0; if (xb>SCREEN_W-1) xb=SCREEN_W-1;
      for (int x=xa;x<=xb;x++){
        float t=((x*ca+y*sa)-pmin)*inv; t=(t-lo)/bw; if (t<0) t=0; if (t>1) t=1;
        float thr=(GBAYER[y&3][x&3]+0.5f)/16.0f;
        pset(x,y, t>thr ? c1 : c0);
      } }
  }
}

static void stamp(float x, float y, float r, int c){
  if (r < 0.75f) pset((int)(x+.5f),(int)(y+.5f),c);
  else circfill((int)(x+.5f),(int)(y+.5f),(int)(r+.5f),c);
}
// the squishy OUTLINE: walk the boiled path stamping round dabs of width `w` — a
// fat hand-drawn ink line (bubble-letter rim), with a little per-dab width wobble.
static void stroke_poly(const P *p, int n, unsigned seed, float jit, float w, int color){
  if (w<=0) return;
  P q[MAXP]; if (n>MAXP) n=MAXP;
  for (int i=0;i<n;i++){ q[i].x=p[i].x+bo(seed,i,0x11,jit); q[i].y=p[i].y+bo(seed,i,0x22,jit); }
  float r=w*0.5f;
  for (int i=0;i<n;i++){ int j=(i+1)%n;
    float dx=q[j].x-q[i].x, dy=q[j].y-q[i].y, seg=sqrtf(dx*dx+dy*dy);
    int steps=(int)(seg/0.7f); if (steps<1) steps=1;
    for (int k=0;k<=steps;k++){ float u=(float)k/steps;
      float ww=r*(1.0f + (hashf(seed ^ (unsigned)(i*131u+k*977u))*2-1)*0.15f);   // hand-drawn width wobble
      stamp(q[i].x+dx*u, q[i].y+dy*u, ww, color); }
  }
}

// ── the Shape primitive ──────────────────────────────────────────────────────
enum { SH_CIRCLE, SH_OVAL, SH_RECT, SH_RRECT, SH_POLY, SH_N };
static const char *SKIND[SH_N] = { "circle", "oval", "rect", "rrect", "poly" };

typedef struct {
  int   kind;
  float x, y;      // centre
  float w, h;      // full width / height (radius = w/2)
  int   sides;     // POLY only (3..12)
  float rot;       // rotation, degrees
  float round;     // RRECT corner radius, 0..1 fraction of the short side
  int   fill;      // fill colour
  int   edge;      // outline colour (-1 = none)
  float boil;      // hand-drawn wobble amount, px
  float bevel;     // candy rim size, px
  float ew;        // outline THICKNESS, px (squishy stamped stroke)
  int   dither;    // fill dither pattern index (0 = solid) — dpaint density ramp
  int   dcol;      // dither "hole" colour (shows through the fill)
  unsigned seed;   // stable per-shape seed (keeps the boil coherent)
  float squash;    // -1 = squash (wide+short) .. +1 = stretch (tall+thin), 0 = neutral
  int   grad;      // GRADIENT fill on? (dither fill → gcol across the shape)
  int   gcol;      // gradient far colour (fill is the near colour)
  float gangle;    // gradient direction, degrees (PROCEDURAL — no drawing, hookable to state)
  float gcenter;   // where the blend midpoint sits along the axis, 0..1 ("from…to")
  float gband;     // blend width, 0..1 (small = sharp two-colour split, 1 = full ramp)
} Shape;

// dpaint-style DENSITY ramp (from squishy): 0 = solid, then ~12/25/50/75/87% ink.
static const int PATTERNS[] = { 0x0000, 0x7FDF, 0x5F5F, 0x5A5A, 0x0A0A, 0x0208 };
#define NPAT ((int)(sizeof(PATTERNS)/sizeof(PATTERNS[0])))

// generate the outline points for a shape (local, centred), then rotate + translate.
static int shape_points(const Shape *s, P *o){
  // squash/stretch scales the two axes oppositely (the animation primitive)
  float rw=s->w*0.5f*(1+0.8f*s->squash), rh=s->h*0.5f*(1-0.8f*s->squash);
  if (rw<1) rw=1; if (rh<1) rh=1;
  int n=0;
  switch (s->kind){
    case SH_CIRCLE: { int N=28; for(int i=0;i<N;i++){ float a=(float)i/N*TAU; o[n].x=cosf(a)*rw; o[n].y=sinf(a)*rw; n++; } } break;
    case SH_OVAL:   { int N=28; for(int i=0;i<N;i++){ float a=(float)i/N*TAU; o[n].x=cosf(a)*rw; o[n].y=sinf(a)*rh; n++; } } break;
    case SH_RECT:   { o[0].x=-rw;o[0].y=-rh; o[1].x=rw;o[1].y=-rh; o[2].x=rw;o[2].y=rh; o[3].x=-rw;o[3].y=rh; n=4; } break;
    case SH_RRECT:  {
      float rr = s->round * fminf(rw,rh); if (rr<0.5f) rr=0.5f;
      float ox=rw-rr, oy=rh-rr; int K=5;
      // four corners, clockwise (screen y down): TR, BR, BL, TL
      float ccx[4]={ ox, ox,-ox,-ox}, ccy[4]={-oy, oy, oy,-oy}, a0[4]={-90,0,90,180};
      for (int c=0;c<4;c++) for (int k=0;k<=K;k++){ float a=(a0[c]+(float)k/K*90)*DEG;
        o[n].x=ccx[c]+cosf(a)*rr; o[n].y=ccy[c]+sinf(a)*rr; n++; }
    } break;
    case SH_POLY:   { int N=s->sides; if(N<3)N=3; if(N>12)N=12;
      for(int i=0;i<N;i++){ float a=-TAU*0.25f + (float)i/N*TAU; o[n].x=cosf(a)*rw; o[n].y=sinf(a)*rh; n++; } } break;
  }
  float ca=cosf(s->rot*DEG), sa=sinf(s->rot*DEG);
  for (int i=0;i<n;i++){ float lx=o[i].x, ly=o[i].y;
    o[i].x=s->x + lx*ca - ly*sa; o[i].y=s->y + lx*sa + ly*ca; }
  return n;
}

// draw a shape, squishy-style, so OUTLINE and BEVEL coexist (they used to collide):
//   1. the fat outline goes UNDER, as the outermost silhouette (bubble-letter rim)
//   2. the candy bevel rims sit just INSIDE it (clamped so a thin outline still shows)
//   3. the body fill (solid / dither / gradient) covers the centre
// So from the edge inward you read: outline · bevel highlight/shadow · body.
static void shape_draw(const Shape *s){
  P pts[MAXP]; int n=shape_points(s,pts);
  int has_edge = (s->edge>=0 && s->ew>0);

  if (has_edge) stroke_poly(pts,n,s->seed,s->boil,s->ew,s->edge);   // 1. outline (under)

  float bev=s->bevel; if (bev>2.5f) bev=2.5f;
  if (has_edge){ float lim=s->ew*0.5f-0.5f; if (bev>lim) bev=lim; } // keep bevel inside the outline
  if (bev>0){                                                       // 2. bevel rims
    fill_poly(pts,n,-SUNX*bev,-SUNY*bev,s->seed,s->boil,CLR_BLACK);
    fill_poly(pts,n, SUNX*bev, SUNY*bev,s->seed,s->boil,CLR_WHITE);
  }
  if (s->grad){                                                     // 3. body fill
    fill_poly_grad(pts,n,s->seed,s->boil,s->fill,s->gcol,s->gangle,s->gcenter,s->gband);
  } else {
    if (s->dither) fillp(PATTERNS[s->dither], s->dcol);
    fill_poly(pts,n,0,0,s->seed,s->boil,s->fill);
    if (s->dither) fillp_reset();
  }
}

// ── colour cycle (candy palette + "none" for the edge) ───────────────────────
// small helpers for the eye shapes (petkit's shape_draw inlines these otherwise)
static int mkoval(P *o, float cx, float cy, float rx, float ry, int n){
  for (int i=0;i<n;i++){ float a=(float)i/n*TAU; o[i].x=cx+cosf(a)*rx; o[i].y=cy+sinf(a)*ry; }
  return n;
}
// ── EYES — just PUPILS (dot/pip). No sclera, no lids, no brows. A filled boiled dot
// with its OWN w/h/squash; expression comes from size, gaze offset, a blink, and a
// happy ∩ arc. Simple = right for the lo-fi candy toy. ────────────────────────────
typedef struct { float open, squint, pupil, gx, gy, lid, browY, browA; int glint; } Eye;

static void peteye(float cx,float cy,float ew,float eh,float esq,int color,float boil,const Eye*e){
  float rw=ew*(1+0.6f*esq), rh=eh*(1-0.6f*esq); if(rw<1)rw=1; if(rh<1)rh=1;
  unsigned sd=(unsigned)((int)cx*131 + (int)cy*17 + 3);
  if (e->squint>0.6f){                         // happy ∩ arc (peak up)
    float aH=rh*1.3f, pxp=0,pyp=0;
    for(int i=0;i<=8;i++){ float u=(float)i/8*2-1; float x=cx+u*rw, y=cy-aH*(1-u*u)+rh*0.4f;
      if(i>0){ line((int)pxp,(int)pyp,(int)x,(int)y,color); line((int)pxp,(int)pyp+1,(int)x,(int)y+1,color);} pxp=x;pyp=y; }
    return;
  }
  // the pupil: a filled boiled dot, nudged by gaze. OPEN scales its HEIGHT, so a
  // dropping OPEN squashes the dot smoothly down to a line — the blink is a transition.
  float px=cx+e->gx*rw*0.6f, py=cy+e->gy*rh*0.6f, oh=rh*e->open;
  if (oh < 0.8f){                              // (nearly) closed → a flat line
    float w=fmaxf(2,rw);
    line((int)(px-w),(int)py,(int)(px+w),(int)py,color); line((int)(px-w),(int)py+1,(int)(px+w),(int)py+1,color);
    return;
  }
  P o[12]; mkoval(o,px,py,rw,oh,12); fill_poly(o,12,0,0,sd,boil,color);
  if (e->glint && rw>2.5f) pset((int)(px-rw*0.35f),(int)(py-oh*0.35f),CLR_WHITE);
}
// simple MOUTH — same story as the eyes: a boiled curve (curve>0 smile / <0 frown),
// or a filled boiled oval when OPEN (gasp/talk). Own width + own boil, absolute size.
static void pet_mouth(float cx,float cy,float w,float curve,float open,int color,float boil){
  // ONE shape (no curve↔open pop): a lens between an upper & lower lip, both following
  // the smile curve. A minimum thickness makes closed = a line; OPEN spreads it smoothly.
  unsigned sd=(unsigned)((int)cx*57 + (int)cy*13 + 9);
  float amp=curve*w*0.30f, openH=fmaxf(0.7f, open*w*0.42f);
  P poly[24]; int n=0;
  for(int i=0;i<=11;i++){ float u=(float)i/11*2-1; float x=cx+u*w*0.5f, yc=cy+amp*(1-u*u);
    poly[n].x=x+bo(sd,i,0x51,boil); poly[n].y=yc-openH*(1-u*u)+bo(sd,i,0x52,boil); n++; }   // upper lip
  for(int i=11;i>=0;i--){ float u=(float)i/11*2-1; float x=cx+u*w*0.5f, yc=cy+amp*(1-u*u);
    poly[n].x=x+bo(sd,i+20,0x53,boil); poly[n].y=yc+openH*(1-u*u)+bo(sd,i+20,0x54,boil); n++; }  // lower lip
  fill_poly(poly,n,0,0,sd,0,color);
}
static void pet_eyes(float cx,float cy,float gap,float ew,float eh,float esq,int color,float boil,Eye e){
  peteye(cx-gap,cy,ew,eh,esq,color,boil,&e);
  peteye(cx+gap,cy,ew,eh,esq,color,boil,&e);
}

static const int PAL[] = { CLR_LIME_GREEN, CLR_YELLOW, CLR_ORANGE, CLR_PINK, CLR_RED,
  CLR_TRUE_BLUE, CLR_MAUVE, CLR_INDIGO, CLR_WHITE, CLR_LIGHT_GREY, CLR_DARK_PURPLE };
#define NPAL ((int)(sizeof(PAL)/sizeof(PAL[0])))
static int fill_i = 0;    // index into PAL
// the edge palette is its own list (an ink outline wants darks PAL doesn't hold)
static const int EDGEPAL[] = { CLR_BROWNISH_BLACK, CLR_BLACK, CLR_WHITE, CLR_RED, CLR_DARK_PURPLE };
#define NEDGE ((int)(sizeof(EDGEPAL)/sizeof(EDGEPAL[0])))
static int edge_i = 0;    // -1 = no edge, else index into EDGEPAL

// ── the editable hero shape ──────────────────────────────────────────────────
//                       kind      x   y   w   h  sd rot round fill            edge               boil bevel ew  dith dcol             seed     squash
static Shape hero = { SH_CIRCLE, 200,100, 70, 70, 6, 0, 0.4f, CLR_LIME_GREEN, CLR_BROWNISH_BLACK, 1.0f,1.4f, 2.0f, 0, CLR_DARK_PURPLE, 0x1234u, 0.0f, 0, CLR_TRUE_BLUE, 0.0f, 0.5f, 1.0f };

// slider-backed 0..1 values, mapped into `hero` each frame (see draw())
static float sl_w=0.42f, sl_h=0.42f, sl_sq=0.5f, sl_boil=0.25f,
             sl_bevel=0.35f, sl_thick=0.25f, sl_round=0.4f, sl_dith=0.0f,
             sl_gang=0.0f, sl_goff=0.5f, sl_gband=1.0f;
static int gcol_i = 5;   // gradient far colour index into PAL (5 = TRUE_BLUE)
// eye machine: emotion levers + identity dials (see peteyes). U flips the slider panel.
static float se_open=0.8f, se_squint=0, se_pupil=0.5f, se_gx=0.5f, se_gy=0.5f,
             se_lid=0.5f, se_browy=0.5f, se_browa=0.5f,
             se_gap=0.5f, se_ew=0.35f, se_eh=0.4f, se_esq=0.5f, se_eboil=0.4f;  // pupil geometry + OWN boil
static float sm_w=0.5f, sm_curve=0.7f, sm_open=0.0f;   // mouth: width, curve(smile), open
static int eyes_on=1, panel=0;   // panel: 0 = shape dials, 1 = eye dials
static float blinkt=0;           // auto-blink timer (a smooth transition, not a snap)
#define BLINK_DUR 0.16f

// boil ticking — advances the wobble frame on a beat (the "boil on the BPM" feel)
static float clk = 0;
static int   btick = -1, boil_on = 1;
static float boil_rate = 8.0f;   // wobble frames per second (≈16ths at 120bpm)

void update(void){
  if (boil_on){ clk += dt()*boil_rate; int t=(int)clk; if (t!=btick){ g_fseed = hashu((unsigned)t*2654435761u); btick=t; } }

  // discrete CHOICES stay on keys (the sliders own the continuous look params)
  for (int k=0;k<SH_N;k++) if (keyp('1'+k)) hero.kind=k;
  if (keyp('[')) hero.sides = hero.sides>3 ? hero.sides-1 : 3;
  if (keyp(']')) hero.sides = hero.sides<12 ? hero.sides+1 : 12;
  if (keyp('C')) { fill_i=(fill_i+1)%NPAL; hero.fill=PAL[fill_i]; }
  if (keyp('E')) { edge_i = edge_i+1>=NEDGE ? -1 : edge_i+1; hero.edge = edge_i<0?-1:EDGEPAL[edge_i]; }
  if (keyp('G')) { hero.grad = !hero.grad; }
  if (keyp('V')) { gcol_i=(gcol_i+1)%NPAL; hero.gcol=PAL[gcol_i]; }
  if (keyp(KEY_SPACE)) boil_on=!boil_on;
  if (keyp('Y')) eyes_on=!eyes_on;      // show/hide the eyes
  if (keyp('U')) panel=!panel;          // flip slider column: shape <-> eyes
  if (keyp('B')) blinkt=BLINK_DUR;      // manual blink
  if (blinkt>0) blinkt-=dt();
  if (blinkt<=0 && rnd(150)==0) blinkt=BLINK_DUR;   // idle auto-blink
  float d=dt();
  if (key(',')) hero.rot -= 90*d;
  if (key('.')) hero.rot += 90*d;
}

void draw(void){
  cls(CLR_DARKER_BLUE);
  ui_begin();

  // ── tiny slider column — flip between SHAPE dials and EYE dials with U ──
  int sx=4, sw=52, sy=44, sp=10;
  print(panel?"FACE  (U:shape)":"SHAPE (U:face)", sx, 34, CLR_MEDIUM_GREY);
  if (!panel){
    ui_slider(&sl_w,     sx, sy+0*sp, sw, "W");
    ui_slider(&sl_h,     sx, sy+1*sp, sw, "H");
    ui_slider(&sl_sq,    sx, sy+2*sp, sw, "SQUASH");
    ui_slider(&sl_boil,  sx, sy+3*sp, sw, "BOIL");
    ui_slider(&sl_bevel, sx, sy+4*sp, sw, "BEVEL");
    ui_slider(&sl_thick, sx, sy+5*sp, sw, "THICK");
    ui_slider(&sl_round, sx, sy+6*sp, sw, "ROUND");
    ui_slider(&sl_dith,  sx, sy+7*sp, sw, "DITHER");
    ui_slider(&sl_gang,  sx, sy+8*sp, sw, "G.ANGLE");
    ui_slider(&sl_goff,  sx, sy+9*sp, sw, "G.OFFSET");
    ui_slider(&sl_gband, sx, sy+10*sp,sw, "G.BAND");
  } else {
    ui_slider(&se_ew,     sx, sy+0*sp, sw, "EYE W");
    ui_slider(&se_eh,     sx, sy+1*sp, sw, "EYE H");
    ui_slider(&se_esq,    sx, sy+2*sp, sw, "EYE SQUASH");
    ui_slider(&se_gap,    sx, sy+3*sp, sw, "SPACING");
    ui_slider(&se_open,   sx, sy+4*sp, sw, "OPEN/BLINK");
    ui_slider(&se_squint, sx, sy+5*sp, sw, "HAPPY ARC");
    ui_slider(&se_gx,     sx, sy+6*sp, sw, "GAZE X");
    ui_slider(&se_gy,     sx, sy+7*sp, sw, "GAZE Y");
    ui_slider(&se_eboil,  sx, sy+8*sp, sw, "EYE BOIL");
    ui_slider(&sm_w,      sx, sy+9*sp, sw, "MOUTH W");
    ui_slider(&sm_curve,  sx, sy+10*sp,sw, "MOUTH CURVE");
    ui_slider(&sm_open,   sx, sy+11*sp,sw, "MOUTH OPEN");
  }
  // map sliders → hero (always, so both persist while the other panel shows)
  hero.w=6+sl_w*154; hero.h=6+sl_h*154; hero.squash=sl_sq*2-1;
  hero.boil=sl_boil*4; hero.bevel=sl_bevel*4; hero.ew=sl_thick*8;
  hero.round=sl_round; hero.dither=(int)(sl_dith*(NPAT-0.001f));
  hero.gangle=sl_gang*360; hero.gcenter=sl_goff; hero.gband=sl_gband;

  // ── reference row: all five kinds at a fixed size, boiling ──
  int refy=20;
  for (int k=0;k<SH_N;k++){
    int cx=32+k*58;
    Shape r = { k, cx, refy, 30, 24, 6, 0, 0.5f, PAL[(k+1)%NPAL], CLR_BROWNISH_BLACK, 0.6f, 1.2f, 1.5f, 0, CLR_DARK_PURPLE, (unsigned)(k*97+7), 0.0f, 0, CLR_TRUE_BLUE, 0.0f, 0.5f, 1.0f };
    shape_draw(&r);
    print(SKIND[k], cx-14, refy+18, CLR_MEDIUM_GREY);
  }
  line(0,42,SCREEN_W,42,CLR_INDIGO);

  // ── the hero shape (on a neutral backdrop so a dark OUTLINE and a light BEVEL
  //     both read — against the dark page a dark outline would vanish) ──
  rrectfill(92,48,224,104,6,CLR_INDIGO);
  shape_draw(&hero);

  // gradient direction indicator: a small procedural arrow (near dot → far dot)
  if (hero.grad){
    float ca=cosf(hero.gangle*DEG), sa=sinf(hero.gangle*DEG);
    int ax=(int)(hero.x-ca*22), ay=(int)(hero.y-sa*22), bx=(int)(hero.x+ca*22), by=(int)(hero.y+sa*22);
    line(ax,ay,bx,by,CLR_WHITE);
    circfill(ax,ay,2,hero.fill); circfill(bx,by,2,hero.gcol); circ(bx,by,2,CLR_WHITE);
  }

  // ── eyes on the hero — the shape becomes a face (lids painted in the body colour,
  //     eye size/spacing scaled to the shape so it stays a character at any size) ──
  if (eyes_on){
    float bopen = 1.0f;                        // blink transition (1 → 0 → 1)
    if (blinkt>0){ float x=blinkt/BLINK_DUR; bopen=fabsf(x*2-1); }
    Eye e = { se_open*bopen, se_squint, se_pupil, se_gx*2-1, se_gy*2-1,
              se_lid*2-1, se_browy, se_browa*2-1, 1 };
    // POSITION tracks the head (so eyes stay in the face as it resizes/squashes)…
    float ehw = hero.w*0.5f*(1+0.8f*hero.squash);   // effective head half-width
    float ehh = hero.h*0.5f*(1-0.8f*hero.squash);   // …half-height
    float gap = ehw*(0.15f + se_gap*0.55f);
    float cyv = hero.y - ehh*0.22f;
    // …but SIZE stays absolute (doesn't grow with the head). Eyes keep their OWN boil.
    float eew = 2 + se_ew*16, eeh = 2 + se_eh*16, eesq = se_esq*2-1;
    pet_eyes(hero.x, cyv, gap, eew, eeh, eesq, CLR_BLACK, se_eboil*2, e);
    // mouth — position tracks the head, width absolute, its own boil
    float mw = 6 + sm_w*40, myv = hero.y + ehh*0.35f;
    pet_mouth(hero.x, myv, mw, sm_curve*2-1, sm_open, CLR_BLACK, se_eboil*2);
  }

  // ── readout ──
  char buf[64];
  snprintf(buf,sizeof buf,"kind %-6s  w %3d  h %3d  sides %d  rot %3d",
           SKIND[hero.kind],(int)hero.w,(int)hero.h,hero.sides,((int)hero.rot%360+360)%360);
  print(buf,6,168,CLR_LIGHT_GREY);
  snprintf(buf,sizeof buf,"squash %+.2f  boil %.1f  bevel %.1f  thick %.1f  dither %d  grad %s  fill#%d/far#%d",
           hero.squash,hero.boil,hero.bevel,hero.ew,hero.dither, hero.grad?"ON":"off", fill_i, gcol_i);
  print(buf,6,178,CLR_LIGHT_GREY);
  // near/far colour swatches for the gradient
  print("grad",250,168,hero.grad?CLR_WHITE:CLR_DARK_GREY);
  rectfill(272,168,7,7,hero.fill); rectfill(281,168,7,7,hero.gcol);
  print("keys: 1-5 kind  [] sides  ,. rot  C fill  E edge  G grad  Y eyes  U panel  B blink",6,190,CLR_DARK_GREY);

  ui_end();
}
