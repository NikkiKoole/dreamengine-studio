#include "studio.h"

// ── LOUNGE LARRY — a branching chat-up ────────────────────────
// A Leisure-Suit-Larry-style conversation. A woman's big face fills
// the screen; pick what to say and watch her react — happy, annoyed,
// angry, sad — as a VIBE meter tracks how it's going. Land the right
// lines and she gives you her number; push your luck and you wear her
// martini.
//
// THE WHOLE POINT (for the docs): there is NO dialogue "engine" here.
// Branching conversation is just a table of nodes + an int for "where
// am I" + an int for "how much does she like me". print() + btnp()
// draw it. No new API was needed — that's the lesson.
//
//   ↑/↓ or 1/2/3 pick a reply   ·   Z choose   ·   click a reply too
//
// The branch map: a smooth open → she warms up → you close the deal.
// A creepy open → she's offended → you can still apologise your way
// back, or dig in and get slapped.

// ── expressions ───────────────────────────────────────────────
enum { NEU, HAP, ANG, SAD };

// ── conversation nodes ────────────────────────────────────────
// nxt codes:  >=0 go to that node   -1 WIN   -2 SLAP   -3 LEAVE   -99 resolve by mood
typedef struct {
    const char *l1, *l2;        // what SHE says (l2 may be "")
    const char *ch[3];          // your three replies
    signed char dlt[3];         // mood change per reply
    signed char rex[3];         // her reaction face per reply
    signed char nxt[3];         // where each reply leads
} Node;

static const Node SCRIPT[] = {
    // 0 — the opening
    { "You've been staring for ten minutes.",
      "Got something to say, or just drool?",
      { "\"How does a person get that gorgeous?\"",
        "\"I was checking the clock behind you.\"",
        "\"Nice everything, baby. The whole package.\"" },
      {  18,  -5, -22 }, { HAP, NEU, ANG }, {  1,  2,  3 } },

    // 1 — charmed
    { "Smooth. Most guys just grunt and buy a drink.",
      "What's your name, charmer?",
      { "\"Larry Laffer. At your service. Yours alone.\"",
        "\"Names are for people who plan to leave.\"",
        "\"Does it really matter how tonight ends?\"" },
      {  12,   6, -16 }, { HAP, HAP, ANG }, {  4,  4,  3 } },

    // 2 — skeptical
    { "The clock. Right.",
      "You're not very good at this, are you?",
      { "\"No. But you're worth the embarrassment.\"",
        "\"I'm great at plenty. Lying isn't one.\"",
        "\"Cut me a break — I'm a little nervous.\"" },
      {  16,   8,   4 }, { HAP, NEU, HAP }, {  4,  4,  4 } },

    // 3 — offended (recovery node)
    { "And there it is.",
      "One more line and you're wearing my martini.",
      { "\"That was out of line. Let me start over.\"",
        "\"Oh come on, you know you love it.\"",
        "(grin and say absolutely nothing)" },
      {  14, -30, -10 }, { NEU, ANG, ANG }, {  1, -2, -3 } },

    // 4 — the close
    { "Okay, Larry. I'll admit it — you're not boring.",
      "So... what happens now?",
      { "\"One drink. You pick the place.\"",
        "\"Your number. Then I leave you wondering.\"",
        "\"Now you fall hopelessly for me, obviously.\"" },
      {  10,   6,   4 }, { HAP, HAP, HAP }, { -99, -99, -99 } },
};

// scene codes for `scene`
#define SC_TITLE  -100
#define SC_WIN      -1
#define SC_SLAP     -2
#define SC_LEAVE    -3

// ── layout ────────────────────────────────────────────────────
#define FCX 160            // face centre x
#define FCY  60            // face centre y
#define HRX  44            // head half-width
#define HRY  52            // head half-height
#define PANEL_Y 120        // dialogue panel top

// ── colours ───────────────────────────────────────────────────
#define SKIN     CLR_LIGHT_PEACH
#define SKIN_DK  CLR_DARK_PEACH
#define HAIR     CLR_DARK_RED
#define HAIR_HI  CLR_RED
#define LIPS     CLR_DARK_RED

// ── state ─────────────────────────────────────────────────────
static int   scene;
static int   mood;             // -40..70, how much she likes you
static int   sel;              // highlighted reply 0..2
static float react_t;          // >0 while her reaction plays out
static int   react_expr;       // her face during the reaction
static int   pending;          // scene to go to once the reaction ends
static const char *you_said;   // the reply being echoed
static float slap_flash;       // white-flash frames on a slap
static bool  ready = false;

