#include "studio.h"
#include <math.h>
#include <stdio.h>

// OVERPASS — raised highways in the cityview pseudo-3D look. Same projection as
// cityview (height extrudes STRAIGHT UP the screen), but now a road can have a
// Z that ramps up: an overpass is just a strip of (x,y,z) nodes held up by
// pillars, with the city passing underneath. Drive UP the ramp and the camera
// rises with you — the city sinks below, GTA-style.
//
// Press V to cycle FOUR experimental layouts:
//   1 STRAIGHT   a plain overpass: ramp up, flat span, ramp down.
//   2 S-CURVE    the deck curves in plan while it climbs — banks through 3D.
//   3 SPIRAL     a 3/4-loop on-ramp that spirals up to a high straight.
//   4 STACK      two decks crossing at different heights (a mini interchange) —
//                stress-tests depth sorting between elevated structures.
//
// Each deck reads as a real structure: a grey running surface with a dashed
// centre line, dark side-beams (fascia) giving it thickness, concrete pillars
// to the ground, and a dithered shadow on the ground under it. The car's Z
// snaps to the nearest deck within its width, so you board at a ramp end
// (z≈0) and climb. A second shadow under the car shows the gap to the ground.
//
// CONTROLS — arrows drive · V layout · [ ] zoom · R reseed city

#define PITCH    46          // city block spacing
#define RANGE    6           // city cells each side of camera
#define PN       64          // nodes per deck path
#define DECKW    22.0f       // deck width (world)
#define DECKT    4.0f        // deck thickness (fascia depth)
#define BARRIER  3.0f        // side-barrier height
#define PHALF    3.5f        // pillar half-width
#define PEVERY   6           // a pillar every N nodes
#define QUARTER  0x7BDE      // ~25% black dither

enum { V_STRAIGHT, V_CURVE, V_SPIRAL, V_STACK, V_COUNT };
static const char *VNAME[V_COUNT] = {
  "1 STRAIGHT overpass", "2 S-CURVE flyover", "3 SPIRAL on-ramp", "4 STACK interchange"
};

typedef struct { float x, y, z; } Node;
typedef struct { Node n[PN]; int cnt; float w; } Deck;

typedef struct {
  float px, py, ang, spd, carz;       // car (carz = current deck height)
  float camx, camy, camz, rot, zoom;  // camera (camz rises with the car)
  int   variant, seed;
  bool  started;
  Deck  deck[2];
  int   ndeck;
} State;
static State S;

static float clampf(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }
static float smooth01(float u){ u=clampf(u,0,1); return u*u*(3-2*u); }

// ramp profile: 0 at both ends, 1 across the middle span (smoothed)
static float ramp(float t){
  float rf=0.30f;
  float u = (t<rf) ? t/rf : (t>1-rf) ? (1-t)/rf : 1.0f;
  return smooth01(u);
}

static unsigned hashu(int x,int y,int salt){
  unsigned h=(unsigned)(x*374761393 + y*668265263 + salt*2246822519u + S.seed*3266489917u);
  h=(h^(h>>13))*1274126177u; return h^(h>>16);
}
static float hf(int x,int y,int s){ return (hashu(x,y,s)&0xffffff)/(float)0xffffff; }

