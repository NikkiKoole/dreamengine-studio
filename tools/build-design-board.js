#!/usr/bin/env node
// build-design-board.js — generate docs/design-board.html, the VISUAL twin of
// design-board.js: every design doc + ADR as a clickable item in a lo-fi CONSOLE
// CHECKLIST — grouped by lifecycle phase, ASCII checkboxes ([ ] ready · [~] wip ·
// ( ) idea · [x] done), PICO-8 palette, faint CRT scanline. A todo-list deluxe
// that mirrors the docs' own `- [ ]` markdown and the fantasy-console identity.
// Shown in the editor's ★ designs Docs tab in an iframe; clicking an item
// postMessages the parent editor to open that doc's markdown.
//
//   node tools/build-design-board.js          # regenerate docs/design-board.html
//   node tools/build-design-board.js --check   # exit 1 if the page is stale (CI gate)
//
// Data comes from design-board.js's collectBoard() (the same scan the terminal
// board uses), so the page can't drift from the CLI. Re-run after editing any
// design doc's STATUS line.

const fs = require("fs");
const path = require("path");
const { collectBoard } = require("./design-board.js");

const ROOT = path.resolve(__dirname, "..");
const OUT = path.join(ROOT, "docs", "design-board.html");
const check = process.argv.includes("--check");

const esc = s => String(s == null ? "" : s)
  .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");

const { board, adrs, unmarked } = collectBoard();

// per-phase: the ASCII checkbox glyph, the PICO-8 accent, the label, and whether
// it renders as full rows (the actionable ones) or compact chips (the long tails).
const PH = {
  ready:     { glyph: "[ ]", c: "var(--green)",  label: "READY TO BUILD",       mode: "row"  },
  building:  { glyph: "[~]", c: "var(--orange)", label: "IN PROGRESS",          mode: "row"  },
  other:     { glyph: "[?]", c: "var(--indigo)", label: "LIVING / UNCLASSIFIED", mode: "row" },
  exploring: { glyph: "( )", c: "var(--blue)",   label: "EXPLORING / IDEAS",    mode: "chip" },
  shipped:   { glyph: "[x]", c: "var(--dim)",    label: "SHIPPED",              mode: "chip" },
  cut:       { glyph: "[/]", c: "var(--red)",    label: "CUT / SUPERSEDED",     mode: "chip" },
};
const RENDER_ORDER = ["ready", "building", "other", "exploring", "shipped", "cut"];

const MON = ["jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"];
const fmtDate = d => { if (!d) return ""; const [, m, day] = d.split("-"); return `${+day} ${MON[+m - 1] || ""}`; };
const bar = (done, total) => { const n = 8, f = total ? Math.round(n * done / total) : 0; return "█".repeat(f) + "░".repeat(n - f); };

const docRel = e => e.rel.replace(/^docs\//, "");

function rowItem(e) {
  let meta = "";
  if (e.plan) meta += `<span class="pb">${bar(e.plan.done, e.plan.total)}</span><span class="pn">${e.plan.done}/${e.plan.total}</span>`;
  if (e.date) meta += `<span class="dt">${esc(fmtDate(e.date))}</span>`;
  const status = e.status ? ` title="${esc(e.status)}"` : "";
  return `<a class="row" data-doc="${esc(docRel(e))}" href="#"${status}><span class="ck">${PH[e.phase].glyph}</span><span class="ttl">${esc(e.title)}</span><span class="meta">${meta}</span></a>`;
}
function chipItem(e) {
  return `<a class="chip" data-doc="${esc(docRel(e))}" href="#" title="${esc(e.status || "")}">${PH[e.phase].glyph} ${esc(e.title)}</a>`;
}

function phaseBlock(k) {
  const group = board.filter(e => e.phase === k);
  if (!group.length) return "";
  group.sort((a, b) => (b.date || "").localeCompare(a.date || "") || a.title.localeCompare(b.title));
  const ph = PH[k];
  const body = ph.mode === "row"
    ? group.map(rowItem).join("")
    : `<div class="chips">${group.map(chipItem).join("")}</div>`;
  return `<section class="ph" style="--c:${ph.c}">
    <h2><span class="lead">▸</span>${esc(ph.label)} <span class="n">${group.length}</span></h2>
    ${body}
  </section>`;
}

const n = k => board.filter(e => e.phase === k).length;
const summary = [`${n("ready")} open`, `${n("building")} in progress`, `${n("exploring")} exploring`, `${n("shipped")} done`]
  .filter(s => !/^0 /.test(s)).join("  ·  ");
const newest = [...board, ...adrs].map(e => e.date).filter(Boolean).sort().pop() || "";

const adrCut = e => /\b(cut|rejected|superseded)\b/i.test(e.status || "");
const adrChips = adrs
  .sort((a, b) => (b.date || "").localeCompare(a.date || "") || a.rel.localeCompare(b.rel))
  .map(e => `<a class="chip${adrCut(e) ? " struck" : ""}" data-doc="${esc(docRel(e))}" href="#" title="${esc(e.status || "")}">${adrCut(e) ? "[/]" : "[x]"} ${esc(e.title)}</a>`)
  .join("");

const html = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Todo list</title>
<style>${CSS()}</style>
</head>
<body>
<div id="root">
  <header>
    <h1>TODO&nbsp;LIST</h1>
    <div class="count">${esc(summary)}<span class="sep">·</span>${board.length} designs<span class="sep">·</span>${adrs.length} ADRs<span class="sep">·</span>${unmarked} unmarked${newest ? `<span class="sep">·</span>newest ${esc(fmtDate(newest))}` : ""}</div>
    <p class="sub">Every design &amp; decision, by phase — read live from each doc's STATUS line. <b class="hi">[ ] READY</b> is specced work nobody's started. Click any line to open the doc.</p>
  </header>
  ${RENDER_ORDER.map(phaseBlock).join("")}
  <section class="ph" style="--c:var(--dim)">
    <h2><span class="lead">▸</span>DECISIONS <span class="n">${adrs.length}</span></h2>
    <div class="chips">${adrChips}</div>
  </section>
  <footer># generated by tools/build-design-board.js from tools/design-board.js — re-run after editing a STATUS line · unmarked: node tools/design-board.js --lint</footer>
</div>
<script>${JS()}</script>
</body>
</html>
`;

if (check) {
  const cur = fs.existsSync(OUT) ? fs.readFileSync(OUT, "utf8") : "";
  if (cur.trim() !== html.trim()) {
    console.error("docs/design-board.html is STALE — run: node tools/build-design-board.js");
    process.exit(1);
  }
  console.log("design-board.html up to date ✓");
  process.exit(0);
}

fs.writeFileSync(OUT, html);
console.log(`wrote ${path.relative(ROOT, OUT)} — ${board.length} docs, ${adrs.length} ADRs`);

// ---- CSS: lo-fi console checklist — near-black phosphor terminal, PICO-8 accents,
//      ASCII checkboxes, faint CRT scanline. Calm: ready is bright, done recedes. ----
function CSS() {
  return `
:root{
  --bg:#0c0d11; --panel:#13151b; --ink:#cbd3dc; --bright:#f0f4f8; --dim:#5b6470;
  --green:#00e436; --yellow:#ffec27; --orange:#ffa300; --blue:#29adff; --red:#ff004d; --indigo:#a39bc4;
  --mono:ui-monospace,"SF Mono","JetBrains Mono",Menlo,Consolas,"Liberation Mono",monospace;
}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--ink);font-family:var(--mono);font-size:13px;line-height:1.5}
/* faint CRT scanline + vignette, non-interactive */
body::before{content:"";position:fixed;inset:0;pointer-events:none;z-index:9;
  background:repeating-linear-gradient(0deg,rgba(0,0,0,0) 0 2px,rgba(0,0,0,.16) 2px 3px);
  mix-blend-mode:multiply;opacity:.5}
