/* de:meta
{
  "title": "mass effect",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "dialogue-tree",
    "camera-follow",
    "state-machine"
  ],
  "lineage": "Mass Effect distilled to its hub-map-mission-dialogue loop; novel in implementing the radial dialogue wheel with morality accumulation that forks the ending, across three distinct runtime scenes.",
  "genre": "rpg",
  "homage": "Mass Effect (2007)",
  "description": "A space opera distilled to its loop. Command the SSV Normandy from the bridge: chat with your crew through a paragon/renegade DIALOGUE WHEEL, chart a course on the GALAXY MAP, then drop into a top-down cover-shooter. Move and aim twin-stick while two AI squadmates flank and fire; duck beside crates and walls that genuinely block bullets AND line-of-sight (real cover), regen your shield between firefights, and unleash a biotic blast that slows time and flings the swarm. Clear the husk waves, then break the Reaper Core. The crowd-cart trick runs the whole cast — ONE fighter sprite, recoloured with pal() into the hero, both squadmates and every enemy — and each planet is its own tile theme. A final wheel choice (spare / disable / execute), weighed against the morality you built on the bridge, forks the ending. HUB/MAP/DIALOGUE: mouse + Z (X backs out of the map). MISSION: WASD move, mouse aim, Z or left-click fire, X biotic blast."
}
de:meta */
#include "studio.h"

// ── MASS EFFECT — a space opera distilled to its loop ──────────────────────
// Command the Normandy: talk to your crew on the bridge, chart a course on the
// galaxy map, then drop into a top-down cover-shooter mission with squadmates,
// regenerating shields and a biotic blast. A dialogue-wheel choice at the boss
// forks the ending paragon vs renegade.
//
//   HUB:      WASD walk · Z talk to crew / use the galaxy table
//   MAP:      ←/→ (A/D) pick a planet · Z launch · X back
//   MISSION:  WASD move · mouse aim · Z / left-click fire · X biotic blast
//             (stand by a crate or wall to take COVER — it blocks fire & sight)
//   DIALOGUE: ←/→ choose · Z select
//
// Engine features leaned on: pal() magic-color recolor (one fighter sprite →
// hero, two squadmates, every enemy), a real tile map() for both scenes built
// at runtime with mset(), camera follow, an entity pool + wave loop, line-of-
// sight cover, slow-mo + shake + flash juice, and a synth bed per scene.

// ── scenes ─────────────────────────────────────────────────────────────────
enum { SC_HUB, SC_MAP, SC_MISSION, SC_DIALOGUE, SC_END };
static int scene = SC_HUB;

// ── tiles (also the sprite-sheet slots they draw from) ──────────────────────
#define T_BFLOOR    16
#define T_BWALL     17
#define T_CONSOLE   18
#define T_TERMINAL  19
#define T_MFLOOR    20
#define T_MWALL     21
#define T_COVER     22
// per-planet environment variants (map() ignores pal(), so theme by tile slot)
#define T_MFLOOR2   23
#define T_MFLOOR3   24
#define T_MWALL2    25
#define T_MWALL3    26
static const int FLOOR_SLOT[3] = { T_MFLOOR, T_MFLOOR2, T_MFLOOR3 };
static const int WALL_SLOT[3]  = { T_MWALL,  T_MWALL2,  T_MWALL3 };

// ── magic recolor indices (the crowd-cart trick) ────────────────────────────
#define MAGIC_ARMOR  28
#define MAGIC_ACCENT 29

// ── scene dimensions ────────────────────────────────────────────────────────
#define BRIDGE_W 20
#define BRIDGE_H 12
#define MISSION_W 30
#define MISSION_H 22
#define MWORLD_W (MISSION_W*16)
#define MWORLD_H (MISSION_H*16)

// ── pools ────────────────────────────────────────────────────────────────────
#define MAX_ENE  40
#define MAX_SHOT 120
#define MAX_PART 150

enum { K_GRUNT, K_HEAVY, K_BOSS };

typedef struct { float x, y, vx, vy; bool on; int kind; int hp, maxhp; float cd; float flash; } Ene;
typedef struct { float x, y, vx, vy; bool on; int team; int dmg; float life; int col; } Shot;
typedef struct { float x, y, vx, vy; float life, max; int col; } Part;
typedef struct { float x, y; int hp, maxhp; float cd; float down; float aim; bool active; int col; } Mate;

static Ene  ene[MAX_ENE];
static Shot shot[MAX_SHOT];
static Part part[MAX_PART];
static Mate mate[2];

// ── player (mission) ─────────────────────────────────────────────────────────
static float px, py;
static float aim_deg;
static int   php, php_max;
static float pshield, pshield_max;
static float shield_cd, fire_cd, power_cd;
static bool  in_cover;
static int   mission_state;          // 0 playing · 1 failed
static int   shield_break_flash;

// ── biotic / juice ────────────────────────────────────────────────────────────
static float biotic_t, biotic_x, biotic_y;
static float slowmo_t, hitstop_t;

// ── mission flow ───────────────────────────────────────────────────────────────
static int   wave_no, waves_total = 3, phase;   // phase 0 waves · 1 boss · 2 cleared
static float wave_timer, cleared_timer;
static bool  boss_active;

// ── camera ─────────────────────────────────────────────────────────────────────
static int cam_x, cam_y;

// ── run / story state ───────────────────────────────────────────────────────────
static int morality;          // + paragon · - renegade
static int chosen_planet;     // 0..2
static int missions_done;
static int ending;            // 0 paragon · 1 renegade · 2 pragmatist
static int prog;              // music progression step

// ── hub ───────────────────────────────────────────────────────────────────────
static float hub_x, hub_y, hub_face = 90;
static const float CREW_X[2] = { 64, 248 };
static const float CREW_Y[2] = { 72, 72 };

// ── dialogue ─────────────────────────────────────────────────────────────────
#define CONV_PILOT  0
#define CONV_DOCTOR 1
#define CONV_END    2
static int  conv_id, conv_stage, wheel_sel;
static bool conv_is_end;
static const char *conv_speaker, *conv_line, *conv_resp;
static const char *opt_label[3];
static int  opt_tone[3];        // 0 neutral · 1 paragon · 2 renegade
static int  opt_n;

