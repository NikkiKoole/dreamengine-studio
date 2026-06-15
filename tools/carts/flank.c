#include "studio.h"
#include <stdio.h>
#include <math.h>

// FLANK — top-down tactical AI: a squad that isn't dumb. (A testbed for a GTA1 /
// Hotline-Miami enemy brain.) Four ideas from the roguelike toolkit, applied to
// a real-time shooter:
//   * COMMUNICATION — a shared blackboard. The instant ANY enemy sees you it
//     calls your position to the whole squad; blind enemies converge on it.
//     Knowledge decays, so break contact and they go to your last-known spot.
//   * FLOW FIELD — a Dijkstra map flooded from your last-known cell; enemies
//     roll downhill to approach AROUND cover, not into walls.
//   * HEATMAP / FLANKING — your aim projects a DANGER CONE; enemies score
//     firing spots and AVOID the cone, so they peel off to hit you from the
//     sides and behind. Toggle H to see the heat.
//   * COVER + SPREAD — they favour cells beside walls and away from each other,
//     so they peek from cover and don't bunch into one kill-funnel.
//
// WASD move · Shift sneak · mouse aim · L-click gun (loud) · R-click knife (quiet)
// · H heat · L comms · V reveal · TAB spectate · R reset
// Design notes + behaviour menu + sloop porting: docs/design/flank-tactical-ai.md

#define GW 40
#define GH 23
#define TILE 8
#define HUD_Y (GH * TILE)          // 184
#define NE 6
#define NB 96
#define INF 1e9f
#define DIAG 1.41421356f

enum { E_PATROL, E_SUSPECT, E_HUNT, E_ENGAGE, E_DOWN };   // graded alertness drives the state
static const int ECOL[5] = { CLR_DARK_GREY, CLR_YELLOW, CLR_ORANGE, CLR_RED, CLR_BLACK };

// enemy types — distinct tactics. range = preferred firing distance; coverW/heatW
// = how much it values cover / fears your aim cone; strafeW = how much it circles.
enum { TY_RUSH, TY_CAMP, TY_FLANK, NTY };
typedef struct { char g; int hp, mag, gap; float range, coverW, heatW, speed, strafeW, spread; } EType;
static const EType TY[NTY] = {
    { 'R', 26,  6, 16, 26,  0.4f, 2.5f, 1.45f, 0.5f, 18 },   // rusher  — charges in close, sprays, ignores danger
    { 'C', 34, 10, 26, 96,  3.5f, 7.0f, 0.62f, 0.1f, 8  },   // camper  — holds cover at long range, accurate, slow
    { 'F', 30,  8, 22, 60,  1.3f, 9.0f, 1.15f, 1.5f, 13 },   // flanker — circles to your sides, dodges the cone
};

static unsigned char cell[GH][GW];  // 0 floor · 1 FULL cover (blocks move+LOS+bullets) · 2 LOW cover (blocks move only)
static float flow[GH][GW];          // Dijkstra from last-known player cell
static float heat[GH][GW];          // danger projected by the player's aim cone

#define KEY_LSHIFT 340             // raylib left-shift — held to sneak
#define VIS_R 96                   // player's own vision radius (fog of war)
typedef struct { float x, y, vx, vy, aim, pinned; int hp, mag, reload, shake, spectate, sneak; } Player;  // pinned 0..1 = suppression
typedef struct { float x, y, aim, lsx, lsy, alert; int hp, alive, state, shootcd, mag, reload, callcd, type, strafe, strafeT, suppressing, everseen, invx, invy; } Enemy;  // alert 0..100; inv = spot to investigate
typedef struct { float x, y, vx, vy; int alive, foe; } Bullet;
static Player pl;
static Enemy en[NE];
static Bullet bul[NB];

// shared blackboard (the squad's collective knowledge of you)
static float kx, ky; static int known, kage, flow_cx = -1, flow_cy = -1;
static int show_heat = 1, show_comms = 0, reveal, kills, msg_t, tick; static char msg[40];