// floating hearts ───────────────────────────────────────────────
typedef struct { float x, y, vy, ph; int life, col; } Heart;
static Heart hearts[24];

static void spawn_heart(float x, float y) {
    for (int i = 0; i < 24; i++)
        if (hearts[i].life <= 0) {
            hearts[i].x = x + rnd_between(-18, 19);
            hearts[i].y = y + rnd_between(-8, 9);
            hearts[i].vy = rnd_float_between(-1.4f, -0.6f);
            hearts[i].ph = rnd_float_between(0, 360);
            hearts[i].life = (int)rnd_between(40, 75);
            int c[3] = { CLR_PINK, CLR_RED, CLR_MAUVE };
            hearts[i].col = c[(int)rnd_between(0, 3)];
            break;
        }
}

static void reset_game(void) {
    scene = SC_TITLE;
    mood = 0; sel = 0;
    react_t = 0; pending = 0; you_said = 0; slap_flash = 0;
    for (int i = 0; i < 24; i++) hearts[i].life = 0;
    ready = true;
}

// ── apply a reply ─────────────────────────────────────────────
static void choose(const Node *n, int i) {
    mood = (int)clamp(mood + n->dlt[i], -40, 70);
    react_expr = n->rex[i];
    you_said   = n->ch[i];
    react_t    = 1.4f;

    int nx = n->nxt[i];
    if (nx == -99) nx = (mood >= 30) ? SC_WIN : (mood <= 2) ? SC_SLAP : SC_LEAVE;
    pending = nx;

    // her immediate reaction, in sound
    if (react_expr == HAP) { hit(67 + (mood > 25 ? 4 : 0), INSTR_TRI, 3, 160); spawn_heart(FCX, FCY); }
    else if (react_expr == ANG) { note(40, INSTR_SAW, 4); shake(3); }
    else if (react_expr == SAD) hit(55, INSTR_SINE, 2, 220);
    else hit(60, INSTR_TRI, 2, 120);
}

// commit the pending scene once the reaction has played
static void commit(void) {
    scene = pending;
    sel = 0;
    if (scene == SC_WIN) {
        strum(60, CHORD_MAJ7, INSTR_TRI, 5, 60);
        for (int k = 0; k < 12; k++) spawn_heart(FCX, FCY);
    } else if (scene == SC_SLAP) {
        note(36, INSTR_NOISE, 6); shake(7); slap_flash = 6;
    } else if (scene == SC_LEAVE) {
        hit(48, INSTR_TRI, 3, 300);
    }
}

void update(void) {
    if (!ready) reset_game();

    // hearts drift up
    for (int i = 0; i < 24; i++)
        if (hearts[i].life > 0) {
            hearts[i].x += sin_deg(hearts[i].ph + now() * 120) * 0.4f;
            hearts[i].y += hearts[i].vy;
            hearts[i].vy *= 0.99f;
            hearts[i].life--;
        }

    // a reaction is playing — wait it out, then move on
    if (react_t > 0) {
        react_t -= dt();
        if (react_t <= 0) commit();
        return;
    }

    // title / endings: A restarts (endings) or begins (title)
    if (scene < 0) {
        if (btnp(0, BTN_A) || mouse_pressed(MOUSE_LEFT)) {
            if (scene == SC_TITLE) { scene = 0; sel = 0; }
            else reset_game();
        }
        return;
    }

    // ── a live conversation node ──
    const Node *n = &SCRIPT[scene];

    if (btnp(0, BTN_UP))   { sel = (sel + 2) % 3; note(72, INSTR_TRI, 1); }
    if (btnp(0, BTN_DOWN)) { sel = (sel + 1) % 3; note(72, INSTR_TRI, 1); }

    // click a reply row to pick it
    int rowY = PANEL_Y + 36;
    if (mouse_pressed(MOUSE_LEFT) && mouse_y() >= rowY) {
        int r = (mouse_y() - rowY) / 10;
        if (r >= 0 && r < 3) { sel = r; choose(n, r); return; }
    }

    // number-key shortcuts
    if (keyp('1')) { choose(n, 0); return; }
    if (keyp('2')) { choose(n, 1); return; }
    if (keyp('3')) { choose(n, 2); return; }

    if (btnp(0, BTN_A)) { choose(n, sel); return; }

#ifdef DE_TRACE
    watch("scene", "%d", scene);
    watch("mood",  "%d", mood);
    watch("sel",   "%d", sel);
#endif
}

