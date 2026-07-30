#!/usr/bin/env node
// ============================================================================
// lint-fxicons.js — every FX_* insert kind must have a shared GLYPH.
//
//   node tools/lint-fxicons.js            # report
//   node tools/lint-fxicons.js --quiet    # CI: print only on failure
//   node tools/lint-fxicons.js --json     # machine-readable
//   node tools/lint-fxicons.js --selfcheck  # assert the CHECKER (known-answer fixture)
//
// THE FRICTION THIS KILLS. `runtime/fxicons.h` is the shared VISUAL LANGUAGE for
// the engine effects: one icon + body/accent colour per FX_* kind, so every cart's
// pedals read the same. Adding an effect means adding a kind to studio.h — and
// nothing made you add it here. The failure is SILENT and worse than blank,
// because fx_icon()'s trailing `else` draws the FALLBACK glyph: an unregistered
// kind renders as a perfectly convincing REVERB pedal, and fx_name() labels it
// "FX" in default grey.
//
// That is not hypothetical. FX_DRIVE (2026-06-15) and FX_MULTIBAND (2026-06-15)
// both shipped without entries and drew reverb rings for six weeks. Worse, two
// carts (pedalboard.c, pedalicon.c) each hand-rolled a private `od_icon()` for
// FX_DRIVE and special-cased it before the fx_icon() call — the exact
// copy-paste-per-cart that fxicons.h exists to prevent. Both were fixed
// 2026-07-30 (the glyph promoted verbatim) and this gate exists so the next one
// can't happen quietly. Same family as lint-docs.js's discoverability gates and
// lint-aux-params.js's "the width is declared in five places that must agree":
// a capability that lands in N places, where N-1 is silently wrong.
//
// WHAT IT CHECKS. Every `#define FX_<NAME> <n>` in studio.h (the pedal-kind
// roster) must appear in all four of fxicons.h's dispatchers:
//   fx_body() · fx_accent() · fx_name() · fx_icon()
// Exception, derived not hardcoded: fx_icon()'s trailing `} else {` comment names
// the kind it draws as the fallback (today REVERB), so that one kind is allowed
// to have no explicit branch. If you re-point the fallback, this follows.
//
// Advisory by default (exit 0); `--strict` exits 1 on any finding. It sits at
// ZERO — keep it there: a new finding is a missing glyph, not a new exempt class.
// ============================================================================

const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const args = process.argv.slice(2);
const has = (f) => args.includes(f);

// ── --selfcheck: assert the CHECKER against known answers ────────────────────
// See docs/guides/checks-and-oracles.md "Self-test the checker". This one parses two headers with
// regexes, so it can rot in BOTH directions: a broken kind-regex prints a clean 0 findings while
// seeing nothing, and a broken fallback-detector floods with a false "reverb has no icon".
if (has("--selfcheck")) {
  const cp = require("child_process");
  const fx = path.join(__dirname, "fixtures", "lint-fxicons");
  let raw;
  try {
    raw = cp.execFileSync(process.execPath, [__filename, "--json"], {
      env: { ...process.env,
             // `.h.txt`, not `.h`: a fixture header is never compiled, and a real .h here
             // makes clangd index it and report phantom "undeclared FX_ALPHA" errors at you.
             DE_FX_STUDIO_H: path.join(fx, "studio.h.txt"),
             DE_FX_ICONS_H:  path.join(fx, "fxicons.h.txt") },
      encoding: "utf8", maxBuffer: 1 << 26,
    });
  } catch (e) { raw = e.stdout; }
  const g = JSON.parse(raw);
  const missingIn = (kind, fn) => g.findings.some(f => f.kind === kind && f.missing.includes(fn));
  const T = [];
  const t = (n, ok) => T.push({ n, ok });

  t("finds the roster at all  [broken-regex guard]", g.kinds.length === 4);
  t("FX_INST(kind,inst) is not parsed as a kind  [regression guard]",
    !g.kinds.includes("FX_INST"));
  t("a kind absent from fx_name is reported", missingIn("FX_BETA", "fx_name"));
  t("a kind absent from ALL four is reported once, with all four named", (() => {
    const f = g.findings.filter(x => x.kind === "FX_DELTA");
    return f.length === 1 && f[0].missing.length === 4;
  })());
  t("the fallback kind needs no fx_icon branch  [exempt-class guard]",
    !missingIn("FX_GAMMA", "fx_icon"));
  t("...but the fallback kind DOES still need a colour + name", missingIn("FX_GAMMA", "fx_body"));
  t("a fully-registered kind is silent  [noise guard]",
    !g.findings.some(f => f.kind === "FX_ALPHA"));

  const bad = T.filter(x => !x.ok);
  for (const x of T) console.log(`  ${x.ok ? "\x1b[32m✓\x1b[0m" : "\x1b[31m✗\x1b[0m"} ${x.n}`);
  console.log(bad.length
    ? `\x1b[31mlint-fxicons --selfcheck FAILED\x1b[0m — ${bad.length} of ${T.length} expectations broken`
    : `lint-fxicons --selfcheck: ${T.length}/${T.length} known answers correct`);
  process.exit(bad.length ? 1 : 0);
}

