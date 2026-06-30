#!/usr/bin/env node
// ============================================================================
// cart-outline.js — a per-cart READING MAP so a large cart can be understood
// (and navigated) without reading the whole .c into context.
//
//   node tools/cart-outline.js <name>            # e.g. sloop, or cart:sloop
//   node tools/cart-outline.js sloop --full      # + every macro's value & role
//   node tools/cart-outline.js sloop --fn <name> # print ONE function's body
//   node tools/cart-outline.js sloop --json      # machine-readable
//
// CHEAP BY DEFAULT. The map prints macro NAMES only (a tuning-heavy cart like
// sloop has ~140 #defines — the values are noise unless you're tuning). Add
// --full for the values + role comments. --fn <name> dumps just that one
// function (with its leading comment + line numbers) so you never guess the
// end line for a Read — exact match wins, else substring; matches a struct/enum
// name too.
//
// THE FRICTION THIS KILLS. The big carts run 1000–3000 lines (sloop is ~3k).
// To answer one question about such a cart you either read the whole file
// (~30k tokens) or grep blind. Neither is cheap. This tool distills the cart
// into ~150 lines: its data shapes (structs/enums VERBATIM — the load-bearing
// part), its global state (one-line signatures), and a FUNCTION INDEX with a
// line number + a one-line role for each. You read the map (~2–4k tokens),
// then Read the exact slice the map points at instead of the whole file.
//
// Twin of build-context.js: that gathers a cart's EXTERNAL context (docs/notes
// that mention it); this maps the cart's OWN source. Sibling to cart-analyze.js
// (fleet-wide complexity ranking — this is the single-cart drill-down) and
// cart-info.js (metadata/drift — this is the code structure).
//
// HEURISTIC, NOT A COMPILER. Top-level brace-matching over comment-stripped
// (but line-preserving) source. Catches the cart idioms; won't chase macros
// that hide a brace. Plain node, CommonJS.
// ============================================================================

const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..");
const SRC_DIR = path.join(ROOT, "tools/carts");

// ---- args ----
const args = process.argv.slice(2);
const json = args.includes("--json");
const full = args.includes("--full");
let fnQuery = null;
const positional = [];
for (let i = 0; i < args.length; i++) {
  const a = args[i];
  if (a === "--fn") { fnQuery = args[++i] || ""; continue; }   // value is the next token
  if (a.startsWith("--fn=")) { fnQuery = a.slice(5); continue; }
  if (a.startsWith("--")) continue;                            // other flags
  positional.push(a);
}
const raw = positional[0];
if (!raw) {
  console.error("usage: node tools/cart-outline.js <cart-name> [--full] [--fn <name>] [--json]");
  process.exit(2);
}
const name = raw.replace(/^cart:/, "");
const cPath = path.join(SRC_DIR, name + ".c");
if (!fs.existsSync(cPath)) {
  console.error(`no cart source at ${path.relative(ROOT, cPath)} — is the name right? (try: ls tools/carts/)`);
  process.exit(1);
}

const src = fs.readFileSync(cPath, "utf8");
const rawLines = src.split("\n");

// ---- line-preserving comment/string strip -------------------------------
// Blank out comment + string CONTENTS (so braces/parens inside them don't fool
// the brace matcher) but keep every newline so a position maps to its real line.
function stripPreserving(s) {
  let out = "";
  let i = 0;
  const n = s.length;
  let st = 0; // 0 code, 1 line-comment, 2 block-comment, 3 string, 4 char
  let q = "";
  while (i < n) {
    const c = s[i], d = s[i + 1];
    if (st === 0) {
      if (c === "/" && d === "/") { out += "  "; i += 2; st = 1; continue; }
      if (c === "/" && d === "*") { out += "  "; i += 2; st = 2; continue; }
      if (c === '"') { out += '"'; i++; st = 3; q = '"'; continue; }
      if (c === "'") { out += "'"; i++; st = 4; q = "'"; continue; }
      out += c; i++; continue;
    }
    if (st === 1) { if (c === "\n") { out += "\n"; i++; st = 0; } else { out += " "; i++; } continue; }
    if (st === 2) {
      if (c === "*" && d === "/") { out += "  "; i += 2; st = 0; continue; }
      out += c === "\n" ? "\n" : " "; i++; continue;
    }
    // string / char: blank contents, keep the closing quote + newlines
    if (c === "\\") { out += "  "; i += 2; continue; }
    if (c === q) { out += q; i++; st = 0; continue; }
    out += c === "\n" ? "\n" : " "; i++;
  }
  return out;
}
const code = stripPreserving(src);

// position -> 1-based line number
const lineStarts = [0];
for (let i = 0; i < code.length; i++) if (code[i] === "\n") lineStarts.push(i + 1);
function lineAt(pos) {
  let lo = 0, hi = lineStarts.length - 1;
  while (lo < hi) { const mid = (lo + hi + 1) >> 1; if (lineStarts[mid] <= pos) lo = mid; else hi = mid - 1; }
  return lo + 1;
}

