// runtime/loficity/style_medieval.h — ported from styles/medieval.js, verified bit-exact against the oracle.
// ── style: medieval (styles/medieval.js) — lute, recorder, old modal tunes over a soft hand-drum beat ──
// plan = the MODE (dorian/aeolian/mixolydian/ionian read off the progressions), a drone, a Picardy third,
// ornament odds, the viol's pattern, a frame-drum kit and a folk motif for the recorder;
// voicing = a four-course lute shape (bass + three upper courses) chosen by cost;
// bass = the viol: a held root, root + fifth, or root + fifth + a modal step into the next chord.

enum { MED_IONIAN, MED_DORIAN, MED_AEOLIAN, MED_MIXOLYDIAN };
static const char *MED_MODE_NAME[4] = { "ionian", "dorian", "aeolian", "mixolydian" };
static const int MED_MODES[4][7] = {
    { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 5, 7, 9, 10 },
};
static const char *MED_VIOL_NAME[3] = { "root", "fifth", "walk" };

static int med_mode_of(const Plan *P) {
    const char *m = kvs(P, "medieval.mode");
    for (int i = 0; i < 4; i++) if (!strcmp(m, MED_MODE_NAME[i])) return i;
    return MED_DORIAN;
}
// pickW4 over a {root, fifth, walk} table, in its own key order
static int med_pick_w(Rng *r, const double *w, int n) {
    double sum = 0; for (int i = 0; i < n; i++) sum += w[i];
    double u = rn(r) * sum;
    for (int i = 0; i < n; i++) if ((u -= w[i]) < 0) return i;
    return 0;
}
static int med_qual_has(int q, int iv) { for (int i = 0; i < QUAL[q].n; i++) if (QUAL[q].iv[i] == iv) return 1; return 0; }

