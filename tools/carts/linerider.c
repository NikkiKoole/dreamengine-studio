/* de:meta
{
  "slug": "linerider",
  "collection": ["physics"],
  "title": "line rider",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "toy"
  ],
  "teaches": [
    "verlet-integration",
    "spring-damper"
  ],
  "lineage": "Homage to Line Rider (2006); novel: a 5-point Verlet ragdoll with rigid rod constraints slides over user-drawn line segments using signed-distance side-tracking for two-sided collision.",
  "genre": "sandbox",
  "description": "Draw a track with the mouse, hit Space and watch Bosh slide. Verlet physics — gravity, rolling friction, crash detection. Left drag: draw lines. Right click: erase. Space: play / pause. R: reset rider (or clear track while in draw mode). Arrow keys: pan. The rider's scarf trails behind when moving fast enough."
}
de:meta */
#include "studio.h"
#include "physics.h"   // shared verlet toolkit (Layer 0) — rod_solve/integrate delegate to it

// LINE RIDER — draw a track, watch Bosh slide.
// Left drag: draw line   Right click: erase   Space: play/pause   R: reset / clear

#define MAX_SEGS 512
#define GRAV     0.17f
#define FRIC     0.992f   // rolling friction on sled contact

// --- track ---
typedef struct { float x1,y1, x2,y2; } Seg;
static Seg  segs[MAX_SEGS];
static int  nseg;

// --- rider — 5 Verlet points ---
//  0: sled_back   1: sled_front
//  2: butt        3: chest
//  4: hand (grips front of sled)
#define NP 5
typedef PhysPt Vp;   // physics.h point (adds w=inverse-mass, r=radius; the rider uses w=1, r=0)
static Vp   vp[NP];

// --- rods (rigid constraints between points) ---
typedef struct { int a,b; float len; } Rod;
#define NR 6
static Rod rod[NR];

// --- state ---
static float spx, spy;     // spawn position
static bool  playing, crashed;
static bool  drag_on;
static float drag_sx, drag_sy;   // last emitted segment endpoint (updates as you draw)
static float drag_px, drag_py;   // true start of the whole stroke (for the preview line)
static float cam_x, cam_y;
static int   pan_mx, pan_my;     // middle-drag: last mouse position

// ---- helpers ----

// rigid distance rod — now shared: physics.h phys_link (equal-mass split == the old 0.5 rod_solve).
static void rod_solve(int i) { phys_link(&vp[rod[i].a], &vp[rod[i].b], rod[i].len); }

// Resolve point against all lines; fric = friction coefficient (0-1 keeps velocity).
// Two-sided: uses the previous-frame signed distance to decide which side to push
// back to, so lines work regardless of drawing direction and at any speed.
static void collide(Vp *p, float fric) {
    for (int i = 0; i < nseg; i++) {
        float ex = segs[i].x2 - segs[i].x1;
        float ey = segs[i].y2 - segs[i].y1;
        float len2 = ex*ex + ey*ey;
        if (len2 < 1.0f) continue;
        float len = fsqrt(len2);
        float nx = ey/len, ny = -ex/len;   // one of the two normals (arbitrary side)

        float t = ((p->x - segs[i].x1)*ex + (p->y - segs[i].y1)*ey) / len2;
        if (t < 0.0f || t > 1.0f) continue;
        float sd = (p->x - (segs[i].x1 + t*ex))*nx
                 + (p->y - (segs[i].y1 + t*ey))*ny;

        // Previous position — tells us which side of the line the rider came from.
        float t_p = ((p->px-segs[i].x1)*ex + (p->py-segs[i].y1)*ey) / len2;
        t_p = clamp(t_p, 0.0f, 1.0f);
        float sd_p = (p->px - (segs[i].x1 + t_p*ex))*nx
                   + (p->py - (segs[i].y1 + t_p*ey))*ny;

        // side = +1 or -1: which half-space the rider occupies
        float side   = (sd_p >= 0.0f) ? 1.0f : -1.0f;
        float sd_eff = sd * side;   // positive = safely on own side of the line

        if (sd_eff > 0.5f) continue;   // far enough away: no contact

        float vx = p->x - p->px, vy = p->y - p->py;   // velocity before push

        // push back to 0.5px on the correct side
        p->x += nx * side * (0.5f - sd_eff);
        p->y += ny * side * (0.5f - sd_eff);

        float tx = ex/len, ty = ey/len;
        float vt = (vx*tx + vy*ty) * fric;
        p->px = p->x - tx*vt;
        p->py = p->y - ty*vt;
    }
}