// ---- walk top-level brace blocks -----------------------------------------
// Each block: the "head" text before its opening `{` at depth 0, and the line
// span. Heads classify into functions vs type defs vs initializers.
const codeLines = code.split("\n");
const blocks = [];
{
  let depth = 0, headStart = 0;
  for (let i = 0; i < code.length; i++) {
    const c = code[i];
    if (c === "{") {
      if (depth === 0) {
        const head = code.slice(headStart, i);
        blocks.push({ head, openPos: i, startPos: headStart, closePos: -1 });
      }
      depth++;
    } else if (c === "}") {
      depth--;
      if (depth === 0 && blocks.length) {
        const b = blocks[blocks.length - 1];
        if (b.closePos === -1) b.closePos = i;
      }
      if (depth === 0) headStart = i + 1;
    } else if (c === ";" && depth === 0) {
      headStart = i + 1;
    } else if (c === "\n" && depth === 0) {
      // preprocessor directives don't end in `;` — drop them so they don't
      // glue onto the next definition's head.
      if (/^\s*#/.test(codeLines[lineAt(i) - 1] || "")) headStart = i + 1;
    }
  }
}

const KW = new Set(["if", "for", "while", "switch", "do", "else", "return", "sizeof"]);

const shapes = [];   // { startLine, endLine, name, kind }
const funcs = [];    // { line, endLine, sig, name, role }

for (const b of blocks) {
  const headRaw = b.head;
  const head = headRaw.replace(/\s+/g, " ").trim();
  if (!head) continue;

  // type definition: typedef struct/enum/union, or a bare struct/enum/union tag
  if (/\b(typedef\s+)?(struct|enum|union)\b/.test(head) && !/\)\s*$/.test(head)) {
    // anchor the start at the keyword (or `typedef`), not at headStart
    const kwOff = headRaw.search(/\b(typedef|struct|enum|union)\b/);
    const startLine = lineAt(b.startPos + (kwOff >= 0 ? kwOff : 0));
    // the def ends at the `;` after the closing brace — find it in raw code
    let j = b.closePos + 1;
    while (j < code.length && code[j] !== ";") j++;
    const endLine = lineAt(j);
    // name: trailing identifier after the close brace (typedef), else tag after keyword
    let nm = "";
    const after = code.slice(b.closePos + 1, j).replace(/\s+/g, " ").trim();
    if (after) nm = (after.match(/([A-Za-z_]\w*)\s*;?$/) || [, after])[1] || after;
    else { const t = head.match(/\b(?:struct|enum|union)\s+([A-Za-z_]\w*)/); if (t) nm = t[1]; }
    const kind = /\benum\b/.test(head) ? "enum" : /\bunion\b/.test(head) ? "union" : "struct";
    shapes.push({ startLine, endLine, name: nm, kind });
    continue;
  }

  // function: head has an identifier immediately before a `(`, last token before
  // the open brace is `)`, and it isn't a control-flow head.
  const parenIdx = head.indexOf("(");
  if (parenIdx > 0 && /\)\s*$/.test(head)) {
    const beforeParen = head.slice(0, parenIdx).trim();
    const nameM = beforeParen.match(/([A-Za-z_]\w*)$/);
    if (nameM && !KW.has(nameM[1])) {
      const startLine = lineAt(b.openPos);
      const endLine = b.closePos >= 0 ? lineAt(b.closePos) : startLine;
      const role = leadingComment(startLine) || trailingComment(startLine - 1);
      funcs.push({ line: startLine, endLine, sig: head, name: nameM[1], role });
    }
  }
}

// the trailing `// …` (or inline `/* … */`) comment on a raw line, if any.
// codeLines[i] is char-for-char the same length as rawLines[i] with comment
// content blanked, so the code ends at its trimEnd() length — everything the
// raw line has past there is the comment. It's already in the source: free.
function commentStart(i) { return (codeLines[i] || "").replace(/\s+$/, "").length; }
function trailingComment(i) {
  const rest = (rawLines[i] || "").slice(commentStart(i));
  const m = rest.match(/\/\/+\s?(.*)$/) || rest.match(/\/\*+\s?([\s\S]*?)\s*\*\/\s*$/);
  return m ? m[1].trim() : "";
}
// the raw code on a line minus its trailing comment (string literals intact —
// codeLines blanks string contents, so display from the raw text, cut at the comment).
function codeOnly(i) { return (rawLines[i] || "").slice(0, commentStart(i)).trim(); }

