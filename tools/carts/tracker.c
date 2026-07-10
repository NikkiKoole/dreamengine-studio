/* de:meta
{
  "slug": "tracker",
  "title": "tracker",
  "status": "active",
  "created": "2026-07-02",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "song-arrangement"
  ],
  "homage": "LSDJ / Dirtywave M8",
  "lineage": "Chassis after drummachine's beat()-clock sixteenth counter + keybed.h QWERTY note entry; the Song→Chain→Phrase nested-pattern model after LSDJ and the Dirtywave M8 (mnemonic FX, single-track phrase view, chain transpose). First cart on the shelf with song ARRANGEMENT — see docs/design/tracker-cart.md.",
  "description": {
    "summary": "A real music tracker: phrases of 16 steps chain into songs, four tracks, mnemonic FX — the LSDJ/M8 model on a 40-column screen.",
    "detail": "Four independent tracks each walk a column of CHAINS; a chain strings PHRASES together with per-entry transpose (one bassline serves every chord); a phrase is 16 steps of note + instrument + one FX command (ARP POR VIB VOL CUT PAN RET DEL HOP TPO). Eight hand-tuned presets — lead, bass, bell, pluck, pad, kick, snare, hat — so drums and melody interleave on any track. A demo song is loaded on first boot; your edits autosave. Press R to ROLL A RANDOM SONG (B hops genre: CHIP, HOUSE, HIPHOP, DNB) — it rewrites the same phrase/chain slots every time, so you can flip through the views and read how each genre builds its groove.",
    "controls": "TAB or [ ] switch view · arrows move · type notes on A–L / W–P (Z/X octave) · ENTER drill in / place · -/= nudge, ,/. big nudge · Q note-off · BACKSPACE clear · C/V copy paste · SPACE play/stop · 1/2 tempo · R random song · B genre"
  }
}
de:meta */
#include "studio.h"
#include "keybed.h"   // QWERTY (GarageBand map) + MIDI note entry, preview voices

// tracker — the Song→Chain→Phrase tracker (design/tracker-cart.md).
//
// The data model is three nested layers, smallest first:
//   PHRASE  16 steps, each = note + instrument + one FX command (+ value)
//   CHAIN   up to 16 phrase ids, each with a semitone TRANSPOSE
//   SONG    per track (4), an ordered column of chain ids
// Tracks advance INDEPENDENTLY (chains can differ in length — polymeter for
// free); each loops its own column. The clock is drummachine's idiom: a
// sixteenth counter derived from the synth's own beat()/beat_pos(), no timers.
//
// The phrase view shows ONE track's phrase (the M8 answer to a 40-char
// screen); the song view is where all four tracks sit side by side.

#define NTRK     4
#define PLEN     16     // steps per phrase
#define CLEN     16     // phrase slots per chain
#define SONG_LEN 16     // song rows
#define NPHRASE  64
#define NCHAIN   32
#define NONE     0xFF   // empty chain/phrase id
#define NOFF     255    // note column: the note-off marker
#define DOC_MAGIC 0x54524b31u   // "TRK1"

// ── instruments: 8 hand-tuned presets on engine slots 5..12 ──
enum { I_LEAD, I_BASS, I_BELL, I_PLUK, I_PAD, I_KICK, I_SNAR, I_HAT, NINST };
#define SLOT(i) (5 + (i))
static const char *INAME[NINST] = { "LEAD", "BASS", "BELL", "PLUK", "PAD",  "KICK", "SNAR", "HAT" };
static const char *IDESC[NINST] = {
    "hollow pulse, low-pass",   "round square sub",       "2-op FM bell",
    "karplus-strong string",    "slow saw wash, lfo",     "sine boom, pitch env",
    "band-passed noise bark",   "high-passed noise tick",
};
static const int ICLR[NINST] = { CLR_LIGHT_YELLOW, CLR_BLUE, CLR_PINK, CLR_LIME_GREEN,
                                 CLR_INDIGO, CLR_RED, CLR_ORANGE, CLR_YELLOW };

// ── FX commands (3-letter mnemonics, the M8 lesson — never raw hex) ──
enum { TFX_NONE, TFX_ARP, TFX_POR, TFX_VIB, TFX_VOL, TFX_CUT, TFX_PAN, TFX_RET, TFX_DEL, TFX_HOP, TFX_TPO, NFX };
static const char *FXNAME[NFX] = { "---", "ARP", "POR", "VIB", "VOL", "CUT", "PAN", "RET", "DEL", "HOP", "TPO" };

// ── the document (exactly what save_bytes persists — ~5 KB) ──
typedef struct { unsigned char note, inst, fx, val; } Step;   // note 0=empty, NOFF=off
typedef struct { Step s[PLEN]; } Phrase;
typedef struct { unsigned char ph[CLEN]; signed char tr[CLEN]; } Chain;
typedef struct {
    unsigned int  magic;
    unsigned char tempo;
    Phrase        phr[NPHRASE];
    Chain         chn[NCHAIN];
    unsigned char song[NTRK][SONG_LEN];
} Doc;
static Doc doc;
static bool dirty = false;   // autosaved (debounced) in update

// ── playback state, per track ──
typedef struct {
    bool  on;                 // this track is sounding this run
    int   row, cpos, step;    // song row, position in chain, step in phrase
    int   loop_row;           // where this track's column loops back to
    int   handle;             // live voice handle, -1 = silent
    int   inst;               // instrument of the live voice (for RET)
    float base;               // live voice pitch (post-transpose midi)
    int   fx, val;            // FX latched from the current row
    int   ret_done;           // RET sub-hits already fired this row
    bool  pend;               // DEL: a trigger is waiting inside the row
    float pend_at;            // ...at this fraction of the row
    int   pend_midi, pend_inst;
    float vibph;              // VIB phase, degrees
    float sent;               // last pitch actually sent to the engine (spec-checkable)
    int   flash;              // frame() of last trigger (draw pulse)
} Trk;
static Trk trk[NTRK];

static bool  playing = false;
static int   tempo   = 122;
static float anchor  = 0;     // sixteenth counter at play-start
static int   last16  = -9999; // last sixteenth processed