static void rider_reset(void) {
    float x = spx, y = spy;
    vp[0] = (Vp){x-10, y,    x-10, y,    1,0};   // sled back  (w=1 movable, r=0)
    vp[1] = (Vp){x+10, y,    x+10, y,    1,0};   // sled front
    vp[2] = (Vp){x,    y-11, x,    y-11, 1,0};   // butt
    vp[3] = (Vp){x-2,  y-24, x-2,  y-24, 1,0};  // chest
    vp[4] = (Vp){x+10, y-15, x+10, y-15, 1,0};  // hand

    rod[0] = (Rod){0,1, 20.0f};    // sled beam
    rod[1] = (Rod){0,2, 15.5f};    // back  → butt
    rod[2] = (Rod){1,2, 15.5f};    // front → butt  (triangle)
    rod[3] = (Rod){2,3, 13.4f};    // spine
    rod[4] = (Rod){3,4, 12.8f};    // arm
    float dx = vp[4].x - vp[1].x, dy = vp[4].y - vp[1].y;
    rod[5] = (Rod){1,4, fsqrt(dx*dx+dy*dy)};  // hand grips nose

    crashed = false;
}

static void phys_step(void) {
    if (crashed) return;

    // integrate with air drag — shared: physics.h phys_integrate (identical math)
    for (int i = 0; i < NP; i++) phys_integrate(&vp[i], 0.0f, GRAV, 0.996f);

    // 8 iterations: constraints then collision each round (stable at realistic speeds)
    for (int iter = 0; iter < 8; iter++) {
        // Upright spring — fights the sled tilt, keeps Bosh from lying flat on steep slopes.
        // Applied before rod_solve so the rods negotiate a balance between sled angle and vertical.
        {
            float lean = vp[3].x - vp[2].x;   // chest_x − butt_x: +forward, −backward lean
            vp[3].x -= lean * 0.010f;
            vp[2].x += lean * 0.010f;
        }
        for (int r = 0; r < NR; r++) rod_solve(r);
        collide(&vp[0], FRIC);
        collide(&vp[1], FRIC);
        collide(&vp[2], 0.87f);
        collide(&vp[3], 0.87f);
        collide(&vp[4], 0.87f);
    }

#ifdef DE_TRACE
    watch("sled0", "%.1f,%.1f", vp[0].x, vp[0].y);
    watch("sled1", "%.1f,%.1f", vp[1].x, vp[1].y);
    watch("butt",  "%.1f,%.1f", vp[2].x, vp[2].y);
    {
        float _vx = vp[0].x-vp[0].px, _vy = vp[0].y-vp[0].py;
        watch("spd", "%.2f", fsqrt(_vx*_vx+_vy*_vy));
    }
    watch("crash", "%d", (int)crashed);
#endif
    // crash: butt crossed below the sled plane
    float sdx = vp[1].x - vp[0].x;
    float sdy = vp[1].y - vp[0].y;
    float slen = fsqrt(sdx*sdx + sdy*sdy);
    if (slen > 0.001f) {
        float snx = sdy/slen, sny = -sdx/slen;  // right-hand normal, same as collision
        float bx = vp[2].x - (vp[0].x+vp[1].x)*0.5f;
        float by = vp[2].y - (vp[0].y+vp[1].y)*0.5f;
        if (bx*snx + by*sny < -6.0f) crashed = true;
    }
}

void init(void) {
    spx = 30.0f;  spy = 62.0f;
    rider_reset();

    // starter track (visible in thumbnail + good for first rides)
    segs[nseg++] = (Seg){ 10,  70, 160, 105};
    segs[nseg++] = (Seg){160, 105, 260,  85};
    segs[nseg++] = (Seg){260,  85, 380, 140};
    segs[nseg++] = (Seg){380, 140, 480,  90};

    cam_x = spx - SCREEN_W*0.35f;
    cam_y = spy - SCREEN_H*0.42f;
}

