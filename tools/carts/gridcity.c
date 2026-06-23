#include "studio.h"
#include <string.h>
#ifdef DE_SPEC
#include "spec.h"
#endif

// ── GRIDCITY — the data layer behind SimCity (v1, bare) ─────────────
// Not road geometry, not a generator — the AGGREGATE TILE-DATA SIM that
// SimCity 2000 runs behind its map overlays. Paint terrain, roads, and
// R/C/I zones; the engine then COMPUTES emergent layers from them and
// from each other, every sim tick. v1 proves the core feedback loop:
//
//     crime ↑  →  land value ↓  →  crime ↑      (the slum spiral)
//
// …broken only by POLICE coverage. Drop a station on a slum and watch
// crime fall and land value climb back.
//
// It's all two mechanisms (docs/design/data-layers.md):
//   1. cross-layer formulas — each field is f(the other fields)
//   2. smooth() — a 4-neighbour blur so influence SPREADS across tiles.
//      One primitive; police radius, water-value, density all reuse it.
//
//   LEFT-drag   paint the current brush      RIGHT-drag  erase to land
//   R C I       zone Residential/Commercial/Industrial
//   O           road        W  water        P  police station   E  erase
//   1..5        overlay: City / Land value / Crime / Police / Density
//   SPACE       reseed the sample city

#define GW 64
#define GH 48
#define MAXC (GW*GH)
#define TS 3            // pixels per cell
#define OX 4            // grid origin
#define OY 20
#define HUDX 202        // right panel

// cell kinds
enum { K_LAND, K_WATER, K_ROAD, K_R, K_C, K_I, K_POLICE };

// overlays
enum { V_CITY, V_LANDVAL, V_CRIME, V_POLICE, V_DENSITY, V_POLLUTION, V_COUNT };

// ── state ──
static int  kind[MAXC];
static int  landvalue[MAXC], crime[MAXC], police[MAXC], popden[MAXC], pollution[MAXC];
static int  terrainv[MAXC];   // static water-proximity value (recomputed on edit)
static int  roadacc[MAXC];    // static road-access field — a service radius, not strict adjacency
static int  tmp[MAXC];
static int  view = V_CITY;
static int  brush = K_R;
static long ticks = 0;        // sim ticks since reseed
static int  fcount = 0;

#define TICK_EVERY 8          // frames between sim ticks (slow enough to watch converge)

// tuning knobs (first-guess; see data-layers.md "constants are tuned")
#define CP_MAX   100          // land value from being near the city centre
#define CP_FALL    2
#define POP_BASE 200          // a developed cell's population contribution
#define CRIME_BASE 30

static inline int idx(int x, int y){ return y*GW + x; }
static inline int developed(int i){
    int k = kind[i];
    return (k==K_R||k==K_C||k==K_I);
}

// SERVICE-COVERAGE spread: each cell takes the strongest neighbour minus a
// per-step decay — a cheap distance falloff (strong at the source, fading with
// radius). This is the right primitive for a station's radius of effect; the
// averaging blur below is for DIFFUSION (pollution/density), which conserves
// mass and so can't give a point source a strong wide reach. radius ≈ src/decay.
static void spread(int *f, int passes, int decay){
    for(int p=0;p<passes;p++){
        for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
            int i=idx(x,y), m=f[i];
            if(x>0    && f[i-1]-decay>m)  m=f[i-1]-decay;
            if(x<GW-1 && f[i+1]-decay>m)  m=f[i+1]-decay;
            if(y>0    && f[i-GW]-decay>m) m=f[i-GW]-decay;
            if(y<GH-1 && f[i+GW]-decay>m) m=f[i+GW]-decay;
            tmp[i]=m;
        }
        memcpy(f, tmp, sizeof(int)*MAXC);
    }
}

