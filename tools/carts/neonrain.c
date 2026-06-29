/* de:meta
{
  "title": "neon rain",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "game"
  ],
  "teaches": [
    "dialogue-tree",
    "inventory-system",
    "state-machine",
    "particle-system"
  ],
  "lineage": "An original point-and-click adventure in the LucasArts verb-interface tradition; novel in faking all scene lighting, neon tinting, fog, and rain entirely with primitives and pal().",
  "genre": "adventure",
  "description": "A noir point-and-click murder case. Work one rain-soaked night across five hand-painted scenes - flat, bar, alley, office - hunting the killer of singer Mara Vance. Hover hotspots to LOOK/TAKE/TALK/GO, collect evidence, grill two suspects through branching dialogue, fill your case notebook, then name the killer. Heavy pal() neon lighting, fillp() fog + wet-street reflections, a rain particle pool, and a slow synth bed (lowpass pad, vibrato sax, a sting on the reveal). MOUSE: act / click a held item onto a hotspot. RIGHT-CLICK: drop item. N: notebook. Z or click: advance text."
}
de:meta */
#include "studio.h"

// ── NEON RAIN ─────────────────────────────────────────────────────────────
// A noir point-and-click murder case. You're a private eye working one night,
// one death: the singer Mara Vance, found cold in her flat. Five rain-soaked
// scenes, mouse hotspots (LOOK / TAKE / TALK / GO), an evidence inventory, a
// case notebook, branching dialogue with two suspects, and a final accusation.
//
//   MOUSE  : move the pointer — hovered hotspots name themselves
//   LEFT   : look / take / talk / travel  (or click a held item onto a hotspot)
//   RIGHT  : drop the held item
//   N      : open the case notebook        Z / click : advance text
//
// Mood is the headline: animated rain, flickering neon that tints the scene
// via pal(), fillp() fog bands, wet-street reflections, and a slow synth pad
// bed with a lowpass filter, a vibrato sax motif, and a sting on the reveal.
// Solve it: place Silas at the scene (matchbook + the letter signed 'S'), then
// accuse him in your office. Pure primitives — no sprite sheet.

// ── scenes & state machine ─────────────────────────────────────────────────
enum { SC_STREET, SC_APT, SC_BAR, SC_ALLEY, SC_OFFICE, SC_COUNT };
enum { ST_TITLE, ST_PLAY, ST_MSG, ST_DLG, ST_NOTE, ST_ACCUSE, ST_END };

// per-scene neon accent colors (the light that bleeds into everything)
static const int ACCENT [SC_COUNT] = { CLR_PINK,       CLR_TRUE_BLUE, CLR_DARK_ORANGE, CLR_LIME_GREEN, CLR_ORANGE     };
static const int ACCENT2[SC_COUNT] = { CLR_TRUE_BLUE,  CLR_INDIGO,    CLR_RED,         CLR_DARK_GREEN, CLR_TRUE_BLUE  };
static const char *SCENE_NAME[SC_COUNT] = { "RAIN STREET", "VANCE FLAT", "BLUE PARROT", "BACK ALLEY", "MY OFFICE" };

// ── case flags / clues ──────────────────────────────────────────────────────
#define F_NEWS      (1u<<0)
#define F_GLASSES   (1u<<1)
#define F_PHOTO     (1u<<2)
#define F_MATCH     (1u<<3)   // matchbook — also an inventory item
#define F_KEY       (1u<<4)   // brass key  — also an inventory item
#define F_LETTER    (1u<<5)
#define F_ALIBI     (1u<<6)
#define F_INFORMANT (1u<<7)
#define F_SILAS     (1u<<8)

// ── verbs & actions ─────────────────────────────────────────────────────────
enum { V_LOOK, V_TAKE, V_TALK, V_GO, V_USE };
static const char *VERB[5] = { "LOOK", "TAKE", "TALK", "GO", "USE" };

enum {
    A_NONE,
    A_GO_STREET, A_GO_APT, A_GO_BAR, A_GO_ALLEY, A_GO_OFFICE,
    A_NEWS, A_NEON,
    A_BODY, A_GLASSES, A_PHOTO, A_MATCH, A_DRAWER,
    A_BOTTLES, A_SILAS,
    A_KEY, A_INFORMANT,
    A_NOTEBOOK, A_ACCUSE, A_WHISKEY,
};

// held inventory item
enum { HELD_NONE, HELD_MATCH, HELD_KEY };

typedef struct { int x, y, w, h; int verb; const char *label; int act; } Hot;

// ── globals ─────────────────────────────────────────────────────────────────
static int      g_state, g_scene;
static unsigned g_flags;
static int      held;
static bool     drawer_open;
static float    trans;            // scene transition fade (1 -> 0)

// message box (typewriter)
static const char *msg_a, *msg_b;
static int   msg_clr;
static float msg_rev;

// dialogue
static int   dlg_tree, dlg_node;
static float dlg_rev;

// notebook
static const char *clue_list[10];
static int   clue_n;
static char  toast[40];
static float toast_until;

// ending
static int         end_win;       // 1 win, 0 loss
static const char *end_reason;

// hover (recomputed each PLAY frame)
static int         hov_act;
static int         hov_verb;
static const char *hov_label;

// typewriter blip throttle
static float tw_last;

// rain pool
typedef struct { float x, y, len, sp; int col; } Drop;
#define NDROP 150
static Drop rain[NDROP];

// ── tiny helpers ─────────────────────────────────────────────────────────────
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static bool has(unsigned f) { return (g_flags & f) != 0; }

// return the first `n` chars of `s` in a rotating scratch buffer
static const char *clipn(const char *s, int n) {
    static char buf[6][96];
    static int  slot = 0;
    slot = (slot + 1) % 6;
    char *b = buf[slot];
    int i = 0;
    for (; i < n && i < 95 && s[i]; i++) b[i] = s[i];
    b[i] = 0;
    return b;
}

// ── sound ────────────────────────────────────────────────────────────────────
#define I_PAD 5
#define I_SAX 6
#define I_UI  7

static void blip(void)  { note(88, I_UI, 2); }
static void chime(void) { strum(72, CHORD_MAJ7, I_UI, 3, 55); }
static void sting(void) { hit(34, INSTR_SAW, 5, 520); hit(40, INSTR_TRI, 3, 520); }

static void set_toast(const char *t) {
    int i = 0; for (; t[i] && i < 38; i++) toast[i] = t[i]; toast[i] = 0;
    toast_until = now() + 2.3f;
}

// register a clue: set flag, push to notebook, chime + toast (once only)
static void add_clue(unsigned f, const char *text) {
    if (has(f)) return;
    g_flags |= f;
    if (clue_n < 10) clue_list[clue_n++] = text;
    chime();
    set_toast("* added to case notes");
}

