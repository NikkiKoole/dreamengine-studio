#!/usr/bin/env node
// ============================================================================
// lint-capability-claims.js — docs that say we CAN'T do something we now can.
//
//   node tools/lint-capability-claims.js           # full report
//   node tools/lint-capability-claims.js radio     # SCOPED to matching doc paths
//   node tools/lint-capability-claims.js --quiet   # CI: print only on findings
//   node tools/lint-capability-claims.js --json
//   node tools/lint-capability-claims.js --selfcheck   # assert the CHECKER
//
// THE FRICTION THIS KILLS. stale-doc-check.js finds docs citing code that ONCE
// EXISTED and is now gone. This is the INVERSE and nothing covered it: a doc
// citing an ABSENCE that has since been FILLED. "missing (no reverb engine)",
// "not a vocoder — a future effect once the sidechain lands", "no sampler
// exists". Every one of those was true when written and is now a lie that an
// agent will believe and route around, hand-rolling a workaround for a shipped
// effect. lint-docs catches a broken link; nothing catches a false premise.
//
// It is the same failure as ADR-0015's dead "~12 functions, forever" cap: prose
// asserting a limit that the code outgrew. The 2026-07-30 audit found it in six
// rows of radio-genre-fidelity.md (still recommending a noise burst to fake a
// spring reverb, three weeks after reverb_spring shipped) and in four places
// swearing the vocoder was unbuildable, thirteen days after it shipped.
//
// HOW IT AVOIDS THE 0%-PRECISION TRAP. stale-doc-check's "broken references"
// tier once ran at 47 findings / 0% true positives because it couldn't tell a
// renamed path from one that never existed. The discriminators here, in order of
// how much noise each kills:
//
//   1. THE CAPABILITY MUST PROVABLY EXIST. Each entry names a studio.h symbol
//      that proves we ship it. No symbol → not in the roster → "no octaver" is a
//      TRUE statement and is never flagged. Delete an API and its claims go
//      quiet on their own.
//   2. ACKNOWLEDGEMENT, AT PARAGRAPH SCOPE. If any PARAGRAPH pairs the capability
//      with a shipped-marker ("reverb ✓ SHIPPED", "now exists", "landed"), every
//      claim about that capability in that doc goes quiet. This is what makes a
//      dated wishlist (air-effects-wants.md: a table of 2026-06 wants + an UPDATE
//      block saying reverb landed) correctly silent, while a ledger with no such
//      note fires. Paragraph, deliberately: line-scope missed the common
//      multi-line "UPDATE: … what landed: reverb, tape, chorus" correction, and
//      doc-scope would silence audio-notes.md entirely (it mentions every effect
//      near the word "shipped" somewhere).
//   3. AMBIGUOUS WORDS NEED A QUALIFIER. "no gate" in this repo usually means a
//      TEST gate ("we had no gate for does-it-click"); "no filter" can be a
//      search filter. For gate/filter/drive/echo/eq/tape/shallow/compression the
//      bare form is ignored — only "no X engine|bus|effect|insert|pedal|stage".
//   4. ALREADY-ANNOTATED lines are skipped: `was "no reverb engine"`,
//      `[SUPERSEDED …]`, "no longer true". Correcting in place must not re-fire.
//   5. Fenced code blocks and docs/archive/ are skipped.
//
// ADVISORY by design (exit 0 even with findings; `--strict` to gate). A claim can
// be legitimately historical in a way no regex sees — a blind-brief quoting what
// an expert believed, a retrospective. Fix it or add an acknowledgement line; the
// acknowledgement is the escape hatch, and it doubles as the useful edit.
// ============================================================================

const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const args = process.argv.slice(2);
const has = (f) => args.includes(f);

