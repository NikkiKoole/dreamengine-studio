/* de:meta
{
  "title": "solvalou (xevious)",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "title-play-gameover-loop",
    "noise-terrain",
    "particle-system"
  ],
  "lineage": "Direct homage to Namco's Xevious (1982); distinctive for the dual-layer weapon system (zapper vs. blaster bomb on a reticle) and noise-driven scrolling terrain generation.",
  "genre": "shooter",
  "homage": "Xevious (1983)",
  "description": "A Xevious — the vertical shooter with TWO weapons for two layers: Z zaps flying enemies (toroids, zoshis), X drops a blaster bomb on the reticle ahead of you to hit GROUND targets (tanks, bases). Terrain scrolls past, ground bases shoot back, and the Andor Genesis mothership shows up for big points. WASD/arrows fly, Z zap, X bomb."
}
de:meta */
#include "studio.h"

// SOLVALOU — a Xevious. The trick is TWO weapons for two layers:
//   Z  ZAPPER  — fires forward, hits FLYING enemies
//   X  BLASTER — drops a bomb on the reticle ahead of you, hits GROUND targets
// Terrain scrolls past; toroids and zoshis fly, tanks and bases sit on the
// ground and shoot back. Blow up the Andor Genesis mothership for big points.
//
// WASD / arrows fly    Z zap    X bomb

#define HUD 14
#define SCRL 34.0f                 // terrain scroll px/sec
#define RET  46                    // reticle distance ahead of the ship

enum { TOROID, ZOSHI };            // air
enum { TANK, BASE, ANDOR };        // ground

typedef struct { float x, y, vx, vy, t; int type, hp, alive, shootT; } Air;
typedef struct { float x, y, t; int type, hp, alive; float shootT; } Gnd;
typedef struct { float x, y, vx, vy; int alive; } Shot;

#define NA 24
#define NG 16
#define NB 24
#define NE 48
static Air  ar[NA];
static Gnd  gr[NG];
static Shot pb[NB];                // player zapper
static Shot eb[NE];                // enemy bullets
static struct { int active, state; float x, y, tx, ty, t; } bomb;

static float px, py, scrollf, zcool, spawnA, spawnG;
static int   score, lives, state, invuln, nextAndor;
// expanding blasts (visual + ground damage)
static struct { float x, y, t; int alive; } blast[8];


static void add_pb(float x,float y){ for(int i=0;i<NB;i++) if(!pb[i].alive){ pb[i]=(Shot){x,y,0,-260,1}; return; } }
static void add_eb(float x,float y,float tx,float ty){ float a=angle_to((int)x,(int)y,(int)tx,(int)ty);
    for(int i=0;i<NE;i++) if(!eb[i].alive){ eb[i]=(Shot){x,y,cos_deg(a)*110,sin_deg(a)*110,1}; return; } }
static void add_blast(float x,float y){ for(int i=0;i<8;i++) if(!blast[i].alive){ blast[i].x=x; blast[i].y=y; blast[i].t=0; blast[i].alive=1; return; } }

static void spawn_air(void){
    for(int i=0;i<NA;i++) if(!ar[i].alive){
        int z = rnd(100)<40;
        ar[i]=(Air){ (float)rnd_between(16,SCREEN_W-16), -10, rnd_float_between(-20,20), rnd_float_between(34,60),
                     0, z?ZOSHI:TOROID, z?2:1, 1, rnd_between(40,110) };
        return;
    }
}
static void spawn_gnd(int type){
    for(int i=0;i<NG;i++) if(!gr[i].alive){
        gr[i]=(Gnd){ (float)rnd_between(20,SCREEN_W-20), -16, 0, type, type==ANDOR?6:type==TANK?2:2, 1, rnd_float_between(1.0f,2.4f) };
        return;
    }
}

