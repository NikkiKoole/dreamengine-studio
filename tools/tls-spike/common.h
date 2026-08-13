/* common.h — knobs shared by the three variants (set from the compile line by run.sh). */
#ifndef TLS_SPIKE_COMMON_H
#define TLS_SPIKE_COMMON_H

/* INLINE_OK=1 lets clang inline the stage functions (best case: the context lookup gets hoisted
 * out of the loop). The real sound.h stages are far too big for that, so the default models them
 * as opaque calls, which is where option (b) actually pays. */
#if defined(INLINE_OK) && INLINE_OK
#define NOINLINE
#else
#define NOINLINE __attribute__((noinline))
#endif

#ifndef ENTRY
#define ENTRY run_variant
#endif
#ifndef SETUP
#define SETUP setup_variant
#endif

#endif
