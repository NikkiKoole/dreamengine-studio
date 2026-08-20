#!/usr/bin/env node
// gen-rhythmbox.js — GENERATE runtime/rhythmbox.h from docs/design/rhythm-box-patterns.md.
//
// The DOC is the source of truth (it carries the provenance, the page/figure numbers and the
// per-rhythm confidence); the header is a derived view for carts. Never hand-edit the header.
//
//   node tools/gen-rhythmbox.js            # write runtime/rhythmbox.h
//   node tools/gen-rhythmbox.js --check    # exit nonzero if the header is stale (CI / repo-doctor)
//   node tools/gen-rhythmbox.js --dry      # parse + report, write nothing
//
// It REFUSES to guess: any line inside a pattern block that it cannot parse with certainty is
// reported and the run fails, because a silently dropped lane is a lie about a sourced dataset.
const fs = require('fs'), path = require('path');
const DOC = path.join(__dirname, '..', 'docs', 'design', 'rhythm-box-patterns.md');
const OUT = path.join(__dirname, '..', 'runtime', 'rhythmbox.h');

const MACHINES = [
  { id: 'RB_FR2L',   sect: '### 8.1', counts: 48, per_bar: 24, per_beat: 6, bars: 2,
    tag: 'FR2L',  human: 'Ace Tone Rhythm Ace FR-2L (c.1969)' },
  { id: 'RB_TR77',   sect: '### 8.2', counts: 16, per_bar: 16, per_beat: 4, bars: 1,
    tag: 'TR77',  human: 'Roland Rhythm TR-77 (1972), 16 ruled columns of a 32-state bar' },
  { id: 'RB_SGS',    sect: '### 8.3', counts: 32, per_bar: 32, per_beat: 8, bars: 1,
    tag: 'SGS',   human: 'SGS M252 rhythm LSI, factory masks AA and AD' },
];

// ── per-rhythm subdivision overrides ─────────────────────────────────────
// The chart gives DOTS; how a rhythm divides the clock is stated in the doc's prose, so it
// cannot be parsed out of a block. Only rhythms the doc states EXPLICITLY appear here; every
// other rhythm keeps its machine default. Each entry cites where the claim comes from.
const SUBDIV = {
  // doc §3.1: "the FR-2L waltz splits the same 24-count bar by three" → 8-count beats
  'RB_FR2L/Waltz':      { per_bar: 24, per_beat: 8, bars: 2 },
  // doc §3.1 + §8.1 Slow Rock: the whole 48 counts are ONE 12/8 bar, triplet = 4 counts
  'RB_FR2L/Slow Rock':  { per_bar: 48, per_beat: 12, bars: 1 },
  // doc §8.2: hatching leaves 12 slots = two bars of 6/8 (6/8 MARCH), or 6 slots per bar in 3/4
  'RB_TR77/6/8 MARCH':  { per_bar: 8, per_beat: 4, bars: 2 },
  'RB_TR77/JAZZ WALTZ': { per_bar: 8, per_beat: 4, bars: 2 },
  'RB_TR77/WALTZ':      { per_bar: 8, per_beat: 4, bars: 2 },
  // doc §8.2 SLOW ROCK: deleting the hatched columns gives a 12-slot 12/8, so a beat is 4
  // columns of which one is hatched (3 surviving triplets)
  'RB_TR77/SLOW ROCK':  { per_bar: 16, per_beat: 4, bars: 1 },
  'RB_TR77/BALLAD':     { per_bar: 16, per_beat: 4, bars: 1 },
  // BOLERO is hatched too but the doc says its marks are NOT on a triplet grid (a 3:1 shuffled
  // sixteenth grid), so it deliberately gets no override: read its `unused` mask instead.
};

const doc = fs.readFileSync(DOC, 'utf8');
const problems = [];

