// runtime/loficity/style_guitar.h — ported from styles/guitar.js, verified bit-exact against the oracle.
// ── style: guitar (styles/guitar.js) — clean electric guitar, fingerpicked chords, soft licks ──
// voicing = a real GUITAR shape: a bass note on the low strings + 3-5 upper notes, every candidate
// fingered on six strings in standard tuning (open strings favoured, a <=4-fret span), chosen by cost.
// bass = a finger bass on the kicks + changes with an approach note and the odd fifth.

static const int GTR_OPEN[6] = { 40, 45, 50, 55, 59, 64 };
typedef struct { int open, pos, span; } GtrFing;
typedef struct { int v[8], nv; int b; GtrFing f; int colour; double mean; } GtrShape;   // u = v + 1, nu = nv - 1
typedef struct { GtrShape *s; int n, done; } GtrShapes;
static GtrShapes GTR_CACHE[12 * NQUAL];                        // SHAPES2: global, keyed `${root}:${q}`

// fingering: the best way to fret v on six strings, ascending string per note
typedef struct { const int *v; int n; GtrFing best; int has; } GtrWalk;
static void gtr_walk(GtrWalk *w, int i, int s0, int lo, int hi, int open) {
    if (i == w->n) {
        int pos = hi < 0 ? 0 : lo, span2 = hi < 0 ? 0 : hi - lo;
        if (!w->has || open > w->best.open || (open == w->best.open && span2 + pos * 0.2 < w->best.span + w->best.pos * 0.2)) {
            w->best.open = open; w->best.pos = pos; w->best.span = span2; w->has = 1;
        }
        return;
    }
    for (int s = s0; s <= 6 - (w->n - i); s++) {
        int f = w->v[i] - GTR_OPEN[s];
        if (f < 0 || f > 12) continue;
        if (f == 0) gtr_walk(w, i + 1, s + 1, lo, hi, open + 1);
        else {
            int l = lo < f ? lo : f, h = hi > f ? hi : f;
            if (h - l <= 4) gtr_walk(w, i + 1, s + 1, l, h, open);
        }
    }
}
static int gtr_fingering(const int *v, int n, GtrFing *out) {
    GtrWalk w = { v, n, { 0, 0, 0 }, 0 };
    gtr_walk(&w, 0, 0, 99, -1, 0);
    *out = w.best; return w.has;
}
// combos(a, k) in their order
static int gtr_combos(const int *a, int n, int k, int out[64][6]) {
    if (k == 0) { return 1; }                                  // one empty combo (caller zero-length)
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        int sub[64][6]; int m = gtr_combos(a + i + 1, n - i - 1, k - 1, sub);
        for (int j = 0; j < m; j++) { out[cnt][0] = a[i]; for (int q = 0; q < k - 1; q++) out[cnt][q + 1] = sub[j][q]; cnt++; }
    }
    return cnt;
}
// perms2(a) in their order
static int gtr_perms(const int *a, int n, int out[120][6]) {
    if (n <= 1) { for (int i = 0; i < n; i++) out[0][i] = a[i]; return 1; }
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        int rest[6], rn_ = 0; for (int j = 0; j < n; j++) if (j != i) rest[rn_++] = a[j];
        static int sub[6][120][6]; int (*S)[6] = sub[n - 1];
        int m = gtr_perms(rest, n - 1, S);
        int tmp[120][6]; memcpy(tmp, S, sizeof(int) * 6 * m);
        for (int j = 0; j < m; j++) { out[cnt][0] = a[i]; for (int q = 0; q < n - 1; q++) out[cnt][q + 1] = tmp[j][q]; cnt++; }
    }
    return cnt;
}
static int gtr_arr_has(const int *a, int n, int x) { for (int i = 0; i < n; i++) if (a[i] == x) return 1; return 0; }
static const GtrShapes *gtr_shapes(int root2, int q) {
    GtrShapes *C = &GTR_CACHE[root2 * NQUAL + q];
    if (C->done) return C;
    C->done = 1; C->n = 0; int cap = 256; C->s = malloc(sizeof(GtrShape) * cap);
    int up[8], nup = 0;
    for (int i = 0; i < QUAL[q].n; i++) { int v = QUAL[q].iv[i] % 12; if (v && !gtr_arr_has(up, nup, v)) up[nup++] = v; }
    int third = -1, sev = -1;
    for (int i = 0; i < nup; i++) if (up[i] == 3 || up[i] == 4) { third = up[i]; break; }
    if (third < 0) for (int i = 0; i < nup; i++) if (up[i] == 5 || up[i] == 2) { third = up[i]; break; }
    for (int i = 0; i < nup; i++) if (up[i] == 10 || up[i] == 11) { sev = up[i]; break; }
    if (sev < 0) for (int i = 0; i < nup; i++) if (up[i] == 9) { sev = up[i]; break; }
    int tens[8], nt = 0;
    for (int i = 0; i < QUAL[q].n; i++) if (QUAL[q].iv[i] > 12) tens[nt++] = QUAL[q].iv[i] % 12;
    struct { int s[6], n, top; } sets[80]; int ns = 0;
    for (int k = 3; k <= 4; k++) {
        int cb[64][6]; int m = k <= nup ? gtr_combos(up, nup, k, cb) : 0;
        for (int j = 0; j < m; j++) {
            if (third >= 0 && !gtr_arr_has(cb[j], k, third)) continue;
            if (sev >= 0 && !gtr_arr_has(cb[j], k, sev)) continue;
            memcpy(sets[ns].s, cb[j], sizeof(int) * k); sets[ns].n = k; sets[ns].top = -1; ns++;
        }
    }
    if (nup == 3) {
        int D[3] = { 0, 7, third };
        for (int t = 0; t < 3; t++) { int d = D[t];
            if (d >= 0 && (d == 0 || gtr_arr_has(up, nup, d))) { memcpy(sets[ns].s, up, sizeof(int) * 3); sets[ns].n = 3; sets[ns].top = d; ns++; } }
    }
    int basses[4], nb = 0;
    for (int m = 40 + mod12(root2 - 40); m <= 52; m += 12) basses[nb++] = m;
#define ABOVE(m_, iv_) ((m_) + 1 + mod12(root2 + (iv_) - (m_) - 1))
    int (*seen)[8] = malloc(sizeof(int) * 8 * 8192); int nseen = 0;
    for (int si = 0; si < ns; si++) {
        int pm[120][6]; int np = gtr_perms(sets[si].s, sets[si].n, pm), n = sets[si].n, top = sets[si].top;
        for (int p = 0; p < np; p++) for (int bi = 0; bi < nb; bi++) {
            int b = basses[bi];
            for (int u0 = ABOVE(b + (b < 45 ? 5 : 3) - 1, pm[p][0]); u0 <= 64; u0 += 12) {
                int u[8], nu = 0; u[nu++] = u0;
                for (int i = 1; i < n; i++) { u[nu] = ABOVE(u[nu - 1], pm[p][i]); nu++; }
                if (top >= 0) { u[nu] = ABOVE(u[nu - 1], top); nu++; }
                int t = u[nu - 1];
                int v[8], nv = 0; v[nv++] = b; for (int i = 0; i < nu; i++) v[nv++] = u[i];
                if (t < 57 || t > 76 || t - u[0] > 17) continue;
                int dup = 0;
                for (int z = 0; z < nseen && !dup; z++) if (seen[z][0] == nv && !memcmp(seen[z] + 1, v, sizeof(int) * nv)) dup = 1;
                if (dup) continue;
                int gap = 0; for (int i = 1; i < nu; i++) if (u[i] - u[i - 1] < 2) gap = 1;
                if (gap) continue;
                seen[nseen][0] = nv; memcpy(seen[nseen] + 1, v, sizeof(int) * nv); nseen++;
                GtrFing f; int ok = gtr_fingering(v, nv, &f);
                int colour = 0; for (int i = 0; i < nu; i++) if (gtr_arr_has(tens, nt, mod12(u[i] - root2))) colour = 1;
                if (ok) {
                    if (C->n >= cap) { cap *= 2; C->s = realloc(C->s, sizeof(GtrShape) * cap); }
                    GtrShape *x = &C->s[C->n++];
                    memcpy(x->v, v, sizeof(int) * nv); x->nv = nv; x->b = b; x->f = f; x->colour = colour;
                    int sum = 0; for (int i = 0; i < nu; i++) sum += u[i];
                    x->mean = (double)sum / nu;
                }
            }
        }
    }
#undef ABOVE
    free(seen);
    return C;
}
static double gtr_near(int x, const int *a, int n) { int m = 1 << 30; for (int i = 0; i < n; i++) { int d = abs(x - a[i]); if (d < m) m = d; } return m; }
static double gtr_move(const int *u, int nu, const int *pu, int npu) {
    if (nu == npu) { double s = 0; for (int i = 0; i < nu; i++) s += abs(u[i] - pu[i]); return s; }
    double a = 0, b = 0;
    for (int i = 0; i < nu; i++) a += gtr_near(u[i], pu, npu);
    for (int i = 0; i < npu; i++) b += gtr_near(pu[i], u, nu);
    return (a + b) / 2 + 1;
}
#define GTR_PREVKEY 15                         // st->si[15] = 1 + the shapes key prev came from, 0 = fallback/none
static Voicing gtr_voicing(const Spelled *sp, const Voicing *prev, const Chord *c, const Plan *P, BarState *st) {
    int reg2 = P->reg, root2 = mod12(P->key.tonic + c->root), key = root2 * NQUAL + c->q;
    const GtrShapes *cands = gtr_shapes(root2, c->q);
    double home = 61 + (prev ? 0 : reg2);
    Voicing out;
    if (!cands->n) {
        out.n = 0; out.m[out.n++] = 40 + mod12(P->key.tonic + c->root - 40);
        int tail[8]; for (int i = 0; i < sp->n; i++) tail[i] = 52 + mod12(sp->root + sp->iv[i] - 52);
        isort(tail, sp->n);
        for (int i = 0; i < sp->n; i++) out.m[out.n++] = tail[i];
        st->si[GTR_PREVKEY] = 0;
        return out;
    }
    if (prev && st->si[GTR_PREVKEY] == key + 1) return *prev;   // prev is one of these very shapes (identity)
    const GtrShape *best = &cands->s[0]; double bc = INFINITY;
    for (int ci = 0; ci < cands->n; ci++) {
        const GtrShape *x = &cands->s[ci];
        const int *u = x->v + 1; int nu = x->nv - 1, top = u[nu - 1];
        double k = 0.4 * fabs(x->mean - home) + 0.6 * fmax(0, top - 72) - 0.7 * x->f.open + 0.3 * fmax(0, x->f.pos - 7) +
                   (x->f.span >= 4 ? 1.2 : 0) - (x->colour ? 0.8 : 0);
        if (prev) {
            const int *pu = prev->m + 1; int npu = prev->n - 1;
            k += gtr_move(u, nu, pu, npu) + 0.45 * abs(x->b - prev->m[0]) + 2 * fmax(0, abs(top - pu[npu - 1]) - 3);
        } else k += 0.5 * abs(top - 67 - reg2);
        if (k < bc) { bc = k; best = x; }
    }
    out.n = best->nv; memcpy(out.m, best->v, sizeof(int) * best->nv);
    st->si[GTR_PREVKEY] = key + 1;
    return out;
}

