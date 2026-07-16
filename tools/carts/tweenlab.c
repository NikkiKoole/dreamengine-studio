/* de:meta
{
  "slug": "tweenlab",
  "title": "tweenlab",
  "status": "active",
  "created": "2026-07-16",
  "kind": [
    "toy"
  ],
  "teaches": [
    "algorithm-visualization",
    "motion-sequencing"
  ],
  "lineage": "A homage to GreenSock/GSAP's tween + timeline + ease-visualizer, rebuilt in C. The engine ships only lerp + four eases (ease_in/out/in_out/back); this cart grows the whole GSAP ease family (sine/quad/cubic/expo/circ/back/elastic/bounce/steps) plus the ideas that make GSAP feel alive: a sequenced TIMELINE with overlapping position offsets (incl. the TimelineMax extras: timeline-level yoyo, currentLabel, tweenTo), motion paths, split-text, gsap.utils, staggered reveals, and repeat/yoyo/repeatDelay. Carries a spec() (52 assertions) pinning the ease endpoints/overshoot/quantization, gsap.utils, the Catmull-Rom motion path, seg_p clamping, and the timeline advance + yoyo bounce.",
  "description": {
    "summary": "A GreenSock (GSAP) playground: the ease-curve wall, a scrubbable timeline, staggered grids, and repeat/yoyo — the whole tween vocabulary made visible and playable.",
    "detail": "Four rooms of GSAP in C. (1) EASE WALL - sixteen easing curves drawn as graphs with a dot riding each one in a loop, so you feel back-out's overshoot vs elastic's wobble vs bounce - including GSAP's EasePack/Custom specials: rough (jagged flicker), slowmo (linear hang-time middle) and wiggle (damped oscillation). (2) TIMELINE - a logo animates in a sequence (slide in on back.out, pop in on elastic.out, spin on cubic.io, glow) laid out on a scrubbable track with overlapping position offsets, exactly like a gsap.timeline(); SPACE plays/pauses, V reverses, Y toggles TimelineMax yoyo (ping-pong at the ends), -/= set timeScale, arrows scrub, R restarts. Each segment start is a LABEL: the HUD shows the currentLabel, a callback fires (a blip + ring pop) as the playhead crosses one - either direction - CLICKING a label tick does a tweenTo() (glides the playhead there and stops); DRAGGING from one label to another does a tweenFromTo() (jumps to the first, glides to the second, stops). (3) STAGGER - a grid reveals with staggered delays; O cycles the stagger origin (start / center / edges / random). (4) REPEAT - three lanes contrasting repeat, yoyo, and repeatDelay. This is a toy for LOOKING at motion, the way GSAP's ease visualizer is.",
    "controls": "1-4 pick a room. EASE WALL: 16 curves incl. GSAP's specials (rough/slowmo/wiggle); watch the dots ride them. TIMELINE: SPACE play/pause, V reverse, -/= timeScale (0.25x..4x), LEFT/RIGHT scrub, R restart - each segment start is a LABEL that fires a callback blip+ring as the playhead crosses it. STAGGER: O cycles origin, R replays. H toggles help."
  }
}
de:meta */
#include "studio.h"
#include <math.h>

// ── TWEENLAB — GreenSock/GSAP in a fantasy console ──────────────────────────
// GSAP is a JS animation library; its soul is (a) a huge, expressive family of
// EASES and (b) TIMELINES that sequence tweens with overlaps, stagger, and
// repeat/yoyo. The engine only gives us lerp + four eases, so this cart grows
// the rest in C and makes each idea something you can WATCH.
//
//   1  EASE WALL  — the ease vocabulary as riding-dot curve graphs
//   2  TIMELINE   — a sequenced logo animation on a scrubbable track
//   3  STAGGER    — a grid revealed with staggered delays (cycle the origin)
//   4  REPEAT     — repeat vs yoyo vs repeatDelay, side by side
//
// The eases below are the standard GSAP set (Penner + GSAP's back/elastic/
// bounce constants). Each takes t in [0,1] and returns the eased value —
// which OVERSHOOTS past 0/1 for back and elastic. That overshoot is the point.

#define PI  3.14159265358979f
#define TAU 6.28318530717959f

// ── the ease family ─────────────────────────────────────────────────────────
static float e_linear(float t){ return t; }

static float e_sineIn (float t){ return 1.0f - cosf(t*PI*0.5f); }
static float e_sineOut(float t){ return sinf(t*PI*0.5f); }
static float e_sineIO (float t){ return -0.5f*(cosf(PI*t)-1.0f); }

static float e_quadIn (float t){ return t*t; }
static float e_quadOut(float t){ float u=1-t; return 1-u*u; }
static float e_quadIO (float t){ return t<0.5f? 2*t*t : 1-powf(-2*t+2,2)*0.5f; }

static float e_cubicIn (float t){ return t*t*t; }
static float e_cubicOut(float t){ float u=1-t; return 1-u*u*u; }
static float e_cubicIO (float t){ return t<0.5f? 4*t*t*t : 1-powf(-2*t+2,3)*0.5f; }

static float e_expoOut(float t){ return t>=1?1: 1-powf(2,-10*t); }

static float e_circIO(float t){ return t<0.5f? (1-sqrtf(1-4*t*t))*0.5f
                                              : (sqrtf(1-powf(-2*t+2,2))+1)*0.5f; }

static float e_backIn (float t){ float s=1.70158f; return t*t*((s+1)*t-s); }
static float e_backOut(float t){ float s=1.70158f; float u=t-1; return u*u*((s+1)*u+s)+1; }
static float e_backIO (float t){ float s=1.70158f*1.525f;
  return t<0.5f? (powf(2*t,2)*((s+1)*2*t-s))*0.5f
               : (powf(2*t-2,2)*((s+1)*(2*t-2)+s)+2)*0.5f; }

