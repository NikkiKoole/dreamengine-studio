/* de:meta
{
  "title": "1. hello screen",
  "status": "active",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Your very first cart. Clear the screen and print some text."
}
de:meta */
#include "studio.h"

void draw() {
    cls(CLR_DARK_BLUE);
    print("hello, world!", 10, 10, CLR_YELLOW);
    print("welcome to dreamengine.", 10, 26, CLR_WHITE);
    print("edit this code and hit run.", 10, 42, CLR_LIGHT_GREY);
}
