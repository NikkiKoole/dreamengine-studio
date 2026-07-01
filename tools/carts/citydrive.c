/* de:meta
{
  "title": "citydrive",
  "status": "active",
  "created": "2026-06-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "isometric-projection",
    "camera-follow"
  ],
  "todo": [
    "Collision against building footprints — right now there's no collision so the car drives straight through buildings (you end up 'inside' a tower wall, obvious in perspective among Manhattan's skyscrapers). Block/slide the car against footprint edges so it feels like real driving.",
    "Per-feature depth sort for perspective (terrain cells draw in grid order, buildings sort by centroid) — occasional wrong overlaps on steep/dense views.",
    "Bridge decks from OSM bridge/layer DONE (raised deck + fascia + shadow, ramped); tunnels DONE (dashed). Refinements left: tunnel PORTALS (dark mouths), pillars under long spans, and per-dataset lift tuning (Delft 3 m; motorway cities want more).",
    "Inner-ring holes (islands in lakes, courtyards) and the hashed other_area understory; >64-vertex footprint clamp (MAXBV)."
  ],
  "lineage": "The data-driven successor to cityview. cityview is a procedural render BENCH (seeded axis-aligned boxes) for the pseudo-3D city look; citydrive keeps its proven projection/camera/driving machinery but extrudes ARBITRARY POLYGON footprints from a REAL OpenStreetMap export (roadview's .rvb data, loaded at runtime), so you drive an actual city in pseudo-3D. Building heights ride along in the RVB2 format (OSM height/building:levels, with a per-footprint fallback where untagged). Sibling of roadview (same data, 2D top-down) — swap the file, see a different city.",
  "description": "Drive a REAL city in pseudo-3D. citydrive loads the SAME OpenStreetMap data as roadview (a .rvb fetched by data-tools/roadview/osm-roads.js) but instead of drawing it flat top-down, it EXTRUDES every building footprint straight up the screen — GTA1-meets-Zelda, the cityview look applied to real geometry. Footprints are arbitrary OSM polygons (not boxes), extruded with winding-based wall culling + polyfill roof caps; heights come from the RVB2 format (OSM height / building:levels tags, ~6% coverage, with a per-footprint fallback for the untagged majority). Roads draw as flat ground ribbons in the real road hierarchy, over a ground of flat area fills — blue water, green parks/woods, peach sand, and muted developed-land/zoning — so the city isn't bare between buildings. If the data carries a terrain heightfield (fetch with osm-roads --dem, e.g. San Francisco's hills or a fjord/lake like Konigssee), the whole scene DRAPES over a shaded low-poly hillside — roads, footprints and the car all ride the real elevation (exaggeration auto-scales so 138 m city hills and 1200 m mountain walls both read). It needn't be a city: a nature bbox renders as water + forest + mountains (forest dithers so the shaded slopes show through), and you can zoom right out to take in a whole landscape. You spawn in the dense building mass (robust against the OSM bbox-balloon). V flattens the whole thing to a top-down 2D map (handy to check that footprints sit beside roads, not on them). Opens on data/demo.rvb; DRAG any .rvb/.json from the data folder onto the window to load that town, or OPEN reveals the folder. Arrows/WASD drive; M cycles the CAMERA — north-up / heading-up / a real pitched PERSPECTIVE (pinhole, perspective divide → horizon + foreshortening; with terrain, mountains rise against the skyline), with live ,/. pitch and ;/' eye tuning; V top-down map, T textures, R toggles roads, [ ] zoom. Roads are drawn geometry-first: one connected asphalt surface with class-based widths, light pavement/kerb bands, dashed lane centre-lines, and — from an in-cart junction graph — give-way haaientanden on the minor approach of each priority-controlled crossing (the bigger road keeps voorrang); separate cycleways render as red Dutch fietspaden. P/L/G toggle pavement / lane-markings / give-way independently (fine detail fades when you zoom far out)."
}
de:meta */
#include "studio.h"
#include "json.h"      // EXPERIMENTAL cart-land JSON reader (vendored jsmn)
#include <math.h>
#include <stdio.h>
#include <stdlib.h>    // abs
#include <string.h>    // memcpy/memcmp

// CITYDRIVE — drive a REAL OpenStreetMap city in pseudo-3D.
//
// The data-driven successor to cityview (which stays as the procedural bench).
// Same data as roadview — a packed .rvb of an actual town — but where roadview
// draws it flat top-down, citydrive EXTRUDES each building footprint straight up
// the screen (the cityview look), so you can drive around the real streets.
//
// Two capabilities cityview lacked, both proven here:
//   · ARBITRARY POLYGON extrusion — real OSM footprints are N-gons, never boxes.
//     Winding-based per-edge outward normals → visible-wall cull (works on
//     concave rings); polyfill roof caps (even-odd coverage → concave-safe).
//   · REAL HEIGHTS — the RVB2 format carries an OSM height/building:levels metre
//     value per building; only ~6% of buildings are tagged, so untagged ones get
//     a per-footprint fallback (a low-rise, so the tagged talls still stand out).
//
//   make the data:  node data-tools/roadview/osm-roads.js --place "Amersfoort, Netherlands"
//   run it:         opens on data/demo.rvb; DRAG a town's .rvb/.json onto the window to swap.
//
//   ◄▲▼► / WASD  drive · mouse wheel / [ ] zoom · left-drag orbit the view
//   M cycle camera: north-up / heading-up / PERSPECTIVE (1/2/3) — in perspective ,/. pitch  ;/' eye
//   V top-down map · T textures · R roads · drop a file → load

#define HSCALE  1.0f        // metres of height → up-screen units (× zoom). taller = more dramatic 3D
                            // (a tall building's up-screen extrusion occludes the road BEHIND it — correct,
                            // verified via the V top-down map that footprints sit beside roads, not on them).
#define NMAT    6
#define TILE_W  4.0f        // world metres per 16px texture tile
#define QUARTER 0x7BDE      // ~25% black dither
#define MAXREP  6
#define MAXBV   64          // max footprint vertices we extrude (real OSM rings are mostly < 20)
#define RANGE_M 320.0f      // extrude buildings / draw roads within this radius of the camera
#define TERR_RANGE 1700.0f  // terrain mesh + water/green areas draw FAR (so mountains & lakes read as landscape)
#define MAXDRAW 2000        // hard cap on buildings extruded per frame (perf backstop)
#define DATA_DIR "../data"

// ── feature kinds — TWIN of roadview.c's enum and KIND_IX in osm-roads.js ─────
// APPEND ONLY, never reorder: the .rvb encodes each feature's kind as this index.
enum { K_HIGHWAY, K_ARTERIAL, K_ROAD, K_TRACK, K_WATER, K_CANAL, K_BUILDING, K_GREEN, K_TREE,
       K_RESIDENTIAL, K_COMMERCIAL, K_INDUSTRIAL, K_FARM, K_PARKING, K_SAND, K_RAIL, K_COAST,
       K_SECONDARY, K_TERTIARY, K_SERVICE, K_CYCLEWAY, K_FOOTWAY, K_OTHER_AREA, K_OTHER_LINE, K_N };

// road line classes drawn as flat ground ribbons: colour + real half-width (m).
typedef struct { int col; float hw_m; } Road;
static const Road ROAD[K_N] = {
    [K_HIGHWAY]   = { CLR_ORANGE,     7.0f },
    [K_ARTERIAL]  = { CLR_YELLOW,     5.0f },
    [K_ROAD]      = { CLR_LIGHT_GREY, 3.0f },
    [K_TRACK]     = { CLR_BROWN,      1.5f },
    [K_CANAL]     = { CLR_BLUE,       4.0f },
    [K_RAIL]      = { CLR_DARKER_GREY,2.0f },
    [K_SECONDARY] = { CLR_LIGHT_YELLOW,4.0f },
    [K_TERTIARY]  = { CLR_PEACH,      3.0f },
    [K_SERVICE]   = { CLR_MEDIUM_GREY,1.5f },
    [K_CYCLEWAY]  = { CLR_DARK_ORANGE,1.4f },   // Dutch fietspad — warm red surface (dark-red casing added in draw)
    [K_FOOTWAY]   = { CLR_INDIGO,     0.9f },
};
static bool is_road(int k){ return ROAD[k].hw_m > 0.0f; }

static const int MATSLOT[NMAT] = { 1, 0, 3, 2, 1, 3 };   // material → wall texture slot
static int   clampi(int v, int lo, int hi) { return v<lo?lo:(v>hi?hi:v); }

typedef struct { int roof, lit, shade; } Mat;
static const Mat MAT[NMAT] = {
  { CLR_LIGHT_GREY,  CLR_MEDIUM_GREY, CLR_DARK_GREY    },  // concrete
  { CLR_LIGHT_PEACH, CLR_ORANGE,      CLR_BROWN        },  // brick
  { CLR_PINK,        CLR_DARK_RED,    CLR_DARK_PURPLE  },  // painted
  { CLR_BLUE,        CLR_TRUE_BLUE,   CLR_DARKER_BLUE  },  // glass
  { CLR_LIME_GREEN,  CLR_MEDIUM_GREEN,CLR_DARK_GREEN   },  // green roof
  { CLR_LIGHT_YELLOW,CLR_DARK_ORANGE, CLR_DARK_BROWN   },  // sand
};

