/* de:meta
{
  "title": "gridcity",
  "status": "active",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "cellular-automata",
    "algorithm-visualization"
  ],
  "lineage": "The consequence-side mirror of procplaces: procplaces paints R/C/I top-down from noise; gridcity DERIVES the data layers bottom-up from placed zones + feedback, the way SimCity 2000 runs its map. One smooth() diffusion primitive (pollution/density/water-value) plus one spread() distance-falloff primitive (service coverage), wired by cross-layer formulas into the crime↔land-value slum spiral. v1 carries a spec() proving the loop and the police-coverage reversal.",
  "description": "EXPERIMENT (v1, bare): the AGGREGATE TILE-DATA SIMULATION behind SimCity 2000 — not road geometry (roadlab/streetlab) and not a top-down generator (procplaces), but the consequence side: paint terrain, tile roads, and R/C/I zones, and the engine COMPUTES the emergent data layers from them and from each other every sim tick. v1 proves the core feedback loop crime↑ → land value↓ → crime↑ (the slum spiral), broken only by POLICE coverage. It is all two spreading primitives over a 64x48 grid: a DIFFUSION blur (4-neighbour average, mass-conserving) for population density + near-water land-value, and a DECAYING SPREAD (strongest-neighbour minus a per-step decay = a distance falloff) for service coverage — because an averaging blur thins a single police station to nothing and never suppresses crime (the one non-obvious v1 finding). Cross-layer formulas wire them: land value = centre-proximity + near-water − crime; crime = base + density − land value − police. Opens on a seeded sample city with one unpoliced slum. Pick the brush from a cute icon toolbar under the grid (or the keys), and click an overlay name to switch the data view. Number keys 1-5 toggle overlays (City / Land value / Crime / Police / Density); LEFT-drag paints the current brush, RIGHT-drag erases; R/C/I zone, O road, W water, P police station, E erase; SPACE reseeds. Carries a spec() that pins the loop: an unpoliced dense block reaches crime 87 / land value 44, and dropping a station collapses crime to 0 and rebounds land value to 87. Design + roadmap (v1 bare → v1.5 pollution + the develop/abandon growth valve driven by an RCI demand meter → v2 full: densities, power, water, traffic, fire/education/health coverage) in docs/design/data-layers.md."
}
de:meta */
#include "studio.h"
#include "ui.h"          // ui_spr_button_styled — the brush toolbar icons
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
//   Pick a brush by CLICKING the cute icon row under the grid (or the keys below);
//   click an overlay name on the right to switch the data view. Keys still work.
//   LEFT-drag   paint the current brush      RIGHT-drag  erase to land
//   R C I       zone Residential/Commercial/Industrial (D toggles light↔dense zoning:
//               light caps low = suburbs, dense rises tall = downtown)
//   O road  W water  P police  G park  S school  H hospital  L power plant
//   F fire station   B ignite a building   U water pump (place by the lake)   E erase
//   Power floods from plants through roads/zones; an unpowered (or road-less) lot
//   can't develop — cut a district off and it goes dark and stops growing. Fire spreads
//   building→building unless fire-station coverage contains it.
//   1..9,0 overlay (City…Fire); [ ] cycle (reaches Water). Water from lake-side pumps is a
//   SOFT gate — an unwatered lot still builds but can't rise tall.
//   Traffic = commute trips routed home↔job along the roads; busy roads add pollution.
//   SPACE       reseed the sample city

#define GW 64
#define GH 48
#define MAXC (GW*GH)
#define TS 3            // pixels per cell
#define OX 4            // grid origin
#define OY 20
#define HUDX 202        // right panel

// cell kinds
enum { K_LAND, K_WATER, K_ROAD, K_R, K_C, K_I, K_POLICE, K_PARK, K_SCHOOL, K_HOSPITAL, K_PLANT, K_FIRESTN, K_PUMP };
#define B_IGNITE 50    // a pseudo-brush: clicking sets a building alight (doesn't change kind)

// overlays
enum { V_CITY, V_LANDVAL, V_CRIME, V_POLICE, V_DENSITY, V_POLLUTION, V_SERVICES, V_POWER, V_TRAFFIC, V_FIRE, V_WATER, V_COUNT };

