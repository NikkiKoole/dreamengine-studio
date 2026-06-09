#include "studio.h"

// OERSOEP — the cell stage of Spore, in a drop of pond water. ("oersoep" is Dutch
// for primordial soup.) Swim a microbe toward the cursor, eat to earn DNA, then
// EVOLVE: at each DNA cap you mate and drop into a cell editor to bolt on a part —
// flagellum, spike, jaw, poison sac — that changes how you play. Climb the food
// chain from prey to apex over four tiers, then crawl ashore.
//
// The pond is a single boid flock. Each cell is judged against YOUR size every
// frame: smaller ones FLEE and can be eaten; bigger ones HUNT you and bite — a bite
// drains health and knocks you back, and zero health is death. So the same cell
// that chased you at tier 1 is lunch at tier 3. That two-way danger is the whole
// difference from rollswarm (which only ever lets you eat down, never be eaten).
//
//   MOUSE / touch : the cell always swims toward the pointer
//   at MATE       : reach the glowing twin to evolve
//   in the EDITOR : Left/Right pick a part, Z confirm
//   Z             : choose mouth on the title, replay when it's over

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

#define WORLDW  600
#define WORLDH  400
#define N        80           // flock size
#define NFOOD    70           // drifting food pool
#define NMOTE    64           // ambient motes + eat puffs
#define VIEW     22.0f        // boid neighbour radius
#define SEP2     90.0f        // squared distance below which boids push apart
#define FLEE_R   78.0f        // base flee/hunt sense radius (grows with your size)
#define TIERS     4           // evolutions before you can go ashore

// modes
#define M_START  0
#define M_SWIM   1
#define M_EDITOR 2
#define M_OVER   3

// diets
#define D_HERB   0            // plant flecks only
#define D_CARN   1            // meat gobbets + smaller cells
#define D_OMNI   2            // everything (the jaw upgrade)

// part bitmask
#define P_FLAG   1            // flagellum  — speed
#define P_SPIKE  2            // spike      — rammers recoil + shrink, you take no bite
#define P_JAW    4            // jaw        — diet becomes omnivore
#define P_POISON 8            // poison sac — aura shrinks + repels predators
#define PART(p)  (S->parts & (p))

// food kinds
#define F_PLANT  0
#define F_MEAT   1

typedef struct {
    float x, y, vx, vy;
    float size;               // body radius — gates eat-vs-hunt against the player
    float panic;              // 0..1 flee intensity (prey)
    float lunge;              // 0..1 hunt flash (predator)
    int   bite_cd;            // frames before this cell can bite again
    bool  alive;
} Microbe;

typedef struct { float x, y, vx, vy, wob; int kind; bool alive; } Food;
typedef struct { float x, y, vx, vy; int life, col; } Mote;

STATE {
    float px, py, vx, vy;     // cell centre + velocity
    float r;                  // body radius
    float health, maxhealth;
    float dna, dna_cap;
    int   tier;               // 1..TIERS
    int   diet;
    int   parts;
    float flagphase;          // tail wiggle
    int   combo;              // consecutive eats → rising pitch
    int   hitstop, flash, ifr;// juice + invuln after a bite
    int   mode;
    bool  won;

    bool  mating;             // DNA full → twin appears
    float matex, matey, matephase;

    int   ncards, card[4], sel;   // editor offer

    Microbe herd[N];
    Food    food[NFOOD];
    Mote    mote[NMOTE];
};

static const char *PART_NAME[4] = { "FLAGELLUM", "SPIKE", "JAW", "POISON SAC" };
static const char *PART_DESC[4] = { "swim faster", "rammers recoil", "eat anything", "burn hunters" };
static const int   PART_BIT [4] = { P_FLAG, P_SPIKE, P_JAW, P_POISON };

static void  step_flock(void);                                    // defined below init()

static float rcap(void)   { return 10.0f + S->tier * 6.0f; }      // radius ceiling this tier
static float capspeed(void) {
    float base = (PART(P_FLAG) ? 3.6f : 2.6f) - S->r * 0.012f;
    return clamp(base, 1.4f, 4.0f);
}
// can the player's mouth eat this food kind?
static bool eats_food(int kind) {
    if (S->diet == D_OMNI) return true;
    return kind == F_PLANT ? (S->diet == D_HERB) : (S->diet == D_CARN);
}
// can the player eat other CELLS (meat)? herbivores can't, until the jaw upgrade
static bool eats_cells(void) { return S->diet == D_CARN || S->diet == D_OMNI; }

