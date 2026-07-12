#ifndef DE_JSON_H
#define DE_JSON_H
// json.h — EXPERIMENTAL cart-land JSON reader for data-driven carts.
//
//   A cart that wants to load data at RUNTIME (instead of baking C arrays into its
//   source) reads de_data_path() (from studio.h: the --data <file> flag, or $DE_DATA),
//   slurps the file, and walks it with the helpers below. The parser is jsmn
//   (https://github.com/zserge/jsmn, MIT) — a zero-allocation tokenizer: it fills an
//   array of {type,start,end,size} tokens that INDEX into the original text; it never
//   copies or allocates. You size the token array, jsmn fills it, you walk it.
//
//   This is the engine deliberately NOT owning data loading (ADR-0006, like ui.h /
//   cards.h): a header any cart can copy. If the data-cart experiment is dropped this
//   file just goes away. NOT committed API — no studioDocs/shell.js entry.
//
//   Typical use (see tools/carts/roadview.c):
//     char *js = json_slurp(de_data_path(), NULL);     // read whole file (malloc)
//     jsmntok_t *tok; int n = json_parse(js, &tok);     // tokenize (malloc, exact)
//     int feats = json_get(js, tok, 0, "features");     // child of the root object
//     int fi = feats + 1;                               // first element of the array
//     for (int k = 0; k < tok[feats].size; k++) {
//         int kind = json_get(js, tok, fi, "kind");     // a string token
//         int pts  = json_get(js, tok, fi, "pts");      // an array of numbers
//         ... json_num(js, &tok[pts + 1 + j]) ...
//         fi += json_span(tok, fi);                     // step over this feature
//     }
//     free(tok); free(js);
//
// ── vendored jsmn (single-file, non-strict, no parent links) ─────────────────────
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef enum {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT    = 1 << 0,
    JSMN_ARRAY     = 1 << 1,
    JSMN_STRING    = 1 << 2,
    JSMN_PRIMITIVE = 1 << 3
} jsmntype_t;

enum jsmnerr {
    JSMN_ERROR_NOMEM = -1,   // not enough tokens
    JSMN_ERROR_INVAL = -2,   // invalid character inside JSON
    JSMN_ERROR_PART  = -3    // truncated JSON
};

typedef struct {
    jsmntype_t type;
    int start;               // byte offset of the token's first char in the source
    int end;                 // byte offset just past the token's last char
    int size;                // child count (object/array) — 0 for scalars
} jsmntok_t;

typedef struct {
    unsigned int pos;        // offset in the source string
    unsigned int toknext;    // next token to allocate
    int toksuper;            // superior token node, e.g. parent object/array
} jsmn_parser;

static jsmntok_t *jsmn_alloc_token(jsmn_parser *p, jsmntok_t *tokens, size_t num) {
    jsmntok_t *t;
    if (p->toknext >= num) return NULL;
    t = &tokens[p->toknext++];
    t->start = t->end = -1;
    t->size = 0;
    return t;
}

static void jsmn_fill_token(jsmntok_t *t, jsmntype_t type, int start, int end) {
    t->type = type; t->start = start; t->end = end; t->size = 0;
}

static int jsmn_parse_primitive(jsmn_parser *p, const char *js, size_t len,
                                jsmntok_t *tokens, size_t num) {
    jsmntok_t *t;
    int start = p->pos;
    for (; p->pos < len && js[p->pos] != '\0'; p->pos++) {
        switch (js[p->pos]) {
        case ':': case '\t': case '\r': case '\n': case ' ':
        case ',': case ']': case '}':
            goto found;
        default: break;
        }
        if (js[p->pos] < 32 || js[p->pos] >= 127) { p->pos = start; return JSMN_ERROR_INVAL; }
    }
found:
    if (tokens == NULL) { p->pos--; return 0; }
    t = jsmn_alloc_token(p, tokens, num);
    if (t == NULL) { p->pos = start; return JSMN_ERROR_NOMEM; }
    jsmn_fill_token(t, JSMN_PRIMITIVE, start, p->pos);
    p->pos--;
    return 0;
}

