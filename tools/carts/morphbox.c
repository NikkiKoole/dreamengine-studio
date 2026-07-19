/* de:meta
{
  "slug": "morphbox",
  "title": "Morphbox",
  "resizable": true,
  "status": "active",
  "created": "2026-07-19",
  "kind": ["instrument", "tech-demo"],
  "genre": null,
  "teaches": ["step-sequencer"],
  "lineage": "The groovebox transport (self-playing grid + summed-bus sidechain pump) crossed with a NEW honest core: runtime/morphdrum.h, where each of three drum voices is ONE parametric synth and the 808/909 are just two points in it — a CHARACTER knob morphs the 808↔909 STRUCTURE while a deep panel of absolute knobs shapes the sound past both machines (electro→rock). Complements acidcandy (which keeps 808/909 as separate faces); this is the continuum, exposed as knobs.",
  "description": {
    "summary": "A three-voice drum synth where every drum is a MORPH, not a preset: focus KICK, SNARE or HAT and it fills two rows of ~10 knobs — CHARACTER slides the 808↔909 voicing while TUNE/DEC/PUNCH/DRIVE/CUT/SUB… carve the sound well past both machines. A 3-row step grid plays itself; real summed-bus PUMP off the kick.",
    "detail": "The honest core is runtime/morphdrum.h — an 808 kick and a 909 kick are the SAME synthesis structure at different numbers, so each voice is one parametric model with a full knob panel. Tap a voice name (left of the grid) to FOCUS it; the two knob rows up top edit that voice. CHAR morphs the STRUCTURAL 808↔909 voicing (click brightness, noise mode, FM metal floor); the rest are absolute controls with ranges that reach beyond both (sub-deep TUNE, endless DEC, PUNCH pitch-sweep, DRIVE into distortion). SWG shuffles the whole grid; PUMP is a real master sidechain keyed off the kick.",
    "controls": "Tap/drag grid cells to paint steps (each finger paints). Tap a voice NAME to focus it, then drag the top knobs (vertical = value, double-tap = reset). Top row's 6th knob = SWG, 2nd row's 6th = PUMP. BPM readout, PLAY/STOP top-right."
  }
}
de:meta */
#include "studio.h"
#define UI_KNOB_R 6              // small candy knobs for the 160x100 face
#define UI_KNOB_DRAG_PX 40       // shorter full-sweep drag to suit the tiny screen
#include "ui.h"
#include "morphdrum.h"   // the honest core: one parametric voice per role, 808↔909 as endpoints
#include <stdio.h>       // snprintf (the BPM readout)

// MORPHBOX — a three-voice drum box on the idea that the 808 and the 909 are two ends
// of ONE knob. morphdrum.h owns the synthesis (a deep parametric panel per voice); this
// owns the pattern, the transport, and the humble candy surface.

#define ROWS  MD_NV      // 3 voices
#define STEPS 16

#define GX 20            // grid left edge (name column is 1..GX-2)
#define GY 61            // grid top
#define SX 8             // column stride
#define SY 12            // row stride
#define CW 7             // cell size
#define CH 10

static MorphKit kit;
static bool grid[ROWS][STEPS];
static bool hopen[STEPS];        // HAT row: this step is an OPEN hat (long, choked by the next closed)

static const int LIT[ROWS] = { CLR_RED, CLR_ORANGE, CLR_YELLOW };

// a default groove that shows the box off the moment it loads
static const char *PRESET[ROWS] = {
    "x...x...x...x...",   // KICK — four on the floor
    "....x.......x...",   // SNARE — backbeat
    "..x.x.x.x.x.x.x.",   // HAT — running off-beats
};

// the MORPH PAD (left): CHAR × TUNE of the focused voice, with the 808/909 landmarks
#define PADX 3
#define PADY 14
#define PADW 42
#define PADH 40
// knob columns to the RIGHT of the pad: four voice knobs + one global (col 4)
static const int KX[5] = { 56, 78, 100, 122, 144 };
#define KY1 21           // knob row 1 centre
#define KY2 43           // knob row 2 centre

static int  focus    = 0;        // which voice the top knob rows edit
static int  cur_step = 0;
static int  last_16  = -1;
static int  flash[ROWS];
static bool playing  = true;
static int  g_bpm    = 124;
static float k_swing = 0.0f;     // global shuffle 0..1
static float k_pump  = 0.55f;    // master sidechain depth

typedef struct { int id, paint, lastR, lastC; } Ptr;   // id MUST be first
static Ptr ptr[8];

static void fire_row(int r, int delay) {
    if (r == MD_HAT && hopen[cur_step]) morph_fire_open(&kit, 0, delay);
    else                                morph_fire(&kit, r, 0, delay);
    flash[r] = frame();
}

