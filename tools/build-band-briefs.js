#!/usr/bin/env node
// build-band-briefs.js — generate docs/band-briefs.html, the VISUAL ★ Band Briefs
// page: every "blind band brief" (docs/design/*-blind-brief.md) paired with the radio
// cart it became. Sibling of build-reflections.js / build-design-board.js — same
// Comic-Mono console base, but a gallery of cart thumbnails instead of a list.
//
// The firewall (docs/guides/cart-authoring-prompt.md): before reading ANY cousin
// cart, you imagine the ideal band for the genre and write it down. That intent doc
// is the blind brief; the shipped station is what it became. This page is the
// brief↔station pairing, so the discipline is visible and the briefs stay findable.
//
//   node tools/build-band-briefs.js          # regenerate docs/band-briefs.html
//   node tools/build-band-briefs.js --check   # exit 1 if stale (CI / pre-commit gate)
//
// Data: each docs/design/*-blind-brief.md. The cart name comes from the STATUS line
// (`<name>.c`), NOT the filename — vaporwave-blind-brief.md ships as vapor.c. The
// thumbnail is /carts/<cart>.cart.png (served by vite + the static site alike).

const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const DESIGN = path.join(ROOT, "docs", "design");
const CARTS = path.join(ROOT, "editor", "public", "carts");
const OUT = path.join(ROOT, "docs", "band-briefs.html");
const check = process.argv.includes("--check");

const esc = s => String(s == null ? "" : s)
  .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");

// ---- collect: parse every *-blind-brief.md into { file, title, cart, flavor, hasCart } ----
function collectBriefs() {
  const files = fs.readdirSync(DESIGN).filter(f => /-blind-brief\.md$/.test(f)).sort();
  const briefs = [];
  for (const file of files) {
    const text = fs.readFileSync(path.join(DESIGN, file), "utf8");
    const h1 = (text.match(/^#\s+(.+)$/m) || [])[1] || file.replace(/-blind-brief\.md$/, "");
    const title = h1
      .replace(/\s*—\s*the blind band brief.*$/i, "")
      .replace(/\s*\(Phase\s*\d+\)\s*$/i, "")
      .trim();
    const statusLine = (text.match(/^STATUS:\s*(.+)$/m) || [])[1] || "";
    const cart = (statusLine.match(/\b([a-z0-9]+)\.c\b/) || [])[1] || null;
    // flavor: the descriptor the STATUS line carries in parens, when present
    const flavor = (statusLine.match(/\(([^)]+)\)/) || [])[1] || "";
    const hasCart = cart && fs.existsSync(path.join(CARTS, cart + ".cart.png"));
    briefs.push({ file, title, cart, flavor: flavor.trim(), hasCart });
  }
  // shipped-with-cart first (the gallery), then any brief without a built cart
  return briefs.sort((a, b) =>
    (b.hasCart ? 1 : 0) - (a.hasCart ? 1 : 0) || a.title.localeCompare(b.title));
}

const briefs = collectBriefs();
const built = briefs.filter(b => b.hasCart);
const pending = briefs.filter(b => !b.hasCart);

function card(b) {
  const docPath = "design/" + b.file;
  const thumb = b.hasCart
    ? `<div class="thumb"><img src="/carts/${esc(b.cart)}.cart.png" loading="lazy" alt="${esc(b.title)} cart"></div>`
    : `<div class="thumb empty">no cart yet</div>`;
  const flavor = b.flavor ? `<div class="bf">${esc(b.flavor)}</div>` : "";
  const cartTag = b.hasCart
    ? `<span class="play" data-cart="${esc(b.cart)}">▶ ${esc(b.cart)}.c</span>`
    : `<span class="play off">brief only</span>`;
  // the card loads the cart; the "read brief" pill opens the markdown (data-doc wins
  // in the click handler, so it's safe nested inside the data-cart card).
  return `<div class="brief"${b.hasCart ? ` data-cart="${esc(b.cart)}"` : ""}>
    ${thumb}
    <div class="body">
      <div class="bt">${esc(b.title)}</div>
      ${flavor}
      <div class="bactions">${cartTag}<span class="readbrief" data-doc="${esc(docPath)}">read brief →</span></div>
    </div>
  </div>`;
}

const countLine = `${built.length} stations built${pending.length ? `  ·  ${pending.length} brief-only` : ""}  ·  ${briefs.length} briefs`;

const html = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Band Briefs</title>
<style>${CSS()}</style>
</head>
<body>
<div id="root">
  <header>
    <h1>THE BAND BRIEFS</h1>
    <div class="count">${esc(countLine)}</div>
    <p class="sub">Before reading any cousin cart, you imagine the ideal band for the genre and write it down — the <b class="hi">firewall</b>. That intent doc is the <i>blind brief</i>; the radio station is what it became. Click a card to load the station, or <b>read brief →</b> to open the doc.</p>
  </header>
  <section class="grid">
    ${built.map(card).join("\n    ")}
  </section>
  ${pending.length ? `<section class="ph"><h2>brief, no cart yet</h2><div class="grid">${pending.map(card).join("")}</div></section>` : ""}
  <footer># the ${briefs.length} blind band briefs (docs/design/*-blind-brief.md), paired with the radio cart each became. The cart is read from each brief's STATUS line. Generated by tools/build-band-briefs.js — re-run after adding a brief or shipping its station.</footer>
</div>
<script>${JS()}</script>
</body>
</html>
`;

if (check) {
  const cur = fs.existsSync(OUT) ? fs.readFileSync(OUT, "utf8") : "";
  if (cur.trim() !== html.trim()) {
    console.error("docs/band-briefs.html is STALE — run: node tools/build-band-briefs.js");
    process.exit(1);
  }
  console.log("band-briefs.html up to date ✓");
  process.exit(0);
}
fs.writeFileSync(OUT, html);
console.log(`wrote ${path.relative(ROOT, OUT)} — ${built.length} stations, ${pending.length} brief-only`);

module.exports = { collectBriefs };

// ---- CSS: the shared console base; a thumbnail gallery of stations ----
function CSS() {
  return `
@font-face{font-family:'ComicMono';src:url('/ComicMono.ttf') format('truetype');font-weight:400;font-display:optional}
@font-face{font-family:'ComicMono';src:url('/ComicMono-Bold.ttf') format('truetype');font-weight:700;font-display:optional}
:root{
  --bg:#0c0d11; --panel:#13151b; --ink:#cbd3dc; --bright:#f0f4f8; --dim:#5b6470;
  --green:#00e436; --yellow:#ffec27; --orange:#ffa300; --blue:#29adff; --red:#ff004d; --indigo:#a39bc4;
  --mono:'ComicMono',ui-monospace,"SF Mono",Menlo,Consolas,"Liberation Mono",monospace;
}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--ink);font-family:var(--mono);font-size:13px;line-height:1.5}
#root{max-width:980px;margin:0 auto;padding:34px 26px 120px}
header{border-bottom:1px solid #20242c;padding-bottom:18px;margin-bottom:18px}
h1{font-size:30px;letter-spacing:4px;margin:0;color:var(--bright);font-weight:700;text-shadow:0 0 14px rgba(163,155,196,.32),0 0 2px rgba(163,155,196,.5)}
.count{margin-top:9px;color:var(--dim);font-size:12px}
.sub{margin:12px 0 0;color:var(--dim);max-width:760px;line-height:1.6}
.sub b.hi{color:var(--indigo);font-weight:700} .sub i{color:var(--ink);font-style:italic} .sub b{color:var(--bright)}

.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:14px}
.brief{background:var(--panel);border:1px solid #20242c;border-radius:4px;overflow:hidden;display:flex;flex-direction:column;
  transition:border-color .12s,transform .12s}
