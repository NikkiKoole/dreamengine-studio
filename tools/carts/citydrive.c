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
  "lineage": "The data-driven successor to cityview. cityview is a procedural render BENCH (seeded axis-aligned boxes) for the pseudo-3D city look; citydrive keeps its proven projection/camera/driving machinery but extrudes ARBITRARY POLYGON footprints from a REAL OpenStreetMap export (roadview's .rvb data, loaded at runtime), so you drive an actual city in pseudo-3D. Building heights ride along in the RVB2 format (OSM height/building:levels, with a per-footprint fallback where untagged). Sibling of roadview (same data, 2D top-down) — swap the file, see a different city.",
  "description": "Drive a REAL city in pseudo-3D. citydrive loads the SAME OpenStreetMap data as roadview (a .rvb fetched by data-tools/roadview/osm-roads.js) but instead of drawing it flat top-down, it EXTRUDES every building footprint straight up the screen — GTA1-meets-Zelda, the cityview look applied to real geometry. Footprints are arbitrary OSM polygons (not boxes), extruded with winding-based wall culling + polyfill roof caps; heights come from the RVB2 format (OSM height / building:levels tags, ~6% coverage, with a per-footprint fallback for the untagged majority). Roads draw as flat ground ribbons in the real road hierarchy beneath the buildings. The bounding box auto-fits and you spawn in the dense building mass (robust against the OSM bbox-balloon). Opens on data/demo.rvb; DRAG any .rvb/.json from the data folder onto the window to load that town, or OPEN reveals the folder. Arrows/WASD drive, M toggles north/heading-up camera, T textures, R toggles roads, [ ] zoom."
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
//   ◄▲▼► / WASD  drive · M north/heading · V top-down map · T textures · R roads · [ ] zoom · drop a file → load

#define HSCALE  1.0f        // metres of height → up-screen units (× zoom). taller = more dramatic 3D
                            // (a tall building's up-screen extrusion occludes the road BEHIND it — correct,
                            // verified via the V top-down map that footprints sit beside roads, not on them).
#define NMAT    6
#define TILE_W  4.0f        // world metres per 16px texture tile
#define QUARTER 0x7BDE      // ~25% black dither
#define MAXREP  6
#define MAXBV   64          // max footprint vertices we extrude (real OSM rings are mostly < 20)
#define RANGE_M 320.0f      // only extrude buildings / draw roads within this radius of the camera
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
    [K_CYCLEWAY]  = { CLR_DARK_RED,   1.2f },
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

enum { M_NORTH, M_HEADING, M_COUNT };
static const char *MODE_NAME[M_COUNT] = { "1 NORTH-UP", "2 HEADING-UP" };

// ── data pools (sized for a whole big city, like roadview) ───────────────────
#define MAXPTS  5000000
#define MAXBLD   300000
#define MAXWAY   300000
static float PX[MAXPTS], PY[MAXPTS];                 // shared point pool (local metres, Y north-up)
static int   nps;
typedef struct { int start, count; float h, cx, cy; int mat; } DBldg;   // building footprint ring
static DBldg blds[MAXBLD]; static int nbld;
static struct { int kind, start, count; } rways[MAXWAY]; static int nway;  // road lines (flat)

static char dname[64] = "";
static char err[160]  = "";
static int  loaded_ok = 0, truncated = 0;

typedef struct {
  float px, py, ang, spd;         // car: world pos (metres), heading(deg), speed
  float camx, camy, rot, zoom;    // camera: eased pos + rotation(deg) + px-per-metre
  int   mode;
  bool  started, tex, roads_on, topdown;
} State;
static State S;
static float g_hscale = HSCALE;   // 0 in top-down mode → buildings collapse to flat footprints (a 2D map to verify placement)

// ── projection (ADR 0021) — world (x,y,z metres) → screen, height straight up ─
static void project(float wx, float wy, float wz, float *sx, float *sy) {
  float dx = wx - S.camx, dy = wy - S.camy;
  float c = cos_deg(S.rot), s = sin_deg(S.rot);
  float rx = dx*c - dy*s, ry = dx*s + dy*c;
  *sx = SCREEN_W*0.5f + rx*S.zoom;
  *sy = SCREEN_H*0.5f + ry*S.zoom - wz*S.zoom*g_hscale;   // height up-screen (matches cityview's sign → forward drives forward)
}
static void wpt(float x,float y,float z,int*ix,int*iy){ float a,b; project(x,y,z,&a,&b); *ix=(int)a; *iy=(int)b; }