function section(mark, next) {
  const a = doc.indexOf(mark); if (a < 0) throw new Error('section not found: ' + mark);
  const b = next ? doc.indexOf(next, a) : doc.length;
  return doc.slice(a, b < 0 ? doc.length : b);
}
function blocks(text) {
  const out = [];
  const re = /^\*\*(.+?)\*\*[^\n]*\n```\n([\s\S]*?)```/gm;
  let m; while ((m = re.exec(text))) out.push({ name: m[1].trim(), body: m[2] });
  return out;
}
// ── the doc uses FIVE lane notations; each is parsed explicitly, and anything that
// matches none of them is REPORTED rather than skipped (a dropped lane would be a lie).
//   A  grid      "Cy'   code 5   x..x..  x..x.."      1-2 runs of [x./?:]
//   B  cells     "CY    x  ?  x  /  .  .  .  /"       single chars, space separated
//   C  states    "SD    trig 31  states 0,3,4,7"      a comma list of state indices
//   D  gate      "GU    trig 6   SUSTAINED BARS"      held, extent in prose
//   E  skip      rulers ("count:", "cols"), prose continuations
// a label may be two tokens ("L1  M", "OUT 1", "(row 2)"), but the second token must not be a
// bare pattern character, or "CY  x  ?  x" would read as a label of "CY x" and lose a cell.
const LABEL = String.raw`([A-Za-z(\[][A-Za-z0-9'+?()\[\]\-]*(?:\s+(?![x.\/?:]\s)(?:[0-9][0-9)]*|[A-Za-z][A-Za-z0-9'+?)\-]*))?)`;
const CODE  = String.raw`(?:(?:code|trig)\s+([0-9][0-9'+ ]*(?:\([^)]*\))?[0-9'+K ]*|[0-9]+\?|\?)\s+)?`;
const RE_A  = new RegExp('^\\s*' + LABEL + '\\s+' + CODE + '([x.\\/?:]{12,32})(?:\\s\\s+([x.\\/?:]{12,32}))?\\s*(.*)$');
const RE_B  = new RegExp('^\\s*' + LABEL + '\\s+((?:[x.\\/?:]\\s+){7,}[x.\\/?:])\\s*$');
const RE_C  = new RegExp('^\\s*' + LABEL + '\\s+' + CODE + 'states\\s+([0-9,\\s]+)(.*)$');
const SKIP  = /^\s*(count:|cols?\b|col\s+0|which |know |two share|states\s)/i;

