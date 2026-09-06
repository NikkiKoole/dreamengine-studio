// pmcart.c — the CART half of the patch matcher. It exists only so the host can
// reach the sound API at all: instrument()/hit() resolve against a THREAD-LOCAL
// current instance that is bound by de_frame, so a candidate has to be built
// from inside update(). Same handshake shape as tools/state-check/statecart.c.
//
// ⚠ THE TRAP THIS FILE IS WRITTEN AROUND. studio.h, on instrument():
//     "REDEFINING a slot does NOT clear what else was attached to it: instrument_env/
//      _lfo/_filter/_drive/_level/_pan and the sends all SURVIVE."
// A search re-uses ONE slot for thousands of candidates. If apply() only set the
// dials a candidate happens to use, every candidate would inherit the leftovers
// of the last one — the score would depend on evaluation ORDER, the same patch
// would score differently twice, and the printed snippet (a fresh slot in a real
// cart) would not reproduce what was rendered. So pm_apply writes EVERY dial,
// every time, including the ones it wants OFF. Silence is set, never assumed.
#include "studio.h"
#include "../../tools/patch-match/pmpatch.h"

// ── handshake with the host (plain globals; the host only touches them between frames)
PmPatch pm_req;
int pm_fire     = 0;      // host: 1 = apply pm_req and strike
int pm_silence  = 0;      // host: 1 = kill every sounding voice
int pm_midi     = 60;     // note to strike
int pm_hold_ms  = 1000;   // how long to hold it (hit() schedules its own note-off, in SAMPLES)
int pm_vol      = 5;      // 0..7

static void pm_apply(const PmPatch *p)
{
    const int s = PM_SLOT;

    // amp envelope + waveform
    instrument(s, p->engine, pm_atk_ms(p->v[V_ATK]), pm_dec_ms(p->v[V_DEC]),
               pm_sus(p->v[V_SUS]), pm_rel_ms(p->v[V_REL]));

    // the three engine macros
    instrument_harmonics(s, p->v[V_HARM]);
    instrument_timbre(s, p->v[V_TIMB]);
    instrument_morph(s, p->v[V_MORPH]);

    // filter
    int fmode = PM_FILTER_BIN[pm_bin(p->v[V_FMODE], 4)];
    instrument_filter(s, fmode, pm_cut_hz(p->v[V_CUT]), pm_res(p->v[V_RES]));
    instrument_keytrack(s, 0.0f);

    // modulation — all three envelopes and all three LFOs written every time,
    // amount/depth 0 where unused (see the header note).
    instrument_env(s, 0, ENV_CUTOFF_OCT, 0, pm_env_ms(p->v[V_ENVDEC]), pm_env_oct(p->v[V_ENVAMT]));
    instrument_env(s, 1, ENV_PITCH, 0, 0, 0.0f);
    instrument_env(s, 2, ENV_TIMBRE, 0, 0, 0.0f);
    instrument_lfo(s, 0, LFO_PITCH,  pm_vib_hz(p->v[V_VIBRATE]), pm_vib_semi(p->v[V_VIBDEP]));
    instrument_lfo(s, 1, LFO_VOLUME, pm_vib_hz(p->v[V_VIBRATE]), p->v[V_TREMDEP]);
    instrument_lfo(s, 2, LFO_CUTOFF, 1.0f, 0.0f);
    instrument_follow(s, LFO_CUTOFF, 0, 0, 0.0f);

    // Per-engine structure dials. Only the ones this patch OWNS are written: a
    // MODE_* the search is not moving must keep the ENGINE's default, because
    // some of these are thresholds and no value here is neutral (pmpatch.h).
    // Safe because each candidate gets a fresh instance, so nothing is inherited.
    int midx[4];
    int nm = pm_engine_modes(p->engine, midx);
    if (nm > p->nmode) nm = p->nmode;
    for (int i = 0; i < nm; i++) instrument_mode(s, midx[i], p->v[V_MODE0 + i]);

    // level/pan/tuning: pinned, not searched. Level is normalized away by the loss
    // and pan would make a mono comparison meaningless.
    instrument_level(s, 1.0f);
    instrument_pan(s, 0.0f);
    instrument_tune(s, 0.0f);
    instrument_glide(s, 0);
    instrument_duty(s, 0.5f);
    instrument_unison(s, 1, 0.0f);
    instrument_bandlimit(s, 0);

    // ── fx. Written every time too, bypassed (mix/amount 0) unless stage 3 opened them.
    const float *f = p->f;
    instrument_drive(s, f[F_DRIVE]);
    instrument_drive_mode(s, pm_bin(f[F_DRIVEMODE], 4));
    instrument_tape(s, f[F_TAPEWOW], f[F_TAPEFLUT], f[F_TAPESAT]);
    instrument_crush(s, pm_crush_bits(f[F_CRUSHBITS]), pm_crush_rate(f[F_CRUSHRATE]), f[F_CRUSHMIX]);
    instrument_chorus(s, pm_ch_rate(f[F_CHRATE]), f[F_CHDEP], f[F_CHMIX]);
    instrument_tremolo(s, pm_trem_rate(f[F_TREMRATE]), f[F_TREMDEP], LFO_SHAPE_SINE);
    instrument_eq(s, pm_eq_db(f[F_EQLOW]), pm_eq_db(f[F_EQMID]), pm_eq_db(f[F_EQHIGH]));
    echo(pm_echo_ms(f[F_ECHOTIME]), pm_echo_fb(f[F_ECHOFB]), f[F_ECHOTONE]);
    instrument_echo(s, f[F_ECHOSEND]);
    reverb(f[F_RVBSIZE], f[F_RVBDAMP]);
    instrument_reverb(s, f[F_RVBSEND]);
}

void init(void) { }

void update(void)
{
    if (pm_silence) { note_off_all(); pm_silence = 0; }
    if (pm_fire)    { pm_apply(&pm_req); hit(pm_midi, PM_SLOT, pm_vol, pm_hold_ms); pm_fire = 0; }
}

void draw(void) { }