// ── face ──────────────────────────────────────────────────────
// `in` = intensity 0..1 (how strongly the expression shows)
static void draw_face(int expr, float in) {
    float bob = sin_deg(now() * 55) * 1.5f;
    int cy = FCY + (int)bob;

    // hair behind (shows as a frame around the skin)
    ovalfill(FCX, cy - 12, 50, 50, HAIR);
    ovalfill(FCX - 41, cy + 8, 13, 40, HAIR);
    ovalfill(FCX + 41, cy + 8, 13, 40, HAIR);

    // neck + shoulders (mostly tucked behind the panel)
    rectfill(FCX - 9, cy + 42, 18, 24, SKIN);
    ovalfill(FCX, cy + 90, 92, 42, CLR_INDIGO);
    ovalfill(FCX, cy + 86, 92, 40, CLR_DARKER_BLUE);

    // face
    ovalfill(FCX, cy, HRX, HRY, SKIN);
    ovalfill(FCX, cy + 22, HRX - 4, HRY - 18, SKIN);   // jaw/chin fill
    // bangs sweeping over the forehead
    ovalfill(FCX, cy - HRY + 8, HRX - 4, 16, HAIR);
    trifill(FCX - HRX + 6, cy - HRY + 18, FCX + 6, cy - HRY + 6, FCX + 2, cy - 8, HAIR);

    // ears + earrings
    circfill(FCX - HRX + 2, cy + 14, 5, SKIN);
    circfill(FCX + HRX - 2, cy + 14, 5, SKIN);
    circfill(FCX - HRX + 2, cy + 22, 2, CLR_YELLOW);
    circfill(FCX + HRX - 2, cy + 22, 2, CLR_YELLOW);

    int ey = cy - 6, lx = FCX - 17, rx = FCX + 17, ew = 8;
    bool blink = (anim(46, 11, 0) == 0);   // a quick blink every ~4s
    int eh = blink ? 1 : (expr == HAP ? 5 : expr == ANG ? 5 : 7);

    // eye whites + iris
    ovalfill(lx, ey, ew, eh, CLR_WHITE);
    ovalfill(rx, ey, ew, eh, CLR_WHITE);
    if (!blink) {
        int dy = (expr == SAD) ? 2 : 0;     // sad: eyes look down
        circfill(lx + 1, ey + dy, 4, CLR_BLUE_GREEN);
        circfill(rx - 1, ey + dy, 4, CLR_BLUE_GREEN);
        circfill(lx + 1, ey + dy, 2, CLR_BLACK);
        circfill(rx - 1, ey + dy, 2, CLR_BLACK);
        pset(lx + 2, ey - 1 + dy, CLR_WHITE);
        pset(rx,     ey - 1 + dy, CLR_WHITE);

        // eyelids carry a lot of the emotion
        int lid = (int)(5 * in);
        if (expr == ANG && lid > 0) {   // inner corners pulled DOWN
            trifill(lx - ew, ey - eh, lx + ew, ey - eh, lx + ew, ey - eh + lid, SKIN);
            trifill(rx + ew, ey - eh, rx - ew, ey - eh, rx - ew, ey - eh + lid, SKIN);
        } else if (expr == SAD && lid > 0) {  // outer corners droop
            trifill(lx + ew, ey - eh, lx - ew, ey - eh, lx - ew, ey - eh + lid, SKIN);
            trifill(rx - ew, ey - eh, rx + ew, ey - eh, rx + ew, ey - eh + lid, SKIN);
        }
    }

    // eyebrows — outer end (oy) and inner end (iy), relative to base
    int by = ey - 13, oy = 0, iy = 0;
    if (expr == HAP) { oy = -3; iy = -4; }
    else if (expr == ANG) { oy = -2; iy =  5; }
    else if (expr == SAD) { oy =  3; iy = -4; }
    oy = by + (int)(oy * in); iy = by + (int)(iy * in);
    // left eye: outer is to the left, inner toward centre
    line(lx - 11, oy, lx + 7, iy, HAIR); line(lx - 11, oy + 1, lx + 7, iy + 1, HAIR);
    line(rx + 11, oy, rx - 7, iy, HAIR); line(rx + 11, oy + 1, rx - 7, iy + 1, HAIR);

    // nose
    pset(FCX - 2, cy + 8, SKIN_DK);
    pset(FCX + 1, cy + 10, SKIN_DK);
    line(FCX - 2, cy + 11, FCX + 2, cy + 11, SKIN_DK);

    // blush — happy only
    if (expr == HAP && in > 0.35f) {
        ovalfill(FCX - 24, cy + 8, 6, 4, CLR_PINK);
        ovalfill(FCX + 24, cy + 8, 6, 4, CLR_PINK);
    }
    // anger flush
    if (expr == ANG && in > 0.5f) {
        ovalfill(FCX - 25, cy + 6, 5, 3, CLR_RED);
        ovalfill(FCX + 25, cy + 6, 5, 3, CLR_RED);
    }
    // a tear — sad, strong
    if (expr == SAD && in > 0.5f) {
        int ty = cy + 4 + (int)((1.0f - react_t / 1.4f) * 18);
        circfill(lx - 2, ty, 2, CLR_BLUE_GREEN);
    }

    // mouth — a curve; mc>0 smile, mc<0 frown
    int my = cy + 22;
    float mc = (expr == HAP ? 6 : expr == ANG ? -3 : expr == SAD ? -5 : 0) * (0.4f + 0.6f * in);
    int px2 = 0, py2 = 0;
    for (int i = 0; i <= 26; i++) {
        float t = i / 13.0f - 1.0f;
        int x = FCX - 13 + i;
        int y = my + (int)(mc * (1.0f - t * t));
        if (i) { line(px2, py2, x, y, LIPS); line(px2, py2 + 1, x, y + 1, LIPS); }
        px2 = x; py2 = y;
    }
    if (expr == HAP && in > 0.6f)   // a hint of teeth in a big smile
        ovalfill(FCX, my + 2, 8, 2, CLR_WHITE);
}

