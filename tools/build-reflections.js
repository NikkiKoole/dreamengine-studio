#!/usr/bin/env node
// build-reflections.js — generate docs/reflections.html, the VISUAL ★ Reflections
// page: the research journal (docs/field-notes/) as a styled lifecycle board, the
// sibling of build-design-board.js. The TODO board is the *work* lens (designs by
// phase); Reflections is the *thinking* lens (journal notes by where they sit on
// hunch → adopted). Same Comic-Mono console base, but the journal's own organic
// lifecycle badges (🌱 🔭 🔍 🧪 ✅ 🗄️) instead of checkboxes.
//
//   node tools/build-reflections.js          # regenerate docs/reflections.html
//   node tools/build-reflections.js --check   # exit 1 if stale (CI gate)
//
// Data comes from build-field-notes.js's collectNotes() — the SAME parse that
// builds FIELD-NOTES.md, so the page can't drift from the markdown index. Only
// NUMBERED journal notes appear (the actual reflections); the unnumbered working
// material is deliberately excluded — see the footer + the README curation note.

const fs = require("fs");
const path = require("path");
const { collectNotes, LIFECYCLE, STATUS_BADGE } = require("./build-field-notes.js");

const ROOT = path.resolve(__dirname, "..");
const OUT = path.join(ROOT, "docs", "reflections.html");
const check = process.argv.includes("--check");

const esc = s => String(s == null ? "" : s)
  .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");

const { journal, working } = collectNotes();

// lifecycle → accent + one-line gloss + default-open. The "live edge" (hunches,
// things under review, forming theories) opens; settled/archived collapses.
const LC = {
  "Hypothesis":     { c: "var(--green)",  open: true,  gloss: "a hunch, not yet tested." },
  "Observed":       { c: "var(--blue)",   open: true,  gloss: "something we saw happen, recorded." },
  "Review":         { c: "var(--yellow)", open: true,  gloss: "under active reconsideration." },
  "Working Theory": { c: "var(--orange)", open: true,  gloss: "a model we're provisionally running with." },
  "Incorporated":   { c: "var(--dim)",    open: false, gloss: "absorbed into how the repo works." },
  "Superseded":     { c: "var(--red)",    open: false, gloss: "replaced by a later note (kept for history)." },
  "Unmarked":       { c: "var(--dim)",    open: false, gloss: "no lifecycle status yet." },
};

const num = n => (n.num != null ? String(n.num).padStart(3, "0") : "");
const docOf = n => "field-notes/" + n.file;

function noteRow(n) {
  const meta = `<span class="nmeta">${num(n)}${n.date ? ` · ${esc(n.date)}` : ""}</span>`;
  const sum = n.summary ? `<div class="nsum">${esc(n.summary)}</div>` : "";
  return `<a class="note" data-doc="${esc(docOf(n))}" href="#">
    <div class="nhead"><span class="badge">${STATUS_BADGE[n.status] || "·"}</span><span class="ntitle">${esc(n.title)}</span>${meta}</div>${sum}
  </a>`;
}

function lifecycleBlock(st) {
  const group = journal.filter(n => n.status === st);
  if (!group.length) return "";
  group.sort((a, b) => a.num - b.num);
  const lc = LC[st] || LC.Unmarked;
  const hd = `<span class="lead">▸</span>${STATUS_BADGE[st] || "·"} ${esc(st)} <span class="n">${group.length}</span>`;
  return `<section class="ph" style="--c:${lc.c}">
    <details${lc.open ? " open" : ""}><summary>${hd}</summary>
    <p class="gloss">${esc(lc.gloss)}</p>
    ${group.map(noteRow).join("")}</details>
  </section>`;
}

// concepts: tag → note numbers
const conceptMap = {};
for (const n of journal) for (const c of n.concepts) (conceptMap[c] ||= []).push(n);
const conceptChips = Object.keys(conceptMap).sort()
  .map(c => `<span class="chip">${esc(c)} <b>${conceptMap[c].map(num).join(" ")}</b></span>`).join("");

// timeline (collapsed) — the spine, in order
const timeline = [...journal].sort((a, b) =>
  (a.date && b.date && a.date !== b.date) ? a.date.localeCompare(b.date) : a.num - b.num);
const timelineRows = timeline.map(n =>
  `<a class="tl" data-doc="${esc(docOf(n))}" href="#"><span class="badge">${STATUS_BADGE[n.status] || "·"}</span> <span class="tlnum">${num(n)}</span> ${esc(n.title)} <span class="nmeta">${n.date ? esc(n.date) : "undated"}</span></a>`).join("");

const counts = LIFECYCLE.map(st => {
  const c = journal.filter(n => n.status === st).length;
  return c ? `${STATUS_BADGE[st]} ${c} ${esc(st.toLowerCase())}` : "";
}).filter(Boolean).join("  ·  ");

