/* de:meta
{
  "slug": "dreamcad",
  "title": "dreamcad",
  "status": "active",
  "created": "2026-06-02",
  "kind": [
    "tool"
  ],
  "teaches": [
    "software-rasterizer"
  ],
  "lineage": "Inspired by picoCAD; implements perspective + orthographic projection, painter's-algorithm depth sort, UV mapping, extrusion, and orbit camera entirely in dreamengine's 2D draw API via tritex/trifill.",
  "homage": "picoCAD (2020)",
  "description": "A picoCAD-style 3D model editor. Orbit camera in perspective view, three orthographic views (Tab to cycle). Object list in the sidebar. v1: view the default cube, orbit and zoom with mouse. More editing coming soon."
}
de:meta */
// ════════════════════════════════════════════════════════════════════════════
// dreamcad — picoCAD-style 3D model editor
// ════════════════════════════════════════════════════════════════════════════
//
// WHAT'S IN HERE
//   Perspective + 3 ortho views + quad split (Tab), ortho toggle (O)
//   Orbit camera — turntable, unlimited pitch, live FOV ([/])
//   Vertex select/drag (LMB), face select + color picker
//   Extrude (E), flip normal (F), delete face/vert (DEL), snap grid (G)
//   Single-level undo (Z), auto-save on every edit (save_bytes)
//   Add mesh: cube / plane / pyramid / diamond (+ add in sidebar)
//   Rename objects (re-click name), RMB drag to move whole object
//   UV editor (U) — sprite-sheet backed, drag 4 UV corners per face
//   tritex() for textured faces, trifill() for flat-color faces
//   Face normal debug overlay (N), reset to cube (R), help panel (H)
//
// IF YOU REVISIT — things worth adding
//
//   OBJECT OPS
//     - Delete whole object (currently you can only delete face/vert)
//     - Duplicate object (copy + offset)
//     - Toggle object visibility (sidebar "o" shows state, doesn't click)
//     - Object-level rename via double-click already works; add confirm on click-away
//
//   EDITING
//     - Multi-vertex select: shift-click or box-drag to move several verts at once
//     - Double-sided face flag UI: FACE_DOUBLE is in the data model but has no toggle
//     - Extrude along axis (X/Y/Z constrained), not just face normal
//     - Scale object (uniform S key, stretch individual axes)
//     - Merge vertices that are close together (weld)
//     - Loop cuts / edge loop inserts
//
//   UV EDITOR
//     - Move whole UV quad at once (not just individual corners)
//     - Snap UV corners to sprite-sheet grid (8px boundaries)
//     - Scale / rotate the UV quad (R/S in UV mode)
//     - Copy UV from one face to another
//     - Show 3D preview thumbnail alongside the UV sheet
//
//   RENDERING / DISPLAY
//     - Flat shading: darken back-facing faces using fillp() dither
//       (dot face-normal with a light direction, map to FILL_CHECKER etc.)
//     - Smooth shading: average normals at shared vertices
//     - Wireframe-only mode (no face fill) for cleaner ortho editing
//     - Backface indicator in 3D view (dim tint instead of fully visible)
//
//   CAMERA
//     - Pan (middle-mouse or Alt+drag) so you can work on off-center models
//     - Focus on selected object (F key: recentre cam on sel_obj bbox)
//     - Camera presets: front / right / top / isometric (numpad-style shortcuts)
//
//   PERSISTENCE / EXPORT
//     - Multiple save slots (currently one slot per cart)
//     - Export model as C struct so you can paste it into another cart
//     - Load another cart's embedded model into the scene
//
// ════════════════════════════════════════════════════════════════════════════
#include "studio.h"
#include "cursor.h"
#include <math.h>
#include <string.h>
#include <stdint.h>

// ── limits ────────────────────────────────────────────────────────────────────
#define MAX_OBJECTS  16
#define MAX_VERTS    64
#define MAX_FACES    64
#define TEX_W        64
#define TEX_H        64

// ── views ─────────────────────────────────────────────────────────────────────
#define VIEW_PERSP   0
#define VIEW_TOP     1
#define VIEW_FRONT   2
#define VIEW_SIDE    3
#define VIEW_QUAD    4
static const char *VIEW_NAMES[] = { "PERSP","TOP","FRONT","SIDE","QUAD" };

// perspective focal length — higher = narrower FoV, less distortion. tweakable live with [ / ]
static float focal = 220.0f;

// ── UI layout (320×200) ───────────────────────────────────────────────────────
#define TOP_H    11
#define BOT_H    10
#define SIDE_W   64
#define VP_X     SIDE_W
#define VP_Y     TOP_H
#define VP_W     (SCREEN_W - SIDE_W)
#define VP_H     (SCREEN_H - TOP_H - BOT_H)
#define VP_CX    (VP_X + VP_W / 2)
#define VP_CY    (VP_Y + VP_H / 2)

// UV editor — sprite sheet shown at 1:1 centred in the viewport
#define UV_OX  (VP_X + (VP_W - 128) / 2)   // 128
#define UV_OY  (VP_Y + (VP_H - 128) / 2)   // 36

// color picker dimensions (4 rows × 8 cols of 6×6 swatches with 1px gap)
#define CP_COLS  8
#define CP_ROWS  4
#define CP_SW    6
#define CP_SH    6
#define CP_GAP   1
#define CP_X     4
#define CP_Y     (SCREEN_H - BOT_H - CP_ROWS*(CP_SH+CP_GAP) - 4)

// ── data model ────────────────────────────────────────────────────────────────
typedef struct { float x, y, z; } Vec3;

typedef struct {
    int   vi[4];
    float fu[4], fv[4];
    int   color;
    int   flags;
} Face;

#define FACE_DOUBLE  1
#define FACE_NOTEX   2
#define FACE_SHADE   4

typedef struct {
    char  name[16];
    Vec3  verts[MAX_VERTS];
    int   vert_count;
    Face  faces[MAX_FACES];
    int   face_count;
    bool  visible;
} Object;

typedef struct {
    Object        objects[MAX_OBJECTS];
    int           object_count;
    unsigned char tex[TEX_W * TEX_H];
} Scene;

// ── globals ───────────────────────────────────────────────────────────────────
static Scene scene;
static int   view_mode   = VIEW_PERSP;
static int   sel_obj     = 0;
static float cam_yaw     =  0.5f;
static float cam_pitch   =  0.3f;
static float cam_dist    =  4.0f;
static float ortho_scale =  50.0f;

static bool  dragging    = false;
static int   drag_sx, drag_sy;  // previous mouse position, updated per-frame

static int   sel_vert    = -1;
static int   hover_vert  = -1;
static bool  vdragging   = false;
static int   vdrag_sx, vdrag_sy;
static int   vdrag_view;
static float vdrag_cx, vdrag_cy, vdrag_os, vdrag_pz;

static int   sel_face    = -1;
static int   hover_face  = -1;

static bool  snap_on     = false;
static int   save_flash  = 0;
static bool  show_help   = false;
static bool  show_normals = false;
static bool  ortho_mode   = false;

// UV editor
static bool  uv_mode         = false;
static int   uv_sel_corner   = -1;
static int   uv_hover_corner = -1;

// rename
static bool  renaming    = false;
static int   rename_obj  = -1;
static char  rename_buf[16];
static int   rename_len  = 0;

// add-mesh menu
static bool  show_add_menu = false;

