#include "studio.h"
#include <string.h>
#ifdef DE_SPEC
#include "spec.h"
#endif

// ── GRIDCITY — the data layer behind SimCity (v1.5) ─────────────────
// Not road geometry, not a generator — the AGGREGATE TILE-DATA SIM that
// SimCity 2000 runs behind its map overlays. Paint terrain, roads, and
// R/C/I zones; the engine then COMPUTES emergent layers from them and
// from each other, every sim tick. The core feedback loop:
//
//     crime ↑  →  land value ↓  →  crime ↑      (the slum spiral)
//
// …broken only by POLICE coverage. Drop a station on a slum and watch
// crime fall and land value climb back.
//
// It's all cross-layer formulas + two spreading primitives (data-layers.md):
//   • smooth()  — a 4-neighbour blur (DIFFUSION): pollution, density, water-value
//   • spread()  — a decaying distance falloff: service coverage (police radius)
//
// v1.5 adds the LIVING half: zones aren't instantly full — each carries a
// development LEVEL (empty lot → built up) that GROWS or DECLINES every few
// ticks based on local land value / crime / pollution / road access AND a
// global RCI DEMAND meter (R wants jobs, C wants residents, I wants workers).
// Paint a zone and it fills in on its own; choke it with pollution or crime
// and it empties out. The city you open is pre-grown from these rules.
//
// Service buildings (police/school/hospital) spread a coverage radius; police cuts
// crime, schools+hospitals raise land value and drive the EQ / LE city stats. Parks
// and nearby commerce push land value up too — so a well-served, park-adjacent
// downtown can finally reach the high end (centre-proximity alone capped it before).
//
//   LEFT-drag   paint the current brush      RIGHT-drag  erase to land
//   R C I       zone Residential/Commercial/Industrial (D toggles light↔dense zoning:
//               light caps low = suburbs, dense rises tall = downtown)
//   O road  W water  P police  G park  S school  H hospital  E erase
//   1..7        overlay: City / Land value / Crime / Police / Density / Pollution / Services
//   SPACE       reseed the sample city

#define GW 64
#define GH 48
#define MAXC (GW*GH)
#define TS 3            // pixels per cell
#define OX 4            // grid origin
#define OY 20
#define HUDX 202        // right panel

// cell kinds
enum { K_LAND, K_WATER, K_ROAD, K_R, K_C, K_I, K_POLICE, K_PARK, K_SCHOOL, K_HOSPITAL };

// overlays
enum { V_CITY, V_LANDVAL, V_CRIME, V_POLICE, V_DENSITY, V_POLLUTION, V_SERVICES, V_COUNT };

// ── state ──
static int  kind[MAXC];
static int  level[MAXC];      // development level of a zoned cell: 0 = empty lot … LMAX = full
static int  zdense[MAXC];     // zoning density: 0 = light (caps low, suburbs), 1 = dense (allows tall)
static int  landvalue[MAXC], crime[MAXC], police[MAXC], popden[MAXC], pollution[MAXC];
static int  school[MAXC], health[MAXC];   // education / health coverage (spread from schools/hospitals)
static int  terrainv[MAXC];   // static water-proximity value (recomputed on edit)
static int  parkv[MAXC];      // static park-amenity value (recomputed on edit)
static int  commv[MAXC];      // commercial-vitality premium (downtown raises nearby land value)
static int  roadacc[MAXC];    // static road-access field — a service radius, not strict adjacency
static int  tmp[MAXC];
static int  view = V_CITY;
static int  brush = K_R;
static int  brush_dense = 0;  // paint zones as dense (D toggles) — only meaningful for R/C/I
static long ticks = 0;        // sim ticks since reseed
static int  fcount = 0;
static int  demR, demC, demI; // global RCI demand meter (drives growth/decline)
static int  eduQ, lifeE;      // city aggregates: Education Quotient (0..100), Life Expectancy (years)
static int  growth_on = 1;    // gate the develop/abandon pass (spec freezes it for formula tests)

#define TICK_EVERY 8          // frames between sim ticks (slow enough to watch converge)

