/* de:meta
{
  "slug": "dreamchip",
  "title": "dreamchip",
  "status": "active",
  "created": "2026-11-17",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "step-sequencer",
    "fm-synth"
  ],
  "lineage": "The last stop of a retro-chip tour (ZX Spectrum beeper -> NES 2A03 -> C64 SID -> Mega Drive YM2612 -> SNES S-DSP -> AdLib OPL3) that played one shared groove on every machine. This is the 'next generation': a FANTASY chip spec, DC-16, built from dreamengine's own engines (FM4, a wavetable-style unison pad, PCM sample voices, the echo + plate reverb DSP) and shown as a live register monitor.",
  "description": {
    "summary": "A fantasy next-gen sound chip playing one groove, with a live register monitor.",
    "detail": "DREAMCHIP DC-16: 4 FM channels (4-operator), 4 wavetable channels, 8 PCM channels (16-bit, 44.1 kHz drum samples synthesised at boot), and a DSP (tempo-synced echo + plate reverb). It plays the groove from the retro tour: octave-bouncing FM bass and drums, an FM electric-piano arpeggio ping-ponging through the echo from bar 9, a wide filtered pad from bar 17. Notes are booked ahead on the audio clock; the monitor lights each channel and logs each register write as it sounds, over an oscilloscope of the real mix.",
    "controls": "Just listen. SPACE / tap restarts the song."
  }
}
de:meta */
// dreamchip — a fantasy "next generation" sound chip, DC-16, built from dreamengine engines.
//
// The chip spec (what the monitor shows):
//   FM0-FM3   4-operator FM        FM0 bass, FM1/FM2 electric-piano arp (left/right)
//   WT0-WT3   wavetable + filter   WT0-WT2 the pad chord
//   PCM0-PCM7 16-bit 44.1 kHz PCM  PCM0 kick, PCM1 snare, PCM2 hi-hat (synthesised at boot)
//   DSP       echo (dotted 8th, tempo-synced) + plate reverb
// The groove is the one the retro tour used (zxspec/groove.py), ported to C below.
#include "studio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------- sound setup
#define S_BASS   10
#define S_ARPL   11
#define S_ARPR   12
#define S_PAD    13
#define S_KICK   14
#define S_SNARE  15
#define S_HAT    16
#define PCM_KICK  0
#define PCM_SNARE 1
#define PCM_HAT   2

#define BPM       128
#define STEP_SEC  (60.0 / BPM / 4.0)      // one 16th note
#define LOOKAHEAD 0.15                    // book notes this far ahead of the audio clock

static unsigned int rng_state = 0x2026u;
static float noise1(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return (float)(rng_state >> 9) / 4194304.0f - 1.0f;
}

static void load_pcm(int slot, float *buf, int n) {
    float pk = 0.0f;
    for (int i = 0; i < n; i++) if (fabsf(buf[i]) > pk) pk = fabsf(buf[i]);
    if (pk > 0.0001f) for (int i = 0; i < n; i++) buf[i] *= 0.95f / pk;
    sample_load(slot, buf, n);
}

static void make_drums(void) {
    const int sr = 44100;
    int n = (int)(0.25f * sr);
    float *buf = (float *)calloc((size_t)n, sizeof(float));
    float ph = 0.0f;                                   // kick: falling sine + a click
    for (int i = 0; i < n; i++) {
        float t = (float)i / sr;
        ph += 2.0f * 3.14159265f * (46.0f + 150.0f * expf(-t / 0.03f)) / sr;
        float click = noise1() * expf(-t / 0.002f) * 0.35f;
        buf[i] = tanhf(1.6f * (sinf(ph) * expf(-t / 0.10f) + click));
    }
    load_pcm(PCM_KICK, buf, n);

    float prev = 0.0f;                                 // snare: two tones + bright noise
    for (int i = 0; i < n; i++) {
        float t = (float)i / sr, nz = noise1();
        float body = (sinf(2 * 3.14159265f * 185 * t) + 0.5f * sinf(2 * 3.14159265f * 330 * t))
                     * expf(-t / 0.035f) * 0.55f;
        buf[i] = tanhf(1.4f * (body + (nz - 0.6f * prev) * expf(-t / 0.065f) * 0.75f));
        prev = nz;
    }
    load_pcm(PCM_SNARE, buf, n);

    int nh = (int)(0.08f * sr);                        // hat: high-passed noise
    prev = 0.0f;
    for (int i = 0; i < nh; i++) {
        float nz = noise1();
        buf[i] = (nz - prev) * expf(-(float)i / sr / 0.016f);
        prev = nz;
    }
    load_pcm(PCM_HAT, buf, nh);
    free(buf);
}

