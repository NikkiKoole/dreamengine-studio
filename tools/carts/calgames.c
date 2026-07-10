/* de:meta
{
  "slug": "calgames",
  "title": "California Games: Half-Pipe",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "spring-damper",
    "particle-system",
    "title-play-gameover-loop"
  ],
  "lineage": "Homage to California Games (1987) half-pipe; the ramp is modeled as a pendulum spring (du += -u * k * dt) with a separate free-flight parabola for air tricks, over a euclidean-hat / filtered-saw synth bed.",
  "genre": "sports",
  "homage": "California Games (1987)",
  "description": "Carve a neon half-pipe under a banded synthwave sunset in this Epyx-style vert event. Your skater swings wall to wall as a pendulum: tap PUMP at the bottom of the ramp (on the beat for extra pop) to build speed, fire off the lip into the air, then throw kickflips, 360s, methods, indys and ollies for points that only bank if you stick the landing upright over the wall — over-rotate and you BAIL with a screen-shaking slam. A filtered synth bass, euclidean hats and a pentatonic arp react to your airtime over a 60-second high-score chase with a saved best. Controls: Down/Z = pump at the bottom; in the air Z = kickflip, X = 360, Left/Right = method/indy grab, Up = ollie/straighten; Z or X to start and restart.",
  "todo": [
    "Bug: the guy leaves a trail.",
    "Unclear how to steer (is it Z?); the mouse sends him flying; he's rotated the wrong way — currently unplayable.",
    "If steering works, add a touch of verlet to make it look nicer."
  ]
}
de:meta */
#include "studio.h"
#include <stddef.h>

// CALIFORNIA GAMES — HALF-PIPE
// One Epyx-style vert event under a synthwave sunset. You swing wall-to-wall in a
// U-ramp. PUMP at the bottom to gain speed; launch off the lip; throw TRICKS in the
// air for points; land clean (upright over the wall) or BAIL. 60-second high-score chase.
//
// PUMP:   Down / Z   (tap as you cross the bottom — on the beat is best)
// AIR:    Z=Kickflip  X=360  Left/Right=Method/Indy grab  Up=Ollie straighten
// START / RESTART:  Z or X

// ---------- ramp geometry --------------------------------------------------
// The pipe is a U: two quarter-pipe walls + a flat bottom. We model the skater's
// position as a single value u in [-1..1]: -1 = top of left wall, 0 = bottom-center,
// +1 = top of right wall. Speed lives as du/dt; gravity pulls toward 0, pumping adds it.

#define CX        (SCREEN_W / 2)        // ramp center x
#define HALF      120                   // half the ramp opening width
#define BOTTOM_Y  176                   // y of the flat bottom
#define LIP_Y     58                    // y of the wall lips (top)
#define WALL_DEG  78.0f                 // steepest wall lean (degrees from vertical-ish)

// states
#define ST_TITLE  0
#define ST_PLAY   1
#define ST_OVER   2

static int   state;
static float runtime;                   // seconds left
static int   score, best, combo;

// swing
static float u, du;                     // position [-1..1] and velocity
static float lean;                      // skater body lean in degrees (0 = upright)
static bool  airborne;
static float air_x, air_y, air_vx, air_vy;   // free-flight position/velocity (pixels)
static float spin;                      // accumulated rotation in the air (degrees)
static float spin_v;                    // spin speed
static int   launch_side;               // -1 left, +1 right
static float air_score;                 // points banked this air, scored only on clean land
static int   trick_id;                  // current trick (for the label)
static float bail_t;                    // bail recovery timer
static int   pump_flash;                // frames of "PUMP!" cue
static int   pump_good;                 // frames of on-beat-pump glow
static float air_apex;                  // highest reached (for big-air bonus)

// floating score popup
static float pop_y, pop_t; static int pop_val; static int pop_col;

// particles
typedef struct { float x, y, vx, vy; int life, max, col; } Part;
static Part parts[80];
static void spawn(float x, float y, float vx, float vy, int life, int col) {
    for (int i = 0; i < 80; i++) if (parts[i].life <= 0) {
        parts[i] = (Part){ x, y, vx, vy, life, life, col }; return; }
}

