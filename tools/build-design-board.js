#!/usr/bin/env node
// build-design-board.js — generate docs/design-board.html, the VISUAL twin of
// design-board.js: every design doc + ADR as a clickable card, grouped by
// lifecycle phase (READY TO BUILD headlined), styled like the ★ history /
// ★ techniques pages and shown in the editor's Docs tab in an iframe. Clicking a
// card postMessages the parent editor to open that doc's markdown.
//
//   node tools/build-design-board.js          # regenerate docs/design-board.html
//   node tools/build-design-board.js --check   # exit 1 if the page is stale (CI gate)
//
// Data comes from design-board.js's collectBoard() (the same scan the terminal
// board uses), so the page can't drift from the CLI. Re-run after editing any
// design doc's STATUS line.

const fs = require("fs");
const path = require("path");
const { collectBoard, ORDER, LABEL } = require("./design-board.js");

const ROOT = path.resolve(__dirname, "..");
const OUT = path.join(ROOT, "docs", "design-board.html");
const check = process.argv.includes("--check");

const esc = s => String(s == null ? "" : s)
  .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");

const { board, adrs, unmarked } = collectBoard();

// phase → accent colour (pink = hottest/ready, fading to muted for shipped)
const ACCENT = { ready: "var(--pink)", building: "var(--orange)", exploring: "var(--yellow)",
  other: "var(--dim)", shipped: "var(--green)", cut: "var(--dim)" };
const BLURB = {
  ready: "Specced, planned, nobody on it — pick one up.",
  building: "In flight right now.",
  exploring: "Idea / brainstorm / open research — not yet committed.",
  other: "Has a status that doesn't map to a phase (a living doc, or reword it).",
  shipped: "Done and in the engine.",
  cut: "Decided against / superseded.",
};

const newest = [...board, ...adrs].map(e => e.date).filter(Boolean).sort().pop() || "";