// 4-neighbour box blur (include self), `passes` times. Influence spreads.
static void smooth(int *f, int passes){
    for(int p=0;p<passes;p++){
        for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
            int i = idx(x,y), sum = f[i], n = 1;
            if(x>0){ sum += f[i-1]; n++; }
            if(x<GW-1){ sum += f[i+1]; n++; }
            if(y>0){ sum += f[i-GW]; n++; }
            if(y<GH-1){ sum += f[i+GW]; n++; }
            tmp[i] = sum / n;
        }
        memcpy(f, tmp, sizeof(int)*MAXC);
    }
}

// recompute the static fields that change only on map edit, not every tick:
//   terrainv — near-water desirability (raises land value)
//   roadacc  — road access as a smoothed SERVICE RADIUS (a zone a couple
//              cells from a road still counts as served — far less brittle
//              than strict 4-neighbour adjacency)
static void recompute_static(void){
    for(int i=0;i<MAXC;i++) terrainv[i] = (kind[i]==K_WATER) ? 250 : 0;
    smooth(terrainv, 3);
    for(int i=0;i<MAXC;i++) roadacc[i]  = (kind[i]==K_ROAD)  ? 250 : 0;
    smooth(roadacc, 2);
}

// ── one simulation tick: the dependency DAG ──
static void sim_step(void){
    // city centre = centroid of zoned cells (fallback: grid centre)
    long sx=0, sy=0, nz=0;
    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++)
        if(developed(idx(x,y))){ sx+=x; sy+=y; nz++; }
    int cx = nz ? (int)(sx/nz) : GW/2;
    int cy = nz ? (int)(sy/nz) : GH/2;

    // 1. population density — developed + road-served cells emit population, then spread
    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
        int i=idx(x,y);
        popden[i] = (developed(i) && roadacc[i]>40) ? POP_BASE : 0;
    }
    smooth(popden, 2);

    // 2. police coverage — stations are the source; a decaying spread gives a
    //    strong radius (~250/18 ≈ 14 cells) that actually suppresses crime
    for(int i=0;i<MAXC;i++) police[i] = (kind[i]==K_POLICE) ? 250 : 0;
    spread(police, 14, 18);

    // 3. pollution — industry is the heavy emitter, commerce a little; it then
    //    diffuses onto neighbours (the wind-agnostic v1.5 model) and drags land value down
    for(int i=0;i<MAXC;i++)
        pollution[i] = kind[i]==K_I ? 220 : kind[i]==K_C ? 40 : 0;
    smooth(pollution, 3);

    // 4. land value — reads LAST tick's crime (so this tick's crime feeds the NEXT
    //    land value = the slum spiral). ↑centre-proximity ↑near-water ↓pollution ↓crime
    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
        int i=idx(x,y);
        if(kind[i]==K_WATER){ landvalue[i]=0; continue; }
        int dx=x-cx, dy=y-cy;
        int dist=(dx<0?-dx:dx)+(dy<0?-dy:dy);          // manhattan, cheap
        int prox=CP_MAX - dist*CP_FALL; if(prox<0) prox=0;
        int lv = prox + terrainv[i]/2 - pollution[i]/3 - crime[i]/2;
        if(lv<0) lv=0; if(lv>250) lv=250;
        landvalue[i]=lv;
    }

    // 5. crime — ↑population density ↓land value ↓police. only where people are.
    for(int i=0;i<MAXC;i++){
        if(popden[i]<=0){ crime[i]=0; continue; }
        int cr = CRIME_BASE + popden[i] - landvalue[i]/2 - police[i];
        if(cr<0) cr=0; if(cr>250) cr=250;
        crime[i]=cr;
    }
    ticks++;
}

