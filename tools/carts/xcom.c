/* de:meta
{
  "title": "xcom (enemy unknown)",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "turn-based-loop",
    "save-load-persistence",
    "finite-state-ai",
    "noise-terrain"
  ],
  "lineage": "Demake of XCOM: Enemy Unknown (2012); novel in adding a persistent cross-mission soldier roster (save_bytes), procedurally noise-scattered cover maps, and overwatch reaction fire.",
  "genre": "strategy",
  "homage": "X-COM: UFO Defense (1994)",
  "description": "Turn-based squad tactics against the alien threat. Move 4 soldiers across a procedurally scattered cover map, take percentage-chance shots with a live hit/crit forecast, duck behind half/full cover (shown as shields), and set OVERWATCH to snap-fire at anything that moves. Fog-of-war hides the battlefield until your soldiers see it; alien pods activate and scatter to cover with an alarm when spotted. Soldiers you name, keep and promote between missions in a light barracks screen — and lose for good if they fall (the roster persists). Mouse: click a soldier, click a tile to move (range shown) or an enemy to open the shot, then FIRE; buttons for OVERWATCH / HUNKER / RELOAD / END TURN. Keyboard: arrows move the cursor, Z act, X cancel, O/H/R actions, E end turn, TAB next soldier. Wipe out every alien to win."
}
de:meta */
#include "studio.h"

// XCOM — ENEMY UNKNOWN
// Turn-based squad tactics. Move 4 soldiers across a procedurally scattered
// cover map, take percentage shots, use overwatch, and wipe out the alien pods
// before they wipe you. Soldiers you keep, name and promote between missions;
// a fallen soldier is gone for good (the roster persists via save_bytes).
//
//   MOUSE (primary): click a soldier, click a tile to move (range shown),
//   click an enemy to open the shot forecast, then FIRE. Buttons for
//   OVERWATCH / HUNKER / RELOAD / END TURN.
//   KEYBOARD: arrows move the cursor, Z = act on the tile, X = cancel,
//   O overwatch, H hunker, R reload, E / ENTER end turn, TAB next soldier.

// ── layout ────────────────────────────────────────────────────
#define GW    20          // battlefield in tiles
#define GH    9
#define TILE  16
#define HUDY  (GH * TILE) // HUD starts here (144), 56px tall

// ── tiles (also the map sprite slots) ─────────────────────────
enum { T_EMPTY, FLOOR, WALL, SAND, CRATE };   // FLOOR=1 WALL=2 SAND=3 CRATE=4
#define SPR_SOLDIER 8
#define SPR_SECTOID 9
#define SPR_MUTON   10

// ── rules ─────────────────────────────────────────────────────
#define SQUAD   4
#define AMAX    8
#define SIGHT   8          // soldier vision radius (tiles)
#define SMOVE   5          // tiles per single move (1 AP)
#define SDASH   10         // tiles per dash (2 AP, no shot)
#define SRANGE  16         // soldier weapon reach (penalised by distance)
#define CLIP    4          // rounds per magazine
#define COVER_HALF 20
#define COVER_FULL 40
#define MAGIC_ARMOR 28     // placeholder index in the soldier sprite

// alien stats indexed by type: 0 = sectoid, 1 = muton
static const int  AHP[2]    = { 4, 9 };
static const int  AAIM[2]   = { 58, 64 };
static const int  ADMGLO[2] = { 2, 3 };
static const int  ADMGHI[2] = { 5, 7 };   // exclusive upper for rnd_between
static const int  ARANGE[2] = { 10, 9 };
static const int  AMOVE[2]  = { 5, 4 };
static const int  ASLOT[2]  = { SPR_SECTOID, SPR_MUTON };
static const char *ANAME[2] = { "sectoid", "muton" };

// soldier armour tints (the pal() magic-colour recolour, one per slot)
static const int ARMOR[SQUAD] = { CLR_GREEN, CLR_BLUE, CLR_ORANGE, CLR_PINK };

// unit states
enum { ST_NORMAL, ST_OVERWATCH, ST_HUNKER };
// game screens / battle phases
enum { GS_BARRACKS, GS_BATTLE, GS_DEBRIEF };
enum { PH_PLAYER, PH_ENEMY };

// ── persistent roster ─────────────────────────────────────────
#define SAVE_MAGIC 0x58434F31   // "XCO1"
typedef struct { char name[12]; int kills; } Trooper;
typedef struct { int magic, missions; Trooper t[SQUAD]; } Save;
static Save sav;

static const char *RNAMES[] = {
    "Vasquez","Hicks","Hudson","Frost","Mox","Wei","Kovac","Reyes","Stone",
    "Patel","Cole","Nox","Vega","Ash","Cruz","Lund","Pike","Rourke","Six","Dietl"
};
static const char *RANKN[8] = {
    "Rookie","Squaddie","Corporal","Sergeant","Lieut.","Captain","Major","Colonel"
};
static int rank_for(int k){
    if(k>=30)return 7; if(k>=22)return 6; if(k>=15)return 5; if(k>=10)return 4;
    if(k>=6)return 3;  if(k>=3)return 2;  if(k>=1)return 1;  return 0;
}
static int maxhp_for(int r){ return 6 + r; }
static int aim_for(int r){ return 62 + r * 3; }

// ── battle state ──────────────────────────────────────────────
typedef struct {
    int x, y, hp, maxhp, aim, ap, state, alive, ammo, face, flash;
    int killsThis, killsBefore;
} Unit;
typedef struct {
    int type, x, y, hp, maxhp, active, alive, pod, mp, shots, flash;
} Alien;

static Unit  u[SQUAD];
static Alien al[AMAX];
static int   nal;

static bool vis[GH][GW], seen[GH][GW];
static int  reach[GH][GW];