// ── edit state ──
enum { V_SONG, V_CHAIN, V_PHRASE, V_INST, NVIEW };
static const char *VNAME[NVIEW] = { "SONG", "CHAIN", "PHRASE", "INST" };
static int view = V_SONG;
static int scur_r = 0, scur_t = 0;          // song cursor: row, track
static int ccur_r = 0, ccur_c = 0;          // chain cursor: row, col (0 phrase / 1 transpose)
static int pcur_r = 0, pcur_c = 0;          // phrase cursor: row, col (0 note 1 inst 2 fx 3 val)
static int cur_track = 0, cur_chain = 0, cur_phrase = 0, cur_inst = 0;
static int last_note = 69, last_chain = 0, last_phrase = 0;
static int typed_note = -1;                 // set by the keybed callback, consumed in update
static Step clipstep = { 0, 0, 0, 0 };          // C/V copy buffer

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

// ── document helpers ──
static Chain *tchain(int t) {
    unsigned char c = doc.song[t][trk[t].row];
    return c == NONE ? 0 : &doc.chn[c];
}
static Phrase *tphrase(int t) {
    Chain *c = tchain(t);
    if (!c || c->ph[trk[t].cpos] == NONE) return 0;
    return &doc.phr[c->ph[trk[t].cpos]];
}

static const char *note_name(int n) {
    static const char *NM[12] = { "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-" };
    if (n == 0)    return "...";
    if (n == NOFF) return "OFF";
    return str("%s%d", NM[n % 12], n / 12 - 1);
}

// ── voices ──
static void trigger(Trk *t, int midi, int inst, float vol) {
    if (t->handle >= 0) note_off(t->handle);
    t->handle = note_on(midi, SLOT(inst % NINST), (int)vol);
    note_glide(t->handle, 0);                 // glide is sticky per handle — reset it
    t->base = midi;
    t->sent = midi;
    t->inst = inst % NINST;
}
static void all_off(void) {
    for (int t = 0; t < NTRK; t++)
        if (trk[t].handle >= 0) { note_off(trk[t].handle); trk[t].handle = -1; }
}

// ── transport ──
static void play_start(int from_row) {
    for (int t = 0; t < NTRK; t++) {
        trk[t] = (Trk){ 0 };
        trk[t].handle   = -1;
        trk[t].row      = from_row;
        trk[t].loop_row = from_row;
        trk[t].on       = (doc.song[t][from_row] != NONE);
    }
    anchor  = beat() * 4 + beat_pos() * 4.0f;
    last16  = -9999;
    playing = true;
}
static void play_stop(void) { playing = false; all_off(); }

// play the current step of one track (called once per sixteenth)
static void row_play(int t) {
    Trk *k = &trk[t];
    if (!k->on) return;
    Phrase *p = tphrase(t);
    if (!p) return;
    Chain *c  = tchain(t);
    Step  *s  = &p->s[k->step];
    int    tr = c ? c->tr[k->cpos] : 0;

    // an ARP/VIB row must not LEAK: ride() left the live pitch wherever the
    // arp/wobble last put it (up to +7 st!) — snap a surviving voice back to
    // its true pitch before this row takes over
    if (k->handle >= 0 && (k->fx == TFX_ARP || k->fx == TFX_VIB) &&
        s->fx != TFX_ARP && s->fx != TFX_VIB) {
        note_pitch(k->handle, k->base);
        k->sent = k->base;
    }
    k->fx = s->fx; k->val = s->val;
    k->ret_done = 0; k->pend = false; k->vibph = 0;

    if (s->fx == TFX_TPO && s->val) tempo = 40 + s->val * 2;   // 42..238 bpm

    if (s->note == NOFF) {
        if (k->handle >= 0) { note_off(k->handle); k->handle = -1; }
    } else if (s->note) {
        int midi = clampi(s->note + tr, 1, 126);
        if (s->fx == TFX_POR && k->handle >= 0) {
            // tone portamento: no retrigger — glide the live voice to the new note
            note_glide(k->handle, 10 + s->val * 5);
            note_pitch(k->handle, midi);
            k->base = midi;
            k->sent = midi;
        } else if (s->fx == TFX_DEL && s->val) {
            k->pend = true; k->pend_at = s->val / 100.0f;
            k->pend_midi = midi; k->pend_inst = s->inst;
        } else {
            trigger(k, midi, s->inst, 6);
        }
        k->flash = frame();
    }
    if (k->handle >= 0) {   // instant per-row voice FX
        if (s->fx == TFX_VOL) note_vol(k->handle, s->val * 7.0f / 99.0f);
        if (s->fx == TFX_CUT) note_cutoff(k->handle, 100 + s->val * 40);
        if (s->fx == TFX_PAN) note_pan(k->handle, (s->val - 50) / 50.0f);
    }
}

// advance one track's position pointers by one sixteenth
static void advance(int t) {
    Trk *k = &trk[t];
    if (!k->on) return;
    bool hop = (k->fx == TFX_HOP);   // HOP = end the phrase now (odd meters)
    k->step++;
    if (k->step >= PLEN || hop) {
        k->step = 0; k->fx = TFX_NONE;
        k->cpos++;
        Chain *c = tchain(t);
        if (!c || k->cpos >= CLEN || c->ph[k->cpos] == NONE) {
            k->cpos = 0;
            k->row++;
            if (k->row >= SONG_LEN || doc.song[t][k->row] == NONE) k->row = k->loop_row;
            if (doc.song[t][k->row] == NONE) {   // column truly empty — go quiet
                k->on = false;
                if (k->handle >= 0) { note_off(k->handle); k->handle = -1; }
            }
        }
    }
}

// one sequencer tick: every track advances one sixteenth and plays its new row.
// The ONLY way positions move — update() calls it off the beat clock, spec() by hand.
static void seq_tick(void) {
    for (int t = 0; t < NTRK; t++) { advance(t); row_play(t); }
}

