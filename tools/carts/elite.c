#include "studio.h"

// ELITE  — cockpit wireframe dogfight
// Pirates approach as 3D wireframes out of the dark. Pitch and yaw to line one
// up in your crosshair, then fire your lasers. The elliptical scanner shows
// every ship around you: position around the ring + a stalk for above/below.
// Your shields recharge between hits; lose them and you're space dust.
// Arrows / WASD: pitch + yaw    A or Z: fire lasers

#define NENEMY  5
#define NSTAR   64
#define FOV     170.0f
#define CX      160
#define CYV     80          // centre of the cockpit view
#define NEAR    12.0f
#define FAR     780.0f
#define ROT     1.7f        // degrees turned per frame
#define RANGE   620.0f      // scanner range

typedef struct { V3 p; int shields; bool alive; float fire_cd, roll; } Ship;   // V3 is an engine type now (studio.h)

// a little dart-shaped ship: nose points toward the camera (-z)
static const V3 MODEL[5] = {
    {   0,  0, -26 },   // 0 nose
    { -20,  0,  12 },   // 1 left wing
    {  20,  0,  12 },   // 2 right wing
    {   0,  9,   8 },   // 3 top fin
    {   0, -5,  10 },   // 4 belly
};
static const int EDGE[9][2] = { {0,1},{0,2},{1,2},{0,3},{1,3},{2,3},{0,4},{1,4},{2,4} };

static Ship  enemy[NENEMY];
static V3    stars[NSTAR];
static int   shields, score, kills, hiscore, state;
static int   laser_t, hit_flash;
// expanding-ring explosion
static V3    boom_p; static int boom_t;

// ====================================================================

static void rotY(V3 *p, float s, float c) { float nx = p->x * c + p->z * s, nz = -p->x * s + p->z * c; p->x = nx; p->z = nz; }
static void rotX(V3 *p, float s, float c) { float ny = p->y * c - p->z * s, nz = p->y * s + p->z * c; p->y = ny; p->z = nz; }

static bool project(V3 p, int *sx, int *sy) {
    if (p.z <= NEAR) return false;
    *sx = CX + (int)(p.x * FOV / p.z);
    *sy = CYV - (int)(p.y * FOV / p.z);
    return true;
}

static void spawn_enemy(int i) {
    enemy[i].p.x = rnd_between(-200, 200);
    enemy[i].p.y = rnd_between(-130, 130);
    enemy[i].p.z = rnd_between(450, (int)FAR);
    enemy[i].shields = 2 + kills / 6;
    enemy[i].alive = true;
    enemy[i].fire_cd = rnd_between(40, 120);
    enemy[i].roll = rnd(360);
}

static void new_game() {
    shields = 100; score = 0; kills = 0; state = 0; laser_t = 0; hit_flash = 0; boom_t = 0;
    for (int i = 0; i < NENEMY; i++) spawn_enemy(i);
    for (int i = 0; i < NSTAR; i++) stars[i] = (V3){ rnd_between(-400, 400), rnd_between(-300, 300), rnd_between(NEAR + 5, (int)FAR) };
}

void init() { hiscore = load(0); new_game(); }

// ====================================================================

static void rotate_world(float yaw, float pitch) {
    float ys = sin_deg(yaw), yc = cos_deg(yaw), ps = sin_deg(pitch), pc = cos_deg(pitch);
    for (int i = 0; i < NENEMY; i++) if (enemy[i].alive) { rotY(&enemy[i].p, ys, yc); rotX(&enemy[i].p, ps, pc); }
    for (int i = 0; i < NSTAR; i++) { rotY(&stars[i], ys, yc); rotX(&stars[i], ps, pc); }
}

void update() {
    if (state != 0) { if (btnp(0, BTN_A) || btnp(0, BTN_B)) new_game(); return; }

    if (laser_t > 0) laser_t--;
    if (hit_flash > 0) hit_flash--;
    if (boom_t > 0) boom_t--;

    float yaw = 0, pitch = 0;
    if (btn(0, BTN_LEFT))  yaw   = -ROT;
    if (btn(0, BTN_RIGHT)) yaw   =  ROT;
    if (btn(0, BTN_UP))    pitch =  ROT;
    if (btn(0, BTN_DOWN))  pitch = -ROT;
    rotate_world(yaw, pitch);

    // stars drift past (gives a sense of forward cruise)
    for (int i = 0; i < NSTAR; i++) {
        stars[i].z -= 3.0f;
        if (stars[i].z <= NEAR) stars[i] = (V3){ rnd_between(-400, 400), rnd_between(-300, 300), FAR };
    }

    // enemies approach + occasionally shoot
    for (int i = 0; i < NENEMY; i++) {
        Ship *e = &enemy[i];
        if (!e->alive) continue;
        e->p.z -= 2.4f;
        e->p.x += sin_deg(now() * 40 + i * 60) * 1.2f;
        e->p.y += cos_deg(now() * 30 + i * 90) * 0.9f;
        e->roll += 1.5f;
        if (e->p.z < 30) { spawn_enemy(i); continue; }      // flew past — comes round again

        int sx, sy;
        if (project(e->p, &sx, &sy) && e->p.z < 460) {
            bool aimed = abs(sx - CX) < 44 && abs(sy - CYV) < 32;
            if (aimed && (e->fire_cd -= 1) <= 0) {
                e->fire_cd = rnd_between(50, 130);
                shields -= 9; hit_flash = 6;
                note(40, INSTR_NOISE, 4);
                if (shields <= 0) { state = 1; if (score > hiscore) { hiscore = score; save(0, hiscore); } }
            }
        }
    }

    if (shields < 100 && frame() % 12 == 0) shields++;       // recharge

    // fire (hold with a short cooldown)
    if ((btn(0, BTN_A) || btn(0, BTN_B)) && laser_t == 0) {
        laser_t = 6;
        note(70, INSTR_SAW, 2);
        // hit the enemy nearest the crosshair, in front and in range
        int best = -1; float bestd = 46;
        for (int i = 0; i < NENEMY; i++) {
            int sx, sy;
            if (!enemy[i].alive || enemy[i].p.z > 520 || !project(enemy[i].p, &sx, &sy)) continue;
            float d = distance(sx, sy, CX, CYV);
            if (d < bestd) { bestd = d; best = i; }
        }
        if (best >= 0) {
            if (--enemy[best].shields <= 0) {
                boom_p = enemy[best].p; boom_t = 16;
                score += 25 + kills; kills++;
                note(48, INSTR_NOISE, 5);
                spawn_enemy(best);
            } else note(60, INSTR_SQUARE, 2);
        }
    }
}