enum { M_NORTH, M_HEADING, M_PERSP, M_COUNT };
static const char *MODE_NAME[M_COUNT] = { "1 NORTH-UP", "2 HEADING-UP", "3 PERSPECTIVE" };

// ── data pools (sized for a whole big city, like roadview) ───────────────────
#define MAXPTS  5000000
#define MAXBLD   300000
#define MAXWAY   300000
static float PX[MAXPTS], PY[MAXPTS];                 // shared point pool (local metres, Y north-up)
static int   nps;
typedef struct { int start, count; float h, cx, cy; int mat; } DBldg;   // building footprint ring
static DBldg blds[MAXBLD]; static int nbld;
static struct { int kind, start, count; signed char deck, surf, sw; } rways[MAXWAY]; static int nway;  // road lines; deck>0=bridge(layer),<0=tunnel,0=grade; surf/sw = OSM surface/sidewalk codes (0=unknown)

// parse a road's `sub` bridge code (osm-roads.js): "B<layer>" → +layer (bridge), "T..." → -1 (tunnel),
// else 0 (at grade). s is null-terminated. Layer is a single digit in practice; clamp to [1,3].
static signed char parse_deck(const char *s){
  if (!s || !s[0]) return 0;
  if (s[0]=='B'){ int L = (s[1]>='1' && s[1]<='9') ? s[1]-'0' : 1; return (signed char)(L>3?3:L); }
  if (s[0]=='T') return -1;
  return 0;
}
// read one attribute from the ';'-delimited `sub` token string (osm-roads.js roadSub): find the token
// whose FIRST char is `key`, return the char after it (its payload) — or 1 for a bare flag token, 0 if
// absent. Keys: U surface(a/p/g/w/o) · W sidewalk(b/l/r/n) · M maxspeed · L lanes · O oneway · C cycleway · R roundabout.
static char sub_tok(const char *s, char key){
  if (!s) return 0;
  for (const char *p=s; *p; ){
    if (*p==key){ char c=p[1]; return (c && c!=';') ? c : 1; }
    while (*p && *p!=';') p++;
    if (*p==';') p++;
  }
  return 0;
}

// area fills — flat ground polygons (water/green/zoning) drawn BENEATH roads + buildings.
#define MAXAREA 80000
// parks/water/sand keep their real colour; DEVELOPED land (zoning) is muted to quiet ground
// tones so it fills the bare gaps without turning the whole city green/blue (it covers whole
// neighbourhoods). Driving doesn't need a readable zoning legend — just ground that isn't bare.
static const int AREA_COL[K_N] = {
  [K_WATER]=CLR_BLUE, [K_GREEN]=CLR_DARK_GREEN, [K_SAND]=CLR_LIGHT_PEACH,
  [K_RESIDENTIAL]=CLR_DARK_GREY, [K_COMMERCIAL]=CLR_DARK_GREY,
  [K_INDUSTRIAL]=CLR_DARK_BROWN, [K_FARM]=CLR_DARK_BROWN, [K_PARKING]=CLR_DARKER_GREY,
};
static bool is_area(int k){ return AREA_COL[k] != 0; }   // (no area maps to CLR_BLACK=0)
static struct { int kind, start, count; float cx, cy; } areas[MAXAREA]; static int narea;

// ── junction/node model — the at-grade road graph, built once at load (port of
// data-tools/roadview/osm-junction.js). A node = a quantized shared vertex; a JUNCTION = a node
// with >=3 distinct incident-arm bearings. Feeds crosswalks now; the roadkit grade-dispatch later.
#define J_QUANT   1.0f        // node-merge grid (m) — shared OSM vertices share coords
#define J_RAWARM  12          // raw incident directions accumulated per node before dedup
#define MAXARM    8           // arms kept per junction after dedup
#define MAXJUNC   60000       // junction capacity (near-field detail; truncates gracefully)
typedef struct { float x, y; unsigned char narm; float brg[MAXARM]; unsigned char cls[MAXARM]; } Junc;
static Junc junc[MAXJUNC]; static int njunc;
static void build_junctions(void);    // defined below (after the road painters it reuses)

static char dname[64] = "";
static char err[160]  = "";
static int  loaded_ok = 0, truncated = 0;
static float bbminx, bbminy, bbmaxx, bbmaxy;         // data bounds (metres) — maps the heightfield grid
#define MAXHF (256*256)
static float hf_z[MAXHF]; static int hf_g = 0;       // terrain heightfield (metres); hf_g 0 = flat (no DEM)

typedef struct {
  float px, py, ang, spd;         // car: world pos (metres), heading(deg), speed
  float camx, camy, camz, rot, zoom;  // camera: eased pos + ground height + rotation(deg) + px-per-metre
  int   mode;
  bool  started, tex, roads_on, topdown;
  bool  show_pave, show_mark, show_give;   // per-feature dressing toggles (P/L/G) — master roads_on (R) wraps all
  float lookrot;                  // mouse-drag orbit offset on top of the heading; decays while driving
  bool  dragging; int dragx;
  float pitch, eye, setback;      // M_PERSP pitched camera: tilt(deg below horizontal), eye height, chase setback
} State;
static State S;
static float g_hscale = HSCALE;   // 0 in top-down mode → buildings collapse to flat footprints (a 2D map to verify placement)

// terrain ground height at a world point — bilinear sample of the heightfield grid over the bbox.
// 0 when there's no DEM (flat city). Drives the drape: roads/bases sit at ground_z, roofs at +h.
// ADAPTIVE vertical exaggeration: relief reads subtly in this lo-fi oblique view, so we scale the
// elevation to a consistent on-screen drama (TARGET_RELIEF world-units) regardless of whether the
// data is 138 m city hills or 1500 m fjord walls. Set per-dataset from the heightfield's max.
#define TARGET_RELIEF 330.0f
static float g_exag = 1.0f;
static float ground_z(float wx, float wy) {
  if (!hf_g) return 0;
  float u = (wx-bbminx)/(bbmaxx-bbminx+1e-4f), v = (wy-bbminy)/(bbmaxy-bbminy+1e-4f);
  if (u<0) u=0; else if (u>1) u=1;  if (v<0) v=0; else if (v>1) v=1;
  float fx = u*(hf_g-1), fy = v*(hf_g-1);
  int x0=(int)fx, y0=(int)fy, x1=x0<hf_g-1?x0+1:x0, y1=y0<hf_g-1?y0+1:y0;
  float tx=fx-x0, ty=fy-y0;
  float a=hf_z[y0*hf_g+x0], b=hf_z[y0*hf_g+x1], c=hf_z[y1*hf_g+x0], d=hf_z[y1*hf_g+x1];
  return g_exag * ((a*(1-tx)+b*tx)*(1-ty) + (c*(1-tx)+d*tx)*ty);
}

// ── projection (ADR 0021) — world (x,y,z metres) → screen, height straight up ─
static void project(float wx, float wy, float wz, float *sx, float *sy) {
  float dx = wx - S.camx, dy = wy - S.camy;
  float c = cos_deg(S.rot), s = sin_deg(S.rot);
  float rx = dx*c - dy*s, ry = dx*s + dy*c;
  if (S.mode == M_PERSP) {
    // pitched PINHOLE camera (ported from cityview): perspective divide → horizon + foreshortening.
    // combined with terrain, mountains rise against the skyline. AHEAD is -ry in this heading-up frame.
    float F  = S.setback - ry;                 // forward distance ahead of the eye
    float Hh = (wz - S.camz) - S.eye;          // height relative to the eye (camz tracks the ground under the car)
    float cd = cos_deg(S.pitch), sd = sin_deg(S.pitch);
    float depth = F*cd - Hh*sd;                // into-screen depth
    float up    = F*sd + Hh*cd;                // screen-vertical (taller → higher)
    if (depth < 1.0f) depth = 1.0f;
    float f = S.zoom * 90.0f;
    *sx = SCREEN_W*0.5f + f * rx / depth;
    *sy = SCREEN_H*0.5f - f * up / depth;
    return;
  }
  *sx = SCREEN_W*0.5f + rx*S.zoom;
  *sy = SCREEN_H*0.5f + ry*S.zoom - (wz - S.camz)*S.zoom*g_hscale;  // height up-screen, relative to camera ground (terrain)
}
static void wpt(float x,float y,float z,int*ix,int*iy){ float a,b; project(x,y,z,&a,&b); *ix=(int)a; *iy=(int)b; }

