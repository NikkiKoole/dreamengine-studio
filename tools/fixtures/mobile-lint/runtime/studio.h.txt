// FIXTURE — a stand-in studio.h. mobile-lint SKIPS studio.h on purpose: it is declarations only,
// and its prototypes name every input function, so inlining it would make EVERY cart look
// touch-ready. This file is deliberately full of those prototypes to prove the skip works.
int  touch_count(void);
int  touch_x(int i);
int  touch_y(int i);
int  tap(int x, int y, int w, int h);
int  btn(int player, int button);
int  key(int code);
int  mouse_down(int b);
int  mouse_wheel(void);
int  text_input(char *buf, int cap);
