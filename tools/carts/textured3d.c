#include "studio.h"
#include <math.h>

// TEXTURED 3D — the full PS1 / picoCAD look: affine texture-mapped polygons
// plus dithered shading. This is `solid3d` with pictures glued on.
//
// tritex() maps each screen corner to a pixel on the sprite sheet and lets the
// GPU smear the texture across the triangle in SCREEN space — no perspective
// correction, so textures warp and swim exactly like a PlayStation 1.
//
// THE WARP, AND HOW TO TAME IT: a quad is two triangles, each interpolated on
// its own. Viewed at an angle, the texture creases along the shared diagonal —
// the classic affine artifact. The fix games used was SUBDIVISION: chop each
// face into an N×N grid and re-project every sub-corner with its own
// perspective divide. Smaller patches = smaller error = the crease melts away.
// Press X to crank the subdivision level (1→8) and watch it happen.
//
//   Z  shading on/off     X  subdivision     up/down  zoom     L/R  spin
//
// (V3, rot3, project, zsort and quadfill are engine helpers now — see studio.h.)

static V3 cubeV[8] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
// 6 quads (corners go around the face: TL,TR,BR,BL) + which sprite slot textures it
static int quad[6][4] = {
    {4,5,6,7},  // front
    {1,0,3,2},  // back
    {5,1,2,6},  // right
    {0,4,7,3},  // left
    {0,1,5,4},  // top
    {3,2,6,7},  // bottom
};
static int quadTex[6] = { 0, 1, 0, 1, 0, 1 };   // slot 0 = bricks, slot 1 = crate

static float ax = 22, ay = 32, spin = 1.0f, zoom = 55;
static bool  shade = true;
static int   sub = 4;            // subdivision level (1 = raw two-triangle warp)

#define FOCAL 4.0f
#define TEX 16                   // each texture is 16×16 on the sheet
#define QUARTER 0x7BDE           // ~25% black dither (1-bits transparent)

// project a rotated (camera-space) point to the screen — wraps the engine's
// project(), with this cart's small -4 vertical framing nudge.
static int PX(V3 r) { int x = 0; project3(r, FOCAL, zoom, &x, 0); return x; }
static int PY(V3 r) { int y = 0; project3(r, FOCAL, zoom, 0, &y); return y - 4; }
// bilinear point inside a quad: top edge A→B, bottom edge D→C, then lerp by t
static V3 quadpt(V3 A, V3 B, V3 C, V3 D, float s, float t) {
    V3 top = { A.x+(B.x-A.x)*s, A.y+(B.y-A.y)*s, A.z+(B.z-A.z)*s };
    V3 bot = { D.x+(C.x-D.x)*s, D.y+(C.y-D.y)*s, D.z+(C.z-D.z)*s };
    return (V3){ top.x+(bot.x-top.x)*t, top.y+(bot.y-top.y)*t, top.z+(bot.z-top.z)*t };
}

void update(void) {
    if (btnp(0, BTN_A)) shade = !shade;
    if (btnp(0, BTN_B)) sub = sub >= 8 ? 1 : sub * 2;     // 1 → 2 → 4 → 8 → 1
    if (btn(0, BTN_UP))    zoom = min(110, zoom + 1.5f);
    if (btn(0, BTN_DOWN))  zoom = max(25,  zoom - 1.5f);
    if (btnp(0, BTN_LEFT))  spin = max(0, spin - 0.4f);
    if (btnp(0, BTN_RIGHT)) spin = min(4, spin + 0.4f);
    ax += 0.5f * spin; ay += 0.8f * spin;
}

