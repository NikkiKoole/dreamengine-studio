// Render one sustained note from an STK instrument to a raw f32 stream on stdout.
#include "Stk.h"
#include "Brass.h"
#include "Flute.h"
#include "Clarinet.h"
#include "Bowed.h"
#include <cstdio>
#include <cstring>
#include <string>
using namespace stk;
int main(int argc, char **argv) {
  std::string which = argc > 1 ? argv[1] : "Brass";
  double freq = argc > 2 ? atof(argv[2]) : 220.0;
  double amp = argc > 3 ? atof(argv[3]) : 0.8;
  double lip = argc > 4 ? atof(argv[4]) : -1;
  Stk::setSampleRate(44100.0);
  Stk::setRawwavePath("stk/rawwaves/");
  Instrmnt *inst = 0;
  if (which == "Brass")    inst = new Brass();
  else if (which == "Flute")    inst = new Flute(50.0);
  else if (which == "Clarinet") inst = new Clarinet();
  else if (which == "Bowed")    inst = new Bowed();
  else { fprintf(stderr, "unknown %s\n", which.c_str()); return 1; }
  const int SR = 44100, N = SR * 7;
  for (int i = 0; i < N; i++) {
    double t = (double)i / SR;
    if (i == (int)(0.5 * SR)) { inst->noteOn(freq, amp); if (lip >= 0) inst->controlChange(2, lip); }
    if (i == (int)(6.0 * SR)) inst->noteOff(0.5);
    float s = (float)inst->tick();
    fwrite(&s, sizeof(float), 1, stdout);
    (void)t;
  }
  return 0;
}