static int  gstate, phase;
static int  sel = -1, target = -1;       // selected soldier / targeted alien
static int  kcx, kcy, pmx, pmy;          // cursor (mouse-synced) + last mouse pos
static int  ai_cur;
static float ai_timer, fxlock, boomT;
static int  boomx, boomy;
static int  dwin;                        // last debrief result
static int  ren = -1, rlen;              // barracks rename target + buffer len
static char rbuf[12];

// ── tiny fx pools ─────────────────────────────────────────────
#define FX 18
typedef struct { float x, y, t; int col; char txt[10]; } Floater;
typedef struct { float x1, y1, x2, y2, t; int col; } Tracer;
static Floater flt[FX];
static Tracer  trc[FX];
#define FLT_DUR 0.9f
#define TR_DUR  0.26f

static void spawn_float(int x, int y, int col, const char *s){
    int b = 0; for(int i=0;i<FX;i++) if(flt[i].t < flt[b].t) b=i;
    flt[b].x=x; flt[b].y=y; flt[b].t=FLT_DUR; flt[b].col=col;
    int i=0; for(; s[i] && i<9; i++) flt[b].txt[i]=s[i]; flt[b].txt[i]=0;
}
static void spawn_trace(int x1,int y1,int x2,int y2,int col){
    int b = 0; for(int i=0;i<FX;i++) if(trc[i].t < trc[b].t) b=i;
    trc[b].x1=x1; trc[b].y1=y1; trc[b].x2=x2; trc[b].y2=y2; trc[b].t=TR_DUR; trc[b].col=col;
}
static void update_fx(float d){
    for(int i=0;i<FX;i++){ if(flt[i].t>0){flt[i].t-=d; flt[i].y-=12*d;} if(trc[i].t>0)trc[i].t-=d; }
}

// ── grid helpers ──────────────────────────────────────────────
static int unit_at(int x,int y){ for(int i=0;i<SQUAD;i++) if(u[i].alive&&u[i].x==x&&u[i].y==y) return i; return -1; }
static int alien_at(int x,int y){ for(int i=0;i<nal;i++) if(al[i].alive&&al[i].x==x&&al[i].y==y) return i; return -1; }
static int cover_at(int x,int y){
    if(x<0||y<0||x>=GW||y>=GH) return 0;
    int m=mget(x,y); if(m==WALL||m==CRATE) return COVER_FULL; if(m==SAND) return COVER_HALF; return 0;
}
// best cover a unit at (tx,ty) gets against a shooter at (fx,fy)
static int cover_value(int tx,int ty,int fx,int fy){
    int dxs=sgn(fx-tx), dys=sgn(fy-ty), best=0;
    if(dxs) best=max(best, cover_at(tx+dxs, ty));
    if(dys) best=max(best, cover_at(tx, ty+dys));
    return best;
}
static bool los_clear(int x0,int y0,int x1,int y1){
    int dx=abs(x1-x0), dy=abs(y1-y0), sx=x0<x1?1:-1, sy=y0<y1?1:-1, err=dx-dy, x=x0, y=y0;
    while(1){
        if(!(x==x0&&y==y0)&&!(x==x1&&y==y1) && mget(x,y)==WALL) return false;
        if(x==x1&&y==y1) return true;
        int e2=2*err; if(e2>-dy){err-=dy;x+=sx;} if(e2<dx){err+=dx;y+=sy;}
    }
}
static int aliens_alive(void){ int n=0; for(int i=0;i<nal;i++) if(al[i].alive) n++; return n; }
static int soldiers_alive(void){ int n=0; for(int i=0;i<SQUAD;i++) if(u[i].alive) n++; return n; }
static int nearest_soldier(int x,int y){
    int b=-1, bd=9999; for(int i=0;i<SQUAD;i++) if(u[i].alive){ int d=abs(u[i].x-x)+abs(u[i].y-y); if(d<bd){bd=d;b=i;} } return b;
}
static int nearest_alien(int x,int y){
    int b=-1, bd=9999; for(int i=0;i<nal;i++) if(al[i].alive&&al[i].active){ int d=abs(al[i].x-x)+abs(al[i].y-y); if(d<bd){bd=d;b=i;} } return b;
}

// ── fog of war + pod activation ───────────────────────────────
static void scatter(int ai);   // fwd
static void cast(int sx,int sy,int tx,int ty){
    int dx=abs(tx-sx), dy=abs(ty-sy), ssx=sx<tx?1:-1, ssy=sy<ty?1:-1, err=dx-dy, x=sx, y=sy;
    while(1){
        if(x>=0&&y>=0&&x<GW&&y<GH){ vis[y][x]=true; seen[y][x]=true; if(mget(x,y)==WALL) return; }
        if(x==tx&&y==ty) return;
        int e2=2*err; if(e2>-dy){err-=dy;x+=ssx;} if(e2<dx){err+=dx;y+=ssy;}
    }
}
static void recompute_vis(void){
    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++) vis[y][x]=false;
    for(int i=0;i<SQUAD;i++) if(u[i].alive){
        seen[u[i].y][u[i].x]=vis[u[i].y][u[i].x]=true;
        for(int ty=u[i].y-SIGHT; ty<=u[i].y+SIGHT; ty++)
            for(int tx=u[i].x-SIGHT; tx<=u[i].x+SIGHT; tx++)
                if((tx-u[i].x)*(tx-u[i].x)+(ty-u[i].y)*(ty-u[i].y) <= SIGHT*SIGHT) cast(u[i].x,u[i].y,tx,ty);
    }
    // a freshly-seen, still-hidden alien activates its whole pod
    for(int i=0;i<nal;i++) if(al[i].alive && !al[i].active && vis[al[i].y][al[i].x]){
        int pod=al[i].pod; bool any=false;
        for(int j=0;j<nal;j++) if(al[j].alive && !al[j].active && al[j].pod==pod){ al[j].active=true; any=true; }
        if(any){
            shake(5); fxlock = fxlock>0.7f?fxlock:0.7f;
            note(72,INSTR_SAW,4); schedule(90,79,INSTR_SAW,4); schedule(180,84,INSTR_SAW,4);
            hit(48,INSTR_NOISE,3,200);
            for(int j=0;j<nal;j++) if(al[j].alive && al[j].pod==pod){
                spawn_float(al[j].x*TILE+6, al[j].y*TILE-4, CLR_RED, "!");
                scatter(j);
            }
        }
    }
}
static void scatter(int ai){
    Alien *a=&al[ai];
    int bx=a->x, by=a->y, bestcov=-1;
    for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++){
        if(abs(dx)+abs(dy)!=1) continue;          // orthogonal step
        int nx=a->x+dx, ny=a->y+dy;
        if(nx<0||ny<0||nx>=GW||ny>=GH) continue;
        if(mget(nx,ny)!=FLOOR || unit_at(nx,ny)>=0 || alien_at(nx,ny)>=0) continue;
        int cov=cover_value(nx,ny,nx<GW/2?GW:0,ny);   // prefer cover toward our side
        if(cov>bestcov || (cov==bestcov && rnd(2))){ bestcov=cov; bx=nx; by=ny; }
    }
    a->x=bx; a->y=by;
}