// ── the roster: capability → the studio.h symbol that PROVES we ship it ──────
// `proof` is matched as a declaration in studio.h (`… name(`). Drop the API and the
// capability leaves the roster automatically, which is the point of discriminator 1.
// `ambiguous: true` = the word has a common non-audio meaning here, so it only counts
// with a qualifier noun (discriminator 3).
const CAPS = [
  { cap: "reverb",     proof: "reverb",           words: ["reverb"] },
  { cap: "chorus",     proof: "chorus",           words: ["chorus"] },
  { cap: "flanger",    proof: "flanger",          words: ["flanger"] },
  { cap: "phaser",     proof: "phaser",           words: ["phaser"] },
  { cap: "tremolo",    proof: "tremolo",          words: ["tremolo"] },
  { cap: "leslie",     proof: "leslie",           words: ["leslie", "rotary speaker"] },
  { cap: "univibe",    proof: "univibe",          words: ["univibe", "uni-vibe"] },
  { cap: "auto-wah",   proof: "wah",              words: ["auto-wah", "autowah"] },
  { cap: "formant",    proof: "formant",          words: ["formant filter", "vowel filter"] },
  { cap: "vocoder",    proof: "vocoder",          words: ["vocoder"] },
  { cap: "ring mod",   proof: "ringmod",          words: ["ring mod", "ringmod", "ring modulator"] },
  { cap: "auto-pan",   proof: "autopan",          words: ["auto-pan", "autopan"] },
  { cap: "multiband",  proof: "multiband",        words: ["multiband"] },
  { cap: "shimmer",    proof: "shimmer",          words: ["shimmer reverb"] },
  { cap: "bitcrush",   proof: "crush",            words: ["bitcrush", "bit crush", "bitcrusher"] },
  { cap: "granular",   proof: "grains",           words: ["granular"] },
  { cap: "sidechain",  proof: "sidechain",        words: ["sidechain", "side-chain"] },
  { cap: "varispeed",  proof: "varispeed",        words: ["varispeed"] },
  { cap: "sampler",    proof: "sample_record",    words: ["sampler"] },
  { cap: "pitch-shift",proof: "sample_shift",     words: ["pitch-shift", "pitch shifter"] },
  { cap: "mic input",  proof: "mic_level",        words: ["mic input", "microphone input", "audio input"] },
  // ambiguous: bare "no gate"/"no filter" are usually about something else entirely
  { cap: "gate",        proof: "gate",            words: ["gate"],        ambiguous: true },
  { cap: "filter",      proof: "filter",          words: ["filter"],      ambiguous: true },
  { cap: "drive",       proof: "instrument_drive",words: ["drive", "overdrive"], ambiguous: true },
  { cap: "echo/delay",  proof: "echo",            words: ["echo", "delay"], ambiguous: true },
  { cap: "EQ",          proof: "eq",              words: ["eq"],          ambiguous: true },
  { cap: "tape",        proof: "tape",            words: ["tape"],        ambiguous: true },
  { cap: "shallow",     proof: "shallow",         words: ["shallow"],     ambiguous: true },
  { cap: "compression", proof: "glue",            words: ["compression", "compressor"], ambiguous: true },
];

const QUALIFIER = "(?:engine|bus|effect|insert|pedal|stage|voicing|module|unit|section)";

// ── --selfcheck: assert the CHECKER against known answers ────────────────────
// This tier JUDGES prose with regexes over a pile of suppressions, so it rots in both
// directions: a broken pattern prints a clean 0 while blind, and a broken suppression
// floods. The blessed pattern from status-check.js --selfcheck.
if (has("--selfcheck")) {
  const cp = require("child_process");
  const fx = path.join(__dirname, "fixtures", "lint-capability-claims");
  let raw;
  try {
    raw = cp.execFileSync(process.execPath, [__filename, "--json"], {
      env: { ...process.env,
             DE_CAP_STUDIO_H: path.join(fx, "studio.h.txt"),
             DE_CAP_DOCS_DIR: path.join(fx, "docs") },
      encoding: "utf8", maxBuffer: 1 << 26,
    });
  } catch (e) { raw = e.stdout; }
  const g = JSON.parse(raw);
  const inFile = (f) => g.findings.filter(x => x.file.endsWith(f));
  const T = [];
  const t = (n, ok) => T.push({ n, ok });

  t("the roster loads from the proof symbols  [broken-parse guard]",
    g.roster.includes("reverb") && g.roster.includes("vocoder"));
  t("a capability with NO proof symbol is not in the roster  [discriminator 1]",
    !g.roster.includes("shallow"));
  t("a flat 'there is no reverb engine' is reported", inFile("flagged.md").length >= 1);
  t("...and the vocoder 'future effect' shape too",
    inFile("flagged.md").some(x => x.cap === "vocoder"));
  t("a doc that acknowledges the ship elsewhere is silent  [discriminator 2]",
    inFile("acknowledged.md").length === 0);
  t("a claim inside a ``` fence is ignored  [discriminator 5]",
    inFile("fenced.md").length === 0);
  t("bare 'no gate yet' (a TEST gate) is ignored  [discriminator 3]",
    !inFile("ambiguous.md").some(x => x.line.includes("no gate yet")));
  t("...but 'no gate effect' IS reported  [discriminator 3, other direction]",
    inFile("ambiguous.md").some(x => x.line.includes("no gate effect")));
  t("an already-annotated correction does not re-fire  [discriminator 4]",
    inFile("annotated.md").length === 0);
  t("a genuinely-absent capability (octaver) is never flagged  [discriminator 1]",
    !g.findings.some(x => x.cap === "octaver"));
  t("'a real clav has no reverb' is musicology, not an engine limit  [discriminator 6]",
    !inFile("realworld.md").some(x => x.line.includes("a real clav")));
  t("'both now exist' (PLURAL) acknowledges the ship  [discriminator 2 regression guard]",
    !inFile("realworld.md").some(x => x.cap === "chorus"));
  t("an ack in ONE paragraph doesn't silence other paragraphs  [scope: not doc-wide]",
    inFile("parascope.md").some(x => x.cap === "reverb"));
  t("...and a multi-line UPDATE block's ack DOES cover its own lines  [scope: not line-only]",
    !inFile("parascope.md").some(x => x.cap === "gate"));
  t("an acked capability is silent in the same doc  [discriminator 2]",
    !inFile("parascope.md").some(x => x.cap === "vocoder"));

  const bad = T.filter(x => !x.ok);
  for (const x of T) console.log(`  ${x.ok ? "\x1b[32m✓\x1b[0m" : "\x1b[31m✗\x1b[0m"} ${x.n}`);
  console.log(bad.length
    ? `\x1b[31mlint-capability-claims --selfcheck FAILED\x1b[0m — ${bad.length} of ${T.length} expectations broken`
    : `lint-capability-claims --selfcheck: ${T.length}/${T.length} known answers correct`);
  process.exit(bad.length ? 1 : 0);
}

