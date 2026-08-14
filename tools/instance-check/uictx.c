// Does an opted-in cart-land header get PER-INSTANCE state?
// Built twice by run-uictx.sh: plain, and with -DDE_CART_CTX. Checking BOTH matters — a seam that
// only works when enabled is half a seam, and the default path is what all 553 carts compile.
#include <stdio.h>
#include <stdint.h>
#include "../../runtime/platform.h"
extern int probe_write, probe_seen;
int main(void) {
    DeInstance *a = de_instance_create(DE_RENDERER_SOFTWARE);
    DeInstance *b = de_instance_create(DE_RENDERER_SOFTWARE);
    de_frame(a, 0);  de_frame(b, 0);                 // both exist, both have run a frame
    probe_write = 7; de_frame(a, 1/60.0);            // write 7 into A's ui_wid_n
    de_frame(a, 2/60.0); int seen_a = probe_seen;    // read A's back
    de_frame(b, 2/60.0); int seen_b = probe_seen;    // and B's
#ifdef DE_CART_CTX
    printf("▸ ui.h OPTED IN (-DDE_CART_CTX)\n     A reads %d, B reads %d\n", seen_a, seen_b);
    int ok = (seen_a == 7 && seen_b != 7);
    printf(ok ? "  \033[32m✓\033[0m per-instance: B does not see A's write\n"
              : "  \033[31m✗\033[0m STILL SHARED: B sees %d\n", seen_b);
    printf("\n%s\n", ok ? "PASS" : "FAILED");
    return ok ? 0 : 1;
#else
    printf("▸ ui.h DEFAULT (what all 553 carts compile)\n     A reads %d, B reads %d\n", seen_a, seen_b);
    int ok = (seen_a == 7 && seen_b == 7);
    printf(ok ? "  \033[32m✓\033[0m shared, exactly as today — the default path is unchanged\n"
              : "  \033[31m✗\033[0m the DEFAULT path changed behaviour: B reads %d\n", seen_b);
    printf("\n%s\n", ok ? "PASS" : "FAILED");
    return ok ? 0 : 1;
#endif
}
