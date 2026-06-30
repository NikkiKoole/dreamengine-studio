/* de:meta
{
  "title": "streetlab",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "road-network",
    "at-grade-junction"
  ],
  "lineage": "The at-grade sibling of roadlab, one tier down: roadlab draws grade-separated interchange ramps, streetlab draws the streets that cross AT GRADE — curb returns built as tangent-arc fillets (an arc tangent to both kerb lines), a leg-based junction layout that re-solves under skew and the T for free, left-turn bays with channelizing median islands, a pedestrian layer (sidewalks + crosswalks), a mini-roundabout (traversable island + give-way entries), and a typed cross-section (median / bike / parking lane types, the whole junction re-solving as the carriageway widens) — all from the one angle-agnostic leg model. Plus a network view (the street web in five canonical patterns — incl. the Stage-2 fused-grid superblock — with the full SNDi metrics: degree, dead-ends, node-mix, sinuosity, and circuity). First cart to carry a spec() (pins the curb-return tangency, topology counts, network graph + roundabout geometry).",
  "description": "The FOUNDATION sandbox for the AT-GRADE world, one tier BELOW roadlab. roadlab draws the grade-separated INTERCHANGE family (loop/flyover ramps that ELIMINATE the crossing); streetlab draws the streets that cross AT GRADE — a different geometry grammar (curb returns, small radii, turn lanes) flagged as unmodeled in docs/design/road-hierarchy-notes.md. The seam between the tiers is access control: freeway = ramp-only = roadlab, everything below = at grade = streetlab. M1: CURB-RETURN corner fillets — the headline at-grade primitive, an arc tangent to both curb lines whose radius is kept SMALL (it slows turns and shortens the pedestrian crossing, the opposite goal of a high-speed ramp). M2: SKEW + the T family via a leg model (a junction is a list of arms at bearings, nothing hardcoded to 90); the curb-return grammar is angle-agnostic so it re-solves for free, and a T drops the north arm to 3 legs / 2 corners. M3: TURN LANES + channelizing islands — a left-turn lane marked INSIDE the carriageway: a raised MEDIAN splitter on the centreline (the channelizing island) + the inner inbound lane delineated turn-only + a left-turn arrow; no outer widening, so it composes with skew. M4: the street WEB — a second VIEW (press v) generating a seed-driven GRAPH of intersections (nodes) + street segments (edges); gen_network() is a pure fn of (pattern, seed) with grid / organic / radial / cul-de-sac / SUPERBLOCK patterns + a curvature knob, and live SNDi metrics — mean degree, dead-ends, sinuosity, the degree-1/3/4 NODE MIX (the discriminator mean degree alone hides), and CIRCUITY (network path / straight-line, Floyd-Warshall). Stage 2: the SUPERBLOCK (fused-grid) is a continuous arterial grid wrapping a vehicle-discontinuous interior (cul-de-sac stems off the perimeter) — the one pattern with no algorithm in the planning literature; circuity proves it distinct, ordering grid 1.21 < superblock 1.59 < cul-de-sac 2.18 where degree/node-mix cannot tell the superblock from a cul-de-sac (the 'node degree alone is not enough' thesis). The curve knob winds only the interior local streets — the arterial frame stays straight. M5: the PEDESTRIAN layer (press k) — SIDEWALKS (the footprint inflated by SW, wrapping the corners) + ZEBRA CROSSWALKS at each mouth, stop bar set back behind them. M6: the MINI-ROUNDABOUT (press r) — the at-grade counterpart of roadlab's grade-separated ring: a circulatory disc with a MINI, TRAVERSABLE (mountable/domed) central island, flush teardrop SPLITTER islands, GIVE-WAY (yield) entries + counterclockwise circulating arrows; leg-model based, so it composes with skew/T/lanes/sidewalks. M7: the typed CROSS-SECTION (OpenDRIVE lane types) — a lane-section from the centre out, [median] driving×N [bike] [parking], toggled per element (m/b/;); the median is the same island primitive run continuously, and because the half-width HW = the SUM of the lanes present and every junction primitive keys off HW, widening the section re-solves the whole junction (curb returns, mouth, roundabout, sidewalks) for free. curb_return() + count_corners() + arm_face() + the network graph + round_icr() + cross_hw() + mean_circuity() are pure fns pinned by the cart's spec() (the first cart with one; 104 assertions). Controls: v switches junction↔network; junction: [ / ] curb radius (island radius in roundabout mode), - / = lanes, , / . skew, t = T, p = turn lanes, r = roundabout, k = sidewalks+crosswalks, m = median, b = bike lane, ; = parking; network: [ / ] seed, b = pattern, c = curve. Next: the modal active-travel layer (paths-as-edges, the superblock's permeable cut-throughs), then the §8.4 two-tier world. See docs/design/road-hierarchy-notes.md and docs/design/road-program-state.md."
}
de:meta */
#include "studio.h"
#include "ui.h"        // on-screen buttons (per-finger capture, fat-finger hit pads) — never hand-roll
#include <stdio.h>
#include <math.h>

// streetlab — the FOUNDATION cart for the AT-GRADE world, one tier BELOW roadlab. roadlab draws the
// grade-separated INTERCHANGE family (loop/flyover ramps that ELIMINATE the crossing); streetlab draws
// the streets that cross AT GRADE — a fundamentally different geometry grammar (curb returns, small radii,
// turn lanes, channelization) flagged as unmodeled in docs/design/road-hierarchy-notes.md §2. The seam
// between the two tiers is "access control" (§1): freeway = ramp-only = roadlab; everything below = at
// grade = streetlab. The sibling framing is exact — both are sandboxes that prove a grammar before a
// seed-driven world consumes them.
//   MILESTONE 1: a perpendicular 4-way at-grade crossing with CURB-RETURN corner fillets. The headline
//                at-grade primitive: where two carriageways meet, the curb is rounded by an arc TANGENT to
//                both curb lines (radius R kept SMALL — it slows turns + shortens the pedestrian crossing,
//                the opposite goal of a high-speed ramp). curb_return() is the closed-form tangent-arc
//                construction (pure fn of K, the two edge directions, R) — and the first spec() target.
//   MILESTONE 2: SKEW + the T (3-leg) family. Like roadlab's leg layer: a junction is a list of LEGS, each
//                an arm at a `bearing`; the arms are laid out from the legs, NOTHING hardcoded to 90°. The
//                curb-return grammar was already angle-agnostic (it takes arbitrary edge directions), so it
//                re-solves for free — only the STAGE generalised (rotated bands; corners = where adjacent
//                inner edges cross). `skew` tilts the crossing street; `T` drops the north arm → 3 legs, 2
//                corners (the straight "back" of the through road has no corner). count_corners() is pure.
//   MILESTONE 3: TURN LANES + channelizing islands. A left-turn lane is marked INSIDE the carriageway
//                (no outer widening — that detached into tabs under skew): a raised MEDIAN splitter on the
//                centreline + a solid delineation of the inner inbound lane + a left-turn arrow. All in the
//                arm's local frame ⇒ skew-safe like the curb returns.
//   MILESTONE 4: the street WEB (network topology — road-hierarchy-notes §8). A second VIEW ('v'): a GRAPH
//                of intersections (nodes) + street segments (edges) generated deterministically from a seed.
//                gen_network() is a PURE fn of (pattern, seed) — the spec asserts on the graph (node/edge
//                counts, mean degree, dead-ends = the SNDi measures §8.2). FIVE patterns, and the metrics
//                SEPARATE them (the §8.2 point): grid/organic (degree ~3.4, 0 dead-ends) · radial (a hub +
//                rings, degree ~3.9) · cul-de-sac (a random spanning tree + sparse loops ⇒ dendritic, degree
//                ~2.2, many dead-ends).
//   STAGE 2:     the SUPERBLOCK / FUSED-GRID pattern (§8.3) — a continuous arterial grid wrapping a vehicle-
//                DISCONTINUOUS interior (local cul-de-sac stems off the perimeter). The one pattern with no
//                algorithm in the 2001/2008 pillars (an original bit). Ships with CIRCUITY (§8.2 #3, the 4th
//                SNDi measure: network path ÷ straight-line, Floyd–Warshall) — §8.2's thesis in action: the
//                superblock and the cul-de-sac read NEARLY IDENTICAL on degree/node-mix (both ~dendritic), and
//                only circuity tells them apart — it orders GRID < SUPERBLOCK < CUL-DE-SAC (the arterial frame
//                gives through-routes a pure dendritic net lacks, but the sealed interior still detours cars).
//                Next: the modal active-travel layer (its cut-throughs ARE the superblock's permeable edges),
//                then the §8.4 two-tier world.
//   MILESTONE 5: the PEDESTRIAN layer (§2 — the reason at-grade exists: small radii are "to shorten the ped
//                crossing"). 'k' toggles SIDEWALKS (the whole junction footprint inflated by SW, drawn under
//                the road ⇒ an SW-wide kerbside ring that wraps the corners for free) + ZEBRA CROSSWALKS at
//                each mouth (stripes along travel, stop bar set back behind them). Composes with skew/T.
//   MILESTONE 6: the MINI-ROUNDABOUT (§2) — the at-grade counterpart of roadlab's grade-separated ring. 'r'
//                lays a circulatory disc over the centre with a MINI + TRAVERSABLE (mountable/domed) central
//                island, flush SPLITTER islands on each approach, GIVE-WAY (yield) entry lines (not stop
//                bars) + CCW circulating arrows. Built on the same leg model ⇒ any leg count / skew for free;
//                mutually exclusive with turn lanes (both are "treatments" of the crossing). island radius is
//                the headline knob — it reuses the [ ] slot in this mode. round_icr() (island + circulatory)
//                is pure ⇒ spec'd: the island sits strictly inside the inscribed circle and stays "mini".
//   MILESTONE 7: the typed CROSS-SECTION (OpenDRIVE lane types, §5). The carriageway stops being uniform
//                asphalt: a lane-section from the centre out — [median] · driving×N · [bike] · [parking] —
//                toggled per element (m/b/;). The MEDIAN is itself a lane type (the same island primitive as
//                M3's splitter / M6's central island, now run continuously down the centre). The key trick:
//                the half-width HW = cross_hw() = the SUM of the lanes present, and EVERY junction primitive
//                (curb returns, the mouth datum, the roundabout, sidewalks) keys off HW ⇒ widening the
//                section re-solves the whole junction for free. cross_hw() is the pure, spec'd datum.
//   FIELD-BASED ROADS (docs/design/field-based-road-rendering.md): the junction can render from ONE coverage
//   field (uniform 1px kerb, symmetric, at any skew/curve) instead of per-arm casing + mirror_blit. SHIPPED
//   as OPT-IN — the 'g' key (or build -DDE_FIELD_ROADS) toggles it; DEFAULT OFF, the per-arm route still ships.
//   See fr_render() below. Gate: `node tools/road-check.js streetlab --all` (the correctness oracle). When the
//   software canvas (docs/design/software-canvas.md) makes the per-pixel fill cheap, the field becomes default
//   and the old route (mirror_blit/casing-fillet/stroke_corner) is deleted. 'n' = toggle painted markings.
//   Controls: ui.h toolbar (clickable; keyboard too). v = junction↔network view. JUNCTION: [ ] curb radius
//             (island R in roundabout mode) · -/= lanes · ,/. skew · t = T · p = turn lanes · r = roundabout ·
//             k = pavement (sidewalk+crossings) · m = median · b = bike lane (off/lanes/+crossing) · ; = parking ·
//             g = field rendering A/B · n = painted markings on/off.
//             NETWORK: [ ] seed ·
//             b = pattern · c = curve (M4c: the §8.5 curvature knob bows each edge ⇒ sinuosity goes live).

#define LANEW    8     // one lane width (px)
#define TOOLBAR  62    // bottom control strip height (three rows; M7 added the cross-section row)
#define POCKET   22    // M3: turn-bay length at the stop bar
#define PTAPER   14    // M3: upstream taper of the turn bay
#define MEDW     2     // M3: raised median splitter half-width
#define SW       4     // M5: sidewalk width (the strip outside each kerb)
#define CWDEP    6     // M5: crosswalk depth (how far the zebra band reaches out from the box)
#define NETTOP   28    // M4: network-view top edge (leaves room for 3 header lines: title/metrics/degree mix)
#define BIKEW    5     // M7: bike-lane width (a tinted lane outside the driving lanes)
#define PARKW    7     // M7: parking-lane width (at the kerb, marked with bay ticks)
#define MEDHW    4     // M7: continuous centre-median half-width (the median IS a cross-section lane type)
#define TWLTLHW  5     // Stage-1 #3: two-way left-turn lane half-width (a painted centre lane, ~10px ⇒ one lane)
#define DWAY_W   10    // Stage-1 #3c: driveway width along the arm (the kerb-cut opening)
#define DWAY_FL  3     // Stage-1 #3c: apron flare — the kerb-cut wings splay this much wider at the road edge
#define SLIPW    6     // Stage-1 #2: free-right slip-lane width (the gap between the kerb and the floating island)
#define FR_NOSE  2.f   // Stage-1 #2: nose offset — the island is set back this far from the box corner (traveled way)
#define PCLEAR   16    // Pass 2: parking CLEAR ZONE — markings end this far back from the junction (no parking near it)
#define DEG2RAD  0.01745329252f
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// SNAP near-zero to exactly 0: at cardinal angles cos/sin return ~1e-7, not 0, and over a long arm (×REACH)
// that sub-pixel error tips an (int) truncation by 1px — a horizontal/vertical arm would drift 1px down its
// length (the two halves of a through road then mismatch by 1px at the hub). 1e-4 is far below any real bearing.
static float ux(float d){ float c=cos_deg(d); return (c>-1e-4f&&c<1e-4f)?0.f:c; }
static float uy(float d){ float s=sin_deg(d); return (s>-1e-4f&&s<1e-4f)?0.f:s; }
// round-to-nearest pixel — symmetric about 0, unlike a plain (int) cast which truncates TOWARD zero and so
// breaks left/right mirror on fractional coords (cx+12.7→172 but cx-12.7→147 = 13 away). Use on curved kerbs.
static int ri(float v){ return (int)roundf(v); }

// TWO layers over the structural road (declared early — corner_bike/cross_markings read them):
//   paint    = PAINTED-ON markings — lane lines, dashes, dividers, turn arrows, stop bars, give-way,
//              crosswalks, elephant's feet, parking ticks, circulation arrows. Toggled live by 'n'.
//   surfaces = raised/paved SURFACES — bike & median lane tints, the roundabout & free-right islands,
//              the splitter, bulb-outs, driveways. These are road STRUCTURE, so they stay on in play.
// road-check builds -DDE_NO_MARKINGS to drop BOTH ⇒ the bare asphalt+kerb the framebuffer oracle needs.
#ifdef DE_NO_MARKINGS
static int   paint = 0, surfaces = 0;   // headless gate: bare structural road (asphalt + kerb only)
#else
static int   paint = 1, surfaces = 1;
#endif

// ── SEAM (2026-06-23): the ≤1px CORNER FLOOR is the kerb STROKE, not the fill. ─────────────────
// Investigation (linesym probe + mirror-diff colour-classify; full writeup in docs/design/
// streetlab-corner-symmetry-plan.md) pinned it: the corner FILL (fill_corner → polyfill) is ALREADY
// mirror-symmetric — polyfill decides its pixels on the CPU (float edge-crossings → integer span),
// so it reflects exactly. The whole floor is the kerb LINE: stroke_corner → line() → raylib's GPU
// DrawLine, whose staircase is direction-dependent and not reflection-symmetric.
//
// IMPORTANT NEGATIVE RESULT: a symmetric software line (sline, below) does NOT fix it — wiring it
// into stroke_corner REGRESSES the kerb 7->27 (measured). Why: the fill is point-mirrored about the
// true centre cx=160 (ri-snapped arc verts land cells summing to 320), but a 1px stroke is a CELL,
// and mirror-diff pairs cells x<->319-x (sum 319). sline is exact only for CELL-mirror endpoints, so
// fed the fill's point-mirror endpoints it lands 1px off. Fills and 1px strokes want OPPOSITE snap
// conventions on an even grid. THE FIX SHIPPED: (a) MIRROR-BLIT — see mirror_blit() + its call in
// draw(); it reflects the rendered junction-core PIXELS, sidestepping snapping entirely, and takes
// the kerb floor to 0 (verified: node tools/mirror-diff.js streetlab --band 20,110 → 0 BROWNISH_BLACK
// pairs). Alternatives not taken: (b) draw the kerb as the fill's coverage-boundary so it inherits the
// fill's symmetry; (c) the SOFTWARE CANVAS (docs/design/software-canvas.md), where line() becomes a
// CPU rasteriser engine-wide with one convention + bit-identical determinism (sline, below, is its
// missing piece — kept banked here for that day, NOT wired given the negative result above).
// Rule of thumb still holds: spec the geometry, eyeball the pixels.

// sline — reflection-symmetric software line. Iterate the MAJOR axis per integer step, round the
// MINOR coord; ties break toward the segment midpoint (reflection-invariant), and the lone
// degenerate (tie AT the midpoint) plots BOTH candidates. No direction-dependent error term ⇒ a line
// and its mirror rasterise to mirror-identical pixels — PROVEN 0-mismatch in the linesym petri-dish
// WHEN endpoints are true cell-mirrors. Kept here as the canvas's missing CPU-line piece (SEAM:
// promote to runtime/studio.c line() under the software canvas); deliberately NOT wired into
// stroke_corner (see the negative result above). __attribute__((unused)) until then.
__attribute__((unused))
static void sl_plot(int maj, float v, float mid, int horiz, int c){
    float f=floorf(v); int fi=(int)f, m; float r=v-f;
    if (r!=0.5f) m=(r<0.5f)?fi:fi+1;
    else if (v==mid){ if(horiz){pset(maj,fi,c);pset(maj,fi+1,c);} else {pset(fi,maj,c);pset(fi+1,maj,c);} return; }
    else m=(mid>v)?fi+1:fi;
    if (horiz) pset(maj,m,c); else pset(m,maj,c);
}
__attribute__((unused))
static void sline(int x0,int y0,int x1,int y1,int c){
    int dx=x1-x0, dy=y1-y0, adx=dx<0?-dx:dx, ady=dy<0?-dy:dy;
    if (adx==0&&ady==0){ pset(x0,y0,c); return; }
    if (adx>=ady){ int lo=x0<x1?x0:x1, hi=x0<x1?x1:x0; float ym=(y0+y1)*0.5f;
        for (int x=lo;x<=hi;x++) sl_plot(x, y0+(float)(x-x0)*dy/(float)dx, ym, 1, c); }
    else { int lo=y0<y1?y0:y1, hi=y0<y1?y1:y0; float xm=(x0+x1)*0.5f;
        for (int y=lo;y<=hi;y++) sl_plot(y, x0+(float)(y-y0)*dx/(float)dy, xm, 0, c); }
}