// ── reachable tiles for the selected soldier (BFS flood) ──────
static void compute_reach(int si){
    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++) reach[y][x]=-1;
    if(si<0 || !u[si].alive) return;
    int qx[GW*GH], qy[GW*GH], qd[GW*GH], h=0, t=0;
    reach[u[si].y][u[si].x]=0; qx[t]=u[si].x; qy[t]=u[si].y; qd[t]=0; t++;
    int DX[4]={1,-1,0,0}, DY[4]={0,0,1,-1};
    while(h<t){
        int cx=qx[h], cy=qy[h], cd=qd[h]; h++;
        if(cd>=SDASH) continue;
        for(int k=0;k<4;k++){
            int nx=cx+DX[k], ny=cy+DY[k];
            if(nx<0||ny<0||nx>=GW||ny>=GH) continue;
            if(reach[ny][nx]!=-1) continue;
            if(mget(nx,ny)!=FLOOR || unit_at(nx,ny)>=0 || alien_at(nx,ny)>=0) continue;
            reach[ny][nx]=cd+1; qx[t]=nx; qy[t]=ny; qd[t]=cd+1; t++;
        }
    }
}

// ── combat resolution ─────────────────────────────────────────
static void check_end(void);

static void forecast(int si,int ai,int *ph,int *pc){
    Unit *s=&u[si]; Alien *a=&al[ai];
    int dist=max(abs(s->x-a->x), abs(s->y-a->y));
    int cov=cover_value(a->x,a->y,s->x,s->y);
    int hitc=(int)clamp(s->aim - dist*2 - cov, 5, 95);
    int crit=(int)clamp(15 + (cov==0?35:0), 0, 90);
    *ph=hitc; *pc=crit;
}
static bool can_target(int si,int ai){
    if(si<0||ai<0) return false;
    Unit *s=&u[si]; Alien *a=&al[ai];
    if(!s->alive||s->ap<1||s->ammo<1) return false;
    if(!a->alive||!a->active||!vis[a->y][a->x]) return false;
    if(max(abs(s->x-a->x),abs(s->y-a->y))>SRANGE) return false;
    return los_clear(s->x,s->y,a->x,a->y);
}
static void do_fire(void){
    if(!can_target(sel,target)) { target=-1; return; }
    Unit *s=&u[sel]; Alien *a=&al[target];
    int sx=s->x*TILE+8, sy=s->y*TILE+8, ax=a->x*TILE+8, ay=a->y*TILE+8;
    int hitc,crit; forecast(sel,target,&hitc,&crit);
    s->ammo--; s->ap=0; s->face = sgn(a->x - s->x) ? sgn(a->x - s->x) : s->face;
    spawn_trace(sx,sy,ax,ay,CLR_YELLOW);
    note(70,INSTR_SQUARE,2); hit(52,INSTR_NOISE,4,55);
    fxlock=0.40f;
    if(rnd(100)<hitc){
        bool cr = rnd(100)<crit;
        int dmg = rnd_between(3,7); if(cr) dmg += dmg/2;
        a->hp-=dmg; a->flash=8;
        spawn_float(ax-2, ay-6, CLR_LIGHT_PEACH, str("%d",dmg));
        shake(cr?5.0f:2.0f); hit(44,INSTR_NOISE,5,90);
        if(cr){ boomT=0.6f; boomx=ax; boomy=ay-14; spawn_float(ax-6,ay-16,CLR_ORANGE,"CRIT"); }
        if(a->hp<=0){ a->alive=false; s->killsThis++; spawn_float(ax-6,ay,CLR_GREEN,"DOWN"); hit(40,INSTR_SAW,4,140); }
    } else {
        spawn_float(ax-6, ay-6, CLR_LIGHT_GREY, "MISS"); hit(82,INSTR_NOISE,2,40);
    }
    target=-1;
    recompute_vis();
    check_end();
}

