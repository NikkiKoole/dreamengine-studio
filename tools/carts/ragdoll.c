#include "studio.h"

// RAGDOLL — verlet physics stick figures that struggle to stay upright.
// Mouse: drag to grab+throw characters or balls. Right-click: drop new ball. R: reset.

#define MAX_CH   50
#define N_PTS    12
#define N_SK     12
#define MAX_BL   50
#define GY       (SCREEN_H - 22)
#define ITERS     8
#define STAND_MAX  90   // frames to recover from ragdoll to standing
#define HOP_UP     4.5f // upward velocity per hop (pixels/frame)
#define HOP_FWD    1.8f // forward velocity per hop

// cute one-shot instrument slots (5..15 are user-definable) — patched in init()
#define SND_BLIP   5   // round sine pluck (lowpassed) — grabs, body bumps
#define SND_POP    6   // bubbly thin-square — ball spawn / let-go
#define SND_BELL   7   // shimmery sine + tiny vibrato — sparkle-in, throw "whee"
#define SND_THUMP  8   // soft low sine + boing wobble (lowpass) — floor bounce + landings
#define SND_TICK   9   // band-passed noise tap — ball whacks a body
#define SND_DRAG  10   // soft pad re-triggered while dragging — lowpass opens as you lift

typedef struct { float x, y, px, py; } Pt;
typedef struct { int a, b; float r;   } Sk;
typedef struct { Pt p[N_PTS]; int col; } Rag;
typedef struct { float x, y, px, py, r; } Bl;

// point roles: 0=head 1=neck 2=chest 3=hip
//              4=elbow_l 5=elbow_r 6=hand_l 7=hand_r
//              8=knee_l  9=knee_r  10=foot_l 11=foot_r
static const Sk SKEL[N_SK] = {
    {0,1,4.0f},{1,2,4.0f},{2,3,8.0f},   // spine: exact
    {2,4,7.8f},{2,5,7.8f},               // chest-elbow: sqrt(36+25)=7.81
    {4,6,6.3f},{5,7,6.3f},               // elbow-hand:  sqrt(4+36)=6.32
    {3,8,8.5f},{3,9,8.5f},               // hip-knee:    sqrt(9+64)=8.54 (was 9 → pushed knee into floor)
    {8,10,8.0f},{9,11,8.0f},             // knee-foot: exact
    {1,3,12.0f}                          // neck-hip diagonal: exact
};

static Rag   rags[MAX_CH];
static Bl    bls[MAX_BL];
static int   n_chars, nbl;
static int   gtype, gci, gpi, gbi;
static float pmx, pmy;
static int   rtimer[MAX_CH];
static float wdest[MAX_CH];
static bool  whop[MAX_CH];
static int   cool_bonk, cool_whack, cool_bump;  // sfx cooldowns (frames) — stop collisions machine-gunning

static float vl(float x, float y) { return fsqrt(x*x + y*y); }

static void sprag(int i, float cx, float cy) {
    Rag *r = &rags[i];
    float y = cy;   // foot baseline — pass GY to stand on the ground, or my to drop from the sky
#define PT(n,ox,oy) r->p[n]=(Pt){cx+(ox),y-(oy),cx+(ox),y-(oy)}
    PT(10,-3, 0); PT(11, 3, 0);   // feet
    PT( 8,-3, 8); PT( 9, 3, 8);   // knees
    PT( 3, 0,16);                  // hip
    PT( 2, 0,24);                  // chest
    PT( 4,-6,19); PT( 5, 6,19);   // elbows
    PT( 6,-8,13); PT( 7, 8,13);   // hands
    PT( 1, 0,28); PT( 0, 0,32);   // neck, head
#undef PT
    static const int COLS[] = {
        CLR_RED, CLR_BLUE, CLR_YELLOW, CLR_GREEN, CLR_ORANGE,
        CLR_PINK, CLR_LIGHT_PEACH, CLR_LIME_GREEN, CLR_DARK_PEACH, CLR_INDIGO
    };
    r->col = COLS[i % 10];
}

