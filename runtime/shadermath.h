// shadermath.h — a tiny GLSL-vocabulary toolkit for CPU shaders, cart-land.
//
// Why this exists: dreamengine has no GPU shader hook — a "fragment shader" here is just
// a C function returning a 0xRRGGBB through pset_rgb / rectfill_rgb (see shadelab). Written
// with bare floats that gets verbose fast, and it reads nothing like the GLSL you'd find on
// Shadertoy. This header gives you the missing vocabulary — vec2 / vec3, dot / normalize /
// length, fract / smoothstep / mix / step, the Iñigo-Quílez cosine palette(), and the
// signed-distance-field primitives a raymarcher needs (sd_sphere/box/torus/plane + smin) —
// so a CPU shader reads almost exactly like its GLSL cousin. Learn the words here and a
// Shadertoy snippet becomes a near-paste. (raymarch.c is the worked example.)
//
// Usage — include AFTER studio.h (it leans on clamp / fsqrt / sin_deg / cos_deg):
//   #include "studio.h"
//   #include "shadermath.h"
//   vec3 p = v3(1,0,0);
//   float d = sd_sphere(p, 0.5f);
//   int col = vrgb(palette(d, PAL_A, PAL_B, PAL_C, PAL_RAINBOW_D));
//
// Engine deliberately does NOT own this (ADR-0006): it's cart-land convenience, all static,
// each cart compiles its own copy. Nothing here touches engine state — pure math.

#ifndef SHADERMATH_H
#define SHADERMATH_H

