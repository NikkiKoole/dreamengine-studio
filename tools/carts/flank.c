/* de:meta
{
  "title": "flank",
  "status": "active",
  "kind": [
    "tech-demo",
    "game"
  ],
  "teaches": [
    "pathfinding",
    "finite-state-ai",
    "steering-behaviors"
  ],
  "lineage": "Testbed for GTA1/Hotline-Miami enemy brains; novel in combining a Dijkstra flow field (navigation around cover) with a player-aim danger heatmap (flanking cone avoidance) and a shared blackboard with knowledge decay.",
  "genre": "shooter",
  "description": "Top-down tactical AI -- a squad that isn't dumb (a testbed for a GTA1 / Hotline-Miami enemy brain). Four ideas: COMMUNICATION -- a shared blackboard; the instant any enemy sees you it calls your position to the whole squad and blind ones converge (knowledge decays, so break contact and they search your last-known spot). FLOW FIELD -- a Dijkstra map flooded from your last-known cell, so they approach AROUND cover, not into walls. HEATMAP / FLANKING -- your aim projects a danger cone; enemies score firing spots to AVOID the cone, so they peel off and hit you from the sides and behind. COVER -- XCOM-style FULL cover (grey: blocks sight + bullets) and LOW cover (orange crates: shoot OVER them, but a shot from the side they face has ~50% to be absorbed); both sides seek low cover that faces the enemy. THREE ENEMY TYPES with distinct tactics, visible at once (glyph = type, colour = state): R rusher charges in close and sprays, C camper holds cover at long range, F flanker circles to your sides. SUPPRESSION -- campers anchor and pour inaccurate fire to PIN you (you slow to a crawl + your aim shakes) while rushers/flankers maneuver: emergent fire-and-maneuver. STEALTH -- hold Shift to sneak (slower, quieter, lower profile, harder to detect); you have limited vision (fog of war), enemies you can't see leave a dim last-seen ghost, and the squad investigates your last-known spot (?) while you slip away elsewhere. DETECTION is graded -- calm/suspicious/alarmed; a guard hears a NOISE and walks over to check, then gives up and returns. Your loud gun makes noise that draws the room; a silent knife (right-click, close range) does not -- so loud vs quiet is the core stealth choice. A DIFFICULTY PANEL (O) has easy/normal/hard presets + live sliders for squad size, damage, fire rate, accuracy, sight, suppression and healing; pick up green health packs (easy also slowly auto-heals out of combat; hard has neither). WASD move, Shift sneak, mouse aim, L-click gun, R-click knife; O difficulty, H heat, L comms, V reveal-all, TAB spectate, R reset."
}
de:meta */
#include "studio.h"
#include "ui.h"
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
//   * PEACETIME → COMBAT — the room STARTS unaware: patrols with dulled senses.
//     A shot, a confirmed sighting, or a found corpse flips the WHOLE squad to
//     combat time (one-way). So the opening is a stealth phase — slip in, knife
//     the isolated one quietly (no noise), and don't leave bodies on patrol routes.
//
// WASD move · Shift sneak · mouse aim · L-click gun (loud) · R-click knife (quiet:
//   instant takedown on an UNAWARE target; against an alarmed one it only wounds)
// · H heat · L comms · V reveal · TAB spectate · R back to menu
// Design notes + behaviour menu + sloop porting: docs/design/flank-tactical-ai.md

#define GW 40
#define GH 23
#define TILE 8
#define HUD_Y (GH * TILE)          // 184
#define NE 8                        // array size / max squad
#define NB 96
#define NPK 3                       // health packs
#define ND 8                        // ammo drops (one per fallen enemy, reused)
#define MAG_CAP 12                  // player magazine size
#define KNIFE_BACKSTAB 70           // knife instakills a target whose alert is BELOW this (unaware); aware = damage only

// ---- difficulty (live via the panel; sliders are 0..1, mapped to these) ----
static float sl_count=0.66f, sl_dmg=0.30f, sl_rate=0.50f, sl_acc=0.50f, sl_sight=0.45f, sl_supp=0.50f, sl_heal=0.5f, sl_ammo=0.5f;
static float d_dmg, d_gap, d_spread, d_sight, d_supp, d_regen, d_packs;   // derived each frame
static int   ecount = 6, show_panel, d_unlimited;   // d_unlimited: easy ammo tier → magazine never runs dry
// game flow: a slim menu (presets + start) bookends the fight; the full slider
// panel is the "tweak" sub-screen (also reachable mid-fight via O).
enum { PH_START, PH_PLAYING, PH_OVER };
static int phase = PH_START, won, cur_preset = 1;   // cur_preset: 0/1/2 = easy/normal/hard, -1 = custom
static void recompute_difficulty(void) {
    d_dmg    = 2 + sl_dmg * 10;          // 2..12 damage per hit
    d_gap    = 1.8f - sl_rate * 1.2f;    // shot interval mult: 1.8 slow .. 0.6 fast
    d_spread = 1.6f - sl_acc  * 1.15f;   // spread mult: 1.6 loose .. 0.45 tight
    d_sight  = 0.7f + sl_sight* 0.7f;    // sight mult: 0.7 .. 1.4
    d_supp   = sl_supp * 1.6f;           // suppression strength: 0 .. 1.6
    d_packs  = sl_heal >= 0.33f ? 1 : 0; // healing tiers: none (<.33, hard) / packs (.33-.66, normal) / packs+regen (>.66, easy)
    d_regen  = sl_heal >= 0.66f ? 1 : 0;
    d_unlimited = sl_ammo >= 0.66f ? 1 : 0;   // ammo tiers: scarce (<.33, hard) / reserve+drops (.33-.66) / unlimited (>.66, easy)
}
static void set_preset(int p) {          // 0 easy · 1 normal · 2 hard — LETHALITY only (count is the scenario's job)
    if      (p==0) { sl_dmg=.10f; sl_rate=.20f; sl_acc=.12f; sl_sight=.18f; sl_supp=.30f; sl_heal=1.0f; sl_ammo=1.0f; }
    else if (p==1) { sl_dmg=.30f; sl_rate=.50f; sl_acc=.50f; sl_sight=.45f; sl_supp=.50f; sl_heal=0.5f; sl_ammo=0.5f; }
    else           { sl_dmg=.60f; sl_rate=.85f; sl_acc=.90f; sl_sight=.85f; sl_supp=1.0f; sl_heal=0.0f; sl_ammo=0.15f; }
    cur_preset = p;
}
// ---- scenarios: WHAT KIND of fight (orthogonal to difficulty's HOW HARD) ----
// A scenario sets the squad's STARTING POSTURE + the headcount. The posture is the
// keystone: you can only stealth-open a fight the room starts unaware of. A scenario also sets
// the LOADOUT (which weapons the squad carries) — so it sets both posture AND what kind of fight.
enum { ST_ASLEEP, ST_ALERTED, ST_HOT };  // start posture: unaware patrol · searching (knows something's up) · hunting you from frame 0
enum { LD_GUNS, LD_MELEE };              // loadout: bullet weapons · knife/brawl
typedef struct { const char *name; int posture; float count; int loadout; } Scen;
static const Scen SCEN[] = {
    { "fight",  ST_ASLEEP,  0.66f, LD_GUNS  },   // full squad, unaware — open it loud OR stealth, your call
    { "sneak",  ST_ASLEEP,  0.35f, LD_GUNS  },   // sparse patrol, unaware — slip in, knife the isolated one
    { "alarm",  ST_ALERTED, 0.66f, LD_GUNS  },   // alarm's up: full squad SEARCHING the area, but they don't know where you are yet
    { "ambush", ST_HOT,     0.66f, LD_GUNS  },   // they already know — no stealth, gunfight from the first frame
    { "brawl",  ST_HOT,     0.66f, LD_MELEE },   // knife + brawl rush, already on you — a close-quarters melee
};
#define NSCEN ((int)(sizeof SCEN / sizeof SCEN[0]))
static int scenario = 0;
static void set_scenario(int s) { scenario = s; sl_count = SCEN[s].count; }   // posture is applied in reset()
#define INF 1e9f
#define DIAG 1.41421356f