// object drag (right-click)
static bool  obj_dragging = false;
static int   obj_drag_sx, obj_drag_sy;
static int   obj_drag_view;
static float obj_drag_cx, obj_drag_cy, obj_drag_os, obj_drag_pz;

// single-level undo (stores one object's geometry)
static Vec3  undo_verts[MAX_VERTS];
static int   undo_vc;
static Face  undo_faces[MAX_FACES];
static int   undo_fc;
static bool  undo_avail  = false;

// per-frame projected vertex cache
static float px[MAX_VERTS], py[MAX_VERTS], pz[MAX_VERTS];
static float fdepth[MAX_FACES];
static int   forder[MAX_FACES];

// ── 3D math ───────────────────────────────────────────────────────────────────
// turntable orbit: pitch around world X first, then yaw around world Y
// this keeps horizontal drag as a clean world-Y spin at all pitch angles
static Vec3 rot_yp(Vec3 p, float yaw, float pitch) {
    float cp = cosf(pitch), sp = sinf(pitch);
    float y1 = p.y*cp - p.z*sp;
    float z1 = p.y*sp + p.z*cp;
    float cy = cosf(yaw), sy = sinf(yaw);
    float x2 =  p.x*cy + z1*sy;
    float z2 = -p.x*sy + z1*cy;
    return (Vec3){ x2, y1, z2 };
}

static Vec3 rot_yp_inv(Vec3 p, float yaw, float pitch) {
    float cy = cosf(yaw), sy = sinf(yaw);
    float x1 = p.x*cy - p.z*sy;
    float z1 = p.x*sy + p.z*cy;
    float cp = cosf(pitch), sp = sinf(pitch);
    return (Vec3){ x1, p.y*cp + z1*sp, -p.y*sp + z1*cp };
}

static Vec3 vec3_add(Vec3 a, Vec3 b)   { return (Vec3){a.x+b.x, a.y+b.y, a.z+b.z}; }
static Vec3 vec3_sub(Vec3 a, Vec3 b)   { return (Vec3){a.x-b.x, a.y-b.y, a.z-b.z}; }
static Vec3 vec3_scale(Vec3 v, float s){ return (Vec3){v.x*s, v.y*s, v.z*s}; }
static Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){ a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x };
}
static Vec3 vec3_norm(Vec3 v) {
    float l = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    return l > 0.0001f ? (Vec3){v.x/l, v.y/l, v.z/l} : v;
}
static Vec3 face_normal(Object *obj, Face *f) {
    return vec3_norm(vec3_cross(
        vec3_sub(obj->verts[f->vi[1]], obj->verts[f->vi[0]]),
        vec3_sub(obj->verts[f->vi[2]], obj->verts[f->vi[0]])));
}

// ── projection ────────────────────────────────────────────────────────────────
static void project_verts(Object *obj, int view, float cx, float cy, float os) {
    for (int i = 0; i < obj->vert_count; i++) {
        Vec3 v = obj->verts[i];
        if (view == VIEW_PERSP) {
            Vec3 r = rot_yp(v, cam_yaw, cam_pitch);
            float z = r.z + cam_dist;
            if (z < 0.05f) z = 0.05f;
            pz[i] = z;
            if (ortho_mode) {
                float s = focal / cam_dist;  // match apparent size to persp at center dist
                px[i] = cx + s * r.x;
                py[i] = cy - s * r.y;
            } else {
                px[i] = cx + focal * r.x / z;
                py[i] = cy - focal * r.y / z;
            }
        } else if (view == VIEW_TOP) {
            pz[i] = -v.y;
            px[i] = cx + os * v.x;
            py[i] = cy + os * v.z;
        } else if (view == VIEW_FRONT) {
            pz[i] = v.z;
            px[i] = cx + os * v.x;
            py[i] = cy - os * v.y;
        } else {
            pz[i] = -v.x;
            px[i] = cx - os * v.z;
            py[i] = cy - os * v.y;
        }
    }
}

// ── geometry ops ──────────────────────────────────────────────────────────────
static void push_undo(Object *obj) {
    memcpy(undo_verts, obj->verts, obj->vert_count * sizeof(Vec3));
    undo_vc = obj->vert_count;
    memcpy(undo_faces, obj->faces, obj->face_count * sizeof(Face));
    undo_fc = obj->face_count;
    undo_avail = true;
}

static void pop_undo(Object *obj) {
    if (!undo_avail) return;
    memcpy(obj->verts, undo_verts, undo_vc * sizeof(Vec3));
    obj->vert_count = undo_vc;
    memcpy(obj->faces, undo_faces, undo_fc * sizeof(Face));
    obj->face_count = undo_fc;
    undo_avail = false;
    sel_vert = -1; sel_face = -1;
}

static void do_extrude(Object *obj, int fi) {
    if (fi < 0 || fi >= obj->face_count) return;
    if (obj->vert_count + 4 > MAX_VERTS) return;
    if (obj->face_count + 4 > MAX_FACES) return;
    push_undo(obj);
    Face *f = &obj->faces[fi];
    Vec3 n = face_normal(obj, f);
    int base = obj->vert_count;
    for (int i = 0; i < 4; i++)
        obj->verts[base+i] = vec3_add(obj->verts[f->vi[i]], vec3_scale(n, 0.5f));
    obj->vert_count += 4;
    int old[4] = { f->vi[0], f->vi[1], f->vi[2], f->vi[3] };
    int fc = obj->face_count;
    for (int i = 0; i < 4; i++) {
        Face *sf = &obj->faces[fc+i];
        sf->vi[0] = old[i]; sf->vi[1] = old[(i+1)%4];
        sf->vi[2] = base+(i+1)%4; sf->vi[3] = base+i;
        sf->color = f->color; sf->flags = FACE_NOTEX;
    }
    obj->face_count += 4;
    for (int i = 0; i < 4; i++) f->vi[i] = base + i;  // cap = new face
    sel_face = fi;
}

static void do_flip_face(Object *obj, int fi) {
    if (fi < 0 || fi >= obj->face_count) return;
    push_undo(obj);
    Face *f = &obj->faces[fi];
    // reverse winding: swap vi[1] and vi[3] to flip the normal
    int tmp = f->vi[1]; f->vi[1] = f->vi[3]; f->vi[3] = tmp;
    float tu, tv;
    tu=f->fu[1]; f->fu[1]=f->fu[3]; f->fu[3]=tu;
    tv=f->fv[1]; f->fv[1]=f->fv[3]; f->fv[3]=tv;
}

static void do_delete_face(Object *obj, int fi) {
    if (fi < 0 || fi >= obj->face_count) return;
    push_undo(obj);
    for (int i = fi; i < obj->face_count-1; i++) obj->faces[i] = obj->faces[i+1];
    obj->face_count--;
    sel_face = -1;
}

static void do_delete_vert(Object *obj, int vi) {
    if (vi < 0 || vi >= obj->vert_count) return;
    push_undo(obj);
    int fi = 0;
    while (fi < obj->face_count) {
        Face *f = &obj->faces[fi];
        bool uses = false;
        for (int k = 0; k < 4; k++) if (f->vi[k] == vi) { uses = true; break; }
        if (uses) {
            for (int k = fi; k < obj->face_count-1; k++) obj->faces[k] = obj->faces[k+1];
            obj->face_count--;
        } else {
            for (int k = 0; k < 4; k++) if (f->vi[k] > vi) f->vi[k]--;
            fi++;
        }
    }
    for (int i = vi; i < obj->vert_count-1; i++) obj->verts[i] = obj->verts[i+1];
    obj->vert_count--;
    sel_vert = -1; sel_face = -1;
}

