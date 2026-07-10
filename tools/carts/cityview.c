/* de:meta
{
  "slug": "cityview",
  "title": "cityview",
  "status": "active",
  "created": "2026-06-24",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "isometric-projection",
    "camera-follow"
  ],
  "lineage": "The missing height layer for the flat top-down lineage (sloop/cityplan/streetlab render footprints only). Takes their quadfill house style and grafts on screen-up wall extrusion to get a drivable Zelda/GTA1 cityscape; a four-mode bench to pick the projection before wiring it into the world composer.",
  "description": "RENDER TEST: pseudo-3D top-down city (GTA1 meets Zelda) WITH drivable raised highways folded in (was the separate overpass experiment). Drive a seeded, brick/window/block/stucco-TEXTURED city. M cycles five building view modes: (1) NORTH-UP camera-locked, always the south fronts, zero diagonals; (2) HEADING-UP camera rotates to heading, you see the 1-2 walls you drive toward, every wall UP still screen-up (the keeper); (3) BILLBOARD flat cards (props not boxes); (4) LEAN-OUT roofs pushed off-centre (the melty contrast); (5) PERSPECTIVE a real pitched pinhole camera (perspective divide -> horizon, foreshortening, towering buildings) the parallel-oblique modes can't do, with live pitch/eye tuning (,. and ;'). F lays a FLYOVER over the city: off / straight overpass / s-curve / spiral on-ramp / two-level stack interchange. A flyover is a road whose nodes carry a z; because height extrudes straight up the screen it reads as an elevated deck (running surface + dashed centre line + fascia thickness + pillars + dithered ground shadow). Drive UP a ramp and the camera rises (camz) so the city sinks below; the car z snaps to the nearest REACHABLE deck so you board via ramps and drive UNDER high decks. T toggles wall textures. Proves the ADR-0021 projected-primitive vocabulary in one place: project() (world x,y,z -> screen), pdisc (a ground circle is an ELLIPSE once rotated -> projected N-gon: ponds, car shadow, future roundabout islands), pline/pdash (projected dashed line: the deck centre line), pproj_poly (project-then-polyfill: lots + deck shadows). Whole renderer is flat quadfill + tritex-textured walls. Arrows drive, M view, F flyover, T textures, [ ] zoom, R reseed. The cityscape-renderer bench for big-game seam #2."
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// CITYVIEW — pseudo-3D top-down city: GTA1 meets Zelda. A bench for the
// "see the FRONTS, height goes straight UP the screen, no needless diagonals"
// look — now with DRIVABLE RAISED HIGHWAYS folded in (was overpass.c).
//
// Press M to cycle FIVE building view modes:
//   1 NORTH-UP    camera locked north; you always see the south-facing fronts.
//   2 HEADING-UP  camera rotates to heading; you see the 1-2 walls you drive
//                 toward, every wall's UP still screen-up. (the keeper)
//   3 BILLBOARD   heading-up, buildings face you as flat cards (props, not boxes).
//   4 LEAN-OUT    roofs pushed away from screen centre — the melty contrast.
//   5 PERSPECTIVE a real pitched PINHOLE camera (perspective divide → horizon,
//                 foreshortening, towering buildings) — the thing modes 1-4's
//                 parallel-oblique projection can't do. Tune live: ,/. pitch  ;/' eye.
//
// Press F to lay a FLYOVER over the city (off/straight/s-curve/spiral/stack).
// A flyover is just a road whose nodes carry a Z: because height extrudes
// straight up the screen, a deck that ramps up reads as an elevated structure
// (running surface + dashed centre line + fascia + pillars + ground shadow).
// Drive UP the ramp and the camera rises with you (camz) so the city sinks
// below; the car's Z snaps to the nearest REACHABLE deck, so you board via a
// ramp and drive UNDER high decks without popping onto them.
//
// THE PROJECTED-PRIMITIVE HELPERS (ADR 0021 — these graduate to a runtime
// header once a second 3D front-end wants them; proven here first):
//   project()  the one adapter — world (x,y,z) → screen, rotated+scaled, z up.
//   pdisc()    a ground circle is an ELLIPSE once rotated — fill a projected
//              N-gon, never circfill. (ponds, car shadow, roundabout islands)
//   pline()/pdash()  projected (dashed) line — project the endpoints, draw.
//   pproj_poly()     project-then-polyfill — corners/islands/deck shadows.
//
// CONTROLS — arrows drive · M view (5=perspective) · F flyover · T textures · [ ] zoom · R city
//            in PERSPECTIVE: ,/. tilt pitch · ;/' eye height

#define PITCH   42          // block centre-to-centre, world units
#define ROADH   7.0f        // half road width
#define RANGE   7           // cells drawn each side of the car
#define HSCALE  1.0f        // how tall a height unit reads on screen
#define NMAT    6
#define TILE_W  6.0f        // world units per 16px texture tile (keeps bricks crisp)
#define QUARTER 0x7BDE      // ~25% black dither (1-bits transparent)
#define MAXREP  6           // cap texture repeats per wall (stress-test ceiling)
#define PN      48          // nodes per flyover deck
#define DECKW   20.0f       // deck width (world)
#define DECKT   4.0f        // deck thickness (fascia depth)
#define BARRIER 3.0f        // side-barrier height
#define PHALF   3.5f        // pillar half-width
#define PEVERY  5           // a pillar every N nodes
#define REACH   12.0f       // max z step to board/stay on a deck (drive-under gate)

// material → wall texture slot (sprites in cityview.cart.js): 0 brick 1 block 2 glass 3 stucco
static const int MATSLOT[NMAT] = { 1, 0, 3, 2, 1, 3 };
static int   clampi(int v, int lo, int hi) { return v<lo?lo:(v>hi?hi:v); }
static float clampf(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }
static float smooth01(float u){ u=clampf(u,0,1); return u*u*(3-2*u); }

typedef struct { int roof, lit, shade; } Mat;
static const Mat MAT[NMAT] = {
  { CLR_LIGHT_GREY,  CLR_MEDIUM_GREY, CLR_DARK_GREY    },  // concrete
  { CLR_LIGHT_PEACH, CLR_ORANGE,      CLR_BROWN        },  // brick
  { CLR_PINK,        CLR_DARK_RED,    CLR_DARK_PURPLE  },  // painted
  { CLR_BLUE,        CLR_TRUE_BLUE,   CLR_DARKER_BLUE  },  // glass
  { CLR_LIME_GREEN,  CLR_MEDIUM_GREEN,CLR_DARK_GREEN   },  // green roof
  { CLR_LIGHT_YELLOW,CLR_DARK_ORANGE, CLR_DARK_BROWN   },  // sand
};

enum { M_NORTH, M_HEADING, M_BILLBOARD, M_LEANOUT, M_PERSP, M_COUNT };
static const char *MODE_NAME[M_COUNT] = {
  "1 NORTH-UP (zelda)", "2 HEADING-UP (fronts)", "3 BILLBOARD", "4 LEAN-OUT", "5 PERSPECTIVE (pitched)"
};
enum { F_OFF, F_STRAIGHT, F_CURVE, F_SPIRAL, F_STACK, F_COUNT };
static const char *FNAME[F_COUNT] = { "off", "straight", "s-curve", "spiral", "stack" };

typedef struct { float x, y, z; } Node;
typedef struct { Node n[PN]; int cnt; float w; } Deck;

typedef struct {
  float px, py, ang, spd, carz;   // car: pos, heading(deg), speed, deck height
  float camx, camy, camz, rot;    // camera: eased pos + height + rotation(deg)
  float zoom;
  float pitch, eye, setback;      // M_PERSP camera: tilt(deg below horizontal), eye height, chase setback
  int   mode, fly, seed, ndeck;
  bool  started, tex;
  Deck  deck[2];
} State;
static State S;

// deterministic per-cell hash → 0..1 floats
static unsigned hashu(int x, int y, int salt) {
  unsigned h = (unsigned)(x*374761393 + y*668265263 + salt*2246822519u + S.seed*3266489917u);
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
static float hf(int x, int y, int salt) { return (hashu(x,y,salt) & 0xffffff) / (float)0xffffff; }

// a building living in cell (gx,gy): footprint rect (world) + height + material
typedef struct { float x0,y0,x1,y1,h; int mat; bool exists; } Bldg;
static Bldg cell_bldg(int gx, int gy) {
  Bldg b; b.exists = false;
  float cx = gx*(float)PITCH, cy = gy*(float)PITCH;
  if (hf(gx,gy,9) < 0.12f) return b;            // ~12% of lots are empty (parks/plazas)
  float lot = PITCH*0.5f - ROADH;               // half-extent of the buildable lot
  float mx = lot * (0.55f + 0.40f*hf(gx,gy,1)); // building half-extents
  float my = lot * (0.55f + 0.40f*hf(gx,gy,2));
  float jx = (hf(gx,gy,3)-0.5f) * (lot-mx)*1.2f;// jitter inside the lot
  float jy = (hf(gx,gy,4)-0.5f) * (lot-my)*1.2f;
  b.x0 = cx+jx-mx; b.x1 = cx+jx+mx;
  b.y0 = cy+jy-my; b.y1 = cy+jy+my;
  float t = hf(gx,gy,5); b.h = 5 + t*t*46;       // skewed toward short, a few towers
  b.mat = hashu(gx,gy,6) % NMAT;
  b.exists = true;
  return b;
}

// ── flyover layout for the current F mode ───────────────────────────────────
static float ramp(float t){ float rf=0.30f;
  float u=(t<rf)?t/rf:(t>1-rf)?(1-t)/rf:1.0f; return smooth01(u); }

static void build_flyover(void){
  S.ndeck=0;
  if (S.fly==F_OFF) return;
  Deck *d=&S.deck[0]; d->cnt=PN; d->w=DECKW;
  if (S.fly==F_STRAIGHT){
    for(int i=0;i<PN;i++){ float t=(float)i/(PN-1); d->n[i]=(Node){ -200+400*t, 0, ramp(t)*28.0f }; }
    S.ndeck=1;
  } else if (S.fly==F_CURVE){
    for(int i=0;i<PN;i++){ float t=(float)i/(PN-1); d->n[i]=(Node){ -200+400*t, sin_deg(t*360)*56.0f, ramp(t)*30.0f }; }
    S.ndeck=1;
  } else if (S.fly==F_SPIRAL){
    int sp=30; float R=55, H=38;
    for(int i=0;i<sp;i++){ float t=(float)i/(sp-1); float a=-90+270*t;
      d->n[i]=(Node){ cos_deg(a)*R, sin_deg(a)*R+R, smooth01(t)*H }; }
    float dx=d->n[sp-1].x-d->n[sp-2].x, dy=d->n[sp-1].y-d->n[sp-2].y, L=sqrtf(dx*dx+dy*dy)+1e-4f; dx/=L; dy/=L;
    for(int i=sp;i<PN;i++){ int k=i-sp+1; d->n[i]=(Node){ d->n[sp-1].x+dx*9*k, d->n[sp-1].y+dy*9*k, H }; }
    d->cnt=PN; S.ndeck=1;
  } else { // F_STACK
    for(int i=0;i<PN;i++){ float t=(float)i/(PN-1); d->n[i]=(Node){ -190+380*t, 0, ramp(t)*20.0f }; }
    Deck *e=&S.deck[1]; e->cnt=PN; e->w=DECKW;
    for(int i=0;i<PN;i++){ float t=(float)i/(PN-1); e->n[i]=(Node){ 0, -190+380*t, ramp(t)*38.0f }; }
    S.ndeck=2;
  }
}

// project a world point (x,y,z) to screen — height AND camera-height go up-screen
static void project(float wx, float wy, float wz, float *sx, float *sy) {
  float dx = wx - S.camx, dy = wy - S.camy;
  float c = cos_deg(S.rot), s = sin_deg(S.rot);
  float rx = dx*c - dy*s, ry = dx*s + dy*c;     // rotate world by S.rot (rx=right, ry=forward)
  if (S.mode == M_PERSP) {
    // a real pitched PINHOLE camera, chasing from behind+above: perspective divide by depth →
    // foreshortening + a horizon + towering buildings (the thing the parallel-oblique modes can't do).
    // pitch = degrees the optical axis tilts BELOW horizontal (small = look ahead, 90 = straight down).
    float F  = S.setback - ry;                  // forward ahead of the eye (AHEAD is -ry in this heading-up frame)
    float Hh = (wz - S.camz) - S.eye;           // height relative to the eye
    float cd = cos_deg(S.pitch), sd = sin_deg(S.pitch);
    float depth = F*cd - Hh*sd;                 // along the optical axis (into screen)
    float up    = F*sd + Hh*cd;                 // screen-vertical (positive = up); taller Hh → higher
    if (depth < 1.0f) depth = 1.0f;             // near clamp (at/behind the lens)
    float f = S.zoom * 90.0f;                    // focal length (× zoom)
    *sx = SCREEN_W*0.5f + f * rx / depth;
    *sy = SCREEN_H*0.5f - f * up / depth;
    return;
  }
  *sx = SCREEN_W*0.5f + rx*S.zoom;
  *sy = SCREEN_H*0.5f + ry*S.zoom - (wz - S.camz)*S.zoom*HSCALE;
}
static void wpt(float x,float y,float z,int*ix,int*iy){ float a,b; project(x,y,z,&a,&b); *ix=(int)a; *iy=(int)b; }

// ── projected-primitive helpers (ADR 0021) ──────────────────────────────────
// a circle on the GROUND is an ellipse once the camera rotates → fill an N-gon.
static void pdisc(float cx,float cy,float r,float z,int color){
  enum { N=14 }; int xy[2*N];
  for(int i=0;i<N;i++){ float a=i*(360.0f/N); wpt(cx+cos_deg(a)*r, cy+sin_deg(a)*r, z, &xy[2*i], &xy[2*i+1]); }
  polyfill(xy,N,color);
}
// project a polygon's world corners (all at height z) and fill — respects fillp().
static void pproj_poly(const float*wx,const float*wy,int n,float z,int color){
  int xy[2*8]; if(n>8) n=8;
  for(int i=0;i<n;i++) wpt(wx[i],wy[i],z,&xy[2*i],&xy[2*i+1]);
  polyfill(xy,n,color);
}
// projected line / dashed line — project both endpoints, draw at any height.
static void pline(float x0,float y0,float z0,float x1,float y1,float z1,int color){
  int ax,ay,bx,by; wpt(x0,y0,z0,&ax,&ay); wpt(x1,y1,z1,&bx,&by); line(ax,ay,bx,by,color);
}
static void pdash(float x0,float y0,float z0,float x1,float y1,float z1,float dash,int color){
  float dx=x1-x0,dy=y1-y0,dz=z1-z0; float L=sqrtf(dx*dx+dy*dy)+1e-4f;
  int seg=(int)(L/dash); if(seg<1) seg=1;
  for(int i=0;i<seg;i+=2){ float t0=(float)i/seg,t1=(float)(i+1)/seg;
    pline(x0+dx*t0,y0+dy*t0,z0+dz*t0, x0+dx*t1,y0+dy*t1,z0+dz*t1, color); }
}

// ── flyover geometry ────────────────────────────────────────────────────────
// deck height at (wx,wy) REACHABLE from curz — board via ramps, drive under high decks.
static bool deck_height(float wx,float wy,float curz,float*z){
  float best=1e9f; bool on=false;
  for(int d=0;d<S.ndeck;d++) for(int i=0;i<S.deck[d].cnt;i++){
    Node *p=&S.deck[d].n[i];
    float dx=p->x-wx, dy=p->y-wy, dd=dx*dx+dy*dy, hw=S.deck[d].w*0.5f;
    if(dd<hw*hw && fabsf(p->z-curz)<REACH && dd<best){ best=dd; *z=p->z; on=true; }
  }
  return on;
}

// a vertical box (pillars): footprint cx,cy ± half, z0..z1
static void draw_box(float cx,float cy,float hx,float hy,float z0,float z1,int lit,int sh,int roof){
  float wx[4]={cx-hx,cx+hx,cx+hx,cx-hx}, wy[4]={cy-hy,cy-hy,cy+hy,cy+hy};
  int bx[4],by[4],tx[4],ty[4];
  for(int i=0;i<4;i++){ wpt(wx[i],wy[i],z0,&bx[i],&by[i]); wpt(wx[i],wy[i],z1,&tx[i],&ty[i]); }
  float c=cos_deg(S.rot), s=sin_deg(S.rot);
  float nwx[4]={0,1,0,-1}, nwy[4]={-1,0,1,0};
  int order[4],n=0; float key_[4];
  for(int e=0;e<4;e++){ float nsy=nwx[e]*s+nwy[e]*c; if(nsy>0.001f){ order[n]=e; key_[n]=nsy; n++; } }
  for(int i=0;i<n;i++) for(int j=i+1;j<n;j++) if(key_[j]<key_[i]){
    float t=key_[i];key_[i]=key_[j];key_[j]=t; int o=order[i];order[i]=order[j];order[j]=o; }
  for(int k=0;k<n;k++){ int e=order[k], a=e, bb=(e+1)&3;
    quadfill(bx[a],by[a],bx[bb],by[bb],tx[bb],ty[bb],tx[a],ty[a], key_[k]>0.6f?lit:sh); }
  quadfill(tx[0],ty[0],tx[1],ty[1],tx[2],ty[2],tx[3],ty[3], roof);
}

static void deck_edge(Deck*d,int i,float zoff,int*lx,int*ly,int*rx,int*ry){
  int j=(i+1<d->cnt)?i+1:i, k=(i>0)?i-1:i;
  float dx=d->n[j].x-d->n[k].x, dy=d->n[j].y-d->n[k].y, L=sqrtf(dx*dx+dy*dy)+1e-4f;
  float px=-dy/L, py=dx/L, hw=d->w*0.5f; Node*nd=&d->n[i];
  wpt(nd->x+px*hw, nd->y+py*hw, nd->z+zoff, lx, ly);
  wpt(nd->x-px*hw, nd->y-py*hw, nd->z+zoff, rx, ry);
}

static void draw_deck_seg(Deck*d,int i){
  int la,lay,ra,ray, lb,lby,rb,rby;
  deck_edge(d,i,0,&la,&lay,&ra,&ray); deck_edge(d,i+1,0,&lb,&lby,&rb,&rby);
  int la2,lay2,ra2,ray2, lb2,lby2,rb2,rby2;
  deck_edge(d,i,-DECKT,&la2,&lay2,&ra2,&ray2); deck_edge(d,i+1,-DECKT,&lb2,&lby2,&rb2,&rby2);
  quadfill(la,lay,lb,lby,lb2,lby2,la2,lay2, CLR_BROWNISH_BLACK);   // fascia (thickness)
  quadfill(ra,ray,rb,rby,rb2,rby2,ra2,ray2, CLR_BROWNISH_BLACK);
  quadfill(la,lay,ra,ray,rb,rby,lb,lby, CLR_DARK_GREY);            // running surface
  pdash(d->n[i].x,d->n[i].y,d->n[i].z+0.3f, d->n[i+1].x,d->n[i+1].y,d->n[i+1].z+0.3f, 4.0f, CLR_YELLOW);
  int lat,layt,rat,rayt, lbt,lbyt,rbt,rbyt;
  deck_edge(d,i,BARRIER,&lat,&layt,&rat,&rayt); deck_edge(d,i+1,BARRIER,&lbt,&lbyt,&rbt,&rbyt);
  quadfill(la,lay,lb,lby,lbt,lbyt,lat,layt, CLR_LIGHT_GREY);       // barriers
  quadfill(ra,ray,rb,rby,rbt,rbyt,rat,rayt, CLR_LIGHT_GREY);
}

// ── lifecycle ──────────────────────────────────────────────────────────────
static void reset(void) {
  build_flyover();
  if (S.ndeck>0) {
    Node a=S.deck[0].n[0], b=S.deck[0].n[1];
    S.px=a.x; S.py=a.y; S.ang=atan2f(b.y-a.y,b.x-a.x)*57.2957795f;
  } else {
    S.px=6; S.py=ROADH+2; S.ang=0;
  }
  S.spd=0; S.carz=0; S.camx=S.px; S.camy=S.py; S.camz=0;
  if (!S.started) { S.mode = M_HEADING; S.zoom = 2.0f; S.tex = true;
                    S.pitch = 25.0f; S.eye = 22.0f; S.setback = 35.0f; }
  S.rot = (S.mode==M_NORTH) ? 0.0f : (-90.0f - S.ang);
  S.started = true;
}

void init(void) { S.seed = 1; S.fly = F_SPIRAL; reset(); S.zoom = 1.5f; }  // open on the showpiece

void update(void) {
  if (keyp('M')) S.mode = (S.mode+1) % M_COUNT;
  if (keyp('1')) S.mode = M_NORTH;
  if (keyp('2')) S.mode = M_HEADING;
  if (keyp('3')) S.mode = M_BILLBOARD;
  if (keyp('4')) S.mode = M_LEANOUT;
  if (keyp('5')) S.mode = M_PERSP;
  if (keyp('T')) S.tex = !S.tex;
  if (keyp('F')) { S.fly = (S.fly+1) % F_COUNT; reset(); }
  if (key('[')) S.zoom = fmaxf(0.9f, S.zoom*0.97f);
  if (key(']')) S.zoom = fminf(6.0f, S.zoom*1.03f);
  if (keyp('R')) { S.seed++; reset(); }
  // M_PERSP camera tuning (play with the pitched-perspective look):
  if (key(',')) S.pitch = clampf(S.pitch-1.0f, 5.0f, 89.0f);   // tilt toward horizontal
  if (key('.')) S.pitch = clampf(S.pitch+1.0f, 5.0f, 89.0f);   // tilt toward top-down
  if (key(';')) S.eye = fmaxf(2.0f, S.eye-1.0f);               // lower the eye
  if (key('\'')) S.eye = fminf(200.0f, S.eye+1.0f);            // raise the eye

  // simple arcade driving
  float thr = (key(KEY_UP)?1:0) - (key(KEY_DOWN)?1:0);
  float steer = (key(KEY_RIGHT)?1:0) - (key(KEY_LEFT)?1:0);
  S.spd += thr*0.06f;
  S.spd *= 0.97f;                               // drag
  if (fabsf(S.spd) < 0.005f) S.spd = 0;
  S.ang += steer * (1.6f + fminf(fabsf(S.spd)*2.0f, 2.4f)) * (S.spd>=0?1:-1);
  S.px += cos_deg(S.ang)*S.spd;
  S.py += sin_deg(S.ang)*S.spd;

  // drive UP onto a reachable deck; camera rises so the city sinks below
  float tz=0; deck_height(S.px,S.py,S.carz,&tz);
  S.carz += (tz - S.carz)*0.25f;
  S.camx += (S.px - S.camx)*0.18f;
  S.camy += (S.py - S.camy)*0.18f;
  S.camz += (S.carz - S.camz)*0.12f;

  // camera rotation: locked north, or eased toward heading-up
  float target = (S.mode == M_NORTH) ? 0.0f : (-90.0f - S.ang);
  float d = fmodf(target - S.rot + 540.0f, 360.0f) - 180.0f;
  S.rot += d * 0.16f;
}

// texture-map one wall quad: ground edge a→b, roof edge ca→cb (screen coords).
// the wall is subdivided into RX×RY tiles, each mapped to the FULL 16px texture
// slot — that REPEATS the bricks (no stretch) AND keeps each affine triangle
// small (no PS1 swim). a dither-shadow overlay darkens side-facing walls.
static void wall_tex(float ax,float ay, float bx,float by, float cax,float cay, float cbx,float cby,
                     int slot, int rxN, int ryN, float facing) {
  int scol = slot % 8, srow = slot / 8;
  float su = scol*16.0f, sv = srow*16.0f;
  float u0 = su+0.5f, u1 = su+15.5f, vtop = sv+0.5f, vbot = sv+15.5f;
  for (int j=0;j<ryN;j++) {
    float g0=(float)j/ryN, g1=(float)(j+1)/ryN;   // 0 = ground, 1 = roof
    for (int i=0;i<rxN;i++) {
      float f0=(float)i/rxN, f1=(float)(i+1)/rxN;  // along the wall
      #define PX(f,g) (int)(((ax+(bx-ax)*(f))*(1-(g))) + ((cax+(cbx-cax)*(f))*(g)))
      #define PY(f,g) (int)(((ay+(by-ay)*(f))*(1-(g))) + ((cay+(cby-cay)*(f))*(g)))
      int x00=PX(f0,g0),y00=PY(f0,g0), x10=PX(f1,g0),y10=PY(f1,g0);
      int x11=PX(f1,g1),y11=PY(f1,g1), x01=PX(f0,g1),y01=PY(f0,g1);
      #undef PX
      #undef PY
      tritex(x00,y00,u0,vbot, x10,y10,u1,vbot, x11,y11,u1,vtop);
      tritex(x00,y00,u0,vbot, x11,y11,u1,vtop, x01,y01,u0,vtop);
    }
  }
  // sparse patterns only — a 50% checker collapses into diagonal lines on a
  // sheared wall. front wall = clean, mid = dots, most side-on = ~25% dither.
  int pat = facing>0.66f ? 0 : (facing>0.40f ? FILL_DOTS : QUARTER);
  if (pat) {
    fillp(pat, -1);                               // black on 0-bits, holes transparent
    quadfill((int)ax,(int)ay,(int)bx,(int)by,(int)cbx,(int)cby,(int)cax,(int)cay, CLR_BLACK);
    fillp_reset();
  }
}

// draw one building's geometry (modes 1/2/4) — visible walls then roof cap.
static void draw_bldg_geo(Bldg *b, bool leanout) {
  // footprint corners CCW-ish
  float wx[4] = { b->x0, b->x1, b->x1, b->x0 };
  float wy[4] = { b->y0, b->y0, b->y1, b->y1 };
  float gx[4], gy[4], rx[4], ry[4];
  for (int i=0;i<4;i++) project(wx[i], wy[i], 0, &gx[i], &gy[i]);

  if (leanout) {
    // roof pushed AWAY from screen centre, scaled by height — the GTA extrusion
    for (int i=0;i<4;i++) {
      float dirx = gx[i]-SCREEN_W*0.5f, diry = gy[i]-SCREEN_H*0.5f;
      float k = b->h * 0.012f;
      rx[i] = gx[i] + dirx*k; ry[i] = gy[i] + diry*k;
    }
  } else {
    // roof straight UP the screen by height — the Zelda extrusion
    for (int i=0;i<4;i++) project(wx[i], wy[i], b->h, &rx[i], &ry[i]);
  }

  // visible walls: a wall is exposed iff its screen-space outward normal points
  // OPPOSITE the roof's shift direction (the side the roof slides off of).
  //   straight-up extrusion → shift = screen-up, so the down-facing (south) walls
  //   show;  lean-out → shift = radially away from screen centre, so the walls
  //   facing back TOWARD centre show. One test, both behaviours.
  // edges: 0:(0-1) 1:(1-2) 2:(2-3) 3:(3-0)
  float c = cos_deg(S.rot), s = sin_deg(S.rot);
  // world outward normals of each edge for an axis-aligned footprint:
  // edge0 top(-y): (0,-1) · edge1 right(+x): (1,0) · edge2 bottom(+y): (0,1) · edge3 left(-x): (-1,0)
  float nwx[4] = { 0,  1, 0, -1 };
  float nwy[4] = {-1,  0, 1,  0 };
  int   order[4]; int n=0; float key_[4];
  if (S.mode == M_PERSP) {
    // PERSPECTIVE: true back-face cull in plan view — a wall shows iff its outward normal faces the
    // eye (the oblique "opposite the roof shift" test assumes screen-up = up, which perspective breaks).
    float bcx=(b->x0+b->x1)*0.5f - S.camx, bcy=(b->y0+b->y1)*0.5f - S.camy;
    float brx = bcx*c - bcy*s, bry = (bcx*s + bcy*c) - S.setback;  // building centre relative to the eye (yaw space; eye sits +setback behind)
    float bl = sqrtf(brx*brx+bry*bry)+1e-4f;
    for (int e=0;e<4;e++) {
      float nsx = nwx[e]*c - nwy[e]*s, nsy = nwx[e]*s + nwy[e]*c;
      float facing = -(nsx*brx + nsy*bry)/bl;   // >0 = normal points back toward the eye; ~1 = head-on
      if (facing > 0.001f) { order[n]=e; key_[n]=facing; n++; }
    }
  } else {
    float sdx, sdy;                             // unit roof-shift direction (screen)
    if (leanout) {
      float bcx=(gx[0]+gx[1]+gx[2]+gx[3])*0.25f - SCREEN_W*0.5f;
      float bcy=(gy[0]+gy[1]+gy[2]+gy[3])*0.25f - SCREEN_H*0.5f;
      float L=sqrtf(bcx*bcx+bcy*bcy)+1e-4f; sdx=bcx/L; sdy=bcy/L;
    } else { sdx=0; sdy=-1; }
    for (int e=0;e<4;e++) {
      float nsx = nwx[e]*c - nwy[e]*s;          // rotate world normal into screen
      float nsy = nwx[e]*s + nwy[e]*c;
      float facing = -(nsx*sdx + nsy*sdy);      // >0 = exposed wall
      if (facing > 0.001f) { order[n]=e; key_[n]=facing; n++; }
    }
  }
  // sort visible walls by facing (least-front first → painter's within the box)
  for (int i=0;i<n;i++) for (int j=i+1;j<n;j++)
    if (key_[j] < key_[i]) { float t=key_[i];key_[i]=key_[j];key_[j]=t; int o=order[i];order[i]=order[j];order[j]=o; }

  // perspective draws the roof FIRST so the near (front) walls paint OVER it (street-level
  // occlusion); the oblique modes draw it LAST so it caps the top. tritex is affine (wrong under
  // perspective) and heavy on big close-up walls, so perspective uses flat-shaded walls.
  bool persp = (S.mode == M_PERSP);
  if (persp)
    quadfill((int)rx[0],(int)ry[0],(int)rx[1],(int)ry[1],(int)rx[2],(int)ry[2],(int)rx[3],(int)ry[3], MAT[b->mat].roof);

  for (int k=0;k<n;k++) {
    int e = order[k]; int a=e, bb=(e+1)&3;
    if (S.tex && !persp) {
      float wlen = (e&1) ? (b->y1-b->y0) : (b->x1-b->x0);
      int rxN = clampi((int)(wlen/TILE_W + 0.5f), 1, MAXREP);
      int ryN = clampi((int)(b->h /TILE_W + 0.5f), 1, MAXREP);
      wall_tex(gx[a],gy[a], gx[bb],gy[bb], rx[a],ry[a], rx[bb],ry[bb],
               MATSLOT[b->mat], rxN, ryN, key_[k]);
    } else {
      // flat fill: brighter for the most front-facing wall, darker for the side
      int col = (key_[k] > 0.6f) ? MAT[b->mat].lit : MAT[b->mat].shade;
      quadfill((int)gx[a],(int)gy[a], (int)gx[bb],(int)gy[bb],
               (int)rx[bb],(int)ry[bb], (int)rx[a],(int)ry[a], col);
    }
  }
  if (!persp)  // roof cap (oblique: on top)
    quadfill((int)rx[0],(int)ry[0],(int)rx[1],(int)ry[1],(int)rx[2],(int)ry[2],(int)rx[3],(int)ry[3], MAT[b->mat].roof);
}

// mode 3: screen-aligned billboard card always facing the camera
static void draw_bldg_billboard(Bldg *b) {
  float cxw = (b->x0+b->x1)*0.5f, cyw=(b->y0+b->y1)*0.5f;
  float sx, sy; project(cxw, cyw, 0, &sx, &sy);
  float w = (b->x1-b->x0)*S.zoom*0.5f;
  float ht = b->h*S.zoom*HSCALE;
  // body (front card) + a thin roof strip on top for read
  rectfill((int)(sx-w),(int)(sy-ht),(int)(w*2),(int)ht, MAT[b->mat].lit);
  rectfill((int)(sx-w),(int)(sy-ht),(int)(w*2),2, MAT[b->mat].roof);
}

// ── ground: asphalt everywhere, then a grass/pavement lot quad per cell ──────
static void draw_ground(int cgx, int cgy) {
  for (int gy=cgy-RANGE; gy<=cgy+RANGE; gy++)
  for (int gx=cgx-RANGE; gx<=cgx+RANGE; gx++) {
    float cx = gx*(float)PITCH, cy = gy*(float)PITCH;
    float h2 = PITCH*0.5f - ROADH;
    float lx0=cx-h2, ly0=cy-h2, lx1=cx+h2, ly1=cy+h2;
    bool park = hf(gx,gy,9) < 0.12f;
    float wxq[4]={lx0,lx1,lx1,lx0}, wyq[4]={ly0,ly0,ly1,ly1};
    pproj_poly(wxq,wyq,4,0, park?CLR_DARK_GREEN:CLR_BROWNISH_BLACK);
    if (park && hf(gx,gy,7) < 0.5f) {           // a round pond — the disc→ellipse helper
      pdisc(cx,cy, h2*0.66f, 0, CLR_BLUE_GREEN);
      pdisc(cx,cy, h2*0.52f, 0, CLR_TRUE_BLUE);
    }
  }
}

void draw(void) {
  cls(CLR_DARK_GREY);                            // asphalt = road grid shows through
  int cgx = (int)floorf(S.camx/PITCH + 0.5f);
  int cgy = (int)floorf(S.camy/PITCH + 0.5f);

  draw_ground(cgx, cgy);

  // flyover ground shadows (dithered) — ground the structure under the deck
  if (S.ndeck) {
    fillp(QUARTER,-1);
    for (int d=0; d<S.ndeck; d++) for (int i=0; i+1<S.deck[d].cnt; i++) {
      Deck*dk=&S.deck[d];
      int j=(i+1<dk->cnt)?i+1:i, k=(i>0)?i-1:i;
      float dx=dk->n[j].x-dk->n[k].x, dy=dk->n[j].y-dk->n[k].y, L=sqrtf(dx*dx+dy*dy)+1e-4f;
      float pxn=-dy/L, pyn=dx/L, hw=dk->w*0.5f;
      float wxq[4]={dk->n[i].x+pxn*hw, dk->n[i].x-pxn*hw, dk->n[i+1].x-pxn*hw, dk->n[i+1].x+pxn*hw};
      float wyq[4]={dk->n[i].y+pyn*hw, dk->n[i].y-pyn*hw, dk->n[i+1].y-pyn*hw, dk->n[i+1].y+pyn*hw};
      pproj_poly(wxq,wyq,4,0,CLR_BLACK);
    }
    fillp_reset();
  }

  // collect visible buildings, sort back-to-front by ground-centre screen-y
  static Bldg list[(2*RANGE+1)*(2*RANGE+1)];
  static float depth[(2*RANGE+1)*(2*RANGE+1)];
  int nb=0;
  for (int gy=cgy-RANGE; gy<=cgy+RANGE; gy++)
  for (int gx=cgx-RANGE; gx<=cgx+RANGE; gx++) {
    Bldg b = cell_bldg(gx,gy);
    if (!b.exists) continue;
    float sx,sy; project((b.x0+b.x1)*0.5f,(b.y0+b.y1)*0.5f,0,&sx,&sy);
    if (sx<-60||sx>SCREEN_W+60||sy<-80||sy>SCREEN_H+80) continue;
    list[nb]=b; depth[nb]=sy; nb++;
  }
  for (int i=0;i<nb;i++) for (int j=i+1;j<nb;j++)
    if (depth[j]<depth[i]) { float t=depth[i];depth[i]=depth[j];depth[j]=t; Bldg tb=list[i];list[i]=list[j];list[j]=tb; }

  for (int i=0;i<nb;i++) {
    if (S.mode==M_BILLBOARD) draw_bldg_billboard(&list[i]);
    else                     draw_bldg_geo(&list[i], S.mode==M_LEANOUT);
  }

  // flyover deck segments across all decks — depth-sort, draw far→near (pillar then deck)
  if (S.ndeck) {
    static int sd[2*PN], si[2*PN]; static float sdep[2*PN]; int ns=0;
    for (int d=0; d<S.ndeck; d++) for (int i=0; i+1<S.deck[d].cnt; i++) {
      int mx,my; wpt((S.deck[d].n[i].x+S.deck[d].n[i+1].x)*0.5f,
                     (S.deck[d].n[i].y+S.deck[d].n[i+1].y)*0.5f, 0, &mx,&my);
      sd[ns]=d; si[ns]=i; sdep[ns]=my; ns++;
    }
    for (int i=0;i<ns;i++) for (int j=i+1;j<ns;j++) if (sdep[j]<sdep[i]) {
      float t=sdep[i];sdep[i]=sdep[j];sdep[j]=t; int a=sd[i];sd[i]=sd[j];sd[j]=a; int b=si[i];si[i]=si[j];si[j]=b; }
    for (int q=0;q<ns;q++) { int d=sd[q], i=si[q]; Deck*dk=&S.deck[d];
      if (i%PEVERY==0 && dk->n[i].z>3) draw_box(dk->n[i].x,dk->n[i].y,PHALF,PHALF,0,dk->n[i].z-DECKT,
                                                CLR_MEDIUM_GREY,CLR_DARK_GREY,CLR_LIGHT_GREY);
      draw_deck_seg(dk,i);
    }
  }

  // car — ground shadow (ellipse), then the body at its height
  fillp(QUARTER,-1); pdisc(S.px,S.py,4.0f,0,CLR_BLACK); fillp_reset();
  int cxp,cyp; wpt(S.px,S.py,S.carz,&cxp,&cyp);
  float sa = S.ang + S.rot;
  rectfill_rot(cxp, cyp, 10, 6, sa, CLR_WHITE);
  rectfill_rot(cxp + (int)(cos_deg(sa)*3), cyp + (int)(sin_deg(sa)*3), 4, 5, sa, CLR_RED);

  // HUD
  rectfill(0,0,SCREEN_W,9, CLR_BLACK);
  print(MODE_NAME[S.mode], 3, 2, CLR_YELLOW);
  char hud[40]; snprintf(hud,sizeof hud,"F:fly=%s  T:tex%s", FNAME[S.fly], S.tex?"*":"");
  print(hud, SCREEN_W-150, 2, CLR_LIGHT_GREY);
  if (S.carz>2) { char b[24]; snprintf(b,sizeof b,"ELEVATED  z=%d",(int)S.carz);
    print(b, SCREEN_W/2-40, SCREEN_H-12, CLR_LIME_GREEN); }
  if (S.mode==M_PERSP) { char b[40]; snprintf(b,sizeof b,",.pitch=%d  ;'eye=%d", (int)S.pitch,(int)S.eye);
    print(b, 3, SCREEN_H-9, CLR_LIGHT_GREY); }
}
