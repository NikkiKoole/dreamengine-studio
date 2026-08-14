// Does an opted-in cart-land header get PER-INSTANCE state?
// Built twice by run-uictx.sh: plain, and with -DDE_CART_CTX. Checking BOTH matters — a seam that
// only works when enabled is half a seam, and the default path is what all 553 carts compile.
// de:engine-owner multi — this probe EXISTS to run several engines at once
#include <stdio.h>
#include <stdint.h>
#include "../../runtime/platform.h"
extern int probe_write, probe_seen;
extern int cart_write, cart_seen, cart_boot;
int main(void) {
    DeInstance *a = de_instance_create(DE_RENDERER_SOFTWARE);
    DeInstance *b = de_instance_create(DE_RENDERER_SOFTWARE);
    de_frame(a, 0);  de_frame(b, 0);                 // both exist, both have run a frame
    probe_write = 7; de_frame(a, 1/60.0);            // write 7 into A's ui_wid_n
    de_frame(a, 2/60.0); int seen_a = probe_seen;    // read A's back
    de_frame(b, 2/60.0); int seen_b = probe_seen;    // and B's

    // the same handshake for the CART's own state, which takes the other shape (template copy)
    cart_write = 9; de_frame(a, 3/60.0);
    de_frame(a, 4/60.0); int cart_a = cart_seen;
    de_frame(b, 4/60.0); int cart_b = cart_seen, boot_b = cart_boot;

    int ok;
#ifdef DE_CART_CTX
    printf("▸ ui.h OPTED IN (-DDE_CART_CTX)\n     A reads %d, B reads %d\n", seen_a, seen_b);
    ok = (seen_a == 7 && seen_b != 7);
    printf(ok ? "  \033[32m✓\033[0m per-instance: B does not see A's write\n"
              : "  \033[31m✗\033[0m STILL SHARED: B sees %d\n", seen_b);
    printf("▸ the CART's OWN state (the ctx-gen --target cart shape)\n     A reads %d, B reads %d\n", cart_a, cart_b);
    int c1 = (cart_a == 9 && cart_b != 9);
    printf(c1 ? "  \033[32m✓\033[0m per-instance: B does not see A's write\n"
              : "  \033[31m✗\033[0m STILL SHARED: B sees %d\n", cart_b);
    // The half a single-instance gate cannot see: de_state_for returns ZEROED memory, so an
    // instance that never copied the template would boot with every default at 0 and still render
    // a perfectly valid — and completely wrong — rack.
    int c2 = (boot_b == 42);
    printf(c2 ? "  \033[32m✓\033[0m and it inherited the compile-time defaults  — probe_boot = %d, not 0\n"
              : "  \033[31m✗\033[0m the template was NOT copied: probe_boot = %d (should be 42)\n", boot_b);
    ok = ok && c1 && c2;
#else
    printf("▸ ui.h DEFAULT (what all 553 carts compile)\n     A reads %d, B reads %d\n", seen_a, seen_b);
    ok = (seen_a == 7 && seen_b == 7);
    printf(ok ? "  \033[32m✓\033[0m shared, exactly as today — the default path is unchanged\n"
              : "  \033[31m✗\033[0m the DEFAULT path changed behaviour: B reads %d\n", seen_b);
    printf("▸ the CART's OWN state (the ctx-gen --target cart shape)\n     A reads %d, B reads %d\n", cart_a, cart_b);
    int c1 = (cart_a == 9 && cart_b == 9);
    printf(c1 ? "  \033[32m✓\033[0m shared, exactly as today\n"
              : "  \033[31m✗\033[0m the DEFAULT path changed behaviour: B reads %d\n", cart_b);
    int c2 = (boot_b == 42);
    printf(c2 ? "  \033[32m✓\033[0m and the compile-time default is still there  — probe_boot = %d\n"
              : "  \033[31m✗\033[0m probe_boot = %d (should be 42)\n", boot_b);
    ok = ok && c1 && c2;
#endif
    printf("\n%s\n", ok ? "PASS" : "FAILED");
    return ok ? 0 : 1;
}
