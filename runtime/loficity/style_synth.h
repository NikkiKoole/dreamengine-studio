// runtime/loficity/style_synth.h — ported from styles/synth.js, verified bit-exact against the oracle.
// ── style: synth (styles/synth.js) — analog pads + bass, 80s city pop ──
// no voicing hook (the default voice-leading); bass = a per-bar CELL from a two-cell pool, main groove only.

static const char *SYN_CELL_ID[7] = { "hold", "lazy", "pop", "sync", "funk", "disco", "kick" };
static const char *SYN_CELLS[6] = {           // BASS_CELLS (kick = the kick pattern)
    "R...............", "R.......R.....o.", "R..o..R...R.o...", "R..R..5...R..R..", "R..oR.o.R..o.Ro.", "R.o.R.o.R.o.R.o.",
};
enum { SYN_HOLD, SYN_LAZY, SYN_POP, SYN_SYNC, SYN_FUNK, SYN_DISCO, SYN_KICK };
// BASS_POOL[e], in each object's own key order
static const struct { int n; int id[6]; double w[6]; } SYN_POOL[3] = {
    { 4, { SYN_HOLD, SYN_LAZY, SYN_KICK, SYN_POP }, { 1.2, 1.4, 1, 0.5 } },
    { 6, { SYN_LAZY, SYN_KICK, SYN_POP, SYN_SYNC, SYN_FUNK, SYN_DISCO }, { 0.6, 1, 1.2, 1, 0.6, 0.3 } },
    { 5, { SYN_POP, SYN_SYNC, SYN_FUNK, SYN_DISCO, SYN_KICK }, { 1.2, 1, 1, 1, 0.4 } },
};
static int syn_pick_w(Rng *r, int e) {
    double sum = 0; for (int i = 0; i < SYN_POOL[e].n; i++) sum += SYN_POOL[e].w[i];     // reduce, left to right
    double u = rn(r) * sum;
    for (int i = 0; i < SYN_POOL[e].n; i++) if ((u -= SYN_POOL[e].w[i]) < 0) return SYN_POOL[e].id[i];
    return SYN_POOL[e].id[0];
}
static int syn_cell_of(const char *s) { for (int i = 0; i < 7; i++) if (!strcmp(SYN_CELL_ID[i], s)) return i; return SYN_KICK; }
static double syn_pick(Rng *r, double a, double b, int d) { return rnd2(r, a, b, d); }   // pick10, default d = 3

static void syn_plan(Plan *P, Rng *r, const Energy *E, int energy) {
    (void)E;
    int e = energy;
    int main_ = !strcmp(style_of(P)->grids[P->pattern].id, "disco") && rn(r) < 0.6 ? SYN_DISCO : syn_pick_w(r, e);
    double rate = syn_pick(r, 0.4, 0.9, 2);
    static const double CUT[3][2] = { { 700, 1100 }, { 850, 1400 }, { 1e3, 1700 } };
    static const double ATK[3][2] = { { 0.03, 0.06 }, { 0.02, 0.045 }, { 0.012, 0.03 } };
    kv_set(P, "synth.detune", syn_pick(r, 6, 10, 1));
    kv_set(P, "synth.cutoff", syn_pick(r, CUT[e][0], CUT[e][1], 0));
    kv_set(P, "synth.env", syn_pick(r, 1.6, 2.6, 2));
    kv_set(P, "synth.res", syn_pick(r, 0.9, 2.2, 2));
    kv_set(P, "synth.atk", syn_pick(r, ATK[e][0], ATK[e][1], 3));
    kv_set(P, "synth.rel", syn_pick(r, 0.14, 0.26, 3));
    kv_set(P, "synth.chorus.rate", rate);
    { double c = syn_pick(r, 6, 10, 1); char buf[64]; snprintf(buf, sizeof buf, "%.5f", c / (1731 * 4 * rate)); kv_set(P, "synth.chorus.depth", strtod(buf, NULL)); }
    kv_set(P, "synth.bass.sq", syn_pick(r, 0.3, 0.7, 2));
    kv_set(P, "synth.bass.q", syn_pick(r, 2, 4.5, 2));
    kv_set(P, "synth.bass.sus", syn_pick(r, 3, 4.5, 2));
    kv_set(P, "synth.bass.env", syn_pick(r, 0.8, 1.2, 2));
    kv_set(P, "synth.bass.dec", syn_pick(r, 0.07, 0.14, 3));
    kv_sets(P, "synth.bass.cells.0", SYN_CELL_ID[main_]);
    kv_sets(P, "synth.bass.cells.1", SYN_CELL_ID[syn_pick_w(r, e)]);
    kv_set(P, "synth.bass.approach", (const double[]){ 0.35, 0.45, 0.5 }[e]);
    kv_set(P, "synth.lead.vib", syn_pick(r, 8, 14, 1));
    kv_set(P, "synth.lead.glide", syn_pick(r, 0.025, 0.05, 3));
    kv_set(P, "drums.kit.kickF0", syn_pick(r, 165, 185, 1));        // Object.assign: lp is kept
    kv_set(P, "drums.kit.kickF1", syn_pick(r, 48, 55, 1));
    kv_set(P, "drums.kit.kickDecay", syn_pick(r, 0.085, 0.115, 3));
    kv_set(P, "drums.kit.snareDecay", syn_pick(r, 0.07, 0.1, 3));
    kv_set(P, "drums.kit.hatHP", syn_pick(r, 7e3, 9e3, 0));
    kv_set(P, "drums.kit.hatClosed", syn_pick(r, 9e-3, 0.014, 3));
    kv_set(P, "drums.perc.clap.f", syn_pick(r, 1100, 1500, 0));
    kv_set(P, "drums.perc.shaker.hp", 5e3);
    kv_set(P, "drums.perc.shaker.decay", 0.028);
    kv_set(P, "drums.perc.gain.clap", syn_pick(r, 0.8, 1, 2));
    kv_set(P, "drums.perc.gain.shaker", 0.8);
    if (P->hasLead) kv_sets(P, "lead.voice", !strcmp(kvs(P, "lead.timbre"), "soft") ? "square" : "glass");
}