// ── build the deck layout for the current variant ───────────────────────────
static void build_world(void){
  S.ndeck=0;
  Deck *d=&S.deck[0]; d->cnt=PN; d->w=DECKW;
  if (S.variant==V_STRAIGHT){
    for(int i=0;i<PN;i++){ float t=(float)i/(PN-1);
      d->n[i]=(Node){ -210+420*t, 0, ramp(t)*30.0f }; }
    S.ndeck=1;
  } else if (S.variant==V_CURVE){
    for(int i=0;i<PN;i++){ float t=(float)i/(PN-1);
      d->n[i]=(Node){ -210+420*t, sin_deg(t*360)*60.0f, ramp(t)*32.0f }; }
    S.ndeck=1;
  } else if (S.variant==V_SPIRAL){
    int sp=44; float R=58, H=40;
    for(int i=0;i<sp;i++){ float t=(float)i/(sp-1); float a=-90+270*t;
      d->n[i]=(Node){ cos_deg(a)*R, sin_deg(a)*R+R, smooth01(t)*H }; }
    // straight run continuing the spiral's exit tangent, held at H
    float dx=d->n[sp-1].x-d->n[sp-2].x, dy=d->n[sp-1].y-d->n[sp-2].y;
    float L=sqrtf(dx*dx+dy*dy)+1e-4f; dx/=L; dy/=L;
    for(int i=sp;i<PN;i++){ int k=i-sp+1;
      d->n[i]=(Node){ d->n[sp-1].x+dx*8*k, d->n[sp-1].y+dy*8*k, H }; }
    d->cnt=PN; S.ndeck=1;
  } else { // V_STACK — two crossing decks at different heights
    for(int i=0;i<PN;i++){ float t=(float)i/(PN-1);
      d->n[i]=(Node){ -200+400*t, 0, ramp(t)*22.0f }; }
    d->cnt=PN; d->w=DECKW;
    Deck *e=&S.deck[1]; e->cnt=PN; e->w=DECKW;
    for(int i=0;i<PN;i++){ float t=(float)i/(PN-1);
      e->n[i]=(Node){ 0, -200+400*t, ramp(t)*42.0f }; }
    S.ndeck=2;
  }
}

static void reset(void){
  build_world();
  Node a=S.deck[0].n[0], b=S.deck[0].n[1];
  S.px=a.x; S.py=a.y; S.carz=0;
  S.ang = atan2f(b.y-a.y, b.x-a.x)*57.2957795f;
  S.spd=0; S.camx=S.px; S.camy=S.py; S.camz=0; S.rot=-90-S.ang;
  if(!S.started){ S.zoom=1.9f; S.started=true; }
}

void init(void){ S.seed=1; S.variant=V_SPIRAL; reset(); S.zoom=1.5f; }  // open on the showpiece

// project a world point (x,y,z) to screen — height & camera-height go up-screen
static void project(float wx,float wy,float wz,float*sx,float*sy){
  float dx=wx-S.camx, dy=wy-S.camy;
  float c=cos_deg(S.rot), s=sin_deg(S.rot);
  float rx=dx*c-dy*s, ry=dx*s+dy*c;
  *sx=SCREEN_W*0.5f+rx*S.zoom;
  *sy=SCREEN_H*0.5f+ry*S.zoom-(wz-S.camz)*S.zoom;
}
static void P(float wx,float wy,float wz,int*x,int*y){ float a,b; project(wx,wy,wz,&a,&b); *x=(int)a; *y=(int)b; }

// deck height at (wx,wy) that is REACHABLE from the car's current height curz —
// so you board/climb a deck via its ramp, and driving UNDER a high deck (a big
// z gap) doesn't snap you up onto it. REACH must exceed the ramp's per-frame
// z-step but stay below the gap between stacked decks.
#define REACH 12.0f
static bool deck_height(float wx,float wy,float curz,float*z){
  float best=1e9f; bool on=false;
  for(int d=0;d<S.ndeck;d++) for(int i=0;i<S.deck[d].cnt;i++){
    Node *p=&S.deck[d].n[i];
    float dx=p->x-wx, dy=p->y-wy, dd=dx*dx+dy*dy;
    float hw=S.deck[d].w*0.5f;
    if(dd<hw*hw && fabsf(p->z-curz)<REACH && dd<best){ best=dd; *z=p->z; on=true; }
  }
  return on;
}