static void puff(float x, float y, int n, int col) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < NMOTE; i++)
            if (S->mote[i].life <= 0) {
                S->mote[i] = (Mote){ x, y,
                    rnd_float_between(-1.2f, 1.2f), rnd_float_between(-1.2f, 1.2f),
                    rnd_between(10, 20), col };
                break;
            }
}

static void spawn_food(int idx, bool offscreen) {
    int kind = chance(55) ? F_PLANT : F_MEAT;
    float x = rnd_between(10, WORLDW - 10), y = rnd_between(10, WORLDH - 10);
    if (offscreen) {                       // restock from the edges
        if (chance(50)) x = chance(50) ? 4 : WORLDW - 4;
        else            y = chance(50) ? 4 : WORLDH - 4;
    }
    S->food[idx] = (Food){ x, y, rnd_float_between(-0.2f, 0.2f), rnd_float_between(-0.2f, 0.2f),
                           rnd(360), kind, true };
}

static void spawn_microbe(int idx) {
    // Sizes track your CURRENT radius so the pond is mostly prey with a minority of
    // predators. As you grow the whole pond scales with you, so there's always lunch
    // and always a threat — you survive by dodging and evolving, not by outgrowing it
    // (you can't: predators are always proportional). Herbivores can't eat cells to
    // thin the herd or grow fast, so the pond hunts a grazer LESS — fewer, gentler
    // predators — to keep that path alive.
    float r = S->r < 4 ? 4 : S->r;
    int pred_cut = (S->diet == D_HERB) ? 91 : 85;   // herb: ~9% predators, carn: ~15%
    int roll = rnd(100);
    float sz;
    if      (roll < 58)        sz = rnd_float_between(r * 0.30f, r * 0.85f);  // prey
    else if (roll < pred_cut)  sz = rnd_float_between(r * 0.90f, r * 1.10f);  // peers (neutral)
    else                       sz = rnd_float_between(r * 1.20f, r * 1.65f);  // predators
    float a = rnd(360);
    S->herd[idx] = (Microbe){
        rnd_between(20, WORLDW - 20), rnd_between(20, WORLDH - 20),
        cos_deg(a) * 1.2f, sin_deg(a) * 1.2f, sz, 0, 0, 0, true
    };
}

static void restock(void) {
    for (int i = 0; i < N; i++)     spawn_microbe(i);
    for (int i = 0; i < NFOOD; i++) spawn_food(i, false);
}

void init(void) {
    S->px = WORLDW / 2.0f; S->py = WORLDH / 2.0f; S->vx = S->vy = 0;
    S->r = 5.0f;
    S->maxhealth = 120; S->health = 120;
    S->tier = 1; S->dna = 0; S->dna_cap = 12;
    S->diet = D_HERB; S->parts = 0;
    S->combo = 0; S->hitstop = S->flash = S->ifr = 0;
    S->won = false; S->mating = false;
    S->mode = M_START;
    for (int i = 0; i < NMOTE; i++) S->mote[i].life = 0;
    restock();
    for (int i = 0; i < NMOTE; i++)          // a few ambient motes already drifting
        S->mote[i] = (Mote){ (float)rnd(WORLDW), (float)rnd(WORLDH),
            rnd_float_between(-0.3f, 0.3f), rnd_float_between(-0.3f, 0.3f),
            rnd_between(40, 200), CLR_DARK_BLUE };

    for (int i = 0; i < 60; i++) step_flock();       // let the flock spread before frame 1

    instrument(5, INSTR_TRI, 220, 160, 3, 420);     // soft anxious pad bed
    instrument_filter(5, FILTER_LOW, 650, 5);
    bpm(108);
}

