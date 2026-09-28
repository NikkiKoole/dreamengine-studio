// runtime/loficity/style_bossa.h — ported from styles/bossa.js, verified bit-exact against the oracle.
// ── style: bossa (styles/bossa.js) — nylon guitar and a straight bossa nova groove ──
// voicing = a guitar grip: a thumb BASS under three upper strings, chosen by cost from every
// permutation in register (a held-over chord re-grips with the thumb on its FIFTH, the "alt" set);
// bass = the upright's bossa figure: root on 1, fifth (or the next chord) on 3, a pickup on the "and"
// of 4 that may tie over the barline into the next bar.

// upperSet: the quality minus its roots; drop the 5th if more than 3, then drop the second-to-last
static int bos_upper(int q, int *u) {
    int n = 0, has7 = 0;
    for (int i = 0; i < QUAL[q].n; i++) { int v = QUAL[q].iv[i]; if (v % 12 != 0) { u[n++] = v; if (v == 7) has7 = 1; } }
    if (n > 3 && has7) { int k = 0; for (int i = 0; i < n; i++) if (u[i] != 7) u[k++] = u[i]; n = k; }
    while (n > 3) { int at = n - 2; for (int i = at; i < n - 1; i++) u[i] = u[i + 1]; n--; }
    return n;
}
// altSet: [0, third, seventh] when the quality has a 5th, a 3rd and a 6th/7th — else null (returns 0)
static int bos_alt(int q, int *u) {
    int third = -1, sev = -1, has7 = 0;
    for (int i = 0; i < QUAL[q].n; i++) {
        int v = QUAL[q].iv[i];
        if (v == 7) has7 = 1;
        if (third < 0 && (v == 3 || v == 4)) third = v;
        if (sev < 0 && v >= 9 && v <= 11) sev = v;
    }
    if (!(has7 && third >= 0 && sev >= 0)) return 0;
    u[0] = 0; u[1] = third; u[2] = sev; return 3;
}
static const int BOS_PERMS[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
typedef struct { int m[4]; } BosCand;
// candidates: every [bass, u0, u1, u2] grip (a set shorter than 3 yields none, as NaN does in theirs)
static int bos_candidates(int root, int q, int alt, BosCand *out) {
    int up[7], nu = alt ? bos_alt(q, up) : bos_upper(q, up), n = 0;
    if (!nu || nu < 3) return 0;
    int pcs[3]; for (int i = 0; i < 3; i++) pcs[i] = mod12(root + up[i]);
    int bpc = alt ? root + 7 : root, basses[4], nbs = 0;
    for (int m = 40 + mod12(bpc - 40); m <= 52; m += 12) basses[nbs++] = m;
    for (int p = 0; p < 6; p++)
        for (int base = 52; base <= 66; base += 12) {
            int u[3], nuu = 0;
            for (int j = 0; j < 3; j++) {
                int m = nuu ? u[nuu - 1] + 1 : base;
                m += mod12(pcs[BOS_PERMS[p][j]] - m);
                u[nuu++] = m;
            }
            if (u[0] > 66 || u[2] > 74 || u[2] - u[0] > 14) continue;
            for (int k = 0; k < nbs; k++) {
                int b = basses[k];
                if (u[0] - b >= (b < 45 ? 5 : 3) && u[2] - b <= 29) { BosCand c = { { b, u[0], u[1], u[2] } }; out[n++] = c; }
            }
        }
    return n;
}
static Voicing bos_voicing(const Spelled *sp, const Voicing *prev, const Chord *c, const Plan *P, BarState *st) {
    (void)sp; (void)st;
    int root = mod12(P->key.tonic + c->root);
    BosCand cands[128]; int nc = 0;
    if (!c->onset) nc = bos_candidates(root, c->q, 1, cands);
    if (!nc) nc = bos_candidates(root, c->q, 0, cands);
    double home = 64 + (prev ? 0 : P->reg);
    int best = -1; double bc = INFINITY;
    for (int i = 0; i < nc; i++) {
        int b = cands[i].m[0], u0 = cands[i].m[1], u1 = cands[i].m[2], u2 = cands[i].m[3];
        double k = 0.5 * fabs((u0 + u1 + u2) / 3.0 - home) + 0.35 * abs(b - 46) + (u1 - u0 == 1 ? 8 : 0) + (u2 - u1 == 1 ? 8 : 0);
        if (prev && prev->n == 4)
            k += abs(u0 - prev->m[1]) + abs(u1 - prev->m[2]) + abs(u2 - prev->m[3]) + 0.4 * abs(b - prev->m[0]) + 2 * fmax(0, abs(u2 - prev->m[3]) - 4);
        if (k < bc) { bc = k; best = i; }
    }
    Voicing v;
    if (best >= 0) { v.n = 4; memcpy(v.m, cands[best].m, sizeof cands[best].m); return v; }
    int up[7], nu = bos_upper(c->q, up), t[7];
    for (int i = 0; i < nu; i++) t[i] = 52 + mod12(root + up[i] - 52);
    isort(t, nu);
    v.n = 1 + nu; v.m[0] = 40 + mod12(root - 40); memcpy(v.m + 1, t, sizeof(int) * nu);
    return v;
}
static int bos_qhas(int q, int iv) { for (int i = 0; i < QUAL[q].n; i++) if (QUAL[q].iv[i] == iv) return 1; return 0; }
static int bos_fifth(int q, int m) {
    int f2 = bos_qhas(q, 7) ? 7 : bos_qhas(q, 6) ? 6 : bos_qhas(q, 8) ? 8 : 7;
    return m + f2 <= 47 ? m + f2 : m + f2 - 12;
}
// chordAt(s): the index of the first chord covering step s, else the last
static int bos_chord_at(const BassCtx *cx, int s) {
    for (int k = 0; k < cx->nch; k++) if (s >= cx->chords[k].start * 4 && s < (cx->chords[k].start + cx->chords[k].beats) * 4) return k;
    return cx->nch - 1;
}
#define BOS_TIE st->si[0]           // st.bossaTie (a bar index)
#define BOS_TIE_SET st->si[1]       // … and whether it has ever been set (theirs starts undefined)
static int bos_bass(BassCtx *cx, BassNote *out) {
    const Plan *P = cx->plan; BarState *st = cx->st; int n = 0;
#define PC(c) mod12(P->key.tonic + (c)->root)
#define ADD(s_, m_, v_, d_, sl_) ({ int s__ = (s_); int ok__ = cx->stop < 0 || s__ < cx->stop; \
        if (ok__) { out[n].s = s__; out[n].midi = (m_); out[n].vel = (v_); out[n].dur = (d_); out[n].slide = (sl_); n++; } ok__; })
    if (cx->mode == BS_WHOLE || cx->mode == BS_LAST || (cx->mode == BS_OUTRO && cx->b->j == 2)) {
        for (int k = 0; k < cx->nch; k++) {
            const Chord *c = &cx->chords[k];
            int m = bass_note(PC(c), st->prevBass);
            ADD((int)(c->start * 4), m, 0.8, cx->mode == BS_OUTRO ? 28 : c->beats * 4 * 0.95, 0);
            st->prevBass = m;
        }
        return n;
    }
    int E = P->energy; double f = P->bassFactor; Rng *r = &st->rS;
    int i0 = bos_chord_at(cx, 0), i8 = bos_chord_at(cx, 8), change = i8 != i0;
    const Chord *c0 = &cx->chords[i0], *c8 = &cx->chords[i8];
    int r0 = bass_note(PC(c0), st->prevBass), three = change ? bass_note(PC(c8), r0) : bos_fifth(c0->q, r0);
    if (!(BOS_TIE_SET && BOS_TIE == cx->i)) ADD(0, r0, 0.9, 6 * f, 0);
    static const double P6[3] = { 0.35, 0.6, 0.75 }, P14[3] = { 0.45, 0.65, 0.8 };
    if (rn(r) < P6[E]) ADD(6, three, 0.66, 1.8, 0);
    ADD(8, three, 0.84, 6 * f, 0);
    st->prevBass = three;
    const Chord *nx = cx->nextChords && cx->nnext ? &cx->nextChords[0] : NULL;
    if (rn(r) < P14[E]) {
        int target2 = nx ? bass_note(PC(nx), three) : r0;
        int approach = nx && PC(nx) != PC(c8) && rn(r) < 0.3;
        int tie = !approach && rn(r) < 0.5;
        int m = approach ? target2 + (rn(r) < 0.6 ? -1 : 1) : target2;
        double dur = tie ? 2 + 6 * f : 1.8;
        int slide = !approach && rn(r) < 0.08;
        if (ADD(14, m, 0.64, dur, slide) && tie) { BOS_TIE = cx->i + 1; BOS_TIE_SET = 1; }
        st->prevBass = target2;
    }
