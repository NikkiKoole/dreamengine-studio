// runtime/loficity/style_piano.h — ported from styles/piano.js, verified bit-exact against the oracle.
// ── style: piano (styles/piano.js) — felt piano, simple warm chords, soft or no drums ──
// voicing = a LEFT-HAND root under a right-hand shape chosen by cost; bass = the rest of the left hand.

// rhTones: the right hand's intervals per quality (RH table, else the quality minus its roots)
static int pno_rh_tones(int q, int *iv) {
    static const struct { int q; int iv[4]; } RH[] = {
        { Q_MAJ7, { 4, 7, 11, 14 } }, { Q_MAJ9, { 4, 7, 11, 14 } }, { Q_M7, { 3, 7, 10, 14 } }, { Q_M9, { 3, 7, 10, 14 } },
        { Q_ADD9, { 4, 7, 12, 14 } }, { Q_MADD9, { 3, 7, 12, 14 } }, { Q_N6, { 4, 7, 9, 14 } }, { Q_N69, { 4, 7, 9, 14 } },
        { Q_M6, { 3, 7, 9, 14 } }, { Q_N9SUS4, { 5, 7, 10, 14 } }, { Q_N7SUS4, { 5, 7, 10, 14 } }, { Q_N7, { 4, 7, 10, 12 } },
        { Q_N9, { 4, 7, 10, 14 } }, { Q_N13, { 4, 10, 14, 21 } }, { Q_M7B5, { 3, 6, 10, 17 } }, { Q_N7B9, { 4, 7, 10, 13 } },
    };
    for (unsigned k = 0; k < sizeof RH / sizeof *RH; k++) if (RH[k].q == q) { memcpy(iv, RH[k].iv, sizeof RH[k].iv); return 4; }
    int n = 0;
    for (int i = 0; i < QUAL[q].n; i++) {                    // [...new Set(iv.filter(i => i % 12))]
        int v = QUAL[q].iv[i]; if (!(v % 12)) continue;
        int dup = 0; for (int j = 0; j < n; j++) if (iv[j] == v) dup = 1;
        if (!dup) iv[n++] = v;
    }
    if (n > 4) { int k = 0; for (int i = 0; i < n; i++) if (iv[i] != 7) iv[k++] = iv[i]; n = k > 4 ? 4 : k; }
    if (!n) { iv[0] = 4; iv[1] = 7; n = 2; }
    return n;
}
// PERMS[n], in perms()' own order: perms(n) = perms(n-1).flatMap(p => [0..n-1].map(k => insert n-1 at k))
static int pno_perms(int n, int out[24][4]) {
    int cur[24][4], nc = 1; cur[0][0] = 0;
    for (int m = 2; m <= n; m++) {
        int nx[24][4], nn = 0;
        for (int p = 0; p < nc; p++) for (int k = 0; k < m; k++) {
            int w = 0; for (int i = 0; i < k; i++) nx[nn][w++] = cur[p][i];
            nx[nn][w++] = m - 1;
            for (int i = k; i < m - 1; i++) nx[nn][w++] = cur[p][i];
            nn++;
        }
        memcpy(cur, nx, sizeof nx); nc = nn;
    }
    memcpy(out, cur, sizeof cur);
    return nc;
}
static int pno_lh_root(int pc, int prev) {
    int best = 34 + mod12(pc - 34); double bc = INFINITY; int b = best;
    for (int m = best; m <= 50; m += 12) { double c = abs(m - prev) + 0.4 * abs(m - 41); if (c < bc) { bc = c; b = m; } }
    return b;
}
static double pno_avg(const int *v, int n) { double s = 0; for (int i = 0; i < n; i++) s += v[i]; return s / n; }
static double pno_rh_cost(const int *v, int n, const int *prev, int pn, int lh, int reg) {
    int top = v[n - 1], r = prev ? 0 : reg;
    double c = 0.35 * fabs(pno_avg(v, n) - 63 - r) + 0.6 * fmax(0, top - 74) + 0.15 * abs(top - v[0] - 15) + 2 * fmax(0, lh + 7 - v[0]);
    if (!prev) return c + 0.3 * abs(top - 70 - r);
    int mn = n < pn ? n : pn;
    for (int i = 1; i <= mn; i++) c += abs(v[n - i] - prev[pn - i]);
    int dt = abs(top - prev[pn - 1]);
    c += 1.5 * fmax(0, dt - 2) + (dt > 5 ? 4 : 0);
    for (int i = 0; i < n; i++) for (int j = 0; j < pn; j++) if (prev[j] == v[i]) { c -= 0.5; break; }
    return c;
}
static Voicing pno_voicing(const Spelled *sp, const Voicing *prev, const Chord *c, const Plan *P, BarState *st) {
    (void)sp; (void)st;
    int root = mod12(P->key.tonic + c->root), iv[7], n = pno_rh_tones(c->q, iv), pcs[7];
    for (int i = 0; i < n; i++) pcs[i] = mod12(root + iv[i]);
    if (prev && mod12(prev->m[0]) == root && prev->n == n + 1) {        // the same chord shape: keep it
        int ok = 1;
        for (int i = 1; i < prev->n && ok; i++) { int f = 0; for (int j = 0; j < n; j++) if (pcs[j] == mod12(prev->m[i])) f = 1; if (!f) ok = 0; }
        for (int j = 0; j < n && ok; j++) { int f = 0; for (int i = 1; i < prev->n; i++) if (mod12(prev->m[i]) == pcs[j]) f = 1; if (!f) ok = 0; }
        if (ok) return *prev;
    }
    int lh = pno_lh_root(root, prev ? prev->m[0] : 41);
    const int *pr = prev ? prev->m + 1 : NULL; int pn = prev ? prev->n - 1 : 0;
    int perms[24][4], np = pno_perms(n, perms), best[7], nb = 0; double bc = INFINITY;
    for (int o = 0; o < np; o++) for (int lo = 48; lo <= 61; lo++) {
        if (mod12(lo) != pcs[perms[o][0]]) continue;
        int v[7]; v[0] = lo;
        for (int k = 1; k < n; k++) { int m = v[k - 1] + 1; while (mod12(m) != pcs[perms[o][k]]) m++; v[k] = m; }
        int top = v[n - 1], span = top - lo;
        if (top < 62 || top > 76 || span < 9 || span > 22) continue;
        int ok = 1;
        for (int k = 1; k < n && ok; k++) if (v[k] - v[k - 1] < 2 || (v[k - 1] < 55 && v[k] - v[k - 1] < 3)) ok = 0;
        for (int a = 0; a < n && ok; a++) for (int b2 = a + 1; b2 < n; b2++) if (v[b2] - v[a] == 13) ok = 0;
        if (!ok) continue;
        double cst = pno_rh_cost(v, n, pr, pn, lh, P->reg);
        if (cst < bc) { bc = cst; memcpy(best, v, sizeof(int) * n); nb = n; }
    }
    if (!nb) {
        for (int i = 0; i < n; i++) { int m = 55 + mod12(pcs[i] - 55); while (nb && m <= best[nb - 1]) m += 12; best[nb++] = m; }
    }
    Voicing out; out.n = nb + 1; out.m[0] = lh; memcpy(out.m + 1, best, sizeof(int) * nb);
    return out;
}
// pickW over LH2 = { fifth: 1, octave: 0.8, tenth: 0.6, still: 0.3 }
static const char *PNO_LH[4] = { "fifth", "octave", "tenth", "still" };
static int pno_pick_lh(Rng *r) {
    static const double W[4] = { 1, 0.8, 0.6, 0.3 };
    double u = rn(r) * (1 + 0.8 + 0.6 + 0.3);
    for (int i = 0; i < 4; i++) if ((u -= W[i]) < 0) return i;
    return 0;
}
static double rr2(Rng *r, double a, double b) { return rnd2(r, a, b, 2); }
static void pno_plan(Plan *P, Rng *r, const Energy *E, int energy) {
    (void)energy;
    kv_set(P, "piano.detune", rr2(r, 0.5, 1.5));
    kv_set(P, "piano.stretch", rr2(r, 0.7, 1.3));
    kv_set(P, "piano.hammer", rr2(r, 0.7, 1.3));
    kv_set(P, "piano.bright", rr2(r, 0.85, 1.15));
    kv_set(P, "piano.roll", rr2(r, E->x[ST_PIANO_X_ROLL].v[0][0], E->x[ST_PIANO_X_ROLL].v[0][1]));
    kv_sets(P, "piano.lh", PNO_LH[pno_pick_lh(r)]);
    kv_set(P, "piano.restrike", rr2(r, E->x[ST_PIANO_X_RESTRIKE].v[0][0], E->x[ST_PIANO_X_RESTRIKE].v[0][1]));
    double lp = kv(P, "drums.kit.lp");
    kv_del_prefix(P, "drums.kit.");
    kv_set(P, "drums.kit.kickF0", rnd2(r, 105, 125, 1));
    kv_set(P, "drums.kit.kickF1", rnd2(r, 44, 52, 1));
    kv_set(P, "drums.kit.kickDecay", rnd2(r, 0.16, 0.24, 3));
    kv_set(P, "drums.kit.snareDecay", rnd2(r, 0.1, 0.16, 3));
    kv_set(P, "drums.kit.snareBP", rnd2(r, 1700, 2400, 0));
    kv_set(P, "drums.kit.hatHP", rnd2(r, 4200, 5600, 0));
    kv_set(P, "drums.kit.hatClosed", rnd2(r, 0.02, 0.035, 3));
    kv_set(P, "drums.kit.lp", lp);
    if (P->hasLead) kv_sets(P, "lead.timbre", "piano");
}
static int qual_has(int q, int iv) { for (int i = 0; i < QUAL[q].n; i++) if (QUAL[q].iv[i] == iv) return 1; return 0; }
// the rest of the LEFT HAND under the voicing's root: a broken fifth (+ octave), the octave, or a tenth,
// plus a few re-strikes on the kick; held to the chord's end (the pedal), cut by a fill's stop or a push
static int pno_bass(BassCtx *cx, BassNote *out) {
    const Plan *P = cx->plan; BarState *st = cx->st; Rng *rS = &st->rS;
    const Voicing *v = st->hasPrevVoicing ? &st->prevVoicing : NULL;
    int n = 0;
    int end = (int)fmin(cx->stop >= 0 ? cx->stop : 16, st->pushed == cx->i + 1 ? 14 : 16);
    int lh = rn(rS) < 0.2 ? pno_pick_lh(rS) : -1;
    const char *lhs = lh >= 0 ? PNO_LH[lh] : kvs(P, "piano.lh");
    double soft = cx->mode == BS_KICK ? 1 : 0.8;
    for (int k = 0; k < cx->nch; k++) {
        const Chord *c = &cx->chords[k];
        double c0 = c->start * 4, c1 = c->fin ? (cx->b->n - 2) * 16 : fmin(c0 + c->beats * 4, end);
        if (c0 >= c1) continue;
        int pc = mod12(P->key.tonic + c->root);
        int root = v && mod12(v->m[0]) == pc ? v->m[0] : pno_lh_root(pc, v ? v->m[0] : 41), rhLo = v ? v->m[1] : 55;
        int fifth = root + (qual_has(c->q, 7) ? 7 : qual_has(c->q, 6) ? 6 : 7);
        int third = qual_has(c->q, 4) ? 4 : qual_has(c->q, 3) ? 3 : -1;
#define FITS(m) ((m) < rhLo - 1)
#define ADD(s_, m_, vel_) do { double s__ = (s_); int m__ = (m_); double v__ = (vel_); \
        if (s__ < c1 && FITS(m__)) { out[n].s = (int)s__; out[n].midi = m__; out[n].vel = v__; out[n].dur = c1 - s__; out[n].slide = 0; n++; } } while (0)
        double len = c1 - c0;
        const char *style = c->fin ? "fifth" : lhs;
        int tenth = third >= 0 ? root + 12 + third : -1, brk = len >= 4 ? 2 : 0;
        if (!strcmp(style, "fifth")) {
            ADD(c0 + brk, fifth, 0.62 * soft);
            if (len >= 8) ADD(c0 + 4, root + 12, 0.56 * soft);
        } else if (!strcmp(style, "octave")) {
            if (root >= 43) ADD(c0, root - 12, 0.62 * soft);
            else if (FITS(root + 12)) ADD(c0, root + 12, 0.6 * soft);
            else ADD(c0 + brk, fifth, 0.6 * soft);
        } else if (!strcmp(style, "tenth"))
            ADD(c0 + (len >= 4 ? 1 : 0), tenth >= 0 && tenth < rhLo - 2 ? tenth : fifth, 0.6 * soft);
        if (cx->mode == BS_KICK) {
            for (int q = 0; q < cx->nkicks; q++) {
                int s = cx->kicks[q];
                if (s > c0 + 3 && s < c1 - 1 && rn(rS) < kv(P, "piano.restrike")) {
                    int m = rn(rS) < 0.6 ? root : root + 12;       // argument order: midi first, then vel
                    double vv = 0.5 + 0.1 * rn(rS);
                    ADD(s, m, vv);
                }
            }
        }
#undef ADD
#undef FITS
        st->prevBass = root;
    }
    for (int a = 1; a < n; a++) { BassNote x = out[a]; int j = a - 1; while (j >= 0 && out[j].s > x.s) { out[j + 1] = out[j]; j--; } out[j + 1] = x; }
    return n;
}
