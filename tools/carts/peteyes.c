/* de:meta
{
  "slug": "peteyes",
  "collection": ["device-face"],
  "title": "peteyes (eye emotion machine)",
  "status": "active",
  "created": "2026-07-17",
  "kind": [
    "tool"
  ],
  "teaches": [],
  "description": "The second acid-pets building block (docs/design/acid-pets.md): a pair of EYES on a face, wired to the full cartoon-eye emotion machine — openness, squint (lower-lid raise), lid tilt (inner-down=angry / inner-up=sad), pupil size, gaze X/Y, and brows (raise + angle) + a glint. All scalar params (hookable to state like the petkit shape dials, procedural, no drawing). Tune with the slider column; number keys jump to emotion PRESETS (neutral/happy/angry/sad/surprised/sleepy/skeptical/dead) so you can see the machine express. Eyes + brows do most of a character's feeling; this gets them right before composing the whole pet."
}
de:meta */
#include "studio.h"
#define UI_COL_BG       CLR_INDIGO
#define UI_COL_FILL     CLR_MEDIUM_GREY
#define UI_COL_FILL_HOT CLR_LIGHT_GREY
#define UI_COL_TEXT     CLR_WHITE
#define UI_COL_TEXT_HOT CLR_WHITE
#include "ui.h"
#include <math.h>
#include <stdio.h>

#define INK   CLR_BROWNISH_BLACK
#define FACE  CLR_YELLOW          // the head colour (lids are painted in it, so they hide the sclera)

// ── the eye emotion machine — all scalars, hookable to state ──────────────────
typedef struct {
  float open;    // 0 shut .. 1 wide (top lid)
  float squint;  // 0 .. 1 lower-lid raise (happy/skeptical)
  float pupil;   // 0 pinpoint .. 1 big
  float gx, gy;  // gaze -1..1
  float lid;     // top-lid TILT: -1 sad (inner up) .. +1 angry (inner down)
  float browY;   // brow raise 0 low(furrowed) .. 1 high(surprised)
  float browA;   // brow angle -1 sad(inner up) .. +1 angry(inner down)
  int   glint;   // alive sparkle?
} Eye;

static void quad(float ax,float ay,float bx,float by,float cx,float cy,float dx,float dy,int col){
  trifill((int)ax,(int)ay,(int)bx,(int)by,(int)cx,(int)cy,col);
  trifill((int)ax,(int)ay,(int)cx,(int)cy,(int)dx,(int)dy,col);
}

// draw ONE eye. innerdir = +1 if the nose is to the RIGHT (left eye), -1 if to the left.
static void draw_eye(float cx, float cy, float er, int innerdir, const Eye *e){
  float top=cy-er, bot=cy+er;
  float inX=cx+innerdir*er, outX=cx-innerdir*er;   // inner/outer corners

  // HAPPY closed eye: a thick ∩ arc (peak up, ends down) — reads as laughing, unlike
  // a flat squint-slit which reads sleepy. Triggered by a high squint + low openness.
  int happy = (e->squint>0.55f && e->open<0.7f);
  if (happy){
    float aH=er*0.95f*e->squint;
    float pxp=0,pyp=0;
    for (int i=0;i<=12;i++){ float u=(float)i/12*2-1; float x=cx+u*er, y=cy - aH*(1-u*u) + er*0.35f;
      if (i>0){ line((int)pxp,(int)pyp,(int)x,(int)y,INK); line((int)pxp,(int)pyp+1,(int)x,(int)y+1,INK); }
      pxp=x; pyp=y; }
  } else {
    // sclera + a soft outline
    ovalfill((int)cx,(int)cy,(int)er,(int)er,CLR_WHITE);
    circ((int)cx,(int)cy,(int)er,INK);
    // pupil (drawn UNDER the lids, so a blink/squint covers it)
    float px=cx+e->gx*er*0.45f, py=cy+e->gy*er*0.45f, pr=er*(0.22f+e->pupil*0.4f);
    circfill((int)px,(int)py,(int)fmaxf(1,pr),INK);
    if (e->glint) circfill((int)(px-pr*0.35f),(int)(py-pr*0.35f),(int)fmaxf(1,pr*0.3f),CLR_WHITE);
    // TOP lid — covers from above down to a tilted line (open + lid tilt). Painted in
    // FACE colour so anything past the eye just repaints the head (no clipping needed).
    float baseY=top + (1.0f-e->open)*2.0f*er;
    float tilt=e->lid*er*0.8f;                        // + = inner lower = angry
    float inY=baseY+tilt, outY=baseY-tilt;
    quad(inX,top-6, outX,top-6, outX,outY, inX,inY, FACE);
    line((int)inX,(int)inY,(int)outX,(int)outY,INK);  // lid crease
    // BOTTOM lid — straight raise on squint (skeptical / half-lidded)
    float bY=bot - e->squint*1.4f*er;
    quad(cx-er,bot+6, cx+er,bot+6, cx+er,bY, cx-er,bY, FACE);
    if (e->squint>0.05f) line((int)(cx-er),(int)bY,(int)(cx+er),(int)bY,INK);
  }

  // BROW — raise (browY) + angle (browA), mirrored so "inner" is consistent
  float by0=top-4 - e->browY*5;
  float ba=e->browA*er*0.6f;                         // + = inner down = angry
  float bix=cx+innerdir*er*1.05f, box=cx-innerdir*er*1.05f;
  float biy=by0+ba, boy=by0-ba*0.7f;
  line((int)bix,(int)biy,(int)box,(int)boy,INK);
  line((int)bix,(int)biy+1,(int)box,(int)boy+1,INK);   // 2px thick
}