static void spbl(float x, float y, float vx, float vy, float r) {
    if (nbl >= MAX_BL) return;
    Bl *b = &bls[nbl++];
    b->x = x; b->y = y; b->r = r;
    b->px = x - vx; b->py = y - vy;  // encode initial velocity (px/frame)
}

static void add_char(float cx, float cy) {
    if (n_chars >= MAX_CH) return;
    int i = n_chars;
    sprag(i, cx, cy);
    rtimer[i] = 0;
    wdest[i]  = cx + (rnd(2) ? 80.0f : -80.0f);
    wdest[i]  = clamp(wdest[i], 20, SCREEN_W - 20);
    whop[i]   = true;
    n_chars++;
}

static void reset_scene(void) {
    n_chars = 0; nbl = 0; gtype = 0;
    add_char(60, GY); add_char(160, GY); add_char(260, GY);
    // stagger timers so they don't all hop at once
    for (int i = 0; i < n_chars; i++)
        rtimer[i] = STAND_MAX - i * 25;
    spbl(75,  GY-60,  2.2f, -3.5f, 12.0f);
    spbl(250, GY-80, -1.8f, -2.5f, 10.0f);
}

// Hop tick — when fully standing, launch a hop toward the destination
static void hop_tick(int ci) {
    // whop resets to true whenever not at full standing (airborne / recovering)
    if (rtimer[ci] < STAND_MAX) { whop[ci] = true; return; }
    if (!whop[ci]) return;  // already hopped this cycle, wait for next landing

    float hip_x = rags[ci].p[3].x;
    float ddx   = wdest[ci] - hip_x;
    if (ddx > -15.0f && ddx < 15.0f)  // arrived — pick far side
        wdest[ci] = (hip_x < SCREEN_W * 0.5f)
            ? SCREEN_W * 0.55f + (float)rnd(SCREEN_W / 4)
            : SCREEN_W * 0.10f + (float)rnd(SCREEN_W / 4);

    float wdir = (wdest[ci] > hip_x) ? 1.0f : -1.0f;
    // apply impulse to every point — upward + forward
    // in Verlet: velocity = pos - prev_pos, so py += HOP_UP gives upward kick
    for (int pi = 0; pi < N_PTS; pi++) {
        rags[ci].p[pi].py += HOP_UP;
        rags[ci].p[pi].px -= wdir * HOP_FWD;
    }
    whop[ci]    = false;
    rtimer[ci]  = 0;    // immediately switch to ragdoll — prevents foot-pin from erasing the hop impulse
}

void init(void) {
    // patch the cute SFX instruments — ADSR + filter + a touch of LFO (see 20/21/22 tutorial carts)
    instrument(SND_BLIP, INSTR_SINE, 1, 80, 0, 90);        // fast attack, quick decay to silence = round pluck
    instrument_filter(SND_BLIP, FILTER_LOW, 1600, 3);      // shave the highs so it's soft, not clicky

    instrument(SND_POP, INSTR_SQUARE, 1, 60, 0, 70);       // bubbly little pop
    instrument_duty(SND_POP, 0.30f);                       // thin pulse = nasal/"boopy"
    instrument_filter(SND_POP, FILTER_LOW, 1300, 4);

    instrument(SND_BELL, INSTR_SINE, 2, 150, 0, 320);      // longer release = a ringing bell
    instrument_lfo(SND_BELL, 0, LFO_PITCH, 7.0f, 0.25f);   // subtle vibrato shimmer

    instrument(SND_THUMP, INSTR_SINE, 1, 110, 0, 130);     // soft low body
    instrument_filter(SND_THUMP, FILTER_LOW, 600, 5);      // deep + round
    instrument_lfo(SND_THUMP, 0, LFO_PITCH, 11.0f, 0.8f);  // fast pitch wobble = "boing"

    instrument(SND_TICK, INSTR_NOISE, 0, 35, 0, 45);       // very short noise burst
    instrument_filter(SND_TICK, FILTER_BAND, 1400, 7);     // bandpass = a "tok", not white hiss

    instrument(SND_DRAG, INSTR_TRI, 40, 0, 7, 350);        // soft sustaining pad (full sustain, long tail)
    instrument_filter(SND_DRAG, FILTER_LOW, 600, 7);       // its cutoff is swept live from the drag (see update)
    instrument_lfo(SND_DRAG, 0, LFO_PITCH, 5.0f, 0.2f);    // faint vibrato so the hum feels alive

    reset_scene();
}