// ── CURB RETURN = the at-grade corner primitive. Two curb edges leave the corner K in directions e1,e2
//    (degrees, pointing AWAY from K along each curb). The return is the arc of radius R TANGENT to both
//    edges; its centre O sits on the bisector of (e1,e2) on the SIDEWALK side (the curbs point outward up
//    the two streets, so their sum points away from the intersection = toward the block corner). Closed
//    form: tangent points at distance R/tan(half) along each edge, centre at R/sin(half) along the
//    bisector. PURE fn — the geometry the spec pins (tangency: dist(O, each edge line) == R). ──
typedef struct { float ox, oy, t1x, t1y, t2x, t2y; } CurbReturn;
static CurbReturn curb_return(float kx, float ky, float e1, float e2, float R){
    float d1x=ux(e1), d1y=uy(e1), d2x=ux(e2), d2y=uy(e2);
    float bx=d1x+d2x, by=d1y+d2y;                          // bisector (toward the block corner)
    float bl=sqrtf(bx*bx+by*by); if(bl<1e-4f){ bx=-d1y; by=d1x; bl=1; } bx/=bl; by/=bl;
    float coshalf = bx*d1x + by*d1y;                       // cos(half-angle) = bisector · edge
    if (coshalf>1) coshalf=1; if (coshalf<-1) coshalf=-1;
    float half = acosf(coshalf);
    float sinhalf = sinf(half); if (sinhalf<1e-4f) sinhalf=1e-4f;
    float tanhalf = sinhalf/coshalf; if (tanhalf<1e-4f) tanhalf=1e-4f;
    float tdist = R / tanhalf;                             // K → tangent point, along each edge
    float cdist = R / sinhalf;                             // K → centre, along the bisector
    CurbReturn c;
    c.ox  = kx + bx*cdist;   c.oy  = ky + by*cdist;
    c.t1x = kx + d1x*tdist;  c.t1y = ky + d1y*tdist;
    c.t2x = kx + d2x*tdist;  c.t2y = ky + d2y*tdist;
    return c;
}

// fill the fillet: the curvy triangle bounded by the two curb edges (meeting at the SHARP corner K) and
// the arc. PAVEMENT sits on the K side of the arc (rounding the re-entrant corner inward); the block keeps
// its square outer shape. Apex = K (NOT the arc centre O — that fills the block side, the inverted bug).
static void fill_corner(float kx, float ky, CurbReturn c, float R, int col){
    float a0 = atan2f(c.t1y-c.oy, c.t1x-c.ox);
    float a1 = atan2f(c.t2y-c.oy, c.t2x-c.ox);
    float d  = a1-a0; while(d> M_PI)d-=2*M_PI; while(d<-M_PI)d+=2*M_PI;   // shorter sweep (the ≤90° arc)
    enum { N=10 };
    int xy[2*(N+2)]; int k=0;
    xy[k++]=ri(kx); xy[k++]=ri(ky);                        // apex = the sharp corner (pavement side)
    for (int i=0;i<=N;i++){ float a=a0+d*i/N; xy[k++]=ri(c.ox+cosf(a)*R); xy[k++]=ri(c.oy+sinf(a)*R); }
    polyfill(xy, N+2, col);
}
// stroke just the curb arc (the rounded kerb line)
static void stroke_corner(CurbReturn c, float R, int col){
    float a0 = atan2f(c.t1y-c.oy, c.t1x-c.ox);
    float a1 = atan2f(c.t2y-c.oy, c.t2x-c.ox);
    float d  = a1-a0; while(d> M_PI)d-=2*M_PI; while(d<-M_PI)d+=2*M_PI;
    enum { N=10 }; float px=c.t1x, py=c.t1y;
    for (int i=1;i<=N;i++){ float a=a0+d*i/N, x=c.ox+cosf(a)*R, y=c.oy+sinf(a)*R;
        // SEAM: sline() (above) is the symmetric line, but a NAIVE swap here REGRESSES the kerb
        // 7->27 (measured): the fill is point-mirrored about cx=160 (cells sum 320) while sline is
        // exact only for CELL-mirror endpoints (sum 319). Fixing the kerb needs the stroke snapped
        // cell-consistently with the fill, or drawn as a coverage-boundary, or mirror-blit. NOT here.
        line(ri(px),ri(py),ri(x),ri(y),col); px=x; py=y; }
}
// Pass 2 (#5a): the bike lane WRAPS the corner — a terracotta annular band JUST INSIDE the curb-return arc
// (radii R..R+BIKEW about the return centre O), sweeping the same t1→t2 the kerb does. The carriageway sits on
// the far (K) side of the arc, so radii > R are inside it ⇒ the band hugs the kerb and meets each arm's
// outer (kerb-side) bike lane at the tangent points. Reuses the CurbReturn the curb returns already computed.
static void corner_bike(CurbReturn c, float R){
    float a0=atan2f(c.t1y-c.oy,c.t1x-c.ox), a1=atan2f(c.t2y-c.oy,c.t2x-c.ox);
    float d=a1-a0; while(d>M_PI)d-=2*M_PI; while(d<-M_PI)d+=2*M_PI;
    enum { N=10 }; int xy[4*(N+1)]; int k=0;
    for (int i=0;i<=N;i++){ float a=a0+d*i/N; xy[k++]=ri(c.ox+cosf(a)*R);         xy[k++]=ri(c.oy+sinf(a)*R); }
    for (int i=N;i>=0;i--){ float a=a0+d*i/N; xy[k++]=ri(c.ox+cosf(a)*(R+BIKEW)); xy[k++]=ri(c.oy+sinf(a)*(R+BIKEW)); }
    polyfill(xy, 2*(N+1), CLR_BROWN);                              // the terracotta band is SURFACE
    if (!paint) return;
    float px=c.ox+cosf(a0)*(R+BIKEW), py=c.oy+sinf(a0)*(R+BIKEW);   // the carriageway-side edge line (white) is PAINT
    for (int i=1;i<=N;i++){ float a=a0+d*i/N, x=c.ox+cosf(a)*(R+BIKEW), y=c.oy+sinf(a)*(R+BIKEW);
        line(ri(px),ri(py),ri(x),ri(y),CLR_WHITE); px=x; py=y; }
}
static void dashed(float x0,float y0,float x1,float y1,int col){       // a dashed lane/centre line
    float dx=x1-x0, dy=y1-y0, L=sqrtf(dx*dx+dy*dy); if(L<1)return; dx/=L; dy/=L;
    for (float t=0;t<L;t+=8){ float a=t, b=t+4>L?L:t+4;
        line((int)(x0+dx*a),(int)(y0+dy*a),(int)(x0+dx*b),(int)(y0+dy*b),col); }
}

// ── LEG model (mirrors roadlab): a junction is a list of arms, each at a bearing. The two arms of a street
//    are collinear (b and b+180). `crossing` marks the street that SKEWS; T drops the north arm. ──
#define NLEG 4
typedef struct { float base; int crossing; } Leg;          // base bearing (deg); crossing = part of skewable street
static Leg legs[NLEG] = {
    {   0, 0 },   // 0 East  (through street)
    {  90, 1 },   // 1 South (crossing street — skews)
    { 180, 0 },   // 2 West  (through street)
    { 270, 1 },   // 3 North (crossing street — skews)  ← dropped for a T
};
static float cornerR = 8.f;   // curb-return radius (the headline at-grade knob)
static int   lanesPer = 2;    // lanes PER DIRECTION (street width = 2 * lanesPer)
static int   skew     = 0;    // degrees added to the crossing street (the N–S pair)
static int   isT      = 0;    // drop the north arm → a T-junction
static int   turnLanes= 0;    // M3: per-approach left-turn bay + raised median splitter
static int   peds     = 0;    // M5: sidewalks (a strip outside each kerb) + zebra crosswalks at the mouths
static int   roundabout=0;    // M6: a MINI-ROUNDABOUT (traversable island + give-way entries) — excl. turnLanes
static int   freeRight = 0;   // Stage-1 #2: CHANNELIZED RIGHT-TURN slip + pork-chop island — excl. turn lanes/roundabout
static float islandR  = 8.f;  // M6: central-island radius (the headline roundabout knob; reuses [ ] in this mode)
static float frR      = 14.f; // Stage-1 #2: free-right TURN radius (the slip centreline; reuses [ ] in free-right mode)
static int   medOn    = 0;    // M7: continuous raised centre median (a cross-section lane type, not the turn splitter)
static int   twltlOn  = 0;    // Stage-1 #3: two-way left-turn lane (painted centre lane) — excl. with the median (both centre)
static int   driveways = 0;   // Stage-1 #3c: mid-block driveways, a per-side BITMASK — 1 = +90 side, 2 = -90 side ('d' cycles off→+→−→both)
static int   bikeOn   = 0;    // M7/Pass3: 0 off · 1 lanes (+corner-wrap, the default) · 2 +straight-through crossing
static int   parkOn   = 0;    // M7: a parking lane at the kerb
enum { PAT_GRID, PAT_ORGANIC, PAT_RADIAL, PAT_CULDESAC, PAT_SUPERBLOCK, NPAT };   // M4 + Stage-2: street-web patterns
static int   netview  = 0;    // M4: 0 = junction detail (M1–M3), 1 = the street-web network
static int   pattern  = PAT_GRID;
static int   netseed  = 1;
static int   curveAmt = 0;    // M4c: 0 = straight chords … 3 = winding (the §8.5 curvature knob; bows each edge)
// FIELD-BASED ROADS (docs/design/field-based-road-rendering.md) — 'g' toggles it LIVE for A/B. Default
// OFF (shipping = per-arm casing + mirror_blit); -DDE_FIELD_ROADS flips the default on for headless gates.
#ifdef DE_FIELD_ROADS
static int   field_roads = 1;
#else
static int   field_roads = 0;
#endif

// ── RUNG 1 — the OSM→streetlab spike (docs/design/driving-world-program.md, bridge #1). ─────────────
//    Feed the junction renderer a FOREIGN, INDEPENDENT set of arm bearings — exactly what an OSM node
//    hands you (incident ways' bearings) — bypassing the base±skew COLLINEAR-PAIR model. The gut-check:
//    do real-world angles render with clean curb returns through the existing path, unchanged? 'o'
//    cycles off → each sample → off. Every other treatment (curb radius, peds, lanes…) still composes.
//    NLEG=4 caps it at 4 arms; a 5/6-way node is the predicted next step (raise NLEG + a present-count).
//    These bearings are hand-read off real intersections — stand-ins until osm-roads.js emits the graph.
typedef struct { const char *name; int n; float brg[NLEG]; } OsmJunc;
static const OsmJunc osm_samples[] = {
    { "4-way, independent skew", 4, {  10, 100, 195, 280 } },   // NOT collinear pairs — the real test
    { "Y junction (3-way)",      3, {  25, 145, 265 } },        // uneven 3-way, no straight-through
    { "acute fork (4-way)",      4, {  18,  78, 200, 262 } },   // tight corners stress the curb return
};
#define N_OSM ((int)(sizeof osm_samples / sizeof osm_samples[0]))
static int osmMode = 0;   // 0 = off (normal leg model); 1..N_OSM = render sample (osmMode-1)

static float leg_bearing(int i){
    if (osmMode) return fmodf(osm_samples[osmMode-1].brg[i] + 3600, 360);   // RUNG 1: raw OSM bearing, no skew
    float b=legs[i].base + (legs[i].crossing?(float)skew:0.f); return fmodf(b+3600,360); }
static int   leg_present(int i){
    if (osmMode) return i < osm_samples[osmMode-1].n;                       // RUNG 1: arm count from the OSM node
    return !(isT && i==3); }              // T drops North (leg 3)