void draw(void) {
    cls(CLR_BLACK);
    fillp(FILL_CHECKER, CLR_BLACK);
    rectfill(0, SCREEN_H - 28, SCREEN_W, 28, CLR_DARKER_BLUE);   // dithered ground
    fillp_reset();

    // rotate + project all 8 corners (the face-level positions, for cull/sort/shadow)
    static int sx[8], sy[8]; static V3 rv[8];
    for (int i = 0; i < 8; i++) {
        rv[i] = rot3(cubeV[i], ay, ax);
        sx[i] = PX(rv[i]); sy[i] = PY(rv[i]);
    }

    float lx = -0.5f, ly = -0.6f, lz = -0.62f;   // light direction

    // gather visible faces with depth + brightness
    typedef struct { int q; float z; float lam; } Face;
    Face face[6]; int nf = 0;
    for (int i = 0; i < 6; i++) {
        int *Q = quad[i];
        V3 a = rv[Q[0]], b = rv[Q[1]], c = rv[Q[2]], d = rv[Q[3]];
        float ux=b.x-a.x, uy=b.y-a.y, uz=b.z-a.z;
        float vx=c.x-a.x, vy=c.y-a.y, vz=c.z-a.z;
        float nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx;
        float cx=(a.x+b.x+c.x+d.x)/4, cy=(a.y+b.y+c.y+d.y)/4, cz=(a.z+b.z+c.z+d.z)/4;
        if (nx*cx+ny*cy+nz*cz < 0) { nx=-nx; ny=-ny; nz=-nz; }
        if (nz >= 0) continue;                                  // back face → skip
        float len = sqrtf(nx*nx+ny*ny+nz*nz); if (len < 0.0001f) len = 1;
        float lam = (nx*lx+ny*ly+nz*lz)/len; if (lam < 0) lam = 0;
        face[nf].q = i; face[nf].z = cz; face[nf].lam = lam; nf++;
    }
    // painter's sort: far first — zsort orders the faces for us
    float fz[6]; int order[6];
    for (int i = 0; i < nf; i++) fz[i] = face[i].z;
    zsort(fz, order, nf);

    // draw each face as an N×N grid of textured patches, then a dither shadow
    for (int i = 0; i < nf; i++) {
        Face fc = face[order[i]];
        int *Q = quad[fc.q];
        int uo = quadTex[fc.q] * TEX;
        V3 A=rv[Q[0]], B=rv[Q[1]], C=rv[Q[2]], D=rv[Q[3]];      // TL TR BR BL
        for (int gy = 0; gy < sub; gy++) {
            for (int gx = 0; gx < sub; gx++) {
                float s0 = (float)gx/sub,     t0 = (float)gy/sub;
                float s1 = (float)(gx+1)/sub, t1 = (float)(gy+1)/sub;
                V3 p00=quadpt(A,B,C,D,s0,t0), p10=quadpt(A,B,C,D,s1,t0);
                V3 p11=quadpt(A,B,C,D,s1,t1), p01=quadpt(A,B,C,D,s0,t1);
                int x00=PX(p00),y00=PY(p00), x10=PX(p10),y10=PY(p10);
                int x11=PX(p11),y11=PY(p11), x01=PX(p01),y01=PY(p01);
                float u0=uo+s0*TEX, u1=uo+s1*TEX, v0=t0*TEX, v1=t1*TEX;
                tritex(x00,y00,u0,v0, x10,y10,u1,v0, x11,y11,u1,v1);
                tritex(x00,y00,u0,v0, x11,y11,u1,v1, x01,y01,u0,v1);
            }
        }
        if (shade) {
            int x0=sx[Q[0]],y0=sy[Q[0]], x1=sx[Q[1]],y1=sy[Q[1]];
            int x2=sx[Q[2]],y2=sy[Q[2]], x3=sx[Q[3]],y3=sy[Q[3]];
            int pat = fc.lam > 0.66f ? 0 : fc.lam > 0.33f ? QUARTER : FILL_CHECKER;
            if (pat) {
                fillp(pat, -1);                       // black on 0-bits, holes transparent
                quadfill(x0,y0, x1,y1, x2,y2, x3,y3, CLR_BLACK);
                fillp_reset();
            }
        }
    }

    print("TEXTURED CUBE  (tritex + dither)", 4, 4, CLR_WHITE);
    print(str("SUBDIVISION: %dx%d  %s", sub, sub, sub == 1 ? "(raw affine warp)" : "(perspective-tamed)"),
          4, 13, sub == 1 ? CLR_ORANGE : CLR_LIME_GREEN);
    print("Z shading   X subdivide   up/down zoom   L/R spin", 4, SCREEN_H - 9, CLR_LIGHT_GREY);
}