static int jsmn_parse_string(jsmn_parser *p, const char *js, size_t len,
                             jsmntok_t *tokens, size_t num) {
    jsmntok_t *t;
    int start = p->pos;
    p->pos++;
    for (; p->pos < len && js[p->pos] != '\0'; p->pos++) {
        char c = js[p->pos];
        if (c == '\"') {
            if (tokens == NULL) return 0;
            t = jsmn_alloc_token(p, tokens, num);
            if (t == NULL) { p->pos = start; return JSMN_ERROR_NOMEM; }
            jsmn_fill_token(t, JSMN_STRING, start + 1, p->pos);
            return 0;
        }
        if (c == '\\' && p->pos + 1 < len) {
            int i;
            p->pos++;
            switch (js[p->pos]) {
            case '\"': case '/': case '\\': case 'b':
            case 'f': case 'r': case 'n': case 't': break;
            case 'u':
                p->pos++;
                for (i = 0; i < 4 && p->pos < len && js[p->pos] != '\0'; i++) {
                    char h = js[p->pos];
                    if (!((h >= 48 && h <= 57) || (h >= 65 && h <= 70) || (h >= 97 && h <= 102))) {
                        p->pos = start; return JSMN_ERROR_INVAL;
                    }
                    p->pos++;
                }
                p->pos--;
                break;
            default: p->pos = start; return JSMN_ERROR_INVAL;
            }
        }
    }
    p->pos = start;
    return JSMN_ERROR_PART;
}

// Parse JSON in `js` of `len` bytes into up to `num` tokens. With tokens==NULL it only
// COUNTS (returns how many tokens the text needs) — the two-pass trick that lets a
// caller allocate exactly. Returns the token count, or a negative JSMN_ERROR_*.
static int jsmn_parse(jsmn_parser *p, const char *js, size_t len,
                      jsmntok_t *tokens, unsigned int num) {
    int r, i, count = p->toknext;
    jsmntok_t *t;
    for (; p->pos < len && js[p->pos] != '\0'; p->pos++) {
        char c = js[p->pos];
        switch (c) {
        case '{': case '[':
            count++;
            if (tokens == NULL) break;
            t = jsmn_alloc_token(p, tokens, num);
            if (t == NULL) return JSMN_ERROR_NOMEM;
            if (p->toksuper != -1) tokens[p->toksuper].size++;
            t->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
            t->start = p->pos;
            p->toksuper = p->toknext - 1;
            break;
        case '}': case ']': {
            jsmntype_t type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
            if (tokens == NULL) break;
            for (i = p->toknext - 1; i >= 0; i--) {
                t = &tokens[i];
                if (t->start != -1 && t->end == -1) {
                    if (t->type != type) return JSMN_ERROR_INVAL;
                    p->toksuper = -1;
                    t->end = p->pos + 1;
                    break;
                }
            }
            if (i == -1) return JSMN_ERROR_INVAL;
            for (; i >= 0; i--) {
                t = &tokens[i];
                if (t->start != -1 && t->end == -1) { p->toksuper = i; break; }
            }
            break;
        }
        case '\"':
            r = jsmn_parse_string(p, js, len, tokens, num);
            if (r < 0) return r;
            count++;
            if (p->toksuper != -1 && tokens != NULL) tokens[p->toksuper].size++;
            break;
        case '\t': case '\r': case '\n': case ' ': break;
        case ':':
            p->toksuper = p->toknext - 1;
            break;
        case ',':
            if (tokens != NULL && p->toksuper != -1 &&
                tokens[p->toksuper].type != JSMN_ARRAY &&
                tokens[p->toksuper].type != JSMN_OBJECT) {
                for (i = p->toknext - 1; i >= 0; i--) {
                    if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) {
                        if (tokens[i].start != -1 && tokens[i].end == -1) { p->toksuper = i; break; }
                    }
                }
            }
            break;
        default:
            r = jsmn_parse_primitive(p, js, len, tokens, num);
            if (r < 0) return r;
            count++;
            if (p->toksuper != -1 && tokens != NULL) tokens[p->toksuper].size++;
            break;
        }
    }
    if (tokens != NULL)
        for (i = p->toknext - 1; i >= 0; i--)
            if (tokens[i].start != -1 && tokens[i].end == -1) return JSMN_ERROR_PART;
    return count;
}