static float e_elasticOut(float t){ if(t<=0)return 0; if(t>=1)return 1;
  float p=0.3f; return powf(2,-10*t)*sinf((t-p/4)*TAU/p)+1; }
static float e_elasticIO (float t){ if(t<=0)return 0; if(t>=1)return 1;
  float p=0.45f; t*=2; if(t<1) return -0.5f*powf(2,10*t-10)*sinf((t-1-p/4)*TAU/p);
  return powf(2,-10*(t-1))*sinf((t-1-p/4)*TAU/p)*0.5f+1; }

static float e_bounceOut(float t){ const float n=7.5625f, d=2.75f;
  if(t<1/d)        return n*t*t;
  if(t<2/d){ t-=1.5f/d;  return n*t*t+0.75f; }
  if(t<2.5f/d){ t-=2.25f/d; return n*t*t+0.9375f; }
  t-=2.625f/d; return n*t*t+0.984375f; }

static float e_steps5(float t){ float s=floorf(t*5)/5.0f; return s>1?1:s; }  // steps(5)

// ── shared clock ─────────────────────────────────────────────────────────────
static int   scene   = 1;
static int   help    = 0;
static float gt      = 0.0f;   // free-running seconds (drives auto-loops)

// timeline (scene 2)
static float tl      = 0.0f;   // playhead seconds
static int   tl_play = 1;
static int   tl_dir  = 1;      // +1 forward, -1 reversed — GSAP's .reversed()/.timeScale(-1)
static float tl_scale= 1.0f;   // GSAP's .timeScale() — speed multiplier
static float tl_prev = 0.0f;   // last frame's playhead (to detect label crossings)
static float tl_flash= 0.0f;   // callback pulse timer (a label just fired)
static int   tl_flashseg=-1;   // which label fired (for the label text pop)
// TimelineMax extras (the features TimelineMax added over TimelineLite):
static int   tl_yoyo = 0;      // timeline-level yoyo — ping-pong at the ends instead of hard-looping
static int   tl_tweening=0;    // a tweenTo()/tweenFromTo() is scrubbing the playhead, then stopping
static float tl_tw_from=0, tl_tw_to=0, tl_tw_t=0;
static int   tl_press_label=-1;// label the mouse pressed on (click vs drag -> tweenTo vs tweenFromTo)

// stagger (scene 3)
static int   stag_origin = 0;  // 0 start, 1 center, 2 edges, 3 random
static float stag_t  = 0.0f;

// ── small helpers ─────────────────────────────────────────────────────────────
static float clampf(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }

// gsap.utils, rebuilt: the little math toolkit GSAP ships alongside the tweens
static float mapRange(float a,float b,float c,float d,float v){ return c+(v-a)/(b-a)*(d-c); }  // remap v from [a,b] to [c,d]
static float snapf(float inc,float v){ return floorf(v/inc+0.5f)*inc; }                        // round v to the nearest multiple of inc
static float wrapf(float lo,float hi,float v){ float r=hi-lo, m=fmodf(v-lo,r); if(m<0)m+=r; return lo+m; }  // wrap v into [lo,hi)

// ── GSAP's "special" eases (the EasePack / Custom plugins) ────────────────────
// SlowMo(linearRatio,power): steep in, a slow LINEAR middle (the "hang time"),
// steep out — GSAP's slow-motion ease. This is the real EasePack formula.
static float e_slowmo(float p){
    const float lr=0.7f, pw=0.7f;
    float p1=(1-lr)*0.5f, p3=p1+lr;         // 0.15 .. 0.85 is the linear stretch
    float r=p+(0.5f-p)*pw;
    if(p<p1){ float u=1-p/p1;      return r-(u*u*u)*r; }
    if(p>p3){ float u=(p-p3)/p1;   return r+(p-r)*(u*u*u); }
    return r;
}

// RoughEase: a jagged, randomized curve (flame flicker / static). Piecewise-linear
// through seeded random points perturbed off the linear ramp — deterministic.
#define ROUGH_N 17
static float rough_y[ROUGH_N];
static int   rough_ready=0;
static unsigned int rseed=0x9E3779B9u;
static float rrand(void){ rseed=rseed*1664525u+1013904223u; return (float)((rseed>>8)&0xFFFF)/65535.0f; }
static void rough_build(void){
    for(int i=0;i<ROUGH_N;i++){
        float x=(float)i/(ROUGH_N-1);
        float y=x+(rrand()*2-1)*0.42f;      // strength ~0.42 around the linear template
        rough_y[i]=(i==0)?0:(i==ROUGH_N-1)?1:clampf(y,0,1);
    }
    rough_ready=1;
}
static float e_rough(float t){
    if(!rough_ready) rough_build();
    if(t<=0)return 0; if(t>=1)return 1;
    float f=t*(ROUGH_N-1); int i=(int)f; if(i>ROUGH_N-2)i=ROUGH_N-2;
    return rough_y[i]+(f-i)*(rough_y[i+1]-rough_y[i]);
}

// CustomWiggle (type easeOut): a damped oscillation that wiggles then settles at
// the middle — N wiggles with amplitude tapering to 0. Centred at 0.5 to read.
static float e_wiggle(float t){ return 0.5f+0.5f*(1-t)*sinf(t*PI*6.0f); }