// tonesOf: the lute's intervals — the medieval triads as they are, anything else reduced to one
static int med_tones_of(const Chord *c, const Plan *P, int *iv) {
    static const int MED[3] = { 0, 4, 7 }, MEDM[3] = { 0, 3, 7 }, MED5[2] = { 0, 7 }, SUS2[3] = { 0, 2, 7 }, SUS4[3] = { 0, 5, 7 };
    const int *t; int n = 3;
    if (c->q == Q_MED) t = MED;
    else if (c->q == Q_MEDM) t = MEDM;
    else if (c->q == Q_MED5) { t = MED5; n = 2; }
    else if (c->q == Q_MEDSUS2) t = SUS2;
    else if (c->q == Q_MEDSUS4) t = SUS4;
    else if (c->fin && !P->key.major) t = kv(P, "medieval.picardy") ? MED : MEDM;
    else if (med_qual_has(c->q, 4)) t = MED;
    else if (med_qual_has(c->q, 3)) t = MEDM;
    else { t = MED5; n = 2; }
    memcpy(iv, t, sizeof(int) * n);
    return n;
}
static const int MED_ORD3[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
static const int MED_ORD2[2][3] = { { 1, 0, 1 }, { 0, 1, 0 } };
// candidates: every [bass, u0, u1, u2] lute shape for this chord, in their loop order
static int med_candidates(int root, const int *tones, int nt, int out[][4]) {
    int pcs[3], basses[4], nb = 0, n = 0;
    for (int i = 0; i < nt; i++) pcs[i] = mod12(root + tones[i]);
    for (int m = 41 + mod12(root - 41); m <= 53; m += 12) basses[nb++] = m;
    int no = nt == 3 ? 6 : 2;
    for (int o = 0; o < no; o++) {
        const int *order = nt == 3 ? MED_ORD3[o] : MED_ORD2[o];
        for (int u0 = 50 + mod12(pcs[order[0]] - 50); u0 <= 66; u0 += 12) {
            int u[3] = { u0 };
            for (int k = 1; k < 3; k++) { int m = u[k - 1] + 2; m += mod12(pcs[order[k]] - m); u[k] = m; }
            if (u[2] < 60 || u[2] > 72 || u[2] - u[0] > 14 || (u[0] < 55 && u[1] - u[0] < 3)) continue;
            for (int q = 0; q < nb; q++) {
                int b = basses[q];
                if (u[0] - b >= (b < 47 ? 5 : 3)) { out[n][0] = b; out[n][1] = u[0]; out[n][2] = u[1]; out[n][3] = u[2]; n++; }
            }
        }
    }
    return n;
}
static Voicing med_voicing(const Spelled *sp, const Voicing *prev, const Chord *c, const Plan *P, BarState *st) {
    (void)sp; (void)st;
    int root = mod12(P->key.tonic + c->root), tones[3], nt = med_tones_of(c, P, tones);
    int home = 63 + P->reg;
    if (prev && prev->n == 4 && mod12(prev->m[0]) == root) {       // the same chord: keep the shape
        int want[3], nw = 0, have[4], nh = 0;
        for (int i = 0; i < nt; i++) { int p = mod12(root + tones[i]), d = 0; for (int j = 0; j < nw; j++) if (want[j] == p) d = 1; if (!d) want[nw++] = p; }
        for (int i = 0; i < 4; i++) { int p = mod12(prev->m[i]), d = 0; for (int j = 0; j < nh; j++) if (have[j] == p) d = 1; if (!d) have[nh++] = p; }
        if (nw == nh) {
            int ok = 1;
            for (int i = 0; i < nw && ok; i++) { int f = 0; for (int j = 0; j < nh; j++) if (have[j] == want[i]) f = 1; if (!f) ok = 0; }
            if (ok) return *prev;
        }
    }
    int cand[64][4], nc = med_candidates(root, tones, nt, cand), best = -1; double bc = INFINITY;
    for (int x = 0; x < nc; x++) {
        int b = cand[x][0], u0 = cand[x][1], u1 = cand[x][2], u2 = cand[x][3];
        double k = 0.5 * fabs((u0 + u1 + u2) / 3.0 - home) + 0.3 * abs(b - 47);
        if (prev && prev->n == 4)
            k += abs(u0 - prev->m[1]) + abs(u1 - prev->m[2]) + abs(u2 - prev->m[3]) + 0.4 * abs(b - prev->m[0]) + 2 * fmax(0, abs(u2 - prev->m[3]) - 3);
        if (k < bc) { bc = k; best = x; }
    }
    Voicing v; v.n = 4;
    if (best >= 0) { for (int i = 0; i < 4; i++) v.m[i] = cand[best][i]; return v; }
    int t[4], n = 0;
    for (int i = 0; i < nt; i++) t[n++] = tones[i];
    t[n++] = 12;
    v.m[0] = 41 + mod12(root - 41);
    int u[3]; for (int i = 0; i < 3; i++) u[i] = 55 + mod12(root + t[i] - 55);
    isort(u, 3);
    for (int i = 0; i < 3; i++) v.m[1 + i] = u[i];
    return v;
}

// violLine — the viol under the lute
static int med_bass(BassCtx *cx, BassNote *out) {
    const Plan *P = cx->plan; BarState *st = cx->st; const BarInfo *b = cx->b;
    int n = 0, end = cx->stop >= 0 ? cx->stop : 16;
#define ADD(s_, m_, v_, d_) do { double s__ = (s_); double d__ = (d_); \
        if (s__ < end) { out[n].s = (int)s__; out[n].midi = (m_); out[n].vel = (v_); out[n].dur = fmin(d__, cx->stop >= 0 ? end - s__ : d__); out[n].slide = 0; n++; } } while (0)
#define PC(c) mod12(P->key.tonic + (c)->root)
    if (cx->mode == BS_WHOLE || cx->mode == BS_LAST || (cx->mode == BS_OUTRO && b->j == 2)) {
        for (int k = 0; k < cx->nch; k++) {
            const Chord *c = &cx->chords[k];
            int m = bass_note(PC(c), st->prevBass);
            ADD(c->start * 4, m, 0.74, cx->mode == BS_OUTRO ? 28 : c->beats * 4 * 0.96);
            st->prevBass = m;
        }
        return n;
    }
    int pat;
    if (rn(&st->rS) < 0.2) { static const double ALT[3] = { 1, 1, 0.3 }; pat = med_pick_w(&st->rS, ALT, 3); }
    else { const char *vs = kvs(P, "medieval.viol"); pat = 0; for (int q = 0; q < 3; q++) if (!strcmp(vs, MED_VIOL_NAME[q])) pat = q; }
    for (int k = 0; k < cx->nch; k++) {
        const Chord *c = &cx->chords[k];
        double c0 = c->start * 4, c1 = c0 + c->beats * 4;
        int root = bass_note(PC(c), st->prevBass);
        int fifth = med_qual_has(c->q, 7) ? (root + 7 <= 50 ? root + 7 : root - 5) : root;
        int last = k == cx->nch - 1;
        double v0 = c->onset ? 0.8 : 0.62;
        if (pat == 0 || c1 - c0 < 16) ADD(c0, root, v0, (c1 - c0) * 0.96);
        else if (pat == 1 || !last || !cx->nextChords) { ADD(c0, root, v0, 7.6); ADD(8, fifth, 0.7, 7.6); }
        else {
            ADD(c0, root, v0, 7.6); ADD(8, fifth, 0.7, 3.8);
            int target = bass_note(PC(&cx->nextChords[0]), root);
            const int *sc = MED_MODES[med_mode_of(P)];
#define INMODE(m) ({ int p_ = mod12((m) - P->key.tonic), f_ = 0; for (int q_ = 0; q_ < 7; q_++) if (sc[q_] == p_) f_ = 1; f_; })
            int a = target + (rn(&st->rS) < 0.5 ? -1 : 1);
            if (!INMODE(a)) a = target + (a < target ? -2 : 2);
            if (a != target && INMODE(a)) ADD(12, a, 0.62, 3.8);
#undef INMODE
        }
        st->prevBass = root;
    }
#undef PC
#undef ADD
    return n;
}

