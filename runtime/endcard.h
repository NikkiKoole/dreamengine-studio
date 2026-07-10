#ifndef ENDCARD_H
#define ENDCARD_H

// endcard.h — the shared WIN/LOSE end-screen treatment. Include AFTER studio.h,
// like ui.h / cards.h.
//
// Fixes the recurring "win screen fades to half" issue (action-plan.md → "A nicer
// shared end-state"): a hand-rolled flat fade(0.5) + an instantly-appearing box
// reads as unfinished. This header owns the one good version, in three beats:
//
//   1. DIM   — the world behind recedes on an eased ~0.6s curve: a smooth fade
//              plus an ordered-Bayer dither of black pixels that steps in with
//              it, so the curtain lands with lo-fi grain instead of a flat
//              half-transparent wash.
//   2. CARD  — a drop-shadowed, double-bordered panel POPS in (ease_back
//              overshoot) once the dim is underway.
//   3. WORDS — yours. When .settled turns true, print your title / lines /
//              blinking prompt inside the returned rect (and any decoration —
//              a popping heart, a trophy). The card owns the chrome, the cart
//              owns the words.
//
// Usage — call every frame the end state is showing, t = seconds since it began:
//
//   win_t += dt();
//   EndCard c = endcard(win_t, 240, 60, 48, CLR_DARK_BLUE, CLR_LIGHT_YELLOW, CLR_INDIGO);
//   if (c.settled) {
//       print_centered("YOU WIN!", SCREEN_W/2, c.y + 12, CLR_LIGHT_YELLOW);
//       print_centered("A closing line.", SCREEN_W/2, c.y + 26, CLR_LIGHT_PEACH);
//       if (blink(18)) print_centered("- click to play again -", SCREEN_W/2, c.y + c.h - 14, CLR_LIGHT_GREY);
//   }
//
// colors: bg = card fill, edge = outer border, edge2 = inner border — keep each
// cart's own palette; the shared part is the motion + chrome, not the colors.

typedef struct {
    int  x, y, w, h;   // the card's resting rect (valid every frame, even mid-pop)
    bool settled;      // true once the pop has landed — gate your text on this
} EndCard;

// The dim alone (reusable for death-tints etc): an ordered-Bayer dissolve that
// eases in and STAYS at ~10/16 black coverage — the Genesis/SNES pause-curtain.
// Deliberately NOT fade(): fade() is a window-space pass drawn over EVERYTHING
// at present time, so it dims the card + its text along with the world, and it
// never shows in --dump frames / gifs / baked thumbnails. This dissolve lives
// in the canvas: the world recedes, the card stays bright, captures are honest.
static void endcard_dim(float t) {
    float p = ease_out(clamp(t / 0.6f, 0, 1));
    static const int bayer[16] = { 0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5 };
    int level = (int)(p * 10.0f + 0.5f);             // settles at 10/16 covered
    if (level > 0) {
        int pat = 0;
        for (int c = 0; c < 16; c++) if (bayer[c] >= level) pat |= (1 << (15 - c));
        fillp(pat, -1);                              // 1-bits stay transparent
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK);
        fillp_reset();
    }
}

static EndCard endcard(float t, int w, int h, int cy, int bg, int edge, int edge2) {
    endcard_dim(t);

    EndCard c = { (SCREEN_W - w) / 2, cy, w, h, t >= 0.55f };

    // pop: starts once the dim is underway, ease_back overshoot over 0.3s
    float s = ease_back(clamp((t - 0.25f) / 0.3f, 0, 1));
    if (s <= 0) return c;
    int pw = (int)(w * s), ph = (int)(h * s);
    if (pw < 4 || ph < 4) return c;
    int px = c.x + (w - pw) / 2, py = cy + (h - ph) / 2;

    rectfill(px + 3, py + 3, pw, ph, CLR_BLACK);     // drop shadow
    rectfill(px, py, pw, ph, bg);
    rect(px, py, pw, ph, edge);
    if (pw > 8 && ph > 8) rect(px + 2, py + 2, pw - 4, ph - 4, edge2);
    return c;
}

#endif // ENDCARD_H