enum { E_PATROL, E_SUSPECT, E_HUNT, E_ENGAGE, E_DOWN };   // graded alertness drives the state
static const int ECOL[5] = { CLR_DARK_GREY, CLR_YELLOW, CLR_ORANGE, CLR_RED, CLR_BLACK };

// WEAPONS — the enemy's weapon drives HOW it fights. The same engage scorer reads these
// numbers and produces a different play style per weapon, no per-weapon AI:
//   range = preferred firing distance (the big lever) · coverW/heatW = cover-pref / cone-fear
//   strafeW = circling · flip = strafe-flip interval (low = jumpy) · pellets = bullets per shot
//   suppress = anchors and sprays to PIN you · react = spot→fire reaction beat
// (melee weapons — knife/brawl — land in the next slice.)
enum { WK_BULLET, WK_MELEE };            // delivery mechanic
enum { W_SMG, W_SHOTGUN, W_MARKSMAN, W_KNIFE, W_BRAWL, NWEAP };
typedef struct { char g; int kind, hp, mag, gap; float range, coverW, heatW, strafeW; int flip; float speed, spread;
                 int pellets, dmg, reach, react, suppress, hat, build; } Weapon;   // dmg/reach: melee only
static const Weapon WEAP[NWEAP] = {
    //  g  kind        hp mag gap range coverW heatW strafeW flip speed spread pel dmg reach react sup  hat            build
    { 's', WK_BULLET,  30, 20, 22,  58,  1.3f,  9.0f, 1.5f,   70, 1.15f, 18,  1,  0,  0, 20, 1, CLR_INDIGO,     3 },  // SMG      — run-and-gun, circles, suppresses
    { 'g', WK_BULLET,  28,  6, 30,  20,  0.5f,  2.0f, 0.6f,   90, 1.45f, 30,  3,  0,  0, 18, 0, CLR_PINK,       7 },  // shotgun  — charges to point-blank, 3 pellets
    { 'm', WK_BULLET,  24, 10, 42, 112,  4.0f,  8.0f, 0.1f,  150, 0.60f, 10,  1,  0,  0, 26, 0, CLR_WHITE,      5 },  // marksman — holds a vantage, still, precise
    { 'k', WK_MELEE,   22, 99, 16,  10,  0.4f,  1.0f, 2.2f,   18, 1.72f,  0,  0, 12, 14, 16, 0, CLR_RED,        3 },  // knife    — jumpy assassin, silent, fast light swings (outruns a fleeing player)
    { 'b', WK_MELEE,   40, 99, 30,  10,  0.3f,  0.8f, 0.4f,   80, 1.10f,  0,  0, 22, 14, 22, 0, CLR_DARK_GREEN, 7 },  // brawl    — steady bruiser, tanky, heavy slow swings
};
// carried-weapon line length (px) drawn from the man's chest toward his aim — a thin
// barrel that varies JUST enough to read the weapon in the world: long rifle for the
// marksman, stubby barrels for SMG/shotgun, a short blade for the knife, nothing for
// the bare-fisted brawler. Indexed by the W_* enum (matches WEAP order).
static const int GUNLEN[NWEAP] = { 6, 7, 11, 3, 0 };   // SMG · shotgun · marksman · knife · brawl

// SEAM — a PERSONALITY layer composes on top of the weapon ("we might want both"): the weapon
// says HOW it fights, a persona says WHO carries it (skill/nerve), scaling the weapon's numbers
// with NO new AI. Today there's one neutral 'regular'; future rows — green conscript (slow react,
// loose), veteran (snappy), zealot (pushes past range), coward (hangs back) — just plug in here
// and modulate. The old rusher/camper/flanker "types" can return as personas, orthogonal to weapon.
typedef struct { const char *name; float skill; float aggro; } Persona;  // skill: faster reaction · aggro: pushes inside its weapon's range (wired later)
static const Persona PERS[] = { { "regular", 1.0f, 1.0f } };
#define NPERS ((int)(sizeof PERS / sizeof PERS[0]))

static unsigned char cell[GH][GW];  // 0 floor · 1 FULL cover (blocks move+LOS+bullets) · 2 LOW cover (blocks move only)
static float flow[GH][GW];          // Dijkstra from last-known player cell
static float heat[GH][GW];          // danger projected by the player's aim cone

#define KEY_LSHIFT 340             // raylib left-shift — held to sneak
#define VIS_R 96                   // player's own vision radius (fog of war)
typedef struct { float x, y, vx, vy, aim, pinned, bob, ox, oy; int hp, mag, reload, reserve, shake, spectate, sneak, calm; } Player;  // pinned 0..1; calm = frames since hit (regen); bob = walk-hop phase, ox/oy = prev pos; reserve = spare rounds (reload pulls from here; 0+empty mag = knife only)
typedef struct { float x, y, aim, lsx, lsy, alert, bob, ox, oy; int hp, alive, state, shootcd, mag, reload, callcd, weapon, persona, strafe, strafeT, suppressing, everseen, invx, invy, react, losgap; } Enemy;  // alert 0..100; inv = spot to investigate; bob = walk-hop phase, ox/oy = prev pos; react = frames left before this enemy fires after spotting you; losgap = frames since it last had a sightline (a real loss re-arms react; brief flicker doesn't)
typedef struct { float x, y, vx, vy; int alive, foe; } Bullet;
typedef struct { int x, y, active, cd; } Pack;     // health pickup (px coords)
typedef struct { float x, y, vx, vy; int life; } Splat;   // blood: sprays along the shot, decelerates, fades & stains
typedef struct { float x, y; int amt, alive, life; } Drop; // ammo a fallen enemy dropped — walk over to add to your reserve
#define NSP 60
static Player pl;
static Enemy en[NE];
static Bullet bul[NB];
static Pack pack[NPK];
static Splat splat[NSP];
static Drop drops[ND];
// spray n blood specks from (x,y) along a hit direction (dx,dy), with scatter
static void spawn_blood(float x, float y, float dx, float dy, int n) {
    float m = sqrtf(dx*dx+dy*dy); if (m<0.01f) m=1; dx/=m; dy/=m;
    for (int k=0; k<n; k++) for (int i=0;i<NSP;i++) if (splat[i].life<=0) {
        float sp = 0.6f + (rnd(100)/100.0f)*1.6f;                 // speck speed
        float jx = (rnd(100)-50)/90.0f, jy = (rnd(100)-50)/90.0f; // sideways scatter
        splat[i] = (Splat){ x, y, (dx+jx)*sp, (dy+jy)*sp, 26 + rnd(22) };
        break;
    }
}
// a fallen enemy drops their leftover rounds (caught full = a fat resupply; caught mid-reload = little).
// Skipped when ammo is unlimited (easy) — nothing to scavenge.
static void spawn_drop(float x, float y, int amt) {
    if (d_unlimited || amt <= 0) return;
    for (int k=0;k<ND;k++) if (!drops[k].alive) { drops[k] = (Drop){ x, y, amt, 1, 480 }; return; }
}