static void pproj_poly(const float*wx,const float*wy,int n,int color){
  int xy[2*MAXBV]; if(n>MAXBV) n=MAXBV;
  for(int i=0;i<n;i++) wpt(wx[i],wy[i],0,&xy[2*i],&xy[2*i+1]);
  polyfill(xy,n,color);
}
static void pdisc(float cx,float cy,float r,int color){
  enum { N=14 }; int xy[2*N];
  for(int i=0;i<N;i++){ float a=i*(360.0f/N); wpt(cx+cos_deg(a)*r, cy+sin_deg(a)*r, 0, &xy[2*i], &xy[2*i+1]); }
  polyfill(xy,N,color);
}

// ── data loading (twin of roadview's loader; reads RVB2 heights) ─────────────
static void reset_pools(void){ nps=0; nbld=0; nway=0; loaded_ok=0; truncated=0; dname[0]=0; err[0]=0; }

static int bld_mat(int start){ unsigned h=(unsigned)start*2654435761u; h^=h>>13; return (h>>8)%NMAT; }

// route one decoded feature into the building or road pools.
static void store_feature(int kind, int start, int count, float h){
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
    rways[nway].kind = kind; rways[nway].start = start; rways[nway].count = count; nway++;
  }
  // areas / trees / hashed understory: ignored for now (the flat ground is roads-only — see TODO)
}

