# Effects bus architecture — reorderable inserts, multi-reverb, reverb-as-bus, sidechain

**Status: Increments A, C, D all SHIPPED 2026-06-12.** A: reorderable inserts (`fx_order`). C:
multi-reverb + reverb-as-bus-insert + effects-after-reverb (`reverb_bus_fx`) — see §5. **D: sidechain
& bus compression (`sidechain`/`sidechain_key`/`glue`) — the sc_key send accumulator + envelope→gain
stage of "Increment D" below, shipped; `groovebox` PUMP/GLUE is the showcase.** A design map for
three *independent* increments to the master-FX / aux-bus layer. The point of this doc is that the
engine is already shaped for all three, and to record the exact costs so the build isn't re-derived
from scratch.

> **✅ Increment A SHIPPED (2026-06-12)** — `fx_order(bus, kinds, n)` + `FX_*` constants; the
> per-bus `insert_order[]` the audio thread walks; byte-identical when unused. Showcase: the
> **pedalboard** cart rebuilt as a drag-and-drop chain builder. See STATUS.md (2026-06-12). The §3
> sketch below is what landed (modulo the roster growing to 8 inserts incl. tremolo + phaser).
>
> **Conclusion (2026-06-12): build A, then C. Skip B as a standalone — C subsumes it.** C costs
> the same memory as B (same tank pool) for marginally more routing code, is strictly more
> capable (effects *after* the reverb), composes with A, and can still expose B's dead-simple
> "send knob" as its friendly face. Its one disadvantage — breaking bit-identical *old* renders —
> turned out to be a **one-time re-baseline with no committed suite in this repo to break** (see
> §6), so it doesn't gate the decision. Rationale worked out below. Detail-level companion to the parked stub in
[`sound-next-steps.md`](sound-next-steps.md) → "Parked — reorderable effects chain". Bounded by
[decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md) (effects roster is
closed — this is *routing* work, not new effects) and the §8.10 effects layer in
[`instrument-engines.md`](instrument-engines.md).

> Line numbers below are anchors at time of writing (2026-06-12) and **drift** as `sound.h`
> is edited — they name the *region*, not a frozen address. `sound.h` is a hot shared file:
> re-read the region before editing, compile-gate, and sentinel-check the commit (CLAUDE.md).

---

## 0. "I want an effect" — recipe, or real bus effect? **The pedalboard test**

Before building any effect, ask one question:

> **Would I ever want it as a pedal on the pedalboard?** (i.e. dropped into a chain, reordered
> against other effects, run on a whole mix or a routed instrument.)

- **Yes → it must be a *real bus effect*** — a rostered `FX_*` insert the audio thread walks via
  `fx_order` (or a send/pinned-output stage). Examples: reverb, chorus, flanger, tape, wah, bitcrush,
  EQ, tremolo, **stereo auto-pan**.
- **No — it only ever shapes one voice/note inside one cart, never reordered → a *recipe*** is right:
  a per-voice LFO (`instrument_lfo` + `LFO_*`), an envelope (`instrument_env`), a `note_*` sweep, a
  filter move. Examples: a clav's per-note filter-quack, a vibrato on one held pad, the **bark**
  (the engine `morph` macro).

