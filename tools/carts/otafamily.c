/* de:meta
{
  "title": "ota family",
  "status": "active",
  "created": "2026-07-02",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling",
    "step-sequencer"
  ],
  "lineage": "The otamatone cart's held-note reed-buzz voice, instantiated FOUR times as a family (the VOICE-size trick done with ribbon range + glide + duty + vibrato per member), crossed with loopstation's gesture-stream looper (EV_ON/EV_CC/EV_OFF, delta-filtered) so each member's singing loops - the looper-as-notation idea from radiophonic-workshop.md section 4, second customer. Two delia-style rhythm rows underneath.",
  "homage": "Otamatone (Maywa Denki, 1998)",
  "todo": [
    "family portrait pass: better faces (sprite-draw glyphs), dog tail wag on woof",
    "overdub vs replace: arming currently layers; a punch-in replace mode",
    "save_bytes: persist loops + drum rows between runs",
    "park a duet seed clip at tools/clips/otafamily/"
  ],
  "description": {
    "summary": "A singing family of four otamatones - daddy, mommy, junior and the dog - each with its own voice, plus a live-looper so the whole household ends up singing rounds together.",
    "detail": "Four otamatones drawn as a family, each a drag-to-play fretless ribbon with its OWN character: DADDY sings low and slow (lazy 90ms glide, dark muffled mouth, slow wide vibrato), MOMMY mid and graceful, JUNIOR high and jumpy (22ms glide, thin bright buzz, eager fast vibrato), and the DOG doesn't really sing - every press starts a WOOF (the note starts +6 semitones and drops), holding turns it into a HOWL that drifts sharp with a huge wobble, and it's permanently a little out of tune. Wiggle sideways off the ribbon to open each mouth (a resonant-filter wah - the faces' pixel mouths open to match). The LOOP: arm a member with its REC button and everything you play on it - the continuous pitch/mouth gesture, not quantized notes - records into a shared 8-beat tape loop and replays verbatim as a ghost dot on the ribbon while you layer the next family member on top. Two step-sequencer rows (BOOM/TICK) run underneath for rhythm, delia-style tap cells. The loop bar up top shows every member's recorded gestures as colored dots.",
    "controls": "drag a stem to play (multitouch: one finger per family member) . wiggle sideways = open the mouth . REC arms that member's loop, X clears it, M mutes . tap BOOM/TICK cells for rhythm . 1-4 = a little yip from each member . UP/DOWN tempo"
  }
}
de:meta */
#include "studio.h"
#include "pointer.h"
#include <math.h>
#include <stdio.h>

// ── OTA FAMILY ────────────────────────────────────────────────────────────
// The otamatone voice (thin nasal square + resonant lowpass, held note ridden
// every frame) times four, differentiated the way a real family is: range,
// patience (glide), thinness (duty) and vibrato. The looper records each
// member's RIBBON GESTURE as a CC stream (loopstation's EV scheme) - the
// "looper is the notation for buttonless instruments" idea, second customer.
// Voice budget note: 4 live + 4 replay + 2 short drums can brush the 8-voice
// ceiling; drums are short and steal gracefully.

#define LOOP_BEATS 8
#define MAXEV 256
#define NSTEP 16
#define RIB_TOP 76
#define RIB_BOT 140

enum { C_DAD, C_MOM, C_KID, C_DOG, NC };
enum { EV_ON, EV_CC, EV_OFF };
enum { SL_DAD = 9, SL_MOM, SL_KID, SL_DOG, SL_BOOM, SL_TICK };

typedef struct { float pos; int kind; float pitch, mouth; int born; } Ev;

typedef struct {
    const char *name;
    int   bx;                 // box left (all boxes 70 wide)
    int   headr;
    float mlo, mhi;           // ribbon MIDI range
    int   glide;              // ms - the family member's patience
    float duty;               // pulse thinness = voice nasality
    int   cut_lo, cut_hi;     // mouth-closed .. mouth-open cutoff
    float lfo_hz, lfo_dep;    // vibrato personality
    int   col, vol;
} Spec;

