#!/usr/bin/env node
/*
 * ctx-gen.js — move engine state into a per-instance context struct, mechanically.
 *
 * The edit half of the AUv3 per-instance work (design: docs/design/engine-context.md). It reads the
 * authoritative list of file-scope statics from `engine-statics.js --list` (clang's AST, so it cannot
 * miss a declaration shape) and the exceptions from `ctx-classification.json`, then emits:
 *
 *   runtime/<x>_ctx.h     the struct + a compile-time-initialised default instance + the macro block
 *   runtime/<x>            with those declarations removed and the header included at the top
 *
 * `--target sound.h` (default) or `--target studio.c`. Each engine file gets its OWN context struct
 * rather than one giant one, so a file can land and be verified byte-identical on its own.
 *
 * WHY A GENERATOR AND NOT AN AGENT OR A HUMAN. It is ~300 near-identical edits in one 9,200-line
 * file. A generator is auditable, re-runnable, and bisectable by batch; if it is wrong it is wrong in
 * one place. Hand edits at that scale fail silently and are found only by the byte-exact gate, at
 * which point you are bisecting 300 changes instead of fixing one function.
 *
 * THE SAFETY PROPERTY THAT MAKES THIS A PURE MOVE. The default instance is a `static` with
 * DESIGNATED INITIALISERS, so the non-zero values are still set at compile time exactly as they are
 * today, and everything else is still zero-initialised static storage:
 *
 *     static DeSound de_snd_default = { .echo_fb = 0.35f, … };
 *     static DeSound *de_snd = &de_snd_default;
 *     #define echo_fb (de_snd->echo_fb)
 *
 * There is no init function to call and therefore no init-ORDER risk: nothing can read a member
 * before it is set, because it is set by the linker, as before. Step B (a real per-instance context)
 * then allocates by copying this template, which is exact by construction.
 *
 * USAGE
 *   node tools/ctx-gen.js [--target studio.c]   dry run: what would move, what would be skipped
 *   node tools/ctx-gen.js --probe               apply to a COPY of runtime/ and compile it
 *   node tools/ctx-gen.js --write               apply for real (then run tools/refactor-guard.js)
 *   node tools/ctx-gen.js --primitive           BATCH 1: only statics of primitive type
 *   node tools/ctx-gen.js --check               self-test: known answers on a fixture
 *
 * BATCHING. `--primitive` restricts to statics whose type is built-in (float/int/bool/…). Those need
 * no type-hoist, because the struct can sit above everything. The remaining ones are typed
 * (`Voice`, `ReverbTank`, …) and need the transitive type-hoist described in the design doc — a
 * second batch, so the first one lands and is verified byte-identical on its own.
 *
 * CONSERVATISM IS DELIBERATE. Anything this cannot parse with certainty is SKIPPED and reported, not
 * guessed at. A declaration line whose names are only partly in scope is skipped whole. The tool
 * would rather move 250 variables and tell you about 3 than move 253 and be wrong about 1.
 */
'use strict';

const { execFileSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');

// One generator, several engine files. Each gets its own context struct rather than one giant one,
// so a file can land and be verified byte-identical on its own — and so the audio context can be
// created per plug-in instance independently of the video one, which the platform seam may want to
// scope differently.
const TARGETS = {
  'sound.h':  { file: 'runtime/sound.h',  header: 'sound_ctx.h',  type: 'DeSound', ptr: 'de_snd' },
  'studio.c': { file: 'runtime/studio.c', header: 'studio_ctx.h', type: 'DeVideo', ptr: 'de_vid' },
};
const TARGET_KEY = (() => {
  const i = process.argv.indexOf('--target');
  const k = i >= 0 ? process.argv[i + 1] : 'sound.h';
  if (!TARGETS[k]) { console.error('ctx-gen: unknown --target ' + k + ' (have: ' + Object.keys(TARGETS).join(', ') + ')'); process.exit(2); }
  return k;
})();
const TARGET = TARGETS[TARGET_KEY].file;
const CTX_HEADER = TARGETS[TARGET_KEY].header;
const CTX_TYPE = TARGETS[TARGET_KEY].type;
const CTX_PTR = TARGETS[TARGET_KEY].ptr;

const PRIMITIVES = new Set([
  'float', 'double', 'int', 'bool', 'char', 'short', 'long', 'unsigned', 'signed', 'void',
  'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t', 'int8_t', 'int16_t', 'int32_t', 'int64_t',
  'size_t', 'atomic_int', 'atomic_bool', 'atomic_uint', 'atomic_long', 'atomic_llong',
]);

/* ───────────────────────────────────────────────────────────────── inputs ── */

function loadStatics() {
  const out = execFileSync('node', ['tools/engine-statics.js', '--list'], { cwd: ROOT, maxBuffer: 1 << 26 }).toString();
  return JSON.parse(out).filter(v => v.file === TARGET);
}

// Names a `#define name (ctx->name)` would break: a local, a parameter, or a STRUCT FIELD. The
// field case is the one that bites — the preprocessor does not know about `->`, so a same-named
// field turns `ins->rvb_tank` into `ins->(de_snd->rvb_tank)`. Refusing to move these is what stops
// the generator producing a file that cannot compile.
function loadCollisions() {
  const out = execFileSync('node', ['tools/engine-statics.js', '--json'], { cwd: ROOT, maxBuffer: 1 << 26 }).toString();
  const found = new Set(Object.keys(JSON.parse(out).collisions || {}));
  // A waiver means the clash is handled AT THE SOURCE (see ctx-classification.json), not that it is
  // believed harmless. Each one has to say how.
  const c = JSON.parse(fs.readFileSync(path.join(ROOT, 'tools', 'ctx-classification.json'), 'utf8'));
  for (const name of Object.keys(c.collision_waivers || {})) if (!name.startsWith('_')) found.delete(name);
  return found;
}

function loadExclusions() {
  const c = JSON.parse(fs.readFileSync(path.join(ROOT, 'tools', 'ctx-classification.json'), 'utf8'));
  const ex = new Map();
  // Per-target classification: sound.h's groups live at the top level (written first), studio.c's
  // under a `studio_c` key. A target with no classification yet yields no exclusions, which is
  // deliberately LOUD rather than silently permissive — the dry run shows everything moving, which
  // is the signal to go and classify first.
  const scope = TARGET_KEY === 'sound.h' ? c : (c.studio_c || {});
  for (const group of ['shared', 'harness', 'function_local', 'dead_weight', 'defer']) {
    for (const name of Object.keys(scope[group] || {})) {
      if (name.startsWith('_')) continue;
      ex.set(name, group);
    }
  }
  return ex;
}

/* ─────────────────────────────────────────────── declaration text parsing ── */

// Strip a trailing // comment that is not inside a string literal.
function stripComment(s) {
  const i = s.indexOf('//');
  return i < 0 ? s : s.slice(0, i);
}

// Read the FULL declaration starting at `line` (1-based), joining continuation lines up to the `;`.
function readDecl(src, line) {
  let text = stripComment(src[line - 1]);
  let last = line - 1;
  while (!/;/.test(text) && last + 1 < src.length) { last++; text += ' ' + stripComment(src[last]); }
  return { text: text.replace(/;[\s\S]*$/, ''), firstLine: line - 1, lastLine: last };
}

// Split a declaration body into declarators on TOP-LEVEL commas only.
function splitDeclarators(body) {
  const parts = [];
  let depth = 0, start = 0;
  for (let i = 0; i < body.length; i++) {
    const c = body[i];
    if (c === '{' || c === '(' || c === '[') depth++;
    else if (c === '}' || c === ')' || c === ']') depth--;
    else if (c === ',' && depth === 0) { parts.push(body.slice(start, i)); start = i + 1; }
  }
  parts.push(body.slice(start));
  return parts.map(s => s.trim()).filter(Boolean);
}

// From `float  echo_buf[88200]` → { name, stars, dims, init }. Returns null if not confidently parsed.
function parseDeclarator(d, knownNames) {
  const m = d.match(/^([\s\S]*?)(\**)\s*([A-Za-z_]\w*)\s*((?:\[[^\]]*\])*)\s*(?:=\s*([\s\S]*))?$/);
  if (!m) return null;
  const [, lead, stars, name, dims, init] = m;
  if (!knownNames.has(name)) return null;         // the AST did not list this name: do not touch it
  return { lead: lead.trim(), stars, name, dims: dims || '', init: init === undefined ? null : init.trim() };
}

