# Brief: build a support page for "Tiny Jam" (App Store support URL)

**For:** an agent working in the website / gallery repo (github.io or the mipolai.com site) —
NOT this repo. This brief is self-contained; everything you need is inline.

**Why this exists:** Apple **requires** a support URL to submit an iOS app for review, and the
reviewer actually opens it — a broken, parked, empty, or irrelevant page is a common rejection.
Right now Tiny Jam has no support URL, and it's the one thing blocking submission. Your job: ship
a small, real, reachable support page and hand back its **public URL**.

## The one input needed from the maker (do not guess)
- **Contact email** to publish on the page (e.g. a `support@…` or personal address). Everything
  else below is provided. If it's not given to you, ask before shipping — the page is useless
  without a way to reach the developer.

## What the page must do (the whole bar)
A user who taps "App Support" on the App Store listing lands here and can **get help or reach the
developer**. That means, at minimum:
1. **A contact method** — the email above, plainly visible (a `mailto:` link is ideal). A form is
   fine too but not required.
2. **A short, relevant FAQ** — a few lines specific to Tiny Jam (see suggested Q&A below).
3. **Clear app identity** — the app name + a one-line description so it's obviously the right page.

It does **not** need: login, accounts, a ticketing system, analytics, or fancy design. Simple and
honest matches the product.

## Where it must live
- A **stable, public HTTPS URL** that returns 200 (not behind auth, not a redirect to a parked
  domain). Options, cheapest first:
  - A page in the existing **github.io gallery** (guaranteed live) — e.g. `/tinyjam/support/`.
  - A page on **mipolai.com** — e.g. `mipolai.com/tinyjam/support` (on-brand; bundle id is
    `com.mipolai.tinyjam`).
- Keep it **distinct from the marketing/developer URL** (Apple wants support = "get help",
  marketing = "learn about the app"). Same domain, different path is fine.

## App facts (use verbatim; don't invent new marketing copy)
- **Name:** Tiny Jam  ·  **Full title:** Tiny Jam: Pocket Music Toys
- **Platform:** iOS (iPhone + iPad)  ·  **Price:** Free with in-app purchases  ·  no ads, no tracking
- **What it is (the maker's own words):**
  > Tiny Jam is a growing collection of pocket-sized music toys. Each one is a small, playable
  > instrument — a groovebox, a synth, a drum machine — that fits in a pocket and starts making
  > sound the second you open it. No menus to learn, no projects to manage: tap a step, turn a
  > knob, and it plays. New instruments join the collection over time, so it grows with you.
- **In-app purchases (for the FAQ):**
  - "Acid Rack" ($2.99) — unlocks the acid rack (two 303s, a 909, an 808)
  - "E-Piano" ($2.99) — unlocks a warm electric-piano keybed
  - "Master Pass" ($5.00) — unlocks every rack, now and future
  - (Some instruments are free to play without any purchase.)

## Suggested FAQ (adapt in the maker's plain, honest voice — no fluff)
- **How do I unlock an instrument?** Tap a locked rack → "unlock" → it's a one-time in-app purchase.
- **I bought a rack / the Master Pass and it's not showing up.** Use **Restore Purchases** in the
  app (or on this page, tell them where that button is). If it still doesn't restore, email us.
- **Is my data collected?** No ads, no tracking, no accounts.
- **Something's broken / I have a request.** Email [the contact address] — it's one person, you'll
  reach them directly.

## Constraints
- **Maker's own words only.** Reuse the description above and write the FAQ in the maker's honest,
  plain voice. Do NOT generate glossy marketing prose (house rule for this product).
- **Self-contained + reachable.** Inline CSS is fine; no external dependencies that could 404.
- Theme/branding to taste, but simplicity > polish.

## Deliverable (hand back)
- The **final public support URL**. It will be written into `apps/tinyjam/metadata/en-US/support_url.txt`
  in the app repo (this is the file Apple's support URL is pushed from via `tools/asc-push.js`).
- (Optional but welcome) if you also stand up a marketing/about page, hand back that URL too — it
  fills the app's `marketing_url` slot.

## Acceptance check
- URL returns 200 over HTTPS, no auth wall, no parked-domain redirect.
- Page names Tiny Jam, shows a working contact method, and has app-relevant help.
- Distinct path from the marketing URL.