static const char *trick_name(int t) {
    switch (t) {
        case 1: return "KICKFLIP";
        case 2: return "360 SPIN";
        case 3: return "METHOD AIR";
        case 4: return "INDY GRAB";
        case 5: return "OLLIE";
        default: return "";
    }
}

// map u -> screen point on the ramp surface and the surface tangent angle
static float surf_x(float uu) { return CX + uu * HALF; }
static float surf_y(float uu) {
    // flat-ish near 0, curving up to the lips near +-1 (a smooth U via 1-cos)
    float a = uu < 0 ? -uu : uu;
    float c = (1.0f - cos_deg(a * 90.0f));      // 0 at center, 1 at lip
    return BOTTOM_Y - c * (BOTTOM_Y - LIP_Y);
}
// wall angle (how steep the ramp leans) at position u, in degrees, signed
static float surf_lean(float uu) {
    return uu * WALL_DEG;                        // 0 flat, +-WALL_DEG at the lips
}

static void reset_run(void) {
    state = ST_PLAY; runtime = 60.0f; score = 0; combo = 0;
    u = -0.45f; du = 0.9f; lean = surf_lean(u);
    airborne = false; spin = 0; spin_v = 0; air_score = 0; trick_id = 0;
    bail_t = 0; pump_flash = 0; pump_good = 0; air_apex = BOTTOM_Y;
    pop_t = 0;
    for (int i = 0; i < 80; i++) parts[i].life = 0;
}

void init(void) {
    best = load_int("calgames_best", 0);
    state = ST_TITLE;
    u = -0.45f; du = 0.9f; lean = surf_lean(u);
    for (int i = 0; i < 80; i++) parts[i].life = 0;
    // synth voices
    instrument(5, INSTR_SAW, 4, 120, 4, 180);            // warm bass
    instrument_filter(5, FILTER_LOW, 700, 6);
    instrument(6, INSTR_SQUARE, 2, 90, 3, 140);          // arp lead
    instrument_duty(6, 0.30f);
    instrument_lfo(6, 0, LFO_PITCH, 5.5f, 0.25f);
    bpm(118);
}

static bool pump_pressed(void) {
    return btnp(0, BTN_DOWN) || btnp(0, BTN_A) || btnp(1, BTN_DOWN) || btnp(1, BTN_A)
        || touch_count() > 0;
}

static void start_pop(int val, float y, int col) { pop_val = val; pop_y = y; pop_t = 1.0f; pop_col = col; }

// ---------- air handling ---------------------------------------------------
static void enter_air(void) {
    airborne = true;
    launch_side = u < 0 ? -1 : 1;
    float lx = surf_x(u), ly = surf_y(u);
    air_x = lx; air_y = ly;
    // launch velocity: tangent to the lip, scaled by swing energy
    float energy = du < 0 ? -du : du;
    float speed = clamp(energy * 130.0f, 40.0f, 320.0f);
    // off the right lip you fly up-and-left, off the left lip up-and-right
    air_vx = -launch_side * speed * 0.45f;
    air_vy = -speed;                                   // up
    spin = 0; spin_v = 0; air_score = 0; trick_id = 0; air_apex = ly;
    // launch sparks
    for (int i = 0; i < 10; i++)
        spawn(lx, ly, rnd_float_between(-1.5f, 1.5f), rnd_float_between(-2.2f, -0.4f),
              rnd_between(12, 22), (i & 1) ? CLR_YELLOW : CLR_LIGHT_PEACH);
    hit(76, INSTR_SQUARE, 3, 60);
}

static void bail(void) {
    airborne = false; bail_t = 1.1f; combo = 0; air_score = 0; trick_id = 0;
    u = clamp(u, -0.92f, 0.92f); du = 0;
    shake(6);
    for (int i = 0; i < 20; i++)
        spawn(air_x, air_y, rnd_float_between(-2.6f, 2.6f), rnd_float_between(-2.8f, 1.0f),
              rnd_between(16, 28), (i & 1) ? CLR_RED : CLR_ORANGE);
    hit(36, INSTR_NOISE, 6, 240);
    start_pop(0, air_y, CLR_RED);
}

