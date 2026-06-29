/* de:meta
{
  "title": "raymarch",
  "status": "active",
  "created": "2026-06-12",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "raycasting",
    "software-rasterizer"
  ],
  "lineage": "Third in the shadelab → caustics → raymarch shader-chapter sequence; novel in that it introduces a full signed-distance-field scene (op_smin smooth metaballs, op_sub boolean carve) rendered purely via sphere-marching, using the new shadermath.h GLSL-vocabulary header.",
  "description": "A 3D scene that is JUST A MATH FUNCTION - the third chapter after shadelab (a pixel is a function of u,v) and caustics (warp where you look up a colour), and the worked showcase for the new shadermath.h GLSL-vocabulary header. There are no triangles, no mesh: the whole world is one function, scene(p), that returns how far the nearest surface is from any point (a SIGNED DISTANCE FIELD). Each pixel shoots a ray and marches it forward in steps of exactly that distance (it can never overshoot) until the distance drops to ~zero - a hit. The surface normal for lighting is just the gradient of that same function, sampled with four taps. scene() is six lines because shadermath.h supplies vec3 + dot/normalize/length, the sd_sphere/box/torus/plane primitives, and the op_smin smooth-minimum that BLENDS two surfaces into a gooey metaball merge - trivial with distance fields, painful with polygons. Three scenes show the vocabulary: melting metaballs (op_smin), a tumbling torus, and a box with a sphere carved out (op_sub); shaded with a sun + blinn specular, an Inigo-Quilez cosine palette() for colour, a checker ground and distance fog. Written so it reads almost exactly like the GLSL you'd find on Shadertoy. MOUSE orbits the camera, UP/DOWN zoom, 1/2/3 or LEFT/RIGHT switch scene, [ ] pixel size (raymarching is heavy - chunkier trades detail for speed), SPACE freezes time."
}
de:meta */
#include "studio.h"
#include "shadermath.h"

// RAYMARCH — a 3D scene that is JUST A MATH FUNCTION. The third chapter after shadelab
// (a pixel is a function of u,v) and caustics (warp where you look up a colour): here every
// pixel shoots a RAY into the scene and walks it forward until it hits a surface. There is
// no geometry, no triangles, no mesh — the entire world is one function, scene(p), that
// answers "how far is the nearest surface from this point?" (a SIGNED DISTANCE FIELD). You
// march along the ray in steps of exactly that distance — guaranteed never to overshoot —
// and when the distance drops to ~zero you've hit something. The surface NORMAL (for
// lighting) is just the gradient of that same function, sampled with four taps.
//
// This is the cart that shows off shadermath.h: written with bare floats a raymarcher is
// unreadable, but with vec3 + dot/normalize/length + the sd_* primitives it reads almost
// exactly like the GLSL you'd find on Shadertoy. Look how short scene() is — that whole 3D
// world is six lines. op_smin() is the star: it BLENDS two surfaces into a gooey metaball
// merge (mode 1), the thing that's trivial with distance fields and painful with polygons.
//
//   1/2/3 or LEFT/RIGHT : scene (metaballs / torus / box−sphere)
//   MOUSE : orbit the camera      UP/DOWN : zoom      [ ] : pixel size (detail ⇄ speed)
//   SPACE : freeze time

static int   cur = 0;            // which scene
static int   ps  = 3;            // pixel size — raymarching is heavy, so chunkier = faster
static float zoom = 4.6f;        // camera distance
static bool  frozen;
static float t;

#define MAXSTEP 64
#define FAR     20.0f
#define SURF    0.0025f

// THE WORLD — distance from point p to the nearest surface. Negative = inside. That's it.
static float scene(vec3 p) {
    float ground = sd_plane(p, -1.0f);             // a floor at y = -1
    float obj;
    if (cur == 0) {                                 // three spheres melted together (op_smin)
        vec3 a = v3(0.9f * sin_deg(t * 60), 0.2f, 0.6f + 0.3f * cos_deg(t * 40));
        vec3 b = v3(-0.9f + 0.3f * sin_deg(t * 55), -0.1f, -0.5f * cos_deg(t * 70));
        float s1 = sd_sphere(v3sub(p, a), 0.65f);
        float s2 = sd_sphere(v3sub(p, b), 0.6f);
        float s3 = sd_sphere(p, 0.7f);              // anchor at the origin
        obj = op_smin(op_smin(s1, s2, 0.5f), s3, 0.5f);
    } else if (cur == 1) {                          // a torus tumbling about its x-axis
        float ca = cos_deg(t * 50), sa = sin_deg(t * 50);
        vec3 q = v3(p.x, p.y * ca - p.z * sa, p.y * sa + p.z * ca);
        obj = sd_torus(q, 1.0f, 0.35f);
    } else {                                        // a box with a sphere carved out (op_sub)
        float ca = cos_deg(t * 40), sa = sin_deg(t * 40);
        vec3 q = v3(p.x * ca - p.z * sa, p.y, p.x * sa + p.z * ca);
        obj = op_sub(sd_box(q, v3(0.72f, 0.72f, 0.72f)), sd_sphere(p, 0.95f));
    }
    return op_union(ground, obj);
}

// the surface normal is the GRADIENT of scene() — sampled with four taps (IQ's tetrahedron).
static vec3 calc_normal(vec3 p) {
    const float e = 0.0015f;
    vec3 n = v3(0, 0, 0);
    n = v3add(n, v3mul(v3( 1, -1, -1), scene(v3add(p, v3( e, -e, -e)))));
    n = v3add(n, v3mul(v3(-1, -1,  1), scene(v3add(p, v3(-e, -e,  e)))));
    n = v3add(n, v3mul(v3(-1,  1, -1), scene(v3add(p, v3(-e,  e, -e)))));
    n = v3add(n, v3mul(v3( 1,  1,  1), scene(v3add(p, v3( e,  e,  e)))));
    return v3norm(n);
}

