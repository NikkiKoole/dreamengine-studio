STATUS: BUILDING (2026-11-20) · islandstudio places buildings, backroads draws them; catalog = runtime/buildingkit.h

# Island buildings: an OSM-shaped catalog

**The idea.** islandstudio decides *where* buildings go and exports them; backroads decides how
they look (split agreed with the maker). The building vocabulary is **OpenStreetMap's own**, so a
generated island exports exactly what `data-tools/roadview/osm-roads.js` writes for a real city, and
backroads draws a generated island and real Delft with the same code.

Related: [island-studio.md](island-studio.md) (the generator, "Houses"), [backroads.md](backroads.md)
(the renderer).

## What real OSM gives us (measured 2026-11-20)

Five fetched cities (Delft, Helmond, Breda, Rotterdam-Maas, central London; ~340,000 buildings)
carry **147 distinct `building=*` values**. 42 % are `yes` (untyped) and 40 % `house`. Then come
`apartments`, `shed`, `garage`, `industrial`, `retail`, `commercial`, `office` and `terrace`. The
ones that make a place read as a place are rarer: `school` 514, `church` 284, `hotel` 148,
`hospital` 80, `train_station` 78, `castle` 52, `chapel`, `tower`, `pub`, `fire_station`,
`cathedral`, `windmill`, `water_tower`, `museum`, `library`, `police_station`, `ruins`,
`monument`. Farms use `farm` (the farmhouse), `barn`, `cowshed`, `stable`, `sty`, `greenhouse` and
`silo`.

OSM describes a building in **two layers**:
1. `building=*`: what the structure is (house, church, barn).
2. `amenity=*` / `shop=*` / `tourism=*` / `historic=*` / `man_made=*`: what is in or at it (a pub
   is often `building=yes` + `amenity=pub`; a lighthouse is `man_made=lighthouse`).