static void land_clean(void) {
    airborne = false;
    // convert vertical speed back into swing energy, point skater into the wall
    float incoming = (air_vy > 0 ? air_vy : -air_vy);
    du = -launch_side * clamp(incoming / 150.0f, 0.5f, 1.6f);
    lean = surf_lean(u);
    int big = (int)((BOTTOM_Y - air_apex) * 0.6f);      // height bonus
    int gained = (int)air_score + big;
    combo++;
    if (combo > 1) gained = (int)(gained * (1.0f + combo * 0.15f));
    score += gained;
    if (gained > 0) {
        start_pop(gained, air_y, combo > 1 ? CLR_GREEN : CLR_YELLOW);
        for (int i = 0; i < 12; i++)
            spawn(air_x, air_y, rnd_float_between(-1.8f, 1.8f), rnd_float_between(-2.4f, -0.2f),
                  rnd_between(14, 24), CLR_GREEN);
        strum(60, CHORD_MAJ7, INSTR_SQUARE, 4, 45);
    }
    air_score = 0; trick_id = 0;
}

void update(void) {
    float d = dt();

    // particles + popup always tick
    for (int i = 0; i < 80; i++) if (parts[i].life > 0) {
        parts[i].x += parts[i].vx; parts[i].y += parts[i].vy; parts[i].vy += 0.12f;
        parts[i].life--;
    }
    if (pop_t > 0) { pop_t -= d * 1.4f; pop_y -= d * 22.0f; if (pop_t < 0) pop_t = 0; }

    // ---- synth bed (plays under everything once running) ----
    if (state == ST_PLAY) {
        if (every(1)) {
            int root = airborne ? 31 : 29;             // bass note rises while airborne
            note(root, 5, 4);
        }
        if (euclid(5, 8, beat())) hit(80, INSTR_NOISE, 1, 26);   // hats
        if (every(2)) {
            int deg = degree(SCALE_PENTA, 4, beat());
            note(deg, 6, airborne ? 4 : 2);
        }
    }

    if (state == ST_TITLE) {
        // idle swing behind the title
        du += -sgn((int)(u * 1000)) * 1.6f * d;
        u += du * d; lean = surf_lean(u);
        if (btnp(0, BTN_A) || btnp(0, BTN_B) || btnp(1, BTN_A) || keyp(KEY_SPACE) || touch_count() > 0)
            reset_run();
        return;
    }
    if (state == ST_OVER) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B) || btnp(1, BTN_A) || keyp(KEY_SPACE) || touch_count() > 0)
            reset_run();
        return;
    }

    // ---- PLAY ----
    runtime -= d;
    if (pump_flash > 0) pump_flash--;
    if (pump_good > 0)  pump_good--;
    if (bail_t > 0)     bail_t -= d;

    if (runtime <= 0) {
        runtime = 0; state = ST_OVER;
        if (score > best) { best = score; save_int("calgames_best", best); }
        return;
    }

    if (airborne) {
        // free flight
        air_vy += 540.0f * d;                          // gravity (px/s^2)
        air_x  += air_vx * d;
        air_y  += air_vy * d;
        if (air_y < air_apex) air_apex = air_y;
        spin += spin_v * d;

        // trick input — start/keep a trick, build air points
        if (btnp(0, BTN_A) || btnp(1, BTN_A)) { trick_id = 1; spin_v = launch_side * -520; air_score += 40; hit(72, INSTR_SQUARE, 3, 50); }
        else if (btnp(0, BTN_B) || btnp(1, BTN_B)) { trick_id = 2; spin_v = launch_side * -360; air_score += 50; hit(67, INSTR_SQUARE, 3, 50); }
        else if (btnp(0, BTN_LEFT) || btnp(1, BTN_LEFT))  { trick_id = 3; air_score += 35; hit(64, INSTR_TRI, 4, 80); }
        else if (btnp(0, BTN_RIGHT) || btnp(1, BTN_RIGHT)) { trick_id = 4; air_score += 35; hit(64, INSTR_TRI, 4, 80); }
        else if (btnp(0, BTN_UP) || btnp(1, BTN_UP)) { trick_id = 5; spin_v *= 0.4f; air_score += 20; hit(70, INSTR_SQUARE, 2, 40); }

        // continuous spin keeps banking small points (a held flip)
        air_score += (spin_v < 0 ? -spin_v : spin_v) * d * 0.20f;
        if (frame() % 3 == 0)
            spawn(air_x, air_y, rnd_float_between(-0.6f, 0.6f), rnd_float_between(-0.3f, 0.3f), 10, CLR_PINK);

        // landing test: came back down to the ramp surface near the launch lip
        float target_u = launch_side * 0.92f;          // approx wall we left from
        float sy = surf_y(target_u);
        if (air_vy > 0 && air_y >= sy - 2) {
            u = target_u;
            // clean if the skater is roughly upright (net spin near a multiple of 360)
            float r = spin; while (r < 0) r += 360; while (r >= 360) r -= 360;
            bool clean = (r < 55 || r > 305);
            if (clean) land_clean(); else bail();
            spin = 0; spin_v = 0;
        }
        return;
    }

    // ---- on-ramp swing (pendulum) ----
    if (bail_t > 0) {
        // recover: settle toward the bottom slowly, no input
        du += -u * 2.2f * d;
        du *= 0.985f;
        u += du * d; lean = surf_lean(u);
        return;
    }

    // gravity pulls toward bottom (u=0); proportional to current u (small-angle pendulum)
    du += -u * 3.4f * d;
    // mild friction
    du *= (1.0f - 0.18f * d);
    u  += du * d;
    lean = lerp(lean, surf_lean(u), clamp(8 * d, 0, 1));

    // PUMP window: near the bottom and moving
    bool in_window = (u > -0.32f && u < 0.32f);
    if (in_window && (du > 0.15f || du < -0.15f)) pump_flash = 4;
    if (pump_pressed()) {
        if (in_window) {
            float bp = beat_pos();
            bool on_beat = (bp < 0.18f || bp > 0.82f);
            float gain = on_beat ? 0.55f : 0.30f;
            // pump adds energy in the current direction of travel
            du += sgn((int)(du * 1000)) * gain;
            if (on_beat) { pump_good = 10; hit(48, INSTR_SQUARE, 4, 70); }
            else hit(44, INSTR_TRI, 3, 60);
            for (int i = 0; i < 6; i++)
                spawn(surf_x(u), surf_y(u), rnd_float_between(-1.0f, 1.0f),
                      rnd_float_between(-1.6f, -0.2f), 12, on_beat ? CLR_GREEN : CLR_LIGHT_GREY);
        }
    }

    // launch: swing reached past a lip
    if (u >= 1.0f || u <= -1.0f) { u = clamp(u, -1.0f, 1.0f); enter_air(); }
}

