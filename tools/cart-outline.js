#!/usr/bin/env node
// ============================================================================
// cart-outline.js — a per-cart READING MAP so a large cart can be understood
// (and navigated) without reading the whole .c into context.
//
//   node tools/cart-outline.js <name>        # e.g. sloop, or cart:sloop
//   node tools/cart-outline.js sloop --json  # machine-readable
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
const raw = args.find(a => !a.startsWith("--"));
if (!raw) {
  console.error("usage: node tools/cart-outline.js <cart-name> [--json]");
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
      funcs.push({ line: startLine, endLine, sig: head, name: nameM[1], role: leadingComment(startLine) });
    }
  }
}

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
  let decl = rawLines[i].trim();
  // drop the initializer (we want the shape, not the data)
  const eq = decl.search(/\s=\s|\s=\{|=\s*\{|\s=$/);
  if (eq >= 0) decl = decl.slice(0, eq).replace(/[,{[(\s]+$/, "") + ";";
  decl = decl.replace(/\s*\{\s*$/, ";");                 // multiline array init head
  globals.push({ line: ln, decl });
}

// ---- object/function-like #define at col 0 ----------------------------------
const defines = [];
for (let i = 0; i < rawLines.length; i++) {
  const m = rawLines[i].match(/^#\s*define\s+([A-Za-z_]\w*)(\([^)]*\))?/);
  if (m) defines.push({ line: i + 1, name: m[1] + (m[2] || ""), text: rawLines[i].trim() });
}

// ---- entry points -----------------------------------------------------------
const ENTRY = ["update", "draw", "spec", "setup"];
const entries = ENTRY
  .map(n => funcs.find(f => f.name === n))
  .filter(Boolean)
  .map(f => ({ name: f.name, start: f.line, end: f.endLine, span: f.endLine - f.line + 1 }));

const loc = code.split("\n").filter(l => l.trim()).length;
const usesState = /\bde_state\s*\(|\bSTATE\b|\bS->/.test(code);

// ============================================================================
// output
// ============================================================================
if (json) {
  console.log(JSON.stringify({ cart: name, loc, usesState, shapes, globals, defines, funcs, entries }, null, 2));
  process.exit(0);
}

const tty = process.stdout.isTTY;
const bold = s => (tty ? `\x1b[1m${s}\x1b[0m` : s);
const dim = s => (tty ? `\x1b[2m${s}\x1b[0m` : s);
const head = s => "\n" + bold(s) + "\n" + dim("─".repeat(s.length));
const clip = (s, n) => (s.length > n ? s.slice(0, n - 1) + "…" : s);
const lpad = (s, n) => String(s).padStart(n);
const rpad = (s, n) => String(s).padEnd(n);

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

// global state — one-line signatures
if (globals.length) {
  o.push(head(`GLOBAL STATE  (${globals.length})`));
  for (const g of globals) o.push(dim(lpad(g.line, 6) + "  ") + clip(g.decl, 86));
}

// defines
if (defines.length) {
  o.push(head(`MACROS  (${defines.length})`));
  for (const d of defines) o.push(dim(lpad(d.line, 6) + "  ") + clip(d.text, 86));
}

// function index — line · signature · role
o.push(head(`FUNCTIONS  (${funcs.length})`));
if (!funcs.length) o.push(dim("  none found (macro-heavy cart? read it directly)."));
funcs.sort((a, b) => a.line - b.line);
const sigW = Math.min(54, Math.max(...funcs.map(f => f.sig.length), 0));
for (const f of funcs) {
  const sig = clip(f.sig, sigW);
  o.push(dim(lpad(f.line, 6) + "  ") + rpad(sig, sigW) + (f.role ? dim("  // " + clip(f.role, 48)) : ""));
}

// entry points
if (entries.length) {
  o.push(head("ENTRY POINTS"));
  for (const e of entries) o.push("  " + rpad(e.name + "()", 10) + dim(`${e.start}-${e.end}  (${e.span} lines)`));
}

o.push(head("READ A SLICE"));
o.push(dim("  Read with offset/limit, or: ") + `sed -n '<a>,<b>p' ${path.relative(ROOT, cPath)}`);
o.push(dim("  external context for this cart: ") + `node tools/build-context.js ${name}`);

console.log(o.join("\n"));