static void steppt(Pt *p) {
    float vx = (p->x - p->px) * 0.99f;
    float vy = (p->y - p->py) * 0.99f;
    p->px = p->x; p->py = p->y;
    p->x += vx;
    p->y += vy + 0.38f;  // gravity: pixels/frame²
}

// Angular spring: keeps bone a→b near target direction (tx,ty).
// cross = sin(angle error). Only applied when within 90° of target (dot > 0) —
// past 90° the cross-product direction inverts and would push the wrong way.
static void angsp(Pt *a, Pt *b, float tx, float ty, float str) {
    float dx = b->x - a->x, dy = b->y - a->y;
    float d = vl(dx, dy);
    if (d < 0.001f) return;
    float cx = dx/d, cy = dy/d;
    if (cx*tx + cy*ty <= 0.0f) return;  // >90° error: skip, let position springs handle
    float cross = cx*ty - cy*tx;
    b->x += -cy * cross * str;  b->y +=  cx * cross * str;
    a->x -= -cy * cross * str;  a->y -=  cx * cross * str;
}

static void solvesk(Pt *pts, const Sk *sk) {
    Pt *a = &pts[sk->a], *b = &pts[sk->b];
    float dx = b->x - a->x, dy = b->y - a->y;
    float d = vl(dx, dy);
    if (d < 0.001f) return;
    float f = (d - sk->r) / (d * 2.0f);
    a->x += dx*f; a->y += dy*f;
    b->x -= dx*f; b->y -= dy*f;
}

static void clpt(Pt *p) {
    if (p->y > GY)         { p->y = GY;         p->py = p->y + (p->py - p->y) * 0.35f; }
    if (p->x < 1)          { p->x = 1;          p->px = p->x + (p->px - p->x) * 0.5f;  }
    if (p->x > SCREEN_W-1) { p->x = SCREEN_W-1; p->px = p->x + (p->px - p->x) * 0.5f;  }
}

// returns downward impact speed when the ball hits the floor (0 otherwise) — for the bonk sfx
static float clbl(Bl *b) {
    float floorhit = 0;
    if (b->y+b->r > GY)        { float v=b->y-b->py; if (v>0) floorhit=v; b->y = GY-b->r; b->py=b->y+v*0.45f; }
    if (b->x-b->r < 0)         { b->x = b->r;            float v=b->x-b->px; b->px=b->x+v*0.55f; }
    if (b->x+b->r > SCREEN_W)  { b->x = SCREEN_W-b->r;   float v=b->x-b->px; b->px=b->x+v*0.55f; }
    if (b->y-b->r < 0)         { b->y = b->r;            float v=b->y-b->py; b->py=b->y+v*0.4f;  }
    return floorhit;
}

// returns the strongest ball↔point closing speed this frame (0 = resting contact) — for the whack sfx
static float blvrag(Bl *b, Rag *r) {
    float impact = 0;
    for (int i = 0; i < N_PTS; i++) {
        Pt *p = &r->p[i];
        float dx = p->x - b->x, dy = p->y - b->y;
        float d = vl(dx, dy), md = b->r + 2.5f;
        if (d < md && d > 0.001f) {
            float rel = vl((p->x-p->px)-(b->x-b->px), (p->y-p->py)-(b->y-b->py));
            if (rel > impact) impact = rel;   // relative speed BEFORE we move them
            float f = (md - d) / d;
            p->x += dx*f*0.6f; p->y += dy*f*0.6f;
            b->x -= dx*f*0.4f; b->y -= dy*f*0.4f;
        }
    }
    return impact;
}

