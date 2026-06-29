/* de:meta
{
  "title": "bones",
  "status": "active",
  "kind": [
    "tool",
    "probe"
  ],
  "teaches": [
    "save-load-persistence"
  ],
  "lineage": "Original tool; no direct homage — a forward-kinematic skeleton editor paired with a hex-cell sequencer grid.",
  "description": "A pixel-man skeleton animator — pose an 18-bone stick figure and flip-book him to life. Each bone is a 1px line set to one of 16 rotations; you author motion in a sequencer grid (bones down the rows, 16 frames across, every cell a hex digit 0-f for that bone's angle) while a live forward-kinematic preview shows the assembled figure — turn the torso and the whole body swings with it. A playhead loops the frames at an adjustable FPS with eased in-betweens, so it moves smoothly despite the 16-step quantization. TAB flips to a RIG view where you set each body part's length AND line width (odd, 1-9px) (left & right share a value) and watch the figure re-proportion live; the whole animation AND its proportions persist via save_bytes, and it opens on a friendly default wave. Arrows move the cell marker (in RIG: pick a part + resize), Z/X turn the selected bone, C copies the previous frame, SPACE plays/stops, [ ] set FPS, - = set the loop length, S/L save/load — or click a cell and mouse-wheel to turn it, or just drag a limb in the preview to pose it directly (snaps to the nearest of 16)."
}
de:meta */
#include "studio.h"

// ============================================================
//  BONES — a pixel-man skeleton animator.
//
//  An 18-bone stick figure of 1px lines. You pose him by setting each bone's
//  ROTATION (1 of 16 directions, 22.5° apart) and author motion in a
//  SEQUENCER GRID: bones down the rows, 16 time frames across the columns.
//  Every cell holds one hex digit 0-f = that bone's rotation in that frame.
//  Hit play and a playhead sweeps the columns, animating the little guy in
//  the live preview on the right.
//
//  TWO VIEWS share the preview (TAB to switch):
//    ANIM — the sequencer grid (above).
//    RIG  — set the LENGTH and WIDTH (odd 1-9px, so thickness stays centered
//           on the bone) of each body part (left & right share one value),
//           watching the figure re-proportion live.
//
//  The skeleton is forward-kinematic: a bone's stored angle is RELATIVE to
//  its parent, so turning the torso swings everything attached.
//
//    TAB     switch ANIM <-> RIG          SPACE  play / stop
//    arrows  ANIM: move cell · RIG: pick part + set length
//    Z / X   ANIM: turn bone CCW/CW · RIG: part width 1-9 odd (also , / .)
//    C       copy the previous frame      [ ] fps     - = loop length
//    S / L   save / load (animation + proportions persist via save_bytes)
//    mouse   click · wheel turns/resizes · DRAG a limb in the preview to pose
// ============================================================

#define NB 18      // bones (grid rows)
#define NF 16      // frames (grid columns)
#define NPARTS 12  // length parameters (L/R share)
#define STEP 22.5f // degrees per rotation index

#define ANIM 0
#define RIG  1

// ── the skeleton table ── parent (-1 = root), rest local angle, region
// color, crossbar?, which end of the parent crossbar (0 none/1 L/2 R), tag
typedef struct { int parent; float rest; int region, cross, side; const char *tag; } Bone;
static const Bone BONE[NB] = {
    //  parent  rest   reg cr sd  tag
    {  1,   0.0f, 0, 0, 0, "hd"  },  // 0 head      (drawn as a circle on its tip)
    {  2,   0.0f, 0, 0, 0, "nk"  },  // 1 neck
    { -1, 270.0f, 0, 0, 0, "UT"  },  // 2 upper torso  (up from root)
    { -1,  90.0f, 0, 0, 0, "LT"  },  // 3 lower torso  (down from root)
    {  3, 270.0f, 0, 1, 0, "hip" },  // 4 hip crossbar (at lower-torso tip)
    {  2,  90.0f, 0, 1, 0, "sho" },  // 5 shoulder crossbar (at upper-torso tip)
    {  5,  90.0f, 1, 0, 1, "Lua" },  // 6 L upper arm  (off shoulder left end)
    {  6,   0.0f, 1, 0, 0, "Lla" },  // 7 L lower arm
    {  7,   0.0f, 1, 0, 0, "Lha" },  // 8 L hand
    {  5,  90.0f, 2, 0, 2, "Rua" },  // 9 R upper arm  (off shoulder right end)
    {  9,   0.0f, 2, 0, 0, "Rla" },  // 10 R lower arm
    { 10,   0.0f, 2, 0, 0, "Rha" },  // 11 R hand
    {  4,  90.0f, 3, 0, 1, "Lul" },  // 12 L upper leg (off hip left end)
    { 12,   0.0f, 3, 0, 0, "Lll" },  // 13 L lower leg
    { 13,  90.0f, 3, 0, 0, "Lft" },  // 14 L foot  (points outward, left)
    {  4,  90.0f, 4, 0, 2, "Rul" },  // 15 R upper leg (off hip right end)
    { 15,   0.0f, 4, 0, 0, "Rll" },  // 16 R lower leg
    { 16, 270.0f, 4, 0, 0, "Rft" },  // 17 R foot  (points outward, right)
};
// which length-part each bone draws from (L/R bones map to the same part)
static const int   PART [NB]     = { 0,1,2,3,4,5, 6,7,8, 6,7,8, 9,10,11, 9,10,11 };
static const char *PNAME[NPARTS] = { "head","neck","upper torso","lower torso",
    "hip width","shoulder wid","upper arm","lower arm","hand","upper leg",
    "lower leg","foot" };