// ---- voices ----------------------------------------------------------------
typedef struct { int h, ttl; } Voice; static Voice voices[16];
static void play_pan(int midi, int instr, int vol, float pan, int ttl) {
    for (int i = 0; i < 16; i++) if (voices[i].ttl <= 0) { voices[i].h = note_on(midi,instr,vol); note_pan(voices[i].h,pan); voices[i].ttl = ttl; return; }
}
static void voices_tick(void) { for (int i = 0; i < 16; i++) if (voices[i].ttl > 0 && --voices[i].ttl == 0) note_off(voices[i].h); }
static float panx(float x) { return x / SCREEN_W * 2 - 1; }

// ---- geometry --------------------------------------------------------------
static bool wallpx(float x, float y) { int cx = (int)(x/TILE), cy = (int)(y/TILE); return cx<0||cx>=GW||cy<0||cy>=GH||cell[cy][cx]!=0; }   // SOLID (full+low) — movement & nav
static bool wcell(int x, int y) { return x<0||x>=GW||y<0||y>=GH||cell[y][x]!=0; }                                                          // SOLID
static bool opaquepx(float x, float y) { int cx = (int)(x/TILE), cy = (int)(y/TILE); return cx<0||cx>=GW||cy<0||cy>=GH||cell[cy][cx]==1; } // blocks LOS + bullets (FULL only)
static bool islow(int x, int y) { return x>=0&&x<GW&&y>=0&&y<GH&&cell[y][x]==2; }
// is there LOW cover on the side of (x,y) facing direction (dirx,diry)? (= cover between you and a threat there)
static bool low_facing(float x, float y, float dirx, float diry) {
    int cx=(int)(x/TILE), cy=(int)(y/TILE), sx = dirx>0?1:dirx<0?-1:0, sy = diry>0?1:diry<0?-1:0;
    return (sx && islow(cx+sx,cy)) || (sy && islow(cx,cy+sy)) || (sx && sy && islow(cx+sx,cy+sy));
}
static bool los_px(float x0, float y0, float x1, float y1) {       // FULL cover blocks sight; you see OVER low cover
    float dx = x1-x0, dy = y1-y0, d = fsqrt(dx*dx+dy*dy); int n = (int)(d/4)+1;
    for (int i = 1; i < n; i++) { float t = (float)i/n; if (opaquepx(x0+dx*t, y0+dy*t)) return false; }
    return true;
}
static float angnorm(float a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }

// ---- Dijkstra flow field to last-known position ----------------------------
static void relax(float f[GH][GW]) {
    bool ch = true; int g = 0;
    while (ch && g++ < GW*GH) { ch = false;
        for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
            if (cell[y][x]) continue; float b = f[y][x];
            for (int dy=-1; dy<=1; dy++) for (int dx=-1; dx<=1; dx++) {
                if (!dx&&!dy) continue; if (wcell(x+dx,y+dy)) continue;
                if (dx&&dy&&(wcell(x+dx,y)||wcell(x,y+dy))) continue;
                float v = f[y+dy][x+dx] + (dx&&dy?DIAG:1.0f); if (v < b) b = v;
            }
            if (b < f[y][x]) { f[y][x] = b; ch = true; }
        }
    }
}
static void reflow(int cx, int cy) {
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) flow[y][x] = INF;
    if (!wcell(cx,cy)) flow[cy][cx] = 0;
    relax(flow); flow_cx = cx; flow_cy = cy;
}

// ---- the danger heatmap: a cone in front of the player's aim ----------------
static void compute_heat(void) {
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) {
        float cxp = x*TILE+4, cyp = y*TILE+4, dx = cxp-pl.x, dy = cyp-pl.y, d = fsqrt(dx*dx+dy*dy);
        float h = 0;
        if (d < 150) {
            float a = angnorm(angle_to(pl.x,pl.y,cxp,cyp) - pl.aim);
            float cone = 1 - fabsf(a)/55.0f;                 // 55deg half-cone
            if (cone > 0) h = cone * (1 - d/150);
            if (d < 30) h = h < 0.5f ? 0.5f : h;             // point-blank ring
        }
        heat[y][x] = h;
    }
}
static float heat_at(float x, float y) { int cx=(int)(x/TILE), cy=(int)(y/TILE); return wcell(cx,cy)?1:heat[cy][cx]; }
static bool near_cover(int cx, int cy) {
    for (int k=0;k<4;k++) { int nx=cx+(k==0)-(k==1), ny=cy+(k==2)-(k==3); if (wcell(nx,ny)) return true; }
    return false;
}

