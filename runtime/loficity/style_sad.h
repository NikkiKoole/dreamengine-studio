// runtime/loficity/style_sad.h — ported from styles/sad.js, verified bit-exact against the oracle.
// ── style: sad (styles/sad.js) — minor chords, slow and sparse, a lonely muted lead ──
// voicing = a LEFT-HAND root under a right-hand shape chosen by cost (a lower, darker register than
// Piano's, and a top line that prefers to FALL); bass = a held sub root with an occasional walk down.

// rhTones2: the right hand's intervals per quality (RH2 table, else the quality minus its roots)
static int sad_rh_tones(int q, int *iv) {
    static const struct { int q, n; int iv[4]; } RH[] = {
        { Q_M9, 4, { 3, 7, 10, 14 } }, { Q_M11, 4, { 3, 10, 14, 17 } }, { Q_MADD9, 4, { 3, 7, 12, 14 } },
        { Q_SADMMAJ9, 4, { 3, 7, 11, 14 } }, { Q_M6, 4, { 3, 7, 9, 14 } }, { Q_M7, 3, { 3, 7, 10 } },
        { Q_M7B5, 4, { 3, 6, 10, 17 } }, { Q_MAJ7, 4, { 4, 7, 11, 14 } }, { Q_MAJ9, 4, { 4, 7, 11, 14 } },
        { Q_ADD9, 4, { 4, 7, 12, 14 } }, { Q_N6, 4, { 4, 7, 9, 12 } }, { Q_N69, 4, { 4, 7, 9, 14 } },
        { Q_N7SUS4, 4, { 5, 7, 10, 12 } }, { Q_N9SUS4, 4, { 5, 7, 10, 14 } }, { Q_SADSUS2, 4, { 7, 12, 14, 19 } },
        { Q_N7, 4, { 4, 7, 10, 12 } },
    };
    for (unsigned k = 0; k < sizeof RH / sizeof *RH; k++) if (RH[k].q == q) { memcpy(iv, RH[k].iv, sizeof(int) * RH[k].n); return RH[k].n; }
    int n = 0;
    for (int i = 0; i < QUAL[q].n; i++) {                    // [...new Set(iv.filter(i => i % 12))]
        int v = QUAL[q].iv[i]; if (!(v % 12)) continue;
        int dup = 0; for (int j = 0; j < n; j++) if (iv[j] == v) dup = 1;
        if (!dup) iv[n++] = v;
    }
    if (n > 4) { int k = 0; for (int i = 0; i < n; i++) if (iv[i] != 7) iv[k++] = iv[i]; n = k > 4 ? 4 : k; }
    if (!n) { iv[0] = 3; iv[1] = 7; n = 2; }
    return n;
}
static int sad_lh_root(int pc, int prev) {
    int best = 36 + mod12(pc - 36); double bc = INFINITY; int b = best;
    for (int m = best; m <= 48; m += 12) { double c = abs(m - prev) + 0.4 * abs(m - 40); if (c < bc) { bc = c; b = m; } }
    return b;
}
static double sad_rh_cost(const int *v, int n, const int *prev, int pn, int lh, int reg) {
    int top = v[n - 1], home = prev ? 0 : reg;
    double c = 0.35 * fabs(pno_avg(v, n) - 60 - home) + 0.6 * fmax(0, top - 69 - home) + 0.15 * abs(top - v[0] - 12) + 2 * fmax(0, lh + 7 - v[0]);
    if (!prev) return c + 0.3 * abs(top - 67 - reg);
    int mn = n < pn ? n : pn;
    for (int i = 1; i <= mn; i++) c += abs(v[n - i] - prev[pn - i]);
    int dt = top - prev[pn - 1];
    c += 1.5 * fmax(0, abs(dt) - 2) + (abs(dt) > 5 ? 4 : 0) + (dt > 0 ? 0.8 : dt >= -2 && dt < 0 ? -0.8 : 0);
    for (int i = 0; i < n; i++) for (int j = 0; j < pn; j++) if (prev[j] == v[i]) { c -= 0.6; break; }
    return c;
}
static Voicing sad_voicing(const Spelled *sp, const Voicing *prev, const Chord *c, const Plan *P, BarState *st) {
    (void)sp; (void)st;
    int root = mod12(P->key.tonic + c->root), iv[7], n = sad_rh_tones(c->q, iv), pcs[7];
    for (int i = 0; i < n; i++) pcs[i] = mod12(root + iv[i]);
    if (prev && mod12(prev->m[0]) == root && prev->n == n + 1) {        // the same chord shape: keep it
        int ok = 1;
        for (int i = 1; i < prev->n && ok; i++) { int f = 0; for (int j = 0; j < n; j++) if (pcs[j] == mod12(prev->m[i])) f = 1; if (!f) ok = 0; }
        for (int j = 0; j < n && ok; j++) { int f = 0; for (int i = 1; i < prev->n; i++) if (mod12(prev->m[i]) == pcs[j]) f = 1; if (!f) ok = 0; }
        if (ok) return *prev;
    }
    int reg2 = P->reg, creg = prev ? 0 : reg2;
    int lh = sad_lh_root(root, prev ? prev->m[0] : 40 + reg2);
    const int *pr = prev ? prev->m + 1 : NULL; int pn = prev ? prev->n - 1 : 0;
    int lo0 = 50 + (creg < 0 ? creg : 0), lo1 = 62 + (creg > 0 ? creg : 0), t0 = 60 + (creg < 0 ? creg : 0), t1 = 72 + (creg > 0 ? creg : 0);
    int perms[24][4], np = pno_perms(n, perms), best[7], nb = 0; double bc = INFINITY;
    for (int o = 0; o < np; o++) for (int lo = lo0; lo <= lo1; lo++) {
        if (mod12(lo) != pcs[perms[o][0]]) continue;
        int v[7]; v[0] = lo;
        for (int k = 1; k < n; k++) { int m = v[k - 1] + 1; while (mod12(m) != pcs[perms[o][k]]) m++; v[k] = m; }
        int top = v[n - 1], span = top - lo;
        if (top < t0 || top > t1 || span < 5 || span > 19) continue;
        int ok = 1;
        for (int k = 1; k < n && ok; k++) if (v[k] - v[k - 1] < 2 || (v[k - 1] < 55 && v[k] - v[k - 1] < 3)) ok = 0;
        for (int a = 0; a < n && ok; a++) for (int b2 = a + 1; b2 < n; b2++) if (v[b2] - v[a] == 13) ok = 0;
        if (!ok) continue;                                   // (their de-dup of candidates cannot change the first minimum)
        double cst = sad_rh_cost(v, n, pr, pn, lh, reg2);
        if (cst < bc) { bc = cst; memcpy(best, v, sizeof(int) * n); nb = n; }
    }
    if (!nb) {
        for (int i = 0; i < n; i++) { int m = 53 + mod12(pcs[i] - 53); while (nb && m <= best[nb - 1]) m += 12; best[nb++] = m; }
    }
    Voicing out; out.n = nb + 1; out.m[0] = lh; memcpy(out.m + 1, best, sizeof(int) * nb);
    return out;
}
// SIGHS: the lead's own motifs [steps, durs]; FALLS[n]: their contours (falling)
static const int SAD_SIGH_N[6] = { 2, 3, 3, 3, 4, 3 };
static const int SAD_SIGHS[6][2][4] = {
    { { 0, 6 }, { 6, 14 } }, { { 0, 8, 12 }, { 8, 4, 14 } }, { { 2, 8, 16 }, { 6, 8, 12 } },
    { { 0, 4, 16 }, { 4, 12, 12 } }, { { 0, 10, 16, 22 }, { 10, 6, 6, 10 } }, { { 4, 8, 20 }, { 4, 12, 10 } },
};
static const int SAD_FALLN[5] = { 0, 0, 3, 4, 3 };
static const int SAD_FALLS[5][4][4] = {
    { { 0 } }, { { 0 } },
    { { 0, -1 }, { 1, 0 }, { 1, -1 } },
    { { 0, -1, -2 }, { 1, 0, -1 }, { 2, 1, 0 }, { 1, 0, -2 } },
    { { 1, 0, -1, -2 }, { 2, 1, 0, -1 }, { 0, -1, -2, -1 } },
};
static void sad_plan(Plan *P, Rng *r, const Energy *E, int energy) {
    (void)energy;
#define XR(k) E->x[k].v[0][0], E->x[k].v[0][1]
    kv_set(P, "piano.detune", rr2(r, 1.6, 3));
    kv_set(P, "piano.stretch", rr2(r, 0.8, 1.2));
    kv_set(P, "piano.hammer", rr2(r, 0.45, 0.75));
    kv_set(P, "piano.bright", rr2(r, 0.72, 0.9));
    kv_set(P, "piano.roll", rr2(r, XR(ST_SAD_X_ROLL)));
    kv_set(P, "sad.warble", rnd2(r, XR(ST_SAD_X_WARBLE), 1));
    kv_set(P, "sad.rate", rr2(r, 0.32, 0.5));
    kv_set(P, "sad.pad", rr2(r, 0.8, 1.2));
    kv_set(P, "sad.padAtk", rr2(r, 0.5, 0.8));
    kv_set(P, "sad.padRel", rr2(r, 0.7, 1));
    kv_set(P, "sad.det", rnd2(r, 6, 11, 1));
    kv_set(P, "sad.padLp", rnd2(r, 1e3, 1500, 0));
    kv_set(P, "sad.walk", rr2(r, 0.2, 0.5));
    kv_set(P, "sad.lean", rr2(r, XR(ST_SAD_X_LEAN)));
    kv_set(P, "sad.vib", rnd2(r, 10, 16, 1));
    double lp = kv(P, "drums.kit.lp");
    kv_del_prefix(P, "drums.kit.");
    kv_set(P, "drums.kit.kickF0", rnd2(r, 95, 110, 1));
    kv_set(P, "drums.kit.kickF1", rnd2(r, 40, 46, 1));
    kv_set(P, "drums.kit.kickDecay", rnd2(r, 0.2, 0.28, 3));
    kv_set(P, "drums.kit.snareBP", rnd2(r, 1100, 1600, 0));
    kv_set(P, "drums.kit.snareDecay", rnd2(r, 0.12, 0.2, 3));
    kv_set(P, "drums.kit.hatHP", rnd2(r, 3600, 4800, 0));
    kv_set(P, "drums.kit.hatClosed", rnd2(r, 0.018, 0.03, 3));
    kv_set(P, "drums.kit.lp", lp);
    int firstA = 1;
    for (int i = 0; i < P->nsec; i++) {
        Section *s = &P->sec[i];
        if (s->name != S_A && s->name != S_B) continue;
        double odds = s->name == S_A && firstA ? E->x[ST_SAD_X_BAREFIRST].v[0][0] : E->x[ST_SAD_X_BARE].v[0][0];
        if (rn(r) < odds * fmin(1, 8.0 / s->bars)) s->L.drums = DR_NONE;
        if (s->name == S_A) firstA = 0;
    }
    if (P->hasLead) {
        int si = rfloor(r, 6), n = SAD_SIGH_N[si];
        kv_sets(P, "lead.timbre", "mute");
        Motif *m = &P->motif; m->n = n;
        for (int k = 0; k < n; k++) { m->steps[k] = SAD_SIGHS[si][0][k]; m->durs[k] = SAD_SIGHS[si][1][k]; }
        int fi = rfloor(r, SAD_FALLN[n]);
        for (int k = 0; k < n; k++) m->contour[k] = SAD_FALLS[n][fi][k];
        for (int k = 0; k < n; k++) m->vel[k] = k ? rr2(r, 0.56, 0.64) : 0.72;
    }
#undef XR
}
// stepAbove: the next scale step above m (a semitone if it is in the key, else a tone)
static int sad_step_above(int m, Key key) {
    static const int SC[2][7] = { { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 5, 7, 9, 11 } };
    int pc = mod12(m + 1 - key.tonic);
    for (int i = 0; i < 7; i++) if (SC[key.major][i] == pc) return m + 1;
    return m + 2;
}
// bass2: one held root per chord (the final one rings to the end); a long last chord walks down to the
// next root through the step above it, on the "4" of the bar
static int sad_bass(BassCtx *cx, BassNote *out) {
    const Plan *P = cx->plan; BarState *st = cx->st; Key K = P->key;
    int n = 0, end = cx->stop >= 0 ? cx->stop : 16;
#define PUT(s_, m_, v_, d_) do { out[n].s = (s_); out[n].midi = (m_); out[n].vel = (v_); out[n].dur = (d_); out[n].slide = 0; n++; } while (0)
    for (int k = 0; k < cx->nch; k++) {
        const Chord *c = &cx->chords[k];
        double s = c->start * 4;
        if (s >= end) continue;
        int m = bass_note(mod12(K.tonic + c->root), st->prevBass);
        st->prevBass = m;
        if (c->fin) { PUT((int)s, m, 0.8, (cx->b->n - 2) * 16); continue; }
        double e = fmin(end, s + c->beats * 4), vel = c->onset ? 0.84 : 0.72;
        const Chord *nx = k == cx->nch - 1 && cx->nextChords ? &cx->nextChords[0] : NULL;
        if (cx->mode == BS_KICK && nx && e == 16 && e - s >= 8) {
            int tgt = bass_note(mod12(K.tonic + nx->root), m), gap = m - tgt;
            if (gap >= 3 && gap <= 7 && rn(&st->rS) < kv(P, "sad.walk")) {
                PUT((int)s, m, vel, 12 - s);
                PUT(12, sad_step_above(tgt, K), 0.64, 4);
                continue;
            }
        }
        PUT((int)s, m, vel, e - s);
    }
#undef PUT
    return n;
}
