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
//   5. Fenced code blocks, docs/archive/ and docs/notes/ (the scratchpad) are skipped.
//   6. REAL-WORLD statements ("a real clav has no reverb") are musicology, not us.
//   7. QUOTED claims are being discussed, not asserted: `It closed the "waiting on the
//      sidechain path" wait`. Quoted spans are blanked before matching. The general form
//      of 4, which only caught the `was "…"` shape.
//
// THE 2026-07-31 RECALL PASS. The checker shipped catching outright denials ("no reverb",
// "X is missing") and reported the corpus clean while afrobeat-effects-wants.md claimed five
// shipped effects were unavailable. Two defects, both now fixture-asserted:
//   (a) BLIND TO THE BACKLOG SHAPE. This repo mostly denies a capability by SCHEDULING it:
//       "still open: wah, tape, leslie", "blocked on the effects bus", "not yet rostered",
//       "neither amp character nor compression". Seven of the eight phrasings in that one
//       doc were invisible. See backlogShapes().
//   (b) THE ACK BLED. Discriminator 2 ran at PARAGRAPH scope, so an "UPDATE — reverb
//       SHIPPED. … Still open: wah, tape, leslie" block silenced the very capabilities its
//       own sentence called still open. Now SENTENCE scope. See ackbleed.md.
// The lesson for the next widening: --selfcheck asserting only the SUPPRESSIONS proves
// nothing about what the tool FINDS, so a green run and a blind run looked identical. The
// recall cases are now known answers too, and so is the precision they cost (falsepos.md).
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

// ── the roster ───────────────────────────────────────────────────────────────
// Moved to tools/capability-roster.js (2026-07-31) so wants-check.js reads the SAME
// list. Two copies drift, and the drift is invisible: a capability missing from one
// makes that tool quietly blind, the exact failure this tool exists to catch.
const { CAPS, shipsIn } = require("./capability-roster");

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

  // ── RECALL. The checker's failure mode is a clean 0 while blind, and a green
  // selfcheck asserting only the discriminators proves nothing about what it FINDS.
  // Every case below is a real phrasing from afrobeat-effects-wants.md that the
  // 2026-07-30 version missed while reporting the corpus clean.
  const backlog = inFile("backlog.md");
  const sawLine = (frag) => backlog.some(x => x.line.toLowerCase().includes(frag));
  t("'Still open: wah, leslie' is a denial  [recall: the backlog shape]",
    sawLine("still open: wah"));
  t("'The reverb is still open' is a denial  [recall]", sawLine("the reverb is still open"));
  t("'Blocked on the vocoder' is a denial  [recall]", sawLine("blocked on the vocoder"));
  t("'not yet rostered' is a denial  [recall]", sawLine("not yet rostered"));
  t("'neither a room nor a vocoder' is a denial  [recall]", sawLine("neither a room nor"));
  t("'No tape/saturation stage' survives the slash  [recall: qualifier compound]",
    sawLine("no tape/saturation stage"));
  t("bare 'no wah pedal' is a denial  [recall: wah was not even a roster word]",
    sawLine("no wah pedal"));

  // ── the ack must not bleed onto the still-open list beside it ────────────────
  t("an ack silences the capability it actually names  [ack scope: sentence]",
    !inFile("ackbleed.md").some(x => x.cap === "reverb"));
  t("...but NOT one the same paragraph calls still open  [ack scope: the afrobeat bug]",
    inFile("ackbleed.md").some(x => x.cap === "leslie"));

  // ── the PRECISION cost of that recall, paid back. Both fired on the live corpus
  // while the patterns were widening; neither is a denial.
  t("a claim QUOTED in order to close it is not a denial  [discriminator 7]",
    !inFile("falsepos.md").some(x => x.cap === "sidechain"));
  t("'(middle C, no wah)' is a test condition, not a missing pedal  [discriminator 3]",
    !inFile("falsepos.md").some(x => x.cap === "auto-wah"));

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
const ships = (sym) => shipsIn(studioSrc, sym);   // capability-roster.js owns the shape
const roster = CAPS.filter(c => ships(c.proof));