static const Spec SPEC[NC] = {
    { "DADDY",   8, 15, 26, 50, 90, 0.20f, 300, 2200, 4.2f, 0.12f, CLR_BLUE,   6 },
    { "MOMMY",  86, 12, 45, 69, 50, 0.12f, 480, 3600, 5.5f, 0.20f, CLR_PINK,   5 },
    { "JUNIOR", 164, 9, 62, 86, 22, 0.08f, 700, 5200, 7.5f, 0.34f, CLR_YELLOW, 5 },
    { "DOG",   242, 12, 38, 62, 14, 0.30f, 380, 2800, 6.5f, 0.45f, CLR_ORANGE, 6 },
};

typedef struct {
    int   v;                  // live voice handle (-1 = silent)
    float pitch, mouth, held, squash, yt;
    int   armed, muted;
    Ev    ev[MAXEV]; int n;
    int   rv, ron;            // replay voice + ghost active
    float rp, rm;             // ghost pitch/mouth (for drawing)
    float lastp, lastm;       // CC delta filter
} Chr;

static Chr  fam[NC];
typedef struct { int id, c; } Ptr;   // finger -> family member (-1 = elsewhere)
static Ptr  ptr[PTR_MAX];
static float songb, lp, prev_lp;
static int   loop_i, tempo = 100, last8 = -1;
static unsigned char boom[NSTEP], tick[NSTEP];

static float rib_midi(int c, int y) {
    float t = clamp((float)(y - RIB_TOP) / (RIB_BOT - RIB_TOP), 0, 1);
    return lerp(SPEC[c].mhi, SPEC[c].mlo, t);
}
static int rib_y(int c, float midi) {
    float t = (SPEC[c].mhi - midi) / (SPEC[c].mhi - SPEC[c].mlo);
    return RIB_TOP + (int)(clamp(t, 0, 1) * (RIB_BOT - RIB_TOP));
}
static int mouth_cut(int c, float m) { return (int)lerp(SPEC[c].cut_lo, SPEC[c].cut_hi, m); }

static void rec_ev(int c, int kind, float pitch, float mouth) {
    Chr *F = &fam[c];
    if (!F->armed || F->n >= MAXEV) return;
    F->ev[F->n++] = (Ev){ lp, kind, pitch, mouth, loop_i };
}

static void rep_off(int c) {
    if (fam[c].rv >= 0) { note_off(fam[c].rv); fam[c].rv = -1; }
    fam[c].ron = 0;
}

static void fire_ev(int c, Ev *e) {
    Chr *F = &fam[c];
    if (e->kind == EV_ON) {
        if (F->rv >= 0) note_off(F->rv);
        F->rv = note_on((int)e->pitch, SL_DAD + c, SPEC[c].vol);
        note_glide(F->rv, SPEC[c].glide);
        F->ron = 1; F->squash = 1;
    } else if (e->kind == EV_CC && F->rv >= 0) {
        note_pitch(F->rv, e->pitch);
        note_cutoff(F->rv, mouth_cut(c, e->mouth));
    } else if (e->kind == EV_OFF) rep_off(c);
    F->rp = e->pitch; F->rm = e->mouth;
}

// fire every recorded event the playhead crossed since last frame (wrap-aware,
// skipping events born this pass - already heard live). The loopstation scheme.
static void fire_replay(void) {
    int wrap = lp < prev_lp;
    for (int c = 0; c < NC; c++) {
        Chr *F = &fam[c];
        if (F->muted) continue;
        for (int i = 0; i < F->n; i++) {
            Ev *e = &F->ev[i];
            int hitnow = wrap ? (e->pos > prev_lp || e->pos <= lp)
                              : (e->pos > prev_lp && e->pos <= lp);
            if (!hitnow) continue;
            int pass = (wrap && e->pos > prev_lp) ? loop_i - 1 : loop_i;
            if (e->born == pass) continue;
            fire_ev(c, e);
        }
    }
}