// ── dialogue trees ────────────────────────────────────────────────────────────
typedef struct {
    const char *l1, *l2;
    const char *opt [3];
    unsigned    need[3];   // flag required to show this option (0 = always)
    unsigned    sets[3];   // flag set when chosen (0 = none)
    const char *clue[3];   // clue text if sets != 0
    signed char next[3];   // node to go to, -1 = end conversation
} DNode;

static const DNode SILAS[] = {
    { "We're closed, detective.", "...Mara? She sang here. Bad business.",
      { "You knew her well?", "Where were you last night?", "[Leave]" },
      { 0, 0, 0 },
      { 0, F_ALIBI, 0 },
      { 0, "Silas's alibi: 'at the bar all night.'", 0 },
      { 1, 2, -1 } },
    { "Everybody loved Mara.", "Me, I just poured the drinks.",
      { "I found your photo in her flat.", "Last night - where?", "[Leave]" },
      { F_PHOTO, 0, 0 },
      { F_SILAS, F_ALIBI, 0 },
      { "Silas: he and Mara were once lovers.", "Silas's alibi: 'at the bar all night.'", 0 },
      { 3, 2, -1 } },
    { "Right here. All night.", "Ask any of the regulars.",
      { "...Sure you were.", "[Leave]", 0 },
      { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 1, -1, -1 } },
    { "...We were close, once.", "People change. That's all I'll say.",
      { "[Leave]", 0, 0 },
      { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { -1, -1, -1 } },
};

static const DNode INFORMANT[] = {
    { "(a voice from the dark)", "Lookin' for the Vance killer? Word's pricey.",
      { "I'm listening.", "[Leave]", 0 },
      { 0, 0, 0 },
      { F_INFORMANT, 0, 0 },
      { "Witness: a man fled the tower at midnight.", 0, 0 },
      { 1, -1, -1 } },
    { "Big fella left the Glass Tower at midnight.", "Reeked of cheap bar smoke.",
      { "What did he look like?", "[Leave]", 0 },
      { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 2, -1, -1 } },
    { "Didn't catch the face. Hat pulled low.", "But he moved like he owned the place.",
      { "[Leave]", 0, 0 },
      { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { -1, -1, -1 } },
};

static const DNode *dnode(void) {
    return (dlg_tree == 0 ? &SILAS[dlg_node] : &INFORMANT[dlg_node]);
}

// ── lifecycle ──────────────────────────────────────────────────────────────────
static void reset_game(void) {
    g_flags = 0; held = HELD_NONE; drawer_open = false;
    clue_n = 0; toast_until = 0;
    g_scene = SC_STREET; g_state = ST_TITLE;
    trans = 1.0f;
}

void init(void) {
    // synth-noir bed -----------------------------------------------------------
    instrument(I_PAD, INSTR_TRI, 600, 300, 6, 1200);     // slow-attack warm pad
    instrument_filter(I_PAD, FILTER_LOW, 700, 4);
    instrument_lfo(I_PAD, 0, LFO_CUTOFF, 0.15f, 300);    // a slow filter drift
    lfo_shape(I_PAD, 0, LFO_SHAPE_RANDOM);   // organic drift, not a mechanical sine (STATUS #39)

    instrument(I_SAX, INSTR_SAW, 150, 200, 4, 500);      // a lonely sax line
    instrument_filter(I_SAX, FILTER_LOW, 1200, 7);
    instrument_lfo(I_SAX, 0, LFO_PITCH, 5.0f, 0.25f);    // vibrato

    instrument(I_UI, INSTR_SINE, 3, 120, 0, 200);        // UI bell / blips
    bpm(50);

    for (int i = 0; i < NDROP; i++) {
        rain[i].x   = rnd(SCREEN_W);
        rain[i].y   = rnd(SCREEN_H);
        rain[i].len = rnd_between(4, 11);
        rain[i].sp  = rnd_float_between(180, 340);
        rain[i].col = (rnd(3) == 0) ? CLR_INDIGO : CLR_DARKER_BLUE;
    }
    reset_game();
}

// ── the slow synth bed (always running) ──────────────────────────────────────
static void music_bed(void) {
    static const int roots[4] = { 45, 41, 48, 43 };                 // Am F C G, low
    static const int types[4] = { CHORD_MIN7, CHORD_MAJ7, CHORD_MAJ7, CHORD_DOM7 };
    if (every(8)) { int i = (beat() / 8) % 4; strum(roots[i], types[i], I_PAD, 3, 110); }
    if (every(16)) note(degree(SCALE_PENTA_MIN, 4, rnd(5)), I_SAX, 3);   // sparse sax
    if (every(4))  hit(33, I_PAD, 2, 280);                              // heartbeat pulse
}

// ── hotspot table (rebuilt per frame from scene + flags) ──────────────────────
static void add_hot(Hot *a, int *n, int x, int y, int w, int h, int verb, const char *label, int act) {
    if (*n >= 12) return;
    a[*n] = (Hot){ x, y, w, h, verb, label, act };
    (*n)++;
}

static int build_hotspots(Hot *a) {
    int n = 0;
    switch (g_scene) {
    case SC_STREET:
        add_hot(a, &n, 14,  118, 36, 36, V_GO,   "my office",          A_GO_OFFICE);
        add_hot(a, &n, 92,  112, 36, 42, V_GO,   "the Glass Tower",    A_GO_APT);
        add_hot(a, &n, 186, 116, 36, 38, V_GO,   "The Blue Parrot",    A_GO_BAR);
        add_hot(a, &n, 258, 86,  44, 68, V_GO,   "a dark alley",       A_GO_ALLEY);
        add_hot(a, &n, 150, 132, 14, 20, V_LOOK, "the evening paper",  A_NEWS);
        add_hot(a, &n, 184, 96,  62, 16, V_LOOK, "a buzzing sign",     A_NEON);
        break;
    case SC_APT:
        add_hot(a, &n, 128, 132, 56, 40, V_LOOK, "a chalk outline",    A_BODY);
        add_hot(a, &n, 92,  110, 40, 22, V_LOOK, "two wine glasses",   A_GLASSES);
        add_hot(a, &n, 24,  58,  34, 28, V_LOOK, "a framed photo",     A_PHOTO);
        add_hot(a, &n, 232, 104, 60, 40, drawer_open ? V_LOOK : V_USE,
                                          "a desk drawer",             A_DRAWER);
        if (!has(F_MATCH))
            add_hot(a, &n, 176, 156, 16, 12, V_TAKE, "a matchbook",    A_MATCH);
        add_hot(a, &n, 2,   150, 30, 30, V_GO,   "back to the street", A_GO_STREET);
        break;
    case SC_BAR:
        add_hot(a, &n, 132, 70,  52, 56, V_TALK, "Silas, the barman",  A_SILAS);
        add_hot(a, &n, 30,  44,  130,28, V_LOOK, "the back bar",       A_BOTTLES);
        add_hot(a, &n, 2,   150, 30, 30, V_GO,   "back to the street", A_GO_STREET);
        break;
    case SC_ALLEY:
        add_hot(a, &n, 214, 78,  48, 70, V_TALK, "a figure in shadow", A_INFORMANT);
        if (!has(F_KEY))
            add_hot(a, &n, 96, 158, 30, 16, V_TAKE, "a glint in the puddle", A_KEY);
        add_hot(a, &n, 2,   150, 30, 30, V_GO,   "back to the street", A_GO_STREET);
        break;
    case SC_OFFICE:
        add_hot(a, &n, 188, 36,  118,76, V_USE,  "the evidence board", A_ACCUSE);
        add_hot(a, &n, 40,  140, 44, 24, V_LOOK, "my case notebook",   A_NOTEBOOK);
        add_hot(a, &n, 110, 136, 16, 26, V_LOOK, "a bottle of rye",    A_WHISKEY);
        add_hot(a, &n, 2,   150, 30, 30, V_GO,   "out into the rain",  A_GO_STREET);
        break;
    }
    return n;
}

static void show_msg(const char *a, const char *b, int c) {
    msg_a = a; msg_b = b ? b : ""; msg_clr = c; msg_rev = 0; g_state = ST_MSG;
}

static void travel(int scene) { g_scene = scene; trans = 1.0f; held = HELD_NONE; blip(); }

// ── act on a clicked hotspot ──────────────────────────────────────────────────
static void do_action(int act) {
    switch (act) {
    case A_GO_STREET: travel(SC_STREET); break;
    case A_GO_APT:    travel(SC_APT);    break;
    case A_GO_BAR:    travel(SC_BAR);    break;
    case A_GO_ALLEY:  travel(SC_ALLEY);  break;
    case A_GO_OFFICE: travel(SC_OFFICE); break;

    case A_NEWS:
        add_clue(F_NEWS, "Victim: Mara Vance, found dead in her flat.");
        show_msg("FRONT PAGE: 'Torch singer Mara", "Vance found dead in tower flat.'", CLR_LIGHT_YELLOW);
        break;
    case A_NEON:
        show_msg("Neon hums in the wet dark.", "The whole city smells of rain.", CLR_PINK);
        break;

    case A_BODY:
        show_msg("The chalk outline of Mara Vance.", "No wounds. Poison, the coroner says.", CLR_LIGHT_GREY);
        break;
    case A_GLASSES:
        add_clue(F_GLASSES, "Two wine glasses - she had a guest.");
        show_msg("Two wine glasses on the table.", "She wasn't drinking alone.", CLR_LIGHT_PEACH);
        break;
    case A_PHOTO:
        add_clue(F_PHOTO, "Photo: Mara laughing with Silas the barman.");
        show_msg("A framed photo: Mara, laughing,", "with Silas from the Blue Parrot.", CLR_LIGHT_PEACH);
        break;
    case A_MATCH:
        add_clue(F_MATCH, "Matchbook 'Blue Parrot' - at the scene.");
        show_msg("A matchbook: 'THE BLUE PARROT'.", "Silas's bar. Left at the scene.", CLR_RED);
        break;
    case A_DRAWER:
        if (drawer_open) { show_msg("The drawer is empty now.", 0, CLR_LIGHT_GREY); }
        else if (held == HELD_KEY) {
            drawer_open = true; held = HELD_NONE; sting();
            add_clue(F_LETTER, "Threat letter in her drawer, signed 'S'.");
            show_msg("The brass key fits. Inside: a letter.", "A threat. Signed with one letter: 'S'.", CLR_RED);
        } else { show_msg("A locked desk drawer.", "It needs a small key.", CLR_LIGHT_GREY); }
        break;

    case A_BOTTLES:
        show_msg("Backlit bottles, dust on the good stuff.", "Business has been slow.", CLR_LIGHT_GREY);
        break;
    case A_SILAS:   dlg_tree = 0; dlg_node = 0; dlg_rev = 0; g_state = ST_DLG; blip(); break;

    case A_KEY:
        add_clue(F_KEY, "A small brass key from the alley.");
        show_msg("A small brass key in the gutter.", "Dropped by someone in a hurry.", CLR_LIGHT_YELLOW);
        break;
    case A_INFORMANT: dlg_tree = 1; dlg_node = 0; dlg_rev = 0; g_state = ST_DLG; blip(); break;

    case A_NOTEBOOK: g_state = ST_NOTE; blip(); break;
    case A_ACCUSE:   g_state = ST_ACCUSE; blip(); break;
    case A_WHISKEY:
        show_msg("A bottle of rye and one clean glass.", "For when the case finally cracks.", CLR_ORANGE);
        break;
    }
}

// ── accusation outcome ────────────────────────────────────────────────────────
static void accuse(int who) {  // 0 = Silas, 1 = informant, 2 = nobody
    if (who == 0) {
        if (has(F_LETTER) && has(F_MATCH)) {
            end_win = 1;
            end_reason = "Matchbook at the scene. A letter signed 'S'. The alibi falls apart.";
            strum(60, CHORD_MAJ7, I_PAD, 4, 80);
            g_state = ST_END;
        } else {
            show_msg("The Captain waves you off:", "\"A hunch isn't proof. Get evidence.\"", CLR_ORANGE);
            blip();
        }
    } else if (who == 1) {
        end_win = 0;
        end_reason = "You jailed a petty snitch. The real killer slips into the rain.";
        strum(45, CHORD_MIN, I_PAD, 4, 130);
        g_state = ST_END;
    } else {
        end_win = 0;
        end_reason = "The case goes cold. Just another ghost in the neon.";
        strum(45, CHORD_MIN, I_PAD, 4, 130);
        g_state = ST_END;
    }
}

// ── update ────────────────────────────────────────────────────────────────────
static void update_rain(void) {
    float d = dt();
    for (int i = 0; i < NDROP; i++) {
        rain[i].y += rain[i].sp * d;
        rain[i].x -= rain[i].sp * 0.18f * d;          // wind slant
        if (rain[i].y > SCREEN_H) { rain[i].y = -rain[i].len; rain[i].x = rnd(SCREEN_W); }
        if (rain[i].x < 0) rain[i].x += SCREEN_W;
    }
}

static int visible_opts(const DNode *d, int *idx) {
    int m = 0;
    for (int i = 0; i < 3; i++)
        if (d->opt[i] && (d->need[i] == 0 || has(d->need[i]))) idx[m++] = i;
    return m;
}

void update(void) {
    update_rain();
    music_bed();
    if (trans > 0) trans -= dt() * 2.2f;
#ifdef DE_TRACE
    watch("scene", "%d", g_scene);   // 0 street 1 apt 2 bar 3 alley 4 office
    watch("state", "%d", g_state);   // 0 title 1 play 2 msg 3 dlg 4 note 5 accuse 6 end
    watch("held",  "%d", held);
    watch("mouse", "%d,%d", mouse_x(), mouse_y());
#endif

    bool click = mouse_pressed(MOUSE_LEFT);
    bool ok    = btnp(0, BTN_A) || keyp(KEY_SPACE) || keyp(KEY_ENTER);

    switch (g_state) {
    case ST_TITLE:
        if (click || ok) { g_state = ST_PLAY; blip(); }
        return;

    case ST_MSG:
        msg_rev += dt() * 42.0f;
        if (click || ok) {
            int total = slen(msg_a) + slen(msg_b);
            if ((int)msg_rev < total) msg_rev = total;     // first click: finish typing
            else { g_state = ST_PLAY; blip(); }
        }
        return;

    case ST_NOTE:
        if (click || ok || keyp('N')) { g_state = ST_PLAY; blip(); }
        return;

    case ST_ACCUSE: {
        if (mouse_pressed(MOUSE_RIGHT) || keyp(KEY_ESCAPE)) { g_state = ST_PLAY; blip(); return; }
        if (click) {
            int my = mouse_y();
            if      (my >= 70  && my < 92)  accuse(0);
            else if (my >= 100 && my < 122) accuse(1);
            else if (my >= 130 && my < 152) accuse(2);
        }
        return;
    }

    case ST_END:
        if (click || ok) reset_game();
        return;

    case ST_DLG: {
        const DNode *d = dnode();
        int full = slen(d->l1) + slen(d->l2);
        dlg_rev += dt() * 44.0f;
        if (dlg_rev > full + 4) dlg_rev = full + 4;

        if (click) {
            if ((int)dlg_rev < full) { dlg_rev = full + 4; blip(); return; }   // finish typing
            int idx[3]; int m = visible_opts(d, idx);
            int my = mouse_y();
            for (int r = 0; r < m; r++) {
                int ry = 150 + r * 14;
                if (my >= ry - 2 && my < ry + 11) {
                    int o = idx[r];
                    if (d->sets[o]) add_clue(d->sets[o], d->clue[o]); else blip();
                    if (d->next[o] < 0) g_state = ST_PLAY;
                    else { dlg_node = d->next[o]; dlg_rev = 0; }
                    return;
                }
            }
        }
        return;
    }

    default: break;   // ST_PLAY
    }

    // ───── ST_PLAY ─────
    if (keyp('N')) { g_state = ST_NOTE; blip(); return; }

    Hot hs[12];
    int n = build_hotspots(hs);
    int mx = mouse_x(), my = mouse_y();

    hov_act = A_NONE; hov_label = 0; hov_verb = 0;
    for (int i = 0; i < n; i++)
        if (point_in_box(mx, my, hs[i].x, hs[i].y, hs[i].w, hs[i].h)) {
            hov_act = hs[i].act; hov_label = hs[i].label; hov_verb = hs[i].verb;
        }

    if (mouse_pressed(MOUSE_RIGHT)) { if (held) { held = HELD_NONE; blip(); } return; }

    if (click) {
        // inventory bar first
        if (my >= 184) {
            if (has(F_MATCH) && mx >= 4  && mx < 22) { held = (held == HELD_MATCH) ? HELD_NONE : HELD_MATCH; blip(); return; }
            if (has(F_KEY)   && mx >= 24 && mx < 42) { held = (held == HELD_KEY)   ? HELD_NONE : HELD_KEY;   blip(); return; }
            return;
        }
        if (hov_act) {
            // leaving a scene always works and drops any held item — never get trapped
            if (hov_verb == V_GO) { do_action(hov_act); return; }
            // using the key on the drawer is the one real combine
            if (held == HELD_KEY && hov_act != A_DRAWER) { show_msg("The key doesn't fit there.", 0, CLR_LIGHT_GREY); return; }
            if (held == HELD_MATCH) { show_msg("Striking a match here proves nothing.", 0, CLR_LIGHT_GREY); held = HELD_NONE; return; }
            do_action(hov_act);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// DRAWING
// ════════════════════════════════════════════════════════════════════════════

static void dither_panel(int x, int y, int w, int h, int border) {
    rectfill(x, y, w, h, CLR_BLACK);
    fillp(FILL_CHECKER, -1); rectfill(x + 1, y + 1, w - 2, h - 2, CLR_DARKER_BLUE); fillp_reset();
    rect(x, y, w, h, border);
}

// soft neon glow behind a sign (dithered halo) + the sign box
static void neon_sign(int x, int y, int w, int h, const char *txt, int col) {
    bool buzz = blink(23);                          // flickers on/off
    int  c    = buzz ? col : CLR_DARK_GREY;
    fillp(FILL_DOTS, -1);
    ovalfill(x + w / 2, y + h / 2, w / 2 + 10, h / 2 + 8, buzz ? col : CLR_BLACK);
    fillp_reset();
    rectfill(x, y, w, h, CLR_BLACK);
    rect(x, y, w, h, c);
    print(txt, x + (w - text_width(txt)) / 2, y + (h - 7) / 2, c);
}

// horizontal drifting fog band
static void fog_band(int y, int h, int col) {
    int off = (int)(sin_deg(now() * 12.0f) * 8.0f);
    fillp(FILL_DOTS, -1);
    rectfill(-off, y, SCREEN_W + 16, h, col);
    fillp_reset();
}

static void draw_rain(int accent) {
    for (int i = 0; i < NDROP; i++) {
        int c = rain[i].col;
        if (i % 11 == 0) c = accent;                // a few drops catch the neon
        line((int)rain[i].x, (int)rain[i].y,
             (int)rain[i].x - 2, (int)rain[i].y + (int)rain[i].len, c);
    }
}

// darken screen edges (vignette) with a dither so the center stays lit
static void vignette(void) {
    fillp(FILL_CHECKER, -1);
    rectfill(0, 0, SCREEN_W, 16, CLR_BLACK);
    rectfill(0, SCREEN_H - 20, SCREEN_W, 20, CLR_BLACK);
    rectfill(0, 0, 12, SCREEN_H, CLR_BLACK);
    rectfill(SCREEN_W - 12, 0, 12, SCREEN_H, CLR_BLACK);
    fillp_reset();
}

// a person bust drawn from primitives, lit by the scene's neon via pal().
// CLR_MEDIUM_GREY = "magic" rim that we pal-swap to the accent; one glowing eye.
static void draw_bust(int x, int y, int body, int skin, int accent, bool hooded) {
    pal(CLR_MEDIUM_GREY, accent);                   // rim light follows the neon
    if (hooded) {
        trifill(x - 16, y + 34, x + 16, y + 34, x, y - 18, CLR_BROWNISH_BLACK);  // cloak
        circfill(x, y, 13, CLR_BROWNISH_BLACK);                                   // hood
        line(x + 10, y - 8, x + 4, y + 26, CLR_MEDIUM_GREY);                      // rim
        circfill(x, y + 2, 8, CLR_DARKER_GREY);                                   // shadowed face
    } else {
        rectfill(x - 17, y + 16, 34, 22, body);                                   // shoulders
        line(x + 14, y + 16, x + 16, y + 36, CLR_MEDIUM_GREY);                     // rim light
        circfill(x, y, 12, skin);                                                 // head
        line(x + 9, y - 8, x + 11, y + 8, CLR_MEDIUM_GREY);                        // cheek rim
    }
    pal_reset();
    // eyes: one catches the light
    circfill(x - 4, y - 1, 1, CLR_BLACK);
    pset(x + 4, y - 1, accent);
}

// rising smoke / steam curl from (x,y)
static void smoke(int x, int y, int col) {
    fillp(FILL_DOTS, -1);
    for (int i = 0; i < 5; i++) {
        float t = now() * 0.6f + i * 0.7f;
        int sx = x + (int)(sin_deg(t * 120 + i * 60) * 6);
        int sy = y - i * 7 - (int)(t * 4) % 30;
        ovalfill(sx, sy, 4 + i, 3 + i, col);
    }
    fillp_reset();
}

// ── scene backdrops ────────────────────────────────────────────────────────────
static void draw_street(void) {
    cls(CLR_BROWNISH_BLACK);
    // sky
    rectfill(0, 0, SCREEN_W, 90, CLR_DARKER_BLUE);
    fillp(FILL_CHECKER, -1); rectfill(0, 60, SCREEN_W, 34, CLR_DARK_BLUE); fillp_reset();

    // distant skyline + lit windows
    int sky[6][3] = { {0,40,30}, {34,28,24}, {62,52,22}, {236,46,30}, {270,30,28}, {300,50,20} };
    for (int b = 0; b < 6; b++) {
        rectfill(sky[b][0], 90 - sky[b][1], sky[b][2], sky[b][1], CLR_DARKER_PURPLE);
        for (int wy = 90 - sky[b][1] + 3; wy < 88; wy += 6)
            for (int wx = sky[b][0] + 2; wx < sky[b][0] + sky[b][2] - 2; wx += 5)
                if ((wx * 7 + wy) % 3 == 0) pset(wx, wy, CLR_LIGHT_YELLOW);
    }

    // storefronts
    rectfill(0, 90, SCREEN_W, 64, CLR_DARKER_GREY);     // facade row
    // office (left)
    rectfill(10, 90, 50, 64, CLR_DARKER_PURPLE);
    rectfill(14, 118, 36, 36, CLR_BLACK); rect(14, 118, 36, 36, CLR_DARK_GREY);
    rectfill(20, 124, 24, 16, CLR_DARKER_BLUE);          // frosted glass
    print("EYE", 22, 128, CLR_BLUE);
    // glass tower (apartment)
    rectfill(86, 70, 48, 84, CLR_DARKER_BLUE);
    for (int wy = 74; wy < 110; wy += 7)
        for (int wx = 90; wx < 132; wx += 7)
            pset(wx, wy, ((wx + wy) % 3 == 0) ? CLR_LIGHT_YELLOW : CLR_DARKER_GREY);
    rectfill(92, 112, 36, 42, CLR_BLACK); rect(92, 112, 36, 42, CLR_DARK_GREY);
    rectfill(98, 120, 24, 34, CLR_DARK_BLUE);
    // the bar
    rectfill(170, 90, 84, 64, CLR_DARKER_PURPLE);
    neon_sign(184, 96, 62, 16, "BLUE PARROT", ACCENT[SC_STREET]);
    rectfill(186, 116, 36, 38, CLR_BLACK); rect(186, 116, 36, 38, CLR_DARK_GREY);
    rectfill(192, 122, 24, 32, CLR_DARK_PURPLE);
    // alley gap
    rectfill(256, 70, 46, 84, CLR_BLACK);
    rect(256, 70, 46, 84, CLR_DARKER_GREY);
    fillp(FILL_DOTS, -1); rectfill(262, 80, 34, 60, CLR_DARKER_BLUE); fillp_reset();

    // wet street + reflections
    rectfill(0, 154, SCREEN_W, SCREEN_H - 154, CLR_BROWNISH_BLACK);
    fillp(FILL_VLINES, -1);
    rectfill(186, 154, 60, 44, ACCENT[SC_STREET]);       // neon puddle reflection
    rectfill(92,  154, 36, 44, CLR_DARK_BLUE);
    fillp_reset();

    // newspaper box
    rectfill(150, 132, 14, 22, CLR_DARK_RED);
    rect(150, 132, 14, 22, CLR_BLACK);
    rectfill(151, 135, 12, 8, CLR_LIGHT_GREY);
}

static void draw_apt(void) {
    cls(CLR_DARKER_PURPLE);
    rectfill(0, 0, SCREEN_W, 130, CLR_DARKER_BLUE);          // wall
    rectfill(0, 130, SCREEN_W, SCREEN_H - 130, CLR_DARKER_GREY); // floor

    // window with blinds, neon glow through
    rectfill(232, 14, 78, 78, CLR_DARK_BLUE);
    fillp(FILL_DOTS, -1); rectfill(232, 14, 78, 78, ACCENT[SC_APT]); fillp_reset();
    for (int by = 18; by < 90; by += 8) line(234, by, 308, by, CLR_DARKER_BLUE);
    rect(232, 14, 78, 78, CLR_BLACK);

    // framed photo (left wall) — Mara + Silas
    rectfill(24, 58, 34, 28, CLR_DARK_BROWN); rect(24, 58, 34, 28, CLR_BROWN);
    rectfill(28, 62, 26, 20, CLR_DARKER_GREY);
    circfill(35, 72, 4, CLR_LIGHT_PEACH); circfill(47, 72, 4, CLR_PEACH);

    // table + two wine glasses
    rectfill(88, 122, 48, 6, CLR_DARK_BROWN);
    for (int g = 0; g < 2; g++) {
        int gx = 100 + g * 22;
        ovalfill(gx, 114, 4, 5, CLR_DARK_RED); pset(gx, 114, CLR_RED);
        line(gx, 118, gx, 122, CLR_LIGHT_GREY);
    }

    // chalk outline of the body
    int bx = 150, by = 142;
    int chalk = CLR_LIGHT_GREY;
    circ(bx, by - 4, 5, chalk);
    line(bx, by + 1, bx + 4, by + 18, chalk);
    line(bx, by + 4, bx - 14, by + 2, chalk);
    line(bx, by + 4, bx + 16, by + 12, chalk);
    line(bx + 4, by + 18, bx - 6, by + 30, chalk);
    line(bx + 4, by + 18, bx + 18, by + 26, chalk);

    // matchbook on the floor
    if (!has(F_MATCH)) {
        rectfill(176, 156, 16, 11, CLR_DARK_RED); rect(176, 156, 16, 11, CLR_BLACK);
        line(178, 158, 190, 158, CLR_LIGHT_PEACH);
    }

    // desk + drawer (right)
    rectfill(232, 104, 60, 56, CLR_DARK_BROWN);
    rect(232, 104, 60, 56, CLR_BROWNISH_BLACK);
    if (drawer_open) {
        rectfill(236, 112, 52, 22, CLR_BROWNISH_BLACK);
        rectfill(240, 116, 30, 14, CLR_LIGHT_PEACH);        // the letter
        print("S", 252, 119, CLR_DARK_RED);
    } else {
        rectfill(238, 112, 48, 18, CLR_BROWN); rect(238, 112, 48, 18, CLR_BROWNISH_BLACK);
        circfill(262, 121, 2, CLR_LIGHT_YELLOW);            // handle
    }

    // exit arrow
    // (exit arrow is drawn in draw_hud, on top of the fog/vignette so it's visible)
}

static void draw_bar(void) {
    cls(CLR_BROWNISH_BLACK);
    rectfill(0, 0, SCREEN_W, 130, CLR_DARKER_PURPLE);
    // back-bar shelves with backlit bottles
    rectfill(20, 36, 150, 56, CLR_BLACK); rect(20, 36, 150, 56, CLR_DARK_BROWN);
    int bot[7] = { CLR_DARK_GREEN, CLR_TRUE_BLUE, CLR_DARK_RED, CLR_ORANGE, CLR_DARK_GREEN, CLR_INDIGO, CLR_DARK_ORANGE };
    for (int i = 0; i < 7; i++) {
        int x = 30 + i * 19;
        fillp(FILL_DOTS, -1); ovalfill(x, 60, 7, 18, bot[i]); fillp_reset();
        rectfill(x - 3, 44, 6, 30, bot[i]);
        circfill(x, 44, 3, bot[i]);
    }
    neon_sign(190, 30, 56, 14, "PARROT", ACCENT[SC_BAR]);

    // Silas behind the counter
    draw_bust(158, 88, CLR_DARK_GREY, CLR_PEACH, ACCENT[SC_BAR], false);
    smoke(120, 96, CLR_LIGHT_GREY);

    // counter
    rectfill(0, 128, SCREEN_W, 10, CLR_DARK_BROWN);
    rectfill(0, 138, SCREEN_W, SCREEN_H - 138, CLR_BROWNISH_BLACK);
    fillp(FILL_VLINES, -1); rectfill(0, 138, SCREEN_W, 30, ACCENT[SC_BAR]); fillp_reset();

    // (exit arrow is drawn in draw_hud, on top of the fog/vignette so it's visible)
}

static void draw_alley(void) {
    cls(CLR_BLACK);
    // converging brick walls
    rectfill(0, 0, 70, SCREEN_H, CLR_DARKER_PURPLE);
    rectfill(SCREEN_W - 70, 0, 70, SCREEN_H, CLR_DARKER_PURPLE);
    for (int y = 8; y < 160; y += 10) {
        line(0, y, 70, y - 6, CLR_BROWNISH_BLACK);
        line(SCREEN_W, y, SCREEN_W - 70, y - 6, CLR_BROWNISH_BLACK);
    }
    // far wall with a lit doorway back to the street
    rectfill(70, 40, SCREEN_W - 140, 110, CLR_DARKER_GREY);
    fillp(FILL_DOTS, -1); rectfill(120, 40, 80, 70, ACCENT2[SC_ALLEY]); fillp_reset();

    // fire escape
    for (int y = 30; y < 120; y += 16) line(40, y, 70, y, CLR_DARK_GREY);
    // dumpster
    rectfill(74, 120, 40, 34, CLR_DARK_GREEN); rect(74, 120, 40, 34, CLR_BROWNISH_BLACK);
    // grate steam
    smoke(150, 150, CLR_MEDIUM_GREY);

    // ground + green puddle
    rectfill(0, 150, SCREEN_W, SCREEN_H - 150, CLR_BROWNISH_BLACK);
    fillp(FILL_DOTS, -1); ovalfill(110, 166, 26, 8, ACCENT[SC_ALLEY]); fillp_reset();
    if (!has(F_KEY)) {                                      // key glinting in the puddle
        if (blink(18)) { pset(110, 164, CLR_WHITE); pset(111, 165, CLR_YELLOW); }
        rectfill(106, 163, 6, 2, CLR_LIGHT_YELLOW);
    }

    // the informant against the wall + cigarette ember
    draw_bust(238, 96, CLR_BROWNISH_BLACK, CLR_DARKER_GREY, ACCENT[SC_ALLEY], true);
    if (blink(40)) circfill(228, 104, 1, CLR_ORANGE);      // cigarette glow

    // (exit arrow is drawn in draw_hud, on top of the fog/vignette so it's visible)
}

static void draw_office(void) {
    cls(CLR_BROWNISH_BLACK);
    rectfill(0, 0, SCREEN_W, 168, CLR_DARKER_PURPLE);
    // window with blinds (right) + city glow
    rectfill(250, 12, 60, 90, CLR_DARK_BLUE);
    fillp(FILL_DOTS, -1); rectfill(250, 12, 60, 90, CLR_TRUE_BLUE); fillp_reset();
    for (int by = 16; by < 100; by += 7) line(252, by, 308, by, CLR_DARKER_BLUE);
    rect(250, 12, 60, 90, CLR_BLACK);

    // evidence corkboard
    rectfill(188, 36, 118, 76, CLR_DARK_BROWN); rect(188, 36, 118, 76, CLR_BROWNISH_BLACK);
    print("THE CASE", 214, 40, CLR_LIGHT_YELLOW);
    // pinned cards = clues collected, strung in red
    int px = 196, py = 54;
    for (int i = 0; i < clue_n && i < 9; i++) {
        int cx = px + (i % 3) * 38, cy = py + (i / 3) * 18;
        rectfill(cx, cy, 32, 14, CLR_LIGHT_PEACH);
        rect(cx, cy, 32, 14, CLR_DARK_RED);
        if (i > 0) line(px + 16, py + 7, cx + 16, cy + 7, CLR_RED);
        circfill(cx + 16, cy, 1, CLR_RED);
    }
    print("USE: accuse", 214, 102, has(F_LETTER) && has(F_MATCH) ? CLR_GREEN : CLR_DARK_GREY);

    // desk + lamp cone
    rectfill(0, 150, SCREEN_W, SCREEN_H - 150, CLR_DARK_BROWN);
    rectfill(150, 110, 6, 30, CLR_DARK_GREY);              // lamp stand
    fillp(FILL_DOTS, -1); trifill(153, 112, 110, 168, 200, 168, CLR_LIGHT_YELLOW); fillp_reset();
    circfill(153, 110, 5, CLR_YELLOW);

    // notebook on the desk
    rectfill(40, 140, 44, 24, CLR_DARK_BLUE); rect(40, 140, 44, 24, CLR_LIGHT_GREY);
    line(62, 142, 62, 162, CLR_LIGHT_GREY);
    print("CASE", 44, 144, CLR_LIGHT_PEACH);
    print("NOTE", 66, 144, CLR_LIGHT_PEACH);

    // bottle of rye
    rectfill(112, 136, 8, 26, CLR_DARK_ORANGE); rect(112, 136, 8, 26, CLR_BROWNISH_BLACK);
    rectfill(114, 132, 4, 6, CLR_DARK_GREY);

    // (exit arrow is drawn in draw_hud, on top of the fog/vignette so it's visible)
}

// ── overlays / HUD ───────────────────────────────────────────────────────────
static void draw_item_icon(int item, int x, int y) {
    if (item == HELD_MATCH) {
        rectfill(x, y, 14, 10, CLR_DARK_RED); rect(x, y, 14, 10, CLR_BLACK);
        line(x + 2, y + 2, x + 11, y + 2, CLR_LIGHT_PEACH);
    } else if (item == HELD_KEY) {
        circfill(x + 3, y + 5, 3, CLR_YELLOW); circ(x + 3, y + 5, 3, CLR_ORANGE);
        rectfill(x + 6, y + 4, 7, 2, CLR_YELLOW);
        rectfill(x + 11, y + 6, 2, 2, CLR_YELLOW);
    }
}

static void draw_hud(void) {
    // hover label (top center bar)
    if (hov_label) {
        const char *lbl = str("%s  %s", VERB[hov_verb], hov_label);
        int w = text_width(lbl) + 10;
        int x = (SCREEN_W - w) / 2;
        rectfill(x, 2, w, 11, CLR_BLACK);
        rect(x, 2, w, 11, ACCENT[g_scene]);
        print_centered(lbl, SCREEN_W/2, 4, CLR_LIGHT_PEACH);
    }
    // scene name (top-right)
    print_right(SCENE_NAME[g_scene], SCREEN_W - 4, 4, CLR_DARK_GREY);

    // inventory bar
    rectfill(0, 184, SCREEN_W, SCREEN_H - 184, CLR_BLACK);
    line(0, 184, SCREEN_W, 184, CLR_DARKER_GREY);
    if (has(F_MATCH)) { if (held == HELD_MATCH) rect(2, 186, 18, 12, CLR_YELLOW); draw_item_icon(HELD_MATCH, 4, 187); }
    if (has(F_KEY))   { if (held == HELD_KEY)   rect(22, 186, 18, 12, CLR_YELLOW); draw_item_icon(HELD_KEY, 24, 187); }
    print_right("[N] notes", SCREEN_W - 4, 188, CLR_DARK_GREY);

    // exit-to-street arrow — every scene but the street hub. Drawn here in the HUD
    // (after the scene's fog + vignette) so it's never buried, with a bright neon
    // border + glow so it clearly reads as the way out. Hotspot: (2,150,30,30).
    if (g_scene != SC_STREET) {
        bool lit = point_in_box(mouse_x(), mouse_y(), 2, 150, 30, 30);
        int  ac  = ACCENT[g_scene];
        rectfill(2, 150, 30, 30, CLR_BLACK);
        rect(2, 150, 30, 30, lit ? CLR_WHITE : ac);
        print("<<", 9, 156, lit ? CLR_WHITE : CLR_LIGHT_PEACH);
        print("OUT", 6, 168, lit ? CLR_WHITE : ac);
    }

    // toast
    if (now() < toast_until) print_centered(toast, SCREEN_W/2, 176, CLR_LIGHT_YELLOW);
}

static void draw_cursor(void) {
    int mx = mouse_x(), my = mouse_y();
    int c = hov_act ? ACCENT[g_scene] : CLR_LIGHT_GREY;
    line(mx - 4, my, mx - 1, my, c); line(mx + 1, my, mx + 4, my, c);
    line(mx, my - 4, mx, my - 1, c); line(mx, my + 1, mx, my + 4, c);
    if (hov_act) circ(mx, my, 5, c);
    if (held) draw_item_icon(held, mx + 5, my + 4);
}

static void draw_msgbox(void) {
    int w = 250, h = 46, bx = (SCREEN_W - w) / 2, by = 60;
    dither_panel(bx, by, w, h, msg_clr);
    int n  = (int)msg_rev;
    int l1 = slen(msg_a);
    print_centered(clipn(msg_a, n), SCREEN_W/2, by + 10, msg_clr);
    if (msg_b[0]) print_centered(clipn(msg_b, n - l1), SCREEN_W/2, by + 22, msg_clr);
    if (n >= l1 + slen(msg_b) && blink(18)) print_centered("- click -", SCREEN_W/2, by + h - 11, CLR_DARK_GREY);
}

static void draw_dialogue(void) {
    const DNode *d = dnode();
    const char *who = (dlg_tree == 0) ? "SILAS" : "INFORMANT";
    int accent = (dlg_tree == 0) ? CLR_DARK_ORANGE : CLR_LIME_GREEN;

    // portrait
    dither_panel(8, 30, 56, 64, accent);
    draw_bust(36, 60, dlg_tree == 0 ? CLR_DARK_GREY : CLR_BROWNISH_BLACK,
              dlg_tree == 0 ? CLR_PEACH : CLR_DARKER_GREY, accent, dlg_tree == 1);
    print(who, 12, 84, accent);

    // line box (typewriter)
    dither_panel(72, 30, SCREEN_W - 80, 64, CLR_LIGHT_GREY);
    int n  = (int)dlg_rev;
    int l1 = slen(d->l1);
    print(clipn(d->l1, n), 80, 42, CLR_WHITE);
    if (d->l2[0]) print(clipn(d->l2, n - l1), 80, 54, CLR_LIGHT_GREY);

    // options
    if (n >= l1 + slen(d->l2)) {
        int idx[3]; int m = visible_opts(d, idx);
        int mx = mouse_x(), my = mouse_y();
        for (int r = 0; r < m; r++) {
            int o = idx[r], ry = 150 + r * 14;
            bool hot = (my >= ry - 2 && my < ry + 11);
            print(str("%d.", r + 1), 10, ry, CLR_DARK_GREY);
            print(d->opt[o], 24, ry, hot ? CLR_LIGHT_YELLOW : CLR_LIGHT_GREY);
        }
    }
}

static void draw_notebook(void) {
    int w = 240, h = 150, bx = (SCREEN_W - w) / 2, by = 24;
    dither_panel(bx, by, w, h, CLR_LIGHT_YELLOW);
    print_centered("CASE NOTES - MARA VANCE", SCREEN_W/2, by + 8, CLR_LIGHT_YELLOW);
    line(bx + 8, by + 18, bx + w - 8, by + 18, CLR_DARK_GREY);
    if (clue_n == 0) print_centered("(nothing yet - go investigate)", SCREEN_W/2, by + 40, CLR_DARK_GREY);
    for (int i = 0; i < clue_n; i++) {
        print("-", bx + 10, by + 24 + i * 12, CLR_RED);
        print(clue_list[i], bx + 18, by + 24 + i * 12, CLR_LIGHT_PEACH);
    }
    print_centered("click to close", SCREEN_W/2, by + h - 11, CLR_DARK_GREY);
}

static void draw_accuse(void) {
    int w = 250, h = 130, bx = (SCREEN_W - w) / 2, by = 40;
    dither_panel(bx, by, w, h, CLR_RED);
    print_centered("NAME THE KILLER", SCREEN_W/2, by + 6, CLR_RED);

    const char *opt[3] = { "SILAS  - the barman", "THE INFORMANT - the snitch", "NOBODY - close the case cold" };
    int mx = mouse_x(), my = mouse_y();
    for (int i = 0; i < 3; i++) {
        int ry = 70 + i * 30;
        bool hot = (my >= ry && my < ry + 22);
        rectfill(bx + 16, ry, w - 32, 20, hot ? CLR_DARK_RED : CLR_BLACK);
        rect(bx + 16, ry, w - 32, 20, hot ? CLR_RED : CLR_DARK_GREY);
        print_centered(opt[i], SCREEN_W/2, ry + 7, hot ? CLR_LIGHT_PEACH : CLR_LIGHT_GREY);
    }
    print_centered("right-click to back out", SCREEN_W/2, by + h - 11, CLR_DARK_GREY);
}

static void draw_title(void) {
    cls(CLR_BLACK);
    rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BROWNISH_BLACK);
    fog_band(150, 40, CLR_DARKER_BLUE);
    draw_rain(CLR_PINK);
    const char *t = "NEON RAIN";
    int bw = text_width(t) * 3;
    int c = blink(30) ? CLR_PINK : CLR_DARK_PURPLE;        // neon flicker
    print_scaled(t, (SCREEN_W - bw) / 2, 54, c, 3);
    print_centered("a rain-soaked murder case", SCREEN_W/2, 90, CLR_TRUE_BLUE);
    print_centered("- one night, one death -", SCREEN_W/2, 104, CLR_DARK_GREY);
    if (blink(22)) print_centered("click to light a cigarette", SCREEN_W/2, 150, CLR_LIGHT_YELLOW);
    vignette();
}

static void draw_end(void) {
    cls(CLR_BROWNISH_BLACK);
    fog_band(150, 40, CLR_DARKER_BLUE);
    draw_rain(end_win ? CLR_LIME_GREEN : CLR_DARK_RED);
    vignette();
    int w = 270, h = 90, bx = (SCREEN_W - w) / 2, by = 40;
    dither_panel(bx, by, w, h, end_win ? CLR_GREEN : CLR_RED);
    print_centered(end_win ? "CASE CLOSED" : "THE CASE GOES COLD", SCREEN_W/2, by + 10, end_win ? CLR_GREEN : CLR_RED);
    // wrap the reason crudely onto two lines
    int half = slen(end_reason) / 2;
    while (half < slen(end_reason) && end_reason[half] != ' ') half++;
    print_centered(clipn(end_reason, half), SCREEN_W/2, by + 30, CLR_LIGHT_PEACH);
    print_centered(end_reason + (end_reason[half] ? half + 1 : half), SCREEN_W/2, by + 42, CLR_LIGHT_PEACH);
    if (blink(20)) print_centered("click to work it again", SCREEN_W/2, by + h - 12, CLR_LIGHT_YELLOW);
}

// ── the frame ──────────────────────────────────────────────────────────────────
void draw(void) {
    if (g_state == ST_TITLE) { draw_title(); draw_cursor(); return; }
    if (g_state == ST_END)   { draw_end();   draw_cursor(); return; }

    switch (g_scene) {
    case SC_STREET: draw_street(); break;
    case SC_APT:    draw_apt();    break;
    case SC_BAR:    draw_bar();    break;
    case SC_ALLEY:  draw_alley();  break;
    case SC_OFFICE: draw_office(); break;
    }

    // atmosphere on top of every scene
    fog_band(150, 40, ACCENT2[g_scene]);
    draw_rain(ACCENT[g_scene]);
    vignette();
    // overlay dim — set EVERY frame so a modal panel's dim can't stick after it
    // closes (fade is author-owned sticky state; the engine never clears it).
    float dim = (trans > 0) ? clamp(trans, 0, 1) : 0.0f;   // fade-in on travel
    if (g_state == ST_NOTE)   dim = 0.5f;                   // notebook dims the scene
    if (g_state == ST_ACCUSE) dim = 0.55f;                  // accuse panel dims harder
    fade(dim);

    // hovered hotspot glint
    if (g_state == ST_PLAY && hov_act && blink(14)) {
        // nothing extra: the cursor ring already signals it
    }

    draw_hud();

    if (g_state == ST_MSG)    draw_msgbox();
    if (g_state == ST_DLG)    draw_dialogue();
    if (g_state == ST_NOTE)   draw_notebook();
    if (g_state == ST_ACCUSE) draw_accuse();

    draw_cursor();
}
