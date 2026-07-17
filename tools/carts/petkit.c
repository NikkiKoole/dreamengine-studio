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
// GRADIENT fill: dither from c0 → c1 across the shape along `ang`, stretched by
// `spread` (small = solid ends + a narrow blend BAND; 1 = an even ramp end-to-end).
static void fill_poly_grad(const P *p, int n, unsigned seed, float jit,
                           int c0, int c1, float ang, float spread){
  P q[MAXP]; if (n>MAXP) n=MAXP;
  float miny=1e9f, maxy=-1e9f, ca=cosf(ang*DEG), sa=sinf(ang*DEG), pmin=1e9f, pmax=-1e9f;
  for (int i=0;i<n;i++){ q[i].x=p[i].x+bo(seed,i,0x11,jit); q[i].y=p[i].y+bo(seed,i,0x22,jit);
    if (q[i].y<miny) miny=q[i].y; if (q[i].y>maxy) maxy=q[i].y;
    float pr=q[i].x*ca+q[i].y*sa; if (pr<pmin) pmin=pr; if (pr>pmax) pmax=pr; }
  if (pmax-pmin<0.001f) pmax=pmin+1;
  float sp=spread; if (sp<0.02f) sp=0.02f;
  float lo=0.5f-sp*0.5f, inv=1.0f/(pmax-pmin);
  int y0=(int)miny, y1=(int)maxy; if (y0<0) y0=0; if (y1>SCREEN_H-1) y1=SCREEN_H-1;
  for (int y=y0;y<=y1;y++){
    float yc=y+0.5f, xs[MAXP]; int cnt=0;
    for (int i=0;i<n;i++){ int j=(i+1)%n; float ay=q[i].y, by=q[j].y;
      if ((ay<=yc)==(by<=yc)) continue;
      float t=(yc-ay)/(by-ay); if (cnt<MAXP) xs[cnt++]=q[i].x+(q[j].x-q[i].x)*t; }
    for (int a=1;a<cnt;a++){ float v=xs[a]; int b=a-1; while(b>=0&&xs[b]>v){xs[b+1]=xs[b];b--;} xs[b+1]=v; }
    for (int k=0;k+1<cnt;k+=2){ int xa=(int)ceilf(xs[k]), xb=(int)floorf(xs[k+1]);
      if (xa<0) xa=0; if (xb>SCREEN_W-1) xb=SCREEN_W-1;
      for (int x=xa;x<=xb;x++){
        float t=((x*ca+y*sa)-pmin)*inv; t=(t-lo)/sp; if (t<0) t=0; if (t>1) t=1;
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
  float gangle;    // gradient ramp direction, degrees
  float gspread;   // gradient band width 0..1 (small = solid ends + narrow blend)
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
    fill_poly_grad(pts,n,s->seed,s->boil,s->fill,s->gcol,s->gangle,s->gspread);
  } else {
    if (s->dither) fillp(PATTERNS[s->dither], s->dcol);
    fill_poly(pts,n,0,0,s->seed,s->boil,s->fill);
    if (s->dither) fillp_reset();
  }
}

// ── colour cycle (candy palette + "none" for the edge) ───────────────────────
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
static Shape hero = { SH_CIRCLE, 200,100, 70, 70, 6, 0, 0.4f, CLR_LIME_GREEN, CLR_BROWNISH_BLACK, 1.0f,1.4f, 2.0f, 0, CLR_DARK_PURPLE, 0x1234u, 0.0f, 0, CLR_TRUE_BLUE, 0.0f, 0.5f };

// slider-backed 0..1 values, mapped into `hero` each frame (see draw())
static float sl_w=0.42f, sl_h=0.42f, sl_sq=0.5f, sl_boil=0.25f,
             sl_bevel=0.35f, sl_thick=0.25f, sl_round=0.4f, sl_dith=0.0f,
             sl_gang=0.0f, sl_gspr=0.5f;
static int gcol_i = 5;   // gradient far colour index into PAL (5 = TRUE_BLUE)

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
  float d=dt();
  if (key(',')) hero.rot -= 90*d;
  if (key('.')) hero.rot += 90*d;
}

void draw(void){
  cls(CLR_DARKER_BLUE);
  ui_begin();

  // ── tiny slider column (the continuous look dials) ──
  int sx=4, sw=52, sy=46;
  ui_slider(&sl_w,     sx, sy+0*11, sw, "W");
  ui_slider(&sl_h,     sx, sy+1*11, sw, "H");
  ui_slider(&sl_sq,    sx, sy+2*11, sw, "SQUASH");
  ui_slider(&sl_boil,  sx, sy+3*11, sw, "BOIL");
  ui_slider(&sl_bevel, sx, sy+4*11, sw, "BEVEL");
  ui_slider(&sl_thick, sx, sy+5*11, sw, "THICK");
  ui_slider(&sl_round, sx, sy+6*11, sw, "ROUND");
  ui_slider(&sl_dith,  sx, sy+7*11, sw, "DITHER");
  ui_slider(&sl_gang,  sx, sy+8*11, sw, "GANGLE");
  ui_slider(&sl_gspr,  sx, sy+9*11, sw, "GBAND");
  // map sliders → hero
  hero.w=6+sl_w*154; hero.h=6+sl_h*154; hero.squash=sl_sq*2-1;
  hero.boil=sl_boil*4; hero.bevel=sl_bevel*4; hero.ew=sl_thick*8;
  hero.round=sl_round; hero.dither=(int)(sl_dith*(NPAT-0.001f));
  hero.gangle=sl_gang*360; hero.gspread=sl_gspr;

  // ── reference row: all five kinds at a fixed size, boiling ──
  int refy=20;
  for (int k=0;k<SH_N;k++){
    int cx=32+k*58;
    Shape r = { k, cx, refy, 30, 24, 6, 0, 0.5f, PAL[(k+1)%NPAL], CLR_BROWNISH_BLACK, 0.6f, 1.2f, 1.5f, 0, CLR_DARK_PURPLE, (unsigned)(k*97+7), 0.0f, 0, CLR_TRUE_BLUE, 0.0f, 0.5f };
    shape_draw(&r);
    print(SKIND[k], cx-14, refy+18, CLR_MEDIUM_GREY);
  }
  line(0,42,SCREEN_W,42,CLR_INDIGO);

  // ── the hero shape (on a neutral backdrop so a dark OUTLINE and a light BEVEL
  //     both read — against the dark page a dark outline would vanish) ──
  rrectfill(92,48,224,104,6,CLR_INDIGO);
  shape_draw(&hero);

  // ── readout ──
  char buf[64];
  snprintf(buf,sizeof buf,"kind %-6s  w %3d  h %3d  sides %d  rot %3d",
           SKIND[hero.kind],(int)hero.w,(int)hero.h,hero.sides,((int)hero.rot%360+360)%360);
  print(buf,6,168,CLR_LIGHT_GREY);
  snprintf(buf,sizeof buf,"squash %+.2f  boil %.1f  bevel %.1f  thick %.1f  dither %d  grad %s  fill#%d/far#%d",
           hero.squash,hero.boil,hero.bevel,hero.ew,hero.dither, hero.grad?"ON":"off", fill_i, gcol_i);
  print(buf,6,178,CLR_LIGHT_GREY);
  print("keys: 1-5 kind  [] sides  ,. rot  C fill  E edge  G grad  V farcol  SPACE boil",6,190,CLR_DARK_GREY);

  ui_end();
}
