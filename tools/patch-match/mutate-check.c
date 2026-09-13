// mutate-check.c — known answers for pm_mutate(), no engine.
//
//   clang tools/patch-match/mutate-check.c -I tools/patch-match -lm -o build/pm-mutate
//   ./build/pm-mutate
//
// pmpatch.h is pure float math. This file exists so a broken operator cannot
// hide behind a cart that failed to compile (the same reason detents.c --check
// does not go through play.js).
#include <stdio.h>
#include "pmpatch.h"

int main(void)
{
    int n = pm_mutate_selfcheck();
    if (n) { printf("pm_mutate: %d failed\n", n); return 1; }
    printf("pm_mutate: ok\n");
    return 0;
}