void update(void){
  if(keyp('V')){ S.variant=(S.variant+1)%V_COUNT; reset(); }
  if(keyp('R')){ S.seed++; build_world(); }
  if(key('[')) S.zoom=fmaxf(0.9f,S.zoom*0.97f);
  if(key(']')) S.zoom=fminf(6.0f,S.zoom*1.03f);

  float thr=(key(KEY_UP)?1:0)-(key(KEY_DOWN)?1:0);
  float steer=(key(KEY_RIGHT)?1:0)-(key(KEY_LEFT)?1:0);
  S.spd+=thr*0.06f; S.spd*=0.97f; if(fabsf(S.spd)<0.005f) S.spd=0;
  S.ang+=steer*(1.6f+fminf(fabsf(S.spd)*2.0f,2.4f))*(S.spd>=0?1:-1);
  S.px+=cos_deg(S.ang)*S.spd; S.py+=sin_deg(S.ang)*S.spd;

  float tz=0; deck_height(S.px,S.py,S.carz,&tz);  // snap to a reachable deck, else fall to ground
  S.carz+=(tz-S.carz)*0.25f;

  S.camx+=(S.px-S.camx)*0.18f; S.camy+=(S.py-S.camy)*0.18f;
  S.camz+=(S.carz-S.camz)*0.12f;
  float target=-90.0f-S.ang;
  float dd=fmodf(target-S.rot+540.0f,360.0f)-180.0f; S.rot+=dd*0.16f;
}