static void pproj_poly(const float*wx,const float*wy,int n,int color){
  int xy[2*MAXBV]; if(n>MAXBV) n=MAXBV;
  for(int i=0;i<n;i++) wpt(wx[i],wy[i],0,&xy[2*i],&xy[2*i+1]);
  polyfill(xy,n,color);
}
static void pdisc(float cx,float cy,float r,int color){
  enum { N=14 }; int xy[2*N]; float z=ground_z(cx,cy);     // flat disc at the ground height under its centre
  for(int i=0;i<N;i++){ float a=i*(360.0f/N); wpt(cx+cos_deg(a)*r, cy+sin_deg(a)*r, z, &xy[2*i], &xy[2*i+1]); }
  polyfill(xy,N,color);
}

// ── data loading (twin of roadview's loader; reads RVB2 heights) ─────────────
static void reset_pools(void){ nps=0; nbld=0; nway=0; narea=0; njunc=0; hf_g=0; g_exag=1.0f; loaded_ok=0; truncated=0; dname[0]=0; err[0]=0; }

static int bld_mat(int start){ unsigned h=(unsigned)start*2654435761u; h^=h>>13; return (h>>8)%NMAT; }

// route one decoded feature into the building or road pools.
static void store_feature(int kind, int start, int count, float h, const char *sub){
  if (kind == K_BUILDING){
    if (nbld >= MAXBLD || count < 3) return;
    if (PX[start]==PX[start+count-1] && PY[start]==PY[start+count-1]) count--;   // drop closing dup
    if (count < 3) return;
    float cx=0, cy=0; for(int i=0;i<count;i++){ cx+=PX[start+i]; cy+=PY[start+i]; }
    cx/=count; cy/=count;
    if (h <= 0) h = 6 + (float)(((unsigned)start>>3) % 4) * 4;   // fallback low-rise where untagged
    blds[nbld++] = (DBldg){ start, count, h, cx, cy, bld_mat(start) };
  } else if (is_road(kind)){
    if (nway >= MAXWAY || count < 2) return;
    rways[nway].kind = kind; rways[nway].start = start; rways[nway].count = count;
    rways[nway].deck = parse_deck(sub);
    rways[nway].surf = (signed char)sub_tok(sub,'U');   // a/p/g/w/o surface (0=unknown)
    rways[nway].sw   = (signed char)sub_tok(sub,'W');   // b/l/r/n sidewalk (0=unknown)
    nway++;
  } else if (is_area(kind)){
    if (narea >= MAXAREA || count < 3) return;
    if (PX[start]==PX[start+count-1] && PY[start]==PY[start+count-1]) count--;   // drop closing dup
    if (count < 3) return;
    float cx=0, cy=0; for(int i=0;i<count;i++){ cx+=PX[start+i]; cy+=PY[start+i]; }
    areas[narea].kind=kind; areas[narea].start=start; areas[narea].count=count;
    areas[narea].cx=cx/count; areas[narea].cy=cy/count; narea++;
  }
  // trees / hashed other_area+other_line understory: still ignored (see TODO)
}

// fast binary path (magic RVB1/RVB2) — RVB2 carries a float height per feature.
static void load_bin(const char *buf, long len){
  int ver = buf[3];
  const char *p = buf+4, *end = buf+len;
  int nfeat, namelen; memcpy(&nfeat,p,4); p+=4; memcpy(&namelen,p,4); p+=4;
  int nl=namelen; if(nl<0)nl=0; if(nl>(int)sizeof dname-1)nl=(int)sizeof dname-1;
  memcpy(dname,p,(size_t)nl); dname[nl]=0; p+=namelen;
  float bb[4]; memcpy(bb,p,16); p+=16;
  bbminx=bb[0]; bbminy=bb[1]; bbmaxx=bb[2]; bbmaxy=bb[3];
  for(int f=0; f<nfeat && p+16<=end; f++){
    int kind, sublen, npts; float fh=0;
    memcpy(&kind,p,4); p+=4;
    if (ver=='2'||ver=='3'){ memcpy(&fh,p,4); p+=4; }
    memcpy(&sublen,p,4); p+=4;
    char subb[32]={0}; { int sl = sublen<31?sublen:31; if(sl>0) memcpy(subb,p,(size_t)sl); }   // road attr tokens (bridge/surface/sidewalk/…)
    p+=sublen;
    memcpy(&npts,p,4); p+=4;
    if (kind<0||kind>=K_N) kind=K_ROAD;
    int start=nps, got=0;
    for(int j=0;j<npts && p+8<=end;j++){ if(nps<MAXPTS){ float xy[2]; memcpy(xy,p,8); PX[nps]=xy[0]; PY[nps]=xy[1]; nps++; got++; } p+=8; }
    if (got>=2) store_feature(kind, start, got, fh, subb);
  }
  if (ver=='3' && p+4<=end){                               // trailing terrain heightfield: int32 G + float32 grid[G*G]
    int g; memcpy(&g,p,4); p+=4;
    if (g>0 && g<=256 && p + (long)g*g*4 <= end){
      hf_g=g; memcpy(hf_z, p, (size_t)g*g*4);
      float mx=0; for(int i=0;i<g*g;i++) if(hf_z[i]>mx) mx=hf_z[i];   // adaptive exag: normalise relief on-screen
      g_exag = mx>1 ? fmaxf(0.12f, fminf(3.0f, TARGET_RELIEF/mx)) : 1.0f;
    }
  }
}

// JSON path (readable .json) — features carry an optional "h".
static void load_json(char *js, long len){
  jsmntok_t *tok; int nt = json_parse(js, &tok);
  if (nt < 1 || tok[0].type != JSMN_OBJECT){ snprintf(err,sizeof err,"bad json"); free(tok); return; }
  int ni = json_get(js,tok,0,"name"); if (ni>=0) json_str(dname,sizeof dname,js,&tok[ni]);
  int fa = json_get(js,tok,0,"features");
  if (fa>=0 && tok[fa].type==JSMN_ARRAY){
    int fi = fa+1;
    for (int f=0; f<tok[fa].size; f++){
      int ki=json_get(js,tok,fi,"kind"), pi=json_get(js,tok,fi,"pts"), hi=json_get(js,tok,fi,"h");
      int kind = K_ROAD;
      if (ki>=0){ char kb[24]; json_str(kb,sizeof kb,js,&tok[ki]);
        // map kind name → index via roadview's order (only the ones we use here)
        static const char *NAMES[K_N] = {
          [K_HIGHWAY]="highway",[K_ARTERIAL]="arterial",[K_ROAD]="road",[K_TRACK]="track",
          [K_WATER]="water",[K_CANAL]="canal",[K_BUILDING]="building",[K_GREEN]="green",[K_TREE]="tree",
          [K_RESIDENTIAL]="residential",[K_COMMERCIAL]="commercial",[K_INDUSTRIAL]="industrial",
          [K_FARM]="farm",[K_PARKING]="parking",[K_SAND]="sand",[K_RAIL]="rail",[K_COAST]="coast",
          [K_SECONDARY]="secondary",[K_TERTIARY]="tertiary",[K_SERVICE]="service",
          [K_CYCLEWAY]="cycleway",[K_FOOTWAY]="footway",[K_OTHER_AREA]="other_area",[K_OTHER_LINE]="other_line" };
        for (int k=0;k<K_N;k++) if (NAMES[k] && !strcmp(kb,NAMES[k])){ kind=k; break; }
      }
      float fh = (hi>=0) ? (float)json_num(js,&tok[hi]) : 0;
      int si=json_get(js,tok,fi,"sub"); char sb[32]={0}; if(si>=0) json_str(sb,sizeof sb,js,&tok[si]);
      if (pi>=0 && tok[pi].type==JSMN_ARRAY){
        int cnt=tok[pi].size/2, start=nps, got=0;
        for (int j=0;j<cnt && nps<MAXPTS;j++){ PX[nps]=(float)json_num(js,&tok[pi+1+j*2]); PY[nps]=(float)json_num(js,&tok[pi+1+j*2+1]); nps++; got++; }
        if (got>=2) store_feature(kind, start, got, fh, sb);
      }
      fi += json_span(tok, fi);
    }
  }
  free(tok);
}

static void spawn_in_mass(void);
static void load_from(const char *path){
  reset_pools();
  long len; char *js = json_slurp(path, &len);
  if (!js){ snprintf(err,sizeof err,"cannot read %s",path); return; }
  if (len>=4 && memcmp(js,"RVB",3)==0 && js[3]>='1' && js[3]<='3') load_bin(js,len);
  else load_json(js, len);
  free(js);
  if (!nbld && !nway){ if(!err[0]) snprintf(err,sizeof err,"no features in %s",path); return; }
  if (nps>=MAXPTS || nbld>=MAXBLD || nway>=MAXWAY) truncated = 1;
  build_junctions();     // detect at-grade intersections → junc[] (crosswalks etc.)
  spawn_in_mass();
  loaded_ok = 1;
}

static void load_data(void){
  const char *path = de_data_path();
  load_from(path ? path : DATA_DIR "/demo.rvb");
  if (!loaded_ok && !path)
    snprintf(err,sizeof err,"no data -- run: node data-tools/roadview/osm-roads.js --demo  (then drop a file)");
}

