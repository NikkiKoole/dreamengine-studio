#!/usr/bin/env node
// build-compendium.js — generate docs/cart-compendium.html, the browsable CART
// TECHNIQUE COMPENDIUM (editor Docs tab → ★ techniques). A sibling of
// build-history.js: derives everything from the same sources cart-index.js uses —
// in-source `// TEACHES:` / `// LINEAGE:` headers (source of truth) with the
// tools/carts/teaches.json sidecar as fallback — so it's a reproducible artifact of
// the tag data, not hand-maintained. Re-run after re-tagging or adding tagged carts.
//
//   node tools/build-compendium.js     → writes docs/cart-compendium.html
//
// The page is also self-contained (publishes fine as a standalone artifact); when it
// runs inside the editor's docs iframe, clicking a cart name loads that cart.

const fs = require('fs');
const path = require('path');
const ROOT = path.resolve(__dirname, '..');
const CARTS = path.join(ROOT, 'tools/carts');

const idx = require(path.join(ROOT, 'editor/public/carts/index.json'));
const REG = Array.isArray(idx) ? idx : Object.values(idx)[0];
const meta = Object.fromEntries(REG.map(c => [c.file.replace('.cart.png', ''), c]));

// teaches[] + lineage live IN the index.json entry (one home, can't drift from the registry)
function tagsFor(name) {
  const m = meta[name] || {};
  return { teaches: Array.isArray(m.teaches) ? m.teaches : [], lineage: m.lineage || '' };
}

// concept → category (mirrors the vocab grouping in cart-index.js)
const CAT = {
  world: ['gene-based-procgen','wave-function-collapse','marching-squares','autotiling','noise-terrain','maze-generation','dungeon-generation','l-system','cellular-automata'],
  ai: ['state-machine','finite-state-ai','pathfinding','flocking','car-following','steering-behaviors','traffic-sim','schedule-driven-agents'],
  physics: ['verlet-integration','spring-damper','rigid-body','particle-system','fluid-sim','soft-body','segment-collision'],
  render: ['raycasting','mode7','parallax','camera-follow','dithering-gradient','palette-cycling','software-rasterizer','procedural-mesh','sprite-stacking','no-sprite-vehicles','radial-symmetry','isometric-projection'],
  game: ['title-play-gameover-loop','tilemap-collision','inventory-system','dialogue-tree','turn-based-loop','grid-movement','save-load-persistence','screen-shake-juice','genetic-crossover','algorithm-visualization'],
  audio: ['subtractive-synth','granular-synth','waveguide-synth','fm-synth','additive-synth','step-sequencer','euclidean-rhythm','adsr-envelope','generative-melody','chord-voicing','drum-synthesis','analog-voice-modeling','swing-timing','sonification','audio-occlusion','positional-audio','wavetable-drawing'],
};
const catOf = {};
for (const [c, tags] of Object.entries(CAT)) tags.forEach(t => catOf[t] = c);
const CATMETA = {
  world:   { label: 'World & procgen', color: '#00E436' },
  ai:      { label: 'Agents & AI',     color: '#FFA300' },
  physics: { label: 'Physics',         color: '#29ADFF' },
  render:  { label: 'Rendering',       color: '#FF77A8' },
  game:    { label: 'Game structure',  color: '#FFEC27' },
  audio:   { label: 'Audio',           color: '#FF004D' },
  other:   { label: 'Other',           color: '#C2C3C7' },
};

const carts = [];
for (const name of Object.keys(meta)) {
  const { teaches, lineage } = tagsFor(name);
  if (!teaches.length) continue;
  carts.push({ name, kind: meta[name].kind || [], teaches, lineage });
}
carts.sort((a, b) => a.name.localeCompare(b.name));

const concepts = {};
for (const c of carts) for (const t of c.teaches) (concepts[t] = concepts[t] || []).push(c.name);
const conceptList = Object.entries(concepts).map(([tag, cs]) => ({ tag, cat: catOf[tag] || 'other', n: cs.length }))
  .sort((a, b) => b.n - a.n);

const DATA = JSON.stringify({ carts, conceptList, catOf, CATMETA });

// embed the editor's own font (Comic Mono) so the page matches the IDE and stays
// self-contained as a standalone artifact (the CSP there blocks external fonts).
let FONT_FACE = '';
try {
  const b64 = fs.readFileSync(path.join(ROOT, 'editor/public/ComicMono.ttf')).toString('base64');
  FONT_FACE = `@font-face{font-family:'Comic Mono';src:url(data:font/ttf;base64,${b64}) format('truetype');font-display:swap;}`;
} catch {}

