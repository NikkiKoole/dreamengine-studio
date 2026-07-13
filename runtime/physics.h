#ifndef PHYSICS_H
#define PHYSICS_H
/* physics.h — a tiny shared VERLET (position-based) toolkit for carts.
 *
 * INCLUDE AFTER studio.h (it uses fsqrt/clamp from there). Cart-land library,
 * ADR-0006: the engine doesn't own physics; this is a copy-me toolkit carts opt into.
 *
 * The whole idea is three lines you already know from the `physics` cart:
 *   1. a POINT (PhysPt) remembers where it was last frame; velocity is just (now - last).
 *   2. a LINK keeps two points a fixed distance apart (a rigid "stick").
 *   3. each frame: integrate every point, then relax the links/collisions a few times.
 * From that: ropes, cloth, ragdolls, blobs, chains, bouncy balls, stick-built pseudo-boxes.
 *
 * The point carries an INVERSE MASS `w` (w = 0 means "immovable" — a pinned anchor, or a
 * point you're dragging) and a collision radius `r`. That one field expresses pinning,
 * grabbing, and heavy-vs-light, all through the same relax math.
 *
 * The cart OWNS its point/link arrays and its own step loop — this header is transparent
 * primitives, not a managed world. You call these verbs on your own PhysPts. Carts with
 * extra per-point data either keep a parallel array (colours, etc.) or embed PhysPt as the
 * first member of their own struct (`struct { PhysPt p; int col; }`) and pass `&thing.p`.
 *
 * CONTRACT (all deterministic, no global state, no allocation):
 *   phys_pt(p,x,y,w,r)              init a point at rest at (x,y)
 *   phys_integrate(p,gx,gy,damp)    move it: v=(pos-prev)*damp; carry over + (gx,gy). Skips w==0.
 *   phys_link(a,b,rest)             rigid distance link (inverse-mass split); returns strain (d-rest)
 *   phys_spring(a,b,rest,k)         springy link — pulls back a fraction k (0..1) each call
 *   phys_collide(a,b)              -> two points as circles; shove apart if overlapping. bool: did they?
 *   phys_collide_seg(p,a,b)         point (circle) vs the segment a-b; returns penetration depth
 *   phys_bounds(p,x0,y0,x1,y1,rest,fric)  clamp inside the box, bounce (rest) + floor friction (fric); bool: hit
 *   phys_aim(a,b,tx,ty,str)         angular spring: nudge bone a->b toward unit direction (tx,ty)
 *   phys_vx(p) / phys_vy(p)         the implied velocity (pos - prev), e.g. for a hit-speed sfx
 *   phys_kick(p,vx,vy)             add an impulse (throw / hop / knockback)
 *   phys_grab(p,x,y,px,py)          pin to a dragged position; pass prev-mouse so release throws at mouse speed
 *   phys_place(p,x,y)               teleport with zero velocity (spawn / reset)
 *
 * Typical update():  for each pt phys_integrate(...);  then a few times: links, collisions, bounds.
 * Chop the frame into a few SUBSTEPS (pass gy = GRAVITY/substeps) so fast points don't tunnel.
 */

typedef struct { float x, y, px, py, w, r; } PhysPt;   /* w = inverse mass (0 = fixed), r = radius */

static void phys_pt(PhysPt *p, float x, float y, float w, float r) {
    p->x = p->px = x; p->y = p->py = y; p->w = w; p->r = r;
}

/* velocity = (current - previous), carried over with drag, plus this substep's gravity */
static void phys_integrate(PhysPt *p, float gx, float gy, float damp) {
    if (p->w == 0) return;                 /* pinned / held: someone else controls it */
    float vx = (p->x - p->px) * damp;
    float vy = (p->y - p->py) * damp;
    p->px = p->x; p->py = p->y;
    p->x += vx + gx;
    p->y += vy + gy;
}

/* pull two points back to `rest` apart, split by inverse mass. returns strain (>0 stretched) */
static float phys_link(PhysPt *a, PhysPt *b, float rest) {
    float dx = b->x - a->x, dy = b->y - a->y;
    float d  = fsqrt(dx*dx + dy*dy);
    if (d < 0.0001f) return 0.0f;
    float ws = a->w + b->w;
    if (ws == 0) return 0.0f;              /* both immovable */
    float diff = (d - rest) / d;
    a->x += dx * diff * (a->w / ws);  a->y += dy * diff * (a->w / ws);
    b->x -= dx * diff * (b->w / ws);  b->y -= dy * diff * (b->w / ws);
    return d - rest;
}

/* like phys_link but soft — only closes a fraction k (0 = slack, 1 = rigid) of the gap */
static float phys_spring(PhysPt *a, PhysPt *b, float rest, float k) {
    float dx = b->x - a->x, dy = b->y - a->y;
    float d  = fsqrt(dx*dx + dy*dy);
    if (d < 0.0001f) return 0.0f;
    float ws = a->w + b->w;
    if (ws == 0) return 0.0f;
    float diff = (d - rest) / d * k;
    a->x += dx * diff * (a->w / ws);  a->y += dy * diff * (a->w / ws);
    b->x -= dx * diff * (b->w / ws);  b->y -= dy * diff * (b->w / ws);
    return d - rest;
}