// fast binary path (magic RVB1/RVB2) — RVB2 carries a float height per feature.
static void load_bin(const char *buf, long len){
  int ver = buf[3];
  const char *p = buf+4, *end = buf+len;
  int nfeat, namelen; memcpy(&nfeat,p,4); p+=4; memcpy(&namelen,p,4); p+=4;
  int nl=namelen; if(nl<0)nl=0; if(nl>(int)sizeof dname-1)nl=(int)sizeof dname-1;
  memcpy(dname,p,(size_t)nl); dname[nl]=0; p+=namelen;
  float bb[4]; memcpy(bb,p,16); p+=16;
  for(int f=0; f<nfeat && p+16<=end; f++){
    int kind, sublen, npts; float fh=0;
    memcpy(&kind,p,4); p+=4;
    if (ver=='2'){ memcpy(&fh,p,4); p+=4; }
    memcpy(&sublen,p,4); p+=4; p+=sublen;
    memcpy(&npts,p,4); p+=4;
    if (kind<0||kind>=K_N) kind=K_ROAD;
    int start=nps, got=0;
    for(int j=0;j<npts && p+8<=end;j++){ if(nps<MAXPTS){ float xy[2]; memcpy(xy,p,8); PX[nps]=xy[0]; PY[nps]=xy[1]; nps++; got++; } p+=8; }
    if (got>=2) store_feature(kind, start, got, fh);
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
      if (pi>=0 && tok[pi].type==JSMN_ARRAY){
        int cnt=tok[pi].size/2, start=nps, got=0;
        for (int j=0;j<cnt && nps<MAXPTS;j++){ PX[nps]=(float)json_num(js,&tok[pi+1+j*2]); PY[nps]=(float)json_num(js,&tok[pi+1+j*2+1]); nps++; got++; }
        if (got>=2) store_feature(kind, start, got, fh);
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
  if (len>=4 && memcmp(js,"RVB",3)==0 && (js[3]=='1'||js[3]=='2')) load_bin(js,len);
  else load_json(js, len);
  free(js);
  if (!nbld && !nway){ if(!err[0]) snprintf(err,sizeof err,"no features in %s",path); return; }
  if (nps>=MAXPTS || nbld>=MAXBLD || nway>=MAXWAY) truncated = 1;
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
  float gx = n ? (float)(sx/n) : 0, gy = n ? (float)(sy/n) : 0;   // mass centroid
  // snap to the building NEAREST the centroid so we start AMONG buildings, not in a gap
  float best=1e30f; S.px=gx; S.py=gy;
  for (int i=0;i<nbld;i++){ float dx=blds[i].cx-gx, dy=blds[i].cy-gy, d=dx*dx+dy*dy;
    if (d<best){ best=d; S.px=blds[i].cx; S.py=blds[i].cy; } }
  S.ang = -45; S.spd = 0;
  S.camx = S.px; S.camy = S.py;
  if (!S.started){ S.mode=M_HEADING; S.zoom=1.5f; S.tex=true; S.roads_on=true; }
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
    quadfill((int)ax,(int)ay,(int)bx,(int)by,(int)cbx,(int)cby,(int)cax,(int)cay, CLR_BLACK);
    fillp_reset();
  }
}

// ── extrude one real polygon footprint (the key technique) ───────────────────
static void draw_bldg(int start, int count, float h, int mat){
  int nv = count; if (nv > MAXBV) nv = MAXBV;
  float gx[MAXBV], gy[MAXBV], rx[MAXBV], ry[MAXBV];
  for (int i=0;i<nv;i++){
    project(PX[start+i], PY[start+i], 0, &gx[i], &gy[i]);
    project(PX[start+i], PY[start+i], h, &rx[i], &ry[i]);
  }
  // winding sign → consistent outward normals (concave-safe)
  float area=0; for(int i=0;i<nv;i++){ int j=(i+1)%nv; area += PX[start+i]*PY[start+j]-PX[start+j]*PY[start+i]; }
  float wind = area>0 ? 1.0f : -1.0f;
  float c = cos_deg(S.rot), s = sin_deg(S.rot);

  int order[MAXBV]; float depth[MAXBV], facing[MAXBV]; int n=0;
  for (int e=0;e<nv;e++){
    int a=e, bb=(e+1)%nv;
    float ex=PX[start+bb]-PX[start+a], ey=PY[start+bb]-PY[start+a];
    float L=sqrtf(ex*ex+ey*ey)+1e-4f;
    float nx=(ey/L)*wind, ny=-(ex/L)*wind;
    float nsy = nx*s + ny*c;                     // screen-y of the outward normal; >0 = faces the viewer (down-screen)
    if (nsy > 0.001f){ order[n]=e; facing[n]=nsy; depth[n]=(gy[a]+gy[bb])*0.5f; n++; }
  }
  for(int i=0;i<n;i++) for(int j=i+1;j<n;j++)
    if(depth[j]<depth[i]){ float t=depth[i];depth[i]=depth[j];depth[j]=t;
      t=facing[i];facing[i]=facing[j];facing[j]=t; int o=order[i];order[i]=order[j];order[j]=o; }

  for(int k=0;k<n;k++){
    int e=order[k], a=e, bb=(e+1)%nv;
    if (S.tex){
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
  int rxy[2*MAXBV]; for(int i=0;i<nv;i++){ rxy[2*i]=(int)rx[i]; rxy[2*i+1]=(int)ry[i]; }
  polyfill(rxy, nv, MAT[mat].roof);
}

// squared distance from point (px,py) to segment a→b — for a pop-free cull that
// keeps any segment passing near the camera, even a long one whose ENDS are far.
static float seg_d2(float ax,float ay,float bx,float by,float px,float py){
  float dx=bx-ax, dy=by-ay, L2=dx*dx+dy*dy;
  float t = L2>0 ? ((px-ax)*dx+(py-ay)*dy)/L2 : 0;
  if (t<0) t=0; else if (t>1) t=1;
  float qx=ax+t*dx-px, qy=ay+t*dy-py; return qx*qx+qy*qy;
}

// ── flat ground: one road segment as a world-space quad ribbon ───────────────
static void road_seg(float ax,float ay,float bx,float by,float hw,int col){
  float dx=bx-ax, dy=by-ay, L=sqrtf(dx*dx+dy*dy)+1e-4f;
  float px=-dy/L*hw, py=dx/L*hw;
  float wx[4]={ax+px,bx+px,bx-px,ax-px}, wy[4]={ay+py,by+py,by-py,ay-py};
  pproj_poly(wx,wy,4,col);
}

// ── lifecycle ────────────────────────────────────────────────────────────────
void init(void){ load_data(); }

void update(void) {
  const char *dropped = de_dropped_file();
  if (dropped) load_from(dropped);
  if (mouse_pressed(MOUSE_LEFT) && mouse_y() < 11 && mouse_x() >= SCREEN_W - 40){ de_open_path(DATA_DIR); return; }
  if (!loaded_ok) return;

  if (keyp('M')) S.mode = (S.mode+1) % M_COUNT;
  if (keyp('1')) S.mode = M_NORTH;
  if (keyp('2')) S.mode = M_HEADING;
  if (keyp('T')) S.tex = !S.tex;
  if (keyp('R')) S.roads_on = !S.roads_on;
  if (keyp('V')) S.topdown = !S.topdown;          // flat 2D map to verify footprint vs road placement
  g_hscale = S.topdown ? 0.0f : HSCALE;
  if (key('[')) S.zoom = fmaxf(0.7f, S.zoom*0.97f);
  if (key(']')) S.zoom = fminf(8.0f, S.zoom*1.03f);

  float thr = (key(KEY_UP)||key('W')?1:0) - (key(KEY_DOWN)||key('S')?1:0);
  float steer = (key(KEY_RIGHT)||key('D')?1:0) - (key(KEY_LEFT)||key('A')?1:0);
  S.spd += thr*0.06f; S.spd *= 0.95f;
  if (fabsf(S.spd) < 0.004f) S.spd = 0;
  S.ang += steer * (1.6f + fminf(fabsf(S.spd)*2.0f, 2.4f)) * (S.spd>=0?1:-1);
  S.px += cos_deg(S.ang)*S.spd;
  S.py += sin_deg(S.ang)*S.spd;
  S.camx += (S.px - S.camx)*0.18f;
  S.camy += (S.py - S.camy)*0.18f;

  float target = (S.topdown || S.mode == M_NORTH) ? 0.0f : (-90.0f - S.ang);   // top-down locks north-up
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

  // flat roads first (ground layer) — only segments near the camera
  if (S.roads_on){
    for (int w=0; w<nway; w++){
      int k=rways[w].kind, st=rways[w].start, ct=rways[w].count;
      float hw=ROAD[k].hw_m; int col=ROAD[k].col;
      float rhw = fmaxf(hw, 1.5f);
      for (int i=0;i+1<ct;i++){
        float ax=PX[st+i], ay=PY[st+i], bx=PX[st+i+1], by=PY[st+i+1];
        if (seg_d2(ax,ay,bx,by,S.camx,S.camy) > R2) continue;   // keep any segment passing near (no pop)
        road_seg(ax,ay,bx,by,rhw,col);
        if (rhw >= 2.0f){                                       // round the joints so segments fuse, not read as separate rects
          if (i==0) pdisc(ax,ay,rhw,col);
          pdisc(bx,by,rhw,col);
        }
      }
    }
  }

  // collect near + on-screen buildings, depth-sort back-to-front, extrude
  static int idx[MAXDRAW]; static float dep[MAXDRAW]; int nd=0;
  for (int i=0;i<nbld && nd<MAXDRAW;i++){
    float mx=blds[i].cx-S.camx, my=blds[i].cy-S.camy;
    if (mx*mx+my*my > R2) continue;
    float sx,sy; project(blds[i].cx, blds[i].cy, 0, &sx, &sy);
    if (sx<-80||sx>SCREEN_W+80||sy<-160||sy>SCREEN_H+120) continue;
    idx[nd]=i; dep[nd]=sy; nd++;
  }
  for (int i=0;i<nd;i++) for (int j=i+1;j<nd;j++)
    if (dep[j]<dep[i]){ float t=dep[i];dep[i]=dep[j];dep[j]=t; int o=idx[i];idx[i]=idx[j];idx[j]=o; }
  for (int i=0;i<nd;i++){ DBldg*b=&blds[idx[i]]; draw_bldg(b->start,b->count,b->h,b->mat); }
#ifdef DE_TRACE
  watch("nbld","%d",nbld); watch("nd","%d",nd);   // buildings loaded / extruded this frame
#endif

  // car — ground shadow then body
  fillp(QUARTER,-1); pdisc(S.px,S.py,4.0f,CLR_BLACK); fillp_reset();
  int cxp,cyp; wpt(S.px,S.py,0,&cxp,&cyp);
  float sa = S.ang + S.rot;
  rectfill_rot(cxp, cyp, 10, 6, sa, CLR_WHITE);
  rectfill_rot(cxp + (int)(cos_deg(sa)*3), cyp + (int)(sin_deg(sa)*3), 4, 5, sa, CLR_RED);

  // HUD — small font so the place name + count fit beside the OPEN button
  rectfill(0,0,SCREEN_W,9, CLR_BLACK);
  font(FONT_SMALL);
  print(dname[0]?dname:"citydrive", 3, 2, CLR_YELLOW);
  char hud[48]; snprintf(hud,sizeof hud,"%s%d bldg", S.topdown?"TOPDOWN  ":"", nbld);
  print(hud, SCREEN_W-116, 2, CLR_LIGHT_GREY);
  int bx=SCREEN_W-40, hot=(mouse_y()<11 && mouse_x()>=bx);
  rectfill(bx,1,38,8, hot?CLR_BLUE:CLR_DARK_GREY);
  print("OPEN", bx+9,2, CLR_WHITE);
  font(FONT_NORMAL);
  if (truncated) print("(truncated)", 3, SCREEN_H-10, CLR_RED);
}