body::after{content:"";position:fixed;inset:0;pointer-events:none;z-index:8;
  background:radial-gradient(120% 90% at 50% 0%,transparent 60%,rgba(0,0,0,.45))}
#root{max-width:880px;margin:0 auto;padding:34px 26px 120px;position:relative;z-index:1}

header{border-bottom:1px solid #20242c;padding-bottom:18px;margin-bottom:8px}
h1{font-size:30px;letter-spacing:4px;margin:0;color:var(--bright);font-weight:700;
  text-shadow:0 0 14px rgba(0,228,54,.30),0 0 2px rgba(0,228,54,.5)}
.count{margin-top:9px;color:var(--dim);font-size:12px}
.count .sep{margin:0 8px;opacity:.5}
.sub{margin:12px 0 0;color:var(--dim);max-width:680px;line-height:1.6}
.sub b.hi{color:var(--green);font-weight:700}

section.ph{margin:26px 0 0}
section.ph h2{font-size:12px;letter-spacing:2px;color:var(--c);margin:0 0 8px;font-weight:700;
  text-transform:uppercase;display:flex;align-items:center;gap:8px}
section.ph h2 .lead{opacity:.7}
section.ph h2 .n{color:var(--dim);font-weight:400;letter-spacing:0}

/* full rows — the actionable phases */
a.row{display:flex;align-items:center;gap:10px;padding:4px 8px;text-decoration:none;color:var(--ink);
  border-left:2px solid transparent}
a.row:hover{background:var(--panel);border-left-color:var(--c)}
a.row .ck{color:var(--c);flex:none;font-weight:700}
a.row .ttl{color:var(--bright);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;flex:1 1 auto}
a.row .meta{flex:none;display:flex;align-items:center;gap:10px;color:var(--dim);font-size:11px;white-space:nowrap}
a.row .pb{color:var(--c);letter-spacing:-1px}
a.row .pn{color:var(--dim)}
a.row .dt{color:var(--dim);min-width:48px;text-align:right}

/* chip rows — the long tails (exploring / shipped / cut / ADRs) */
.chips{display:flex;flex-wrap:wrap;gap:6px}
a.chip{text-decoration:none;color:var(--ink);background:var(--panel);border:1px solid #20242c;
  padding:3px 9px;font-size:11.5px;white-space:nowrap}
a.chip:hover{border-color:var(--c);color:var(--bright)}
section.ph[style*="--dim"] a.chip,.ph .chip{} /* shipped/ADR phases use --c=dim → muted naturally */
a.chip.struck{text-decoration:line-through;color:var(--dim)}

footer{margin-top:48px;padding-top:16px;border-top:1px solid #20242c;color:#3f4651;font-size:11px}
`;
}

// ---- page JS: click an item → open that doc's markdown in the parent editor ----
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