// spawn in the CENTROID of the building mass (robust against the OSM bbox-balloon,
// where a few long ways stretch the bounds far past the actual town).
static void spawn_in_mass(void){
  double sx=0, sy=0; int n=0;
  for (int i=0;i<nbld;i++){ sx+=blds[i].cx; sy+=blds[i].cy; n++; }
  if (!n){ for (int i=0;i<nway;i++){ sx+=PX[rways[i].start]; sy+=PY[rways[i].start]; n++; } }
  float gx = n ? (float)(sx/n) : (bbminx+bbmaxx)*0.5f;            // mass centroid, else bbox centre (pure nature)
  float gy = n ? (float)(sy/n) : (bbminy+bbmaxy)*0.5f;
  // snap to the building NEAREST the centroid so we start AMONG buildings, not in a gap
  float best=1e30f; S.px=gx; S.py=gy;
  for (int i=0;i<nbld;i++){ float dx=blds[i].cx-gx, dy=blds[i].cy-gy, d=dx*dx+dy*dy;
    if (d<best){ best=d; S.px=blds[i].cx; S.py=blds[i].cy; } }
  S.ang = -45; S.spd = 0;
  S.camx = S.px; S.camy = S.py;
  if (!S.started){ S.mode=M_HEADING; S.zoom=1.5f; S.tex=true; S.roads_on=true;
                   S.show_pave=S.show_mark=S.show_give=true;
                   S.pitch=22.0f; S.eye=20.0f; S.setback=30.0f; }
  S.rot = (S.mode==M_NORTH) ? 0.0f : (-90.0f - S.ang);
  S.started = true;
}

// ── wall texture (same as cityview) ──────────────────────────────────────────
static void wall_tex(float ax,float ay, float bx,float by, float cax,float cay, float cbx,float cby,
                     int slot, int rxN, int ryN, float facing) {
  int scol = slot % 8, srow = slot / 8;
  float su = scol*16.0f, sv = srow*16.0f;
  float u0 = su+0.5f, u1 = su+15.5f, vtop = sv+0.5f, vbot = sv+15.5f;
  for (int j=0;j<ryN;j++) {
    float g0=(float)j/ryN, g1=(float)(j+1)/ryN;
    for (int i=0;i<rxN;i++) {
      float f0=(float)i/rxN, f1=(float)(i+1)/rxN;
      #define PXX(f,g) (int)(((ax+(bx-ax)*(f))*(1-(g))) + ((cax+(cbx-cax)*(f))*(g)))
      #define PYY(f,g) (int)(((ay+(by-ay)*(f))*(1-(g))) + ((cay+(cby-cay)*(f))*(g)))
      int x00=PXX(f0,g0),y00=PYY(f0,g0), x10=PXX(f1,g0),y10=PYY(f1,g0);
      int x11=PXX(f1,g1),y11=PYY(f1,g1), x01=PXX(f0,g1),y01=PYY(f0,g1);
      #undef PXX
      #undef PYY
      tritex(x00,y00,u0,vbot, x10,y10,u1,vbot, x11,y11,u1,vtop);
      tritex(x00,y00,u0,vbot, x11,y11,u1,vtop, x01,y01,u0,vtop);
    }
  }
  int pat = facing>0.66f ? 0 : (facing>0.40f ? FILL_DOTS : QUARTER);
  if (pat) {
    fillp(pat, -1);
    fillp_anchor((int)((ax+bx+cax+cbx)*0.25f), (int)((ay+by+cay+cby)*0.25f));  // pin to THIS wall → no crawl when the camera moves
    quadfill((int)ax,(int)ay,(int)bx,(int)by,(int)cbx,(int)cby,(int)cax,(int)cay, CLR_BLACK);
    fillp_reset(); fillp_anchor(0,0);
  }
}

// ── extrude one real polygon footprint (the key technique) ───────────────────
static void draw_bldg(int start, int count, float h, int mat, float gz){
  int nv = count; if (nv > MAXBV) nv = MAXBV;
  float gx[MAXBV], gy[MAXBV], rx[MAXBV], ry[MAXBV];
  for (int i=0;i<nv;i++){                                  // base sits LEVEL at the terrain height under the footprint
    project(PX[start+i], PY[start+i], gz,   &gx[i], &gy[i]);
    project(PX[start+i], PY[start+i], gz+h, &rx[i], &ry[i]);
  }
  // winding sign → consistent outward normals (concave-safe)
  float area=0; for(int i=0;i<nv;i++){ int j=(i+1)%nv; area += PX[start+i]*PY[start+j]-PX[start+j]*PY[start+i]; }
  float wind = area>0 ? 1.0f : -1.0f;
  float c = cos_deg(S.rot), s = sin_deg(S.rot);

  bool persp = (S.mode == M_PERSP);
  float eyx = S.camx - S.setback*cos_deg(S.ang), eyy = S.camy - S.setback*sin_deg(S.ang);  // eye behind the car

  int order[MAXBV]; float depth[MAXBV], facing[MAXBV]; int n=0;
  for (int e=0;e<nv;e++){
    int a=e, bb=(e+1)%nv;
    float ex=PX[start+bb]-PX[start+a], ey=PY[start+bb]-PY[start+a];
    float L=sqrtf(ex*ex+ey*ey)+1e-4f;
    float nx=(ey/L)*wind, ny=-(ex/L)*wind;       // world outward normal
    float vis, fval;
    if (persp){                                  // back-face cull: wall shows iff its normal faces the eye
      float mmx=(PX[start+a]+PX[start+bb])*0.5f, mmy=(PY[start+a]+PY[start+bb])*0.5f;
      float ddx=mmx-eyx, ddy=mmy-eyy, dd=sqrtf(ddx*ddx+ddy*ddy)+1e-4f;
      vis = -(ddx*nx + ddy*ny); fval = vis/dd;   // fval ≈ cos(facing): ~1 head-on
    } else {                                      // oblique: normal points down-screen toward the viewer
      vis = nx*s + ny*c; fval = vis;
    }
    if (vis > 0.001f){ order[n]=e; facing[n]=fval; depth[n]=(gy[a]+gy[bb])*0.5f; n++; }
  }
  for(int i=0;i<n;i++) for(int j=i+1;j<n;j++)
    if(depth[j]<depth[i]){ float t=depth[i];depth[i]=depth[j];depth[j]=t;
      t=facing[i];facing[i]=facing[j];facing[j]=t; int o=order[i];order[i]=order[j];order[j]=o; }

  // perspective draws the roof FIRST (near walls paint over it — street-level occlusion) and uses
  // flat walls (tritex is affine → wrong under perspective + heavy on big close-up walls).
  if (persp){ int rxy[2*MAXBV]; for(int i=0;i<nv;i++){ rxy[2*i]=(int)rx[i]; rxy[2*i+1]=(int)ry[i]; }
              polyfill(rxy, nv, MAT[mat].roof); }
  for(int k=0;k<n;k++){
    int e=order[k], a=e, bb=(e+1)%nv;
    if (S.tex && !persp){
      float ex=PX[start+bb]-PX[start+a], ey=PY[start+bb]-PY[start+a];
      float wlen=sqrtf(ex*ex+ey*ey);
      int rxN=clampi((int)(wlen/TILE_W+0.5f),1,MAXREP);
      int ryN=clampi((int)(h/TILE_W+0.5f),1,MAXREP);
      wall_tex(gx[a],gy[a], gx[bb],gy[bb], rx[a],ry[a], rx[bb],ry[bb], MATSLOT[mat], rxN, ryN, facing[k]);
    } else {
      int col = (facing[k]>0.6f) ? MAT[mat].lit : MAT[mat].shade;
      quadfill((int)gx[a],(int)gy[a],(int)gx[bb],(int)gy[bb],(int)rx[bb],(int)ry[bb],(int)rx[a],(int)ry[a], col);
    }
  }
  if (!persp){ int rxy[2*MAXBV]; for(int i=0;i<nv;i++){ rxy[2*i]=(int)rx[i]; rxy[2*i+1]=(int)ry[i]; }
               polyfill(rxy, nv, MAT[mat].roof); }
}

// squared distance from point (px,py) to segment a→b — for a pop-free cull that
// keeps any segment passing near the camera, even a long one whose ENDS are far.
static float seg_d2(float ax,float ay,float bx,float by,float px,float py){
  float dx=bx-ax, dy=by-ay, L2=dx*dx+dy*dy;
  float t = L2>0 ? ((px-ax)*dx+(py-ay)*dy)/L2 : 0;
  if (t<0) t=0; else if (t>1) t=1;
  float qx=ax+t*dx-px, qy=ay+t*dy-py; return qx*qx+qy*qy;
}

