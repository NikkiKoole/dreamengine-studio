# Nintendo Switch as a target — feasibility notes

STATUS: IDEA / parked (2026-06-29). Nothing built, nothing committed to. Captured because the iOS
work got the maker dreaming about console. The short verdict: **technically very feasible — the
hard part is legal/logistical, not the port.** And the iOS spike already retired most of the
*engineering* risk.

Companion reading: [`ios-plan.md`](ios-plan.md) (the AOT cart model this leans on),
[`engine-portability.md`](engine-portability.md) (the platform seam + software-canvas decision),
[`../decisions/0023-ship-carts-as-apps-not-the-editor.md`](../decisions/0023-ship-carts-as-apps-not-the-editor.md)
(we ship finished carts as apps, precompiled — never the editor).

## Why the iOS work already did the hard part

The two constraints that block Switch are the *same two* iOS forced us to solve:

1. **No JIT.** Switch (official builds) bans executable-memory JIT, exactly like the iOS App Store.
   So the `libtcc` live-compile path can't ship there either — Switch needs the **AOT-compiled cart
   baked into the app** model, which the iOS spikes (6.5 signed standalone on a real iPhone; 7
   self-hosted render proof) already validated. That architecture transfers directly.
2. **Software canvas is the gift.** A console port's riskiest piece is the GPU/graphics backend. The
   `DE_CPU_RASTER` software canvas lets us hand the platform a finished CPU framebuffer
   (`sw_cbuf[SCREEN_W*SCREEN_H]`) and blit one texture — no GL backend required on day one. This is
   the same reason iOS chose the software-canvas render path (see `ios-plan.md`). On Switch it dodges
   the fact that Raylib has **no official Switch backend** (the retail SDK is under NDA).

So the platform seam being designed for iOS in `engine-portability.md` is the same seam Switch would
plug into. Switch is not a from-scratch effort — it's a third backend behind a seam built for two.

## The two roads (very different)

### Homebrew — devkitPro / libnx
- `devkitA64` + `libnx` is a mature C toolchain. Audio via miniaudio; graphics via the software
  canvas straight to the framebuffer (or community Raylib-on-Switch GL ES, not needed for us).
- Feasible as a **weekend-to-two-week spike** given the iOS groundwork.
- Runs only on homebrew-enabled (hacked) consoles. **Cannot ship on the eShop.** This is the "I made
  my engine run on a Switch I own" version — a portability proof, not a product.

### Official — Nintendo Developer Portal
- Apply → approval → NDA → real devkit + the NDA'd SDK. **devkitPro cannot be used for retail.**
- The bottleneck is Nintendo's approval, **not the code**. Indies get in, but it's a relationship +
  a plausible product, not an instant signup.
- Switch 1 is late in its life (mid-2026). For a *product*, the real target is **Switch 2** — its
  dev-program terms are what would matter.

## Hacking a unit for the homebrew spike

Hackability depends entirely on the hardware revision:
- **Launch-era "Erista" (2017–mid-2018):** unpatchable Tegra X1 boot-ROM bug (Fusée Gelée / RCM).
  Jig + USB payload → custom firmware. Coldboot, hardware-level, **reversible** (hack lives on the SD
  card). The gold standard for homebrew dev — easy and safe.
- **"Mariko" and later (revised Switch, Lite, OLED, Switch 2):** boot ROM patched. Requires a
  **soldered modchip** (Picofly/HWFLY) — fiddly, brick risk, not casual.
- Check before buying: serial-prefix "is my Switch patchable" checkers, or manufacture date before
  ~mid-2018.
- Caveats: Nintendo bans hacked consoles from online services → keep a **dedicated offline test
  unit** (which is what you'd want for engine testing anyway); voids warranty.

Recommendation: a **cheap second-hand launch-model Erista** makes a perfect dedicated homebrew test
box. Don't hack a current/primary console.

## Does an App Store presence help get a Nintendo dev account?

Some, but less than intuition suggests — and not for the reason people assume. Approval vets
**"will this become a real, finished, shippable product, and are you a legitimate business,"** not
engineering competence. What actually moves the needle:
- **A shipped, finished game** anywhere (Steam / App Store / itch / web) — the strongest signal. An
  App Store *game* counts; an App Store *utility* much less.
- **A registered business entity** (even a one-person LLC) — smooths the contract/NDA step.
- **A concrete pitch for the specific Switch title** (footage, why it fits) — they approve products,
  not engines.

What doesn't help much: a tech demo, an engine with no game on top, or non-game apps.

**Highest-leverage move:** ship **one actual finished game made in dreamengine** on the easiest
platform (iOS / web / Steam). It simultaneously proves the engine end-to-end, *becomes* the pitch
artifact for the Nintendo application, and is the thing you'd port anyway. The application itself is
free and the bar is "show us you can finish and ship," so apply early once one finished thing exists.

## If/when we pursue it — the first spike

A homebrew spike mirroring the iOS ladder: get `studio.c`'s main loop running under devkitPro with
the **software canvas blitting to the framebuffer**, audio stubbed, then add input, then audio. A
contained proof that reuses the AOT cart model directly, retiring the engineering risk before any
Nintendo paperwork. (Not started — left as a sketch.)