static void setup_chip(void) {
    // FM0: bass (fm4op/bass)
    instrument(S_BASS, INSTR_FM4, 1, 300, 4, 110);
    instrument_harmonics(S_BASS, 0.56f); instrument_timbre(S_BASS, 0.70f); instrument_morph(S_BASS, 0.25f);
    instrument_level(S_BASS, 0.9f);
    // FM1/FM2: soft tine electric piano (fm4op/tine), one per side, into the echo
    for (int s = S_ARPL; s <= S_ARPR; s++) {
        instrument(s, INSTR_FM4, 2, 420, 3, 600);
        instrument_harmonics(s, 0.06f); instrument_timbre(s, 0.40f); instrument_morph(s, 0.08f);
        instrument_level(s, 0.55f);
        instrument_echo(s, 0.40f);
        instrument_reverb(s, 0.25f);
    }
    instrument_pan(S_ARPL, -0.6f);
    instrument_pan(S_ARPR, 0.6f);
    // WT0-WT2: the pad, a detuned unison wall through a slowly breathing low-pass
    instrument(S_PAD, INSTR_SAW, 650, 0, 7, 1400);
    instrument_bandlimit(S_PAD, 1);
    instrument_unison(S_PAD, 4, 0.12f);
    instrument_filter(S_PAD, FILTER_LOW, 1100, 2);
    instrument_lfo(S_PAD, 0, LFO_CUTOFF, 0.08f, 450.0f);   // depth in Hz
    instrument_level(S_PAD, 0.32f);
    instrument_reverb(S_PAD, 0.55f);
    // PCM0-PCM2: drum samples
    make_drums();
    instrument(S_KICK, INSTR_SAMPLE, 0, 0, 7, 40);
    instrument_sample(S_KICK, PCM_KICK, 60);
    instrument(S_SNARE, INSTR_SAMPLE, 0, 0, 7, 60);
    instrument_sample(S_SNARE, PCM_SNARE, 60);
    instrument_reverb(S_SNARE, 0.30f);
    instrument_level(S_SNARE, 0.8f);
    instrument(S_HAT, INSTR_SAMPLE, 0, 0, 7, 30);
    instrument_sample(S_HAT, PCM_HAT, 60);
    instrument_pan(S_HAT, 0.3f);
    instrument_level(S_HAT, 0.35f);
    // DSP: dotted-eighth echo + plate reverb
    echo((int)(3 * STEP_SEC * 1000.0), 0.35f, 0.45f);
    reverb(0.6f, 0.5f);
    reverb_plate(0.7f);
}

// ---------------------------------------------------------------- the groove
#define BARS      33                      // + one silent bar before it loops
#define LOOP_BARS 34
static const char ROOTS[BARS + 1] = "AFCGAFGEAFCGAFGEAFCGAFGEAFCGAFGEA";
static int bass_root(char r) {            // the low root (C sits an octave up, as on every chip)
    switch (r) { case 'A': return 45; case 'F': return 41; case 'C': return 48; case 'G': return 43; default: return 40; }
}
static void chord_of(char r, int *root, int *third) {
    switch (r) {
        case 'A': *root = 57; *third = 3; break;
        case 'F': *root = 53; *third = 4; break;
        case 'C': *root = 60; *third = 4; break;
        case 'G': *root = 55; *third = 4; break;
        default:  *root = 52; *third = 4; break;
    }
}
static const int PAD_VOICING[5][3] = {{57, 60, 64}, {53, 57, 60}, {55, 60, 64}, {55, 59, 62}, {56, 59, 64}};
static int pad_index(char r) { return r == 'A' ? 0 : r == 'F' ? 1 : r == 'C' ? 2 : r == 'G' ? 3 : 4; }
static const int ARP_SHAPE[8] = {0, 1, 2, 3, 2, 1, 0, 2};