// ---- bullets ---------------------------------------------------------------
static void fire(float x, float y, float aim, int foe, float spread) {
    for (int i=0;i<NB;i++) if (!bul[i].alive) {
        float a = aim + (rnd(1000)/1000.0f-0.5f)*spread;
        bul[i] = (Bullet){ x, y, dx(5.5f,a), dy(5.5f,a), 1, foe };
        play_pan(foe?38:50, INSTR_NOISE, foe?3:4, panx(x), 5);
        return;
    }
}

// ---- setup -----------------------------------------------------------------
static void reset(void) {
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) cell[y][x] = (x==0||y==0||x==GW-1||y==GH-1);
    // FULL cover blocks (the arena walls)
    int bx[]={8,8,18,18,28,30,14,24,33,6}, by[]={5,15,9,18,5,16,12,12,20,9}, bw[]={3,4,2,5,4,3,2,2,3,2}, bh[]={4,2,5,2,3,2,2,3,2,4};
    for (int b=0;b<10;b++) for (int y=by[b]; y<by[b]+bh[b] && y<GH-1; y++) for (int x=bx[b]; x<bx[b]+bw[b] && x<GW-1; x++) cell[y][x]=1;
    // LOW cover (crates you shoot OVER) scattered in the open midfield
    int lx[]={12,16,22,26,11,21,31,35,15,27,7,34}, ly[]={6,14,4,19,18,10,8,15,8,17,12,4};
    for (int b=0;b<12;b++) if (!cell[ly[b]][lx[b]]) cell[ly[b]][lx[b]] = 2;
    pl = (Player){ .x = TILE*3, .y = TILE*(GH/2), .hp = 100, .mag = 12, .spectate = pl.spectate };
    for (int i=0;i<NE;i++) {
        int x,y,tr=0; do { x=GW/2+rnd(GW/2-2); y=2+rnd(GH-4); tr++; } while (wcell(x,y) && tr<80);
        int t = i % NTY;                                   // 2 of each type for 6 enemies
        en[i] = (Enemy){ .x = x*TILE+4, .y = y*TILE+4, .aim = 180, .hp = TY[t].hp, .alive = 1,
                         .state = E_PATROL, .mag = TY[t].mag, .type = t,
                         .strafe = rnd(2) ? 1 : -1, .strafeT = rnd(120) };
    }
    for (int i=0;i<NB;i++) bul[i].alive = 0;
    known = 0; kage = 999; flow_cx = -1; kills = 0; msg_t = 0;
}
void init(void) { reverb(0.25f, 0.5f); reset(); }

static void setmsg(const char *s) { snprintf(msg,sizeof msg,"%s",s); msg_t=90; }

// ---- enemy brain -----------------------------------------------------------
static int enemy_at_px(float x, float y, int self) {
    for (int i=0;i<NE;i++) { if (i==self||!en[i].alive) continue; float dx=en[i].x-x, dy=en[i].y-y; if (dx*dx+dy*dy < 100) return i; }
    return -1;
}
static void move_enemy(Enemy *e, float ax, float ay, float spd) {
    float dx=ax-e->x, dy=ay-e->y, d=fsqrt(dx*dx+dy*dy);
    if (d < 0.5f) return;
    float nx = e->x + dx/d*spd, ny = e->y + dy/d*spd;
    if (!wallpx(nx, e->y) && enemy_at_px(nx, e->y, (int)(e-en)) < 0) e->x = nx;   // slide on walls/peers (no jam)
    if (!wallpx(e->x, ny) && enemy_at_px(e->x, ny, (int)(e-en)) < 0) e->y = ny;
}

// a NOISE at (x,y): every enemy within radius gets more alert and turns to investigate it.
// loud gunfire is a big noise; a silent knife makes none — this is the heart of stealth.
static void noise_at(float x, float y, float radius, float amount) {
    for (int i=0;i<NE;i++) if (en[i].alive) {
        float d = fsqrt((en[i].x-x)*(en[i].x-x)+(en[i].y-y)*(en[i].y-y));
        if (d < radius) { en[i].alert += amount * (1 - d/radius); if (en[i].alert>100) en[i].alert=100; en[i].invx=(int)x; en[i].invy=(int)y; }
    }
}