// ── state ──
static int  kind[MAXC];
static int  level[MAXC];      // development level of a zoned cell: 0 = empty lot … LMAX = full
static int  zdense[MAXC];     // zoning density: 0 = light (caps low, suburbs), 1 = dense (allows tall)
static int  landvalue[MAXC], crime[MAXC], police[MAXC], popden[MAXC], pollution[MAXC];
static int  school[MAXC], health[MAXC];   // education / health coverage (spread from schools/hospitals)
static int  traffic[MAXC];                // road-traffic density (deterministic trip generation)
static int  distJob[MAXC], distHome[MAXC];// BFS routing fields along roads (dist to nearest job / home)
static int  firecov[MAXC];                // fire-station coverage (spread)
static int  burning[MAXC];                // fire intensity / fuel remaining (0 = not on fire)
static int  terrainv[MAXC];   // static water-proximity value (recomputed on edit)
static int  parkv[MAXC];      // static park-amenity value (recomputed on edit)
static int  commv[MAXC];      // commercial-vitality premium (downtown raises nearby land value)
static int  roadacc[MAXC];    // static road-access field — a service radius, not strict adjacency
static int  powered[MAXC];    // static: 1 if the tile is reached by the power grid (flood from plants)
static int  watered[MAXC];    // static: 1 if reached by the water network (flood from pumps by the lake)
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
#define INF (1<<29)           // "unreachable" for the BFS routing fields
#define TRIP_MAX    60        // a commute trip walks at most this many road tiles
#define FIRE_MAX     8        // fuel of a freshly-lit fire
#define FIRE_SPREAD  5        // a fire this fierce jumps to flammable neighbours
#define FIRE_BLOCK 120        // fire coverage this strong stops a fire taking hold
#define WATER_DRY_CAP 1       // without water a lot can't develop past this (a soft gate)
#define R_BASE     30         // baseline residential pull (people want in)
#define C_BASE     20         // baseline commercial pull
#define I_BASE     20         // baseline industrial pull (external market)

static inline int idx(int x, int y){ return y*GW + x; }
static inline int is_zone(int k){ return k==K_R||k==K_C||k==K_I; }
static inline int developed(int i){ return is_zone(kind[i]) && level[i]>0; }
// power conducts through the built fabric (roads/zones/civic/plants), not bare land or water
static inline int conducts(int k){ return k!=K_LAND && k!=K_WATER; }

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
    // power grid: flood from every plant through connected conductive tiles. A district
    // cut off from a plant (e.g. its road link bulldozed) goes dark and stops developing.
    static int q[MAXC]; int head=0, tail=0;
    for(int i=0;i<MAXC;i++) powered[i]=0;
    for(int i=0;i<MAXC;i++) if(kind[i]==K_PLANT){ powered[i]=1; q[tail++]=i; }
    while(head<tail){
        int i=q[head++], x=i%GW, y=i/GW;
        int nb[4], nn=0;
        if(x>0)nb[nn++]=i-1; if(x<GW-1)nb[nn++]=i+1; if(y>0)nb[nn++]=i-GW; if(y<GH-1)nb[nn++]=i+GW;
        for(int j=0;j<nn;j++){ int n=nb[j]; if(!powered[n] && conducts(kind[n])){ powered[n]=1; q[tail++]=n; } }
    }
    // water grid: flood from ACTIVE pumps (a pump touching a water tile) through the fabric.
    // Same shape as power, but a pump must sit beside the lake to draw — and water is a SOFT
    // gate (see the growth valve): unwatered lots build, but can't rise past WATER_DRY_CAP.
    head=tail=0;
    for(int i=0;i<MAXC;i++) watered[i]=0;
    for(int i=0;i<MAXC;i++){
        if(kind[i]!=K_PUMP) continue;
        int x=i%GW, y=i/GW, near=0;
        if(x>0&&kind[i-1]==K_WATER)near=1; if(x<GW-1&&kind[i+1]==K_WATER)near=1;
        if(y>0&&kind[i-GW]==K_WATER)near=1; if(y<GH-1&&kind[i+GW]==K_WATER)near=1;
        if(near){ watered[i]=1; q[tail++]=i; }
    }
    while(head<tail){
        int i=q[head++], x=i%GW, y=i/GW, nb[4], nn=0;
        if(x>0)nb[nn++]=i-1; if(x<GW-1)nb[nn++]=i+1; if(y>0)nb[nn++]=i-GW; if(y<GH-1)nb[nn++]=i+GW;
        for(int j=0;j<nn;j++){ int n=nb[j]; if(!watered[n] && conducts(kind[n])){ watered[n]=1; q[tail++]=n; } }
    }
}

