/* de:meta
{
  "slug": "slipstep",
  "title": "slipstep",
  "status": "active",
  "created": "2026-09-28",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "polymeter",
    "step-sequencer"
  ],
  "lineage": "Row 7 of docs/design/choochootracker-borrow-list.md: Choochootracker's per-track playback speed (after Nerdseq and Elektron), the one sequencer idea from that repo our racks did not already have (acidcandy + morphbox carry probability, p-locks and trig conditions). Five loops, each with its own LENGTH (polymeter) and its own SPEED RATIO (polyrhythm), so the patterns drift apart and meet again. Voiced on the three engines the same borrow list produced: INSTR_SINTER kick + snare, INSTR_METAL hat, INSTR_MME bass, plus an INSTR_PLUCK lead.",
  "homage": "Nerdseq and the Elektron boxes' per-track scale, by way of Choochootracker (2026); Steve Reich's phase pieces for the listening.",
  "description": {
    "summary": "Five loops, each with its own length and its own speed, drifting apart and coming back together. A readout counts the bars until they all line up again.",
    "detail": "Every track is a short step loop with two chips: LEN (3..16 steps) and SPEED (1/2, 2/3, 3/4, 1, 5/4, 4/3, 3/2, 2). A track at 3/2 plays three steps in the time the others play two; a 7-step loop against 16-step ones lands its downbeat somewhere new every bar. Step times come from the audio beat clock and each track's exact ratio, queued a few ms ahead, so an odd ratio stays sample-accurate instead of snapping to frames. The footer says how many bars until every track is back on its own first step at the same moment (the least common multiple of the track periods), which is the thing a polymeter is always walking toward. Tap cells to edit, tap a name to mute.",
    "controls": "SPACE: play / stop · R: resync (all tracks back to step 1 on the next 16th) · 1-5: select a track · LEFT/RIGHT: its speed · UP/DOWN: its length · M: mute it · tap a cell to toggle a step, tap the LEN / SPEED chips to step them, tap a name to mute"
  },
  "todo": [
    "the same SPEED chip on acidcandy's tracks (its own design pass: acidcandy's steps are p-locked, so a slowed track must still read its locks per step)",
    "a swing that follows each track's own speed"
  ]
}
de:meta */
// slipstep — per-track playback speed + per-track loop length: polyrhythm and polymeter at once.
//
// The one idea from Choochootracker's sequencer the racks did not already have. Each track is
// a loop of LEN steps played at SPEED × the base 16th, so its period is LEN × den/num 16ths.
//
// TIMING. Frames are 16.7 ms and a 3/2 track's step is 2/3 of a 16th, so stepping per frame
// would jitter. Instead every track keeps an anchor (an audio time t0 and a step count k from
// it) and its k-th step lands at t0 + k × step_ms × den/num EXACTLY; each frame queues the
// steps that fall inside the next LOOKAHEAD ms with schedule_hit(delay). "Now" is the audio
// beat clock (beat() + beat_pos()), not frame time, so the grid follows the sound. A speed or
// length change re-anchors the track at its next pending step, so it never skips or doubles.
//
// THE READOUT. All tracks are back on their own step 1 together after
//   LCM over tracks of (LEN × den / num)  16ths,
// the LCM of rationals being lcm(numerators) / gcd(denominators) once each is reduced.
//
// controls: SPACE play · R resync · 1-5 select · LEFT/RIGHT speed · UP/DOWN length · M mute

#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>

#define NT        5
#define MAXLEN    16
#define LOOKAHEAD 40.0                      // ms of steps queued ahead of the audio clock
#define BPM       112

enum { T_KICK, T_SNARE, T_HAT, T_BASS, T_LEAD };
static const char *TNAME[NT] = { "KICK", "SNARE", "HAT", "BASS", "LEAD" };
#define S_KICK  5
#define S_SNARE 6
#define S_HAT   7
#define S_BASS  8
#define S_LEAD  9

#define NSPEED 8
static const int SP_NUM[NSPEED] = { 1, 2, 3, 1, 5, 4, 3, 2 };
static const int SP_DEN[NSPEED] = { 2, 3, 4, 1, 4, 3, 2, 1 };
static const char *SP_NAME[NSPEED] = { "1/2", "2/3", "3/4", "1", "5/4", "4/3", "3/2", "2" };