// ----- boids: flee if smaller than you, hunt if bigger -----
static void step_flock(void) {
    for (int i = 0; i < N; i++) {
        if (!S->herd[i].alive) continue;
        Microbe *b = &S->herd[i];

        float sepx = 0, sepy = 0, alx = 0, aly = 0, cox = 0, coy = 0;
        int n = 0;
        for (int j = 0; j < N; j++) {
            if (j == i || !S->herd[j].alive) continue;
            float dx = S->herd[j].x - b->x, dy = S->herd[j].y - b->y;
            float d2 = dx * dx + dy * dy;
            if (d2 < VIEW * VIEW) {
                n++;
                alx += S->herd[j].vx; aly += S->herd[j].vy;
                cox += S->herd[j].x;  coy += S->herd[j].y;
                if (d2 < SEP2) { sepx -= dx; sepy -= dy; }
            }
        }
        if (n > 0) {
            alx /= n; aly /= n; cox /= n; coy /= n;
            b->vx += (alx - b->vx) * 0.05f + (cox - b->x) * 0.0010f + sepx * 0.020f;
            b->vy += (aly - b->vy) * 0.05f + (coy - b->y) * 0.0010f + sepy * 0.020f;
        }

        // relationship to the player, recomputed every frame
        bool predator = b->size > S->r * 1.08f;
        bool prey     = b->size <= S->r * 0.92f;

        float pdx = b->x - S->px, pdy = b->y - S->py;
        float pd  = fsqrt(pdx * pdx + pdy * pdy);
        if (predator) {
            // Hunters only notice you UP CLOSE — the pond is mostly ambient, and the
            // danger is blundering near a big mouth, not a pack converging from
            // off-screen. (Prey, below, are skittish from much farther.)
            float hunt_r = 42.0f + S->r;
            if (pd < hunt_r && pd > 0.01f) {
                float t = 1.0f - pd / hunt_r;
                b->lunge = t;
                float pull = 0.4f + t * 1.9f;
                b->vx -= pdx / pd * pull;
                b->vy -= pdy / pd * pull;
            }
        } else if (prey) {
            float sense = FLEE_R + S->r;       // prey bolt from far off
            if (pd < sense && pd > 0.01f) {
                float t = 1.0f - pd / sense;
                b->panic = t > b->panic ? t : b->panic;
                float push = 0.5f + t * t * 4.0f;
                b->vx += pdx / pd * push;
                b->vy += pdy / pd * push;
            }
        }
        b->panic *= 0.90f;
        b->lunge *= 0.90f;

        // caps tuned around the player's base swim (~2.6): prey top out just below it
        // (you slowly reel them in; a wall corner or a flagellum seals the chase),
        // and a hunter is only barely faster than you can flee — barely.
        float cap = predator ? 1.6f + b->lunge * 1.0f
                  : 1.3f + b->panic * 1.0f;
        float sp = fsqrt(b->vx * b->vx + b->vy * b->vy);
        if (sp > cap)                 { b->vx = b->vx / sp * cap;  b->vy = b->vy / sp * cap; }
        else if (sp > 0 && sp < 0.5f) { b->vx = b->vx / sp * 0.5f; b->vy = b->vy / sp * 0.5f; }

        b->x += b->vx; b->y += b->vy;
        if (b->bite_cd > 0) b->bite_cd--;

        if (b->x < 6)          { b->x = 6;          b->vx = -b->vx * 0.6f; }
        if (b->x > WORLDW - 6) { b->x = WORLDW - 6; b->vx = -b->vx * 0.6f; }
        if (b->y < 6)          { b->y = 6;          b->vy = -b->vy * 0.6f; }
        if (b->y > WORLDH - 6) { b->y = WORLDH - 6; b->vy = -b->vy * 0.6f; }
    }
}

