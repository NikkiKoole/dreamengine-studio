/* variant T — OPTION (b): state lives in a context struct, reached through a _Thread_local
 * pointer. No function signature changes anywhere; every function resolves the context itself. */
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

static _Thread_local Ctx *tls_ctx;

#define ctx tls_ctx        /* <- the ONLY difference from option (c) */
#include "access.h"

#define CTX_DECL           /* signatures unchanged */
#define CTX_PASS           /* nothing threaded through calls */
#define CTX_LOCAL          /* the boundary does not resolve it; every function does */

#include "body.h"

void SETUP(void) {
    if (!tls_ctx) tls_ctx = calloc(1, sizeof(Ctx));   /* allocate ONCE, so trials measure the loop
                                                        and not the allocator */
    memset(tls_ctx, 0, sizeof(Ctx));
#include "reset.h"
}