// per-frame FX riding (VIB/ARP wobble the live pitch; RET/DEL fire inside the row)
static void ride(int t, float frac) {
    Trk *k = &trk[t];
    if (!k->on) return;
    if (k->pend && frac >= k->pend_at) {
        k->pend = false;
        trigger(k, k->pend_midi, k->pend_inst, 6);
        k->flash = frame();
    }
    if (k->handle < 0) return;
    if (k->fx == TFX_VIB && k->val) {
        int rate = k->val / 10, dep = k->val % 10;
        k->vibph += dt() * (1.5f + rate * 0.8f) * 360.0f;
        k->sent = k->base + sin_deg(k->vibph) * dep * 0.08f;
        note_pitch(k->handle, k->sent);
    } else if (k->fx == TFX_ARP && k->val) {
        int idx = (frame() / 2) % 3;   // 0 → +tens → +ones, 30Hz cycle
        int add = idx == 0 ? 0 : idx == 1 ? k->val / 10 : k->val % 10;
        k->sent = k->base + add;
        note_pitch(k->handle, k->sent);
    } else if (k->fx == TFX_RET && k->val % 10) {
        int n   = k->val % 10;
        int due = (int)(frac * (n + 1));
        if (due > k->ret_done) {
            k->ret_done = due;
            trigger(k, (int)k->base, k->inst, 5);
            k->flash = frame();
        }
    }
}

// ── the demo song (Am–F–C–G; boots making music, never to silence) ──
static void put(int p, int s, int note, int in, int fx, int val) {
    doc.phr[p].s[s] = (Step){ (unsigned char)note, (unsigned char)in,
                              (unsigned char)fx,   (unsigned char)val };
}
static void load_demo(void) {
    doc = (Doc){ 0 };
    doc.magic = DOC_MAGIC;
    doc.tempo = 122;
    for (int c = 0; c < NCHAIN; c++)
        for (int i = 0; i < CLEN; i++) doc.chn[c].ph[i] = NONE;
    for (int t = 0; t < NTRK; t++)
        for (int r = 0; r < SONG_LEN; r++) doc.song[t][r] = NONE;

    // P0/P1 — drums: kick/snare/hat interleave on ONE track (the tracker trick)
    put(0,  0, 36, I_KICK, 0, 0);  put(0,  2, 92, I_HAT, 0, 0);
    put(0,  4, 58, I_SNAR, 0, 0);  put(0,  6, 92, I_HAT, 0, 0);
    put(0,  8, 36, I_KICK, 0, 0);  put(0, 10, 92, I_HAT, 0, 0);
    put(0, 11, 36, I_KICK, TFX_VOL, 40);
    put(0, 12, 58, I_SNAR, 0, 0);  put(0, 14, 92, I_HAT, 0, 0);
    put(0, 15, 92, I_HAT,  TFX_VOL, 30);
    put(1,  0, 36, I_KICK, 0, 0);  put(1,  2, 92, I_HAT, 0, 0);
    put(1,  4, 58, I_SNAR, 0, 0);  put(1,  6, 92, I_HAT, 0, 0);
    put(1,  8, 36, I_KICK, 0, 0);  put(1, 10, 58, I_SNAR, TFX_VOL, 50);
    put(1, 12, 58, I_SNAR, TFX_RET, 3);
    put(1, 14, 92, I_HAT,  0, 0);  put(1, 15, 36, I_KICK, 0, 0);

    // P2 — bass: root/octave bounce, a POR slide up at the turn
    put(2,  0, 45, I_BASS, 0, 0);       put(2,  2, 45, I_BASS, TFX_VOL, 45);
    put(2,  4, 57, I_BASS, 0, 0);       put(2,  6, 45, I_BASS, 0, 0);
    put(2,  8, 45, I_BASS, 0, 0);       put(2, 10, 45, I_BASS, TFX_VOL, 45);
    put(2, 12, 52, I_BASS, 0, 0);       put(2, 14, 57, I_BASS, TFX_POR, 20);

    // P3/P4 — lead: A-minor pentatonic, vibrato on the long notes
    put(3,  0, 69, I_LEAD, 0, 0);       put(3,  2, 72, I_LEAD, 0, 0);
    put(3,  4, 74, I_LEAD, TFX_VIB, 35);
    put(3,  8, 76, I_LEAD, 0, 0);       put(3, 10, 74, I_LEAD, 0, 0);
    put(3, 12, 72, I_LEAD, 0, 0);       put(3, 14, 69, I_LEAD, TFX_POR, 25);
    put(4,  0, 76, I_LEAD, 0, 0);       put(4,  2, 79, I_LEAD, TFX_POR, 30);
    put(4,  6, 74, I_LEAD, 0, 0);       put(4,  8, 72, I_LEAD, TFX_VIB, 45);
    put(4, 12, 69, I_LEAD, 0, 0);       put(4, 14, NOFF, 0, 0, 0);

    // P5 — pad: one long note a bar, swaying across the stereo field
    put(5,  0, 57, I_PAD, TFX_PAN, 30);  put(5,  8, 57, I_PAD, TFX_PAN, 70);

    // chains — ONE phrase each, re-pitched by transpose: Am F C G
    static const signed char PROG[4] = { 0, -4, 3, -2 };
    for (int i = 0; i < 4; i++) {
        doc.chn[0].ph[i] = (i == 3) ? 1 : 0;  doc.chn[0].tr[i] = 0;        // drums
        doc.chn[1].ph[i] = 2;                 doc.chn[1].tr[i] = PROG[i];  // bass
        doc.chn[2].ph[i] = (i % 2) ? 4 : 3;   doc.chn[2].tr[i] = PROG[i];  // lead A/B
        doc.chn[3].ph[i] = 5;                 doc.chn[3].tr[i] = PROG[i];  // pad
        doc.chn[4].ph[i] = (i % 2) ? 3 : 4;   doc.chn[4].tr[i] = PROG[i];  // lead B/A
    }
    doc.song[0][0] = 0; doc.song[1][0] = 1; doc.song[2][0] = 2; doc.song[3][0] = 3;
    doc.song[0][1] = 0; doc.song[1][1] = 1; doc.song[2][1] = 4; doc.song[3][1] = 3;
}

// ── the random-song generator: LEARNING BY SEEING ──
// Rerolls the SAME slots the demo song uses (phrases 0/1 drums, 2 bass, 3/4 lead,
// 5 pad; chains 0-4; song rows 0/1), so the STRUCTURE stays recognizable across
// rolls and only the musical content changes — flip through the views after a
// roll and read how each genre builds its groove.
enum { G_CHIP, G_HOUSE, G_HIPHOP, G_DNB, NGENRE };
static const char *GNAME[NGENRE] = { "CHIP", "HOUSE", "HIPHOP", "DNB" };
static int genre = G_CHIP;