// tuning knobs (first-guess; see data-layers.md "constants are tuned")
#define CP_MAX   100          // land value from being near the city centre
#define CP_FALL    1          // gentle falloff so the whole map is buildable (crime/pollution make the variation)
#define CRIME_BASE 30
#define LMAX        4         // max development level (dense zoning)
#define LIGHT_CAP   2         // light zoning tops out here (suburbs / low-rise)
#define GROW_EVERY  4         // sim ticks between growth/decline passes
#define GROW_THRESH 30        // desirability a lot needs (above) to build up
#define DECL_THRESH 10        // …and (below) to start emptying out
#define DEM_DEAD    10        // demand dead band: near balance the city HOLDS (anti-oscillation)
#define R_BASE     30         // baseline residential pull (people want in)
#define C_BASE     20         // baseline commercial pull
#define I_BASE     20         // baseline industrial pull (external market)

static inline int idx(int x, int y){ return y*GW + x; }
static inline int is_zone(int k){ return k==K_R||k==K_C||k==K_I; }
static inline int developed(int i){ return is_zone(kind[i]) && level[i]>0; }

// positive land-value drivers (this phase): parks + service coverage + downtown premium
#define PARK_SRC   220        // a park's land-value boost at the tile (falls off ~8 cells)
#define COMM_PREM   40        // per-level commercial vitality emitted to neighbours

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
    smooth(roadacc, 3);
    // parks: a strong, fairly LOCAL land-value boost (decaying spread, ~8-cell reach)
    for(int i=0;i<MAXC;i++) parkv[i] = (kind[i]==K_PARK) ? PARK_SRC : 0;
    spread(parkv, 8, 28);
}