// present legs, sorted by bearing 0..360; fills idx[]/brg[], returns n
static int present_legs(int *idx, float *brg){
    int n=0;
    for (int i=0;i<NLEG;i++) if (leg_present(i)){ idx[n]=i; brg[n]=leg_bearing(i); n++; }
    for (int a=0;a<n;a++) for (int b=a+1;b<n;b++) if (brg[b]<brg[a]){
        float tb=brg[a]; brg[a]=brg[b]; brg[b]=tb; int ti=idx[a]; idx[a]=idx[b]; idx[b]=ti; }
    return n;
}
// number of CONVEX corners = adjacent gaps strictly between 0 and 180 (a 180° gap is the straight T back). PURE.
static int count_corners(void){
    int idx[NLEG]; float brg[NLEG]; int n=present_legs(idx,brg);
    if (n<2) return 0;
    int c=0;
    for (int i=0;i<n;i++){ float g=fmodf(brg[(i+1)%n]-brg[i]+3600,360); if (g>0.5f && g<179.5f) c++; }
    return c;
}
// corner K = where the two band edges FACING the gap between arms A,B cross (each offset HW toward bisector bm)
static void edge_corner(float cx,float cy,float HW,float bA,float bB,float bm,float*kx,float*ky){
    float dAx=ux(bA),dAy=uy(bA), nAx=ux(bA+90),nAy=uy(bA+90);
    float sA = (nAx*ux(bm)+nAy*uy(bm))>0 ? 1.f : -1.f;     // pick the edge on the gap (grass) side
    float PAx=cx+sA*HW*nAx, PAy=cy+sA*HW*nAy;
    float dBx=ux(bB),dBy=uy(bB), nBx=ux(bB+90),nBy=uy(bB+90);
    float sB = (nBx*ux(bm)+nBy*uy(bm))>0 ? 1.f : -1.f;
    float PBx=cx+sB*HW*nBx, PBy=cy+sB*HW*nBy;
    float det = dAx*(-dBy) - (-dBx)*dAy;                   // solve PA + t·DA = PB + u·DB
    if (fabsf(det)<1e-4f){ *kx=(PAx+PBx)*0.5f; *ky=(PAy+PBy)*0.5f; return; }
    float t = ((PBx-PAx)*(-dBy) - (-dBx)*(PBy-PAy)) / det;
    *kx=PAx+dAx*t; *ky=PAy+dAy*t;
}
// ── M7: the typed CROSS-SECTION (OpenDRIVE lane types §5). A lane-section, centre→out: [median] · driving×N ·
//    [bike] · [parking] · (sidewalk = the M5 peds layer). The carriageway half-width is the SUM of the lanes
//    present — and because every junction primitive (curb returns, the mouth datum, the roundabout, sidewalks)
//    keys off HW, widening the section re-solves the whole junction for free. cross_hw() is the pure datum. ──
// the CENTRE treatment's half-width: a raised median OR a (painted) two-way left-turn lane — mutually exclusive,
// both occupy the centreline. This is the single offset the rest of the section (and every junction primitive
// that reads drive_inner) keys off, so adding TWLTL as a centre lane type re-solves the junction for free.
static float centre_hw(void){ return medOn?MEDHW : twltlOn?TWLTLHW : 0; }
static float cross_hw(void){
    return centre_hw() + lanesPer*LANEW + (bikeOn?BIKEW:0) + (parkOn?PARKW:0);
}
// ── the lane-section as the SINGLE SOURCE OF TRUTH. Offsets from the centreline (one side), centre→out:
//    [median 0..MEDHW] · driving lanes · [bike] · [parking]. Turn lanes, the stop bar, and the roundabout
//    approaches ALL read these instead of hardcoding centre-relative offsets — so the section drives every
//    junction primitive. The HALF-section is the unit (mirrored at the draw site) ⇒ one-way is later a flag. ──
static float drive_inner(void){ return centre_hw(); }                    // where the driving lanes begin (past the centre treatment)
static float drive_outer(void){ return drive_inner()+lanesPer*LANEW; }   // where they end (parking begins)
// Pass 2: the bike lane is OUTERMOST (at the kerb), parking INBOARD of it (the Dutch protected arrangement) —
// so the bike lane can follow the curb-return arc around the corner, and parking ends in the clear zone.
static float park_inner(void){ return drive_outer(); }                   // parking sits just outside the driving lanes
static float bike_inner(void){ return drive_outer()+(parkOn?PARKW:0); }  // bike is outermost (kerb-side) = HW-BIKEW
// a solid lane line along the arm (offset o from centre, on side s = +1/-1), from startd to reach
static void edge_line(float cx,float cy,float b,float o,int s,float startd,float reach,int col){
    float dx=ux(b),dy=uy(b), nx=ux(b+90)*s*o, ny=uy(b+90)*s*o;
    line((int)(cx+dx*startd+nx),(int)(cy+dy*startd+ny),(int)(cx+dx*reach+nx),(int)(cy+dy*reach+ny),col);
}
// fill the band [a,c] from centre on side s along the arm (a tinted lane surface)
static void lane_band(float cx,float cy,float b,float a,float c,int s,float startd,float reach,int col){
    float dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90);
    float x0=cx+dx*startd, y0=cy+dy*startd, x1=cx+dx*reach, y1=cy+dy*reach;
    int q[8]={(int)(x0+nx*s*a),(int)(y0+ny*s*a),(int)(x0+nx*s*c),(int)(y0+ny*s*c),
              (int)(x1+nx*s*c),(int)(y1+ny*s*c),(int)(x1+nx*s*a),(int)(y1+ny*s*a)};
    polyfill(q,4,col);
}
// markings along one arm: the typed cross-section. TWO start datums: the CENTRE markings (centreline/median +
// driving dividers) start at `cstd` — pushed past the turn bay so they don't draw over the median splitter; the
// KERB-side lanes (bike/parking) start at `kstd` (the corner clearance), since they sit at the kerb and are
// unaffected by a central turn bay (the fix: a turn bay no longer shoves the bike lane back off the arm).
static void cross_markings(float cx,float cy,float b,float kstdP,float kstdM,float cstd,float reach){
    float dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90);
    float cx0=cx+dx*cstd, cy0=cy+dy*cstd, x1=cx+dx*reach, y1=cy+dy*reach;
    float inner = drive_inner();                            // driving lanes start here (outside the median)
    if (medOn){                                             // a raised centre median (SURFACE: light-grey + kerbs)
        if (surfaces){
        lane_band(cx,cy,b, 0, MEDHW, +1, cstd,reach, CLR_LIGHT_GREY);
        lane_band(cx,cy,b, 0, MEDHW, -1, cstd,reach, CLR_LIGHT_GREY);
        edge_line(cx,cy,b, MEDHW,+1, cstd,reach, CLR_BROWNISH_BLACK);
        edge_line(cx,cy,b, MEDHW,-1, cstd,reach, CLR_BROWNISH_BLACK);
        }
    } else if (twltlOn){                                     // Stage-1 #3: TWO-WAY LEFT-TURN LANE (PAINTED centre lane)
        // each side: a SOLID yellow (outer, no through-cross) + a DASHED yellow just inside (left-turners enter);
        // surface stays asphalt. Alternating left-turn arrows down the centreline = the bidirectional tell.
        if (paint){
        for (int s=-1;s<=1;s+=2){
            edge_line(cx,cy,b, TWLTLHW, s, cstd,reach, CLR_YELLOW);            // outer solid yellow
            float o=TWLTLHW-1.5f;                                             // inner dashed yellow (may enter)
            dashed(cx0+nx*s*o,cy0+ny*s*o, x1+nx*s*o,y1+ny*s*o, CLR_YELLOW);
        }
        float L=sqrtf((x1-cx0)*(x1-cx0)+(y1-cy0)*(y1-cy0));                   // alternate the arrow side each step
        for (float t=10, k=0; t<L && t<150; t+=22, k++){
            int s=(((int)k)&1)?1:-1; float px=cx0+dx*t, py=cy0+dy*t, ax=px+nx*s*2.5f, ay=py+ny*s*2.5f;
            line((int)(px-dx*3),(int)(py-dy*3),(int)ax,(int)ay,CLR_YELLOW);   // turning stroke → tip on side s
            line((int)ax,(int)ay,(int)(ax-nx*s*2-dx*1.5f),(int)(ay-ny*s*2-dy*1.5f),CLR_YELLOW);   // barb
            line((int)ax,(int)ay,(int)(ax-nx*s*2+dx*1.5f),(int)(ay-ny*s*2+dy*1.5f),CLR_YELLOW);   // barb
        }
        }
    } else if (paint){
        dashed(cx0,cy0,x1,y1,CLR_YELLOW);                   // the centreline (no centre treatment)
    }
    if (paint) for (int k=1;k<lanesPer;k++){               // dashed driving-lane dividers
        float o=inner+k*LANEW;
        dashed(cx0+nx*o,cy0+ny*o, x1+nx*o,y1+ny*o, CLR_WHITE);
        dashed(cx0-nx*o,cy0-ny*o, x1-nx*o,y1-ny*o, CLR_WHITE);
    }
    // the kerb-side lanes start PER SIDE (kstdP on the +90 side, kstdM on the -90 side) so a skewed arm's
    // gentler corner doesn't leave a gap to the corner-wrap. (Both equal on a straight 4-way.)
    float pi=park_inner(), bi=bike_inner();
    for (int s=-1;s<=1;s+=2){
        float kstd = (s>0)?kstdP:kstdM;
        if (parkOn && paint){                               // PARKING is paint only (bay edge + ticks) — ends PCLEAR back
            float ps=kstd+PCLEAR;
            edge_line(cx,cy,b, pi,s, ps,reach, CLR_WHITE);
            float qx0=cx+dx*ps, qy0=cy+dy*ps; float L=sqrtf((x1-qx0)*(x1-qx0)+(y1-qy0)*(y1-qy0));
            for (float t=2; t<L; t+=16){                     // bay-delimiter ticks
                float qx=cx+dx*(ps+t), qy=cy+dy*(ps+t);
                line((int)(qx+nx*s*pi),(int)(qy+ny*s*pi),(int)(qx+nx*s*(pi+PARKW)),(int)(qy+ny*s*(pi+PARKW)),CLR_WHITE);
            }
        }
        if (bikeOn){                                        // BIKE: terracotta SURFACE (always) + white edge LINE (paint)
            if (surfaces) lane_band(cx,cy,b, bi, bi+BIKEW, s, kstd,reach, CLR_BROWN);
            if (paint)    edge_line(cx,cy,b, bi,s, kstd,reach, CLR_WHITE);
        }
    }
}
// Pass 3 (#5b, OPTIONAL): the straight-through bike CROSSING — "elephant's feet", a dashed row of terracotta
// squares continuing the (outermost) bike lane across the junction box to the hub. Contextual (signalised vs
// priority), so it's opt-in (bike level 2). The dash phase is anchored at the HUB (squares at fixed multiples of
// the step) — NOT the mouth — so collinear arms align and the gaps stay EVEN across the box at any skew.
#define BTSTEP 5    // elephant's-feet pitch (px)
static void bike_thru(float cx,float cy,float b,float mouth){
    float dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90);
    float off=(bike_inner()+cross_hw())*0.5f;               // centre of the kerb-side bike lane
    for (float d=BTSTEP; d<mouth; d+=BTSTEP)                 // hub-anchored ⇒ even + symmetric across the box
        for (int s=-1;s<=1;s+=2)
            rectfill((int)(cx+dx*d+nx*s*off)-1,(int)(cy+dy*d+ny*s*off)-1, 3,3, CLR_BROWN);
}

// ── Stage-1 #3c: DRIVEWAY — a mid-block low-volume access point (FHWA access management §3). A flared kerb-cut
//    apron crosses the kerb (and the sidewalk, if peds) out into the grass toward an implied off-screen property:
//    the asphalt apron covers the kerb casing at the mouth (= the cut), brownish-black kerb "wings" splay the
//    flare. Drawn AFTER the junction so it composes with any treatment. d = distance along arm b, side s = ±1. ──
static void draw_driveway(float cx,float cy,float b,float d,int s){
    float dx=ux(b),dy=uy(b), nx=ux(b+90)*s, ny=uy(b+90)*s;   // along-arm + the chosen side's outward normal
    float in=cross_hw(), out=cross_hw() + (peds?SW:0) + 8;   // road edge → past the footway into the grass
    float w=DWAY_W*0.5f, fl=DWAY_FL;
    float ax=cx+dx*(d-w-fl), ay=cy+dy*(d-w-fl);              // kerb-side wings (wider), upstream + downstream
    float bx=cx+dx*(d+w+fl), by=cy+dy*(d+w+fl);
    float pdx=cx+dx*(d-w), pdy=cy+dy*(d-w);                  // property-side (narrower)
    float pex=cx+dx*(d+w), pey=cy+dy*(d+w);
    int q[8]={(int)(ax+nx*in),(int)(ay+ny*in),(int)(bx+nx*in),(int)(by+ny*in),
              (int)(pex+nx*out),(int)(pey+ny*out),(int)(pdx+nx*out),(int)(pdy+ny*out)};
    polyfill(q,4,CLR_DARK_GREY);                             // apron asphalt (covers the kerb casing ⇒ the cut)
    line((int)(ax+nx*in),(int)(ay+ny*in),(int)(pdx+nx*out),(int)(pdy+ny*out),CLR_BROWNISH_BLACK);   // kerb wings
    line((int)(bx+nx*in),(int)(by+ny*in),(int)(pex+nx*out),(int)(pey+ny*out),CLR_BROWNISH_BLACK);
}

// ── M3: turn lanes + channelizing islands ──
// arm_face = axial distance from the hub to where this arm's curb returns sit (its "mouth"). It's the
// largest corner distance over the arm's two convex gaps (an acute gap reaches further down the arm). PURE.
static float arm_face(const float *brg,int n,int i,float HW){
    float gP=fmodf(brg[i]-brg[(i-1+n)%n]+3600,360), gN=fmodf(brg[(i+1)%n]-brg[i]+3600,360);
    float m=0;
    if (gP>0.5f&&gP<179.5f){ float d=HW/tanf(gP*0.5f*DEG2RAD); if(d>m)m=d; }
    if (gN>0.5f&&gN<179.5f){ float d=HW/tanf(gN*0.5f*DEG2RAD); if(d>m)m=d; }
    return m;
}
// PER-SIDE kerb-lane START — a SKEWED arm's two sides face DIFFERENT gaps (one acute, one obtuse), so the
// kerb-side lanes (bike/parking) must start per side, else the gentler side gaps from the corner-wrap. For convex
// gaps the +90 side faces the NEXT arm's gap, the -90 side the PREV. The lane must start at the curb-return
// TANGENT, which sits at (HW+R)/tan(half) from the hub along the arm — NOT HW/tan(half) + R (equal only at 90°,
// so on obtuse corners with a big R the lane would start PAST the tangent and gap). A 2px overlap onto the arc. PURE.
static float kerb_start(const float *brg,int n,int i,float HW,float R,int side){
    float g = (side>0) ? fmodf(brg[(i+1)%n]-brg[i]+3600,360)
                       : fmodf(brg[i]-brg[(i-1+n)%n]+3600,360);
    if (g<=0.5f || g>=179.5f) return 0;                     // straight (a T's back / through side) — NO corner, so the
                                                            // lane runs continuously to the hub (both halves meet there)
    return (HW+R)/tanf(g*0.5f*DEG2RAD) - 2.f;
}
// LEFT-TURN ARROW: shaft pointing into the junction (inbound) with a head hooking LEFT (across the
// centreline) — the glyph that marks a lane as turn-only. Lives in the arm's local frame ⇒ skew-safe.
// a turn arrow in the inbound (b-90 side) lane centred at offset `lc`, hooking LEFT (h=+1, across the centreline =
// a left-turn bay) or RIGHT (h=-1, toward the kerb = a right-turn pocket). The h sign mirrors the whole arrowhead.
static void turn_arrow_at(float cx,float cy,float b,float df,float lc,int h,int col){
    float ax=ux(b),ay=uy(b), ix=ux(b-90),iy=uy(b-90);     // axis + inbound-side normal
    float bx=cx+ax*(df+9)+ix*lc, by=cy+ay*(df+9)+iy*lc;   // base, in the inbound lane
    float tx=cx+ax*(df+2)+ix*lc, ty=cy+ay*(df+2)+iy*lc;   // tip, nearer the stop bar
    line((int)bx,(int)by,(int)tx,(int)ty,col);                                       // shaft (inbound)
    float hx=tx+ux(b+90*h)*3, hy=ty+uy(b+90*h)*3;                                     // hook (left across centre / right to kerb)
    line((int)tx,(int)ty,(int)hx,(int)hy,col);
    line((int)hx,(int)hy,(int)(hx+ux(b-30*h)*3),(int)(hy+uy(b-30*h)*3),col);          // arrowhead on the hook
    line((int)hx,(int)hy,(int)(hx+ux(b+150*h)*3),(int)(hy+uy(b+150*h)*3),col);
}
static void turn_arrow(float cx,float cy,float b,float df,int col){                  // left-turn bay arrow (inner lane)
    turn_arrow_at(cx,cy,b,df, drive_inner()+LANEW*0.5f, +1, col);
}
// RAISED MEDIAN SPLITTER (the channelizing island): a thin island on the centreline from df out to
// df+POCKET+PTAPER with a rounded nose upstream — separates opposing traffic and frames the left-turn bay.
static void draw_median(float cx,float cy,float b,float df){
    float ax=ux(b),ay=uy(b), nx=ux(b+90),ny=uy(b+90);
    float s0=df+2, s1=df+POCKET+PTAPER;
    int q[8]={ (int)(cx+ax*s0+nx*MEDW),(int)(cy+ay*s0+ny*MEDW),
               (int)(cx+ax*s0-nx*MEDW),(int)(cy+ay*s0-ny*MEDW),
               (int)(cx+ax*s1-nx*MEDW),(int)(cy+ay*s1-ny*MEDW),
               (int)(cx+ax*s1+nx*MEDW),(int)(cy+ay*s1+ny*MEDW) };
    polyfill(q,4,CLR_LIGHT_GREY);
    circfill((int)(cx+ax*s1),(int)(cy+ay*s1),MEDW,CLR_LIGHT_GREY);   // rounded nose
    line(q[0],q[1],q[6],q[7],CLR_BROWNISH_BLACK);          // kerb edges
    line(q[2],q[3],q[4],q[5],CLR_BROWNISH_BLACK);
}
// ── M5: a ZEBRA CROSSWALK across one approach at the junction mouth (df). Stripes run ALONG the travel
//    direction (cars drive over them lengthwise), spaced across the full carriageway width. The whole-street
//    sidewalks are drawn as the footprint inflated by SW (in draw()); this is the per-approach crossing. ──
static void draw_crosswalk(float cx,float cy,float b,float HW,float df){
    float ax=ux(b),ay=uy(b), nx=ux(b+90),ny=uy(b+90);
    float s0=df+1, s1=df+1+CWDEP;
    for (float o=-HW+1.5f; o<HW; o+=3.f)                    // a stripe per ~3px across the width
        line((int)(cx+ax*s0+nx*o),(int)(cy+ay*s0+ny*o),(int)(cx+ax*s1+nx*o),(int)(cy+ay*s1+ny*o),CLR_WHITE);
}
// ── Stage-1 #1: a BULB-OUT (curb extension). The kerb extends into the PARKING lane at the crossing — it fills
//    the parking CLEAR ZONE (#8) we already leave, shortening the ped crossing and tightening the turn. Sidewalk-
//    grey + an inboard kerb edge, in the parking band on both sides of the arm, from the mouth out to where
//    parking begins. Only with pavement+parking (you bulb INTO the parking lane, never a travel lane). ──
static void draw_bulb(float cx,float cy,float b,float kstdP,float kstdM){
    float pi=park_inner();
    float ax=ux(b),ay=uy(b), nx=ux(b+90),ny=uy(b+90);
    for (int s=-1;s<=1;s+=2){
        float kstd=(s>0)?kstdP:kstdM, s1=kstd+PCLEAR;       // per side (a skewed corner differs each side)
        lane_band(cx,cy,b, pi, pi+PARKW, s, kstd, s1, CLR_LIGHT_GREY);   // the kerb extension (sidewalk surface)
        edge_line(cx,cy,b, pi, s, kstd, s1, CLR_BROWNISH_BLACK);         // its inboard kerb (against the carriageway)
        line((int)(cx+ax*s1+nx*s*pi),(int)(cy+ay*s1+ny*s*pi),           // the nose, tapering back to the parking lane
             (int)(cx+ax*s1+nx*s*(pi+PARKW)),(int)(cy+ay*s1+ny*s*(pi+PARKW)),CLR_BROWNISH_BLACK);
    }
}

// ── M6: the MINI-ROUNDABOUT (at-grade, §2) — distinct from roadlab's GRADE-SEPARATED ring. The at-grade
//    tells: the central island is MINI + TRAVERSABLE (mountable/domed — long vehicles drive straight OVER
//    it), the splitters are flush, and entries GIVE WAY (yield), they don't stop. Built on the same leg
//    model ⇒ any leg count / skew for free. Circulatory width = one direction's HW; the inscribed circle =
//    island + circulatory. island radius is the headline knob (reuses the [ ] slot in this mode). PURE: ──
static float round_circ_w(void){ return cross_hw(); }            // circulatory = the FULL approach half-width (incl.
                                                                 // bike/parking/median) ⇒ the disc always bulges a
                                                                 // consistent islandR past the arms + the sidewalk ring
                                                                 // meets the arm sidewalks (else parking undersizes it)
static float round_icr(void){ return islandR + round_circ_w(); } // inscribed-circle radius (island + ring)
// where an approach bike lane STARTS so its kerb-side edge (lateral HW from the arm centreline) lands exactly on
// the ring (radius ICR): the axial distance d with sqrt(d^2 + HW^2) == ICR. Straight lanes are laterally offset,
// so this FLARE point — not ICR itself — is where they meet the concentric ring. PURE (ICR>HW always). ──
static float round_flare(float icr,float hw){ float d=icr*icr-hw*hw; return d>1.f ? sqrtf(d) : 1.f; }