enum { G_INTRO, G_VERSE, G_DRIVE, G_FILL, G_END };
static const char *GROOVE_NAME[] = {"INTRO", "VERSE", "DRIVE", "FILL", "END"};
// 1 kick, 2 snare, 3 hat, per 16th
static const unsigned char GROOVES[5][16] = {
    {1, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0},
    {1, 0, 3, 0, 2, 0, 3, 0, 1, 0, 3, 0, 2, 0, 3, 0},
    {1, 0, 3, 1, 2, 0, 3, 0, 1, 0, 1, 0, 2, 0, 3, 3},
    {1, 0, 0, 0, 2, 0, 0, 0, 1, 0, 2, 0, 2, 2, 2, 2},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static int groove_of(int bar) {
    if (bar == 32) return G_END;
    if (bar % 8 == 7) return G_FILL;
    if (bar < 8) return G_INTRO;
    if (bar < 16) return G_VERSE;
    return G_DRIVE;
}

// ---------------------------------------------------------------- the monitor
#define NCH 16
static const char *CH_NAME[NCH] = {"FM0", "FM1", "FM2", "FM3", "WT0", "WT1", "WT2", "WT3",
                                   "PCM0", "PCM1", "PCM2", "PCM3", "PCM4", "PCM5", "PCM6", "PCM7"};
static const char *CH_ROLE[NCH] = {"BASS", "EP L", "EP R", "", "PAD", "PAD", "PAD", "",
                                   "KICK", "SNARE", "HAT", "", "", "", "", ""};
static float ch_lamp[NCH];
static int ch_note[NCH];
static double ch_until[NCH];              // a held note keeps its lamp lit until here

typedef struct { double t; int ch, midi; float dur; } Ev;
#define EVQ 128
static Ev evq[EVQ];
static int ev_head, ev_tail;
#define LOGN 7
static char log_lines[LOGN][48];
static int log_count;
static float kick_flash;
static int cur_bar = -1, cur_groove;

static const char *NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static void note_name(int midi, char *out) { sprintf(out, "%s%d", NOTE_NAMES[midi % 12], midi / 12 - 1); }

static void push_ev(double t, int ch, int midi, float dur) {
    int nx = (ev_tail + 1) % EVQ;
    if (nx == ev_head) return;
    evq[ev_tail] = (Ev){t, ch, midi, dur};
    ev_tail = nx;
}

static void book(double t, int slot, int ch, int midi, int vol, int dur_ms) {
    schedule_note_at(t, midi, slot, vol, dur_ms, NULL);
    push_ev(t, ch, midi, dur_ms / 1000.0f);
}

// ---------------------------------------------------------------- sequencer
static double t0;
static long step;
static bool booted;

static void book_step(long s, double t) {
    int bar = (int)((s / 16) % LOOP_BARS), i = (int)(s % 16);
    if (bar >= BARS) { if (i == 0) push_ev(t, -1, bar, 0); return; }
    char r = ROOTS[bar];
    bool last = bar == BARS - 1;
    if (i == 0) push_ev(t, -1, bar, 0);                   // bar marker for the display

    int lo = bass_root(r);                             // FM0: octave-bouncing bass
    if (last) { if (i == 0) book(t, S_BASS, 0, lo, 6, (int)(14 * STEP_SEC * 1000)); }
    else if (i % 2 == 0) book(t, S_BASS, 0, lo + ((i / 2) % 2 ? 12 : 0), 6, 95);

    if (bar >= 8) {                                    // FM1/FM2: arpeggio, ping-pong
        int root, third;
        chord_of(r, &root, &third);
        int tones[4] = {root, root + third, root + 7, root + 12};
        if (last) { if (i == 0) book(t, S_ARPL, 1, root + 12, 5, (int)(14 * STEP_SEC * 1000)); }
        else if (i % 2 == 0) {
            int side = (i / 2) % 2;
            book(t, side ? S_ARPR : S_ARPL, 1 + side, tones[ARP_SHAPE[i / 2]], 5, 260);
        }
    }

    if (bar >= 16 && i == 0) {                         // WT0-WT2: the pad chord
        int dur = (int)((last ? 14 : 16) * STEP_SEC * 1000) - 40;
        for (int v = 0; v < 3; v++) book(t, S_PAD, 4 + v, PAD_VOICING[pad_index(r)][v], 5, dur);
    }

    int d = GROOVES[groove_of(bar)][i];                // PCM0-PCM2: drums
    if (d == 1) book(t, S_KICK, 8, 60, 7, 250);
    if (d == 2) book(t, S_SNARE, 9, 60, 6, 250);
    if (d == 3) book(t, S_HAT, 10, 60, 5, 80);
}

static void restart(void) {
    note_off_all();
    memset(ch_until, 0, sizeof ch_until);
    t0 = audio_time() + 0.3;
    step = 0;
    ev_head = ev_tail = 0;
    cur_bar = -1;
}

void update(void) {
    if (!booted) { setup_chip(); restart(); booted = true; }
    if (btnp(0, BTN_A) || keyp(KEY_SPACE) || mouse_pressed(MOUSE_LEFT)) restart();
    double now_t = audio_time();
    while (t0 + step * STEP_SEC < now_t + LOOKAHEAD) { book_step(step, t0 + step * STEP_SEC); step++; }

    while (ev_head != ev_tail && evq[ev_head].t <= now_t) {   // what is sounding NOW
        Ev e = evq[ev_head];
        ev_head = (ev_head + 1) % EVQ;
        if (e.ch < 0) { cur_bar = e.midi; cur_groove = e.midi < BARS ? groove_of(e.midi) : G_END; continue; }
        ch_lamp[e.ch] = 1.0f;
        ch_until[e.ch] = e.t + e.dur;
        ch_note[e.ch] = e.midi;
        if (e.ch == 8) kick_flash = 1.0f;
        int reg = (e.ch < 4 ? 0x00 : e.ch < 8 ? 0x40 : 0x80) + (e.ch % (e.ch < 8 ? 4 : 8)) * 8 + 4;
        char nn[8];
        note_name(e.midi, nn);
        memmove(log_lines[1], log_lines[0], sizeof(log_lines[0]) * (LOGN - 1));
        snprintf(log_lines[0], sizeof(log_lines[0]), "%7.2f  $%02X <- $%02X   %-4s KEY-ON %-4s %s",
                 e.t - t0, reg, e.midi, CH_NAME[e.ch], nn, CH_ROLE[e.ch]);
        if (log_count < LOGN) log_count++;
    }
    for (int c = 0; c < NCH; c++) {
        ch_lamp[c] *= 0.90f;
        if (now_t < ch_until[c] && ch_lamp[c] < 0.45f) ch_lamp[c] = 0.45f;   // still held
    }
    kick_flash *= 0.85f;
}

// ---------------------------------------------------------------- draw
static int lamp_colour(int c, float v) {
    int hot = c < 4 ? CLR_ORANGE : c < 8 ? CLR_BLUE : CLR_GREEN;
    int warm = c < 4 ? CLR_DARK_ORANGE : c < 8 ? CLR_TRUE_BLUE : CLR_MEDIUM_GREEN;
    int cool = c < 4 ? CLR_DARK_BROWN : c < 8 ? CLR_DARKER_BLUE : CLR_DARK_GREEN;
    return v > 0.6f ? hot : v > 0.25f ? warm : v > 0.05f ? cool : CLR_BROWNISH_BLACK;
}

void draw(void) {
    camera(0, 0);
    cls(kick_flash > 0.5f ? CLR_DARKER_PURPLE : CLR_BLACK);

    font(FONT_NORMAL);
    print("DREAMCHIP DC-16", 6, 5, CLR_YELLOW);
    font(FONT_SMALL);
    print("4 FM  4 WAVETABLE  8 PCM  DSP: ECHO + PLATE", 6, 16, CLR_INDIGO);
    char buf[64];
    if (cur_bar >= 0 && cur_bar < BARS) snprintf(buf, sizeof buf, "BAR %02d  %s", cur_bar + 1, GROOVE_NAME[cur_groove]);
    else snprintf(buf, sizeof buf, "BAR --  %s", cur_bar < 0 ? "READY" : "BREATH");
    print_right(buf, 314, 7, CLR_LIGHT_GREY);

    for (int c = 0; c < NCH; c++) {                    // the 16 channels
        int x = 6 + (c % 8) * 39, y = 28 + (c / 8) * 30;
        rectfill(x, y, 36, 26, lamp_colour(c, ch_lamp[c]));
        rect(x, y, 36, 26, CLR_DARKER_GREY);
        print(CH_NAME[c], x + 3, y + 3, CLR_WHITE);
        if (CH_ROLE[c][0]) {
            print(CH_ROLE[c], x + 3, y + 10, CLR_LIGHT_PEACH);
            if (ch_lamp[c] > 0.05f && c < 8) { char nn[8]; note_name(ch_note[c], nn); print(nn, x + 3, y + 17, CLR_WHITE); }
        } else {
            print("idle", x + 3, y + 10, CLR_DARKER_GREY);
        }
    }

    font(FONT_TINY);                                   // register-write log
    rectfill(6, 90, 308, 50, CLR_DARKER_BLUE);
    print("REGISTER WRITES", 9, 92, CLR_INDIGO);
    for (int i = 0; i < log_count; i++)
        print(log_lines[i], 9, 99 + i * 6, i == 0 ? CLR_WHITE : CLR_LIGHT_GREY);

    float sc[308];                                     // oscilloscope of the real mix
    scope_read(sc, 308);
    rectfill(6, 144, 308, 50, CLR_BROWNISH_BLACK);
    line(6, 169, 313, 169, CLR_DARKER_GREY);
    for (int x = 1; x < 308; x++) {
        int y0 = 169 - (int)(sc[x - 1] * 50.0f), y1 = 169 - (int)(sc[x] * 50.0f);
        y0 = y0 < 146 ? 146 : y0 > 192 ? 192 : y0; y1 = y1 < 146 ? 146 : y1 > 192 ? 192 : y1;
        line(5 + x, y0, 6 + x, y1, kick_flash > 0.3f ? CLR_PINK : CLR_BLUE);
    }
    print("DSP OUT", 9, 146, CLR_INDIGO);
    font(FONT_NORMAL);
}
