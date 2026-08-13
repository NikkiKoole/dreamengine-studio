/* variant P — THE ENGINE TODAY: state is file-scope statics, nothing is passed anywhere. */
#include "common.h"
#include <string.h>

#define DECL(t, n)         static t n;
#define DECL_ARR(t, n, s)  static t n[s];
#include "state.h"
#undef DECL
#undef DECL_ARR

#define CTX_DECL           /* no context parameter */
#define CTX_PASS           /* nothing to pass */
#define CTX_LOCAL          /* nothing to resolve */

#include "body.h"

void SETUP(void) {
#include "reset.h"
}