const ZERO = /^(0|0\.0f?|0u|0L|0x0u?|NULL|false|\{\s*0\s*\}|\{\s*\{\s*0\s*\}\s*\}|\{\s*\}|"")$/;

/* ─────────────────────────────────────────────────────────────── planning ── */

function plan(opts) {
  // ⚠ RE-RUNNING ON AN ALREADY-PROCESSED FILE IS DESTRUCTIVE. The generator builds the context from
  // the statics it can SEE, so a second run over a file whose statics have already moved regenerates
  // the header from the handful that remain — silently discarding the hundreds already migrated.
  // The generated header is not cumulative and cannot be: the declarations it was built from are
  // gone from the source. To redo a target, restore both files from git first.
  const already = fs.readFileSync(path.join(ROOT, TARGET), 'utf8').includes('#include "' + CTX_HEADER + '"');
  if (already) {
    console.error(`\nctx-gen: ${TARGET} already includes ${CTX_HEADER} — it has been processed.`);
    console.error('Running again would rebuild the context from only the statics that REMAIN and');
    console.error('throw away everything already moved. Refusing.\n');
    console.error(`To redo it:  git checkout <commit> -- ${TARGET} && rm runtime/${CTX_HEADER}`);
    process.exit(4);
  }

  const statics = loadStatics();
  const exclusions = loadExclusions();
  const collisions = loadCollisions();
  const src = fs.readFileSync(path.join(ROOT, TARGET), 'utf8').split('\n');

  const byLine = new Map();
  for (const v of statics) {
    if (!byLine.has(v.line)) byLine.set(v.line, []);
    byLine.get(v.line).push(v);
  }

  const move = [];       // { name, memberDecl, init, line }
  const skipped = [];    // { line, names, why }

  for (const [line, vars] of [...byLine].sort((a, b) => a[0] - b[0])) {
    const names = vars.map(v => v.name);

    // an excluded name anywhere on the line takes the WHOLE line out — a declaration is one
    // statement and splitting it is a guess
    const ex = names.filter(n => exclusions.has(n));
    if (ex.length) { skipped.push({ line, names, why: 'classified ' + exclusions.get(ex[0]) + ' (' + ex.join(' ') + ')' }); continue; }

    // A static whose ADDRESS is taken in another file-scope initialiser cannot move: `&game_font`
    // is a constant expression today, but `&(de_vid->game_font)` is not, so the table that points
    // at it (`static Font *const FONT_SLOT[] = { &game_font, … }`) stops compiling. Caught by the
    // probe as an error; reported here so it reads as a decision rather than a crash.
    const addressed = names.filter(n => addressTakenAtFileScope(src, n));
    if (addressed.length) { skipped.push({ line, names, why: 'ADDRESS TAKEN in a file-scope initialiser (' + addressed.join(' ') + ') — &member is not a constant expression; move the table to runtime init first, or keep this shared' }); continue; }

    const clash = names.filter(n => collisions.has(n));
    if (clash.length) { skipped.push({ line, names, why: 'NAME COLLISION — the macro would rewrite another use of this identifier (' + clash.join(' ') + '); rename one side first' }); continue; }

    // batch filter: primitive base types only, no user-defined type => no type-hoist needed
    if (opts.primitive) {
      const nonPrim = vars.filter(v => {
        const base = v.type.replace(/\[[^\]]*\]/g, '').replace(/\*/g, '')
                           .replace(/\b(const|volatile|_Atomic|struct|enum|union)\b/g, '').trim();
        return !base.split(/\s+/).every(w => !w || PRIMITIVES.has(w));
      });
      if (nonPrim.length) { skipped.push({ line, names, why: 'non-primitive type (' + nonPrim.map(v => v.type).join(', ') + ') — needs the type-hoist batch' }); continue; }
    }

    const decl = readDecl(src, line);
    const body = decl.text.replace(/^\s*static\s+/, '');
    const declarators = splitDeclarators(body);
    const known = new Set(names);
    const parsed = declarators.map(d => parseDeclarator(d, known));

    if (parsed.some(p => p === null) || parsed.length !== names.length) {
      skipped.push({ line, names, why: 'could not parse the declaration with certainty' });
      continue;
    }

    const baseType = parsed[0].lead;
    if (!baseType) { skipped.push({ line, names, why: 'no base type found' }); continue; }

    for (const p of parsed) {
      // qualifiers that must travel with the member
      const memberType = (p.lead || baseType).replace(/^\s*static\s+/, '');
      move.push({
        name: p.name,
        member: `${memberType} ${p.stars}${p.name}${p.dims};`,
        init: p.init && !ZERO.test(p.init) ? p.init : null,
        line,
      });
    }
    decl.moved = true;
    byLine.get(line).decl = decl;
  }

  // ── THE ACCOUNTING INVARIANT ────────────────────────────────────────────────────────────────
  // Every static must be either MOVED or explicitly SKIPPED. Anything else has been silently
  // dropped: left as a file-scope static while the report claims the batch is done, which is the
  // one failure mode of a generator that no downstream gate can catch — refactor-guard stays green
  // because a variable that did not move cannot change the output. Four `beat_*`/`sound_bpm`
  // variables went missing exactly this way.
  const movedNames = new Set(move.map(m => m.name));
  const skippedNames = new Set(skipped.flatMap(s => s.names));
  const unaccounted = statics.filter(v => !movedNames.has(v.name) && !skippedNames.has(v.name));
  if (unaccounted.length) {
    console.error('\nctx-gen: ACCOUNTING FAILURE — ' + unaccounted.length + ' static(s) neither moved nor skipped.');
    console.error('These would be silently left behind while the report claims success:\n');
    for (const v of unaccounted) console.error('   line ' + String(v.line).padStart(5) + '  ' + v.name + '   ' + v.type);
    console.error('\nRefusing to continue. Fix the parser, or classify them.');
    process.exit(3);
  }

  return { move, skipped, byLine, src };
}