// walk the ray forward in steps of the distance field until we hit (or run to the horizon).
static int raymarch(vec3 ro, vec3 rd, float *hit_dist) {
    float d = 0;
    for (int i = 0; i < MAXSTEP; i++) {
        float h = scene(v3mad(ro, rd, d));          // p = ro + rd*d
        if (h < SURF) { *hit_dist = d; return 1; }  // close enough = a hit
        d += h;                                      // safe to leap exactly `h` — can't overshoot
        if (d > FAR) break;
    }
    *hit_dist = d;
    return 0;
}

void init(void) { cur = 0; ps = 3; zoom = 4.6f; t = 0; }

void update(void) {
    if (!frozen) t += dt();
    if (keyp(KEY_SPACE)) frozen = !frozen;
    if (keyp(KEY_RIGHT)) cur = (cur + 1) % 3;
    if (keyp(KEY_LEFT))  cur = (cur + 2) % 3;
    for (int k = 0; k < 3; k++) if (keyp('1' + k)) cur = k;
    if (keyp(KEY_UP))   zoom = clamp(zoom - 0.4f, 2.5f, 9.0f);
    if (keyp(KEY_DOWN)) zoom = clamp(zoom + 0.4f, 2.5f, 9.0f);
    if (keyp(']') && ps < 8) ps++;
    if (keyp('[') && ps > 1) ps--;
#ifdef DE_TRACE
    watch("ray", "scene=%d zoom=%.1f ps=%d", cur, zoom, ps);
#endif
}

static const char *SCENE[] = { "metaballs", "torus", "box - sphere" };

void draw(void) {
    // ── build an orbiting camera (lookAt). Mouse drives azimuth + elevation. ──
    float az   = t * 22.0f + ((float)mouse_x() / SCREEN_W - 0.5f) * 360.0f;
    float elev = mix(8.0f, 70.0f, sat(1.0f - (float)mouse_y() / SCREEN_H));
    vec3  ro   = v3mul(v3(cos_deg(az) * cos_deg(elev), sin_deg(elev), sin_deg(az) * cos_deg(elev)), zoom);
    ro.y += 0.4f;
    vec3  fwd  = v3norm(v3sub(v3(0, 0.1f, 0), ro));   // look at the origin
    vec3  rgt  = v3norm(v3cross(fwd, v3(0, 1, 0)));
    vec3  upv  = v3cross(rgt, fwd);
    vec3  lite = v3norm(v3(0.6f, 0.85f, 0.4f));        // one sun

    const float fov = 1.35f;
    for (int sy = 0; sy < SCREEN_H; sy += ps)
        for (int sx = 0; sx < SCREEN_W; sx += ps) {
            // screen → ray direction (divide by H for a square aspect, flip y for up)
            float u = (sx + ps * 0.5f - SCREEN_W * 0.5f) / SCREEN_H;
            float v = -(sy + ps * 0.5f - SCREEN_H * 0.5f) / SCREEN_H;
            vec3 rd = v3norm(v3add(fwd, v3add(v3mul(rgt, u * fov), v3mul(upv, v * fov))));

            float dist;
            vec3 col;
            if (raymarch(ro, rd, &dist)) {
                vec3 p = v3mad(ro, rd, dist);
                vec3 n = calc_normal(p);
                float diff = sat(v3dot(n, lite));
                float amb  = 0.25f + 0.2f * n.y;          // a little sky fill from above
                vec3 base;
                if (p.y < -0.985f) {                       // the ground plane: a checker
                    int chk = ((int)sm_floor(p.x * 0.7f) + (int)sm_floor(p.z * 0.7f)) & 1;
                    base = chk ? v3(0.26f, 0.28f, 0.34f) : v3(0.12f, 0.13f, 0.17f);
                } else {                                   // objects: coloured by the IQ palette
                    base = palette(0.55f + p.y * 0.22f + t * 0.04f, PAL_A, PAL_B, PAL_C, PAL_RAINBOW_D);
                }
                vec3 lit = v3mul(base, amb + diff * 0.9f);
                // blinn specular highlight
                vec3 hlf = v3norm(v3sub(lite, rd));
                float sp = sat(v3dot(n, hlf)); sp = sp * sp; sp = sp * sp; sp = sp * sp;  // ^8
                lit = v3add(lit, v3mul(v3(1, 1, 1), sp * 0.5f));
                // distance fog into the sky colour
                float fog = sat(dist / FAR); fog *= fog;
                col = v3mix(lit, v3(0.55f, 0.7f, 0.92f), fog);
            } else {
                float gy = sat(0.5f + rd.y);               // sky gradient
                col = v3mix(v3(0.78f, 0.86f, 0.96f), v3(0.28f, 0.45f, 0.78f), gy);
            }
            int bw = (sx + ps <= SCREEN_W) ? ps : SCREEN_W - sx;
            int bh = (sy + ps <= SCREEN_H) ? ps : SCREEN_H - sy;
            rectfill_rgb(sx, sy, bw, bh, vrgb(col));
        }

    // HUD
    rectfill(0, 0, SCREEN_W, 10, CLR_BLACK);
    print(str("raymarch: %s", SCENE[cur]), 4, 2, CLR_WHITE);
    print_right("mouse: orbit", SCREEN_W - 4, 2, CLR_YELLOW);
    rectfill(0, SCREEN_H - 9, SCREEN_W, 9, CLR_BLACK);
    print(str("1/2/3 scene   up/dn zoom   [ ] pixel %d   space %s", ps, frozen ? "frozen" : "live"),
          4, SCREEN_H - 7, CLR_LIGHT_GREY);
}