function card(e) {
  const docRel = e.rel.replace(/^docs\//, "");                 // editor opens docs-relative .md
  const date = e.date ? `<span class="date">${esc(e.date)}</span>` : "";
  let plan = "";
  if (e.plan) {
    const pct = e.plan.total ? Math.round(100 * e.plan.done / e.plan.total) : 0;
    plan = `<div class="plan" title="${e.plan.done} of ${e.plan.total} plan steps done">
      <div class="bar"><i style="width:${pct}%"></i></div><span>${e.plan.done}/${e.plan.total}</span></div>`;
  }
  const status = e.status ? `<p class="status">${esc(e.status)}</p>` : "";
  return `<a class="card" data-doc="${esc(docRel)}" href="#" style="--c:${ACCENT[e.phase] || "var(--dim)"}">
    <div class="chead"><b>${esc(e.title)}</b>${date}</div>
    ${status}${plan}
    <code>${esc(docRel)}</code>
  </a>`;
}

function section(k) {
  const group = board.filter(e => e.phase === k);
  if (!group.length) return "";
  group.sort((a, b) => (b.date || "").localeCompare(a.date || "") || a.title.localeCompare(b.title));
  return `<section class="phase phase-${k}">
    <h2 style="--c:${ACCENT[k] || "var(--dim)"}">${esc(LABEL[k])} <span class="n">${group.length}</span></h2>
    <p class="blurb">${esc(BLURB[k] || "")}</p>
    <div class="grid">${group.map(card).join("")}</div>
  </section>`;
}

const counts = ORDER.map(k => {
  const n = board.filter(e => e.phase === k).length;
  return n ? `<span class="chip" style="--c:${ACCENT[k]}"><i></i>${n} ${esc(LABEL[k].toLowerCase())}</span>` : "";
}).join("");

const adrAccepted = adrs.filter(e => /accept/i.test(e.status || "")).length;
const adrCards = adrs
  .sort((a, b) => (b.date || "").localeCompare(a.date || "") || a.rel.localeCompare(b.rel))
  .map(e => {
    const docRel = e.rel.replace(/^docs\//, "");
    const cut = /\b(cut|rejected|superseded)\b/i.test(e.status || "");
    return `<a class="adr${cut ? " cut" : ""}" data-doc="${esc(docRel)}" href="#" title="${esc(e.status || "")}">${esc(e.title)}</a>`;
  }).join("");

const html = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Design board</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Archivo+Black&family=Space+Grotesk:wght@400;500;700&display=swap" rel="stylesheet">
<style>${CSS()}</style>
</head>
<body>
<div id="root">
  <header class="top">
    <h1>Design Board</h1>
    <p class="sub">Every design doc &amp; decision in the repo, by lifecycle phase — derived live from each doc's <code>STATUS</code> line. The headline is <b>Ready to Build</b>: specced work nobody's started.</p>
    <div class="meta"><b>${board.length}</b> design docs · <b>${adrs.length}</b> ADRs · <b>${unmarked}</b> unmarked${newest ? ` · newest ${esc(newest)}` : ""}</div>
    <div class="legend">${counts}</div>
  </header>
  ${ORDER.map(section).join("")}
  <section class="phase phase-adr">
    <h2 style="--c:var(--dim)">Decisions <span class="n">${adrs.length}</span></h2>
    <p class="blurb">${adrAccepted} accepted · ${adrs.length - adrAccepted} proposed/other — settled calls (docs/decisions/).</p>
    <div class="adrs">${adrCards}</div>
  </section>
  <footer>Generated by <code>tools/build-design-board.js</code> from <code>tools/design-board.js</code>. Re-run after editing a doc's STATUS line. Unmarked docs: <code>node tools/design-board.js --lint</code>.</footer>
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

// ---- CSS (neo-brutalism, matching history.html: dark, pink/orange/yellow, hard shadows) ----
function CSS() {
  return `
:root{
  --bg:#1b1c1f; --panel:#22232a; --panel2:#2a2c34; --ink:#e7e9ee; --dim:#9094a2;
  --edge:#000; --pink:#ff6fb5; --orange:#ff7a2f; --yellow:#ffd23f; --green:#5fd07a;
  --on:#141414; --sh:5px 5px 0 var(--edge); --sh-sm:3px 3px 0 var(--edge);
  --disp:"Archivo Black",Impact,system-ui,sans-serif;
  --body:"Space Grotesk",ui-sans-serif,system-ui,sans-serif;
  --mono:ui-monospace,"SF Mono",Menlo,monospace;
}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--ink);font-family:var(--body);font-size:14px;line-height:1.5;
  background-image:radial-gradient(var(--panel2) 1px,transparent 1px);background-size:22px 22px}
code{font-family:var(--mono);font-size:11px}
#root{max-width:1040px;margin:0 auto;padding:0 24px 120px}
header.top{padding:40px 0 8px}
header.top h1{font-family:var(--disp);margin:0 0 12px;font-size:42px;line-height:.98;letter-spacing:-1px;
  text-transform:uppercase;text-shadow:4px 4px 0 var(--pink)}
header.top .sub{font-size:15px;max-width:720px;font-weight:500;color:var(--ink)}
header.top .sub b{color:var(--pink)}
header.top .meta{display:inline-block;color:var(--on);background:var(--yellow);border:3px solid var(--edge);
  box-shadow:var(--sh-sm);font-size:12px;margin-top:16px;padding:6px 12px;font-weight:700;font-family:var(--mono)}
.legend{display:flex;flex-wrap:wrap;gap:8px;margin:18px 0 4px}
.chip{display:inline-flex;align-items:center;gap:6px;background:var(--panel);border:2px solid var(--edge);
  box-shadow:2px 2px 0 var(--edge);padding:3px 10px;font-size:12px;font-weight:700;text-transform:capitalize}
.chip i{width:10px;height:10px;border:2px solid var(--edge);background:var(--c)}
.phase{margin:40px 0 0}
.phase h2{font-family:var(--disp);font-size:22px;text-transform:uppercase;letter-spacing:-.5px;margin:0;
  display:inline-block;background:var(--c);color:var(--on);border:3px solid var(--edge);box-shadow:var(--sh-sm);padding:5px 14px}
.phase h2 .n{font-family:var(--mono);font-size:13px;opacity:.7}
.phase .blurb{color:var(--dim);font-size:13px;margin:10px 0 16px;font-weight:500}
.phase-ready h2{transform:rotate(-1deg)}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(290px,1fr));gap:14px}
.card{display:block;background:var(--panel);border:3px solid var(--edge);box-shadow:var(--sh);padding:13px 14px;
  border-left:7px solid var(--c);transition:transform .08s,box-shadow .08s;cursor:pointer}
.card:hover{transform:translate(-2px,-2px);box-shadow:7px 7px 0 var(--edge)}
.card .chead{display:flex;justify-content:space-between;align-items:baseline;gap:8px}
.card .chead b{font-size:14.5px;font-weight:700;line-height:1.2}
.card .date{font-family:var(--mono);font-size:10px;color:var(--dim);white-space:nowrap}
.card .status{color:var(--dim);font-size:12px;margin:7px 0 8px;line-height:1.45;
  display:-webkit-box;-webkit-line-clamp:3;-webkit-box-orient:vertical;overflow:hidden}
.card code{color:var(--c);opacity:.85}
.card .plan{display:flex;align-items:center;gap:8px;margin:6px 0 9px}
.card .plan .bar{flex:1;height:8px;background:var(--panel2);border:2px solid var(--edge);overflow:hidden}
.card .plan .bar i{display:block;height:100%;background:var(--c)}
.card .plan span{font-family:var(--mono);font-size:10px;color:var(--dim)}
.adrs{display:flex;flex-wrap:wrap;gap:8px}
.adr{background:var(--panel);border:2px solid var(--edge);box-shadow:2px 2px 0 var(--edge);padding:5px 11px;
  font-size:12px;font-weight:600;cursor:pointer}
.adr:hover{background:var(--panel2)}
.adr.cut{text-decoration:line-through;color:var(--dim)}
footer{margin-top:54px;padding-top:18px;border-top:2px dashed var(--panel2);color:var(--dim);font-size:12px}
`;
}

// ---- page JS: click a card/adr → open that doc's markdown in the parent editor ----
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