static const signed char PROGS[5][4] = {   // 4-chord minor progressions, as transposes
    { 0, -4, 3, -2 },    // i VI III VII   (Am F C G)
    { 0, -2, -4, -2 },   // i VII VI VII   (driving)
    { 0, 3, -2, 5 },     // i III VII iv
    { 0, -4, -2, 3 },    // i VI VII III
    { 0, 0, 5, -2 },     // i i iv VII     (slow burn)
};
static const int PENT[8] = { 0, 3, 5, 7, 10, 12, 15, 17 };   // minor pentatonic

// a lead phrase: a random walk on the pentatonic, FX sprinkled by genre flags
static void gen_lead(int p, int density, int inst, bool arps, bool lazy) {
    int idx = 2 + rnd(3);
    for (int s = 0; s < 16; s += 2) {
        if (rnd(100) >= density) continue;
        idx = clampi(idx + rnd(5) - 2, 0, 7);
        int fx = 0, val = 0;
        if      (rnd(4) == 0)         { fx = TFX_VIB; val = (2 + rnd(3)) * 10 + 3 + rnd(4); }
        else if (arps && rnd(5) == 0) { fx = TFX_ARP; val = 37; }          // minor arp
        else if (lazy && rnd(3) == 0) { fx = TFX_DEL; val = 5 + rnd(8); }  // behind the beat
        else if (rnd(6) == 0)         { fx = TFX_POR; val = 15 + rnd(25); }
        put(p, s, 69 + PENT[idx], inst, fx, val);
    }
    put(p, 14 + rnd(2), NOFF, 0, 0, 0);   // breathe at the phrase turn
}

static void gen_song(void) {
    for (int p = 0; p <= 5; p++) doc.phr[p] = (Phrase){ 0 };

    for (int p = 0; p <= 1; p++) {   // drums: phrase 0 = groove, 1 = same + a fill
        switch (genre) {
        case G_CHIP:   // four-square backbeat, hats on the 8ths
            put(p, 0, 36, I_KICK, 0, 0);  put(p, 8, 36, I_KICK, 0, 0);
            if (rnd(2)) put(p, 6 + rnd(2) * 4, 36, I_KICK, TFX_VOL, 55);
            put(p, 4, 58, I_SNAR, 0, 0);  put(p, 12, 58, I_SNAR, 0, 0);
            for (int s = 2; s < 16; s += 2)
                put(p, s, 92, I_HAT, (s % 4) ? TFX_VOL : 0, (s % 4) ? 40 : 0);
            break;
        case G_HOUSE:  // four on the floor, hats pushing the offbeats
            for (int s = 0; s < 16; s += 4) put(p, s, 36, I_KICK, 0, 0);
            for (int s = 2; s < 16; s += 4) put(p, s, 92, I_HAT, TFX_VOL, 70);
            put(p, 4, 58, I_SNAR, 0, 0);  put(p, 12, 58, I_SNAR, 0, 0);
            break;
        case G_HIPHOP: // sparse kick, hats DRAGGED late with DEL — the drunk pocket
            put(p, 0, 36, I_KICK, 0, 0);  put(p, 7 + rnd(2) * 3, 36, I_KICK, 0, 0);
            put(p, 4, 58, I_SNAR, 0, 0);  put(p, 12, 58, I_SNAR, 0, 0);
            for (int s = 2; s < 16; s += 2)
                put(p, s, 92, I_HAT, (s % 4) ? TFX_DEL : 0, (s % 4) ? 5 + rnd(6) : 0);
            break;
        case G_DNB:    // the two-step: kick 0+10, snare 4+12, scattered hats
            put(p, 0, 36, I_KICK, 0, 0);  put(p, 10, 36, I_KICK, 0, 0);
            put(p, 4, 58, I_SNAR, 0, 0);  put(p, 12, 58, I_SNAR, 0, 0);
            for (int s = 1; s < 16; s += 2)
                if (rnd(2)) put(p, s, 92, I_HAT, TFX_VOL, 35);
            break;
        }
    }
    switch (genre) {   // the fill lives at the end of phrase 1
        case G_CHIP:   put(1, 12, 58, I_SNAR, TFX_RET, 3); put(1, 14, 58, I_SNAR, TFX_VOL, 60); break;
        case G_HOUSE:  put(1, 14, 92, I_HAT,  TFX_RET, 2); break;
        case G_HIPHOP: put(1, 14, 36, I_KICK, TFX_VOL, 55); break;
        case G_DNB:    put(1, 12, 58, I_SNAR, TFX_RET, 5); put(1, 8, 36, I_KICK, 0, 0); break;
    }

    switch (genre) {   // bass, phrase 2
        case G_CHIP:   // driving 8ths, octave pops
            for (int s = 0; s < 16; s += 2)
                put(2, s, 45 + (rnd(4) == 0 ? 12 : 0), I_BASS, (s % 4) ? TFX_VOL : 0, (s % 4) ? 45 : 0);
            break;
        case G_HOUSE:  // the offbeat pump
            for (int s = 2; s < 16; s += 4) put(2, s, 45, I_BASS, 0, 0);
            put(2, 14, 57, I_BASS, 0, 0);
            break;
        case G_HIPHOP: // sparse, sliding
            put(2, 0, 45, I_BASS, 0, 0);
            put(2, 7, 45 + PENT[1 + rnd(2)], I_BASS, TFX_POR, 25 + rnd(20));
            put(2, 10 + rnd(3), 45, I_BASS, TFX_VOL, 55);
            break;
        case G_DNB:    // long subs gliding under the break
            put(2, 0, 33, I_BASS, 0, 0);
            put(2, 8, 33 + PENT[1 + rnd(3)], I_BASS, TFX_POR, 50 + rnd(30));
            break;
    }

    switch (genre) {   // lead, phrases 3 + 4 (the A/B pair the chain alternates)
        case G_CHIP:   gen_lead(3, 75, I_LEAD, true, false); gen_lead(4, 75, I_LEAD, true, false); break;
        case G_HOUSE:  { int i2 = rnd(2) ? I_BELL : I_PLUK;
                         gen_lead(3, 40, i2, false, false);  gen_lead(4, 40, i2, false, false); } break;
        case G_HIPHOP: { int i2 = rnd(2) ? I_PLUK : I_BELL;
                         gen_lead(3, 45, i2, false, true);   gen_lead(4, 45, i2, false, true); } break;
        case G_DNB:    gen_lead(3, 30, I_BELL, false, false); gen_lead(4, 30, I_BELL, false, false); break;
    }

    doc.phr[5] = (Phrase){ 0 };   // pad, phrase 5: one note a bar, swaying in the field
    put(5, 0, 57, I_PAD, TFX_PAN, 20 + rnd(20));
    put(5, 8, 57, I_PAD, TFX_PAN, 60 + rnd(20));

    // chains + song: the demo's exact layout, over a rolled progression
    const signed char *pr = PROGS[rnd(5)];
    for (int c = 0; c < 5; c++)
        for (int i = 0; i < CLEN; i++) doc.chn[c].ph[i] = NONE;
    for (int i = 0; i < 4; i++) {
        doc.chn[0].ph[i] = (i == 3) ? 1 : 0;  doc.chn[0].tr[i] = 0;
        doc.chn[1].ph[i] = 2;                 doc.chn[1].tr[i] = pr[i];
        doc.chn[2].ph[i] = (i % 2) ? 4 : 3;   doc.chn[2].tr[i] = pr[i];
        doc.chn[3].ph[i] = 5;                 doc.chn[3].tr[i] = pr[i];
        doc.chn[4].ph[i] = (i % 2) ? 3 : 4;   doc.chn[4].tr[i] = pr[i];
    }
    for (int t = 0; t < NTRK; t++)
        for (int r = 0; r < SONG_LEN; r++) doc.song[t][r] = NONE;
    doc.song[0][0] = 0; doc.song[1][0] = 1; doc.song[2][0] = 2; doc.song[3][0] = 3;
    doc.song[0][1] = 0; doc.song[1][1] = 1; doc.song[2][1] = 4; doc.song[3][1] = 3;

    static const int TLO[NGENRE] = { 128, 120, 84, 168 }, TRNG[NGENRE] = { 22, 8, 12, 8 };
    tempo = TLO[genre] + rnd(TRNG[genre]);
    doc.tempo = (unsigned char)tempo;
}

