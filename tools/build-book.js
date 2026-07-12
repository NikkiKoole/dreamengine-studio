#!/usr/bin/env node
// build-book.js — generate docs/learn-you-a-dreamengine.html, the illustrated
// "Learn You a Haskell"-style field guide to the studio.h API.
//
// The whole point: every illustration is REAL console output. Each is a tiny cart
// in tools/carts/book/, rendered here through the deterministic harness —
// stills via play.js --dump, animations via make-gif --format gif — so the book
// can never drift from the engine it teaches. Re-run this after an engine change
// and the pictures re-bake themselves.
//
//   node tools/build-book.js          → render assets + write the page
//
// Assets land in docs/book-assets/ (referenced relatively, like history.html's clips).
// Add a chapter: drop a cart in tools/carts/book/, add an entry to ILLUS + prose below.

const fs   = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const ROOT    = path.join(__dirname, '..');
const ASSETS  = path.join(ROOT, 'docs', 'book-assets');
const TMP      = path.join(ROOT, 'build', '.book-tmp');
const OUT      = path.join(ROOT, 'docs', 'learn-you-a-dreamengine.html');

// the harness writes per-cart build artifacts under build/<name>… — for a
// book/<name> cart that means a build/book/ subdir must exist first.
for (const d of [ASSETS, TMP, path.join(ROOT, 'build', 'book'),
                 path.join(ROOT, 'build', '.gif', 'book')]) {
  fs.mkdirSync(d, { recursive: true });
}

// ── the illustrations, each a cart under tools/carts/book/ ────────────────
const ILLUS = {
  sun:     { kind: 'still' },
  palette: { kind: 'still' },
  gremlin: { kind: 'still' },
  edge:    { kind: 'still' },
  bounce:  { kind: 'gif', frames: 96,  fps: 20 },
  scene:   { kind: 'still' },
  wave:    { kind: 'gif', frames: 120, fps: 20 },
  catch:   { kind: 'gif', frames: 150, fps: 20 },
  sprites: { kind: 'gif', frames: 120, fps: 20 },
  song:    { kind: 'video', frames: 144, fps: 30 },   // a webm — it has SOUND
  collide: { kind: 'gif', frames: 120, fps: 20 },
  swarm:   { kind: 'gif', frames: 90,  fps: 20 },
  juice:   { kind: 'gif', frames: 96,  fps: 20 },
  world:   { kind: 'gif', frames: 160, fps: 20 },
  states:  { kind: 'gif', frames: 170, fps: 20 },
  shooter: { kind: 'video', frames: 180, fps: 30 },   // a webm — pew + boom
  phone:   { kind: 'gif', frames: 96,  fps: 20 },
  ship:    { kind: 'gif', frames: 90,  fps: 20 },
  // mood creatures — one per chapter, drawn by the console like everything else
  greeter:  { kind: 'still' },
  leaper:   { kind: 'still' },
  juggler:  { kind: 'still' },
  dreamer:  { kind: 'still' },
  champion: { kind: 'still' },
  painter:  { kind: 'still' },
  crooner:  { kind: 'still' },
  bumper:   { kind: 'still' },
  crowd:    { kind: 'still' },
  spark:    { kind: 'still' },
  scout:    { kind: 'still' },
  keeper:   { kind: 'still' },
  ace:      { kind: 'still' },
  gamer:    { kind: 'still' },
  farewell: { kind: 'still' },
};

function render(id, spec) {
  const run = (cmd, args) =>
    execFileSync('node', [path.join('tools', cmd), ...args], { cwd: ROOT, stdio: 'pipe' });
  if (spec.kind === 'gif') {
    const out = path.join(ASSETS, `${id}.gif`);
    run('make-gif.js', [`book/${id}`, '--format', 'gif',
      '--frames', String(spec.frames), '--fps', String(spec.fps), '--out', out]);
    return `book-assets/${id}.gif`;
  }
  if (spec.kind === 'video') {                    // webm carries the cart's real audio
    const out = path.join(ASSETS, `${id}.webm`);
    run('make-gif.js', [`book/${id}`, '--format', 'webm',
      '--frames', String(spec.frames), '--fps', String(spec.fps), '--out', out]);
    return `book-assets/${id}.webm`;
  }
  const dir = path.join(TMP, id);
  fs.mkdirSync(dir, { recursive: true });
  run('play.js', [`book/${id}`, 'run', '--headless', '--frames', '1', '--dump', dir]);
  fs.copyFileSync(path.join(dir, 'frame_00000.png'), path.join(ASSETS, `${id}.png`));
  return `book-assets/${id}.png`;
}

const SRC = {};
for (const [id, spec] of Object.entries(ILLUS)) {
  process.stdout.write(`rendering ${id} (${spec.kind})… `);
  SRC[id] = render(id, spec);
  console.log('ok');
}

// ── little HTML builders ──────────────────────────────────────────────────
const screen = (id, alt, cap) => {
  const src = SRC[id];
  const media = src.endsWith('.webm')
    ? `<video src="${src}" controls loop playsinline preload="metadata" aria-label="${alt}"></video>`
    : `<img src="${src}" alt="${alt}">`;
  return `
  <figure class="screen">
    <div class="bezel"><div class="glass">${media}</div></div>
    <figcaption>${cap}</figcaption>
  </figure>`;
};

const aside = (cls, lead, body) => `
  <aside class="${cls}">
    <img class="g" src="${SRC.gremlin}" alt="The green pixel gremlin, narrating">
    <div class="say"><span class="lead">${lead}</span>${body}</div>
  </aside>`;

const chapHead = (id, no, kicker, title) => `
  <div class="chap-head" id="${id}">
    <span class="chap-no">Chapter ${no}</span>
    <span class="chap-kick">${kicker}</span>
    <h2 class="chap-title">${title}</h2>
  </div>`;

// a mood creature — decorative, uncaptioned, no CRT bezel (this is atmosphere, not
// an honest-output "screen"). Still drawn by the console, of course.
const creature = (id, alt) => `
  <div class="creature"><img src="${SRC[id]}" alt="${alt}"></div>`;

// a real, verbatim compiler error, shown the way it actually appears
const term = (body) => `<pre class="term">${body}</pre>`;