// ── traffic: deterministic trip generation ──
// route_field: multi-source BFS over ROAD tiles outward from every road tile that touches a
// developed target zone (kind ka or kb). dist[r] = road steps to the nearest such zone.
static void route_field(int *dist, int ka, int kb){
    static int q[MAXC]; int head=0, tail=0;
    for(int i=0;i<MAXC;i++) dist[i]=INF;
    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
        int i=idx(x,y); if(kind[i]!=K_ROAD) continue;
        int t=0;
        if(x>0    && developed(i-1)  && (kind[i-1]==ka ||kind[i-1]==kb))  t=1;
        if(x<GW-1 && developed(i+1)  && (kind[i+1]==ka ||kind[i+1]==kb))  t=1;
        if(y>0    && developed(i-GW) && (kind[i-GW]==ka||kind[i-GW]==kb)) t=1;
        if(y<GH-1 && developed(i+GW) && (kind[i+GW]==ka||kind[i+GW]==kb)) t=1;
        if(t){ dist[i]=0; q[tail++]=i; }
    }
    while(head<tail){
        int i=q[head++], x=i%GW, y=i/GW;
        int nb[4],nn=0;
        if(x>0)nb[nn++]=i-1; if(x<GW-1)nb[nn++]=i+1; if(y>0)nb[nn++]=i-GW; if(y<GH-1)nb[nn++]=i+GW;
        for(int j=0;j<nn;j++){ int n=nb[j]; if(kind[n]==K_ROAD && dist[n]==INF){ dist[n]=dist[i]+1; q[tail++]=n; } }
    }
}
// deposit_trips: each developed `src` cell sends a trip down the gradient of `dist` to the
// nearest destination, depositing its population (level) as traffic on every road tile en route.
static void deposit_trips(const int *dist, int src){
    for(int y=0;y<GH;y++) for(int x=0;x<GW;x++){
        int i=idx(x,y); if(kind[i]!=src || !developed(i)) continue;
        int best=INF, br=-1, nb[4], nn=0;
        if(x>0)nb[nn++]=i-1; if(x<GW-1)nb[nn++]=i+1; if(y>0)nb[nn++]=i-GW; if(y<GH-1)nb[nn++]=i+GW;
        for(int j=0;j<nn;j++){ int r=nb[j]; if(kind[r]==K_ROAD && dist[r]<best){ best=dist[r]; br=r; } }
        if(br<0 || best>=INF) continue;            // no road access / no reachable destination
        int cur=br, steps=0, load=level[i];
        while(steps<TRIP_MAX){
            traffic[cur]+=load;
            if(dist[cur]==0) break;                // arrived at the destination's doorstep
            int cx=cur%GW, cy=cur/GW, nx=-1, mb[4], mn=0;
            if(cx>0)mb[mn++]=cur-1; if(cx<GW-1)mb[mn++]=cur+1; if(cy>0)mb[mn++]=cur-GW; if(cy<GH-1)mb[mn++]=cur+GW;
            for(int j=0;j<mn;j++){ int r=mb[j]; if(kind[r]==K_ROAD && dist[r]==dist[cur]-1){ nx=r; break; } }
            if(nx<0) break; cur=nx; steps++;
        }
    }
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

    // 1b. traffic — residents commute to the nearest job; jobs draw workers from the nearest
    //     homes. Each trip walks the road network (BFS routing field) depositing its level as
    //     traffic, so roads on many home↔job paths congest. Feeds pollution below.
    for(int i=0;i<MAXC;i++) traffic[i]=0;
    route_field(distJob,  K_C, K_I); deposit_trips(distJob,  K_R);
    route_field(distHome, K_R, K_R); deposit_trips(distHome, K_C);
                                     deposit_trips(distHome, K_I);

    // 2. service coverage — each civic building is a source spread into a radius (the
    //    same falloff police uses). Police suppresses crime; school/health raise land
    //    value and drive the EQ/LE city stats.
    for(int i=0;i<MAXC;i++) police[i] = (kind[i]==K_POLICE)   ? 250 : 0;
    for(int i=0;i<MAXC;i++) school[i]  = (kind[i]==K_SCHOOL)   ? 250 : 0;
    for(int i=0;i<MAXC;i++) health[i]  = (kind[i]==K_HOSPITAL) ? 250 : 0;
    for(int i=0;i<MAXC;i++) firecov[i] = (kind[i]==K_FIRESTN)  ? 250 : 0;
    spread(police, 14, 18);
    spread(school, 16, 16);
    spread(health, 16, 16);
    spread(firecov, 14, 18);
    // commercial vitality — built downtown raises nearby land value (the shops-nearby premium)
    for(int i=0;i<MAXC;i++) commv[i] = (kind[i]==K_C) ? COMM_PREM*level[i] : 0;
    smooth(commv, 3);

    // 3. pollution — industry is the heavy emitter, commerce a little (both scaled by level),
    //    plus busy ROADS (traffic fumes); it diffuses onto neighbours and drags land value down
    for(int i=0;i<MAXC;i++)
        pollution[i] = kind[i]==K_I ? 55*level[i] : kind[i]==K_C ? 10*level[i]
                     : kind[i]==K_ROAD ? traffic[i]/3 : 0;
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
            int served = roadacc[i]>40 && powered[i];   // hard gate: needs BOTH a road and power
            int cap = zdense[i] ? LMAX : LIGHT_CAP;       // light zoning tops out low
            if(!watered[i] && cap>WATER_DRY_CAP) cap=WATER_DRY_CAP;  // soft gate: no water → can't rise tall
            if(served && dem>DEM_DEAD && des>GROW_THRESH){ if(level[i]<cap) level[i]++; }
            else if(!served || dem<-DEM_DEAD || des<DECL_THRESH){ if(level[i]>0) level[i]--; }
        }
    }

    // 8. fire — a dynamic hazard (runs regardless of the growth gate). A burning building
    //    spreads to flammable, poorly-covered neighbours while fierce, loses fuel each tick
    //    (fire coverage puts it out faster), and is knocked down as it burns; burn long
    //    enough and it's razed to an empty lot. Player lights fires with the ignite brush.
    {
        static int ignite[MAXC];
        for(int i=0;i<MAXC;i++) ignite[i]=0;
        for(int i=0;i<MAXC;i++){
            if(burning[i]<FIRE_SPREAD) continue;             // only a fierce fire jumps
            int x=i%GW, y=i/GW, nb[4], nn=0;
            if(x>0)nb[nn++]=i-1; if(x<GW-1)nb[nn++]=i+1; if(y>0)nb[nn++]=i-GW; if(y<GH-1)nb[nn++]=i+GW;
            for(int j=0;j<nn;j++){ int n=nb[j];
                if(is_zone(kind[n]) && level[n]>0 && burning[n]==0 && firecov[n]<FIRE_BLOCK) ignite[n]=1; }
        }
        for(int i=0;i<MAXC;i++){
            if(ignite[i]) burning[i]=FIRE_MAX;
            if(burning[i]>0){
                if(ticks&1 && level[i]>0) level[i]--;        // damage every other tick it burns
                burning[i] -= 1 + firecov[i]/80;             // coverage extinguishes faster
                if(burning[i]<0) burning[i]=0;
            }
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
    kind[idx(57,39)]=K_PLANT;  // a power plant on the grid's edge feeds the whole connected city
    kind[idx(33,33)]=K_FIRESTN; kind[idx(15,27)]=K_FIRESTN;  // two fire stations cover the core
    kind[idx(9,13)]=K_PUMP;    // a water pump drawing from the lake supplies the connected city

    recompute_static();
    // pre-warm: let the zones grow from empty to a settled city before the player
    // ever sees it (so we open on a living town, not a grid of empty lots)
    growth_on=1;
    for(int i=0;i<80;i++) sim_step();
    ticks=0;
}

void init(void){ seed_city(); colorkey(0); }   // index 0 = transparent for the toolbar icons

// ── brush toolbar: cute clickable icons under the grid (sprites in gridcity.cart.js)
// button i draws spr(i); BR_VAL[i] is its brush (i < NBR), and the last button
// (i == NBR) is the light↔dense zoning toggle.
static const int BR_VAL[] = { K_LAND, K_ROAD, K_WATER, K_R, K_C, K_I, K_POLICE,
                              K_PARK, K_SCHOOL, K_HOSPITAL, K_PLANT, K_FIRESTN, K_PUMP, B_IGNITE };
static const char *BR_TIP[] = { "erase","road","water","R zone","C zone","I zone","police",
                                "park","school","hospital","power","fire stn","pump","ignite" };
#define NBR    14                     // brush buttons
#define NTOOL  15                     // + the dense toggle (slot 14)
#define TLBX   4
#define TLBY   (OY + GH*TS + 1)        // just under the grid
#define TLBSZ  16
#define TLBP   18                      // button pitch
#define TLBCOLS 8
#define OVLY   30                      // overlay-list first item y (keep == draw())

// which toolbar button is under (mx,my)?  -1 = none
static int tool_hit(int mx, int my){
    for(int i=0;i<NTOOL;i++){
        int bx=TLBX+(i%TLBCOLS)*TLBP, by=TLBY+(i/TLBCOLS)*TLBP;
        if(mx>=bx && mx<bx+TLBSZ && my>=by && my<by+TLBSZ) return i;
    }
    return -1;
}

// ── painting ──
static void paint_at(int mx,int my,int k){
    int x=(mx-OX)/TS, y=(my-OY)/TS;
    if(x<0||x>=GW||y<0||y>=GH) return;
    int i=idx(x,y);
    if(k==B_IGNITE){ if(is_zone(kind[i]) && level[i]>0) burning[i]=FIRE_MAX; return; }  // light it up
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
    if(keyp('L')) brush=K_PLANT;               // L = power pLant
    if(keyp('F')) brush=K_FIRESTN;             // F = Fire station
    if(keyp('B')) brush=B_IGNITE;              // B = Burn (ignite a building)
    if(keyp('U')) brush=K_PUMP;                // U = water pUmp (place by the lake)
    if(keyp('E')) brush=K_LAND;
    if(keyp('D')) brush_dense=!brush_dense;   // toggle light/dense zoning for the zone brushes
    // overlay select
    if(keyp('1')) view=V_CITY;    if(keyp('2')) view=V_LANDVAL; if(keyp('3')) view=V_CRIME;
    if(keyp('4')) view=V_POLICE;  if(keyp('5')) view=V_DENSITY; if(keyp('6')) view=V_POLLUTION;
    if(keyp('7')) view=V_SERVICES; if(keyp('8')) view=V_POWER; if(keyp('9')) view=V_TRAFFIC;
    if(keyp('0')) view=V_FIRE;
    if(keyp('[')) view=(view+V_COUNT-1)%V_COUNT;   // cycle overlays (reaches WATER, the 11th)
    if(keyp(']')) view=(view+1)%V_COUNT;
    if(keyp(KEY_SPACE)) seed_city();

    int mx=mouse_x(), my=mouse_y();
    // click an overlay name (right panel) to switch view; the brush buttons themselves are
    // ui_spr_button_styled in draw() now (they set brush / dense on activation).
    if(mouse_pressed(0) && mx>=HUDX)
        for(int v=0;v<V_COUNT;v++){ int oy=OVLY+v*8; if(my>=oy&&my<oy+8){ view=v; break; } }
    // paint — paint_at bounds-checks to the grid, so the toolbar + right panel never paint
    if(mouse_down(0)) paint_at(mx,my,brush);
    if(mouse_down(1)) paint_at(mx,my,K_LAND);

    if(++fcount>=TICK_EVERY){ fcount=0; sim_step(); }
}

// 6-step heat ramps (low→high), one per data overlay
static const int RAMP_LV[6] = {CLR_DARK_RED, CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_LIME_GREEN, CLR_GREEN};
static const int RAMP_CR[6] = {CLR_DARK_BLUE, CLR_DARK_PURPLE, CLR_DARK_RED, CLR_RED, CLR_ORANGE, CLR_YELLOW};
static const int RAMP_PO[6] = {CLR_BLACK, CLR_DARKER_BLUE, CLR_BLUE_GREEN, CLR_TRUE_BLUE, CLR_BLUE, CLR_WHITE};
static const int RAMP_PD[6] = {CLR_DARK_GREEN, CLR_MEDIUM_GREEN, CLR_GREEN, CLR_LIME_GREEN, CLR_YELLOW, CLR_WHITE};
static const int RAMP_PL[6] = {CLR_BLACK, CLR_DARKER_PURPLE, CLR_DARK_PURPLE, CLR_MAUVE, CLR_DARK_ORANGE, CLR_ORANGE};
static const int RAMP_SV[6] = {CLR_BLACK, CLR_DARKER_GREY, CLR_BLUE_GREEN, CLR_MEDIUM_GREEN, CLR_LIME_GREEN, CLR_WHITE};
static const int RAMP_TR[6] = {CLR_DARKER_GREY, CLR_DARK_GREEN, CLR_LIME_GREEN, CLR_YELLOW, CLR_ORANGE, CLR_RED};

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
        case K_PLANT:    return CLR_RED;
        case K_FIRESTN:  return CLR_DARK_ORANGE;
        case K_PUMP:     return CLR_TRUE_BLUE;
        default:         return CLR_DARK_GREEN;   // land
    }
}