// shared blackboard (the squad's collective knowledge of you)
static float kx, ky; static int known, kage, flow_cx = -1, flow_cy = -1;
static int show_heat = 1, show_comms = 0, reveal, kills, msg_t, tick; static char msg[40];
// squad posture: the room STARTS in peacetime (unaware you're a threat). The first hard
// stimulus — a shot heard, a confirmed sighting, or a comrade's corpse found — flips the
// WHOLE squad to combat time (a one-way latch on top of per-enemy alert). Peacetime = the
// stealth phase: dulled senses, you can slip around and knife the isolated one quietly.
static int combat, combat_t;   // 0 = peacetime · 1 = combat time; combat_t = frames since the flip

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
    pl = (Player){ .x = TILE*3, .y = TILE*(GH/2), .hp = 100, .mag = MAG_CAP, .spectate = pl.spectate };
    pl.reserve = sl_ammo >= 0.66f ? 0 : sl_ammo >= 0.33f ? 36 : MAG_CAP;   // unlimited / 3 spare mags / 1 spare mag
    ecount = 2 + (int)(sl_count * 6 + 0.5f);               // 2..8 enemies (the count slider)
    for (int i=0;i<NE;i++) {
        if (i >= ecount) { en[i].alive = 0; continue; }
        int x,y,tr=0; do { x=GW/2+rnd(GW/2-2); y=2+rnd(GH-4); tr++; } while (wcell(x,y) && tr<80);
        int w = SCEN[scenario].loadout==LD_MELEE ? (W_KNIFE + (i&1))   // melee: alternate knife / brawl
                                                 : (i % 3);            // guns: SMG / shotgun / marksman
        en[i] = (Enemy){ .x = x*TILE+4, .y = y*TILE+4, .aim = 180, .hp = WEAP[w].hp, .alive = 1,
                         .state = E_PATROL, .mag = WEAP[w].mag, .weapon = w, .persona = rnd(NPERS), .losgap = 999,
                         .strafe = rnd(2) ? 1 : -1, .strafeT = rnd(120) };
    }
    for (int i=0;i<NB;i++) bul[i].alive = 0;
    for (int i=0;i<NSP;i++) splat[i].life = 0;
    for (int i=0;i<ND;i++) drops[i].alive = 0;
    int packs_on = sl_heal >= 0.33f;                      // no packs on hard
    for (int i=0;i<NPK;i++) {                              // health packs on open floor
        int x,y,tr=0; do { x=4+rnd(GW-8); y=2+rnd(GH-4); tr++; } while (wcell(x,y) && tr<60);
        pack[i] = (Pack){ x*TILE+4, y*TILE+4, packs_on, 0 };
    }
    known = 0; kage = 999; flow_cx = -1; kills = 0; msg_t = 0; combat_t = 0;
    // apply the scenario's starting posture (pure params on the alert/combat/known model)
    int post = SCEN[scenario].posture;
    combat = (post != ST_ASLEEP);
    if (post == ST_ALERTED) for (int i=0;i<NE;i++) if (en[i].alive)     // a sweep is on: searching, but they don't know where you are
        { en[i].alert = 35; en[i].invx = (int)en[i].x; en[i].invy = (int)en[i].y; }
    if (post == ST_HOT) { known = 1; kx = pl.x; ky = pl.y; kage = 0;    // ambush: converging on your spawn from frame 0
        for (int i=0;i<NE;i++) if (en[i].alive) { en[i].alert = 85; en[i].invx = (int)pl.x; en[i].invy = (int)pl.y; } }
}
void init(void) { colorkey(0); reverb(0.25f, 0.5f); reset(); phase = PH_PLAYING; }   // colorkey 0 = sprite glyphs draw transparent over the field; boot straight into the fight

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