const html = `<title>Learn You a dreamengine for Great Good!</title>
<style>
  :root{
    --ink:#fff1e8; --ink-soft:#c2c3c7; --ink-faint:#8b8f9c;
    --ground:#141d3a; --panel:#10162e;
    --yellow:#ffec27; --green:#00e436; --red:#ff004d; --blue:#29adff;
    --line:#2b3a63;
    --serif:Georgia,"Iowan Old Style","Times New Roman",serif;
    --mono:ui-monospace,"Cascadia Code",Menlo,Consolas,monospace;
    --comic:"Comic Sans MS","Comic Neue","Chalkboard SE",cursive;
    --col:680px;
  }
  *{box-sizing:border-box}
  html{color-scheme:dark}
  body{margin:0;background:
      radial-gradient(120% 80% at 50% -10%, #23336a 0%, var(--ground) 55%, #0d1329 100%);
      background-attachment:fixed;
      color:var(--ink);font-family:var(--serif);
      font-size:19px;line-height:1.72;-webkit-font-smoothing:antialiased;}
  .wrap{max-width:var(--col);margin:0 auto;padding:0 24px 60px;}
  header.hero{max-width:860px;margin:0 auto;padding:76px 24px 24px;text-align:center;}
  .eyebrow{font-family:var(--mono);font-size:13px;letter-spacing:.32em;text-transform:uppercase;
    color:var(--yellow);margin:0 0 22px;}
  .booktitle{font-family:var(--comic);font-weight:700;line-height:1.05;margin:0;
    font-size:clamp(38px,7vw,68px);text-wrap:balance;color:var(--ink);text-shadow:4px 4px 0 var(--red);}
  .booktitle .b{color:var(--yellow);text-shadow:4px 4px 0 #1d2b53;}
  .sub{font-family:var(--serif);font-style:italic;color:var(--ink-soft);font-size:19px;margin-top:26px;}
  .toc{max-width:var(--col);margin:26px auto 0;padding:0 24px;display:grid;gap:8px;}
  .toc a{display:flex;gap:14px;align-items:baseline;text-decoration:none;font-family:var(--mono);
    font-size:14px;color:var(--ink-soft);border:1px solid var(--line);border-radius:9px;
    padding:11px 15px;background:var(--panel);}
  .toc a:hover{border-color:var(--yellow);color:var(--ink);}
  .toc .num{color:var(--yellow);}
  .chap-head{margin:2.8em 0 1.2em;padding-top:2.4em;border-top:1px solid var(--line);}
  .chap-head:first-of-type{border-top:0;padding-top:.6em;}
  .chap-no{font-family:var(--mono);font-size:12px;letter-spacing:.28em;text-transform:uppercase;color:var(--yellow);}
  .chap-kick{font-family:var(--mono);font-size:12px;letter-spacing:.2em;text-transform:uppercase;color:var(--ink-faint);margin-left:12px;}
  .chap-title{font-family:var(--comic);font-size:clamp(27px,5vw,38px);line-height:1.1;
    text-wrap:balance;margin:.35em 0 0;color:var(--green);}
  p{margin:0 0 1.15em;}
  p.lead::first-letter{font-family:var(--comic);font-size:3.2em;line-height:.8;
    float:left;padding:6px 12px 0 0;color:var(--yellow);}
  .creature{display:flex;justify-content:center;margin:.4em 0 1.4em;}
  .creature img{width:230px;max-width:70%;image-rendering:pixelated;border-radius:12px;
    border:1px solid var(--line);box-shadow:0 10px 26px -14px #000;}
  pre.term{border-left:3px solid var(--red);color:var(--ink-soft);white-space:pre;}
  pre.term .e{color:var(--red);} pre.term .p{color:var(--green);}
  strong{color:var(--yellow);font-weight:700;}
  em{color:var(--ink);font-style:italic;}
  code{font-family:var(--mono);font-size:.84em;background:var(--panel);border:1px solid var(--line);
    padding:1px 6px;border-radius:5px;color:var(--green);}
  h3{font-family:var(--comic);font-size:22px;color:var(--ink);margin:2em 0 .2em;}
  figure.screen{margin:2.2em 0;}
  .bezel{background:linear-gradient(#3a3f52,#23273a);border-radius:16px;padding:16px 16px 14px;
    box-shadow:0 18px 40px -18px #000, inset 0 1px 0 #52586e;position:relative;}
  .bezel::after{content:"dreamengine";position:absolute;bottom:5px;left:0;right:0;text-align:center;
    font-family:var(--mono);font-size:9px;letter-spacing:.3em;color:#6a7088;text-transform:uppercase;}
  .glass{position:relative;border-radius:6px;overflow:hidden;background:#000;
    box-shadow:inset 0 0 60px rgba(0,0,0,.6);aspect-ratio:320/200;}
  .glass img,.glass video{width:100%;height:100%;display:block;image-rendering:pixelated;}
  .glass video{object-fit:fill;background:#000;}
  figcaption{font-family:var(--mono);font-size:13px;color:var(--ink-faint);text-align:center;
    margin-top:12px;line-height:1.5;}
  figcaption b{color:var(--ink-soft);font-weight:400;}
  pre{font-family:var(--mono);font-size:14.5px;line-height:1.6;background:var(--panel);
    border:1px solid var(--line);border-left:3px solid var(--yellow);border-radius:8px;
    padding:16px 18px;overflow-x:auto;margin:1.6em 0;color:var(--ink-soft);}
  pre .c{color:#6a7088;} pre .f{color:var(--yellow);} pre .k{color:var(--red);}
  pre .n{color:var(--blue);} pre .s{color:var(--green);}
  aside{display:grid;grid-template-columns:88px 1fr;gap:16px;align-items:start;
    background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:16px 18px;margin:2em 0;}
  aside.warn{border-color:#5a2740;border-left:3px solid var(--red);}
  aside .g{width:88px;image-rendering:pixelated;border-radius:8px;align-self:center;}
  aside .say{font-family:var(--comic);font-size:16px;line-height:1.5;color:var(--ink);}
  aside .say .lead{display:block;font-size:12px;letter-spacing:.14em;text-transform:uppercase;
    font-family:var(--mono);margin-bottom:4px;color:var(--green);}
  aside.warn .say .lead{color:var(--red);}
  aside p{margin:0;} aside p+p{margin-top:.6em;}
  footer{max-width:var(--col);margin:60px auto 0;padding:26px 24px 0;border-top:1px solid var(--line);
    font-family:var(--mono);font-size:13px;color:var(--ink-faint);text-align:center;line-height:1.7;}
  footer b{color:var(--yellow);font-weight:400;}
  @media (max-width:520px){ aside{grid-template-columns:60px 1fr} aside .g{width:60px} body{font-size:17px} }
  @media (prefers-reduced-motion:reduce){*{animation:none!important}}
</style>

<header class="hero">
  <p class="eyebrow">A field guide to a very small machine</p>
  <p class="booktitle">Learn You a <span class="b">dreamengine</span><br>for Great Good!</p>
  <p class="sub">In which we fill a tiny screen with coloured dots, and call it a game.</p>
</header>

<nav class="toc">
  <a href="#ch1"><span class="num">1</span><span>So, You Want To Make A Little World</span></a>
  <a href="#ch2"><span class="num">2</span><span>Keeping The Sun On The Screen</span></a>
  <a href="#ch3"><span class="num">3</span><span>A Small Cast Of Shapes</span></a>
  <a href="#ch4"><span class="num">4</span><span>A World That Moves On Its Own</span></a>
  <a href="#ch5"><span class="num">5</span><span>Your First Actual Game</span></a>
  <a href="#ch6"><span class="num">6</span><span>Drawing With Pictures, Not Just Shapes</span></a>
  <a href="#ch7"><span class="num">7</span><span>Making A Racket</span></a>
  <a href="#ch8"><span class="num">8</span><span>Do Two Things Touch?</span></a>
  <a href="#ch9"><span class="num">9</span><span>A Cast of Thousands</span></a>
  <a href="#ch10"><span class="num">10</span><span>Juice: Making It Feel Good</span></a>
  <a href="#ch11"><span class="num">11</span><span>Worlds Bigger Than the Screen</span></a>
  <a href="#ch12"><span class="num">12</span><span>Remembering Things</span></a>
  <a href="#ch13"><span class="num">13</span><span>A Proper Little Game</span></a>
  <a href="#ch14"><span class="num">14</span><span>Make It Work on a Phone</span></a>
  <a href="#ch15"><span class="num">15</span><span>Ship It</span></a>
</nav>

<main class="wrap">

  ${chapHead('ch1', '1', 'the one job', 'So, You Want To Make A Little World')}
  ${creature('greeter', 'A little green blob waving hello')}

  <p class="lead">Hello! Come in, sit down, mind the pixels &mdash; they get everywhere. You&rsquo;re here because you want to make a game, and everybody told you that means C, and C means pointers, and pointers mean crying. Forget all that for now. In here we have exactly <strong>one job</strong>: fill a tiny screen &mdash; 320 by 200, smaller than a postage stamp with delusions of grandeur &mdash; with coloured dots, sixty times a second. That&rsquo;s a game. That&rsquo;s the whole racket.</p>

  <h3>The one function you cannot escape</h3>
  <p>You fill the screen by writing a function called <code>draw</code>. The console calls it for you, over and over, forever, until someone closes the window or the heat death of the universe, whichever comes first. Here is a complete, real, honest-to-goodness dreamengine program:</p>

  <pre><span class="c">#include "studio.h"</span>

<span class="k">void</span> <span class="f">draw</span>(<span class="k">void</span>) {
    <span class="f">cls</span>(<span class="s">CLR_DARK_BLUE</span>);            <span class="c">// paint the whole sky one colour</span>
    <span class="f">circfill</span>(<span class="n">160</span>, <span class="n">100</span>, <span class="n">20</span>, <span class="s">CLR_YELLOW</span>); <span class="c">// a sun, right in the middle</span>
}</pre>

  <p>Run it, and the machine does what you said. Not a mock-up of what it might do &mdash; the actual thing:</p>

  ${screen('sun', 'A yellow filled circle centred on a dark blue screen',
    '<b>the output of the four lines above &mdash; nothing else.</b><br>every picture in this book was drawn by the console it is teaching you.')}

  <p><code>cls</code> means <em>clear screen</em>. It&rsquo;s the first thing you do every frame, like wiping the whiteboard before you draw the next silly cat. Then <code>circfill</code> stamps a filled circle: the two numbers say <em>where</em> (160 across, 100 down &mdash; smack in the middle), the third says <em>how big</em>, and the last says <em>what colour</em>. That&rsquo;s the shape of nearly every drawing call you&rsquo;ll ever write: where, how big, what colour.</p>

  ${aside('', 'Psst. A secret.',
    '<p>If you <em>forget</em> the <code>cls</code>, yesterday&rsquo;s cats don&rsquo;t get wiped &mdash; they pile up, frame on frame, smeared across the screen.</p><p>Sometimes cats piling up is <em>exactly</em> the effect you want. Then it isn&rsquo;t a bug. Then it&rsquo;s called being clever. (Hold that thought until Chapter 2.)</p>')}

  <h3>Thirty-two crayons, and not one more</h3>
  <p>You may have noticed <code>CLR_YELLOW</code> up there and wondered how many of these there are. Not sixteen million. Not a colour picker with a little eyedropper. There are <strong>thirty-two</strong>. That&rsquo;s the whole crayon box, numbered 0 to 31, and yellow happens to be number 10.</p>

  ${screen('palette', 'A grid of 32 numbered colour swatches',
    '<b>the entire palette, swatch by swatch.</b><br>you can write <b>CLR_YELLOW</b> or just <b>10</b> &mdash; the machine doesn&rsquo;t mind which.')}

  <p>Fewer crayons is a <em>gift</em>, not a limitation. It&rsquo;s the reason everything you make will look like it belongs together &mdash; like it came from one world &mdash; whether or not you happen to have any taste. The console is quietly doing your art direction for you. Say thank you.</p>

  ${aside('warn', 'Here be a small dragon',
    '<p>The greys in this box are spelled the British way: <code>CLR_DARK_GREY</code>, with an <em>E</em>. Type <code>GRAY</code> and the compiler will look at you like you&rsquo;ve tracked mud through its house. Everyone does this once. Now you&rsquo;ve done it here, for free.</p>')}

  <h3>What the machine says when you get it wrong</h3>
  <p>Let&rsquo;s actually make that mistake on purpose, because you <em>will</em> make it by accident, and the first time the compiler shouts at you it&rsquo;s good to know it&rsquo;s only shouting one small, specific thing. Type <code>cls(CLR_GRAY)</code>, hit run, and here is the entire tantrum, word for word:</p>

  ${term('<span class="e">error:</span> use of undeclared identifier <span class="p">\'CLR_GRAY\'</span>\n    cls(CLR_GRAY);\n        ^~~~~~~~')}

  <p>That&rsquo;s it. That&rsquo;s the whole scary red wall of text everyone dreads. Read it plainly: <em>&ldquo;you used a name, <code>CLR_GRAY</code>, that I&rsquo;ve never heard of&rdquo;</em> (because it&rsquo;s spelled <code>GREY</code>). The little <code>^~~~~</code> is the machine politely pointing its finger at the <em>exact</em> spot it got confused. A compiler error is almost never a disaster &mdash; it&rsquo;s a colleague telling you which word it choked on. Fix that word, run again.</p>

  <h3>A painting that flinches when you poke it</h3>
  <p>A sun just sitting there is a <em>painting</em>, not a game. A game is a painting that flinches when you poke it. So let&rsquo;s give ourselves something to poke with, and let the sun slide when we press a button:</p>

  <pre><span class="c">#include "studio.h"</span>

<span class="k">int</span> x = <span class="n">160</span>, y = <span class="n">100</span>;

<span class="k">void</span> <span class="f">update</span>(<span class="k">void</span>) {           <span class="c">// the thinking half — runs before every draw</span>
    <span class="k">if</span> (<span class="f">btn</span>(<span class="n">0</span>, <span class="s">BTN_LEFT</span>))  x--;  <span class="c">// player 0 holding left? nudge left</span>
    <span class="k">if</span> (<span class="f">btn</span>(<span class="n">0</span>, <span class="s">BTN_RIGHT</span>)) x++;
}

<span class="k">void</span> <span class="f">draw</span>(<span class="k">void</span>) {             <span class="c">// the painting half — runs every frame</span>
    <span class="f">cls</span>(<span class="s">CLR_DARK_BLUE</span>);
    <span class="f">circfill</span>(x, y, <span class="n">20</span>, <span class="s">CLR_YELLOW</span>);
}</pre>

  <p>And that&rsquo;s the whole secret, really. <code>draw</code> is the hand that paints; <code>update</code> is the little brain that decides <em>what</em> to paint next. <code>btn</code> asks a simple question &mdash; <em>is this button held down right now?</em> &mdash; and answers <em>yes</em> or <em>no</em>. Everything else &mdash; sprites, sound, whole galaxies &mdash; is just more of these three ideas, stacked up until they look impressive.</p>


  ${chapHead('ch2', '2', 'motion & edges', 'Keeping The Sun On The Screen')}
  ${creature('leaper', 'The blob leaping across a gap at the edge of the world')}

  <p>We left our sun able to walk. There is a problem with letting things walk: they walk <em>off</em>. Hold <em>right</em> long enough and the sun strolls clean past the edge of the world and is never seen again.</p>

  ${screen('edge', 'A sun mostly off the right edge of the screen, past a red dashed line',
    '<b>x kept counting up past 320.</b><br>the screen ends; the number did not.')}

  <p>The screen is <strong>320</strong> pixels wide and <strong>200</strong> tall, and the machine hands you those two numbers as <code>SCREEN_W</code> and <code>SCREEN_H</code> so you never have to remember them. To keep the sun home, we just refuse to let <code>x</code> wander out of bounds. This is called <em>clamping</em>, which is a big word for &ldquo;nope, that&rsquo;s far enough&rdquo;:</p>

  <pre><span class="k">void</span> <span class="f">update</span>(<span class="k">void</span>) {
    <span class="k">if</span> (<span class="f">btn</span>(<span class="n">0</span>, <span class="s">BTN_LEFT</span>))  x--;
    <span class="k">if</span> (<span class="f">btn</span>(<span class="n">0</span>, <span class="s">BTN_RIGHT</span>)) x++;

    <span class="k">if</span> (x &lt; <span class="n">20</span>)              x = <span class="n">20</span>;            <span class="c">// don't cross the left wall</span>
    <span class="k">if</span> (x &gt; <span class="s">SCREEN_W</span> - <span class="n">20</span>)  x = <span class="s">SCREEN_W</span> - <span class="n">20</span>; <span class="c">// or the right one (20 = the radius)</span>
}</pre>

  <p>Now the sun can shove itself against either wall and simply <em>stops</em>, politely, like a good sun. Notice we clamp at <code>20</code> from each edge &mdash; that&rsquo;s the sun&rsquo;s radius, so its <em>rim</em> kisses the wall instead of its centre. Little details like that are the whole difference between &ldquo;works&rdquo; and &ldquo;feels right.&rdquo;</p>

  ${aside('warn', 'The famous off-by-one',
    '<p>The screen is 320 wide, but the pixels are numbered <em>0 to 319</em> &mdash; there is no pixel 320, the same way a dozen eggs are numbered 0 through 11 if you&rsquo;re a programmer and 1 through 12 if you&rsquo;re a normal person. Nearly every &ldquo;why is there a one-pixel gap&rdquo; bug in history is this, wearing a disguise.</p>')}

  <h3>Now make it bounce</h3>
  <p>Standing still at a wall is fine, but remember the secret from Chapter 1 &mdash; skip the <code>cls</code> and old frames pile up? Let&rsquo;s <em>spend</em> it. Give the ball a falling <em>speed</em> that gravity tugs on, flip it when it smacks the floor, and instead of wiping the screen, only <code>fade</code> it a touch each frame. The console paints its own long-exposure photograph, live:</p>

  ${screen('bounce', 'A yellow ball bouncing across a dark screen, leaving a glowing comet trail',
    '<b>one ball, one <code>fade</code> per frame.</b><br>this is animated &mdash; the trail is just old frames not-quite-erased.')}

  <p>That whole glowing streak is a <em>single</em> circle, drawn once per frame at a spot that moved a little each time, over a screen we dim rather than clear. Motion, it turns out, is just arithmetic you do sixty times a second. Which raises a fair question: do we even <em>need</em> a button? Could the world move all by itself? (Yes. Chapter 4. Patience.)</p>


  ${chapHead('ch3', '3', 'the drawing kit', 'A Small Cast Of Shapes')}
  ${creature('juggler', 'The blob juggling a triangle, a square and a circle')}

  <p>So far our whole cast has been one circle in a hat. Time to meet the rest of the troupe. The console gives you a little kit of shapes, and &mdash; this is the important part &mdash; <em>a picture is just those shapes, stacked</em>. No art program, no imported files. Here is a whole scene, and every single thing in it is one plain shape call:</p>

  ${screen('scene', 'A pixel scene: a house with a red roof, a tree, and a sun over a green hill',
    '<b>sun, hill, house, roof, door, window, tree, label.</b><br>eight shape calls. no sprites were harmed.')}

  <pre><span class="f">cls</span>(<span class="s">CLR_DARK_BLUE</span>);
<span class="f">circfill</span>(<span class="n">268</span>,<span class="n">44</span>,<span class="n">22</span>, <span class="s">CLR_YELLOW</span>);            <span class="c">// sun</span>
<span class="f">ovalfill</span>(<span class="n">160</span>,<span class="n">230</span>,<span class="n">220</span>,<span class="n">90</span>, <span class="s">CLR_DARK_GREEN</span>);     <span class="c">// a rolling hill (a squashed circle)</span>
<span class="f">rectfill</span>(<span class="n">96</span>,<span class="n">118</span>,<span class="n">72</span>,<span class="n">56</span>, <span class="s">CLR_LIGHT_GREY</span>);       <span class="c">// walls</span>
<span class="f">trifill</span>(<span class="n">90</span>,<span class="n">118</span>, <span class="n">174</span>,<span class="n">118</span>, <span class="n">132</span>,<span class="n">86</span>, <span class="s">CLR_RED</span>); <span class="c">// roof (three corners)</span>
<span class="f">rectfill</span>(<span class="n">120</span>,<span class="n">142</span>,<span class="n">22</span>,<span class="n">32</span>, <span class="s">CLR_BROWN</span>);          <span class="c">// door</span>
<span class="f">line</span>(<span class="n">210</span>,<span class="n">174</span>, <span class="n">210</span>,<span class="n">140</span>, <span class="s">CLR_BROWN</span>);          <span class="c">// a tree trunk</span>
<span class="f">circfill</span>(<span class="n">210</span>,<span class="n">130</span>,<span class="n">16</span>, <span class="s">CLR_GREEN</span>);             <span class="c">// leaves</span>
<span class="f">print</span>(<span class="s">"home."</span>, <span class="n">132</span>,<span class="n">182</span>, <span class="s">CLR_WHITE</span>);          <span class="c">// a label</span></pre>

  <p>You&rsquo;ve now met most of the family: <code>rectfill</code> and <code>rect</code> (solid box / just the outline), <code>ovalfill</code> (a squashed circle &mdash; hills, shadows, sleepy eyes), <code>trifill</code> (three corners, one triangle), <code>line</code>, and <code>print</code> for words. They all sing the same tune you already know: <em>where, how big, what colour</em>.</p>

  ${aside('', 'Order is everything',
    '<p>The console is a lazy painter: it slaps down each shape right over whatever was already there. So you paint <em>back to front</em> &mdash; sky, then hill, then house, then the label on top.</p><p>Draw the roof <em>before</em> the walls and the walls sit on top of it, and you&rsquo;ll spend twenty confused minutes wondering who stole your roof. (It was you. It is always you. It was me too.)</p>')}


  ${chapHead('ch4', '4', 'time', 'A World That Moves On Its Own')}
  ${creature('dreamer', 'The blob dozing, swaying on a gentle sine wave under a moon')}

  <p>Every moving thing so far has needed a finger on a button. But the machine has its own quiet heartbeat, ticking whether anyone is watching or not. It&rsquo;s called <code>frame</code>, and it just counts: 1, 2, 3&hellip; sixty times a second, forever. Feed that ever-climbing number into a <em>wave</em> &mdash; <code>sin_deg</code>, sine measured in friendly degrees &mdash; and the whole thing rocks gently along on its own:</p>

  ${screen('wave', 'A row of coloured dots riding a sine wave that drifts sideways',
    '<b>each dot&rsquo;s height is sin_deg() of its position + frame().</b><br>nobody is touching anything. it just goes.')}

  <pre><span class="k">void</span> <span class="f">draw</span>(<span class="k">void</span>) {
    <span class="f">cls</span>(<span class="s">CLR_DARK_BLUE</span>);
    <span class="k">int</span> y = <span class="n">100</span> + <span class="f">sin_deg</span>(<span class="f">frame</span>() * <span class="n">2</span>) * <span class="n">40</span>;  <span class="c">// bob 40px up and down over time</span>
    <span class="f">circfill</span>(<span class="n">160</span>, y, <span class="n">20</span>, <span class="s">CLR_YELLOW</span>);
}</pre>

  <p>No button, no <code>update</code>, and the sun bobs like a cork on water &mdash; because <code>frame()</code> keeps climbing and the sine keeps folding it back into a gentle up-and-down. Swap <code>sin_deg</code> for <code>cos_deg</code> on the <em>x</em> too and it swims in a circle. This one little trick &mdash; a clock, poured through a wave &mdash; is secretly behind half the juice in every game you&rsquo;ve ever loved: the coin that spins, the text that throbs, the enemy that hovers, the grass that sways.</p>

  ${aside('', 'Degrees, not the other thing',
    '<p>We used <code>sin_deg</code>, which thinks in <em>degrees</em> &mdash; 0 to 360, the way a protractor does. There&rsquo;s a version that thinks in radians too, but it involves &pi; and a small amount of weeping. Start with degrees. Your future self, drawing a clock face at 90&deg; and having it Just Work, will thank you.</p>')}

  <p>You now have four ideas: fill the screen, poke it, keep things inside it, and let them move on their own. That&rsquo;s&hellip; actually a whole game, if you stack them. Shall we? Let&rsquo;s go get our great good.</p>


  ${chapHead('ch5', '5', 'the payoff', 'Your First Actual Game')}
  ${creature('champion', 'The blob holding up a basket with a caught cherry, confetti everywhere')}

  <p>Here it is &mdash; a real, whole, playable game, and there is not one single new idea in it. It&rsquo;s just the four things you already know, holding hands. A basket you slide left and right; cherries that fall; catch them, score goes up. Watch (this one is playing itself &mdash; the console driving its own basket, because we asked it to):</p>

  ${screen('catch', 'A basket catching falling cherries, score counting up',
    '<b>caught: a real game.</b><br>cls + shapes + btn + clamping + falling &mdash; chapters one through four, at once.')}

  <p>Look at what&rsquo;s actually in there. The basket <strong>clamps</strong> to the screen (Chapter 2). The cherry <strong>falls</strong> a little further each frame (Chapter 4). Everything is drawn from <strong>shapes</strong> (Chapter 3) onto a screen we <strong>clear</strong> every frame (Chapter 1). The only genuinely new line is the one question a game asks over and over: <em>did the thing that&rsquo;s falling land where the thing I&rsquo;m holding is?</em></p>

  <pre><span class="k">void</span> <span class="f">update</span>(<span class="k">void</span>) {
    <span class="k">if</span> (<span class="f">btn</span>(<span class="n">0</span>, <span class="s">BTN_LEFT</span>))       bx -= <span class="n">4</span>;   <span class="c">// slide the basket…</span>
    <span class="k">else if</span> (<span class="f">btn</span>(<span class="n">0</span>, <span class="s">BTN_RIGHT</span>)) bx += <span class="n">4</span>;
    <span class="k">else</span>                          bx += (fx - bx) * <span class="n">0.1</span>; <span class="c">// …or let it play itself</span>

    <span class="k">if</span> (bx &lt; half)              bx = half;                <span class="c">// clamp — chapter 2</span>
    <span class="k">if</span> (bx &gt; <span class="s">SCREEN_W</span> - half)  bx = <span class="s">SCREEN_W</span> - half;

    fy += fvy;                                    <span class="c">// the cherry falls — chapter 4</span>
    <span class="k">if</span> (fy &gt;= basket_y) {                          <span class="c">// it reached the basket line</span>
        <span class="k">if</span> (fx &gt; bx - half &amp;&amp; fx &lt; bx + half) score++;  <span class="c">// THE question</span>
        <span class="f">spawn</span>();                                  <span class="c">// send down a fresh one</span>
    }
}</pre>

  ${aside('', 'The oldest trick in the arcade',
    '<p>See that <code>else</code> &mdash; the basket easing toward the cherry when nobody&rsquo;s pressing anything? That&rsquo;s <em>attract mode</em>, the demo that plays on its own in every arcade cabinet to lure you over.</p><p>You got it for free: it&rsquo;s just Chapter 4&rsquo;s &ldquo;move on its own&rdquo; pointed at the cherry instead of a sine wave. Old ideas wearing new hats. It&rsquo;s hats all the way down.</p>')}

  <p>And that&rsquo;s the trick nobody tells you: there is no secret fifth thing. Pong, Mario, a flight simulator, whatever galaxy you&rsquo;re dreaming of &mdash; it is all this. Fill a screen, poke it, keep it in bounds, let it move, and ask small true/false questions sixty times a second. You have, right now, everything you need to make something nobody has ever played before.</p>

  <p>That&rsquo;s the whole engine, honestly. What&rsquo;s left is toys &mdash; nicer ways to say the same things. There are a couple still in the box; let&rsquo;s open one.</p>


  ${chapHead('ch6', '6', 'pictures with names', 'Drawing With Pictures, Not Just Shapes')}
  ${creature('painter', 'A little green blob painting a tiny pixel picture on a canvas')}

  <p>Circles and rectangles are grand, but sooner or later you want a proper little <em>guy</em> &mdash; a hero, a coin, a heart &mdash; without stacking fifteen shape calls every frame. So you draw the picture <strong>once</strong>, the console remembers it, and from then on you stamp it with a single call. These remembered pictures are called <strong>sprites</strong>, and each one gets a <em>number</em>.</p>

  ${screen('sprites', 'A numbered sprite sheet on top; below, a hero walking past coins and bushes',
    '<b>top: five sprites, each with a number. bottom: a scene made of those numbers.</b><br>the hero is really just sprite 0 and sprite 1, swapped fast.')}

  <p>You draw the pictures in a little side file, one per numbered slot &mdash; here, code that paints a 16&times;16 coin into slot 2:</p>

  <pre><span class="c">// sprites.cart.js — drawn once, remembered forever</span>
sprites: {
  <span class="n">2</span>: <span class="f">coin</span>(),   <span class="c">// a yellow disc with a shine → slot 2</span>
}</pre>

  <p>And then, in the game itself, you stamp any sprite by its number with <code>spr</code> &mdash; same old tune, <em>which picture, where</em>:</p>

  <pre><span class="f">spr</span>(<span class="n">2</span>, <span class="n">108</span>, <span class="n">92</span>);   <span class="c">// draw sprite 2 (the coin) at (108, 92)</span>
<span class="f">spr</span>(<span class="n">4</span>, <span class="n">34</span>, <span class="n">138</span>);   <span class="c">// draw sprite 4 (a bush)</span></pre>

  <p>The best part is what animation costs: <em>nothing new</em>. Draw the hero mid-stride in slot 0 and mid-stride-the-other-way in slot 1, then let Chapter 4&rsquo;s clock pick between them &mdash; <code>spr(frame()/6 % 2, x, y)</code> &mdash; and he walks. Two pictures, swapped fast, is the oldest cartoon trick there is.</p>

  ${aside('warn', 'What&rsquo;s the see-through bit?',
    '<p>Sprites are little squares, but your hero isn&rsquo;t square &mdash; so which pixels are <em>him</em> and which are empty air? You pick: <code>colorkey(0)</code> tells the console &ldquo;colour 0 means transparent, draw straight through it.&rdquo; Forget that line and your hero struts around inside an ugly black box. (It happens to everyone. Once.)</p>')}

  <p>Your worlds can move now, and be built from little guys. But they&rsquo;re still <em>silent films</em>. One toy left in the box &mdash; and this is the only page in the book you don&rsquo;t just look at. You listen to it.</p>


  ${chapHead('ch7', '7', 'sound', 'Making A Racket')}
  ${creature('crooner', 'A little green blob singing into a microphone, notes rising')}

  <p>Everything so far has been a mime. Let&rsquo;s give the machine a voice. There is one honest little function for it &mdash; <code>note</code> &mdash; and it asks almost the same three-word question everything else does: <em>which pitch, which instrument, how loud.</em></p>

  <pre><span class="f">note</span>(<span class="n">60</span>, <span class="s">INSTR_PLUCK</span>, <span class="n">5</span>);   <span class="c">// middle C, on a plucked string, medium loud</span></pre>

  <p>The pitch is a <strong>MIDI number</strong> &mdash; 60 is middle C, 62 the D above it, and so on up the piano. The instrument is the <em>voice</em> doing the singing (a pluck, a piano, a bowed cello&hellip;), and the last number, 0 to 7, is just how hard you hit it. Play one note a frame apart from the next and you have a melody. Here&rsquo;s a tiny sixteen-step sequencer doing exactly that &mdash; <strong>press play, and turn your sound on:</strong></p>

  ${screen('song', 'A colourful 16-step piano-roll sequencer, a playhead sweeping across, with audio',
    '<b>this one makes actual sound.</b><br>each bar is a note; the playhead sweeps; the lit block is the note you&rsquo;re hearing right now.')}

  <p>That&rsquo;s Chapter 4 again, wearing headphones: a clock (<code>frame()</code>) marches a playhead across sixteen steps, and each step it lands on fires one <code>note</code>. The tune is built entirely from a five-note scale (a <em>pentatonic</em>, the black keys on a piano), which has a lovely property &mdash; there is no combination of those notes that sounds actually wrong. Pick from them at random and you get something pleasant. It&rsquo;s cheating, and it&rsquo;s allowed.</p>

  ${aside('', 'Set it up once, then just play',
    '<p>A plain beep needs no setup, but a <em>nice</em> voice does &mdash; one line, once, at the start: <code>instrument(5, INSTR_PLUCK, &hellip;)</code> teaches slot 5 how to sound. After that you just <code>note(&hellip;, 5, &hellip;)</code> forever. Same shape as sprites: define the thing once, then refer to it by number.</p>')}

  <p>And that really is the whole box. Shapes to draw with, sprites to draw <em>fast</em>, a clock to move it all, buttons to poke it, a loop to hold it together, and now a voice. Those are the six words. You could stop here and make things &mdash; people have made wonderful things with less.</p>

  <p>But knowing the six words isn&rsquo;t the same as writing poetry with them. The rest of this book isn&rsquo;t new vocabulary &mdash; it&rsquo;s learning to say those words <em>well</em>: how things touch, how to have a hundred of them, how to make a hit <em>feel</em> like a hit, how to build a world bigger than the window, and finally how to hand the whole thing to a friend. Turn the page.</p>


  ${chapHead('ch8', '8', 'collision', 'Do Two Things Touch?')}
  ${creature('bumper', 'A dizzy green blob that has walked straight into a wall, stars spinning')}

  <p>Back in Chapter 5 the basket caught a cherry, and we waved our hands at <em>how</em> &mdash; &ldquo;if the cherry is near the basket.&rdquo; That hand-wave has a name. It&rsquo;s the single most-asked question in all of games: <em>are these two things touching?</em> A bullet and an alien. A foot and a floor. A hero and a coin. Learn to answer it once and you can build almost anything.</p>

  <p>The trick is to stop thinking about your hero&rsquo;s lovely round shape and pretend, just for the maths, that everything is a plain rectangle. Two rectangles overlap when &mdash; and only when &mdash; <strong>all four</strong> of these are true at once:</p>

  ${screen('collide', 'One box sweeping through another; they turn red and show a yellow overlap patch when touching',
    '<b>the blue box sweeps through the grey one.</b><br>overlapping → both red, and the shared patch (the actual overlap) glows yellow.')}

  <pre><span class="k">bool</span> <span class="f">overlap</span>(<span class="k">int</span> ax, <span class="k">int</span> ay, <span class="k">int</span> aw, <span class="k">int</span> ah,
             <span class="k">int</span> bx, <span class="k">int</span> by, <span class="k">int</span> bw, <span class="k">int</span> bh) {
    <span class="k">return</span> ax     &lt; bx + bw   <span class="c">// A's left  is left of B's right, AND</span>
        &amp;&amp; ax + aw &gt; bx        <span class="c">// A's right is right of B's left, AND</span>
        &amp;&amp; ay     &lt; by + bh   <span class="c">// A's top   is above B's bottom, AND</span>
        &amp;&amp; ay + ah &gt; by;       <span class="c">// A's bottom is below B's top.</span>
}</pre>

  <p>Read it as four little &ldquo;are we past each other yet?&rdquo; checks &mdash; two for the left/right, two for the up/down. If even one is false, there&rsquo;s a gap on that side and they can&rsquo;t be touching. All four true, and they&rsquo;re overlapping. That&rsquo;s the whole thing. Every &ldquo;did I get hit?&rdquo; in every game you&rsquo;ve played is, underneath, these four lines.</p>

  ${aside('warn', 'The honest little lie',
    '<p>Your hero isn&rsquo;t a rectangle, so treating him as one is <em>cheating</em> &mdash; a corner of his invisible box can &ldquo;touch&rdquo; a spike his art clearly missed. Everyone cheats anyway. It&rsquo;s called a <em>hitbox</em>, and the trick is to draw it a little <em>smaller</em> than the sprite: players forgive a near-miss that should&rsquo;ve hit, but they curse a hit that looks like a miss. Generosity, quietly encoded.</p>')}

  <p>One box against one box is easy. But a real game has a hero and <em>forty</em> coins, or a ship and a swarm of bullets. For that, we need a way to hold many things at once &mdash; which is the next page.</p>


  ${chapHead('ch9', '9', 'many things', 'A Cast of Thousands')}
  ${creature('crowd', 'One big green blob surrounded by a crowd of tiny ones')}

  <p>Every star of the show so far has been a soloist &mdash; one sun, one basket, one ball. But games are crowds: forty coins, a hundred bullets, a sky full of stars. You do <em>not</em> write a hundred variables called <code>coin1</code>, <code>coin2</code>, <code>coin37</code> and weep. You write one <strong>array</strong> &mdash; a numbered row of boxes, all the same kind &mdash; and let a <code>for</code> loop do the same thing to every box in it.</p>

  ${screen('swarm', 'Sixty-four coloured dots bouncing around the screen',
    '<b>sixty-four dots, each with its own position and speed.</b><br>not sixty-four blocks of code &mdash; one loop, run sixty-four times.')}

  <pre><span class="k">float</span> x[<span class="n">64</span>], y[<span class="n">64</span>], vx[<span class="n">64</span>], vy[<span class="n">64</span>];   <span class="c">// 64 of every number</span>

<span class="k">void</span> <span class="f">update</span>(<span class="k">void</span>) {
    <span class="k">for</span> (<span class="k">int</span> i = <span class="n">0</span>; i &lt; <span class="n">64</span>; i++) {        <span class="c">// the whole cast, one loop</span>
        x[i] += vx[i];                     <span class="c">// move dot #i (chapter 4, ×64)</span>
        y[i] += vy[i];
    }
}</pre>

  <p>The <code>[64]</code> makes sixty-four of something; the <code>[i]</code> means &ldquo;the i-th one.&rdquo; The loop counts <code>i</code> from 0 to 63 and does the exact same handful of lines to each. Want a thousand instead of sixty-four? Change one number. The loop never notices. <em>This</em> is the leap from a toy to a game: not harder code, just the same code aimed at a list.</p>

  ${aside('', 'Everything is secretly this',
    '<p>Bullets, enemies, coins, raindrops, particles &mdash; all of it is an array of things and a loop. And it plugs straight into the last chapter: to see which coins the hero grabbed, loop over all of them and run <code>overlap(hero, coin[i])</code> on each. Many-things plus does-it-touch is <em>most</em> of what a game engine even does.</p>')}


  ${chapHead('ch10', '10', 'game feel', 'Juice: Making It Feel Good')}
  ${creature('spark', 'A green blob leaping with joy, sparkles all around')}

  <p>Here is the difference between a program that works and a game that feels <em>alive</em>, and it has almost nothing to do with the rules. Watch this ball land &mdash; then imagine it just&hellip; stopping, politely, with no fuss:</p>

  ${screen('juice', 'A ball bouncing; each landing squashes it, flashes a ring, kicks the screen, and flings dust',
    '<b>the same bounce as chapter 2 &mdash; but every landing means it.</b><br>squash + flash + a screen kick + a puff of dust, all fired by one event.')}

  <p>That&rsquo;s called <strong>juice</strong>, and the whole art of it is one rule: <em>every bit of feedback is tied to a specific event.</em> The event here is the landing, and the instant it happens we fire off four little things at once:</p>

  <pre><span class="k">if</span> (landed) {
    <span class="f">shake</span>(<span class="n">4</span>);        <span class="c">// kick the whole screen — a built-in, decays on its own</span>
    squash = <span class="n">6</span>;      <span class="c">// draw the ball squished for 6 frames</span>
    flash  = <span class="n">5</span>;      <span class="c">// a white ring, expanding for 5 frames</span>
    <span class="f">spawn_dust</span>();   <span class="c">// a little burst of particles (chapter 9's array-of-things!)</span>
}</pre>

  <p>None of that is new. <code>shake</code> is a gift the engine hands you. <code>squash</code> and <code>flash</code> are just little countdown timers &mdash; Chapter 4&rsquo;s clock, in miniature. The dust is Chapter 9&rsquo;s array-of-things, spat out in a fan. Juice isn&rsquo;t a new skill; it&rsquo;s the tricks you already have, all aimed at a single moment so the player <em>feels</em> it in their gut instead of reading it off the screen.</p>

  ${aside('warn', 'A little goes a long way',
    '<p>The temptation, once you can shake and flash and spray sparks, is to do it constantly. Don&rsquo;t. If <em>everything</em> shakes, nothing lands. Keep the effects short (a few frames), and tie each one to a real event &mdash; a hit, a pickup, a jump. If you removed an effect and the action didn&rsquo;t feel any less clear, it was noise. Cut it.</p>')}

  <p>Your worlds can move, hold a crowd, and hit like they mean it now. There&rsquo;s just one wall left: everything still has to fit inside one little 320&times;200 window. Next, we knock that wall down.</p>


  ${chapHead('ch11', '11', 'a bigger world', 'Worlds Bigger Than the Screen')}
  ${creature('scout', 'A green blob with binoculars, spying a world off in the distance')}

  <p>The screen is small, but your world doesn&rsquo;t have to be. The trick is to stop thinking of the screen as <em>the world</em> and start thinking of it as a <strong>window</strong> that slides around over a much bigger one. You move the window with a single call: <code>camera</code>.</p>

  ${screen('world', 'A side-scrolling world twice as wide as the screen; the camera follows a walking hero, a minimap shows the viewport',
    '<b>the world is 640 wide; the window is 320.</b><br>the camera slides to follow the hero; the yellow bar up top is the slice you can actually see.')}

  <pre><span class="k">void</span> <span class="f">draw</span>(<span class="k">void</span>) {
    <span class="k">int</span> camx = hero_x - <span class="s">SCREEN_W</span> / <span class="n">2</span>;   <span class="c">// centre the window on the hero</span>
    <span class="f">camera</span>(camx, <span class="n">0</span>);                <span class="c">// shift EVERYTHING drawn after this by -camx</span>

    <span class="f">rectfill</span>(<span class="n">0</span>, <span class="n">150</span>, <span class="n">640</span>, <span class="n">60</span>, <span class="s">CLR_DARK_GREEN</span>);  <span class="c">// draw the whole wide world…</span>
    <span class="c">// …houses, trees, the hero — all at their real world positions</span>

    <span class="f">camera</span>(<span class="n">0</span>, <span class="n">0</span>);                   <span class="c">// reset, so the score/HUD stays put on screen</span>
}</pre>

  <p>That&rsquo;s the whole idea. You draw everything at its true position in the big world, and <code>camera</code> quietly slides all of it so the part you care about lands in the window. Flip it back to <code>camera(0, 0)</code> before you draw the score, or your HUD will go scrolling off with the scenery. For worlds made of <em>tiles</em> &mdash; a proper level, laid out on a grid &mdash; there&rsquo;s a matching <code>map</code> call that paints a whole chunk at once, and it respects the camera too.</p>

  ${aside('', 'Draw the world, not the screen',
    '<p>The freeing little shift here: once the camera exists, you stop doing screen arithmetic in your head. The hero is at world-x 500; a coin is at world-x 512; you draw them <em>there</em>, and let the camera worry about where &ldquo;there&rdquo; currently falls on the glass. Your game logic gets to live in the world, not the window.</p>')}


  ${chapHead('ch12', '12', 'memory', 'Remembering Things')}
  ${creature('keeper', 'A proud green blob beside a treasure chest with a saved number floating out')}

  <p>Everything the machine knows vanishes the instant the window closes &mdash; every variable, every score, gone. Usually that&rsquo;s fine. But some things should outlive the run: the high score, which level you reached, whether the player has seen the intro. For those, two little functions reach past the end of the program:</p>

  <pre><span class="f">save_int</span>(<span class="s">"hiscore"</span>, <span class="n">500</span>);            <span class="c">// tuck a number away under a name</span>
<span class="k">int</span> best = <span class="f">load_int</span>(<span class="s">"hiscore"</span>, <span class="n">0</span>);   <span class="c">// get it back next time (0 if never saved)</span></pre>

  <p>That&rsquo;s the entire idea of saving: a named cubbyhole that survives closing the game. And it pairs with the other thing this chapter is quietly about &mdash; that a game is really just a few <strong>screens</strong> it flips between. A title, the game itself, a game-over. Here&rsquo;s one flipping through all three on its own, remembering its best:</p>

  ${screen('states', 'A title / playing / game-over screen cycling, with a persistent best score',
    '<b>title → playing → game over → title.</b><br>the "best" survives because it was saved; beat it and NEW BEST lights up.')}

  <p>You track which screen you&rsquo;re on with a single variable &mdash; call it <code>state</code> &mdash; and <code>draw</code> just asks &ldquo;which screen am I? draw that one.&rdquo; Press start on the title and <code>state</code> becomes <em>playing</em>; die and it becomes <em>game over</em>; and at that moment you compare your score to the saved best and, if you beat it, <code>save_int</code> the new one. Every arcade machine you&rsquo;ve ever fed a coin is, underneath, this little three-way switch and one saved number.</p>

  ${aside('warn', 'Saves are per cart',
    '<p>Your saved numbers live in a folder named after your cart, so one game can never clobber another&rsquo;s high score. Handy &mdash; but it means if you rename your cart, it &ldquo;forgets&rdquo; everything, because it&rsquo;s now looking in a differently-named cubbyhole. Not a bug; just the filing system being honest.</p>')}

  <p>Look how far we&rsquo;ve come: you can draw, move, collide, juice, scroll a world, and remember a score. That is, no kidding, a whole game&rsquo;s worth of engine. Time to build a proper one.</p>


  ${chapHead('ch13', '13', 'the second payoff', 'A Proper Little Game')}
  ${creature('ace', 'A green blob grinning in the cockpit of its own little rocket')}

  <p>Chapter 5 was your first game. This is your first <em>real</em> one &mdash; a little shooter &mdash; and the wonderful thing is that we can build the whole thing out of chapters you&rsquo;ve already read, with no new ideas at all. Turn your sound on and watch it play itself:</p>

  ${screen('shooter', 'A self-playing space shooter: a ship tracking and firing at descending invaders, with sound',
    '<b>the ship hunts the nearest invader and fires; hits explode.</b><br>every piece of this is a chapter you already know.')}

  <p>Take it apart and there&rsquo;s nothing in the box you haven&rsquo;t seen:</p>

  <pre><span class="c">// bullets and enemies are just arrays of things ......... chapter 9</span>
<span class="k">for</span> (b in bullets) <span class="k">for</span> (e in enemies)
    <span class="k">if</span> (<span class="f">overlap</span>(b, e)) {          <span class="c">// does it touch? ................ chapter 8</span>
        <span class="f">shake</span>(<span class="n">3</span>); <span class="f">spawn_dust</span>(e.x, e.y);  <span class="c">// juice on the kill ......... chapter 10</span>
        <span class="f">hit</span>(<span class="n">38</span>, <span class="s">INSTR_NOISE</span>, <span class="n">5</span>, <span class="n">130</span>); <span class="c">// an explosion sound ..... chapter 7</span>
        score++;                     <span class="c">// and a number we could save . chapter 12</span>
    }</pre>

  <p>That&rsquo;s the secret the whole book has been sneaking up on: a big game isn&rsquo;t a big idea, it&rsquo;s a <em>pile</em> of small ones you already own, leaning on each other. Draw a thing, move it with the clock, ask if it touched another thing, make the answer feel good, keep score. The shooter is maybe forty lines longer than the catch game &mdash; and it is a whole different, richer toy. That&rsquo;s the leverage you&rsquo;ve been building this entire time.</p>

  ${aside('', 'The self-playing trick, again',
    '<p>Notice it plays itself, same as the catch game &mdash; here the &ldquo;AI&rdquo; is one line: slide the ship toward the nearest enemy&rsquo;s x. That&rsquo;s not really intelligence, it&rsquo;s Chapter 4&rsquo;s &ldquo;ease toward a target&rdquo; pointed at a bad guy. Half of what looks like game AI is exactly this: move toward (or away from) a number. Start there.</p>')}

  <p>You can make things now &mdash; real ones. There are only two pages left, and neither is about the game itself. They&rsquo;re about the two best moments in making anything: putting it in your pocket, and handing it to a friend.</p>


  ${chapHead('ch14', '14', 'in your pocket', 'Make It Work on a Phone')}
  ${creature('gamer', 'A green blob happily playing its own game on a phone')}

  <p>Here is a small miracle: the game you just made already runs on a phone. The touchscreen, though, has no buttons &mdash; so the engine offers to paint some <em>onto</em> the screen for you. One line, and a thumb-stick and a couple of buttons appear, wired to the very same <code>btn</code> and <code>stick</code> calls you already use:</p>

  <pre><span class="k">void</span> <span class="f">init</span>(<span class="k">void</span>) {
    <span class="f">touch_layout</span>(<span class="s">TOUCH_DPAD8</span>, <span class="n">2</span>);   <span class="c">// an 8-way pad + 2 buttons, drawn for you</span>
}</pre>

  ${screen('phone', 'A game with an on-screen thumb-stick and A/B buttons; the hero moves and jumps',
    '<b>a stick and two buttons, painted on the glass.</b><br>they drive the same btn()/stick() your keyboard did &mdash; you change nothing else.')}

  <p>That&rsquo;s genuinely most of it. The engine even keeps the controls clear of the notch and the home-bar for you (<code>safe_rect</code>), and sizes them to a real thumb (<code>finger_px</code>) &mdash; because a button sized in raw pixels is a coin-flip on a phone. The lovely part is that your <em>game</em> didn&rsquo;t change at all. It still just asks &ldquo;is left held?&rdquo; It neither knows nor cares that &ldquo;held&rdquo; now means a thumb on glass.</p>

  ${aside('', 'Design for the thumb',
    '<p>Two humane touches turn a technically-playable phone game into a nice one: put the important stuff where thumbs <em>aren&rsquo;t</em> (the bottom corners are covered by hands), and make touch targets generous &mdash; the same forgiveness you gave hitboxes in Chapter 8, now for fingers. When in doubt, bigger.</p>')}


  ${chapHead('ch15', '15', 'the send-off', 'Ship It')}
  ${creature('farewell', 'The Chapter-1 greeter blob, back to wave goodbye, a heart drifting up')}

  <p>A game nobody plays is a diary. The last, best step &mdash; the one that turns a project into a <em>thing</em> &mdash; is to hand it to someone. And you don&rsquo;t need an app store or a publisher or anyone&rsquo;s permission: dreamengine can bake your cart into a web page, a single link you can text to a friend. They tap it; it just&hellip; plays.</p>

  ${screen('ship', 'The game running inside a browser window, captioned "your friend is playing it"',
    '<b>your cart, baked to a web page, on someone else&rsquo;s screen.</b><br>no store, no install &mdash; a link. that&rsquo;s the whole distance from your machine to theirs.')}

  <p>And that&rsquo;s the book. Look back at what those six little words became: a sun, then a game you could poke, then worlds and crowds and juice and a shooter that sings and a thing your friend can play from a link. None of it was hard. It was small ideas, stacked patiently, each one leaning on the last &mdash; which is, it turns out, the only way anything big has ever been made.</p>

  <p>The gremlin met you on the first page not knowing a single function. You&rsquo;re leaving able to build and <em>ship</em> a game. Same tiny machine, same thirty-two crayons &mdash; the only thing that changed is you.</p>

  <p>So: go do a great good. Make the silly thing. Share the rough thing. The window is 320&times;200, the crayons are cheap, and the whole point was never the manual &mdash; it was the little world only you were going to make. We can&rsquo;t wait to play it.</p>

  <p style="text-align:center;font-family:var(--comic);color:var(--yellow);font-size:20px;margin-top:1.6em">&mdash; fin &mdash;</p>

</main>

<footer>
  <b>Every picture on this page is real console output</b> &mdash; and the sequencer is real console <em>sound</em>.<br>
  Stills baked with <code>play.js</code>, the moving ones with <code>make-gif</code> (the sound chapter is a
  webm carrying the cart&rsquo;s own audio) &mdash; each from a tiny cart in <code>tools/carts/book/</code>,
  rebuilt by <code>tools/build-book.js</code>. The book is illustrated by the very machine it teaches.
</footer>
`;

fs.writeFileSync(OUT, html);
console.log('\nwrote docs/learn-you-a-dreamengine.html —', Object.keys(ILLUS).length, 'illustrations');