typedef struct {
    int    len, speed;                      // speed = index into SP_*
    bool   on[MAXLEN];
    bool   mute;
    // scheduling: the k-th step after the anchor sits at t0 + k × period, and it is step
    // (pos0 + k) % len of the loop
    double t0;                              // anchor time, ms (audio clock)
    long   k;                               // steps already queued since the anchor
    int    pos0;                            // loop position at the anchor
    int    shown;                           // last step whose time has PASSED (for the playhead)
} Track;

static Track T[NT];
static bool  playing = true;
static bool  started = false;
static int   sel = 0;
static float glow[NT];
static double last_now = 0.0;

// melodic content: a scale degree per step (bass + lead), minor pentatonic
static const int BASS_DEG[MAXLEN] = { 0, 0, 3, 0, 4, 0, 2, 3, 0, 5, 0, 3, 4, 2, 0, 1 };
static const int LEAD_DEG[MAXLEN] = { 7, 9, 10, 7, 12, 10, 9, 7, 5, 7, 9, 12, 10, 9, 7, 5 };

// ── pure timing maths (spec'd) ──────────────────────────────────────────────────
static double step16_ms(void) { return 60000.0 / (double)BPM / 4.0; }
static double track_step_ms(const Track *t) { return step16_ms() * (double)SP_DEN[t->speed] / (double)SP_NUM[t->speed]; }
static long gcdl(long a, long b) { while (b) { long r = a % b; a = b; b = r; } return a < 0 ? -a : a; }
static long lcml(long a, long b) { return a / gcdl(a, b) * b; }
// period of one loop of track t, as a reduced fraction of 16ths: len × den / num
static void period_frac(const Track *t, long *num, long *den) {
    long n = (long)t->len * SP_DEN[t->speed], d = SP_NUM[t->speed];
    long g = gcdl(n, d);
    *num = n / g; *den = d / g;
}
// 16ths until every track is on its own step 1 at the same moment (the LCM of the periods)
static long meet_16ths(const Track *ts, int n, long *den_out) {
    long L = 1, G = 0;
    for (int i = 0; i < n; i++) {
        long a, b; period_frac(&ts[i], &a, &b);
        L = lcml(L, a);
        G = G ? gcdl(G, b) : b;
    }
    if (den_out) *den_out = G;
    return L;                                   // the meeting point is L / G 16ths
}
static double meet_bars(const Track *ts, int n) { long g; long l = meet_16ths(ts, n, &g); return (double)l / (double)g / 16.0; }

// the time of the NEXT unqueued step of t, and which loop step it is
static double next_time(const Track *t) { return t->t0 + (double)t->k * track_step_ms(t); }
static int    next_pos(const Track *t)  { return (int)((t->pos0 + t->k) % t->len); }

// re-anchor at the next pending step (a speed/length change keeps time continuous)
static void reanchor(Track *t) {
    double tn = next_time(t);
    int    pn = next_pos(t);
    t->t0 = tn; t->pos0 = pn % t->len; t->k = 0;
}

// queue every step of t that falls before t_ms + LOOKAHEAD; `emit` receives (pos, delay_ms).
// Pure over (t, t_ms): the audio side lives in emit.
static int schedule_window(Track *t, double t_ms, void (*emit)(int track, int pos, int delay_ms), int ti) {
    int fired = 0;
    for (;;) {
        double tn = next_time(t);
        if (tn >= t_ms + LOOKAHEAD) break;
        int pos = next_pos(t);
        int delay = (int)(tn - t_ms + 0.5);
        if (delay < 0) delay = 0;                 // a late step (a long frame) fires t_ms, never skipped
        if (emit) emit(ti, pos, delay);
        t->k++;
        fired++;
    }
    return fired;
}

// ── sound ───────────────────────────────────────────────────────────────────────
static int penta(int deg, int base) { static const int d[5] = { 0, 3, 5, 7, 10 }; return base + (deg / 5) * 12 + d[deg % 5]; }

