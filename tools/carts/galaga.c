/* de:meta
{
  "title": "galaga",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "title-play-gameover-loop",
    "finite-state-ai"
  ],
  "lineage": "Port of Namco's 1981 Galaga; per-enemy ENTER→FORM→DIVE→CAPTURE FSM drives the formation fly-in and tractor-beam mechanic.",
  "genre": "shooter",
  "homage": "Galaga (1981)",
  "description": "The formation shooter. Bees, butterflies and bosses fly in and lock into a swaying grid, then peel off to dive and shoot at you. The green Boss can drop a tractor beam — if it catches your ship you lose it, but shoot that boss while it tows your ship and you get it back as a DUAL FIGHTER with double fire. Every 4th wave from stage 3 is a CHALLENGING STAGE: enemies fly through in patterns and can not hurt you — hit as many as you can for a bonus (PERFECT = all 40). Extra ship at score milestones. A/D or arrows move, Z fire."
}
de:meta */
#include "studio.h"

// GALAGA — the formation shooter. Enemies fly in and lock into a swaying grid,
// then peel off to dive at you. The green BOSS can drop a tractor beam: if it
// catches your ship you lose it — but shoot that boss while it tows your ship
// and you get it back as a DUAL FIGHTER with double fire.
//
// A/D or Left/Right move    Z fire

#define HUD   12
#define COLS  8
#define ROWS  5
#define SPX   30
#define SPY   17
#define TOPY  30
#define NE    (COLS * ROWS)
#define NPB   8
#define NEB   40

enum { NONE, BEE, BUTTERFLY, BOSS };
enum { ENTER, FORM, DIVE, CAPTURE };

typedef struct {
    int type, hp, alive, state, col, row, towing;
    float x, y, sx, sy, et, vx, vy, fireT, beamT;
} En;
typedef struct { float x, y, vy; int alive; } PB;
typedef struct { float x, y, vx, vy; int alive; } EB;

static En  en[NE];
static PB  pb[NPB];
static EB  eb[NEB];

static float plx;                 // player x
static int   palive, dual, lives, score, stage, state;
static float invuln, respawnT, diveT;
// challenging stage + extras
static int   challenge, chHits, chSpawned, chTotal, nextExtend, rHits, rBonus;
static float chSpawnT, banner, resultT, extraT;

static float sway(void){ return sin_deg(now() * 42) * 12; }
static float slotx(int col){ return SCREEN_W/2 + sway() + (col - (COLS-1)/2.0f) * SPX; }
static float sloty(int row){ return TOPY + row * SPY; }

static int etype(int row, int col){
    if (row == 0) return (col >= 2 && col <= 5) ? BOSS : NONE;
    if (row <= 2) return BUTTERFLY;
    return BEE;
}

static void setup(int instant){
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++){
            En *e = &en[r * COLS + c];
            int ty = etype(r, c);
            e->type = ty; e->col = c; e->row = r; e->towing = 0; e->fireT = rnd_float_between(1, 4);
            e->alive = ty != NONE;
            e->hp = ty == BOSS ? 2 : 1;
            if (!e->alive) continue;
            if (instant){ e->state = FORM; e->x = slotx(c); e->y = sloty(r); }
            else { e->state = ENTER; e->et = -(r * COLS + c) * 0.05f;
                   e->sx = (c & 1) ? -20 : SCREEN_W + 20; e->sy = 10 + r * 8; e->x = e->sx; e->y = e->sy; }
        }
}

// Galaga's challenging stages: 3, 7, 11, ... (every 4th from stage 3)
static int is_challenge(int s){ return s >= 3 && (s - 3) % 4 == 0; }

static void start_stage(void){
    challenge = is_challenge(stage);
    banner = 2.0f; diveT = 1.6f;
    for (int i = 0; i < NE; i++)  en[i].alive = 0;
    for (int i = 0; i < NEB; i++) eb[i].alive = 0;
    if (challenge){ chHits = 0; chSpawned = 0; chTotal = 40; chSpawnT = 0.6f; }
    else setup(stage == 1 ? 1 : 0);
}

