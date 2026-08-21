// pro.h — "may this player use the paid thing?", for a cart.
//
// The cart-land face of the in-app purchase (ADR-0035: an app is FREE and one non-consumable
// "Pro" unlock buys the paths that carry audio OUT of the app — WAV export, MIDI, the AUv3).
// Plan + the whole picture: docs/design/pro-unlock.md.
//
// THE ONE RULE: a cart never names a product id and never mentions StoreKit. It asks
// pro_unlocked() and draws accordingly. Which product, at what price, comes from the app
// manifest via a generated header, so a cart is the same cart inside a paid app, inside the
// free web gallery and in the editor.
//
//   if (ui_button(x, y, w, h, "WAV")) {
//       if (pro_unlocked()) export_wav();
//       else                pro_sheet_open(&sheet);   // the cart owns `sheet`
//   }
//   ...
//   pro_sheet(&sheet);          // last in draw(), like cursor_draw()
//
// ── WHAT IT ANSWERS WHERE, and why the defaults lean the way they do ────────────────────────
//   · iOS/macOS APP with the unlock bought .......... unlocked
//   · iOS/macOS APP without it ...................... LOCKED, and pro_can_purchase() is true
//   · AUv3 EXTENSION ................................ reads the App Group the app wrote; cannot
//                                                     purchase (pro_can_purchase() false), so the
//                                                     sheet says "open the app to unlock"
//   · desktop / editor / the wasm gallery ........... UNLOCKED. There is no store there. The wall
//                                                     is an App Store business model, not a
//                                                     product boundary, and the gallery is a
//                                                     funnel — see pro-unlock.md §4.
//   · a bundle whose manifest declares no Pro product  UNLOCKED (nothing to sell = nothing to gate)
//
// ⚠ THE FAIL-OPEN TRAP, and it already had teeth. The Store_* bridge is linked as WEAK
// DEFINITIONS below, so a build with no Swift store still links (that is how the editor and the
// web build work), and those stubs answer "unlocked". That is right for a build with no store and
// CATASTROPHIC for one that has a store but forgot to link the answer: before 2026-08-19 no AU
// target compiled Store.swift or AppGroup.swift, so a gated cart inside the plug-in would have
// linked the stub and handed Pro to everyone, silently. The fix is on the Swift side
// (ios/Sources/Entitlements.swift is a STRONG, fail-closed definition that every AU target now
// compiles). If you add a target that runs a cart, add that file to it.
//
// STATELESS ON PURPOSE. This header declares no file-scope state, so it needs no
// DE_CTX_STATICS block (docs/design/engine-context.md) and can never end up in a saved-state
// slice. The sheet's state is a ProSheet the CART owns, exactly like physics.h's arrays.
#pragma once
#include <stdbool.h>
#include <stdio.h>
#include "studio.h"
#include "ui.h"

// The Pro product for THIS app, baked from the manifest by tools/build-app.js. Absent outside a
// bundle (the editor, a bare `play.js` run, the web build), which is the "no store" case.
#if defined(__has_include)
#  if __has_include("app_pro.h")
#    include "app_pro.h"
#  endif
#endif
#ifndef APP_PRO_ID
#define APP_PRO_ID    ""      // no product declared → pro_unlocked() is true
#define APP_PRO_PRICE ""
#endif

// ── the Store bridge (ios/Sources/Entitlements.swift, @_cdecl over StoreKit 2) ───────────────
// WEAK DEFINITIONS, not weak_import: a weak undefined *reference* does not link on the current
// Darwin ld, a weak definition does. Swift's strong symbols override these inside an app.
// THIS IS THE ONLY COPY — do not re-declare them in a cart (tinyjam-menu.c used to).
__attribute__((weak)) bool Store_IsModuleUnlocked(const char *id) { (void)id; return true; }
__attribute__((weak)) void Store_Purchase(const char *id)         { (void)id; }
__attribute__((weak)) void Store_Restore(void)                    { }
__attribute__((weak)) bool Store_CanPurchase(void)                { return false; }
__attribute__((weak)) void Store_ResetPurchases(void)             { }
__attribute__((weak)) bool Store_TestingAvailable(void)           { return false; }

// ── the questions a cart asks ────────────────────────────────────────────────────────────────
static inline const char *pro_id(void)    { return APP_PRO_ID; }     // "" = this app sells nothing
static inline const char *pro_price(void) { return APP_PRO_PRICE; }  // display string, e.g. "4.99"