// flip the whole squad from peacetime to combat time (one-way). The room "goes loud":
// alert floor jumps so everyone draws and searches, all pointed at what tripped the alarm.
static void go_combat(float tx, float ty) {
    if (combat) return;
    combat = 1; combat_t = 0;
    setmsg("ALARM - the squad goes loud!");
    play_pan(48, INSTR_REED, 4, panx(tx), 28);                 // alarm horn
    for (int i=0;i<NE;i++) if (en[i].alive) {
        if (en[i].alert < 45) en[i].alert = 45;                // snap everyone to at-least-searching
        en[i].invx = (int)tx; en[i].invy = (int)ty;            // converge on the trigger
    }
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
    const Weapon *W = &WEAP[e->weapon];
    const Persona *P = &PERS[e->persona];  // SEAM: persona composes on top of the weapon (neutral today)
    float prev_alert = e->alert;          // alertness BEFORE this frame's stimuli — drives reaction surprise
    if (e->shootcd > 0) e->shootcd--;
    if (e->callcd > 0) e->callcd--;
    e->suppressing = 0;
    float pd = fsqrt((pl.x-e->x)*(pl.x-e->x) + (pl.y-e->y)*(pl.y-e->y));
    bool canlos = los_px(e->x, e->y, pl.x, pl.y);
    float ss = pl.sneak ? 0.5f : 1.0f, sh = pl.sneak ? 0.3f : 1.0f;
    float peace = combat ? 1.0f : 0.5f;                       // peacetime: senses dialled down — they aren't looking for a threat yet
    bool see   = canlos && pd <= 132 * d_sight * ss * peace;  // spotted by sight (difficulty + sneak + posture scale it)
    bool heard = pd <= 48 * sh * peace;                       // or heard moving nearby
    if (canlos && pd <= VIS_R) { e->everseen = 1; e->lsx = e->x; e->lsy = e->y; }   // YOUR last-seen memory of this enemy

    // --- graded alertness: stimuli RAISE it, time cools it down ---
    if (see)        { e->alert = 100;                                   e->invx=(int)pl.x; e->invy=(int)pl.y; }   // direct sight = instant alarm
    else if (heard) { e->alert = e->alert<55 ? 55 : (e->alert<100?e->alert+3:100); e->invx=(int)pl.x; e->invy=(int)pl.y; }  // a noise nearby = suspicious, rising
    e->alert -= 0.22f; if (e->alert < 0) e->alert = 0;
    if (combat && e->alert < 30) e->alert = 30;               // combat time: the squad stays at least searching (the latch holds)

    // in peacetime, stumbling within sight of a comrade's corpse trips the alarm (hide the bodies!)
    if (!combat) for (int j=0;j<NE;j++) if (!en[j].alive && en[j].state==E_DOWN) {
        float bd = fsqrt((en[j].x-e->x)*(en[j].x-e->x)+(en[j].y-e->y)*(en[j].y-e->y));
        if (bd < 30 && los_px(e->x,e->y,en[j].x,en[j].y)) { go_combat(en[j].x, en[j].y); break; }
    }

    // --- reaction time: a beat between getting a SIGHTLINE on you and opening fire ---
    // Re-armed on every fresh LOS acquisition (so quick-peek / break-contact works), and
    // shorter the more alarmed the enemy already was: a startled patroller gives you ~half a
    // second, a hunter who's been tracking you snaps to the shot. The headline player-skill lever.
    if (canlos) {
        if (e->losgap > 10) e->react = (int)(W->react * (0.25f + 0.75f*(1 - prev_alert/100.0f)) / P->skill);  // fresh sightline → draw a bead (skill = faster; neutral now)
        else if (e->react > 0) e->react--;                                                         // kept the bead (brief flicker is forgiven) → count down
        e->losgap = 0;
    } else e->losgap++;

    if ((see || heard) && e->alert >= 70) {                   // fully alarmed AND fresh contact -> tell the squad
        if (!known && e->callcd <= 0) { play_pan(72, INSTR_REED, 3, panx(e->x), 16); setmsg("enemy: \"contact!\""); e->callcd = 120; }
        known = 1; kx = pl.x; ky = pl.y; kage = 0;
        go_combat(pl.x, pl.y);                                 // a confirmed sighting that sticks = combat time
    }
    // alert level -> behaviour
    if (e->alert >= 70)            e->state = canlos ? E_ENGAGE : E_HUNT;  // alarmed: engage if you can see me, else close in
    else if (known && kage < 300)  e->state = E_HUNT;                      // squad has a FRESH fix on you → keep pursuing (don't lapse to a local search)
    else if (e->alert >= 30)       e->state = E_SUSPECT;                   // suspicious: go check the disturbance
    else                           e->state = E_PATROL;                    // calm: back to wandering ("gave up")

    int kcx = (int)(kx/TILE), kcy = (int)(ky/TILE);
    if (known && (kcx != flow_cx || kcy != flow_cy)) reflow(kcx, kcy);

    // --- act per state ---
    if (e->state == E_ENGAGE) {
        if (W->suppress && los_px(e->x,e->y,pl.x,pl.y)) {   // SUPPRESSION (auto weapons): anchor + pour inaccurate fire to PIN you
            e->aim = angle_to(e->x, e->y, pl.x, pl.y);
            e->suppressing = e->mag > 0 && e->react <= 0;     // not pinning you until the reaction beat is up
            if (e->mag > 0 && e->shootcd <= 0 && e->react <= 0) { fire(e->x, e->y, e->aim, 1, 30*d_spread); e->mag--; e->shootcd = (int)(7*d_gap); if (e->mag==0) e->reload = 100; }
            if (e->mag == 0 && --e->reload <= 0) e->mag = W->mag;  // reload = the gap in the pin (your window to move)
            return;
        }
        // pick a firing stance per type: keep LOS, dodge the heat cone, hold the
        // type's range, hug cover, circle (strafe), spread from squadmates.
        if (--e->strafeT <= 0) { e->strafe = -e->strafe; e->strafeT = W->flip + rnd(W->flip); }   // flip cadence = the weapon's jumpiness
        float perp = angle_to(pl.x, pl.y, e->x, e->y) + 90 * e->strafe;   // lateral-around-player dir
        float best = -1e9f, bx = e->x, by = e->y;
        for (int a=0;a<8;a++) {
            float ang = a*45, ox = e->x + dx(9, ang), oy = e->y + dy(9, ang);
            if (wallpx(ox, oy) || enemy_at_px(ox, oy, i) >= 0) continue;
            int cx=(int)(ox/TILE), cy=(int)(oy/TILE);
            float od = fsqrt((pl.x-ox)*(pl.x-ox)+(pl.y-oy)*(pl.y-oy));
            float sc = los_px(ox,oy,pl.x,pl.y) ? 4 : -7;     // must be able to shoot
            sc -= heat_at(ox,oy) * W->heatW;                 // AVOID the aim cone -> flank
            sc -= fabsf(od - W->range) * 0.05f;              // hold this type's range
            if (near_cover(cx,cy)) sc += W->coverW;          // peek from any cover
            if (low_facing(ox,oy, pl.x-ox, pl.y-oy)) sc += W->coverW + 1.5f;   // LOW cover facing you = protected firing spot
            sc += W->strafeW * cos_deg(angnorm(ang - perp)); // circle the player
            for (int j=0;j<NE;j++) if (j!=i && en[j].alive) { float dd=fsqrt((en[j].x-ox)*(en[j].x-ox)+(en[j].y-oy)*(en[j].y-oy)); if (dd<26) sc -= (26-dd)*0.12f; }
            if (sc > best) { best = sc; bx = ox; by = oy; }
        }
        // hold position ONLY when already safe in cover (campers settle; rushers/
        // flankers stay in motion because they're rarely "safe") — fixes the freeze
        int ecx=(int)(e->x/TILE), ecy=(int)(e->y/TILE);
        bool safe = heat_at(e->x,e->y) < 0.12f && near_cover(ecx,ecy) && los_px(e->x,e->y,pl.x,pl.y) && W->kind==WK_BULLET;  // melee never settles — always closes
        if (!safe) move_enemy(e, bx, by, W->speed);
        e->aim = angle_to(e->x, e->y, pl.x, pl.y);
        if (W->kind == WK_MELEE) {                                    // SWING when in reach (no projectile)
            if (pd < W->reach && e->shootcd <= 0 && e->react <= 0) {
                int dmg = (int)(W->dmg * d_dmg / 8.0f); if (dmg < 1) dmg = 1;   // scales with the difficulty damage knob
                pl.hp -= dmg; pl.shake = 6; pl.calm = 0;
                spawn_blood(pl.x, pl.y, pl.x-e->x, pl.y-e->y, 4);
                play_pan(58, INSTR_MEMBRANE, 3, panx(pl.x), 4); e->shootcd = (int)(W->gap * d_gap);
                if (pl.hp <= 0) { pl.hp = 0; setmsg("cut down. R to retry."); }
            }
        } else if (los_px(e->x,e->y,pl.x,pl.y) && e->shootcd <= 0 && e->mag > 0 && e->react <= 0) {
            for (int p=0;p<W->pellets;p++) fire(e->x, e->y, e->aim, 1, W->spread * d_spread);   // shotgun = a spray of pellets
            e->mag--; e->shootcd = (int)(W->gap * d_gap);
            if (e->mag == 0) e->reload = 70;
        }
        if (e->mag == 0) { if (--e->reload <= 0) e->mag = W->mag; }   // reload window = vulnerable
    } else if (e->state == E_HUNT) {                          // alarmed, no LOS: close in
        if (known) {                                          // squad knows where you are -> follow the flow gradient by CELL (around cover)
            int ex=(int)(e->x/TILE), ey=(int)(e->y/TILE);
            float bestf = flow[ey][ex]; int btx = ex, bty = ey;
            for (int dy=-1;dy<=1;dy++) for (int dx2=-1;dx2<=1;dx2++) {
                if ((!dx2 && !dy) || wcell(ex+dx2, ey+dy)) continue;
                if (dx2 && dy && (wcell(ex+dx2,ey) || wcell(ex,ey+dy))) continue;   // no corner-cutting through walls
                if (flow[ey+dy][ex+dx2] < bestf) { bestf = flow[ey+dy][ex+dx2]; btx = ex+dx2; bty = ey+dy; }
            }
            move_enemy(e, btx*TILE+4, bty*TILE+4, W->speed * 0.92f);   // head to the lowest-flow neighbour cell's centre
        } else move_enemy(e, e->invx, e->invy, W->speed * 0.9f);   // alarmed only by a noise -> head to it
        e->aim = angle_to(e->x, e->y, pl.x, pl.y);
    } else if (e->state == E_SUSPECT) {                       // SEARCH: sweep to a point of interest, then pick another — actively investigating
        float id = fsqrt((e->invx-e->x)*(e->invx-e->x)+(e->invy-e->y)*(e->invy-e->y));
        if (id < 10 || rnd(180)==0) {                         // reached it (or stuck/bored) → check a fresh nearby spot
            int rx,ry,tr=0; do { rx=(int)(e->x/TILE)+rnd(20)-10; ry=(int)(e->y/TILE)+rnd(20)-10;
                rx=rx<2?2:rx>GW-2?GW-2:rx; ry=ry<2?2:ry>GH-2?GH-2:ry; tr++; } while (wcell(rx,ry) && tr<40);
            e->invx = rx*TILE+4; e->invy = ry*TILE+4;
        }
        move_enemy(e, e->invx, e->invy, W->speed * 0.55f);    // cautious
        e->aim = (rnd(28)==0) ? rnd(360) : angle_to(e->x, e->y, e->invx, e->invy);   // glance around as they go
    } else {                                                  // PATROL: small idle drift
        if (rnd(40)==0) e->aim = rnd(360);
        move_enemy(e, e->x+dx(8,e->aim), e->y+dy(8,e->aim), 0.25f);
    }
}