// a flier for the challenging stage — crosses the screen, never attacks
static void spawn_flier(void){
    for (int i = 0; i < NE; i++) if (!en[i].alive){
        En *e = &en[i]; e->type = rnd(3) + 1; e->hp = 1; e->alive = 1; e->state = DIVE; e->towing = 0;
        e->x = e->sx = rnd_between(30, SCREEN_W - 30); e->y = -12;
        e->vy = 50; e->vx = rnd_float_between(-40, 40); e->fireT = 999; e->col = 0; e->row = 0;
        return;
    }
}

void init(void){
    plx = SCREEN_W / 2; palive = 1; dual = 0; lives = 3; score = 0; stage = 1; state = 0;
    invuln = 1.0f; respawnT = 0;
    nextExtend = 10000; resultT = 0; extraT = 0;
    for (int i = 0; i < NPB; i++) pb[i].alive = 0;
    for (int i = 0; i < NEB; i++) eb[i].alive = 0;
    start_stage();
}

static int alive_count(void){ int n = 0; for (int i = 0; i < NE; i++) if (en[i].alive) n++; return n; }
static void add_eb(float x, float y){ float a = angle_to((int)x,(int)y,(int)plx, SCREEN_H-16);
    for (int i = 0; i < NEB; i++) if (!eb[i].alive){ eb[i] = (EB){ x, y, cos_deg(a)*90, sin_deg(a)*90, 1 }; return; } }

static void player_hit(void){
    if (invuln > 0) return;
    if (dual){ dual = 0; invuln = 1.2f; hit(40,INSTR_NOISE,4,90); return; }
    palive = 0; lives--; hit(36,INSTR_NOISE,5,200);
    if (lives < 0){ lives = 0; state = 2; } else respawnT = 1.2f;
}

