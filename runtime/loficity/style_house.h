// runtime/loficity/style_house.h — ported from styles/house.js, verified bit-exact against the oracle.
// ── style: house (styles/house.js) — deep-house organ chords over four-on-the-floor, a sidechain pump ──
// voicing = the cheapest of two fixed shapes per quality; bass = a rolling off-beat sub cell per bar (groove only).

// BASS_CELLS2, in their object's order
static const char *HOU_CELL_ID[6] = { "lazy", "off", "oct", "roll", "sync", "gallop" };
static const char *HOU_CELL[6] = {
    "..R.......R.....", "..R...R...R...R.", "..R...o...R...o.", "..Ro..R...Ro..R.", "..R..R.o..R..R..", "..RR..o...RR..o.",
};
// BASS_POOL2[e]: cell index + weight, in each literal's own key order
static const struct { int n; int idx[5]; double w[5]; } HOU_POOL[3] = {
    { 3, { 0, 1, 2 }, { 1.2, 1, 0.6 } },
    { 5, { 1, 2, 3, 4, 0 }, { 1, 1, 0.6, 0.5, 0.3 } },
    { 5, { 2, 3, 4, 5, 1 }, { 1, 1, 0.8, 0.6, 0.5 } },
};
static int hou_pick_cell(Rng *r, int e) {
    double sum = 0; for (int i = 0; i < HOU_POOL[e].n; i++) sum += HOU_POOL[e].w[i];
    double u = rn(r) * sum;
    for (int i = 0; i < HOU_POOL[e].n; i++) if ((u -= HOU_POOL[e].w[i]) < 0) return HOU_POOL[e].idx[i];
    return HOU_POOL[e].idx[0];
}
static int hou_cell_idx(const char *id) { for (int i = 0; i < 6; i++) if (!strcmp(HOU_CELL_ID[i], id)) return i; return 0; }
static double hou_pick(Rng *r, double a, double b, int d) { return rnd2(r, a, b, d); }
static double hou_fix(double x, int d) { char buf[64]; snprintf(buf, sizeof buf, "%.*f", d, x); return strtod(buf, NULL); }