void update(void) {
    camera((int)cam_x, (int)cam_y);   // set before mouse_world reads

    if (keyp(KEY_SPACE) || btnp(0, BTN_A)) {
        playing = !playing;
        // no reset — resume from wherever the rider is (pause-draw-unpause workflow)
    }
    if (keyp('R') || btnp(0, BTN_B)) {
        rider_reset();   // always resets rider to spawn
    }
    if (keyp(KEY_BACKSPACE)) {
        nseg = 0;        // clear all lines (keep rider position)
    }

    // Middle-drag pan — works in both draw and play modes.
    // Cancels the camera-follow lerp while held so you can look ahead.
    if (mouse_pressed(MOUSE_MIDDLE)) {
        pan_mx = mouse_x();  pan_my = mouse_y();
        drag_on = false;   // cancel any in-progress draw stroke
    }
    if (mouse_down(MOUSE_MIDDLE)) {
        cam_x -= (float)(mouse_x() - pan_mx);
        cam_y -= (float)(mouse_y() - pan_my);
        pan_mx = mouse_x();  pan_my = mouse_y();
        camera((int)cam_x, (int)cam_y);
    }

    if (!playing) {
        float spd = 2.5f;
        if (btn(0, BTN_LEFT))  cam_x -= spd;
        if (btn(0, BTN_RIGHT)) cam_x += spd;
        if (btn(0, BTN_UP))    cam_y -= spd;
        if (btn(0, BTN_DOWN))  cam_y += spd;
        camera((int)cam_x, (int)cam_y);

        float wx = (float)mouse_world_x();   // read after any panning above
        float wy = (float)mouse_world_y();

        // left-drag: continuous drawing — emit segments as mouse moves
        if (mouse_pressed(MOUSE_LEFT)) {
            drag_sx = wx;  drag_sy = wy;
            drag_px = wx;  drag_py = wy;
            drag_on = true;
        }
        if (drag_on) {
            float ex = wx-drag_sx, ey = wy-drag_sy;
            if (ex*ex+ey*ey > 16 && nseg < MAX_SEGS) {
                segs[nseg++] = (Seg){drag_sx, drag_sy, wx, wy};
                drag_sx = wx;  drag_sy = wy;   // advance anchor
            }
        }
        if (drag_on && mouse_released(MOUSE_LEFT)) {
            // flush last bit of the stroke
            float ex = wx-drag_sx, ey = wy-drag_sy;
            if (ex*ex+ey*ey > 4 && nseg < MAX_SEGS)
                segs[nseg++] = (Seg){drag_sx, drag_sy, wx, wy};
            drag_on = false;
        }

        // right-click: erase nearest line
        if (mouse_down(MOUSE_RIGHT)) {
            int best = -1;  float bd = 9.0f;
            for (int i = 0; i < nseg; i++) {
                float ex = segs[i].x2-segs[i].x1, ey = segs[i].y2-segs[i].y1;
                float len2 = ex*ex+ey*ey;
                if (len2 < 1.0f) continue;
                float t = ((wx-segs[i].x1)*ex + (wy-segs[i].y1)*ey) / len2;
                t = clamp(t, 0.0f, 1.0f);
                float cx = segs[i].x1+t*ex, cy = segs[i].y1+t*ey;
                float d = distance((int)wx,(int)wy,(int)cx,(int)cy);
                if (d < bd) { bd = d; best = i; }
            }
            if (best >= 0) segs[best] = segs[--nseg];
        }
    } else {
        phys_step();
        // smooth camera follow — suspended while middle-dragging so panning sticks
        if (!mouse_down(MOUSE_MIDDLE)) {
            float rx = (vp[0].x + vp[1].x) * 0.5f;
            float ry = (vp[0].y + vp[1].y) * 0.5f;
            cam_x = lerp(cam_x, rx - SCREEN_W*0.4f,  0.07f);
            cam_y = lerp(cam_y, ry - SCREEN_H*0.45f, 0.07f);
        }
    }
}