static const Chord *syn_chord_at(const BassCtx *cx, double s) {
    for (int q = 0; q < cx->nch; q++) if (s >= cx->chords[q].start * 4 && s < (cx->chords[q].start + cx->chords[q].beats) * 4) return &cx->chords[q];
    return &cx->chords[cx->nch - 1];
}
// one bar of synth bass (main groove only; intro, break and outro keep the default line)
static int syn_bass(BassCtx *cx, BassNote *out) {
    if (cx->mode != BS_KICK) return -1;
    const Plan *P = cx->plan; BarState *st = cx->st; Rng *r = &st->rS;
    const char *id = cx->b->j % 4 == 3 ? kvs(P, rn(r) < 0.6 ? "synth.bass.cells.1" : "synth.bass.cells.0")
                                       : kvs(P, rn(r) < 0.85 ? "synth.bass.cells.0" : "synth.bass.cells.1");
    int cell = syn_cell_of(id);
    struct { int s; char c; } h[48]; int n = 0;
    if (cell == SYN_KICK) for (int k = 0; k < cx->nkicks; k++) { h[n].s = cx->kicks[k]; h[n].c = 'R'; n++; }
    else for (int s = 0; SYN_CELLS[cell][s]; s++) if (SYN_CELLS[cell][s] != '.') { h[n].s = s; h[n].c = SYN_CELLS[cell][s]; n++; }
    for (int q = 0; q < cx->nch; q++) {
        int s0 = (int)(cx->chords[q].start * 4), k2 = 0;
        for (int k = 0; k < n; k++) if (h[k].s != s0) h[k2++] = h[k];
        n = k2; h[n].s = s0; h[n].c = 'R'; n++;
    }
    for (int a = 1; a < n; a++) { typeof(h[0]) x = h[a]; int j = a - 1; while (j >= 0 && h[j].s > x.s) { h[j + 1] = h[j]; j--; } h[j + 1] = x; }
    const Chord *last = &cx->chords[cx->nch - 1], *nx = cx->nnext ? &cx->nextChords[0] : NULL;
    if (nx && nx->root != last->root && rn(r) < kv(P, "synth.bass.approach")) {
        int s = rn(r) < 0.5 ? 14 : 15, k2 = 0;
        for (int k = 0; k < n; k++) if (h[k].s < s) h[k2++] = h[k];
        n = k2; h[n].s = s; h[n].c = 'A'; n++;
    }
    if (cx->stop >= 0) { int k2 = 0; for (int k = 0; k < n; k++) if (h[k].s < cx->stop) h[k2++] = h[k]; n = k2; }
    int end = cx->stop >= 0 ? cx->stop : 16;
    for (int k = 0; k < n; k++) {
        int s = h[k].s; char c = h[k].c;
        int root2 = bass_note(mod12(P->key.tonic + syn_chord_at(cx, s)->root), st->prevBass);
        int midi = root2, slide = 0;
        double len = (k + 1 < n ? h[k + 1].s : end) - s;
        double vel = s == 0 ? 0.92 : s % 4 ? 0.74 : 0.84;
        if (c == 'o') {
            midi = root2 + 12; len = fmin(len, 1.2); vel = 0.78; slide = rn(r) < 0.12;
        } else if (c == 'A') {
            int tg = bass_note(mod12(P->key.tonic + nx->root), root2); double u = rn(r);
            midi = u < 0.45 ? tg - 1 : u < 0.7 ? tg + 1 : tg - 5;
            vel = 0.76; len = fmin(len, 1.5); slide = rn(r) < 0.25;
        } else {
            if (c == '5') midi = root2 + 7 <= 50 ? root2 + 7 : root2 - 5;
            len *= P->bassFactor;
            st->prevBass = root2;
        }
        out[k].s = s; out[k].midi = midi; out[k].vel = vel; out[k].dur = fmax(0.5, len); out[k].slide = slide;
    }
    return n;
}