static void enemy_update(int i) {
    Enemy *e = &en[i];
    if (!e->alive) return;
    const EType *T = &TY[e->type];
    if (e->shootcd > 0) e->shootcd--;
    if (e->callcd > 0) e->callcd--;
    e->suppressing = 0;
    float pd = fsqrt((pl.x-e->x)*(pl.x-e->x) + (pl.y-e->y)*(pl.y-e->y));
    bool canlos = los_px(e->x, e->y, pl.x, pl.y);
    float ss = pl.sneak ? 0.5f : 1.0f, sh = pl.sneak ? 0.3f : 1.0f;
    bool see   = canlos && pd <= 132 * ss;                    // spotted by sight (sneaking shrinks range)
    bool heard = pd <= 48 * sh;                               // or heard moving nearby
    if (canlos && pd <= VIS_R) { e->everseen = 1; e->lsx = e->x; e->lsy = e->y; }   // YOUR last-seen memory of this enemy

    // --- graded alertness: stimuli RAISE it, time cools it down ---
    if (see)        { e->alert = 100;                                   e->invx=(int)pl.x; e->invy=(int)pl.y; }   // direct sight = instant alarm
    else if (heard) { e->alert = e->alert<55 ? 55 : (e->alert<100?e->alert+3:100); e->invx=(int)pl.x; e->invy=(int)pl.y; }  // a noise nearby = suspicious, rising
    e->alert -= 0.22f; if (e->alert < 0) e->alert = 0;

    if ((see || heard) && e->alert >= 70) {                   // fully alarmed AND fresh contact -> tell the squad
        if (!known && e->callcd <= 0) { play_pan(72, INSTR_REED, 3, panx(e->x), 16); setmsg("enemy: \"contact!\""); e->callcd = 120; }
        known = 1; kx = pl.x; ky = pl.y; kage = 0;
    }
    // alert level -> behaviour
    if (e->alert >= 70)      e->state = canlos ? E_ENGAGE : E_HUNT;     // alarmed: engage if you can see me, else close in
    else if (e->alert >= 30) e->state = E_SUSPECT;                      // suspicious: go check the disturbance
    else                     e->state = E_PATROL;                       // calm: back to wandering ("gave up")

    int kcx = (int)(kx/TILE), kcy = (int)(ky/TILE);
    if (known && (kcx != flow_cx || kcy != flow_cy)) reflow(kcx, kcy);

    // --- act per state ---
    if (e->state == E_ENGAGE) {
        if (e->type == TY_CAMP && los_px(e->x,e->y,pl.x,pl.y)) {   // SUPPRESSION: anchor + pour inaccurate fire to PIN you
            e->aim = angle_to(e->x, e->y, pl.x, pl.y);
            e->suppressing = e->mag > 0;
            if (e->mag > 0 && e->shootcd <= 0) { fire(e->x, e->y, e->aim, 1, 26); e->mag--; e->shootcd = 5; if (e->mag==0) e->reload = 85; }
            if (e->mag == 0 && --e->reload <= 0) e->mag = T->mag;  // reload = the gap in the pin (your window to move)
            return;
        }
        // pick a firing stance per type: keep LOS, dodge the heat cone, hold the
        // type's range, hug cover, circle (strafe), spread from squadmates.
        if (--e->strafeT <= 0) { e->strafe = -e->strafe; e->strafeT = 60 + rnd(90); }
        float perp = angle_to(pl.x, pl.y, e->x, e->y) + 90 * e->strafe;   // lateral-around-player dir
        float best = -1e9f, bx = e->x, by = e->y;
        for (int a=0;a<8;a++) {
            float ang = a*45, ox = e->x + dx(9, ang), oy = e->y + dy(9, ang);
            if (wallpx(ox, oy) || enemy_at_px(ox, oy, i) >= 0) continue;
            int cx=(int)(ox/TILE), cy=(int)(oy/TILE);
            float od = fsqrt((pl.x-ox)*(pl.x-ox)+(pl.y-oy)*(pl.y-oy));
            float sc = los_px(ox,oy,pl.x,pl.y) ? 4 : -7;     // must be able to shoot
            sc -= heat_at(ox,oy) * T->heatW;                 // AVOID the aim cone -> flank
            sc -= fabsf(od - T->range) * 0.05f;              // hold this type's range
            if (near_cover(cx,cy)) sc += T->coverW;          // peek from any cover
            if (low_facing(ox,oy, pl.x-ox, pl.y-oy)) sc += T->coverW + 1.5f;   // LOW cover facing you = protected firing spot
            sc += T->strafeW * cos_deg(angnorm(ang - perp)); // circle the player
            for (int j=0;j<NE;j++) if (j!=i && en[j].alive) { float dd=fsqrt((en[j].x-ox)*(en[j].x-ox)+(en[j].y-oy)*(en[j].y-oy)); if (dd<26) sc -= (26-dd)*0.12f; }
            if (sc > best) { best = sc; bx = ox; by = oy; }
        }
        // hold position ONLY when already safe in cover (campers settle; rushers/
        // flankers stay in motion because they're rarely "safe") — fixes the freeze
        int ecx=(int)(e->x/TILE), ecy=(int)(e->y/TILE);
        bool safe = heat_at(e->x,e->y) < 0.12f && near_cover(ecx,ecy) && los_px(e->x,e->y,pl.x,pl.y);
        if (!safe) move_enemy(e, bx, by, T->speed);
        e->aim = angle_to(e->x, e->y, pl.x, pl.y);
        if (los_px(e->x,e->y,pl.x,pl.y) && e->shootcd <= 0 && e->mag > 0) {
            fire(e->x, e->y, e->aim, 1, T->spread); e->mag--; e->shootcd = T->gap;
            if (e->mag == 0) e->reload = 70;
        }
        if (e->mag == 0) { if (--e->reload <= 0) e->mag = T->mag; }   // reload window = vulnerable
    } else if (e->state == E_HUNT) {                          // alarmed, no LOS: close in
        if (known) {                                          // squad knows where you are -> flow around cover
            float bestf = flow[(int)(e->y/TILE)][(int)(e->x/TILE)], tx = e->x, ty = e->y;
            for (int a=0;a<8;a++) { float ox=e->x+dx(7,a*45), oy=e->y+dy(7,a*45); int cx=(int)(ox/TILE),cy=(int)(oy/TILE);
                if (wcell(cx,cy)||enemy_at_px(ox,oy,i)>=0) continue; if (flow[cy][cx] < bestf) { bestf=flow[cy][cx]; tx=ox; ty=oy; } }
            move_enemy(e, tx, ty, T->speed * 0.92f);
        } else move_enemy(e, e->invx, e->invy, T->speed * 0.9f);   // alarmed only by a noise -> head to it
        e->aim = angle_to(e->x, e->y, pl.x, pl.y);
    } else if (e->state == E_SUSPECT) {                       // investigate the disturbance, then give up
        move_enemy(e, e->invx, e->invy, T->speed * 0.55f);    // cautious
        e->aim = (rnd(28)==0) ? rnd(360) : angle_to(e->x, e->y, e->invx, e->invy);   // glance around
    } else {                                                  // PATROL: small idle drift
        if (rnd(40)==0) e->aim = rnd(360);
        move_enemy(e, e->x+dx(8,e->aim), e->y+dy(8,e->aim), 0.25f);
    }
}