static void member_on(int c, int y) {
    Chr *F = &fam[c];
    float m = rib_midi(c, y);
    float start = (c == C_DOG) ? m + 6 : m;           // the dog WOOFS: starts high, drops
    F->v = note_on((int)start, SL_DAD + c, SPEC[c].vol);
    note_glide(F->v, SPEC[c].glide);
    F->pitch = m; F->held = 0; F->squash = 1;
    F->lastp = -999; F->lastm = -999;
    rec_ev(c, EV_ON, start, F->mouth);
}

static void member_off(int c) {
    Chr *F = &fam[c];
    if (F->v < 0) return;
    note_off(F->v); F->v = -1;
    rec_ev(c, EV_OFF, F->pitch, F->mouth);
}

void init(void) {
    for (int c = 0; c < NC; c++) {
        const Spec *S = &SPEC[c];
        int sl = SL_DAD + c;
        // the otamatone reed buzz, per family member
        instrument(sl, INSTR_SQUARE, 8, 90, 6, 130);
        instrument_duty(sl, S->duty);
        instrument_filter(sl, FILTER_LOW, S->cut_lo, 9);
        instrument_lfo(sl, 0, LFO_PITCH, S->lfo_hz, S->lfo_dep);
        fam[c].v = fam[c].rv = -1;
    }
    instrument_tune(SL_DOG, 0.35f);                   // the dog is a little off. always.

    instrument(SL_BOOM, INSTR_SINE, 2, 100, 0, 55);   // soft floor thump
    instrument_filter(SL_BOOM, FILTER_LOW, 320, 2);
    instrument(SL_TICK, INSTR_MALLET, 1, 45, 0, 60);  // woodblock tick
    instrument_harmonics(SL_TICK, 0.25f);
    instrument_timbre(SL_TICK, 0.85f);

    boom[0] = boom[8] = boom[11] = 1;                 // a starter groove
    tick[4] = tick[12] = 1;
    bpm(tempo);
    PTR_CLEAR(ptr);
}

