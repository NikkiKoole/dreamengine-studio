# Tinyjam launch kit: Strum-harp (omnichord), Stage 1 drafts

> **Status:** DRAFT copy (2026-06-29). The first concrete instance of the **Stage-1 web outreach kit**
> defined in [`tinyjam-marketing.md`](tinyjam-marketing.md) §7. This file holds the writing tasks
> (positioning, tribe posts, wishlist copy). The build tasks (web demo, share codec, hero clip,
> wishlist plumbing, the single-cart microsite) are tracked there, not here.
>
> These are drafts to react to, not final copy. `[LINK]`, `[SONG URL]`, `[CLIP]` are placeholders to
> fill once the demo, share codec and clip exist.
>
> **House style for outward copy: no spaced em dashes, no emojis.** They read as bot-written marketing,
> and forum musicians bounce off that fast. Plain sentences, commas and periods. Hyphenated compounds
> (pixel-art, strum-harp) are fine.

---

## Two guardrails before anything goes out

1. **Trademark: vibe, not logotype** ([marketing §2](tinyjam-marketing.md)). "Omnichord" and "Q-Chord"
   are Suzuki's marks. Use them only as a plain reference ("inspired by", "for Omnichord fans"), never
   as the product name, the page title, or in the artwork. The product carries an original name (below).
   The cautionary tale is real: ReBirth's iPad app was deleted over a Roland IP claim.
2. **Tone: gift, not ask** ([marketing §9.1](tinyjam-marketing.md)). Show up as one enthusiast sharing a
   free toy. Lead with a playable link plus a song you made, ask one honest question. Never "beta-test
   my product" or "validate my startup".

## Verified channels for this module ([marketing §3.5](tinyjam-marketing.md))

- "Suzuki Omnichord & Qchord" Facebook group (the main bullseye): https://www.facebook.com/groups/Omnichord.QChord/
- Omnichord Heaven (endorsed resource): https://www.omnichord-heaven.com/ , FB page https://www.facebook.com/Omnichord.Heaven/
- Omnichord Discord (around 479 members): invite links rotate, find the current one via Omnichord Heaven or the FB group
- Loopy Pro forum (the iOS/AUv3 cross-cutting channel): https://forum.loopypro.com/
- r/synthesizers: https://www.reddit.com/r/synthesizers/
- Ambient and lo-fi YouTube, plus the omnichord hashtag on IG/TikTok (for the clip)

**Extra audience (cozy-cartoon / Adventure Time-adjacent):** the shimmer fits that crowd's taste, so
they're a natural place to show Strumharpy ([marketing §3.10](tinyjam-marketing.md)). Caveat: this is
an *etiquette* matter, not IP (we ship nothing AT). Fan spaces (r/adventuretime, fan Discords) often
ban self-promo, so gift-tone only and favour promo-tolerant / fan-creation corners, or lean on the
overlapping cozy / chiptune / ukulele communities. A vibe comparison in copy is fine ("cozy cartoon
music, the Adventure Time world"); no logos, art, or implied endorsement.

---

## 0. Product name: Strumharpy (decided) — and the harpy mascot

**Strumharpy** it is. "Omnichord" can't be the name, and rather than dodge the "harpy" overtone we
**lean into it**: the faceplate mascot is a cute pixel-art **harpy — the mythological bird-woman —
strumming the harp**. That flips a naming risk into the cart's identity: an original, ownable character
(our own little BMO, but ours), distinctive and instantly memorable, and a perfect star for the hero
clip and the social art.

Alternatives considered (kept for the record): Strumdrum (strum+drum rhyme, nods to the built-in
rhythm), Strumglow, Chordglow, Harpwave.

> Page URL: `tinyjam.<domain>/strumharpy`.

---

## 1. Positioning

**Name:** Strumharpy
**Tagline:** Strum a chord, sweep the plate, instant sunshine.

**What is this (one paragraph):**
> Strumharpy is a tiny pixel-art strum-harp inspired by the classic 1981 Omnichord. Tap a chord button,
> run your finger across the strum plate, and it shimmers. It's free to play right in your browser, made
> for anyone who loves that auto-harp sound but doesn't have 900 quid for the reissue.

**Three quick points (for the page):**
- Auto-chords plus a strum plate. Playable in seconds, no theory needed.
- Runs in the browser, on your phone, no signup.
- Made a loop you like? Copy a link and it plays for whoever you send it to.

**Mascot:** a cute pixel-art harpy (the mythological bird-woman) who strums the harp — the cart's face,
the hero-clip star, and the social-art hook. Original character, ours; sprite-draw it as a `.cart.js`
(see the sprite-authoring guide), iterate with `sprite-preview.js`.

**App Store subtitle (Stage 2, for later):** Pocket strum-harp. Play, loop, share.

---

## 2. Facebook post: "Suzuki Omnichord & Qchord" group

> Long-time lurker, first post here. I've been a bit obsessed with the Omnichord since the OM-108
> dropped, but can't really justify the price right now, so I did the next best thing and made a tiny
> pixel-art strum-harp you can play right in your browser. Free, no signup, works on a phone: [LINK]
>
> It's got the strum plate and the auto-chords, and it's still pretty rough. Here's a little loop I made
> with it so you can hear the vibe: [SONG URL]
>
> I'd love to know what it gets wrong to an actual Omnichord player's ears. The strum feel? The chord
> voicings? Tear it apart, it'll only get better.

Notes: no app, no price, no "please share", just the toy and a question. Reply to comments yourself.
Post the clip [CLIP] as attached media if the group allows video.

---

## 3. Loopy Pro forum post

> **A little browser strum-harp (Omnichord-flavoured), feedback welcome**
>
> Hi all. I've been building a tiny browser-based strum-harp. Pixel-art, Omnichord-flavoured, free to
> mess with, no signup, runs in iOS Safari too: [LINK]
>
> Right now it's a web toy I'm using to figure out whether the idea has legs. Quick clip with sound:
> [CLIP]. And a playable loop you can open and tweak: [SONG URL]
>
> If there's interest I'd love to take it native and AUv3 down the line so you could strum it straight
> into AUM or Loopy, but first I want to get the instrument right. Brutal feedback very welcome,
> especially on the feel and the sound. Cheers.

Notes: this crowd is AUv3-literate, so naming the AUv3 possibility (as a "if there's interest", not a
promise) is the right hook here. It's the channel where that future sells itself.

---

## 4. Wishlist copy

**Button:** Tell me when the app lands
**Sub-line:** Drop your email and I'll send one message, when Strumharpy hits the App Store. No spam,
unsubscribe anytime.
**Confirmation:** You're on the list. I'll email you exactly once, when it's real. Thanks for helping
shape it.

Notes: tag this list `wishlist:strumharpy` so the Stage-2 launch and sales emails can target just this
module.

---

## What's left for this module (build tasks, see [marketing §7.1](tinyjam-marketing.md))

- (build) The omnichord cart to wasm, mobile-friendly, hosted on the dedicated page (own domain,
  COOP/COEP host, see §7.2).
- (build) Wire the lz-string share button (§6.1).
- (build) Generate the 9:16 hero clip with audio (make-gif.js plus the dancer, §4.1).
- (build) Stand up the wishlist signup (tagged).
- (write) Fill the `[LINK]` / `[SONG URL]` / `[CLIP]` placeholders, do the name find-and-replace.
