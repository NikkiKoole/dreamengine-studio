// pmbreed.h — cart-land face of the patch-match VECTOR + the mutation operator.
//
// tools/patch-match/pmpatch.h owns the representation (PmPatch, the 0..1 mapping,
// the measured detents) and now also pm_mutate(). This header exists so a cart
// can #include it through the usual -I runtime path (pmpatch.h itself lives next
// to the CLI) and APPLY a mutated patch to a slot.
//
// apply writes EVERY dial, including the ones it wants OFF. studio.h: redefining
// a slot does not clear instrument_env/_lfo/_filter/_drive/_level or the sends —
// leftovers of the previous pad would otherwise leak into the next child.
//
// No scoring. No second engine instance. The cart plays what you pick.
// patchbench is the customer (docs/design/patch-matching-cart.md option B).
#ifndef PMBREED_H
#define PMBREED_H

#include "../tools/patch-match/pmpatch.h"

static inline void pm_apply_slot(const PmPatch *p, int s)
{
    instrument(s, p->engine, pm_atk_ms(p->v[V_ATK]), pm_dec_ms(p->v[V_DEC]),
               pm_sus(p->v[V_SUS]), pm_rel_ms(p->v[V_REL]));
    instrument_harmonics(s, p->v[V_HARM]);
    instrument_timbre(s, p->v[V_TIMB]);
    instrument_morph(s, p->v[V_MORPH]);

    int fmode = PM_FILTER_BIN[pm_bin(p->v[V_FMODE], 4)];
    instrument_filter(s, fmode, pm_cut_hz(p->v[V_CUT]), pm_res(p->v[V_RES]));
    instrument_keytrack(s, 0.0f);

    instrument_env(s, 0, ENV_CUTOFF_OCT, 0, pm_env_ms(p->v[V_ENVDEC]), pm_env_oct(p->v[V_ENVAMT]));
    instrument_env(s, 1, ENV_PITCH, 0, 0, 0.0f);
    instrument_env(s, 2, ENV_TIMBRE, 0, 0, 0.0f);
    instrument_lfo(s, 0, LFO_PITCH,  pm_vib_hz(p->v[V_VIBRATE]), pm_vib_semi(p->v[V_VIBDEP]));
    instrument_lfo(s, 1, LFO_VOLUME, pm_vib_hz(p->v[V_VIBRATE]), p->v[V_TREMDEP]);
    instrument_lfo(s, 2, LFO_CUTOFF, 1.0f, 0.0f);
    instrument_follow(s, LFO_CUTOFF, 0, 0, 0.0f);

    int midx[PM_NMODE];
    int nm = pm_engine_modes(p->engine, midx);
    if (nm > p->nmode) nm = p->nmode;
    for (int i = 0; i < nm; i++) instrument_mode(s, midx[i], p->v[V_MODE0 + i]);

    instrument_level(s, 1.0f);
    instrument_pan(s, 0.0f);
    instrument_tune(s, 0.0f);
    instrument_glide(s, 0);
    instrument_duty(s, p->v[V_DUTY]);
    instrument_unison(s, pm_unison_n(p->v[V_UNISON]), pm_detune_st(p->v[V_DETUNE]));
    instrument_sync(s, pm_sync_ratio(p->v[V_SYNC]));
    instrument_bandlimit(s, pm_bandlimit_on(p->v[V_BANDLIMIT]));
    instrument_drive(s, p->v[V_DRIVE]);
    instrument_drive_mode(s, pm_bin(p->v[V_DRIVEMODE], 4));

    const float *f = p->f;
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

#endif