void init(void) {
    morph_build(&kit, 9);        // voice slots 9..9+MD_NSLOT-1

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < STEPS; c++)
            grid[r][c] = (PRESET[r][c] == 'x');

    sidechain_key(kit.base + MDS_KICK, 0, 1.0f);   // the kick body IS the pump trigger

    for (int i = 0; i < 8; i++) ptr[i].id = -1;
}

static int cell_rc(int x, int y, int *r, int *c) {
    if (x < GX || y < GY) return 0;
    int cc = (x - GX) / SX, rr = (y - GY) / SY;
    if (cc < 0 || cc >= STEPS || rr < 0 || rr >= ROWS) return 0;
    if (x > GX + cc * SX + CW || y > GY + rr * SY + CH) return 0;
    *r = rr; *c = cc; return 1;
}

void update(void) {
    bpm(g_bpm);
    morph_ride(&kit);            // rebuild only the voices whose knobs moved (set-and-hold)
#ifdef DE_TRACE
    watch("focus", "%d", focus);
#endif

    static float aPump = -1;
    if (k_pump != aPump) { sidechain(0, 0, k_pump, 1, 180); aPump = k_pump; }

    if (playing) {
        float pos16 = beat() * 4 + beat_pos() * 4.0f;
        int sixteenth = (int)pos16;
        if (sixteenth != last_16) {
            last_16  = sixteenth;
            cur_step = sixteenth % STEPS;
            int sw = (cur_step & 1) ? (int)(k_swing * 0.6f * (15000.0f / g_bpm)) : 0;
            for (int r = 0; r < ROWS; r++)
                if (grid[r][cur_step]) fire_row(r, sw);
        }
    }

    if (btnp(0, BTN_UP))    focus = (focus + ROWS - 1) % ROWS;
    if (btnp(0, BTN_DOWN))  focus = (focus + 1) % ROWS;
    if (btnp(1, BTN_LEFT))  g_bpm = g_bpm > 70  ? g_bpm - 2 : g_bpm;
    if (btnp(1, BTN_RIGHT)) g_bpm = g_bpm < 200 ? g_bpm + 2 : g_bpm;

    // voice focus: ANY finger resting on a name cell picks that voice. Checked every
    // frame (not just on touch-down) so it's robust to the down-frame position lag that
    // a mouse click / quick tap has — the old down-frame-only check missed those.
    for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= 1 && tx < GX - 2 && ty >= GY && ty < GY + ROWS * SY)
            focus = (ty - GY) / SY;
    }

    // MORPH PAD drag: a finger inside the pad sets the focused voice's CHAR (x) + TUNE (y)
    for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i);
        if (tx >= PADX && tx < PADX + PADW && ty >= PADY && ty < PADY + PADH) {
            float cx = (float)(tx - PADX) / (PADW - 1);
            float cy = (float)(ty - PADY) / (PADH - 1);
            kit.p[focus][MD_CHAR] = cx < 0 ? 0 : cx > 1 ? 1 : cx;
            kit.p[focus][MD_TUNE] = 1.0f - (cy < 0 ? 0 : cy > 1 ? 1 : cy);
        }
    }

    // ── touch/mouse: paint cells; each finger carries its own paint value ──
    for (int i = 0; i < touch_count(); i++) {
        int tx = touch_x(i), ty = touch_y(i), tid = touch_id(i);
        int slot = -1;
        for (int j = 0; j < 8; j++) if (ptr[j].id == tid) { slot = j; break; }
        if (slot < 0) {
            for (int j = 0; j < 8; j++) if (ptr[j].id < 0) { slot = j; break; }
            if (slot < 0) continue;
            ptr[slot].id = tid; ptr[slot].lastR = ptr[slot].lastC = -1;
            int r, c;
            if (cell_rc(tx, ty, &r, &c)) {
                if (r == MD_HAT) {                  // hat cell CYCLES: off → closed → open → off
                    if (!grid[r][c])      { grid[r][c] = true;  hopen[c] = false; }   // → closed
                    else if (!hopen[c])   { hopen[c] = true; }                        // → open
                    else                  { grid[r][c] = false; hopen[c] = false; }   // → off
                    ptr[slot].paint = grid[r][c] ? 1 : 0;
                    if (grid[r][c]) {               // audition what it just became
                        if (hopen[c]) morph_fire_open(&kit, 0, 0); else morph_fire(&kit, MD_HAT, 0, 0);
                        flash[r] = frame();
                    }
                } else {
                    ptr[slot].paint = !grid[r][c];
                    grid[r][c] = ptr[slot].paint;
                    if (ptr[slot].paint) fire_row(r, 0);
                }
                ptr[slot].lastR = r; ptr[slot].lastC = c;
            } else ptr[slot].paint = -1;   // off the grid — the name cells are ui buttons (see draw)
        } else {
            int r, c;
            if (ptr[slot].paint >= 0 && cell_rc(tx, ty, &r, &c) &&
                (r != ptr[slot].lastR || c != ptr[slot].lastC)) {
                grid[r][c] = ptr[slot].paint;
                if (r == MD_HAT) hopen[c] = false;   // dragging paints CLOSED hats (tap to cycle open)
                if (ptr[slot].paint) fire_row(r, 0);
                ptr[slot].lastR = r; ptr[slot].lastC = c;
            }
        }
    }
    for (int j = 0; j < 8; j++) {
        if (ptr[j].id < 0) continue;
        bool live = false;
        for (int i = 0; i < touch_count(); i++) if (touch_id(i) == ptr[j].id) { live = true; break; }
        if (!live) ptr[j].id = -1;
    }
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    font(FONT_SMALL);            // 4x6 — the 160x100 face needs the compact font
    ui_begin();

    // ── header: focused voice · BPM · PLAY ──
    print(MD_NAME[focus], 3, 2, LIT[focus]);
    char bt[16]; snprintf(bt, sizeof bt, "%d BPM", g_bpm);
    print(bt, 40, 2, CLR_LIGHT_GREY);
    if (ui_button(SCREEN_W - 34, 0, 32, 9, playing ? "STOP" : "PLAY")) playing = !playing;

    // ── the MORPH PAD (left): CHAR × TUNE of the focused voice, 808/909 landmarks ──
    {
        int x = PADX, y = PADY, w = PADW, h = PADH;
        float ch = kit.p[focus][MD_CHAR], tu = kit.p[focus][MD_TUNE];
        int px = x + (int)(ch * (w - 1)), py = y + (int)((1.0f - tu) * (h - 1));
        rectfill(x, y, w, h, CLR_DARKER_GREY);
        rect(x, y, w, h, CLR_MEDIUM_GREY);
        line(x + 1, py, x + w - 2, py, CLR_DARK_GREY);        // crosshair
        line(px, y + 1, px, y + h - 2, CLR_DARK_GREY);
        print("TUNE", x + 2, y + 1, CLR_MEDIUM_GREY);          // Y axis
        print("808", x + 2, y + h - 7, CLR_MEDIUM_GREY);       // X axis: char 0 → 808 …
        print("909", x + w - 14, y + h - 7, CLR_MEDIUM_GREY);  // … char 1 → 909
        circfill(px, py, 2, LIT[focus]);
        circ(px, py, 2, CLR_WHITE);
    }

    // ── knob rows to the right: the focused voice's 8 remaining panel knobs + SWG/PUMP ──
    const MDKnob *panel = MD_PANEL[focus];   // panel[0]=CHAR, [1]=TUNE live on the pad
    for (int i = 0; i < 4; i++) ui_knob(&kit.p[focus][panel[2 + i].param], KX[i], KY1, panel[2 + i].label);
    ui_knob(&k_swing, KX[4], KY1, "SWG");
    for (int i = 0; i < 4; i++) ui_knob(&kit.p[focus][panel[6 + i].param], KX[i], KY2, panel[6 + i].label);
    ui_knob(&k_pump,  KX[4], KY2, "PUMP");

    // ── the grid: names on the left (tap to focus, handled in update), 16 steps right ──
    for (int r = 0; r < ROWS; r++) {
        int y = GY + r * SY;
        bool foc = (r == focus);
        bool firing = frame() - flash[r] < 4;
        rectfill(1, y, GX - 3, CH, firing ? LIT[r] : (foc ? CLR_DARK_GREY : CLR_DARKER_GREY));
        rect(1, y, GX - 3, CH, foc ? LIT[r] : CLR_DARK_GREY);
        print(MD_NAME[r], 3, y + 3, (firing || foc) ? CLR_WHITE : LIT[r]);

        for (int c = 0; c < STEPS; c++) {
            int x = GX + c * SX;
            int bg = (c % 4 == 0) ? CLR_DARK_GREY : CLR_DARKER_GREY;
            rectfill(x, y, CW, CH, bg);
            if (grid[r][c]) {
                rectfill(x + 1, y + 1, CW - 2, CH - 2, LIT[r]);
                if (r == MD_HAT && hopen[c]) rect(x + 2, y + 2, CW - 4, CH - 4, CLR_WHITE);  // open hat = ringed
            }
            if (c == cur_step && playing) rect(x, y, CW, CH, CLR_WHITE);   // playhead
        }
    }
    ui_end();
}
