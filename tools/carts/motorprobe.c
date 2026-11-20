/* de:meta
{
  "slug": "motorprobe",
  "title": "motor voice probe",
  "status": "active",
  "created": "2026-11-20",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "sonification"
  ],
  "lineage": "Measurement probe for INSTR_MOTOR (the cylinder + exhaust engine voice). Rendered headless and read back against CC0 engine recordings (tools/refs/engines-cc0.txt); not played by hand.",
  "description": "Holds one INSTR_MOTOR engine at a fixed rpm and load, chosen at build time: DE_DEFINES=LAYOUT=3,RPM=1400,LOAD=0.3,EXH=0.3 (plus ROUGH=, PIPE=, SWEEP=1 for a slow rev from idle to 6000 rpm and back). Render it with `node tools/play.js motorprobe script /dev/null --headless --frames 300 --wav out.wav`."
}
de:meta */
// motorprobe: one held INSTR_MOTOR, configured by -D flags, for measurement.
#include "studio.h"
#include <math.h>

#ifndef LAYOUT
#define LAYOUT 3
#endif
#ifndef RPM
#define RPM 1400
#endif
#ifndef LOAD
#define LOAD 0.3f
#endif
#ifndef EXH
#define EXH 0.3f
#endif
#ifndef ROUGH
#define ROUGH 0.5f
#endif
#ifndef INTAKE
#define INTAKE 0.5f
#endif
#ifndef MECH
#define MECH 0.5f
#endif
#ifndef PIPE
#define PIPE 0.5f
#endif

static int h = -1, tick = 0;
static float rpm_midi(float rpm) { return 69.0f + 12.0f * log2f(rpm / 60.0f / 440.0f); }

void init(void) {
#ifdef SAW
    instrument(1, INSTR_SAW, 20, 0, 7, 200);
#else
    instrument(1, INSTR_MOTOR, 20, 0, 7, 200);
#endif
    instrument_harmonics(1, LAYOUT / 6.0f);
    instrument_timbre(1, EXH);
    instrument_morph(1, LOAD);
    instrument_mode(1, MODE_MOTOR_ROUGH, ROUGH);
    instrument_mode(1, MODE_MOTOR_PIPE, PIPE);
    instrument_mode(1, MODE_MOTOR_INTAKE, INTAKE);
    instrument_mode(1, MODE_MOTOR_MECH, MECH);
}

void update(void) {
    tick++;
#ifndef VOL
#define VOL 7
#endif
    if (tick == 2) h = note_on((int)rpm_midi(RPM), 1, VOL);
    if (h >= 0) {
#ifdef SWEEP
        float t = (float)tick / 60.0f, ph = t < 4.0f ? t / 4.0f : (t < 8.0f ? 1.0f - (t - 4.0f) / 4.0f : 0.0f);
        note_pitch(h, rpm_midi(800.0f + ph * 5200.0f));
        note_morph(h, ph > 0.0f && t < 4.0f ? 0.9f : 0.15f);
#else
        note_pitch(h, rpm_midi(RPM));
#endif
    }
}

void draw(void) {
    cls(CLR_BLACK);
    print("motorprobe", 4, 4, CLR_WHITE);
}
