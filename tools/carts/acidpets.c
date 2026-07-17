/* de:meta
{
  "slug": "acidpets",
  "collection": ["device-face"],
  "title": "acid pets (mockup)",
  "status": "active",
  "created": "2026-07-17",
  "kind": [
    "instrument",
    "generative"
  ],
  "genre": null,
  "teaches": [],
  "description": "A DRAW-ONLY vibe mockup for acidcandy: five little creatures — one per machine (303a eel / 303b tadpole / 808 puffer / 909 bug / MST heart) — that are a LIVE READOUT of a faked acid rack. The nav strip is a PET SHELF (five tiny critters, always visible) with a big VIVARIUM below showing the focused pet in detail. Every knob, note, accent, slide, mute, Devil-Fish flip and the sidechain PUMP maps to something visible on the animal. Rendered in the squishy-lines house look: hand-drawn boiled outlines (the wobble frame advances on the BPM) + a beveled candy rim. No audio — this is for iterating the LOOK before it touches the real cart."
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>   // snprintf — the size-ladder px labels

// ── ACID PETS ────────────────────────────────────────────────────────────────
// A mascot-per-instrument mockup for acidcandy. NOT the real rack — a fake little
// sequencer runs five machines, and each machine's whole state is drawn as a
// creature. The point: prove that a knob-turn / a note / a mute reads on the pet.
//
// House look borrowed from squishy.c:
//   BOIL   — every shape is a path of points; each point gets a hashed per-frame
//            jitter, so nothing is geometrically neat. The jitter FRAME advances
//            on the transport (the "boil on the BPM" the maker asked for) → the
//            whole shelf shivers in time with the beat.
//   BEVEL  — the same path filled offset toward a global "sun" (white rim) and away
//            from it (black rim), body on top → a raised candy edge.

#define STEPS 16
#define TAU 6.2831853f

// ── machines (mirrors acidcandy's roster) ────────────────────────────────────
enum { M_303A, M_303B, M_808, M_909, M_MST, M_N };
static const char *MNAME[M_N] = { "303a", "303b", "808", "909", "MST" };
static const int   MCOL[M_N]  = { CLR_LIME_GREEN, CLR_YELLOW, CLR_ORANGE, CLR_LIGHT_GREY, CLR_PINK };
static int  mute[M_N];
static int  foc = 0;            // focused machine (the vivarium subject)

// ── the fake transport ───────────────────────────────────────────────────────
static float tempo = 130.0f;   // (bpm is a studio.h built-in — don't shadow it)
static int   playing = 1;
static float clk16 = 0;         // running position in 16th steps
static int   step = 0, prevstep = -1;
static unsigned g_fseed = 0;    // BOIL frame seed — advanced on each 16th (the BPM boil)
static int   autodrift = 1;     // knobs wander on their own so the pets morph hands-free

// ── patterns ─────────────────────────────────────────────────────────────────
// two 303 lines: on / pitch(semis) / accent / slide / tie
static int on3[2][STEPS]  = {{1,0,1,1,0,1,0,1, 1,0,1,0,1,1,0,1},
                             {0,0,1,0,0,1,0,0, 1,0,0,0,1,0,1,0}};
static int pit3[2][STEPS] = {{0,0,12,0,3,0,0,7, 0,0,10,0,0,3,0,0},
                             {12,0,19,0,0,15,0,0, 24,0,0,0,12,0,15,0}};
static int acc3[2][STEPS] = {{1,0,0,1,0,0,0,1, 1,0,0,0,1,0,0,0},
                             {1,0,0,0,0,1,0,0, 1,0,0,0,0,0,0,0}};
static int sld3[2][STEPS] = {{0,0,1,0,0,0,0,1, 0,0,1,0,0,0,0,0},
                             {0,0,1,0,0,0,0,0, 0,0,0,0,1,0,0,0}};

// 808 lanes: KICK SNARE HAT CLAP COWBELL
enum { E_K, E_S, E_H, E_C, E_CB, E_N8 };
static int g8[E_N8][STEPS] = {
  {1,0,0,0, 1,0,0,0, 1,0,0,1, 1,0,0,0},   // kick
  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,1},   // snare
  {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0},   // hat
  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0},   // clap
  {0,0,0,0, 0,0,0,1, 0,0,0,0, 0,0,0,1},   // cowbell
};
// 909 lanes: KICK SNARE HAT RIDE CLAP  (+ flam markers)
enum { N_K, N_S, N_H, N_R, N_CL, N_N9 };
static int g9[N_N9][STEPS] = {
  {1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,1,0},
  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0},
  {1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1},   // 16th hats
  {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0},   // ride offbeat
  {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0},
};
static int flam9[STEPS] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0};

// ── knobs (0..1) — index meaning depends on machine type ──────────────────────
// 303:  0 CUT  1 RES  2 ENV  3 DEC  4 ACC        (df page adds SUB/TRK feel via df flag)
// 808:  0 TUNE 1 DEC  2 SNAP 3 SWG  4 DRV
// 909:  0 TUNE 1 DEC  2 SNAP 3 METL 4 SWG
// MST:  0 GLU  1 FLT  2 RES  3 FB   4 PUMP
static float kv[M_N][5];
static int   df[2];             // Devil-Fish flip per 303
static int   frozen[M_N][5];    // a knob the user grabbed (stops auto-drift)
static const char *KN3[5]  = {"CUT","RES","ENV","DEC","ACC"};
static const char *KN3D[5] = {"SUB","ADEC","SLDT","TRK","WAV"};
static const char *KN8[5]  = {"TUNE","DEC","SNAP","SWG","DRV"};
static const char *KN9[5]  = {"TUNE","DEC","SNAP","METL","SWG"};
static const char *KNM[5]  = {"GLU","FLT","RES","FB","PUMP"};
static const char **knames(int m){ if(m==M_303A||m==M_303B) return df[m]?KN3D:KN3;
  if(m==M_808) return KN8; if(m==M_909) return KN9; return KNM; }

// ── reactive per-machine state (what the creature reads) ──────────────────────
typedef struct {
  float hit, acc;           // note/step flash + accent flash (303)
  float kick, snare, hat, perc;  // drum EVENT FAMILIES (see docs/design/acid-pets.md)
  float energy;             // rolling activity
  float pit01, pcur;        // 303 target pitch (norm) + glided current
  float open;               // 303 display "filter openness"
  float blink;              // eye-blink countdown
  float flam;               // 909 stroke flash
} Pet;
static Pet pet[M_N];

// ── squishy-lite: boiled + beveled blobs ─────────────────────────────────────
typedef struct { float x, y; } P;

static unsigned hashu(unsigned x){ x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; x^=x>>16; return x; }
static float    hashf(unsigned x){ return (hashu(x) & 0xFFFFFF) / (float)0x1000000; }
// per-point boil offset, keyed by (seed, point, the BPM frame-seed) → coherent wobble
static float bo(unsigned seed, int i, unsigned salt, float jit){
  return (hashf(seed ^ (unsigned)(i*2654435761u) ^ g_fseed ^ salt) * 2 - 1) * jit;
}

#define SUNX (-0.6f)
#define SUNY (-0.7f)   // light from the upper-left

static int mkoval(P *o, float cx, float cy, float rx, float ry, int n){
  for (int i=0;i<n;i++){ float a=(float)i/n*TAU; o[i].x=cx+cosf(a)*rx; o[i].y=cy+sinf(a)*ry; }
  return n;
}

// even-odd scanline fill of a boiled polygon, offset by (ox,oy)
static void fill_poly(const P *p, int n, float ox, float oy, unsigned seed, float jit, int color){
  P q[48]; if (n>48) n=48;
  float miny=1e9f, maxy=-1e9f;
  for (int i=0;i<n;i++){
    q[i].x = p[i].x+ox+bo(seed,i,0x11,jit);
    q[i].y = p[i].y+oy+bo(seed,i,0x22,jit);
    if (q[i].y<miny) miny=q[i].y; if (q[i].y>maxy) maxy=q[i].y;
  }
  int y0=(int)miny, y1=(int)maxy; if (y0<0) y0=0; if (y1>SCREEN_H-1) y1=SCREEN_H-1;
  for (int y=y0;y<=y1;y++){
    float yc=y+0.5f, xs[32]; int cnt=0;
    for (int i=0;i<n;i++){ int j=(i+1)%n; float ay=q[i].y, by=q[j].y;
      if ((ay<=yc)==(by<=yc)) continue;
      float t=(yc-ay)/(by-ay); if (cnt<32) xs[cnt++]=q[i].x+(q[j].x-q[i].x)*t; }
    for (int a=1;a<cnt;a++){ float v=xs[a]; int b=a-1; while(b>=0&&xs[b]>v){xs[b+1]=xs[b];b--;} xs[b+1]=v; }
    for (int k=0;k+1<cnt;k+=2){ int xa=(int)ceilf(xs[k]), xb=(int)floorf(xs[k+1]); if (xb>=xa) rectfill(xa,y,xb-xa+1,1,color); }
  }
}
// boiled outline (the hand-drawn ink line — thin + wobbly)
static void stroke_poly(const P *p, int n, unsigned seed, float jit, int color){
  P q[48]; if (n>48) n=48;
  for (int i=0;i<n;i++){ q[i].x=p[i].x+bo(seed,i,0x11,jit); q[i].y=p[i].y+bo(seed,i,0x22,jit); }
  for (int i=0;i<n;i++){ int j=(i+1)%n;
    line((int)(q[i].x+.5f),(int)(q[i].y+.5f),(int)(q[j].x+.5f),(int)(q[j].y+.5f),color); }
}
// a candy blob: black rim (anti-sun) + white rim (sun) + body + optional ink edge
static void blob(const P *p, int n, unsigned seed, float jit, float bevel, int body, int edge){
  if (bevel>2.5f) bevel=2.5f;   // cap the candy rim so it doesn't swallow a 32px creature
  if (bevel>0){
    fill_poly(p,n,-SUNX*bevel,-SUNY*bevel,seed,jit,CLR_BLACK);
    fill_poly(p,n, SUNX*bevel, SUNY*bevel,seed,jit,CLR_WHITE);
  }
  fill_poly(p,n,0,0,seed,jit,body);
  if (edge>=0) stroke_poly(p,n,seed,jit,edge);
}

// ── the creatures ─────────────────────────────────────────────────────────────
// Each reads pet[m] + kv[m][*]. cx,cy = centre; s = scale (px per design-unit).

// ── simple EYES + MOUTH (pupil dots, same as petkit) ─────────────────────────
// A pupil is a filled boiled dot with its OWN boil; OPEN scales its height (smooth
// blink), squint bends it to a happy ∩ arc, gaze nudges it. No sclera/lids/brows.
typedef struct { float open, squint, pupil, gx, gy, lid, browY, browA; int glint; } Eye;

static void peteye(float cx,float cy,float ew,float eh,int color,float boil,const Eye*e){
  if(ew<1)ew=1; if(eh<1)eh=1;
  unsigned sd=(unsigned)((int)cx*131 + (int)cy*17 + 3);
  if (e->squint>0.6f){                                   // happy ∩ arc
    float aH=eh*1.3f, pxp=0,pyp=0;
    for(int i=0;i<=8;i++){ float u=(float)i/8*2-1; float x=cx+u*ew, y=cy-aH*(1-u*u)+eh*0.4f;
      if(i>0){ line((int)pxp,(int)pyp,(int)x,(int)y,color); line((int)pxp,(int)pyp+1,(int)x,(int)y+1,color);} pxp=x;pyp=y; }
    return;
  }
  float px=cx+e->gx*ew*0.6f, py=cy+e->gy*eh*0.6f, oh=eh*e->open;   // OPEN flattens (blink)
  if (oh<0.8f){ float w=fmaxf(1.5f,ew); line((int)(px-w),(int)py,(int)(px+w),(int)py,color); return; }
  P o[12]; mkoval(o,px,py,ew,oh,12); fill_poly(o,12,0,0,sd,boil,color);
  if (e->glint && ew>2.5f) pset((int)(px-ew*0.35f),(int)(py-oh*0.35f),CLR_WHITE);
}
// simple MOUTH: a boiled curve (curve>0 smile), or a filled oval when OPEN
static void pet_mouth(float cx,float cy,float w,float curve,float open,int color,float boil){
  unsigned sd=(unsigned)((int)cx*57 + (int)cy*13 + 9);
  if (open>0.12f){ P o[14]; mkoval(o,cx,cy,w*0.5f,fmaxf(1.5f,w*0.5f*open),14); fill_poly(o,14,0,0,sd,boil,color); return; }
  float amp=curve*w*0.30f, pxp=0,pyp=0;
  for(int i=0;i<=10;i++){ float u=(float)i/10*2-1;
    float x=cx+u*w*0.5f+bo(sd,i,0x51,boil), y=cy+amp*(1-u*u)+bo(sd,i,0x52,boil);
    if(i>0){ line((int)pxp,(int)pyp,(int)x,(int)y,color); line((int)pxp,(int)pyp+1,(int)x,(int)y+1,color);} pxp=x;pyp=y; }
}
static void pet_eyes(float cx,float cy,float gap,float ew,float eh,int color,float boil,Eye e){
  peteye(cx-gap,cy,ew,eh,color,boil,&e);
  peteye(cx+gap,cy,ew,eh,color,boil,&e);
}

// 303 has TWO faces: the vanilla acid-house SMILEY (draw_smiley), and — when Devil
// Fish is armed — it morphs into the feral FISH (draw_fish, below). The fish is the
// reward for going feral; the pun (Devil *Fish*) lands. Dispatch is at draw303().

// the feral fish (only shown in Devil-Fish mode now — always red/fanged/spiky)
static void draw_fish(int m, float cx, float cy, float s){
  Pet *pt = &pet[m];
  float res=kv[m][1], cut=kv[m][0];
  int   feral = df[m];
  int   body  = feral ? CLR_RED : MCOL[m];
  float jit   = 0.5f + res*0.7f + pt->acc*1.2f + (feral?0.6f:0);   // wobblier when resonant/accented/feral
  // 303a and 303b are the SAME instrument (identical in ReBirth; acidcandy only transposes
  // 303b's base +1 octave). So the fish is the same SPECIES — any difference EMERGES from
  // state: 303b floats higher because its pitches are higher, wags faster only if busier.

  // note squash: stretch wide + a jelly wobble on the belly
  float sx = 1 + 0.35f*pt->hit;
  float sy = 1 - 0.22f*pt->hit + sinf(now()*11)*0.12f*res*(0.4f+pt->hit);
  // pitch → vertical bob; slide → horizontal lean of the whole body
  float bob  = -(pt->pcur-0.5f)*7*s;
  float lean = (pt->pit01-pt->pcur)*10*s;   // mid-slide the body leans toward the target
  cy += bob;

  // sub-osc shadow (Devil-Fish SUB) — a faint second belly below
  if (feral){ P sub[16]; mkoval(sub,cx,cy+3*s,9.5f*s*sx,4.5f*s,16); fill_poly(sub,16,0,0,m*100+9,jit*0.6f,CLR_DARK_RED); }

  // tail — wags faster the busier the line is (emergent: a lead usually IS busier)
  float wag = sinf(now()*(9+pt->energy*9) + m)*3.2f*s*(0.5f+pt->energy);
  float tx = cx-10*s;
  P tail[3] = {{tx,cy-1*s},{tx-6*s,cy-4.5f*s+wag},{tx-6*s,cy+4.5f*s+wag}};
  blob(tail,3,m*100+2,jit,1.0f*s, feral?CLR_DARK_RED:body, CLR_BLACK);

  // top fin — taller/spikier when feral
  float fh = (feral?7:4)*s;
  P fin[3] = {{cx-2*s,cy-5*s},{cx+2*s,cy-5*s},{cx,cy-5*s-fh}};
  blob(fin,3,m*100+3,jit,0.8f*s, feral?CLR_DARK_RED:body, CLR_BLACK);

  // body
  P bd[18]; mkoval(bd,cx+lean,cy,10*s*sx,5.5f*s*sy,18);
  blob(bd,18,m*100+1,jit,1.3f*s,body,CLR_BLACK);

  // eye (right side) — opens with CUT; pupil tracks slide + pitch
  float ex=cx+lean+5.5f*s, ey=cy-1.2f*s;
  float eo=0.35f+0.65f*cut;
  if (pt->blink>0){ line((int)(ex-2*s),(int)ey,(int)(ex+2*s),(int)ey,CLR_BLACK); }
  else {
    P we[12]; mkoval(we,ex,ey,2.4f*s,2.4f*s*eo,12); blob(we,12,m*100+4,jit*0.5f,0.6f*s,CLR_WHITE,CLR_BLACK);
    float px=ex+(pt->pit01-pt->pcur)*6*s, py=ey-(pt->pcur-0.5f)*2*s;
    circfill((int)px,(int)py,(int)fmaxf(1,1.1f*s),CLR_BLACK);
    if (feral) circfill((int)px,(int)py,(int)fmaxf(1,0.6f*s),CLR_RED);   // devil eye
  }
  // angry brow when feral
  if (feral) line((int)(ex-3*s),(int)(ey-3*s),(int)(ex+3*s),(int)(ey-1.5f*s),CLR_DARK_RED);

  // mouth — opens on accent/hit; fangs when feral
  float mo=0.2f+0.8f*fmaxf(pt->acc,pt->hit*0.6f);
  float mx=cx+lean+8.5f*s, my=cy+1.5f*s;
  P mth[10]; mkoval(mth,mx,my,2.2f*s,2.4f*s*mo,10); fill_poly(mth,10,0,0,m*100+5,jit*0.5f,CLR_DARKER_PURPLE);
  if (feral && mo>0.4f){
    trifill((int)(mx-1.8f*s),(int)(my-1.8f*s),(int)(mx-0.4f*s),(int)(my-1.8f*s),(int)(mx-1.1f*s),(int)(my+0.5f*s),CLR_WHITE);
    trifill((int)(mx+1.8f*s),(int)(my-1.8f*s),(int)(mx+0.4f*s),(int)(my-1.8f*s),(int)(mx+1.1f*s),(int)(my+0.5f*s),CLR_WHITE);
  }
}


// 303 vanilla = the ACID-HOUSE SMILEY. Round face (best 32px silhouette): CUT opens
// the eyes, RES jelly-wobbles the whole face, pitch bobs it, slide tilts it, a note
// squash-bounces it, an accent throws a big open grin.
static void draw_smiley(int m, float cx, float cy, float s){
  Pet *pt=&pet[m];
  float res=kv[m][1], cut=kv[m][0];
  float jit=0.4f + res*0.7f + pt->acc*1.0f;
  float sx=1 + 0.30f*pt->hit;
  float sy=1 - 0.20f*pt->hit + sinf(now()*11)*0.10f*res*(0.4f+pt->hit);
  float bob=-(pt->pcur-0.5f)*7*s;         // pitch → height
  float lean=(pt->pit01-pt->pcur)*8*s;    // slide → tilt toward the target
  cy+=bob;
  float R=7*s;

  // round face
  P fc[18]; mkoval(fc,cx+lean,cy,R*sx,R*sy,18);
  blob(fc,18,m*100+1,jit,1.4f*s,MCOL[m],CLR_BLACK);

  // eyes — CUT opens them, energetic line laughs (squint), gaze drifts with the slide/pitch
  Eye e = { .open=0.5f+0.45f*cut, .squint = pt->energy>0.55f?0.65f:pt->acc*0.4f,
            .pupil=0.5f, .gx=clamp((pt->pit01-pt->pcur)*3,-1,1), .gy=-(pt->pcur-0.5f)*1.2f,
            .lid=0, .browY=0.5f, .browA=0, .glint=1 };
  e.open *= fmaxf(0.0f, 1.0f - pt->blink*9.0f);   // blink = a transition
  pet_eyes(cx+lean, cy-R*0.18f, R*0.42f, fmaxf(1.0f,1.1f*s), fmaxf(1.0f,1.3f*s), CLR_BLACK, 0.6f, e);

  // grin — smiles more with energy, opens to an O on an accent
  pet_mouth(cx+lean, cy+R*0.34f, R*0.9f, 0.5f+pt->energy*0.5f, pt->acc*0.7f, CLR_BLACK, 0.6f);
}

static void draw303(int m, float cx, float cy, float s){
  if (df[m]) draw_fish(m,cx,cy,s);     // Devil Fish → the feral fish
  else       draw_smiley(m,cx,cy,s);   // vanilla acid → the smiley
}

// 808 = a big round warm PUFFER. FAMILIES: KICK drops the belly, SNARE shudders +
// clamps the eyes, HATS flick the ears, PERC pulses the cheeks. TUNE=size, DRIVE=grit.
static void draw808(float cx, float cy, float s){
  Pet *pt=&pet[M_808];
  float tune=kv[M_808][0], snap=kv[M_808][2], drive=kv[M_808][4], swg=kv[M_808][3];
  float base=0.9f+tune*0.4f;                        // TUNE → size
  float breathe=1+0.03f*sinf(now()*2);
  float sway=sinf(now()*2.2f)*swg*1.5f*s;           // SWING → idle lilt
  float sy=1-0.30f*pt->kick, sx=1+0.16f*pt->kick;   // KICK squash
  float drop=pt->kick*4*s;                           // …and drops on the thump
  float shud=pt->snare*sinf(now()*60)*1.6f*s;        // SNARE shudder (horizontal)
  int   body = drive>0.6f ? CLR_DARK_ORANGE : CLR_ORANGE;   // DRIVE → grittier tint
  float jit=0.4f+0.5f*snap + drive*0.6f;             // DRIVE → jaggeder outline
  cx+=sway+shud; cy+=drop;
  float rx=11*s*base*sx*breathe, ry=10*s*base*sy*breathe;

  // ears — flick up on HATS
  float lift=pt->hat*5*s;
  P le[3]={{cx-rx*0.6f,cy-ry*0.7f},{cx-rx*0.9f,cy-ry*0.9f-lift},{cx-rx*0.3f,cy-ry*0.95f}};
  P re[3]={{cx+rx*0.6f,cy-ry*0.7f},{cx+rx*0.9f,cy-ry*0.9f-lift},{cx+rx*0.3f,cy-ry*0.95f}};
  blob(le,3,801,jit,1.0f*s,body,CLR_BLACK);
  blob(re,3,802,jit,1.0f*s,body,CLR_BLACK);

  // body
  P bd[20]; mkoval(bd,cx,cy,rx,ry,20); blob(bd,20,800,jit,1.4f*s,body,CLR_BLACK);

  // cheeks — SNAP sets base puff, PERC pulses them
  float cr=(1.4f+snap*1.6f+pt->perc*1.8f)*s;
  circfill((int)(cx-rx*0.55f),(int)(cy+ry*0.15f),(int)cr,CLR_DARK_PEACH);
  circfill((int)(cx+rx*0.55f),(int)(cy+ry*0.15f),(int)cr,CLR_DARK_PEACH);

  // eyes — sleepy & content; SNARE crack clamps them shut, cowbell pops them wide
  Eye e = { .open=0.55f, .squint=0.0f, .gy=0.1f, .glint=1 };
  if (pt->perc>0.3f) e.open=0.95f;                     // cowbell/perc → wide-awake
  e.open *= fmaxf(0.0f, 1.0f - fmaxf(pt->blink, pt->snare>0.35f?pt->snare:0)*9.0f);  // snare/blink clamp (transition)
  pet_eyes(cx, cy-ry*0.22f, rx*0.4f, fmaxf(1.0f,1.2f*s), fmaxf(1.0f,1.3f*s), CLR_BLACK, 0.6f, e);
  // mouth — content, opens to an O on the kick
  pet_mouth(cx, cy+ry*0.42f, rx*0.7f, 0.4f, pt->kick*0.7f, CLR_DARK_BROWN, 0.6f);
}

// 909 = a chunky BEETLE (silhouette-first — survives 32px). "Electric/metallic" comes
// from a JAGGED OUTLINE (raised by METAL) + big amber eye-glow, NOT appendages.
// FAMILIES: KICK vertical squash-pop, SNARE horizontal jab, HATS micro-buzz, PERC tilt.
static void draw909(float cx, float cy, float s){
  Pet *pt=&pet[M_909];
  float metl=kv[M_909][3], tune=kv[M_909][0], dec=kv[M_909][1];
  float base=0.9f+tune*0.35f;
  float sy=1-0.28f*pt->kick, sx=1+0.18f*pt->kick;   // KICK squash-pop (vertical)
  float jab=pt->snare*sinf(now()*55)*2.2f*s;         // SNARE jab (horizontal lurch)
  float buzz=sinf(now()*70)*pt->hat*0.9f*s;          // HATS micro-buzz
  float tilt=pt->perc*0.5f;                           // PERC tilt (skew)
  // METAL → outline jaggedness (more boil jitter) + eye glow
  float jit=0.45f + metl*1.4f + pt->energy*0.4f;
  cx+=jab+buzz;

  // FLAM/STROKE → a faint stutter afterimage behind
  if (pt->flam>0.25f){ P g[10]; mkoval(g,cx-3*s,cy,8*s*base,6*s*base,10); fill_poly(g,10,0,0,919,jit,CLR_DARK_GREY); }

  // carapace — a rounded beetle body, jaggeder when METAL is up. tilt skews it (PERC).
  float rx=8*s*base*sx, ry=6.5f*s*base*sy;
  P bd[16]; for(int i=0;i<16;i++){ float a=(float)i/16*TAU;
    bd[i].x=cx+cosf(a)*rx + sinf(a)*tilt*rx; bd[i].y=cy+sinf(a)*ry; }
  blob(bd,16,900,jit,1.3f*s,CLR_MEDIUM_GREY,CLR_BLACK);
  // elytra seam down the back + a chrome highlight tick (reads as "shell/metal")
  line((int)(cx+tilt*ry),(int)(cy-ry*0.7f),(int)cx,(int)(cy+ry*0.8f),CLR_DARK_GREY);
  line((int)(cx-rx*0.4f),(int)(cy-ry*0.4f),(int)(cx-rx*0.15f),(int)(cy-ry*0.1f),CLR_LIGHT_GREY);

  // two amber pupil dots — alert & darty; brighten with METAL/energy, blink on the SNARE
  int ec = (metl>0.5f||pt->energy>0.4f) ? CLR_YELLOW : CLR_ORANGE;
  float er=fmaxf(1.2f, (1.2f+dec*0.4f)*s*base);
  Eye e = { .open=0.95f, .squint=0,
            .gx=sinf(now()*4)*0.5f*pt->energy, .gy=sinf(now()*3+1)*0.3f*pt->energy, .glint=1 };
  e.open *= fmaxf(0.0f, 1.0f - fmaxf(pt->blink, pt->snare>0.4f?pt->snare:0)*9.0f);   // snare/blink (transition)
  pet_eyes(cx, cy-ry*0.35f, rx*0.42f, er, er, ec, 0.6f, e);
}

// MST = the beating HEART the mix lives in. PUMP ducks it on the kick (sidechain
// made visible), FLT squints it, GLU hugs, FB rings out, energy brightens it.
static void drawmst(float cx, float cy, float s){
  Pet *pt=&pet[M_MST];
  float pump=kv[M_MST][4], flt=kv[M_MST][1], glu=kv[M_MST][0], fb=kv[M_MST][3];
  float duck = 1 - 0.35f*pump*pt->kick;            // sidechain: shrink on the kick
  float beat = 1 + 0.06f*sinf(now()*4);
  float sc = duck*beat;
  float jit=0.5f+pt->energy*0.5f;
  int body = pt->energy>0.4f ? CLR_RED : CLR_PINK;

  // feedback echo rings
  if (fb>0.2f){ float rr=fmodf(now()*22,26)*s*0.6f; circ((int)cx,(int)cy,(int)(rr+6*s),CLR_MAUVE);
                circ((int)cx,(int)cy,(int)(fmodf(now()*22+13,26)*s*0.6f+6*s),CLR_MAUVE); }

  // heart shape as a point path (two lobes + a bottom point)
  P h[16]; int n=0; float R=8*s*sc;
  for (int i=0;i<16;i++){ float t=(float)i/16*TAU; float x=16*powf(sinf(t),3);
    float y=13*cosf(t)-5*cosf(2*t)-2*cosf(3*t)-cosf(4*t);
    h[n].x=cx+x*R/16; h[n].y=cy-y*R/16; n++; }
  blob(h,n,500,jit,1.3f*s,body,CLR_DARK_RED);

  // hugging arms (GLU) — parentheses that close in as glue rises
  float aw=(6-glu*3)*s;
  line((int)(cx-aw-3*s),(int)(cy-2*s),(int)(cx-aw),(int)(cy+3*s),CLR_MAUVE);
  line((int)(cx+aw+3*s),(int)(cy-2*s),(int)(cx+aw),(int)(cy+3*s),CLR_MAUVE);

  // eyes — squint (happy) as the DJ filter closes off-centre; the kick clenches them
  float sq=1-fminf(1,fabsf(flt-0.5f)*2);            // 1 = wide open, 0 = squint
  Eye e = { .open=(0.5f+0.5f*sq)*(1-0.5f*pump*pt->kick), .squint=(1-sq)*0.7f, .glint=1 };
  e.open *= fmaxf(0.0f, 1.0f - pt->blink*9.0f);
  pet_eyes(cx, cy-1*s, 2.6f*s, fmaxf(1.1f,1.3f*s), fmaxf(1.1f,1.4f*s), CLR_BLACK, 0.6f, e);
  // a tiny content smile on the heart
  pet_mouth(cx, cy+3.5f*s, 6*s, 0.6f, 0, CLR_DARK_RED, 0.6f);
}

// approximate full design-unit HEIGHT of each creature (fin/ears/antennae included),
// so we can render one to a target PIXEL height: scale = targetPx / unitH.
static float unitH(int m){
  switch(m){ case M_303A: case M_303B: return df[m] ? 15 : 14;   // fish taller than the smiley
             case M_808: return 20; case M_909: return 14; case M_MST: return 16; } return 16;
}
static float scale_for(int m, float px){ return px / unitH(m); }

static void draw_pet(int m, float cx, float cy, float s){
  switch(m){
    case M_303A: case M_303B: draw303(m,cx,cy,s); break;
    case M_808: draw808(cx,cy,s); break;
    case M_909: draw909(cx,cy,s); break;
    case M_MST: drawmst(cx,cy,s); break;
  }
}

// ── sim ───────────────────────────────────────────────────────────────────────
static float pnorm(int semi){ return clamp((semi+12)/36.0f,0,1); }

static void decay(float *v, float rate){ *v -= dt()*rate; if(*v<0)*v=0; }

static void fire_step(int s){
  // 303s
  for (int m=0;m<2;m++){
    if (mute[m]) continue;
    if (on3[m][s]){ pet[m].hit=1; if(acc3[m][s]) pet[m].acc=1;
      pet[m].pit01=pnorm(pit3[m][s]); pet[m].energy=fminf(1,pet[m].energy+0.4f);
      if (!sld3[m][s]) pet[m].pcur=pet[m].pit01;   // no slide → snap; slide → glide in update
    }
  }
  // 808 — roster bucketed into KICK / SNARE / HATS / PERC families
  if (!mute[M_808]){
    if (g8[E_K][s]){ pet[M_808].kick=1; pet[M_MST].kick=1; }         // KICK  = BD
    if (g8[E_S][s]||g8[E_C][s]) pet[M_808].snare=1;                  // SNARE = SD, clap
    if (g8[E_H][s])  pet[M_808].hat=1;                               // HATS  = CH/OH/CY
    if (g8[E_CB][s]) pet[M_808].perc=1;                              // PERC  = toms/congas/cowbell
    pet[M_808].energy=fminf(1,pet[M_808].energy+0.15f);
  }
  // 909 — same four families
  if (!mute[M_909]){
    if (g9[N_K][s]){ pet[M_909].kick=1; pet[M_MST].kick=1; }         // KICK
    if (g9[N_S][s]||g9[N_CL][s]){ pet[M_909].snare=1; if(flam9[s]) pet[M_909].flam=1; }  // SNARE = SD, RS, clap
    if (g9[N_H][s]||g9[N_R][s]) pet[M_909].hat=1;                    // HATS  = CH/OH/CC/RC (ride)
    pet[M_909].energy=fminf(1,pet[M_909].energy+0.2f);
  }
  // MST energy = whole-mix activity
  pet[M_MST].energy=fminf(1, pet[M_303A].energy*0.3f+pet[M_303B].energy*0.3f+pet[M_808].energy*0.4f+pet[M_909].energy*0.4f);
}

void update(void){
  // transport
  if (playing){ clk16 += dt()*(tempo/60.0f)*4.0f; step=((int)clk16)%STEPS; }
  if (step!=prevstep){
    g_fseed = hashu((unsigned)(clk16)*2654435761u);   // BPM BOIL: new wobble frame each 16th
    if (playing) fire_step(step);
    prevstep=step;
  }

  // auto-drift knobs (unless the user grabbed one) → the pets keep morphing
  for (int m=0;m<M_N;m++) for(int i=0;i<5;i++){
    if (autodrift && !frozen[m][i]) kv[m][i]=0.5f+0.42f*sinf(now()*(0.13f+i*0.05f)+m*1.7f+i*0.9f);
  }

  // decays + glide
  for (int m=0;m<M_N;m++){
    decay(&pet[m].hit,7); decay(&pet[m].acc,5);
    decay(&pet[m].kick,6); decay(&pet[m].snare,7); decay(&pet[m].hat,10); decay(&pet[m].perc,6);
    decay(&pet[m].flam,8); decay(&pet[m].blink,4);
    pet[m].energy -= dt()*0.6f; if(pet[m].energy<0) pet[m].energy=0;
    // 303 pitch glide (slow = a slide)
    if (m<2){ float r = 6.0f; pet[m].pcur += (pet[m].pit01-pet[m].pcur)*fminf(1,dt()*r);
      pet[m].open = clamp(kv[m][0]*0.6f + pet[m].hit*kv[m][2]*0.8f,0,1); }
    // random blinks
    if (pet[m].blink<=0 && rnd(160)==0) pet[m].blink=0.12f;
  }

  // input ------------------------------------------------------------
  if (keyp(KEY_SPACE)) playing=!playing;
  if (keyp('A')) autodrift=!autodrift;
  if (keyp('R')){ for(int m=0;m<M_N;m++){ if(m<2){ for(int s=0;s<STEPS;s++){ on3[m][s]=rnd(2); pit3[m][s]=rnd(24)-6; acc3[m][s]=rnd(3)==0; sld3[m][s]=rnd(4)==0;} }
                    for(int l=0;l<E_N8;l++) for(int s=0;s<STEPS;s++) g8[l][s]=rnd(3)==0 || (l==E_K&&s%4==0);
                    for(int l=0;l<N_N9;l++) for(int s=0;s<STEPS;s++) g9[l][s]=(l==N_H)?1:(rnd(3)==0||(l==N_K&&s%4==0)); } }
  if (keyp('M')) mute[foc]=!mute[foc];
  if (keyp('D') && foc<2) df[foc]=!df[foc];
  for (int k=0;k<M_N;k++) if (keyp('1'+k)) foc=k;
  // click a cartridge to focus / click its LED to mute
  if (mouse_pressed(0)){
    int mx=mouse_x(), my=mouse_y();
    for (int i=0;i<M_N;i++){ int x=10+i*60;
      if (mx>=x+44 && mx<=x+56 && my>=8 && my<=20) { mute[i]=!mute[i]; }
      else if (mx>=x && mx<x+58 && my>=6 && my<=50) foc=i;
    }
  }
  // arrows nudge the focused machine's first two knobs (and freeze them from drift)
  int knob=-1; float d=0;
  if (key(KEY_UP)){knob=0;d=+1;} if(key(KEY_DOWN)){knob=0;d=-1;}
  if (key(KEY_LEFT)){knob=1;d=-1;} if(key(KEY_RIGHT)){knob=1;d=+1;}
  if (knob>=0){ frozen[foc][knob]=1; kv[foc][knob]=clamp(kv[foc][knob]+d*dt()*1.2f,0,1); }
}

// ── chrome ────────────────────────────────────────────────────────────────────
static void knobbar(int x, int y, float v, const char *name, int col){
  rect(x,y,8,22,CLR_DARK_GREY);
  int h=(int)(v*20); rectfill(x+1,y+21-h,6,h,col);
  print(name,x-1,y+24,CLR_MEDIUM_GREY);
}

void draw(void){
  cls(CLR_DARKER_BLUE);

  // ── the PET SHELF (nav strip) ──
  for (int i=0;i<M_N;i++){
    int x=10+i*60, y=6;
    int on = !mute[i];
    // cartridge body
    rrectfill(x,y,58,44,3, i==foc?CLR_DARK_PURPLE:CLR_INDIGO);
    if (i==foc) rrect(x,y,58,44,3,CLR_WHITE);
    // LED (mute toggle)
    circfill(x+50,y+14,3, on?CLR_LIME_GREEN:CLR_DARK_RED);
    // the tiny pet
    if (on) draw_pet(i,x+26,y+26,1.0f);
    else { print("z",x+22,y+22,CLR_MEDIUM_GREY); }   // asleep
    print(MNAME[i],x+4,y+2,CLR_LIGHT_GREY);
  }

  // ── the SIZE LADDER (focused pet at target device sizes) ──
  rrectfill(6,56,308,90,4,CLR_DARKER_PURPLE);
  print(MNAME[foc],10,60,MCOL[foc]);
  if (df[foc] && foc<2) print("DEVIL FISH",42,60,CLR_RED);
  if (mute[foc]) print("(muted)",272,60,CLR_MEDIUM_GREY);
  int   H[4]  = {64,48,32,24};
  int   LX[4] = {56,142,208,262};
  int   ly = 102;
  for (int k=0;k<4;k++){
    rect(LX[k]-H[k]/2, ly-H[k]/2, H[k], H[k], CLR_INDIGO);   // px footprint guide
    if (!mute[foc]) draw_pet(foc, LX[k], ly, scale_for(foc,(float)H[k]));
    char buf[8]; snprintf(buf,sizeof buf,"%dpx",H[k]);
    print(buf, LX[k]-H[k]/2, ly+H[k]/2+2, CLR_MEDIUM_GREY);
  }

  // ── readout: knobs + step row ──
  const char **kn=knames(foc);
  for (int i=0;i<5;i++) knobbar(20+i*30,150,kv[foc][i],kn[i], frozen[foc][i]?CLR_YELLOW:MCOL[foc]);

  // step row of the focused machine's primary lane
  for (int s=0;s<STEPS;s++){
    int x=182+s*8, y=152;
    int on = (foc<2)?on3[foc][s] : (foc==M_808?g8[E_K][s] : foc==M_909?g9[N_K][s] : (s%4==0));
    int ac = (foc<2)?acc3[foc][s] : 0;
    rectfill(x,y,7,14, on?(ac?CLR_RED:MCOL[foc]):CLR_INDIGO);
    if (s==step) rect(x,y,7,14,CLR_WHITE);
  }
  print(playing?"\x10":"||",182,168,CLR_LIGHT_GREY);

  // help
  print("1-5 focus  click:focus  LED/M:mute  D:devilfish  SPACE:play  A:auto  R:rnd  arrows:knobs",6,192,CLR_DARK_GREY);
}