void init(void){
    px=SCREEN_W/2; py=SCREEN_H-30; scrollf=0; score=0; lives=3; state=0; invuln=0;
    zcool=0; spawnA=0.6f; spawnG=1.2f; nextAndor=2500;
    for(int i=0;i<NA;i++) ar[i].alive=0;
    for(int i=0;i<NG;i++) gr[i].alive=0;
    for(int i=0;i<NB;i++) pb[i].alive=0;
    for(int i=0;i<NE;i++) eb[i].alive=0;
    for(int i=0;i<8;i++) blast[i].alive=0;
    bomb.active=0;
    for(int k=0;k<4;k++) spawn_air();
    spawn_gnd(TANK); spawn_gnd(BASE);
    // place the starters on-screen so the action is visible immediately
    for(int i=0;i<4;i++) if(ar[i].alive){ ar[i].x=rnd_between(30,SCREEN_W-30); ar[i].y=rnd_between(HUD+20,90); }
    for(int i=0;i<NG;i++) if(gr[i].alive){ gr[i].x=rnd_between(30,SCREEN_W-30); gr[i].y=rnd_between(60,130); }
}

static void hurt(void){
    if(invuln>0) return;
    lives--; invuln=110; hit(38,INSTR_NOISE,5,200);
    if(lives<0){ lives=0; state=2; }
    px=SCREEN_W/2; py=SCREEN_H-30;
}