// ── a heart sprite ────────────────────────────────────────────
static void draw_heart(int x, int y, int col) {
    circfill(x - 2, y, 2, col);
    circfill(x + 2, y, 2, col);
    trifill(x - 4, y + 1, x + 4, y + 1, x, y + 5, col);
}

// ── the dialogue panel ────────────────────────────────────────
static void draw_panel(void) {
    rectfill(0, PANEL_Y, SCREEN_W, SCREEN_H - PANEL_Y, CLR_DARKER_BLUE);
    line(0, PANEL_Y, SCREEN_W, PANEL_Y, CLR_LIGHT_GREY);
    line(0, PANEL_Y + 1, SCREEN_W, PANEL_Y + 1, CLR_INDIGO);

    if (react_t > 0) {
        // echo your line back while she reacts
        font(FONT_SMALL);
        print("YOU SAID", 8, PANEL_Y + 6, CLR_DARK_GREY);
        font(FONT_NORMAL);
        print(you_said, 8, PANEL_Y + 18, CLR_LIGHT_PEACH);
        font(FONT_SMALL);
        print("...", 8, PANEL_Y + 36, CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        return;
    }

    const Node *n = &SCRIPT[scene];
    font(FONT_SMALL);
    print("SHE SAYS", 8, PANEL_Y + 6, CLR_DARK_GREY);
    font(FONT_NORMAL);
    print(n->l1, 8, PANEL_Y + 14, CLR_WHITE);
    if (n->l2 && n->l2[0]) print(n->l2, 8, PANEL_Y + 24, CLR_WHITE);

    font(FONT_SMALL);
    int rowY = PANEL_Y + 36;
    for (int i = 0; i < 3; i++) {
        int y = rowY + i * 10;
        bool on = (i == sel);
        if (on) { rectfill(2, y - 1, SCREEN_W - 4, 9, CLR_INDIGO); print(">", 4, y, CLR_LIGHT_YELLOW); }
        print(n->ch[i], 14, y, on ? CLR_LIGHT_YELLOW : CLR_LIGHT_GREY);
    }
    font(FONT_NORMAL);
}

// ── the VIBE meter ────────────────────────────────────────────
static void draw_vibe(void) {
    font(FONT_SMALL);
    print("VIBE", 6, 5, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
    float pct = (mood + 40) / 110.0f;
    int col = mood > 20 ? CLR_GREEN : mood < 0 ? CLR_RED : CLR_YELLOW;
    bar(34, 4, 120, 7, pct, col, CLR_DARK_GREY);
}

void draw(void) {
    if (!ready) reset_game();

    if (slap_flash > 0) { cls(CLR_WHITE); slap_flash -= 1; return; }

    // bar-lounge backdrop
    cls(CLR_BROWNISH_BLACK);
    for (int i = 0; i < 7; i++) {                 // out-of-focus warm lights
        int bx = 24 + i * 46, by = 30 + (i % 3) * 16;
        circfill(bx, by, 9, CLR_DARK_PURPLE);
        circfill(bx, by, 5, CLR_DARK_RED);
    }
    rectfill(0, 104, SCREEN_W, 16, CLR_DARK_BROWN);   // the bar top
    line(0, 104, SCREEN_W, 104, CLR_BROWN);

    // pick the face to show
    int expr; float in;
    if (react_t > 0) { expr = react_expr; in = clamp(react_t / 1.4f + 0.2f, 0, 1); }
    else if (scene == SC_WIN)   { expr = HAP; in = 1.0f; }
    else if (scene == SC_SLAP)  { expr = ANG; in = 1.0f; }
    else if (scene == SC_LEAVE) { expr = NEU; in = 0.3f; }
    else if (scene == SC_TITLE) { expr = HAP; in = 0.7f; }  // a welcoming smile
    else {  // resting face follows the mood
        expr = mood > 22 ? HAP : mood < -8 ? ANG : NEU;
        in = 0.5f;
    }
    draw_face(expr, in);

    // her martini (flair)
    int gx = 262, gy = 100;
    trifill(gx - 7, gy - 10, gx + 7, gy - 10, gx, gy, CLR_LIGHT_GREY);
    line(gx, gy, gx, gy + 8, CLR_LIGHT_GREY);
    line(gx - 5, gy + 8, gx + 5, gy + 8, CLR_LIGHT_GREY);
    circfill(gx + 2, gy - 7, 1, CLR_GREEN);   // olive

    // floating hearts on top
    for (int i = 0; i < 24; i++)
        if (hearts[i].life > 0)
            draw_heart((int)hearts[i].x, (int)hearts[i].y, hearts[i].col);

    // ── overlays ──
    if (scene == SC_TITLE) {
        // title lives in the panel area so her face stays the hero
        rectfill(0, PANEL_Y, SCREEN_W, SCREEN_H - PANEL_Y, CLR_DARK_BLUE);
        line(0, PANEL_Y, SCREEN_W, PANEL_Y, CLR_LIGHT_YELLOW);
        print_scaled("LOUNGE LARRY", 70, PANEL_Y + 8, CLR_LIGHT_YELLOW, 2);
        font(FONT_SMALL);
        print_centered("chat her up.  don't get slapped.", SCREEN_W / 2, PANEL_Y + 30, CLR_LIGHT_PEACH);
        print_centered("up/down or 1-2-3 to pick  -  Z to choose", SCREEN_W / 2, PANEL_Y + 42, CLR_LIGHT_GREY);
        font(FONT_NORMAL);
        if (anim(2, 2, 0)) print_centered("- press Z to begin -", SCREEN_W / 2, PANEL_Y + 58, CLR_WHITE);
        return;
    }

    draw_vibe();

    if (scene >= 0) { draw_panel(); return; }

    // ending boxes
    const char *e1 = "", *e2 = "";
    int ec = CLR_WHITE;
    if (scene == SC_WIN)   { e1 = "She folds a napkin into your hand:"; e2 = "a number, and a lipstick kiss. \"Call me.\""; ec = CLR_PINK; }
    if (scene == SC_SLAP)  { e1 = "SLAP! You're wearing the martini now."; e2 = "Smooth, Larry. Real smooth."; ec = CLR_RED; }
    if (scene == SC_LEAVE) { e1 = "She finishes her drink and stands up."; e2 = "\"Maybe next time, Larry.\"  ...She's gone."; ec = CLR_LIGHT_PEACH; }

    rectfill(8, PANEL_Y, SCREEN_W - 16, SCREEN_H - PANEL_Y - 4, CLR_DARK_BLUE);
    rect(8, PANEL_Y, SCREEN_W - 16, SCREEN_H - PANEL_Y - 4, ec);
    if (scene == SC_WIN) print_scaled("YOU'RE IN!", 86, PANEL_Y + 8, CLR_LIGHT_YELLOW, 2);
    if (scene == SC_SLAP) print_scaled("SLAPPED", 104, PANEL_Y + 8, CLR_RED, 2);
    if (scene == SC_LEAVE) print_scaled("...AND GONE", 80, PANEL_Y + 8, CLR_LIGHT_GREY, 2);
    font(FONT_SMALL);
    print_centered(e1, SCREEN_W / 2, PANEL_Y + 30, ec);
    print_centered(e2, SCREEN_W / 2, PANEL_Y + 40, ec);
    print_centered("- press Z to try again -", SCREEN_W / 2, PANEL_Y + 54, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
}
