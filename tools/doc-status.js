// ============================================================================
// doc-status.js — shared helpers for reading a doc's self-declared STATUS line
// and classifying it into a lifecycle phase. Used by topic-brief.js (per-theme)
// and design-board.js (whole-repo overview). This module OWNS the phase
// vocabulary, the way teaches-vocab.js owns `teaches` and lint-carts.js owns the
// cart tags — one place to normalize the drifting free-text status conventions
// (SHIPPED / shipped / ✅ / BUILT all mean the same; IDEA / BRAINSTORM /
// exploration all mean not-started).
//
// Not a CLI — require() it. No top-level side effects.
// ============================================================================

// the convention: a "STATUS:" line near the top of a design doc, or an ADR's
// "Date: … · Status: accepted" line. Returns the status TEXT (markdown stripped),
// or null if the doc declares none.
function extractStatus(lines) {
  for (let i = 0; i < Math.min(lines.length, 16); i++) {
    const l = lines[i].trim();
    let m = l.match(/^>?\s*\*{0,2}STATUS\*{0,2}\s*[:—-]\s*(.+)$/i);
    if (m) return m[1].replace(/\*\*/g, "").trim();
    m = l.match(/Status:\s*([A-Za-z].+?)\s*$/);          // ADR "· Status: accepted" line
    if (m) return m[1].replace(/\*\*/g, "").trim();
  }
  return null;
}

// embedded checklist / plan progress (- [ ] vs - [x]); null if no checklist.
function planProgress(text) {
  const open = (text.match(/^\s*[-*]\s*\[ \]/gm) || []).length;
  const done = (text.match(/^\s*[-*]\s*\[[xX]\]/gm) || []).length;
  if (!open && !done) return null;
  return { done, total: open + done };
}

// first YYYY-MM-DD anywhere in the status text (rev./researched/updated/shipped date).
function statusDate(statusText) {
  const m = (statusText || "").match(/\b(20\d\d-\d\d-\d\d)\b/);
  return m ? m[1] : null;
}

// lifecycle phases, in order. `key` is stable; `label` is for display; `re`
// matches the status text. ORDER MATTERS — first match wins, so terminal/strong
// states (cut, shipped) are tested before earlier ones ("designed AND shipped"
// → shipped). ADR states (accepted/proposed) come after so a design doc that
// merely says "accepted approach" doesn't masquerade as a decision.
const PHASES = [
  { key: "cut",       label: "CUT / SUPERSEDED",   re: /\b(cut|rejected|abandoned|dropped|superseded|deprecated|won'?t)\b/i },
  { key: "shipped",   label: "SHIPPED / BUILT",    re: /\b(shipped|built|done|complete|live|landed|✅|v[12]\b)/i },
  { key: "building",  label: "BUILDING",           re: /\b(building|in[\s-]?progress|wip|underway|started|increments?)\b/i },
  { key: "ready",     label: "READY TO BUILD",     re: /\b(ready|designed|design\b|queued|approved|recommendation)\b/i },
  { key: "exploring", label: "EXPLORING / IDEA",   re: /\b(idea|brainstorm|exploration|explorator|experiment(?:al)?|survey|reflection|proposal|proposed|research|open|scoping)\b/i },
  { key: "accepted",  label: "DECIDED (ADR)",      re: /\baccepted\b/i },
];

// classify a status text → phase key, or "other" if it declares a status that
// matches no phase (vocabulary drift — surfaced by design-board --lint).
function classifyStatus(statusText) {
  if (!statusText) return null;                          // no status line at all → unmarked
  for (const p of PHASES) if (p.re.test(statusText)) return p.key;
  return "other";
}

module.exports = { extractStatus, planProgress, statusDate, PHASES, classifyStatus };
