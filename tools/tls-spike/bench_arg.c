/* variant A — OPTION (c): same context struct, but `ctx` is a real parameter. The boundary
 * resolves it once; every internal function takes it. Signatures change, the DSP text does not. */
#include "common.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
#define DECL(t, n)         t n;
#define DECL_ARR(t, n, s)  t n[s];
#include "state.h"
#undef DECL
#undef DECL_ARR
} Ctx;

static _Thread_local Ctx *tls_ctx;   /* still needed AT THE DOOR: the public API cannot grow a
                                      * parameter without changing every cart. Option (c) is
                                      * "resolve once here, then pass it down". */
#include "access.h"                  /* identical to option (b) — `ctx` is just in scope differently */

#define CTX_DECL   Ctx *ctx,         /* every internal signature grows this */
#define CTX_PASS   ctx,              /* every internal call site grows this */
#define CTX_LOCAL  Ctx *ctx = tls_ctx;   /* the boundary, resolved exactly once per call */

#include "body.h"

void SETUP(void) {
    if (!tls_ctx) tls_ctx = calloc(1, sizeof(Ctx));
    memset(tls_ctx, 0, sizeof(Ctx));
    Ctx *ctx = tls_ctx;
#include "reset.h"
}