// folkMotif — the recorder's tune: a dance cell + a tail, stepwise, strong on the beat
static Motif med_folk_motif(Rng *r) {
    static const int CELL[7][6] = { { 0, 3, 4, 8 }, { 0, 4, 6, 8, 12 }, { 0, 6, 8, 12 }, { 2, 4, 6, 8, 14 }, { 0, 3, 4, 6, 8 }, { 0, 2, 4, 8, 11, 12 }, { 0, 4, 8, 10, 12 } };
    static const int CELLN_[7] = { 4, 5, 4, 5, 5, 6, 5 };
    static const int TAIL[6][3] = { { 0 }, { 16 }, { 16, 20 }, { 16, 19, 20 }, { 20 }, { 16, 22 } };
    static const int TAILN_[6] = { 0, 1, 2, 3, 1, 2 };
    Motif m; memset(&m, 0, sizeof m); int n = 0;
    int ci = rfloor(r, 7);
    for (int i = 0; i < CELLN_[ci]; i++) m.steps[n++] = CELL[ci][i];
    int ti = rfloor(r, 6);
    for (int i = 0; i < TAILN_[ti]; i++) m.steps[n++] = TAIL[ti][i];
    if (n > 7) n = 7;
    m.n = n;
    int last = 5 + rfloor(r, 6);
    for (int i = 0; i < n; i++) m.durs[i] = i < n - 1 ? (6 < m.steps[i + 1] - m.steps[i] ? 6 : m.steps[i + 1] - m.steps[i]) : last;
    m.contour[0] = 0;
    int dir = rn(r) < 0.65 ? 1 : -1, back = 0;
    for (int i = 1; i < n; i++) {
        double u = rn(r); int mv;
        if (back) { mv = -back; back = 0; }
        else if (u < 0.7) mv = dir;
        else if (u < 0.9) mv = 2 * dir;
        else { mv = 2 * dir; back = dir; }
        m.contour[i] = m.contour[i - 1] + mv;
        if (abs(m.contour[i]) >= 3) dir = m.contour[i] > 0 ? -1 : 1;
    }
    for (int i = 0; i < n; i++) m.vel[i] = i == 0 ? 0.8 : m.steps[i] % 4 == 0 ? 0.72 : 0.62 + 0.06 * rn(r);
    return m;
}