.brief[data-cart]{cursor:pointer}
.brief[data-cart]:hover{border-color:var(--indigo);transform:translateY(-2px)}
.thumb{aspect-ratio:16/10;background:#06070a;display:flex;align-items:center;justify-content:center;overflow:hidden;border-bottom:1px solid #20242c}
.thumb img{width:100%;height:100%;object-fit:cover;image-rendering:pixelated}
.thumb.empty{color:var(--dim);font-size:11px;letter-spacing:1px;text-transform:uppercase}
.body{padding:10px 12px 12px;display:flex;flex-direction:column;gap:5px;flex:1}
.bt{color:var(--bright);font-weight:700;font-size:13.5px;line-height:1.3}
.bf{color:var(--dim);font-size:12px;line-height:1.45;flex:1}
.bactions{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-top:4px}
.play{color:var(--green);font-size:12px;font-weight:700} .play.off{color:var(--dim);font-weight:400}
.readbrief{color:var(--blue);font-size:11.5px;cursor:pointer;border-bottom:1px dotted transparent}
.readbrief:hover{border-bottom-color:var(--blue)}

section.ph{margin:34px 0 0}
section.ph h2{font-size:12px;letter-spacing:2px;color:var(--dim);font-weight:700;text-transform:uppercase;margin:0 0 12px}

footer{margin-top:46px;padding-top:16px;border-top:1px solid #20242c;color:#3f4651;font-size:11px;line-height:1.6}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
`;
}
function JS() {
  return `
document.addEventListener('click', function(ev){
  var d = ev.target.closest('[data-doc]');
  if(d){ ev.preventDefault(); var rel = d.getAttribute('data-doc');
    if(/^[\\w./-]+\\.md$/.test(rel)) window.parent.postMessage({type:'open-doc', path: rel}, '*'); return; }
  var c = ev.target.closest('[data-cart]');
  if(c){ ev.preventDefault(); var n = c.getAttribute('data-cart');
    if(/^[\\w-]+$/.test(n)) window.parent.postMessage({type:'load-cart', file: n + '.cart.png', title: n}, '*'); }
});
`;
}
