#!/usr/bin/env node
// pro-check.js — THE GATE for the Pro entitlement seam (runtime/pro.h + ios/Sources/Entitlements.swift).
//
//   node tools/pro-check.js            # run it
//   node tools/pro-check.js --quiet    # PASS/FAIL only, exits nonzero on failure (repo-doctor row)
//   node tools/pro-check.js --selfcheck  # known answers for the yml parser (builds nothing)
//
// WHY THIS EXISTS, and why a green build says nothing. `pro.h` links the Store bridge as WEAK
// DEFINITIONS so a build with no store still links — the editor, the web gallery, a bare play.js
// run. Those stubs answer UNLOCKED. That is right where there is nothing to sell and
// CATASTROPHIC where there is: a target that has a paywall but forgets to link the real answer
// compiles, runs, looks perfect, and hands Pro to everyone. Nothing about it is visible from
// outside. Until 2026-08-19 that was exactly the state of every AUv3 target — none of the three
// project ymls compiled Store.swift OR AppGroup.swift.
//
// So the gate asserts BOTH DIRECTIONS, which is the only way to tell a working wall from a dead
// one: a strongly-linked "no" must LOCK, and a strongly-linked "yes" must UNLOCK. Without the
// second, a probe that always says locked scores full marks.
//
// FOUR NEGATIVE CONTROLS, each stopping a different way of passing for the wrong reason:
//   1. no store linked at all → must be UNLOCKED (else the check is measuring nothing, and every
//      editor/web build is broken)
//   2. the strong symbol says yes → must be UNLOCKED (else "locked" is just the probe's default)
//   3. the strong symbol must be REACHED with the manifest's own product id (else the answer is
//      right by accident, from a path that never consults the store)
//   4. an AU target with the entitlement source REMOVED must FAIL the structural check (else that
//      check is a grep that can no longer see anything)
'use strict'
const fs = require('fs'), path = require('path'), cp = require('child_process')
const ROOT = path.join(__dirname, '..')
const RUNTIME = path.join(ROOT, 'runtime')
const TMP = fs.mkdtempSync(path.join(require('os').tmpdir(), 'pro-check-'))
const quiet = process.argv.includes('--quiet')
let pass = 0, fail = 0
const ok = (cond, what, detail) => {
  if (cond) { pass++; if (!quiet) console.log(`  ✓ ${what}`) }
  else { fail++; console.log(`  ✗ ${what}${detail ? '  — ' + detail : ''}`) }
}

// ── the C half: compile pro.h four ways and read what it answers ─────────────────────────────
const PROBE = `
#include "pro.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    // the module axis too: "" is a free rack and must never consult the bridge
    printf("for_sale=%d unlocked=%d can_purchase=%d free_module=%d asked=%s\\n",
           pro_for_sale(), pro_unlocked(), pro_can_purchase(),
           pro_module_unlocked(""), asked_for());
    return 0;
}
`
// A strong definition of the bridge, as ios/Sources/Entitlements.swift provides it. Records the id
// it was asked about, so the gate can prove the answer came THROUGH the store and not around it.
const strong = (answer, canBuy) => `
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
static char g_asked[128] = "(never asked)";
bool Store_IsModuleUnlocked(const char *id) { snprintf(g_asked, sizeof g_asked, "%s", id); return ${answer}; }
bool Store_CanPurchase(void) { return ${canBuy}; }
const char *asked_for(void) { return g_asked; }
`
const noStrong = `const char *asked_for(void) { return "(no store linked)"; }\n`

function variant(name, { proId, strongC }) {
  const dir = path.join(TMP, name); fs.mkdirSync(dir, { recursive: true })
  if (proId !== null) fs.writeFileSync(path.join(dir, 'app_pro.h'),
    `#pragma once\n#define APP_PRO_ID ${JSON.stringify(proId)}\n#define APP_PRO_PRICE "4.99"\n`)
  fs.writeFileSync(path.join(dir, 'probe.c'), `const char *asked_for(void);\n` + PROBE)
  fs.writeFileSync(path.join(dir, 'store.c'), strongC)
  const bin = path.join(dir, 'probe')
  const r = cp.spawnSync('clang', ['-I' + dir, '-I' + RUNTIME,
    path.join(dir, 'probe.c'), path.join(dir, 'store.c'), '-o', bin], { encoding: 'utf8' })
  if (r.status !== 0) return { error: (r.stderr || '').trim().split('\n').slice(0, 4).join(' / ') }
  const run = cp.spawnSync(bin, { encoding: 'utf8' })
  const out = {}
  for (const kv of (run.stdout || '').trim().split(' ')) { const i = kv.indexOf('='); if (i > 0) out[kv.slice(0, i)] = kv.slice(i + 1) }
  return out
}

