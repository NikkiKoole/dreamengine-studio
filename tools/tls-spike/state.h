/*
 * state.h — the "engine state" the benchmark loop reads and writes.
 *
 * Listed once, as DECL() entries, so the SAME list becomes either a block of file-scope statics
 * (the engine today) or the members of a context struct (the refactor). That is exactly the
 * mechanical move the real refactor makes to runtime/sound.h's 293 statics, in miniature.
 */
DECL(float, f_cut)        DECL(float, f_res)
DECL(float, f_z1)         DECL(float, f_z2)
DECL(float, echo_fb)      DECL(float, echo_tone)
DECL(float, echo_z)       DECL(int,   echo_pos)
DECL(float, cho_depth)    DECL(float, cho_ph)
DECL(float, drive_amt)    DECL(float, drive_tone)
DECL(float, sc_env)       DECL(float, sc_ratio)
DECL(float, master_gain)  DECL(float, pan_law_x)
DECL_ARR(float, echo_buf, 4096)
DECL_ARR(float, vphase,   8)
DECL_ARR(float, vfreq,    8)