#undef ADD
#undef PC
    return n;
}
#undef BOS_TIE
#undef BOS_TIE_SET
static double bos_rnd(Rng *r, double a, double b) { return rnd2(r, a, b, 3); }
static void bos_plan(Plan *P, Rng *r, const Energy *E, int energy) {
    (void)E; (void)energy;
    double lp = kv(P, "drums.kit.lp");
    kv_del_prefix(P, "drums.kit.");
    kv_set(P, "drums.kit.kickF0", rnd2(r, 95, 120, 1));
    kv_set(P, "drums.kit.kickF1", rnd2(r, 45, 53, 1));
    kv_set(P, "drums.kit.kickDecay", bos_rnd(r, 0.16, 0.22));
    kv_set(P, "drums.kit.brush", bos_rnd(r, 0.1, 0.16));
    kv_set(P, "drums.kit.hatHP", rnd2(r, 7000, 9000, 0));
    kv_set(P, "drums.kit.hatClosed", bos_rnd(r, 0.009, 0.015));
    kv_set(P, "drums.kit.rimF", rnd2(r, 520, 620, 0));
    kv_set(P, "drums.kit.shaker", bos_rnd(r, 0.03, 0.045));
    kv_set(P, "drums.kit.lp", lp);
    kv_set(P, "bass.lp", rnd2(r, 460, 600, 0));
    if (P->hasLead) kv_sets(P, "lead.timbre", !strcmp(kvs(P, "lead.timbre"), "soft") ? "flute" : "nylon");
}