// ── keybed: a typed note previews AND (in the phrase view) writes the cell ──
static void on_typed(int midi, int vel) { (void)vel; typed_note = midi; }

void init() {
    // the band: lead/bass/bell/pluck/pad + the drummachine kit recipes
    // (kick/snare/hat reused on purpose — instrument-recipes.md "drummachine")
    instrument(SLOT(I_LEAD), INSTR_SQUARE, 2, 120, 5, 90);
    instrument_duty(SLOT(I_LEAD), 0.25f);
    instrument_filter(SLOT(I_LEAD), FILTER_LOW, 2600, 2);
    instrument(SLOT(I_BASS), INSTR_SQUARE, 1, 100, 4, 60);
    instrument_filter(SLOT(I_BASS), FILTER_LOW, 700, 5);
    instrument(SLOT(I_BELL), INSTR_FM, 1, 400, 0, 300);
    instrument(SLOT(I_PLUK), INSTR_PLUCK, 1, 200, 0, 150);
    instrument(SLOT(I_PAD),  INSTR_SAW, 180, 300, 5, 400);
    instrument_filter(SLOT(I_PAD), FILTER_LOW, 1200, 2);
    instrument_lfo(SLOT(I_PAD), 0, LFO_CUTOFF, 0.7f, 500);
    instrument(SLOT(I_KICK), INSTR_SINE, 0, 280, 0, 60);
    instrument_env(SLOT(I_KICK), 1, ENV_PITCH, 0, 55, 30);
    instrument(SLOT(I_SNAR), INSTR_NOISE, 0, 130, 0, 50);
    instrument_filter(SLOT(I_SNAR), FILTER_BAND, 1400, 3);
    instrument(SLOT(I_HAT),  INSTR_NOISE, 0, 25, 0, 15);
    instrument_filter(SLOT(I_HAT), FILTER_HIGH, 6500, 2);

    if (load_bytes(&doc, sizeof doc) != sizeof doc || doc.magic != DOC_MAGIC)
        load_demo();
    tempo = doc.tempo;

    keybed_config(SLOT(I_LEAD), 4, 14);   // QWERTY+MIDI entry; no drawn manual
    keybed_layout(0, 0, 0, 0);
    keybed_on_note(on_typed);

    for (int t = 0; t < NTRK; t++) trk[t].handle = -1;
}

// ── cell editing ──
// nudge the value under the cursor by d (small) — the LSDJ edit grammar
static void nudge(int d) {
    dirty = true;
    if (view == V_SONG) {
        unsigned char *c = &doc.song[scur_t][scur_r];
        *c = (*c == NONE) ? (unsigned char)last_chain
                          : (unsigned char)clampi(*c + d, 0, NCHAIN - 1);
        last_chain = *c;
    } else if (view == V_CHAIN) {
        Chain *c = &doc.chn[cur_chain];
        if (ccur_c == 0) {
            unsigned char *p = &c->ph[ccur_r];
            *p = (*p == NONE) ? (unsigned char)last_phrase
                              : (unsigned char)clampi(*p + d, 0, NPHRASE - 1);
            last_phrase = *p;
        } else c->tr[ccur_r] = (signed char)clampi(c->tr[ccur_r] + d, -24, 24);
    } else if (view == V_PHRASE) {
        Step *s = &doc.phr[cur_phrase].s[pcur_r];
        switch (pcur_c) {
            case 0:
                if (s->note == 0 || s->note == NOFF) { s->note = (unsigned char)last_note; s->inst = (unsigned char)cur_inst; }
                else s->note = (unsigned char)clampi(s->note + d, 1, 126);
                last_note = s->note;
                break;
            case 1: s->inst = (unsigned char)((s->inst + d + NINST * 8) % NINST); break;
            case 2: s->fx   = (unsigned char)((s->fx + d + NFX * 8) % NFX);       break;
            case 3: s->val  = (unsigned char)clampi(s->val + d, 0, 99);           break;
        }
    } else {
        cur_inst = (cur_inst + d + NINST * 8) % NINST;
        dirty = false;
    }
}