const PID = 'com.mipolai.tinypedalboard.pro'

if (!process.argv.includes('--selfcheck')) {
  if (!quiet) console.log('\nthe C seam (runtime/pro.h)')

  // NEGATIVE CONTROL 1 — no store anywhere: this is the editor / web / desktop case and it MUST
  // read unlocked, or every non-store build of every gated cart is broken.
  const a = variant('no-store', { proId: null, strongC: noStrong })
  ok(!a.error, 'no app_pro.h: compiles with no engine and no store', a.error)
  ok(a.for_sale === '0' && a.unlocked === '1', 'no app_pro.h → nothing for sale, everything unlocked',
     JSON.stringify(a))
  ok(a.can_purchase === '0', 'no app_pro.h → no purchase offered')

  // The documented FAIL-OPEN: a product exists but nobody linked an answer. Unlocked, and this is
  // the exact shape that shipped Pro for free inside the AUv3 — hence assertion H below.
  const b = variant('weak-only', { proId: PID, strongC: noStrong })
  ok(b.for_sale === '1', 'app_pro.h present → pro_for_sale()')
  ok(b.unlocked === '1', 'weak stubs only → FAILS OPEN (documented; the structural check is what catches it)')
  ok(b.can_purchase === '0', 'weak stubs only → cannot purchase, so the sheet says "open the app"')

  // THE GATE — a strong "no" must lock.
  const c = variant('strong-no', { proId: PID, strongC: strong('false', 'true') })
  ok(c.unlocked === '0', 'strong Store_IsModuleUnlocked(false) → LOCKED', JSON.stringify(c))
  ok(c.can_purchase === '1', 'strong Store_CanPurchase(true) → the app can show a sheet')
  // NEGATIVE CONTROL 3 — the answer must have come THROUGH the bridge, carrying the real id.
  ok(c.asked === PID, `the bridge was asked about the manifest's own product id`, `asked=${c.asked}`)
  ok(c.free_module === '1', 'a free ("" product) rack never consults the bridge')

  // NEGATIVE CONTROL 2 — a strong "yes" must unlock, or "locked" is just the probe's default.
  const d = variant('strong-yes', { proId: PID, strongC: strong('true', 'true') })
  ok(d.unlocked === '1', 'strong Store_IsModuleUnlocked(true) → UNLOCKED (else the gate always says locked)',
     JSON.stringify(d))
}

// ── the STRUCTURAL half: does every target that runs a cart link a real answer? ───────────────
// This is the one that would have caught the shipped defect. A target declaring AU_EXT is an
// audio-unit extension: it runs a cart, it cannot reach StoreKit, and its ONLY entitlement source
// is Entitlements.swift + AppGroup.swift over the App Group. Miss them and it fails open.
const NEEDED = ['Sources/Entitlements.swift', 'Sources/AppGroup.swift']