// ---- player ----------------------------------------------------------------
static void player_update(void) {
    if (pl.spectate) {                                        // autopilot: kite the nearest visible enemy
        // ammo-aware: when running low, break off to scavenge the nearest dropped clip (pickup happens in update())
        int dr=-1; float dd=1e9f; for (int k=0;k<ND;k++) if (drops[k].alive) { float d=(drops[k].x-pl.x)*(drops[k].x-pl.x)+(drops[k].y-pl.y)*(drops[k].y-pl.y); if (d<dd){dd=d;dr=k;} }
        bool low = !d_unlimited && (pl.mag + pl.reserve) < 6;
        if (low && dr>=0) {                                   // go get ammo
            pl.aim = angle_to(pl.x,pl.y,drops[dr].x,drops[dr].y);
            float mx=dx(1.5f,pl.aim), my=dy(1.5f,pl.aim);
            if (!wallpx(pl.x+mx,pl.y)) pl.x+=mx; if (!wallpx(pl.x,pl.y+my)) pl.y+=my;
        } else {
            int t=-1; float bd=1e9f;
            for (int i=0;i<NE;i++) if (en[i].alive && los_px(pl.x,pl.y,en[i].x,en[i].y)) { float d=(en[i].x-pl.x)*(en[i].x-pl.x)+(en[i].y-pl.y)*(en[i].y-pl.y); if (d<bd){bd=d;t=i;} }
            if (t>=0) { pl.aim = angle_to(pl.x,pl.y,en[t].x,en[t].y);   // visible: kite + fire
                float perp = pl.aim+90, mx = dx(1.4f,perp)+dx(0.6f,pl.aim+180), my = dy(1.4f,perp)+dy(0.6f,pl.aim+180);
                if (!wallpx(pl.x+mx,pl.y)) pl.x+=mx; if (!wallpx(pl.x,pl.y+my)) pl.y+=my;
                if (pl.mag>0 && (tick%9==0)) { fire(pl.x,pl.y,pl.aim,0,6); pl.mag--; if(pl.mag==0) pl.reload=40; noise_at(pl.x,pl.y,260,70); go_combat(pl.x,pl.y); } }
            else {                                                  // none visible: advance to make contact
                int n=-1; float nd=1e9f;
                for (int i=0;i<NE;i++) if (en[i].alive) { float d=(en[i].x-pl.x)*(en[i].x-pl.x)+(en[i].y-pl.y)*(en[i].y-pl.y); if (d<nd){nd=d;n=i;} }
                if (n>=0) { pl.aim = angle_to(pl.x,pl.y,en[n].x,en[n].y);
                    float mx=dx(1.5f,pl.aim), my=dy(1.5f,pl.aim);
                    if (!wallpx(pl.x+mx,pl.y)) pl.x+=mx; if (!wallpx(pl.x,pl.y+my)) pl.y+=my; }
            }
        }
        if (pl.reload>0) pl.reload--;                         // honour the reserve, same as the player (no more infinite mag)
        if (pl.mag==0 && pl.reload<=0) { if (d_unlimited) pl.mag=MAG_CAP; else if (pl.reserve>0) { int load=pl.reserve<MAG_CAP?pl.reserve:MAG_CAP; pl.mag=load; pl.reserve-=load; } }
        return;
    }
    pl.sneak = key(KEY_LSHIFT);                               // hold Shift = sneak: slower but quiet + low profile
    float spd = 1.6f * (pl.sneak ? 0.5f : 1.0f) * (1 - 0.45f*pl.pinned);   // sneak + suppression both slow you
    float mx=0,my=0;
    if (key('A')||key(KEY_LEFT)) mx=-spd; else if (key('D')||key(KEY_RIGHT)) mx=spd;
    if (key('W')||key(KEY_UP)) my=-spd; else if (key('S')||key(KEY_DOWN)) my=spd;
    if (!wallpx(pl.x+mx, pl.y)) pl.x += mx; if (!wallpx(pl.x, pl.y+my)) pl.y += my;
    pl.aim = angle_to(pl.x, pl.y, mouse_x(), mouse_y());
    if ((mouse_down(0)||key(KEY_SPACE)) && pl.mag>0 && pl.reload<=0) {        // LOUD gun
        fire(pl.x,pl.y,pl.aim,0,5 + pl.pinned*11); pl.mag--; pl.reload=8; if(pl.mag==0) pl.reload=45;   // suppressed = your aim shakes too
        noise_at(pl.x, pl.y, 260, 70);                                          // a gunshot carries across most of the room — nearby enemies come look on one shot, farther ones after a few
        go_combat(pl.x, pl.y);                                                  // going loud ends the stealth phase for the whole room
    }
    if (mouse_pressed(1)) {                                                   // QUIET knife — silent, no noise
        for (int j=0;j<NE;j++) if (en[j].alive) {
            float d = fsqrt((en[j].x-pl.x)*(en[j].x-pl.x)+(en[j].y-pl.y)*(en[j].y-pl.y));
            if (d < 13) {
                if (en[j].alert < KNIFE_BACKSTAB) {                          // UNAWARE → silent takedown (the stealth kill)
                    spawn_blood(en[j].x, en[j].y, en[j].x-pl.x, en[j].y-pl.y, 10); spawn_drop(en[j].x, en[j].y, en[j].mag);
                    en[j].alive=0; en[j].state=E_DOWN; kills++; pl.shake=3; play_pan(64,INSTR_MEMBRANE,1,panx(en[j].x),4);
                } else {                                                     // ALARMED → it's a knife FIGHT: damage, not a free kill
                    en[j].hp -= WEAP[W_KNIFE].dmg; spawn_blood(en[j].x, en[j].y, en[j].x-pl.x, en[j].y-pl.y, 5); pl.shake=3; play_pan(60,INSTR_MEMBRANE,2,panx(en[j].x),4);
                    if (en[j].hp <= 0) { spawn_blood(en[j].x, en[j].y, en[j].x-pl.x, en[j].y-pl.y, 12); spawn_drop(en[j].x, en[j].y, en[j].mag); en[j].alive=0; en[j].state=E_DOWN; kills++; play_pan(40,INSTR_REED,3,panx(en[j].x),18); }
                }
                break;
            }
        }
    }
    if (pl.reload>0) pl.reload--;
    if (pl.mag==0 && pl.reload<=0) {                          // reload: pull a fresh magazine from reserve
        if (d_unlimited) pl.mag = MAG_CAP;
        else if (pl.reserve > 0) { int load = pl.reserve < MAG_CAP ? pl.reserve : MAG_CAP; pl.mag = load; pl.reserve -= load; }
        // else dry: mag stays 0 → gun is silent, knife only until you scavenge a drop
    }
}