// ── a vertical box (pillars + buildings): footprint cx,cy ± half, z0..z1 ─────
static void draw_box(float cx,float cy,float hx,float hy,float z0,float z1,int lit,int sh,int roof){
  float wx[4]={cx-hx,cx+hx,cx+hx,cx-hx}, wy[4]={cy-hy,cy-hy,cy+hy,cy+hy};
  int bx[4],by[4],tx[4],ty[4];
  for(int i=0;i<4;i++){ P(wx[i],wy[i],z0,&bx[i],&by[i]); P(wx[i],wy[i],z1,&tx[i],&ty[i]); }
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

// edge points of deck node i (left/right of travel) at a height offset
static void deck_edge(Deck*d,int i,float zoff,int*lx,int*ly,int*rx,int*ry){
  int j=(i+1<d->cnt)?i+1:i, k=(i>0)?i-1:i;
  float dx=d->n[j].x-d->n[k].x, dy=d->n[j].y-d->n[k].y;
  float L=sqrtf(dx*dx+dy*dy)+1e-4f; float px=-dy/L, py=dx/L; float hw=d->w*0.5f;
  Node*nd=&d->n[i];
  P(nd->x+px*hw, nd->y+py*hw, nd->z+zoff, lx, ly);
  P(nd->x-px*hw, nd->y-py*hw, nd->z+zoff, rx, ry);
}

static void draw_deck_seg(Deck*d,int i){
  int la,lay,ra,ray, lb,lby,rb,rby;            // top edges at deck height
  deck_edge(d,i,0,&la,&lay,&ra,&ray); deck_edge(d,i+1,0,&lb,&lby,&rb,&rby);
  int la2,lay2,ra2,ray2, lb2,lby2,rb2,rby2;    // bottom edges (fascia underside)
  deck_edge(d,i,-DECKT,&la2,&lay2,&ra2,&ray2); deck_edge(d,i+1,-DECKT,&lb2,&lby2,&rb2,&rby2);
  // side beams (fascia) — gives the deck thickness
  quadfill(la,lay,lb,lby,lb2,lby2,la2,lay2, CLR_BROWNISH_BLACK);
  quadfill(ra,ray,rb,rby,rb2,rby2,ra2,ray2, CLR_BROWNISH_BLACK);
  // running surface
  quadfill(la,lay,ra,ray,rb,rby,lb,lby, CLR_DARK_GREY);
  // dashed centre line
  if(i%2==0){ int ax,ay,bx,by; P(d->n[i].x,d->n[i].y,d->n[i].z+0.3f,&ax,&ay);
    P(d->n[i+1].x,d->n[i+1].y,d->n[i+1].z+0.3f,&bx,&by); line(ax,ay,bx,by,CLR_YELLOW); }
  // barriers (low walls along both edges)
  int lat,layt,rat,rayt, lbt,lbyt,rbt,rbyt;
  deck_edge(d,i,BARRIER,&lat,&layt,&rat,&rayt); deck_edge(d,i+1,BARRIER,&lbt,&lbyt,&rbt,&rbyt);
  quadfill(la,lay,lb,lby,lbt,lbyt,lat,layt, CLR_LIGHT_GREY);
  quadfill(ra,ray,rb,rby,rbt,rbyt,rat,rayt, CLR_LIGHT_GREY);
}

// ── city ground grid + buildings ────────────────────────────────────────────
static void draw_ground_grid(int cgx,int cgy){
  for(int gy=cgy-RANGE;gy<=cgy+RANGE;gy++){ int ax,ay,bx,by;
    P((cgx-RANGE)*PITCH, gy*PITCH,0,&ax,&ay); P((cgx+RANGE)*PITCH, gy*PITCH,0,&bx,&by);
    line(ax,ay,bx,by,CLR_DARK_GREEN); }
  for(int gx=cgx-RANGE;gx<=cgx+RANGE;gx++){ int ax,ay,bx,by;
    P(gx*PITCH,(cgy-RANGE)*PITCH,0,&ax,&ay); P(gx*PITCH,(cgy+RANGE)*PITCH,0,&bx,&by);
    line(ax,ay,bx,by,CLR_DARK_GREEN); }
}

typedef struct { float x,y,h; int mat; } Bldg;
static const int LIT[4]={CLR_MEDIUM_GREY,CLR_ORANGE,CLR_TRUE_BLUE,CLR_DARK_RED};
static const int SH [4]={CLR_DARK_GREY,CLR_BROWN,CLR_DARKER_BLUE,CLR_DARK_PURPLE};
static const int RF [4]={CLR_LIGHT_GREY,CLR_LIGHT_PEACH,CLR_BLUE,CLR_PINK};

void draw(void){
  cls(CLR_BLUE_GREEN);
  int cgx=(int)floorf(S.camx/PITCH+0.5f), cgy=(int)floorf(S.camy/PITCH+0.5f);
  draw_ground_grid(cgx,cgy);

  // deck ground shadows (dithered) — grounds the structure
  fillp(QUARTER,-1);
  for(int d=0;d<S.ndeck;d++) for(int i=0;i+1<S.deck[d].cnt;i++){
    Deck*dk=&S.deck[d];
    int j=(i+1<dk->cnt)?i+1:i,k=(i>0)?i-1:i;
    float dx=dk->n[j].x-dk->n[k].x,dy=dk->n[j].y-dk->n[k].y,L=sqrtf(dx*dx+dy*dy)+1e-4f;
    float px=-dy/L,py=dx/L,hw=dk->w*0.5f;
    int la,lay,ra,ray,lb,lby,rb,rby;
    P(dk->n[i].x+px*hw,dk->n[i].y+py*hw,0,&la,&lay);   P(dk->n[i].x-px*hw,dk->n[i].y-py*hw,0,&ra,&ray);
    P(dk->n[i+1].x+px*hw,dk->n[i+1].y+py*hw,0,&lb,&lby); P(dk->n[i+1].x-px*hw,dk->n[i+1].y-py*hw,0,&rb,&rby);
    quadfill(la,lay,ra,ray,rb,rby,lb,lby,CLR_BLACK);
  }
  fillp_reset();

  // buildings — collect, depth-sort, draw
  static Bldg bl[(2*RANGE+1)*(2*RANGE+1)]; static float dep[(2*RANGE+1)*(2*RANGE+1)]; int nb=0;
  for(int gy=cgy-RANGE;gy<=cgy+RANGE;gy++) for(int gx=cgx-RANGE;gx<=cgx+RANGE;gx++){
    if(hf(gx,gy,9)<0.45f) continue;             // sparse city, leave room for the road
    float cx=gx*(float)PITCH, cy=gy*(float)PITCH;
    Bldg b; b.x=cx+(hf(gx,gy,3)-0.5f)*10; b.y=cy+(hf(gx,gy,4)-0.5f)*10;
    b.h=5+hf(gx,gy,5)*hf(gx,gy,5)*14; b.mat=hashu(gx,gy,6)&3;
    int sx,sy; P(b.x,b.y,0,&sx,&sy);
    if(sx<-50||sx>SCREEN_W+50||sy<-60||sy>SCREEN_H+80) continue;
    bl[nb]=b; dep[nb]=sy; nb++;
  }
  for(int i=0;i<nb;i++) for(int j=i+1;j<nb;j++) if(dep[j]<dep[i]){
    float t=dep[i];dep[i]=dep[j];dep[j]=t; Bldg tb=bl[i];bl[i]=bl[j];bl[j]=tb; }
  for(int i=0;i<nb;i++) draw_box(bl[i].x,bl[i].y,11,11,0,bl[i].h, LIT[bl[i].mat],SH[bl[i].mat],RF[bl[i].mat]);

  // deck segments across all decks — depth-sort, draw far→near (pillar then deck)
  static int sd[2*PN], si[2*PN]; static float sdep[2*PN]; int ns=0;
  for(int d=0;d<S.ndeck;d++) for(int i=0;i+1<S.deck[d].cnt;i++){
    int mx,my; P((S.deck[d].n[i].x+S.deck[d].n[i+1].x)*0.5f,
                 (S.deck[d].n[i].y+S.deck[d].n[i+1].y)*0.5f, 0, &mx,&my);
    sd[ns]=d; si[ns]=i; sdep[ns]=my; ns++;
  }
  for(int i=0;i<ns;i++) for(int j=i+1;j<ns;j++) if(sdep[j]<sdep[i]){
    float t=sdep[i];sdep[i]=sdep[j];sdep[j]=t; int a=sd[i];sd[i]=sd[j];sd[j]=a; int b=si[i];si[i]=si[j];si[j]=b; }
  for(int q=0;q<ns;q++){ int d=sd[q], i=si[q]; Deck*dk=&S.deck[d];
    if(i%PEVERY==0 && dk->n[i].z>3) draw_box(dk->n[i].x,dk->n[i].y,PHALF,PHALF,0,dk->n[i].z-DECKT,
                                             CLR_MEDIUM_GREY,CLR_DARK_GREY,CLR_LIGHT_GREY);
    draw_deck_seg(dk,i);
  }

  // car — shadow on the ground, then the body at its height
  int shx,shy; P(S.px,S.py,0,&shx,&shy);
  fillp(QUARTER,-1); circfill(shx,shy,4,CLR_BLACK); fillp_reset();
  int cxp,cyp; P(S.px,S.py,S.carz,&cxp,&cyp);
  float sa=S.ang+S.rot;
  rectfill_rot(cxp,cyp,11,6,sa,CLR_WHITE);
  rectfill_rot(cxp+(int)(cos_deg(sa)*3),cyp+(int)(sin_deg(sa)*3),4,5,sa,CLR_RED);

  // HUD
  rectfill(0,0,SCREEN_W,9,CLR_BLACK);
  print(VNAME[S.variant],3,2,CLR_YELLOW);
  print("V:layout [ ]:zoom R:city",SCREEN_W-150,2,CLR_LIGHT_GREY);
  if(S.carz>2){ char b[32]; snprintf(b,sizeof b,"ELEVATED  z=%d",(int)S.carz);
    print(b,SCREEN_W/2-40,SCREEN_H-12,CLR_LIME_GREEN); }
}