// ── scene init ────────────────────────────────────────────────────────────────
static void build_default_cube(void) {
    memset(&scene, 0, sizeof(scene));
    scene.object_count = 1;
    Object *obj = &scene.objects[0];
    strcpy(obj->name, "cube");
    obj->visible = true;
    obj->vert_count = 8; obj->face_count = 6;
    float h = 0.5f;
    obj->verts[0]=(Vec3){-h,-h, h}; obj->verts[1]=(Vec3){ h,-h, h};
    obj->verts[2]=(Vec3){ h, h, h}; obj->verts[3]=(Vec3){-h, h, h};
    obj->verts[4]=(Vec3){-h,-h,-h}; obj->verts[5]=(Vec3){ h,-h,-h};
    obj->verts[6]=(Vec3){ h, h,-h}; obj->verts[7]=(Vec3){-h, h,-h};
    int quads[6][4] = {{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{3,2,6,7},{4,5,1,0}};
    int colors[6]   = { CLR_RED, CLR_BLUE, CLR_GREEN, CLR_ORANGE, CLR_YELLOW, CLR_INDIGO };
    for (int i = 0; i < 6; i++) {
        Face *f = &obj->faces[i];
        for (int k = 0; k < 4; k++) f->vi[k] = quads[i][k];
        f->color = colors[i]; f->flags = FACE_NOTEX;
        f->fu[0]=0; f->fv[0]=0;  f->fu[1]=16; f->fv[1]=0;
        f->fu[2]=16;f->fv[2]=16; f->fu[3]=0;  f->fv[3]=16;
    }
}

static void add_object(const char *name, int vc, int fc,
                        Vec3 *verts, int quads[][4], int *colors) {
    if (scene.object_count >= MAX_OBJECTS) return;
    Object *obj = &scene.objects[scene.object_count];
    memset(obj, 0, sizeof(Object));
    strncpy(obj->name, name, 15);
    obj->visible = true;
    obj->vert_count = vc;
    obj->face_count = fc;
    for (int i = 0; i < vc; i++) obj->verts[i] = verts[i];
    for (int i = 0; i < fc; i++) {
        Face *f = &obj->faces[i];
        for (int k = 0; k < 4; k++) f->vi[k] = quads[i][k];
        f->color = colors[i]; f->flags = FACE_NOTEX;
    }
    sel_obj = scene.object_count++;
    sel_vert = -1; sel_face = -1;
}

static void add_cube(void) {
    float h = 0.5f;
    Vec3 v[] = {
        {-h,-h, h},{h,-h, h},{h, h, h},{-h, h, h},
        {-h,-h,-h},{h,-h,-h},{h, h,-h},{-h, h,-h}
    };
    int q[][4] = {{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{3,2,6,7},{4,5,1,0}};
    int c[] = { CLR_RED, CLR_BLUE, CLR_GREEN, CLR_ORANGE, CLR_YELLOW, CLR_INDIGO };
    add_object("cube", 8, 6, v, q, c);
}

static void add_plane(void) {
    float h = 0.5f;
    Vec3 v[] = { {-h,0,-h},{h,0,-h},{h,0,h},{-h,0,h} };
    int q[][4] = {{0,1,2,3}};
    int c[] = { CLR_LIGHT_GREY };
    add_object("plane", 4, 1, v, q, c);
}

static void add_pyramid(void) {
    float h = 0.5f;
    Vec3 v[] = {
        {-h,0,-h},{h,0,-h},{h,0,h},{-h,0,h},{0,1,0}
    };
    int q[][4] = {
        {0,1,2,3},   // base  (normal -Y)
        {3,2,4,4},   // front (normal +Z)
        {2,1,4,4},   // right (normal +X)
        {1,0,4,4},   // back  (normal -Z)
        {0,3,4,4},   // left  (normal -X)
    };
    int c[] = { CLR_INDIGO, CLR_RED, CLR_ORANGE, CLR_BLUE, CLR_GREEN };
    add_object("pyramid", 5, 5, v, q, c);
}

static void add_diamond(void) {
    Vec3 v[] = {
        {0, 1,0},{0,-1,0},
        {1,0, 0},{-1,0,0},{0,0, 1},{0,0,-1}
    };
    int q[][4] = {
        {0,2,4,4},{0,4,3,3},{0,3,5,5},{0,5,2,2},
        {1,4,2,2},{1,3,4,4},{1,5,3,3},{1,2,5,5},
    };
    int c[] = {
        CLR_RED,CLR_ORANGE,CLR_YELLOW,CLR_GREEN,
        CLR_BLUE,CLR_INDIGO,CLR_PINK,CLR_LIGHT_PEACH
    };
    add_object("diamond", 6, 8, v, q, c);
}

static void init_scene(void) {
    if (load_bytes(&scene, sizeof(scene)) == sizeof(scene)) {
        if (scene.object_count >= 1 && scene.object_count <= MAX_OBJECTS) return;
    }
    build_default_cube();
}

// ── 2D hit testing ────────────────────────────────────────────────────────────
static bool pt_in_tri(float tx, float ty,
                      float ax, float ay, float bx, float by,
                      float cx, float cy) {
    float d1 = (tx-ax)*(by-ay) - (bx-ax)*(ty-ay);
    float d2 = (tx-bx)*(cy-by) - (cx-bx)*(ty-by);
    float d3 = (tx-cx)*(ay-cy) - (ax-cx)*(ty-cy);
    bool has_neg = (d1<0)||(d2<0)||(d3<0);
    bool has_pos = (d1>0)||(d2>0)||(d3>0);
    return !(has_neg && has_pos);
}

static int face_at(Object *obj, float mx, float my) {
    // iterate front-to-back so first hit is the topmost visible face
    for (int si = obj->face_count-1; si >= 0; si--) {
        int fi = forder[si];
        Face *f = &obj->faces[fi];
        // only skip faces that are strongly back-facing — still allow selecting edge-on faces
        if (!(f->flags & FACE_DOUBLE)) {
            Vec3 rn = rot_yp(face_normal(obj, f), cam_yaw, cam_pitch);
            if (rn.z < -0.5f) continue;
        }
        float x0=px[f->vi[0]], y0=py[f->vi[0]];
        float x1=px[f->vi[1]], y1=py[f->vi[1]];
        float x2=px[f->vi[2]], y2=py[f->vi[2]];
        float x3=px[f->vi[3]], y3=py[f->vi[3]];
        if (pt_in_tri(mx,my, x0,y0,x1,y1,x2,y2) ||
            pt_in_tri(mx,my, x0,y0,x2,y2,x3,y3))
            return fi;
    }
    return -1;
}

// ── rendering ─────────────────────────────────────────────────────────────────
static void draw_grid(void) {
    for (int i = -8; i <= 8; i++) {
        int gx = VP_CX + (int)(ortho_scale * i);
        int gy = VP_CY + (int)(ortho_scale * i);
        int gc = (i == 0) ? CLR_DARK_BLUE : CLR_BROWNISH_BLACK;
        line(gx, VP_Y, gx, VP_Y+VP_H-1, gc);
        line(VP_X, gy, VP_X+VP_W-1, gy, gc);
    }
}

static void draw_face_filled(Face *f) {
    int x0=(int)px[f->vi[0]], y0=(int)py[f->vi[0]];
    int x1=(int)px[f->vi[1]], y1=(int)py[f->vi[1]];
    int x2=(int)px[f->vi[2]], y2=(int)py[f->vi[2]];
    int x3=(int)px[f->vi[3]], y3=(int)py[f->vi[3]];
    if (f->flags & FACE_NOTEX) {
        trifill(x0,y0, x1,y1, x2,y2, f->color);
        trifill(x0,y0, x2,y2, x3,y3, f->color);
    } else {
        tritex(x0,y0, f->fu[0],f->fv[0],
               x1,y1, f->fu[1],f->fv[1],
               x2,y2, f->fu[2],f->fv[2]);
        tritex(x0,y0, f->fu[0],f->fv[0],
               x2,y2, f->fu[2],f->fv[2],
               x3,y3, f->fu[3],f->fv[3]);
    }
}

static void draw_face_wire(Face *f, int color) {
    for (int i = 0; i < 4; i++) {
        int a = f->vi[i], b = f->vi[(i+1)%4];
        line((int)px[a],(int)py[a], (int)px[b],(int)py[b], color);
    }
}

static void draw_objects(int view, float cx, float cy, float os, int wsel) {
    for (int oi = 0; oi < scene.object_count; oi++) {
        Object *obj = &scene.objects[oi];
        if (!obj->visible) continue;
        project_verts(obj, view, cx, cy, os);
        for (int fi = 0; fi < obj->face_count; fi++) {
            float d = 0.0f;
            for (int vi = 0; vi < 4; vi++) d += pz[obj->faces[fi].vi[vi]];
            fdepth[fi] = d * 0.25f;
        }
        zsort(fdepth, forder, obj->face_count);
        for (int si = 0; si < obj->face_count; si++)
            draw_face_filled(&obj->faces[forder[si]]);
        int wcol = (oi == wsel) ? CLR_WHITE : CLR_DARKER_GREY;
        for (int fi = 0; fi < obj->face_count; fi++)
            draw_face_wire(&obj->faces[fi], wcol);
        if (oi == wsel) {
            if (hover_face >= 0 && hover_face != sel_face)
                draw_face_wire(&obj->faces[hover_face], CLR_PINK);
            if (sel_face >= 0)
                draw_face_wire(&obj->faces[sel_face], CLR_YELLOW);
        }
    }
    // vertex dots for selected object
    if (wsel >= 0 && wsel < scene.object_count && scene.objects[wsel].visible) {
        Object *obj = &scene.objects[wsel];
        project_verts(obj, view, cx, cy, os);
        for (int vi = 0; vi < obj->vert_count; vi++) {
            int vx=(int)px[vi], vy=(int)py[vi];
            int vc = (vi == sel_vert)   ? CLR_YELLOW :
                     (vi == hover_vert) ? CLR_PINK   : CLR_MEDIUM_GREY;
            rectfill(vx-1, vy-1, 3, 3, vc);
        }
        // face normal debug overlay
        if (show_normals && view == VIEW_PERSP) {
            for (int fi = 0; fi < obj->face_count; fi++) {
                Face *f = &obj->faces[fi];
                // face center in screen space
                float fcx = (px[f->vi[0]]+px[f->vi[1]]+px[f->vi[2]]+px[f->vi[3]])*0.25f;
                float fcy = (py[f->vi[0]]+py[f->vi[1]]+py[f->vi[2]]+py[f->vi[3]])*0.25f;
                // project face center + normal tip to screen
                Vec3 wc = { (obj->verts[f->vi[0]].x+obj->verts[f->vi[1]].x+
                             obj->verts[f->vi[2]].x+obj->verts[f->vi[3]].x)*0.25f,
                            (obj->verts[f->vi[0]].y+obj->verts[f->vi[1]].y+
                             obj->verts[f->vi[2]].y+obj->verts[f->vi[3]].y)*0.25f,
                            (obj->verts[f->vi[0]].z+obj->verts[f->vi[1]].z+
                             obj->verts[f->vi[2]].z+obj->verts[f->vi[3]].z)*0.25f };
                Vec3 n  = face_normal(obj, f);
                Vec3 tip = { wc.x+n.x*0.4f, wc.y+n.y*0.4f, wc.z+n.z*0.4f };
                Vec3 rt  = rot_yp(tip, cam_yaw, cam_pitch);
                float tz = rt.z + cam_dist; if (tz < 0.05f) tz = 0.05f;
                float tx = cx + focal * rt.x / tz;
                float ty_ = cy - focal * rt.y / tz;
                // color: green=front-facing, red=back-facing
                Vec3 rn = rot_yp(n, cam_yaw, cam_pitch);
                int nc = (rn.z > 0.0f) ? CLR_GREEN :
                         (rn.z < -0.3f) ? CLR_RED : CLR_WHITE;
                line((int)fcx,(int)fcy, (int)tx,(int)ty_, nc);
                rectfill((int)tx-1,(int)ty_-1, 3,3, nc);
            }
        }
    }
}

static void draw_help(void) {
    // floating help panel, top-right of viewport
    int pw = 118, ph = 157;
    int px_ = VP_X + VP_W - pw - 4;
    int py_ = VP_Y + 4;
    rectfill(px_, py_, pw, ph, CLR_DARKER_BLUE);
    rect(px_, py_, pw, ph, CLR_DARK_BLUE);
    font(FONT_TINY);
    int x = px_+3, y = py_+3;
    int lh = 7;  // line height for FONT_TINY
    int kc = CLR_YELLOW, dc = CLR_LIGHT_GREY, sc = CLR_DARK_GREY;
    print("DREAMCAD  H:close", x, y, CLR_WHITE); y += lh+1;
    line(px_, y, px_+pw-1, y, CLR_DARK_BLUE); y += 2;
    print("LMB", x, y, kc); print("sel vert/face", x+18, y, dc); y += lh;
    print("RMB", x, y, kc); print("move object",   x+18, y, dc); y += lh;
    line(px_, y, px_+pw-1, y, CLR_DARK_BLUE); y += 2;
    print("TAB", x, y, kc); print("cycle views", x+18, y, dc); y += lh;
    print("S",   x, y, kc); print("save",        x+18, y, dc); y += lh;
    print("Z",   x, y, kc); print("undo",        x+18, y, dc); y += lh;
    print("G",   x, y, kc); print("snap 0.25",   x+18, y, dc); y += lh;
    print("R",   x, y, kc); print("reset cube",  x+18, y, dc); y += lh;
    print("N",   x, y, kc); print("normals dbg", x+18, y, dc); y += lh;
    print("O",   x, y, kc); print("ortho toggle",x+18, y, dc); y += lh;
    print("U",   x, y, kc); print("UV editor",   x+18, y, dc); y += lh;
    print("[/]", x, y, kc); print("fov",         x+18, y, dc); y += lh;
    line(px_, y, px_+pw-1, y, CLR_DARK_BLUE); y += 2;
    print("face selected:", x, y, sc); y += lh;
    print("E",   x, y, kc); print("extrude",     x+18, y, dc); y += lh;
    print("F",   x, y, kc); print("flip normal", x+18, y, dc); y += lh;
    print("T",   x, y, kc); print("tex on/off",  x+18, y, dc); y += lh;
    print("DEL", x, y, kc); print("delete face", x+18, y, dc); y += lh;
    print("click color",   x, y, sc); y += lh;
    line(px_, y, px_+pw-1, y, CLR_DARK_BLUE); y += 2;
    print("vert selected:", x, y, sc); y += lh;
    print("drag",x, y, kc); print("move",        x+18, y, dc); y += lh;
    print("DEL", x, y, kc); print("delete vert", x+18, y, dc);
    font(FONT_NORMAL);
}

static void draw_uv_editor(void) {
    rectfill(VP_X, VP_Y, VP_W, VP_H, CLR_BROWNISH_BLACK);

    // sprite sheet at 1:1
    sspr(0, 0, 128, 128, UV_OX, UV_OY, 128, 128);

    // grid at sprite boundaries (every 8px)
    for (int i = 0; i <= 128; i += 8) {
        line(UV_OX+i, UV_OY, UV_OX+i, UV_OY+127, CLR_DARKER_GREY);
        line(UV_OX, UV_OY+i, UV_OX+127, UV_OY+i, CLR_DARKER_GREY);
    }

    // UV quad for selected face
    if (sel_face >= 0 && sel_obj >= 0) {
        Face *f = &scene.objects[sel_obj].faces[sel_face];
        for (int i = 0; i < 4; i++) {
            int a = i, b = (i+1)%4;
            line(UV_OX+(int)f->fu[a], UV_OY+(int)f->fv[a],
                 UV_OX+(int)f->fu[b], UV_OY+(int)f->fv[b], CLR_YELLOW);
        }
        for (int i = 0; i < 4; i++) {
            int cx = UV_OX+(int)f->fu[i];
            int cy = UV_OY+(int)f->fv[i];
            int col = (i == uv_sel_corner)   ? CLR_YELLOW :
                      (i == uv_hover_corner) ? CLR_PINK   : CLR_WHITE;
            rectfill(cx-2, cy-2, 5, 5, col);
        }
    }

    font(FONT_SMALL);
    print("UV  ESC:back", VP_X+3, VP_Y+3, CLR_DARK_GREY);
    if (sel_face >= 0 && sel_obj >= 0) {
        bool textured = !(scene.objects[sel_obj].faces[sel_face].flags & FACE_NOTEX);
        int tc = textured ? CLR_GREEN : CLR_RED;
        int tx = SCREEN_W - 4;
        tx = print(textured ? "TEX ON" : "TEX OFF", tx - text_width(textured ? "TEX ON" : "TEX OFF"), VP_Y+3, tc) - 4;
        print("T:", tx - text_width("T:"), VP_Y+3, CLR_DARK_GREY);
    }
    font(FONT_NORMAL);
}

static void handle_uv_input(void) {
    if (keyp(KEY_ESCAPE) || keyp('U')) { uv_mode = false; return; }

    if (keyp('T') && sel_face >= 0 && sel_obj >= 0) {
        scene.objects[sel_obj].faces[sel_face].flags ^= FACE_NOTEX;
        save_bytes(&scene, sizeof(scene));
    }

    int mx = mouse_x(), my = mouse_y();

    // hover detection
    uv_hover_corner = -1;
    if (sel_face >= 0 && sel_obj >= 0) {
        Face *f = &scene.objects[sel_obj].faces[sel_face];
        for (int i = 0; i < 4; i++) {
            float dx = mx - (UV_OX + f->fu[i]);
            float dy = my - (UV_OY + f->fv[i]);
            if (dx*dx + dy*dy < 25.0f) { uv_hover_corner = i; break; }
        }
    }

    // press: select corner
    if (mouse_pressed(MOUSE_LEFT)) uv_sel_corner = uv_hover_corner;

    // drag: move selected corner
    if (uv_sel_corner >= 0 && mouse_down(MOUSE_LEFT) && sel_face >= 0 && sel_obj >= 0) {
        Face *f = &scene.objects[sel_obj].faces[sel_face];
        float u = mx - UV_OX;
        float v = my - UV_OY;
        if (u <   0) u =   0;
        if (u > 127) u = 127;
        if (v <   0) v =   0;
        if (v > 127) v = 127;
        f->fu[uv_sel_corner] = u;
        f->fv[uv_sel_corner] = v;
    }
    if (mouse_released(MOUSE_LEFT) && uv_sel_corner >= 0) {
        save_bytes(&scene, sizeof(scene));
        uv_sel_corner = -1;
    }
}

static void draw_scene(void) {
    rectfill(VP_X, VP_Y, VP_W, VP_H, CLR_BROWNISH_BLACK);
    if (view_mode == VIEW_QUAD) {
        int qw = VP_W/2, qh = VP_H/2;
        int svs[4] = { VIEW_TOP, VIEW_FRONT, VIEW_SIDE, VIEW_PERSP };
        int sxs[4] = { VP_X, VP_X+qw, VP_X, VP_X+qw };
        int sys[4] = { VP_Y, VP_Y, VP_Y+qh, VP_Y+qh };
        int sws[4] = { qw, VP_W-qw, qw, VP_W-qw };
        int shs[4] = { qh, qh, VP_H-qh, VP_H-qh };
        float qos  = ortho_scale * 0.5f;
        for (int si = 0; si < 4; si++) {
            float cx = sxs[si] + sws[si]*0.5f;
            float cy = sys[si] + shs[si]*0.5f;
            if (svs[si] != VIEW_PERSP) {
                for (int i = -8; i <= 8; i++) {
                    int gx=(int)(cx+qos*i), gy=(int)(cy+qos*i);
                    int gc = (i==0) ? CLR_DARK_BLUE : CLR_BROWNISH_BLACK;
                    line(gx, sys[si], gx, sys[si]+shs[si]-1, gc);
                    line(sxs[si], gy, sxs[si]+sws[si]-1, gy, gc);
                }
            }
            draw_objects(svs[si], cx, cy, qos, sel_obj);
            font(FONT_SMALL);
            print(VIEW_NAMES[svs[si]], sxs[si]+2, sys[si]+2, CLR_DARK_GREY);
            font(FONT_NORMAL);
        }
        line(VP_X+qw, VP_Y, VP_X+qw, VP_Y+VP_H-1, CLR_DARK_GREY);
        line(VP_X, VP_Y+qh, VP_X+VP_W-1, VP_Y+qh, CLR_DARK_GREY);
        return;
    }
    if (view_mode != VIEW_PERSP) draw_grid();
    draw_objects(view_mode, (float)VP_CX, (float)VP_CY, ortho_scale, sel_obj);
    font(FONT_SMALL);
    const char *vlab = (view_mode == VIEW_PERSP && ortho_mode) ? "ORTHO" : VIEW_NAMES[view_mode];
    print(vlab, VP_X+3, VP_Y+3, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

static void draw_topbar(void) {
    rectfill(0, 0, SCREEN_W, TOP_H, CLR_DARKER_BLUE);
    line(0, TOP_H-1, SCREEN_W-1, TOP_H-1, CLR_DARK_BLUE);
    font(FONT_SMALL);
    int tx = 3;
    tx = print("FILE", tx, 3, CLR_WHITE) + 5;
    tx = print("MESH", tx, 3, CLR_WHITE) + 5;
    tx = print("FACE", tx, 3, CLR_WHITE) + 5;
         print("UV",   tx, 3, uv_mode ? CLR_YELLOW : CLR_WHITE);
    if (scene.object_count > 0) {
        const char *name = (renaming && rename_obj == sel_obj) ? rename_buf : scene.objects[sel_obj].name;
        int nc = (renaming && rename_obj == sel_obj) ? CLR_ORANGE : CLR_YELLOW;
        print(name, VP_CX - text_width(name)/2, 3, nc);
    }
    if (save_flash > 0) {
        print("SAVED", SCREEN_W-27, 3, CLR_GREEN);
        save_flash--;
    }
    font(FONT_NORMAL);
}

static void draw_colorpicker(void) {
    font(FONT_SMALL);
    print("COLOR", CP_X, CP_Y-8, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
    for (int i = 0; i < 32; i++) {
        int col = i % CP_COLS;
        int row = i / CP_COLS;
        int x = CP_X + col * (CP_SW + CP_GAP);
        int y = CP_Y + row * (CP_SH + CP_GAP);
        rectfill(x, y, CP_SW, CP_SH, i);
        if (sel_face >= 0 && sel_obj >= 0 &&
            scene.objects[sel_obj].faces[sel_face].color == i)
            rect(x-1, y-1, CP_SW+2, CP_SH+2, CLR_YELLOW);
    }
}

static void draw_sidebar(void) {
    rectfill(0, TOP_H, SIDE_W, SCREEN_H-TOP_H, CLR_DARKER_BLUE);
    line(SIDE_W-1, TOP_H, SIDE_W-1, SCREEN_H-1, CLR_DARK_BLUE);
    font(FONT_SMALL);
    print("OBJECTS", 3, TOP_H+3, CLR_LIGHT_GREY);
    line(0, TOP_H+11, SIDE_W-2, TOP_H+11, CLR_DARK_BLUE);
    for (int i = 0; i < scene.object_count; i++) {
        int y = TOP_H + 14 + i*10;
        if (i == sel_obj) rectfill(1, y-1, SIDE_W-3, 9, CLR_DARK_BLUE);
        int col = (i == sel_obj) ? CLR_WHITE : CLR_LIGHT_GREY;
        print(scene.objects[i].visible ? "o" : " ", 3, y, CLR_DARK_GREY);
        if (renaming && i == sel_obj) {
            // show edit buffer with blinking cursor
            int tx = print(rename_buf, 11, y, CLR_YELLOW);
            if ((int)(now() * 2) % 2 == 0) print("|", tx, y, CLR_YELLOW);
        } else {
            print(scene.objects[i].name, 11, y, col);
        }
    }
    // + ADD button
    int add_y = TOP_H + 14 + scene.object_count * 10 + 2;
    int add_col = show_add_menu ? CLR_YELLOW : CLR_DARK_GREY;
    print("+ add", 3, add_y, add_col);
    font(FONT_NORMAL);
    line(0, CP_Y-10, SIDE_W-2, CP_Y-10, CLR_DARK_BLUE);
    draw_colorpicker();

    // add-mesh dropdown (overlaps viewport)
    if (show_add_menu) {
        const char *items[] = { "cube", "plane", "pyramid", "diamond" };
        int n = 4, mw = 52, mh = n*10+2;
        int mx_ = SIDE_W, my_ = add_y - 2;
        rectfill(mx_, my_, mw, mh, CLR_DARKER_BLUE);
        rect(mx_, my_, mw, mh, CLR_DARK_BLUE);
        font(FONT_SMALL);
        for (int i = 0; i < n; i++)
            print(items[i], mx_+4, my_+2+i*10, CLR_WHITE);
        font(FONT_NORMAL);
    }
}

static void draw_statusbar(void) {
    rectfill(0, SCREEN_H-BOT_H, SCREEN_W, BOT_H, CLR_DARKER_BLUE);
    line(0, SCREEN_H-BOT_H, SCREEN_W-1, SCREEN_H-BOT_H, CLR_DARK_BLUE);
    font(FONT_SMALL);
    const char *hint = "drag:orbit  scroll:zoom  tab:view";
    if (sel_face >= 0)
        hint = "E:extrude  F:flip  DEL:delete  S:save";
    else if (sel_vert >= 0)
        hint = "drag:move vert  DEL:delete  S:save";
    else if (view_mode != VIEW_PERSP && view_mode != VIEW_QUAD)
        hint = "drag vert  scroll:zoom  tab:view";
    print(hint, 3, SCREEN_H-BOT_H+2, CLR_DARK_GREY);
    // right-align items: compute positions right-to-left
    int rx = SCREEN_W - 3;
    int ry = SCREEN_H - BOT_H + 2;
    rx -= text_width("H:help");
    print("H:help", rx, ry, CLR_DARKER_GREY);
    rx -= 5;
    if (view_mode == VIEW_PERSP || view_mode == VIEW_QUAD) {
        const char *fov_str = str("fov:%d", (int)focal);
        rx -= text_width(fov_str);
        print(fov_str, rx, ry, CLR_DARK_GREY);
        rx -= 5;
    }
    if (snap_on) {
        rx -= text_width("SNAP");
        print("SNAP", rx, ry, CLR_YELLOW);
    }
    font(FONT_NORMAL);
}

// ── input helpers ─────────────────────────────────────────────────────────────
static void get_active_panel(int mx, int my, int *vout, float *cx, float *cy, float *os) {
    if (view_mode == VIEW_QUAD) {
        int qw=VP_W/2, qh=VP_H/2;
        int svs[4]={ VIEW_TOP, VIEW_FRONT, VIEW_SIDE, VIEW_PERSP };
        int sxs[4]={ VP_X, VP_X+qw, VP_X, VP_X+qw };
        int sys[4]={ VP_Y, VP_Y, VP_Y+qh, VP_Y+qh };
        int sws[4]={ qw, VP_W-qw, qw, VP_W-qw };
        int shs[4]={ qh, qh, VP_H-qh, VP_H-qh };
        int idx = ((my>=VP_Y+qh)?1:0)*2 + ((mx>=VP_X+qw)?1:0);
        *vout = svs[idx];
        *cx = sxs[idx] + sws[idx]*0.5f;
        *cy = sys[idx] + shs[idx]*0.5f;
        *os = ortho_scale * 0.5f;
    } else {
        *vout = view_mode;
        *cx = (float)VP_CX;
        *cy = (float)VP_CY;
        *os = ortho_scale;
    }
}

// ── input ─────────────────────────────────────────────────────────────────────
static void handle_input(void) {
    if (keyp(KEY_TAB)) view_mode = (view_mode + 1) % 5;

    // key commands
    // ── rename mode: absorb input before anything else ────────────────────────
    if (renaming) {
        const char *ti = text_input();
        while (*ti) {
            if (rename_len < 15) { rename_buf[rename_len++] = *ti; rename_buf[rename_len] = '\0'; }
            ti++;
        }
        if (keyp(KEY_BACKSPACE) && rename_len > 0) rename_buf[--rename_len] = '\0';
        if (keyp(KEY_ENTER)) {
            if (rename_len > 0 && rename_obj >= 0)
                strncpy(scene.objects[rename_obj].name, rename_buf, 15);
            save_bytes(&scene, sizeof(scene));
            renaming = false;
        }
        if (keyp(KEY_ESCAPE)) renaming = false;
        return;  // swallow all other input while renaming
    }

    if (keyp('S')) { save_bytes(&scene, sizeof(scene)); save_flash = 90; }
    if (keyp('G')) snap_on = !snap_on;
    if (keyp('H')) show_help = !show_help;
    if (keyp('N')) show_normals = !show_normals;
    if (keyp('O')) ortho_mode  = !ortho_mode;
    if (keyp('U')) {
        uv_mode = true; uv_sel_corner = -1;
        // entering UV mode implies you want texturing — auto-enable it
        if (sel_face >= 0 && sel_obj >= 0)
            scene.objects[sel_obj].faces[sel_face].flags &= ~FACE_NOTEX;
    }
    if (keyp('T') && sel_face >= 0 && sel_obj >= 0) {
        scene.objects[sel_obj].faces[sel_face].flags ^= FACE_NOTEX;
        save_bytes(&scene, sizeof(scene));
    }
    // click UV in topbar
    if (mouse_pressed(MOUSE_LEFT) && mouse_y() < TOP_H) {
        // rough hit test for the UV label position
        font(FONT_SMALL);
        int uv_x = 3 + (text_width("FILE")+5) + (text_width("MESH")+5) + (text_width("FACE")+5);
        font(FONT_NORMAL);
        if (mouse_x() >= uv_x && mouse_x() < uv_x + 16) {
            uv_mode = true; uv_sel_corner = -1;
            if (sel_face >= 0 && sel_obj >= 0)
                scene.objects[sel_obj].faces[sel_face].flags &= ~FACE_NOTEX;
        }
    }
    if (key('[')) { focal -= 1.0f; if (focal < 60.0f)  focal = 60.0f;  }
    if (key(']')) { focal += 1.0f; if (focal > 600.0f) focal = 600.0f; }
    if (keyp('R')) {
        build_default_cube();
        save_bytes(&scene, sizeof(scene));
        sel_vert=-1; sel_face=-1; cam_yaw=0.5f; cam_pitch=0.3f; cam_dist=4.0f;
    }
    if (keyp('Z') && sel_obj >= 0) pop_undo(&scene.objects[sel_obj]);
    if (keyp('E') && sel_face >= 0 && sel_obj >= 0) {
        do_extrude(&scene.objects[sel_obj], sel_face);
        save_bytes(&scene, sizeof(scene));
    }
    if (keyp('F') && sel_face >= 0 && sel_obj >= 0) {
        do_flip_face(&scene.objects[sel_obj], sel_face);
        save_bytes(&scene, sizeof(scene));
    }
    if (keyp(KEY_BACKSPACE)) {
        if (sel_face >= 0 && sel_obj >= 0) {
            do_delete_face(&scene.objects[sel_obj], sel_face);
            save_bytes(&scene, sizeof(scene));
        } else if (sel_vert >= 0 && sel_obj >= 0) {
            do_delete_vert(&scene.objects[sel_obj], sel_vert);
            save_bytes(&scene, sizeof(scene));
        }
    }

    int mx=mouse_x(), my=mouse_y();
    bool in_vp = mx>=VP_X && mx<VP_X+VP_W && my>=VP_Y && my<VP_Y+VP_H;

    int pview; float pcx, pcy, pos;
    get_active_panel(mx, my, &pview, &pcx, &pcy, &pos);

    // ── hover ─────────────────────────────────────────────────────────────────
    hover_vert = -1; hover_face = -1;
    if (!vdragging && in_vp && sel_obj >= 0) {
        Object *obj = &scene.objects[sel_obj];
        project_verts(obj, pview, pcx, pcy, pos);
        int best=-1; float best_d=36.0f;
        for (int i = 0; i < obj->vert_count; i++) {
            float dx=px[i]-mx, dy=py[i]-my;
            float d=dx*dx+dy*dy;
            if (d < best_d) { best_d=d; best=i; }
        }
        if (best >= 0) {
            hover_vert = best;
        } else {
            // refresh depth sort so face_at uses current frame's order
            for (int fi = 0; fi < obj->face_count; fi++) {
                float d = 0.0f;
                for (int vi = 0; vi < 4; vi++) d += pz[obj->faces[fi].vi[vi]];
                fdepth[fi] = d * 0.25f;
            }
            zsort(fdepth, forder, obj->face_count);
            hover_face = face_at(obj, mx, my);
        }
    }

    // ── vertex drag: continue or stop ─────────────────────────────────────────
    if (vdragging) {
        if (!mouse_down(MOUSE_LEFT)) {
            vdragging = false;
            if (snap_on && sel_obj >= 0 && sel_vert >= 0) {
                Vec3 *vp = &scene.objects[sel_obj].verts[sel_vert];
                vp->x = roundf(vp->x*4)/4.0f;
                vp->y = roundf(vp->y*4)/4.0f;
                vp->z = roundf(vp->z*4)/4.0f;
            }
            save_bytes(&scene, sizeof(scene));
        } else {
            float dmx=mx-vdrag_sx, dmy=my-vdrag_sy;
            vdrag_sx=mx; vdrag_sy=my;
            Vec3 *vp = &scene.objects[sel_obj].verts[sel_vert];
            if (vdrag_view == VIEW_PERSP) {
                float scale = ortho_mode ? cam_dist/focal : vdrag_pz/focal;
                Vec3 dw = rot_yp_inv(
                    (Vec3){dmx*scale, -dmy*scale, 0},
                    cam_yaw, cam_pitch);
                vp->x+=dw.x; vp->y+=dw.y; vp->z+=dw.z;
                Vec3 rc = rot_yp(*vp, cam_yaw, cam_pitch);
                vdrag_pz = rc.z + cam_dist;
                if (vdrag_pz < 0.05f) vdrag_pz = 0.05f;
            } else if (vdrag_view == VIEW_TOP) {
                vp->x += dmx/vdrag_os; vp->z += dmy/vdrag_os;
            } else if (vdrag_view == VIEW_FRONT) {
                vp->x += dmx/vdrag_os; vp->y -= dmy/vdrag_os;
            } else {
                vp->z -= dmx/vdrag_os; vp->y -= dmy/vdrag_os;
            }
            if (snap_on) {
                vp->x=roundf(vp->x*4)/4.0f;
                vp->y=roundf(vp->y*4)/4.0f;
                vp->z=roundf(vp->z*4)/4.0f;
            }
        }
    }

    // ── on press: vert, face, or orbit ────────────────────────────────────────
    if (!vdragging && mouse_pressed(MOUSE_LEFT) && in_vp) {
        bool hit = false;
        if (sel_obj >= 0) {
            Object *obj = &scene.objects[sel_obj];
            project_verts(obj, pview, pcx, pcy, pos);
            int best=-1; float best_d=36.0f;
            for (int i = 0; i < obj->vert_count; i++) {
                float dx=px[i]-mx, dy=py[i]-my;
                float d=dx*dx+dy*dy;
                if (d < best_d) { best_d=d; best=i; }
            }
            if (best >= 0) {
                sel_vert=best; sel_face=-1;
                vdragging=true;
                vdrag_view=pview; vdrag_cx=pcx; vdrag_cy=pcy; vdrag_os=pos;
                vdrag_sx=mx; vdrag_sy=my;
                Vec3 rc = rot_yp(obj->verts[best], cam_yaw, cam_pitch);
                vdrag_pz = rc.z + cam_dist;
                if (vdrag_pz < 0.05f) vdrag_pz = 0.05f;
                push_undo(obj);
                hit = true;
            } else {
                for (int fi2 = 0; fi2 < obj->face_count; fi2++) {
                    float d = 0.0f;
                    for (int vi = 0; vi < 4; vi++) d += pz[obj->faces[fi2].vi[vi]];
                    fdepth[fi2] = d * 0.25f;
                }
                zsort(fdepth, forder, obj->face_count);
                int fi = face_at(obj, mx, my);
                if (fi >= 0) { sel_face=fi; sel_vert=-1; hit=true; }
                else         { sel_vert=-1; sel_face=-1; }
            }
        }
        if (!hit && pview == VIEW_PERSP) {
            dragging=true; drag_sx=mx; drag_sy=my;
        }
    }

    // ── object drag (right-click) ─────────────────────────────────────────────
    if (obj_dragging) {
        if (!mouse_down(MOUSE_RIGHT)) {
            obj_dragging = false;
            save_bytes(&scene, sizeof(scene));
        } else if (sel_obj >= 0) {
            float dmx = mx - obj_drag_sx;
            float dmy = my - obj_drag_sy;
            obj_drag_sx = mx; obj_drag_sy = my;
            Object *obj = &scene.objects[sel_obj];
            float scale = ortho_mode ? cam_dist/focal : obj_drag_pz/focal;
            Vec3 dw = {0,0,0};
            if (obj_drag_view == VIEW_PERSP) {
                dw = rot_yp_inv((Vec3){dmx*scale, -dmy*scale, 0}, cam_yaw, cam_pitch);
            } else if (obj_drag_view == VIEW_TOP) {
                dw = (Vec3){ dmx/obj_drag_os, 0, dmy/obj_drag_os };
            } else if (obj_drag_view == VIEW_FRONT) {
                dw = (Vec3){ dmx/obj_drag_os, -dmy/obj_drag_os, 0 };
            } else {
                dw = (Vec3){ 0, -dmy/obj_drag_os, -dmx/obj_drag_os };
            }
            for (int i = 0; i < obj->vert_count; i++) {
                obj->verts[i].x += dw.x;
                obj->verts[i].y += dw.y;
                obj->verts[i].z += dw.z;
            }
        }
    } else if (mouse_pressed(MOUSE_RIGHT) && in_vp && sel_obj >= 0) {
        push_undo(&scene.objects[sel_obj]);
        obj_dragging   = true;
        obj_drag_sx    = mx; obj_drag_sy = my;
        obj_drag_view  = pview;
        obj_drag_cx    = pcx; obj_drag_cy = pcy; obj_drag_os = pos;
        // compute average depth of object for persp scale
        Object *obj = &scene.objects[sel_obj];
        project_verts(obj, pview, pcx, pcy, pos);
        float sum = 0.0f;
        for (int i = 0; i < obj->vert_count; i++) sum += pz[i];
        obj_drag_pz = (obj->vert_count > 0) ? sum / obj->vert_count : cam_dist;
    }

    // ── camera orbit ──────────────────────────────────────────────────────────
    if (mouse_released(MOUSE_LEFT)) dragging = false;
    if (dragging && !vdragging && mouse_down(MOUSE_LEFT)) {
        cam_yaw   += (mx - drag_sx) * 0.008f;
        cam_pitch -= (my - drag_sy) * 0.008f;
        drag_sx = mx;
        drag_sy = my;
    }

    // ── scroll zoom ────────────────────────────────────────────────────────────
    float wheel = mouse_wheel();
    if (in_vp && wheel != 0.0f) {
        if (pview == VIEW_PERSP) {
            cam_dist -= wheel*0.3f;
            if (cam_dist < 0.5f)  cam_dist = 0.5f;
            if (cam_dist > 20.0f) cam_dist = 20.0f;
        } else if (view_mode != VIEW_QUAD) {
            ortho_scale += wheel*5.0f;
            if (ortho_scale < 10.0f)  ortho_scale = 10.0f;
            if (ortho_scale > 200.0f) ortho_scale = 200.0f;
        }
    }

    // ── color picker click ─────────────────────────────────────────────────────
    if (mouse_pressed(MOUSE_LEFT) &&
        mx >= CP_X && mx < CP_X + CP_COLS*(CP_SW+CP_GAP) &&
        my >= CP_Y && my < CP_Y + CP_ROWS*(CP_SH+CP_GAP)) {
        int ci = (my-CP_Y)/(CP_SH+CP_GAP) * CP_COLS + (mx-CP_X)/(CP_SW+CP_GAP);
        if (ci >= 0 && ci < 32 && sel_face >= 0 && sel_obj >= 0) {
            scene.objects[sel_obj].faces[sel_face].color = ci;
            save_bytes(&scene, sizeof(scene));
        }
    }

    // ── add-menu dropdown clicks ──────────────────────────────────────────────
    int add_y = TOP_H + 14 + scene.object_count * 10 + 2;
    if (mouse_pressed(MOUSE_LEFT) && show_add_menu) {
        int n = 4, mw = 52, mh = n*10+2;
        if (mx >= SIDE_W && mx < SIDE_W+mw && my >= add_y-2 && my < add_y-2+mh) {
            int item = (my - add_y) / 10;
            if      (item == 0) add_cube();
            else if (item == 1) add_plane();
            else if (item == 2) add_pyramid();
            else if (item == 3) add_diamond();
            save_bytes(&scene, sizeof(scene));
        }
        show_add_menu = false;
    }

    // ── object list click ──────────────────────────────────────────────────────
    if (mouse_pressed(MOUSE_LEFT) && mx < SIDE_W-1 && my >= TOP_H+14) {
        if (my >= add_y && my < add_y+8) {
            show_add_menu = !show_add_menu;  // toggle + ADD button
        } else if (my < CP_Y-10) {
            int clicked = (my-(TOP_H+14))/10;
            if (clicked >= 0 && clicked < scene.object_count) {
                if (clicked == sel_obj && !renaming) {
                    renaming = true; rename_obj = clicked;
                    strncpy(rename_buf, scene.objects[clicked].name, 15);
                    rename_len = strlen(rename_buf);
                } else {
                    sel_obj=clicked; sel_vert=-1; sel_face=-1;
                    show_add_menu = false;
                }
            }
        }
    }
    if (mouse_pressed(MOUSE_LEFT) && mx >= SIDE_W) show_add_menu = false;
}

// ── entry points ──────────────────────────────────────────────────────────────
static bool started = false;

void update(void) {
    if (!started) { init_scene(); started = true; }
    if (uv_mode) handle_uv_input();
    else         handle_input();
}

void draw(void) {
    cls(CLR_BLACK);
    if (uv_mode) draw_uv_editor();
    else         draw_scene();
    draw_topbar();
    draw_sidebar();
    if (!uv_mode) draw_statusbar();
    if (show_help) draw_help();

    // pixel cursor: a fist while dragging; a finger when hovering something
    // grabbable (a vertex/face, or a UV corner); a crosshair for precise picking
    // elsewhere in the viewport; a plain arrow over the side/top/status panels.
    int mx = mouse_x(), my = mouse_y();
    int over_vp = mx >= VP_X && my >= VP_Y && my < SCREEN_H - BOT_H;
    int hover_grab = uv_mode ? (uv_hover_corner >= 0) : (hover_vert >= 0 || hover_face >= 0);
    if (dragging || vdragging || obj_dragging) cursor_draw(CUR_GRAB);
    else if (hover_grab)                        cursor_draw(CUR_HAND);
    else if (over_vp)                           cursor_draw(CUR_CROSS);
    else                                        cursor_draw(CUR_ARROW);
}