// ── one simulation tick: the dependency DAG ──
static void sim_step(void){
    // city centre = centroid of zoned cells (fallback: grid centre)
    long sx=0, sy=0, nz=0;
    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++)
        if(developed(idx(x,y))){ sx+=x; sy+=y; nz++; }
    int cx = nz ? (int)(sx/nz) : GW/2;
    int cy = nz ? (int)(sy/nz) : GH/2;

    // 1. population density — a built, road-served zone emits people scaled by its
    //    development level (R/C bustle most, I a bit less), then it spreads
    for(int i=0;i<MAXC;i++){
        int k=kind[i], per = (k==K_R||k==K_C)?55 : k==K_I?35 : 0;
        popden[i] = (level[i]>0 && roadacc[i]>40) ? per*level[i] : 0;
    }
    smooth(popden, 2);

    // 2. service coverage — each civic building is a source spread into a radius (the
    //    same falloff police uses). Police suppresses crime; school/health raise land
    //    value and drive the EQ/LE city stats.
    for(int i=0;i<MAXC;i++) police[i] = (kind[i]==K_POLICE)   ? 250 : 0;
    for(int i=0;i<MAXC;i++) school[i] = (kind[i]==K_SCHOOL)   ? 250 : 0;
    for(int i=0;i<MAXC;i++) health[i] = (kind[i]==K_HOSPITAL) ? 250 : 0;
    spread(police, 14, 18);
    spread(school, 16, 16);
    spread(health, 16, 16);
    // commercial vitality — built downtown raises nearby land value (the shops-nearby premium)
    for(int i=0;i<MAXC;i++) commv[i] = (kind[i]==K_C) ? COMM_PREM*level[i] : 0;
    smooth(commv, 3);

    // 3. pollution — industry is the heavy emitter, commerce a little (both scaled by
    //    level); it then diffuses onto neighbours (wind-agnostic v1.5) and drags land value down
    for(int i=0;i<MAXC;i++)
        pollution[i] = kind[i]==K_I ? 55*level[i] : kind[i]==K_C ? 10*level[i] : 0;
    smooth(pollution, 3);

    // 4. land value — reads LAST tick's crime (so this tick's crime feeds the NEXT
    //    land value = the slum spiral). Positive drivers (this phase) let it climb high:
    //    ↑centre-proximity ↑near-water ↑parks ↑service coverage ↑near-commerce
    //    ↓pollution ↓crime
    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
        int i=idx(x,y);
        if(kind[i]==K_WATER){ landvalue[i]=0; continue; }
        int dx=x-cx, dy=y-cy;
        int dist=(dx<0?-dx:dx)+(dy<0?-dy:dy);          // manhattan, cheap
        int prox=CP_MAX - dist*CP_FALL; if(prox<0) prox=0;
        int amenity = (police[i]+school[i]+health[i])/8;   // services make an area desirable
        int lv = prox + terrainv[i]/2 + parkv[i] + amenity + commv[i]
                 - pollution[i]/3 - crime[i]/2;
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

    // 6. RCI demand — the global meter. Supply = built levels; the city pulls each
    //    zone type toward balance: R wants jobs, C wants residents, I wants workers.
    long totR=0,totC=0,totI=0;
    for(int i=0;i<MAXC;i++){
        if(kind[i]==K_R) totR+=level[i];
        else if(kind[i]==K_C) totC+=level[i];
        else if(kind[i]==K_I) totI+=level[i];
    }
    long pop=totR, jobs=totC+totI;
    // gentle balance: a roughly-balanced city sits mildly POSITIVE (so it grows and
    // holds); demand only goes negative under real oversupply. WHICH lots build up is
    // then decided locally by land value / crime / pollution, not by demand whiplash.
    demR = R_BASE + (int)(jobs - pop)/6;       // homes wanted to fill the jobs
    demC = C_BASE + (int)(pop - 2*totC)/6;     // shops wanted per resident
    demI = I_BASE + (int)(pop - 2*totI)/6;     // factories: external base + workforce

    // city stats — Education Quotient + Life Expectancy, averaged over where people live
    long edu=0, hea=0, ncov=0;
    for(int i=0;i<MAXC;i++) if(popden[i]>0){ edu+=school[i]; hea+=health[i]; ncov++; }
    eduQ  = ncov ? (int)(edu/ncov)*100/250 : 0;        // 0..100
    lifeE = 60 + (ncov ? (int)(hea/ncov)*30/250 : 0);  // ~60..90 years

    // 7. the develop/abandon valve — every GROW_EVERY ticks each zoned lot nudges its
    //    level up (good local desirability AND its type clearly in demand) or down
    //    (starved of road access, choked by crime/pollution, or clearly oversupplied).
    //    Two anti-oscillation guards so the city SETTLES instead of pulsing the whole
    //    grid between two states: (a) a demand DEAD BAND — within ±DEM_DEAD the city
    //    holds; (b) STAGGER — only ~a quarter of lots update each pass (rotating phase),
    //    so supply changes in small steps and demand can't whiplash past the dead band.
    if(growth_on && ticks % GROW_EVERY == 0){
        int phase = (int)(ticks / GROW_EVERY) & 3;
        for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
            int i=idx(x,y);
            if(!is_zone(kind[i])) continue;
            if(((x*3 + y*5) & 3) != phase) continue;       // this pass touches one quarter
            int dem = kind[i]==K_R?demR : kind[i]==K_C?demC : demI;
            int des = landvalue[i] - crime[i]/2 - pollution[i]/4;
            int served = roadacc[i]>40;
            int cap = zdense[i] ? LMAX : LIGHT_CAP;   // light zoning tops out low
            if(served && dem>DEM_DEAD && des>GROW_THRESH){ if(level[i]<cap) level[i]++; }
            else if(!served || dem<-DEM_DEAD || des<DECL_THRESH){ if(level[i]>0) level[i]--; }
        }
    }
    ticks++;
}