// ---- player ----------------------------------------------------------------
static void player_update(void) {
    if (pl.spectate) {                                        // autopilot: kite the nearest visible enemy
        int t=-1; float bd=1e9f;
        for (int i=0;i<NE;i++) if (en[i].alive && los_px(pl.x,pl.y,en[i].x,en[i].y)) { float d=(en[i].x-pl.x)*(en[i].x-pl.x)+(en[i].y-pl.y)*(en[i].y-pl.y); if (d<bd){bd=d;t=i;} }
        if (t>=0) { pl.aim = angle_to(pl.x,pl.y,en[t].x,en[t].y);   // visible: kite + fire
            float perp = pl.aim+90, mx = dx(1.4f,perp)+dx(0.6f,pl.aim+180), my = dy(1.4f,perp)+dy(0.6f,pl.aim+180);
            if (!wallpx(pl.x+mx,pl.y)) pl.x+=mx; if (!wallpx(pl.x,pl.y+my)) pl.y+=my;
            if (pl.mag>0 && (tick%9==0)) { fire(pl.x,pl.y,pl.aim,0,6); pl.mag--; if(pl.mag==0) pl.reload=40; noise_at(pl.x,pl.y,155,60); } }
        else {                                                      // none visible: advance to make contact
            int n=-1; float nd=1e9f;
            for (int i=0;i<NE;i++) if (en[i].alive) { float d=(en[i].x-pl.x)*(en[i].x-pl.x)+(en[i].y-pl.y)*(en[i].y-pl.y); if (d<nd){nd=d;n=i;} }
            if (n>=0) { pl.aim = angle_to(pl.x,pl.y,en[n].x,en[n].y);
                float mx=dx(1.5f,pl.aim), my=dy(1.5f,pl.aim);
                if (!wallpx(pl.x+mx,pl.y)) pl.x+=mx; if (!wallpx(pl.x,pl.y+my)) pl.y+=my; }
        }
        if (pl.mag==0 && --pl.reload<=0) pl.mag=12;
        return;
    }
    pl.sneak = key(KEY_LSHIFT);                               // hold Shift = sneak: slower but quiet + low profile
    float spd = 1.6f * (pl.sneak ? 0.5f : 1.0f) * (1 - 0.6f*pl.pinned);   // sneak + suppression both slow you
    float mx=0,my=0;
    if (key('A')||key(KEY_LEFT)) mx=-spd; else if (key('D')||key(KEY_RIGHT)) mx=spd;
    if (key('W')||key(KEY_UP)) my=-spd; else if (key('S')||key(KEY_DOWN)) my=spd;
    if (!wallpx(pl.x+mx, pl.y)) pl.x += mx; if (!wallpx(pl.x, pl.y+my)) pl.y += my;
    pl.aim = angle_to(pl.x, pl.y, mouse_x(), mouse_y());
    if ((mouse_down(0)||key(KEY_SPACE)) && pl.mag>0 && pl.reload<=0) {        // LOUD gun
        fire(pl.x,pl.y,pl.aim,0,5 + pl.pinned*16); pl.mag--; pl.reload=8; if(pl.mag==0) pl.reload=45;   // suppressed = your aim shakes too
        noise_at(pl.x, pl.y, 155, 60);                                          // a gunshot carries — the whole room may hear
    }
    if (mouse_pressed(1)) {                                                   // QUIET knife — silent close kill (makes NO noise)
        for (int j=0;j<NE;j++) if (en[j].alive) {
            float d = fsqrt((en[j].x-pl.x)*(en[j].x-pl.x)+(en[j].y-pl.y)*(en[j].y-pl.y));
            if (d < 13) { en[j].alive=0; en[j].state=E_DOWN; kills++; pl.shake=3; play_pan(64,INSTR_MEMBRANE,1,panx(en[j].x),4); break; }
        }
    }
    if (pl.reload>0) pl.reload--;
    if (pl.mag==0 && pl.reload<=0) pl.mag=12;
}