// Is there anything to sell here at all? False in the editor, on desktop, in the web build, and
// in any app whose manifest declares no Pro product. Use it to hide the whole shopfront.
static inline bool pro_for_sale(void) { return APP_PRO_ID[0] != '\0'; }

// THE GATE. True = run the paid path.
static inline bool pro_unlocked(void) {
    return !pro_for_sale() || Store_IsModuleUnlocked(APP_PRO_ID);
}

// Can THIS process put a purchase sheet on screen? False inside an AUv3 extension, which is a
// separate sandboxed process with no StoreKit. Draw "open the app to unlock" instead of a button
// that does nothing.
static inline bool pro_can_purchase(void) { return pro_for_sale() && Store_CanPurchase(); }

static inline void pro_buy(void)     { if (pro_can_purchase()) Store_Purchase(APP_PRO_ID); }
static inline void pro_restore(void) { if (pro_can_purchase()) Store_Restore(); }

// A rack/module unlock (Tiny Jam's per-rack IAP + master pass) — the CONTENT axis, next to Pro's
// feature axis. Same bridge, a different product id, and the launcher is its only caller.
static inline bool pro_module_unlocked(const char *id) { return !id || !id[0] || Store_IsModuleUnlocked(id); }
static inline void pro_module_buy(const char *id)      { if (id && id[0] && Store_CanPurchase()) Store_Purchase(id); }

// ── the shared Pro sheet ─────────────────────────────────────────────────────────────────────
// One look for the offer across every app, for the same reason fxicons.h exists: three carts
// drawing their own paywall is three paywalls. The cart owns the struct (zero-initialised =
// closed), this draws it.
typedef struct {
    bool   open;
    float  buy_at;      // when pro_buy() fired — StoreKit takes 0.5–5s, so the sheet says "…"
    int    lines;       // how many PRO_LINES entries to show (0 = all)
} ProSheet;

#define PRO_SHEET_W 210
#define PRO_SHEET_H 108

static inline void pro_sheet_open(ProSheet *s)  { if (s) { s->open = true; } }
static inline void pro_sheet_close(ProSheet *s) { if (s) { s->open = false; s->buy_at = 0; } }

// Draw + handle. Call LAST in draw(), like cursor_draw(). Returns true while it is up (so the
// cart can skip its own input that frame). Closes itself the moment the unlock lands.
// `what` = the one-line pitch, e.g. "Export WAV, MIDI and the AUv3 plug-in".
static inline bool pro_sheet(ProSheet *s, const char *what) {
    if (!s || !s->open) return false;
    if (pro_unlocked()) { pro_sheet_close(s); return false; }   // it landed — get out of the way

    int x = (screen_w() - PRO_SHEET_W) / 2, y = (screen_h() - PRO_SHEET_H) / 2;
    fade(0.55f);
    rectfill(x, y, PRO_SHEET_W, PRO_SHEET_H, CLR_DARK_BLUE);
    rect(x, y, PRO_SHEET_W, PRO_SHEET_H, CLR_LIGHT_GREY);
    print("PRO", x + 10, y + 9, CLR_YELLOW);
    print(what ? what : "Unlock the paid features", x + 10, y + 24, CLR_WHITE);

    bool waiting = s->buy_at > 0 && now() - s->buy_at < 30.0f;
    int by = y + PRO_SHEET_H - 46;
    if (pro_can_purchase()) {
        char label[48];
        if (waiting) snprintf(label, sizeof label, "one moment...");
        else if (pro_price()[0]) snprintf(label, sizeof label, "unlock  $%s", pro_price());
        else snprintf(label, sizeof label, "unlock");
        if (ui_button(x + 10, by, PRO_SHEET_W - 20, 18, label) && !waiting) {
            s->buy_at = now(); pro_buy();
        }
        if (ui_button(x + 10, by + 22, 92, 15, "restore")) pro_restore();
        if (ui_button(x + PRO_SHEET_W - 102, by + 22, 92, 15, "not now")) pro_sheet_close(s);
    } else {
        // an AUv3 extension, or a build with no store: no sheet to show, just the truth
        print("Open the app to unlock.", x + 10, by, CLR_LIGHT_GREY);
        if (ui_button(x + PRO_SHEET_W - 102, by + 22, 92, 15, "ok")) pro_sheet_close(s);
    }
    return true;
}