static void cursor_move(int dx_, int dy_) {
    if (view == V_SONG)   { scur_t = clampi(scur_t + dx_, 0, NTRK - 1); scur_r = clampi(scur_r + dy_, 0, SONG_LEN - 1); }
    if (view == V_CHAIN)  { ccur_c = clampi(ccur_c + dx_, 0, 1);        ccur_r = clampi(ccur_r + dy_, 0, CLEN - 1); }
    if (view == V_PHRASE) { pcur_c = clampi(pcur_c + dx_, 0, 3);        pcur_r = clampi(pcur_r + dy_, 0, PLEN - 1); }
    if (view == V_INST)   { cur_inst = clampi(cur_inst + dy_, 0, NINST - 1); }
}

void update() {
    bpm(tempo);
    keybed_velocity(6);
    kb_slot = SLOT(cur_inst);   // keybed previews whatever instrument is selected
    keybed_update();

    // ── typed note: preview happened in keybed; in the phrase view it also writes ──
    if (typed_note >= 0) {
        if (view == V_PHRASE) {
            Step *s = &doc.phr[cur_phrase].s[pcur_r];
            s->note = (unsigned char)clampi(typed_note, 1, 126);
            s->inst = (unsigned char)cur_inst;
            last_note = s->note;
            pcur_r = (pcur_r + 1) % PLEN;   // tracker convention: entry advances
            dirty = true;
        }
        typed_note = -1;
    }

    // ── keys ──
    if (keyp(KEY_TAB) || keyp(']')) view = (view + 1) % NVIEW;
    if (keyp('['))                  view = (view + NVIEW - 1) % NVIEW;
    if (keyp(KEY_UP))    cursor_move(0, -1);
    if (keyp(KEY_DOWN))  cursor_move(0, 1);
    if (keyp(KEY_LEFT))  cursor_move(-1, 0);
    if (keyp(KEY_RIGHT)) cursor_move(1, 0);
    if (keyp('-')) nudge(-1);
    if (keyp('=')) nudge(1);
    if (keyp(',')) nudge(view == V_PHRASE && pcur_c == 0 ? -12 : view == V_CHAIN && ccur_c == 1 ? -12 : -10);
    if (keyp('.')) nudge(view == V_PHRASE && pcur_c == 0 ?  12 : view == V_CHAIN && ccur_c == 1 ?  12 :  10);
    if (keyp('1')) { tempo = clampi(tempo - 2, 40, 260); doc.tempo = (unsigned char)tempo; dirty = true; }
    if (keyp('2')) { tempo = clampi(tempo + 2, 40, 260); doc.tempo = (unsigned char)tempo; dirty = true; }

    if (keyp(KEY_ENTER)) {
        if (view == V_SONG) {
            unsigned char c = doc.song[scur_t][scur_r];
            if (c == NONE) nudge(1);                       // empty: place last chain
            else { cur_chain = c; cur_track = scur_t; view = V_CHAIN; }
        } else if (view == V_CHAIN) {
            unsigned char p = doc.chn[cur_chain].ph[ccur_r];
            if (ccur_c != 0) { /* transpose col: nothing to drill into */ }
            else if (p == NONE) nudge(1);                  // empty: place last phrase
            else { cur_phrase = p; view = V_PHRASE; }
        } else if (view == V_PHRASE) {                     // place last note + audition
            Step *s = &doc.phr[cur_phrase].s[pcur_r];
            s->note = (unsigned char)last_note; s->inst = (unsigned char)cur_inst;
            hit(last_note, SLOT(cur_inst), 6, 180);
            pcur_r = (pcur_r + 1) % PLEN;
            dirty = true;
        }
    }
    if (keyp(KEY_BACKSPACE)) {
        dirty = true;
        if (view == V_SONG)  doc.song[scur_t][scur_r] = NONE;
        if (view == V_CHAIN) { if (ccur_c == 0) doc.chn[cur_chain].ph[ccur_r] = NONE; else doc.chn[cur_chain].tr[ccur_r] = 0; }
        if (view == V_PHRASE) {
            Step *s = &doc.phr[cur_phrase].s[pcur_r];
            if (pcur_c == 0) *s = (Step){ 0, 0, 0, 0 };
            else if (pcur_c == 1) s->inst = 0;
            else { s->fx = TFX_NONE; s->val = 0; }
        }
    }
    if (keyp('Q') && view == V_PHRASE) {   // note-off marker
        doc.phr[cur_phrase].s[pcur_r] = (Step){ NOFF, 0, 0, 0 };
        pcur_r = (pcur_r + 1) % PLEN;
        dirty = true;
    }
    if (view == V_PHRASE) {
        if (keyp('C')) clipstep = doc.phr[cur_phrase].s[pcur_r];
        if (keyp('V')) { doc.phr[cur_phrase].s[pcur_r] = clipstep; pcur_r = (pcur_r + 1) % PLEN; dirty = true; }
    }

    if (keyp(KEY_SPACE)) {
        if (playing) play_stop();
        else play_start(view == V_SONG ? scur_r : 0);
    }

    // ── the learning lever: R rolls a random song (B first hops genre), then plays ──
    bool roll  = keyp('R') || tapp(92, 2, 32, 12);
    bool gswap = keyp('B') || tapp(128, 2, 56, 12);
    if (gswap) genre = (genre + 1) % NGENRE;
    if (roll || gswap) {
        gen_song();
        dirty = true;
        if (!playing) play_start(0);
    }

    // ── transport: the drummachine sixteenth counter, anchored at play-start ──
    if (playing) {
        float pos16 = beat() * 4 + beat_pos() * 4.0f - anchor;
        int   n     = (int)pos16;
        float frac  = pos16 - n;
        if (last16 < -100) {   // first tick after play: fire row 0 without advancing
            last16 = n;
            for (int t = 0; t < NTRK; t++) row_play(t);
        }
        while (last16 < n) {   // never skip a row on a dropped frame
            last16++;
            seq_tick();
        }
        for (int t = 0; t < NTRK; t++) ride(t, frac);
    }

    // ── autosave (debounced — save_bytes writes a file, don't hammer it) ──
    if (dirty && frame() % 30 == 0) {
        doc.tempo = (unsigned char)tempo;
        save_bytes(&doc, sizeof doc);
        dirty = false;
    }

#ifdef DE_TRACE
    watch("view",    "%d", view);
    watch("playing", "%d", playing);
    watch("tempo",   "%d", tempo);
    watch("t0",      "%d/%d/%d", trk[0].row, trk[0].cpos, trk[0].step);
#endif
}