// ── galaxy map ─────────────────────────────────────────────────────────────────
static int  starx[64], stary[64], starb[64];
static const char *PLANET_NAME[3] = { "FERIS III", "NOVERIA", "EDEN PRIME" };
static const char *PLANET_DESC[3] = { "red desert · marauders", "ice world · entrenched", "garden world · overrun" };
static const int   PNX[3] = { 78, 168, 252 };
static const int   PNY[3] = { 118, 70, 138 };
static const int   P_DISC[3] = { CLR_DARK_ORANGE, CLR_BLUE, CLR_GREEN };

// ════════════════════════════════════════════════════════════════════════════
//  GRID HELPERS
// ════════════════════════════════════════════════════════════════════════════

static void grid_clear(int w, int h, int floor) {
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            mset(x, y, (x < w && y < h) ? floor : 0);
}
static void grid_rect(int x, int y, int w, int h, int t) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) mset(x + i, y + j, t);
}
static void grid_border(int w, int h, int t) {
    for (int x = 0; x < w; x++) { mset(x, 0, t); mset(x, h - 1, t); }
    for (int y = 0; y < h; y++) { mset(0, y, t); mset(w - 1, y, t); }
}

static void build_bridge(void) {
    grid_clear(BRIDGE_W, BRIDGE_H, T_BFLOOR);
    grid_border(BRIDGE_W, BRIDGE_H, T_BWALL);
    grid_rect(2, 1, 2, 2, T_CONSOLE);  grid_rect(16, 1, 2, 2, T_CONSOLE);
    grid_rect(2, 8, 2, 2, T_CONSOLE);  grid_rect(16, 8, 2, 2, T_CONSOLE);
    grid_rect(8, 4, 4, 3, T_TERMINAL);   // the galaxy-map table (centre)
}

static void build_mission(void) {
    int fl = FLOOR_SLOT[chosen_planet], wl = WALL_SLOT[chosen_planet];
    grid_clear(MISSION_W, MISSION_H, fl);
    grid_border(MISSION_W, MISSION_H, wl);
    // interior wall blocks (rooms / pillars)
    grid_rect(6, 5, 2, 2, wl);  grid_rect(22, 5, 2, 2, wl);
    grid_rect(13, 9, 4, 2, wl);                            // central bunker
    grid_rect(6, 15, 2, 2, wl); grid_rect(22, 15, 2, 2, wl);
    // cover crates — tactical scatter
    mset(10, 17, T_COVER); mset(13, 18, T_COVER); mset(16, 18, T_COVER); mset(19, 17, T_COVER);
    mset(4, 12, T_COVER);  mset(25, 12, T_COVER);
    mset(10, 8, T_COVER);  mset(19, 8, T_COVER);
    mset(14, 13, T_COVER); mset(15, 13, T_COVER);
    mset(11, 4, T_COVER);  mset(18, 4, T_COVER);
}

static bool is_wallslot(int t) {
    return t == T_BWALL || t == T_CONSOLE || t == T_TERMINAL ||
           t == T_MWALL || t == T_MWALL2 || t == T_MWALL3;
}
static bool is_floorslot(int t) {
    return t == T_BFLOOR || t == T_MFLOOR || t == T_MFLOOR2 || t == T_MFLOOR3;
}
static bool tile_solid(int wx, int wy) {
    int t = tile_at(wx, wy);
    return t == 0 || is_wallslot(t) || t == T_COVER;
}

static bool los(float x1, float y1, float x2, float y2) {
    float d = distance((int)x1, (int)y1, (int)x2, (int)y2);
    int steps = (int)(d / 6) + 1;
    for (int i = 1; i < steps; i++) {
        float t = (float)i / steps;
        if (tile_solid((int)lerp(x1, x2, t), (int)lerp(y1, y2, t))) return false;
    }
    return true;
}

static void move_ent(float *x, float *y, float mx, float my, float r) {
    float nx = *x + mx;
    if (!tile_solid((int)(nx + (mx > 0 ? r : mx < 0 ? -r : 0)), (int)*y)) *x = nx;
    float ny = *y + my;
    if (!tile_solid((int)*x, (int)(ny + (my > 0 ? r : my < 0 ? -r : 0)))) *y = ny;
}

// ════════════════════════════════════════════════════════════════════════════
//  POOLS
// ════════════════════════════════════════════════════════════════════════════

static void add_shot(float x, float y, float deg, float spd, int team, int dmg, int col, float life) {
    for (int i = 0; i < MAX_SHOT; i++) if (!shot[i].on) {
        shot[i].x = x; shot[i].y = y;
        shot[i].vx = dx(spd, deg); shot[i].vy = dy(spd, deg);
        shot[i].on = true; shot[i].team = team; shot[i].dmg = dmg;
        shot[i].col = col; shot[i].life = life;
        return;
    }
}

static void burst(float x, float y, int n, float spd, int col) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < MAX_PART; i++) if (part[i].life <= 0) {
            float a = rnd(360), s = rnd_float_between(spd * 0.25f, spd);
            part[i].x = x; part[i].y = y;
            part[i].vx = dx(s, a); part[i].vy = dy(s, a);
            part[i].life = part[i].max = rnd_float_between(10, 24);
            part[i].col = col;
            break;
        }
}

static void spawn_ene(int kind, float x, float y) {
    for (int i = 0; i < MAX_ENE; i++) if (!ene[i].on) {
        ene[i].on = true; ene[i].kind = kind; ene[i].x = x; ene[i].y = y;
        ene[i].vx = ene[i].vy = 0; ene[i].flash = 0; ene[i].cd = rnd_between(30, 90);
        if (kind == K_GRUNT)      ene[i].hp = ene[i].maxhp = 18;
        else if (kind == K_HEAVY) ene[i].hp = ene[i].maxhp = 52;
        else { ene[i].hp = ene[i].maxhp = 420; boss_active = true; }
        return;
    }
}

static void spawn_at_top(int kind) {
    for (int tries = 0; tries < 24; tries++) {
        int cx = rnd_between(2, MISSION_W - 2), cy = rnd_between(2, 5);
        int wx = cx * 16 + 8, wy = cy * 16 + 8;
        if (is_floorslot(tile_at(wx, wy))) { spawn_ene(kind, wx, wy); return; }
    }
    spawn_ene(kind, (MISSION_W / 2) * 16, 40);
}

static int count_enemies(void) {
    int n = 0;
    for (int i = 0; i < MAX_ENE; i++) if (ene[i].on && ene[i].kind != K_BOSS) n++;
    return n;
}