// ---------- drawing --------------------------------------------------------
static void draw_sky(void) {
    // synthwave gradient bands
    int bands[7] = { CLR_DARK_PURPLE, CLR_DARKER_PURPLE, CLR_MAUVE, CLR_DARK_RED,
                     CLR_DARK_ORANGE, CLR_ORANGE, CLR_PEACH };
    int top = 0, h = 120;
    for (int i = 0; i < 7; i++) {
        int y0 = top + i * h / 7;
        rectfill(0, y0, SCREEN_W, h / 7 + 1, bands[i]);
    }
    // banded setting sun
    int sx = CX, sy = 78, sr = 34;
    for (int yy = -sr; yy <= sr; yy++) {
        if (((sy + yy) / 3) % 2 == 0 && yy > -8) continue;   // horizontal scan-bands lower half
        int w = (int)fsqrt((float)(sr * sr - yy * yy));
        int col = yy < -6 ? CLR_LIGHT_YELLOW : (yy < 6 ? CLR_YELLOW : CLR_ORANGE);
        line(sx - w, sy + yy, sx + w, sy + yy, col);
    }
    // neon perspective grid on the horizon
    int hz = 120;
    rectfill(0, hz, SCREEN_W, 6, CLR_DARKER_PURPLE);
    float scroll = now() * 18.0f; int off = (int)scroll % 14;
    for (int i = 0; i < 10; i++) {
        int gy = hz + 6 + i * i;
        if (gy > SCREEN_H) break;
        line(0, gy, SCREEN_W, gy, CLR_DARK_PURPLE);
    }
    for (int gx = -off; gx < SCREEN_W; gx += 14)
        line(gx, hz + 6, (gx - CX) * 3 + CX, SCREEN_H, CLR_DARKER_PURPLE);
}