static void build_kit(void) {
    instrument(S_KICK, INSTR_SINTER, 0, 0, 7, 20);                 // sinter/knot kick
    instrument_harmonics(S_KICK, 0.08f); instrument_timbre(S_KICK, 0.55f); instrument_morph(S_KICK, 0.20f);
    instrument_mode(S_KICK, MODE_SINTER_A, 0.35f); instrument_mode(S_KICK, MODE_SINTER_B, 0.30f);
    instrument_mode(S_KICK, MODE_SINTER_MOTION, 0.85f); instrument_mode(S_KICK, MODE_SINTER_DECAY, 0.40f);
    instrument(S_SNARE, INSTR_SINTER, 0, 0, 7, 20);                // sinter/shard snare
    instrument_harmonics(S_SNARE, 0.25f); instrument_timbre(S_SNARE, 0.45f); instrument_morph(S_SNARE, 0.35f);
    instrument_mode(S_SNARE, MODE_SINTER_A, 0.62f); instrument_mode(S_SNARE, MODE_SINTER_B, 0.40f);
    instrument_mode(S_SNARE, MODE_SINTER_MOTION, 0.70f); instrument_mode(S_SNARE, MODE_SINTER_DECAY, 0.30f);
    instrument_level(S_SNARE, 0.8f);
    instrument(S_HAT, INSTR_METAL, 0, 0, 7, 25);                   // metal/closed hat
    instrument_harmonics(S_HAT, 0.45f); instrument_timbre(S_HAT, 0.55f); instrument_morph(S_HAT, 0.85f);
    instrument_mode(S_HAT, MODE_METAL_DECAY, 0.22f); instrument_mode(S_HAT, MODE_METAL_SPREAD, 0.20f);
    instrument_filter(S_HAT, FILTER_HIGH, 700, 1);
    instrument_level(S_HAT, 0.55f);
    instrument(S_BASS, INSTR_MME, 3, 0, 7, 120);                   // mme/ring bass
    instrument_harmonics(S_BASS, 0.07f); instrument_timbre(S_BASS, 0.55f); instrument_morph(S_BASS, 0.15f);
    instrument_mode(S_BASS, MODE_MME_FEEDBACK, 0.25f);
    instrument_level(S_BASS, 0.7f);
    instrument(S_LEAD, INSTR_PLUCK, 1, 260, 0, 120);
    instrument_harmonics(S_LEAD, 0.45f); instrument_timbre(S_LEAD, 0.6f);
    instrument_level(S_LEAD, 0.6f);
    instrument_echo(S_LEAD, 0.25f);
    echo(3 * (int)step16_ms(), 0.35f, 0.5f);
}

static void emit_audio(int ti, int pos, int delay) {
    Track *t = &T[ti];
    if (t->mute || !t->on[pos]) return;
    switch (ti) {
        case T_KICK:  schedule_hit(delay, 36, S_KICK, 7, 700); break;
        case T_SNARE: schedule_hit(delay, 51, S_SNARE, 6, 500); break;
        case T_HAT:   schedule_hit(delay, 57 + (pos & 1) * 2, S_HAT, (pos & 1) ? 4 : 6, 300); break;
        case T_BASS:  schedule_hit(delay, penta(BASS_DEG[pos], 33), S_BASS, 6, (int)(track_step_ms(t) * 0.8)); break;
        case T_LEAD:  schedule_hit(delay, penta(LEAD_DEG[pos] - 5, 60), S_LEAD, 5, 260); break;
    }
}

// ── transport ───────────────────────────────────────────────────────────────────
static double audio_now(void) { return ((double)beat() + (double)beat_pos()) * 60000.0 / (double)BPM; }

static void resync(double at) {
    for (int i = 0; i < NT; i++) { T[i].t0 = at; T[i].k = 0; T[i].pos0 = 0; T[i].shown = -1; }
}
static void set_speed(int ti, int sp) {
    Track *t = &T[ti];
    if (sp < 0) sp = 0; if (sp >= NSPEED) sp = NSPEED - 1;
    if (sp == t->speed) return;
    reanchor(t); t->speed = sp;
}
static void set_len(int ti, int len) {
    Track *t = &T[ti];
    if (len < 3) len = 3; if (len > MAXLEN) len = MAXLEN;
    if (len == t->len) return;
    reanchor(t); t->len = len; t->pos0 %= len;
}