// ── the seeded sample city: a town with one unpoliced slum corner ──
static void seed_city(void){
    memset(kind,0,sizeof kind);
    memset(level,0,sizeof level);
    memset(zdense,0,sizeof zdense);
    memset(landvalue,0,sizeof landvalue);
    memset(crime,0,sizeof crime);
    memset(police,0,sizeof police);
    memset(popden,0,sizeof popden);
    memset(pollution,0,sizeof pollution);

    // a lake along the top-left (raises nearby land value)
    for(int y=2;y<13;y++) for(int x=2;x<17;x++)
        if((x-9)*(x-9)+(y-7)*(y-7)*2 < 70) kind[idx(x,y)]=K_WATER;

    // a DENSE street grid (every 6 cells) so every block is road-served — zones a
    // couple cells from a street still develop
    for(int x=6;x<=58;x+=6) for(int y=6;y<46;y++) if(kind[idx(x,y)]==K_LAND) kind[idx(x,y)]=K_ROAD;
    for(int y=6;y<46;y+=6) for(int x=4;x<60;x++) if(kind[idx(x,y)]==K_LAND) kind[idx(x,y)]=K_ROAD;

    // zone the blocks by region: commercial core in the centre, industry top-right,
    // residential everywhere else. Levels start at 0 — the growth valve fills them in.
    for(int y=6;y<46;y++) for(int x=4;x<60;x++){
        int i=idx(x,y);
        if(kind[i]!=K_LAND) continue;                 // skip roads + water
        if(x>=22 && x<=40 && y>=14 && y<=30)      kind[i]=K_C;   // downtown
        else if(x>=44 && y<=28)                    kind[i]=K_I;  // industrial quarter
        else                                       kind[i]=K_R;  // residential
        // dense zoning in the inner city (rises tall); light suburbs on the outskirts
        int dx=x-32, dy=y-24;
        zdense[i] = ((dx<0?-dx:dx) <= 14 && (dy<0?-dy:dy) <= 11);
    }

    // civic buildings — parks lift value, schools/hospitals add coverage + EQ/LE,
    // a couple of police cover downtown (the outskirts stay a little rougher)
    kind[idx(33,21)]=K_PARK;   kind[idx(15,33)]=K_PARK;
    kind[idx(45,33)]=K_SCHOOL; kind[idx(21,15)]=K_SCHOOL;
    kind[idx(21,33)]=K_HOSPITAL; kind[idx(45,15)]=K_HOSPITAL;
    kind[idx(27,21)]=K_POLICE; kind[idx(39,27)]=K_POLICE;

    recompute_static();
    // pre-warm: let the zones grow from empty to a settled city before the player
    // ever sees it (so we open on a living town, not a grid of empty lots)
    growth_on=1;
    for(int i=0;i<80;i++) sim_step();
    ticks=0;
}

void init(void){ seed_city(); }

// ── painting ──
static void paint_at(int mx,int my,int k){
    int x=(mx-OX)/TS, y=(my-OY)/TS;
    if(x<0||x>=GW||y<0||y>=GH) return;
    int i=idx(x,y);
    int redense = is_zone(k) && kind[i]==k && zdense[i]!=brush_dense;  // re-zone same tile light↔dense
    if(kind[i]==k && !redense) return;
    kind[i]=k;
    level[i]=0;                              // a freshly painted lot starts empty; it grows on its own
    zdense[i] = is_zone(k) ? brush_dense : 0;
    recompute_static();    // cheap; keep terrain + road-access fields fresh
}