const STUDIO = process.env.DE_CAP_STUDIO_H || path.join(ROOT, "runtime", "studio.h");
const DOCS   = process.env.DE_CAP_DOCS_DIR || path.join(ROOT, "docs");
const scope  = args.find(a => !a.startsWith("--")) || "";

// ── discriminator 1: keep only capabilities studio.h proves we ship ──────────
const studioSrc = fs.readFileSync(STUDIO, "utf8");
const ships = (sym) => new RegExp(`^\\s*(?:void|float|int|const char\\s*\\*)\\s+${sym}\\s*\\(`, "m").test(studioSrc);
const roster = CAPS.filter(c => ships(c.proof));

// ── the claim patterns ───────────────────────────────────────────────────────
const esc = (w) => w.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
function patterns(cap) {
  const alt = cap.words.map(esc).join("|");
  const W = `(?:${alt})`;
  if (cap.ambiguous) {
    // only the qualified forms — the bare word means something else here too often
    return [
      new RegExp(`\\bno\\s+(?:real\\s+|true\\s+|proper\\s+|actual\\s+|dedicated\\s+)?${W}\\s+${QUALIFIER}\\b`, "i"),
      new RegExp(`\\b${W}\\s+${QUALIFIER}\\s+(?:is|are)(?:\\s+still)?\\s+(?:missing|absent|unbuilt|not\\s+built)\\b`, "i"),
      new RegExp(`\\b(?:lacks|lacking)\\s+(?:a\\s+|any\\s+)?${W}\\s+${QUALIFIER}\\b`, "i"),
    ];
  }
  return [
    new RegExp(`\\bno\\s+(?:real\\s+|true\\s+|proper\\s+|actual\\s+|dedicated\\s+|a\\s+|any\\s+)*${W}\\b`, "i"),
    new RegExp(`\\b${W}\\s+(?:is|are)(?:\\s+still)?\\s+(?:missing|absent|unbuilt|not\\s+built|nonexistent)\\b`, "i"),
    new RegExp(`\\b(?:lacks|lacking)\\s+(?:a\\s+|any\\s+)?${W}\\b`, "i"),
    new RegExp(`\\b(?:there\\s+is|we\\s+have|it\\s+has|has)\\s+no\\s+(?:real\\s+)?${W}\\b`, "i"),
    new RegExp(`\\b${W}\\b[^.]{0,40}\\bdoes(?:n't|\\s+not)\\s+exist\\b`, "i"),
    new RegExp(`\\b${W}\\b[^.]{0,60}\\b(?:a\\s+)?future\\s+effect\\b`, "i"),
    new RegExp(`\\bwaiting\\s+on\\b[^.]{0,60}\\b${W}\\b|\\b${W}\\b[^.]{0,40}\\bstill\\s+waits?\\b`, "i"),
  ];
}
const PATS = new Map(roster.map(c => [c.cap, patterns(c)]));