// ⚠ Match a DECLARATION, on a line that is not a COMMENT. Both ymls discuss the group in prose
// (project.yml's whole note is about why it is absent), and an earlier version of this matched that
// prose — so the check reported the group as present precisely because the file was explaining that
// it is not. Strip comment lines first; a plist `<string>` line is never a comment, a yml `#` is.
function declaresGroup(text, groupId) {
  return text.split('\n')
    .filter(l => !/^\s*#/.test(l))
    .some(l => l.includes('application-groups') || new RegExp('<string>\\s*' + groupId + '\\s*</string>').test(l))
}
function auditYml(text) {
  // Targets are 2-space keys under `targets:`; we only need "which target block is this line in".
  const out = []
  let cur = null
  for (const line of text.split('\n')) {
    const m = /^  ([A-Za-z0-9_.-]+):\s*$/.exec(line)
    if (m) { cur = { name: m[1], au: false, sources: [] }; out.push(cur); continue }
    if (!cur) continue
    if (line.includes('AU_EXT')) cur.au = true
    const src = /^\s*-\s*(?:path:\s*)?(\S+)\s*$/.exec(line)
    if (src) cur.sources.push(src[1])
  }
  return out.filter(t => t.au).map(t => ({
    name: t.name,
    missing: NEEDED.filter(n => !t.sources.includes(n) && !t.sources.includes(path.dirname(n))),
  }))
}

if (process.argv.includes('--selfcheck')) {
  // known answers for the parser, both directions — a structural check that has gone blind reports
  // "no AU targets" and passes, so the fixture pins that it FINDS them and that it FAILS one.
  const good = `targets:\n  App:\n    sources:\n      - path: Sources\n  TinyjamAU:\n    sources:\n      - Sources/CanvasView.swift\n      - Sources/Entitlements.swift\n      - Sources/AppGroup.swift\n    settings:\n      base:\n        SWIFT_ACTIVE_COMPILATION_CONDITIONS: [$(inherited), AU_EXT]\n`
  const bad = good.replace('      - Sources/Entitlements.swift\n', '')
  const none = `targets:\n  App:\n    sources:\n      - path: Sources\n`
  const g = auditYml(good), b = auditYml(bad), n = auditYml(none)
  ok(g.length === 1, 'selfcheck: finds the AU target', JSON.stringify(g))
  ok(g[0] && g[0].missing.length === 0, 'selfcheck: a complete AU target is clean')
  ok(b.length === 1 && b[0].missing.length === 1, 'selfcheck: a stripped AU target reports the missing file',
     JSON.stringify(b))
  ok(n.length === 0, 'selfcheck: a project with no AU target reports none (not a false pass on absence)')
  // the whole-file path, so a parser change that breaks on the REAL yml shows up here too
  const real = auditYml(fs.readFileSync(path.join(ROOT, 'ios/project.yml'), 'utf8'))
  ok(real.length >= 1, 'selfcheck: the real ios/project.yml still parses to at least one AU target',
     JSON.stringify(real))
  // the app-group declaration test, both directions. The false-positive half is not hypothetical:
  // it shipped for one run, and it reported the group PRESENT because a comment explained it is not.
  const G = 'group.com.mipolai.shared'
  ok(declaresGroup(`        com.apple.security.application-groups: [${G}]\n`, G),
     'selfcheck: a real yml declaration counts')
  ok(declaresGroup(`\t<key>com.apple.security.application-groups</key>\n\t<array>\n\t\t<string>${G}</string>\n`, G),
     'selfcheck: a real plist declaration counts')
  ok(!declaresGroup(`    # NOTE: re-add com.apple.security.application-groups: [${G}] once registered\n`, G),
     'selfcheck: a COMMENT mentioning it does NOT count (the false positive this shipped with)')
  ok(!declaresGroup('        com.apple.security.app-sandbox: true\n', G),
     'selfcheck: an unrelated entitlement does not count')
} else {
  if (!quiet) console.log('\nthe structural half (every AU_EXT target links a real answer)')
  // Only the TRACKED specs. ios/project-{dev,store}.yml and project-mac-dev.yml are cp/sed copies
  // that device.sh / testflight.sh / mac.sh derive at build time from these two (and are gitignored),
  // so a stale copy on disk is not a defect and patching one would be lost on the next build. Read
  // the list from git rather than hardcoding it, so a NEW tracked spec is audited automatically.
  const tracked = cp.spawnSync('git', ['ls-files', 'ios/project*.yml'], { cwd: ROOT, encoding: 'utf8' })
    .stdout.trim().split('\n').filter(Boolean)
  ok(tracked.length >= 2, `git tracks ${tracked.length} project spec(s) (0 = the list went blind)`,
     tracked.join(', '))
  let seen = 0
  for (const rel of tracked) {
    const f = path.basename(rel)
    const targets = auditYml(fs.readFileSync(path.join(ROOT, rel), 'utf8'))
    for (const t of targets) {
      seen++
      ok(t.missing.length === 0, `ios/${f} → ${t.name} links the entitlement source`,
         'missing ' + t.missing.join(', '))
    }
  }
  // NEGATIVE CONTROL 4 — if the parser stops seeing AU targets this check silently becomes a no-op.
  ok(seen >= 2, `found ${seen} AU_EXT target(s) in the tracked specs (a parser that finds none passes vacuously)`)
  const mutated = fs.readFileSync(path.join(ROOT, 'ios/project.yml'), 'utf8')
    .replace('      - Sources/Entitlements.swift\n', '')
  const m = auditYml(mutated).filter(t => t.missing.length)
  ok(m.length >= 1, 'negative control: removing Entitlements.swift from an AU target FAILS the check')

  // ── the CROSS-PLATFORM chain: does a purchase on one device reach the plug-in on another? ──
  // docs/design/pro-unlock.md section 8. Two links are code and are asserted; the third is a
  // developer-portal action nobody can check from here, so it is REPORTED rather than gated.
  if (!quiet) console.log('\nthe cross-platform chain (buy on iPhone → plug-in on Mac)')
  const ident = fs.readFileSync(path.join(ROOT, 'ios/au-identity.sh'), 'utf8')
  // Universal Purchase keys on the bundle id being IDENTICAL across platforms. The dev carrier is
  // deliberately suffixed; the STORE identity must be the manifest's bundleId untouched.
  ok(/CARRIER_STORE_APP_ID="\$base"/.test(ident),
     'the Mac STORE identity is the iOS bundle id unchanged (Universal Purchase needs that)')
  ok(/CARRIER_APP_ID="\$base\.mac"/.test(ident),
     'the local dev carrier stays suffixed, so it cannot be mistaken for the store app')
  // NEGATIVE CONTROL for the pair above: if au-identity.sh stopped defining either, both regexes
  // would simply not match and this section would go quiet rather than red.
  ok(/CARRIER_APPEX_ID=/.test(ident), 'negative control: au-identity.sh still defines the carrier block at all')

  // The App Group is the ONLY way an entitlement reaches an extension, and a missing declaration is
  // silent: UserDefaults(suiteName:) hands back a usable suite with no entitlement, so everything
  // works inside one process and nothing crosses the app/appex boundary. Report, do not gate: it is
  // deliberately absent until the group is registered for automatic provisioning (re-adding it
  // early blocks plain device signing), and pro-unlock.md section 8 owns the sequence.
  const groupId = (/static let id = "([^"]+)"/.exec(
    fs.readFileSync(path.join(ROOT, 'ios/Sources/AppGroup.swift'), 'utf8')) || [])[1]
  // ⚠ match the DECLARATION, never the name: ios/project.yml mentions the group in a COMMENT
  // recording why it was removed, and counting that as a declaration turns this into a check that
  // reports "1 of 4 present" while the true answer is none.
  const carriers = ['ios/project.yml', 'ios/project-mac.yml', 'ios/Mac.entitlements', 'ios/MacAU.entitlements']
  const declares = f => {
    try { return declaresGroup(fs.readFileSync(path.join(ROOT, f), 'utf8'), groupId) } catch { return false }
  }
  const withGroup = carriers.filter(declares)
  ok(!!groupId, 'AppGroup.swift names a group id', String(groupId))
  if (withGroup.length === 0 && !quiet) {
    console.log(`  ⚠ ${groupId} is declared by NO built target (${carriers.length}/${carriers.length} missing).`)
    console.log('    Not a failure yet — see docs/design/pro-unlock.md section 8: it waits on the group')
    console.log('    being registered for automatic provisioning, and re-adding it early blocks device signing.')
    console.log('    ⚠ Until then a purchase CANNOT reach the AUv3 on a signed build, on either platform.')
  }

  // and the generator seam: a single-cart app must still get its app_pro.h (it used to be written
  // only inside `if (launcher)`, so pedalboard and tinyacidjam generated nothing at all)
  if (!quiet) console.log('\nthe generator seam (tools/build-app.js)')
  for (const app of ['pedalboard', 'tinyacidjam']) {
    cp.spawnSync('node', [path.join(ROOT, 'tools/build-app.js'), app, '--dry'], { cwd: ROOT })
  }
  const readPro = d => { try { return fs.readFileSync(path.join(ROOT, 'build', d, 'app_pro.h'), 'utf8') } catch { return '' } }
  ok(/APP_PRO_ID\s+"com\.mipolai\.tinypedalboard\.pro"/.test(readPro('.app-tinypedalboard')),
     'single-cart app pedalboard generates app_pro.h with its product id')
  ok(/APP_PRO_PRICE\s+"4\.99"/.test(readPro('.app-tinyacidjam')),
     'single-cart app tinyacidjam generates app_pro.h with its price')
}

fs.rmSync(TMP, { recursive: true, force: true })
const label = process.argv.includes('--selfcheck') ? 'pro-check --selfcheck' : 'pro-check'
console.log(`\n${label}: ${pass} passed, ${fail} failed`)
if (fail) process.exit(1)   // spelt out, not `exit(fail ? 1 : 0)`: gate-controls.js greps for this exact shape
process.exit(0)