// fill one flat ground-area polygon (water/green/zoning) at z=0.
static void area_fill(int start,int count,int col){
  enum { CAP=512 }; int n = count>CAP ? CAP : count; int xy[2*CAP];
  for(int i=0;i<n;i++){ float x=PX[start+i],y=PY[start+i]; wpt(x,y,ground_z(x,y),&xy[2*i],&xy[2*i+1]); }   // drape over terrain
  polyfill(xy,n,col);
}

// draw the terrain as a shaded low-poly mesh — the heightfield grid cells near the camera,
// each lit by its slope normal. THIS is what makes hills read as hills (the geometry-drape
// alone is too subtle); area fills / roads / buildings then sit on top of it.
static void draw_terrain(void){
  if (!hf_g) return;
  float gw=bbmaxx-bbminx, gh=bbmaxy-bbminy;
  float cw=gw/(hf_g-1), ch=gh/(hf_g-1);
  int cj=(int)((S.camx-bbminx)/gw*(hf_g-1)), ci=(int)((S.camy-bbminy)/gh*(hf_g-1));
  int rad=(int)(TERR_RANGE/fmaxf(cw,ch))+2;
  for (int i=ci-rad;i<ci+rad;i++){ if(i<0||i>=hf_g-1) continue;
    for (int j=cj-rad;j<cj+rad;j++){ if(j<0||j>=hf_g-1) continue;
      float za=hf_z[i*hf_g+j]*g_exag, zb=hf_z[i*hf_g+(j+1)]*g_exag;
      float zc=hf_z[(i+1)*hf_g+j]*g_exag, zd=hf_z[(i+1)*hf_g+(j+1)]*g_exag;
      float nx=-(zb-za)*ch, ny=-(zc-za)*cw, nz=cw*ch;         // cell normal (world)
      float nl=sqrtf(nx*nx+ny*ny+nz*nz)+1e-4f;
      float L=(nx*-0.45f + ny*-0.30f + nz*0.84f)/nl;          // lambert vs an up-left sun (flat ground ≈ 0.84)
      int col = L>0.90f?CLR_LIGHT_GREY : L>0.80f?CLR_MEDIUM_GREY : L>0.66f?CLR_DARK_GREY : CLR_DARKER_GREY;
      float x0=bbminx+j*cw, x1=x0+cw, y0=bbminy+i*ch, y1=y0+ch;
      int xy[8];
      wpt(x0,y0,za,&xy[0],&xy[1]); wpt(x1,y0,zb,&xy[2],&xy[3]);
      wpt(x1,y1,zd,&xy[4],&xy[5]); wpt(x0,y1,zc,&xy[6],&xy[7]);
      polyfill(xy,4,col);
    }
  }
}

// ── flat ground: one road segment as a world-space quad ribbon ───────────────
static void road_seg(float ax,float ay,float bx,float by,float hw,int col){
  float dx=bx-ax, dy=by-ay, L=sqrtf(dx*dx+dy*dy)+1e-4f;
  float px=-dy/L*hw, py=dx/L*hw;
  float wx[4]={ax+px,bx+px,bx-px,ax-px}, wy[4]={ay+py,by+py,by-py,ay-py};
  int xy[8]; for(int i=0;i<4;i++) wpt(wx[i],wy[i],ground_z(wx[i],wy[i]),&xy[2*i],&xy[2*i+1]);  // drape over terrain
  polyfill(xy,4,col);
}

// Motor carriageways render as ONE connected surface (casing + asphalt, hierarchy by WIDTH) so a junction
// fuses into a coherent road instead of a patchwork of per-class neon fills; bike/foot/canal/rail keep
// their own colour (separate networks). Connectivity Phase 0 — see docs/design/roadkit.md.
static bool is_motor_road(int k){
  return k==K_HIGHWAY||k==K_ARTERIAL||k==K_ROAD||k==K_TRACK||k==K_SECONDARY||k==K_TERTIARY||k==K_SERVICE;
}
// urban street-tier roads carry a pavement/kerb band; motorways (no sidewalk), tracks & service
// alleys stay bare. Street-dressing Phase 2 — docs/design/roadkit.md.
static bool has_pavement(int k){ return k==K_ARTERIAL||k==K_ROAD||k==K_SECONDARY||k==K_TERTIARY; }
// carriageway fill colour from the OSM surface code (rways.surf): brick/klinker roads (most of a Dutch
// centre) read warm, unpaved reads brown, asphalt/concrete/unknown stay grey. (osm-roads.js roadSub.)
static int surf_col(signed char s){
  switch(s){ case 'p': return CLR_BROWN;       // paving_stones/sett/cobblestone — klinker
             case 'g': return CLR_DARK_BROWN;   // gravel/ground/sand — unpaved
             case 'w': return CLR_BROWN;        // wood (a plank bridge/boardwalk)
             default:  return CLR_DARK_GREY; }  // asphalt/concrete/unknown
}
// paint one way as quads + round-joint discs (radius rhw, colour col); near-camera segments only (r2 cull).
static void paint_way(int w, float rhw, int col, float r2){
  int st=rways[w].start, ct=rways[w].count;
  for (int i=0;i+1<ct;i++){
    float ax=PX[st+i], ay=PY[st+i], bx=PX[st+i+1], by=PY[st+i+1];
    if (seg_d2(ax,ay,bx,by,S.camx,S.camy) > r2) continue;   // keep any segment passing near (no pop)
    road_seg(ax,ay,bx,by,rhw,col);
    if (rhw >= 2.0f){ if (i==0) pdisc(ax,ay,rhw,col); pdisc(bx,by,rhw,col); }  // round joints → fuse
  }
}

// centre-line lane markings — a dashed white strip down a carriageway's centre. Cheap street-dressing
// (roadkit.md Phase 2): a 2D ground decal drawn OVER the asphalt, projected by the same adapter. Only
// roads wide enough to carry a lane division get one; narrow tracks/service alleys stay bare. Dashes
// run straight across junctions for now (markings don't stop at nodes yet) — a minor artifact the
// field renderer removes later.
static void paint_markings(int w, float r2){
  int st=rways[w].start, ct=rways[w].count;
  const float DASH=3.0f, MW=0.25f;                          // dash cell (m); marking half-width (m)
  float s=0;
  for(int i=0;i+1<ct;i++){
    float ax=PX[st+i],ay=PY[st+i],bx=PX[st+i+1],by=PY[st+i+1];
    float segL=sqrtf((bx-ax)*(bx-ax)+(by-ay)*(by-ay))+1e-4f;
    if (seg_d2(ax,ay,bx,by,S.camx,S.camy)<=r2)
      for(float d=0; d<segL; d+=DASH){
        if (((int)((s+d)/DASH)) & 1){ continue; }           // gap cell → dashed
        float d1=fminf(d+DASH,segL), t0=d/segL, t1=d1/segL;
        road_seg(ax+(bx-ax)*t0, ay+(by-ay)*t0, ax+(bx-ax)*t1, ay+(by-ay)*t1, MW, CLR_WHITE);
      }
    s+=segL;
  }
}

