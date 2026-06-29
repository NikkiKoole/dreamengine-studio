# Tinyjam launch kit — Strum-harp (omnichord) · Stage 1 drafts

> **Status:** DRAFT copy (2026-06-29). The first concrete instance of the **Stage-1 web outreach kit**
> defined in [`tinyjam-marketing.md`](tinyjam-marketing.md) §7. This file holds the *writing tasks* (✍️)
> — positioning, tribe posts, wishlist copy. The *build tasks* (➤ web demo, share codec, hero clip,
> wishlist plumbing, the single-cart microsite) are tracked there, not here.
>
> These are **drafts to react to, not final copy.** `[LINK]`, `[SONG URL]`, `[CLIP]` are placeholders
> to fill once the demo + share codec + clip exist.

---

## ⚠️ Two guardrails before anything goes out

1. **Trademark — vibe, not logotype** ([marketing §2](tinyjam-marketing.md)). "Omnichord" / "Q-Chord"
   are **Suzuki's marks.** Use them only as a *nominative reference* ("inspired by", "for Omnichord
   fans") — **never as the product name, the page title, or in the artwork/logo.** The product carries
   an **original name** (below). The cautionary tale is real: ReBirth's iPad app was deleted over a
   Roland IP claim.
2. **Tone — gift, not ask** ([marketing §9.1](tinyjam-marketing.md)). Show up as one enthusiast sharing
   a free toy, lead with a playable link + a song you made, ask one humble question. Never "beta-test
   my product / validate my startup."

## Verified channels for this module ([marketing §3.5](tinyjam-marketing.md))

- **"Suzuki Omnichord & Qchord"** Facebook group (the main bullseye)
- **Omnichord Heaven** (omnichord-heaven.com — endorsed resource + FB page)
- **Omnichord Discord** (~479 members)
- **Loopy Pro forum** (the iOS/AUv3 cross-cutting channel)
- Ambient / lo-fi YouTube + the omnichord hashtag on IG/TikTok (for the clip)

---

## 0. Product name — pick one (decision)

"Omnichord" can't be the name. Working title below is **Strumglow**; alternatives to choose from:

| Option | Feel |
|---|---|
| **Strumglow** *(working pick)* | warm, sunny, lo-fi |
| **Chordglow** | leans on the auto-chord |
| **Harpwave** | the strum-plate sweep |
| **Strumceleste** | softer, celestial |

> Decision owner: pick the name (and the page URL, e.g. `tinyjam.<domain>/strumglow`) before posting —
> the copy below uses **Strumglow** as a find-and-replace placeholder.

---

## 1. Positioning ✍️

**Name:** Strumglow
**Tagline:** *Strum a chord, sweep the plate — instant sunshine.*

**What is this (one paragraph):**
> Strumglow is a tiny, pixel-art strum-harp inspired by the classic 1981 Omnichord: tap a chord button,
> run your finger across the strum plate, and it shimmers. Free to play right in your browser — made for
> anyone who loves that auto-harp sound but doesn't have £900 for the reissue.

**Three quick points (for the page):**
- 🎵 Auto-chords + a strum plate — playable in seconds, no theory needed.
- 📱 Runs in the browser, on your phone, no signup.
- 🔗 Made a loop you like? Copy a link — it plays for whoever you send it to.

**App Store subtitle (Stage 2, for later):** *Pocket strum-harp · play, loop, share.*

---

## 2. Facebook post — "Suzuki Omnichord & Qchord" group ✍️

> Long-time lurker, first post 👋 I've been a bit obsessed with the Omnichord since the OM-108 dropped,
> but can't really justify the price right now — so I did the next best thing and made a tiny pixel-art
> strum-harp you can play right in your browser. Free, no signup, works on a phone: **[LINK]**
>
> It's got the strum plate and the auto-chords, and it's honestly still rough around the edges. Here's a
> little loop I made with it so you can hear the vibe: **[SONG URL]**
>
> I'd genuinely love to know what it gets wrong to an actual Omnichord player's ears — the strum feel?
> the chord voicings? Tear it apart, it'll only get better. 🙏

*Notes:* no app, no price, no "please share" — just the toy + a question. Reply to comments personally.
Post the clip **[CLIP]** as the attached media if the group allows video.

---

## 3. Loopy Pro forum post ✍️

> **A little browser strum-harp (Omnichord-flavoured) — feedback welcome**
>
> Hi all 👋 I've been building a tiny browser-based strum-harp — pixel-art, Omnichord-flavoured, free to
> mess with (no signup, runs in iOS Safari too): **[LINK]**
>
> Right now it's a web toy I'm using to figure out whether the idea has legs. Quick clip with sound:
> **[CLIP]** — and a playable loop you can open and tweak: **[SONG URL]**
>
> If there's interest I'd love to take it native + AUv3 down the line so you could strum it straight into
> AUM/Loopy — but first I want to get the *instrument* right. Brutal feedback very welcome, especially on
> the feel and the sound. Cheers!

*Notes:* this crowd is AUv3-literate, so naming the AUv3 *possibility* (as a "if there's interest", not a
promise) is the right hook here — it's the channel where that future sells itself.

---

## 4. Wishlist copy ✍️

**Button:** 🔔 Tell me when the app lands
**Sub-line:** Drop your email and I'll send **one** message — when Strumglow hits the App Store. No spam,
unsubscribe anytime.
**Confirmation:** You're on the list. I'll email you exactly once, when it's real. Thanks for helping
shape it. 🙏

*Notes:* tag this list `wishlist:strumglow` so Stage-2 launch + sales emails can target just this module.

---

## What's left for this module (build tasks → [marketing §7.1](tinyjam-marketing.md))

- ➤ Build the **omnichord cart to wasm**, mobile-friendly, hosted on the **dedicated page** (own domain,
  COOP/COEP host — §7.2).
- ➤ Wire the **lz-string share button** (§6.1).
- ➤ Generate the **9:16 hero clip with audio** (make-gif.js + dancer — §4.1).
- ➤ Stand up the **wishlist signup** (tagged).
- ✍️ Fill the `[LINK]` / `[SONG URL]` / `[CLIP]` placeholders, do the name find-and-replace.