// ── scalar idioms (the GLSL built-ins studio.h doesn't already have) ─────────────
static float sm_floor(float x) { int i = (int)x; return (x < 0 && (float)i != x) ? i - 1 : i; }
static float fract(float x)    { return x - sm_floor(x); }          // wrap into 0..1 (works for negatives)
static float sat(float x)      { return clamp(x, 0, 1); }           // saturate: clamp to 0..1
static float mix(float a, float b, float t)  { return a + (b - a) * t; }     // GLSL mix == lerp
static float step(float edge, float x)       { return x < edge ? 0.0f : 1.0f; }
static float smoothstep(float e0, float e1, float x) {              // hermite 0..1 with soft ends
    float t = sat((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
static float fmin2(float a, float b) { return a < b ? a : b; }
static float fmax2(float a, float b) { return a > b ? a : b; }
static float fabs2(float x)          { return x < 0 ? -x : x; }

// ── vec2 ─────────────────────────────────────────────────────────────────────────
typedef struct { float x, y; } vec2;
static vec2  v2(float x, float y)        { vec2 r = { x, y }; return r; }
static vec2  v2add(vec2 a, vec2 b)       { return v2(a.x + b.x, a.y + b.y); }
static vec2  v2sub(vec2 a, vec2 b)       { return v2(a.x - b.x, a.y - b.y); }
static vec2  v2mul(vec2 a, float s)      { return v2(a.x * s, a.y * s); }
static float v2dot(vec2 a, vec2 b)       { return a.x * b.x + a.y * b.y; }
static float v2len(vec2 a)               { return fsqrt(a.x * a.x + a.y * a.y); }
static vec2  v2fract(vec2 a)             { return v2(fract(a.x), fract(a.y)); }

// ── vec3 (what a raymarcher lives on) ─────────────────────────────────────────────
typedef struct { float x, y, z; } vec3;
static vec3  v3(float x, float y, float z) { vec3 r = { x, y, z }; return r; }
static vec3  v3add(vec3 a, vec3 b)  { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static vec3  v3sub(vec3 a, vec3 b)  { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static vec3  v3mul(vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static vec3  v3mad(vec3 a, vec3 b, float s) { return v3(a.x + b.x * s, a.y + b.y * s, a.z + b.z * s); } // a + b*s
static float v3dot(vec3 a, vec3 b)  { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float v3len(vec3 a)          { return fsqrt(v3dot(a, a)); }
static vec3  v3norm(vec3 a)         { float l = v3len(a); return l > 1e-6f ? v3mul(a, 1.0f / l) : a; }
static vec3  v3cross(vec3 a, vec3 b){ return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static vec3  v3mix(vec3 a, vec3 b, float t) { return v3(mix(a.x, b.x, t), mix(a.y, b.y, t), mix(a.z, b.z, t)); }
static vec3  v3abs(vec3 a)          { return v3(fabs2(a.x), fabs2(a.y), fabs2(a.z)); }
static vec3  v3maxs(vec3 a, float m){ return v3(fmax2(a.x, m), fmax2(a.y, m), fmax2(a.z, m)); }

// ── colour ─────────────────────────────────────────────────────────────────────
static int rgb(float r, float g, float b) {                     // 0..1 channels → 0xRRGGBB
    int R = (int)(sat(r) * 255), G = (int)(sat(g) * 255), B = (int)(sat(b) * 255);
    return (R << 16) | (G << 8) | B;
}
static int vrgb(vec3 c)        { return rgb(c.x, c.y, c.z); }    // a vec3 colour → 0xRRGGBB
static int scale_rgb(int c, float b) {
    int R = (int)(((c >> 16) & 0xFF) * b), G = (int)(((c >> 8) & 0xFF) * b), B = (int)((c & 0xFF) * b);
    return (R << 16) | (G << 8) | B;
}
static int cadd(int a, int b) {                                 // saturating add
    int R = ((a >> 16) & 0xFF) + ((b >> 16) & 0xFF); if (R > 255) R = 255;
    int G = ((a >> 8)  & 0xFF) + ((b >> 8)  & 0xFF); if (G > 255) G = 255;
    int B = (a & 0xFF)         + (b & 0xFF);          if (B > 255) B = 255;
    return (R << 16) | (G << 8) | B;
}
static int hsv(float h, float s, float v) {                     // hue/sat/val → 0xRRGGBB (h wraps)
    h = fract(h);
    float i = h * 6.0f, f = i - (int)i;
    float p = v * (1 - s), q = v * (1 - s * f), tt = v * (1 - s * (1 - f));
    switch ((int)i % 6) {
        case 0: return rgb(v, tt, p);  case 1: return rgb(q, v, p);
        case 2: return rgb(p, v, tt);  case 3: return rgb(p, q, v);
        case 4: return rgb(tt, p, v);  default: return rgb(v, p, q);
    }
}

// IQ cosine palette: colour = a + b * cos(2π (c·t + d)), per channel. Four vec3 coeffs
// make a smooth looping gradient — the Shadertoy classic. PAL_* below are a good default.
static vec3 palette(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
    return v3(a.x + b.x * cos_deg(360.0f * (c.x * t + d.x)),
              a.y + b.y * cos_deg(360.0f * (c.y * t + d.y)),
              a.z + b.z * cos_deg(360.0f * (c.z * t + d.z)));
}
#define PAL_A          v3(0.5f, 0.5f, 0.5f)
#define PAL_B          v3(0.5f, 0.5f, 0.5f)
#define PAL_C          v3(1.0f, 1.0f, 1.0f)
#define PAL_RAINBOW_D  v3(0.00f, 0.33f, 0.67f)   // the rainbow phase (IQ's first example)
#define PAL_WARM_D     v3(0.00f, 0.10f, 0.20f)   // ambers → magentas
#define PAL_OCEAN_D    v3(0.30f, 0.20f, 0.20f)   // teals → blues

// ── signed distance fields (for raymarching) — distance from p to the surface ────
static float sd_sphere(vec3 p, float r)        { return v3len(p) - r; }
static float sd_plane(vec3 p, float y)         { return p.y - y; }              // ground at height y
static float sd_box(vec3 p, vec3 b) {
    vec3 q = v3sub(v3abs(p), b);
    return v3len(v3maxs(q, 0.0f)) + fmin2(fmax2(q.x, fmax2(q.y, q.z)), 0.0f);
}
static float sd_torus(vec3 p, float R, float r) {                               // ring of radius R, tube r
    float qx = v2len(v2(p.x, p.z)) - R;
    return fsqrt(qx * qx + p.y * p.y) - r;
}
static float op_union(float a, float b)        { return fmin2(a, b); }
static float op_sub(float a, float b)          { return fmax2(a, -b); }         // carve b out of a
// smooth minimum (IQ polynomial): blends two surfaces into a gooey metaball merge. k = blend radius.
static float op_smin(float a, float b, float k) {
    float h = sat(0.5f + 0.5f * (b - a) / k);
    return mix(b, a, h) - k * h * (1.0f - h);
}

#endif // SHADERMATH_H
