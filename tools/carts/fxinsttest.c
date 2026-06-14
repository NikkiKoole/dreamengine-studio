// fxinsttest — verification cart (throwaway): confirm FX_INST instances route through the new
// 5-bit-kind / 3-bit-instance fx_order packing. A sine, driven ONLY on FX_DRIVE instance 1 (instance
// 0 left off), placed via FX_INST(FX_DRIVE, 1). If the instance routes, the sine saturates.
#include "studio.h"

#define I_S 0

void init(void) {
    instrument(I_S, INSTR_SINE, 1, 0, 7, 50);
    instrument_env(I_S, 0, ENV_CUTOFF, 0, 0, 0.0f);
    drive_insert(0.0f, DRIVE_HARD, 0.0f);           // instance 0 OFF
    drive_insert_inst(1, 0.9f, DRIVE_HARD, 1.0f);   // instance 1 hard-drives
    int chain[] = { FX_INST(FX_DRIVE, 1) };         // only instance 1 in the chain
    fx_order(0, chain, 1);
    note_on(74, I_S, 6);
}
void update(void) {}
void draw(void) { cls(0); print("fxinsttest: sine -> FX_INST(FX_DRIVE,1)", 8, 8, 7); }
