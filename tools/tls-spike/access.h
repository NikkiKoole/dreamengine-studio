/*
 * access.h — THE MACRO BLOCK. This is the entire trick, and the honest answer to "are we hiding
 * this behind a macro?": yes, and this is all of it. One line per variable, mapping the name the
 * DSP code already uses onto a member of the context struct.
 *
 * In the real refactor this block is 293 lines for sound.h, GENERATED from the same clang AST
 * that `node tools/engine-statics.js` reads, so it cannot drift from the struct.
 *
 * Note what is NOT here: `ctx` itself. Each variant defines that — as a thread-local (option b)
 * or as a function parameter (option c) — and this block is identical either way. That is why
 * the choice between them is reversible, and can be made per function later.
 */
#define f_cut        (ctx->f_cut)
#define f_res        (ctx->f_res)
#define f_z1         (ctx->f_z1)
#define f_z2         (ctx->f_z2)
#define echo_fb      (ctx->echo_fb)
#define echo_tone    (ctx->echo_tone)
#define echo_z       (ctx->echo_z)
#define echo_pos     (ctx->echo_pos)
#define cho_depth    (ctx->cho_depth)
#define cho_ph       (ctx->cho_ph)
#define drive_amt    (ctx->drive_amt)
#define drive_tone   (ctx->drive_tone)
#define sc_env       (ctx->sc_env)
#define sc_ratio     (ctx->sc_ratio)
#define master_gain  (ctx->master_gain)
#define pan_law_x    (ctx->pan_law_x)
#define echo_buf     (ctx->echo_buf)
#define vphase       (ctx->vphase)
#define vfreq        (ctx->vfreq)
