// buildingkit.h — the shared BUILDING CATALOG (cart-land, ADR-0006): one row per building type, in
// OpenStreetMap's own vocabulary, read by BOTH sides of a split:
//   · islandstudio PLACES buildings from it (size, storeys, who gets one, the siting rule), and
//   · backroads DRAWS them from it (roof shape, wall + roof material, landmark flag, tower, sign).
// The draw code and the placement code stay in their carts; this header only NAMES things.
// Design + the measured OSM vocabulary: docs/design/island-buildings.md.
//
// EXPORT FORM (identical to what data-tools/roadview/osm-roads.js writes for a real city):
//   kind "building", sub = the OSM building= value, optionally ";<key>=<value>" for what's in or
//   at it ("yes;amenity=pub", "tower;man_made=lighthouse"); an EMPTY sub = untyped (building=yes);
//   the feature's height field h = metres (levels × 3). bk_from_sub() maps a sub back to a row,
//   for generated and real data alike (unknown values fall back by their building= token, else
//   BK_YES).
//
// RULES: ids are APPEND-ONLY (like osm-roads.js KIND_IX): never reorder or delete a row, add new
// ones at the end before BK_N. Everything is static const: no state, safe in any number of carts.
#ifndef BUILDINGKIT_H
#define BUILDINGKIT_H
#include <string.h>

enum {                       // APPEND ONLY
    BK_YES, BK_HOUSE, BK_SEMI, BK_BUNGALOW, BK_TERRACE, BK_APARTMENTS, BK_RETAIL, BK_CHURCH,
    BK_FARM, BK_BARN, BK_PUB, BK_FUEL, BK_FUEL_CANOPY, BK_SCHOOL, BK_TOWNHALL, BK_POST_OFFICE,
    BK_SUPERMARKET, BK_LIGHTHOUSE, BK_WINDMILL, BK_MAST, BK_CASTLE, BK_WATER_TOWER, BK_MONUMENT,
    BK_CHAPEL, BK_COWSHED, BK_STABLE, BK_SILO, BK_INDUSTRIAL, BK_WAREHOUSE, BK_HOTEL, BK_RUINS,
    BK_N
};
enum { BK_ROOF_GABLE, BK_ROOF_HIP, BK_ROOF_FLAT, BK_ROOF_NONE };                  // NONE: towers, masts
enum { BK_WALL_BRICK, BK_WALL_WHITEWASH, BK_WALL_TIMBER, BK_WALL_STONE, BK_WALL_CONCRETE };
enum { BK_RMAT_TILE, BK_RMAT_SLATE, BK_RMAT_THATCH, BK_RMAT_METAL };
enum { BK_CAP_NONE, BK_CAP_SPIRE, BK_CAP_BATTLEMENT, BK_CAP_SAILS, BK_CAP_LAMP, BK_CAP_DOME };
// siting (islandstudio's side): where a type goes
enum { BK_SITE_FRONTAGE, BK_SITE_HIGH_STREET, BK_SITE_CENTRE, BK_SITE_EDGE_MAIN_ROAD, BK_SITE_COAST,
       BK_SITE_HARBOUR, BK_SITE_HILLTOP, BK_SITE_HEADLAND, BK_SITE_SUMMIT, BK_SITE_MOOR,
       BK_SITE_RIVER, BK_SITE_FARMYARD, BK_SITE_NONE };
// who gets one (islandstudio's side)
enum { BK_WHO_ANY, BK_WHO_TOWN, BK_WHO_VILLAGE_UP, BK_WHO_BIGGEST_TOWN, BK_WHO_FARM, BK_WHO_ISLAND, BK_WHO_NONE };
enum { BK_STATUS_LATER, BK_STATUS_PLACED };     // does islandstudio place it yet?

typedef struct { float u, w, h; unsigned char cap; } BkTower;   // u: offset along the frontage (-0.5..0.5 of w), w/h metres
typedef struct {
    const char *building, *tag;        // OSM building= value; optional second layer "key=value" (or NULL)
    float w0, w1, d0, d1;              // footprint: frontage w, depth d (m)
    unsigned char lv0, lv1;            // storeys
    unsigned char roof, wall, rmat;    // look (backroads)
    unsigned char landmark;            // drawn as a skyline silhouette from far away
    BkTower tower;                     // h = 0: none
    const char *sign;                  // painted on the road-facing facade, or NULL
    unsigned char site, who, status;   // placement (islandstudio)
} BkType;