void update(void) {
    voices_tick();
    tick++;
    if (msg_t>0) msg_t--;
    if (keyp('R')) { reset(); return; }
    if (keyp('H')) show_heat = !show_heat;
    if (keyp('L')) show_comms = !show_comms;
    if (keyp('V')) reveal = !reveal;
    if (keyp(KEY_TAB)) { pl.spectate = !pl.spectate; }
    if (pl.hp <= 0) return;

    if (known) kage++;
    compute_heat();
    player_update();
    for (int i=0;i<NE;i++) enemy_update(i);

    // suppression: are you pinned right now? (any suppressor with LOS to you)
    int sup = 0;
    for (int i=0;i<NE;i++) if (en[i].alive && en[i].suppressing && los_px(en[i].x,en[i].y,pl.x,pl.y)) sup = 1;
    pl.pinned += sup ? 0.05f : -0.035f;
    if (pl.pinned < 0) pl.pinned = 0; if (pl.pinned > 1) pl.pinned = 1;
    if (pl.pinned > 0.3f && tick%3==0) pl.shake = 2;          // pinned = jittery

    // bullets
    for (int i=0;i<NB;i++) {
        if (!bul[i].alive) continue;
        bul[i].x += bul[i].vx; bul[i].y += bul[i].vy;
        if (opaquepx(bul[i].x, bul[i].y)) { bul[i].alive = 0; continue; }   // full cover stops it; flies over low cover
        if (bul[i].foe) {
            if (fabsf(bul[i].x-pl.x)<4 && fabsf(bul[i].y-pl.y)<4) {
                bul[i].alive=0;
                if (low_facing(pl.x,pl.y,-bul[i].vx,-bul[i].vy) && rnd(2)==0) play_pan(56,INSTR_MEMBRANE,2,panx(pl.x),4);  // crate ate it
                else { pl.hp -= 8; pl.shake=6; play_pan(34,INSTR_NOISE,4,panx(pl.x),6); if (pl.hp<=0){ pl.hp=0; setmsg("you are down. R to retry."); } }
            }
        } else for (int j=0;j<NE;j++) if (en[j].alive && fabsf(bul[i].x-en[j].x)<4 && fabsf(bul[i].y-en[j].y)<4) {
            bul[i].alive=0;
            if (low_facing(en[j].x,en[j].y,-bul[i].vx,-bul[i].vy) && rnd(2)==0) { play_pan(56,INSTR_MEMBRANE,2,panx(en[j].x),4); break; }  // covered
            en[j].hp -= 12; play_pan(60,INSTR_MEMBRANE,3,panx(en[j].x),5);
            if (en[j].hp<=0) { en[j].alive=0; en[j].state=E_DOWN; kills++; play_pan(40,INSTR_REED,3,panx(en[j].x),18); }
            break;
        }
    }
    if (pl.shake>0) pl.shake--;

#ifdef DE_TRACE
    int alive=0,eng=0; for(int i=0;i<NE;i++) if(en[i].alive){alive++; if(en[i].state==E_ENGAGE)eng++;}
    watch("hp","%d",pl.hp); watch("alive","%d",alive); watch("engaging","%d",eng);
    watch("known","%d",known); watch("kills","%d",kills);
#endif
}