// ── the seeded sample city: a town with one unpoliced slum corner ──
static void seed_city(void){
    memset(kind,0,sizeof kind);
    memset(landvalue,0,sizeof landvalue);
    memset(crime,0,sizeof crime);
    memset(police,0,sizeof police);
    memset(popden,0,sizeof popden);
    memset(pollution,0,sizeof pollution);

    // a lake along the top-left (raises nearby land value)
    for(int y=2;y<12;y++) for(int x=2;x<16;x++)
        if((x-9)*(x-9)+(y-7)*(y-7)*2 < 60) kind[idx(x,y)]=K_WATER;

    // a road grid
    for(int x=4;x<60;x++){ kind[idx(x,16)]=K_ROAD; kind[idx(x,30)]=K_ROAD; kind[idx(x,42)]=K_ROAD; }
    for(int y=16;y<43;y++){ kind[idx(18,y)]=K_ROAD; kind[idx(34,y)]=K_ROAD; kind[idx(50,y)]=K_ROAD; }

    // commercial core near the lake + centre, residential around, industry to the right
    for(int y=18;y<29;y++) for(int x=20;x<33;x++) if(kind[idx(x,y)]!=K_ROAD) kind[idx(x,y)]=K_C;
    for(int y=18;y<41;y++) for(int x=4;x<17;x++)  if(kind[idx(x,y)]==K_LAND) kind[idx(x,y)]=K_R;
    for(int y=18;y<29;y++) for(int x=36;x<49;x++) if(kind[idx(x,y)]!=K_ROAD) kind[idx(x,y)]=K_I;
    // the slum: a dense residential block far from centre, NO police
    for(int y=32;y<41;y++) for(int x=36;x<49;x++) if(kind[idx(x,y)]!=K_ROAD) kind[idx(x,y)]=K_R;
    for(int y=32;y<41;y++) for(int x=52;x<60;x++) if(kind[idx(x,y)]!=K_ROAD) kind[idx(x,y)]=K_R;

    recompute_static();
    ticks=0;
}

void init(void){ seed_city(); }

// ── painting ──
static void paint_at(int mx,int my,int k){
    int x=(mx-OX)/TS, y=(my-OY)/TS;
    if(x<0||x>=GW||y<0||y>=GH) return;
    int i=idx(x,y);
    if(kind[i]==k) return;
    kind[i]=k;
    if(k==K_WATER || /*was water*/ 1) recompute_static();  // cheap; keep terrain field fresh
}

void update(void){
    // brush select
    if(keyp('R')) brush=K_R;   if(keyp('C')) brush=K_C;   if(keyp('I')) brush=K_I;
    if(keyp('O')) brush=K_ROAD;if(keyp('W')) brush=K_WATER;if(keyp('P')) brush=K_POLICE;
    if(keyp('E')) brush=K_LAND;
    // overlay select
    if(keyp('1')) view=V_CITY;    if(keyp('2')) view=V_LANDVAL; if(keyp('3')) view=V_CRIME;
    if(keyp('4')) view=V_POLICE;  if(keyp('5')) view=V_DENSITY; if(keyp('6')) view=V_POLLUTION;
    if(keyp(KEY_SPACE)) seed_city();

    if(mouse_down(0)) paint_at(mouse_x(),mouse_y(),brush);
    if(mouse_down(1)) paint_at(mouse_x(),mouse_y(),K_LAND);

    if(++fcount>=TICK_EVERY){ fcount=0; sim_step(); }
}

// 6-step heat ramps (low→high), one per data overlay
static const int RAMP_LV[6] = {CLR_DARK_RED, CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_LIME_GREEN, CLR_GREEN};
static const int RAMP_CR[6] = {CLR_DARK_BLUE, CLR_DARK_PURPLE, CLR_DARK_RED, CLR_RED, CLR_ORANGE, CLR_YELLOW};
static const int RAMP_PO[6] = {CLR_BLACK, CLR_DARKER_BLUE, CLR_BLUE_GREEN, CLR_TRUE_BLUE, CLR_BLUE, CLR_WHITE};
static const int RAMP_PD[6] = {CLR_DARK_GREEN, CLR_MEDIUM_GREEN, CLR_GREEN, CLR_LIME_GREEN, CLR_YELLOW, CLR_WHITE};
static const int RAMP_PL[6] = {CLR_BLACK, CLR_DARKER_PURPLE, CLR_DARK_PURPLE, CLR_MAUVE, CLR_DARK_ORANGE, CLR_ORANGE};

static int ramp(const int *r,int v){ int s=v*6/251; if(s<0)s=0; if(s>5)s=5; return r[s]; }