// ── drawing ──
#define LIST_Y 28
#define ROW_H  10

static void draw_row_bg(int y, bool play_here, bool every4) {
    int c = play_here ? CLR_DARK_GREEN : every4 ? CLR_DARKER_BLUE : -1;
    if (c >= 0) rectfill(0, y - 1, 200, ROW_H, c);
}
static void draw_cursor(int x, int y, int w) {
    rect(x - 2, y - 2, w + 3, ROW_H, playing ? CLR_MEDIUM_GREY : CLR_GREEN);
}

static void draw_help(void) {
    static const char *CAP[NVIEW][3] = {
        { "a cell is a CHAIN",  "id; tracks walk",   "their own column" },
        { "a chain strings",    "PHRASES; TR",       "re-pitches them" },
        { "typed notes write",  "cell + sel. inst",  0 },
        { "sel. inst is",       "written with",      "each typed note" },
    };
    static const char *H[] = {
        "ARROWS cursor", "TAB [ ] view", "ENTER  drill/place",
        "- = , . edit cell", "BKSP   clear", "Q      note off",
        "C V    copy paste", "SPACE  play/stop", "1 2    tempo",
        "R      random song", "B      genre",
        "A..L   play notes", "Z X    octave", 0,
    };
    int y = LIST_Y + 2;
    print(str("%d %s", cur_inst, INAME[cur_inst]), 226, y, ICLR[cur_inst]);
    y += 14;
    font(FONT_SMALL);
    for (int i = 0; i < 3 && CAP[view][i]; i++) { print(CAP[view][i], 226, y, CLR_MEDIUM_GREY); y += 8; }
    y += 6;
    for (int i = 0; H[i]; i++) print(H[i], 226, y + i * 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

void draw() {
    cls(CLR_BROWNISH_BLACK);

    // header: title + transport + the view tabs
    print("TRACKER", 4, 4, CLR_LIGHT_YELLOW);
    print(playing ? ">" : "#", 72, 4, playing ? CLR_GREEN : CLR_DARK_GREY);
    rectfill(92, 2, 32, 11, CLR_DARK_GREY);  rect(92, 2, 32, 11, CLR_MEDIUM_GREY);
    print("RND", 98, 4, CLR_LIGHT_GREY);
    rectfill(128, 2, 56, 11, CLR_DARK_GREY); rect(128, 2, 56, 11, CLR_MEDIUM_GREY);
    print(GNAME[genre], 134, 4, CLR_LIGHT_YELLOW);
    print_right(str("%d BPM", tempo), 316, 4, CLR_WHITE);
    for (int v = 0, x = 4; v < NVIEW; v++) {
        int w = text_width(VNAME[v]) + 8;
        if (v == view) rectfill(x - 2, 14, w, 11, CLR_DARK_BLUE);
        print(VNAME[v], x + 2, 16, v == view ? CLR_WHITE : CLR_MEDIUM_GREY);
        x += w + 4;
    }

    if (view == V_SONG) {
        for (int t = 0; t < NTRK; t++)
            print(str("T%d", t + 1), 60 + t * 32, LIST_Y, t == cur_track ? CLR_WHITE : CLR_MEDIUM_GREY);
        for (int r = 0; r < SONG_LEN; r++) {
            int y = LIST_Y + 12 + r * ROW_H;
            if (r % 4 == 0) rectfill(0, y - 1, 190, ROW_H, CLR_DARKER_BLUE);
            print(str("%02d", r), 30, y, CLR_DARK_GREY);
            for (int t = 0; t < NTRK; t++) {
                unsigned char c = doc.song[t][r];
                bool here = playing && trk[t].on && trk[t].row == r;
                if (here) rectfill(58 + t * 32, y - 1, 20, ROW_H, CLR_DARK_GREEN);
                print(c == NONE ? "--" : str("%02d", c), 60 + t * 32, y,
                      c == NONE ? CLR_DARKER_GREY : here ? CLR_WHITE : CLR_LIGHT_GREY);
            }
        }
        draw_cursor(60 + scur_t * 32, LIST_Y + 12 + scur_r * ROW_H, 18);
    }

    else if (view == V_CHAIN) {
        Chain *c = &doc.chn[cur_chain];
        print(str("CHAIN %02d", cur_chain), 4, LIST_Y, CLR_WHITE);
        print("PH", 88, LIST_Y, CLR_MEDIUM_GREY);
        print("TR", 124, LIST_Y, CLR_MEDIUM_GREY);
        bool live = playing && trk[cur_track].on && doc.song[cur_track][trk[cur_track].row] == cur_chain;
        for (int r = 0; r < CLEN; r++) {
            int y = LIST_Y + 12 + r * ROW_H;
            draw_row_bg(y, live && trk[cur_track].cpos == r, r % 4 == 0);
            print(str("%02d", r), 30, y, CLR_DARK_GREY);
            print(c->ph[r] == NONE ? "--" : str("%02d", c->ph[r]), 88, y,
                  c->ph[r] == NONE ? CLR_DARKER_GREY : CLR_LIGHT_GREY);
            print(str("%+03d", c->tr[r]), 124, y, c->tr[r] ? CLR_PINK : CLR_DARKER_GREY);
        }
        draw_cursor(ccur_c == 0 ? 88 : 124, LIST_Y + 12 + ccur_r * ROW_H, ccur_c == 0 ? 18 : 26);
    }

    else if (view == V_PHRASE) {
        Phrase *p = &doc.phr[cur_phrase];
        print(str("PHRASE %02d", cur_phrase), 4, LIST_Y, CLR_WHITE);
        print("NOTE", 88, LIST_Y, CLR_MEDIUM_GREY);
        print("I", 126, LIST_Y, CLR_MEDIUM_GREY);
        print("FX", 144, LIST_Y, CLR_MEDIUM_GREY);
        print("VL", 176, LIST_Y, CLR_MEDIUM_GREY);
        Chain *cc = tchain(cur_track);
        bool live = playing && trk[cur_track].on && cc && cc->ph[trk[cur_track].cpos] == cur_phrase;
        for (int r = 0; r < PLEN; r++) {
            int y = LIST_Y + 12 + r * ROW_H;
            Step *s = &p->s[r];
            draw_row_bg(y, live && trk[cur_track].step == r, r % 4 == 0);
            print(str("%02d", r), 30, y, CLR_DARK_GREY);
            print(note_name(s->note), 88, y,
                  s->note == 0 ? CLR_DARKER_GREY : s->note == NOFF ? CLR_MAUVE : CLR_WHITE);
            print(s->note && s->note != NOFF ? str("%d", s->inst) : ".", 126, y, ICLR[s->inst % NINST]);
            print(FXNAME[s->fx % NFX], 144, y, s->fx ? CLR_PINK : CLR_DARKER_GREY);
            print(s->fx ? str("%02d", s->val) : "..", 176, y, s->fx ? CLR_LIGHT_GREY : CLR_DARKER_GREY);
        }
        static const int CX[4] = { 88, 126, 144, 176 }, CW_[4] = { 26, 10, 26, 18 };
        draw_cursor(CX[pcur_c], LIST_Y + 12 + pcur_r * ROW_H, CW_[pcur_c]);
    }

    else {   // V_INST
        print("INSTRUMENTS", 4, LIST_Y, CLR_WHITE);
        for (int i = 0; i < NINST; i++) {
            int y = LIST_Y + 14 + i * 14;
            if (i == cur_inst) rectfill(0, y - 2, 220, 13, CLR_DARK_BLUE);
            print(str("%d", i), 8, y, CLR_DARK_GREY);
            print(INAME[i], 24, y, ICLR[i]);
            font(FONT_SMALL);
            print(IDESC[i], 68, y + 1, i == cur_inst ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY);
            font(FONT_NORMAL);
        }
    }

    // track activity lamps (all views) — see the song breathe while you edit
    for (int t = 0; t < NTRK; t++) {
        bool lit = playing && frame() - trk[t].flash < 5;
        circfill(268 + t * 12, 8, 3, lit ? CLR_GREEN : trk[t].on && playing ? CLR_DARK_GREEN : CLR_DARKER_GREY);
    }

    draw_help();
}

#ifdef DE_SPEC
#include "spec.h"
// The beat clock is frozen in the headless spec build (sound_tick never runs),
// so the spec drives seq_tick() by hand — testing the ARRANGEMENT logic
// (step/chain/song walking, transpose, HOP) rather than wall-clock timing.
void spec(void) {
    step(1);                       // init() ran (maybe loaded a stale save)
    load_demo();                   // force the known document
    tempo = doc.tempo;
    play_start(0);
    step(1);                       // transport fires row 0 of every track
    expect(trk[0].on && trk[1].on && trk[2].on && trk[3].on, "all four tracks armed at row 0");
    expect(trk[1].handle >= 0, "bass holds a live voice after row 0");
    expect_eq(doc.chn[1].tr[1], -4, "bass chain entry 1 transposes to F");

    for (int i = 0; i < 15; i++) seq_tick();
    expect_eq(trk[0].step, 15, "15 ticks land on the last step of the phrase");
    seq_tick();
    expect_eq(trk[0].cpos, 1, "the 16th tick rolls into chain slot 1");
    expect_eq(trk[0].step, 0, "step counter wrapped with the rollover");
    expect_eq((long)trk[1].base, 41, "chain transpose re-pitches the bass root (A- -> F)");

    // HOP ends a phrase early: a 12-step phrase = an odd meter
    play_stop();
    doc.phr[10].s[0]  = (Step){ 60, I_PLUK, 0, 0 };
    doc.phr[10].s[11] = (Step){ 0, 0, TFX_HOP, 0 };
    for (int i = 0; i < CLEN; i++) doc.chn[9].ph[i] = NONE;
    doc.chn[9].ph[0] = 10; doc.chn[9].ph[1] = 10;
    for (int r = 0; r < SONG_LEN; r++) doc.song[0][r] = NONE;
    doc.song[0][0] = 9;
    play_start(0);
    step(1);                       // fire row 0
    for (int i = 0; i < 12; i++) seq_tick();
    expect_eq(trk[0].cpos, 1, "HOP at step 11 ended the phrase early (12-step meter)");
    expect_eq(trk[0].step, 0, "HOP restarted the next phrase at step 0");

    // ARP/VIB must not leak a bent pitch past their row (the untuned-note bug):
    // hold a note through an ARP row, ride a few frames, then hit an empty row
    play_stop();
    doc.phr[11] = (Phrase){ 0 };
    doc.phr[11].s[0] = (Step){ 60, I_LEAD, TFX_ARP, 37 };
    for (int i = 0; i < CLEN; i++) doc.chn[9].ph[i] = NONE;
    doc.chn[9].ph[0] = 11;
    for (int r = 0; r < SONG_LEN; r++) doc.song[0][r] = NONE;
    doc.song[0][0] = 9;
    play_start(0);
    step(6);                       // row 0 fired; ride() bent the pitch (+3/+7)
    expect(trk[0].handle >= 0, "ARP note is held");
    seq_tick();                    // row 1 is empty — the bend must snap home
    expect(trk[0].sent == trk[0].base && (int)trk[0].base == 60,
           "ARP bend snapped back to true pitch on the next row");

    // the random-song generator: every genre rolls a well-formed song
    for (genre = 0; genre < NGENRE; genre++) {
        gen_song();
        expect(doc.phr[0].s[0].note == 36 && doc.phr[0].s[0].inst == I_KICK,
               "rolled genre lands a kick on step 0");
        int ok = 1;
        for (int p = 0; p <= 5; p++)
            for (int s = 0; s < PLEN; s++) {
                unsigned char n = doc.phr[p].s[s].note;
                if (n != 0 && n != NOFF && (n < 21 || n > 108)) ok = 0;
            }
        expect(ok, "all rolled notes in playable range");
        expect(doc.song[0][0] == 0 && doc.song[2][1] == 4, "rolled song fills both rows");
        expect(tempo >= 40 && tempo <= 260, "rolled tempo in range");
    }
    genre = G_CHIP;
}
#endif