static void soldier_dies(int si){
    u[si].alive=false; u[si].state=ST_NORMAL;
    spawn_float(u[si].x*TILE-6, u[si].y*TILE-2, CLR_RED, str("%s", sav.t[si].name));
    shake(6); note(36,INSTR_SINE,4); schedule(160,31,INSTR_SINE,3);
}
static void alien_shoot(int ai,int si){
    Alien *a=&al[ai]; Unit *s=&u[si];
    int ax=a->x*TILE+8, ay=a->y*TILE+8, sx=s->x*TILE+8, sy=s->y*TILE+8;
    int dist=max(abs(a->x-s->x),abs(a->y-s->y));
    int cov=cover_value(s->x,s->y,a->x,a->y) + (s->state==ST_HUNKER?20:0);
    int hitc=(int)clamp(AAIM[a->type] - dist*2 - cov, 5, 95);
    spawn_trace(ax,ay,sx,sy,CLR_RED);
    note(58,INSTR_SQUARE,2); hit(50,INSTR_NOISE,4,55);
    fxlock=0.45f;
    if(rnd(100)<hitc){
        int dmg=rnd_between(ADMGLO[a->type],ADMGHI[a->type]);
        s->hp-=dmg; s->flash=8; shake(3);
        spawn_float(sx-2,sy-6,CLR_RED,str("%d",dmg)); hit(46,INSTR_NOISE,5,80);
        if(s->hp<=0) soldier_dies(si);
    } else {
        spawn_float(sx-6,sy-6,CLR_LIGHT_GREY,"MISS"); hit(84,INSTR_NOISE,2,40);
    }
    check_end();
}
// overwatching soldiers snap-fire at an alien that just moved into view
static void reactions(int ai){
    Alien *a=&al[ai];
    int ax=a->x*TILE+8, ay=a->y*TILE+8;
    for(int i=0;i<SQUAD;i++){
        if(!u[i].alive || u[i].state!=ST_OVERWATCH || u[i].ammo<1) continue;
        int dist=max(abs(u[i].x-a->x),abs(u[i].y-a->y));
        if(dist>SRANGE || !los_clear(u[i].x,u[i].y,a->x,a->y)) continue;
        u[i].state=ST_NORMAL; u[i].ammo--;
        int sx=u[i].x*TILE+8, sy=u[i].y*TILE+8;
        int cov=cover_value(a->x,a->y,u[i].x,u[i].y);
        int hitc=(int)clamp(u[i].aim - 15 - dist*2 - cov, 5, 90);
        spawn_trace(sx,sy,ax,ay,CLR_LIGHT_YELLOW);
        note(76,INSTR_SQUARE,3); hit(54,INSTR_NOISE,4,50); fxlock=0.45f;
        if(rnd(100)<hitc){
            int dmg=rnd_between(3,6); a->hp-=dmg; a->flash=8; shake(3);
            spawn_float(ax-2,ay-6,CLR_LIGHT_YELLOW,str("%d",dmg));
            if(a->hp<=0){ a->alive=false; u[i].killsThis++; spawn_float(ax-6,ay,CLR_GREEN,"DOWN"); }
        } else spawn_float(ax-6,ay-6,CLR_LIGHT_GREY,"MISS");
        if(!a->alive) break;
    }
}

// ── soldier actions ───────────────────────────────────────────
static void try_move(int si,int cx,int cy){
    if(cx<0||cy<0||cx>=GW||cy>=GH) return;
    int d=reach[cy][cx];
    if(d<=0) return;
    if(d<=SMOVE && u[si].ap>=1)      u[si].ap-=1;
    else if(d<=SDASH && u[si].ap>=2) u[si].ap=0;
    else return;
    if(sgn(cx-u[si].x)) u[si].face=sgn(cx-u[si].x);
    u[si].x=cx; u[si].y=cy; u[si].state=ST_NORMAL;
    target=-1;
    hit(44,INSTR_NOISE,1,20);
    recompute_vis();
}
static void do_tile(int cx,int cy){
    if(cx<0||cy<0||cx>=GW||cy>=GH) return;
    int si=unit_at(cx,cy);
    if(si>=0){ sel=si; target=-1; return; }
    if(sel<0 || !u[sel].alive) return;
    int ai=alien_at(cx,cy);
    if(ai>=0 && al[ai].active && vis[cy][cx]){
        if(can_target(sel,ai)){ if(target==ai) do_fire(); else target=ai; }
        return;
    }
    if(target>=0){ target=-1; return; }
    try_move(sel,cx,cy);
}
static int next_soldier(int from){
    for(int k=1;k<=SQUAD;k++){ int i=(from+k)%SQUAD; if(u[i].alive) return i; }
    return from;
}

// ── turn flow ─────────────────────────────────────────────────
static void begin_player_turn(void){
    phase=PH_PLAYER;
    for(int i=0;i<SQUAD;i++) if(u[i].alive){ u[i].ap=2; u[i].state=ST_NORMAL; }
    target=-1;
    if(sel<0 || !u[sel].alive) sel=next_soldier(-1);
    recompute_vis();
}
static void begin_enemy_turn(void){
    phase=PH_ENEMY; sel=-1; target=-1; ai_cur=0; ai_timer=0.55f;
    for(int i=0;i<nal;i++) if(al[i].alive&&al[i].active){ al[i].mp=AMOVE[al[i].type]; al[i].shots=1; }
    note(31,INSTR_SAW,3); schedule(120,33,INSTR_SAW,2);   // tense drone swell
}
static bool alien_step(int ai,int gx,int gy){
    Alien *a=&al[ai];
    int dx=sgn(gx-a->x), dy=sgn(gy-a->y);
    int nx=a->x, ny=a->y;
    if(abs(gx-a->x) >= abs(gy-a->y)) nx+=dx; else ny+=dy;
    bool ok = nx>=0&&ny>=0&&nx<GW&&ny<GH && mget(nx,ny)==FLOOR && unit_at(nx,ny)<0 && alien_at(nx,ny)<0;
    if(!ok){                              // blocked → try the other axis
        nx=a->x; ny=a->y;
        if(abs(gx-a->x) >= abs(gy-a->y)) ny+=dy; else nx+=dx;
        ok = nx>=0&&ny>=0&&nx<GW&&ny<GH && mget(nx,ny)==FLOOR && unit_at(nx,ny)<0 && alien_at(nx,ny)<0;
    }
    if(ok){ a->x=nx; a->y=ny; return true; }
    return false;
}
static bool ai_action(void){
    while(ai_cur<nal){
        Alien *a=&al[ai_cur];
        if(!a->alive || !a->active || (a->mp<=0 && a->shots<=0)){ ai_cur++; continue; }
        int si=nearest_soldier(a->x,a->y);
        if(si<0){ a->mp=0; a->shots=0; ai_cur++; continue; }
        int dist=max(abs(a->x-u[si].x),abs(a->y-u[si].y));
        if(a->shots>0 && dist<=ARANGE[a->type] && los_clear(a->x,a->y,u[si].x,u[si].y)){
            alien_shoot(ai_cur,si); a->shots=0; a->mp=0; return true;
        }
        if(a->mp>0){
            bool moved=alien_step(ai_cur,u[si].x,u[si].y); a->mp--;
            if(a->alive) reactions(ai_cur);
            if(!moved) a->mp=0;
            return true;
        }
        a->mp=0; a->shots=0; ai_cur++;
    }
    return false;
}