void update(void){
    float d = dt();
    if (state != 0){ if (btnp(0,BTN_A)||btnp(1,BTN_A)) init(); return; }

    if (invuln > 0) invuln -= d;
    if (banner > 0) banner -= d;
    if (resultT > 0) resultT -= d;
    if (extraT > 0) extraT -= d;
    if (!palive){ respawnT -= d; if (respawnT <= 0 && lives >= 0){ palive = 1; plx = SCREEN_W/2; invuln = 1.2f; } }

    // extra ship at score milestones
    while (score >= nextExtend){ lives++; extraT = 1.8f; note(84, INSTR_SINE, 4); schedule(120, 88, INSTR_SINE, 4);
                                 nextExtend += nextExtend < 20000 ? 10000 : 20000; }

    // challenging stage: stream fliers in
    if (challenge && chSpawned < chTotal){ chSpawnT -= d; if (chSpawnT <= 0){ chSpawnT = rnd_float_between(0.12f, 0.30f); spawn_flier(); chSpawned++; } }

    // ---- player ----
    if (palive){
        float spd = 130;
        int ix = (btn(0,BTN_RIGHT)||btn(1,BTN_RIGHT)) - (btn(0,BTN_LEFT)||btn(1,BTN_LEFT));
        plx = clamp(plx + ix * spd * d, 10, SCREEN_W - 10);
        int cnt = 0; for (int i = 0; i < NPB; i++) if (pb[i].alive) cnt++;
        if ((btnp(0,BTN_A)||btnp(1,BTN_A)) && cnt < (dual ? 4 : 2)){
            for (int i = 0; i < NPB; i++) if (!pb[i].alive){ pb[i] = (PB){ plx - (dual?9:0), SCREEN_H-18, -300, 1 };
                if (dual){ for (int j = 0; j < NPB; j++) if (!pb[j].alive){ pb[j] = (PB){ plx+9, SCREEN_H-18, -300, 1 }; break; } }
                break; }
            note(76, INSTR_SQUARE, 2);
        }
    }

    // ---- player bullets ----
    for (int i = 0; i < NPB; i++){ if (!pb[i].alive) continue; pb[i].y += pb[i].vy * d; if (pb[i].y < HUD){ pb[i].alive = 0; continue; }
        for (int j = 0; j < NE; j++){ En *e = &en[j]; if (!e->alive) continue;
            if (distance((int)pb[i].x,(int)pb[i].y,(int)e->x,(int)e->y) < 8){
                pb[i].alive = 0; e->hp--;
                if (e->hp <= 0){ e->alive = 0; score += e->type==BOSS?150:e->type==BUTTERFLY?80:50;
                    if (challenge) chHits++;
                    if (e->towing){ dual = 1; if (!palive){ palive = 1; plx = SCREEN_W/2; invuln = 1.0f; } }   // rescue!
                }
                break;
            }
        }
    }

    // ---- enemies ----
    for (int i = 0; i < NE; i++){ En *e = &en[i]; if (!e->alive) continue;
        if (e->state == ENTER){
            e->et += d * 0.7f; float p = e->et < 0 ? 0 : e->et > 1 ? 1 : ease_out(e->et);
            e->x = lerp(e->sx, slotx(e->col), p); e->y = lerp(e->sy, sloty(e->row), p);
            if (e->et >= 1) e->state = FORM;
        } else if (e->state == FORM){
            e->x = slotx(e->col); e->y = sloty(e->row);
        } else if (e->state == DIVE){
            e->vy += 60 * d; e->y += e->vy * d;
            e->x += (e->vx + sin_deg(e->y * 3) * 40) * d;
            if (!challenge){ e->fireT -= d; if (e->fireT <= 0 && e->y < SCREEN_H - 30){ e->fireT = rnd_float_between(0.5f,1.2f); add_eb(e->x, e->y); } }
            if (e->y > SCREEN_H + 16){ if (challenge) e->alive = 0; else { e->state = ENTER; e->et = 0; e->sx = e->x = rnd_between(20,SCREEN_W-20); e->sy = e->y = -16; } }
        } else if (e->state == CAPTURE){
            e->beamT -= d;
            if (e->beamT > 1.4f){ e->y += 70 * d; }                          // descend
            else if (e->beamT > 0){                                            // beaming
                if (palive && invuln <= 0 && plx > e->x - 16 && plx < e->x + 16 && SCREEN_H-16 > e->y){
                    palive = 0; lives--; e->towing = 1; invuln = 0;
                    hit(30, INSTR_NOISE, 5, 260);
                    if (lives < 0){ lives = 0; state = 2; } else respawnT = 1.6f;
                }
            } else { e->state = FORM; }                                        // done, snap home
        }

        // contact (harmless during a challenging stage)
        if (!challenge && e->state != FORM && e->state != ENTER && palive && invuln <= 0 &&
            distance((int)e->x,(int)e->y,(int)plx, SCREEN_H-14) < 9) { player_hit(); }
    }

    // ---- enemy bullets ----
    for (int i = 0; i < NEB; i++){ if (!eb[i].alive) continue; eb[i].x += eb[i].vx*d; eb[i].y += eb[i].vy*d;
        if (eb[i].y > SCREEN_H || eb[i].x < 0 || eb[i].x > SCREEN_W){ eb[i].alive = 0; continue; }
        if (palive && invuln <= 0 && distance((int)eb[i].x,(int)eb[i].y,(int)plx, SCREEN_H-14) < 7){ eb[i].alive = 0; player_hit(); }
    }

    // ---- pick a diver (not during a challenging stage) ----
    if (!challenge){
        diveT -= d;
        if (diveT <= 0){
            diveT = clamp(1.8f - stage * 0.15f, 0.5f, 1.8f);
            int forms[NE], n = 0; for (int i = 0; i < NE; i++) if (en[i].alive && en[i].state == FORM) forms[n++] = i;
            if (n > 0){
                En *e = &en[forms[rnd(n)]];
                if (e->type == BOSS && palive && chance(35)){ e->state = CAPTURE; e->beamT = 2.6f; }
                else { e->state = DIVE; e->vy = 30; e->vx = (plx - e->x) * 0.4f; }
            }
        }
    }

    // ---- advance to the next stage ----
    if (challenge){
        if (chSpawned >= chTotal && alive_count() == 0){
            rHits = chHits; rBonus = (chHits >= chTotal) ? 1000 : chHits * 30;
            score += rBonus; resultT = 2.8f;
            stage++; start_stage();
        }
    } else if (alive_count() == 0){ stage++; start_stage(); }
}