static int nearest_enemy(float x, float y, float maxd) {
    int best = -1; float bd = maxd;
    for (int i = 0; i < MAX_ENE; i++) {
        if (!ene[i].on) continue;
        float d = distance((int)x, (int)y, (int)ene[i].x, (int)ene[i].y);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void begin_wave(void) {
    int d = chosen_planet;
    int g = 4 + wave_no * 2 + d;
    int h = (wave_no >= 1) ? (wave_no - 1) + (d > 0 ? 1 : 0) : 0;
    for (int i = 0; i < g; i++) spawn_at_top(K_GRUNT);
    for (int i = 0; i < h; i++) spawn_at_top(K_HEAVY);
    note(degree(SCALE_MINOR, 3, 0), 5, 4);
}

static void kill_enemy(int i) {
    bool boss = ene[i].kind == K_BOSS;
    burst(ene[i].x, ene[i].y, boss ? 70 : 12, boss ? 5 : 3, boss ? CLR_ORANGE : CLR_RED);
    if (boss) {
        boss_active = false; phase = 2; cleared_timer = 130;
        hitstop_t = 10; shake(11); note(28, INSTR_NOISE, 7);
    } else {
        hit(40, INSTR_NOISE, 3, 90);
    }
    ene[i].on = false;
}

static void damage_enemy(int i, int dmg) {
    ene[i].hp -= dmg; ene[i].flash = 6;
    if (ene[i].hp <= 0) kill_enemy(i);
}

static void hurt_player(float dmg) {
    if (in_cover) dmg *= 0.45f;
    shield_cd = 150;
    if (pshield > 0) {
        pshield -= dmg;
        if (pshield <= 0) {
            php += (int)pshield; pshield = 0;
            shield_break_flash = 10; shake(5); note(40, INSTR_NOISE, 4);
        }
    } else {
        php -= (int)dmg;
    }
    if (php <= 0) { php = 0; mission_state = 1; note(26, INSTR_NOISE, 7); shake(9); }
}

static void do_biotic(void) {
    power_cd = 360; biotic_x = px; biotic_y = py; biotic_t = 24;
    slowmo_t = 30; shake(7);
    burst(px, py, 34, 4, CLR_BLUE);
    note(34, 7, 6);                  // filter-swept whoosh instrument
    for (int i = 0; i < MAX_ENE; i++) {
        if (!ene[i].on) continue;
        float d = distance((int)px, (int)py, (int)ene[i].x, (int)ene[i].y);
        if (d < 80) {
            float a = angle_to((int)px, (int)py, (int)ene[i].x, (int)ene[i].y);
            ene[i].x += dx(22, a); ene[i].y += dy(22, a);   // knockback
            damage_enemy(i, ene[i].kind == K_BOSS ? 55 : 140);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  DIALOGUE
// ════════════════════════════════════════════════════════════════════════════

static void start_conv(int id) {
    conv_id = id; conv_stage = 0; wheel_sel = 1; conv_resp = "";
    conv_is_end = (id == CONV_END);
    if (id == CONV_PILOT) {
        conv_speaker = "JOKER  -  helmsman";
        conv_line    = "Relay's hot, Commander. Where to?";
        opt_label[0] = "\"We protect the colony.\"";   opt_tone[0] = 1;
        opt_label[1] = "\"Plot the fastest route.\"";   opt_tone[1] = 0;
        opt_label[2] = "\"Anything in our path burns.\""; opt_tone[2] = 2;
        opt_n = 3;
    } else if (id == CONV_DOCTOR) {
        conv_speaker = "DR. T'SARA  -  medbay";
        conv_line    = "The husks were people once.";
        opt_label[0] = "\"We save who we can.\"";   opt_tone[0] = 1;
        opt_label[1] = "\"Understood, doctor.\"";   opt_tone[1] = 0;
        opt_label[2] = "\"They're targets now.\"";  opt_tone[2] = 2;
        opt_n = 3;
    } else {
        conv_speaker = "REAPER CORE  -  exposed";
        conv_line    = "The core kneels. Your call, Commander.";
        opt_label[0] = "SPARE - take its data";  opt_tone[0] = 1;
        opt_label[1] = "DISABLE - leave it dark"; opt_tone[1] = 0;
        opt_label[2] = "EXECUTE - burn it all";  opt_tone[2] = 2;
        opt_n = 3;
    }
}

static const char *resp_for_tone(int tone) {
    return tone == 1 ? "The crew stands a little taller."
         : tone == 2 ? "The crew exchanges a wary glance."
         :             "\"Acknowledged, Commander.\"";
}

// ════════════════════════════════════════════════════════════════════════════
//  INIT
// ════════════════════════════════════════════════════════════════════════════

void init(void) {
    colorkey(0);
    missions_done = load(0);
    build_bridge();
    hub_x = 160; hub_y = 150;

    // synth voices
    bpm(80);
    instrument(5, INSTR_TRI, 400, 200, 4, 600);    // orchestral pad
    instrument_filter(5, FILTER_LOW, 760, 3);
    instrument(6, INSTR_SINE, 4, 110, 0, 120);     // UI / melody pluck
    instrument(7, INSTR_SAW, 2, 70, 3, 260);       // biotic whoosh
    instrument_filter(7, FILTER_LOW, 500, 9);
    instrument_lfo(7, 0, LFO_CUTOFF, 1.2f, 1100);
    lfo_shape(7, 0, LFO_SHAPE_RANDOM);   // organic drift, not a mechanical sine (STATUS #39)

    // galaxy starfield (fixed)
    for (int i = 0; i < 64; i++) {
        starx[i] = rnd(SCREEN_W); stary[i] = rnd(SCREEN_H);
        starb[i] = rnd(3);
    }
}

static void start_mission(void) {
    build_mission();
    px = (MISSION_W / 2) * 16; py = (MISSION_H - 3) * 16;
    aim_deg = -90; php = php_max = 100; pshield = pshield_max = 100;
    shield_cd = fire_cd = power_cd = 0; in_cover = false;
    mission_state = 0; shield_break_flash = 0; biotic_t = slowmo_t = hitstop_t = 0;
    for (int i = 0; i < MAX_ENE; i++)  ene[i].on = false;
    for (int i = 0; i < MAX_SHOT; i++) shot[i].on = false;
    for (int i = 0; i < MAX_PART; i++) part[i].life = 0;
    wave_no = 0; phase = 0; boss_active = false; wave_timer = 0; cleared_timer = 0;
    int mc[2] = { CLR_GREEN, CLR_ORANGE };
    for (int m = 0; m < 2; m++) {
        mate[m].x = px + (m == 0 ? -26 : 26); mate[m].y = py + 20;
        mate[m].hp = mate[m].maxhp = 70; mate[m].cd = 0; mate[m].down = 0;
        mate[m].aim = -90; mate[m].active = true; mate[m].col = mc[m];
    }
    begin_wave();
    bpm(124);
}

// ════════════════════════════════════════════════════════════════════════════
//  UPDATE — per scene
// ════════════════════════════════════════════════════════════════════════════

static void update_hub(void) {
    float k = dt() * 60.0f;
    float vx = 0, vy = 0;
    if (btn(0, BTN_LEFT))  vx -= 1;
    if (btn(0, BTN_RIGHT)) vx += 1;
    if (btn(0, BTN_UP))    vy -= 1;
    if (btn(0, BTN_DOWN))  vy += 1;
    if (vx || vy) hub_face = angle_to(0, 0, (int)(vx * 10), (int)(vy * 10));
    move_ent(&hub_x, &hub_y, vx * 1.4f * k, vy * 1.4f * k, 6);

    // what can we interact with?
    int crew = -1;
    for (int i = 0; i < 2; i++)
        if (near((int)hub_x, (int)hub_y, (int)CREW_X[i], (int)CREW_Y[i], 26)) crew = i;
    bool at_table = near((int)hub_x, (int)hub_y, 160, 104, 40);

    if (btnp(0, BTN_A)) {
        if (at_table)     { scene = SC_MAP; bpm(70); prog = 0; note(72, 6, 5); }
        else if (crew >= 0) { start_conv(crew == 0 ? CONV_PILOT : CONV_DOCTOR);
                              scene = SC_DIALOGUE; note(67, 6, 4); }
    }

    if (every(4)) { int r[4] = { 48, 53, 55, 52 }; chord(r[prog++ % 4], CHORD_MAJ7, 5, 2); }
}

static void update_map(void) {
    if (btnp(0, BTN_LEFT)  || btnp(1, BTN_LEFT))  { chosen_planet = (chosen_planet + 2) % 3; note(69, 6, 4); }
    if (btnp(0, BTN_RIGHT) || btnp(1, BTN_RIGHT)) { chosen_planet = (chosen_planet + 1) % 3; note(71, 6, 4); }
    for (int i = 0; i < 3; i++)
        if (near(mouse_x(), mouse_y(), PNX[i], PNY[i], 16)) chosen_planet = i;

    if (btnp(0, BTN_B)) { scene = SC_HUB; bpm(80); prog = 0; }
    if (btnp(0, BTN_A) || mouse_pressed(MOUSE_LEFT)) {
        scene = SC_MISSION; prog = 0; note(60, 6, 5);
        start_mission();
    }
    if (every(4)) { int r[3] = { 50, 57, 55 }; chord(r[prog++ % 3], CHORD_MAJ7, 5, 1); }
}

static void update_dialogue(void) {
    // wheel geometry (used here for mouse hover — must match draw_wheel)
    int cx = 108, cy = 126, R = 60;
    float ang[3] = { -38, 0, 38 };
    for (int i = 0; i < opt_n; i++) {
        int nx = cx + (int)(cos_deg(ang[i]) * R), ny = cy + (int)(sin_deg(ang[i]) * R);
        if (near(mouse_x(), mouse_y(), nx, ny, 34)) wheel_sel = i;
    }
    if (btnp(0, BTN_LEFT)  || btnp(1, BTN_LEFT))  wheel_sel = (wheel_sel + opt_n - 1) % opt_n;
    if (btnp(0, BTN_RIGHT) || btnp(1, BTN_RIGHT)) wheel_sel = (wheel_sel + 1) % opt_n;

    bool confirm = btnp(0, BTN_A) || mouse_pressed(MOUSE_LEFT);
    if (!confirm) return;

    if (conv_stage == 0) {
        if (conv_is_end) {
            int delta = wheel_sel == 0 ? 2 : wheel_sel == 2 ? -2 : 0;
            int s = morality + delta;
            ending = s >= 1 ? 0 : s <= -1 ? 1 : 2;
            missions_done++; save(0, missions_done); save(1, ending);
            scene = SC_END; bpm(74);
            if (ending == 0)      strum(60, CHORD_MAJ7, INSTR_TRI, 5, 60);
            else if (ending == 1) strum(45, CHORD_MIN, INSTR_SAW, 5, -50);
            else                  note(52, 6, 4);
        } else {
            morality += opt_tone[wheel_sel] == 1 ? 1 : opt_tone[wheel_sel] == 2 ? -1 : 0;
            conv_resp = resp_for_tone(opt_tone[wheel_sel]);
            conv_stage = 1; note(64, 6, 4);
        }
    } else {
        scene = SC_HUB; bpm(80); prog = 0;
    }
}

static void update_mission(void) {
    cam_x = (int)clamp(px - SCREEN_W / 2, 0, MWORLD_W - SCREEN_W);
    cam_y = (int)clamp(py - SCREEN_H / 2, 0, MWORLD_H - SCREEN_H);

    if (mission_state == 1) {                       // failed
        if (btnp(0, BTN_A)) { scene = SC_HUB; bpm(80); hub_x = 160; hub_y = 150; prog = 0; }
        return;
    }

    float rf = dt() * 60.0f;
    if (hitstop_t > 0) hitstop_t -= rf;
    if (slowmo_t > 0)  slowmo_t  -= rf;
    if (biotic_t > 0)  biotic_t  -= rf;
    if (shield_break_flash > 0) shield_break_flash--;
    float ts = (hitstop_t > 0) ? 0.0f : (slowmo_t > 0 ? 0.35f : 1.0f);
    float step = rf * ts;

    // ── player ──
    float mvx = 0, mvy = 0;
    if (btn(0, BTN_LEFT))  mvx -= 1;
    if (btn(0, BTN_RIGHT)) mvx += 1;
    if (btn(0, BTN_UP))    mvy -= 1;
    if (btn(0, BTN_DOWN))  mvy += 1;
    float ml = length((int)(mvx * 100), (int)(mvy * 100)) / 100.0f;
    if (ml > 0) { mvx /= ml; mvy /= ml; }
    move_ent(&px, &py, mvx * 1.9f * step, mvy * 1.9f * step, 5);

    float wmx = mouse_x() + cam_x, wmy = mouse_y() + cam_y;
    aim_deg = angle_to((int)px, (int)py, (int)wmx, (int)wmy);

    // cover detection
    in_cover = false;
    for (int a = 0; a < 360; a += 45) {
        int t = tile_at((int)(px + dx(12, a)), (int)(py + dy(12, a)));
        if (t == T_COVER || is_wallslot(t)) { in_cover = true; break; }
    }

    // fire
    if (fire_cd > 0) fire_cd -= step;
    if ((mouse_down(MOUSE_LEFT) || btn(0, BTN_A)) && fire_cd <= 0) {
        float a = aim_deg + rnd_float_between(-3.5f, 3.5f);
        add_shot(px + dx(9, aim_deg), py + dy(9, aim_deg), a, 6.5f, 0, 8, CLR_LIGHT_YELLOW, 60);
        burst(px + dx(11, aim_deg), py + dy(11, aim_deg), 2, 2, CLR_YELLOW);
        fire_cd = 8; hit(70, INSTR_SQUARE, 2, 28);
    }

    // biotic
    if (power_cd > 0) power_cd -= step;
    if (btnp(0, BTN_B) && power_cd <= 0) do_biotic();

    // shield regen
    if (shield_cd > 0) shield_cd -= step;
    else if (pshield < pshield_max) {
        pshield += (in_cover ? 0.9f : 0.45f) * step;
        if (pshield > pshield_max) pshield = pshield_max;
    }

    // ── squad ──
    for (int m = 0; m < 2; m++) {
        if (!mate[m].active) continue;
        if (mate[m].down > 0) { mate[m].down -= step; if (mate[m].down <= 0) mate[m].hp = mate[m].maxhp; continue; }
        float fx = px + (m == 0 ? -26 : 26), fy = py + 22;
        float d = distance((int)mate[m].x, (int)mate[m].y, (int)fx, (int)fy);
        if (d > 5) {
            float a = angle_to((int)mate[m].x, (int)mate[m].y, (int)fx, (int)fy);
            float spd = clamp(d * 0.12f, 0.4f, 2.4f);
            move_ent(&mate[m].x, &mate[m].y, dx(spd, a) * step, dy(spd, a) * step, 5);
        }
        int tgt = nearest_enemy(mate[m].x, mate[m].y, 150);
        if (mate[m].cd > 0) mate[m].cd -= step;
        if (tgt >= 0 && los(mate[m].x, mate[m].y, ene[tgt].x, ene[tgt].y)) {
            mate[m].aim = angle_to((int)mate[m].x, (int)mate[m].y, (int)ene[tgt].x, (int)ene[tgt].y);
            if (mate[m].cd <= 0) {
                add_shot(mate[m].x, mate[m].y, mate[m].aim + rnd_float_between(-4, 4), 6.0f, 0, 6, mate[m].col, 80);
                mate[m].cd = 22; hit(74, INSTR_SQUARE, 1, 24);
            }
        }
    }

    // ── enemies ──
    for (int i = 0; i < MAX_ENE; i++) {
        if (!ene[i].on) continue;
        if (ene[i].flash > 0) ene[i].flash -= rf;
        float ex = ene[i].x, ey = ene[i].y;
        float a = angle_to((int)ex, (int)ey, (int)px, (int)py);
        float d = distance((int)ex, (int)ey, (int)px, (int)py);

        if (ene[i].kind == K_GRUNT) {
            move_ent(&ene[i].x, &ene[i].y, dx(1.0f, a) * step, dy(1.0f, a) * step, 6);
            ene[i].cd -= step;
            if (d < 175 && ene[i].cd <= 0 && los(ex, ey, px, py)) {
                ene[i].cd = 70 + rnd(40);
                add_shot(ex, ey, a + rnd_float_between(-5, 5), 3.2f, 1, 9, CLR_RED, 130);
                hit(48, INSTR_SAW, 2, 50);
            }
        } else if (ene[i].kind == K_HEAVY) {
            move_ent(&ene[i].x, &ene[i].y, dx(0.6f, a) * step, dy(0.6f, a) * step, 6);
            ene[i].cd -= step;
            if (d < 200 && ene[i].cd <= 0 && los(ex, ey, px, py)) {
                ene[i].cd = 110 + rnd(40);
                for (int b = -1; b <= 1; b++)
                    add_shot(ex, ey, a + b * 9, 3.4f, 1, 14, CLR_ORANGE, 150);
                hit(38, INSTR_SAW, 3, 80);
            }
        } else {  // BOSS
            float spd = d > 150 ? 0.85f : d < 95 ? -0.7f : 0;
            float strafe = sin_deg(now() * 60) * 0.6f;
            move_ent(&ene[i].x, &ene[i].y,
                     (dx(spd, a) + dx(strafe, a + 90)) * step,
                     (dy(spd, a) + dy(strafe, a + 90)) * step, 14);
            ene[i].cd -= step;
            if (ene[i].cd <= 0) {
                ene[i].cd = 52; ene[i].flash = 4; shake(2);
                for (int b = 0; b < 12; b++)
                    add_shot(ex, ey, b * 30 + now() * 25, 2.7f, 1, 11, CLR_DARK_ORANGE, 160);
                if (los(ex, ey, px, py))
                    for (int b = -1; b <= 1; b++)
                        add_shot(ex, ey, a + b * 7, 4.0f, 1, 13, CLR_RED, 170);
                hit(34, INSTR_NOISE, 4, 120);
            }
        }
        // melee contact
        if (near((int)px, (int)py, (int)ene[i].x, (int)ene[i].y, ene[i].kind == K_BOSS ? 16 : 9))
            hurt_player((ene[i].kind == K_BOSS ? 0.7f : 0.35f) * step);
    }

    // ── bullets ──
    for (int i = 0; i < MAX_SHOT; i++) {
        if (!shot[i].on) continue;
        shot[i].x += shot[i].vx * step; shot[i].y += shot[i].vy * step;
        shot[i].life -= step;
        if (shot[i].life <= 0) { shot[i].on = false; continue; }
        int sxp = (int)shot[i].x, syp = (int)shot[i].y;
        if (tile_solid(sxp, syp)) {
            burst(shot[i].x, shot[i].y, 3, 2, tile_at(sxp, syp) == T_COVER ? CLR_ORANGE : CLR_LIGHT_GREY);
            shot[i].on = false; continue;
        }
        if (shot[i].team == 0) {
            for (int j = 0; j < MAX_ENE; j++) {
                if (!ene[j].on) continue;
                int r = ene[j].kind == K_BOSS ? 15 : 7;
                if (near(sxp, syp, (int)ene[j].x, (int)ene[j].y, r)) {
                    burst(shot[i].x, shot[i].y, 3, 2, CLR_LIGHT_YELLOW);
                    damage_enemy(j, shot[i].dmg);
                    shot[i].on = false; break;
                }
            }
        } else {
            if (near(sxp, syp, (int)px, (int)py, 8)) {
                hurt_player(shot[i].dmg);
                burst(shot[i].x, shot[i].y, 4, 2.5f, CLR_RED);
                shot[i].on = false; continue;
            }
            for (int m = 0; m < 2; m++) {
                if (!mate[m].active || mate[m].down > 0) continue;
                if (near(sxp, syp, (int)mate[m].x, (int)mate[m].y, 7)) {
                    mate[m].hp -= shot[i].dmg;
                    if (mate[m].hp <= 0) { mate[m].down = 360; note(42, INSTR_NOISE, 3); }
                    shot[i].on = false; break;
                }
            }
        }
    }

    // ── particles ──
    for (int i = 0; i < MAX_PART; i++) {
        if (part[i].life <= 0) continue;
        part[i].x += part[i].vx * step; part[i].y += part[i].vy * step;
        part[i].vx *= 0.92f; part[i].vy *= 0.92f;
        part[i].life -= step;
    }

    // ── wave / boss flow ──
    if (phase == 0 && count_enemies() == 0) {
        wave_timer += step;
        if (wave_timer > 70) {
            wave_no++; wave_timer = 0;
            if (wave_no >= waves_total) { phase = 1; wave_timer = 0; }
            else begin_wave();
        }
    }
    if (phase == 1 && !boss_active) {
        wave_timer += step;
        if (wave_timer > 60) { spawn_ene(K_BOSS, (MISSION_W / 2) * 16, 56); shake(6); note(30, INSTR_SAW, 5); }
    }
    if (phase == 2) {
        cleared_timer -= rf;
        if (cleared_timer <= 0) { start_conv(CONV_END); scene = SC_DIALOGUE; bpm(70); }
    }

    // mission music
    if (every(2)) { int r[4] = { 45, 45, 43, 48 }; chord(r[prog++ % 4], CHORD_MIN, 5, 2); }
    if (euclid(5, 8, beat())) hit(36, INSTR_NOISE, 1, 28);
}

void update(void) {
    switch (scene) {
        case SC_HUB:      update_hub();      break;
        case SC_MAP:      update_map();      break;
        case SC_MISSION:  update_mission();  break;
        case SC_DIALOGUE: update_dialogue(); break;
        case SC_END:      if (btnp(0, BTN_A)) { scene = SC_HUB; bpm(80); hub_x = 160; hub_y = 150; prog = 0; } break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  DRAW HELPERS
// ════════════════════════════════════════════════════════════════════════════

static void draw_fighter(int sprite_idx, float x, float y, float deg, int armor, int accent, bool flash) {
    ovalfill((int)x, (int)y + 6, 6, 3, CLR_BROWNISH_BLACK);   // shadow
    if (flash) { pal(MAGIC_ARMOR, CLR_WHITE); pal(MAGIC_ACCENT, CLR_WHITE); }
    else       { pal(MAGIC_ARMOR, armor);     pal(MAGIC_ACCENT, accent);    }
    spr_rot(sprite_idx, (int)x - 8, (int)y - 8, deg);
    pal_reset();
}

static void draw_boss(float x, float y, float deg, bool flash) {
    ovalfill((int)x, (int)y + 12, 12, 5, CLR_BROWNISH_BLACK);
    if (flash) { pal(MAGIC_ARMOR, CLR_WHITE);       pal(MAGIC_ACCENT, CLR_WHITE); }
    else       { pal(MAGIC_ARMOR, CLR_DARK_PURPLE); pal(MAGIC_ACCENT, CLR_RED);   }
    sspr_ex(16, 0, 16, 16, (int)x - 16, (int)y - 16, 32, 32, deg, 16, 16);
    pal_reset();
}

static void draw_wheel(int cx, int cy, int R) {
    static const int TONE_COL[3] = { CLR_LIGHT_GREY, CLR_BLUE, CLR_RED };
    float ang[3] = { -38, 0, 38 };
    circfill(cx, cy, 6, CLR_DARK_BLUE);
    circ(cx, cy, 6, CLR_INDIGO);
    for (int i = 0; i < opt_n; i++) {
        int nx = cx + (int)(cos_deg(ang[i]) * R), ny = cy + (int)(sin_deg(ang[i]) * R);
        bool sel = (i == wheel_sel);
        int col = TONE_COL[opt_tone[i]];
        line(cx, cy, nx, ny, sel ? col : CLR_DARK_GREY);
        circfill(nx, ny, sel ? 5 : 3, sel ? col : CLR_DARKER_GREY);
        int tw = text_width(opt_label[i]);
        int lx = nx + 10, ly = ny - 3;
        if (lx + tw > SCREEN_W - 4) lx = SCREEN_W - 4 - tw;
        rectfill(lx - 2, ly - 1, tw + 4, 9, CLR_BLACK);
        print(opt_label[i], lx, ly, sel ? CLR_WHITE : CLR_LIGHT_GREY);
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  DRAW — per scene
// ════════════════════════════════════════════════════════════════════════════

static void draw_hub(void) {
    cls(CLR_BROWNISH_BLACK);
    map(0, 0, 0, 0, BRIDGE_W, BRIDGE_H);

    // holographic galaxy over the table
    int hx = 160, hy = 52;
    circfill(hx, hy, 2, CLR_LIGHT_YELLOW);
    for (int p = 0; p < 3; p++) {
        float a = now() * (38 + p * 14) + p * 120;
        int ox = hx + (int)(cos_deg(a) * (7 + p * 4));
        int oy = hy + (int)(sin_deg(a) * (3 + p * 2));
        circfill(ox, oy, 1, P_DISC[p]);
    }
    if (blink(5)) line(hx - 10, hy, hx + 10, hy, CLR_TRUE_BLUE);
    circ(hx, hy, 11, CLR_DARK_BLUE);

    // crew
    draw_fighter(0, CREW_X[0], CREW_Y[0], 90, CLR_INDIGO, CLR_LIGHT_PEACH, false);
    draw_fighter(0, CREW_X[1], CREW_Y[1], 90, CLR_PINK,   CLR_LIGHT_PEACH, false);
    print("JOKER",  (int)CREW_X[0] - 14, (int)CREW_Y[0] - 18, CLR_LIGHT_GREY);
    print("T'SARA", (int)CREW_X[1] - 16, (int)CREW_Y[1] - 18, CLR_LIGHT_GREY);

    // player
    draw_fighter(0, hub_x, hub_y, hub_face, CLR_BLUE, CLR_WHITE, false);

    // prompt
    int crew = -1;
    for (int i = 0; i < 2; i++)
        if (near((int)hub_x, (int)hub_y, (int)CREW_X[i], (int)CREW_Y[i], 26)) crew = i;
    bool at_table = near((int)hub_x, (int)hub_y, 160, 104, 40);
    const char *prompt = 0;
    if (at_table)       prompt = "[Z]  open the galaxy map";
    else if (crew == 0) prompt = "[Z]  talk to Joker";
    else if (crew == 1) prompt = "[Z]  talk to Dr. T'Sara";
    if (prompt && blink(20)) print_centered(prompt, SCREEN_W/2, 120, CLR_YELLOW);

    // title + morality meter
    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    print("SSV NORMANDY", 4, 1, CLR_LIGHT_GREY);
    int mm = (int)clamp(morality, -5, 5);
    print("P", SCREEN_W - 64, 1, CLR_BLUE);
    bar(SCREEN_W - 56, 2, 44, 6, (mm + 5) / 10.0f, CLR_BLUE, CLR_RED);
    print("R", SCREEN_W - 8, 1, CLR_RED);
}

static void draw_map(void) {
    cls(CLR_BLACK);
    for (int i = 0; i < 64; i++) {
        int c = starb[i] == 0 ? CLR_DARK_GREY : starb[i] == 1 ? CLR_LIGHT_GREY : CLR_WHITE;
        pset(starx[i], stary[i], c);
    }
    // links
    line(PNX[0], PNY[0], PNX[1], PNY[1], CLR_DARKER_BLUE);
    line(PNX[1], PNY[1], PNX[2], PNY[2], CLR_DARKER_BLUE);

    for (int i = 0; i < 3; i++) {
        bool sel = (i == chosen_planet);
        if (sel) { circ(PNX[i], PNY[i], 12 + (frame() % 20) / 5, CLR_YELLOW); }
        circfill(PNX[i], PNY[i], 7, P_DISC[i]);
        circ(PNX[i], PNY[i], 7, CLR_WHITE);
        int tw = text_width(PLANET_NAME[i]);
        print(PLANET_NAME[i], PNX[i] - tw / 2, PNY[i] + 12, sel ? CLR_WHITE : CLR_LIGHT_GREY);
    }

    rectfill(0, 0, SCREEN_W, 9, CLR_DARKER_BLUE);
    print("GALAXY MAP  -  select a destination", 4, 1, CLR_WHITE);

    rectfill(0, SCREEN_H - 28, SCREEN_W, 28, CLR_BROWNISH_BLACK);
    print(str("> %s", PLANET_NAME[chosen_planet]), 6, SCREEN_H - 24, P_DISC[chosen_planet]);
    print_right("<- ->  pick   Z  launch   X  back", SCREEN_W - 4, SCREEN_H - 24, CLR_LIGHT_GREY);
    print(PLANET_DESC[chosen_planet], 6, SCREEN_H - 13, CLR_LIGHT_PEACH);
}

static void draw_mission(void) {
    cls(CLR_BLACK);
    camera(cam_x, cam_y);
    map(0, 0, 0, 0, MISSION_W, MISSION_H);   // per-planet tile slots (set in build_mission)

    // squad
    for (int m = 0; m < 2; m++) {
        if (!mate[m].active) continue;
        if (mate[m].down > 0) {
            ovalfill((int)mate[m].x, (int)mate[m].y, 7, 3, CLR_DARK_RED);
            print("!", (int)mate[m].x - 2, (int)mate[m].y - 4, blink(6) ? CLR_RED : CLR_ORANGE);
        } else {
            draw_fighter(0, mate[m].x, mate[m].y, mate[m].aim, mate[m].col, CLR_WHITE, false);
        }
    }
    // enemies
    for (int i = 0; i < MAX_ENE; i++) {
        if (!ene[i].on) continue;
        float a = angle_to((int)ene[i].x, (int)ene[i].y, (int)px, (int)py);
        bool fl = ene[i].flash > 0;
        if (ene[i].kind == K_BOSS) draw_boss(ene[i].x, ene[i].y, a, fl);
        else draw_fighter(1, ene[i].x, ene[i].y, a, ene[i].kind == K_HEAVY ? CLR_DARK_RED : CLR_DARK_PURPLE, CLR_RED, fl);
    }
    // player
    draw_fighter(0, px, py, aim_deg, CLR_BLUE, CLR_WHITE, false);
    if (in_cover) circ((int)px, (int)py, 9, blink(8) ? CLR_YELLOW : CLR_ORANGE);

    // bullets (tracers)
    for (int i = 0; i < MAX_SHOT; i++) {
        if (!shot[i].on) continue;
        line((int)shot[i].x, (int)shot[i].y,
             (int)(shot[i].x - shot[i].vx * 1.6f), (int)(shot[i].y - shot[i].vy * 1.6f), shot[i].col);
        pset((int)shot[i].x, (int)shot[i].y, CLR_WHITE);
    }
    // particles
    for (int i = 0; i < MAX_PART; i++) {
        if (part[i].life <= 0) continue;
        if (part[i].life > part[i].max * 0.5f) pset((int)part[i].x, (int)part[i].y, part[i].col);
        else pset((int)part[i].x, (int)part[i].y, CLR_DARK_GREY);
    }
    // biotic ring
    if (biotic_t > 0) {
        int r = (int)((24 - biotic_t) * 3.6f);
        fillp(FILL_CHECKER, -1);
        circfill((int)biotic_x, (int)biotic_y, r, CLR_BLUE);
        fillp_reset();
        circ((int)biotic_x, (int)biotic_y, r, CLR_WHITE);
    }

    camera(0, 0);

    // low-health vignette
    if (php < 35 && mission_state == 0) {
        fillp(FILL_DOTS, -1);
        rectfill(0, 0, SCREEN_W, 14, CLR_RED);
        rectfill(0, SCREEN_H - 14, SCREEN_W, 14, CLR_RED);
        rectfill(0, 0, 14, SCREEN_H, CLR_RED);
        rectfill(SCREEN_W - 14, 0, 14, SCREEN_H, CLR_RED);
        fillp_reset();
    }
    if (shield_break_flash > 0) {
        fillp(FILL_CHECKER, -1);
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLUE);
        fillp_reset();
    }

    // boss health bar
    if (boss_active) {
        for (int i = 0; i < MAX_ENE; i++) if (ene[i].on && ene[i].kind == K_BOSS) {
            rectfill(50, 12, 220, 12, CLR_BLACK);
            bar(54, 15, 212, 6, ene[i].hp / (float)ene[i].maxhp, CLR_RED, CLR_DARKER_GREY);
            print_centered("REAPER CORE", SCREEN_W/2, 13, CLR_RED);
            break;
        }
    }

    // HUD
    rectfill(0, SCREEN_H - 22, SCREEN_W, 22, CLR_BROWNISH_BLACK);
    line(0, SCREEN_H - 22, SCREEN_W, SCREEN_H - 22, CLR_DARK_GREY);
    print("HP", 4, SCREEN_H - 19, CLR_LIGHT_GREY);
    bar(20, SCREEN_H - 19, 80, 5, php / (float)php_max, CLR_GREEN, CLR_DARKER_GREY);
    print("SH", 4, SCREEN_H - 10, CLR_LIGHT_GREY);
    bar(20, SCREEN_H - 10, 80, 5, pshield / pshield_max, CLR_BLUE, CLR_DARKER_GREY);
    print("BIOTIC", 108, SCREEN_H - 19, power_cd <= 0 ? CLR_BLUE : CLR_DARK_GREY);
    bar(108, SCREEN_H - 10, 56, 5, 1 - power_cd / 360.0f, CLR_BLUE, CLR_DARKER_GREY);
    if (in_cover) print("IN COVER", 174, SCREEN_H - 19, blink(10) ? CLR_YELLOW : CLR_ORANGE);
    const char *obj = phase < 1 ? str("WAVE %d/%d", wave_no + 1, waves_total)
                    : boss_active ? "KILL THE CORE" : "...";
    print_right(obj, SCREEN_W - 4, SCREEN_H - 19, CLR_LIGHT_PEACH);
    // squad pips
    for (int m = 0; m < 2; m++) {
        int sx = SCREEN_W - 70 + m * 34, sy = SCREEN_H - 9;
        circfill(sx, sy, 3, mate[m].down > 0 ? CLR_DARK_RED : mate[m].col);
        bar(sx + 6, sy - 2, 22, 4, mate[m].down > 0 ? 0 : mate[m].hp / (float)mate[m].maxhp,
            mate[m].col, CLR_DARKER_GREY);
    }

    // crosshair
    int mx = mouse_x(), myy = mouse_y();
    int cc = power_cd <= 0 ? CLR_BLUE : CLR_RED;
    circ(mx, myy, 4, cc);
    line(mx - 7, myy, mx - 3, myy, CLR_RED); line(mx + 3, myy, mx + 7, myy, CLR_RED);
    line(mx, myy - 7, mx, myy - 3, CLR_RED); line(mx, myy + 3, mx, myy + 7, CLR_RED);

    if (mission_state == 1) {
        rectfill(SCREEN_W / 2 - 80, SCREEN_H / 2 - 22, 160, 44, CLR_BLACK);
        rect(SCREEN_W / 2 - 80, SCREEN_H / 2 - 22, 160, 44, CLR_RED);
        print_centered("MISSION FAILED", SCREEN_W/2, SCREEN_H / 2 - 12, CLR_RED);
        print_centered("Z: return to the Normandy", SCREEN_W/2, SCREEN_H / 2 + 4, CLR_LIGHT_GREY);
    }
}

static void draw_dialogue(void) {
    // dim backdrop (keep mission/hub readable behind)
    if (conv_is_end) draw_mission(); else draw_hub();
    fillp(FILL_CHECKER, -1);
    rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK);
    fillp_reset();

    // speaker + line panel
    rectfill(8, 14, SCREEN_W - 16, 40, CLR_DARK_BLUE);
    rect(8, 14, SCREEN_W - 16, 40, CLR_INDIGO);
    print(conv_speaker, 16, 20, CLR_LIGHT_YELLOW);
    print(conv_stage == 0 ? conv_line : conv_resp, 16, 34, CLR_WHITE);

    if (conv_stage == 0) {
        draw_wheel(108, 126, 60);
        print_right("<- ->  choose    Z  select", SCREEN_W - 6, SCREEN_H - 9, CLR_LIGHT_GREY);
    } else {
        print_centered("- Z to continue -", SCREEN_W/2, 150, blink(20) ? CLR_LIGHT_GREY : CLR_DARK_GREY);
    }
}

static void draw_end(void) {
    cls(CLR_BLACK);
    for (int i = 0; i < 64; i++) pset(starx[i], stary[i], starb[i] == 2 ? CLR_WHITE : CLR_DARK_GREY);

    static const char *TITLE[3] = { "PARAGON", "RENEGADE", "PRAGMATIST" };
    static const int   TCOL[3]  = { CLR_BLUE, CLR_RED, CLR_LIGHT_GREY };
    static const char *L1[3] = {
        "You spared the construct. Its core",
        "You burned it to the core. The",
        "You left it dark and walked away."
    };
    static const char *L2[3] = {
        "data will shield a hundred worlds.",
        "threat ends here -- and they will",
        "No data. No ashes. The mission,"
    };
    static const char *L3[3] = {
        "The galaxy remembers mercy.",
        "fear the name of the Normandy.",
        "simply done."
    };
    print_scaled(TITLE[ending], SCREEN_W / 2 - text_width(TITLE[ending]), 44, TCOL[ending], 2);
    print_centered(L1[ending], SCREEN_W/2, 84, CLR_LIGHT_PEACH);
    print_centered(L2[ending], SCREEN_W/2, 96, CLR_LIGHT_PEACH);
    print_centered(L3[ending], SCREEN_W/2, 108, CLR_LIGHT_PEACH);
    print_centered(str("MISSIONS COMPLETED: %d", missions_done), SCREEN_W/2, 140, CLR_YELLOW);
    if (blink(22)) print_centered("Z: return to the Normandy", SCREEN_W/2, 170, CLR_LIGHT_GREY);
}

void draw(void) {
    switch (scene) {
        case SC_HUB:      draw_hub();      break;
        case SC_MAP:      draw_map();      break;
        case SC_MISSION:  draw_mission();  break;
        case SC_DIALOGUE: draw_dialogue(); break;
        case SC_END:      draw_end();      break;
    }
}