void draw(void){
    cls(CLR_BLACK);
    ui_begin();                 // ui.h: snapshot input before the toolbar buttons
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
            case V_POWER: col = conducts(kind[i]) ? (powered[i]?CLR_YELLOW:CLR_DARK_RED)
                                                  : CLR_BROWNISH_BLACK; break;   // lit vs blackout
            case V_TRAFFIC: col = (kind[i]==K_ROAD) ? ramp(RAMP_TR, traffic[i]) : CLR_BROWNISH_BLACK; break;
            case V_FIRE: col=(firecov[i]<=4)?CLR_BROWNISH_BLACK:ramp(RAMP_PO,firecov[i]); break; // coverage; flames drawn on top below
            case V_WATER: col = conducts(kind[i]) ? (watered[i]?CLR_BLUE:CLR_DARKER_GREY) : CLR_BROWNISH_BLACK; break;
            default: col=CLR_BLACK;
        }
        // in data overlays, keep roads readable as a faint grid (but power + traffic colour the roads themselves)
        if(view!=V_CITY && view!=V_POWER && view!=V_TRAFFIC && view!=V_FIRE && view!=V_WATER && kind[i]==K_ROAD) col=CLR_DARKER_GREY;
        // live flames burn bright in the city view and the fire view
        if((view==V_CITY||view==V_FIRE) && burning[i]>0) col = (burning[i]*2>=FIRE_MAX)?CLR_RED:CLR_ORANGE;
        rectfill(OX+x*TS, OY+y*TS, TS, TS, col);
    }

    // ── HUD ──
    static const char *VNAME[V_COUNT]={"1 CITY","2 LAND VALUE","3 CRIME","4 POLICE","5 DENSITY","6 POLLUTION","7 SERVICES","8 POWER","9 TRAFFIC","0 FIRE","[ ] WATER"};
    static const char *BNAME[]={"land","water","road","R zone","C zone","I zone","POLICE","park","school","hospital","power plant","fire stn","water pump"};
    int mx=mouse_x(), my=mouse_y();
    int hy=22;
    print("OVERLAY", HUDX, hy, CLR_DARK_GREY); hy+=8;
    for(int v=0;v<V_COUNT;v++){
        int hov = (mx>=HUDX && my>=hy && my<hy+8);
        print(VNAME[v], HUDX, hy, v==view?CLR_WHITE : (hov?CLR_LIGHT_GREY:CLR_DARK_GREY)); hy+=8;
    }
    hy+=4;
    print("BRUSH", HUDX, hy, CLR_DARK_GREY); hy+=8;
    print(str("> %s%s", brush==B_IGNITE?"ignite fire":BNAME[brush],
              (is_zone(brush)&&brush_dense)?" (dense)":""),
          HUDX, hy, CLR_YELLOW); hy+=10;
    // legend for the active data overlay — a ramp for scalar fields, two swatches for power
    if(view==V_POWER || view==V_WATER){
        int off = view==V_POWER?CLR_DARK_RED:CLR_DARKER_GREY, on = view==V_POWER?CLR_YELLOW:CLR_BLUE;
        rectfill(HUDX, hy, 12, 8, off); print(view==V_POWER?"dark":"dry", HUDX+15, hy, CLR_DARK_GREY);
        rectfill(HUDX+54, hy, 12, 8, on); print(view==V_POWER?"lit":"wet", HUDX+69, hy, CLR_DARK_GREY);
        hy+=14;
    } else if(view!=V_CITY){
        const int *r = view==V_LANDVAL?RAMP_LV : view==V_CRIME?RAMP_CR : view==V_POLICE?RAMP_PO
                     : view==V_POLLUTION?RAMP_PL : view==V_SERVICES?RAMP_SV
                     : view==V_TRAFFIC?RAMP_TR : RAMP_PD;
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
    print(str("ticks %ld", ticks), HUDX, hy, CLR_LIGHT_GREY); hy+=8;
    font(FONT_SMALL); print("SPACE = reseed city", HUDX, hy, CLR_DARK_GREY); font(FONT_NORMAL);

    // ── brush toolbar: cute clickable icons under the grid ──
    // active brush = white frame + yellow halo; dense toggle lights when on; hover = light frame
    // ui_spr_button_styled keeps gridcity's look (black panel · grey/white frame · yellow
    // selected-halo) and owns the press/capture/hit-pad; a click sets the brush/dense here.
    UiSprStyle TBS = { CLR_BLACK, CLR_BLACK, CLR_DARKER_GREY, CLR_LIGHT_GREY, CLR_WHITE, CLR_YELLOW };
    int hov_tool = tool_hit(mx,my);                  // kept only for the hovered-name tooltip below
    for(int i=0;i<NTOOL;i++){
        int bx=TLBX+(i%TLBCOLS)*TLBP, by=TLBY+(i/TLBCOLS)*TLBP;
        int sel = (i<NBR) ? (brush==BR_VAL[i]) : brush_dense;
        if(ui_spr_button_styled(i, bx-1, by-1, TLBSZ+2, TLBSZ+2, sel, TBS)){
            if(i==NBR) brush_dense=!brush_dense;
            else       brush=BR_VAL[i];
        }
    }
    // hovered-tool name in the gap right of the toolbar (transient — only on hover,
    // so nothing persistent sits next to the stats column at HUDX)
    if(hov_tool>=0){
        font(FONT_SMALL);
        print(hov_tool==NBR ? (brush_dense?"dense on":"dense off") : BR_TIP[hov_tool],
              TLBX + TLBCOLS*TLBP + 4, TLBY+4, CLR_YELLOW);
        font(FONT_NORMAL);
    }

    ui_end();                   // ui.h: resolve clicks against the toolbar buttons drawn above
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
    kind[idx(24,19)]=K_PLANT;     // power the connected grid (the isolated block stays dark + dead)
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
    kind[idx(9,19)]=K_PLANT;                          // power the grid so both blocks can develop
    kind[idx(7,23)]=K_WATER; kind[idx(8,23)]=K_PUMP;  // …and water it so dense can rise past the dry cap
    recompute_static();
    step(80*TICK_EVERY);
    int maxL=0,maxD=0;
    for(int y=20;y<26;y++) for(int x=10;x<16;x++){ int v=level[idx(x,y)]; if(v>maxL)maxL=v; }
    for(int y=20;y<26;y++) for(int x=48;x<54;x++){ int v=level[idx(x,y)]; if(v>maxD)maxD=v; }
    expect(1, str("DBG zoning: light max=%d  dense max=%d  (LIGHT_CAP=%d)", maxL, maxD, LIGHT_CAP));
    expect(maxL <= LIGHT_CAP, "light zoning caps development low");
    expect(maxD >  LIGHT_CAP, "dense zoning lets development rise taller");

    // ── power: a road-served, in-demand zone still can't develop with no power; wire a
    //    plant into the connected grid and it does ──
    memset(kind,0,sizeof kind); memset(level,0,sizeof level);
    memset(zdense,0,sizeof zdense); memset(crime,0,sizeof crime);
    growth_on=1;
    for(int y=20;y<26;y++) for(int x=20;x<30;x++){ kind[idx(x,y)]=K_R; zdense[idx(x,y)]=1; }
    for(int y=20;y<26;y++) for(int x=32;x<40;x++) kind[idx(x,y)]=K_I;          // jobs
    for(int x=18;x<42;x++){ kind[idx(x,19)]=K_ROAD; kind[idx(x,23)]=K_ROAD; kind[idx(x,26)]=K_ROAD; }
    for(int y=19;y<27;y++) for(int x=18;x<42;x+=4) kind[idx(x,y)]=K_ROAD;
    recompute_static();
    step(50*TICK_EVERY);
    long unpowered=0; for(int y=20;y<26;y++) for(int x=20;x<30;x++) unpowered+=level[idx(x,y)];
    expect_eq(unpowered, 0, "an unpowered zone cannot develop (no plant on the grid)");
    kind[idx(40,23)]=K_PLANT;     // wire a plant onto the connected road network
    recompute_static();
    step(50*TICK_EVERY);
    long powered_built=0; for(int y=20;y<26;y++) for(int x=20;x<30;x++) powered_built+=level[idx(x,y)];
    expect(1, str("DBG power: built unpowered=%ld -> powered=%ld", unpowered, powered_built));
    expect(powered_built > 0, "a connected power plant lets the zone develop");

    // ── traffic: homes + jobs joined by roads generate commute traffic; remove the jobs
    //    and there's nowhere to commute, so the roads quieten ──
    memset(kind,0,sizeof kind); memset(level,0,sizeof level);
    memset(zdense,0,sizeof zdense); memset(crime,0,sizeof crime);
    growth_on=1;
    for(int y=20;y<26;y++) for(int x=12;x<20;x++){ kind[idx(x,y)]=K_R; zdense[idx(x,y)]=1; }  // homes
    for(int y=20;y<26;y++) for(int x=44;x<52;x++){ kind[idx(x,y)]=K_I; zdense[idx(x,y)]=1; }  // jobs, far off
    for(int x=11;x<53;x++){ kind[idx(x,19)]=K_ROAD; kind[idx(x,23)]=K_ROAD; kind[idx(x,26)]=K_ROAD; }
    for(int y=19;y<27;y++) for(int x=11;x<53;x+=4) kind[idx(x,y)]=K_ROAD;
    kind[idx(32,23)]=K_PLANT;
    recompute_static();
    step(80*TICK_EVERY);
    long tr_jobs=0; for(int i=0;i<MAXC;i++) if(kind[i]==K_ROAD) tr_jobs+=traffic[i];
    expect(tr_jobs > 0, "commuting between homes and jobs generates road traffic");
    // turn the jobs into more homes — now nobody has anywhere to commute TO
    for(int y=20;y<26;y++) for(int x=44;x<52;x++) kind[idx(x,y)]=K_R;
    recompute_static();
    step(20*TICK_EVERY);
    long tr_none=0; for(int i=0;i<MAXC;i++) if(kind[i]==K_ROAD) tr_none+=traffic[i];
    expect(1, str("DBG traffic: with jobs=%ld  all-homes=%ld", tr_jobs, tr_none));
    expect(tr_none < tr_jobs, "with no jobs to reach, commute traffic drops");

    // ── fire: an ignited building spreads and razes a block; fire-station coverage contains it.
    //    Frozen valve + a built block, so the only thing changing levels is the blaze. ──
    growth_on=0;
    memset(kind,0,sizeof kind); memset(zdense,0,sizeof zdense); memset(burning,0,sizeof burning);
    for(int i=0;i<MAXC;i++) level[i]=0;
    for(int y=10;y<26;y++) for(int x=10;x<26;x++){ kind[idx(x,y)]=K_R; level[idx(x,y)]=3; }
    recompute_static();
    burning[idx(17,17)]=FIRE_MAX;                 // light the middle of the block (no fire cover)
    step(24*TICK_EVERY);
    int razed_open=0; for(int y=10;y<26;y++) for(int x=10;x<26;x++) if(level[idx(x,y)]==0) razed_open++;
    expect(razed_open > 1, "an uncovered fire spreads and razes buildings");

    // same block, now with a fire station covering it
    memset(burning,0,sizeof burning);
    for(int y=10;y<26;y++) for(int x=10;x<26;x++) level[idx(x,y)]=3;
    kind[idx(18,18)]=K_FIRESTN;
    recompute_static();
    burning[idx(14,14)]=FIRE_MAX;
    step(24*TICK_EVERY);
    int razed_cov=0; for(int y=10;y<26;y++) for(int x=10;x<26;x++) if(level[idx(x,y)]==0) razed_cov++;
    expect(1, str("DBG fire: razed open=%d  covered=%d", razed_open, razed_cov));
    expect(razed_cov < razed_open, "fire-station coverage contains the blaze");

    // ── water: a powered, served, in-demand dense block still can't rise past WATER_DRY_CAP
    //    with no water; add a lake + a pump beside it and it grows tall ──
    memset(kind,0,sizeof kind); memset(level,0,sizeof level);
    memset(zdense,0,sizeof zdense); memset(crime,0,sizeof crime);
    growth_on=1;
    for(int y=20;y<26;y++) for(int x=20;x<30;x++){ kind[idx(x,y)]=K_R; zdense[idx(x,y)]=1; } // dense homes
    for(int y=20;y<26;y++) for(int x=32;x<40;x++) kind[idx(x,y)]=K_I;                         // jobs
    for(int x=18;x<42;x++){ kind[idx(x,19)]=K_ROAD; kind[idx(x,23)]=K_ROAD; kind[idx(x,26)]=K_ROAD; }
    for(int y=19;y<27;y++) for(int x=18;x<42;x+=4) kind[idx(x,y)]=K_ROAD;
    kind[idx(40,23)]=K_PLANT;                          // power present, but NO water
    recompute_static();
    step(80*TICK_EVERY);
    int dry_max=0; for(int y=20;y<26;y++) for(int x=20;x<30;x++){ int v=level[idx(x,y)]; if(v>dry_max)dry_max=v; }
    expect(dry_max <= WATER_DRY_CAP, "without water, development can't rise past the dry cap");
    // add a lake tile + a pump beside it, wired to the grid
    kind[idx(16,23)]=K_WATER; kind[idx(17,23)]=K_PUMP;
    recompute_static();
    step(80*TICK_EVERY);
    int wet_max=0; for(int y=20;y<26;y++) for(int x=20;x<30;x++){ int v=level[idx(x,y)]; if(v>wet_max)wet_max=v; }
    expect(1, str("DBG water: dry max=%d  watered max=%d  (cap=%d)", dry_max, wet_max, WATER_DRY_CAP));
    expect(wet_max > WATER_DRY_CAP, "a lake-side pump lets development rise tall");
}
#endif