static const int   PLEN0[NPARTS] = { 10,9,22,20,12,14,18,16,7,22,20,9 };   // length defaults
static const int   PWID0[NPARTS] = { 3,1,5,5,3,3,3,3,1,3,3,1 };            // width defaults (odd 1-9 px)
static const int   REGCOL[5]     = { CLR_LIGHT_GREY, CLR_BLUE, CLR_GREEN, CLR_ORANGE, CLR_PINK };
static const char *HEX = "0123456789abcdef";

// ── layout ──
#define GX 34      // grid left
#define GY 28      // grid top (first row)
#define CWp 11     // cell width
#define RS 11      // row stride
#define PVX 300    // preview root x
#define PVY 88     // preview root y
#define RY0 26     // rig list top
#define RRS 14     // rig row stride

// ── state ──
static int   partLen[NPARTS];
static int   partWid[NPARTS];
static unsigned char rot[NB][NF];     // 0..15 per cell
static int   view;                    // ANIM / RIG
static int   selBone, selFrame;       // the cell marker (ANIM)
static int   selPart;                 // selected length row (RIG)
static int   playFrame, anim_fps = 8, loopLen = NF;
static bool  playing;
static float playAcc;
static float saveMsg; static const char *saveTxt = "";
static int   dragBone = -1;

// ── per-frame computed pose ──
static float wAng[NB], pWorld[NB];
static float stx[NB], sty[NB], jx[NB], jy[NB];
static float xL[NB], yL[NB], xR[NB], yR[NB];
static float dispLocal[NB];
static bool  posed;

static int   mod16(int v) { return ((v % 16) + 16) % 16; }
static int   iround(float x) { return (int)(x + (x >= 0 ? 0.5f : -0.5f)); }
static float angwrap(float d) { while (d > 180) d -= 360; while (d < -180) d += 360; return d; }
static bool  hi(int b) { return view == ANIM ? b == selBone : PART[b] == selPart; }

// walk the tree (parents precede children); dispLocal eases toward the target
// so playback looks smooth despite the 16-step quantization.
static void compute_pose(int frame) {
    float k = clamp(dt() * 16.0f, 0.0f, 1.0f);
    for (int b = 0; b < NB; b++) {
        float L = (float)partLen[PART[b]];
        float target = BONE[b].rest + rot[b][frame] * STEP;
        dispLocal[b] = posed ? dispLocal[b] + angwrap(target - dispLocal[b]) * k : target;

        int p = BONE[b].parent;
        float px, py, pw;
        if (p < 0)              { px = PVX;  py = PVY;  pw = 0; }
        else if (BONE[p].cross) { pw = wAng[p];
                                  if (BONE[b].side == 1) { px = xL[p]; py = yL[p]; }
                                  else                   { px = xR[p]; py = yR[p]; } }
        else                    { px = jx[p]; py = jy[p]; pw = wAng[p]; }

        pWorld[b] = pw; stx[b] = px; sty[b] = py;
        float w = pw + dispLocal[b];
        wAng[b] = w;

        if (BONE[b].cross) {
            xL[b] = px + dx(L, w + 180); yL[b] = py + dy(L, w + 180);
            xR[b] = px + dx(L, w);       yR[b] = py + dy(L, w);
            jx[b] = xR[b]; jy[b] = yR[b];
        } else {
            jx[b] = px + dx(L, w); jy[b] = py + dy(L, w);
        }
    }
    posed = true;
}