// ── the claim patterns ───────────────────────────────────────────────────────
const esc = (w) => w.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
// The BACKLOG shapes (added 2026-07-31). The original set only caught outright denials
// ("no reverb", "X is missing"). This repo's design docs mostly deny a capability by
// SCHEDULING it instead — "still open: wah, tape, leslie", "blocked on the effects bus",
// "not yet rostered", "we have neither amp character nor compression". Seven of eight real
// phrasings from afrobeat-effects-wants.md were invisible to the original set, which is
// why that doc read clean while claiming five shipped effects were unavailable.
function backlogShapes(W) {
  return [
    new RegExp(`\\b${W}\\b[^.]{0,60}\\b(?:is|are|remains?)?\\s*still\\s+open\\b`, "i"),
    new RegExp(`\\bstill\\s+open\\b[^.]{0,80}\\b${W}\\b`, "i"),
    new RegExp(`\\bblocked\\s+on\\b[^.]{0,60}\\b${W}\\b`, "i"),
    new RegExp(`\\b${W}\\b[^.]{0,40}\\bnot\\s+yet\\s+(?:rostered|built|on\\s+the\\s+roster|a\\s+roster\\s+entry)\\b`, "i"),
    new RegExp(`\\bnot[-\\s]yet[-\\s]rostered\\b[^.]{0,40}\\b${W}\\b`, "i"),
    new RegExp(`\\bneither\\b[^.]{0,60}\\bnor\\b[^.]{0,40}\\b${W}\\b`, "i"),
  ];
}

function patterns(cap) {
  const alt = cap.words.map(esc).join("|");
  const W = `(?:${alt})`;
  if (cap.ambiguous) {
    // only the qualified forms — the bare word means something else here too often.
    // WQ tolerates a slashed compound before the qualifier ("no tape/saturation stage"),
    // which the bare `${W}\s+${QUALIFIER}` form missed.
    const WQ = `${W}(?:\\s*/\\s*[\\w-]+)*`;
    return [
      new RegExp(`\\bno\\s+(?:real\\s+|true\\s+|proper\\s+|actual\\s+|dedicated\\s+)?${WQ}\\s+${QUALIFIER}\\b`, "i"),
      new RegExp(`\\b${WQ}\\s+${QUALIFIER}\\s+(?:is|are)(?:\\s+still)?\\s+(?:missing|absent|unbuilt|not\\s+built)\\b`, "i"),
      new RegExp(`\\b(?:lacks|lacking)\\s+(?:a\\s+|any\\s+)?${WQ}\\s+${QUALIFIER}\\b`, "i"),
      // backlog shapes stay QUALIFIED for ambiguous words: a bare "tape" or "drive" in a
      // "still open:" list is too often the other meaning to flag on its own.
      ...backlogShapes(`${WQ}\\s+${QUALIFIER}`),
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
    ...backlogShapes(W),
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
// ── discriminator 7: a QUOTED claim is being discussed, not asserted ──────────
// Prose that closes a wait quotes the wait to name it: `It closed the "waiting on the
// sidechain path" wait`. That is the opposite of a live denial, but the pattern sees only
// the quoted words. Blanking quoted spans before matching is the general form of the
// already-annotated rule (discriminator 4), which only caught the `was "…"` shape.
const unquote = (s) => s.replace(/"[^"]*"|“[^”]*”/g, " ");

function walk(dir, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    if (e.name === "archive" || e.name === "notes" || e.name.startsWith(".")) continue;
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
  // ...and SENTENCE scope inside that paragraph (2026-07-31). Paragraph scope let one
  // sentence's ack bleed onto the capabilities a NEIGHBOURING sentence declared still open:
  //   **UPDATE — reverb + chorus SHIPPED.** … Still open: wah, tape, leslie, drive.
  // silenced wah/tape/leslie/drive doc-wide, the exact inversion of the intent, and it is
  // what hid afrobeat-effects-wants.md. Sentence scope keeps the multi-line-UPDATE fix that
  // motivated paragraph scope (a sentence still spans lines once the paragraph is joined)
  // while an ack can no longer cover a still-open list beside it. Emphasis is stripped first
  // so a bolded "SHIPPED.**" still reads as a sentence end.
  const sentences = [];
  for (const text of paraText.values())
    for (const s of text.replace(/[*_`]/g, "").split(/(?<=[.!?])\s+/))
      if (s.trim()) sentences.push(s);

  const acked = new Set();
  for (const c of roster) {
    const alt = new RegExp(`(?:${c.words.map(esc).join("|")})`, "i");
    if (sentences.some(s => alt.test(s) && ACK.test(s))) acked.add(c.cap);
  }

  let fenced = false;
  lines.forEach((line, i) => {
    if (/^\s*```/.test(line)) { fenced = !fenced; return; }
    if (fenced) return;                                   // discriminator 5
    if (ANNOTATED.test(line)) return;                     // discriminator 4
    if (REAL_WORLD.test(line)) return;                    // discriminator 6
    const claimText = unquote(line);                      // discriminator 7
    for (const c of roster) {
      if (acked.has(c.cap)) continue;                      // discriminator 2
      if (PATS.get(c.cap).some(re => re.test(claimText))) {
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
