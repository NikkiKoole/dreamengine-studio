# roadlab — the working method (a replicable playbook, 2026-06-17)

STATUS: LIVING (playbook) — the replicable working loop behind roadlab; read before starting the next frontier.

> How the roadlab interchange work got built fast and stayed correct, written down so the **next phase**
> (at-grade junctions, network topology, the seed-driven world — see
> [`road-hierarchy-notes.md`](road-hierarchy-notes.md)) runs the same way. The artifact matters; the *loop*
> that produced it matters more. Map: [`road-geometry-handoff.md`](road-geometry-handoff.md).

## The loop (what we actually did, in order)
1. **Standard/research FIRST, then build.** Before any geometry we reconciled against OpenDRIVE + the
   roads.org.uk catalogue and wrote the data model down ([`junction-lanelink.md`](junction-lanelink.md)). The
   standard *validated* the design and named the milestones — we didn't invent a model and hope.
2. **A milestone ladder — one honest layer at a time.** M1 arc-spline → M2 clothoid → M3 multilane → M4
   width → M5 elevation → M6 schema → loop → flyover → generator → leg/skew → topology → ring. Each step is
   small, self-contained, compiles, and is **committed before the next**. No half-layers.
3. **Sandbox-first, pure + deterministic.** roadlab is one cart, no RNG / audio / timing, clean seams
   (`make_junction` is a pure fn, the splines are relational). One thing under test at a time.
4. **Bake-and-look is the test.** After every change: build → bake a screenshot → **Read the image** → judge
   the geometry by eye → tune. For subtle geometry, the owner sends an **annotated screenshot** (ideal path in
   one colour, ✗ on the problem). Far faster than prose for curves.
5. **Verify gates before every commit:** `clang -fsyntax-only` (+ `tools/build-all.js` — *all* carts compile
   against the API), `tools/ui-audit.js <cart> --explore` (no off-screen/overlapping text, no hidden UI),
   `tools/cart-status.js` (embedded thumbnail source synced). Commit only when green.
6. **Commit per milestone, by pathspec, docs in the same commit.** Small commits, each a complete working
   step; explicit pathspec (the parallel-agent rule — never sweep foreign dirty files). The doc update ships
   *with* the code, never after.
7. **Docs are the durable spine.** The ★ handoff (how-we-got-here / where-we're-going) + the frontier doc
   (schema, specs §8, reflections §7, the plan §9) are updated every milestone, so **context can turn over
   with zero loss** — a fresh session resumes from the doc.
8. **Forks: prose → recommend → owner picks → record.** At each decision (new world vs retrofit roadnet2;
   restrict-vs-flag the UI; default view) we discussed in prose, gave a recommendation, let the owner choose,
   and wrote the decision into the doc. No silent calls.
9. **Be honest in the artifact.** Mark verified vs thin (research), open polish, foreign files, caveats. The
   value is trust, not the appearance of completeness.

## Why it worked (the leverage)
- **Get the data model right and later features become small local changes.** "A port = a lane + a
  direction" + relational splines meant skew was *free* (ramps re-solved, zero spline edits) and topology was
  a `present` flag. **If the single primitive looks right, the composites inherit it.**
- **The bake loop made correctness visible** — every milestone produced a screenshot you could judge, so bugs
  (wrong-side entry lane, pinched loops, text overflow) were caught at the source, not downstream.
- **Doc-per-milestone meant we never re-derived** — the cheapest moment to build is while context is warm; the
  doc is the backstop for when it isn't.

## How the NEXT phase reuses this
The next frontier (at-grade junctions / network topology / the seed-driven world) runs the **same loop** — the
only swap is the *spec source*:
- **Spec source = the research docs**, the way OpenDRIVE was for roadlab:
  [`road-hierarchy-notes.md`](road-hierarchy-notes.md) §8 (network-topology patterns + the SNDi validation
  metrics + the Parish-Müller / tensor-field generation precedents) and §2 (the at-grade primitive set).
- **Milestone ladder** — pick the smallest first rung (e.g. a grid generator → add a dendricity knob → add
  radial bias → …), each compiling + committed before the next; *or* for at-grade: curb-return → T-junction →
  signalised crossroads → mini-roundabout.
- **Bake-and-look** — same screenshot-as-test loop; the network work even has a **numeric** check to add to
  the visual one: compute SNDi's four measures + degree-1/3/4 shares on the generated graph and confirm they
  land in the target pattern's range (research §8.2/§8.5).
- **Determinism preserved** — derive everything from the seed/cell-hash (like roadnet2's `seedZ`+`hash2`), so
  the same seed grows the same network; `make_junction`-style pure functions.
- **New sandbox or extend roadlab?** Decide it as a fork (prose → recommend → owner picks → record). roadlab
  is the interchange lab; at-grade and network-topology are arguably their own labs (§ the world fork in
  [`junction-lanelink.md`](junction-lanelink.md) §7). Whichever — it gets its own ★ handoff + frontier doc on
  day one, updated per milestone.

**Parked, same playbook when it's time:** the spec + lightweight **test harness for cart logic, built on
roadlab** (chosen testbed — pure, deterministic, clean seams). Breadcrumb in the handoff; not started.