static const BkType BK[BK_N] = {
    //  building        tag                      w0  w1  d0  d1 lv  roof          wall               rmat            lm tower {u,w,h,cap}                  sign      site                    who                 status
    [BK_YES]         = { "yes",          NULL,                   6, 12,  6, 12, 1, 2, BK_ROOF_GABLE, BK_WALL_BRICK,     BK_RMAT_TILE,   0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_NONE,           BK_WHO_NONE,        BK_STATUS_LATER },
    [BK_HOUSE]       = { "house",        NULL,                   7, 12,  8, 13, 1, 2, BK_ROOF_GABLE, BK_WALL_BRICK,     BK_RMAT_TILE,   0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_FRONTAGE,       BK_WHO_ANY,         BK_STATUS_PLACED },
    [BK_SEMI]        = { "semidetached_house", NULL,            12, 16,  9, 11, 2, 2, BK_ROOF_HIP,   BK_WALL_BRICK,     BK_RMAT_TILE,   0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_FRONTAGE,       BK_WHO_VILLAGE_UP,  BK_STATUS_LATER },
    [BK_BUNGALOW]    = { "bungalow",     NULL,                   9, 13,  8, 11, 1, 1, BK_ROOF_HIP,   BK_WALL_WHITEWASH, BK_RMAT_TILE,   0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_FRONTAGE,       BK_WHO_VILLAGE_UP,  BK_STATUS_LATER },
    [BK_TERRACE]     = { "terrace",      NULL,                  18, 36,  9, 12, 2, 3, BK_ROOF_GABLE, BK_WALL_BRICK,     BK_RMAT_SLATE,  0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_FRONTAGE,       BK_WHO_TOWN,        BK_STATUS_PLACED },
    [BK_APARTMENTS]  = { "apartments",   NULL,                  15, 30, 12, 16, 3, 4, BK_ROOF_FLAT,  BK_WALL_CONCRETE,  BK_RMAT_METAL,  0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_CENTRE,         BK_WHO_BIGGEST_TOWN, BK_STATUS_LATER },
    [BK_RETAIL]      = { "retail",       NULL,                   8, 14, 10, 14, 2, 3, BK_ROOF_GABLE, BK_WALL_BRICK,     BK_RMAT_SLATE,  0, { 0, 0, 0, 0 },                     "SHOP",   BK_SITE_HIGH_STREET,    BK_WHO_TOWN,        BK_STATUS_PLACED },
    [BK_CHURCH]      = { "church",       NULL,                  12, 12, 26, 26, 3, 3, BK_ROOF_GABLE, BK_WALL_STONE,     BK_RMAT_SLATE,  1, { -0.5f, 5, 24, BK_CAP_SPIRE },      NULL,     BK_SITE_CENTRE,         BK_WHO_VILLAGE_UP,  BK_STATUS_PLACED },
    [BK_FARM]        = { "farm",         NULL,                   9, 13,  8, 11, 2, 2, BK_ROOF_GABLE, BK_WALL_WHITEWASH, BK_RMAT_SLATE,  0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_FARMYARD,       BK_WHO_FARM,        BK_STATUS_PLACED },
    [BK_BARN]        = { "barn",         NULL,                  18, 32, 12, 20, 1, 1, BK_ROOF_GABLE, BK_WALL_TIMBER,    BK_RMAT_METAL,  0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_FARMYARD,       BK_WHO_FARM,        BK_STATUS_PLACED },
    [BK_PUB]         = { "yes",          "amenity=pub",         10, 14, 10, 14, 2, 2, BK_ROOF_GABLE, BK_WALL_WHITEWASH, BK_RMAT_SLATE,  0, { 0, 0, 0, 0 },                     "PUB",    BK_SITE_CENTRE,         BK_WHO_VILLAGE_UP,  BK_STATUS_PLACED },
    [BK_FUEL]        = { "retail",       "amenity=fuel",        10, 12,  7,  9, 1, 1, BK_ROOF_FLAT,  BK_WALL_WHITEWASH, BK_RMAT_METAL,  0, { 0, 0, 0, 0 },                     "FUEL",   BK_SITE_EDGE_MAIN_ROAD, BK_WHO_TOWN,        BK_STATUS_PLACED },
    [BK_FUEL_CANOPY] = { "roof",         "amenity=fuel",        14, 18, 10, 12, 1, 1, BK_ROOF_FLAT,  BK_WALL_CONCRETE,  BK_RMAT_METAL,  0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_EDGE_MAIN_ROAD, BK_WHO_TOWN,        BK_STATUS_PLACED },
    [BK_SCHOOL]      = { "school",       NULL,                  30, 50, 20, 30, 1, 2, BK_ROOF_FLAT,  BK_WALL_BRICK,     BK_RMAT_METAL,  0, { 0, 0, 0, 0 },                     "SCHOOL", BK_SITE_FRONTAGE,       BK_WHO_VILLAGE_UP,  BK_STATUS_LATER },
    [BK_TOWNHALL]    = { "civic",        "amenity=townhall",    20, 30, 15, 20, 2, 3, BK_ROOF_HIP,   BK_WALL_STONE,     BK_RMAT_SLATE,  0, { 0, 4, 8, BK_CAP_DOME },           NULL,     BK_SITE_CENTRE,         BK_WHO_TOWN,        BK_STATUS_LATER },
    [BK_POST_OFFICE] = { "retail",       "amenity=post_office",  8, 12, 10, 10, 2, 2, BK_ROOF_GABLE, BK_WALL_BRICK,     BK_RMAT_SLATE,  0, { 0, 0, 0, 0 },                     "POST",   BK_SITE_HIGH_STREET,    BK_WHO_TOWN,        BK_STATUS_LATER },
    [BK_SUPERMARKET] = { "retail",       "shop=supermarket",    30, 50, 25, 40, 1, 1, BK_ROOF_FLAT,  BK_WALL_CONCRETE,  BK_RMAT_METAL,  0, { 0, 0, 0, 0 },                     "MARKET", BK_SITE_EDGE_MAIN_ROAD, BK_WHO_TOWN,        BK_STATUS_LATER },
    [BK_LIGHTHOUSE]  = { "tower",        "man_made=lighthouse",  6,  6,  6,  6, 1, 1, BK_ROOF_NONE,  BK_WALL_WHITEWASH, BK_RMAT_METAL,  1, { 0, 6, 26, BK_CAP_LAMP },          NULL,     BK_SITE_HEADLAND,       BK_WHO_ISLAND,      BK_STATUS_PLACED },
    [BK_WINDMILL]    = { "windmill",     NULL,                   8,  8,  8,  8, 1, 1, BK_ROOF_NONE,  BK_WALL_TIMBER,    BK_RMAT_THATCH, 1, { 0, 7, 16, BK_CAP_SAILS },         NULL,     BK_SITE_HILLTOP,        BK_WHO_ISLAND,      BK_STATUS_PLACED },
    [BK_MAST]        = { "tower",        "man_made=mast",        4,  4,  4,  4, 1, 1, BK_ROOF_NONE,  BK_WALL_CONCRETE,  BK_RMAT_METAL,  1, { 0, 2, 60, BK_CAP_NONE },          NULL,     BK_SITE_SUMMIT,         BK_WHO_ISLAND,      BK_STATUS_PLACED },
    [BK_CASTLE]      = { "castle",       "historic=castle",     40, 60, 30, 50, 2, 3, BK_ROOF_NONE,  BK_WALL_STONE,     BK_RMAT_SLATE,  1, { -0.5f, 10, 22, BK_CAP_BATTLEMENT }, NULL,   BK_SITE_HILLTOP,        BK_WHO_BIGGEST_TOWN, BK_STATUS_PLACED },
    [BK_WATER_TOWER] = { "water_tower",  NULL,                   8,  8,  8,  8, 1, 1, BK_ROOF_NONE,  BK_WALL_CONCRETE,  BK_RMAT_METAL,  1, { 0, 8, 25, BK_CAP_DOME },          NULL,     BK_SITE_EDGE_MAIN_ROAD, BK_WHO_TOWN,        BK_STATUS_LATER },
    [BK_MONUMENT]    = { "yes",          "historic=monument",    4,  4,  4,  4, 1, 1, BK_ROOF_NONE,  BK_WALL_STONE,     BK_RMAT_SLATE,  1, { 0, 3, 10, BK_CAP_SPIRE },         NULL,     BK_SITE_SUMMIT,         BK_WHO_ISLAND,      BK_STATUS_LATER },
    [BK_CHAPEL]      = { "chapel",       NULL,                   8,  8, 14, 14, 2, 2, BK_ROOF_GABLE, BK_WALL_STONE,     BK_RMAT_SLATE,  0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_CENTRE,         BK_WHO_NONE,        BK_STATUS_LATER },
    [BK_COWSHED]     = { "cowshed",      NULL,                  20, 30, 10, 14, 1, 1, BK_ROOF_GABLE, BK_WALL_TIMBER,    BK_RMAT_METAL,  0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_FARMYARD,       BK_WHO_FARM,        BK_STATUS_LATER },
    [BK_STABLE]      = { "stable",       NULL,                  12, 20,  8, 10, 1, 1, BK_ROOF_GABLE, BK_WALL_TIMBER,    BK_RMAT_TILE,   0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_FARMYARD,       BK_WHO_FARM,        BK_STATUS_LATER },
    [BK_SILO]        = { "silo",         NULL,                   5,  5,  5,  5, 1, 1, BK_ROOF_NONE,  BK_WALL_CONCRETE,  BK_RMAT_METAL,  1, { 0, 5, 18, BK_CAP_DOME },          NULL,     BK_SITE_FARMYARD,       BK_WHO_FARM,        BK_STATUS_LATER },
    [BK_INDUSTRIAL]  = { "industrial",   NULL,                  30, 60, 20, 40, 1, 1, BK_ROOF_FLAT,  BK_WALL_CONCRETE,  BK_RMAT_METAL,  0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_EDGE_MAIN_ROAD, BK_WHO_TOWN,        BK_STATUS_LATER },
    [BK_WAREHOUSE]   = { "warehouse",    NULL,                  30, 60, 20, 40, 1, 1, BK_ROOF_FLAT,  BK_WALL_CONCRETE,  BK_RMAT_METAL,  0, { 0, 0, 0, 0 },                     NULL,     BK_SITE_EDGE_MAIN_ROAD, BK_WHO_TOWN,        BK_STATUS_LATER },
    [BK_HOTEL]       = { "hotel",        NULL,                  20, 30, 12, 15, 3, 3, BK_ROOF_HIP,   BK_WALL_WHITEWASH, BK_RMAT_SLATE,  0, { 0, 0, 0, 0 },                     "HOTEL",  BK_SITE_HIGH_STREET,    BK_WHO_TOWN,        BK_STATUS_LATER },
    [BK_RUINS]       = { "ruins",        "historic=ruins",      20, 20, 40, 40, 1, 1, BK_ROOF_NONE,  BK_WALL_STONE,     BK_RMAT_SLATE,  1, { -0.5f, 6, 12, BK_CAP_NONE },      NULL,     BK_SITE_RIVER,          BK_WHO_ISLAND,      BK_STATUS_LATER },
};