// the traversable central ISLAND: a low domed/painted disc — the mini tell. Apron (mountable) + white dome.
static void draw_island(float cx,float cy){
    int r=(int)islandR;
    circfill((int)cx,(int)cy,r+1,CLR_BROWNISH_BLACK);            // thin kerb ring (low — you can mount it)
    circfill((int)cx,(int)cy,r,  CLR_LIGHT_GREY);               // mountable apron
    circfill((int)cx,(int)cy,(int)(islandR*0.55f),CLR_WHITE);    // painted dome
}
// circulating ARROWS on the ring midline. Drive-on-right ⇒ keep the island on the LEFT ⇒ travel visually
// COUNTERCLOCKWISE; with screen y pointing down that's the a-90 tangent. Each = a little shaft + arrowhead.
static void draw_circulation(float cx,float cy){
    float rm = islandR + round_circ_w()*0.5f;
    for (int k=0;k<3;k++){
        float a = 60 + k*120;                                    // 3 arrows, between the arms
        float px=cx+ux(a)*rm, py=cy+uy(a)*rm, t=a-90;            // CCW tangent (visual, y-down)
        float bx=px-ux(t)*2, by=py-uy(t)*2, hx=px+ux(t)*3, hy=py+uy(t)*3;   // base → head
        line((int)bx,(int)by,(int)hx,(int)hy,CLR_YELLOW);                                   // shaft
        line((int)hx,(int)hy,(int)(hx+ux(t+145)*3),(int)(hy+uy(t+145)*3),CLR_YELLOW);        // barb
        line((int)hx,(int)hy,(int)(hx+ux(t-145)*3),(int)(hy+uy(t-145)*3),CLR_YELLOW);        // barb
    }
}
// a GIVE-WAY (yield) line across the inbound half of one approach at `base` — dashes, NOT a solid stop bar.
static void give_way(float cx,float cy,float b,float base,float hw){
    float dx=ux(b),dy=uy(b), ix=ux(b-90),iy=uy(b-90);
    float gx=cx+dx*base, gy=cy+dy*base;
    for (float o=1.5f; o<hw; o+=4.f)                            // transverse dashes (the give-way look)
        line((int)(gx+ix*o),(int)(gy+iy*o),(int)(gx+ix*(o+2)),(int)(gy+iy*(o+2)),CLR_WHITE);
}
// a SPLITTER island: a teardrop on the approach centreline — widest at the circulatory (`inner`), tapering
// to a point `len` upstream. Deflects entering traffic + separates entry from exit; doubles as a ped refuge.
static void draw_splitter(float cx,float cy,float b,float inner,float len){
    float ax=ux(b),ay=uy(b), nx=ux(b+90),ny=uy(b+90); float W=3.f;
    float s0=inner, s1=inner+len;
    int t[6]={ (int)(cx+ax*s0+nx*W),(int)(cy+ay*s0+ny*W),
               (int)(cx+ax*s0-nx*W),(int)(cy+ay*s0-ny*W),
               (int)(cx+ax*s1),     (int)(cy+ay*s1)     };     // point, upstream
    polyfill(t,3,CLR_LIGHT_GREY);
    line(t[0],t[1],t[4],t[5],CLR_BROWNISH_BLACK);              // kerb edges
    line(t[2],t[3],t[4],t[5],CLR_BROWNISH_BLACK);
}

// ── Stage-1 #2: the CHANNELIZED RIGHT-TURN (free-right) slip + triangular "pork-chop" channelizing island.
//    The kerb-side lane peels onto a wider corner arc that bypasses the junction box; the wedge left between the
//    slip and the box corner K becomes a raised, YIELD-controlled island (a ped refuge too). Grounded in the
//    AASHTO Green Book / NCHRP "Design Guidance for Channelized Right-Turn Lanes": a SMALL turning radius (slow,
//    ped-friendly — research §2/§6), island min side ~15ft, yield at the exit, crosswalk across the slip centre.
//    Built from two curb returns sharing the corner K: the slip's outer kerb is the corner BULGED OUT by SLIPW
//    (Ro = cornerR+SLIPW, tangent to both arms ⇒ the slip merges with each arm's kerb lane); the island is the
//    normal cornerR fillet (the tight turning radius), painted ON TOP, so the slip survives as the SLIPW-wide
//    ring hugging its building side. Drive-on-right ⇒ one per convex corner; the exit (yield) is the bA arm. ──
// fill a constant-width ANNULAR ring [r0,r1] about a fillet centre, swept over t1→t2 — the slip lane band.
static void fill_ring(CurbReturn c, float r0, float r1, int col){
    float a0=atan2f(c.t1y-c.oy,c.t1x-c.ox), a1=atan2f(c.t2y-c.oy,c.t2x-c.ox);
    float d=a1-a0; while(d>M_PI)d-=2*M_PI; while(d<-M_PI)d+=2*M_PI;
    enum{N=12}; int xy[4*(N+1)]; int k=0;
    for(int i=0;i<=N;i++){ float a=a0+d*i/N; xy[k++]=ri(c.ox+cosf(a)*r1); xy[k++]=ri(c.oy+sinf(a)*r1); }
    for(int i=N;i>=0;i--){ float a=a0+d*i/N; xy[k++]=ri(c.ox+cosf(a)*r0); xy[k++]=ri(c.oy+sinf(a)*r0); }
    polyfill(xy,2*(N+1),col);
}
// a free-right serves a roughly-perpendicular right turn — drawn only for a sensible turn-angle window. PURE.
static int fr_fits(float gap){ return gap > 40.f && gap < 145.f; }
// ── the free-right slip = the KERB-SIDE LANE turned into a curve that cuts the corner; the raised pork-chop
//    island is the wedge it leaves between itself and the junction box. Anchored on the kerb-side-lane CENTRELINE
//    (lc inboard of the kerb), so the slip occupies only the OUTER lane — the inner through lanes stay clear (the
//    fix for the island overlapping the straight-ahead lanes). Slip = the centreline ±SLIPW/2 (concentric ⇒
//    constant width). Drive-on-right ⇒ the right turn exits onto the bA arm (the yield is there). ──
static void draw_freeright(float cx,float cy,float bA,float bB,float bm){
    // anchor on the DRIVING-carriageway edge (drive_outer), NOT the kerb (HW): the slip is the kerb-side DRIVING
    // lane bent into a curve, so any bike lane / parking sits OUTBOARD of it (the cycle track wraps the corner as
    // its own thing — drawn at the corner site). Without bike/parking, drive_outer()==HW ⇒ identical to before.
    float cw=drive_outer(), lc=cw-SLIPW*0.5f;         // lc = kerb-side DRIVING lane centre (half a slip-width in)
    float kcx,kcy; edge_corner(cx,cy, lc, bA,bB,bm, &kcx,&kcy);
    CurbReturn cc=curb_return(kcx,kcy, bA,bB, frR);   // the slip CENTRELINE fillet (turn radius frR), about cc.o
    float ri=frR-SLIPW*0.5f, ro=frR+SLIPW*0.5f;       // slip lane = centreline ± half-width (concentric ⇒ constant)
    // ISLAND: the pork-chop lives ENTIRELY in the corner zone OUTSIDE both through roads' kerb lines — it must
    // never sit in a travel lane (a straight-ahead driver would have to swerve round it). Recipe: fill the corner
    // generously from the kerb corner out to the slip's inner edge, THEN re-lay each road's gap-side carriageway
    // (centre→kerb) as asphalt, carving the island back to just the wedge beyond the kerb lines. At 90° that wedge
    // is almost nothing (correct — a perpendicular free-right barely needs an island, and a fat one only blocks the
    // through movement); as the corner skews acute it opens into a real triangle that's clear of every lane.
    float kkx,kky; edge_corner(cx,cy, cw, bA,bB,bm, &kkx,&kky);   // the carriageway corner (the two driving-edge lines meet)
    fill_corner(kkx,kky, cc, ro, CLR_LIGHT_GREY);
    float REACH=SCREEN_W+SCREEN_H;
    for (int s=0;s<2;s++){                            // reopen ONLY the gap-facing half of each arm (centre→carriageway edge),
        float b=s?bB:bA, dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90);   // so a neighbouring corner's slip is untouched
        float sg=(nx*ux(bm)+ny*uy(bm))>0?1.f:-1.f, ox=cx+dx*REACH, oy=cy+dy*REACH;
        int q[8]={(int)cx,(int)cy,(int)(cx+nx*sg*cw),(int)(cy+ny*sg*cw),
                  (int)(ox+nx*sg*cw),(int)(oy+ny*sg*cw),(int)ox,(int)oy};
        polyfill(q,4,CLR_DARK_GREY);
    }
    fill_ring(cc, ri, ro, CLR_DARK_GREY);             // the SLIP lane, re-laid over the reopened kerb lane
    stroke_corner(cc, ro, CLR_BROWNISH_BLACK);        // island kerb = the slip's inner edge
    stroke_corner(cc, ri, CLR_WHITE);                 // the slip's outer (kerb-side) lane line ⇒ reads as a lane
    // YIELD where the slip merges onto the EXIT arm (bA = the cc.t1 side). The radius to cc.t1 is ⊥ the arm, so a
    // dashed bar along that radial (across the slip band ri..ro) crosses the slip throat square-on.
    float a0=atan2f(cc.t1y-cc.oy, cc.t1x-cc.ox), dx=cosf(a0), dy=sinf(a0);
    for (float r=ri+0.5f; r<ro; r+=2.f)               // transverse dashes across the slip = give-way
        line((int)(cc.ox+dx*r),(int)(cc.oy+dy*r),(int)(cc.ox+dx*(r+1)),(int)(cc.oy+dy*(r+1)),CLR_WHITE);
}
// FIELD free-right: a clean pork-chop ISLAND overlay. The field (fr_render) already laid the GENEROUS
// rounded corner (radius frR = the turn) as asphalt + a uniform kerb; this drops a raised LIGHT_GREY island
// (the small cornerR fillet at the corner) into it, leaving the right-turn SLIP as the asphalt ring around it.
// A markings-layer overlay (drive-on-right, gated under `markings`) — so a markings-off render stays the clean
// field road and road-check stays meaningful. Far simpler + tidier than the old slip/ring/re-lay dance.
static void draw_freeright_island(float cx,float cy,float HW,float bA,float bB,float bm){
    float kx,ky; edge_corner(cx,cy,HW, bA,bB,bm,&kx,&ky);
    CurbReturn cc = curb_return(kx,ky, bA,bB, frR);      // the SAME fillet the field corner used (centre cc.o)
    float islR = frR - SLIPW; if (islR < 2) islR = 2;    // island = corner shrunk by one slip-width ⇒ CONCENTRIC arcs
    fill_corner(kx,ky, cc, islR, CLR_LIGHT_GREY);        // raised island surface (mountable kerb-grey)
    stroke_corner(cc, islR, CLR_BROWNISH_BLACK);         // island kerb = the slip's inner edge
    // the slip is now a CONSTANT-width ring [islR, frR] about cc.o at any angle (no more skew-dependent width).
    // give-way dashes (PAINT) across it where it rejoins the exit arm (the bA tangent radius).
    if (!paint) return;
    float a0=atan2f(cc.t1y-cc.oy, cc.t1x-cc.ox), dx=cosf(a0), dy=sinf(a0);
    for (float r=islR+0.5f; r<frR; r+=2.f)
        line((int)(cc.ox+dx*r),(int)(cc.oy+dy*r),(int)(cc.ox+dx*(r+1)),(int)(cc.oy+dy*(r+1)),CLR_WHITE);
}

// the junction TREATMENT is exactly one of {0 plain, 1 turn lanes, 2 free-right, 3 roundabout} — mutually
// exclusive (each is a whole-corner channelization scheme that re-draws the corners differently).
static void set_treatment(int t){ turnLanes=(t==1); freeRight=(t==2); roundabout=(t==3); }
static int  cur_treatment(void){ return roundabout?3 : freeRight?2 : turnLanes?1 : 0; }
// the CENTRE cross-section treatment cycles none → raised median → TWLTL → none (they're mutually exclusive —
// both live on the centreline). One control for both ⇒ the exclusion is structural, not a guard.
static void cycle_centre(void){ if (medOn){ medOn=0; twltlOn=1; } else if (twltlOn){ twltlOn=0; } else medOn=1; }

void update(void){
    if (keyp('v')||keyp('V')) netview = !netview;            // M4: junction-detail ↔ street-web view
    if (netview){
        if (keyp('[')) { netseed--; if(netseed<0) netseed=0; }
        if (keyp(']')) netseed++;
        if (keyp('b')||keyp('B')) pattern=(pattern+1)%NPAT;
        if (keyp('c')||keyp('C')) curveAmt=(curveAmt+1)%4;   // M4c: cycle the winding knob 0..3
        return;
    }
    if (keyp('['))  { if (roundabout) islandR-=2; else if (freeRight) frR-=2; else cornerR-=2; }  // [ ] = the active radius
    if (keyp(']'))  { if (roundabout) islandR+=2; else if (freeRight) frR+=2; else cornerR+=2; }
    if (keyp('-'))  lanesPer--;
    if (keyp('='))  lanesPer++;
    if (keyp(','))  skew -= 5;
    if (keyp('.'))  skew += 5;
    if (keyp('t')||keyp('T')) isT = !isT;
    if (keyp('p')||keyp('P')) set_treatment(cur_treatment()==1 ? 0 : 1);   // turn lanes (mutually excl.)
    if (keyp('f')||keyp('F')) set_treatment(cur_treatment()==2 ? 0 : 2);   // Stage-1 #2: free-right slip (excl.)
    if (keyp('r')||keyp('R')) set_treatment(cur_treatment()==3 ? 0 : 3);   // M6: mini-roundabout (excl.)
    if (keyp('k')||keyp('K')) peds = !peds;                  // M5: sidewalks + crosswalks
    if (keyp('m')||keyp('M')) cycle_centre();                // M7/#3: centre cross-section — none → median → TWLTL
    if (keyp('b')||keyp('B')) bikeOn = (bikeOn+1)%3;         // M7/Pass3: off → lanes → +straight-through crossing
    if (keyp(';'))            parkOn = !parkOn;              // M7: parking lane
    if (keyp('d')||keyp('D')) driveways = (driveways+1)&3;   // Stage-1 #3c: driveways per-side cycle off→+→−→both
    if (keyp('n')||keyp('N')) paint = !paint;                // toggle the PAINTED markings (lines/dashes/arrows); surfaces stay
    if (keyp('g')||keyp('G')) field_roads = !field_roads;    // toggle field-based road rendering (A/B vs the old per-arm path)
    if (keyp('o')||keyp('O')) osmMode = (osmMode+1) % (N_OSM+1);   // RUNG 1: cycle off → OSM samples (foreign bearings)
    if (cornerR<0) cornerR=0;  if (cornerR>28) cornerR=28;
    if (islandR<3) islandR=3;  if (islandR>20) islandR=20;   // M6: stays MINI (small, traversable)
    if (frR<8) frR=8;       if (frR>24) frR=24;           // Stage-1 #2: free-right turning radius
    if (lanesPer<1) lanesPer=1; if (lanesPer>3) lanesPer=3;
    if (skew<-60) skew=-60;    if (skew>60) skew=60;
#ifdef DE_TRACE
    watch("cornerR","%.1f",cornerR); watch("skew","%d",skew); watch("corners","%d",count_corners());
#endif
}

// step_btn: a "-/+" stepper as two ui buttons; returns -1, 0 or +1 (borrowed from roadlab's chassis)
static int step_btn(int x,int y,int w,const char*lm,const char*rm){
    int d=0;
    if (ui_button(x,     y, w, 13, lm)) d=-1;
    if (ui_button(x+w+1, y, w, 13, rm)) d=+1;
    return d;
}

// ── M4: the street WEB (network topology) ── one tier ABOVE the single junction: a GRAPH of intersections
//    (nodes) + street segments (edges), generated deterministically from a seed. The canonical planning
//    patterns (grid / organic / radial / cul-de-sac — road-hierarchy-notes §8) are real archetypes; here
//    they're a morph over one generator. gen_network() is a PURE fn of (pattern, seed) — the spec asserts
//    on the graph directly (node/edge counts, mean degree, dead-ends — the SNDi measures, §8.2). M4a does
//    grid + organic (a jitter morph on one lattice); radial + cul-de-sac (real topology changes) are M4b.
#define GW 10                      // node columns (10 ⇒ arterial cols {0,3,6,9} divide cleanly by SB)
#define GH 7                       // node rows    (7  ⇒ arterial rows {0,3,6} divide cleanly by SB)
#define SB 3                       // Stage-2 superblock: an arterial on every SB-th lattice line ⇒ 3×3 cells
#define R_RINGS   5                // radial: concentric rings
#define R_SECTORS 8                // radial: spokes
#define MAXNODE (GW*GH)
#define MAXEDGE (MAXNODE*3)
static const char* PAT_NAME[NPAT] = { "grid", "organic", "radial", "cul-de-sac", "superblock" };
typedef struct { float x,y; } NetNode;
typedef struct { int a,b,arterial; } NetEdge;   // arterial=1 ⇒ engineered through-route: drawn STRAIGHT (the curve knob skips it). Default 0 (C zero-init) ⇒ every other pattern bows as before.
static NetNode nnodes[MAXNODE]; static NetEdge nedges[MAXEDGE];
static int nn=0, ne=0;

// a hash → [0,1): deterministic noise from integer coords + seed (NOT engine rnd, so it's frame-independent
// and spec-stable — the network is a pure function of (pattern, seed), reproducible anywhere).
static float hash01(int x,int y,int s){
    unsigned int h=(unsigned)(x*374761393 + y*668265263 + s*1274126177 + 0x9e3779b9);
    h=(h^(h>>15))*2246822519u; h=(h^(h>>13))*3266489917u; h^=h>>16;
    return (h & 0xffffff) / (float)0x1000000;
}
static int uf[MAXNODE];
static int ufind(int x){ while(uf[x]!=x){ uf[x]=uf[uf[x]]; x=uf[x]; } return x; }