// ── emotion presets ───────────────────────────────────────────────────────────
//                       open squint pupil gx  gy   lid  browY browA glint
static const Eye PRESET[] = {
  { 0.85f,0.00f,0.50f, 0, 0,   0.00f, 0.55f, 0.00f, 1 },   // neutral
  { 0.50f,0.80f,0.50f, 0, 0,  -0.10f, 0.70f,-0.15f, 1 },   // happy  (^_^)
  { 0.70f,0.10f,0.35f, 0, 0,   0.55f, 0.10f, 0.75f, 1 },   // angry
  { 0.60f,0.00f,0.70f, 0, 0.35f,-0.45f,0.85f,-0.65f, 1 },  // sad
  { 1.00f,0.00f,0.35f, 0, 0,   0.00f, 1.00f,-0.10f, 1 },   // surprised
  { 0.28f,0.15f,0.50f, 0, 0.15f,0.05f, 0.30f, 0.05f, 1 },  // sleepy
  { 0.60f,0.30f,0.45f,-0.4f,0, 0.15f, 0.70f, 0.20f, 1 },   // skeptical (side-eye)
  { 0.90f,0.00f,0.10f, 0, 0,   0.00f, 0.50f, 0.00f, 0 },   // dead (pinpoint, no glint)
};
static const char *PNAME[] = { "neutral","happy","angry","sad","surprised","sleepy","skeptical","dead" };
#define NPRE ((int)(sizeof(PRESET)/sizeof(PRESET[0])))

// slider-backed values (0..1; the bipolar ones map to -1..1)
static float sl_open=0.85f, sl_squint=0, sl_pupil=0.5f, sl_gx=0.5f, sl_gy=0.5f,
             sl_lid=0.5f, sl_browy=0.55f, sl_browa=0.5f;
static int glint=1, preset=0;

static void load_preset(int i){
  const Eye *p=&PRESET[i];
  sl_open=p->open; sl_squint=p->squint; sl_pupil=p->pupil;
  sl_gx=(p->gx+1)*0.5f; sl_gy=(p->gy+1)*0.5f; sl_lid=(p->lid+1)*0.5f;
  sl_browy=p->browY; sl_browa=(p->browA+1)*0.5f; glint=p->glint; preset=i;
}

static float blinkt=0;

void update(void){
  for (int i=0;i<NPRE;i++) if (keyp('1'+i)) load_preset(i);
  if (keyp('G')) glint=!glint;
  if (keyp(KEY_SPACE)) blinkt=0.14f;     // manual blinkt
  if (blinkt<=0 && rnd(180)==0) blinkt=0.12f;
  if (blinkt>0) blinkt-=dt();
}

void draw(void){
  cls(CLR_DARKER_BLUE);
  ui_begin();

  // slider column (all procedural, all hookable)
  int sx=4, sw=54, sy=20, sp=11;
  ui_slider(&sl_open,   sx, sy+0*sp, sw, "OPEN");
  ui_slider(&sl_squint, sx, sy+1*sp, sw, "SQUINT");
  ui_slider(&sl_pupil,  sx, sy+2*sp, sw, "PUPIL");
  ui_slider(&sl_gx,     sx, sy+3*sp, sw, "GAZE X");
  ui_slider(&sl_gy,     sx, sy+4*sp, sw, "GAZE Y");
  ui_slider(&sl_lid,    sx, sy+5*sp, sw, "LID TILT");
  ui_slider(&sl_browy,  sx, sy+6*sp, sw, "BROW Y");
  ui_slider(&sl_browa,  sx, sy+7*sp, sw, "BROW ANG");

  Eye e = { sl_open, sl_squint, sl_pupil, sl_gx*2-1, sl_gy*2-1,
            sl_lid*2-1, sl_browy, sl_browa*2-1, glint };
  if (blinkt>0) e.open=0;   // blinkt overrides openness

  // the face (a Mr-Men head) + two eyes
  int fx=200, fy=90, fr=58;
  circfill(fx,fy,fr,FACE); circ(fx,fy,fr,INK);
  float er=18, gap=27;
  draw_eye(fx-gap, fy-4, er, +1, &e);   // left eye  (nose to its right)
  draw_eye(fx+gap, fy-4, er, -1, &e);   // right eye (nose to its left)

  // preset row
  print("presets (1-8):",96,182,CLR_MEDIUM_GREY);
  for (int i=0;i<NPRE;i++){
    int x=96+i*28;
    print(PNAME[i], x, 190, i==preset?CLR_WHITE:CLR_DARK_GREY);
  }
  print("current:",96,168,CLR_MEDIUM_GREY);
  print(PNAME[preset],140,168,CLR_YELLOW);
  print("G glint  SPACE blinkt",210,168,CLR_DARK_GREY);

  ui_end();
}