static void enter_editor(void) {
    // offer the parts you don't have yet (shuffled, up to 3 cards)
    int pool[4], np = 0;
    for (int i = 0; i < 4; i++) if (!PART(PART_BIT[i])) pool[np++] = i;
    for (int i = 0; i < np; i++) { int j = i + rnd(np - i); int t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
    S->ncards = np < 3 ? np : 3;
    for (int i = 0; i < S->ncards; i++) S->card[i] = pool[i];
    S->sel = 0;
    S->mating = false;
    S->mode = M_EDITOR;
    strum(60, CHORD_MAJ, INSTR_TRI, 4, 40);
}

static void apply_part(int p) {
    S->parts |= PART_BIT[p];
    if (PART_BIT[p] == P_JAW) S->diet = D_OMNI;
    // grow into the new tier
    S->tier++;
    S->dna = 0; S->dna_cap = 12 + (S->tier - 1) * 9;
    S->maxhealth += 40; S->health = S->maxhealth;
    restock();
    S->mode = M_SWIM;
}

void update(void) {
    if (S->mode == M_START) {
        step_flock();                 // a living pond churns behind the menu
        S->flagphase += 14;
        if (btnp(0, BTN_A) || keyp('Z')) { S->diet = D_HERB; S->mode = M_SWIM; }
        if (btnp(0, BTN_B) || keyp('X')) { S->diet = D_CARN; S->mode = M_SWIM; }
        return;
    }
    if (S->mode == M_OVER) {
        if (btnp(0, BTN_A) || btnp(1, BTN_A) || keyp('Z')) init();
        return;
    }
    if (S->mode == M_EDITOR) {
        if (btnp(0, BTN_LEFT)  || btnp(1, BTN_LEFT))  S->sel = (S->sel + S->ncards - 1) % S->ncards;
        if (btnp(0, BTN_RIGHT) || btnp(1, BTN_RIGHT)) S->sel = (S->sel + 1) % S->ncards;
        if (btnp(0, BTN_A) || btnp(1, BTN_A) || keyp('Z')) {
            apply_part(S->card[S->sel]);
            chord(67, CHORD_MAJ, INSTR_TRI, 4);
        }
        return;
    }

    // ===== M_SWIM =====
    if (S->hitstop > 0) { S->hitstop--; if (S->flash > 0) S->flash--; return; }
    if (S->flash > 0) S->flash--;
    if (S->ifr > 0) S->ifr--;
    S->flagphase += 14;

    follow((int)S->px, (int)S->py, WORLDW, WORLDH);   // set camera BEFORE mouse_world read
    float tx = mouse_world_x(), ty = mouse_world_y();
    float dx = tx - S->px, dy = ty - S->py;
    float d = fsqrt(dx * dx + dy * dy);
    if (d > 3.0f) {
        float accel = PART(P_FLAG) ? 0.34f : 0.24f;
        S->vx += dx / d * accel;
        S->vy += dy / d * accel;
    }
    float cap = capspeed();
    float sp = fsqrt(S->vx * S->vx + S->vy * S->vy);
    if (sp > cap) { S->vx = S->vx / sp * cap; S->vy = S->vy / sp * cap; }
    S->vx *= 0.90f; S->vy *= 0.90f;
    S->px = clamp(S->px + S->vx, S->r, WORLDW - S->r);
    S->py = clamp(S->py + S->vy, S->r, WORLDH - S->r);

    step_flock();

    // ambient motes drift; eat-puffs decay
    for (int i = 0; i < NMOTE; i++) {
        if (S->mote[i].life <= 0) continue;
        S->mote[i].x += S->mote[i].vx; S->mote[i].y += S->mote[i].vy;
        S->mote[i].life--;
        if (S->mote[i].col == CLR_DARK_BLUE && S->mote[i].life < 20) S->mote[i].life = 200;  // ambient never dies
    }

    // food drifts; eat what your mouth allows
    for (int i = 0; i < NFOOD; i++) {
        if (!S->food[i].alive) continue;
        S->food[i].x += S->food[i].vx; S->food[i].y += S->food[i].vy;
        S->food[i].wob += 4;
        if (S->food[i].x < 2 || S->food[i].x > WORLDW - 2) S->food[i].vx *= -1;
        if (S->food[i].y < 2 || S->food[i].y > WORLDH - 2) S->food[i].vy *= -1;
        if (!eats_food(S->food[i].kind)) continue;
        if (distance((int)S->px, (int)S->py, (int)S->food[i].x, (int)S->food[i].y) < S->r + 3) {
            S->food[i].alive = false;
            S->dna += S->food[i].kind == F_MEAT ? 2.0f : 1.6f;   // plants pay a grazer well
            if (S->r < rcap()) S->r += 0.15f;
            S->combo++;
            puff(S->food[i].x, S->food[i].y, 3, S->food[i].kind == F_MEAT ? CLR_DARK_RED : CLR_LIME_GREEN);
            note(60 + (S->combo % 12), INSTR_SQUARE, 3);
            spawn_food(i, true);          // replace from the edges — pond never empties
        }
    }

    // cells: eat the small (if your diet allows), get bitten by the big
    for (int i = 0; i < N; i++) {
        if (!S->herd[i].alive) continue;
        Microbe *b = &S->herd[i];
        float dd = distance((int)S->px, (int)S->py, (int)b->x, (int)b->y);

        // EAT — smaller cell, in reach, carnivore/omni only
        if (eats_cells() && b->size <= S->r * 0.92f && dd < S->r + b->size * 0.6f + 2) {
            S->r = fsqrt(S->r * S->r + b->size * b->size * 0.55f);
            if (S->r > rcap()) S->r = rcap();
            S->dna += b->size * 0.8f;
            S->combo++;
            b->alive = false;
            puff(b->x, b->y, 6, CLR_DARK_RED);
            note(52 + (S->combo % 14), INSTR_SQUARE, 4);
            hit(46, INSTR_NOISE, 3, 40);
            shake(1.4f);
            spawn_microbe(i);             // a fresh cell drifts in elsewhere
            continue;
        }

        // BIG cell touches you
        bool predator = b->size > S->r * 1.08f;
        if (predator && dd < S->r + b->size * 0.5f) {
            if (PART(P_SPIKE)) {          // spike turns the tables
                if (b->bite_cd <= 0) {
                    b->size -= 2.5f; b->bite_cd = 30;
                    float a = angle_to((int)S->px, (int)S->py, (int)b->x, (int)b->y);
                    b->vx += cos_deg(a) * 4.0f; b->vy += sin_deg(a) * 4.0f;
                    puff(b->x, b->y, 4, CLR_LIGHT_GREY);
                    hit(64, INSTR_SQUARE, 3, 50);
                    if (b->size < 2.0f) b->alive = false;
                }
            } else if (S->ifr <= 0) {     // a real bite
                float dmg = 5.0f + b->size * 0.8f;
                S->health -= dmg;
                S->combo = 0;
                S->ifr = 42; S->flash = 4; S->hitstop = 3;
                float a = angle_to((int)b->x, (int)b->y, (int)S->px, (int)S->py);
                S->vx += cos_deg(a) * 5.0f; S->vy += sin_deg(a) * 5.0f;
                shake(3.5f);
                hit(34, INSTR_NOISE, 5, 110);
                puff(S->px, S->py, 5, CLR_RED);
                if (S->health <= 0) { S->health = 0; S->mode = M_OVER; S->won = false;
                                      shake(6); hit(28, INSTR_NOISE, 6, 220); return; }
            }
        }

        // POISON aura — shrink + repel anything bigger that's close
        if (PART(P_POISON) && predator && dd < S->r + 14) {
            b->size -= 0.06f;
            float a = angle_to((int)S->px, (int)S->py, (int)b->x, (int)b->y);
            b->vx += cos_deg(a) * 0.5f; b->vy += sin_deg(a) * 0.5f;
            if (frame() % 8 == 0) puff(b->x, b->y, 1, CLR_LIME_GREEN);
        }
    }

    // DNA full → a mate appears; reach it to evolve (or, at the apex, to win)
    if (!S->mating && S->dna >= S->dna_cap) {
        S->mating = true;
        float a = rnd(360);
        S->matex = clamp(S->px + cos_deg(a) * 140, 30, WORLDW - 30);
        S->matey = clamp(S->py + sin_deg(a) * 140, 30, WORLDH - 30);
        chord(55, CHORD_MAJ, INSTR_TRI, 3);
    }
    if (S->mating) {
        S->matephase += 6;
        if (distance((int)S->px, (int)S->py, (int)S->matex, (int)S->matey) < S->r + 10) {
            puff(S->matex, S->matey, 10, CLR_PINK);
            if (S->tier >= TIERS) {       // apex + bred = ashore
                S->mode = M_OVER; S->won = true;
                strum(60, CHORD_MAJ, INSTR_TRI, 5, 60);
            } else {
                enter_editor();
            }
        }
    }

    if (every(2)) note(36, 5, 2);         // low anxious pulse

#ifdef DE_TRACE
    watch("mode", "%d", S->mode);
    watch("tier", "%d", S->tier);
    watch("r",    "%.1f", S->r);
    watch("dna",  "%.1f", S->dna);
    watch("hp",   "%.0f", S->health);
    watch("mate", "%d", S->mating ? 1 : 0);
    watch("parts","%d", S->parts);
#endif
}

// ----- drawing -----
static void draw_microbe(const Microbe *b) {
    bool predator = b->size > S->r * 1.08f;
    bool prey     = b->size <= S->r * 0.92f;
    int body, rim;
    if      (predator) { body = CLR_RED;          rim = CLR_DARK_RED; }
    else if (prey)     { body = CLR_MEDIUM_GREEN; rim = CLR_DARK_GREEN; }
    else               { body = CLR_BLUE_GREEN;   rim = CLR_TRUE_BLUE; }
    if (prey      && b->panic > 0.55f) body = CLR_WHITE;       // terrified flash
    if (predator  && b->lunge > 0.45f) body = CLR_ORANGE;      // lunging
    int s = (int)b->size;
    circfill((int)b->x, (int)b->y, s, body);
    circ((int)b->x, (int)b->y, s, rim);
    if (s >= 4) pset((int)b->x, (int)b->y, rim);               // nucleus
}

static void draw_cell(float x, float y, float r, int body, int rim, int nuc, float heading, bool me) {
    // poison aura
    if (me && PART(P_POISON)) {
        fillp(FILL_CHECKER, -1);
        circfill((int)x, (int)y, (int)r + 12, CLR_LIME_GREEN);
        fillp_reset();
    }
    // flagellum tail, opposite heading, whipping on flagphase
    float ha = heading + 180;
    float wob = sin_deg(S->flagphase);
    int bx = (int)(x + cos_deg(ha) * r),        by = (int)(y + sin_deg(ha) * r);
    int ex = (int)(x + cos_deg(ha) * (r + r * 1.5f) + cos_deg(ha + 90) * wob * (r * 0.5f));
    int ey = (int)(y + sin_deg(ha) * (r + r * 1.5f) + sin_deg(ha + 90) * wob * (r * 0.5f));
    int cx = (int)(x + cos_deg(ha) * (r + r * 0.7f) + cos_deg(ha + 90) * wob * (r * 0.8f));
    int cy = (int)(y + sin_deg(ha) * (r + r * 0.7f) + sin_deg(ha + 90) * wob * (r * 0.8f));
    bezier(bx, by, cx, cy, ex, ey, rim);
    if (me && PART(P_FLAG)) bezier(bx, by, cx, cy, ex, ey, CLR_LIGHT_GREY);  // brighter, doubled

    // body
    circfill((int)x, (int)y, (int)r, body);
    circ((int)x, (int)y, (int)r, rim);
    // nucleus, offset toward heading
    circfill((int)(x + cos_deg(heading) * r * 0.25f), (int)(y + sin_deg(heading) * r * 0.25f),
             (int)(r * 0.35f) + 1, nuc);
    // specular highlight → reads as a sphere
    circfill((int)(x - r * 0.35f), (int)(y - r * 0.35f), (int)(r * 0.22f) + 1, CLR_LIGHT_PEACH);

    // spikes around the rim
    if (me && PART(P_SPIKE)) {
        for (int k = 0; k < 8; k++) {
            float a = k * 45.0f + (S->flagphase * 0.2f);
            int x0 = (int)(x + cos_deg(a) * r),        y0 = (int)(y + sin_deg(a) * r);
            int x1 = (int)(x + cos_deg(a) * (r + 4)),  y1 = (int)(y + sin_deg(a) * (r + 4));
            line(x0, y0, x1, y1, CLR_LIGHT_GREY);
        }
    }
    // a little mouth showing diet, on the leading edge
    if (me) {
        int mc = S->diet == D_HERB ? CLR_LIME_GREEN : S->diet == D_CARN ? CLR_RED : CLR_YELLOW;
        int mx = (int)(x + cos_deg(heading) * r * 0.8f), my = (int)(y + sin_deg(heading) * r * 0.8f);
        circfill(mx, my, 2, mc);
    }
}

// the whole pond in world space, ending with the camera still shifted — both the
// title backdrop and live play draw through this.
static void draw_pond(void) {
    cls(CLR_DARKER_BLUE);
    follow((int)S->px, (int)S->py, WORLDW, WORLDH);

    for (int gx = 0; gx <= WORLDW; gx += 40) line(gx, 0, gx, WORLDH, CLR_DARK_BLUE);
    for (int gy = 0; gy <= WORLDH; gy += 40) line(0, gy, WORLDW, gy, CLR_DARK_BLUE);
    for (int i = 0; i < NMOTE; i++)
        if (S->mote[i].life > 0) pset((int)S->mote[i].x, (int)S->mote[i].y, S->mote[i].col);
    rect(0, 0, WORLDW, WORLDH, CLR_TRUE_BLUE);

    if (S->mode == M_SWIM)            // faint terror ring (where the flock reacts to you)
        circ((int)S->px, (int)S->py, (int)(FLEE_R + S->r), CLR_DARK_BLUE);

    for (int i = 0; i < NFOOD; i++) {
        if (!S->food[i].alive) continue;
        Food *f = &S->food[i];
        if (f->kind == F_PLANT) {
            circfill((int)f->x, (int)f->y, 2, CLR_LIME_GREEN);
            pset((int)(f->x + cos_deg(f->wob) * 2), (int)(f->y - 2), CLR_GREEN);
        } else {
            circfill((int)f->x, (int)f->y, 2, CLR_DARK_RED);
            pset((int)f->x, (int)f->y, CLR_RED);
        }
    }

    for (int i = 0; i < N; i++) if (S->herd[i].alive) draw_microbe(&S->herd[i]);

    if (S->mating) {
        int pr = 6 + (int)(2 + 2 * sin_deg(S->matephase));
        circ((int)S->matex, (int)S->matey, pr + 4, CLR_PINK);
        circfill((int)S->matex, (int)S->matey, (int)S->r, CLR_BLUE_GREEN);
        circ((int)S->matex, (int)S->matey, (int)S->r, CLR_PINK);
        pset((int)S->matex, (int)S->matey, CLR_INDIGO);
    }

    float heading = (fsqrt(S->vx * S->vx + S->vy * S->vy) > 0.2f)
                    ? angle_to(0, 0, (int)(S->vx * 10), (int)(S->vy * 10)) : 0;
    int body = S->flash > 0 ? CLR_WHITE : (S->ifr > 0 && (frame() / 2) % 2 ? CLR_PINK : CLR_BLUE_GREEN);
    draw_cell(S->px, S->py, S->r, body, CLR_TRUE_BLUE, CLR_INDIGO, heading, true);
}

void draw(void) {
    camera(0, 0); pal_reset(); fillp_reset();

    if (S->mode == M_START) {
        draw_pond();                  // living soup churns behind the menu
        camera(0, 0);
        rectfill(0, 16, SCREEN_W, 24, CLR_BLACK);            // title strip
        print_centered("OERSOEP", SCREEN_W / 2, 20, CLR_LIME_GREEN);
        print_centered("the primordial soup", SCREEN_W / 2, 32, CLR_TRUE_BLUE);
        print_centered("choose your mouth", SCREEN_W / 2, 70, CLR_WHITE);
        rectfill(40, 88, 100, 40, CLR_DARK_GREEN);  rect(40, 88, 100, 40, CLR_LIME_GREEN);
        print_centered("Z  HERBIVORE", 90, 96, CLR_WHITE);
        print_centered("eats plants", 90, 110, CLR_LIME_GREEN);
        rectfill(180, 88, 100, 40, CLR_DARK_RED);   rect(180, 88, 100, 40, CLR_RED);
        print_centered("X  CARNIVORE", 230, 96, CLR_WHITE);
        print_centered("eats meat + cells", 230, 110, CLR_RED);
        rectfill(0, 162, SCREEN_W, 12, CLR_BLACK);
        print_centered("swim toward the cursor - eat - evolve", SCREEN_W / 2, 164, CLR_LIGHT_GREY);
        return;
    }

    draw_pond();
    camera(0, 0);

    // ===== HUD =====
    rectfill(0, 0, SCREEN_W, 18, CLR_DARKER_PURPLE);
    bar(6, 3, 90, 5, S->health / S->maxhealth, CLR_RED, CLR_DARK_RED);
    print(str("T%d", S->tier), 100, 2, CLR_WHITE);
    const char *dn = S->diet == D_HERB ? "HERB" : S->diet == D_CARN ? "CARN" : "OMNI";
    print(dn, 122, 2, S->diet == D_HERB ? CLR_LIME_GREEN : S->diet == D_CARN ? CLR_RED : CLR_YELLOW);
    print_right(str("size %d", (int)S->r), SCREEN_W - 6, 2, CLR_LIGHT_GREY);
    bar(6, 10, SCREEN_W - 12, 4, clamp(S->dna / S->dna_cap, 0, 1), CLR_PINK, CLR_DARKER_GREY);

    if (S->mating)
        print_centered("DNA FULL - find your mate", SCREEN_W / 2, 24, CLR_PINK);

    // editor overlay
    if (S->mode == M_EDITOR) {
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK);
        print_centered("EVOLVE", SCREEN_W / 2, 26, CLR_LIME_GREEN);
        print_centered("Left / Right, then Z", SCREEN_W / 2, 40, CLR_LIGHT_GREY);
        int cw = 92, gap = 8, total = S->ncards * cw + (S->ncards - 1) * gap;
        int x0 = (SCREEN_W - total) / 2;
        for (int i = 0; i < S->ncards; i++) {
            int x = x0 + i * (cw + gap), y = 64, h = 74, p = S->card[i];
            rectfill(x, y, cw, h, CLR_DARKER_BLUE);
            rect(x, y, cw, h, i == S->sel ? CLR_YELLOW : CLR_DARK_GREY);
            // a little preview of the cell with this part
            circfill(x + cw / 2, y + 26, 10, CLR_BLUE_GREEN);
            circ(x + cw / 2, y + 26, 10, CLR_TRUE_BLUE);
            if (PART_BIT[p] == P_SPIKE)
                for (int k = 0; k < 8; k++) line(x + cw / 2 + (int)(cos_deg(k * 45) * 10), y + 26 + (int)(sin_deg(k * 45) * 10),
                                                  x + cw / 2 + (int)(cos_deg(k * 45) * 14), y + 26 + (int)(sin_deg(k * 45) * 14), CLR_LIGHT_GREY);
            if (PART_BIT[p] == P_FLAG)   bezier(x + cw / 2 - 10, y + 26, x + cw / 2 - 16, y + 20, x + cw / 2 - 22, y + 30, CLR_LIGHT_GREY);
            if (PART_BIT[p] == P_POISON) circ(x + cw / 2, y + 26, 14, CLR_LIME_GREEN);
            if (PART_BIT[p] == P_JAW)    circfill(x + cw / 2 + 8, y + 26, 3, CLR_YELLOW);
            print_centered(PART_NAME[p], x + cw / 2, y + 48, i == S->sel ? CLR_YELLOW : CLR_WHITE);
            print_centered(PART_DESC[p], x + cw / 2, y + 60, CLR_LIGHT_GREY);
        }
        return;
    }

    // game over / win
    if (S->mode == M_OVER) {
        rectfill(50, 64, 220, 64, CLR_BLACK);
        rect(50, 64, 220, 64, S->won ? CLR_LIME_GREEN : CLR_RED);
        if (S->won) {
            print_centered("YOU CRAWLED ASHORE!", SCREEN_W / 2, 76, CLR_LIME_GREEN);
            print_centered("the cell stage is complete", SCREEN_W / 2, 92, CLR_WHITE);
        } else {
            print_centered("DEVOURED", SCREEN_W / 2, 76, CLR_RED);
            print_centered(str("reached tier %d, size %d", S->tier, (int)S->r), SCREEN_W / 2, 92, CLR_WHITE);
        }
        print_centered("Z to replay", SCREEN_W / 2, 112, CLR_LIGHT_GREY);
    }
}
