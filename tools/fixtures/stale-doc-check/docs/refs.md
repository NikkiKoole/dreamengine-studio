# Fixture — stale-doc-check BROKEN REFERENCES known answers

Not a real doc. Each line below pins ONE expected verdict. The suppression cases are the ones that
matter: this tier reported 47 findings at a 0% true-positive rate before they existed (2026-07-30).

## Must be reported as BROKEN

The atlas lives in `runtime/font16x16_data.h` and always has.

Run `node tools/play.js mycart --zzznotaflag` to do the thing.

## Must NOT be reported — a path that exists

The harness driver is `tools/play.js`.

## Must NOT be reported — a path that has NEVER existed (a proposal)

We should carve the engine into `runtime/engines/zzznever.h` one day.

The generator `tools/zzz-gen.js` will emit a table.

## Must NOT be reported — another repo's path

- `~/Projects/navkit/soundsystem/tools/preset_audition.c` — headless preset renderer.

## Must NOT be reported — a real flag on a real tool

Gate it with `node tools/play.js mycart --headless`.