// ── junction detection (port of osm-junction.js) — build the at-grade node graph once at load. The
// transient node table is heap-scratch (freed here); only the compact junc[] result stays resident.
#define J_HASH  (1<<19)       // open-addressing buckets (power of 2) — load factor stays < 0.5 vs MAXNODE
#define MAXNODE 250000        // node capacity; beyond it we stop adding (graceful truncation)
typedef struct { int qx, qy; float x, y; unsigned char nraw; float raw[J_RAWARM]; unsigned char rawk[J_RAWARM]; } JNode;
static JNode *g_jn; static int *g_jhash; static int g_njn;   // scratch, live only inside build_junctions()
static int jn_find(int qx,int qy,float x,float y){
  unsigned h = ((unsigned)(qx*73856093) ^ (unsigned)(qy*19349663)) & (J_HASH-1);
  for(;;){
    int e=g_jhash[h];
    if(!e){ if(g_njn>=MAXNODE) return -1;
      g_jn[g_njn].qx=qx; g_jn[g_njn].qy=qy; g_jn[g_njn].x=x; g_jn[g_njn].y=y; g_jn[g_njn].nraw=0;
      g_jhash[h]=g_njn+1; return g_njn++; }
    if(g_jn[e-1].qx==qx && g_jn[e-1].qy==qy) return e-1;
    h=(h+1)&(J_HASH-1);
  }
}
static void jn_arm(float x,float y,float nx,float ny,int k){
  float dx=nx-x, dy=ny-y; if(dx==0.0f && dy==0.0f) return;
  int qx=(int)lroundf(x/J_QUANT), qy=(int)lroundf(y/J_QUANT);
  int i=jn_find(qx,qy,x,y); if(i<0) return;
  JNode*n=&g_jn[i]; if(n->nraw>=J_RAWARM) return;
  float brg=atan2f(dy,dx)*57.2957795f; if(brg<0) brg+=360.0f;
  n->raw[n->nraw]=brg; n->rawk[n->nraw]=(unsigned char)k; n->nraw++;
}
static void build_junctions(void){
  njunc=0;
  g_jn = (JNode*)calloc(MAXNODE, sizeof(JNode));
  g_jhash = (int*)calloc(J_HASH, sizeof(int));
  if(!g_jn || !g_jhash){ free(g_jn); free(g_jhash); g_jn=NULL; g_jhash=NULL; return; }
  g_njn=0;
  // every at-grade motor-road vertex contributes a bearing toward each along-way neighbour
  for(int w=0; w<nway; w++){ int k=rways[w].kind;
    if(!is_motor_road(k) || rways[w].deck!=0) continue;
    int st=rways[w].start, ct=rways[w].count;
    for(int i=0;i<ct;i++){ float x=PX[st+i], y=PY[st+i];
      if(i>0)    jn_arm(x,y, PX[st+i-1],PY[st+i-1], k);
      if(i<ct-1) jn_arm(x,y, PX[st+i+1],PY[st+i+1], k);
    }
  }
  // dedup each node's arms (collapse directions within DEDUP°, incl. wrap-around); >=3 distinct = junction
  const float DEDUP=8.0f;
  for(int i=0;i<g_njn && njunc<MAXJUNC;i++){
    JNode*n=&g_jn[i]; int m=n->nraw; if(m<3) continue;
    for(int a=1;a<m;a++){ float v=n->raw[a]; unsigned char vk=n->rawk[a]; int b=a-1;   // insertion sort by bearing
      while(b>=0 && n->raw[b]>v){ n->raw[b+1]=n->raw[b]; n->rawk[b+1]=n->rawk[b]; b--; }
      n->raw[b+1]=v; n->rawk[b+1]=vk; }
    float db[J_RAWARM]; unsigned char dk[J_RAWARM]; int nd=0;
    for(int a=0;a<m;a++){ if(nd==0 || fabsf(n->raw[a]-db[nd-1])>=DEDUP){ db[nd]=n->raw[a]; dk[nd]=n->rawk[a]; nd++; } }
    if(nd>1 && fabsf((db[0]+360.0f)-db[nd-1])<DEDUP) nd--;   // first & last wrap onto the same approach
    if(nd<3) continue;
    Junc*j=&junc[njunc++]; j->x=n->x; j->y=n->y; j->narm=(unsigned char)(nd>MAXARM?MAXARM:nd);
    for(int a=0;a<j->narm;a++){ j->brg[a]=db[a]; j->cls[a]=dk[a]; }
  }
  free(g_jn); free(g_jhash); g_jn=NULL; g_jhash=NULL;
}

// road priority rank — LOWER = bigger road = has voorrang. The Dutch "de grotere weg gaat voor":
// motorway > arterial > secondary > tertiary > road(residential/unclassified) > track > service.
static int road_rank(int k){
  switch(k){ case K_HIGHWAY:return 0; case K_ARTERIAL:return 1; case K_SECONDARY:return 2;
             case K_TERTIARY:return 3; case K_ROAD:return 4; case K_TRACK:return 5; case K_SERVICE:return 6; }
  return 7;
}
// one draped white triangle on the ground (world metres → screen via the same adapter as the roads).
static void ground_tri(float x0,float y0,float x1,float y1,float x2,float y2,int col){
  int xy[6];
  wpt(x0,y0,ground_z(x0,y0),&xy[0],&xy[1]);
  wpt(x1,y1,ground_z(x1,y1),&xy[2],&xy[3]);
  wpt(x2,y2,ground_z(x2,y2),&xy[4],&xy[5]);
  polyfill(xy,3,col);
}
// HAAIENTANDEN — the give-way row (B6). We know which road is bigger from the arm classes, so at a
// junction with a clear priority we paint a row of white shark's-teeth across each MINOR approach
// (apex pointing back at the yielding driver); nothing on the priority road. Ambiguous when every arm
// is the same class (rechts-voor-links / roundabout) → we paint nothing there. (roadkit.md Phase 2.)
static void draw_giveway(const Junc*j){
  float mx=j->x-S.camx, my=j->y-S.camy; if(mx*mx+my*my > RANGE_M*RANGE_M) return;
  int best=99, ntop=0; float maxhw=0;
  for(int a=0;a<j->narm;a++){ int r=road_rank(j->cls[a]); if(r<best) best=r;
    float h=ROAD[j->cls[a]].hw_m; if(h>maxhw) maxhw=h; }
  for(int a=0;a<j->narm;a++) if(road_rank(j->cls[a])==best) ntop++;
  if(ntop==j->narm) return;                            // all arms equal class → no clear voorrang, skip
  const float TH=1.1f, HB=0.35f, PITCH=1.1f;           // tooth height (along road) / half-base / spacing across (m)
  for(int a=0;a<j->narm;a++){ int k=j->cls[a];
    if(road_rank(k)==best) continue;                   // priority road → no teeth
    float b=j->brg[a], cb=cos_deg(b), sb=sin_deg(b);   // along = out along this arm
    float ax=-sb, ay=cb;                               // across the carriageway
    float off=maxhw + 1.0f;                            // give-way line, just outside the junction blob
    float hw=ROAD[k].hw_m; int nb=(int)(hw*2.0f/PITCH); if(nb<2) nb=2; if(nb>16) nb=16;
    for(int i=0;i<nb;i++){ float t=(i-(nb-1)*0.5f)*PITCH;   // centred across the road
      float bcx=j->x+cb*off, bcy=j->y+sb*off;              // base line at `off` from the node
      ground_tri(bcx+ax*(t-HB), bcy+ay*(t-HB),             // base-left
                 bcx+ax*(t+HB), bcy+ay*(t+HB),             // base-right
                 j->x+cb*(off+TH)+ax*t, j->y+sb*(off+TH)+ay*t,  // apex, farther out (points at the yielding driver)
                 CLR_WHITE);
    }
  }
}

// ── bridges (Phase 2): a bridge way (deck>0) renders as a RAISED deck — running surface + fascia sides,
// ramped up from grade at both ends over RAMP metres, subdivided so even a 2-node span humps smoothly.
// Height extrudes up-screen (project's z), so it reads as elevated in the 3D camera modes (flat in top-down).
// osm-roads carries the bridge tag; citydrive lifts it. (docs/design/external-data-carts.md — bridges Phase 2.)
#define DECK_H 3.0f     // lift per OSM layer (m). Delft's canal bridges are LOW — kept understated on purpose
#define DECK_T 1.2f     // deck slab thickness (fascia depth, m)
static void deck_quad(float ax,float ay,float za,float bx,float by,float zb,float hw,int col){
  float dx=bx-ax,dy=by-ay,L=sqrtf(dx*dx+dy*dy)+1e-4f; float px=-dy/L*hw,py=dx/L*hw;
  int xy[8]; wpt(ax+px,ay+py,za,&xy[0],&xy[1]); wpt(bx+px,by+py,zb,&xy[2],&xy[3]);
  wpt(bx-px,by-py,zb,&xy[4],&xy[5]); wpt(ax-px,ay-py,za,&xy[6],&xy[7]); polyfill(xy,4,col);
}
static void deck_fascia(float ax,float ay,float za,float bx,float by,float zb,float hw,int sd,int col){
  float dx=bx-ax,dy=by-ay,L=sqrtf(dx*dx+dy*dy)+1e-4f; float px=-dy/L*hw*sd,py=dx/L*hw*sd;
  int xy[8]; wpt(ax+px,ay+py,za,&xy[0],&xy[1]); wpt(bx+px,by+py,zb,&xy[2],&xy[3]);
  wpt(bx+px,by+py,zb-DECK_T,&xy[4],&xy[5]); wpt(ax+px,ay+py,za-DECK_T,&xy[6],&xy[7]); polyfill(xy,4,col);
}
static void draw_bridge_way(int w, float hw, int surfcol, float r2){
  int st=rways[w].start, ct=rways[w].count; if (ct<2) return;
  float lift = DECK_H * (rways[w].deck>0 ? rways[w].deck : 1);
  float Ltot=0; for(int i=0;i+1<ct;i++){ float dx=PX[st+i+1]-PX[st+i],dy=PY[st+i+1]-PY[st+i]; Ltot+=sqrtf(dx*dx+dy*dy); }
  if (Ltot<1e-3f) return;
  const int SUBD=3; const float RAMP=6.0f;
  float s=0;
  for(int i=0;i+1<ct;i++){
    float ax=PX[st+i],ay=PY[st+i],bx=PX[st+i+1],by=PY[st+i+1];
    float segL=sqrtf((bx-ax)*(bx-ax)+(by-ay)*(by-ay));
    if (seg_d2(ax,ay,bx,by,S.camx,S.camy)<=r2)
      for(int k=0;k<SUBD;k++){
        float t0=(float)k/SUBD, t1=(float)(k+1)/SUBD;
        float px0=ax+(bx-ax)*t0, py0=ay+(by-ay)*t0, px1=ax+(bx-ax)*t1, py1=ay+(by-ay)*t1;
        float s0=s+segL*t0, s1=s+segL*t1;
        float z0=lift*fminf(1.0f,fminf(s0,Ltot-s0)/RAMP), z1=lift*fminf(1.0f,fminf(s1,Ltot-s1)/RAMP);
        deck_quad(px0,py0,0,px1,py1,0,hw+0.5f,CLR_BROWNISH_BLACK);   // ground shadow → signals the deck is elevated
        deck_fascia(px0,py0,z0,px1,py1,z1,hw,+1,CLR_DARKER_GREY);
        deck_fascia(px0,py0,z0,px1,py1,z1,hw,-1,CLR_DARKER_GREY);
        deck_quad(px0,py0,z0,px1,py1,z1,hw,surfcol);
      }
    s+=segL;
  }
}