void draw(void) {
    camera(0,0);  pal_reset();  fillp_reset();

    // gradient sky (screen-space)
    vgradient(0, 0, SCREEN_W, SCREEN_H, CLR_WHITE, CLR_LIGHT_GREY);

    camera((int)cam_x, (int)cam_y);

    // track lines
    for (int i = 0; i < nseg; i++) {
        // subtle inner highlight to give the line depth
        line((int)segs[i].x1, (int)segs[i].y1-1,
             (int)segs[i].x2, (int)segs[i].y2-1, CLR_INDIGO);
        thickline((int)segs[i].x1, (int)segs[i].y1,
                  (int)segs[i].x2, (int)segs[i].y2, 2, CLR_DARK_BLUE);
    }

    // drag preview — thin ghost from last anchor to cursor
    if (drag_on) {
        float wx = (float)mouse_x() + cam_x;
        float wy = (float)mouse_y() + cam_y;
        thickline((int)drag_sx, (int)drag_sy, (int)wx, (int)wy, 2, CLR_BLUE);
    }
    // already-drawn part of stroke shows as committed dark blue lines above

    // spawn flag
    if (!playing) {
        line((int)spx, (int)spy-1, (int)spx, (int)spy-11, CLR_DARK_GREEN);
        trifill((int)spx, (int)spy-11, (int)spx+7, (int)spy-8, (int)spx, (int)spy-5, CLR_GREEN);
    }

    // rider
    {
        int s0x = (int)vp[0].x, s0y = (int)vp[0].y;  // sled back
        int s1x = (int)vp[1].x, s1y = (int)vp[1].y;  // sled front
        int bux  = (int)vp[2].x, buy  = (int)vp[2].y;  // butt
        int chx  = (int)vp[3].x, chy  = (int)vp[3].y;  // chest
        int hax  = (int)vp[4].x, hay  = (int)vp[4].y;  // hand

        // scarf — trails behind chest based on velocity
        if (!crashed) {
            float svx = vp[3].x - vp[3].px;
            float svy = vp[3].y - vp[3].py;
            float spd = fsqrt(svx*svx + svy*svy);
            if (spd > 0.25f) {
                for (int k = 1; k <= 8; k++) {
                    float f = (float)k;
                    int rx = (int)(chx - svx*f*0.75f);
                    int ry = (int)(chy - svy*f*0.75f);
                    pset(rx, ry, k < 5 ? CLR_RED : CLR_ORANGE);
                }
            }
        }

        // sled shadow
        thickline(s0x+1, s0y+1, s1x+1, s1y+1, 3, CLR_DARK_GREY);
        // sled
        int sledcol = crashed ? CLR_RED : CLR_DARKER_GREY;
        thickline(s0x, s0y, s1x, s1y, 3, sledcol);
        circfill(s0x, s0y, 2, CLR_MEDIUM_GREY);
        circfill(s1x, s1y, 2, CLR_MEDIUM_GREY);

        // legs (butt to sled midpoint)
        int mx = (s0x+s1x)/2, my = (s0y+s1y)/2;
        line(bux, buy, mx,  my,  CLR_BROWNISH_BLACK);

        // spine + arm
        line(bux, buy, chx, chy, CLR_BROWNISH_BLACK);
        line(chx, chy, hax, hay, CLR_BROWNISH_BLACK);

        // head — offset slightly forward along spine direction
        int hdx = chx + (chx-bux)/5;
        int hdy = chy - 5;
        circfill(hdx, hdy, 4, CLR_LIGHT_PEACH);
        circ(    hdx, hdy, 4, CLR_DARK_BROWN);
        // eye — on the side the sled is moving toward
        int eye_dx = (vp[1].x > vp[0].x) ? 2 : -2;
        pset(hdx + eye_dx, hdy, CLR_DARK_GREY);
    }

    // HUD (screen-space)
    camera(0, 0);
    if (!playing) {
        print("DRAW", 4, 4, CLR_DARK_BLUE);
        font(FONT_SMALL);
        print("SPACE play   R reset   DEL clear   RMB erase   arrows pan", 4, SCREEN_H-8, CLR_DARK_GREY);
        font(FONT_NORMAL);
        print(str("%d", nseg), SCREEN_W-30, 4, CLR_DARK_GREY);
    } else {
        print("PLAY", 4, 4, CLR_DARK_GREEN);
        font(FONT_SMALL);
        print("SPACE pause+draw   R reset", 4, SCREEN_H-8, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (crashed) {
            print_centered("CRASHED!", SCREEN_W/2, SCREEN_H/2 - 5, CLR_RED);
            font(FONT_SMALL);
            print_centered("SPACE to draw   R to retry", SCREEN_W/2, SCREEN_H/2 + 5, CLR_DARK_GREY);
            font(FONT_NORMAL);
        }
    }
}