// ── mission setup ─────────────────────────────────────────────
static void gen_map(void){
    float sxn=rnd_float()*40.0f, syn=rnd_float()*40.0f;
    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
        seen[y][x]=false; vis[y][x]=false;
        if(x<3){ mset(x,y,FLOOR); continue; }                 // deploy zone clear
        float n=noise2(x*0.55f+sxn, y*0.55f+syn);
        int r=rnd(100);
        if(n>0.74f)        mset(x,y,WALL);
        else if(r<10)      mset(x,y,CRATE);
        else if(r<24)      mset(x,y,SAND);
        else               mset(x,y,FLOOR);
    }
}
static void place_soldiers(void){
    int rows[SQUAD]={1,3,5,7};
    for(int i=0;i<SQUAD;i++){
        int r=rank_for(sav.t[i].kills);
        mset(1,rows[i],FLOOR);
        u[i].x=1; u[i].y=rows[i];
        u[i].maxhp=maxhp_for(r); u[i].hp=u[i].maxhp; u[i].aim=aim_for(r);
        u[i].ap=2; u[i].state=ST_NORMAL; u[i].alive=1; u[i].ammo=CLIP; u[i].face=1; u[i].flash=0;
        u[i].killsThis=0; u[i].killsBefore=sav.t[i].kills;
    }
}
static void place_aliens(void){
    nal = min(AMAX, 4 + sav.missions);
    int pod=0, placed=0;
    while(placed<nal){
        int cx=rnd_between(GW/2, GW), cy=rnd(GH);     // a pod's anchor on the right
        int sz=min(nal-placed, 2+rnd(2));
        int got=0, tries=0;
        while(got<sz && tries<60){
            tries++;
            int x=cx+rnd(5)-2, y=cy+rnd(5)-2;
            if(x<GW/2||x>=GW||y<0||y>=GH) continue;
            if(mget(x,y)!=FLOOR || alien_at(x,y)>=0) continue;
            Alien *a=&al[placed];
            a->type = (sav.missions>=2 && rnd(100)<30) ? 1 : 0;
            a->x=x; a->y=y; a->maxhp=AHP[a->type]; a->hp=a->maxhp;
            a->active=0; a->alive=1; a->pod=pod; a->mp=0; a->shots=0; a->flash=0;
            placed++; got++;
        }
        if(got==0) break;   // couldn't place — stop
        pod++;
    }
    nal=placed;
}
static void start_mission(void){
    gen_map(); place_soldiers(); place_aliens();
    sel=next_soldier(-1); target=-1; phase=PH_PLAYER; fxlock=0; boomT=0;
    for(int i=0;i<FX;i++){ flt[i].t=0; trc[i].t=0; }
    recompute_vis();
    gstate=GS_BATTLE;
}

// ── win / lose ────────────────────────────────────────────────
static void enter_debrief(int win){ dwin=win; gstate=GS_DEBRIEF; }
static void check_end(void){
    if(gstate!=GS_BATTLE) return;
    if(aliens_alive()==0)  enter_debrief(1);
    else if(soldiers_alive()==0) enter_debrief(0);
}
static void apply_debrief(void){
    for(int i=0;i<SQUAD;i++){
        if(u[i].alive) sav.t[i].kills += u[i].killsThis;
        else {                                  // KIA → a fresh rookie fills the slot
            const char *nm=RNAMES[rnd((int)(sizeof RNAMES/sizeof *RNAMES))];
            int j=0; for(; nm[j]&&j<11; j++) sav.t[i].name[j]=nm[j]; sav.t[i].name[j]=0;
            sav.t[i].kills=0;
        }
    }
    if(dwin) sav.missions++;
    save_bytes(&sav,sizeof sav);
    gstate=GS_BARRACKS; ren=-1;
}

// ── init / roster ─────────────────────────────────────────────
static void fresh_roster(void){
    sav.magic=SAVE_MAGIC; sav.missions=0;
    int used[SQUAD], nu=0;
    for(int i=0;i<SQUAD;i++){
        int idx, clash;
        do{ idx=rnd((int)(sizeof RNAMES/sizeof *RNAMES)); clash=0; for(int k=0;k<nu;k++) if(used[k]==idx) clash=1; }while(clash);
        used[nu++]=idx;
        const char *nm=RNAMES[idx]; int j=0; for(; nm[j]&&j<11; j++) sav.t[i].name[j]=nm[j]; sav.t[i].name[j]=0;
        sav.t[i].kills=0;
    }
}
void init(void){
    colorkey(0);
    bpm(96);
    instrument(5, INSTR_SAW, 320, 200, 3, 600);     // low tense drone
    instrument_filter(5, FILTER_LOW, 700, 5);
    Save s; int n=load_bytes(&s,sizeof s);
    if(n==(int)sizeof s && s.magic==SAVE_MAGIC) sav=s; else fresh_roster();
    gstate=GS_BARRACKS;
}