static void copy_prev(void) {
    int src = (selFrame - 1 + NF) % NF;
    for (int b = 0; b < NB; b++) rot[b][selFrame] = rot[b][src];
}

// ── save / load (animation + proportions in one blob) ──
typedef struct { int magic, fps, loopLen; unsigned char rot[NB][NF]; int partLen[NPARTS], partWid[NPARTS]; } Anim;
#define MAGIC 0x424F4E34  // "BON4"
static void save_anim(void) {
    Anim a = { MAGIC, anim_fps, loopLen };
    for (int b = 0; b < NB; b++) for (int f = 0; f < NF; f++) a.rot[b][f] = rot[b][f];
    for (int p = 0; p < NPARTS; p++) { a.partLen[p] = partLen[p]; a.partWid[p] = partWid[p]; }
    save_bytes(&a, sizeof a);
    saveMsg = 1.4f; saveTxt = "SAVED";
    note(72, INSTR_SINE, 4);
}
static bool load_anim(void) {
    Anim a;
    if (load_bytes(&a, sizeof a) != (int)sizeof a || a.magic != MAGIC) return false;
    anim_fps = mid(1, a.fps, 30);
    loopLen = mid(2, a.loopLen, NF);
    for (int b = 0; b < NB; b++) for (int f = 0; f < NF; f++) rot[b][f] = a.rot[b][f] & 15;
    for (int p = 0; p < NPARTS; p++) { partLen[p] = mid(2, a.partLen[p], 40); partWid[p] = mid(1, a.partWid[p], 9) | 1; }  // |1 keeps width odd (centered)
    return true;
}

// friendly default so the tool opens alive — an 8-frame wave + idle bob.
static void seed_default(void) {
    static const unsigned char arm []  = { 8, 9, 10, 9, 8, 9, 10, 9 };
    static const unsigned char hand[]  = { 0, 15, 14, 15, 0, 15, 14, 15 };
    loopLen = 8;
    for (int f = 0; f < NF; f++) {
        rot[9][f]  = arm [f % 8];
        rot[10][f] = hand[f % 8];
        rot[0][f]  = 1;
        rot[12][f] = (f % 4 < 2) ? 1 : 15;
        rot[15][f] = (f % 4 < 2) ? 15 : 1;
    }
}

void init(void) {
    for (int p = 0; p < NPARTS; p++) { partLen[p] = PLEN0[p]; partWid[p] = PWID0[p]; }
    selBone = 9; selFrame = 0; selPart = 2; view = ANIM;
    if (!load_anim()) seed_default();
}