// ---- drawing ----
static void draw_enemy(En *e){
    int x = (int)e->x, y = (int)e->y;
    if (e->type == BEE){ rectfill(x-4,y-3,8,6,CLR_YELLOW); rectfill(x-6,y-1,2,3,CLR_ORANGE); rectfill(x+4,y-1,2,3,CLR_ORANGE);
                         pset(x-2,y-1,CLR_BLACK); pset(x+2,y-1,CLR_BLACK); }
    else if (e->type == BUTTERFLY){ rectfill(x-4,y-3,8,6,CLR_RED); trifill(x-7,y-4,x-7,y+3,x-3,y,CLR_BLUE); trifill(x+7,y-4,x+7,y+3,x+3,y,CLR_BLUE); }
    else { rectfill(x-5,y-4,10,7,CLR_GREEN); rectfill(x-3,y-6,6,3,CLR_GREEN); pset(x-2,y-1,CLR_BLUE); pset(x+2,y-1,CLR_BLUE);
           if (e->towing){ trifill(x,y+7,x-5,y+15,x+5,y+15,CLR_LIGHT_GREY); } }   // captured ship below
}
static void draw_ship(int x){
    trifill(x, SCREEN_H-20, x-7, SCREEN_H-8, x+7, SCREEN_H-8, CLR_LIGHT_GREY);
    rectfill(x-1, SCREEN_H-22, 2, 5, CLR_WHITE);
    rectfill(x-7, SCREEN_H-10, 3, 3, CLR_BLUE); rectfill(x+4, SCREEN_H-10, 3, 3, CLR_BLUE);
}

void draw(void){
    cls(CLR_BLACK);
    // starfield
    for (int i = 0; i < 40; i++){ int sx = (i * 71) % SCREEN_W, sy = ((i * 53) + (int)(now()*30)) % (SCREEN_H-HUD) + HUD;
        pset(sx, sy, (i & 3) ? CLR_DARK_GREY : CLR_WHITE); }

    // capture beams
    for (int i = 0; i < NE; i++){ En *e = &en[i]; if (e->alive && e->state == CAPTURE && e->beamT <= 1.4f && e->beamT > 0){
        for (int yy = (int)e->y+6; yy < SCREEN_H-16; yy += 3){ int w = 4 + (yy - (int)e->y)/6;
            line((int)e->x - w, yy, (int)e->x + w, yy, ((yy>>1)&1)?CLR_BLUE:CLR_TRUE_BLUE); } } }

    for (int i = 0; i < NE; i++) if (en[i].alive) draw_enemy(&en[i]);
    for (int i = 0; i < NPB; i++) if (pb[i].alive) rectfill((int)pb[i].x, (int)pb[i].y-3, 2, 5, CLR_LIGHT_YELLOW);
    for (int i = 0; i < NEB; i++) if (eb[i].alive) circfill((int)eb[i].x, (int)eb[i].y, 2, CLR_PINK);

    if (palive && !(invuln > 0 && ((int)(now()*20)&1))){
        if (dual){ draw_ship((int)plx-9); draw_ship((int)plx+9); } else draw_ship((int)plx);
    }

    // HUD
    rectfill(0, 0, SCREEN_W, HUD, CLR_BLACK);
    print(str("%06d", score), 6, 2, CLR_WHITE);
    print(str("ST %d", stage), SCREEN_W/2-16, 2, CLR_LIGHT_GREY);
    for (int i = 0; i < lives; i++) trifill(SCREEN_W-10-i*12, 2, SCREEN_W-16-i*12, 10, SCREEN_W-4-i*12, 10, CLR_LIGHT_GREY);

    // banners
    if (resultT > 0){
        print_centered("CHALLENGING STAGE", SCREEN_W/2, 52, CLR_GREEN);
        print_centered(str("HITS  %d / %d", rHits, chTotal), SCREEN_W/2, 66, CLR_WHITE);
        print_centered(rHits >= chTotal ? "PERFECT!  +1000" : str("BONUS  %d", rBonus), SCREEN_W/2, 80, CLR_YELLOW);
    } else if (banner > 0 && state == 0){
        print_centered(challenge ? "CHALLENGING STAGE" : str("STAGE %d", stage), SCREEN_W/2, 60, challenge ? CLR_GREEN : CLR_WHITE);
    }
    if (extraT > 0) print_centered("EXTRA SHIP!", SCREEN_W/2, 94, CLR_YELLOW);

    if (state == 2){ rectfill(80,82,160,38,CLR_BLACK); rect(80,82,160,38,CLR_RED);
        print_centered("GAME OVER", SCREEN_W/2, 90, CLR_RED); print_centered(str("score %d  -  Z", score), SCREEN_W/2, 104, CLR_LIGHT_GREY); }
}