void update(void) {
    voices_tick();
    tick++;
    if (msg_t>0) msg_t--;
    if (keyp('R')) { phase = PH_START; return; }              // R = back to the main menu (re-pick difficulty there)
    if (keyp('H')) show_heat = !show_heat;
    if (keyp('L')) show_comms = !show_comms;
    if (keyp('V')) reveal = !reveal;
    if (keyp(KEY_TAB)) { pl.spectate = !pl.spectate; }
    recompute_difficulty();
    if (show_panel) return;                                   // sim paused while you tune (widgets live in draw)
    if (phase != PH_PLAYING) return;                          // start / game-over menu up: sim frozen behind it
    if (pl.hp <= 0) { phase = PH_OVER; won = 0; return; }     // you died → end screen

    if (known) kage++;
    if (combat) combat_t++;
    compute_heat();
    player_update();
    for (int i=0;i<NE;i++) enemy_update(i);

    // walk-hop phase: advance while a body is actually moving, reset when still (purely cosmetic)
    { float md = fabsf(pl.x-pl.ox)+fabsf(pl.y-pl.oy); pl.bob = md>0.2f ? pl.bob+0.5f : 0; pl.ox=pl.x; pl.oy=pl.y; }
    for (int i=0;i<NE;i++) { float md = fabsf(en[i].x-en[i].ox)+fabsf(en[i].y-en[i].oy);
        en[i].bob = (en[i].alive && md>0.2f) ? en[i].bob+0.5f : 0; en[i].ox=en[i].x; en[i].oy=en[i].y; }

    // suppression: are you pinned right now? (any suppressor with LOS to you)
    int sup = 0;
    for (int i=0;i<NE;i++) if (en[i].alive && en[i].suppressing && los_px(en[i].x,en[i].y,pl.x,pl.y)) sup = 1;
    pl.pinned += sup ? 0.05f * d_supp : -0.035f;
    if (pl.pinned < 0) pl.pinned = 0; if (pl.pinned > 1) pl.pinned = 1;
    if (pl.pinned > 0.3f && tick%3==0) pl.shake = 2;          // pinned = jittery

    // health packs — walk over an active one to heal (normal+easy; none on hard)
    if (d_packs) for (int i=0;i<NPK;i++) {
        if (!pack[i].active) { if (--pack[i].cd <= 0) pack[i].active = 1; continue; }   // respawns
        if (pl.hp < 100 && fabsf(pack[i].x-pl.x) < 7 && fabsf(pack[i].y-pl.y) < 7) {
            pl.hp = pl.hp + 35 > 100 ? 100 : pl.hp + 35; pack[i].active = 0; pack[i].cd = 720;
            play_pan(79, INSTR_MALLET, 4, panx(pl.x), 12);
        }
    }
    pl.calm++;                                                // EASY only: slow auto-heal out of combat
    if (d_regen > 0.5f && pl.calm > 120 && pl.hp > 0 && pl.hp < 100 && tick%14==0) pl.hp++;

    // ammo drops — walk over a fallen enemy's rounds to top up your reserve (they blink out before expiring)
    for (int k=0;k<ND;k++) if (drops[k].alive) {
        if (--drops[k].life <= 0) { drops[k].alive = 0; continue; }
        if (fabsf(drops[k].x-pl.x) < 7 && fabsf(drops[k].y-pl.y) < 7) {
            pl.reserve += drops[k].amt; if (pl.reserve > 96) pl.reserve = 96; drops[k].alive = 0;
            setmsg(str("+%d ammo", drops[k].amt)); play_pan(76, INSTR_MALLET, 3, panx(pl.x), 8);
        }
    }

    // bullets
    for (int i=0;i<NB;i++) {
        if (!bul[i].alive) continue;
        bul[i].x += bul[i].vx; bul[i].y += bul[i].vy;
        if (opaquepx(bul[i].x, bul[i].y)) { bul[i].alive = 0; continue; }   // full cover stops it; flies over low cover
        if (bul[i].foe) {
            if (fabsf(bul[i].x-pl.x)<4 && fabsf(bul[i].y-pl.y)<4) {
                bul[i].alive=0;
                if (low_facing(pl.x,pl.y,-bul[i].vx,-bul[i].vy) && rnd(2)==0) play_pan(56,INSTR_MEMBRANE,2,panx(pl.x),4);  // crate ate it
                else { pl.hp -= (int)d_dmg; spawn_blood(pl.x, pl.y, bul[i].vx, bul[i].vy, 4); pl.shake=6; pl.calm=0; play_pan(34,INSTR_NOISE,4,panx(pl.x),6); if (pl.hp<=0){ pl.hp=0; setmsg("you are down. R to retry."); } }
            }
        } else for (int j=0;j<NE;j++) if (en[j].alive && fabsf(bul[i].x-en[j].x)<4 && fabsf(bul[i].y-en[j].y)<4) {
            bul[i].alive=0;
            if (low_facing(en[j].x,en[j].y,-bul[i].vx,-bul[i].vy) && rnd(2)==0) { play_pan(56,INSTR_MEMBRANE,2,panx(en[j].x),4); break; }  // covered
            en[j].hp -= 12; spawn_blood(en[j].x, en[j].y, bul[i].vx, bul[i].vy, 5); play_pan(60,INSTR_MEMBRANE,3,panx(en[j].x),5);
            if (en[j].hp<=0) { spawn_blood(en[j].x, en[j].y, bul[i].vx, bul[i].vy, 12); spawn_drop(en[j].x, en[j].y, en[j].mag); en[j].alive=0; en[j].state=E_DOWN; kills++; play_pan(40,INSTR_REED,3,panx(en[j].x),18); }
            break;
        }
    }
    if (pl.shake>0) pl.shake--;

    // blood specks: drift along the shot, decelerate to a stain, fade out
    for (int i=0;i<NSP;i++) if (splat[i].life>0) {
        splat[i].x += splat[i].vx; splat[i].y += splat[i].vy;
        splat[i].vx *= 0.82f; splat[i].vy *= 0.82f;
        if (opaquepx(splat[i].x, splat[i].y)) { splat[i].vx = splat[i].vy = 0; }  // stops at a wall
        splat[i].life--;
    }

    // win: the whole squad is down → end screen
    { int a=0; for(int i=0;i<NE;i++) if(en[i].alive) a++; if(a==0){ phase=PH_OVER; won=1; } }

#ifdef DE_TRACE
    int alive=0,eng=0,reacting=0,searching=0; for(int i=0;i<NE;i++) if(en[i].alive){alive++; if(en[i].state==E_SUSPECT)searching++; if(en[i].state==E_ENGAGE){eng++; if(en[i].react>0)reacting++;}}
    watch("hp","%d",pl.hp); watch("alive","%d",alive); watch("engaging","%d",eng);
    watch("reacting","%d",reacting); watch("known","%d",known); watch("kills","%d",kills);
    watch("combat","%d",combat); watch("searching","%d",searching);
    watch("mag","%d",pl.mag); watch("reserve","%d",pl.reserve);
    { int nd=0; for(int k=0;k<ND;k++) if(drops[k].alive) nd++; watch("drops","%d",nd); }
#endif
}

// ---- draw ------------------------------------------------------------------
// a hop curve (rest → up → peak → down) sampled by the walk-bob phase
static const int HOPS[4] = { 0, 1, 2, 1 };
// a little upright pixel-man (druglord-style), feet planted at (x,y) screen px.
// shirt = role/state colour, cap + build = weapon marker, hop lifts the body, ghost = dim last-seen.
// build = body width (the weapon's WEAP.build; 5 for the player); hat = cap colour, -1 = none.
static void draw_man(int x, int y, float aim, int shirt, int hat, int hop, int ghost, int build, int gun) {
    int skin = ghost ? CLR_DARKER_GREY : CLR_PEACH;
    int pant = ghost ? CLR_DARKER_GREY : CLR_DARK_BLUE;
    if (ghost) { shirt = CLR_DARKER_GREY; hat = -1; }
    int tw = build;                                            // body width = weapon build (bulky shotgunner · lean SMG)
    rectfill(x-2, y+2, 5, 2, CLR_BLACK);                       // planted ground shadow (stays put as he hops)
    int t = y - 5 - hop;                                        // crown of the head, lifted by the hop
    int stride = (hop >= 2);                                     // legs split at the top of the bounce
    rectfill(x-2, t+5, 2, 2 - stride, pant);                    // left leg
    rectfill(x+1, t+5, 2, 2 - !stride, pant);                   // right leg
    rectfill(x-tw/2, t+2, tw, 3, shirt);                        // torso (role/state colour; width = type build)
    rectfill(x-1, t,   3, 2, skin);                             // head
    if (hat >= 0) { int cw = build>=7 ? 5 : 3; rectfill(x-cw/2, t-1, cw, 1, hat); }   // cap (wide build = wide helmet)
    if (!ghost && gun > 0) line(x, t+3, x + (int)dx(gun, aim), (t+3) + (int)dy(gun, aim), CLR_LIGHT_GREY);  // carried barrel toward aim (length = weapon)
}