// ── discriminator 4: a line already marked as corrected ──────────────────────
const ANNOTATED = /was\s*["“]|\[(?:stale|corrected|superseded|SUPERSEDED|half wrong)|no longer\s+(?:true|accurate|the case)|SUPERSEDED|✓\s*SHIPPED|now\s+ships/i;
// ── discriminator 2: the doc admits it shipped, somewhere ────────────────────
// NB "now exist" (plural) as well as "now exists" — the singular-only version of this
// regex let a line reading "no chorus/tape-wow WIRED (both now exist)" fire, which is
// exactly the correction-in-place shape discriminator 4 is meant to respect.
const ACK = /(?:✓|\bSHIPPED\b|\bshipped\b|\blanded\b|now\s+(?:ships?|exist|exists|lands?|have|has)|DOES\s+exist)/;
// ── discriminator 6: a claim about a REAL-WORLD instrument, not about us ──────
// "a real clav has no tremolo" is musicology, not an engine limit. Narrow on purpose.
const REAL_WORLD = /\ba real\b[^.]{0,40}\b(?:has|have|had)\s+no\b|\bon a real\b/i;

function walk(dir, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    if (e.name === "archive" || e.name.startsWith(".")) continue;
    const p = path.join(dir, e.name);
    if (e.isDirectory()) walk(p, out);
    else if (e.name.endsWith(".md")) out.push(p);
  }
  return out;
}

const findings = [];
for (const file of walk(DOCS)) {
  const rel = path.relative(ROOT, file);
  if (scope && !rel.toLowerCase().includes(scope.toLowerCase())) continue;
  const src = fs.readFileSync(file, "utf8");
  const lines = src.split("\n");

  // Acknowledgement set (discriminator 2): which caps does this doc admit shipped?
  // Scope is the PARAGRAPH, not the line and not the whole doc. Line-only was too tight (a
  // multi-line "UPDATE: … what landed: reverb, tape, chorus" block never matched, which is the
  // single most common way these get corrected); whole-doc is too loose (audio-notes.md mentions
  // every effect beside the word "shipped" somewhere, so it would go universally silent).
  // A paragraph break is a blank line, or a blank blockquote line (`>`), so an UPDATE block's
  // own sub-paragraphs count separately.
  const para = [];
  let pi = 0;
  for (const l of lines) {
    if (/^\s*>?\s*$/.test(l)) { pi++; para.push(-1); } else para.push(pi);
  }
  const paraText = new Map();
  lines.forEach((l, i) => {
    if (para[i] < 0) return;
    paraText.set(para[i], (paraText.get(para[i]) || "") + " " + l);
  });
  const acked = new Set();
  for (const c of roster) {
    const alt = new RegExp(`(?:${c.words.map(esc).join("|")})`, "i");
    for (const text of paraText.values()) {
      if (alt.test(text) && ACK.test(text)) { acked.add(c.cap); break; }
    }
  }

  let fenced = false;
  lines.forEach((line, i) => {
    if (/^\s*```/.test(line)) { fenced = !fenced; return; }
    if (fenced) return;                                   // discriminator 5
    if (ANNOTATED.test(line)) return;                     // discriminator 4
    if (REAL_WORLD.test(line)) return;                    // discriminator 6
    for (const c of roster) {
      if (acked.has(c.cap)) continue;                      // discriminator 2
      if (PATS.get(c.cap).some(re => re.test(line))) {
        findings.push({ file: rel, ln: i + 1, cap: c.cap, line: line.trim().slice(0, 160) });
        break;                                             // one finding per line
      }
    }
  });
}

if (has("--json")) {
  console.log(JSON.stringify({ roster: roster.map(c => c.cap), findings }, null, 2));
  process.exit(0);
}

if (!findings.length) {
  if (!has("--quiet")) {
    console.log(`capability claims: ok — no doc denies a shipped capability ` +
                `(${roster.length} capabilities checked across the docs/ corpus)`);
  }
  process.exit(0);
}

console.log(`\x1b[33mDOCS DENYING A SHIPPED CAPABILITY\x1b[0m (${findings.length}) — ` +
            `advisory; each is prose claiming we can't do something we now can`);
let last = "";
for (const f of findings) {
  if (f.file !== last) { console.log(`\n  ${f.file}`); last = f.file; }
  console.log(`    :${f.ln}  [${f.cap}]  ${f.line}`);
}
console.log(`\n  → fix the claim, or if it is deliberately historical add an acknowledgement` +
            `\n    ("…— reverb SHIPPED 2026-06-10") which silences that capability for the doc.` +
            `\n    ${roster.length} capabilities checked. Roster + suppressions: this file's header.`);
process.exit(has("--strict") ? 1 : 0);