/* ─────────────────────────────────────────────────────────────── emitting ── */

/* The struct's array dimensions and designated initialisers use macros that sound.h defines
 * LATER in the file than the header can be included — `SOUND_VOICES`, `EMIT_DL_LEN`, 27 of them,
 * the deepest at line 5738. So they have to come along.
 *
 * Hoisting a #define is unconditionally safe, unlike hoisting a typedef: a macro body is just text
 * until it is expanded, so moving the definition earlier cannot break anything it references. The
 * closure is transitive because a macro body can name another macro. */
function collectMacroHoist(move, src) {
  // ── type definitions ────────────────────────────────────────────────────────────────────────
  // Members of type `Voice`, `ReverbTank`, … need those types complete BEFORE the struct. In
  // sound.h each type is defined immediately before the statics that use it, interleaved through
  // the whole file, so they have to be hoisted too — transitively, because a type's body names
  // other types and macros. Unlike a #define, moving a typedef earlier IS order-sensitive, which
  // is why the closure is computed rather than hand-listed: it pulls in five more types than the
  // obvious set (SoundReqKind, OctaveUp, SoundBiquad, ModState, GrainVoice).
  const typeBlock = new Map();
  for (let i = 0; i < src.length; i++) {
    const m = src[i].match(/\}\s*([A-Za-z_]\w*)\s*;/);
    if (!m || typeBlock.has(m[1])) continue;
    if (/^\s*typedef\b/.test(src[i])) { typeBlock.set(m[1], { first: i, last: i }); continue; }  // one-liner
    let s = -1;
    for (let j = i; j >= 0; j--) if (/^\s*typedef\s+(struct|union|enum)\b/.test(src[j])) { s = j; break; }
    if (s >= 0) typeBlock.set(m[1], { first: s, last: i });
  }

  const defLine = new Map();
  for (let i = 0; i < src.length; i++) {
    const m = src[i].match(/^\s*#\s*define\s+([A-Za-z_]\w*)/);
    if (m && !defLine.has(m[1])) {
      let last = i;
      while (/\\\s*$/.test(src[last]) && last + 1 < src.length) last++;   // line-continued macro
      defLine.set(m[1], { first: i, last });
    }
  }
  // Bare anonymous enums (`enum { AM_SNAP, AM_SHIFT };`) are constants with no dependencies, so
  // they hoist exactly as safely as a #define. A TYPEDEF'd enum is a type and belongs to the
  // type-hoist batch instead, so it is deliberately not matched here.
  for (let i = 0; i < src.length; i++) {
    const m = src[i].match(/^\s*enum\s*\{(.*)\}\s*;/);
    if (!m) continue;
    for (const id of (m[1].match(/[A-Za-z_]\w*/g) || [])) if (!defLine.has(id)) defLine.set(id, { first: i, last: i });
  }

  // one namespace for the closure: a needed identifier is either a macro/enum or a type
  for (const [name, span] of typeBlock) if (!defLine.has(name)) defLine.set(name, span);

  const want = new Set();
  const queue = [];
  const scan = (text) => { for (const id of (text.match(/[A-Za-z_]\w*/g) || [])) if (defLine.has(id)) queue.push(id); };
  for (const m of move) {
    scan(m.member);                                     // the WHOLE member: its type as well as its dims
    if (m.init) scan(m.init);
  }
  while (queue.length) {
    const id = queue.shift();
    if (want.has(id)) continue;
    want.add(id);
    const { first, last } = defLine.get(id);
    scan(src.slice(first, last + 1).join('\n').replace(/^\s*#\s*define\s+\w+/, ''));
  }
  // Dedupe by SPAN, not by name: several identifiers can share one line (an anonymous enum lists
  // all its members at once), and emitting that line per-identifier redefines it.
  const seen = new Set();
  const spans = [];
  for (const id of want) {
    const s = defLine.get(id);
    const key = s.first + ':' + s.last;
    if (seen.has(key)) continue;
    seen.add(key);
    spans.push({ id, ...s });
  }
  return spans.sort((a, b) => a.first - b.first);
}

function emitHeader(move, macroSpans = [], src = []) {
  const L = [];
  L.push('/* ' + CTX_HEADER + ' — GENERATED by tools/ctx-gen.js. Do not edit by hand.');
  L.push(' *');
  L.push(' * The per-instance engine context (docs/design/engine-context.md). An AUv3 puts every plug-in');
  L.push(' * instance in ONE process, so state that lives in file-scope statics is shared by every');
  L.push(' * instance — which is why two DAW tracks fight over one rack. These members used to be those');
  L.push(' * statics; the macro block at the bottom means the DSP code reads exactly as it did before.');
  L.push(' *');
  L.push(' * The default instance below is a `static` with DESIGNATED INITIALISERS, so every value is');
  L.push(' * still set at compile time exactly as it was, and the rest is still zeroed static storage.');
  L.push(' * That is what makes this a pure move with no init-order risk: nothing can read a member');
  L.push(' * before it is set, because the linker sets it. A real per-instance context is then created');
  L.push(' * by copying this template, which is exact by construction.');
  L.push(' */');
  L.push('#ifndef DE_SOUND_CTX_H');
  L.push('#define DE_SOUND_CTX_H');
  L.push('');
  L.push('/* self-contained: the members use these, and depending on where this header lands in');
  L.push(' * sound.h they may not have been pulled in yet. */');
  L.push('#include <stdint.h>');
  L.push('#include <stdbool.h>');
  L.push('#include <stdatomic.h>');
  L.push('');
  if (macroSpans.length) {
    L.push('/* MOVED here from sound.h: the sizes and constants the members below are written in.');
    L.push(' * They have to precede the struct, and moving a #define earlier is always safe because a');
    L.push(' * macro body is only text until it is expanded. */');
    for (const s of macroSpans) for (let i = s.first; i <= s.last; i++) L.push(src[i]);
    L.push('');
  }
  L.push('typedef struct {');
  for (const m of move) L.push('    ' + m.member);
  L.push('} ' + CTX_TYPE + ';');
  L.push('');
  const inits = move.filter(m => m.init);
  L.push('/* the compile-time defaults — one line per static that carried a non-zero initialiser */');
  L.push('static ' + CTX_TYPE + ' ' + CTX_PTR + '_default = {');
  for (const m of inits) L.push(`    .${m.name} = ${m.init},`);
  L.push('};');
  L.push('static ' + CTX_TYPE + ' *' + CTX_PTR + ' = &' + CTX_PTR + '_default;');
  L.push('');
  L.push('/* the access block: every name the engine already uses, pointed at the context.');
  L.push(' * Step B swaps ' + CTX_PTR + ' for a per-instance pointer; NOTHING below this line changes. */');
  const w = Math.max(...move.map(m => m.name.length));
  for (const m of move) L.push(`#define ${m.name.padEnd(w)} (${CTX_PTR}->${m.name})`);
  L.push('');
  L.push('#endif');
  L.push('');
  return L.join('\n');
}

function rewriteSource(src, byLine, movedNames, macroSpans = []) {
  const drop = new Set();
  for (const [line, vars] of byLine) {
    if (!vars.decl) continue;
    if (!vars.every(v => movedNames.has(v.name))) continue;
    for (let i = vars.decl.firstLine; i <= vars.decl.lastLine; i++) drop.add(i);
  }
  for (const s of macroSpans) for (let i = s.first; i <= s.last; i++) drop.add(i);   // they live in the header now
  // The include must land after the standard headers the members need, but at conditional depth
  // ZERO. Taking "the last #include in the first 200 lines" put it inside sound.h's
  // `#if defined(__SSE__)` block, so on arm64 it was never included at all and every moved name
  // became an undeclared identifier. Track #if/#endif depth and only accept a depth-0 include.
  // Which nesting level counts as "not inside a conditional" DIFFERS BY FILE: in a header wrapped in
  // an include guard the real includes sit at depth 1, in a .c file they sit at depth 0. Hardcoding
  // depth<=1 put studio.c's include inside its `#ifdef _WIN32` block, where it was never compiled.
  // So derive it: the base level is the MINIMUM depth at which any include appears.
  const seen = [];
  {
    let depth = 0;
    for (let i = 0; i < Math.min(src.length, 200); i++) {
      const s = src[i];
      if (/^\s*#\s*(if|ifdef|ifndef)\b/.test(s)) depth++;
      else if (/^\s*#\s*endif\b/.test(s)) depth--;
      else if (/^\s*#\s*include/.test(s)) seen.push({ i, depth });
    }
  }
  const base = seen.length ? Math.min(...seen.map(s => s.depth)) : 0;
  const atBase = seen.filter(s => s.depth === base);
  const includeAt = atBase.length ? atBase[atBase.length - 1].i + 1 : 0;

  const out = [];
  for (let i = 0; i < src.length; i++) {
    if (i === includeAt) {
      out.push('#include "' + CTX_HEADER + '"   // GENERATED per-instance context (tools/ctx-gen.js)');
    }
    if (drop.has(i)) continue;
    out.push(src[i]);
  }
  return out.join('\n');
}

/* ──────────────────────────────────────────────────────────────── actions ── */

function apply(dir, res) {
  const movedNames = new Set(res.move.map(m => m.name));
  const macroSpans = collectMacroHoist(res.move, res.src);
  fs.writeFileSync(path.join(dir, CTX_HEADER), emitHeader(res.move, macroSpans, res.src));
  fs.writeFileSync(path.join(dir, path.basename(TARGET)), rewriteSource(res.src, res.byLine, movedNames, macroSpans));
  return macroSpans.length;
}

function probe(res) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ctxgen-'));
  const rt = path.join(dir, 'runtime');
  execFileSync('cp', ['-r', path.join(ROOT, 'runtime'), rt]);
  apply(rt, res);
  console.log('  generated engine at ' + rt);

  // Compile EVERY configuration, not just the default one. The AST is dumped without -DDE_TRACE,
  // so any static inside `#ifdef DE_TRACE` is invisible to the generator — and DE_TRACE is exactly
  // what play.js (and therefore refactor-guard) builds with. A batch that compiles here and fails
  // there would look like the refactor broke the harness rather than the engine.
  const CONFIGS = [
    { name: 'plain',           extra: [] },
    { name: 'DE_TRACE',        extra: ['-DDE_TRACE'] },
    { name: 'DE_SPEC',         extra: ['-DDE_SPEC'] },
    { name: 'DE_TRACE+SPEC',   extra: ['-DDE_TRACE', '-DDE_SPEC'] },
  ];
  // ⚠ COMPILE THE COPY'S studio.c, NOT the repo's. A quoted `#include "sound.h"` resolves relative
  // to the INCLUDING FILE's own directory before any -I path is consulted, so compiling
  // runtime/studio.c picks up runtime/sound.h no matter what -I says. This probe did exactly that
  // and reported "ok" four times for a build that never touched the generated file — a green that
  // meant nothing, the same failure shape as a sed that silently fails to match.
  const studio = path.join(rt, 'studio.c');

  // and prove it: a sentinel that only exists in the generated header must be reachable
  const sentinel = '#error DE_CTX_PROBE_REACHED_GENERATED_HEADER';
  fs.appendFileSync(path.join(rt, CTX_HEADER), '\n#ifdef DE_CTX_PROBE_SENTINEL\n' + sentinel + '\n#endif\n');
  let reached = false;
  try {
    execFileSync('clang', ['-fsyntax-only', studio, '-I', rt, '-I', path.join(ROOT, 'runtime'), '-I', path.join(ROOT, 'build'),
      '-DDE_NO_RAYLIB', '-DDE_CTX_PROBE_SENTINEL', '-DSCREEN_W=320', '-DSCREEN_H=200', '-DSCALE=2',
      '-DMAP_W=128', '-DMAP_H=64', '-DCELL_W=16', '-DCELL_H=16'], { cwd: ROOT, stdio: 'pipe' });
  } catch (e) {
    reached = /DE_CTX_PROBE_REACHED_GENERATED_HEADER/.test((e.stderr || Buffer.from('')).toString());
  }
  console.log('  probe reaches the generated header … ' + (reached ? 'yes' : 'NO — this probe proves NOTHING'));
  if (!reached) return false;

  let allOk = true;
  for (const cfg of CONFIGS) {
    process.stdout.write('  compiling (' + cfg.name + ') … ');
    try {
      execFileSync('clang', ['-fsyntax-only', studio, '-I', rt, '-I', path.join(ROOT, 'runtime'), '-I', path.join(ROOT, 'build'),
        '-DDE_NO_RAYLIB', '-DSCREEN_W=320', '-DSCREEN_H=200', '-DSCALE=2',
        '-DMAP_W=128', '-DMAP_H=64', '-DCELL_W=16', '-DCELL_H=16', ...cfg.extra], { cwd: ROOT, stdio: 'pipe' });
      console.log('ok');
    } catch (e) {
      allOk = false;
      console.log('FAILED');
      console.log((e.stderr || Buffer.from('')).toString().split('\n').slice(0, 20).join('\n'));
    }
  }
  console.log('  (generated engine at ' + rt + ')');
  return allOk;
}

function report(res, opts) {
  console.log(`\nCONTEXT GENERATION PLAN for ${TARGET}${opts.primitive ? '  [batch: primitive types only]' : ''}\n`);
  console.log(`  would move   ${res.move.length} variables into ${CTX_TYPE}`);
  console.log(`  of those     ${res.move.filter(m => m.init).length} carry a non-zero initialiser`);
  console.log(`  skipped      ${res.skipped.length} declaration lines\n`);
  const byWhy = {};
  for (const s of res.skipped) {
    const k = s.why.replace(/\(.*\)/, '').trim();
    (byWhy[k] = byWhy[k] || []).push(s);
  }
  for (const [why, list] of Object.entries(byWhy)) {
    console.log(`  ${list.length} skipped — ${why}`);
    for (const s of list.slice(0, 8)) console.log(`      line ${String(s.line).padStart(5)}  ${s.names.join(' ')}`);
    if (list.length > 8) console.log(`      … and ${list.length - 8} more`);
  }
  console.log('');
}

/* ───────────────────────────────────────────────────────────── self-test ── */

function selfCheck() {
  const checks = [];
  const t = (n, f) => { let ok = false; try { ok = !!f(); } catch (_) {} checks.push([n, ok]); };

  t('splits top-level commas only',   () => splitDeclarators('float a, b[2][3], c').length === 3);
  t('does not split inside braces',   () => splitDeclarators('int x = {1, 2}, y').length === 2);
  t('does not split inside brackets', () => splitDeclarators('float a[F(1,2)], b').length === 2);

  const known = new Set(['echo_buf', 'wavcap_buf', 'drop_lpL']);
  t('parses an array declarator',     () => { const p = parseDeclarator('float  echo_buf[88200]', known); return p.name === 'echo_buf' && p.dims === '[88200]' && p.lead === 'float'; });
  t('parses a pointer declarator',    () => { const p = parseDeclarator('float *wavcap_buf = NULL', known); return p.name === 'wavcap_buf' && p.stars === '*' && p.init === 'NULL'; });
  t('parses a 2D array',              () => { const p = parseDeclarator('float echo_buf[8][2048]', known); return p.dims === '[8][2048]'; });
  t('refuses an unknown name',        () => parseDeclarator('float mystery = 1', known) === null);

  t('zero initialiser detected',      () => ZERO.test('0.0f') && ZERO.test('NULL') && ZERO.test('false') && ZERO.test('{0}'));
  t('non-zero initialiser detected',  () => !ZERO.test('0.35f') && !ZERO.test('PAN_LINEAR'));

  t('strips a trailing comment',      () => stripComment('static int x = 1;  // hi').trim() === 'static int x = 1;');

  // the emitter: a member, its designated initialiser, and its macro must agree on the name
  const hdr = emitHeader([{ name: 'echo_fb', member: 'float echo_fb;', init: '0.35f' },
                          { name: 'echo_lp', member: 'float echo_lp;', init: null }]);
  t('emits the member',               () => /float echo_fb;/.test(hdr));
  t('emits the designated init',      () => /\.echo_fb = 0\.35f,/.test(hdr));
  t('omits an init for zero members', () => !/\.echo_lp/.test(hdr));
  t('emits the access macro',         () => new RegExp('#define echo_fb\\s+\\(' + CTX_PTR + '->echo_fb\\)').test(hdr));
  t('the default instance is static', () => new RegExp('static ' + CTX_TYPE + ' ' + CTX_PTR + '_default = \\{').test(hdr));

  const pass = checks.filter(c => c[1]).length;
  for (const [n, ok] of checks) if (!ok) console.log('  ✗ ' + n);
  console.log(`ctx-gen --check: ${pass}/${checks.length} known answers correct`);
  process.exit(pass === checks.length ? 0 : 1);
}

/* ───────────────────────────────────────────────────────────────── main ── */

const argv = process.argv.slice(2);
const opts = { primitive: argv.includes('--primitive'), write: argv.includes('--write'), probe: argv.includes('--probe') };
if (argv.includes('--check')) selfCheck();
else {
  const res = plan(opts);
  report(res, opts);
  if (opts.probe) process.exit(probe(res) ? 0 : 1);
  if (opts.write) {
    apply(path.join(ROOT, 'runtime'), res);
    console.log('  written. NOW RUN: node tools/refactor-guard.js');
  } else {
    console.log('  dry run. --probe to compile it on a copy, --write to apply.');
  }
}