void update(void){
    float d=dt();
    if(state!=0){ if(btnp(0,BTN_A)||btnp(1,BTN_A)) init(); return; }

    scrollf += SCRL*d;
    if(invuln>0) invuln--;
    if(zcool>0) zcool-=d;

    // ---- move ----
    float spd=120;
    int ix=(btn(0,BTN_RIGHT)||btn(1,BTN_RIGHT))-(btn(0,BTN_LEFT)||btn(1,BTN_LEFT));
    int iy=(btn(0,BTN_DOWN)||btn(1,BTN_DOWN))-(btn(0,BTN_UP)||btn(1,BTN_UP));
    px=clamp(px+ix*spd*d, 8, SCREEN_W-8);
    py=clamp(py+iy*spd*d, HUD+12, SCREEN_H-12);

    float retx=px, rety=py-RET; if(rety<HUD+6) rety=HUD+6;

    // ---- fire ----
    if((btn(0,BTN_A)||btn(1,BTN_A)) && zcool<=0){ add_pb(px,py-8); zcool=0.16f; note(74,INSTR_SQUARE,2); }
    if((btnp(0,BTN_B)||btnp(1,BTN_B)) && !bomb.active){ bomb.active=1; bomb.state=0; bomb.x=px; bomb.y=py;
                                                        bomb.tx=retx; bomb.ty=rety; bomb.t=0; note(50,INSTR_SQUARE,2); }

    // ---- bomb ----
    if(bomb.active){
        if(bomb.state==0){
            bomb.t+=d/0.24f;
            bomb.x=lerp(px,bomb.tx,bomb.t); // follows ship x a touch then to target
            bomb.y=lerp(py,bomb.ty,bomb.t);
            if(bomb.t>=1){ bomb.state=1; add_blast(bomb.tx,bomb.ty); hit(44,INSTR_NOISE,4,120);
                // damage grounds in radius
                for(int i=0;i<NG;i++) if(gr[i].alive && distance((int)bomb.tx,(int)bomb.ty,(int)gr[i].x,(int)gr[i].y)<18){
                    gr[i].hp--; if(gr[i].hp<=0){ gr[i].alive=0; score+=gr[i].type==ANDOR?500:gr[i].type==TANK?80:60; add_blast(gr[i].x,gr[i].y);} }
                bomb.active=0;
            }
        }
    }

    // ---- player bullets (air only) ----
    for(int i=0;i<NB;i++){ if(!pb[i].alive)continue; pb[i].y+=pb[i].vy*d; if(pb[i].y<HUD){pb[i].alive=0;continue;}
        for(int j=0;j<NA;j++) if(ar[j].alive && distance((int)pb[i].x,(int)pb[i].y,(int)ar[j].x,(int)ar[j].y)<8){
            ar[j].hp--; pb[i].alive=0; if(ar[j].hp<=0){ ar[j].alive=0; score+=ar[j].type==ZOSHI?50:30; add_blast(ar[j].x,ar[j].y);} break; }
    }

    // ---- air enemies ----
    for(int i=0;i<NA;i++){ Air*e=&ar[i]; if(!e->alive)continue;
        e->t+=d;
        e->x += (e->vx + sin_deg(e->t*180)*30)*d;
        e->y += e->vy*d;
        if(e->y>SCREEN_H+12){ e->alive=0; continue; }
        if(e->type==ZOSHI){ e->shootT--; if(e->shootT<=0 && e->y>HUD+8 && e->y<py){ e->shootT=rnd_between(70,140); add_eb(e->x,e->y,px,py);} }
        if(invuln<=0 && distance((int)e->x,(int)e->y,(int)px,(int)py)<10){ e->alive=0; add_blast(e->x,e->y); hurt(); }
    }

    // ---- ground targets (scroll down with terrain) ----
    for(int i=0;i<NG;i++){ Gnd*g=&gr[i]; if(!g->alive)continue;
        g->y += SCRL*d; if(g->y>SCREEN_H+16){ g->alive=0; continue; }
        g->shootT-=d; if(g->shootT<=0 && g->y>HUD+10){ g->shootT=g->type==ANDOR?0.6f:rnd_float_between(1.4f,3.0f); add_eb(g->x,g->y,px,py); }
    }

    // ---- enemy bullets ----
    for(int i=0;i<NE;i++){ if(!eb[i].alive)continue; eb[i].x+=eb[i].vx*d; eb[i].y+=eb[i].vy*d;
        if(eb[i].x<0||eb[i].x>SCREEN_W||eb[i].y<0||eb[i].y>SCREEN_H){ eb[i].alive=0; continue; }
        if(invuln<=0 && distance((int)eb[i].x,(int)eb[i].y,(int)px,(int)py)<8){ eb[i].alive=0; hurt(); }
    }

    // ---- blasts ----
    for(int i=0;i<8;i++) if(blast[i].alive){ blast[i].t+=d*3; if(blast[i].t>=1) blast[i].alive=0; }

    // ---- spawning ----
    spawnA-=d; if(spawnA<=0){ spawnA=rnd_float_between(0.5f,1.3f); spawn_air(); }
    spawnG-=d; if(spawnG<=0){ spawnG=rnd_float_between(1.4f,3.0f); spawn_gnd(rnd(2)); }
    if(score>=nextAndor){ nextAndor+=3000; spawn_gnd(ANDOR); }
}

static void terrain(void){
    int TS=16, off=(int)scrollf % TS;
    for(int sy=-TS; sy<SCREEN_H+TS; sy+=TS)
        for(int sx=0; sx<SCREEN_W; sx+=TS){
            float h=noise2(sx*0.06f, (scrollf+sy)*0.06f);
            int c = h<0.40f?CLR_DARK_GREEN : h<0.52f?CLR_MEDIUM_GREEN : h<0.70f?CLR_DARK_GREEN
                  : h<0.80f?CLR_DARK_BROWN : CLR_TRUE_BLUE;
            rectfill(sx, sy+off, TS, TS, c);
        }
}