// RADIAL: a hub + concentric rings of sectors, joined by spokes (hub→ring0, then ring→ring) and ring
// cycles. The hub is the high-degree node (degree = R_SECTORS) — a radial-concentric signature.
static void gen_radial(int seed){
    float cx=SCREEN_W/2.f, cy=(NETTOP + (SCREEN_H-TOOLBAR-10))/2.f;
    float maxR=((SCREEN_H-TOOLBAR-10)-NETTOP)/2.f - 2;
    nnodes[nn++]=(NetNode){ cx, cy };                                  // node 0 = hub
    int ring0=1;
    for (int r=0;r<R_RINGS;r++){ float rad=maxR*(r+1)/R_RINGS;
        for (int s=0;s<R_SECTORS;s++){ float a=360.f*s/R_SECTORS + (hash01(r,s,seed)-0.5f)*8.f;
            nnodes[nn++]=(NetNode){ cx+ux(a)*rad, cy+uy(a)*rad }; }
    }
    for (int s=0;s<R_SECTORS;s++) nedges[ne++]=(NetEdge){ 0, ring0+s };                       // hub spokes
    for (int r=0;r<R_RINGS-1;r++) for (int s=0;s<R_SECTORS;s++)                                // inter-ring spokes
        nedges[ne++]=(NetEdge){ ring0+r*R_SECTORS+s, ring0+(r+1)*R_SECTORS+s };
    for (int r=0;r<R_RINGS;r++) for (int s=0;s<R_SECTORS;s++)                                  // ring cycles
        nedges[ne++]=(NetEdge){ ring0+r*R_SECTORS+s, ring0+r*R_SECTORS+(s+1)%R_SECTORS };
}
// CUL-DE-SAC / dendritic: a random SPANNING TREE over the grid (connected + tree-like ⇒ many dead-ends)
// plus a sparse fraction of extra edges (loops) — the loops-and-lollipops pattern. Kruskal over hash weights.
static void gen_culdesac(int id[GH][GW], int seed){
    NetEdge cand[MAXEDGE]; float wt[MAXEDGE]; int nc=0;
    for (int r=0;r<GH;r++) for (int c=0;c<GW;c++){
        if (c+1<GW){ cand[nc]=(NetEdge){id[r][c],id[r][c+1]}; wt[nc]=hash01(c,r,seed);     nc++; }
        if (r+1<GH){ cand[nc]=(NetEdge){id[r][c],id[r+1][c]}; wt[nc]=hash01(c,r,seed+555); nc++; }
    }
    int ord[MAXEDGE]; for(int i=0;i<nc;i++) ord[i]=i;                  // order candidates by weight (selection sort, small n)
    for(int i=0;i<nc;i++){ int m=i; for(int j=i+1;j<nc;j++) if(wt[ord[j]]<wt[ord[m]]) m=j;
        int t=ord[i]; ord[i]=ord[m]; ord[m]=t; }
    for(int i=0;i<nn;i++) uf[i]=i;
    for(int i=0;i<nc;i++){ NetEdge e=cand[ord[i]]; int ra=ufind(e.a), rb=ufind(e.b);
        if (ra!=rb){ uf[ra]=rb; nedges[ne++]=e; }                                            // tree edge (connectivity)
        else if (hash01(e.a,e.b,seed+999) < 0.12f) nedges[ne++]=e;                           // a sparse loop
    }
}
// SUPERBLOCK / FUSED-GRID (§8.3, Stage 2) — the ONE pattern with no algorithm in the 2001/2008 pillars, so
// an original construction. A CONTINUOUS arterial grid (every SB-th lattice line) carries all through-traffic
// and wraps cells whose INTERIOR is vehicle-DISCONTINUOUS: interior local streets attach to the perimeter as
// a spanning forest (cul-de-sac stems, no cross-cell shortcut) + a sparse loop fraction. The defining pair —
// pedestrian-permeable / vehicle-discontinuous. Distinct from cul-de-sac: the FRAME stays a full grid (a real
// degree-4 / X-crossroads share survives) while CIRCUITY climbs (cars must detour the sealed interior). That
// circuity gap is what PROVES it's a separate pattern when degree/node-mix alone read like a grid.
static void gen_superblock(int id[GH][GW], int seed){
    for (int i=0;i<nn;i++) uf[i]=i;
    // 1) the arterial frame — every SB-th row/col, fully connected (the continuous loop network)
    for (int r=0;r<GH;r++) for (int c=0;c<GW;c++){
        if (c+1<GW && (r%SB)==0){ nedges[ne++]=(NetEdge){id[r][c],id[r][c+1],1}; uf[ufind(id[r][c])]=ufind(id[r][c+1]); }
        if (r+1<GH && (c%SB)==0){ nedges[ne++]=(NetEdge){id[r][c],id[r+1][c],1}; uf[ufind(id[r][c])]=ufind(id[r+1][c]); }
    }
    // 2) interior local streets — Kruskal over the REMAINING (non-arterial) edges: a tree edge only attaches
    //    an interior node to the frame (never closes a cross-cell through-route); a rare loop fraction adds
    //    the occasional interior link without making the interior permeable to cars.
    NetEdge cand[MAXEDGE]; float wt[MAXEDGE]; int nc=0;
    for (int r=0;r<GH;r++) for (int c=0;c<GW;c++){
        if (c+1<GW && (r%SB)!=0){ cand[nc]=(NetEdge){id[r][c],id[r][c+1]}; wt[nc]=hash01(c,r,seed);     nc++; }
        if (r+1<GH && (c%SB)!=0){ cand[nc]=(NetEdge){id[r][c],id[r+1][c]}; wt[nc]=hash01(c,r,seed+555); nc++; }
    }
    int ord[MAXEDGE]; for(int i=0;i<nc;i++) ord[i]=i;
    for(int i=0;i<nc;i++){ int m=i; for(int j=i+1;j<nc;j++) if(wt[ord[j]]<wt[ord[m]]) m=j;
        int t=ord[i]; ord[i]=ord[m]; ord[m]=t; }
    for(int i=0;i<nc;i++){ NetEdge e=cand[ord[i]]; int ra=ufind(e.a), rb=ufind(e.b);
        if (ra!=rb){ uf[ra]=rb; nedges[ne++]=e; }
        else if (hash01(e.a,e.b,seed+999) < 0.06f) nedges[ne++]=e;   // a rare interior loop (kept low ⇒ stays discontinuous)
    }
}
// SEAM (Phase 2 → docs/design/road-program-state.md): gen_network() is the PER-REGION generator a two-tier
// (major→minor, §8.4) world calls — it fills one region's local streets given a (pattern, seed). The world
// owns the major arterials + the region partition and stitches these graphs at the boundaries. Each node is
// a crossing the JUNCTION layer (M1–M3) can render in detail — the network→junction zoom.
static void gen_network(int pat,int seed){
    nn=0; ne=0;
    if (pat==PAT_RADIAL){ gen_radial(seed); return; }
    float x0=18, y0=NETTOP, x1=SCREEN_W-18, y1=SCREEN_H-TOOLBAR-10;
    float cw=(x1-x0)/(GW-1), ch=(y1-y0)/(GH-1);
    float jit = (pat==PAT_ORGANIC) ? 0.42f : (pat==PAT_CULDESAC ? 0.22f : 0.f);
    int id[GH][GW];
    for (int r=0;r<GH;r++) for (int c=0;c<GW;c++){
        float jx=(hash01(c,r,seed)    -0.5f)*cw*jit;
        float jy=(hash01(c,r,seed+97) -0.5f)*ch*jit;
        id[r][c]=nn; nnodes[nn++]=(NetNode){ x0+c*cw+jx, y0+r*ch+jy };
    }
    if (pat==PAT_CULDESAC){ gen_culdesac(id, seed); return; }
    if (pat==PAT_SUPERBLOCK){ gen_superblock(id, seed); return; }
    for (int r=0;r<GH;r++) for (int c=0;c<GW;c++){          // grid / organic: full grid edges
        if (c+1<GW) nedges[ne++]=(NetEdge){ id[r][c], id[r][c+1] };
        if (r+1<GH) nedges[ne++]=(NetEdge){ id[r][c], id[r+1][c] };
    }
}
static int   node_degree(int i){ int d=0; for(int e=0;e<ne;e++) if(nedges[e].a==i||nedges[e].b==i) d++; return d; }
static float mean_degree(void){ if(!nn)return 0; int s=0; for(int i=0;i<nn;i++) s+=node_degree(i); return (float)s/nn; }
static int   dead_ends(void){ int c=0; for(int i=0;i<nn;i++) if(node_degree(i)==1) c++; return c; }
// DEGREE DISTRIBUTION — §8.2's real discriminator: "mean degree ALONE is not enough" (a sparse grid and a
// dense dendritic tree can read the same mean). The SHARE of nodes by degree separates them — cul-de-sac(1),
// T/Y(3), X-crossroads(4+) in Marshall's node taxonomy. PURE over the graph; the network-view readout + spec.
static int   deg_count(int d){    int c=0; for(int i=0;i<nn;i++) if(node_degree(i)==d) c++; return c; }
static int   deg_count_ge(int d){ int c=0; for(int i=0;i<nn;i++) if(node_degree(i)>=d) c++; return c; }
static int   deg_pct(int c){ return nn ? (100*c + nn/2)/nn : 0; }   // rounded share of all nodes (%)

// CIRCUITY (§8.2 #3) — network shortest-path distance ÷ straight-line distance, averaged over every
// connected node pair. 1.0 = every trip a straight shot; higher = more detour. The discriminator the
// SUPERBLOCK needs: a fused grid can read the same degree/node-mix as a plain grid, but its vehicle-
// discontinuous interior forces cars around ⇒ circuity climbs. PURE. Floyd–Warshall over nn≤MAXNODE
// nodes (edge weight = Euclidean length) — O(n^3), trivial at sandbox scale (won't survive a real world;
// that's a Stage-3 concern). See docs/design/road-program-state.md.
static float mean_circuity(void){
    if (nn<2) return 1;
    static float D[MAXNODE][MAXNODE];
    const float INF=1e9f;
    for (int i=0;i<nn;i++) for(int j=0;j<nn;j++) D[i][j]=(i==j)?0.f:INF;
    for (int e=0;e<ne;e++){
        int a=nedges[e].a, b=nedges[e].b;
        float w=sqrtf((nnodes[b].x-nnodes[a].x)*(nnodes[b].x-nnodes[a].x)+(nnodes[b].y-nnodes[a].y)*(nnodes[b].y-nnodes[a].y));
        if (w<D[a][b]){ D[a][b]=w; D[b][a]=w; }
    }
    for (int k=0;k<nn;k++) for(int i=0;i<nn;i++){ if(D[i][k]>=INF) continue;
        for(int j=0;j<nn;j++){ float v=D[i][k]+D[k][j]; if(v<D[i][j]) D[i][j]=v; } }
    float tot=0; int cnt=0;
    for (int i=0;i<nn;i++) for(int j=i+1;j<nn;j++){
        if (D[i][j]>=INF) continue;
        float e=sqrtf((nnodes[j].x-nnodes[i].x)*(nnodes[j].x-nnodes[i].x)+(nnodes[j].y-nnodes[i].y)*(nnodes[j].y-nnodes[i].y));
        if (e<0.5f) continue;
        tot += D[i][j]/e; cnt++;
    }
    return cnt ? tot/cnt : 1;
}
// is the network one connected component? (re-inits uf — call AFTER generation). spec/diagnostic helper.
static int net_connected(void){
    if (!nn) return 1;
    for (int i=0;i<nn;i++) uf[i]=i;
    for (int e=0;e<ne;e++){ int ra=ufind(nedges[e].a), rb=ufind(nedges[e].b); if(ra!=rb) uf[ra]=rb; }
    int root=ufind(0); for(int i=1;i<nn;i++) if(ufind(i)!=root) return 0;
    return 1;
}

// draw one street segment as a road quad (perpendicular extrusion of the centreline)
static void road_segment(float ax,float ay,float bx,float by,float hw,int col){
    float dx=bx-ax, dy=by-ay, L=sqrtf(dx*dx+dy*dy); if(L<0.5f) return; dx/=L; dy/=L;
    float nx=-dy*hw, ny=dx*hw;
    int q[8]={ (int)(ax+nx),(int)(ay+ny),(int)(ax-nx),(int)(ay-ny),
               (int)(bx-nx),(int)(by-ny),(int)(bx+nx),(int)(by+ny) };
    polyfill(q,4,col);
}

// ── M4c: the curvature knob (§8.5 #4 — warped vs straight). Each EDGE bows into a quadratic curve; the
//    graph/topology is untouched (degree, dead-ends unchanged) but the drawn roads WIND — and sinuosity
//    (edge arc-length ÷ chord) becomes a live SNDi measure (it's pinned at 1 by straight chords otherwise).
//    Stage-2 refinement: an ARTERIAL edge never bows (the superblock's frame stays straight; only its calmed
//    interior local streets wind) — the real fused-grid look: engineered through-routes straight, residential
//    interior curved. Other patterns mark no edge arterial, so they bow uniformly as before. ──
static float edge_bow(int i){            // signed ⊥ bow (px) for edge i — seeded per edge, scaled by the knob
    if (curveAmt<=0) return 0;
    if (nedges[i].arterial) return 0;    // engineered through-routes stay STRAIGHT — only the calmed interior winds
                                         // (the superblock marks its frame arterial; other patterns mark nothing ⇒ all bow)
    NetNode a=nnodes[nedges[i].a], b=nnodes[nedges[i].b];
    float L=sqrtf((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y));
    return (hash01(nedges[i].a, nedges[i].b, netseed+313)-0.5f) * 2.f * (curveAmt/3.f) * 0.40f * L;
}
// sample edge i's curve into `out` (pairs); returns point count. bow=0 ⇒ the straight chord (2 points).
static int edge_curve(int i,float *out){
    NetNode a=nnodes[nedges[i].a], b=nnodes[nedges[i].b];
    float bow=edge_bow(i);
    if (bow>-0.5f && bow<0.5f){ out[0]=a.x;out[1]=a.y;out[2]=b.x;out[3]=b.y; return 2; }
    float dx=b.x-a.x, dy=b.y-a.y, L=sqrtf(dx*dx+dy*dy);
    float nx=-dy/L, ny=dx/L;
    float cx=(a.x+b.x)*0.5f+nx*bow, cy=(a.y+b.y)*0.5f+ny*bow;     // quadratic control = bowed midpoint
    int k=0; for (int s=0;s<=6;s++){ float t=s/6.f, it=1-t;
        out[k++]=it*it*a.x + 2*it*t*cx + t*t*b.x;
        out[k++]=it*it*a.y + 2*it*t*cy + t*t*b.y; }
    return 7;
}
static void draw_edge(int i,float hw,int col){
    float p[16]; int m=edge_curve(i,p);
    for (int s=0;s<m-1;s++) road_segment(p[s*2],p[s*2+1],p[s*2+2],p[s*2+3],hw,col);
}
static float mean_sinuosity(void){       // arc-length ÷ chord, averaged over edges (1.0 = straight)
    if (!ne) return 1;
    float tot=0; int cnt=0; float p[16];
    for (int i=0;i<ne;i++){
        NetNode a=nnodes[nedges[i].a], b=nnodes[nedges[i].b];
        float chord=sqrtf((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y)); if(chord<0.5f) continue;
        int m=edge_curve(i,p); float arc=0;
        for (int s=0;s<m-1;s++) arc+=sqrtf((p[s*2+2]-p[s*2])*(p[s*2+2]-p[s*2])+(p[s*2+3]-p[s*2+1])*(p[s*2+3]-p[s*2+1]));
        tot+=arc/chord; cnt++;
    }
    return cnt ? tot/cnt : 1;
}

static void draw_network_view(void){
    gen_network(pattern, netseed);
    for (int i=0;i<ne;i++) draw_edge(i, 2.5f, CLR_BROWNISH_BLACK);    // casing
    for (int i=0;i<ne;i++) draw_edge(i, 1.5f, CLR_DARK_GREY);         // asphalt
    for (int i=0;i<nn;i++) circfill((int)nnodes[i].x,(int)nnodes[i].y,1,CLR_DARK_GREY);

    font(FONT_SMALL);
    char bb[80];
    print("streetlab - the street WEB (M4: network topology)", 4,5, CLR_WHITE);
    snprintf(bb,sizeof bb,"%s  -  %d nodes  -  deg %.1f  -  %d dead-ends  -  sinuosity %.2f",
             PAT_NAME[pattern], nn, mean_degree(), dead_ends(), mean_sinuosity());
    print(bb, 4,13, CLR_ORANGE);
    // §8.2 degree distribution — the SNDi discriminator mean degree hides (Marshall node taxonomy)
    snprintf(bb,sizeof bb,"node mix:  cul(1) %d%%  T(3) %d%%  X(4+) %d%%    circuity %.2f",
             deg_pct(deg_count(1)), deg_pct(deg_count(3)), deg_pct(deg_count_ge(4)), mean_circuity());
    print(bb, 4,21, CLR_LIGHT_GREY);

    rectfill(0, SCREEN_H-TOOLBAR, SCREEN_W, TOOLBAR, CLR_BLACK);
    int d;
    print("seed", 4, SCREEN_H-37, CLR_WHITE);
    d=step_btn(36, SCREEN_H-40, 14, "-", "+"); if (d){ netseed+=d; if(netseed<0)netseed=0; }
    snprintf(bb,sizeof bb,"%d",netseed); print(bb, 70, SCREEN_H-37, CLR_LIGHT_GREY);
    if (ui_button(92, SCREEN_H-40, 90, 13, PAT_NAME[pattern])) pattern=(pattern+1)%NPAT;
    print("curve", 192, SCREEN_H-37, CLR_BLUE);                       // M4c: winding knob
    d=step_btn(228, SCREEN_H-40, 14, "-", "+"); if (d){ curveAmt+=d; if(curveAmt<0)curveAmt=0; if(curveAmt>3)curveAmt=3; }
    snprintf(bb,sizeof bb,"%d",curveAmt); print(bb, 262, SCREEN_H-37, CLR_LIGHT_GREY);
    if (ui_button(220, SCREEN_H-22, 96, 13, "view: junction")) netview=0;
    font(FONT_NORMAL);
}

// MIRROR-BLIT — the corner-symmetry fix (docs/design/streetlab-corner-symmetry-plan.md, Option A).
// Reflect the rendered junction-core PIXELS so the four kerbs are mirror-identical by construction —
// this sidesteps the fill(point-mirror)/stroke(cell-mirror) snap conflict that defeats a symmetric
// line (see the ri() SEAM). Reads the canonical (top-left) quadrant via pget (LAST frame: steady-
// state exact; one transient frame after a parameter change) and writes the other three mirrored.
// Called AFTER corners, BEFORE markings — and the directional markings (arrows/dashes), drawn after,
// over-stamp anything this wrongly mirrors, so only the symmetric kerb/asphalt core is reflected.
static void mirror_blit(int cxr,int cyr,int rad){
    int mx=2*cxr-1, my=2*cyr-1;                          // even-grid pixel mirror: x->mx-x, y->my-y
    for (int y=cyr-rad; y<cyr; y++)
        for (int x=cxr-rad; x<cxr; x++){
            int p=pget(x,y), rx=mx-x, ry=my-y;
            pset(rx,y ,p);                               // → top-right    (mirror x)
            pset(x ,ry,p);                               // → bottom-left  (mirror y)
            pset(rx,ry,p);                               // → bottom-right (mirror both)
        }
}

