// FIXTURE — the "engine" ANCHOR VOCABULARY for cart-dupes --selfcheck. Never compiled.
// cart-dupes builds its anchor set from every identifier appearing in runtime/*.h, and those
// identifiers stay LITERAL through normalization while cart-local names collapse to "V". That
// is the whole trick that makes it find real clones instead of every `for` loop, so the fixture
// needs its own small, known vocabulary rather than the real 5000-name one.
void note_on(int slot, int midi, float vel);
void note_off(int slot);
void rectfill(int x, int y, int w, int h, int col);
void circfill(int x, int y, int r, int col);
int  ui_button(int x, int y, int w, int h, const char *label);
int  screen_w(void);
int  screen_h(void);
#define PTR_ACQUIRE(pool, id, x, y) 0
#define CLR_WHITE 7
// `N` and `V` as PARAMETER names, on purpose: that is exactly how the normalization sentinels
// reach the real anchor set (runtime/*.h really does declare an `N` param). Without them here,
// `anchor.delete('V'/'N')` is a no-op for this fixture and the sentinel-collision assertion in
// --selfcheck can never fail. Mutation-tested: remove the delete and it goes red.
void set_gain(int N, float V);