**Why this is decisive, not taste:** the pedalboard runs **bus inserts**. A per-voice recipe
(`LFO_PAN`, a `note_cutoff` env, …) is *categorically not an insert* — it cannot be placed in an
`fx_order` chain. So "I'll make it a per-voice recipe" and "I want it as a pedal" are **mutually
exclusive**. Wanting the pedal *forces* the bus-effect form. This **sharpens
[decision 0015](../decisions/0015-effects-are-recipes-not-primitives.md)** ("a new primitive must
prove it can't be a recipe"): **"it needs to be a pedalboard pedal / a reorderable bus insert" IS that
proof** — a recipe simply can't be one.

**Then, before minting a brand-new `FX_*` kind, check if it's a MODE of an effect you already have**
(0015's "extend, don't bloat"). The clearest case: **stereo auto-pan is just *stereo tremolo*** — the
same amplitude LFO applied **antiphase** to L/R instead of in-phase. So it's best added as a stereo
mode/shape of the existing `tremolo` insert, not a whole new pedal — one constant + a few lines, and
the TREMOLO pedal gains a stereo toggle.

**Worked examples (the epiano machines, 2026-06):**

| want | verdict | why |
|---|---|---|
| **stereo auto-pan** (Rhodes Suitcase vibrato) | **real bus effect** (a stereo mode of `tremolo`) | you want it as a pedal AND it pans the whole output incl. the tail — the per-voice `LFO_PAN` recipe can't be an insert |
| **Dyno** (bright Rhodes) | **recipe of existing pedals** | = `chorus` + `eq` presence — both are *already* rostered bus effects/pedals; no new effect |
| **bark / dig-in** | **recipe** (per-instrument) | it's the engine `morph` macro on one voice; never a pedal |

So the rule of thumb when an effect idea shows up: **picture it as a stompbox first.** If it belongs
on the board, build the bus effect (and check it isn't a mode of an existing one); if it only ever
lives inside one instrument's voice, keep it a recipe.

---

## 1. The mental model: the engine is already a mixing console

This is the load-bearing realization. The audio render loop in `runtime/sound.h` already
implements per-channel strips + a master strip — the band scenario (guitar + pedalboard, keys
+ pedalboard, drums, master) maps onto it directly, and so do all three increments below.

The structure, traced through one sample of the render loop (`sound.h` ~3576–3822):

```
per-voice ── each voice writes its panned contribution into ONE bus:
             busL[v->bus] += cL                                  (~3777)

per-bus  ── for b = 1..7: run that bus's INSERT chain in place, then fold to master:
             if (wah_used[b])   wah_process(b, &busL[b], &busR[b]);   (~3789–3796)
             if (cho_used[b])   chorus_process(...); ... eq, crush
             busL[0] += busL[b]; busR[0] += busR[b];                  (~3797)

master   ── mixL = busL[0]  (~3807); then TWO ordered stages:
             1. SEND RETURNS (parallel): echo wet, reverb wet added on top  (~3816, ~3840)
             2. INSERT CHAIN (series): wah→chorus→flanger→tape→eq→crush     (~3803–3808 region)
             3. soft-clip limiter — always last                            (~3818)
```

- **8 buses** (`SOUND_FX_BUSES`, `sound.h:498`). Bus 0 = master; buses 1–7 = per-instrument aux.
- An aux bus is auto-allocated **one per instrument slot** on first insert use
  (`fx_bus_for`, `sound.h:3145`). All of one slot's inserts **share its one bus** — a guitar
  with 6 pedals is 1 bus + 6 inserts, not 6 buses.
- **Inserts are per-bus** (chorus/flanger/tape/eq/crush/wah each have `*_used[SOUND_FX_BUSES]`
  state arrays). **Sends (echo, reverb) are ONE shared tank each** (`sound.h:497`) — every
  instrument sends into the same reverb; only the send *amount* varies per slot.

The two facts that matter for what follows: **(a)** an aux bus *is already* a series insert
chain that folds into master; **(b)** the insert order is the one thing that is **hardcoded**,
and reverb/echo are the one thing that is **shared, not per-bus**.

---

## 2. The pedalboard is a friendly fiction (the motivating case)

[`pedalboard.c`](../../tools/carts/pedalboard.c) draws six stompboxes left→right as if they
were a signal chain. They are not — the cart says so at `pedalboard.c:17`:

> *(The engine applies master inserts in its own fixed order; the board layout is the guitar reading.)*

And the two orders don't even agree: the board reads `BITCRUSH·EQ·CHORUS·FLANGER·TAPE·REVERB`
but the engine runs `…chorus·flanger·tape·eq·crush`. So bitcrush is *first* on the board,
*last* in the engine; eq is *second* on the board, *second-to-last* in the engine. And REVERB,
the rightmost "pedal," is a **send** (`instrument_reverb`, `pedalboard.c:123`), not an insert
at all. Reordering the board does nothing audible today. Increment A is what makes the drag
mean something.

---

## 3. Increment A — per-bus reorderable inserts

**Lowest cost, lowest risk, fully self-contained.** No new buffers. Turns the hardcoded ladder
into a data-driven loop. Only changes *the order the existing inserts are visited in*, per bus.

### A.1 Name the inserts; give each bus an order array (near `sound.h:504`)

```c
enum { FX_WAH, FX_CHORUS, FX_FLANGER, FX_TAPE, FX_EQ, FX_CRUSH, N_INSERTS };

// per-bus visit order. Default = today's fixed ladder, so an un-reordered bus
// is byte-identical to current output.
static uint8_t fx_order[SOUND_FX_BUSES][N_INSERTS];
static uint8_t fx_order_n[SOUND_FX_BUSES];   // populated slot count
// reset(): each bus → {WAH,CHORUS,FLANGER,TAPE,EQ,CRUSH}, n = 6
```

### A.2 One dispatch fn (replaces the six `if`s)

```c
static void apply_insert(int kind, int b, float *L, float *R) {
    switch (kind) {
        case FX_WAH:     if (wah_used[b])   wah_process(b, L, R);     break;
        case FX_CHORUS:  if (cho_used[b])   chorus_process(b, L, R);  break;
        case FX_FLANGER: if (flg_used[b])   flanger_process(b, L, R); break;
        case FX_TAPE:    if (tape_used[b])  tape_process(b, L, R);    break;
        case FX_EQ:      if (eq_used[b])    eq_process(b, L, R);      break;
        case FX_CRUSH:   if (crush_used[b]) crush_process(b, L, R);   break;
    }
}
```

### A.3 The render loop becomes a loop

Both the per-bus block (`sound.h:3789-3796`) and the master block (`~3803-3808`) collapse to:

```c
for (int s = 0; s < fx_order_n[b]; s++)
    apply_insert(fx_order[b][s], b, &busL[b], &busR[b]);
```

### A.4 The API (the four-place wiring)

One new request kind `SR_FX_ORDER` carrying `bus` + a packed list of kinds (6 kinds × 3 bits
fit in one int):

```c
void fx_order(int bus, const int *kinds, int n);   // bus 0 = master, 1.. = an instrument's bus
```

Then `pedalboard.c` re-sends `fx_order(my_bus, kinds, 6)` whenever a pedal is dragged to a new
position. The board layout *becomes* the signal chain.

### A.5 Notes / decisions

- **Byte-identical preserved.** The `_used[b]` gates stay inside `apply_insert`, so an untouched
  bus runs zero inserts → bit-identical to today. Default order = today's order → the
  default-order case is literally the same work.
- **CPU:** a switch dispatch instead of a straight branch, at ≤6 inserts — negligible.
- **The soft-clip stays pinned outside the reorderable list** (`sound.h:3818`). It's a safety
  limiter / amp output stage, not a tone pedal. Reorder reorders *pedals*; the clip is always last.
- **Visual-only reorder is NOT worth doing alone** — it would imply an audio change that
  doesn't happen. Ship A.1–A.4 together or not at all.

---

## 4. Increment B — multi-reverb send bus

**Costs memory, not CPU.** Today there's exactly one reverb tank (the `rvb_*` globals,
`sound.h:439-448`); the shared-tank decision (`sound.h:497`) was made *to avoid* paying its
~24 KB more than once. A band wants the drums in a tight room and the keys in a long plate —
**two reverbs at once** — which one tank can't do.

The cost driver: one reverb tank ≈ 6k floats (4 combs + 2 allpass + predelay) ≈ **24 KB**. So
the **flat per-bus array trick chorus used does not apply** (`cho_buf[SOUND_FX_BUSES][...]`
works because chorus's buffer is tiny; 8 × 24 KB = 192 KB of mostly-empty reverb tanks would
not). You need a **sparse tank pool + a `bus → tank` map**.

### B.1 Fold the loose globals into a Tank struct, make a small pool

```c
#define SOUND_REVERB_TANKS 3       // the budget knob: 3 × ~24KB ≈ 72KB. 2 (room+plate) ≈ 48KB

typedef struct {
    float comb1[REVERB_COMB_1], comb2[REVERB_COMB_2],
          comb3[REVERB_COMB_3], comb4[REVERB_COMB_4];   // the 24KB lives here
    float ap1[REVERB_ALLPASS_1], ap2[REVERB_ALLPASS_2];
    float predelay[REVERB_PREDELAY];
    int   cp1, cp2, cp3, cp4, ap_p1, ap_p2, pd_p;        // write positions
    float clp1, clp2, clp3, clp4;                        // per-comb damping LP states
    float fb, damp;                                      // config (was reverb_fb/reverb_damp)
    bool  used;                                          // per-tank dormancy
} ReverbTank;

static ReverbTank rvb_tank[SOUND_REVERB_TANKS];
```

### B.2 The indirection — the cheap dense map over the expensive sparse pool

Mirrors `fx_next_bus` / `fx_bus_for` (`sound.h:3145`) exactly — lazy-allocate, overflow bails dry:

```c
static int8_t bus_tank[SOUND_FX_BUSES];   // bus → tank index, or -1 = none. reset(): all -1
static int    rvb_next_tank = 0;          // next free tank
static int    rvb_tank_overflow = 0;      // dropped reverb requests (pool exhausted)

// Return the reverb tank for a bus, allocating one on first use. -1 = pool exhausted
// (caller bails → that bus stays dry; never silently shares another bus's tank).
static int tank_for(int bus) {
    if (bus_tank[bus] >= 0) return bus_tank[bus];
    if (rvb_next_tank >= SOUND_REVERB_TANKS) { rvb_tank_overflow++; return -1; }
    bus_tank[bus] = (int8_t)rvb_next_tank++;
    return bus_tank[bus];
}
```

`bus_tank` is 8 bytes; it indexes the expensive pool. That's the whole "sparse" idea: instead
of `tank[bus]`, it's `tank[ bus_tank[bus] ]`.

### B.3 `reverb_process` takes a tank (same DSP as `sound.h:468`, `t->` on everything)

```c
static float reverb_process(ReverbTank *t, float sample) {
    float pre = t->predelay[t->pd_p];
    t->predelay[t->pd_p] = sample;
    t->pd_p = (t->pd_p + 1) % REVERB_PREDELAY;
    float c1 = rvb_comb(pre, t->comb1, &t->cp1, REVERB_COMB_1, &t->clp1, t->fb, t->damp);
    float c2 = rvb_comb(pre, t->comb2, &t->cp2, REVERB_COMB_2, &t->clp2, t->fb, t->damp);
    float c3 = rvb_comb(pre, t->comb3, &t->cp3, REVERB_COMB_3, &t->clp3, t->fb, t->damp);
    float c4 = rvb_comb(pre, t->comb4, &t->cp4, REVERB_COMB_4, &t->clp4, t->fb, t->damp);
    float sum = (c1 + c2 + c3 + c4) * 0.25f;
    float a1 = rvb_allpass(sum, t->ap1, &t->ap_p1, REVERB_ALLPASS_1, 0.5f);
    return rvb_allpass(a1, t->ap2, &t->ap_p2, REVERB_ALLPASS_2, 0.5f);
}
```

`rvb_comb` / `rvb_allpass` (`sound.h:451`/`:460`) already take pointers — unchanged.

### B.4 Per-voice routing + send returns

Today a voice has `v->rvb` (amount) and the loop sums one `reverb_in` scalar (`sound.h:3578`,
voice loop). Add *which tank* + loop the returns:

```c
// per-voice: v->rvb (amount) + v->rvb_tank (0..N-1, default 0)
float reverb_in[SOUND_REVERB_TANKS] = {0};
// in the voice loop:
if (v->sfx_idx < 0 && v->rvb > 0.0005f) reverb_in[v->rvb_tank] += contrib * v->rvb;
// send return (replaces sound.h:3840):
for (int t = 0; t < SOUND_REVERB_TANKS; t++)
    if (rvb_tank[t].used) { float wet = reverb_process(&rvb_tank[t], reverb_in[t]); mixL += wet; mixR += wet; }
```

### B.5 The API

```c
void reverb_bus(int tank, float size, float damp);            // configure tank N (reverb() = tank 0)
void instrument_reverb_bus(int slot, int tank, float mix);    // route a slot to tank N
```

Drums → tank 0 (tight room), keys → tank 1 (long plate), guitar → tank 2 (short spring).
`reverb()` / `instrument_reverb()` keep working as sugar for tank 0.

---

> **✅ Increment C engine SHIPPED (2026-06-12).** `SOUND_REVERB_TANKS = 3`: the loose `rvb_*`
> globals are now a `ReverbTank` pool (tank 0 = legacy `reverb()` master send, **bytes-identical** —
> verified by a before/after `--wav --det` diff of `cathedral`; tanks 1–2 = send-buses). `FX_REVERB`
> is the 9th insert kind (the `fx_order` packing widened 3→4 bits across two payload ints). New API:
> **`reverb_bus(tank, size, damp)`** carves a space on its own aux bus (chain = `[FX_REVERB]`,
> wet-replace) + **`instrument_reverb_bus(slot, tank, mix)`** routes a slot's send into it. Showcase:
> the **reverb spaces** cart (a bright mallet in a tight room + a soft organ in a vast plate, ringing
> at once). Multi-reverb (the §4 Increment B win) is subsumed.
>
> **✅ Effects AFTER the reverb SHIPPED too (the fast-follow landed same-day).** The addressing gap is
> closed by **`reverb_bus_fx(tank, fx, a, b, c)`** — tank-keyed, so a cart never sees the auto-allocated
> bus index: it resolves tank→bus on the audio thread, configures the insert (crush/eq/tape/chorus) on
> that bus, and appends it after `FX_REVERB` in the chain (so the pedal chews the wet tail). `reverb→crush`
> is the demo (the **reverb spaces** CRUSH toggle: the plate decays into 5-bit dust). `SR_REVERB_BUS_FX`=68.
> So Increment C is fully cart-exposed: multi-reverb **and** effects-after-the-space.

## 5. Increment C — reverb as an insert on a dedicated bus

**= Increment B + the tank returns through an insert chain instead of as a parallel master
send.** This is what lets you put effects *after* the space: reverb→bitcrush, reverb→eq,
reverb→tape, even reverb→(master echo). The send model can't do that — its wet always returns
clean to master.

The reframe: an aux bus is *already* "a signal that runs a series insert chain and folds to
master" (`sound.h:3789-3797`). A reverb **send-bus** is just an aux bus whose **input is sends**
(not a voice's home bus), with **reverb as insert slot 0** of its (reorderable, Increment A)
chain. You still "send an amount in" — only *where the wet returns* changes.

### C.1 What it reuses from B

The whole tank pool + `bus_tank` indirection (B.1–B.3) is identical. C only changes the
*routing*, not the tank.

### C.2 Feeding the bus + the wet-replace dispatch case

A voice already writes to one bus (`sound.h:3777`). A send means *also* accumulating
`busL[rvb_bus] += cL * v->rvb` — nearly free, it's what `reverb_in` does today, retargeted to a
bus accumulator. Then reverb is a kind in the Increment-A `FX_*` enum:

```c
case FX_REVERB: {
    int t = bus_tank[b];
    if (t >= 0 && rvb_tank[t].used) {
        float wet = reverb_process(&rvb_tank[t], (*L + *R) * 0.5f);  // MONO core, v1
        *L = wet; *R = wet;   // wet-REPLACE: this bus carried nothing but sends
    }
} break;
```

The `*L = wet` (replace, not add) is load-bearing: it's what makes the bus carry pure wet, so
the inserts *after* `FX_REVERB` in `fx_order[b]` chew on the reverb tail. Put `FX_REVERB` first
and you have reverb→crush for free — the Increment-A machinery handles the rest.

### C.3 Why wet-only is enough here (the cost you dodge)

`reverb_process` returns **wet only** (`sound.h:467`). On a bus fed purely by sends, that's
exactly right — the bus carries only the send signal, so no internal dry/wet param is needed.
You'd *only* need an internal mix knob to put reverb in-line on an instrument's own **dry** bus,
which is not this feature. So C gets off easy on wet/dry **because** it's a send.

---

## Increment D — sidechain & bus compression (the side-chain input path)

**The piece that's genuinely new architecture, and the reason to design it carefully once:** a
**second input path**. Every effect so far reads one signal — its own bus. A sidechain comp reads
**two**: the *victim* (what gets ducked) and a *trigger/key* (what drives the ducking). The vocoder
has the same shape (carrier × modulator). `sound-next-steps.md` says it outright — *"design the
side-chain path once, during the bus refactor, so it isn't bolted on twice."* This section is that
design.

Motivating demand: the `house` station and `groovebox` both **fake** the pump (a per-voice
`note_cutoff` duck against the kick); Afrobeat voted compression a missing roster entry. The fake is
per-voice + filter-not-gain + event-triggered. The real thing is summed-bus + gain + audio-keyed.

### D.1 The side-chain SEND — a key accumulator (the reusable bit, shared with the vocoder)

A send already exists in the engine: `reverb_in`/`echo_in` are mono accumulators a voice adds into
(`sound.h:497`, the same shape as `busL[rvb_bus] += cL * v->rvb`). A side-chain key is **the same
thing with a different consumer**:

```c
#define N_SC_KEYS 3                       // independent trigger buses (kick / snare / …)
static float sc_key[N_SC_KEYS];           // mono trigger level, refilled each sample (like reverb_in)
```

During the per-voice write (`sound.h:~3777`), a voice flagged as a key source *also* adds its mono
contribution: `sc_key[v->sc_key] += (cL + cR) * 0.5f * v->sc_send;`. **This accumulator is the only
new input path** — and it is exactly what the vocoder needs (its modulator routes into a key; the
vocoder insert reads that key through its analysis filterbank instead of as a broadband level). Build
the key accumulator + routing here; vocoder reuses it. Keep the *accumulator* generic; the
*consumer* (broadband detector vs filterbank) is the effect's business.

### D.2 The detector + gain stage (the compressor itself)

Per active compressor: a one-pole envelope follower over its key, then a gain applied to the victim
bus. Peak detect (snappy, tracks the kick transient), no lookahead in v1 (matches the fake; a real
lookahead wants a tiny delay line — deferred):

```c
typedef struct { uint8_t used, victim_bus, key; float amount, atkCoef, relCoef, env; } SideChain;
static SideChain sc[SOUND_FX_BUSES];      // at most one keyed comp per victim bus

// in the per-bus / master stage, BEFORE the victim folds to master:
SideChain *s = &sc[b];
if (s->used) {
    float k = fabsf(sc_key[s->key]);
    s->env += (k > s->env ? s->atkCoef : s->relCoef) * (k - s->env);   // attack/release
    float duck = 1.0f - s->amount * clampf(s->env, 0, 1);              // gain reduction
    busL[b] *= duck; busR[b] *= duck;
}
```

- **Victim is any bus.** Master (`b==0`, the usual "duck the whole mix to the kick") applies after the
  master insert chain, before the soft-clip. A per-instrument victim ducks just that aux bus — nearly
  free, the gain multiply rides the bus loop that already runs.
- **Single-sample, no ordering trap.** The kick writes its bus *and* `sc_key` in the same per-voice
  pass, so by the per-bus stage `sc_key` holds the full summed trigger for sample *t*. No frame lag
  (unlike the cart fake, which is frame-quantized at 60 Hz — the engine version is sample-accurate).
- **GLUE = the self-keyed case.** A bus compressor with no external trigger is the same gain stage
  whose detector reads the **victim's own** summed level instead of a key. One bool (`s->key < 0` →
  read `busL[b]+busR[b]` instead of `sc_key`). That's the dormant GLUE knob, woken for free.

### D.3 The API (four-place wiring)

```c
void sidechain(int victim_bus, int key, float amount, int attack_ms, int release_ms);
   // duck victim_bus (0 = master) by up to amount (0..1), keyed off `key`. amount 0 = OFF (byte-identical).
void sidechain_key(int slot, int key, float send);   // route a slot into a trigger key. send 0 = not a trigger.
void glue(int victim_bus, float amount, int attack_ms, int release_ms);   // self-keyed bus comp (no trigger)
```

v1 is **amount + attack/release** (no threshold/ratio/knee) — deliberately, because that's exactly
what the groovebox PUMP already exposes, so the rewire is 1:1 and the cart can't expose params the
engine lacks. Threshold/ratio/soft-knee is a clean follow-up (more detector math, same plumbing).

### D.4 The groovebox rewire (1:1 — this is why the cart was built first)

The PUMP/GLUE knobs were designed onto this. Today (cart-side fake), tomorrow (engine):

| | today (faked) | after Increment D |
|---|---|---|
| trigger | `pumpEnv = 1` on each kick fire (frame-quantized) | `sidechain_key(SL_KICK, 0, 1.0f)` (sample-accurate) |
| duck | `note_cutoff` on the held PAD only, filter | `sidechain(0, 0, k_pump, ~1, ~120)` — gain on the **whole master mix** |
| breadth | one voice | bass + stab + pad + hats all pump together (the real summed pump) |
| GLUE | drawn but dead | `glue(0, k_glue, ~5, ~150)` — wakes up |

The hand-rolled `pumpEnv` / per-frame `note_cutoff` deletes; `k_pump` maps straight to `amount`. The
PUMP meter can read the engine's gain, or keep the cart envelope purely as the visual. **Net: the cart
gets simpler *and* more correct.** Showcase + acceptance test ready on day one.

### D.5 Cost & sequencing

- **Cheapest increment.** `sc_key[N_SC_KEYS]` floats + one `SideChain` per victim bus + a multiply
  per victim per sample. **No buffers.** Far under reverb. Gated on `amount`/`used` → unused is
  **byte-identical**, so **no bit-repro re-baseline** (unlike C).
- **Independent of A/B/C** — D touches the per-voice send accumulator + the bus/master gain stage; C
  touches send→tank routing. D could land before or after C. It does want the **same quiet tree** (it
  edits the hot per-voice write loop + the master stage), so it's **post-tuning**, like C.
- **Open calls when built:** (1) `N_SC_KEYS` = 2 or 3? (2) peak vs RMS detect (lean peak). (3) v1
  amount-only vs ship threshold/ratio (lean amount-only → 1:1 with the cart). (4) master-only victim
  first, or per-bus victim from the start (per-bus is ~free — expose `victim_bus`).

---

## 6. Cost ledger

| Concern | A: reorder | B: multi-reverb | C: reverb-as-bus |
|---|---|---|---|
| New buffers / memory | none | 24 KB × tanks (3 ≈ 72 KB) | same as B |
| New complexity | order array + dispatch | **tank pool + `bus→tank` map** | B's pool + wet-replace case |
| CPU | switch vs branch (negligible) | 1 `reverb_process`/tank/sample | same as B (relocated, not added) |
| Aux bus budget (7) | — | — | one bus per reverb send-bus |
| Byte-identical when unused | ✅ preserved | ✅ per-tank dormancy | ✅ per-tank dormancy |
| **Bit-identical OLD renders** | ✅ (default order = old) | ✅ if `reverb()`=tank 0 unchanged | ⚠️ **one-time re-baseline** (see below — *not* ongoing wobble; no committed suite to break) |
| Wet/dry param needed | n/a | no (send) | no (send-bus is wet by construction) |
| Cart migration | pedalboard re-sends order | `reverb()` = sugar for tank 0 | `reverb()`→bus is the clean end state |

### The bit-reproducibility tax (C-specific) — a one-time re-baseline, NOT ongoing wobble

Today reverb wet is added at the **master** stage (`sound.h:3840`). On an aux bus it folds in
earlier, at `:3797`. That changes floating-point **summation order**, so even a behaviorally
identical reverb won't be bit-for-bit identical to renders made *before* the change.

**The key clarification (2026-06-12): this is a single discontinuity, not permanent
non-determinism.** Two different things hide under "bit reproducibility":

- **Determinism** (same code + same input → same bits every run) is **untouched.** Float add is
  deterministic given a fixed order — it's only non-*associative*. C changes that order *once, in
  source*; after it lands, every run is bit-identical to every other run, forever. Permanent
  wobble would require real non-determinism (wall-clock / RNG / thread timing / uninitialized
  memory) — a fixed reorder is none of those.
- **Matching a snapshot taken before the change** is what goes stale — once. Re-render the
  baseline, and future-vs-future matches bit-for-bit indefinitely. (Analogy: renumbering houses
  on a street — confusing for one day, stable forever after; *not* houses that renumber nightly.)

**And in THIS repo the cost is even smaller than the general case:** there is **no committed
golden-wav regression suite** — every `.wav` lives in gitignored `build/`. The automated audio
tests are `tune-check` (measures **pitch** in cents, not bytes — reverb routing doesn't move a
fundamental, and the `tunecheck` cart doesn't use reverb, so it **won't notice**) and the
`soundcheck` dropped-request tripwire (also unaffected). `wav-analyze`'s byte-diff is an *ad-hoc*
manual tool, not CI. So the entire tax reduces to: *a WAV you saved by hand before the change
won't byte-match one saved after.* Nothing automated breaks; nothing needs re-baselining but your
own ad-hoc comparisons.

Still: migrating the existing `reverb()`/`instrument_reverb` onto the bus path (the clean end
state — otherwise there are *two* reverb mechanisms) is a semantic change to a hot file, so
confirm with `tune-check` + a fresh `--wav --det` before/after diff that only **reproducibility**
moved, not **behavior**, and follow the `sound.h` hot-file discipline (CLAUDE.md).

### No silent caps (B + C)

`tank_for` returning −1 ("bus stays dry") matches `fx_bus_for`'s discipline, but per CLAUDE.md's
no-silent-caps rule `rvb_tank_overflow` must surface as a `[sound] WARNING` (like
`sound_dropped`), so a cart asking for a 4th reverb on a 3-tank build finds out.

### Bus budget arithmetic

7 aux + master. A 3-piece band with its own pedalboards = 3 buses; + one shared reverb send-bus
= 4/7 (roomy). Three *different* reverbs = 3 reverb buses + 3 instrument buses = 6/7 (tight but
legal), + 3 × 24 KB of tanks.

### wasm / mobile footprint — split the two axes

**Memory: effectively free.** The Tank struct is zero-initialized `static` (`.bss`), so it adds
**0 bytes to the `.wasm` download** and 72 KB (3 tanks) of linear memory at load. For scale, the
320×200 RGBA render target alone is **256 KB** — all three tanks are under a third of *one
framebuffer*. Not a real consideration on either target. `SOUND_REVERB_TANKS = 2` (48 KB) is the
frugal setting only if you're squeezing wasm memory for unrelated reasons.

**CPU: the axis to watch — but it scales with *active* tanks, not the pool size.** Reverb is one
of the heavier per-sample effects (4 comb + 2 allpass ≈ 6 filter stages × 44.1 kHz per tank). The
saving grace: **dormant tanks cost zero.** `SOUND_REVERB_TANKS` is only the *ceiling*; a cart
lighting up one reverb pays exactly what one reverb costs today. You pay 3× only when three are
*simultaneously ringing* — rare (a band uses one or two). So the CPU question is never "is the
pool 3," it's "does this cart actually have N reverbs audible at once."

### Player bypass + the tail auto-sleep (a CPU safety valve)

A player can turn a tank off — it's just a bypass footswitch, identical to the pedal toggles in
`pedalboard.c`: `instrument_reverb_bus(slot, tank, 0)` stops feeding that reverb. **Subtlety:** a
configured tank keeps processing *every sample even with zero input*, because the reverb **tail**
must ring out — so "stomp it off" stops the *feed* but the tank still burns CPU while it decays.
To actually reclaim the cost, add a small **auto-sleep**: once the tail decays below a threshold,
flip `rvb_tank[t].used` back off so the tank goes truly idle (free again). A hard cutoff the
instant the send hits zero would guillotine the tail (an audible click), so the auto-sleep — not
an abrupt gate — is the right shape. **Optional refinement, not needed for a first cut.**

---

## 7. Sequencing & decision

**Agreed direction (2026-06-12): A first, then C. B is not built standalone — C subsumes it.**
**Update: A shipped 2026-06-12 (the pedalboard builder is its showcase); C is the next step.**

- **They're independent.** A touches only the insert section; B only the send section; C is the
  B tank-pool + one routing change. **A is the smallest, lowest-risk, and makes the pedalboard
  honest** — the natural first move, and it de-risks the reorder machinery C reuses.
- **Why C over B:** identical memory cost (the same tank pool), marginally more routing code,
  strictly more capable (effects *after* the reverb: reverb→bitcrush / →eq), and it **composes
  with A** — C's `FX_REVERB` is just another reorderable kind. Building A first means C's reverb
  is reorderable on day one for free.
- **C keeps B's simple face.** Cart authors who only want "drums in a room" never touch a bus:
  `instrument_reverb_bus(slot, tank, mix)` routes the send into a reverb-bus whose chain is
  `[FX_REVERB]` and nothing else. The power (downstream FX, reorder) is opt-in.
- **The one tax (bit-repro) doesn't gate it** — §6: one-time re-baseline, no committed suite in
  this repo to break, pitch test unaffected. The user has said bit-repro isn't a concern.
- **Still outside both:** echo gets the same multi-tank treatment as B/C if wanted (it's a
  cheaper single ring buffer, `echo_buf`, `sound.h:401`) — not sketched here, lower priority
  than "everyone in the same room."
- **Increment D (sidechain / bus comp) — ✅ SHIPPED 2026-06-12.** Independent of A/B/C and the
  cheapest (no buffers, no bit-repro tax). Ended the per-voice pump *fake* `house`/`groovebox` shipped,
  and landed the **side-chain input path the vocoder will reuse** (the `sc_key` accumulator) — paying a
  second debt. As built: master + per-bus victim, `glue()` is the self-keyed case, one comp per bus
  (the `groovebox` acceptance test surfaced that PUMP & GLUE must share the master comp). Sketched in
  [Increment D](#increment-d--sidechain--bus-compression-the-side-chain-input-path) above; verified
  compile-gate + 900-frame tripwire + tune-check exit 0.

Open calls still to make when C is actually built (not blockers on the direction):
1. **`SOUND_REVERB_TANKS` = 2 or 3?** (room+plate vs +spring; 48 KB vs 72 KB).
2. **Pre- or post-pan sends?** Today `reverb_in` is MONO pre-pan; a stereo bus send is a choice.
3. **Migrate `reverb()` onto the bus path now or later?** The clean end state is one mechanism
   (`reverb()` = sugar for a master reverb-bus); it can also land *after* C as a follow-up so the
   first C cut leaves the legacy master send untouched.

---

*Researched 2026-06-12 from the live `sound.h`. A menu, not a commitment. When any increment
ships, move its row to STATUS and add a §17 ledger note in
[`audio-notes.md`](audio-notes.md).*