const STUDIO = process.env.DE_FX_STUDIO_H || path.join(ROOT, "runtime", "studio.h");
const ICONS  = process.env.DE_FX_ICONS_H  || path.join(ROOT, "runtime", "fxicons.h");

const studioSrc = fs.readFileSync(STUDIO, "utf8");
const iconsSrc  = fs.readFileSync(ICONS, "utf8");

// The roster: `#define FX_NAME <int>`. Requiring a NUMBER is what keeps the
// FX_INST(kind, inst) helper macro out (it has a paren, not a value).
const kinds = [...studioSrc.matchAll(/^#define\s+(FX_[A-Z0-9_]+)\s+(\d+)\b/gm)]
  .map(m => ({ name: m[1], value: +m[2] }))
  .sort((a, b) => a.value - b.value);

// Slice fxicons.h into its four dispatchers so "is FX_X handled" is asked per function.
const FNS = [
  { fn: "fx_body",   sig: /static\s+int\s+fx_body\s*\(/ },
  { fn: "fx_accent", sig: /static\s+int\s+fx_accent\s*\(/ },
  { fn: "fx_name",   sig: /static\s+const\s+char\s*\*\s*fx_name\s*\(/ },
  { fn: "fx_icon",   sig: /static\s+void\s+fx_icon\s*\(/ },
];
const starts = FNS.map(f => ({ ...f, at: iconsSrc.search(f.sig) }));
for (const s of starts) if (s.at < 0) {
  console.error(`lint-fxicons: could not find ${s.fn}() in ${path.relative(ROOT, ICONS)} — parser needs updating`);
  process.exit(2);
}
const ordered = [...starts].sort((a, b) => a.at - b.at);
const bodies = {};
ordered.forEach((s, i) => {
  bodies[s.fn] = iconsSrc.slice(s.at, i + 1 < ordered.length ? ordered[i + 1].at : iconsSrc.length);
});

// fx_icon()'s trailing `} else { // NAME …` names the kind drawn as the fallback.
// Derive it rather than hardcode, so re-pointing the fallback re-points this gate.
let fallback = null;
const elseTail = bodies.fx_icon.match(/\}\s*else\s*\{([^\n]*)/);
if (elseTail) {
  const hint = elseTail[1].toUpperCase();
  fallback = kinds.map(k => k.name).find(n => hint.includes(n.replace(/^FX_/, ""))) || null;
}

const handles = (fn, kind) =>
  fn === "fx_icon"
    ? new RegExp(`kind\\s*==\\s*${kind}\\b`).test(bodies[fn])
    : new RegExp(`case\\s+${kind}\\s*:`).test(bodies[fn]);

const findings = [];
for (const k of kinds) {
  const missing = FNS.map(f => f.fn).filter(fn => {
    if (fn === "fx_icon" && k.name === fallback) return false;   // the documented fallback
    return !handles(fn, k.name);
  });
  if (missing.length) findings.push({ kind: k.name, value: k.value, missing });
}

if (has("--json")) {
  console.log(JSON.stringify({ kinds: kinds.map(k => k.name), fallback, findings }, null, 2));
  process.exit(0);
}

const relRaw = path.relative(ROOT, ICONS);
const rel = relRaw.startsWith("..") ? ICONS : relRaw;   // an override path outside the repo prints absolute
if (!findings.length) {
  if (!has("--quiet")) {
    console.log(`fxicons: ok — all ${kinds.length} FX_* kinds have a glyph, colours + name` +
                (fallback ? ` (${fallback} = the fx_icon fallback)` : ""));
  }
  process.exit(0);
}

console.log(`\x1b[31mFX_* KINDS WITH NO SHARED GLYPH\x1b[0m (${findings.length}) — ` +
            `they silently draw the ${fallback || "fallback"} icon in every cart`);
for (const f of findings) {
  console.log(`  ${f.kind} (=${f.value})  missing from: ${f.missing.join(", ")}`);
}
console.log(`\n  → add each to ${rel}. A kind needs a body colour, an accent, a NAME and a glyph;` +
            `\n    ${fallback ? `only ${fallback} (the fallback) may skip the glyph.` : ""}`);
process.exit(has("--strict") ? 1 : 0);