static void med_plan(Plan *P, Rng *r, const Energy *E, int energy) {
    (void)energy;
    int minor = !P->key.major;
    // pcsOf: every pitch class (relative to the key) the progressions sound
    int all[12] = { 0 }, A[12] = { 0 };
    for (int i = 0; i < P->progA.n; i++) for (int k = 0; k < QUAL[P->progA.c[i].q].n; k++) { int p = mod12(P->progA.c[i].root + QUAL[P->progA.c[i].q].iv[k]); all[p] = 1; A[p] = 1; }
    for (int i = 0; i < P->progB.n; i++) for (int k = 0; k < QUAL[P->progB.c[i].q].n; k++) all[mod12(P->progB.c[i].root + QUAL[P->progB.c[i].q].iv[k])] = 1;
    int x = minor ? 9 : 10, y = minor ? 8 : 11;
    double coin = rn(r);
#define HAS(s, p, q) ((s)[p] && !(s)[q])
    int useX = HAS(all, x, y) || (!HAS(all, y, x) && (HAS(A, x, y) || (!HAS(A, y, x) && coin < 0.5)));
#undef HAS
    int droneOk = !all[1] && !all[11], fifthOk = !all[6] && !all[8];
    int wantDrone = rn(r) < E->x[ST_MEDIEVAL_X_DRONE].v[0][0];
    kv_sets(P, "medieval.mode", MED_MODE_NAME[minor ? (useX ? MED_DORIAN : MED_AEOLIAN) : (useX ? MED_MIXOLYDIAN : MED_IONIAN)]);
    if (wantDrone && droneOk) {
        kv_set(P, "medieval.drone.0", 0);
        if (fifthOk) kv_set(P, "medieval.drone.1", 7);
    } else kv_sets(P, "medieval.drone", "null");
    kv_set(P, "medieval.picardy", minor && rn(r) < 0.6);
    kv_set(P, "medieval.orn", rnd2(r, E->x[ST_MEDIEVAL_X_ORNAMENT].v[0][0], E->x[ST_MEDIEVAL_X_ORNAMENT].v[0][1], 3));
    {
        const XRange *V = &E->x[ST_MEDIEVAL_X_VIOL]; double w[3]; int nw = V->n;
        for (int k = 0; k < nw; k++) w[k] = V->v[k][0];
        int pick = med_pick_w(r, w, nw);
        kv_sets(P, "medieval.viol", MED_VIOL_NAME[pick]);
    }
    kv_set(P, "viol.lp", rnd2(r, 700, 1000, 0));
    double lp = kv(P, "drums.kit.lp");
    kv_del_prefix(P, "drums.kit.");
    kv_set(P, "drums.kit.kickF0", rnd2(r, 100, 120, 1));
    kv_set(P, "drums.kit.kickF1", rnd2(r, 58, 68, 1));
    kv_set(P, "drums.kit.kickDecay", rnd2(r, 0.16, 0.24, 3));
    kv_set(P, "drums.kit.slapBP", rnd2(r, 1300, 1800, 0));
    kv_set(P, "drums.kit.slapDecay", rnd2(r, 0.04, 0.06, 3));
    kv_set(P, "drums.kit.jingle", rnd2(r, 0.05, 0.08, 3));
    kv_set(P, "drums.kit.lp", lp);
    if (P->hasLead) {
        kv_sets(P, "lead.timbre", !strcmp(kvs(P, "lead.timbre"), "soft") ? "recorder" : "wood");
        P->motif = med_folk_motif(r);
    }
}
