# lint-aux-params fixture — known answers

Three synthetic aux-param channels, each a stand-in for `runtime/sound.h` +
`runtime/studio.h` + the two editor registries. `lint-aux-params.js --selfcheck`
runs the checker against each and asserts what it must say.

Three cases, not one, because the findings are mutually exclusive by construction:
a channel whose `eng_p[]` declarations *disagree* cannot also be the channel that
demonstrates two *agreeing* declarations.

| case | stands for | must be found |
|---|---|---|
| `broken/` | the real bug shape: the channel was widened in some places | a narrow bound · an uncopied index · a `MODE_*` past the width · a `MODE_*` missing from `studioDocs.js` · one missing from `shell.js` |
| `clean/` | a correctly-widened channel | nothing — *and* the width + modes still parsed (a blind pass and a clean pass print the same thing) |
| `stale/` | the engine got refactored out from under the lint | disagreeing declarations · a vanished second bound · a vanished `MODE_*` roster |
| `split/` | the refactor that actually happened (2026-08-14): the declarations moved into the generated `sound_ctx.h` | **nothing** — the lint must look in both files, *and* still be able to say the channel is clean |

`split/` is the only case with a `sound_ctx.h.txt`; the reader treats a missing one as empty, so the
other three are untouched by its addition. It is here because the real lint spent weeks red against
the real source while `repo-doctor` showed green — the selftest row ran this fixture (fine), and
nothing ran the lint itself. Both rows exist in `repo-doctor` now, which is the general lesson: a
`--selfcheck` proves the checker works, only a real run proves it is still looking at the repo.

`.h.txt` / `.js.txt`, never `.h` / `.js`: a fixture header is never compiled, and a
real `.h` here makes clangd index it and report phantom errors at you. A real `.js`
would get picked up by the editor's own tooling.

`broken/` names its modes so each carries exactly one defect:

- `MODE_FIX_OK` (0) — in range, in both registries → must be **silent** (the noise guard)
- `MODE_FIX_NODOCS` (1) — in `shell.js` only
- `MODE_FIX_NOSHELL` (2) — in `studioDocs.js` only
- `MODE_FIX_TOOBIG` (7) — in both registries, but outside `eng_p[4]`

See `docs/guides/checks-and-oracles.md` → "Self-test the checker".