// draw a filled square rotated `deg` around its center.
// When the angle is axis-aligned (a multiple of 90 — and a square is identical at
// 0/90/180/270) draw a crisp rectfill: no two-triangle seam, and no float rounding
// skew from cosf(2*PI) != 1 exactly (the "won't settle back to 0deg" bug at rest).
static void quad_fill(float cx,float cy,float half,float deg,int col){
    float a360=fmodf(deg,360.0f); if(a360<0) a360+=360.0f;
    float m90=fmodf(a360,90.0f);
    if(m90<0.4f || m90>89.6f){
        int h=(int)(half+0.5f);
        rectfill((int)(cx+0.5f)-h, (int)(cy+0.5f)-h, h*2, h*2, col);
        return;
    }
    float a=deg*PI/180.0f, c=cosf(a), s=sinf(a);
    float ox[4]={-half,half,half,-half}, oy[4]={-half,-half,half,half};
    int px[4], py[4];
    // round (not truncate) so opposite corners stay symmetric -> no lopsided quad
    for(int i=0;i<4;i++){ px[i]=(int)floorf(cx+ox[i]*c-oy[i]*s+0.5f); py[i]=(int)floorf(cy+ox[i]*s+oy[i]*c+0.5f); }
    trifill(px[0],py[0], px[1],py[1], px[2],py[2], col);
    trifill(px[0],py[0], px[2],py[2], px[3],py[3], col);
}

// ── SCENE 1 : EASE WALL ───────────────────────────────────────────────────────
typedef struct { const char *name; float (*fn)(float); } Ease;
static const Ease WALL[16] = {
    {"linear",   e_linear},
    {"sine.out", e_sineOut},
    {"quad.io",  e_quadIO},
    {"cubic.in", e_cubicIn},
    {"cubic.out",e_cubicOut},
    {"expo.out", e_expoOut},
    {"circ.io",  e_circIO},
    {"back.out", e_backOut},
    {"back.io",  e_backIO},
    {"elastic.o",e_elasticOut},
    {"elastic.io",e_elasticIO},
    {"bounce.o", e_bounceOut},
    {"steps(5)", e_steps5},
    {"rough",    e_rough},        // EasePack: jagged/randomized
    {"slowmo",   e_slowmo},       // EasePack: linear hang-time middle
    {"wiggle",   e_wiggle},       // CustomWiggle: damped oscillation
};