static void jsmn_init(jsmn_parser *p) { p->pos = 0; p->toknext = 0; p->toksuper = -1; }

// ── cart-friendly helpers on top of jsmn ─────────────────────────────────────────

// Read a whole file into a malloc'd, NUL-terminated buffer. Returns NULL on failure;
// *outlen (if non-NULL) gets the byte length. free() it when done.
static char *json_slurp(const char *path, long *outlen) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    if (outlen) *outlen = (long)got;
    return buf;
}

// Tokenize `js` into an exactly-sized, malloc'd token array (*out). Returns the token
// count (>=0) or a negative JSMN_ERROR_*. Caller free()s *out. Two-pass: count, alloc, fill.
static int json_parse(const char *js, jsmntok_t **out) {
    *out = NULL;
    if (!js) return JSMN_ERROR_INVAL;
    size_t len = strlen(js);
    jsmn_parser p; jsmn_init(&p);
    int n = jsmn_parse(&p, js, len, NULL, 0);
    if (n < 0) return n;
    jsmntok_t *toks = (jsmntok_t *)malloc(sizeof(jsmntok_t) * (n > 0 ? n : 1));
    if (!toks) return JSMN_ERROR_NOMEM;
    jsmn_init(&p);
    int r = jsmn_parse(&p, js, len, toks, (unsigned)(n > 0 ? n : 1));
    if (r < 0) { free(toks); return r; }
    *out = toks;
    return r;
}

// 1 if token `t` is a string/primitive whose text equals `s`.
static int json_eq(const char *js, const jsmntok_t *t, const char *s) {
    int n = t->end - t->start;
    return ((int)strlen(s) == n) && strncmp(js + t->start, s, (size_t)n) == 0;
}

// Numeric value of a primitive token (works for ints and floats).
static double json_num(const char *js, const jsmntok_t *t) { return strtod(js + t->start, NULL); }

// Copy a string/primitive token's text into `out` (NUL-terminated, up to max-1 chars).
static void json_str(char *out, int max, const char *js, const jsmntok_t *t) {
    int n = t->end - t->start;
    if (n > max - 1) n = max - 1;
    if (n < 0) n = 0;
    memcpy(out, js + t->start, (size_t)n);
    out[n] = '\0';
}

// Deeper than any real data file, shallower than a C-stack blowout: jsmn's tokenizer is
// iterative (survives any nesting), but this walk recurses one frame per level, so a crafted
// deeply-nested file (e.g. OSM data — json.h ingests untrusted input) would overflow the stack.
#define JSON_MAX_DEPTH 512
// Number of tokens spanned by tokens[i] INCLUDING itself + all descendants. Lets you
// step over a value (object/array/scalar) without parent links.
static int json_span_d(const jsmntok_t *tokens, int i, int depth) {
    if (depth > JSON_MAX_DEPTH) return 1;   // refuse to recurse further on hostile/corrupt nesting — treat as a leaf
    int j, n;
    switch (tokens[i].type) {
    case JSMN_OBJECT:
        n = 1;
        for (j = 0; j < tokens[i].size; j++) { n += 1; n += json_span_d(tokens, i + n, depth + 1); }
        return n;
    case JSMN_ARRAY:
        n = 1;
        for (j = 0; j < tokens[i].size; j++) n += json_span_d(tokens, i + n, depth + 1);
        return n;
    default:   // JSMN_STRING / JSMN_PRIMITIVE
        return 1;
    }
}
static int json_span(const jsmntok_t *tokens, int i) { return json_span_d(tokens, i, 0); }

// Index of the VALUE token for `key` inside the object token at index `obj`, or -1.
static int json_get(const char *js, const jsmntok_t *tokens, int obj, const char *key) {
    if (obj < 0 || tokens[obj].type != JSMN_OBJECT) return -1;
    int i = obj + 1;
    for (int k = 0; k < tokens[obj].size; k++) {
        if (json_eq(js, &tokens[i], key)) return i + 1;   // value follows the key token
        i += 1 + json_span(tokens, i + 1);                // skip key + its value subtree
    }
    return -1;
}

#endif // DE_JSON_H
