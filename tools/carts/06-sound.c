/* de:meta
{
  "title": "6. sounds and music",
  "status": "active",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Make noise with note(), bpm(), and every(). A simple beat and melody."
}
de:meta */
#include "studio.h"

void update() {
    bpm(110);

    // a simple four-note melody, one note per beat
    int step = beat() % 4;
    if (every(1)) {
        int melody[] = { 60, 64, 67, 69 };  // C E G A
        note(melody[step], INSTR_TRI, 4);
    }

    // kick on beat 1, snare on beat 3
    if (every(4))                note(36, INSTR_NOISE, 5);
    if (every(4) && beat()%4==2) note(48, INSTR_NOISE, 3);
}

void draw() {
    cls(CLR_DARK_PURPLE);
    print("sounds and music.", 4, 4, CLR_WHITE);
    print("bpm sets the tempo.", 4, 20, CLR_LIGHT_GREY);
    print("every(1) fires once per beat.", 4, 30, CLR_LIGHT_GREY);
    print("note(midi, instrument, vol)", 4, 40, CLR_LIGHT_GREY);

    // beat visualiser
    int b = beat() % 4;
    int notes[] = { 60, 64, 67, 69 };
    char *names[] = { "C", "E", "G", "A" };
    for (int i = 0; i < 4; i++) {
        int col = (i == b) ? CLR_YELLOW : CLR_DARK_GREY;
        rectfill(20 + i * 72, 80, 60, 60, col);
        print(names[i], 44 + i * 72, 106, CLR_BLACK);
    }

    print(str("beat %d", beat()), 4, 160, CLR_LIGHT_GREY);
}