// contiguous // (or trailing */ block) comment lines immediately above a def
function leadingComment(line) {
  const out = [];
  for (let i = line - 2; i >= 0; i--) {
    const t = (rawLines[i] || "").trim();
    if (t === "") { if (out.length) break; else continue; }
    if (t.startsWith("//")) { out.unshift(t.replace(/^\/+\s?/, "")); continue; }
    if (/^\*/.test(t) || /\*\/\s*$/.test(t)) { out.unshift(t.replace(/^[/*]+\s?/, "").replace(/\s*\*\/$/, "")); continue; }
    break;
  }
  return (out.find(Boolean) || "").trim();
}

// ---- global state: col-0 `static` declarations that aren't functions --------
const funcLines = new Set(funcs.map(f => f.line));
const shapeRanges = shapes.map(s => [s.startLine, s.endLine]);
const inShape = ln => shapeRanges.some(([a, b]) => ln >= a && ln <= b);

const globals = [];
for (let i = 0; i < rawLines.length; i++) {
  const cl = (codeLines[i] || "");
  if (!/^static\b/.test(cl)) continue;
  if (/\btypedef\b/.test(cl)) continue;
  const ln = i + 1;
  if (funcLines.has(ln) || inShape(ln)) continue;
  // a function definition/prototype on this line — skip
  if (/\)\s*\{?\s*$/.test(cl) && /\w\s*\(/.test(cl)) continue;
  // const/inline aren't mutable state
  if (/^static\s+(const|inline)\b/.test(cl)) continue;
  let decl = codeOnly(i);                                // raw code minus trailing comment
  // drop the initializer (we want the shape, not the data)
  const eq = decl.search(/\s=\s|\s=\{|=\s*\{|\s=$/);
  if (eq >= 0) decl = decl.slice(0, eq).replace(/[,{[(\s]+$/, "") + ";";
  decl = decl.replace(/\s*\{\s*$/, ";");                 // multiline array init head
  globals.push({ line: ln, decl, role: trailingComment(i) });
}

// ---- object/function-like #define at col 0 ----------------------------------
const defines = [];
for (let i = 0; i < rawLines.length; i++) {
  const m = rawLines[i].match(/^#\s*define\s+([A-Za-z_]\w*)(\([^)]*\))?/);
  if (m) defines.push({ line: i + 1, name: m[1] + (m[2] || ""), code: codeOnly(i), role: trailingComment(i) });
}

// ---- entry points -----------------------------------------------------------
const ENTRY = ["update", "draw", "spec", "setup"];
const entries = ENTRY
  .map(n => funcs.find(f => f.name === n))
  .filter(Boolean)
  .map(f => ({ name: f.name, start: f.line, end: f.endLine, span: f.endLine - f.line + 1 }));

const loc = code.split("\n").filter(l => l.trim()).length;
const usesState = /\bde_state\s*\(|\bSTATE\b|\bS->/.test(code);

const tty = process.stdout.isTTY;
const bold = s => (tty ? `\x1b[1m${s}\x1b[0m` : s);
const dim = s => (tty ? `\x1b[2m${s}\x1b[0m` : s);
const head = s => "\n" + bold(s) + "\n" + dim("─".repeat(s.length));
const clip = (s, n) => (s.length > n ? s.slice(0, n - 1) + "…" : s);
const lpad = (s, n) => String(s).padStart(n);
const rpad = (s, n) => String(s).padEnd(n);

// ============================================================================
// --fn <name>: print ONE definition's body (no guessing the end line)
// ============================================================================
// the first line of the contiguous comment block directly above a def (skips
// immediate blank lines, like leadingComment) — so the slice carries its docs.
function sliceStart(defLine) {
  let start = defLine, seen = false;
  for (let i = defLine - 2; i >= 0; i--) {
    const t = (rawLines[i] || "").trim();
    if (t === "") { if (seen) break; else continue; }
    if (t.startsWith("//") || /^\*/.test(t) || /\*\/\s*$/.test(t) || /^\/\*/.test(t)) { start = i + 1; seen = true; continue; }
    break;
  }
  return start;
}
if (fnQuery != null) {
  const q = fnQuery.toLowerCase();
  const named = [
    ...funcs.map(f => ({ name: f.name, a: sliceStart(f.line), b: f.endLine, tag: f.sig })),
    ...shapes.filter(s => s.name).map(s => ({ name: s.name, a: s.startLine, b: s.endLine, tag: `${s.kind} ${s.name}` })),
  ];
  const exact = named.filter(t => t.name.toLowerCase() === q);
  const targets = exact.length ? exact : named.filter(t => t.name.toLowerCase().includes(q));
  if (!targets.length) {
    console.error(`no function or shape matching "${fnQuery}" in ${name}.`);
    console.error(`names: ${named.map(t => t.name).join(", ")}`);
    process.exit(1);
  }
  if (targets.length > 4) {
    console.error(`"${fnQuery}" matches ${targets.length} names — be more specific:`);
    for (const t of targets) console.error(`  ${t.name}  (${t.a}-${t.b})`);
    process.exit(1);
  }
  const out = [];
  for (const t of targets) {
    const lines = t.b - t.a + 1;
    out.push(bold(t.name) + dim(`   ${path.relative(ROOT, cPath)}:${t.a}-${t.b}  (${lines} lines)`));
    for (let i = t.a; i <= t.b; i++) out.push(dim(lpad(i, 6) + "  ") + (rawLines[i - 1] || "").replace(/\s+$/, ""));
    out.push("");
  }
  console.log(out.join("\n").replace(/\n+$/, ""));
  process.exit(0);
}

// wrap a list of tokens into ` · `-joined lines no wider than `width`
function wrapList(items, width) {
  const sep = " · ";
  const out = []; let cur = "";
  for (const it of items) {
    if (cur && cur.length + sep.length + it.length > width) { out.push(cur); cur = it; }
    else cur = cur ? cur + sep + it : it;
  }
  if (cur) out.push(cur);
  return out;
}

// ============================================================================
// output
// ============================================================================
if (json) {
  console.log(JSON.stringify({ cart: name, loc, usesState, shapes, globals, defines, funcs, entries }, null, 2));
  process.exit(0);
}

const o = [];
o.push(bold(`cart: ${name}`) + dim(
  `   ${loc} loc · ${funcs.length} fn · ${globals.length} glob · ${shapes.length} shapes` +
  ` · de_state: ${usesState ? "yes" : "no"}`));
o.push(dim(`   ${path.relative(ROOT, cPath)}`));

// data shapes — verbatim (the load-bearing part), capped per block
o.push(head("DATA SHAPES"));
if (!shapes.length) o.push(dim("  none (no top-level struct/enum/union)."));
const SHAPE_CAP = 40;
for (const s of shapes) {
  const tag = dim(`  [${s.kind}${s.name ? " " + s.name : ""}]  ${s.startLine}-${s.endLine}`);
  o.push(tag);
  const body = rawLines.slice(s.startLine - 1, s.endLine);
  const shown = body.slice(0, SHAPE_CAP);
  for (const l of shown) o.push("    " + l.replace(/\s+$/, ""));
  if (body.length > SHAPE_CAP) o.push(dim(`    … +${body.length - SHAPE_CAP} more lines (read ${s.startLine}-${s.endLine})`));
}

// aligned `line · code · // role` rows — shared by globals, macros, functions.
// the role is the line's own comment (trailing for vars/macros, leading for
// fns), pulled into its own column so the code width never eats the comment.
function rows(items, codeOf, capW) {
  const w = Math.min(capW, Math.max(...items.map(it => codeOf(it).length), 0));
  for (const it of items) {
    const code = clip(codeOf(it), w);
    o.push(dim(lpad(it.line, 6) + "  ") + rpad(code, w) + (it.role ? dim("  // " + clip(it.role, 52)) : ""));
  }
}

// global state — one-line signatures
if (globals.length) {
  o.push(head(`GLOBAL STATE  (${globals.length})`));
  rows(globals, g => g.decl, 48);
}

// defines — names only by default (the values are tuning noise unless asked);
// --full prints each macro's value + role comment.
if (defines.length) {
  if (full) {
    o.push(head(`MACROS  (${defines.length})`));
    rows(defines, d => d.code, 40);
  } else {
    o.push(head(`MACROS  (${defines.length})`) + dim("   names only — values + roles: --full"));
    for (const ln of wrapList(defines.map(d => d.name), 76)) o.push("  " + dim(ln));
  }
}

// function index — line · signature · role
o.push(head(`FUNCTIONS  (${funcs.length})`));
if (!funcs.length) o.push(dim("  none found (macro-heavy cart? read it directly)."));
funcs.sort((a, b) => a.line - b.line);
rows(funcs, f => f.sig, 54);

// entry points
if (entries.length) {
  o.push(head("ENTRY POINTS"));
  for (const e of entries) o.push("  " + rpad(e.name + "()", 10) + dim(`${e.start}-${e.end}  (${e.span} lines)`));
}

o.push(head("READ A SLICE"));
o.push(dim("  one function, full body:   ") + `node tools/cart-outline.js ${name} --fn <name>`);
o.push(dim("  every macro value + role:  ") + `node tools/cart-outline.js ${name} --full`);
o.push(dim("  or by hand: ") + `sed -n '<a>,<b>p' ${path.relative(ROOT, cPath)}` + dim("  (Read offset/limit)"));
o.push(dim("  external context for this cart: ") + `node tools/build-context.js ${name}`);
o.push(dim("  this map + that context in 1:   ") + `node tools/orient.js ${name}`);
o.push(dim("  look up a studio.h API sig:     ") + `node tools/api.js <name>`);

console.log(o.join("\n"));