static int city_color(int k){
    switch(k){
        case K_WATER:  return CLR_DARK_BLUE;
        case K_ROAD:   return CLR_DARK_GREY;
        case K_R:      return CLR_GREEN;
        case K_C:      return CLR_BLUE;
        case K_I:      return CLR_YELLOW;
        case K_POLICE: return CLR_WHITE;
        default:       return CLR_DARK_GREEN;   // land
    }
}

void draw(void){
    cls(CLR_BLACK);
    print("GRIDCITY", OX, 4, CLR_WHITE);
    print("the data layer behind the map", OX+72, 6, CLR_DARK_GREY);

    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
        int i=idx(x,y), col;
        if(view==V_CITY) col=city_color(kind[i]);
        else if(kind[i]==K_WATER) col=CLR_DARKER_BLUE;
        else switch(view){
            case V_LANDVAL: col=ramp(RAMP_LV,landvalue[i]); break;
            case V_CRIME:   col=(popden[i]<=0)?CLR_BROWNISH_BLACK:ramp(RAMP_CR,crime[i]); break;
            case V_POLICE:  col=ramp(RAMP_PO,police[i]); break;
            case V_DENSITY: col=(popden[i]<=0)?CLR_BROWNISH_BLACK:ramp(RAMP_PD,popden[i]); break;
            case V_POLLUTION: col=(pollution[i]<=4)?CLR_BROWNISH_BLACK:ramp(RAMP_PL,pollution[i]); break;
            default: col=CLR_BLACK;
        }
        // in data overlays, keep roads readable as a faint grid
        if(view!=V_CITY && kind[i]==K_ROAD) col=CLR_DARKER_GREY;
        rectfill(OX+x*TS, OY+y*TS, TS, TS, col);
    }

    // ── HUD ──
    static const char *VNAME[V_COUNT]={"1 CITY","2 LAND VALUE","3 CRIME","4 POLICE","5 DENSITY","6 POLLUTION"};
    static const char *BNAME[]={"land","water","road","R zone","C zone","I zone","POLICE"};
    int hy=22;
    print("OVERLAY", HUDX, hy, CLR_DARK_GREY); hy+=8;
    for(int v=0;v<V_COUNT;v++){ print(VNAME[v], HUDX, hy, v==view?CLR_WHITE:CLR_DARK_GREY); hy+=8; }
    hy+=4;
    print("BRUSH", HUDX, hy, CLR_DARK_GREY); hy+=8;
    print(str("> %s", BNAME[brush]), HUDX, hy, CLR_YELLOW); hy+=10;
    // legend ramp for the active data overlay
    if(view!=V_CITY){
        const int *r = view==V_LANDVAL?RAMP_LV : view==V_CRIME?RAMP_CR : view==V_POLICE?RAMP_PO
                     : view==V_POLLUTION?RAMP_PL : RAMP_PD;
        print("low", HUDX, hy, CLR_DARK_GREY);
        print("high", HUDX+86, hy, CLR_DARK_GREY); hy+=8;
        for(int s=0;s<6;s++) rectfill(HUDX+s*18, hy, 17, 8, r[s]);
        hy+=14;
    }
    print(str("ticks %ld", ticks), HUDX, hy, CLR_LIGHT_GREY);

    // bottom help
    print("RCI/O road/W water/P police/E erase  1-6 view  SPACE reseed",
          OX, OY+GH*TS+4, CLR_DARK_GREY);
}