// ── input: barracks ───────────────────────────────────────────
static bool clicked(int x,int y,int w,int h){
    return mouse_pressed(MOUSE_LEFT) && mouse_x()>=x && mouse_x()<x+w && mouse_y()>=y && mouse_y()<y+h;
}
static void update_barracks(void){
    if(ren>=0){
        const char *in=text_input();
        for(int i=0;in[i];i++){ char c=in[i]; if(c>=32&&c<127&&rlen<11){ rbuf[rlen++]=c; rbuf[rlen]=0; } }
        if(keyp(KEY_BACKSPACE)&&rlen>0) rbuf[--rlen]=0;
        if(keyp(KEY_ENTER)){ if(rlen>0){ int j=0; for(;rbuf[j]&&j<11;j++) sav.t[ren].name[j]=rbuf[j]; sav.t[ren].name[j]=0; save_bytes(&sav,sizeof sav); } ren=-1; }
        if(keyp(KEY_ESCAPE)) ren=-1;
        return;
    }
    for(int i=0;i<SQUAD;i++) if(clicked(16,40+i*26,250,24)){ ren=i; rlen=0; rbuf[0]=0; note(72,INSTR_SQUARE,2); return; }
    if(clicked(SCREEN_W/2-50,168,100,22) || keyp(KEY_ENTER) || btnp(0,BTN_A)){ note(64,INSTR_SINE,3); start_mission(); }
}

// ── input: debrief ────────────────────────────────────────────
static void update_debrief(void){
    if(clicked(SCREEN_W/2-50,168,100,22) || btnp(0,BTN_A) || keyp(KEY_ENTER)) apply_debrief();
}

// ── input: battle ─────────────────────────────────────────────
static void update_player(void){
    if(fxlock>0) return;
    compute_reach(sel);

    // cursor: synced to mouse when it moves, else driven by arrows
    int mx=mouse_x(), my=mouse_y();
    if((mx!=pmx||my!=pmy) && my<HUDY){ kcx=mx/TILE; kcy=my/TILE; }
    pmx=mx; pmy=my;
    if(btnp(0,BTN_LEFT))  kcx--; if(btnp(0,BTN_RIGHT)) kcx++;
    if(btnp(0,BTN_UP))    kcy--; if(btnp(0,BTN_DOWN))  kcy++;
    kcx=(int)mid(0,kcx,GW-1); kcy=(int)mid(0,kcy,GH-1);

    // HUD buttons
    bool hasTgt = target>=0 && can_target(sel,target);
    if(hasTgt){
        if(clicked(118,172,58,22)){ do_fire(); return; }     // FIRE
        if(clicked(180,172,58,22)){ target=-1; return; }      // CANCEL
    } else if(sel>=0 && u[sel].alive && u[sel].ap>0){
        if(clicked(70,172,52,22)  || keyp('O')){ u[sel].state=ST_OVERWATCH; u[sel].ap=0; note(67,INSTR_SQUARE,2); return; }
        if(clicked(124,172,52,22) || keyp('H')){ u[sel].state=ST_HUNKER;    u[sel].ap=0; note(55,INSTR_SQUARE,2); return; }
        if(clicked(178,172,52,22) || keyp('R')){ if(u[sel].ammo<CLIP){ u[sel].ammo=CLIP; u[sel].ap--; hit(60,INSTR_SQUARE,3,40);} return; }
    }
    if(clicked(254,172,62,22) || keyp('E') || btnp(0,BTN_B)){ begin_enemy_turn(); return; }
    if(keyp(KEY_TAB)){ sel=next_soldier(sel<0?-1:sel); target=-1; return; }

    // map interaction
    if(mouse_pressed(MOUSE_LEFT) && my<HUDY) do_tile(mx/TILE,my/TILE);
    if(btnp(0,BTN_A)) do_tile(kcx,kcy);
}
static void update_enemy(float d){
    if(fxlock>0) return;
    ai_timer-=d;
    if(ai_timer>0) return;
    ai_timer=0.42f;
    if(!ai_action()) begin_player_turn();
}

void update(void){
    if(gstate==GS_BARRACKS){ update_barracks(); return; }
    if(gstate==GS_DEBRIEF){ update_debrief(); return; }
    float d=dt();
    if(fxlock>0) fxlock-=d;
    if(boomT>0)  boomT-=d;
    update_fx(d);
    for(int i=0;i<SQUAD;i++) if(u[i].flash>0) u[i].flash--;
    for(int i=0;i<nal;i++)   if(al[i].flash>0) al[i].flash--;
    if(phase==PH_ENEMY) update_enemy(d); else update_player();
}

// ── drawing helpers ───────────────────────────────────────────
static void cover_mark(int px,int py,int kind){       // 2 full, 1 half, 0 flanked
    if(kind==0){ print("x",px+11,py,CLR_RED); return; }
    int c = kind==2 ? CLR_GREEN : CLR_YELLOW;
    rectfill(px+11,py+1,4,3,c);
    trifill(px+11,py+4,px+15,py+4,px+13,py+6,c);
}
static void hp_pips(int px,int py,int hp,int maxhp,int col){
    if(hp>=maxhp) return;
    int w=14, fw=w*hp/maxhp; if(fw<1&&hp>0) fw=1;
    rectfill(px+1,py-2,w,2,CLR_BROWNISH_BLACK);
    rectfill(px+1,py-2,fw,2,col);
}
static void flash_box(int px,int py){
    fillp(FILL_CHECKER,-1); rectfill(px,py,16,16,CLR_WHITE); fillp_reset();
}

