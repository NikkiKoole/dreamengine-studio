#!/usr/bin/env node
// road-check.js — the CORRECTNESS oracle for coverage-based ROAD rendering. The third visual gate,
// alongside tools/mirror-diff.js (SYMMETRY only — blind at skew/curve) and tools/spec.js (the pure
// GEOMETRY fns — never looks at pixels). road-check reads the RENDERED framebuffer and asserts
// invariants the kerb/asphalt must satisfy AT ANY ANGLE. Built for the field-based road refactor
// (docs/design/field-based-road-rendering.md); reusable by any cart that paints grass/asphalt/kerb/
// sidewalk as flat fills. Run with MARKINGS OFF so the only play-area colours are the road set.
//
// USAGE
//   node tools/road-check.js <cart>                 # render headless (DE_FIELD_ROADS,DE_NO_MARKINGS) + check 1 frame
//   node tools/road-check.js <cart> --all           # run the cart's whole config matrix = the one-command gate
//   node tools/road-check.js --png <file>           # check an already-rendered frame
//   node tools/road-check.js --selfcheck            # known answers for the analysis (renders nothing)
//   node tools/road-check.js <cart> --keys "..t"    # tap keys first (set skew/T/…); same tokens as a .script
// OPTIONS
//   --overlay <out.png>   write an annotated map: naked=RED, stray=YELLOW, floating=MAGENTA, disconnected=BLUE
//   --zoom                on failure, also emit a 6× crop of the worst bbox next to the overlay (needs ImageMagick)
//   --defines A,B         -D pass-through (default DE_FIELD_ROADS,DE_NO_MARKINGS)
//   --grass/--asphalt/--kerb/--walk <#hex|idx>   override road colours for a non-pico cart (idx = pico32 index)
//   --top N --toolbar N   play-area bounds (default 24 / 62 — streetlab's title band + bottom toolbar)
//   --strict              also fail on disconnected road + unknown colours (default: report only)
//   --json   --quiet      machine output / exit-1-on-fail (CI)
//
// INVARIANTS (all 0 = a correct coverage kerb)
//   naked        ASPHALT pixel touching grass/sidewalk with NO kerb between — missing outline, road
//                "sticks out", or a clipped fillet. (The #1 catch — the per-arm↔field mismatch made these.)
//   stray        KERB pixel with no grass/sidewalk 4-neighbour — an interior dark pixel (per-arm overlap bug).
//   floating     KERB pixel with no asphalt in its 8-neighbourhood — a detached / >1px-fattened kerb.
//   disconnected ASPHALT not reachable from the main road blob (a gap split the road).            [--strict]
//   unknown      a non-road colour inside the play area — a marking that didn't get gated, or wrong palette. [--strict]
//
// ⚠ THE CONTROL. Every invariant here is "no BAD pixel of kind X was found", which is trivially
// true of a frame containing no pixel of kind X. Two measured vacuous passes: an empty play area
// (`--top 190 --toolbar 20`) examined 0 pixels and said PASS, and the wrong palette made all 36480
// play-area pixels `unknown` -- info-only without --strict -- and it said PASS again. controlCheck
// now asks whether the frame could have produced a finding at all, naming the invariant each
// missing class disables. It exits nonzero in BOTH modes, unlike a finding.
//
// SEE ALSO: docs/guides/debug-harness.md ("Visual gates"), docs/design/field-based-road-rendering.md.
const fs = require('fs'), zlib = require('zlib'), { execFileSync } = require('child_process');
const path = require('path'), os = require('os');

const args = process.argv.slice(2);
const opt = (n, d) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : d; };
const has = (n) => args.includes(n);
const QUIET = has('--quiet'), JSON_OUT = has('--json'), STRICT = has('--strict'), ZOOM = has('--zoom');
const pngArg = opt('--png', null);
const cart = pngArg ? null : args.find((a, i) => !a.startsWith('--') && (i === 0 || !args[i - 1].startsWith('--')));
const TOP = parseInt(opt('--top', '24'), 10), TOOLBAR = parseInt(opt('--toolbar', '62'), 10);