static void draw_ramp(void) {
    // fill the U interior with a smooth surface using vertical slabs across u
    int prevx = -1, prevy = 0;
    for (int s = -100; s <= 100; s++) {
        float uu = s / 100.0f;
        int x = (int)surf_x(uu), y = (int)surf_y(uu);
        // ramp body
        rectfill(x, y, 2, BOTTOM_Y + 12 - y, CLR_INDIGO);
        if (prevx >= 0) line(prevx, prevy, x, y, CLR_WHITE);  // coping/surface line
        prevx = x; prevy = y;
    }
    // floor under the bottom
    rectfill(0, BOTTOM_Y + 12, SCREEN_W, SCREEN_H - (BOTTOM_Y + 12), CLR_DARKER_PURPLE);
    // lip copings
    rectfill((int)surf_x(-1.0f) - 4, LIP_Y - 3, 8, 6, CLR_LIGHT_GREY);
    rectfill((int)surf_x( 1.0f) - 4, LIP_Y - 3, 8, 6, CLR_LIGHT_GREY);
    // center pump zone glow
    if (pump_flash > 0 || pump_good > 0) {
        int c = pump_good > 0 ? CLR_GREEN : CLR_LIGHT_YELLOW;
        line((int)surf_x(-0.32f), (int)surf_y(-0.32f), (int)surf_x(0.32f), (int)surf_y(0.32f), c);
    }
}

// the skater: a little body + board, drawn rotated by `ang`, centered at (cx,cy)
static void draw_skater(float cx, float cy, float ang) {
    // build local-space points, rotate by ang, translate
    float cs = cos_deg(ang), sn = sin_deg(ang);
    #define ROTX(lx,ly) (cx + (lx) * cs - (ly) * sn)
    #define ROTY(lx,ly) (cy + (lx) * sn + (ly) * cs)
    // board
    int bx0 = (int)ROTX(-7, 6), by0 = (int)ROTY(-7, 6);
    int bx1 = (int)ROTX( 7, 6), by1 = (int)ROTY( 7, 6);
    line(bx0, by0, bx1, by1, CLR_DARK_ORANGE);
    // wheels
    circfill((int)ROTX(-5, 8), (int)ROTY(-5, 8), 1, CLR_YELLOW);
    circfill((int)ROTX( 5, 8), (int)ROTY( 5, 8), 1, CLR_YELLOW);
    // legs
    line((int)ROTX(-3, 5), (int)ROTY(-3, 5), (int)ROTX(-1, -2), (int)ROTY(-1, -2), CLR_TRUE_BLUE);
    line((int)ROTX( 3, 5), (int)ROTY( 3, 5), (int)ROTX( 1, -2), (int)ROTY( 1, -2), CLR_TRUE_BLUE);
    // torso
    line((int)ROTX(0, -2), (int)ROTY(0, -2), (int)ROTX(0, -9), (int)ROTY(0, -9), CLR_RED);
    // arms
    line((int)ROTX(0, -7), (int)ROTY(0, -7), (int)ROTX(-6, -4), (int)ROTY(-6, -4), CLR_RED);
    line((int)ROTX(0, -7), (int)ROTY(0, -7), (int)ROTX( 6, -5), (int)ROTY( 6, -5), CLR_RED);
    // head
    circfill((int)ROTX(0, -11), (int)ROTY(0, -11), 3, CLR_PEACH);
    #undef ROTX
    #undef ROTY
}