// Point-to-point collision. Broad-phase: skip pairs where hips are > 40px apart.
// Returns the strongest point↔point closing speed (0 = just leaning) — for the bump sfx.
static float ragvrag(int a, int b) {
    float hdx = rags[b].p[3].x - rags[a].p[3].x;
    float hdy = rags[b].p[3].y - rags[a].p[3].y;
    if (hdx*hdx + hdy*hdy > 40.0f*40.0f) return 0;
    float impact = 0;
    for (int i = 0; i < N_PTS; i++)
        for (int j = 0; j < N_PTS; j++) {
            Pt *pa = &rags[a].p[i], *pb = &rags[b].p[j];
            float dx = pb->x - pa->x;
            float dy = pb->y - pa->y;
            float d = vl(dx, dy);
            if (d > 3.5f || d < 0.001f) continue;
            float rel = vl((pa->x-pa->px)-(pb->x-pb->px), (pa->y-pa->py)-(pb->y-pb->py));
            if (rel > impact) impact = rel;
            float f = (3.5f - d) / (d * 2.0f);
            pa->x -= dx*f;    pa->y -= dy*f*0.4f;
            pb->x += dx*f;    pb->y += dy*f*0.4f;
            pa->px -= dx*f*0.5f;   // velocity impulse = bounce
            pb->px += dx*f*0.5f;
        }
    return impact;
}

static void blvbl(Bl *a, Bl *b) {
    float dx = b->x-a->x, dy = b->y-a->y;
    float d = vl(dx, dy), md = a->r + b->r;
    if (d < md && d > 0.001f) {
        float f = (md - d) / (d * 2.0f);
        a->x -= dx*f; a->y -= dy*f;
        b->x += dx*f; b->y += dy*f;
    }
}

static void draw_rag(Rag *r) {
    Pt *p = r->p;
    int c = r->col;
    // oriented torso as a 4px-wide quad
    float tdx = p[3].x-p[2].x, tdy = p[3].y-p[2].y, td = vl(tdx, tdy);
    if (td > 0.1f) {
        float nx = -tdy/td * 2.0f, ny = tdx/td * 2.0f;
        quadfill((int)(p[2].x-nx),(int)(p[2].y-ny), (int)(p[2].x+nx),(int)(p[2].y+ny),
                 (int)(p[3].x+nx),(int)(p[3].y+ny), (int)(p[3].x-nx),(int)(p[3].y-ny), c);
    }
    circfill((int)p[0].x, (int)p[0].y, 3, c);
    line((int)p[0].x,(int)p[0].y, (int)p[1].x,(int)p[1].y, c);   // neck
    line((int)p[2].x,(int)p[2].y, (int)p[4].x,(int)p[4].y, c);   // upper arm L
    line((int)p[4].x,(int)p[4].y, (int)p[6].x,(int)p[6].y, c);   // lower arm L
    line((int)p[2].x,(int)p[2].y, (int)p[5].x,(int)p[5].y, c);   // upper arm R
    line((int)p[5].x,(int)p[5].y, (int)p[7].x,(int)p[7].y, c);   // lower arm R
    line((int)p[3].x,(int)p[3].y, (int)p[8].x,(int)p[8].y,   c); // upper leg L
    line((int)p[8].x,(int)p[8].y, (int)p[10].x,(int)p[10].y, c); // lower leg L
    line((int)p[3].x,(int)p[3].y, (int)p[9].x,(int)p[9].y,   c); // upper leg R
    line((int)p[9].x,(int)p[9].y, (int)p[11].x,(int)p[11].y, c); // lower leg R
}