const html = `<title>dreamengine · cart technique compendium</title>
<style>
  ${FONT_FACE}
  :root{
    --ground:#1D2B53; --panel:#22356A; --text:#FFF1E8; --dim:#8E9BC4; --line:#3A4E86;
    --green:#00E436; --orange:#FFA300; --blue:#29ADFF; --pink:#FF77A8; --yellow:#FFEC27; --red:#FF004D; --grey:#C2C3C7;
    --mono:'Comic Mono',ui-monospace,"SF Mono",Menlo,Consolas,monospace;
  }
  *{box-sizing:border-box;}
  body{margin:0;background:var(--ground);color:var(--text);font-family:var(--mono);
    font-size:14px;line-height:1.5;-webkit-font-smoothing:antialiased;}
  .wrap{max-width:1080px;margin:0 auto;padding:clamp(20px,4vw,56px) clamp(16px,4vw,40px) 80px;}
  header.hero{border-bottom:2px solid var(--line);padding-bottom:28px;margin-bottom:32px;}
  .eyebrow{font-size:11px;letter-spacing:.32em;text-transform:uppercase;color:var(--dim);margin:0 0 14px;}
  h1{font-size:clamp(30px,6vw,56px);line-height:1.02;margin:0;font-weight:700;letter-spacing:-.01em;}
  h1 .blink{color:var(--green);animation:blink 1.1s steps(1) infinite;}
  @keyframes blink{50%{opacity:0;}}
  @media (prefers-reduced-motion:reduce){.blink{animation:none;}}
  .lede{color:var(--dim);max-width:62ch;margin:16px 0 0;font-size:14px;}
  .lede b{color:var(--text);font-weight:600;}
  .stats{display:flex;flex-wrap:wrap;gap:26px;margin-top:24px;}
  .stat .num{font-size:26px;font-weight:700;}
  .stat .lbl{font-size:11px;letter-spacing:.12em;text-transform:uppercase;color:var(--dim);}
  .controls{position:sticky;top:0;z-index:5;background:var(--ground);padding:14px 0;margin-bottom:8px;border-bottom:1px solid var(--line);}
  #q{width:100%;background:var(--panel);border:1px solid var(--line);color:var(--text);font-family:var(--mono);font-size:14px;padding:11px 14px;border-radius:7px;}
  #q::placeholder{color:var(--dim);}
  #q:focus{outline:2px solid var(--green);outline-offset:1px;}
  .legend{display:flex;flex-wrap:wrap;gap:8px;margin:14px 0 4px;}
  .chip{font-family:var(--mono);font-size:12px;padding:4px 10px;border-radius:999px;cursor:pointer;border:1px solid var(--line);background:transparent;color:var(--text);white-space:nowrap;}
  .chip .dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:7px;vertical-align:1px;}
  .chip.off{opacity:.35;}
  .chip:focus-visible{outline:2px solid var(--green);}
  h2.sec{font-size:12px;letter-spacing:.24em;text-transform:uppercase;color:var(--dim);margin:38px 0 14px;border-bottom:1px solid var(--line);padding-bottom:8px;}
  .concepts{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:8px 18px;}
  .crow{display:flex;align-items:baseline;gap:9px;cursor:pointer;padding:3px 0;}
  .crow .dot{width:9px;height:9px;border-radius:50%;flex:none;position:relative;top:1px;}
  .crow .tag{flex:1;}
  .crow:hover .tag,.crow.active .tag{text-decoration:underline;text-underline-offset:3px;}
  .crow .cnt{color:var(--dim);font-size:12px;}
  .cart{padding:15px 0;border-bottom:1px solid var(--line);}
  .cart .top{display:flex;align-items:baseline;gap:12px;flex-wrap:wrap;}
  .cart .nm{font-size:16px;font-weight:700;}
  .cart .nm.link{cursor:pointer;color:var(--green);}
  .cart .nm.link:hover{text-decoration:underline;text-underline-offset:3px;}
  .cart .kind{font-size:11px;color:var(--dim);letter-spacing:.08em;text-transform:uppercase;}
  .tags{display:flex;flex-wrap:wrap;gap:6px;margin:9px 0;}
  .tag-pill{font-size:12px;padding:3px 9px;border-radius:5px;border:1px solid;background:#0e1838;}
  .lineage{color:var(--dim);font-size:13px;max-width:84ch;}
  .lineage::before{content:"↳ ";color:var(--line);}
  mark{background:var(--yellow);color:#000;padding:0 1px;border-radius:2px;}
  .empty{color:var(--dim);padding:40px 0;text-align:center;}
  footer{margin-top:48px;color:var(--dim);font-size:12px;border-top:1px solid var(--line);padding-top:18px;}
  footer code{color:var(--text);}
</style>

<div class="wrap">
  <header class="hero">
    <p class="eyebrow">dreamengine · library index</p>
    <h1>Cart technique<br>compendium<span class="blink">_</span></h1>
    <p class="lede">What each cart can teach you — the <b>conceptual</b> techniques behind the code,
      not the API calls. Lineage notes what each one descends from. Filter by category, click a
      concept to isolate it, search, or click a cart name to open it.</p>
    <div class="stats" id="stats"></div>
  </header>
  <div class="controls">
    <input id="q" type="text" placeholder="search carts, tags, lineage…  (e.g. raycasting, roland, procgen)" autocomplete="off">
    <div class="legend" id="legend"></div>
  </div>
  <h2 class="sec">Concepts — click to isolate</h2>
  <div class="concepts" id="concepts"></div>
  <h2 class="sec" id="cartsHead">Carts</h2>
  <div id="carts"></div>
  <div class="empty" id="empty" hidden>No carts match.</div>
  <footer>
    Generated by <code>tools/build-compendium.js</code> from in-source <code>// TEACHES:</code> headers +
    <code>tools/carts/teaches.json</code>. The <code>.c</code> header is the source of truth; the sidecar fills the back catalogue.
  </footer>
</div>

<script>
const D = ${DATA};
const {carts, conceptList, catOf, CATMETA} = D;
const embedded = window.parent && window.parent !== window;
const col = t => (CATMETA[catOf[t]] || CATMETA.other).color;
let activeCats = new Set(Object.keys(CATMETA));
let activeConcept = null, query = '';

document.getElementById('stats').innerHTML = [
  [carts.length, 'carts tagged'], [conceptList.length, 'concepts'], [Object.keys(CATMETA).length - 1, 'categories'],
].map(([n,l]) => '<div class="stat"><div class="num">'+n+'</div><div class="lbl">'+l+'</div></div>').join('');

const legend = document.getElementById('legend');
legend.innerHTML = Object.entries(CATMETA).filter(([k])=>k!=='other').map(([k,m]) =>
  '<button class="chip" data-cat="'+k+'"><span class="dot" style="background:'+m.color+'"></span>'+m.label+'</button>').join('');
legend.addEventListener('click', e => {
  const b = e.target.closest('[data-cat]'); if(!b) return;
  const c = b.dataset.cat; activeCats.has(c) ? activeCats.delete(c) : activeCats.add(c);
  b.classList.toggle('off', !activeCats.has(c)); render();
});

const conceptsEl = document.getElementById('concepts');
conceptsEl.innerHTML = conceptList.map(c =>
  '<div class="crow" data-tag="'+c.tag+'"><span class="dot" style="background:'+col(c.tag)+'"></span>'+
  '<span class="tag">'+c.tag+'</span><span class="cnt">'+c.n+'</span></div>').join('');
conceptsEl.addEventListener('click', e => {
  const r = e.target.closest('[data-tag]'); if(!r) return;
  const t = r.dataset.tag; activeConcept = activeConcept === t ? null : t;
  [...conceptsEl.children].forEach(ch => ch.classList.toggle('active', ch.dataset.tag === activeConcept));
  render();
});

const q = document.getElementById('q');
q.addEventListener('input', () => { query = q.value.trim().toLowerCase(); render(); });

const cartsEl = document.getElementById('carts');
cartsEl.addEventListener('click', e => {
  const nm = e.target.closest('.nm.link'); if(!nm || !embedded) return;
  window.parent.postMessage({ type:'load-cart', file: nm.dataset.cart + '.cart.png', title: nm.dataset.cart }, '*');
});

const hl = (s) => query ? s.replace(new RegExp('('+query.replace(/[.*+?^\${}()|[\\]\\\\]/g,'\\\\$&')+')','ig'),'<mark>$1</mark>') : s;
const escp = s => s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');

function visible(c){
  if(activeConcept && !c.teaches.includes(activeConcept)) return false;
  if(![...c.teaches].some(t => activeCats.has(catOf[t]||'other'))) return false;
  if(query){ const hay=(c.name+' '+c.kind.join(' ')+' '+c.teaches.join(' ')+' '+c.lineage).toLowerCase(); if(!hay.includes(query)) return false; }
  return true;
}
function render(){
  const list = carts.filter(visible);
  document.getElementById('cartsHead').textContent = 'Carts — ' + list.length + (activeConcept ? ' · '+activeConcept : '');
  document.getElementById('empty').hidden = list.length > 0;
  cartsEl.innerHTML = list.map(c => {
    const tags = c.teaches.map(t => { const cc = col(t); return '<span class="tag-pill" style="border-color:'+cc+';color:'+cc+'">'+escp(t)+'</span>'; }).join('');
    const nmCls = embedded ? 'nm link' : 'nm';
    return '<div class="cart"><div class="top"><span class="'+nmCls+'" data-cart="'+escp(c.name)+'">'+hl(escp(c.name))+'</span>'+
      '<span class="kind">'+escp(c.kind.join(' / '))+'</span></div>'+
      '<div class="tags">'+tags+'</div>'+
      (c.lineage ? '<div class="lineage">'+hl(escp(c.lineage))+'</div>' : '')+'</div>';
  }).join('');
}
render();
</script>`;

const OUT = path.join(ROOT, 'docs/cart-compendium.html');
// --check: is the committed page in sync with the tag data? (deterministic render,
// no timestamps, so a byte-diff is exact). Used by cart-status.js + the pre-commit hook.
if (process.argv.includes('--check')) {
  let cur = '';
  try { cur = fs.readFileSync(OUT, 'utf8'); } catch {}
  if (cur !== html) {
    console.error('STALE: docs/cart-compendium.html is out of date — run: node tools/build-compendium.js');
    process.exit(1);
  }
  console.log('docs/cart-compendium.html up to date — ' + carts.length + ' carts');
  process.exit(0);
}
fs.writeFileSync(OUT, html);
console.log('wrote docs/cart-compendium.html —', carts.length, 'carts,', conceptList.length, 'concepts');