// ── FIELD-BASED KERB (docs/design/field-based-road-rendering.md) — behind DE_FIELD_ROADS, OFF by
//    default (the shipped path stays mirror_blit + casing-fillet). The kerb becomes the COVERAGE
//    OUTLINE of the asphalt region — an asphalt pixel with a non-asphalt 4-neighbour — derived from a
//    geometric is_asphalt() predicate (arm-ray union ∪ curb-return fillets). Uniform 1px at ANY angle
//    and symmetric BY CONSTRUCTION, so it replaces the per-arm casing pass + stroke_corner +
//    casing-fillet + mirror_blit at once. The asphalt SURFACE is still laid by polyfill (arm bands +
//    fill_corner) — only the thin outline is scanned per-pixel, so it's the cheap "math, no fill"
//    cost (~0.7ms), not skewlab's full per-pixel fill. Markings stay a separate longitudinal layer. ──
// Toggled LIVE by the 'g' key (field_roads) so you can A/B against the old per-arm path in one window;
// default OFF (shipping path stays mirror_blit) — built -DDE_FIELD_ROADS just flips the DEFAULT on (for
// the headless gates). The helpers always compile; only the runtime bool decides which path draws.
static float fr_cx, fr_cy, fr_HW2, fr_rad2, fr_disc2;     // fr_disc2 = circulatory-disc radius² (roundabout; 0 = none)
static float fr_dx[NLEG], fr_dy[NLEG]; static int fr_n;
enum { FR_NP = 12 };                                          // fillet polygon verts (apex K + 11 arc samples)
static float fr_fil[NLEG][2*FR_NP]; static int fr_nfil;       // cached fillet polygons (FLOAT — snap-free ⇒ symmetric)

static int fr_pip(const float *xy, int n, float px, float py){   // even-odd point-in-polygon (float)
    int c=0; for (int i=0,j=n-1;i<n;j=i++){
        float yi=xy[2*i+1], yj=xy[2*j+1];
        if ((yi>py)!=(yj>py)){
            float xc=xy[2*i]+(xy[2*j]-xy[2*i])*(py-yi)/(yj-yi);
            if (px<xc) c^=1;
        }
    } return c;
}
// the arm-capsule union: rays from the hub (opposite arm covers behind). Squared distance, no sqrt.
static int fr_arm(float px, float py){
    for (int i=0;i<fr_n;i++){
        float t=(px-fr_cx)*fr_dx[i]+(py-fr_cy)*fr_dy[i]; if(t<0)t=0;
        float ex=px-(fr_cx+fr_dx[i]*t), ey=py-(fr_cy+fr_dy[i]*t);
        if (ex*ex+ey*ey <= fr_HW2) return 1;
    }
    return 0;
}
// inside a curb-return fillet? (only tested near the hub, where the fillets live)
static int fr_fillet(float px, float py){
    float hx=px-fr_cx, hy=py-fr_cy;
    if (hx*hx+hy*hy > fr_rad2) return 0;
    for (int f=0;f<fr_nfil;f++) if (fr_pip(fr_fil[f], FR_NP, px, py)) return 1;
    return 0;
}
// asphalt = arm-ray union ∪ curb-return fillets ∪ (roundabout) circulatory disc.
static int fr_road(float px,float py){
    if (fr_disc2>0){ float hx=px-fr_cx, hy=py-fr_cy; if (hx*hx+hy*hy <= fr_disc2) return 1; }   // circulatory disc
    return fr_arm(px,py) || fr_fillet(px,py);
}
// paint a boundary pixel during the proud-kerb dilation: a PINCH (a lone non-road pixel enclosed by road
// on all 4 sides — the sub-pixel grass cusp where a skewed arm meets the circulatory disc) becomes asphalt,
// so it can't survive as an interior dark "stray"; every other boundary pixel is the 1px proud kerb.
static void fr_put(int x,int y){
    float px=x+0.5f, py=y+0.5f;
    int L=fr_road(px-1,py), R=fr_road(px+1,py), U=fr_road(px,py-1), D=fr_road(px,py+1);
    if ((L&&R)||(U&&D)) pset(x,y,CLR_DARK_GREY);   // 1px-wide pinch (road on opposite sides) → fill it; no interior kerb
    else pset(x,y,CLR_BROWNISH_BLACK);             // ordinary boundary → 1px proud kerb
}
// FIELD RENDER — the SINGLE SOURCE OF TRUTH for the road surface AND its kerb, both from one snap-free
// float predicate (arm union ∪ curb-return fillets). Fills the asphalt itself (so it can't disagree with
// the kerb — the per-arm polyfill is skipped on this path) and draws the kerb PROUD (1px OUTSIDE the
// asphalt, like the old casing convention ⇒ A/B aligns) by dilating: for each road pixel, paint any
// non-road 4-neighbour as kerb. Uniform 1px at any angle, symmetric by construction (float, no ri snap).
static void fr_render(float cx,float cy,float HW,const float*brg,int n,float disc){
    fr_cx=cx; fr_cy=cy; fr_HW2=HW*HW; fr_n=n; fr_disc2=disc*disc;
    for (int i=0;i<n;i++){ fr_dx[i]=ux(brg[i]); fr_dy[i]=uy(brg[i]); }
    fr_nfil=0;
    for (int i=0; i<n && disc<=0; i++){                       // ROUNDABOUT: the disc replaces the corner fillets — skip them
        float bA=brg[i], bB=brg[(i+1)%n], gap=fmodf(bB-bA+3600,360);
        if (gap<=0.5f || gap>=179.5f) continue;
        float R = (freeRight && fr_fits(gap)) ? frR : cornerR;   // free-right: a GENEROUS rounded corner (the right-turn slip)
        float bm=bA+gap*0.5f, kx,ky; edge_corner(cx,cy,HW, bA,bB,bm, &kx,&ky);
        CurbReturn c=curb_return(kx,ky, bA,bB, R);
        float a0=atan2f(c.t1y-c.oy,c.t1x-c.ox), a1=atan2f(c.t2y-c.oy,c.t2x-c.ox);
        float d=a1-a0; while(d>M_PI)d-=2*M_PI; while(d<-M_PI)d+=2*M_PI;
        float *xy=fr_fil[fr_nfil]; int k=0;
        xy[k++]=kx; xy[k++]=ky;
        for (int s=0;s<=10;s++){ float a=a0+d*s/10; xy[k++]=c.ox+cosf(a)*R; xy[k++]=c.oy+sinf(a)*R; }
        fr_nfil++;
    }
    // pip cutoff = the ACTUAL fillet extent (acute skew corners reach (HW+R)/tan(half) from the hub —
    // far past a fixed radius, which clipped them ⇒ sharp corners). Take the furthest fillet vertex.
    fr_rad2 = (HW+cornerR)*(HW+cornerR);
    for (int f=0;f<fr_nfil;f++) for (int v=0;v<FR_NP;v++){
        float vx=fr_fil[f][2*v]-cx, vy=fr_fil[f][2*v+1]-cy, d2=vx*vx+vy*vy;
        if (d2>fr_rad2) fr_rad2=d2;
    }
    fr_rad2 += 4;                                             // small margin
    int y0=0, y1=SCREEN_H-TOOLBAR;                            // fill from the top edge (the N arm runs up behind the title, like the per-arm bands) down to the toolbar
    for (int y=y0;y<y1;y++) for (int x=0;x<SCREEN_W;x++){
        float px=x+0.5f, py=y+0.5f;
        if (!fr_road(px,py)) continue;
        pset(x,y,CLR_DARK_GREY);                              // asphalt (the field fills it ⇒ kerb can't disagree)
        // proud kerb: paint the non-road neighbours — but NOT across a play-area border (a road running
        // off-screen there is a legit exit, not an edge to kerb). pset clips, but the border guard keeps
        // the mouth open so the arm meets the screen edge cleanly.
        if (x>0          && !fr_road(px-1,py)) fr_put(x-1,y);
        if (x<SCREEN_W-1 && !fr_road(px+1,py)) fr_put(x+1,y);
        if (y>y0         && !fr_road(px,py-1)) fr_put(x,y-1);
        if (y<y1-1       && !fr_road(px,py+1)) fr_put(x,y+1);
    }
}