#ifdef DE_SPEC
// helpers for the spec scenario
static int avg_crime_block(int x0,int y0,int x1,int y1){
    long s=0,n=0;
    for(int y=y0;y<y1;y++) for(int x=x0;x<x1;x++){ s+=crime[idx(x,y)]; n++; }
    return n? (int)(s/n):0;
}
static int avg_lv_block(int x0,int y0,int x1,int y1){
    long s=0,n=0;
    for(int y=y0;y<y1;y++) for(int x=x0;x<x1;x++){ s+=landvalue[idx(x,y)]; n++; }
    return n? (int)(s/n):0;
}
static int avg_pd_block(int x0,int y0,int x1,int y1){
    long s=0,n=0;
    for(int y=y0;y<y1;y++) for(int x=x0;x<x1;x++){ s+=popden[idx(x,y)]; n++; }
    return n? (int)(s/n):0;
}
static void clear_to_test_slum(void){
    memset(kind,0,sizeof kind);
    memset(landvalue,0,sizeof landvalue);
    memset(crime,0,sizeof crime);
    // a dense R block (x10..21, y10..21) with a road grid THROUGH it so the
    // whole block is road-served, far from centre, NO police
    for(int y=10;y<22;y++) for(int x=10;x<22;x++) kind[idx(x,y)]=K_R;
    for(int x=9;x<23;x++){ kind[idx(x,9)]=K_ROAD; kind[idx(x,15)]=K_ROAD; kind[idx(x,21)]=K_ROAD; }
    for(int y=9;y<22;y++){ kind[idx(9,y)]=K_ROAD; kind[idx(15,y)]=K_ROAD; kind[idx(21,y)]=K_ROAD; }
    recompute_static();
    ticks=0;
}

void spec(void){
    step(1);                 // trigger init() once, then take over the grid
    clear_to_test_slum();
    step(30*TICK_EVERY);     // let the loop converge (~30 ticks)

    int c1 = avg_crime_block(10,10,22,22);
    int l1 = avg_lv_block(10,10,22,22);
    int p1 = avg_pd_block(10,10,22,22);
    expect(1, str("DBG pre-police: popden=%d crime=%d landval=%d", p1, c1, l1));
    expect(c1 > 30, "an unpoliced dense block develops real crime");

    // drop a police station in the middle and let it settle
    kind[idx(15,15)]=K_POLICE;
    step(30*TICK_EVERY);
    int c2 = avg_crime_block(10,10,22,22);
    int l2 = avg_lv_block(10,10,22,22);
    expect(1, str("DBG post-police: crime=%d landval=%d", c2, l2));

    expect(c2 < c1, "police coverage lowers crime in the block");
    expect(l2 > l1, "lower crime lifts land value (the spiral, reversed)");

    // ── pollution: industry emits, it diffuses, and it drags land value down ──
    // a big R district fixes the city centre; we measure one cell, then drop a
    // small industrial block beside it (too small to move the centroid) and
    // confirm pollution reaches it and depresses its land value.
    memset(kind,0,sizeof kind);
    memset(crime,0,sizeof crime);
    for(int y=8;y<32;y++) for(int x=8;x<32;x++) kind[idx(x,y)]=K_R;   // 24x24 district
    for(int x=7;x<33;x++){ kind[idx(x,8)]=K_ROAD; kind[idx(x,20)]=K_ROAD; kind[idx(x,31)]=K_ROAD; }
    for(int y=8;y<32;y++){ kind[idx(8,y)]=K_ROAD; kind[idx(20,y)]=K_ROAD; kind[idx(31,y)]=K_ROAD; }
    recompute_static();
    step(20*TICK_EVERY);
    int lv_before   = landvalue[idx(15,15)];
    int poll_before = pollution[idx(15,15)];

    for(int y=14;y<18;y++) for(int x=16;x<20;x++) kind[idx(x,y)]=K_I; // industry next door
    recompute_static();
    step(20*TICK_EVERY);
    int poll_here = pollution[idx(17,15)];   // inside the industry
    int poll_at   = pollution[idx(15,15)];   // our measured cell, two off
    int lv_after  = landvalue[idx(15,15)];
    expect(1, str("DBG pollution: src=%d at-cell %d->%d   landval %d->%d",
                  poll_here, poll_before, poll_at, lv_before, lv_after));
    expect(poll_here > 100, "industry emits heavy pollution");
    expect(poll_at > poll_before, "pollution diffuses to nearby cells");
    expect(lv_after < lv_before, "pollution depresses nearby land value");
}
#endif