static void draw_battle(void){
    cls(CLR_BROWNISH_BLACK);
    map(0,0,0,0,GW,GH);

    // fog: hide unseen, dim remembered-but-not-visible
    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
        int px=x*TILE, py=y*TILE;
        if(!seen[y][x]) rectfill(px,py,TILE,TILE,CLR_BLACK);
        else if(!vis[y][x]){ fillp(FILL_CHECKER,-1); rectfill(px,py,TILE,TILE,CLR_BLACK); fillp_reset(); }
    }

    // move-range overlay for the selected soldier
    if(phase==PH_PLAYER && sel>=0 && u[sel].alive && u[sel].ap>0 && target<0){
        for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
            int r=reach[y][x]; if(r<=0) continue;
            int col = (r<=SMOVE) ? CLR_TRUE_BLUE : (u[sel].ap>=2 ? CLR_DARKER_BLUE : -1);
            if(col<0) continue;
            fillp(FILL_CHECKER,-1); rectfill(x*TILE,y*TILE,TILE,TILE,col); fillp_reset();
        }
    }

    // aliens (only the active, visible ones)
    int firstAlien = nearest_alien(sel>=0?u[sel].x:0, sel>=0?u[sel].y:0);
    for(int i=0;i<nal;i++){
        Alien *a=&al[i];
        if(!a->alive || !a->active || !vis[a->y][a->x]) continue;
        int px=a->x*TILE, py=a->y*TILE;
        spr(ASLOT[a->type], px, py);
        if(a->flash>0) flash_box(px,py);
        hp_pips(px,py,a->hp,a->maxhp,CLR_RED);
        if(i==target) rect(px,py,TILE,TILE, blink(6)?CLR_RED:CLR_ORANGE);
    }
    (void)firstAlien;

    // soldiers
    for(int i=0;i<SQUAD;i++){
        Unit *s=&u[i];
        if(!s->alive) continue;
        int px=s->x*TILE, py=s->y*TILE;
        pal(MAGIC_ARMOR, ARMOR[i]);
        sprf(SPR_SOLDIER, px, py, s->face<0, false);
        pal_reset();
        if(s->flash>0) flash_box(px,py);
        hp_pips(px,py,s->hp,s->maxhp,CLR_GREEN);
        if(s->state==ST_OVERWATCH) print("o",px+1,py-1,CLR_BLUE);
        if(s->state==ST_HUNKER)    print("v",px+1,py-1,CLR_LIGHT_GREY);
        // cover read vs nearest active alien
        int na=nearest_alien(s->x,s->y);
        if(na>=0){ int c=cover_value(s->x,s->y,al[na].x,al[na].y); cover_mark(px,py, c>=COVER_FULL?2:c>=COVER_HALF?1:0); }
        if(i==sel) rect(px,py,TILE,TILE, blink(8)?CLR_YELLOW:CLR_WHITE);
    }

    // cursor + shot-line preview
    if(phase==PH_PLAYER && fxlock<=0){
        rect(kcx*TILE,kcy*TILE,TILE,TILE,CLR_WHITE);
    }
    if(target>=0 && sel>=0){
        line(u[sel].x*TILE+8,u[sel].y*TILE+8, al[target].x*TILE+8,al[target].y*TILE+8, CLR_RED);
        int c=cover_value(al[target].x,al[target].y,u[sel].x,u[sel].y);
        cover_mark(al[target].x*TILE, al[target].y*TILE, c>=COVER_FULL?2:c>=COVER_HALF?1:0);
    }

    // tracers + impacts
    for(int i=0;i<FX;i++) if(trc[i].t>0){
        Tracer *t=&trc[i];
        line((int)t->x1,(int)t->y1,(int)t->x2,(int)t->y2,t->col);
        if(t->t > TR_DUR*0.5f) circfill((int)t->x1,(int)t->y1,2,CLR_YELLOW);   // muzzle
        else                   circfill((int)t->x2,(int)t->y2,2,CLR_ORANGE);   // impact
    }
    // floaters
    for(int i=0;i<FX;i++) if(flt[i].t>0) print(flt[i].txt,(int)flt[i].x,(int)flt[i].y,flt[i].col);
    if(boomT>0) print_scaled("BOOM!", boomx-18, boomy, CLR_ORANGE, 2);

    // enemy-turn banner
    if(phase==PH_ENEMY){
        rectfill(0,2,SCREEN_W,12,CLR_DARK_RED);
        print_centered(blink(14)?"ENEMY TURN":"...", SCREEN_W/2, 4, CLR_LIGHT_PEACH);
    }

    // ── HUD ──
    rectfill(0,HUDY,SCREEN_W,SCREEN_H-HUDY,CLR_DARKER_BLUE);
    line(0,HUDY,SCREEN_W,HUDY,CLR_DARK_GREY);
    if(sel>=0 && u[sel].alive){
        Unit *s=&u[sel];
        rectfill(6,HUDY+5,10,10,ARMOR[sel]);
        print(sav.t[sel].name, 20, HUDY+4, CLR_WHITE);
        print(RANKN[rank_for(sav.t[sel].kills)], 20, HUDY+14, CLR_LIGHT_GREY);
        bar(6,HUDY+26,90,6,(float)s->hp/s->maxhp, s->hp>s->maxhp/3?CLR_GREEN:CLR_RED, CLR_DARK_GREY);
        print(str("HP %d/%d",s->hp,s->maxhp),6,HUDY+34,CLR_LIGHT_GREY);
        // AP pips
        for(int k=0;k<2;k++) circfill(116+k*9,HUDY+9,3, k<s->ap?CLR_YELLOW:CLR_DARK_GREY);
        print("AP",116,HUDY+15,CLR_LIGHT_GREY);
        // ammo pips
        for(int k=0;k<CLIP;k++) rectfill(150+k*7,HUDY+6,5,7, k<s->ammo?CLR_ORANGE:CLR_DARK_GREY);
        print("AMMO",150,HUDY+15,CLR_LIGHT_GREY);
    } else if(phase==PH_PLAYER){
        print("select a soldier", 8, HUDY+10, CLR_DARK_GREY);
    }

    // buttons
    if(target>=0 && can_target(sel,target)){
        int hitc,crit; forecast(sel,target,&hitc,&crit);
        print(str("%s  HIT %d%%  CRIT %d%%  DMG 3-6", ANAME[al[target].type], hitc, crit), 6, HUDY+44, CLR_LIGHT_YELLOW);
        rectfill(118,172,58,22,CLR_DARK_RED);   rect(118,172,58,22,CLR_WHITE); print("FIRE",132,178,CLR_WHITE);
        rectfill(180,172,58,22,CLR_DARK_GREY);  rect(180,172,58,22,CLR_WHITE); print("CANCEL",188,178,CLR_LIGHT_GREY);
    } else if(phase==PH_PLAYER && sel>=0 && u[sel].alive && u[sel].ap>0){
        rectfill(70,172,52,22,CLR_BLUE_GREEN);  print("OVERWA",74,175,CLR_WHITE); print("(O)",90,184,CLR_LIGHT_GREY);
        rectfill(124,172,52,22,CLR_DARK_GREEN); print("HUNKER",128,175,CLR_WHITE);print("(H)",144,184,CLR_LIGHT_GREY);
        rectfill(178,172,52,22,CLR_DARK_BROWN); print("RELOAD",182,175,CLR_WHITE);print("(R)",198,184,CLR_LIGHT_GREY);
    }
    rectfill(254,172,62,22, phase==PH_PLAYER?CLR_DARK_PURPLE:CLR_DARKER_PURPLE);
    rect(254,172,62,22,CLR_WHITE); print("END (E)",262,178,CLR_LIGHT_PEACH);
    print(str("aliens %d", aliens_alive()), 258, HUDY+4, CLR_RED);
}