`osm-roads.js` keeps only layer 1 for buildings today (the IR's `sub`), plus a height in metres
(the `.rvb`'s per-feature float `h`: OSM `height`, or `building:levels` × 3).

## Export format (proposed: OSM-identical)

| field | value | real OSM equivalent |
|---|---|---|
| kind | `building` | any `building=*` way |
| `sub` | the `building=*` value (`house`, `church`, …); empty = `yes` | exactly what `osm-roads.js` writes |
| `sub`, 2nd token | optional `;<key>=<value>` for layer 2, e.g. `pub;amenity=pub` | a follow-up for `osm-roads.js` |
| `h` (rvb float / JSON `"h"`) | height in metres = levels × 3 (+ roof allowance for churches etc.) | OSM `height` / `building:levels` × 3 |
| pts | closed ring, 4 corners + the first repeated | the footprint |

Changes from the first house export (`"h=<storeys>;t=<type>"`): storeys move to the real height
field; `t=shop` becomes `retail`; and the **legacy farm diamonds go away**. Real OSM's untyped
buildings also have an empty `sub`, so "empty sub = farm marker" can't survive; backroads reads
farmsteads from `farm` footprints instead.

## The catalog

One row per type we generate. Columns: OSM tags · footprint (m) · levels · who gets it · siting ·
status (**now** = placed today, **next** = this round, **later** = a row only).

### Everyday fill (along streets)

| tags | footprint | levels | who | siting | status |
|---|---|---|---|---|---|
| `house` | 7-12 × 8-13 | 1-2 (towns 2) | every settlement | street frontage, set back 4-8 m | now |
| `semidetached_house` | 12-16 × 9-11 | 2 | towns, villages | street frontage, town edge | next |
| `bungalow` | 9-13 × 8-11 | 1 | villages, town edge | street frontage | next |
| `terrace` | 18-36 × 9-12 (a whole row) | 2-3 | towns | core street frontage | now |
| `apartments` | 15-30 × 12-16 | 3-4 | biggest town only | core | later |
| `retail` | 8-14 × 10-14 | 2-3 | towns | high street (secondary/main in the core) | now (as `shop`) |
| `garage`, `shed` | 3-6 × 5-7 | 1 | behind houses | plot back | later |

### Services (per settlement, by rank)

| tags | footprint | levels | who | siting | status |
|---|---|---|---|---|---|
| `church` | 12 × 26 | tall (h ≈ 12 + tower) | every town and village | nearest fit to the centre | now |
| `yes;amenity=pub` | 10-14 × 10-14 | 2 | every town, most villages | high street / village crossroads | next |
| `school` | 30-50 × 20-30 | 1-2 | towns; villages over a size | town edge, beside a sports field | next |
| `retail;amenity=post_office` | 8-12 × 10 | 2 | towns, big villages | high street | next |
| `civic;amenity=townhall` | 20-30 × 15-20 | 2-3 | towns | the centre, facing the main road | next |
| `retail;shop=supermarket` | 30-50 × 25-40 | 1 | towns | town edge on a main road | next |
| `yes;amenity=fuel` | 12 × 8 + canopy | 1 | towns; main roads between | main road at the town edge | next |
| `fire_station` | 20 × 15 | 2 | towns | main road | later |
| `yes;amenity=police` | 15 × 12 | 2 | towns | centre | later |
| `hospital` | 40-70 × 30-50 | 3 | biggest town only | town edge | later |
| `hotel` | 20-30 × 12-15 | 3 | towns, coastal villages | high street / sea front | later |
| `industrial`, `warehouse` | 30-60 × 20-40 | 1 | towns | edge, on a main road | later |
| `train_station` | 30 × 12 | 1-2 | (once rail exists) | — | later |
| harbour: `boathouse`, `yes;amenity=marketplace` | 12-20 × 8-15 | 1 | coastal towns at a river mouth | the shore | later |

### Farms

| tags | footprint | levels | who | siting | status |
|---|---|---|---|---|---|
| `farm` (farmhouse) | 9-13 × 8-11 | 2 | every farm | the farm cell | now |
| `barn` | 18-32 × 12-20 | 1 (tall) | 1-2 per farm | 22-40 m off the farmhouse | now |
| `cowshed`, `stable`, `silo` | various | 1 | some farms (grass vs crops) | farmyard | later |

### Landmarks (sited by the land, mostly one per island)

| tags | footprint | who | siting | status |
|---|---|---|---|---|
| `yes;man_made=lighthouse` | 6 × 6, h 20-30 | 1-3 per island | headlands (land most surrounded by sea) | next |
| `castle;historic=castle` | 40-60 × 30-50 | 0-1 per island | a hill above a town, within ~3 km | next |
| `windmill` | 8 × 8, h 15 | a few | exposed farmland hills | next |
| `yes;historic=monument` | 4 × 4, h 10 | 0-1 | a summit with a view | next |
| `tower;man_made=mast` | 4 × 4, h 60 | 1 | the highest peak | next |
| `water_tower` | 8 × 8, h 25 | towns | high ground at the town edge (backroads places one now) | next |
| `ruins;historic=ruins` (abbey) | 20 × 40 | 0-1 | a sheltered valley | later |
| `yes;historic=archaeological_site` (stone circle) | 30 × 30, h 2 | 0-1 | moor | later |
| `yes;man_made=watermill` | 10 × 12 | 0-2 | a river at a village | later |
| quarry, dam, pier | areas / lines, not buildings | — | rock, lake outlets, harbours | later |

## Where the table lives (agreed with backroads, built 2026-11-20)

**`runtime/buildingkit.h`**: `BK[]`, one row per type, ids **append-only** (like `KIND_IX`). Each row
has the OSM tags; footprint and storey ranges; the look backroads asked for (roof shape
GABLE/HIP/FLAT/NONE, wall BRICK/WHITEWASH/TIMBER/STONE/CONCRETE, roof TILE/SLATE/THATCH/METAL, a
LANDMARK flag for skyline silhouettes, a tower spec `{u, w, h, cap}` with cap SPIRE/BATTLEMENT/
SAILS/LAMP/DOME, a facade sign); and islandstudio's placement (siting enum, who gets one, placed
yet). `bk_sub(t)` gives the export `sub`; `bk_from_sub(sub)` maps any building sub, generated or
real OSM, back to a row. Draw code stays in backroads, placement code in islandstudio.

### First round placed (2026-11-20)

The export switched to the OSM-identical form (sub = `building=` value + optional tag, the height
field in metres, the legacy farm diamonds dropped). Newly placed, in backroads' driving order:

- **Fuel** (`retail;amenity=fuel` kiosk + `roof;amenity=fuel` canopy): every town on a secondary or
  main road near its edge, plus one per ~12 km of main road in open country. The canopy sits by the
  road and the kiosk behind a 7 m forecourt.
- **Lighthouses**: shore cells scored by how much sea surrounds them on three rings (a headland),
  1-3 per island, 8 km apart.
- **Windmills**: farmland hilltops at least 15 m above their 700 m ring, 2-5 per island, 5 km apart.
- **Mast**: the highest peak. **Castle**: the best hill above the biggest town (within 3 km, at
  least 40 m higher).
- **Pubs**: every town near its centre, ~80 % of villages.

Landmarks may sit on slopes up to 45 % (houses 20 %). Seed 3 @1024: 7,793 buildings (35 pubs, 20
fuel stations, 3 lighthouses, 5 windmills, 1 mast, 1 castle), 0 overlapping a road. Open: the
castle and lighthouses have no access track yet (done next round).

## Open

- `osm-roads.js` could keep a building's layer-2 tag (`amenity`/`shop`/`historic`/`man_made`), so
  real cities carry pubs, schools and lighthouses as well.
- Counts and budget: seed 3 at 1024 has ~7,700 buildings today; backroads asked for "a few
  thousand". Thinning knobs belong in the table (keep-rate per rank).

### Second round: variety and access roads (2026-11-20)

- **Homes mix**: outside a town's core a street draws from detached houses (15 % of them big),
  `semidetached_house` and `bungalow` (town edge ~35 / 15 / 50 %, villages 20 / 25 / 55 %,
  hamlets bungalow 30 %). Detached homes outside the core turn up to ±4°. About 30 % get a
  `garage` beside, another 25 % a `shed` behind. backroads varies walls, roofs and pitch per
  building from the rows' `vwall`/`vrmat`/`vroof` bitmasks (its fields, b731abfc).
- **Every landmark has an access road**: `landmark_sites()` now picks the sites *before* the road
  network is final (same scoring as before), and each gets a track to the nearest road, routed like a
  farm's (a lane up to the castle; a 60 km-cost budget for the summit mast's service road). The
  building goes beside the track's end afterwards.

Seed 3 @1024: 8,764 buildings (2,697 houses, 1,023 semis, 940 bungalows, 389 terraces, 407
garages, 1,035 sheds, ...); every landmark 9-60 m from its road (was 40 m to 1.9 km); 0 buildings
on a road.