void draw(void) {
    cls(CLR_BLACK);
    int sh = pl.shake>0 ? rnd(3)-1 : 0;
    rectfill_rgb(0, 0, SCREEN_W, HUD_Y, 0x1a1c26);                                 // a floor tone (so walls read as walls)
    // heat overlay (danger cone) — dithered so it reads as a translucent zone, not solid
    if (show_heat) { fillp(FILL_CHECKER, -1);
        for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) if (heat[y][x] > 0.05f) {
            int h = heat[y][x] > 0.6f ? CLR_RED : heat[y][x] > 0.3f ? CLR_ORANGE : CLR_BROWN;
            rectfill(x*TILE+sh, y*TILE, TILE, TILE, h);
        }
        fillp_reset();
    }
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++) {
        int px0=x*TILE+sh, py0=y*TILE;
        if (cell[y][x]==1) {                                                                 // FULL cover — a slab with faked height (¾ read)
            rectfill(px0, py0, TILE, TILE, CLR_LIGHT_GREY);
            if (y==0    || cell[y-1][x]!=1) rectfill(px0, py0, TILE, 1, CLR_WHITE);           // lit top edge
            if (y==GH-1 || cell[y+1][x]!=1) rectfill(px0, py0+TILE-3, TILE, 3, CLR_DARK_GREY);// shaded front face = the cue it stands up
            if (x==0    || cell[y][x-1]!=1) rectfill(px0, py0, 1, TILE, CLR_MEDIUM_GREY);     // side edges
            if (x==GW-1 || cell[y][x+1]!=1) rectfill(px0+TILE-1, py0, 1, TILE, CLR_MEDIUM_GREY);
        } else if (cell[y][x]==2) {                                                          // LOW cover — a crate with a lit lid
            rectfill(px0+1, py0+2, TILE-2, TILE-3, CLR_BROWN);
            rectfill(px0+1, py0+2, TILE-2, 1, CLR_LIGHT_PEACH);                               // lid catches light
            rect(px0+1, py0+2, TILE-2, TILE-3, CLR_ORANGE);
        } else if (((x*5 + y*3) & 3) == 0) {                                                 // floor speckle — a little ground texture for the men
            rectfill_rgb(px0 + (x*3 + y) % TILE, py0 + (y*2 + x*7) % TILE, 1, 1, 0x24283a);
        }
    }
    // blood on the floor (drawn under the men): fresh = red, fading to a dark stain
    for (int i=0;i<NSP;i++) if (splat[i].life>0) {
        int c = splat[i].life>34 ? CLR_RED : splat[i].life>16 ? CLR_DARK_PURPLE : CLR_BROWN;
        pset((int)splat[i].x+sh, (int)splat[i].y, c);
    }

    // health packs (a green + cross) — always visible so you can run for one
    for (int i=0;i<NPK;i++) if (d_packs && pack[i].active) { int x=pack[i].x+sh, y=pack[i].y;
        rectfill(x-3,y-1,7,3,CLR_GREEN); rectfill(x-1,y-3,3,7,CLR_GREEN); pset(x,y,CLR_WHITE); }

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

    // corpses — a slumped body where each kill fell (in peacetime, a patroller who sees one
    // trips the alarm, so leaving bodies in patrol routes is risky; drawn under the living)
    for (int i=0;i<NE;i++) if (!en[i].alive && en[i].state==E_DOWN) {
        int x=(int)en[i].x+sh, y=(int)en[i].y;
        rectfill(x-3, y, 7, 2, CLR_DARK_PURPLE); rectfill(x-2, y-1, 3, 1, CLR_PEACH);   // body + a pale head
    }
    // ammo drops — a little clip of rounds (blinks as it nears expiry, so you know to grab it)
    for (int k=0;k<ND;k++) if (drops[k].alive && (drops[k].life > 90 || blink(4))) {
        int x=(int)drops[k].x+sh, y=(int)drops[k].y;
        rectfill(x-2, y-1, 5, 3, CLR_YELLOW); rectfill(x-2, y-1, 5, 1, CLR_LIGHT_PEACH);
    }

    // enemies — FOG OF WAR: draw those you can see; a dim ghost where you last saw the rest
    for (int i=0;i<NE;i++) {
        if (!en[i].alive) continue;
        float ed = fsqrt((en[i].x-pl.x)*(en[i].x-pl.x)+(en[i].y-pl.y)*(en[i].y-pl.y));
        bool vis = reveal || pl.spectate || (ed <= VIS_R && los_px(pl.x,pl.y,en[i].x,en[i].y));
        if (vis) { int x=(int)en[i].x+sh, y=(int)en[i].y, hop=HOPS[((int)en[i].bob)&3];
            if (en[i].suppressing) circ(x,y-4,5,blink(4)?CLR_RED:CLR_ORANGE);    // muzzle bloom of a suppressor
            if (en[i].state==E_ENGAGE && en[i].react>0 && blink(4))              // drawing a bead — your window to break LOS / duck behind cover
                line(x, y-4, (int)(en[i].x+dx(11,en[i].aim))+sh, (y-4)+(int)dy(11,en[i].aim), CLR_ORANGE);
            draw_man(x, y, en[i].aim, ECOL[en[i].state], WEAP[en[i].weapon].hat, hop, 0, WEAP[en[i].weapon].build, GUNLEN[en[i].weapon]);  // shirt = state, cap+build+barrel = weapon
            if (en[i].state==E_SUSPECT) print("?", x-1, y-13, CLR_YELLOW);                    // investigating
            else if (en[i].state>=E_HUNT && en[i].state<=E_ENGAGE) print("!", x-1, y-13, CLR_RED);  // alarmed
        } else if (en[i].everseen)                                            // last-seen ghost (your memory)
            draw_man((int)en[i].lsx+sh, (int)en[i].lsy, en[i].aim, CLR_DARKER_GREY, -1, 0, 1, WEAP[en[i].weapon].build, 0);
    }
    // your vision ring + you (a blue ring keeps you findable in the chaos)
    if (!reveal && !pl.spectate) circ((int)pl.x+sh, (int)pl.y, VIS_R, CLR_DARK_GREY);
    int px=(int)pl.x+sh, py=(int)pl.y, phop=HOPS[((int)pl.bob)&3];
    draw_man(px, py, pl.aim, pl.hp>0 ? (pl.sneak?CLR_DARK_BLUE:CLR_BLUE) : CLR_DARK_GREY, CLR_WHITE, phop, 0, 5, 6);   // player = normal build, pistol-length barrel
    line(px, py-2-phop, (int)(pl.x+dx(8,pl.aim))+sh, (py-2-phop)+(int)dy(8,pl.aim), CLR_YELLOW);  // your aim line (longer, so "you" reads)
    if (pl.pinned > 0.3f) { rect(0,0,SCREEN_W,HUD_Y,CLR_RED); print("PINNED", px-11, py-15, blink(6)?CLR_RED:CLR_ORANGE); }
    if (combat && combat_t < 50 && blink(5)) { rect(0,0,SCREEN_W,HUD_Y,CLR_RED); print_centered("! ALARM !", SCREEN_W/2, 5, CLR_RED); }  // the moment the room goes loud
    if (!d_unlimited && pl.mag==0 && pl.reserve==0 && pl.hp>0 && !pl.spectate)   // dry → forced knife play
        print("OUT-KNIFE!", px-18, py-15, blink(8)?CLR_RED:CLR_YELLOW);

    // HUD — small sprite glyphs (heart/clip/skull) stand in for the HP/ammo/kills labels
    rectfill(0,HUD_Y,SCREEN_W,SCREEN_H-HUD_Y,CLR_DARKER_BLUE);
    spr(18, 3, HUD_Y+2);                                               // heart
    rect(11,HUD_Y+3,40,6,CLR_DARK_GREY); rectfill(12,HUD_Y+4,38*(pl.hp>0?pl.hp:0)/100,4,pl.hp>30?CLR_GREEN:CLR_RED);
    font(FONT_SMALL);                                                   // compact so the row never collides
    print(str("%d", pl.hp>0?pl.hp:0), 53, HUD_Y+3, CLR_WHITE);
    spr(19, 70, HUD_Y+2);                                              // ammo clip
    if (d_unlimited) print(str("%d", pl.mag), 79, HUD_Y+3, pl.mag?CLR_YELLOW:CLR_RED);
    else             print(str("%d|%d", pl.mag, pl.reserve), 79, HUD_Y+3, (pl.mag||pl.reserve)?CLR_YELLOW:CLR_RED);
    spr(20, 114, HUD_Y+2);                                             // skull (kills)
    print(str("%d/%d", kills, ecount), 123, HUD_Y+3, CLR_ORANGE);
    if (pl.sneak) print("SNEAK", 152, HUD_Y+3, CLR_INDIGO);
    print_right(pl.spectate?"SPECTATE":(pl.pinned>0.3f?"PINNED":(!combat?"hidden":(known?"SPOTTED":"ALERT"))), SCREEN_W-4, HUD_Y+3,
                pl.pinned>0.3f?CLR_RED:(!combat?CLR_GREEN:(known?CLR_RED:CLR_ORANGE)));
    font(FONT_TINY);                                                    // controls line: tiny so it fits in one row
    if (msg_t>0) print(msg, 4, HUD_Y+10, CLR_LIGHT_PEACH);
    else print("WASD move  Shift sneak  LMB gun  RMB knife  HLV dbg  TAB spec  R menu", 4, HUD_Y+10, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // ---- difficulty panel (from the menu's "tweak difficulty") — widgets are immediate-mode ui.h ----
    if (show_panel) {
        fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();   // dim
        int X=46, Y=6, W=228;
        rrectfill(X-6, 0, W+12, 198, 4, CLR_DARKER_BLUE); rect(X-6, 0, W+12, 198, CLR_LIGHT_GREY);
        ui_begin();
        // row 1 — SCENARIO (what kind of fight): icon+name tiles. Sets starting posture + headcount.
        print("SCENARIO", X, Y, CLR_WHITE);
        int sxw = (W - (NSCEN-1)*6) / NSCEN;                 // tiles across, 6px gaps
        for (int s=0;s<NSCEN;s++) { int bx = X + s*(sxw+6);
            if (ui_button(bx, Y+9, sxw, 27, "")) set_scenario(s);    // empty box = the hit area; icon+name drawn over it
            spr(24+s, bx + sxw/2 - 8, Y+9);                          // game-mode icon, centred at the top
            font(FONT_SMALL); print_centered(SCEN[s].name, bx + sxw/2, Y+27, CLR_LIGHT_GREY); font(FONT_NORMAL);
            if (scenario==s) rect(bx-1, Y+8, sxw+2, 29, CLR_YELLOW); // active mode framed
        }
        // row 2 — DIFFICULTY (how hard): lethality presets + sliders, each with its own glyph
        print("DIFFICULTY", X, Y+44, CLR_WHITE);
        int pxs[3]={X,X+62,X+132}, pw[3]={56,64,56}; const char *pn[3]={"easy","normal","hard"};
        for (int p=0;p<3;p++) { if (ui_button(pxs[p], Y+55, pw[p], 13, pn[p])) set_preset(p);
            if (cur_preset==p) rect(pxs[p]-1, Y+54, pw[p]+2, 15, CLR_YELLOW); }
        int sy = Y+74, slx = X+10, slw = W-10;               // sliders indented to leave a left-edge glyph; drag → "custom"
        spr(32, X, sy+2); if (ui_slider(&sl_dmg,   slx, sy, slw, "damage"))      cur_preset=-1;  sy += 14;
        spr(33, X, sy+2); if (ui_slider(&sl_rate,  slx, sy, slw, "fire rate"))   cur_preset=-1;  sy += 14;
        spr(34, X, sy+2); if (ui_slider(&sl_acc,   slx, sy, slw, "accuracy"))    cur_preset=-1;  sy += 14;
        spr(35, X, sy+2); if (ui_slider(&sl_sight, slx, sy, slw, "sight"))       cur_preset=-1;  sy += 14;
        spr(36, X, sy+2); if (ui_slider(&sl_supp,  slx, sy, slw, "suppression")) cur_preset=-1;  sy += 14;
        spr(37, X, sy+2); if (ui_slider(&sl_ammo,  slx, sy, slw, "ammo (scarce/reserve/unlimited)")) cur_preset=-1;  sy += 14;
        spr(38, X, sy+2); if (ui_slider(&sl_heal,  slx, sy, slw, "healing (none/packs/+regen)")) cur_preset=-1;  sy += 16;
        if (ui_button(X,     sy, 116, 16, phase==PH_OVER ? "apply + restart" : "apply + start")) { reset(); phase=PH_PLAYING; show_panel=0; }
        if (ui_button(X+126, sy, 62,  16, "back"))                                                 show_panel=0;
        ui_end();                                            // resolve presses → clicks (without this, only hover works)
    } else if (phase != PH_PLAYING) {
        // ---- slim bookend menu: presets + start; "tweak" opens the full panel above ----
        fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();   // dim the field
        int X=96, W=128, Y=64;
        rrectfill(X-12, Y-40, W+24, 152, 4, CLR_DARKER_BLUE); rect(X-12, Y-40, W+24, 152, CLR_LIGHT_GREY);
        if (phase==PH_OVER) {
            spr(won ? 16 : 17, SCREEN_W/2 - 50, Y-41);   // heart on a clear / skull on a death
            print_centered(won ? "CLEARED" : "YOU DIED", SCREEN_W/2, Y-34, won?CLR_GREEN:CLR_RED);
            print_centered(str("%d / %d down", kills, ecount), SCREEN_W/2, Y-22, CLR_LIGHT_GREY);
        } else {
            print_centered("FLANK", SCREEN_W/2, Y-34, CLR_WHITE);
            print_centered("tactical squad AI", SCREEN_W/2, Y-22, CLR_MEDIUM_GREY);
        }
        print_centered(str("scenario: %s  (tweak to change)", SCEN[scenario].name), SCREEN_W/2, Y-10, CLR_INDIGO);
        ui_begin();
        const char *pn[3] = {"easy","normal","hard"}; int pw[3] = {40,44,40}, pxs[3] = {X, X+44, X+92};
        for (int p=0;p<3;p++) {                              // active preset gets a yellow frame
            if (ui_button(pxs[p], Y, pw[p], 16, pn[p])) set_preset(p);
            if (cur_preset==p) rect(pxs[p]-1, Y-1, pw[p]+2, 18, CLR_YELLOW);
        }
        if (ui_button(X, Y+24, W, 18, phase==PH_OVER ? "restart" : "start")) { reset(); phase=PH_PLAYING; }
        if (ui_button(X, Y+46, W, 16, "tweak difficulty")) show_panel = 1;
        ui_end();
        // arsenal + gear legend — the pixel-art sprites showcase what the squad carries
        // (5 weapons, slot == W_*) and what you can scavenge (medkit + ammo clip).
        int ly = Y+76;
        print_centered("arsenal + field gear", SCREEN_W/2, ly-9, CLR_MEDIUM_GREY);
        for (int w=0;w<NWEAP;w++) spr(w, X + 2 + w*16, ly);
        spr(8, X + 90, ly); spr(9, X + 106, ly);
    }
}
