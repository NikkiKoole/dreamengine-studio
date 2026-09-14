// Render one sustained note from an STK instrument to a raw f32 stream on stdout.
#include "Stk.h"
#include "Brass.h"
#include "Flute.h"
#include "Clarinet.h"
#include "Bowed.h"
#include "ModalBar.h"
#include "TubeBell.h"
#include "Rhodey.h"
#include "Wurley.h"
#include "HevyMetl.h"
#include "BeeThree.h"
#include "PercFlut.h"
#include "FMVoices.h"
#include <cstdio>
#include <cstring>
#include <string>
using namespace stk;
int main(int argc, char **argv) {
  std::string which = argc > 1 ? argv[1] : "Brass";
  double freq = argc > 2 ? atof(argv[2]) : 220.0;
  double amp = argc > 3 ? atof(argv[3]) : 0.8;
  // argv[4] is per-instrument: lip pressure CC for Brass, modal PRESET index for ModalBar
  // (0 marimba, 1 vibraphone, 2 agogo, 3 wood1, 4 reso, 5 wood2, 6 beats, 7 twofix, 8 clump).
  double lip = argc > 4 ? atof(argv[4]) : -1;
  Stk::setSampleRate(44100.0);
  Stk::setRawwavePath("stk/rawwaves/");
  Instrmnt *inst = 0;
  if (which == "Brass")    inst = new Brass();
  else if (which == "Flute")    inst = new Flute(50.0);
  else if (which == "Clarinet") inst = new Clarinet();
  else if (which == "Bowed")    inst = new Bowed();
  else if (which == "ModalBar")  inst = new ModalBar();
  // The FOUR-OPERATOR FM references. All seven derive from STK's FM class, whose constructor
  // takes `unsigned int operators = 4`, and each is a named TX81Z algorithm.
  else if (which == "TubeBell")  inst = new TubeBell();
  else if (which == "Rhodey")    inst = new Rhodey();
  else if (which == "Wurley")    inst = new Wurley();
  else if (which == "HevyMetl")  inst = new HevyMetl();
  else if (which == "BeeThree")  inst = new BeeThree();
  else if (which == "PercFlut")  inst = new PercFlut();
  else if (which == "FMVoices")  inst = new FMVoices();
  else { fprintf(stderr, "unknown %s\n", which.c_str()); return 1; }
  // ModalBar's preset must be selected BEFORE the strike: setPreset() rewrites every mode's
  // ratio, radius and gain, plus stick hardness and strike position. Setting it after noteOn
  // would re-voice a bar that is already ringing.
  if (which == "ModalBar") inst->controlChange(16, lip >= 0 ? lip : 0);
  const int SR = 44100, N = SR * 7;
  for (int i = 0; i < N; i++) {
    double t = (double)i / SR;
    if (i == (int)(0.5 * SR)) {
      inst->noteOn(freq, amp);
      if (lip >= 0 && which != "ModalBar") inst->controlChange(2, lip);
    }
    if (i == (int)(6.0 * SR)) inst->noteOff(0.5);
    float s = (float)inst->tick();
    fwrite(&s, sizeof(float), 1, stdout);
    (void)t;
  }
  return 0;
}