void update(void) {
    // ── global: playback clock + view + save ──
    if (playing) {
        playAcc += dt();
        float spf = 1.0f / anim_fps;
        while (playAcc >= spf) { playAcc -= spf; playFrame = (playFrame + 1) % loopLen; }
    } else playAcc = 0;

    if (keyp(KEY_TAB))   view = !view;
    if (keyp(KEY_SPACE)) { playing = !playing; if (playing) { playFrame = selFrame; playAcc = 0; } }
    if (keyp('S')) save_anim();
    if (keyp('L')) { if (load_anim()) { saveMsg = 1.4f; saveTxt = "LOADED"; } }

    int mx = mouse_x(), my = mouse_y();
    float wheel = mouse_wheel();

    if (view == ANIM) {
        if (keyp(KEY_UP))    selBone  = (selBone - 1 + NB) % NB;
        if (keyp(KEY_DOWN))  selBone  = (selBone + 1) % NB;
        if (keyp(KEY_LEFT))  selFrame = (selFrame - 1 + NF) % NF;
        if (keyp(KEY_RIGHT)) selFrame = (selFrame + 1) % NF;

        if (keyp('X') || keyp('.')) rot[selBone][selFrame] = mod16(rot[selBone][selFrame] + 1);
        if (keyp('Z') || keyp(',')) rot[selBone][selFrame] = mod16(rot[selBone][selFrame] - 1);
        if (keyp('C')) copy_prev();
        if (keyp('[')) anim_fps = max(1, anim_fps - 1);
        if (keyp(']')) anim_fps = min(30, anim_fps + 1);
        if (keyp('-')) loopLen = max(2, loopLen - 1);
        if (keyp('=')) loopLen = min(NF, loopLen + 1);

        // grid mouse: click selects a cell, wheel turns it
        if (mouse_pressed(MOUSE_LEFT) && mx >= GX && mx < GX + NF * CWp && my >= GY && my < GY + NB * RS) {
            selFrame = (mx - GX) / CWp; selBone = (my - GY) / RS;
        }
        if (wheel != 0) rot[selBone][selFrame] = mod16(rot[selBone][selFrame] + (wheel > 0 ? 1 : -1));
    } else {
        // ── RIG view: pick a part, resize it ──
        if (keyp(KEY_UP))    selPart = (selPart - 1 + NPARTS) % NPARTS;
        if (keyp(KEY_DOWN))  selPart = (selPart + 1) % NPARTS;
        if (keyp(KEY_LEFT))  partLen[selPart] = max(2,  partLen[selPart] - 1);
        if (keyp(KEY_RIGHT)) partLen[selPart] = min(40, partLen[selPart] + 1);
        if (keyp('Z') || keyp(',')) partWid[selPart] = max(1, partWid[selPart] - 2);  // odd steps
        if (keyp('X') || keyp('.')) partWid[selPart] = min(9, partWid[selPart] + 2);

        if (mouse_pressed(MOUSE_LEFT) && mx < 210 && my >= RY0 && my < RY0 + NPARTS * RRS)
            selPart = (my - RY0) / RRS;
        if (wheel != 0) partLen[selPart] = mid(2, partLen[selPart] + (wheel > 0 ? 1 : -1), 40);
    }

    // ── drag-pose: grab a limb tip in the preview, snap to nearest of 16 (both views) ──
    if (mouse_pressed(MOUSE_LEFT) && mx >= 214 && posed) {
        float best = 14; dragBone = -1;
        for (int b = 0; b < NB; b++) {
            float d = distance((int)jx[b], (int)jy[b], mx, my);
            if (d < best) { best = d; dragBone = b; }
        }
        if (dragBone >= 0) { if (view == ANIM) selBone = dragBone; else selPart = PART[dragBone]; }
    }
    if (!mouse_down(MOUSE_LEFT)) dragBone = -1;
    if (dragBone >= 0 && view == ANIM) {
        float want  = angle_to((int)stx[dragBone], (int)sty[dragBone], mx, my);
        float local = want - pWorld[dragBone] - BONE[dragBone].rest;
        rot[dragBone][selFrame] = mod16(iround(local / STEP));
    }

    if (saveMsg > 0) saveMsg -= dt();
}


// ── shared figure preview ──
static void draw_preview(void) {
    line(212, 14, 212, SCREEN_H - 2, CLR_DARK_BLUE);
    line(216, PVY + 66, SCREEN_W - 4, PVY + 66, CLR_DARKER_GREY);   // ground

    for (int b = 0; b < NB; b++) {
        int col = hi(b) ? CLR_WHITE : REGCOL[BONE[b].region];
        int wdt = partWid[PART[b]];
        if (BONE[b].cross) {
            thickline((int)xL[b], (int)yL[b], (int)xR[b], (int)yR[b], wdt, col);
        } else if (b == 0) {                                   // head: neck-stub + skull + eye
            thickline((int)stx[b], (int)sty[b], (int)jx[b], (int)jy[b], wdt, col);
            circfill((int)jx[b], (int)jy[b], 3 + wdt / 2, col);
            pset((int)jx[b] - 2, (int)jy[b] - 1, CLR_BLACK);
        } else {
            thickline((int)stx[b], (int)sty[b], (int)jx[b], (int)jy[b], wdt, col);
        }
        if (!BONE[b].cross) pset((int)stx[b], (int)sty[b], CLR_LIGHT_YELLOW);
    }
}