void draw(void){
    static bool pget_on=false; if(!pget_on){ enable_pget(true); pget_on=true; }   // for mirror_blit read-back
    ui_begin();
    cls(CLR_DARK_GREEN);
    if (netview){ draw_network_view(); ui_end(); return; }     // M4: the street-web view
    float cx=SCREEN_W/2.f, cy=(SCREEN_H-TOOLBAR)/2.f;
    float HW=cross_hw();                                   // M7: half-width = the sum of the typed lanes present
    float REACH=SCREEN_W+SCREEN_H;                         // run each arm well past the screen edge
    int idx[NLEG]; float brg[NLEG]; int n=present_legs(idx,brg);
    int field_now = field_roads;   // all junction modes on the field (network view is the early-return above)

    // M5: SIDEWALK footprint = the whole junction inflated by SW, drawn FIRST (under the road). The asphalt
    // then covers the inner part, leaving an SW-wide sidewalk ring around everything — wrapping the corners
    // for free (same fillet geometry, wider). Sidewalks line the full length of each arm.
    if (peds){
        for (int i=0;i<n;i++){ float b=brg[i],dx=ux(b),dy=uy(b),nx=ux(b+90),ny=uy(b+90); float w=HW+SW;
            float ox=cx+dx*REACH, oy=cy+dy*REACH;
            int q[8]={(int)(cx+nx*w),(int)(cy+ny*w),(int)(cx-nx*w),(int)(cy-ny*w),
                      (int)(ox-nx*w),(int)(oy-ny*w),(int)(ox+nx*w),(int)(oy+ny*w)};
            polyfill(q,4,CLR_LIGHT_GREY); }
        for (int i=0;i<n;i++){ float bA=brg[i], bB=brg[(i+1)%n]; float gap=fmodf(bB-bA+3600,360);
            if (gap<=0.5f || gap>=179.5f) continue;
            float bm=bA+gap*0.5f, kx,ky; edge_corner(cx,cy,HW+SW, bA,bB,bm, &kx,&ky);
            CurbReturn c=curb_return(kx,ky, bA, bB, cornerR); fill_corner(kx,ky, c, cornerR, CLR_LIGHT_GREY); }
    }

    // asphalt = the union of arm bands (collinear arm pairs ⇒ a continuous strip ⇒ the centre is always
    // covered, no gap). casing pass (1px proud) first, asphalt pass second, so a kerb edge shows at the
    // outer band edges while the central overlap stays clean asphalt.
    for (int pass = field_now?2:0; pass<2; pass++){   // field: skip the per-arm bands entirely — fr_render fills asphalt + kerb
        float w = HW + (pass?0:1);
        int col = pass ? CLR_DARK_GREY : CLR_BROWNISH_BLACK;
        for (int i=0;i<n;i++){
            float b=brg[i], dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90);
            float ox=cx+dx*REACH, oy=cy+dy*REACH;
            int q[8]={(int)(cx+nx*w),(int)(cy+ny*w),(int)(cx-nx*w),(int)(cy-ny*w),
                      (int)(ox-nx*w),(int)(oy-ny*w),(int)(ox+nx*w),(int)(oy+ny*w)};
            polyfill(q,4,col);
        }
    }

  if (roundabout){
    // ── M6: lay a clean CIRCULATORY disc over the messy central overlap (it connects every arm), then the
    //    traversable island. The disc's outer kerb is a full ring; we re-lay each arm's asphalt over it to
    //    REOPEN the mouths, so the kerb survives only on the grass wedges between arms. ──
    float ICR = round_icr();
    if (peds) circfill((int)cx,(int)cy,(int)(ICR+SW),CLR_LIGHT_GREY);   // sidewalk ring around the circulatory
    if (field_now){
        // FIELD: the circulatory disc is just another term in the road predicate (disc ∪ arms) ⇒ the kerb
        // (proud outline) traces the ring + arm mouths uniformly, no circfill/reopen dance.
        fr_render(cx,cy,HW,brg,n, ICR);
    } else {
        circfill((int)cx,(int)cy,(int)ICR+1,CLR_BROWNISH_BLACK);        // circulatory outer kerb (trimmed next)
        circfill((int)cx,(int)cy,(int)ICR,  CLR_DARK_GREY);            // circulatory asphalt
        for (int i=0;i<n;i++){                                          // reopen each arm mouth through the kerb
            float b=brg[i], dx=ux(b),dy=uy(b), nx=ux(b+90),ny=uy(b+90);
            float ox=cx+dx*REACH, oy=cy+dy*REACH;
            int q[8]={(int)(cx+nx*HW),(int)(cy+ny*HW),(int)(cx-nx*HW),(int)(cy-ny*HW),
                      (int)(ox-nx*HW),(int)(oy-ny*HW),(int)(ox+nx*HW),(int)(oy+ny*HW)};
            polyfill(q,4,CLR_DARK_GREY);
        }
    }
    // island + bike ring + splitters are SURFACES; arrows/give-way/crosswalk are PAINT. (Bare = disc+arms.)
    if (surfaces && bikeOn){                                           // the bike lane CIRCULATES — a terracotta ring
        circfill((int)cx,(int)cy,(int)ICR,        CLR_BROWN);          // just inside the kerb (the outer circulatory),
        circfill((int)cx,(int)cy,(int)(ICR-BIKEW),CLR_DARK_GREY);      // carved back to a BIKEW-wide ring; it meets the
        circ((int)cx,(int)cy,(int)(ICR-BIKEW),CLR_WHITE);             // approach bike lanes at each (reopened) mouth
    }
    if (surfaces) draw_island(cx,cy);
    if (paint)    draw_circulation(cx,cy);
    for (int i=0;i<n;i++){                                              // per approach: cross-section, splitter, give-way, crossing
        float b=brg[i];
        // the approach lanes are laterally offset, so a straight bike lane's nearest point to the hub is
        // sqrt(ICR^2+HW^2) > ICR — it never reaches the ring, leaving a grey wedge. Start it at sqrt(ICR^2-HW^2),
        // the axial distance where its kerb-side edge actually crosses the ring radius ⇒ it FLARES in and meets it.
        float din = round_flare(ICR, HW);
        cross_markings(cx,cy,b, din, din, ICR+3, REACH);              // self-splits: bike SURFACE always, lane lines = paint
        if (surfaces) draw_splitter(cx,cy,b, ICR, 16);                 // teardrop splitter (deflect + ped refuge)
        if (paint)    give_way(cx,cy,b, ICR+1.5f, HW);                 // yield line at the circulatory edge
        if (paint && peds) draw_crosswalk(cx,cy,b,HW, ICR+3);          // crossing set back behind the give-way
    }
  } else {
    // curb returns: one per CONVEX gap between adjacent arms (skip the 180° straight back of a T)
    for (int i=0;i<n;i++){
        float bA=brg[i], bB=brg[(i+1)%n];
        float gap=fmodf(bB-bA+3600,360);
        if (gap<=0.5f || gap>=179.5f) continue;
        float bm=bA+gap*0.5f, kx,ky;
        // Stage-1 #2: slip + pork-chop island — only where it FITS (deep enough corner: acute / generous radius).
        // Shallow obtuse corners have no room and fall through to a plain curb return.
        if (freeRight && fr_fits(gap)){
            // FIELD: fr_render lays the GENEROUS rounded corner (radius frR = the turn) as asphalt + kerb;
            // the pork-chop island overlays in the markings section (draw_freeright_island). Old path below.
            if (!field_now){
                if (bikeOn){
                    // the CYCLE TRACK wraps the kerb corner as its own continuous path, OUTSIDE the slip: pave the
                    // corner, lay the slip inboard, then wrap the bike band round it (the slip nests within).
                    edge_corner(cx,cy,HW, bA,bB,bm, &kx,&ky);
                    CurbReturn cw=curb_return(kx,ky, bA,bB, cornerR);
                    fill_corner(kx,ky, cw, cornerR, CLR_DARK_GREY);
                    draw_freeright(cx,cy, bA,bB, bm);
                    corner_bike(cw, cornerR);
                    stroke_corner(cw, cornerR, CLR_BROWNISH_BLACK);
                } else {
                    draw_freeright(cx,cy, bA,bB, bm);
                }
            }
            continue;
        }
        edge_corner(cx,cy,HW, bA,bB,bm, &kx,&ky);
        CurbReturn c=curb_return(kx,ky, bA, bB, cornerR);
        if (field_now){
            // FIELD: fr_render() (after the loop) fills the rounded-corner asphalt AND the kerb from one
            // float predicate — uniform 1px at any angle ⇒ no fill_corner kerb, no casing fillet, no
            // stroke_corner, no mirror_blit. (kx,ky/c computed above are unused here; bike wrap is step 4.)
            (void)c;
        } else {
        // KERB EDGE. Orthogonal (~90°) corners suffered a ~2px dark notch: the straight arms' 1px-proud
        // BROWNISH_BLACK casing (the asphalt pass insets it) pokes past an HW-tangent kerb. Fix = round the
        // casing too — a fillet at HW+1 UNDER the HW asphalt ⇒ a 1px casing ring that meets the straight
        // casing. BUT that ring's width is ~1/sin(half-angle), so on SKEW corners it fattens to 2–3px — so
        // only orthogonal gets the casing fillet; skew keeps the uniform-1px stroke (it never had the notch).
        // The casing fillet uses the HW+1 corner (centre O′); corner_bike and skew strokes use the HW
        // corner (centre O). So restrict it to the case it was made for — orthogonal, NON-bike corners:
        //   • skew → the ring fattens ~1/sin(half-angle); keep the uniform-1px stroke.
        //   • bike → corner_bike hugs the O arc, but the casing kerb sits on the O′ arc 1px out, leaving a
        //     grey band between bike and kerb; keep the stroke (the terracotta band defines that corner).
        int casing = (fabsf(gap-90.f) < 0.5f) && !bikeOn;
        if (casing){                                       // casing fillet first (outermost dark)
            float kxc,kyc; edge_corner(cx,cy,HW+1, bA,bB,bm, &kxc,&kyc);
            fill_corner(kxc,kyc, curb_return(kxc,kyc, bA,bB, cornerR), cornerR, CLR_BROWNISH_BLACK);
        }
        fill_corner(kx,ky, c, cornerR, CLR_DARK_GREY);     // asphalt insets ⇒ leaves the 1px casing kerb
        if (bikeOn) corner_bike(c, cornerR);               // #5a: the bike lane wraps the corner (just inside the kerb)
        if (!casing) stroke_corner(c, cornerR, CLR_BROWNISH_BLACK);   // skew/bike: uniform 1px kerb edge
        }
    }

    if (field_now){
        fr_render(cx,cy,HW,brg,n,0);          // field: fills rounded-corner asphalt + kerb (replaces fill_corner kerb + mirror_blit)
    } else if (!isT && skew==0 && !freeRight){
        // MIRROR-BLIT the symmetric orthogonal 4-way: reflect the junction-core pixels so the four kerbs
        // are pixel-identical (the kerb floor → 0 by construction; a symmetric line couldn't — snap
        // conflict, see the ri() SEAM). Skew/T/free-right keep the per-corner path (no symmetry there).
        int rad=(int)(HW+cornerR+(peds?SW:0))+2;
        mirror_blit(ri(cx),ri(cy),rad);
    }

    // FIELD free-right: the pork-chop ISLAND is a surface (the give-way dashes inside it are paint — gated there).
    if (surfaces && field_now && freeRight) for (int i=0;i<n;i++){
        float bA=brg[i], bB=brg[(i+1)%n], gap=fmodf(bB-bA+3600,360);
        if (gap<=0.5f || gap>=179.5f || !fr_fits(gap)) continue;
        draw_freeright_island(cx,cy,HW, bA,bB, bA+gap*0.5f);
    }
    // FIELD bike: wrap the protected cycle track around each plain corner so it's CONTINUOUS with the arm
    // lanes (the band sits just inside the field kerb). Skip free-right corners — they have their own treatment.
    if (surfaces && field_now && bikeOn && !roundabout) for (int i=0;i<n;i++){
        float bA=brg[i], bB=brg[(i+1)%n], gap=fmodf(bB-bA+3600,360);
        if (gap<=0.5f || gap>=179.5f) continue;
        if (freeRight && fr_fits(gap)) continue;
        float bm=bA+gap*0.5f, kx,ky; edge_corner(cx,cy,HW, bA,bB,bm, &kx,&ky);
        corner_bike(curb_return(kx,ky, bA,bB, cornerR), cornerR);
    }

    // markings, median islands + stop bar per arm. startd clears the arm's corners (acute corners reach
    // further down the arm) — and, with turn lanes, the median nose too (no centreline over the island).
    for (int i=0;i<n;i++){                                   // always runs: cross_markings self-splits surface/paint
        float b=brg[i], df=arm_face(brg,n,i,HW);
        // the cleared THROAT of the arm is at the curb-return TANGENT, ~cornerR beyond the bare corner K
        // (arm_face). Every mouth marking — stop bar, crosswalk, median, turn bay, arrow — keys off this
        // `mouth` datum, NOT df, so they sit at the rounded mouth instead of poking back into the fillet.
        float mouth = df + cornerR;
        // PER SIDE, at the curb-return TANGENT: +90 faces the next gap, -90 the prev. A FREE-RIGHT corner uses
        // the bigger frR radius, so the kerb-side lanes (bike/parking) must start at the frR tangent there —
        // else their white edge line pokes into the slip curve (the loose-pixels artifact).
        float gP = fmodf(brg[(i+1)%n]-brg[i]+3600,360), gM = fmodf(brg[i]-brg[(i-1+n)%n]+3600,360);
        float RP = (freeRight && fr_fits(gP)) ? frR : cornerR, RM = (freeRight && fr_fits(gM)) ? frR : cornerR;
        float kstdP = kerb_start(brg,n,i,HW,RP,+1);
        float kstdM = kerb_start(brg,n,i,HW,RM,-1);
        float cstd = (turnLanes && mouth+POCKET+PTAPER+3 > mouth+3) ? mouth+POCKET+PTAPER+3 : mouth+3;  // centre: past the turn bay
        cross_markings(cx,cy,b,kstdP,kstdM,cstd,REACH);
        float dx=ux(b),dy=uy(b), ix=ux(b-90),iy=uy(b-90);
        if (turnLanes){
            // the channelizing splitter is a raised island (SURFACE); the bay lines + arrows are PAINT.
            if (surfaces && !medOn && !twltlOn) draw_median(cx,cy,b,mouth);   // splitter only when the centre is bare
            if (paint){
            // #1: delineate the inner DRIVING lane (next to the median/centre) as the left-turn lane — a SOLID
            // white line at its outer edge, where the dashed dividers leave off. Median-aware via drive_inner().
            if (lanesPer>=2){
                float o = drive_inner()+LANEW;
                line((int)(cx+dx*mouth+ix*o),(int)(cy+dy*mouth+iy*o),
                     (int)(cx+dx*(mouth+POCKET)+ix*o),(int)(cy+dy*(mouth+POCKET)+iy*o),CLR_WHITE);
            }
            turn_arrow(cx,cy,b,mouth,CLR_WHITE);
            // step 3b: the RIGHT-TURN POCKET (kerb side) — the mirror of the left bay. Peel the OUTER driving lane
            // as right-turn-only: a solid white line at its INNER edge (drive_outer()-LANEW) + a right-hooking arrow.
            // Off drive_outer() ⇒ median/bike/parking/TWLTL-aware. (The signalised cousin of the free-right slip,
            // which is a mutually-exclusive treatment, so the two never collide.)
            if (lanesPer>=2){
                float ro = drive_outer()-LANEW;
                line((int)(cx+dx*mouth+ix*ro),(int)(cy+dy*mouth+iy*ro),
                     (int)(cx+dx*(mouth+POCKET)+ix*ro),(int)(cy+dy*(mouth+POCKET)+iy*ro),CLR_WHITE);
                turn_arrow_at(cx,cy,b,mouth, drive_outer()-LANEW*0.5f, -1, CLR_WHITE);
            }
            }
        }
        // Stage-1 #1: bulb-out — a kerb extension (SURFACE) into the parking clear-zone. Only with no kerb-side bike lane.
        if (surfaces && peds && parkOn && !bikeOn) draw_bulb(cx,cy,b,kstdP,kstdM);
        if (paint){
        // M5: zebra crosswalk at the mouth (PAINT). With bulbs it shortens to the driving lanes.
        if (peds) draw_crosswalk(cx,cy,b, parkOn?drive_outer():HW, mouth);
        // #3: stop bar across the inbound DRIVING lanes only (drive-on-right: inbound = the b-90 side) — spans
        // [drive_inner, drive_outer], so it clears the median and stops short of the bike/parking lanes.
        float sb = peds ? mouth+1+CWDEP+1 : mouth+1;
        float mx=cx+dx*sb, my=cy+dy*sb, si=drive_inner(), so=drive_outer();
        line((int)(mx+ix*si),(int)(my+iy*si),(int)(mx+ix*so),(int)(my+iy*so),CLR_WHITE);
        if (bikeOn>=2 && !isT) bike_thru(cx,cy,b,mouth);   // #5b (opt-in): elephant's feet — paint
        }
    }
  }

    // ── Stage-1 #3c: DRIVEWAYS — mid-block low-volume access, per-side bitmask (1=+90, 2=-90). Drawn for every
    //    present arm AFTER the junction so they compose with any treatment; set back past the corner clearance. ──
    if (surfaces && driveways) for (int i=0;i<n;i++){       // driveway apron + kerb wings = SURFACE (a physical kerb-cut)
        float b=brg[i];
        for (int s=-1;s<=1;s+=2){
            if (!(driveways & (s>0?1:2))) continue;
            draw_driveway(cx,cy,b, 46, s);
            draw_driveway(cx,cy,b, 72, s);
        }
    }

    // ── HUD ──
    font(FONT_SMALL);
    char bb[96];                                            // wide enough for the feature subtitle + driveways suffix
    int xs = medOn||bikeOn||parkOn||twltlOn;               // M7: any typed cross-section element active?
    print(osmMode   ? "streetlab - OSM node -> junction (rung 1: foreign bearings)"
         : xs        ? "streetlab - at-grade junction (M7: typed cross-section)"
         : roundabout ? "streetlab - at-grade junction (M6: mini-roundabout)"
                      : "streetlab - at-grade junction (M5: sidewalks + crosswalks)", 4,5, CLR_WHITE);
    if (osmMode){   // RUNG 1 banner: which sample + the raw incident-way bearings driving the render
        char ob[96]; int p=snprintf(ob,sizeof ob,"[o] %s  -  %d arms @ ", osm_samples[osmMode-1].name, n);
        for (int i=0;i<n && p<(int)sizeof ob-8;i++) p+=snprintf(ob+p,sizeof ob-p,"%s%.0f", i?",":"", (double)brg[i]);
        snprintf(ob+p,sizeof ob-p,"%s"," deg");   // ascii (the 8x8 font has no degree glyph)
        print(ob, 4, 13, CLR_YELLOW);   // sits where the at-grade subtitle would be (suppressed below in OSM mode)
    }
    const char *dwy = (driveways && !peds) ? "  -  driveways" : "";  // omit when peds (the subtitle would overrun); always visible in the render anyway
    if (roundabout)   // RUNG 1 note: in OSM mode bb is built but not printed (the yellow banner replaces this subtitle)
        snprintf(bb,sizeof bb,"mini-roundabout (traversable island)  -  %d arms  -  give-way entries%s%s",
                 n, peds?"  -  pavement + crossings":"", dwy);
    else
        snprintf(bb,sizeof bb,"%s  -  %d corners%s%s%s", isT?"T-junction":"4-way crossing", count_corners(),
                 turnLanes?"  -  turn bays":freeRight?"  -  free-right slips":"", peds?"  -  pavement + crossings":"", dwy);
    if (!osmMode) print(bb, 4,13, CLR_ORANGE);   // RUNG 1: suppressed in OSM mode (the yellow banner takes this row)
    // A/B mode tag — tells the truth: FIELD green when the field is ACTUALLY drawing this mode; ORANGE
    // "old*" when the toggle is on but the mode (roundabout/free-right) still falls back to the per-arm path.
    print(!field_roads?"[g]arm" : field_now?"[g]FIELD":"[g]old*", SCREEN_W-32, 13,
          !field_roads?CLR_DARK_GREY : field_now?CLR_GREEN:CLR_ORANGE);

    // ── toolbar (three rows) ── row1 = cross-section lane types (incl. pavement) · row2 = curb/lanes/view · row3 = skew/topology/treatment
    rectfill(0, SCREEN_H-TOOLBAR, SCREEN_W, TOOLBAR, CLR_BLACK);
    int d;
    if (ui_button(4,   SCREEN_H-58, 72, 13, medOn?"centre: median":twltlOn?"centre: twltl":"centre: none")) cycle_centre();
    if (ui_button(80,  SCREEN_H-58, 56, 13, bikeOn==2?"bike:+xing":bikeOn?"bike: on":"bike: off")) bikeOn=(bikeOn+1)%3;
    if (ui_button(140, SCREEN_H-58, 76, 13, parkOn?"parking: on":"parking: off"))   parkOn=!parkOn;
    if (ui_button(220, SCREEN_H-58, 92, 13, peds  ?"pavement: on":"pavement: off")) peds  =!peds;  // #6: pavement = its own lane-type toggle
    print(roundabout?"island R":freeRight?"slip R":"curb radius", 4, SCREEN_H-37, (roundabout||freeRight)?CLR_BLUE:CLR_ORANGE);
    d=step_btn(64, SCREEN_H-40, 14, "-", "+"); if (d){ if(roundabout) islandR+=2*d; else if(freeRight) frR+=2*d; else cornerR+=2*d; }
    snprintf(bb,sizeof bb,"%d",(int)(roundabout?islandR:freeRight?frR:cornerR)); print(bb, 98, SCREEN_H-37, CLR_LIGHT_GREY);
    print("lanes/dir", 124, SCREEN_H-37, CLR_WHITE);
    d=step_btn(180, SCREEN_H-40, 14, "-", "+"); if (d) lanesPer+=d;
    snprintf(bb,sizeof bb,"%d",lanesPer); print(bb, 214, SCREEN_H-37, CLR_LIGHT_GREY);
    if (ui_button(232, SCREEN_H-40, 84, 13, "view: network")) netview=1;
    print("skew", 4, SCREEN_H-19, CLR_PINK);
    d=step_btn(64, SCREEN_H-22, 14, "-", "+"); if (d) skew+=5*d;
    snprintf(bb,sizeof bb,"%d",skew); print(bb, 98, SCREEN_H-19, CLR_LIGHT_GREY);
    if (ui_button(124, SCREEN_H-22, 90, 13, isT?"topology: T":"topology: 4-way")) isT=!isT;
    // one button cycles the junction TREATMENT: plain → turn lanes → free-right → roundabout (all exclusive)
    static const char *TRT[4]={"junction: plain","junction: turn lanes","junction: free-right","junction: roundabout"};
    if (ui_button(220, SCREEN_H-22, 96, 13, TRT[cur_treatment()])) set_treatment((cur_treatment()+1)%4);
    if (cornerR<0) cornerR=0;  if (cornerR>28) cornerR=28;
    if (islandR<3) islandR=3;  if (islandR>20) islandR=20;
    if (frR<8) frR=8;       if (frR>24) frR=24;
    if (lanesPer<1) lanesPer=1; if (lanesPer>3) lanesPer=3;
    if (skew<-60) skew=-60;    if (skew>60) skew=60;

    ui_end();
    font(FONT_NORMAL);
}

// ── spec() — the cart-logic safety net (docs/design/spec-harness.md). streetlab is the FIRST cart to
//    carry one: curb_return() + count_corners() are pure, deterministic fns (clean seam), so they pin the
//    M1 primitive AND the M2 topology. Run: `node tools/spec.js streetlab` (or all: `node tools/spec.js`). ──
#ifdef DE_SPEC
#include "spec.h"
static float sp_dist(float ax,float ay,float bx,float by){ return sqrtf((ax-bx)*(ax-bx)+(ay-by)*(ay-by)); }
static float sp_dline(float kx,float ky,float dir,float px,float py){     // ⊥ distance from (px,py) to the line through K along dir
    return fabsf((px-kx)*uy(dir) - (py-ky)*ux(dir));                       // dir is a unit vector (ux,uy)
}
// sp_dist/sp_dline are cart-specific; the generic spec_near/spec_tap come from spec.h.