// a tunnel way (deck<0) goes UNDER the surface — draw it as a faint DASHED dark strip (thinner than the
// carriageway) so it reads as "the road continues underground here" rather than a normal surface road.
static void tunnel_way(int w, float hw, float r2){
  int st=rways[w].start, ct=rways[w].count;
  const float DASH=4.0f;                                   // metres per dash cell (on/off alternate)
  float s=0;
  for(int i=0;i+1<ct;i++){
    float ax=PX[st+i],ay=PY[st+i],bx=PX[st+i+1],by=PY[st+i+1];
    float segL=sqrtf((bx-ax)*(bx-ax)+(by-ay)*(by-ay))+1e-4f;
    if (seg_d2(ax,ay,bx,by,S.camx,S.camy)<=r2)
      for(float d=0; d<segL; d+=DASH){
        if (((int)((s+d)/DASH)) & 1) continue;             // gap cell
        float d1=fminf(d+DASH,segL), t0=d/segL, t1=d1/segL;
        road_seg(ax+(bx-ax)*t0, ay+(by-ay)*t0, ax+(bx-ax)*t1, ay+(by-ay)*t1, hw*0.55f, CLR_DARKER_GREY);
      }
    s+=segL;
  }
}

// ── lifecycle ────────────────────────────────────────────────────────────────
void init(void){ load_data(); }

void update(void) {
  const char *dropped = de_dropped_file();
  if (dropped) load_from(dropped);
  if (mouse_pressed(MOUSE_LEFT) && mouse_y() < 11 && mouse_x() >= SCREEN_W - 40){ de_open_path(DATA_DIR); return; }
  if (!loaded_ok) return;

  if (keyp('M')) S.mode = (S.mode+1) % M_COUNT;   // cycle the camera: north-up → heading-up → perspective
  if (keyp('1')) S.mode = M_NORTH;
  if (keyp('2')) S.mode = M_HEADING;
  if (keyp('3')) S.mode = M_PERSP;
  if (keyp('T')) S.tex = !S.tex;
  if (keyp('R')) S.roads_on = !S.roads_on;
  if (keyp('P')) S.show_pave = !S.show_pave;   // pavement/kerb bands
  if (keyp('L')) S.show_mark = !S.show_mark;   // lane centre-line markings
  if (keyp('G')) S.show_give = !S.show_give;   // give-way haaientanden
  if (keyp('V')) S.topdown = !S.topdown;          // flat 2D map to verify footprint vs road placement
  g_hscale = S.topdown ? 0.0f : HSCALE;
  if (S.mode==M_PERSP){                            // live-tune the pitched camera
    if (key(',')) S.pitch = fmaxf(5.0f,  S.pitch-1.0f);
    if (key('.')) S.pitch = fminf(80.0f, S.pitch+1.0f);
    if (key(';'))  S.eye = fmaxf(2.0f,   S.eye-1.0f);
    if (key('\'')) S.eye = fminf(120.0f, S.eye+1.0f);
  }
  if (key('[')) S.zoom = fmaxf(0.12f, S.zoom*0.97f);        // min low enough to pull back over a whole landscape
  if (key(']')) S.zoom = fminf(8.0f, S.zoom*1.03f);
  float wz = mouse_wheel();                                  // mouse wheel → zoom
  if (wz != 0) S.zoom = fmaxf(0.12f, fminf(8.0f, S.zoom * (wz>0 ? 1.12f : 1.0f/1.12f)));

  // left-drag → orbit the camera around the car (look from any side while parked)
  if (mouse_pressed(MOUSE_LEFT) && mouse_y() >= 11){ S.dragging = true; S.dragx = mouse_x(); }
  if (!mouse_down(MOUSE_LEFT)) S.dragging = false;
  if (S.dragging){ S.lookrot += (mouse_x() - S.dragx) * 0.6f; S.dragx = mouse_x(); }

  float thr = (key(KEY_UP)||key('W')?1:0) - (key(KEY_DOWN)||key('S')?1:0);
  float steer = (key(KEY_RIGHT)||key('D')?1:0) - (key(KEY_LEFT)||key('A')?1:0);
  S.spd += thr*0.06f; S.spd *= 0.95f;
  if (fabsf(S.spd) < 0.004f) S.spd = 0;
  S.ang += steer * (1.6f + fminf(fabsf(S.spd)*2.0f, 2.4f)) * (S.spd>=0?1:-1);
  S.px += cos_deg(S.ang)*S.spd;
  S.py += sin_deg(S.ang)*S.spd;
  S.camx += (S.px - S.camx)*0.18f;
  S.camy += (S.py - S.camy)*0.18f;
  S.camz += (ground_z(S.px,S.py) - S.camz)*0.12f;          // camera rises/sinks with the terrain under the car

  if (fabsf(S.spd) > 0.02f) S.lookrot *= 0.90f;                                 // driving → ease the orbit back to heading-up
  float base = (S.topdown || S.mode == M_NORTH) ? 0.0f : (-90.0f - S.ang);      // top-down locks north-up
  float target = base + S.lookrot;
  float d = fmodf(target - S.rot + 540.0f, 360.0f) - 180.0f;
  S.rot += d * 0.16f;
}