static void draw_wall(void){
    // loop: ease over 1.0s, hold 0.6s, reset — the same "watch it settle" beat GSAP's visualizer uses
    float cyc = fmodf(gt, 1.6f);
    float tph = clampf(cyc, 0.0f, 1.0f);

    print("EASE WALL", 6, 3, CLR_WHITE);
    font(FONT_TINY);
    print("the GSAP ease vocabulary - dot rides each curve", 66, 4, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    int cols=4, rows=4, m=6, gap=5;
    int cw=(SCREEN_W-2*m-(cols-1)*gap)/cols;   // ~72
    int y0=14;
    int ch=(SCREEN_H-y0-4-(rows-1)*gap)/rows;  // ~41

    for(int i=0;i<16;i++){
        int cxs=m+(i%cols)*(cw+gap);
        int cys=y0+(i/cols)*(ch+gap);
        rectfill(cxs,cys,cw,ch,CLR_DARKER_GREY);
        rect(cxs,cys,cw,ch,CLR_DARK_GREY);

        int gx=cxs+3, gy=cys+9;           // graph origin (top-left of plot)
        int gw=cw-6, gh=ch-13;            // plot area (leave room for label + overshoot)
        int baseY=gy+gh;                  // t=0 line (value 0 at bottom)
        // faint 0 and 1 guide lines
        line(gx, baseY, gx+gw, baseY, CLR_DARK_GREY);
        line(gx, gy+2,  gx+gw, gy+2,  CLR_DARK_GREY);

        // the curve
        for(int px=0; px<=gw; px++){
            float tt=(float)px/gw;
            float ev=WALL[i].fn(tt);
            int yy=baseY-(int)(ev*(gh-2));
            yy=(int)clampf((float)yy, (float)cys+1, (float)cys+ch-2);
            pset(gx+px, yy, CLR_MEDIUM_GREY);
        }
        // the riding dot (bright) — its height IS the eased value right now
        float ev=WALL[i].fn(tph);
        int dx=gx+(int)(tph*gw);
        int dy=baseY-(int)(ev*(gh-2));
        dy=(int)clampf((float)dy, (float)cys+1, (float)cys+ch-2);
        // a ghost bar down to the baseline so the "position" reads
        line(dx, baseY, dx, dy, CLR_INDIGO);
        circfill(dx, dy, 2, CLR_YELLOW);

        font(FONT_TINY);
        print(WALL[i].name, cxs+3, cys+2, CLR_LIGHT_GREY);
        font(FONT_NORMAL);
    }
}

// ── SCENE 2 : TIMELINE ────────────────────────────────────────────────────────
// A gsap.timeline() sequences tweens; each has a start time (with overlap via
// position offsets like "-=0.3") and its own ease. Here four tweens animate one
// logo. We store [label,start,dur,ease] and evaluate each property from `tl`.
// Because every property is a PURE FUNCTION of the playhead, running tl backward
// (V, tl_dir=-1) plays the whole thing in reverse for free — GSAP's .reverse().
typedef struct { const char *label; float start, dur; float (*ease)(float); int col; } Seg;
static const Seg TLSEG[4] = {
    {"slide",  0.00f, 0.90f, e_backOut,    11},  // x: left -> center, back.out
    {"pop",    0.55f, 1.10f, e_elasticOut, 12},  // scale: 0 -> 1, elastic.out (overlaps slide, "-=0.35")
    {"spin",   1.55f, 0.70f, e_cubicIO,    14},  // rotate 0 -> 360, cubic.io
    {"glow",   2.25f, 0.85f, e_sineIO,      9},  // color pulse, sine.io — appended at spin's END (its onComplete), so the square is settled square before it shines
};
#define TL_TOTAL 3.1f
#define TL_LOOP  3.9f    // total + hold

// progress of segment s at time t (0 before, 1 after), eased
static float seg_p(int s, float t){
    float p=(t-TLSEG[s].start)/TLSEG[s].dur;
    p=clampf(p,0,1);
    return TLSEG[s].ease(p);
}

// the label whose track tick is nearest to screen-x mx (for click/drag targeting)
static int tl_nearest_label(int mx){
    int tx=10, tw=SCREEN_W-20, best=-1; float bd=1e9f;
    for(int i=0;i<4;i++){ int bx=tx+(int)(TLSEG[i].start/TL_LOOP*tw); float d=fabsf((float)(mx-bx)); if(d<bd){bd=d;best=i;} }
    return best;
}

static void draw_timeline(void){
    float t=tl;

    print("TIMELINE", 6, 3, CLR_WHITE);
    int cur=0; for(int i=0;i<4;i++) if(t>=TLSEG[i].start) cur=i;   // currentLabel(): closest label at/before now
    font(FONT_TINY);
    const char *st = tl_tweening?"tweenTo":(tl_play?(tl_dir>0?"PLAY>>":"PLAY<<"):"PAUSE");
    print(str("t=%.2f  %s  x%.2g  @%s%s", t, st, tl_scale, TLSEG[cur].label, tl_yoyo?"  yoyo":""), 66, 2, CLR_MEDIUM_GREY);
    print("SPACE V=rev Y=yoyo -/=speed <>scrub R | click=tweenTo drag=tweenFromTo", 66, 9, CLR_DARK_GREY);
    font(FONT_NORMAL);

    // evaluate the logo transform from the timeline
    float slide = seg_p(0,t);                 // 0..1
    float pop   = seg_p(1,t);                 // 0..1 (overshoots)
    float spin  = seg_p(2,t)*360.0f;          // degrees
    float glow  = seg_p(3,t);                 // 0..1 pulse

    float x = lerp(48.0f, SCREEN_W*0.5f, slide);
    float y = 100.0f;
    float half = 26.0f*pop;                   // scale in (elastic overshoot -> pops big then settles)
    int   col = glow>0.02f ? (glow>0.5f? CLR_WHITE : CLR_YELLOW) : 12;

    // motion path ghost (where the logo slid from)
    line(48, (int)y, (int)x, (int)y, CLR_DARKER_GREY);

    if(half>0.5f) quad_fill(x, y, half, spin, col);
    // a little inner mark so the spin reads even before it fills
    if(half>2){ int e=(int)(half*0.45f); rectfill((int)x-1,(int)(y-e),3,e*2,CLR_DARKER_GREY); }
    // callback pulse: a label just fired -> a ring expands OUT from the logo (drawn on top)
    if(tl_flash>0){ float k=(0.35f-tl_flash)/0.35f; int rr=(int)(30+k*30); circ((int)x,(int)y,rr,CLR_WHITE); }

    // ── the timeline track ──
    int tx=10, tw=SCREEN_W-20, ty=176, th=12;
    rectfill(tx,ty,tw,th,CLR_DARKER_GREY);
    rect(tx,ty,tw,th,CLR_DARK_GREY);
    // each segment as a block on the track
    for(int i=0;i<4;i++){
        int bx=tx+(int)(TLSEG[i].start/TL_LOOP*tw);
        int bw=(int)(TLSEG[i].dur/TL_LOOP*tw);
        int on = (t>=TLSEG[i].start && t<=TLSEG[i].start+TLSEG[i].dur);
        rectfill(bx+1,ty+1,bw-1,th-2, on?TLSEG[i].col:CLR_DARK_GREY);
        // label marker (a diamond tick at the segment start) — pops when its callback fires
        int fired = (tl_flashseg==i && tl_flash>0);
        int mc = fired?CLR_WHITE:CLR_MEDIUM_GREY;
        line(bx,ty-2,bx,ty+th+1, fired?CLR_WHITE:CLR_DARK_GREY);
        font(FONT_TINY);
        print(TLSEG[i].label, bx+2, ty-6, on?CLR_WHITE:mc);
        font(FONT_NORMAL);
    }
    // playhead
    int ph=tx+(int)(t/TL_LOOP*tw);
    line(ph,ty-8,ph,ty+th+1,CLR_RED);
}

// ── SCENE 3 : STAGGER ─────────────────────────────────────────────────────────
// gsap's stagger reveals a set of targets with an incremental delay. The delay
// ORDER is the trick: from the start, from center, from edges, or shuffled.
#define SG_COLS 9
#define SG_ROWS 5
#define SG_N   (SG_COLS*SG_ROWS)
static float sg_delay[SG_N];
static const char *STAG_NAME[4] = {"start","center","edges","random"};

static unsigned int sg_seed=0x2545F491u;
static float sg_rand(void){ sg_seed=sg_seed*1664525u+1013904223u; return (float)((sg_seed>>8)&0xFFFF)/65535.0f; }

static void stag_build(void){
    float step=0.055f;
    for(int i=0;i<SG_N;i++){
        int c=i%SG_COLS, r=i/SG_COLS;
        float d;
        if(stag_origin==0){                       // from start (row-major)
            d=i*step;
        } else if(stag_origin==1){                // from center (grid distance)
            float dc=c-(SG_COLS-1)*0.5f, dr=r-(SG_ROWS-1)*0.5f;
            d=sqrtf(dc*dc+dr*dr)*step*1.6f;
        } else if(stag_origin==2){                // from edges (inverse of center)
            float dc=c-(SG_COLS-1)*0.5f, dr=r-(SG_ROWS-1)*0.5f;
            float maxd=sqrtf(powf((SG_COLS-1)*0.5f,2)+powf((SG_ROWS-1)*0.5f,2));
            d=(maxd-sqrtf(dc*dc+dr*dr))*step*1.6f;
        } else {                                  // random
            d=sg_rand()* (SG_N*step*0.5f);
        }
        sg_delay[i]=d;
    }
    stag_t=0.0f;
}

static void draw_stagger(void){
    print("STAGGER", 6, 3, CLR_WHITE);
    font(FONT_TINY);
    print(str("from: %s", STAG_NAME[stag_origin]), 70, 4, CLR_YELLOW);
    print("O cycle origin   R replay", 150, 4, CLR_DARK_GREY);
    font(FONT_NORMAL);

    float dur=0.55f;
    int m=22, top=24;
    int gw=SCREEN_W-2*m, gh=SCREEN_H-top-14;
    int cw=gw/SG_COLS, chh=gh/SG_ROWS;
    for(int i=0;i<SG_N;i++){
        int c=i%SG_COLS, r=i/SG_COLS;
        float lt=(stag_t-sg_delay[i])/dur;
        float p=e_backOut(clampf(lt,0,1));
        if(lt<0) p=0;
        int cx=m+c*cw+cw/2, cy=top+r*chh+chh/2;
        int rad=(int)(p*(cw*0.32f));
        int col = lt<0? CLR_DARKER_GREY : (lt<1? CLR_ORANGE : CLR_YELLOW);
        if(rad>0) circfill(cx,cy,rad,col);
        else      circ(cx,cy,2,CLR_DARK_GREY);
    }
    // progress line
    float total=0; for(int i=0;i<SG_N;i++) if(sg_delay[i]>total) total=sg_delay[i];
    total+=dur;
    int bw=(int)(clampf(stag_t/total,0,1)*(SCREEN_W-20));
    rectfill(10,SCREEN_H-8,bw,3,CLR_INDIGO);
}

// ── SCENE 4 : REPEAT / YOYO / repeatDelay ─────────────────────────────────────
static void draw_repeat(void){
    print("REPEAT vs YOYO", 6, 3, CLR_WHITE);
    font(FONT_TINY);
    print("the same tween, three loop modes", 108, 4, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    float dur=1.4f;
    int x0=40, x1=SCREEN_W-40, span=x1-x0;
    const char *labels[3]={"repeat:-1  (hard loop)","yoyo:true  (ping-pong)","repeatDelay:0.5  (pause at ends)"};
    int cols[3]={11,12,14};

    for(int lane=0;lane<3;lane++){
        int ly=44+lane*46;
        line(x0,ly,x1,ly,CLR_DARK_GREY);
        circ(x0,ly,3,CLR_DARK_GREY); circ(x1,ly,3,CLR_DARK_GREY);
        font(FONT_TINY);
        print(labels[lane], x0, ly-16, CLR_LIGHT_GREY);
        font(FONT_NORMAL);

        float p;
        if(lane==0){                       // repeat: sawtooth, snaps back
            float ph=fmodf(gt,dur)/dur;
            p=e_cubicIO(ph);
        } else if(lane==1){                // yoyo: triangle (forward then back)
            float ph=fmodf(gt, dur*2)/dur;
            p = ph<1 ? e_cubicIO(ph) : e_cubicIO(2-ph);
        } else {                           // repeatDelay: run dur, hold delay
            float cycle=dur+0.5f;
            float ph=fmodf(gt,cycle);
            p = ph<dur ? e_cubicIO(ph/dur) : 1.0f;
        }
        int px=x0+(int)(p*span);
        line(x0,ly, px,ly, CLR_DARKER_GREY);
        circfill(px,ly,4,cols[lane]);
        circfill(px,ly,2,CLR_WHITE);
    }
}

// ── SCENE 5 : MOTIONPATH ──────────────────────────────────────────────────────
// GSAP's MotionPathPlugin moves a target along a path (SVG or x/y points) and can
// autoRotate so it banks to face its direction of travel. Here: waypoints -> a
// smooth Catmull-Rom loop, an arrow riding it, auto-rotating along the tangent.
#define MP_N 7
static const float MPX[MP_N]={ 60, 150, 250, 285, 200, 110,  40};
static const float MPY[MP_N]={ 60,  40,  55, 120, 150, 130, 120};
static int mp_autorot=1;

// closed-loop Catmull-Rom: point at param u in [0,MP_N)
static void mp_point(float u, float *ox, float *oy){
    int i=(int)u; float t=u-i;
    int i0=(i-1+MP_N)%MP_N, i1=i%MP_N, i2=(i+1)%MP_N, i3=(i+2)%MP_N;
    float t2=t*t, t3=t2*t;
    float b0=-0.5f*t3+t2-0.5f*t, b1=1.5f*t3-2.5f*t2+1, b2=-1.5f*t3+2*t2+0.5f*t, b3=0.5f*t3-0.5f*t2;
    *ox=b0*MPX[i0]+b1*MPX[i1]+b2*MPX[i2]+b3*MPX[i3];
    *oy=b0*MPY[i0]+b1*MPY[i1]+b2*MPY[i2]+b3*MPY[i3];
}

static void draw_motionpath(void){
    print("MOTIONPATH", 6, 3, CLR_WHITE);
    font(FONT_TINY);
    print(str("waypoints -> smooth path, arrow rides it   autoRotate:%s (A)", mp_autorot?"on":"off"), 84, 4, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // draw the smooth path (sampled) + the waypoints
    float px,py,ppx=0,ppy=0;
    for(int s=0;s<=MP_N*16;s++){
        float u=fmodf((float)s/16.0f, MP_N);
        mp_point(u,&px,&py);
        if(s>0) line((int)ppx,(int)ppy,(int)px,(int)py,CLR_DARK_GREY);
        ppx=px; ppy=py;
    }
    for(int i=0;i<MP_N;i++){ circfill((int)MPX[i],(int)MPY[i],2,CLR_INDIGO); }

    // the traveller: position now + a look-ahead to get the heading (tangent)
    float u=fmodf(gt*0.9f, MP_N);
    float ax,ay,bx,by;
    mp_point(u,&ax,&ay);
    mp_point(fmodf(u+0.05f,MP_N),&bx,&by);
    float head = mp_autorot ? atan2f(by-ay,bx-ax)*180.0f/PI : 0.0f;

    // an arrow (triangle) pointing along travel — rotate its 3 points by `head`
    float a=head*PI/180.0f, c=cosf(a), s=sinf(a);
    float lx[3]={9,-6,-6}, ly[3]={0,-5,5};   // arrow in local space, nose at +x
    int qx[3],qy[3];
    for(int i=0;i<3;i++){ qx[i]=(int)(ax+lx[i]*c-ly[i]*s); qy[i]=(int)(ay+lx[i]*s+ly[i]*c); }
    trifill(qx[0],qy[0],qx[1],qy[1],qx[2],qy[2],CLR_YELLOW);
}

// ── SCENE 6 : SPLITTEXT ───────────────────────────────────────────────────────
// GSAP's SplitText breaks a word into per-character pieces so each animates on its
// own — the classic staggered reveal. Here each letter tumbles in (drop + spin)
// on a stagger, with backOut so it overshoots and settles. E cycles the effect.
static const char *ST_WORD="TWEEN";
#define ST_LEN 5
static int st_fx=0;   // 0 tumble, 1 drop, 2 rise, 3 spin-in-place
static const char *ST_FXNAME[4]={"tumble","drop","rise","spin"};

static void draw_splittext(void){
    print("SPLITTEXT", 6, 3, CLR_WHITE);
    font(FONT_TINY);
    print(str("per-letter staggered reveal   effect: %s (E)", ST_FXNAME[st_fx]), 74, 4, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    float cyc=fmodf(gt, 3.0f);     // reveal ~1.4s then hold, then loop
    int scale=4, chw=8*scale, sp=chw+2;
    int total=ST_LEN*sp - 2;
    int x0=(SCREEN_W-total)/2, baseY=95;

    for(int i=0;i<ST_LEN;i++){
        float lt=(cyc - i*0.13f)/0.6f;     // stagger 0.13s/char, each 0.6s long
        float p=e_backOut(clampf(lt,0,1));
        if(lt<0) p=0;
        char ch[2]={ST_WORD[i],0};
        int cx=x0+i*sp;
        float yoff=0, deg=0;
        if(st_fx==0){ yoff=(1-p)*-70; deg=(1-p)*300; }     // tumble: fall + spin
        else if(st_fx==1){ yoff=(1-p)*-70; }                // drop
        else if(st_fx==2){ yoff=(1-p)* 70; }                // rise
        else { deg=(1-p)*260; }                             // spin in place
        int col = lt<1 ? CLR_YELLOW : CLR_WHITE;
        if(p>0.02f) print_rot_scaled(ch, cx+chw/2, (int)(baseY+yoff), deg, scale, col, 1);
    }
    // a faint baseline so the settle reads
    line(x0-6, baseY+chw/2+4, x0+total+6, baseY+chw/2+4, CLR_DARKER_GREY);
}

// ── SCENE 7 : UTILS ───────────────────────────────────────────────────────────
// gsap.utils is a little math toolkit that ships with the tweens. Drive one input
// value (auto-sweeps; mouse overrides) and watch mapRange / snap / wrap / clamp
// transform it live.
static void util_row(int y,const char*name,float in01,float out01,int col,const char*val){
    int x0=90, x1=SCREEN_W-16, span=x1-x0;
    font(FONT_TINY);
    print(name, 8, y-3, CLR_LIGHT_GREY);
    print(val, x1-30, y-9, col);
    font(FONT_NORMAL);
    line(x0,y,x1,y,CLR_DARK_GREY);
    int gx=x0+(int)(clampf(in01,0,1)*span);      // input ghost
    line(gx,y-4,gx,y+4,CLR_DARKER_GREY);
    int ox=x0+(int)(clampf(out01,0,1)*span);     // output ball
    circfill(ox,y,3,col);
}

static void draw_utils(void){
    print("UTILS", 6, 3, CLR_WHITE);
    font(FONT_TINY);
    print("gsap.utils - drive input (mouse or auto), watch it transform", 46, 4, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // input value in 0..1: mouse across the track area, else an auto sweep
    float t;
    int mx=mouse_x();
    if(mx>10 && mx<SCREEN_W-10) t=clampf(mapRange(90,SCREEN_W-16,0,1,(float)mx),0,1);
    else t=0.5f+0.5f*sinf(gt*0.9f);

    float mr=mapRange(0,1,-50,50,t);                 // remap to a signed range
    util_row(40,"mapRange", t, mapRange(-50,50,0,1,mr), CLR_ORANGE, str("%+.0f", mr));
    float sn=snapf(0.1f,t);                          // snap to a 0.1 grid
    util_row(74,"snap .1",  t, sn, CLR_GREEN, str("%.1f", sn));
    float wr=wrapf(0,1,t*2.5f);                       // wrap a value past the end
    util_row(108,"wrap x2.5",t*2.5f>1?1:t*2.5f, wr, CLR_INDIGO, str("%.2f", wr));
    float cl=clampf(t,0.25f,0.75f);                  // clamp to a window
    util_row(142,"clamp .25-.75", t, cl, CLR_RED, str("%.2f", cl));
    // draw the snap grid ticks on the snap row
    for(int i=0;i<=10;i++){ int gx=90+(int)((float)i/10*(SCREEN_W-16-90)); pset(gx,74,CLR_MEDIUM_GREY); }
}

// ── help overlay ──────────────────────────────────────────────────────────────
static void draw_help(void){
    rectfill(30,40,SCREEN_W-60,SCREEN_H-80,CLR_DARKER_GREY);
    rect(30,40,SCREEN_W-60,SCREEN_H-80,CLR_YELLOW);
    int x=40,y=50;
    print("TWEENLAB - GSAP in a cart", x,y,CLR_YELLOW); y+=14;
    font(FONT_SMALL);
    print("A playground for GreenSock's ideas, in C:", x,y,CLR_WHITE); y+=10;
    print("1 EASE WALL - 16 curves incl rough/slowmo/wiggle", x,y,CLR_LIGHT_GREY); y+=8;
    print("2 TIMELINE  - V rev, Y yoyo, -/=speed, click=tweenTo", x,y,CLR_LIGHT_GREY); y+=8;
    print("3 STAGGER   - grid reveal, O cycles the origin", x,y,CLR_LIGHT_GREY); y+=8;
    print("4 REPEAT    - repeat vs yoyo vs repeatDelay", x,y,CLR_LIGHT_GREY); y+=8;
    print("5 MOTIONPATH- arrow rides a path, A autoRotate", x,y,CLR_LIGHT_GREY); y+=8;
    print("6 SPLITTEXT - per-letter reveal, E cycles effect", x,y,CLR_LIGHT_GREY); y+=8;
    print("7 UTILS     - mapRange/snap/wrap/clamp, live", x,y,CLR_LIGHT_GREY); y+=12;
    print("The engine ships lerp + 4 eases; this cart grows the", x,y,CLR_MEDIUM_GREY); y+=8;
    print("rest + timelines, motion paths, split-text, utils.", x,y,CLR_MEDIUM_GREY); y+=12;
    print("H closes this help.", x,y,CLR_YELLOW);
    font(FONT_NORMAL);
}

// ── loop ────────────────────────────────────────────────────────────────────
void update(void){
    float dt=1.0f/60.0f;
    gt+=dt;

    if(keyp('1')) scene=1;
    if(keyp('2')) scene=2;
    if(keyp('3')){ scene=3; stag_build(); }
    if(keyp('4')) scene=4;
    if(keyp('5')) scene=5;
    if(keyp('6')) scene=6;
    if(keyp('7')) scene=7;
    if(keyp('H')) help=!help;

    if(scene==5 && keyp('A')) mp_autorot=!mp_autorot;
    if(scene==6 && keyp('E')) st_fx=(st_fx+1)%4;

    if(scene==2){
        tl_prev=tl;
        if(keyp(' ')) { tl_play=!tl_play; tl_tweening=0; }
        if(keyp('R')) { tl=0; tl_dir=1; tl_scale=1.0f; tl_tweening=0; }
        if(keyp('V')) tl_dir=-tl_dir;                 // reverse — the whole anim plays backward
        if(keyp('Y')) tl_dir = (tl_yoyo=!tl_yoyo) ? tl_dir : 1;   // TimelineMax yoyo — ping-pong at the ends
        if(keyp('-')) tl_scale=clampf(tl_scale*0.5f,0.25f,4.0f);   // GSAP .timeScale()
        if(keyp('=')) tl_scale=clampf(tl_scale*2.0f,0.25f,4.0f);

        // track gestures: CLICK a label = tweenTo() (glide from here to it, then stop);
        // DRAG label A -> B = tweenFromTo() (jump to A, then glide to B, then stop).
        int ty=176, th=12, on_track = mouse_y()>=ty-9 && mouse_y()<=ty+th+2;
        if(mouse_pressed(MOUSE_LEFT) && on_track) tl_press_label = tl_nearest_label(mouse_x());
        if(mouse_released(MOUSE_LEFT) && tl_press_label>=0){
            int rel = on_track ? tl_nearest_label(mouse_x()) : tl_press_label;
            tl_tweening=1; tl_play=0; tl_tw_t=0;
            if(rel==tl_press_label){                          // click -> tweenTo
                tl_tw_from=tl;                        tl_tw_to=TLSEG[rel].start;
            } else {                                          // drag -> tweenFromTo
                tl_tw_from=TLSEG[tl_press_label].start; tl_tw_to=TLSEG[rel].start;
                tl=tl_tw_from; tl_prev=tl;                    // jump to FROM (no callback for the jump)
            }
            tl_press_label=-1;
        }

        int wrapped=0;
        if(tl_tweening){                              // a tweenTo scrub in progress (linear-ish glide, then stop)
            tl_tw_t += dt/0.5f;
            tl = tl_tw_from + (tl_tw_to-tl_tw_from)*e_cubicIO(clampf(tl_tw_t,0,1));
            if(tl_tw_t>=1){ tl=tl_tw_to; tl_tweening=0; }
        } else if(tl_play){
            tl+=dt*tl_dir*tl_scale;
            if(tl_yoyo){                              // bounce off the ends
                if(tl>=TL_LOOP){ tl=TL_LOOP; tl_dir=-1; }
                if(tl<0)       { tl=0;       tl_dir= 1; }
            } else {                                  // hard loop (wrap both ways)
                if(tl>=TL_LOOP){ tl-=TL_LOOP; wrapped=1; }
                if(tl<0)       { tl+=TL_LOOP; wrapped=1; }
            }
        }
        // scrub
        if(key(KEY_LEFT))  { tl=clampf(tl-dt*2.0f,0,TL_LOOP); tl_play=0; tl_tweening=0; }
        if(key(KEY_RIGHT)) { tl=clampf(tl+dt*2.0f,0,TL_LOOP); tl_play=0; tl_tweening=0; }
        // LABELS + CALLBACKS: each segment start is a label; fire a blip+flash as the
        // playhead crosses it (either direction) — GSAP's timeline callbacks/onComplete.
        if(!wrapped){
            float lo=tl_prev<tl?tl_prev:tl, hi=tl_prev<tl?tl:tl_prev;
            for(int i=0;i<4;i++){
                float L=TLSEG[i].start;
                if(L>lo && L<=hi){ tl_flash=0.35f; tl_flashseg=i; note(72+i*3, INSTR_TRI, 3); }
            }
        }
        if(tl_flash>0) tl_flash-=dt;
    }
    if(scene==3){
        if(keyp('O')){ stag_origin=(stag_origin+1)%4; stag_build(); }
        if(keyp('R')) stag_build();
        stag_t+=dt;
        // auto-replay after everything has settled + a beat
        float total=0; for(int i=0;i<SG_N;i++) if(sg_delay[i]>total) total=sg_delay[i];
        if(stag_t>total+0.55f+0.8f) stag_t=0;
    }
}

void draw(void){
    cls(CLR_BLACK);
    switch(scene){
        case 1: draw_wall();       break;
        case 2: draw_timeline();   break;
        case 3: draw_stagger();    break;
        case 4: draw_repeat();     break;
        case 5: draw_motionpath(); break;
        case 6: draw_splittext();  break;
        case 7: draw_utils();      break;
    }
    if(help) draw_help();
}

// ── spec() : the cart-logic gate (node tools/spec.js tweenlab) ────────────────
// All the tweening math is pure, so it pins cleanly: ease endpoints/overshoot,
// gsap.utils, the Catmull-Rom motion path, seg_p clamping — plus a couple of
// behavioural checks driving the timeline through step().
#ifdef DE_SPEC
#include "spec.h"
void spec(void){
    // ── the ease family: every standard ease starts at 0 and ends at 1 ──
    for(int i=0;i<16;i++){
        if(WALL[i].fn==e_wiggle) continue;                 // wiggle is a damped oscillation, settles at 0.5 (by design)
        expect(spec_close(WALL[i].fn(0.f),0.f,0.01f), str("%s: f(0)=0", WALL[i].name));
        expect(spec_close(WALL[i].fn(1.f),1.f,0.01f), str("%s: f(1)=1", WALL[i].name));
    }
    expect(spec_close(e_wiggle(0.f),0.5f,0.01f), "wiggle: settles centred (f(0)=0.5)");
    expect(spec_close(e_wiggle(1.f),0.5f,0.01f), "wiggle: settles centred (f(1)=0.5)");

    // ── symmetric eases pass through 0.5 at the midpoint ──
    expect(spec_close(e_linear(0.5f),0.5f,0.001f),"linear midpoint = 0.5");
    expect(spec_close(e_sineIO(0.5f),0.5f,0.01f), "sine.io midpoint = 0.5");
    expect(spec_close(e_cubicIO(0.5f),0.5f,0.01f),"cubic.io midpoint = 0.5");
    expect(spec_close(e_slowmo(0.5f),0.5f,0.01f), "slowmo passes through the middle at 0.5");

    // ── back.out OVERSHOOTS past 1 before settling (the "pop") ──
    expect(e_backOut(0.6f) > 1.0f,                "back.out overshoots above 1");
    // ── steps(5) quantizes to 0,.2,.4,.6,.8,1 ──
    expect(spec_close(e_steps5(0.19f),0.0f,0.001f),"steps(5): 0.19 -> 0.0");
    expect(spec_close(e_steps5(0.5f), 0.4f,0.001f),"steps(5): 0.50 -> 0.4");

    // ── gsap.utils ──
    expect(spec_close(mapRange(0,1,0,100,0.5f),50.f,0.01f),  "mapRange 0..1 -> 0..100 @0.5 = 50");
    expect(spec_close(mapRange(0,1,-50,50,0.5f),0.f,0.01f),  "mapRange 0..1 -> -50..50 @0.5 = 0");
    expect(spec_close(snapf(0.1f,0.86f),0.9f,0.001f),        "snap .1 of 0.86 = 0.9");
    expect(spec_close(snapf(0.1f,0.84f),0.8f,0.001f),        "snap .1 of 0.84 = 0.8");
    expect(spec_close(wrapf(0,1,2.5f),0.5f,0.001f),          "wrap 0..1 of 2.5 = 0.5");
    expect(spec_close(wrapf(0,1,-0.25f),0.75f,0.001f),       "wrap 0..1 of -0.25 = 0.75");
    expect(spec_close(clampf(0.9f,0.25f,0.75f),0.75f,0.001f),"clamp .25-.75 of 0.9 = 0.75");

    // ── MotionPath: closed Catmull-Rom loop passes THROUGH its waypoints ──
    float ax,ay,bx,by;
    mp_point(0.f,&ax,&ay); mp_point((float)MP_N,&bx,&by);
    expect(spec_near(ax,bx) && spec_near(ay,by),          "motion path is a closed loop (u=0 == u=N)");
    mp_point(2.f,&ax,&ay);
    expect(spec_near(ax,MPX[2]) && spec_near(ay,MPY[2]),  "motion path hits waypoint 2 at integer u");

    // ── seg_p clamps outside a segment's window ──
    expect(spec_close(seg_p(2,0.0f),0.f,0.01f), "seg_p(spin) = 0 before the segment");
    expect(spec_close(seg_p(2,9.0f),1.f,0.01f), "seg_p(spin) = 1 after the segment");

    // ── behavioural: the timeline advances, and TimelineMax yoyo bounces off the end ──
    scene=2; tl=0.5f; tl_play=1; tl_dir=1; tl_scale=1.0f; tl_yoyo=0; tl_tweening=0;
    step(6);
    expect(tl > 0.5f, "timeline advances while playing");
    tl=TL_LOOP-0.01f; tl_dir=1; tl_yoyo=1; tl_tweening=0; tl_play=1;
    step(3);
    expect(tl_dir==-1, "yoyo flips direction at the end (ping-pong)");
}
#endif