void spec(void){
    // ── curb_return() geometry — the M1 primitive, pinned (perpendicular) ──
    CurbReturn c = curb_return(100,100, 270, 0, 10.f);                     // perpendicular corner, edges up + east, R=10
    expect(spec_near(sp_dline(100,100,270, c.ox,c.oy), 10.f), "fillet centre is R from curb edge 1 (tangent)");
    expect(spec_near(sp_dline(100,100,  0, c.ox,c.oy), 10.f), "fillet centre is R from curb edge 2 (tangent)");
    expect(spec_near(sp_dist(c.ox,c.oy,c.t1x,c.t1y), 10.f),   "tangent point 1 sits R from the centre");
    expect(spec_near(sp_dist(c.ox,c.oy,c.t2x,c.t2y), 10.f),   "tangent point 2 sits R from the centre");
    expect(c.ox > 100 && c.oy < 100,                        "centre is on the sidewalk side (NE for up+east)");

    CurbReturn small = curb_return(100,100,270,0, 5.f);
    CurbReturn big   = curb_return(100,100,270,0,20.f);
    expect(sp_dist(100,100,big.ox,big.oy) > sp_dist(100,100,small.ox,small.oy),
           "bigger radius pushes the centre further from the corner");

    CurbReturn zero = curb_return(100,100,270,0, 0.f);
    expect(spec_near(zero.ox,100) && spec_near(zero.oy,100),    "R=0 collapses to a sharp corner (no fillet)");

    // ── M2: tangency holds at a SKEWED (non-90°) corner — proves the grammar is angle-agnostic ──
    CurbReturn sk = curb_return(100,100, 270, 40, 10.f);                   // a 70° corner
    expect(spec_near(sp_dline(100,100,270, sk.ox,sk.oy), 10.f), "skew: centre is R from edge 1 (tangent at 70deg)");
    expect(spec_near(sp_dline(100,100, 40, sk.ox,sk.oy), 10.f), "skew: centre is R from edge 2 (tangent at 70deg)");

    // ── M2: topology — count_corners() is pure over (skew, isT) ──
    skew=0;  isT=0;  expect_eq(count_corners(), 4, "4-way crossing has 4 corners");
    skew=20; isT=0;  expect_eq(count_corners(), 4, "a skewed 4-way still has 4 corners");
    skew=0;  isT=1;  expect_eq(count_corners(), 2, "a T-junction has 2 corners (straight back, no corner)");
    skew=0;  isT=0;                                                        // restore for the loop test

    // ── M3: arm_face() — the mouth distance the turn bay + median key off. On a perpendicular 4-way the
    //    corner projects to HW on the arm axis, so the face = HW. PURE (geometry of the leg layout). ──
    { int ix[NLEG]; float br[NLEG]; int m=present_legs(ix,br);
      expect(spec_near(arm_face(br,m,0,16.f), 16.f), "4-way arm face = HW (corner projects to HW on the axis)"); }

    // ── M4: the street web — gen_network() is pure over (pattern, seed); assert on the GRAPH (SNDi §8.2) ──
    gen_network(PAT_GRID, 1);
    expect_eq(nn, GW*GH, "grid network: GW*GH nodes");
    expect_eq(ne, GW*(GH-1)+GH*(GW-1), "grid network: edges = horizontals + verticals");
    expect_eq(dead_ends(), 0, "a full grid has no dead-ends");
    expect(mean_degree() > 2.5f && mean_degree() < 4.0f, "grid mean degree in (2.5, 4)");
    gen_network(PAT_ORGANIC, 7); float gx=nnodes[5].x;
    gen_network(PAT_ORGANIC, 7); expect(spec_near(nnodes[5].x, gx), "gen_network is deterministic for a fixed seed");
    gen_network(PAT_ORGANIC, 8); expect(!spec_near(nnodes[5].x, gx), "a different seed moves the nodes");
    // M4b topology changes — the patterns now differ structurally (the SNDi point: metrics separate them)
    gen_network(PAT_RADIAL, 1);
    expect_eq(nn, 1+R_RINGS*R_SECTORS, "radial: hub + rings*sectors nodes");
    expect_eq(node_degree(0), R_SECTORS, "radial: the hub is the high-degree node (degree = spokes)");
    gen_network(PAT_CULDESAC, 3);
    expect(ne >= nn-1, "cul-de-sac is connected (>= n-1 edges — a spanning tree + sparse loops)");
    expect(dead_ends() > 0, "cul-de-sac has dead-ends (grid has none — the topology differs)");
    expect(mean_degree() < 3.0f, "cul-de-sac mean degree < a full grid's (dendritic, not gridded)");
    // §8.2: the DEGREE DISTRIBUTION is the real discriminator (mean degree alone can blur two networks).
    // grid = zero cul-de-sacs + a big X-crossroads share; cul-de-sac = real degree-1 share + far fewer Xs.
    gen_network(PAT_GRID, 1);
    expect_eq(deg_count(1), 0,             "grid node mix: zero cul-de-sac (degree-1) nodes");
    expect(deg_count_ge(4) > 0,            "grid node mix: has X-crossroads (degree-4+) nodes");
    int grid_xshare = deg_pct(deg_count_ge(4));
    gen_network(PAT_CULDESAC, 3);
    expect(deg_count(1) > 0,               "cul-de-sac node mix: a real degree-1 (dead-end) share");
    expect(deg_pct(deg_count_ge(4)) < grid_xshare,
           "node mix separates the patterns: cul-de-sac's X(4+) share < grid's");
    // M4c: the curvature knob — straight chords pin sinuosity at 1; winding pushes it above 1 (a live SNDi measure)
    gen_network(PAT_GRID, 1);
    curveAmt=0; expect(spec_near(mean_sinuosity(), 1.0f), "curve=0: straight chords, sinuosity == 1");
    curveAmt=3; expect(mean_sinuosity() > 1.0f,         "curve>0: roads wind, sinuosity > 1");
    curveAmt=0;                                                            // restore

    // ── Stage 2: the SUPERBLOCK + CIRCUITY (§8.2 #3 / §8.3). CIRCUITY is what PROVES the superblock is a
    //    distinct pattern — and this is §8.2's thesis in action ("node degree ALONE is not enough"): the
    //    superblock and the cul-de-sac read NEARLY IDENTICAL on degree/dead-ends/node-mix (both ~dendritic),
    //    yet circuity orders all three GRID < SUPERBLOCK < CUL-DE-SAC. The superblock sits between because its
    //    continuous arterial FRAME gives through-routes a pure dendritic net lacks (circuity below cul-de-sac),
    //    while its sealed interior still forces detours (circuity above the grid). The metric + the pattern
    //    ship as one unit — the discriminator and the thing only it can discriminate. ──
    gen_network(PAT_GRID, 1);       float circ_grid = mean_circuity();
    expect(circ_grid > 1.0f && circ_grid < 1.6f,  "grid circuity is low (near-straight trips)");
    expect(net_connected(),                       "grid is one connected component");
    gen_network(PAT_CULDESAC, 3);   float circ_cds = mean_circuity();
    gen_network(PAT_SUPERBLOCK, 1); float circ_sb  = mean_circuity();
    expect(circ_sb > circ_grid,                   "superblock circuity > grid (cars detour the sealed interior)");
    expect(circ_sb < circ_cds,                    "superblock circuity < cul-de-sac (the arterial frame gives through-routes)");
    expect(net_connected(),                       "superblock is one connected component (interior reaches the frame)");
    expect(deg_count_ge(4) > 0,                   "superblock keeps degree-4 arterial crossings (the continuous frame)");
    expect(dead_ends() > 0,                       "superblock has interior cul-de-sacs (the permeable-edge stubs)");
    // determinism (same contract as the other patterns)
    gen_network(PAT_SUPERBLOCK, 4); float sbx = nnodes[12].x;
    gen_network(PAT_SUPERBLOCK, 4); expect(spec_near(nnodes[12].x, sbx), "superblock is deterministic for a fixed seed");
    // the curve knob curves ONLY the calmed interior — the arterial frame stays straight (engineered through-routes)
    netseed=1; gen_network(PAT_SUPERBLOCK, netseed); curveAmt=3;
    int art_straight=1, local_winds=0;
    for (int i=0;i<ne;i++){
        if (nedges[i].arterial){ if (edge_bow(i)!=0.f) art_straight=0; }
        else if (edge_bow(i)>1.f || edge_bow(i)<-1.f) local_winds=1;
    }
    expect(art_straight, "superblock: the arterial frame stays straight under the curve knob");
    expect(local_winds,  "superblock: the curve knob DOES wind the interior local streets");
    curveAmt=0;                                                            // restore

    // ── M6: mini-roundabout — round_icr() is pure over (islandR, lanesPer); the island sits strictly inside
    //    the inscribed circle and stays MINI (traversable: its radius <= the circulatory width). ──
    islandR=8; lanesPer=2;
    expect(round_icr() > islandR,                            "roundabout: inscribed circle is bigger than the island");
    expect(spec_near(round_icr(), islandR + lanesPer*LANEW), "roundabout: ICR = island radius + circulatory width");
    expect(islandR <= round_circ_w(),                        "mini-roundabout: island is traversable (R <= circulatory width)");
    float icr2 = round_icr(); lanesPer=3;
    expect(round_icr() > icr2,                               "more lanes widen the circulatory => a bigger inscribed circle");
    lanesPer=2;
    // 'r' toggles the roundabout and clears turn lanes (the two crossing treatments are mutually exclusive)
    turnLanes=1; roundabout=0; spec_tap('r');
    expect(roundabout==1 && turnLanes==0, "the 'r' key toggles the roundabout and clears turn lanes");
    spec_tap('r'); expect(roundabout==0,  "'r' again clears the roundabout");
    // in roundabout mode the [ ] knob drives the island radius (not curb radius); it stays mini-clamped
    roundabout=1; islandR=8;
    for (int i=0;i<40;i++) spec_tap(']'); expect(islandR <= 20.f, "roundabout: island radius caps at 20 (stays mini)");
    for (int i=0;i<40;i++) spec_tap('['); expect(islandR >= 3.f,  "roundabout: island radius floors at 3");
    roundabout=0;                                                          // restore before the curb-radius clamps

    // ── M7: typed cross-section — cross_hw() is pure over the lane toggles; each element adds its width, and
    //    every junction primitive keys off the sum. The median is the same island primitive as M3/M6. ──
    medOn=0; bikeOn=0; parkOn=0; lanesPer=2;
    expect(spec_near(cross_hw(), 2*LANEW),                 "cross-section: plain = lanesPer driving lanes wide");
    bikeOn=1; expect(spec_near(cross_hw(), 2*LANEW+BIKEW), "a bike lane adds its width to the carriageway");
    parkOn=1; expect(spec_near(cross_hw(), 2*LANEW+BIKEW+PARKW), "parking adds its width too");
    medOn=1;  expect(spec_near(cross_hw(), MEDHW+2*LANEW+BIKEW+PARKW), "HW = median + driving + bike + parking");
    { float fb[4]={0,90,180,270};                                          // the junction geometry keys off HW:
      expect(arm_face(fb,4,0, cross_hw()) > arm_face(fb,4,0, 2*LANEW),
             "a wider cross-section pushes the mouth further out (arm_face scales with HW)"); }
    // the SHARED lane offsets — turn lanes, the stop bar and the roundabout approaches all read these (Pass 1)
    medOn=0; expect(spec_near(drive_inner(), 0),     "no median: driving lanes start at the centreline");
    medOn=1; expect(spec_near(drive_inner(), MEDHW), "median: driving lanes start outside it (drive_inner == MEDHW)");
    expect(spec_near(drive_outer(), MEDHW+lanesPer*LANEW), "drive_outer = drive_inner + the driving lanes");
    medOn=0; bikeOn=0; parkOn=0;
    // ── Stage-1 #3: TWLTL is a CENTRE lane type — it routes through centre_hw()/drive_inner() exactly like the
    //    median, so every junction primitive absorbs it (no hardcoded offsets ⇒ no M7-style regression). ──
    twltlOn=1; expect(spec_near(cross_hw(), TWLTLHW+2*LANEW), "TWLTL adds its half-width to the carriageway (like a median)");
    expect(spec_near(drive_inner(), TWLTLHW),                 "TWLTL: driving lanes start outside it (drive_inner == TWLTLHW)");
    twltlOn=0;
    // the 'm' control CYCLES the centre treatment none → median → TWLTL → none; median and TWLTL are never both set
    medOn=0; twltlOn=0;
    cycle_centre(); expect(medOn==1 && twltlOn==0, "centre cycle: none → raised median");
    cycle_centre(); expect(medOn==0 && twltlOn==1, "centre cycle: median → TWLTL (median cleared — mutually exclusive)");
    cycle_centre(); expect(medOn==0 && twltlOn==0, "centre cycle: TWLTL → none (full cycle)");
    // ── step 3b: the RIGHT-TURN POCKET peels the OUTER driving lane (delineation at drive_outer()-LANEW), the
    //    mirror of the left bay at drive_inner()+LANEW. Both read the shared datums ⇒ centre-treatment-aware. ──
    medOn=0; twltlOn=0; lanesPer=2;
    expect(spec_near(drive_outer()-LANEW, drive_inner()+LANEW), "2 lanes: right pocket meets the left bay at the shared divider");
    lanesPer=3;
    expect(drive_outer()-LANEW > drive_inner()+LANEW,           "3 lanes: right pocket (outer) sits outboard of the left bay (a through lane between)");
    medOn=1; expect(spec_near(drive_outer()-LANEW, MEDHW+lanesPer*LANEW-LANEW), "right pocket edge tracks drive_outer (median-aware, no hardcoded offset)");
    medOn=0; lanesPer=2;
    // Pass 2: the bike lane is OUTERMOST (kerb-side) so it can wrap the corner; parking sits inboard of it
    parkOn=1; bikeOn=1; lanesPer=2;
    expect(spec_near(park_inner(), drive_outer()),               "parking sits just outside the driving lanes");
    expect(spec_near(bike_inner(), drive_outer()+PARKW),         "bike is outboard of parking (at the kerb)");
    expect(spec_near(bike_inner()+BIKEW, cross_hw()),            "the bike lane is the OUTERMOST lane (its outer edge = HW)");
    parkOn=0; expect(spec_near(bike_inner(), drive_outer()),     "no parking: the bike lane is still outermost (at the kerb)");
    bikeOn=0; parkOn=0;
    // the roundabout circulatory tracks the FULL cross-section (so parking can't undersize the disc/pavement)
    islandR=8; parkOn=1; expect(spec_near(round_icr(), 8 + lanesPer*LANEW + PARKW),
        "roundabout ICR includes parking (circulatory = full approach half-width)");
    parkOn=0;
    // round_flare — the approach bike lane starts here so its kerb-side edge LANDS on the ring (the connection fix)
    { float icr=40.f, hw=21.f, d=round_flare(icr,hw);
      expect(spec_near(sqrtf(d*d+hw*hw), icr), "flare: the kerb-edge at the flare point sits exactly on the ring (sqrt(d^2+HW^2)==ICR)");
      expect(d < icr, "the flare point is inside the inscribed circle (so the lane reaches in to the ring)"); }
    // kerb_start — the PER-SIDE kerb-lane start (bike/parking) that meets the corner-wrap arc. The skew fix: a
    // skewed arm's two sides face different gaps, so each starts at its own corner's TANGENT = (HW+R)/tan(half),
    // NOT HW/tan(half)+R (equal only at 90°; the latter gapped on obtuse corners at a big radius).
    { float fb[4]={0,90,180,270};                                          // perpendicular: both sides equal, tangent = HW+R-2
      expect(spec_near(kerb_start(fb,4,0,16.f,8.f,+1), kerb_start(fb,4,0,16.f,8.f,-1)), "perpendicular: an arm's two sides start equal");
      expect(spec_near(kerb_start(fb,4,0,16.f,8.f,+1), 16.f+8.f-2.f),     "90deg start = (HW+R)/tan45 - 2");
      expect(kerb_start(fb,4,0,16.f,24.f,+1) > kerb_start(fb,4,0,16.f,8.f,+1), "a bigger curb radius pushes the start out"); }
    { float fs[4]={0,120,180,300};                                         // SKEW: obtuse(+side,120) vs acute(-side,60)
      float kp=kerb_start(fs,4,0,16.f,24.f,+1), km=kerb_start(fs,4,0,16.f,24.f,-1);
      expect(!spec_near(kp,km), "skew: an arm's two sides start at DIFFERENT distances (acute vs obtuse corner)");
      expect(km > kp,           "the acute side starts further out than the obtuse side");
      expect(!spec_near(kp, 16.f/tanf(60*DEG2RAD)+24.f),
             "obtuse start uses (HW+R)/tan(half), NOT HW/tan(half)+R — the big-radius skew fix"); }
    { float ft[3]={0,90,180};                                              // a T (north dropped): arm 0's back is a 180 gap
      expect(spec_near(kerb_start(ft,3,0,16.f,8.f,-1), 0), "T back (straight 180 gap): the lane runs through to the hub (start 0)");
      expect(kerb_start(ft,3,0,16.f,8.f,+1) > 0,           "the cornered side of the same arm still starts at its tangent"); }
    medOn=0; twltlOn=0; spec_tap('m'); expect(medOn==1, "the 'm' key drives the centre cycle (none → median)");
    spec_tap('m'); spec_tap('m');                  expect(medOn==0 && twltlOn==0, "'m' x3 returns the centre to none");
    int pk0=parkOn; spec_tap(';'); expect(parkOn!=pk0,"the ';' key toggles the parking lane");
    // Pass 3: 'b' cycles the bike lane through 3 states — off → lanes(+corner-wrap) → +straight-through crossing
    bikeOn=0; spec_tap('b'); expect_eq(bikeOn,1, "'b' #1: bike lanes on (corner-wrap default)");
    spec_tap('b');           expect_eq(bikeOn,2, "'b' #2: + the straight-through crossing (opt-in)");
    spec_tap('b');           expect_eq(bikeOn,0, "'b' #3: back to off (3-state cycle)");
    medOn=0; bikeOn=0; parkOn=0;                                            // restore

    // ── Stage-1 #2: the free-right slip + pork-chop island. The PIXEL invariant (the island never crosses a
    //    kerb line into a travel lane) is enforced by the gap-side re-lay, not pure math — eyeball it. Spec the
    //    geometry that underpins it: the fr_fits corner window, the constant-width slip band, and the toggle. ──
    expect(fr_fits(90.f),   "free-right fits a ~perpendicular corner (90deg gap)");
    expect(fr_fits(120.f),  "free-right fits a moderately obtuse corner");
    expect(!fr_fits(30.f),  "free-right skipped on a too-acute corner (< 40deg gap — no room)");
    expect(!fr_fits(160.f), "free-right skipped on a near-straight corner (> 145deg gap)");
    // the slip = the lane-centreline fillet ± SLIPW/2: two CONCENTRIC arcs (one centre) ⇒ width == SLIPW at any
    // angle. Pin the fillet tangency (the basis) at 90° and at skew, like curb_return above.
    CurbReturn fr90 = curb_return(100,100, 270, 0, 14.f);
    expect(spec_near(sp_dist(fr90.ox,fr90.oy, fr90.t1x,fr90.t1y), 14.f), "free-right: slip fillet is frR from its tangent (90deg)");
    expect(spec_near((14.f+SLIPW*0.5f) - (14.f-SLIPW*0.5f), (float)SLIPW), "free-right: slip band is constant width (ro - ri == SLIPW)");
    CurbReturn frSk = curb_return(100,100, 270, 40, 14.f);                   // a 70° corner
    expect(spec_near(sp_dist(frSk.ox,frSk.oy, frSk.t1x,frSk.t1y), 14.f), "free-right: slip fillet holds frR at skew (angle-agnostic)");
    turnLanes=1; roundabout=0; spec_tap('f');
    expect(freeRight==1 && turnLanes==0, "'f' selects the free-right and clears turn lanes (exclusive treatment)");
    spec_tap('f'); expect(freeRight==0,  "'f' again clears the free-right");
    freeRight=1; for (int i=0;i<40;i++) spec_tap(']'); expect(frR <= 24.f, "free-right: slip radius caps at 24");
    for (int i=0;i<40;i++) spec_tap('['); expect(frR >= 8.f,              "free-right: slip radius floors at 8");
    freeRight=0;                                                            // restore

    // ── update() loop — the radius knob clamps + the turn-lanes toggle (proves step() + key injection) ──
    for (int i=0;i<40;i++) spec_tap(']');                                    // hammer the + key past the cap
    expect(cornerR <= 28.f, "curb radius caps at 28");
    for (int i=0;i<40;i++) spec_tap('[');                                    // hammer the - key past the floor
    expect(cornerR >= 0.f,  "curb radius floors at 0");
    int t0=turnLanes; spec_tap('p'); expect(turnLanes!=t0, "the 'p' key toggles turn lanes");
    int k0=peds;      spec_tap('k'); expect(peds!=k0,      "the 'k' key toggles sidewalks + crosswalks");
    // step 3c: driveways cycle the per-side bitmask off → +90 (1) → -90 (2) → both (3) → off
    driveways=0;
    spec_tap('d'); expect_eq(driveways,1, "'d' #1: driveways on the +90 side");
    spec_tap('d'); expect_eq(driveways,2, "'d' #2: driveways on the -90 side");
    spec_tap('d'); expect_eq(driveways,3, "'d' #3: driveways on both sides");
    spec_tap('d'); expect_eq(driveways,0, "'d' #4: driveways off (full cycle)");
}
#endif