const html = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Reflections</title>
<style>${CSS()}</style>
</head>
<body>
<div id="root">
  <header>
    <h1>REFLECTIONS</h1>
    <div class="count">${counts}</div>
    <p class="sub">The research journal — not what we built (that's <b class="hi">★ todo</b>), but what we've <i>learned</i> and how we work. Each note sits somewhere on the path from a hunch to something the repo adopted. Click one to read it.</p>
  </header>
  ${LIFECYCLE.map(lifecycleBlock).join("")}
  ${conceptChips ? `<section class="ph" style="--c:var(--indigo)"><details><summary><span class="lead">▸</span>CONCEPTS</summary><div class="chips">${conceptChips}</div></details></section>` : ""}
  <section class="ph" style="--c:var(--dim)">
    <details><summary><span class="lead">▸</span>TIMELINE <span class="n">${timeline.length}</span></summary>
    <div class="tlwrap">${timelineRows}</div></details>
  </section>
  <footer># the ${journal.length} numbered journal notes. ${working.length} unnumbered working docs (handoffs, scaffolding, wishlists) are excluded — they're the soil, not the reflections. Generated by tools/build-reflections.js from the field-notes journal; re-run after adding a note.</footer>
</div>
<script>${JS()}</script>
</body>
</html>
`;

if (check) {
  const cur = fs.existsSync(OUT) ? fs.readFileSync(OUT, "utf8") : "";
  if (cur.trim() !== html.trim()) { console.error("docs/reflections.html is STALE — run: node tools/build-reflections.js"); process.exit(1); }
  console.log("reflections.html up to date ✓");
  process.exit(0);
}
fs.writeFileSync(OUT, html);
console.log(`wrote ${path.relative(ROOT, OUT)} — ${journal.length} notes`);

// ---- CSS: same console base as the todo board; emoji lifecycle badges, note rows ----
function CSS() {
  return `
@font-face{font-family:'ComicMono';src:url('/ComicMono.ttf') format('truetype');font-weight:400;font-display:optional}
@font-face{font-family:'ComicMono';src:url('/ComicMono-Bold.ttf') format('truetype');font-weight:700;font-display:optional}
:root{
  --bg:#0a1a0f; --panel:#102816; --ink:#cbd3dc; --bright:#f0f4f8; --dim:#5b6470;
  --green:#00e436; --yellow:#ffec27; --orange:#ffa300; --blue:#29adff; --red:#ff004d; --indigo:#a39bc4;
  --mono:'ComicMono',ui-monospace,"SF Mono",Menlo,Consolas,"Liberation Mono",monospace;
}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--ink);font-family:var(--mono);font-size:13px;line-height:1.5}
#root{max-width:880px;margin:0 auto;padding:34px 26px 120px}
header{border-bottom:1px solid #20242c;padding-bottom:18px;margin-bottom:8px}
h1{font-size:30px;letter-spacing:4px;margin:0;color:var(--bright);font-weight:700;text-shadow:0 0 14px rgba(41,173,255,.28),0 0 2px rgba(41,173,255,.5)}
.count{margin-top:9px;color:var(--dim);font-size:12px}
.sub{margin:12px 0 0;color:var(--dim);max-width:700px;line-height:1.6}
.sub b.hi{color:var(--blue);font-weight:700} .sub i{color:var(--ink);font-style:italic}

section.ph{margin:24px 0 0}
section.ph details>summary{font-size:12px;letter-spacing:2px;color:var(--c);font-weight:700;text-transform:uppercase;
  display:flex;align-items:center;gap:8px;cursor:pointer;list-style:none;margin-bottom:4px}
section.ph details>summary::-webkit-details-marker{display:none}
section.ph summary .lead{display:inline-block;opacity:.7;transition:transform .12s}
section.ph details[open]>summary .lead{transform:rotate(90deg)}
section.ph summary .n{color:var(--dim);font-weight:400;letter-spacing:0}
section.ph details>summary:focus-visible{outline:2px solid var(--c);outline-offset:3px}
.gloss{color:var(--dim);font-size:12px;margin:0 0 10px 18px;font-style:italic}

a.note{display:block;padding:7px 10px 8px;text-decoration:none;color:var(--ink);border-left:2px solid transparent}
a.note:hover{background:var(--panel);border-left-color:var(--c)}
a.note .nhead{display:flex;align-items:baseline;gap:8px}
a.note .badge{flex:none}
a.note .ntitle{color:var(--bright);font-weight:700;flex:1 1 auto}
a.note .nmeta{flex:none;color:var(--dim);font-size:11px;white-space:nowrap}
a.note .nsum{color:var(--dim);font-size:12px;margin:2px 0 0 26px;line-height:1.45}

.chips{display:flex;flex-wrap:wrap;gap:6px}
.chip{background:var(--panel);border:1px solid #20242c;padding:3px 9px;font-size:11.5px}
.chip b{color:var(--indigo);font-weight:700}
.tlwrap{display:flex;flex-direction:column;gap:1px}
a.tl{text-decoration:none;color:var(--ink);padding:3px 10px;font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
a.tl:hover{background:var(--panel)}
a.tl .tlnum{color:var(--dim)} a.tl .nmeta{color:var(--dim);font-size:11px}

footer{margin-top:46px;padding-top:16px;border-top:1px solid #20242c;color:#3f4651;font-size:11px;line-height:1.6}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
`;
}
function JS() {
  return `
document.addEventListener('click', function(ev){
  var a = ev.target.closest('[data-doc]');
  if(!a) return;
  ev.preventDefault();
  var rel = a.getAttribute('data-doc');
  if(/^[\\w./-]+\\.md$/.test(rel)) window.parent.postMessage({type:'open-doc', path: rel}, '*');
});
`;
}