void update(void){
    // brush select
    if(keyp('R')) brush=K_R;   if(keyp('C')) brush=K_C;   if(keyp('I')) brush=K_I;
    if(keyp('O')) brush=K_ROAD;if(keyp('W')) brush=K_WATER;if(keyp('P')) brush=K_POLICE;
    if(keyp('G')) brush=K_PARK;if(keyp('S')) brush=K_SCHOOL;if(keyp('H')) brush=K_HOSPITAL;
    if(keyp('E')) brush=K_LAND;
    if(keyp('D')) brush_dense=!brush_dense;   // toggle light/dense zoning for the zone brushes
    // overlay select
    if(keyp('1')) view=V_CITY;    if(keyp('2')) view=V_LANDVAL; if(keyp('3')) view=V_CRIME;
    if(keyp('4')) view=V_POLICE;  if(keyp('5')) view=V_DENSITY; if(keyp('6')) view=V_POLLUTION;
    if(keyp('7')) view=V_SERVICES;
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
static const int RAMP_SV[6] = {CLR_BLACK, CLR_DARKER_GREY, CLR_BLUE_GREEN, CLR_MEDIUM_GREEN, CLR_LIME_GREEN, CLR_WHITE};

static int ramp(const int *r,int v){ int s=v*6/251; if(s<0)s=0; if(s>5)s=5; return r[s]; }

// a zone cell's colour in city view: empty lot is dim; a built lot brightens with its
// development level, so you can read the city's "height" (and dense vs light) at a glance
static int zone_shade(int k, int lv){
    if(lv<=0) return CLR_DARKER_GREY;            // empty lot
    int hi = lv>=3;                              // tall (only dense zoning reaches here)
    switch(k){
        case K_R: return hi?CLR_GREEN  : CLR_DARK_GREEN;
        case K_C: return hi?CLR_BLUE   : CLR_TRUE_BLUE;
        case K_I: return hi?CLR_YELLOW : CLR_DARK_ORANGE;
    }
    return CLR_DARK_GREEN;
}

static int city_color(int k){
    switch(k){
        case K_WATER:  return CLR_DARK_BLUE;
        case K_ROAD:   return CLR_DARK_GREY;
        case K_R:        return CLR_GREEN;
        case K_C:        return CLR_BLUE;
        case K_I:        return CLR_YELLOW;
        case K_POLICE:   return CLR_WHITE;
        case K_PARK:     return CLR_MEDIUM_GREEN;
        case K_SCHOOL:   return CLR_LIGHT_PEACH;
        case K_HOSPITAL: return CLR_PINK;
        default:         return CLR_DARK_GREEN;   // land
    }
}

void draw(void){
    cls(CLR_BLACK);
    print("GRIDCITY", OX, 4, CLR_WHITE);
    print("the data layer behind the map", OX+72, 6, CLR_DARK_GREY);

    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
        int i=idx(x,y), col;
        if(view==V_CITY){
            col = is_zone(kind[i]) ? zone_shade(kind[i], level[i]) : city_color(kind[i]);
        }
        else if(kind[i]==K_WATER) col=CLR_DARKER_BLUE;
        else switch(view){
            case V_LANDVAL: col=ramp(RAMP_LV,landvalue[i]); break;
            case V_CRIME:   col=(popden[i]<=0)?CLR_BROWNISH_BLACK:ramp(RAMP_CR,crime[i]); break;
            case V_POLICE:  col=ramp(RAMP_PO,police[i]); break;
            case V_DENSITY: col=(popden[i]<=0)?CLR_BROWNISH_BLACK:ramp(RAMP_PD,popden[i]); break;
            case V_POLLUTION: col=(pollution[i]<=4)?CLR_BROWNISH_BLACK:ramp(RAMP_PL,pollution[i]); break;
            case V_SERVICES: { int sv=police[i]+school[i]+health[i]+parkv[i]; if(sv>250)sv=250;
                               col=(sv<=4)?CLR_BROWNISH_BLACK:ramp(RAMP_SV,sv); } break;
            default: col=CLR_BLACK;
        }
        // in data overlays, keep roads readable as a faint grid
        if(view!=V_CITY && kind[i]==K_ROAD) col=CLR_DARKER_GREY;
        rectfill(OX+x*TS, OY+y*TS, TS, TS, col);
    }

    // ── HUD ──
    static const char *VNAME[V_COUNT]={"1 CITY","2 LAND VALUE","3 CRIME","4 POLICE","5 DENSITY","6 POLLUTION","7 SERVICES"};
    static const char *BNAME[]={"land","water","road","R zone","C zone","I zone","POLICE","park","school","hospital"};
    int hy=22;
    print("OVERLAY", HUDX, hy, CLR_DARK_GREY); hy+=8;
    for(int v=0;v<V_COUNT;v++){ print(VNAME[v], HUDX, hy, v==view?CLR_WHITE:CLR_DARK_GREY); hy+=8; }
    hy+=4;
    print("BRUSH", HUDX, hy, CLR_DARK_GREY); hy+=8;
    print(str("> %s%s", BNAME[brush], (is_zone(brush)&&brush_dense)?" (dense)":""),
          HUDX, hy, CLR_YELLOW); hy+=10;
    // legend ramp for the active data overlay
    if(view!=V_CITY){
        const int *r = view==V_LANDVAL?RAMP_LV : view==V_CRIME?RAMP_CR : view==V_POLICE?RAMP_PO
                     : view==V_POLLUTION?RAMP_PL : view==V_SERVICES?RAMP_SV : RAMP_PD;
        print("low", HUDX, hy, CLR_DARK_GREY);
        print("high", HUDX+86, hy, CLR_DARK_GREY); hy+=8;
        for(int s=0;s<6;s++) rectfill(HUDX+s*18, hy, 17, 8, r[s]);
        hy+=14;
    }
    // RCI demand meter — a bar each side of a centre baseline (green = wants more of
    // that zone, red = oversupplied). This is what drives the city to grow or empty.
    hy+=2;
    print("RCI DEMAND", HUDX, hy, CLR_DARK_GREY); hy+=8;
    { const char *dl[3]={"R","C","I"}; int dv[3]={demR,demC,demI};
      int dc[3]={CLR_GREEN,CLR_BLUE,CLR_YELLOW};
      for(int k=0;k<3;k++){
        int by=hy+k*8, mid=HUDX+60, v=dv[k];
        if(v>45)v=45; if(v<-45)v=-45;
        print(dl[k], HUDX, by, dc[k]);
        rectfill(mid,by,1,6,CLR_DARK_GREY);              // centre baseline
        if(v>=0) rectfill(mid+1, by+1, v, 4, CLR_GREEN);
        else     rectfill(mid+v, by+1, -v, 4, CLR_RED);
      }
      hy+=3*8+3;
    }
    print(str("EQ %d   LE %d", eduQ, lifeE), HUDX, hy, CLR_LIGHT_GREY); hy+=8;
    print(str("ticks %ld", ticks), HUDX, hy, CLR_LIGHT_GREY);

    // bottom help
    print("RCI/O/W/P/G/S/H/E brush  D light<>dense  1-7 view  SPACE reseed",
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
    memset(level,0,sizeof level);
    memset(landvalue,0,sizeof landvalue);
    memset(crime,0,sizeof crime);
    growth_on=0;   // freeze the valve: this test isolates the crime↔land-value formulas
    // a dense, fully-BUILT R block (x10..21, y10..21) with a road grid THROUGH it so
    // the whole block is road-served, far from centre, NO police
    for(int y=10;y<22;y++) for(int x=10;x<22;x++){ kind[idx(x,y)]=K_R; level[idx(x,y)]=3; }
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
    memset(level,0,sizeof level);
    memset(crime,0,sizeof crime);
    growth_on=0;   // frozen: isolate the pollution → land-value formula
    for(int y=8;y<32;y++) for(int x=8;x<32;x++){ kind[idx(x,y)]=K_R; level[idx(x,y)]=3; } // built district
    for(int x=7;x<33;x++){ kind[idx(x,8)]=K_ROAD; kind[idx(x,20)]=K_ROAD; kind[idx(x,31)]=K_ROAD; }
    for(int y=8;y<32;y++){ kind[idx(8,y)]=K_ROAD; kind[idx(20,y)]=K_ROAD; kind[idx(31,y)]=K_ROAD; }
    recompute_static();
    step(20*TICK_EVERY);
    int lv_before   = landvalue[idx(15,15)];
    int poll_before = pollution[idx(15,15)];

    for(int y=14;y<18;y++) for(int x=16;x<20;x++){ kind[idx(x,y)]=K_I; level[idx(x,y)]=4; } // heavy industry next door
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

    // ── the growth valve: zones build up from empty when demand + conditions allow,
    //    and a road-starved zone never develops ──
    memset(kind,0,sizeof kind);
    memset(level,0,sizeof level);
    memset(crime,0,sizeof crime);
    growth_on=1;
    // a served R district + an I block (jobs → residential demand), near the grid
    // centre so land value is high enough to build; all starting at level 0
    for(int y=20;y<30;y++) for(int x=26;x<40;x++) kind[idx(x,y)]=K_R;
    for(int y=20;y<26;y++) for(int x=42;x<48;x++) kind[idx(x,y)]=K_I;
    for(int x=24;x<50;x++){ kind[idx(x,19)]=K_ROAD; kind[idx(x,25)]=K_ROAD; kind[idx(x,30)]=K_ROAD; }
    for(int y=19;y<31;y++){ kind[idx(26,y)]=K_ROAD; kind[idx(33,y)]=K_ROAD; kind[idx(40,y)]=K_ROAD; }
    // an UNREACHABLE R block — zoned, but no road anywhere near it
    for(int y=40;y<46;y++) for(int x=4;x<10;x++) kind[idx(x,y)]=K_R;
    recompute_static();
    long built0=0; for(int i=0;i<MAXC;i++) if(kind[i]==K_R) built0+=level[i];
    step(80*TICK_EVERY);                 // ~80 ticks → ~20 growth passes
    long builtR=0; for(int y=20;y<30;y++) for(int x=26;x<40;x++) builtR+=level[idx(x,y)];
    int starved = level[idx(6,42)];      // the road-starved lot
    expect_eq(built0, 0, "the city starts as empty lots (level 0)");
    expect(builtR > 0, "served zones in demand grow from empty into a built district");
    expect(demR != 0 || demC != 0 || demI != 0, "the RCI demand meter is live");
    expect_eq(starved, 0, "a road-starved zone never develops");

    // ── positive drivers: parks + service coverage raise land value; schools/hospitals
    //    drive EQ/LE. Frozen valve, a built road-served R district for population. ──
    memset(kind,0,sizeof kind); memset(level,0,sizeof level); memset(crime,0,sizeof crime);
    growth_on=0;
    for(int y=10;y<26;y++) for(int x=10;x<26;x++){ kind[idx(x,y)]=K_R; level[idx(x,y)]=3; }
    for(int x=9;x<27;x+=4) for(int y=9;y<27;y++) kind[idx(x,y)]=K_ROAD;
    for(int y=9;y<27;y+=4) for(int x=9;x<27;x++) kind[idx(x,y)]=K_ROAD;
    recompute_static();
    step(8*TICK_EVERY);
    int lv0=landvalue[idx(15,15)], eq0=eduQ, le0=lifeE;
    kind[idx(16,16)]=K_PARK; kind[idx(14,16)]=K_SCHOOL; kind[idx(16,14)]=K_HOSPITAL;
    recompute_static();
    step(8*TICK_EVERY);
    expect(1, str("DBG drivers: landval %d->%d  EQ %d->%d  LE %d->%d",
                  lv0, landvalue[idx(15,15)], eq0, eduQ, le0, lifeE));
    expect(landvalue[idx(15,15)] > lv0, "parks + service coverage raise land value");
    expect(eduQ  > eq0, "a school raises the Education Quotient");
    expect(lifeE > le0, "a hospital raises Life Expectancy");

    // ── zoning: two identical served R blocks (jobs nearby), one LIGHT one DENSE.
    //    Light tops out at LIGHT_CAP; dense rises taller. ──
    memset(kind,0,sizeof kind); memset(level,0,sizeof level);
    memset(zdense,0,sizeof zdense); memset(crime,0,sizeof crime);
    growth_on=1;
    for(int y=20;y<28;y++) for(int x=28;x<36;x++) kind[idx(x,y)]=K_I;          // jobs, downtown
    for(int y=20;y<26;y++) for(int x=10;x<16;x++){ kind[idx(x,y)]=K_R; zdense[idx(x,y)]=0; } // LIGHT
    for(int y=20;y<26;y++) for(int x=48;x<54;x++){ kind[idx(x,y)]=K_R; zdense[idx(x,y)]=1; } // DENSE
    for(int x=9;x<55;x++){ kind[idx(x,19)]=K_ROAD; kind[idx(x,23)]=K_ROAD; kind[idx(x,27)]=K_ROAD; }
    for(int y=19;y<28;y++) for(int x=9;x<55;x+=4) kind[idx(x,y)]=K_ROAD;
    recompute_static();
    step(80*TICK_EVERY);
    int maxL=0,maxD=0;
    for(int y=20;y<26;y++) for(int x=10;x<16;x++){ int v=level[idx(x,y)]; if(v>maxL)maxL=v; }
    for(int y=20;y<26;y++) for(int x=48;x<54;x++){ int v=level[idx(x,y)]; if(v>maxD)maxD=v; }
    expect(1, str("DBG zoning: light max=%d  dense max=%d  (LIGHT_CAP=%d)", maxL, maxD, LIGHT_CAP));
    expect(maxL <= LIGHT_CAP, "light zoning caps development low");
    expect(maxD >  LIGHT_CAP, "dense zoning lets development rise taller");
}
#endif