static void init_tracks(void) {
    static const char *PAT[NT] = {
        "x...x...x..x.x..",   // kick: 16 @ 1
        "....x.......x...",   // snare: 16 @ 1 (the anchor)
        "x.xxx.xxx.xx....",   // hat: 12 @ 3/2
        "x.xx.x.x........",   // bass: 7 @ 1
        "x.x.xx..........",   // lead: 5 @ 2/3
    };
    static const int LEN[NT] = { 16, 16, 12, 7, 5 };
    static const int SPD[NT] = { 3, 3, 6, 3, 1 };      // 1, 1, 3/2, 1, 2/3
    for (int i = 0; i < NT; i++) {
        memset(&T[i], 0, sizeof T[i]);
        T[i].len = LEN[i]; T[i].speed = SPD[i]; T[i].shown = -1;
        for (int s = 0; s < MAXLEN; s++) T[i].on[s] = PAT[i][s] == 'x';
    }
}

void init(void) {
    init_tracks();
    build_kit();
    bpm(BPM);
}

void update(void) {
    double t_ms = audio_now();
    last_now = t_ms;
    if (!started) { resync(t_ms + 20.0); started = true; }

    if (keyp(KEY_SPACE)) { playing = !playing; if (playing) resync(t_ms + 20.0); }
    if (keyp('R')) { double q = step16_ms(); resync((floor(t_ms / q) + 1.0) * q); }
    for (int i = 0; i < NT; i++) if (keyp('1' + i)) sel = i;
    if (keyp(KEY_LEFT))  set_speed(sel, T[sel].speed - 1);
    if (keyp(KEY_RIGHT)) set_speed(sel, T[sel].speed + 1);
    if (keyp(KEY_UP))    set_len(sel, T[sel].len + 1);
    if (keyp(KEY_DOWN))  set_len(sel, T[sel].len - 1);
    if (keyp('M')) T[sel].mute = !T[sel].mute;

    if (playing) for (int i = 0; i < NT; i++) schedule_window(&T[i], t_ms, emit_audio, i);

    // playheads: the last step whose time has passed (steps are queued ahead, heard later)
    for (int i = 0; i < NT; i++) {
        Track *t = &T[i];
        double sm = track_step_ms(t);
        long passed = (long)floor((t_ms - t->t0) / sm);
        int pos = passed < 0 ? -1 : (int)((t->pos0 + passed) % t->len);
        if (pos != t->shown && pos >= 0 && playing && !t->mute && t->on[pos]) glow[i] = 1.0f;
        t->shown = playing ? pos : -1;
    }

#ifdef DE_TRACE
    watch("sel", "%d", sel);
    watch("speed", "%s", SP_NAME[T[sel].speed]);
    watch("len", "%d", T[sel].len);
    watch("meet_bars", "%.2f", meet_bars(T, NT));
    for (int i = 0; i < NT; i++) watch(str("k%d", i), "%ld", T[i].k);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();
    print("SLIPSTEP", 6, 4, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("per-track speed + length", 76, 6, CLR_MEDIUM_GREY);
    if (ui_button(SCREEN_W - 50, 2, 44, 13, playing ? "STOP" : "PLAY")) { playing = !playing; if (playing) resync(last_now + 20.0); }
    if (ui_button(SCREEN_W - 98, 2, 44, 13, "RESYNC")) { double q = step16_ms(); resync((floor(last_now / q) + 1.0) * q); }

    const int y0 = 22, rh = 30, cx = 92, cw = 13;
    for (int i = 0; i < NT; i++) {
        Track *t = &T[i];
        int y = y0 + i * rh;
        glow[i] *= 0.86f;
        if (i == sel) rectfill(0, y - 2, SCREEN_W, rh - 2, CLR_DARKER_GREY);
        // name = mute toggle
        int nc = t->mute ? CLR_DARK_GREY : glow[i] > 0.3f ? CLR_LIGHT_YELLOW : CLR_WHITE;
        font(FONT_NORMAL);
        print(TNAME[i], 4, y + 2, nc);
        if (t->mute) { font(FONT_TINY); print("muted", 4, y + 13, CLR_DARK_GREY); }
        if (tapp(0, y - 2, 44, rh - 2)) { if (sel == i) t->mute = !t->mute; sel = i; }
        // chips
        font(FONT_SMALL);
        if (ui_button(46, y, 20, 11, SP_NAME[t->speed])) { sel = i; set_speed(i, (t->speed + 1) % NSPEED); }
        if (ui_button(68, y, 20, 11, str("%d", t->len))) { sel = i; set_len(i, t->len >= MAXLEN ? 3 : t->len + 1); }
        font(FONT_TINY);
        print("speed", 47, y + 13, CLR_DARK_GREY);
        print("len", 71, y + 13, CLR_DARK_GREY);
        // cells: width stretches with the track's step length, so a 3/2 track's cells are 2/3 as
        // wide as a 1× track's and the rows show the drift as geometry, not only as a playhead
        double scale = (double)SP_DEN[t->speed] / (double)SP_NUM[t->speed];
        int w = (int)(cw * scale + 0.5); if (w < 5) w = 5;
        for (int s = 0; s < t->len; s++) {
            int x = cx + s * w;
            if (x + w - 1 > SCREEN_W - 2) break;
            int col = t->on[s] ? (i == T_KICK || i == T_SNARE ? CLR_ORANGE : i == T_HAT ? CLR_PEACH : CLR_LIME_GREEN) : CLR_DARK_BROWN;
            if (s == t->shown && playing) col = t->on[s] ? CLR_LIGHT_YELLOW : CLR_MEDIUM_GREY;
            rectfill(x, y, w - 1, 20, col);
            if (s == 0) rect(x, y, w - 1, 20, CLR_WHITE);
            if (tapp(x, y, w - 1, 20)) { t->on[s] = !t->on[s]; sel = i; }
        }
    }

    // the readout: when do they all meet again?
    font(FONT_SMALL);
    double bars = meet_bars(T, NT);
    double elapsed_bars = (last_now - T[0].t0) / (step16_ms() * 16.0);
    if (bars <= 9999.0)
        print(str("all tracks meet again every %.4g bars", bars), 6, SCREEN_H - 24, CLR_YELLOW);
    else
        print("all tracks meet again in > 9999 bars", 6, SCREEN_H - 24, CLR_YELLOW);
    if (playing && bars > 0.0) {
        double into = fmod(elapsed_bars > 0 ? elapsed_bars : 0.0, bars);
        bar(6, SCREEN_H - 15, SCREEN_W - 12, 4, (float)(into / bars), CLR_ORANGE, CLR_DARKER_GREY);
    }
    font(FONT_TINY);
    print("SPACE play  R resync  1-5 track  LEFT/RIGHT speed  UP/DOWN len  M mute", 6, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
    ui_end();
}

#ifdef DE_SPEC
#include "spec.h"
static int ev_count[NT];
static int ev_pos[64], ev_n;
static int ev_delay_bad;
static void emit_count(int ti, int pos, int delay) {
    ev_count[ti]++;
    if (ev_n < 64 && ti == T_HAT) ev_pos[ev_n++] = pos;
    if (delay < 0 || delay > (int)LOOKAHEAD) ev_delay_bad++;
}
void spec(void) {
    step(1);
    // the readout: the default five meet every 105 bars (16, 16, 12 x 2/3 = 8, 7, 5 x 3/2 = 7.5 → 1680 16ths)
    expect(spec_close((float)meet_bars(T, NT), 105.0f, 0.001f), "default tracks meet every 105 bars");
    {
        Track a[2]; memset(a, 0, sizeof a);
        a[0].len = 16; a[0].speed = 3; a[1].len = 16; a[1].speed = 3;
        expect(spec_close((float)meet_bars(a, 2), 1.0f, 1e-4f), "two 16-step tracks at 1x meet every bar");
        a[1].speed = 6;                                              // 3/2: period 32/3 16ths
        expect(spec_close((float)meet_bars(a, 2), 2.0f, 1e-4f), "16 at 1x vs 16 at 3/2 meet every 2 bars (lcm(16, 32/3) = 32)");
        a[1].len = 12;                                               // 12 at 3/2 = 8 16ths
        expect(spec_close((float)meet_bars(a, 2), 1.0f, 1e-4f), "12 steps at 3/2 fit a 16-step bar exactly");
        a[0].len = 3; a[0].speed = 4; a[1].len = 4; a[1].speed = 3; // 3 at 5/4 = 12/5, 4 at 1 = 4
        expect(spec_close((float)meet_bars(a, 2), 0.75f, 1e-4f), "rational LCM: lcm(12/5, 4) = 12 16ths");
    }
    // timing: a track's k-th step is exactly k × its step length from the anchor
    {
        Track t; memset(&t, 0, sizeof t); t.len = 7; t.speed = 6;   // 3/2
        t.t0 = 1000.0;
        double sm = track_step_ms(&t);
        expect(spec_close((float)sm, (float)(step16_ms() * 2.0 / 3.0), 1e-4f), "a 3/2 track's step is 2/3 of a 16th");
        t.k = 5;
        expect(spec_close((float)next_time(&t), (float)(1000.0 + 5 * sm), 1e-3f), "step k lands at t0 + k × step (no accumulation)");
        expect_eq(next_pos(&t), 5, "step 5 of a 7-loop is position 5");
        t.k = 9;
        expect_eq(next_pos(&t), 2, "and step 9 wraps to position 2");
        // re-anchoring on a speed change keeps the next step's time and position
        double tn = next_time(&t); int pn = next_pos(&t);
        reanchor(&t); t.speed = 3;
        expect(spec_close((float)next_time(&t), (float)tn, 1e-3f) && next_pos(&t) == pn, "a speed change re-anchors at the next step: no skip, no double");
    }
    // the scheduler over 16 bars of simulated frames: counts follow the ratios, delays stay inside the window
    {
        memset(ev_count, 0, sizeof ev_count); ev_n = 0; ev_delay_bad = 0;
        init_tracks();
        resync(0.0);
        double bar_ms = step16_ms() * 16.0;
        for (double t_ms = 0.0; t_ms < 16.0 * bar_ms; t_ms += 1000.0 / 60.0)
            for (int i = 0; i < NT; i++) schedule_window(&T[i], t_ms, emit_count, i);
        // everything up to t_ms + LOOKAHEAD was queued: steps per track = ceil((16 bars + lookahead) / step)
        for (int i = 0; i < NT; i++) {
            double sm = track_step_ms(&T[i]);
            int want = (int)ceil((16.0 * bar_ms - 1000.0 / 60.0 + LOOKAHEAD) / sm);
            expect(abs(ev_count[i] - want) <= 1, str("%s queued %d steps in 16 bars (expected ~%d)", TNAME[i], ev_count[i], want));
        }
        expect(ev_count[T_HAT] * 2 > ev_count[T_KICK] * 3 - 3, "the 3/2 hat queues 3 steps for every 2 of the kick");
        expect(ev_delay_bad == 0, "every queued delay sits inside 0..LOOKAHEAD ms");
        int wrap_ok = 1; for (int j = 0; j < ev_n; j++) if (ev_pos[j] != j % 12) wrap_ok = 0;
        expect(wrap_ok, "the 12-step hat walks 0..11 and wraps");
    }
    // a late frame fires the step t_ms rather than skipping it
    {
        Track t; memset(&t, 0, sizeof t); t.len = 4; t.speed = 3; t.t0 = 0.0;
        memset(ev_count, 0, sizeof ev_count); ev_delay_bad = 0;
        int n = schedule_window(&t, 500.0, emit_count, 0);           // 500 ms late: every missed step
        int want = (int)ceil((500.0 + LOOKAHEAD) / track_step_ms(&t));
        expect(n == want, "a late frame queues every missed step (none skipped)");
    }
    // the panel
    init_tracks(); resync(0.0);
    spec_tap('3');
    expect_eq(sel, T_HAT, "key 3 selects the hat");
    spec_tap(KEY_RIGHT);
    expect_eq(T[T_HAT].speed, 7, "RIGHT raises its speed to 2");
    spec_tap(KEY_RIGHT);
    expect_eq(T[T_HAT].speed, 7, "and stops at 2");
    spec_tap(KEY_DOWN);
    expect_eq(T[T_HAT].len, 11, "DOWN shortens its loop");
    spec_tap('M');
    expect(T[T_HAT].mute, "M mutes it");
    spec_tap(KEY_SPACE);
    expect(!playing, "SPACE stops");
    spec_tap(KEY_SPACE);
    expect(playing, "SPACE plays again");
}
#endif