static void hou_plan(Plan *P, Rng *r, const Energy *E, int energy) {
    (void)E;
    int e = energy;
    double sd = 60.0 / P->bpm / 4;
    // loop = pickW3(r, ep.comp) — its own fallback is the FIRST key
    {
        double sum = 0; for (int i = 0; i < P->ep.ncomp; i++) sum += P->ep.comp[i];
        double u = rn(r) * sum; int loop = 0;
        for (int i = 0; i < P->ep.ncomp; i++) if ((u -= P->ep.comp[i]) < 0) { loop = i; goto found; }
        loop = 0;
    found:
        P->ep.comp[loop] = hou_fix(P->ep.comp[loop] * 4 + 0.5, 2);
    }
    int c0 = hou_pick_cell(r, e), c1 = hou_pick_cell(r, e);
    static const double ATK[3][2] = { { 0.06, 0.14 }, { 0.04, 0.09 }, { 0.025, 0.06 } };
    static const double PLK[3][2] = { { 0.6, 1.2 }, { 0.9, 1.7 }, { 1.2, 2.2 } };
    static const double CDB[3][2] = { { 3, 4.5 }, { 3.8, 5.2 }, { 4.5, 6 } };
    static const double KDEC[3][2] = { { 0.17, 0.22 }, { 0.15, 0.2 }, { 0.13, 0.18 } };
    static const double APPR[3] = { 0.3, 0.4, 0.45 };
    kv_set(P, "house.chords.detune", hou_pick(r, 4, 8, 1));
    kv_set(P, "house.chords.padDetune", hou_pick(r, 8, 14, 1));
    kv_set(P, "house.chords.atk", hou_pick(r, ATK[e][0], ATK[e][1], 3));
    kv_set(P, "house.chords.rel", hou_pick(r, 0.22, 0.4, 3));
    kv_set(P, "house.chords.q", hou_pick(r, 0.9, 1.6, 2));
    kv_set(P, "house.chords.pluck", hou_pick(r, PLK[e][0], PLK[e][1], 2));
    kv_set(P, "house.chords.pluckTau", hou_pick(r, 0.07, 0.12, 3));
    kv_set(P, "house.chords.echo", hou_pick(r, 0.14, 0.24, 2));
    kv_set(P, "house.chords.open", hou_pick(r, 1.35, 1.7, 2));
    kv_set(P, "house.chords.build", hou_pick(r, 1.15, 1.35, 2));
    kv_set(P, "house.pump.chordDb", hou_pick(r, CDB[e][0], CDB[e][1], 2));
    kv_set(P, "house.pump.bassDb", hou_pick(r, 2.5, 4, 2));
    kv_set(P, "house.pump.atk", 4e-3);
    kv_set(P, "house.pump.hold", 0.02);
    kv_set(P, "house.pump.rec", hou_fix(hou_pick(r, 0.28, 0.38, 3) * 2 * sd, 4));
    kv_sets(P, "house.bass.cells.0", HOU_CELL_ID[c0]);
    kv_sets(P, "house.bass.cells.1", HOU_CELL_ID[c1]);
    kv_set(P, "house.bass.approach", APPR[e]);
    kv_set(P, "house.bass.tri", hou_pick(r, 0.22, 0.4, 2));
    kv_set(P, "house.bass.drive", hou_pick(r, 1.3, 2.2, 2));
    kv_set(P, "house.bass.lp", hou_pick(r, 520, 820, 0));
    kv_set(P, "house.lead.vib", hou_pick(r, 10, 16, 1));
    kv_set(P, "house.lead.glide", hou_pick(r, 0.03, 0.05, 3));
    // Object.assign(drums.kit, {…}) — the literal first, in order
    kv_set(P, "drums.kit.kickF0", hou_pick(r, 115, 140, 1));
    kv_set(P, "drums.kit.kickF1", hou_pick(r, 47, 54, 1));
    kv_set(P, "drums.kit.kickSweep", hou_pick(r, 0.035, 0.05, 3));
    kv_set(P, "drums.kit.kickDecay", hou_pick(r, KDEC[e][0], KDEC[e][1], 3));
    kv_set(P, "drums.kit.clapF", hou_pick(r, 1e3, 1400, 0));
    kv_set(P, "drums.kit.clapTail", hou_pick(r, 0.07, 0.11, 3));
    kv_set(P, "drums.kit.snareDecay", hou_pick(r, 0.055, 0.08, 3));
    kv_set(P, "drums.kit.hatHP", hou_pick(r, 7e3, 8800, 0));
    kv_set(P, "drums.kit.hatClosed", hou_pick(r, 0.012, 0.018, 3));
    kv_set(P, "drums.kit.openDecay", hou_pick(r, 0.14, 0.22, 3));
    kv_set(P, "drums.kit.shakerDecay", hou_pick(r, 0.026, 0.038, 3));
    kv_set(P, "drums.kit.rideDecay", hou_pick(r, 0.28, 0.42, 3));
    int n2 = 2 + (int)floor(rn(r) * 3), last = 3 + (int)floor(rn(r) * 4);
    if (P->hasLead) {
        Motif *m = &P->motif;
        int k = n2 < m->n ? n2 : m->n;
        kv_sets(P, "lead.voice", !strcmp(kvs(P, "lead.timbre"), "soft") ? "vox" : "pluck");
        for (int i = 0; i < k; i++) m->durs[i] = i < k - 1 ? (m->durs[i] < 3 ? m->durs[i] : 3) : last;
        m->n = k;
    }
}

// SHAPES: two fixed voicings per quality (else the generic voice())
static int hou_shapes(int q, const int (**sh)[4]) {
    static const int M7[2][4] = { { 10, 15, 19, 24 }, { 3, 7, 10, 15 } };
    static const int M9[2][4] = { { 10, 15, 19, 26 }, { 3, 7, 10, 14 } };
    static const int M11[2][4] = { { 5, 10, 15, 19 }, { 3, 10, 14, 17 } };
    static const int MAJ7[2][4] = { { 11, 16, 19, 24 }, { 4, 7, 11, 16 } };
    static const int MAJ9[2][4] = { { 11, 16, 19, 26 }, { 4, 7, 11, 14 } };
    static const int S74[2][4] = { { 10, 17, 19, 24 }, { 5, 10, 12, 17 } };
    static const int S94[2][4] = { { 5, 10, 14, 19 }, { 10, 14, 17, 19 } };
    static const int ADD9[2][4] = { { 4, 7, 12, 14 }, { 7, 12, 14, 16 } };
    static const int N69[2][4] = { { 4, 9, 14, 19 }, { 9, 14, 16, 19 } };
    switch (q) {
    case Q_M7: *sh = M7; return 1; case Q_M9: *sh = M9; return 1; case Q_M11: *sh = M11; return 1;
    case Q_MAJ7: *sh = MAJ7; return 1; case Q_MAJ9: *sh = MAJ9; return 1; case Q_N7SUS4: *sh = S74; return 1;
    case Q_N9SUS4: *sh = S94; return 1; case Q_ADD9: *sh = ADD9; return 1; case Q_N69: *sh = N69; return 1;
    }
    return 0;
}
static Voicing hou_voicing(const Spelled *sp, const Voicing *prev, const Chord *c, const Plan *P, BarState *st) {
    (void)st;
    const int (*sh)[4];
    if (!hou_shapes(c->q, &sh)) return lc_voice(sp, prev, 0);
    double home = 62 + (prev ? 0 : P->reg), w = prev ? 0.35 : 0.7;
    Voicing best; int any = 0; double bc = INFINITY;
    for (int k = 0; k < 2; k++) for (int o = 2; o <= 6; o++) {
        int v[4]; for (int i = 0; i < 4; i++) v[i] = sp->root + sh[k][i] + 12 * o;
        if (v[0] < 48 || v[0] > 64 || v[3] > 79) continue;
        double mean = (v[0] + v[1] + v[2] + v[3]) / 4.0;
        double cost = w * fabs(mean - home) - (k ? 0 : 0.5);
        if (prev && prev->n == 4) {
            for (int i = 0; i < 4; i++) cost += abs(v[i] - prev->m[i]);
            cost += 1.5 * fmax(0, abs(v[3] - prev->m[3]) - 3);
        }
        if (cost < bc) { bc = cost; best.n = 4; memcpy(best.m, v, sizeof v); any = 1; }
    }
    return any ? best : lc_voice(sp, prev, 0);
}