// the export sub for a type: "<building>" or "<building>;<tag>". Static strings, built once.
static const char *bk_sub(int t) {
    static char buf[BK_N][48]; static int ready;
    if (!ready) {
        for (int i = 0; i < BK_N; i++) {
            if (BK[i].tag) { strcpy(buf[i], BK[i].building); strcat(buf[i], ";"); strcat(buf[i], BK[i].tag); }
            else strcpy(buf[i], strcmp(BK[i].building, "yes") ? BK[i].building : "");   // untyped = empty, as OSM
        }
        ready = 1;
    }
    return (t >= 0 && t < BK_N) ? buf[t] : "";
}
// a building sub (generated or real OSM) -> catalog row. Exact building+tag match first, then the
// building= token alone (the first row with that value and no tag), else BK_YES.
static int bk_from_sub(const char *sub) {
    if (!sub || !*sub) return BK_YES;
    const char *semi = strchr(sub, ';');
    size_t bl = semi ? (size_t)(semi - sub) : strlen(sub);
    const char *tag = semi ? semi + 1 : NULL;
    int by_building = -1;
    for (int i = 0; i < BK_N; i++) {
        if (strlen(BK[i].building) != bl || strncmp(BK[i].building, sub, bl)) continue;
        if (tag && BK[i].tag && !strcmp(BK[i].tag, tag)) return i;
        if (!BK[i].tag && by_building < 0) by_building = i;
    }
    if (by_building >= 0) return by_building;
    if (tag) for (int i = 0; i < BK_N; i++) if (BK[i].tag && !strcmp(BK[i].tag, tag)) return i;   // "yes;amenity=pub" etc.
    if (bl == 4 && !strncmp(sub, "shop", 4)) return BK_RETAIL;   // islandstudio's first export said "shop"
    return BK_YES;
}
#endif