/* two points are circles — if they overlap, shove them apart (split by mass). true = touched */
static bool phys_collide(PhysPt *a, PhysPt *b) {
    float dx = b->x - a->x, dy = b->y - a->y;
    float d  = fsqrt(dx*dx + dy*dy), min = a->r + b->r;
    if (d >= min || d < 0.0001f) return false;
    float ws = a->w + b->w;
    if (ws == 0) return false;
    float diff = (min - d) / d;
    a->x -= dx * diff * (a->w / ws);  a->y -= dy * diff * (a->w / ws);
    b->x += dx * diff * (b->w / ws);  b->y += dy * diff * (b->w / ws);
    return true;
}

/* a point (circle) vs the segment a-b. Push the point out; push the ends the other way,
 * split by how far along the segment (t) the contact landed. This makes faces solid.
 * returns penetration depth (0 = no contact). skips a point against its own endpoints. */
static float phys_collide_seg(PhysPt *p, PhysPt *a, PhysPt *b) {
    if (p == a || p == b) return 0.0f;
    float abx = b->x - a->x, aby = b->y - a->y;
    float ab2 = abx*abx + aby*aby;
    if (ab2 < 0.0001f) return 0.0f;
    float t = ((p->x - a->x) * abx + (p->y - a->y) * aby) / ab2;
    t = clamp(t, 0.0f, 1.0f);
    float dx = p->x - (a->x + t * abx);
    float dy = p->y - (a->y + t * aby);
    float d  = fsqrt(dx*dx + dy*dy);
    if (d >= p->r || d < 0.0001f) return 0.0f;
    float wseg = a->w * (1 - t) + b->w * t;
    float ws   = p->w + wseg;
    if (ws == 0) return 0.0f;
    float diff = (p->r - d) / d;
    float pf = p->w / ws, sf = wseg / ws;
    p->x += dx * diff * pf;             p->y += dy * diff * pf;
    a->x -= dx * diff * sf * (1 - t);   a->y -= dy * diff * sf * (1 - t);
    b->x -= dx * diff * sf * t;         b->y -= dy * diff * sf * t;
    return p->r - d;
}

/* keep a point inside [x0,y0]..[x1,y1], bouncing (rest) and rubbing off horizontal speed on
 * the FLOOR (y1) by `fric` (fraction of speed KEPT, e.g. 0.90). true if it hit any wall. */
static bool phys_bounds(PhysPt *p, float x0, float y0, float x1, float y1, float rest, float fric) {
    if (p->w == 0) return false;
    float r = p->r, v; bool hit = false;
    if (p->x < x0 + r) { v = p->x - p->px; p->x = x0 + r; p->px = p->x + v * rest; hit = true; }
    if (p->x > x1 - r) { v = p->x - p->px; p->x = x1 - r; p->px = p->x + v * rest; hit = true; }
    if (p->y < y0 + r) { v = p->y - p->py; p->y = y0 + r; p->py = p->y + v * rest; hit = true; }
    if (p->y > y1 - r) { v = p->y - p->py; p->y = y1 - r; p->py = p->y + v * rest;
                         float vx = p->x - p->px; p->px = p->x - vx * fric; hit = true; }
    return hit;
}

/* angular spring: nudge bone a->b toward target unit direction (tx,ty). str 0..1.
 * only applies within 90 deg of target (past that the cross-product direction inverts). */
static void phys_aim(PhysPt *a, PhysPt *b, float tx, float ty, float str) {
    float dx = b->x - a->x, dy = b->y - a->y;
    float d = fsqrt(dx*dx + dy*dy);
    if (d < 0.001f) return;
    float cx = dx/d, cy = dy/d;
    if (cx*tx + cy*ty <= 0.0f) return;
    float cross = cx*ty - cy*tx;
    b->x += -cy * cross * str;  b->y +=  cx * cross * str;
    a->x -= -cy * cross * str;  a->y -=  cx * cross * str;
}

static float phys_vx(const PhysPt *p) { return p->x - p->px; }
static float phys_vy(const PhysPt *p) { return p->y - p->py; }

/* add an impulse — in verlet, velocity lives in (pos - prev), so we move prev the other way */
static void phys_kick(PhysPt *p, float vx, float vy) { p->px -= vx; p->py -= vy; }

/* pin to a dragged position; pass the PREVIOUS mouse pos so releasing throws at mouse speed */
static void phys_grab(PhysPt *p, float x, float y, float px, float py) {
    p->x = x; p->y = y; p->px = px; p->py = py;
}

/* teleport with zero velocity (spawn / reset) */
static void phys_place(PhysPt *p, float x, float y) { p->x = p->px = x; p->y = p->py = y; }

#endif /* PHYSICS_H */