static void draw_grid(int curFrame) {
    for (int f = 0; f < NF; f++) {                              // frame-number header
        bool on = (f == curFrame), down = (f % 4 == 0);
        print(str("%c", HEX[f]), GX + f * CWp + 2, GY - 9,
              on ? CLR_WHITE : down ? CLR_LIGHT_GREY : CLR_DARK_GREY);
    }
    for (int b = 0; b < NB; b++) {
        print(BONE[b].tag, 2, GY + b * RS + 2, REGCOL[BONE[b].region]);
        for (int f = 0; f < NF; f++) {
            int x = GX + f * CWp, y = GY + b * RS;
            bool selCell = (b == selBone && f == selFrame);
            bool head = (f == curFrame), beatCol = (f % 4 == 0);
            int bg = selCell ? CLR_WHITE : head ? CLR_DARKER_BLUE
                   : beatCol ? CLR_DARKER_PURPLE : CLR_BROWNISH_BLACK;
            rectfill(x, y, CWp - 1, RS - 1, bg);
            print(str("%c", HEX[rot[b][f]]), x + 2, y + 2,
                  selCell ? CLR_BLACK : REGCOL[BONE[b].region]);
        }
    }
    rect(GX + selFrame * CWp - 1, GY + selBone * RS - 1, CWp + 1, RS + 1, CLR_YELLOW);
    rect(GX - 2, GY - 1, NF * CWp + 1, NB * RS + 1, CLR_DARK_GREY);
    print(str("> %s", BONE[selBone].tag), 216, 16, REGCOL[BONE[selBone].region]);

    int hy = 184, hc = CLR_DARK_GREY;
    print("arrows  move cell",  216, hy,      hc);
    print("Z / X   turn bone",  216, hy + 8,  hc);
    print("C  copy prev frame", 216, hy + 16, hc);
    print("SPACE  play / stop", 216, hy + 24, hc);
    print("[ ] fps   - = loop", 216, hy + 32, hc);
    print("S / L  save / load", 216, hy + 40, hc);
    print("wheel / drag = pose",216, hy + 48, CLR_INDIGO);
}

static void draw_rig(void) {
    print("PART  LENGTH   WIDTH", 4, 16, CLR_LIGHT_PEACH);
    for (int p = 0; p < NPARTS; p++) {
        int y = RY0 + p * RRS;
        bool sel = (p == selPart);
        if (sel) rectfill(2, y - 1, 206, RRS, CLR_DARKER_BLUE);
        print(PNAME[p], 6, y + 2, sel ? CLR_WHITE : CLR_LIGHT_GREY);
        print_right(str("%d", partLen[p]), 128, y + 2, sel ? CLR_YELLOW : CLR_MEDIUM_GREY);
        bar(132, y + 3, 38, 5, partLen[p] / 40.0f, sel ? CLR_GREEN : CLR_BLUE_GREEN, CLR_DARKER_GREY);
        print(str("w%d", partWid[p]), 175, y + 2, sel ? CLR_PEACH : CLR_MEDIUM_GREY);
        for (int i = 0; i < partWid[p]; i++)                 // a swatch at the actual thickness
            line(194, y + 4 + i, 205, y + 4 + i, sel ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    print(str("> %s", PNAME[selPart]), 216, 16, CLR_WHITE);

    int hy = 184, hc = CLR_DARK_GREY;
    print("up/down  pick part", 216, hy,      hc);
    print("left/right  length", 216, hy + 8,  hc);
    print("Z / X  width 1-9",   216, hy + 16, hc);
    print("wheel  length",      216, hy + 24, hc);
    print("SPACE  play / stop", 216, hy + 32, hc);
    print("S / L  save / load", 216, hy + 40, hc);
    print("TAB  back to anim",  216, hy + 48, CLR_INDIGO);
}

void draw(void) {
    int curFrame = playing ? playFrame : selFrame;
    compute_pose(curFrame);

    cls(CLR_BROWNISH_BLACK);

    // top bar with two tabs
    rectfill(0, 0, SCREEN_W, 12, CLR_DARK_BLUE);
    print("BONES", 4, 2, CLR_LIGHT_PEACH);
    print(" ANIM ", 56,  2, view == ANIM ? CLR_YELLOW : CLR_DARK_GREY);
    print(" RIG ",  104, 2, view == RIG  ? CLR_YELLOW : CLR_DARK_GREY);
    print_right(str("FRAME %d/%d   %d FPS   %s", curFrame + 1, loopLen, anim_fps, playing ? "\x10PLAY" : "STOP"),
                SCREEN_W - 4, 2, playing ? CLR_GREEN : CLR_LIGHT_GREY);

    if (view == ANIM) draw_grid(curFrame);
    else              draw_rig();

    draw_preview();

    if (saveMsg > 0) print(saveTxt, 216, 26, CLR_GREEN);
}