void draw(void) {
  cls(CLR_BROWNISH_BLACK);
  if (!loaded_ok){
    print(err[0]?err:"loading...", 6, SCREEN_H/2-4, CLR_LIGHT_GREY);
    return;
  }
  float R2 = RANGE_M*RANGE_M;

  draw_terrain();   // shaded hillside mesh — the base ground (no-op when flat / no DEM)

  // ground-area fills first (bottom layer): zoning → green → water, beneath everything.
  // areas are big, so cull generously by centroid; off-screen polys rasterise to nothing.
  {
    static const int LAYER[] = { K_FARM,K_RESIDENTIAL,K_COMMERCIAL,K_INDUSTRIAL,K_PARKING,K_SAND,K_GREEN,K_WATER };
    float AR2 = TERR_RANGE*TERR_RANGE;
    for (int li=0; li<8; li++){ int L=LAYER[li], col=AREA_COL[L];
      bool dith = hf_g && L!=K_WATER;        // on hilly maps, dither land-cover so the shaded terrain shows THROUGH
      if (dith) fillp(0xA5A5, -1);           // (forest/zoning over a mountain then reads the slope, not a flat patch)
      for (int a=0;a<narea;a++){ if (areas[a].kind!=L) continue;
        float mx=areas[a].cx-S.camx, my=areas[a].cy-S.camy;
        if (mx*mx+my*my > AR2) continue;
        if (dith){ int ax,ay; wpt(areas[a].cx,areas[a].cy,ground_z(areas[a].cx,areas[a].cy),&ax,&ay);
                   fillp_anchor(ax,ay); }   // pin the dither to this area → travels with it, no crawl
        area_fill(areas[a].start, areas[a].count, col);
      }
      if (dith){ fillp_reset(); fillp_anchor(0,0); }
    }
  }

  // flat roads (over the ground areas), near-camera only. Motor carriageways render as ONE connected
  // surface: a dark CASING pass (widened) then an ASPHALT pass (true width) → the network fuses at every
  // junction and each road gets a kerb edge, hierarchy reading by WIDTH not neon fill. Bike/foot/canal/
  // rail draw last in their own colour (separate networks). (docs/design/roadkit.md — connectivity Phase 0.)
  if (S.roads_on){
    const float CASE_M = 1.2f;                                   // kerb/casing width beyond the carriageway (m)
    const float PAVE_M = 1.6f;                                   // pavement band beyond the kerb (m), street-tier only
    // LOD: the fine dressing costs fill and turns to sub-pixel noise when zoomed out, so gate it on the
    // effective px-per-metre at the focus (flat modes = zoom directly; perspective ≈ zoom*90/setback near
    // the car). Pavement bands are wide → survive to a lower zoom than the thin centre-line markings.
    float eff = (S.mode==M_PERSP) ? S.zoom*90.0f/fmaxf(S.setback,1.0f) : S.zoom;
    int lod_pavement = eff >= 0.5f, lod_markings = eff >= 1.2f;
    // canals FIRST — linear WATER. Every road then draws OVER them, so a crossing reads as a (flat) bridge
    // instead of water painted over the road. We don't yet carry OSM bridge/layer, so road-over-water is
    // the sane default; raised decks are the bridge TODO (docs/design/external-data-carts.md · roadkit.md).
    for (int w=0; w<nway; w++) if (rways[w].kind==K_CANAL)
      paint_way(w, fmaxf(ROAD[K_CANAL].hw_m,1.0f), ROAD[K_CANAL].col, R2);
    // TUNNELS (deck<0) next — faint dashed strips, so a covered stretch reads as "goes underground"
    for (int w=0; w<nway; w++){ int k=rways[w].kind;
      if (is_road(k) && k!=K_CANAL && rways[w].deck<0) tunnel_way(w, fmaxf(ROAD[k].hw_m,1.5f), R2); }
    // PAVEMENT band first (widest) — a light kerb/sidewalk ring on urban street-tier roads; the casing
    // then asphalt paint OVER its inner part, leaving a pavement edge. Full sweep before any casing so a
    // crossing road's asphalt correctly covers the pavement at each junction.
    if (lod_pavement && S.show_pave) for (int w=0; w<nway; w++){ int k=rways[w].kind;
      if (rways[w].deck!=0) continue;
      char sw=rways[w].sw;
      int has = sw ? (sw!='n') : has_pavement(k);   // REAL OSM sidewalk if tagged, else fall back to class heuristic
      if (has) paint_way(w, fmaxf(ROAD[k].hw_m,1.5f)+CASE_M+PAVE_M, CLR_LIGHT_GREY, R2); }
    for (int w=0; w<nway; w++) if (is_motor_road(rways[w].kind) && rways[w].deck==0) // motor casing (at-grade only)
      paint_way(w, fmaxf(ROAD[rways[w].kind].hw_m,1.5f)+CASE_M, CLR_BROWNISH_BLACK, R2);
    for (int w=0; w<nway; w++) if (is_motor_road(rways[w].kind) && rways[w].deck==0) // motor surface — colour by OSM surface
      paint_way(w, fmaxf(ROAD[rways[w].kind].hw_m,1.5f), surf_col(rways[w].surf), R2);
    // MARKINGS last of the at-grade motor passes — dashed centre-line on carriageways wide enough to
    // carry a lane division (hw >= 2.5m: highway/arterial/road/secondary/tertiary, not tracks/service).
    if (lod_markings && S.show_mark) for (int w=0; w<nway; w++){ int k=rways[w].kind;
      if (is_motor_road(k) && rways[w].deck==0 && ROAD[k].hw_m>=2.5f) paint_markings(w, R2); }
    // HAAIENTANDEN — give-way teeth on the minor approach of each priority-controlled junction.
    if (lod_markings && S.show_give) for (int i=0;i<njunc;i++) draw_giveway(&junc[i]);
    // CYCLEWAYS — the Dutch red fietspad: a thin dark-red casing under a warm-red surface, so a bike path
    // reads as its own distinct ribbon alongside the roads. (Only SEPARATE cycleway ways; on-road bike lanes
    // — the `cycleway=lane` tag — are dropped by the importer; see roadkit.md's OSM-data note.)
    for (int w=0; w<nway; w++) if (rways[w].kind==K_CYCLEWAY && rways[w].deck==0){
      float chw=fmaxf(ROAD[K_CYCLEWAY].hw_m,1.2f);
      paint_way(w, chw+0.35f, CLR_DARK_RED,          R2);   // casing edge
      paint_way(w, chw,       ROAD[K_CYCLEWAY].col,  R2);   // red surface
    }
    for (int w=0; w<nway; w++){ int k=rways[w].kind;             // foot/rail — own colour (motor/cycleway/canal/tunnels/bridges elsewhere)
      if (!is_motor_road(k) && k!=K_CANAL && k!=K_CYCLEWAY && rways[w].deck==0) paint_way(w, fmaxf(ROAD[k].hw_m,1.0f), ROAD[k].col, R2); }
    // BRIDGES last — raised decks over the flat network. Surface = asphalt for motor roads, own colour
    // otherwise; a min deck width so thin footway/cycleway bridges still read as a raised span.
    for (int w=0; w<nway; w++) if (rways[w].deck>0){ int k=rways[w].kind; int motor=is_motor_road(k);
      draw_bridge_way(w, fmaxf(ROAD[k].hw_m, motor?1.5f:2.0f), motor?CLR_DARK_GREY:ROAD[k].col, R2); }
  }

  // collect near + on-screen buildings, depth-sort back-to-front, extrude
  static int idx[MAXDRAW]; static float dep[MAXDRAW]; int nd=0;
  for (int i=0;i<nbld && nd<MAXDRAW;i++){
    float mx=blds[i].cx-S.camx, my=blds[i].cy-S.camy;
    if (mx*mx+my*my > R2) continue;
    float sx,sy; project(blds[i].cx, blds[i].cy, ground_z(blds[i].cx,blds[i].cy), &sx, &sy);
    if (sx<-80||sx>SCREEN_W+80||sy<-160||sy>SCREEN_H+120) continue;
    idx[nd]=i; dep[nd]=sy; nd++;
  }
  for (int i=0;i<nd;i++) for (int j=i+1;j<nd;j++)
    if (dep[j]<dep[i]){ float t=dep[i];dep[i]=dep[j];dep[j]=t; int o=idx[i];idx[i]=idx[j];idx[j]=o; }
  for (int i=0;i<nd;i++){ DBldg*b=&blds[idx[i]]; draw_bldg(b->start,b->count,b->h,b->mat, ground_z(b->cx,b->cy)); }
#ifdef DE_TRACE
  watch("nbld","%d",nbld); watch("nd","%d",nd);   // buildings loaded / extruded this frame
  watch("njunc","%d",njunc);                       // intersections detected in the road graph
  { int npri=0; for(int i=0;i<njunc;i++){ int best=99,ntop=0;                 // junctions with a clear voorrang (get teeth)
      for(int a=0;a<junc[i].narm;a++){ int r=road_rank(junc[i].cls[a]); if(r<best) best=r; }
      for(int a=0;a<junc[i].narm;a++) if(road_rank(junc[i].cls[a])==best) ntop++;
      if(ntop<junc[i].narm) npri++; }
    watch("npri","%d",npri); }
  watch("camz","%.1f",S.camz); watch("gz","%.1f",ground_z(S.px,S.py));
  { int nbr=0, nbrNear=0; for(int w=0;w<nway;w++) if(rways[w].deck>0){ nbr++;
      float ax=PX[rways[w].start], ay=PY[rways[w].start];
      if ((ax-S.camx)*(ax-S.camx)+(ay-S.camy)*(ay-S.camy) < RANGE_M*RANGE_M) nbrNear++; }
    watch("nbridge","%d",nbr); watch("nbrNear","%d",nbrNear); }
#endif

  // car — sized in METRES so it scales with the roads (a fixed-px car dwarfs a 6m street);
  // a small floor keeps it visible when zoomed right out.
  float clen = fmaxf(4.5f*S.zoom, 5.0f), cwid = fmaxf(2.0f*S.zoom, 3.0f);   // ~4.5m × 2m
  fillp(QUARTER,-1); pdisc(S.px,S.py, 2.8f, CLR_BLACK); fillp_reset();      // shadow ~car-sized (world metres)
  int cxp,cyp; wpt(S.px,S.py,ground_z(S.px,S.py),&cxp,&cyp);   // car rides the terrain
  float sa = S.ang + S.rot;
  rectfill_rot(cxp, cyp, (int)clen, (int)cwid, sa, CLR_WHITE);
  rectfill_rot(cxp + (int)(cos_deg(sa)*clen*0.28f), cyp + (int)(sin_deg(sa)*clen*0.28f),
               (int)fmaxf(clen*0.4f,2), (int)cwid, sa, CLR_RED);

  // HUD — small font so the place name + count fit beside the OPEN button
  rectfill(0,0,SCREEN_W,9, CLR_BLACK);
  font(FONT_SMALL);
  print(dname[0]?dname:"citydrive", 3, 2, CLR_YELLOW);
  char hud[48]; snprintf(hud,sizeof hud,"%s%d bldg", S.topdown?"TOP ":(S.mode==M_PERSP?"PERSP ":""), nbld);
  print(hud, SCREEN_W-116, 2, CLR_LIGHT_GREY);
  if (S.mode==M_PERSP && !S.topdown){ char p[40]; snprintf(p,sizeof p,",.pitch=%d  ;'eye=%d",(int)S.pitch,(int)S.eye);
    print(p, 3, SCREEN_H-8, CLR_LIGHT_GREY); }
  int bx=SCREEN_W-40, hot=(mouse_y()<11 && mouse_x()>=bx);
  rectfill(bx,1,38,8, hot?CLR_BLUE:CLR_DARK_GREY);
  print("OPEN", bx+9,2, CLR_WHITE);
  font(FONT_NORMAL);
  if (truncated) print("(truncated)", 3, SCREEN_H-10, CLR_RED);
}