function bits(spec, marks) {
  let mask = 0n, maybe = 0n, unused = 0n, off = 0n;
  for (let i = 0; i < spec.length; i++) {
    const c = spec[i];
    if (c === 'x') mask |= 1n << BigInt(i);
    else if (c === '?') maybe |= 1n << BigInt(i);
    else if (c === '/') unused |= 1n << BigInt(i);
    else if (c === ':') off |= 1n << BigInt(i);
  }
  return { mask, maybe, unused, off, len: spec.length };
}
function parseBlock(b, mach) {
  const lanes = [];
  for (const raw of b.body.split('\n')) {
    const line = raw.replace(/\s+$/, '');
    if (!line.trim() || SKIP.test(line)) continue;
    let m;
    if ((m = RE_A.exec(line))) {
      const pat = m[4] ? (m[3] + m[4]) : m[3];
      const bb = bits(pat);
      lanes.push({ label: m[1].replace(/\s+/g, ' '), code: (m[2] || '').trim(), kind: 'RB_HIT',
                   ...bb, partial: /UNREAD/i.test(m[5] || '') });
      continue;
    }
    if ((m = RE_C.exec(line))) {                       // a comma list of state indices
      const idx = m[3].split(/[,\s]+/).filter(x => x !== '').map(Number);
      let mask = 0n; for (const i of idx) mask |= 1n << BigInt(i);
      lanes.push({ label: m[1].replace(/\s+/g, ' '), code: (m[2] || '').trim(), kind: 'RB_HIT',
                   mask, maybe: 0n, unused: 0n, off: 0n, len: 32, partial: false, states: true });
      continue;
    }
    if ((m = RE_B.exec(line))) {                       // space-separated single cells
      const spec = m[2].replace(/\s+/g, '');
      const bb = bits(spec);
      lanes.push({ label: m[1].replace(/\s+/g, ' '), code: '', kind: 'RB_HIT', ...bb, partial: false });
      continue;
    }
    if (/^\s*\[[^\]]+\].*\(([x.\/?:]{12,32})/.test(line)) {   // a real lane, unlabelled on the page
      const spec = /\(([x.\/?:]{12,32})/.exec(line)[1];
      lanes.push({ label: '[unlabelled]', code: '', kind: 'RB_HIT', ...bits(spec), partial: false });
      continue;
    }
    if (/\b(GATE|SUSTAINED)\b/i.test(line)) {          // held lane, extent given in prose
      const lab = line.trim().split(/\s+/)[0];
      const code = (/(?:code|trig)\s+([0-9][0-9'+()K ]*)/.exec(line) || [, ''])[1].trim();
      lanes.push({ label: lab, code, kind: 'RB_GATE', mask: 0n, maybe: 0n, unused: 0n, off: 0n,
                   len: 0, partial: true });
      continue;
    }
    // prose continuation: no pattern run at all, and long enough to be a sentence
    if (!/[x.]{8,}/.test(line) && line.trim().length > 24) continue;
    problems.push(`${mach.tag} ${b.name}: unparsed line: ${line.trim()}`);
  }
  if (!lanes.length) problems.push(`${mach.tag} ${b.name}: no lanes parsed`);
  return lanes;
}

const sets = [];
for (let i = 0; i < MACHINES.length; i++) {
  const mach = MACHINES[i];
  const text = section(mach.sect, MACHINES[i + 1] ? MACHINES[i + 1].sect : '\n## 9.');
  const bs = blocks(text);
  const rhythms = bs.map(b => {
    const lanes = parseBlock(b, mach);
    const lens = [...new Set(lanes.filter(l => l.len && !l.states).map(l => l.len))];
    if (lens.length > 1) {
      problems.push(`${mach.tag} ${b.name}: mixed lane lengths ${lens.join(',')}`);
      if (process.env.DEBUG) for (const l of lanes) console.error(`    dbg ${b.name} [${l.label}] len=${l.len} code=[${l.code}]`);
    }
    let unused = 0n; for (const l of lanes) unused |= (l.unused || 0n);
    const d = SUBDIV[`${mach.id}/${b.name}`] || {};
    const counts = lens[0] || mach.counts;
    // SGS: a reset count below 32 is programmed by crossing the 8TH INSTRUMENT column, which then
    // carries the reset (databook: "the column which now represents the reset output, rather than
    // the 8th instrument"). So that lane is a counter, not a drum. Verified: 7 of 7 short rhythms
    // have an empty OUT 8, and no rhythm marks it.
    if (mach.tag === 'SGS' && counts < 32)
      for (const l of lanes) if (l.label === 'OUT 8' && l.mask === 0n) l.resetcol = true;
    return { name: b.name, lanes, counts, unused,
             per_bar_out:  d.per_bar  ?? (mach.tag === 'FR2L' ? 24 : counts),
             per_beat_out: d.per_beat ?? mach.per_beat,
             bars_out:     d.bars     ?? (mach.tag === 'FR2L' ? 2 : 1) };
  });
  sets.push({ mach, rhythms });
}

// ── known-answer selfcheck ────────────────────────────────────────────────
// Asserts the PARSE against facts stated independently in the doc's prose, so a
// silently-wrong regex cannot ship. Every expectation here is a sourced finding.
function selfcheck(sets) {
  const find = (id, re) => {
    const s = sets.find(s => s.mach.id === id);
    const r = s.rhythms.find(r => re.test(r.name));
    if (!r) throw new Error(`selfcheck: no ${id} rhythm matching ${re}`);
    return r;
  };
  const lane = (r, lab) => {
    const l = r.lanes.find(l => l.label === lab);
    if (!l) throw new Error(`selfcheck: ${r.name} has no lane "${lab}" (has: ${r.lanes.map(x=>x.label).join(', ')})`);
    return l;
  };
  const on = l => { const o = []; for (let i = 0; i < 64; i++) if ((l.mask >> BigInt(i)) & 1n) o.push(i); return o; };
  const eq = (name, got, want) => {
    const a = JSON.stringify(got), b = JSON.stringify(want);
    if (a !== b) { console.error(`  ✗ ${name}\n      got  ${a}\n      want ${b}`); return 0; }
    console.log(`  ✓ ${name}`); return 1;
  };
  let pass = 0, total = 0;
  const T = (name, got, want) => { total++; pass += eq(name, got, want); };

  // FR-2L, doc §7: the bossa clave is asymmetric across the two bars
  T('FR2L Bossanova clave lane = 0,9,18 | 30,39',
    on(lane(find('RB_FR2L', /^Bossanova$/), 'Cb+Hb')), [0, 9, 18, 30, 39]);
  // FR-2L, doc §3.3: samba's maracas skip count 0 but hit 24 — the whole two-bar asymmetry
  T('FR2L Samba maracas skip 0, hit 24',
    (() => { const o = on(lane(find('RB_FR2L', /^SAMBA$/), 'M')); return [o.includes(0), o.includes(24)]; })(),
    [false, true]);
  // FR-2L, doc §3.2: Rock'n Roll has two bass lanes, one every beat and one on beat 1 only
  T("FR2L Rock'n Roll Bd = beat 1 of each bar only",
    on(lane(find('RB_FR2L', /Rock'n Roll/), 'Bd')), [0, 24]);
  T("FR2L Rock'n Roll Bd' = every beat",
    on(lane(find('RB_FR2L', /Rock'n Roll/), "Bd'")), [0, 6, 12, 18, 24, 30, 36, 42]);
  // FR-2L, doc §3.4: the shuffle's offbeat is the THIRD triplet (+4 of 6)
  T('FR2L Shuffle Sd offbeats all at +4 within the beat',
    [...new Set(on(lane(find('RB_FR2L', /^Shuffle$/), 'Sd')).map(c => c % 6))], [4]);
  // FR-2L, doc §3.3: one shared fill pulse late in bar 2, in three different rhythms
  T('FR2L fill pulse on count 40 in Dixieland, Fox Trot and Swing',
    ['Dixieland', 'Fox Trot', 'Swing'].map(n => {
      const r = find('RB_FR2L', new RegExp('^' + n.replace(' ', ' ') + '$'));
      return r.lanes.some(l => (l.mask >> 40n) & 1n);
    }), [true, true, true]);
  // FR-2L, doc §2: counts congruent to 1 or 5 (mod 6) are UNRULED and can never carry a mark
  T('FR2L no rhythm marks an unruled count (1 or 5 mod 6)',
    sets.find(s => s.mach.id === 'RB_FR2L').rhythms.flatMap(r =>
      r.lanes.flatMap(l => on(l).filter(c => c % 6 === 1 || c % 6 === 5))), []);
  // TR-77, doc §7: the rumba clave, from a different machine and document
  T('TR77 Rhumba C lane = rumba clave 0,3,6,10,12',
    on(lane(find('RB_TR77', /^RHUMBA$/), 'C')), [0, 3, 6, 10, 12]);
  // TR-77, doc §3.1: hatched columns are states the rhythm skips, so nothing may fall there
  T('TR77 no mark ever falls on a hatched column',
    sets.find(s => s.mach.id === 'RB_TR77').rhythms.flatMap(r =>
      r.lanes.flatMap(l => on(l).filter(c => (r.unused >> BigInt(c)) & 1n))), []);
  T('TR77 Slow Rock hatches every 4th column',
    (() => { const r = find('RB_TR77', /^SLOW ROCK$/); const o = [];
             for (let i = 0; i < 16; i++) if ((r.unused >> BigInt(i)) & 1n) o.push(i); return o; })(),
    [3, 7, 11, 15]);
  // SGS: the databook's own footnote says OUT 2 is used by every rhythm except Tango
  T('SGS mask AD: OUT 2 empty in TANGO, non-empty in the other 14',
    sets.find(s => s.mach.id === 'RB_SGS').rhythms.filter(r => /M252 AD/.test(r.name))
      .filter(r => { const l = r.lanes.find(l => l.label === 'OUT 2'); return !l || l.mask === 0n; })
      .map(r => r.name.replace(/.*RHYTHM \d+ \(|\)$/g, '')), ['TANGO']);

  // the subdivision overrides: a waltz stamped with a 6-count beat was a real bug in v1
  const sub = (id, re) => { const r = find(id, re); return [r.per_bar_out, r.per_beat_out, r.bars_out]; };
  T('FR2L Waltz divides its bar by THREE (8-count beats)', sub('RB_FR2L', /^Waltz$/), [24, 8, 2]);
  T('FR2L Slow Rock is ONE 12/8 bar of 48 counts', sub('RB_FR2L', /^Slow Rock$/), [48, 12, 1]);
  T('FR2L Cha-Cha keeps the 4/4 default', sub('RB_FR2L', /^Cha-Cha$/), [24, 6, 2]);
  // the databook's reset rule, checked in BOTH directions
  const sgs = sets.find(s => s.mach.id === 'RB_SGS').rhythms;
  T('SGS every rhythm shorter than 32 states flags OUT 8 as the reset column',
    sgs.filter(r => r.counts < 32).map(r => !!r.lanes.find(l => l.label === 'OUT 8' && l.resetcol)),
    sgs.filter(r => r.counts < 32).map(() => true));
  T('SGS no 32-state rhythm flags a reset column',
    sgs.filter(r => r.counts === 32).some(r => r.lanes.some(l => l.resetcol)), false);
  T('SGS short-rhythm count is 7 (the datasheet claim has no exceptions)',
    sgs.filter(r => r.counts < 32).length, 7);
  // the SGS edge rule: it must bite on SGS and be a no-op on the other two machines
  const runs = set => set.rhythms.reduce((n, r) => n + r.lanes.reduce((m, l) => {
    let c = 0, prev = 0;
    for (let i = 0; i < r.counts; i++) { const b = Number((l.mask >> BigInt(i)) & 1n);
      if (b && prev) c++; prev = b; }
    return m + c; }, 0), 0);
  T('SGS data really does contain adjacent marks (else the edge rule is untested)',
    runs(sets.find(s => s.mach.id === 'RB_SGS')) > 20, true);
  T('FR2L has almost none, so flagging it edge-only would be wrong',
    runs(sets.find(s => s.mach.id === 'RB_FR2L')) < 6, true);
  console.log(`\n${pass}/${total} known answers`);
  return pass === total;
}

const nRhythm = sets.reduce((n, s) => n + s.rhythms.length, 0);
const nLane   = sets.reduce((n, s) => n + s.rhythms.reduce((m, r) => m + r.lanes.length, 0), 0);
if (problems.length) {
  console.error(`gen-rhythmbox: ${problems.length} problem(s) — refusing to write a partial header`);
  for (const p of problems) console.error('  ' + p);
  process.exit(2);
}

const hex = v => '0x' + v.toString(16) + 'ULL';
const esc = s => s.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
const L = [];
L.push('// rhythmbox.h — GENERATED by tools/gen-rhythmbox.js from');
L.push('// docs/design/rhythm-box-patterns.md. DO NOT HAND-EDIT: edit the doc, then regenerate.');
L.push('//');
L.push(`// ${nRhythm} preset rhythms and ${nLane} lanes, read off three manufacturers' own documents`);
L.push('// (service manuals and a 1979 databook). The doc carries the provenance, the page and');
L.push('// figure numbers, the per-rhythm confidence markers and the caveats; this header carries');
L.push('// only the bits. Anything you want to CLAIM about this data, check in the doc first.');
L.push('//');
L.push('// A lane is a 48-bit mask over COUNTS, so one rhythm is a handful of integers and the whole');
L.push('// library costs a few KB of rodata.');
L.push('//');
L.push('// THREE THINGS THAT ARE NOT OPTIONAL if you play this data (all measured, see the doc):');
L.push('//  1. counts_per_beat and bars are PER RHYTHM, not per machine. The FR-2L waltz divides its');
L.push('//     24-count bar by THREE (8-count beats) while its 4/4 rhythms divide by four, and its');
L.push('//     slow rock reads all 48 counts as ONE 12/8 bar. Read rb->per_beat, never assume.');
L.push('//  2. `unused` marks counts a rhythm SKIPS (the TR-77 hatches them on the page). A hit can');
L.push('//     never fall there; stepping over them is what gives that rhythm its meter.');
L.push('//  3. RB_GATE lanes are HELD, not struck (a brush swish, a guiro). Firing a one-shot for a');
L.push('//     gate lane is wrong. Lanes flagged RB_PARTIAL have a region the source left UNREAD.');
L.push('//  4. A lane flagged RB_RESETCOL is NOT AN INSTRUMENT: on the SGS chip a rhythm shorter than');
L.push('//     32 states is made by crossing its 8th instrument column, which then carries the reset');
L.push('//     instead. The datasheet says so and the data agrees in all 7 cases (0 exceptions), so');
L.push('//     every short SGS rhythm has SEVEN instruments, not eight. Skip these lanes when you');
L.push('//     lay out voices, or you will wire a drum to a counter.');
L.push('//');
L.push('// LANE LABELS ARE PROVISIONAL on the FR-2L: its scan is ~75 dpi and the letterforms are');
L.push('// interpolation, so lane ORDER is reliable and the letters are not. Assign voices by order');
L.push('// and taste; do not claim which voice plays which lane. The SGS lanes are chip PINS, and');
L.push('// its two masks map pins to instruments DIFFERENTLY (doc §4).');
L.push('#ifndef RHYTHMBOX_H');
L.push('#define RHYTHMBOX_H');
L.push('');
L.push('#define RB_HIT     0   // struck: fire a one-shot on each set bit');
L.push('#define RB_GATE    1   // held: the set bits are the counts the sound is SOUNDING');
L.push('#define RB_PARTIAL  1   // lane flag: part of this lane was UNREAD in the source');
L.push('#define RB_RESETCOL 2   // lane flag: NOT an instrument — this column carries the RESET');
L.push('');
L.push('// rhythm flag. SGS only: the chip triggers on the RISING edge of a ROM bit, so two marks on');
L.push('// consecutive states do NOT sound twice — the line goes high and STAYS high, and the second');
L.push('// edge never happens (Elektor, April 1976, p420). 36 runs of adjacent marks exist in the SGS');
L.push('// tables, the longest 6 states, so this is not a corner case: use rb_trigger(), not rb_hit().');
L.push('// The FR-2L and TR-77 are different machines (discrete pulse trains, a diode matrix) and the');
L.push('// rule is NOT transferable to them, so their rhythms do not carry this flag.');
L.push('#define RB_EDGE_ONLY 1');
L.push('');
L.push('typedef struct {');
L.push('    const char        *label;   // as printed (provisional on the FR-2L, see above)');
L.push('    const char        *code;    // the machine\'s own pulse-train / trigger number, verbatim');
L.push('    unsigned char      kind;    // RB_HIT or RB_GATE');
L.push('    unsigned char      flags;   // RB_PARTIAL, or 0');
L.push('    unsigned long long mask;    // bit i set = a mark on count i');
L.push('    unsigned long long maybe;   // bit i set = the source read this cell as UNCERTAIN');
L.push('} RbLane;');
L.push('');
L.push('typedef struct {');
L.push('    const char        *name;');
L.push('    const char        *machine;');
L.push('    unsigned char      counts;    // counts in one pass of the pattern');
L.push('    unsigned char      per_bar;');
L.push('    unsigned char      per_beat;  // PER RHYTHM (see note 1 above)');
L.push('    unsigned char      bars;');
L.push('    unsigned char      nlanes;');
L.push('    unsigned char      flags;     // RB_EDGE_ONLY, or 0');
L.push('    unsigned long long unused;    // counts this rhythm skips (see note 2 above)');
L.push('    const RbLane      *lane;');
L.push('} RbRhythm;');
L.push('');
for (const { mach, rhythms } of sets) {
  L.push(`// ── ${mach.human} ──`);
  rhythms.forEach((r, i) => {
    L.push(`static const RbLane ${mach.id}_L${i}[] = {`);
    for (const l of r.lanes) {
      const fl = [l.partial ? 'RB_PARTIAL' : null, l.resetcol ? 'RB_RESETCOL' : null].filter(Boolean).join('|') || '0';
      L.push(`    { "${esc(l.label)}", "${esc(l.code)}", ${l.kind}, ${fl}, ` +
             `${hex(l.mask)}, ${hex(l.maybe || 0n)} },`);
    }
    L.push('};');
  });
  L.push(`static const RbRhythm ${mach.id}[] = {`);
  rhythms.forEach((r, i) => {
    const d = SUBDIV[`${mach.id}/${r.name}`] || {};
    const perBar  = d.per_bar  ?? (mach.tag === 'FR2L' ? 24 : r.counts);
    const perBeat = d.per_beat ?? mach.per_beat;
    const bars    = d.bars     ?? (mach.tag === 'FR2L' ? 2 : 1);
    const rflags = mach.tag === 'SGS' ? 'RB_EDGE_ONLY' : '0';
    L.push(`    { "${esc(r.name)}", "${mach.tag}", ${r.counts}, ${perBar}, ${perBeat}, ${bars}, ` +
           `${r.lanes.length}, ${rflags}, ${hex(r.unused)}, ${mach.id}_L${i} },`);
  });
  L.push('};');
  L.push(`#define ${mach.id}_N ${rhythms.length}`);
  L.push('');
}
L.push('// ── reading a rhythm ──────────────────────────────────────────────────────');
L.push('static inline int rb_hit(const RbRhythm *r, int lane, int count) {');
L.push('    return (int)((r->lane[lane].mask >> (count % r->counts)) & 1ULL);');
L.push('}');
L.push('// THE ONE YOU WANT IN A SEQUENCER: does this lane FIRE on this count?');
L.push('// Identical to rb_hit() except on an RB_EDGE_ONLY rhythm, where a mark whose predecessor is');
L.push('// also marked is already sounding and cannot retrigger.');
L.push('static inline int rb_trigger(const RbRhythm *r, int lane, int count) {');
L.push('    if (!rb_hit(r, lane, count)) return 0;');
L.push('    if (!(r->flags & RB_EDGE_ONLY)) return 1;');
L.push('    return !rb_hit(r, lane, (count + r->counts - 1) % r->counts);');
L.push('}');
L.push('static inline int rb_uncertain(const RbRhythm *r, int lane, int count) {');
L.push('    return (int)((r->lane[lane].maybe >> (count % r->counts)) & 1ULL);');
L.push('}');
L.push('static inline int rb_used(const RbRhythm *r, int count) {   // false = a skipped count');
L.push('    return (int)(((~r->unused) >> (count % r->counts)) & 1ULL);');
L.push('}');
L.push('static inline int rb_beat_of(const RbRhythm *r, int count) {');
L.push('    return (count % r->per_bar) / (r->per_beat ? r->per_beat : 1);');
L.push('}');
L.push('');
L.push('#endif // RHYTHMBOX_H');
const text = L.join('\n') + '\n';

if (process.argv.includes('--selfcheck')) process.exit(selfcheck(sets) ? 0 : 1);
if (process.argv.includes('--dry')) {
  console.log(`parsed ${nRhythm} rhythms, ${nLane} lanes, 0 problems`);
  for (const { mach, rhythms } of sets)
    console.log(`  ${mach.tag}: ${rhythms.length} rhythms — ${rhythms.map(r => r.name).join(', ')}`);
  process.exit(0);
}
if (process.argv.includes('--check')) {
  const cur = fs.existsSync(OUT) ? fs.readFileSync(OUT, 'utf8') : '';
  if (cur !== text) { console.error('runtime/rhythmbox.h is STALE — run: node tools/gen-rhythmbox.js'); process.exit(1); }
  console.log('runtime/rhythmbox.h is up to date'); process.exit(0);
}
fs.writeFileSync(OUT, text);
console.log(`wrote runtime/rhythmbox.h — ${nRhythm} rhythms, ${nLane} lanes`);