// ── colour resolution: a #hex, or a pico32 palette index. Defaults = streetlab's road set. ──
let PAL = null;
function palette() { if (!PAL) { try { PAL = JSON.parse(fs.readFileSync('editor/public/palettes/pico32.json','utf8')).palette; } catch { PAL = []; } } return PAL; }
function rgb(spec, def) { if (!spec) return def; if (/^#/.test(spec)) { const h=spec.slice(1); return [parseInt(h.slice(0,2),16),parseInt(h.slice(2,4),16),parseInt(h.slice(4,6),16)]; }
  const hx = palette()[parseInt(spec,10)]; return hx ? rgb(hx) : def; }
const GRASS = rgb(opt('--grass',null),[0,135,81]), ASPH = rgb(opt('--asphalt',null),[95,87,79]),
      KERB = rgb(opt('--kerb',null),[41,24,20]), WALK = rgb(opt('--walk',null),[194,195,199]);
const eq = (px,o,c) => px[o]===c[0] && px[o+1]===c[1] && px[o+2]===c[2];

// ── PNG decode (RGBA/RGB/gray, all 5 filters) ──
function decodePNG(file) {
  const b = fs.readFileSync(file); let off = 8, W, H, ct, idat = [];
  while (off < b.length) { const len = b.readUInt32BE(off), type = b.toString('latin1', off+4, off+8);
    const data = b.slice(off+8, off+8+len);
    if (type==='IHDR'){ W=data.readUInt32BE(0); H=data.readUInt32BE(4); ct=data[9]; }
    else if (type==='IDAT') idat.push(data); else if (type==='IEND') break; off += 12+len; }
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const ch = ct===6?4:ct===2?3:ct===0?1:4, stride = W*ch, px = Buffer.alloc(H*stride);
  const paeth=(a,bb,c)=>{const p=a+bb-c,pa=Math.abs(p-a),pb=Math.abs(p-bb),pc=Math.abs(p-c);return pa<=pb&&pa<=pc?a:pb<=pc?bb:c;};
  let p=0,r=0;
  for (let y=0;y<H;y++){ const f=raw[p++];
    for (let x=0;x<stride;x++){ const cur=raw[p++], a=x>=ch?px[r+x-ch]:0, up=y>0?px[r-stride+x]:0, ul=(x>=ch&&y>0)?px[r-stride+x-ch]:0;
      let v; switch(f){case 0:v=cur;break;case 1:v=cur+a;break;case 2:v=cur+up;break;case 3:v=cur+((a+up)>>1);break;case 4:v=cur+paeth(a,up,ul);break;default:v=cur;}
      px[r+x]=v&255; } r+=stride; }
  return { W, H, ch, px };
}
// ── PNG encode (RGB) for the overlay ──
function crc32(buf){let c=~0;for(let i=0;i<buf.length;i++){c^=buf[i];for(let k=0;k<8;k++)c=(c>>>1)^(0xEDB88320&-(c&1));}return ~c>>>0;}
function chunk(t,d){const ty=Buffer.from(t,'latin1'),b=Buffer.concat([ty,d]),o=Buffer.alloc(12+d.length);o.writeUInt32BE(d.length,0);ty.copy(o,4);d.copy(o,8);o.writeUInt32BE(crc32(b),8+d.length);return o;}
function encodePNG(W,H,rgbBuf){const st=W*3,raw=Buffer.alloc(H*(st+1));for(let y=0;y<H;y++){raw[y*(st+1)]=0;rgbBuf.copy(raw,y*(st+1)+1,y*st,y*st+st);}
  const ih=Buffer.alloc(13);ih.writeUInt32BE(W,0);ih.writeUInt32BE(H,4);ih[8]=8;ih[9]=2;
  return Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',ih),chunk('IDAT',zlib.deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]);}

// ── render the cart headless if no --png (mirror-diff's recipe) ──
function renderCart(name, keys, defines) {
  let scriptPath = '/dev/null';
  const toks = (keys||'').split(/\s+/).filter(Boolean);
  if (toks.length) { let lines = [], f = 2; for (const k of toks) { lines.push(`down ${f} ${k}`); lines.push(`up ${f+1} ${k}`); f += 2; }
    scriptPath = path.join(os.tmpdir(), `roadcheck-${name}.script`); fs.writeFileSync(scriptPath, lines.join('\n')+'\n'); }
  const frames = toks.length ? toks.length*2 + 6 : 2;
  const dump = path.join(process.cwd(), 'build', '.road-check', name); fs.rmSync(dump, { recursive: true, force: true }); fs.mkdirSync(dump, { recursive: true });
  execFileSync('node', ['tools/play.js', name, 'script', scriptPath, '--headless', '--frames', String(frames), '--dump', dump],
    { cwd: process.cwd(), stdio: 'pipe', env: { ...process.env, DE_DEFINES: defines } });
  const fr = fs.readdirSync(dump).filter(x => x.endsWith('.png')).sort();
  return path.join(dump, fr[fr.length - 1]);
}

// The analysis, as a pure function of a decoded image + the colour/bounds options. Lifted out of
// checkFile so --selfcheck can put hand-built frames with a known answer through it; checkFile is
// now just "decode, then this". Verified inert against streetlab's --all matrix before anything
// else changed.
function analyzeImage(img, o) {
  const { W, H, ch, px } = img;
  const { TOP, TOOLBAR, GRASS, ASPH, KERB, WALK, STRICT } = o;
  const y1 = H - TOOLBAR;
  // outside the play area = 'edge', NOT grass — a road running off-screen there is a legit exit.
  const cls = (x, y) => { if (x<0||x>=W||y<TOP||y>=y1) return 'edge'; const oo=(y*W+x)*ch;
    if (eq(px,oo,GRASS)) return 'grass'; if (eq(px,oo,ASPH)) return 'asph'; if (eq(px,oo,KERB)) return 'kerb'; if (eq(px,oo,WALK)) return 'walk'; return 'other'; };
  const isOpen = t => t==='grass'||t==='walk';
  const naked = [], stray = [], floating = [], unknown = [];
  const seen = { grass: 0, asph: 0, kerb: 0, walk: 0, other: 0 };   // what the frame is MADE of — the control reads this
  const road = new Uint8Array(W*H);                              // mark asphalt+kerb for connectivity
  for (let y = TOP; y < y1; y++) for (let x = 0; x < W; x++) {
    const t = cls(x, y);
    seen[t]++;
    if (t==='asph'||t==='kerb') road[y*W+x] = 1;
    if (t === 'asph') {
      if (cls(x-1,y)==='grass'||cls(x+1,y)==='grass'||cls(x,y-1)==='grass'||cls(x,y+1)==='grass'
        ||cls(x-1,y)==='walk' ||cls(x+1,y)==='walk' ||cls(x,y-1)==='walk' ||cls(x,y+1)==='walk') naked.push([x,y]);
    } else if (t === 'kerb') {
      // 8-conn for BOTH tests: an interior stray has no grass/sidewalk in ANY direction (4-conn gave false
      // positives on diagonal INSET kerbs, where the open pixel is only a corner neighbour); a floating kerb
      // has no asphalt in ANY direction. So a valid 1px kerb has both an asphalt AND an open 8-neighbour.
      let touchOpen = false, touchAsph = false;
      for (let dy=-1;dy<=1;dy++) for (let dx=-1;dx<=1;dx++) { if (dx===0&&dy===0) continue;
        const c = cls(x+dx,y+dy); if (isOpen(c)||c==='edge') touchOpen = true; if (c==='asph') touchAsph = true; }   // border = legit exit, counts as open
      if (!touchOpen) stray.push([x,y]);
      if (!touchAsph) floating.push([x,y]);
    } else if (t === 'other') unknown.push([x,y]);
  }
  // connectivity: BFS over the largest road blob; anything road but unreached = disconnected.
  let disconnected = [];
  let best = -1, bestSeed = -1;
  const comp = new Int32Array(W*H).fill(-1); let nc = 0;
  for (let i=0;i<W*H;i++) if (road[i] && comp[i]<0) {
    const stack=[i]; comp[i]=nc; let sz=0;
    while (stack.length){ const p=stack.pop(); sz++; const x=p%W,y=(p/W)|0;
      for (const [dx,dy] of [[1,0],[-1,0],[0,1],[0,-1]]){ const nx=x+dx,ny=y+dy; if(nx<0||ny<0||nx>=W||ny>=H) continue; const q=ny*W+nx; if(road[q]&&comp[q]<0){comp[q]=nc;stack.push(q);} } }
    if (sz>best){best=sz;bestSeed=nc;} nc++;
  }
  if (nc>1) for (let y=TOP;y<y1;y++) for (let x=0;x<W;x++){ const i=y*W+x; if(road[i]&&comp[i]!==bestSeed) disconnected.push([x,y]); }

  const bbox = a => a.length ? `x[${Math.min(...a.map(p=>p[0]))}-${Math.max(...a.map(p=>p[0]))}] y[${Math.min(...a.map(p=>p[1]))}-${Math.max(...a.map(p=>p[1]))}]` : '-';
  const marks = { naked, stray, floating, disconnected, unknown };
  const res = { W, H, naked: naked.length, stray: stray.length, floating: floating.length,
    disconnected: disconnected.length, unknown: unknown.length,
    nakedBox: bbox(naked), strayBox: bbox(stray), floatingBox: bbox(floating), disconnectedBox: bbox(disconnected), unknownBox: bbox(unknown) };
  res.fail = !!(res.naked || res.stray || res.floating || (STRICT && (res.disconnected || res.unknown)));
  return { res, marks, seen, area: Math.max(0, y1 - TOP) * W, src: { W, H, ch, px } };
}

// ⚠ THE CONTROL. Every invariant here is "no BAD pixel of kind X was found", and each is trivially
// satisfied by a frame that contains no pixel of kind X at all. Two measured vacuous passes:
//   • `--top 190 --toolbar 20` leaves an EMPTY play area — 0 pixels examined, verdict PASS.
//   • the wrong palette (or a cart that did not draw its road) makes every pixel `unknown`, which
//     is info-only unless --strict. Measured: all 36480 play-area pixels unrecognised, verdict PASS.
// No threshold can catch either, because the counts are honestly zero. So instead of judging the
// findings, this asks whether the frame could have produced a finding at all — one necessary
// condition per invariant, each naming the check it silently disables. No invented percentages:
// these are all "> 0", and a healthy streetlab frame reads grass 62.6% / asphalt 35.4% / kerb 2.0%.
function controlCheck(seen, area) {
  const bad = [];
  if (area === 0) {
    bad.push('the play area is EMPTY (0 pixels) — --top and --toolbar leave no rows to examine, so every invariant below passed without looking at anything');
    return bad;
  }
  const open = seen.grass + seen.walk;
  if (!seen.asph)  bad.push(`no ASPHALT anywhere in the play area (${area} px: ${seen.other} unrecognised) — there is no road here, so "naked" cannot fire`);
  if (!seen.kerb)  bad.push('no KERB anywhere in the play area — "stray" and "floating" are checks on kerb pixels, so both are vacuous');
  if (!open)       bad.push('no GRASS or SIDEWALK anywhere in the play area — "naked" is asphalt touching an open colour, so it cannot fire');
  return bad;
}

// the file wrapper: decode, then analyse with the CLI's options
function checkFile(file) {
  const img = decodePNG(file);
  const out = analyzeImage(img, { TOP, TOOLBAR, GRASS, ASPH, KERB, WALK, STRICT });
  out.res.file = path.relative(process.cwd(), file);
  out.control = controlCheck(out.seen, out.area);
  if (out.control.length) out.res.fail = true;   // a frame that could not fail is not a pass
  return out;
}

function writeOverlay(out, src, marks) {
  const { W, H, ch, px } = src; const o = Buffer.alloc(W*H*3);
  for (let i=0;i<W*H;i++){ o[i*3]=px[i*ch]; o[i*3+1]=px[i*ch+1]; o[i*3+2]=px[i*ch+2]; }   // dim base
  for (let i=0;i<o.length;i++) o[i] = (o[i]*0.45)|0;
  const paint=(arr,r,g,b)=>{ for(const [x,y] of arr){ const k=(y*W+x)*3; o[k]=r;o[k+1]=g;o[k+2]=b; } };
  paint(marks.disconnected,0,128,255); paint(marks.unknown,255,160,0);
  paint(marks.floating,255,0,255); paint(marks.stray,255,255,0); paint(marks.naked,255,0,0);   // naked on top (most important)
  fs.writeFileSync(out, encodePNG(W,H,o));
}
function zoomBbox(overlay, marks) {                               // 6× crop of the worst cluster, for a quick Read
  const all=[...marks.naked,...marks.stray,...marks.floating]; if(!all.length) return null;
  const xs=all.map(p=>p[0]),ys=all.map(p=>p[1]); const x0=Math.max(0,Math.min(...xs)-8),y0=Math.max(0,Math.min(...ys)-8);
  const w=Math.min(120,Math.max(...xs)-x0+16),h=Math.min(120,Math.max(...ys)-y0+16);
  const out=overlay.replace(/\.png$/,'.zoom.png');
  try { execFileSync('magick',[overlay,'-crop',`${w}x${h}+${x0}+${y0}`,'-filter','point','-resize','600%',out],{stdio:'pipe'}); return out; } catch { return null; }
}

// ── --selfcheck: KNOWN ANSWERS FOR THE ANALYSIS ──────────────
// Renders no cart. Every frame is drawn here from an ASCII map, so each expected count is something
// you can verify by looking at the string. `.`=grass  `#`=asphalt  `K`=kerb  `W`=sidewalk  `?`=other
function selfcheck() {
  let pass = 0, fail = 0;
  const ok = (name, cond, got) => {
    if (cond) { pass++; console.log(`  ✓ ${name}`); } else { fail++; console.log(`  ✗ ${name}   got: ${got}`); }
  };
  const COL = { '.': GRASS, '#': ASPH, K: KERB, W: WALK, '?': [1, 2, 3] };
  const frame = (rows) => {
    const H = rows.length, W = rows[0].length, px = Buffer.alloc(W * H * 4);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      const c = COL[rows[y][x]], i = (y * W + x) * 4;
      px[i] = c[0]; px[i + 1] = c[1]; px[i + 2] = c[2]; px[i + 3] = 255;
    }
    return { W, H, ch: 4, px };
  };
  // no title band and no toolbar, so the whole image IS the play area and the counts are the map
  const run = (rows, o = {}) => analyzeImage(frame(rows),
    { TOP: 0, TOOLBAR: 0, GRASS, ASPH, KERB, WALK, STRICT: false, ...o });

  console.log('road-check --selfcheck — known answers for the analysis (nothing is rendered)\n');

  console.log('A CORRECTLY KERBED ROAD RAISES NOTHING');
  const good = ['........', '.KKKKKK.', '.K####K.', '.K####K.', '.KKKKKK.', '........'];
  const g = run(good);
  ok('a road fully ringed by kerb is clean', g.res.naked + g.res.stray + g.res.floating === 0,
     JSON.stringify(g.res));
  // counted off the map above: 8 asphalt (2 rows of 4), 16 kerb (6+2+2+6), 24 grass (the rest)
  ok('  …and the frame really did contain all three classes', g.seen.asph === 8 && g.seen.kerb === 16 && g.seen.grass === 24,
     JSON.stringify(g.seen));
  ok('  …so the control is satisfied', controlCheck(g.seen, g.area).length === 0, controlCheck(g.seen, g.area));

  console.log('\nEACH INVARIANT FIRES ON ITS OWN DEFECT, AND ONLY ITS OWN');
  // one kerb pixel removed from the top edge: the asphalt under it now touches grass
  const naked1 = ['........', '.KKK.KK.', '.K####K.', '.K####K.', '.KKKKKK.', '........'];
  ok('a single missing kerb pixel is exactly 1 naked asphalt pixel', run(naked1).res.naked === 1,
     run(naked1).res.naked);
  ok('  …and raises no stray or floating', run(naked1).res.stray + run(naked1).res.floating === 0,
     `${run(naked1).res.stray}/${run(naked1).res.floating}`);
  // a kerb pixel buried inside the asphalt: no open 8-neighbour anywhere
  const stray1 = ['........', '.KKKKKK.', '.K####K.', '.K#K##K.', '.KKKKKK.', '........'];
  ok('a kerb pixel with no open 8-neighbour is 1 stray', run(stray1).res.stray === 1, run(stray1).res.stray);
  ok('  …and is NOT also counted as floating (it touches asphalt)', run(stray1).res.floating === 0,
     run(stray1).res.floating);
  // a kerb pixel out in the grass, nowhere near the road
  const float1 = ['.....K..', '.KKKKKK.', '.K####K.', '.K####K.', '.KKKKKK.', '........'];
  ok('a kerb pixel detached from the asphalt is 1 floating', run(float1).res.floating === 1,
     run(float1).res.floating);
  // the 8-connectivity rule the comments call out: a DIAGONAL inset kerb is legal, not a stray
  const inset = ['........', '.KKKKKK.', '.K####K.', '.KK###K.', '..KKKKK.', '........'];
  ok('a diagonally inset kerb is legal — 4-connectivity used to call this a stray',
     run(inset).res.stray === 0, run(inset).res.stray);

  console.log('\nTHE PLAY-AREA BOUNDS, AND THE EDGE RULE');
  // asphalt running off the left border is a legitimate exit, not a naked edge
  // asphalt runs off the LEFT border, kerbed above/below and capped on the right
  const exit = ['........', 'KKKKKK..', '#####K..', '#####K..', 'KKKKKK..', '........'];
  ok('a road leaving the frame is not naked (the border is an exit, not grass)',
     run(exit).res.naked === 0, run(exit).res.naked);
  ok('  …and the asphalt really does reach the border (or the case is not being tested)',
     run(exit).seen.asph === 10, run(exit).seen.asph);
  const banded = run(good, { TOP: 2, TOOLBAR: 2 });
  ok('--top/--toolbar really do shrink what is examined', banded.area === 8 * 2, banded.area);
  ok('  …and rows outside the band are classed "edge", so the kerb there is not judged',
     banded.res.stray === 0 && banded.res.floating === 0, JSON.stringify(banded.res));

  console.log('\nTHE CONTROL — a frame that could not have failed is not a pass');
  const empty = run(good, { TOP: 3, TOOLBAR: 3 });
  ok('an empty play area examines 0 pixels', empty.area === 0, empty.area);
  ok('  …and finds nothing wrong, which is the trap', empty.res.fail === false, empty.res.fail);
  ok('  …so the control refuses it', controlCheck(empty.seen, empty.area).some(c => c.includes('EMPTY')),
     controlCheck(empty.seen, empty.area));
  // the measured case: the wrong palette makes every pixel unrecognised
  const alien = run(['????????', '????????', '????????']);
  ok('a frame in colours the tool does not know finds no defects at all',
     alien.res.naked + alien.res.stray + alien.res.floating === 0, JSON.stringify(alien.res));
  ok('  …and `unknown` alone does NOT fail without --strict (which is why it passed)',
     alien.res.fail === false, alien.res.fail);
  ok('  …so the control refuses it, naming all three disabled invariants',
     controlCheck(alien.seen, alien.area).length === 3, controlCheck(alien.seen, alien.area));
  // each necessary condition on its own
  const noKerb = run(['........', '.######.', '.######.', '........']);
  ok('a road with NO kerb is refused — stray and floating are checks on kerb pixels',
     controlCheck(noKerb.seen, noKerb.area).some(c => c.includes('no KERB')), controlCheck(noKerb.seen, noKerb.area));
  const noOpen = run(['KKKKKKKK', 'K######K', 'K######K', 'KKKKKKKK']);
  ok('a frame with no grass or sidewalk is refused — naked cannot fire',
     controlCheck(noOpen.seen, noOpen.area).some(c => c.includes('no GRASS')), controlCheck(noOpen.seen, noOpen.area));
  ok('  …and a WALK-only surround satisfies it (sidewalk counts as open)',
     controlCheck(run(['WWWWWWWW', 'WKKKKKKW', 'WK####KW', 'WKKKKKKW']).seen, 32).length === 0,
     controlCheck(run(['WWWWWWWW', 'WKKKKKKW', 'WK####KW', 'WKKKKKKW']).seen, 32));
  ok('  …which matters because a healthy streetlab frame has ZERO sidewalk pixels',
     true, '');

  console.log('\nSTRICT ONLY CHANGES THE VERDICT, NOT THE COUNTS');
  const withUnknown = ['........', '.KKKKKK.', '.K####K.', '.KKKKKK.', '..??....'];
  ok('unknown pixels are counted either way', run(withUnknown).res.unknown === 2, run(withUnknown).res.unknown);
  ok('  …but only fail under --strict',
     run(withUnknown).res.fail === false && run(withUnknown, { STRICT: true }).res.fail === true,
     `${run(withUnknown).res.fail} / ${run(withUnknown, { STRICT: true }).res.fail}`);

  console.log(`\n${fail ? '✗' : '✓'} ${pass} passed, ${fail} failed`);
  return fail ? 1 : 0;
}

if (has('--selfcheck')) process.exit(selfcheck());

// ── streetlab's config matrix: every mode the field is claimed to cover (add modes as they land). ──
const MATRIX = [
  ['default',''], ['skew+35','. . . . . . .'], ['skew-35',', , , , , , ,'], ['T','t'], ['T+skew','t . . . . . . .'],
  ['turn-lanes','p'], ['pavement','k'], ['median','m'], ['twltl','m m'], ['bike','b'], ['parking',';'],
  ['3-lane','= ='], ['skew+bike+park+peds','. . . . . . . b ; k'],
  ['roundabout','r'], ['roundabout+skew','r . . . . . . .'], ['roundabout+peds','r k'],
  ['free-right','f'], ['free-right+skew','f . . . . . . .'], ['free-right+peds','f k'],
];

if (has('--all')) {
  const defines = opt('--defines', 'DE_FIELD_ROADS,DE_NO_MARKINGS');
  const ovDir = path.join('build', '.road-check'); let anyFail = false; const rows = [];
  let anyControl = false;
  for (const [label, keys] of MATRIX) {
    const f = renderCart(cart, keys, defines); const { res, marks, src, control } = checkFile(f);
    if (res.fail) { const ov = path.join(ovDir, `FAIL-${label.replace(/[^a-z0-9]+/gi,'_')}.overlay.png`); writeOverlay(ov, src, marks); res.overlay = ov; if (ZOOM) res.zoom = zoomBbox(ov, marks); }
    anyFail = anyFail || res.fail; anyControl = anyControl || control.length > 0;
    rows.push({ label, ...res, control });
  }
  if (JSON_OUT) console.log(JSON.stringify(rows, null, 2));
  else {
    console.log(`road-check ${cart} --all  (field-based, markings off)\n`);
    console.log('  config                naked stray float disc unkn');
    for (const r of rows) console.log(`  ${r.fail?'✗':'✓'} ${r.label.padEnd(20)} ${String(r.naked).padStart(4)} ${String(r.stray).padStart(5)} ${String(r.floating).padStart(5)} ${String(r.disconnected).padStart(4)} ${String(r.unknown).padStart(4)}${r.fail?'  → '+(r.overlay||''):''}`);
    if (anyControl) {
      console.error('\n  ✗ SOME FRAMES COULD NOT HAVE FAILED:');
      for (const r of rows) for (const c of r.control) console.error(`      ${r.label}: ${c}`);
    }
    console.log(anyFail ? '\n  FAIL (overlays in build/.road-check/)' : '\n  all PASS');
  }
  // a control failure exits nonzero in BOTH modes: unlike a finding, "nothing was examined" is
  // never the exploratory kind of zero
  process.exit((QUIET && anyFail) || anyControl ? 1 : 0);
}

// ── single frame ──
const file = pngArg || renderCart(cart, opt('--keys', ''), opt('--defines', 'DE_FIELD_ROADS,DE_NO_MARKINGS'));
const { res, marks, src, control } = checkFile(file);
const overlay = opt('--overlay', null);
if (overlay) { writeOverlay(overlay, src, marks); res.overlay = overlay; if (ZOOM) res.zoom = zoomBbox(overlay, marks); }
if (JSON_OUT) console.log(JSON.stringify({ ...res, control }));
else {
  console.log(`road-check ${res.file}`);
  const line=(n,v,box,note)=>console.log(`  ${n.padEnd(13)}${String(v).padStart(4)}\t${v?box+'  ← '+note:''}`);
  line('naked',res.naked,res.nakedBox,'asphalt touching grass/sidewalk (missing kerb)');
  line('stray',res.stray,res.strayBox,'interior dark pixel');
  line('floating',res.floating,res.floatingBox,'kerb not attached to asphalt');
  line('disconnected',res.disconnected,res.disconnectedBox,'road split from the main blob'+(STRICT?'':' (info)'));
  line('unknown',res.unknown,res.unknownBox,'non-road colour in play area'+(STRICT?'':' (info)'));
  if (res.overlay) console.log(`  overlay  ${res.overlay}${res.zoom?'   zoom '+res.zoom:''}`);
  if (control.length) {
    console.error('\n  ✗ THIS FRAME COULD NOT HAVE FAILED:');
    for (const c of control) console.error(`      ${c}`);
    console.error('    `node tools/road-check.js --selfcheck` checks the analysis itself.');
  } else console.log(res.fail ? '  FAIL' : '  PASS');
}
process.exit((QUIET && res.fail) || control.length ? 1 : 0);