// lickMotif: a guitar lick (steps + legato/pick gaps), its durations, contour and velocities
static const struct { int n; int steps[6]; const char *gaps; } GTR_LICKS[8] = {
    { 4, { 0, 2, 3, 6 }, ".ll" }, { 5, { 2, 4, 6, 7, 10 }, "..ll" }, { 5, { 0, 3, 4, 8, 12 }, ".l.l" },
    { 5, { 4, 6, 7, 10, 16 }, "ll.." }, { 5, { 0, 1, 4, 6, 14 }, "l.l." }, { 5, { 6, 8, 9, 12, 18 }, ".ll." },
    { 6, { 0, 4, 6, 8, 11, 12 }, ".l..l" }, { 4, { 3, 4, 6, 10 }, "ll." },
};
static double gtr_fix(double x, int d) { char buf[64]; snprintf(buf, sizeof buf, "%.*f", d, x); return strtod(buf, NULL); }
static Motif gtr_lick(Rng *r) {
    Motif m; int li = (int)floor(rn(r) * 8); const char *gaps = GTR_LICKS[li].gaps;
    int n2 = GTR_LICKS[li].n; m.n = n2;
    for (int i = 0; i < n2; i++) m.steps[i] = GTR_LICKS[li].steps[i];
    int last = 5 + (int)floor(rn(r) * 5);
    for (int i = 0; i < n2; i++) {
        if (i == n2 - 1) m.durs[i] = last;
        else { int d = m.steps[i + 1] - m.steps[i]; if (d > 6) d = 6;
            m.durs[i] = gaps[i] == 'l' ? d : (d - 2 > 1 ? d - 2 : 1); }
    }
    m.contour[0] = 0;
    int dir = rn(r) < 0.6 ? -1 : 1;
    for (int i = 1; i < n2; i++) {
        int mv;
        if (gaps[i - 1] == 'l') { int a = rn(r) < 0.7 ? 1 : 2; mv = a * (rn(r) < 0.5 ? 1 : -1); }
        else if (rn(r) < 0.8) { int a = 1 + (int)floor(rn(r) * 2); mv = a * (rn(r) < 0.65 ? dir : -dir); }
        else mv = 3 * dir;
        int v = m.contour[i - 1] + mv;
        if (abs(v) > 4) { v = m.contour[i - 1] - mv; dir = -dir; }
        m.contour[i] = v;
    }
    for (int i = 0; i < n2; i++) m.vel[i] = i == 0 ? 0.8 : i == n2 - 1 ? 0.74 : gtr_fix(0.64 + 0.1 * rn(r), 3);
    return m;
}
static void gtr_plan(Plan *P, Rng *r, const Energy *E, int energy) {
    (void)E; (void)energy;
#define RN(a, b, d) rnd2(r, a, b, d)
    kv_set(P, "gtr.drive", RN(1.4, 2.2, 2));
    kv_set(P, "gtr.chorus.rate", RN(0.45, 0.85, 2));
    kv_set(P, "gtr.chorus.cents", RN(5, 9, 1));
    kv_set(P, "gtr.spring", RN(0.1, 0.18, 3));
    kv_set(P, "gtr.pan", RN(-0.12, 0.04, 2));
    double lp = kv(P, "drums.kit.lp");
    kv_del_prefix(P, "drums.kit.");
    kv_set(P, "drums.kit.kickF0", RN(112, 132, 1));
    kv_set(P, "drums.kit.kickF1", RN(44, 51, 1));
    kv_set(P, "drums.kit.kickDecay", RN(0.15, 0.2, 3));
    kv_set(P, "drums.kit.snareF", RN(1500, 2000, 0));
    kv_set(P, "drums.kit.snareDecay", RN(0.09, 0.13, 3));
    kv_set(P, "drums.kit.hatHP", RN(5200, 6800, 0));
    kv_set(P, "drums.kit.hatClosed", RN(0.014, 0.022, 3));
    kv_set(P, "drums.kit.rimF", RN(470, 560, 0));
    kv_set(P, "drums.kit.lp", lp);
    kv_set(P, "bass.lp", RN(380, 480, 0));
#undef RN
    if (P->hasLead) {
        kv_sets(P, "lead.timbre", !strcmp(kvs(P, "lead.timbre"), "soft") ? "neck" : "bright");
        double pan = gtr_fix((0.14 + rn(r) * 0.14) * (kv(P, "gtr.pan") > -0.04 ? -1 : 1), 2);
        P->leadPan = pan; kv_set(P, "lead.pan", pan);
        P->motif = gtr_lick(r);
    }
}
// bassLine: on the kicks + the changes (only in "kick" mode), an approach into a new root, the odd fifth
static int gtr_bass(BassCtx *cx, BassNote *out) {
    if (cx->mode != BS_KICK) return -1;
    const Plan *P = cx->plan; BarState *st = cx->st; Rng *r = &st->rS;
    int E = P->energy;
    static const double PA[3] = { 0.25, 0.35, 0.45 }, PF[3] = { 0.3, 0.4, 0.45 };
    const Chord *chords = cx->chords; int nch = cx->nch;
    int changes[14]; for (int k = 0; k < nch; k++) changes[k] = (int)(chords[k].start * 4);
    const Chord *last = &chords[nch - 1], *nx = cx->nnext ? &cx->nextChords[0] : NULL;
    int steps[64], ns = 0;
    for (int k = 0; k < cx->nkicks; k++) if (!gtr_arr_has(steps, ns, cx->kicks[k])) steps[ns++] = cx->kicks[k];
    for (int k = 0; k < nch; k++) if (!gtr_arr_has(steps, ns, changes[k])) steps[ns++] = changes[k];
    isort(steps, ns);
    int approach = 0;
#define PC(c_) mod12(P->key.tonic + (c_)->root)
    if (nx && PC(nx) != PC(last) && rn(r) < PA[E]) {
        int k2 = 0; for (int k = 0; k < ns; k++) if (steps[k] < 14) steps[k2++] = steps[k]; ns = k2;
        steps[ns++] = 14; approach = 1;
    }
    if (cx->stop >= 0) { int k2 = 0; for (int k = 0; k < ns; k++) if (steps[k] < cx->stop) steps[k2++] = steps[k]; ns = k2; }
    int end = cx->stop >= 0 ? cx->stop : 16;
    for (int k = 0; k < ns; k++) {
        int s = steps[k];
        const Chord *c = last;
        for (int q = 0; q < nch; q++) if (s >= chords[q].start * 4 && s < (chords[q].start + chords[q].beats) * 4) { c = &chords[q]; break; }
        int root2 = bass_note(PC(c), st->prevBass), len = (k + 1 < ns ? steps[k + 1] : end) - s;
        int midi = root2, slide = 0;
        double vel = s == 0 ? 0.92 : s % 4 ? 0.74 : 0.84, dur = len * P->bassFactor;
        if (approach && s == 14) {
            int tg = bass_note(PC(nx), root2); double u = rn(r);
            midi = u < 0.4 ? tg - 1 : u < 0.6 ? tg + 1 : tg - 5 >= 33 ? tg - 5 : tg + 7;
            vel = 0.72;
            slide = rn(r) < 0.3;
            dur = fmin(len, 1.6);
        } else if (!gtr_arr_has(changes, nch, s) && s % 8 && rn(r) < PF[E]) {
            int f = qual_has(c->q, 7) ? 7 : qual_has(c->q, 6) ? 6 : 7;
            midi = rn(r) < 0.85 ? (root2 + f <= 50 ? root2 + f : root2 + f - 12) : (root2 + 12 <= 52 ? root2 + 12 : root2);
        } else st->prevBass = root2;
        out[k].s = s; out[k].midi = midi; out[k].vel = vel; out[k].dur = fmax(0.5, dur); out[k].slide = slide;
    }
#undef PC
    return ns;
}