void update(void) {
    prev_lp = lp;
    songb   = beat() + beat_pos();
    lp      = fmodf(songb, (float)LOOP_BEATS);
    loop_i  = (int)(songb / LOOP_BEATS);

    if (keyp(KEY_UP))   { tempo += 4; if (tempo > 180) tempo = 180; bpm(tempo); }
    if (keyp(KEY_DOWN)) { tempo -= 4; if (tempo <  60) tempo =  60; bpm(tempo); }

    // ---- fingers on ribbons (one finger per family member) ----
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;
        if (fresh) {
            p->c = -1;
            if (ty >= 26 && ty <= 146)
                for (int c = 0; c < NC; c++)
                    if (tx >= SPEC[c].bx && tx < SPEC[c].bx + 70 && fam[c].v < 0)
                        { p->c = c; member_on(c, ty); break; }
        } else if (p->c >= 0) {
            int c = p->c; Chr *F = &fam[c];
            if (F->v < 0) continue;
            F->held += 1.0f / 60.0f;
            float target = rib_midi(c, ty);
            if (c == C_DOG && F->held > 0.35f)        // the held woof becomes a HOWL,
                target += clamp((F->held - 0.35f) * 2.0f, 0, 3);   // drifting sharp
            F->pitch = target;
            float mt = clamp(fabsf((float)tx - (SPEC[c].bx + 35)) / 26.0f, 0, 1);
            if (c == C_DOG && F->held < 0.15f) mt = 1.0f;          // woof pops the jaw
            F->mouth = lerp(F->mouth, mt, 0.3f);
            note_pitch(F->v, F->pitch);
            note_cutoff(F->v, mouth_cut(c, F->mouth));
            // record the gesture only when the hand actually moved (delta filter)
            if (fabsf(F->pitch - F->lastp) > 0.12f || fabsf(F->mouth - F->lastm) > 0.06f) {
                rec_ev(c, EV_CC, F->pitch, F->mouth);
                F->lastp = F->pitch; F->lastm = F->mouth;
            }
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (!p) continue;
        if (p->c >= 0) member_off(p->c);
        p->id = PTR_NONE;
    }

    // ---- yips (1-4), timed auto-notes ----
    for (int c = 0; c < NC; c++) {
        Chr *F = &fam[c];
        if (keyp('1' + c) && F->v < 0) {
            member_on(c, (RIB_TOP + RIB_BOT) / 2);
            F->yt = (c == C_DOG) ? 0.14f : 0.25f;
        }
        if (F->yt > 0) {
            F->yt -= 1.0f / 60.0f;
            if (F->yt <= 0) member_off(c);
        }
    }

    // ---- per-member buttons ----
    for (int c = 0; c < NC; c++) {
        int bx = SPEC[c].bx;
        if (tapp(bx + 2, 150, 26, 12)) fam[c].armed = !fam[c].armed;
        if (tapp(bx + 32, 150, 16, 12)) { fam[c].n = 0; rep_off(c); }
        if (tapp(bx + 52, 150, 16, 12)) { fam[c].muted = !fam[c].muted; if (fam[c].muted) rep_off(c); }
    }

    // ---- drum rows: tap cells ----
    for (int s = 0; s < NSTEP; s++) {
        if (tapp(44 + s * 16, 170, 15, 12)) boom[s] = !boom[s];
        if (tapp(44 + s * 16, 185, 15, 12)) tick[s] = !tick[s];
    }

    // ---- 8th-note drum clock, scheduled one step ahead (cr78/delia pattern) ----
    int s8 = (int)(songb * 2.0f);
    if (s8 != last8) {
        bool first = (last8 < 0);
        float frac = songb * 2.0f - s8;
        int step_ms = 30000 / tempo;
        int delay = (int)((1.0f - frac) * step_ms);
        int nx = (s8 + 1) % NSTEP;
        if (boom[nx]) schedule_hit(delay, 41, SL_BOOM, 5, 110);
        if (tick[nx]) schedule_hit(delay, 89, SL_TICK, 3, 28);
        if (first) {
            int cu = s8 % NSTEP;
            if (boom[cu]) schedule_hit(0, 41, SL_BOOM, 5, 110);
            if (tick[cu]) schedule_hit(0, 89, SL_TICK, 3, 28);
        }
        last8 = s8;
    }

    fire_replay();

#ifdef DE_TRACE
    watch("lp", "%.2f", lp);
    watch("ev", "%d %d %d %d", fam[0].n, fam[1].n, fam[2].n, fam[3].n);
#endif
}