// ====================================================================
// drawing
// ====================================================================

static void draw_enemy(Ship *e) {
    int sx[5], sy[5]; bool ok[5];
    float rs = sin_deg(e->roll), rc = cos_deg(e->roll);
    for (int k = 0; k < 5; k++) {
        V3 v = MODEL[k];
        float nx = v.x * rc - v.y * rs, ny = v.x * rs + v.y * rc;   // gentle roll
        V3 w = { e->p.x + nx, e->p.y + ny, e->p.z + v.z };
        ok[k] = project(w, &sx[k], &sy[k]);
    }
    int col = e->shields <= 1 ? CLR_RED : CLR_LIGHT_GREY;
    for (int j = 0; j < 9; j++) {
        int a = EDGE[j][0], b = EDGE[j][1];
        if (ok[a] && ok[b]) line(sx[a], sy[a], sx[b], sy[b], col);
    }
}

void draw() {
    cls(CLR_BLACK);
    if (hit_flash > 0) rectfill(8, 12, 304, 136, CLR_DARK_RED);

    // clip the view to the cockpit window
    clip(8, 12, 304, 136);

    for (int i = 0; i < NSTAR; i++) {
        int sx, sy;
        if (project(stars[i], &sx, &sy)) pset(sx, sy, stars[i].z < 300 ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    for (int i = 0; i < NENEMY; i++) if (enemy[i].alive) draw_enemy(&enemy[i]);

    // lasers converge on the crosshair
    if (laser_t > 0) {
        line(12, 146, CX, CYV, CLR_RED);
        line(308, 146, CX, CYV, CLR_RED);
    }
    // explosion ring
    if (boom_t > 0) {
        int bx, by;
        if (project(boom_p, &bx, &by)) {
            int r = (16 - boom_t) * 2;
            circ(bx, by, r, CLR_ORANGE); circ(bx, by, r / 2, CLR_YELLOW);
        }
    }

    clip(0, 0, 0, 0);

    // crosshair
    line(CX - 8, CYV, CX - 3, CYV, CLR_GREEN); line(CX + 3, CYV, CX + 8, CYV, CLR_GREEN);
    line(CX, CYV - 8, CX, CYV - 3, CLR_GREEN); line(CX, CYV + 3, CX, CYV + 8, CLR_GREEN);

    // cockpit frame
    rect(7, 11, 306, 138, CLR_TRUE_BLUE);
    rect(6, 10, 308, 140, CLR_DARK_BLUE);

    // ---- scanner (the elliptical Elite radar) ----
    int RY = 176, RW = 60, RH = 18;
    for (int a = 0; a < 360; a += 12) {
        int x1 = CX + (int)(cos_deg(a) * RW), y1 = RY + (int)(sin_deg(a) * RH);
        int x2 = CX + (int)(cos_deg(a + 12) * RW), y2 = RY + (int)(sin_deg(a + 12) * RH);
        line(x1, y1, x2, y2, CLR_DARK_GREEN);
    }
    line(CX - RW, RY, CX + RW, RY, CLR_DARK_GREEN);
    for (int i = 0; i < NENEMY; i++) {
        if (!enemy[i].alive) continue;
        float nx = clamp(enemy[i].p.x / RANGE, -1, 1);
        float nz = clamp(enemy[i].p.z / RANGE, -1, 1);
        int bx = CX + (int)(nx * RW);
        int by = RY - (int)(nz * RH);                 // ahead → toward top
        int stalk = (int)(enemy[i].p.y / RANGE * 14); // above/below
        line(bx, by, bx, by - stalk, CLR_DARK_GREEN);
        rectfill(bx - 1, by - stalk - 1, 3, 3, enemy[i].p.z < 460 ? CLR_RED : CLR_YELLOW);
    }

    // ---- HUD ----
    rect(8, 152, 70, 12, CLR_DARK_GREEN);
    int sw = 66 * (shields < 0 ? 0 : shields) / 100;
    rectfill(10, 154, sw, 8, shields > 33 ? CLR_GREEN : CLR_RED);
    print("SHIELD", 14, 155, CLR_BLACK);
    print(str("SCORE %d", score), 4, 190, CLR_WHITE);
    print_right(str("KILLS %d", kills), SCREEN_W - 4, 190, CLR_LIGHT_GREY);
    print_right(str("BEST %d", hiscore), SCREEN_W - 4, 154, CLR_YELLOW);

    if (state == 1) {
        rectfill(CX - 70, CYV - 16, 140, 40, CLR_BLACK);
        rect    (CX - 70, CYV - 16, 140, 40, CLR_RED);
        print_centered("SHIP DESTROYED", SCREEN_W/2, CYV - 8, CLR_RED);
        print_centered(str("score %d", score), SCREEN_W/2, CYV + 2, CLR_YELLOW);
        print_centered("Z to relaunch", SCREEN_W/2, CYV + 13, CLR_LIGHT_GREY);
    }
}