void update(void) {
    float mx = (float)mouse_x(), my = (float)mouse_y();
    if (cool_bonk  > 0) cool_bonk--;
    if (cool_whack > 0) cool_whack--;
    if (cool_bump  > 0) cool_bump--;

    // start a grab
    if (mouse_pressed(MOUSE_LEFT)) {
        float best = 16.0f;
        gtype = 0;
        for (int i = 0; i < nbl; i++) {
            float d = vl(mx-bls[i].x, my-bls[i].y);
            if (d < bls[i].r + 5 && d < best) { best = d; gtype = 2; gbi = i; }
        }
        for (int ci = 0; ci < n_chars; ci++)
            for (int pi = 0; pi < N_PTS; pi++) {
                float d = vl(mx-rags[ci].p[pi].x, my-rags[ci].p[pi].y);
                if (d < best) { best = d; gtype = 1; gci = ci; gpi = pi; }
            }
        if      (gtype == 2) note(degree(SCALE_PENTA, 3, 1), SND_BLIP, 4);   // grabbed a ball — low pluck
        else if (gtype == 1) note(degree(SCALE_PENTA, 4, 2), SND_BLIP, 4);   // grabbed a character — higher pluck
    }
    if (mouse_released(MOUSE_LEFT)) {
        if (gtype == 1) {   // let go of a character — a rising "whee" that climbs higher the harder it's flung
            Pt *g = &rags[gci].p[gpi];
            float spd = vl(g->x - g->px, g->y - g->py);
            int top = 2 + (int)clamp(spd * 0.5f, 0, 6);
            note(degree(SCALE_PENTA, 4, 0), SND_BELL, 4);
            schedule(45, degree(SCALE_PENTA, 4, top), SND_BELL, 4);
        } else if (gtype == 2) {
            note(degree(SCALE_PENTA, 3, 0), SND_POP, 3);
        }
        gtype = 0;
    }

    if (mouse_pressed(MOUSE_RIGHT) || keyp('Z')) { spbl(mx, my, 0, 0, 7.0f + rnd(8)); note(degree(SCALE_PENTA, 3, 2), SND_POP, 5); }  // ball at mouse — pop
    if (keyp('X')) {   // drop a character from the cursor — falls/lands/stands; a 3-note sparkle as it appears
        add_char(mx, my);
        note(degree(SCALE_PENTA, 4, 0), SND_BELL, 4);
        schedule(60,  degree(SCALE_PENTA, 4, 2), SND_BELL, 4);
        schedule(120, degree(SCALE_PENTA, 5, 0), SND_BELL, 4);
    }
    if (keyp('R')) reset_scene();

    // drag = dangle from the grabbed point. The point itself is pinned to the cursor
    // in the solve loop below; the rest of the body hangs off it via the constraints.
    // Freeze the grabbed point's previous position each frame so that on release its
    // verlet velocity is exactly this frame's mouse delta → a natural throw.
    if (gtype == 1 && mouse_down(MOUSE_LEFT)) {
        rags[gci].p[gpi].px = rags[gci].p[gpi].x;  // last frame this was pinned to the old mouse pos
        rags[gci].p[gpi].py = rags[gci].p[gpi].y;
        // a soft pad hums while you hold a character. The higher you lift, the more the
        // lowpass opens (brighter) and the pitch climbs; a slow sine drifts the cutoff on
        // top, so it keeps "breathing" even if you hold still. Re-triggered every 9 frames —
        // each new voice snapshots the freshly-set cutoff, which is how the sweep stays live.
        float lift = clamp((GY - my) / (float)GY, 0.0f, 1.0f);
        if (frame() % 9 == 0) {
            float cutoff = 300.0f + lift * 2400.0f + sin_deg(now() * 40.0f) * 400.0f;
            instrument_filter(SND_DRAG, FILTER_LOW, (int)clamp(cutoff, 200, 3400), 7);
            note(degree(SCALE_PENTA, 4, (int)(lift * 6.0f)), SND_DRAG, 3);
        }
    }
    if (gtype == 2 && mouse_down(MOUSE_LEFT)) {
        Bl *b = &bls[gbi];
        b->px = pmx; b->py = pmy; b->x = mx; b->y = my;
    }

    // advance standing timers — only recover when feet are on the ground
    for (int ci = 0; ci < n_chars; ci++) {
        if (gtype == 1 && gci == ci && mouse_down(MOUSE_LEFT)) {
            rtimer[ci] = 0;  // grabbed = instant full ragdoll: feet unpinned, standing springs off
        } else {
            float avg_foot_y = (rags[ci].p[10].y + rags[ci].p[11].y) * 0.5f;
            if (avg_foot_y >= GY - 3.0f) {
                if (rtimer[ci] < STAND_MAX) rtimer[ci]++;
            } else {
                rtimer[ci] = 0;  // airborne — keep ragdoll
            }
        }
    }

    // landing thud — only on hard touchdowns (sky-drops / big throws), not gentle hops
    for (int ci = 0; ci < n_chars; ci++) {
        float avg_foot_y = (rags[ci].p[10].y + rags[ci].p[11].y) * 0.5f;
        float fv = (rags[ci].p[10].y - rags[ci].p[10].py);   // foot downward speed
        if (rtimer[ci] == 1 && avg_foot_y >= GY - 3.0f && fv > 4.0f && cool_bonk == 0) {
            hit(degree(SCALE_PENTA, 2, 0), SND_THUMP, 3, 130); cool_bonk = 3;   // soft landing boing
        }
    }

    // character vs character bouncing
    for (int a = 0; a < n_chars; a++)
        for (int b = a + 1; b < n_chars; b++)
            if (ragvrag(a, b) > 1.2f && cool_bump == 0) { note(degree(SCALE_PENTA, 4, 1), SND_BLIP, 2); cool_bump = 5; }  // soft bump

    // simulate characters
    for (int ci = 0; ci < n_chars; ci++) {
        Rag *r = &rags[ci];
        float sk = (float)rtimer[ci] / STAND_MAX;

        for (int pi = 0; pi < N_PTS; pi++) {
            if (gtype == 1 && gci == ci && gpi == pi) continue;
            steppt(&r->p[pi]);
        }

        hop_tick(ci);  // may set rtimer=0 to release foot-pin on the hop frame
        sk = (float)rtimer[ci] / STAND_MAX;  // re-read in case hop_tick changed rtimer

        // standing mode: pin feet hard before the solve loop (same trick as rope.c's anchor pin)
        if (sk > 0.0f) {
            r->p[10].y = GY; r->p[10].py = GY;   // zero vertical velocity, locked to floor
            r->p[11].y = GY; r->p[11].py = GY;
            // damp horizontal sliding (feet grip the ground)
            r->p[10].px = r->p[10].x - (r->p[10].x - r->p[10].px) * (1.0f - sk * 0.55f);
            r->p[11].px = r->p[11].x - (r->p[11].x - r->p[11].px) * (1.0f - sk * 0.55f);
        }

        // constraint solve — re-pin feet + balance springs every iteration
        for (int it = 0; it < ITERS; it++) {
            for (int si = 0; si < N_SK; si++) solvesk(r->p, &SKEL[si]);
            if (sk > 0.0f) {
                r->p[10].y = GY;   // re-pin feet y after each stick iteration
                r->p[11].y = GY;
                // position springs: pull each joint toward its standing position.
                // these work from ANY orientation (flat on floor → fully upright).
                float fl_y = r->p[10].y, fr_y = r->p[11].y;
                float fcx   = (r->p[10].x + r->p[11].x) * 0.5f;
                // knees above their respective pinned feet
                r->p[8].x += (r->p[10].x      - r->p[8].x) * sk * 0.22f;
                r->p[8].y += ((fl_y - 8.f)    - r->p[8].y) * sk * 0.22f;
                r->p[9].x += (r->p[11].x      - r->p[9].x) * sk * 0.22f;
                r->p[9].y += ((fr_y - 8.f)    - r->p[9].y) * sk * 0.22f;
                // hip above feet center
                r->p[3].x += (fcx - r->p[3].x) * sk * 0.18f;
                r->p[3].y += ((GY - 16.f)     - r->p[3].y) * sk * 0.20f;
                // chest above hip
                r->p[2].x += (r->p[3].x       - r->p[2].x) * sk * 0.15f;
                r->p[2].y += ((r->p[3].y-8.f) - r->p[2].y) * sk * 0.15f;
                // neck above chest, head above neck
                r->p[1].x += (r->p[2].x       - r->p[1].x) * sk * 0.15f;
                r->p[1].y += (r->p[2].y - 4.f - r->p[1].y) * sk * 0.15f;
                r->p[0].x += (r->p[1].x       - r->p[0].x) * sk * 0.20f;
                r->p[0].y += (r->p[1].y - 4.f - r->p[0].y) * sk * 0.20f;
                // arms hang at their natural side angles (chest→elbow→hand)
                // target directions from standing pose: (-0.768,0.640) / (-0.316,0.949) and mirrored
                angsp(&r->p[2], &r->p[4], -0.768f, 0.640f, sk * 0.22f);
                angsp(&r->p[2], &r->p[5],  0.768f, 0.640f, sk * 0.22f);
                angsp(&r->p[4], &r->p[6], -0.316f, 0.949f, sk * 0.16f);
                angsp(&r->p[5], &r->p[7],  0.316f, 0.949f, sk * 0.16f);
                // angular springs for fine alignment (only within 90° — skip when flat)
                angsp(&r->p[2], &r->p[1],  0.000f,-1.0f,   sk * 0.30f); // chest→neck up
                angsp(&r->p[1], &r->p[0],  0.000f,-1.0f,   sk * 0.35f); // neck→head up
                angsp(&r->p[2], &r->p[3],  0.000f, 1.0f,   sk * 0.40f);
                angsp(&r->p[3], &r->p[8], -0.351f, 0.936f, sk * 0.30f);
                angsp(&r->p[3], &r->p[9],  0.351f, 0.936f, sk * 0.30f);
                angsp(&r->p[8], &r->p[10], 0.000f, 1.0f,   sk * 0.25f);
                angsp(&r->p[9], &r->p[11], 0.000f, 1.0f,   sk * 0.25f);
            }
            if (gtype == 1 && gci == ci) { r->p[gpi].x = mx; r->p[gpi].y = my; }
            for (int pi = 0; pi < N_PTS; pi++) clpt(&r->p[pi]);
        }

#ifdef DE_TRACE
        if (ci == 0) {
            float fcx0 = (r->p[10].x + r->p[11].x) * 0.5f;
            watch("c0_rtimer", "%d", rtimer[0]);
            watch("c0_sk",     "%.2f", (float)rtimer[0]/STAND_MAX);
            watch("c0_lean",   "%.2f", r->p[3].x - fcx0);
            watch("c0_hip_y",  "%.1f", r->p[3].y);
            watch("c0_knee_y", "%.1f", r->p[8].y);
            watch("c0_foot_y", "%.1f", r->p[10].y);
        }
#endif
    }

    // simulate balls — collisions run even for the grabbed ball
    for (int i = 0; i < nbl; i++) {
        Bl *b = &bls[i];
        bool grabbed_ball = (gtype == 2 && gbi == i);
        if (!grabbed_ball) {
            float vx = (b->x - b->px) * 0.997f, vy = (b->y - b->py) * 0.997f;
            b->px = b->x; b->py = b->y;
            b->x += vx; b->y += vy + 0.38f;
        }
        for (int ci = 0; ci < n_chars; ci++)
            if (blvrag(b, &rags[ci]) > 1.8f && cool_whack == 0) { hit(60, SND_TICK, 3, 45); cool_whack = 4; }  // thwap
        for (int j = i + 1; j < nbl; j++) blvbl(b, &bls[j]);
        float fh = clbl(b);
        // only boing on a real impact — a settling/jostled ball micro-bounces well under this
        if (fh > 4.0f && cool_bonk == 0) { hit(degree(SCALE_PENTA, 2, 2), SND_THUMP, (int)clamp(fh*0.5f, 2, 5), 130); cool_bonk = 6; }  // boing
        if (grabbed_ball) { b->x = mx; b->y = my; }  // mouse wins after collision response
    }

    pmx = mx; pmy = my;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    rectfill(0, GY+1, SCREEN_W, SCREEN_H-GY, CLR_DARK_GREEN);
    line(0, GY, SCREEN_W-1, GY, CLR_GREEN);

    for (int i = 0; i < n_chars; i++) draw_rag(&rags[i]);

    for (int i = 0; i < nbl; i++) {
        circfill((int)bls[i].x, (int)bls[i].y, (int)bls[i].r, CLR_ORANGE);
        circ((int)bls[i].x, (int)bls[i].y, (int)bls[i].r, CLR_YELLOW);
    }

    print(str("drag  Z:ball  X:char %d/%d  R:reset", n_chars, MAX_CH), 4, 4, CLR_DARK_GREY);
}