static void draw_barracks(void){
    cls(CLR_DARKER_BLUE);
    print_scaled("XCOM", (SCREEN_W-text_width("XCOM")*2)/2, 6, CLR_GREEN, 2);
    print_centered(str("ENEMY UNKNOWN   -   mission %d", sav.missions+1), SCREEN_W/2, 24, CLR_LIGHT_GREY);
    for(int i=0;i<SQUAD;i++){
        int y=40+i*26;
        rectfill(16,y,250,24, (ren==i)?CLR_DARKER_PURPLE:CLR_DARKER_GREY);
        rect(16,y,250,24,CLR_DARK_GREY);
        rectfill(22,y+6,12,12,ARMOR[i]);
        int r=rank_for(sav.t[i].kills);
        if(ren==i){
            const char *cur=blink(20)?"_":" ";
            print(str("%s%s", rbuf, cur), 42, y+3, CLR_WHITE);
            print("typing... ENTER to save", 42, y+13, CLR_LIGHT_GREY);
        } else {
            print(sav.t[i].name, 42, y+3, CLR_WHITE);
            print(str("%s   %d kills", RANKN[r], sav.t[i].kills), 42, y+13, CLR_LIGHT_GREY);
            print(str("HP %d", maxhp_for(r)), 200, y+3, CLR_GREEN);
            print(str("AIM %d", aim_for(r)), 200, y+13, CLR_ORANGE);
        }
    }
    print_centered("click a soldier to rename", SCREEN_W/2, 152, CLR_DARK_GREY);
    rectfill(SCREEN_W/2-50,168,100,22,CLR_DARK_GREEN); rect(SCREEN_W/2-50,168,100,22,CLR_WHITE);
    print_centered("DEPLOY", SCREEN_W/2, 174, CLR_WHITE);
    print_centered("permadeath - keep them alive", SCREEN_W/2, 192, CLR_DARK_GREY);
}

static void draw_debrief(void){
    cls(dwin?CLR_DARK_GREEN:CLR_DARKER_PURPLE);
    print_scaled(dwin?"VICTORY":"SQUAD LOST",
                 (SCREEN_W-text_width(dwin?"VICTORY":"SQUAD LOST")*2)/2, 10, dwin?CLR_GREEN:CLR_RED, 2);
    for(int i=0;i<SQUAD;i++){
        int y=44+i*24;
        rectfill(22,y+5,10,10,ARMOR[i]);
        if(!u[i].alive){
            print(str("%s", sav.t[i].name), 40, y, CLR_RED);
            print("KILLED IN ACTION", 150, y, CLR_DARK_RED);
        } else {
            int after=u[i].killsBefore+u[i].killsThis;
            bool promo = rank_for(after) > rank_for(u[i].killsBefore);
            print(str("%s   +%d kills", sav.t[i].name, u[i].killsThis), 40, y, CLR_WHITE);
            if(promo) print(str("PROMOTED: %s", RANKN[rank_for(after)]), 150, y, CLR_YELLOW);
            else      print("survived", 150, y, CLR_LIGHT_GREY);
        }
    }
    rectfill(SCREEN_W/2-50,168,100,22,CLR_DARK_BLUE); rect(SCREEN_W/2-50,168,100,22,CLR_WHITE);
    print_centered("CONTINUE", SCREEN_W/2, 174, CLR_WHITE);
}

void draw(void){
    if(gstate==GS_BARRACKS) draw_barracks();
    else if(gstate==GS_DEBRIEF) draw_debrief();
    else draw_battle();
}