static void draw_air(Air*e){
    if(e->type==TOROID){ int c=((int)(e->t*8)&1)?CLR_RED:CLR_ORANGE;
        circ((int)e->x,(int)e->y,5,c); circfill((int)e->x,(int)e->y,2,CLR_LIGHT_YELLOW); }
    else { trifill((int)e->x-5,(int)e->y-4,(int)e->x+5,(int)e->y-4,(int)e->x,(int)e->y+5,CLR_BLUE);
           pset((int)e->x,(int)e->y-1,CLR_WHITE); }
}
static void draw_gnd(Gnd*g){
    if(g->type==TANK){ rectfill((int)g->x-5,(int)g->y-5,10,10,CLR_MEDIUM_GREEN); rect((int)g->x-5,(int)g->y-5,10,10,CLR_DARK_GREEN);
                       rectfill((int)g->x-1,(int)g->y-8,2,5,CLR_DARK_GREY); }
    else if(g->type==BASE){ circfill((int)g->x,(int)g->y,6,CLR_LIGHT_GREY); circfill((int)g->x,(int)g->y,3,CLR_RED); }
    else { circfill((int)g->x,(int)g->y,12,CLR_DARK_GREY); circ((int)g->x,(int)g->y,12,CLR_LIGHT_GREY);
           for(int k=0;k<3;k++) circfill((int)g->x-8+k*8,(int)g->y,2,CLR_RED); circfill((int)g->x,(int)g->y,4,CLR_PINK); }
}

void draw(void){
    terrain();

    // ground targets sit on the terrain
    for(int i=0;i<NG;i++) if(gr[i].alive) draw_gnd(&gr[i]);

    // reticle ahead of the ship
    if(state==0){ int rxq=(int)px, ryq=(int)py-RET; if(ryq<HUD+6)ryq=HUD+6;
        rect(rxq-4,ryq-4,8,8,CLR_LIGHT_YELLOW); line(rxq-6,ryq,rxq+6,ryq,CLR_YELLOW); line(rxq,ryq-6,rxq,ryq+6,CLR_YELLOW); }

    // bomb falling
    if(bomb.active && bomb.state==0) circfill((int)bomb.x,(int)bomb.y,2,CLR_WHITE);

    for(int i=0;i<NB;i++) if(pb[i].alive){ rectfill((int)pb[i].x,(int)pb[i].y-3,2,5,CLR_WHITE); }
    for(int i=0;i<NA;i++) if(ar[i].alive) draw_air(&ar[i]);
    for(int i=0;i<NE;i++) if(eb[i].alive) circfill((int)eb[i].x,(int)eb[i].y,2,CLR_PINK);

    // blasts
    for(int i=0;i<8;i++) if(blast[i].alive){ int r=(int)(blast[i].t*16)+2;
        circ((int)blast[i].x,(int)blast[i].y,r,CLR_ORANGE); circ((int)blast[i].x,(int)blast[i].y,r/2,CLR_LIGHT_YELLOW); }

    // ship (Solvalou)
    if(state==0 && !(invuln>0 && ((int)(now()*20)&1))){
        trifill((int)px,(int)py-8,(int)px-7,(int)py+7,(int)px+7,(int)py+7,CLR_LIGHT_GREY);
        trifill((int)px,(int)py-2,(int)px-3,(int)py+5,(int)px+3,(int)py+5,CLR_BLUE);
        rectfill((int)px-8,(int)py+3,3,5,CLR_WHITE); rectfill((int)px+5,(int)py+3,3,5,CLR_WHITE);
    }

    // HUD
    rectfill(0,0,SCREEN_W,HUD,CLR_BLACK);
    print(str("%06d", score), 6, 3, CLR_WHITE);
    for(int i=0;i<lives;i++) trifill(SCREEN_W-10-i*12,3,SCREEN_W-16-i*12,11,SCREEN_W-4-i*12,11,CLR_LIGHT_GREY);
    print("ZAP / BOMB", SCREEN_W/2-40, 3, CLR_DARK_GREY);

    if(state==2){ rectfill(80,84,160,36,CLR_BLACK); rect(80,84,160,36,CLR_RED);
        print_centered("GAME OVER", SCREEN_W/2, 92, CLR_RED); print_centered(str("score %d  -  Z", score), SCREEN_W/2, 104, CLR_LIGHT_GREY); }
}