static const Chord *hou_chord_at(const BassCtx *cx, int s) {
    for (int q = 0; q < cx->nch; q++) if (s >= cx->chords[q].start * 4 && s < (cx->chords[q].start + cx->chords[q].beats) * 4) return &cx->chords[q];
    return &cx->chords[cx->nch - 1];
}
static int hou_bass(BassCtx *cx, BassNote *out) {
    if (cx->mode != BS_KICK) return -1;
    const Plan *P = cx->plan; BarState *st = cx->st; Rng *r = &st->rS;
    char id[20];
    if (cx->b->j % 4 == 3) snprintf(id, sizeof id, "%s", kvs(P, rn(r) < 0.6 ? "house.bass.cells.1" : "house.bass.cells.0"));
    else snprintf(id, sizeof id, "%s", kvs(P, rn(r) < 0.85 ? "house.bass.cells.0" : "house.bass.cells.1"));
    const char *cell = HOU_CELL[hou_cell_idx(id)];
    int hs[20]; char hc[20]; int nh = 0;
    for (int s = 0; cell[s]; s++) if (cell[s] != '.') { hs[nh] = s; hc[nh] = cell[s]; nh++; }
    const Chord *last = &cx->chords[cx->nch - 1], *nx = cx->nextChords && cx->nnext ? &cx->nextChords[0] : NULL;
    if (nx && nx->root != last->root && rn(r) < kv(P, "house.bass.approach")) {
        int k = 0; for (int q = 0; q < nh; q++) if (hs[q] < 14) { hs[k] = hs[q]; hc[k] = hc[q]; k++; }
        nh = k; hs[nh] = 14; hc[nh] = 'A'; nh++;
    }
    if (cx->stop >= 0) { int k = 0; for (int q = 0; q < nh; q++) if (hs[q] < cx->stop) { hs[k] = hs[q]; hc[k] = hc[q]; k++; } nh = k; }
    int end = cx->stop >= 0 ? cx->stop : 16;
    for (int k = 0; k < nh; k++) {
        int s = hs[k]; char c = hc[k];
        int root = bass_note(mod12(P->key.tonic + hou_chord_at(cx, s)->root), st->prevBass), kick = 4 * (s / 4) + 4;
        int midi = root, slide = 0;
        int nxs = k + 1 < nh ? hs[k + 1] : end;
        double len = (nxs < kick ? nxs : kick) - s, vel = s % 4 == 2 ? 0.88 : 0.72;
        if (c == 'o') { midi = root + 12; len *= 0.8; vel = 0.78; slide = rn(r) < 0.12; }
        else if (c == 'A') {
            int tg = bass_note(mod12(P->key.tonic + nx->root), root); double u = rn(r);
            midi = u < 0.45 ? tg - 1 : u < 0.7 ? tg + 1 : tg - 5;
            vel = 0.8; slide = rn(r) < 0.2;
        } else {
            if (c == '5') midi = root + 7 <= 50 ? root + 7 : root - 5;
            len *= P->bassFactor;
            st->prevBass = root;
        }
        out[k].s = s; out[k].midi = midi; out[k].vel = vel; out[k].dur = fmax(0.5, len); out[k].slide = slide;
    }
    return nh;
}