static void draw_hud(void) {
    // timer bar
    rectfill(8, 6, 120, 7, CLR_DARKER_PURPLE);
    rectfill(8, 6, (int)(120 * (runtime / 60.0f)), 7, runtime < 10 ? CLR_RED : CLR_GREEN);
    rect(8, 6, 120, 7, CLR_WHITE);
    print(str("%02d", (int)runtime), 132, 5, CLR_WHITE);
    print_right(str("SCORE %d", score), SCREEN_W - 6, 5, CLR_YELLOW);
    if (combo > 1) print_right(str("x%d COMBO", combo), SCREEN_W - 6, 15, CLR_GREEN);

    // air read-out
    if (airborne) {
        const char *t = trick_name(trick_id);
        if (t[0]) print_centered(t, SCREEN_W/2, 24, CLR_WHITE);
        print_centered(str("+%d", (int)air_score), SCREEN_W/2, 34, CLR_LIGHT_YELLOW);
    } else if (pump_flash > 0) {
        print_centered("PUMP!", SCREEN_W/2, 150, pump_good > 0 ? CLR_GREEN : CLR_LIGHT_YELLOW);
    } else if (bail_t > 0) {
        print_centered("BAIL!", SCREEN_W/2, 150, CLR_RED);
    }
}

static void draw_particles(void) {
    for (int i = 0; i < 80; i++) if (parts[i].life > 0) {
        int c = parts[i].col;
        if (parts[i].life < parts[i].max / 3) c = CLR_DARK_PURPLE;
        pset((int)parts[i].x, (int)parts[i].y, c);
    }
}

static void draw_world_skater(void) {
    if (airborne) {
        draw_skater(air_x, air_y - 13, spin);
    } else if (bail_t > 0) {
        float sx = surf_x(u), sy = surf_y(u);
        draw_skater(sx, sy - 6, 90 + 18 * sin_deg(now() * 400));   // crumpled
    } else {
        float sx = surf_x(u), sy = surf_y(u);
        draw_skater(sx, sy - 13, lean);
    }
}

void draw(void) {
    // a brief full-red slam flash on a fresh bail
    if (state == ST_PLAY && bail_t > 0.9f) {
        cls(CLR_RED);
        draw_ramp();
        draw_world_skater();
        draw_hud();
        return;
    }

    draw_sky();
    draw_ramp();
    draw_particles();

    if (state == ST_TITLE) {
        float sx = surf_x(u), sy = surf_y(u);
        draw_skater(sx, sy - 13, lean);
        print_scaled("CALIFORNIA", CX - 80, 28, CLR_LIGHT_YELLOW, 2);
        print_scaled("GAMES", CX - 40, 46, CLR_PEACH, 2);
        print_centered("- HALF-PIPE -", SCREEN_W/2, 72, CLR_WHITE);
        if (blink(20)) print_centered("PRESS Z TO SKATE", SCREEN_W/2, 148, CLR_GREEN);
        print_centered(str("BEST  %d", best), SCREEN_W/2, 164, CLR_LIGHT_GREY);
        print_centered("PUMP at the bottom - TRICKS in the air", SCREEN_W/2, 184, CLR_INDIGO);
        return;
    }

    // PLAY + OVER both show the live world
    draw_world_skater();
    draw_hud();

    if (pop_t > 0 && pop_val > 0)
        print_centered(str("+%d", pop_val), SCREEN_W/2, (int)pop_y, pop_col);

    if (state == ST_OVER) {
        fade(0.45f);
        rectfill(70, 60, 180, 78, CLR_DARKER_PURPLE);
        rect(70, 60, 180, 78, CLR_PEACH);
        print_centered("RUN OVER", SCREEN_W/2, 68, CLR_PEACH);
        print_scaled(str("%d", score), CX - text_width(str("%d", score)), 84, CLR_LIGHT_YELLOW, 2);
        if (score >= best && score > 0) print_centered("NEW BEST!", SCREEN_W/2, 110, CLR_GREEN);
        else print_centered(str("BEST  %d", best), SCREEN_W/2, 110, CLR_LIGHT_GREY);
        if (blink(20)) print_centered("PRESS Z TO SKATE AGAIN", SCREEN_W/2, 124, CLR_WHITE);
    }
}