// ---- draw ------------------------------------------------------------------
void draw(void) {
    cls(CLR_BLACK);
    int sh = pl.shake>0 ? rnd(3)-1 : 0;
    // heat overlay (danger cone) — debug viz
    if (show_heat) for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) if (heat[y][x] > 0.05f) {
        int h = heat[y][x] > 0.6f ? CLR_RED : heat[y][x] > 0.3f ? CLR_ORANGE : CLR_BROWN;
        rectfill(x*TILE+sh, y*TILE, TILE, TILE, h);
    }
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) {
        if (cell[y][x]==1) rectfill(x*TILE+sh, y*TILE, TILE, TILE, CLR_DARK_GREY);          // full cover: tall block
        else if (cell[y][x]==2) { rectfill(x*TILE+sh+1, y*TILE+2, TILE-2, TILE-3, CLR_BROWN); rect(x*TILE+sh+1, y*TILE+2, TILE-2, TILE-3, CLR_ORANGE); }  // low cover: a crate
    }

    // comms lines (debug, off by default) + the squad's belief about where you are
    int anysee = 0; for (int i=0;i<NE;i++) if (en[i].alive && los_px(en[i].x,en[i].y,pl.x,pl.y)) anysee=1;
    if (show_comms && known) for (int i=0;i<NE;i++) if (en[i].alive && los_px(en[i].x,en[i].y,pl.x,pl.y))
        line((int)en[i].x+sh, (int)en[i].y, (int)kx+sh, (int)ky, CLR_DARK_GREEN);
    if (known) {
        if (anysee) { pset((int)kx+sh,(int)ky,CLR_GREEN); circ((int)kx+sh,(int)ky,3,CLR_DARK_GREEN); }
        else print("?", (int)kx-1+sh, (int)ky-3, blink(20)?CLR_YELLOW:CLR_ORANGE);   // they're investigating your LAST-SEEN spot
    }

    // bullets
    for (int i=0;i<NB;i++) if (bul[i].alive)
        line((int)bul[i].x+sh,(int)bul[i].y,(int)(bul[i].x-bul[i].vx)+sh,(int)(bul[i].y-bul[i].vy), bul[i].foe?CLR_RED:CLR_YELLOW);

    // enemies — FOG OF WAR: draw those you can see; a dim ghost where you last saw the rest
    for (int i=0;i<NE;i++) {
        if (!en[i].alive) continue;
        float ed = fsqrt((en[i].x-pl.x)*(en[i].x-pl.x)+(en[i].y-pl.y)*(en[i].y-pl.y));
        bool vis = reveal || pl.spectate || (ed <= VIS_R && los_px(pl.x,pl.y,en[i].x,en[i].y));
        if (vis) { int x=(int)en[i].x+sh, y=(int)en[i].y;
            if (en[i].suppressing) circ(x,y,5,blink(4)?CLR_RED:CLR_ORANGE);    // muzzle bloom of a suppressor
            line(x,y,(int)(en[i].x+dx(6,en[i].aim))+sh,(int)(en[i].y+dy(6,en[i].aim)),CLR_LIGHT_GREY);
            print(str("%c", TY[en[i].type].g), x-2, y-3, ECOL[en[i].state]);
            if (en[i].state==E_SUSPECT) print("?", x-1, y-9, CLR_YELLOW);                    // investigating
            else if (en[i].state>=E_HUNT && en[i].state<=E_ENGAGE) print("!", x-1, y-9, CLR_RED);  // alarmed
        } else if (en[i].everseen)                                            // last-seen ghost (your memory)
            print(str("%c", TY[en[i].type].g), (int)en[i].lsx-2+sh, (int)en[i].lsy-3, CLR_DARKER_GREY);
    }
    // your vision ring + you
    if (!reveal && !pl.spectate) circ((int)pl.x+sh, (int)pl.y, VIS_R, CLR_DARKER_GREY);
    int px=(int)pl.x+sh, py=(int)pl.y;
    circfill(px,py,3, pl.sneak ? CLR_DARK_GREY : (pl.hp>0?CLR_WHITE:CLR_DARK_GREY));
    line(px,py,(int)(pl.x+dx(8,pl.aim))+sh,(int)(pl.y+dy(8,pl.aim)),CLR_YELLOW);
    if (pl.pinned > 0.3f) { rect(0,0,SCREEN_W,HUD_Y,CLR_RED); print("PINNED", px-11, py-15, blink(6)?CLR_RED:CLR_ORANGE); }

    // HUD
    rectfill(0,HUD_Y,SCREEN_W,SCREEN_H-HUD_Y,CLR_DARKER_BLUE);
    rect(4,HUD_Y+3,42,6,CLR_DARK_GREY); rectfill(5,HUD_Y+4,40*(pl.hp>0?pl.hp:0)/100,4,pl.hp>30?CLR_GREEN:CLR_RED);
    print(str("HP %d", pl.hp>0?pl.hp:0), 50, HUD_Y+2, CLR_WHITE);
    print(str("ammo %d", pl.mag), 96, HUD_Y+2, pl.mag?CLR_YELLOW:CLR_RED);
    print(str("down %d/%d", kills, NE), 150, HUD_Y+2, CLR_ORANGE);
    if (pl.sneak) print("SNEAK", 206, HUD_Y+2, CLR_INDIGO);
    print_right(pl.spectate?"SPECTATE":(pl.pinned>0.3f?"PINNED":(known?"SPOTTED":"hidden")), SCREEN_W-4, HUD_Y+2,
                pl.pinned>0.3f?CLR_RED:(known?CLR_ORANGE:CLR_GREEN));
    font(FONT_SMALL);
    if (msg_t>0) print(msg, 4, HUD_Y+12, CLR_LIGHT_PEACH);
    else print("WASD+Shift(sneak)   L-click gun (loud!)   R-click knife (quiet)   H/L/V debug  TAB spectate  R reset", 4, HUD_Y+12, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