static void draw_member(int c) {
    const Spec *S = &SPEC[c];
    Chr *F = &fam[c];
    int bx = S->bx, cx = bx + 35;
    int singing = F->v >= 0 || F->ron;
    float m = F->v >= 0 ? F->mouth : (F->ron ? F->rm : 0);

    rect(bx, 26, 70, 120, CLR_DARKER_GREY);

    // head (squashes on note-on, bobs while singing)
    int hy = 48 + (int)(F->squash * 3) + (singing ? (int)(sinf(songb * 6.283f) * 1.5f) : 0);
    F->squash = lerp(F->squash, 0, 0.15f);
    circfill(cx, hy, S->headr, CLR_DARK_GREY);
    circ(cx, hy, S->headr, S->col);
    if (c == C_DOG) {                                  // ears + snout
        rectfill(cx - S->headr - 1, hy - S->headr + 2, 5, 8, S->col);
        rectfill(cx + S->headr - 4, hy - S->headr + 2, 5, 8, S->col);
        pset(cx, hy + 3, CLR_BLACK);                   // nose
    }
    if (c == C_DAD)                                    // the monobrow
        rectfill(cx - 6, hy - 6, 12, 2, S->col);
    if (c == C_MOM) {                                  // eyelashes
        pset(cx - 7, hy - 5, S->col); pset(cx + 7, hy - 5, S->col);
    }
    pset(cx - 4, hy - 3, CLR_WHITE); pset(cx + 4, hy - 3, CLR_WHITE);   // eyes
    int mh = 2 + (int)(m * (S->headr - 2));            // the mouth opens
    rectfill(cx - 3, hy + S->headr / 2 - 1, 7, mh, singing ? CLR_BLACK : CLR_DARK_GREY);

    // ribbon + live/ghost dots
    line(cx, RIB_TOP, cx, RIB_BOT, CLR_MEDIUM_GREY);
    if (F->ron) circ(cx, rib_y(c, F->rp), 3, CLR_LIGHT_GREY);            // the ghost
    if (F->v >= 0) circfill(cx, rib_y(c, F->pitch), 3, S->col);

    // buttons
    rectfill(bx + 2, 150, 26, 12, F->armed ? CLR_RED : CLR_DARKER_GREY);
    rectfill(bx + 32, 150, 16, 12, CLR_DARKER_GREY);
    rectfill(bx + 52, 150, 16, 12, F->muted ? CLR_RED : CLR_DARKER_GREY);
    font(FONT_SMALL);
    print("REC", bx + 7, 153, F->armed ? CLR_WHITE : CLR_MEDIUM_GREY);
    print("X",  bx + 38, 153, CLR_MEDIUM_GREY);
    print("M",  bx + 58, 153, F->muted ? CLR_WHITE : CLR_MEDIUM_GREY);
    print(S->name, cx - text_width(S->name) / 2, 30, S->col);
    font(FONT_NORMAL);
}

void draw(void) {
    cls(CLR_BLACK);

    print("OTA FAMILY", 8, 4, CLR_WHITE);
    font(FONT_SMALL);
    print("a singing household + tape loop", 96, 6, CLR_MEDIUM_GREY);
    char buf[32];
    snprintf(buf, sizeof buf, "%d bpm", tempo);
    print(buf, 284, 6, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    // loop bar: playhead + every member's recorded gestures as dots on its row
    rect(8, 14, 304, 8, CLR_DARK_GREY);
    rectfill(9, 15, (int)(lp / LOOP_BEATS * 302), 6, CLR_DARKER_GREY);
    for (int c = 0; c < NC; c++)
        for (int i = 0; i < fam[c].n; i++)
            if (fam[c].ev[i].kind == EV_ON)
                pset(9 + (int)(fam[c].ev[i].pos / LOOP_BEATS * 302), 15 + c + 1, SPEC[c].col);
    pset(9 + (int)(lp / LOOP_BEATS * 302), 15, CLR_WHITE);

    for (int c = 0; c < NC; c++) draw_member(c);

    // drum rows
    int ph = ((int)(songb * 2.0f)) % NSTEP;
    font(FONT_SMALL);
    print("BOOM", 12, 173, CLR_ORANGE);
    print("TICK", 12, 188, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
    for (int s = 0; s < NSTEP; s++) {
        int bx2 = 44 + s * 16;
        if (boom[s]) rectfill(bx2 + 1, 171, 13, 10, CLR_ORANGE);
        else         rect(bx2 + 1, 171, 13, 10, CLR_DARKER_GREY);
        if (tick[s]) rectfill(bx2 + 1, 186, 13, 10, CLR_LIGHT_GREY);
        else         rect(bx2 + 1, 186, 13, 10, CLR_DARKER_GREY);
        if (s == ph) { rect(bx2, 170, 15, 12, CLR_WHITE); rect(bx2, 185, 15, 12, CLR_WHITE); }
    }
    font(FONT_SMALL);
    print("1-4 yip", 302 - text_width("1-4 yip"), 165, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
