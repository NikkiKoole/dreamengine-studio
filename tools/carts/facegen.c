#include "studio.h"
#include <math.h>    // sinf for the idle head-bob + smoke wisp

// ── FACEGEN — procedural street portraits + conversation box ───────────────
// The NPC-portrait SYSTEM for the GTA-style world (sits beside streetlab,
// sloop, flank, worldgen, lotfill, interiors). Roll a person off the corner
// and they fill the screen and say their piece: gangsters, mobsters, corner
// boys, dealers, pimps, working girls, fiends — every face built from
// primitives (ovals, tris, lines), every one different.
//
// THE POINT: there is no portrait "engine". A person is just a bag of GENES
// rolled by rnd() — skin tone (pale→very dark), face shape (round / long /
// square / heavy / slim / gaunt), nose (button / straight / wide / Roman /
// pointy), hair, facial hair, and a bitmask of 90s accessories: gold rope
// chains, shades, hoops, bandanas, durags, snapbacks, smokes, gold teeth,
// scars, FACE TATTOOS (teardrops / brow slit / cheek ink), beauty marks. Each
// ROLE biases the roll so a mobster reads mobster and a corner boy reads corner
// boy; women get a "babe" trait some of the time (slim face, doe eyes, lashes,
// arched brows, fuller lips, a Monroe mole) — not all do, the rest are plainer
// or older. Then a role-appropriate NAME and BARK get drawn in a comic speech
// box with a typewriter + a tick.
// Lift the Npc struct + roll_npc() + draw_portrait() straight into the game.
// Rendered in a Hotline-Miami-inked style (heavy outlines, hard planar
// light/shadow under a GTA warm upper-left key, sunken glowing eyes, gritted
// teeth) with per-face asymmetry (jaw/eyes/brows/nose nudged, eyes slanted).
//
//   SPACE / click — new person      Z — same person, another line
//   1-9 then A-H — force a role      X — typewriter tick on/off
//
// ── PARKED IDEAS (for another time — not yet done) ──────────────────────────
//   • Extract facegen.h — pull Npc + roll_npc() + draw_portrait()/draw_avatar()
//     into a reusable header so the actual game #includes it. HIGHEST VALUE next
//     step (the whole point is to lift this into the GTA-style game).
//   • Phase-2 PAPER-DOLL: bake hand-pixeled feature parts (sprite-draw.js →
//     .cart.js) and composite by gene, for true HM/GTA sprite quality. Bigger
//     than a DSL and more useful — a textual DSL was considered and rejected
//     (the drawing is procedural geometry, not declarative content; keep content
//     in the existing data tables instead: SKINS[]/HAIRS[]/TAT_*/role loadouts).
//   • Forehead highlight is a plain oval ("the big circle") — reshape it to a
//     less obviously-round catch-light.
//   • Olive/tan skin (skin idx 2, CLR_MEDIUM_GREY base) reads a touch ashen — tune.
//   • Hair / caps aren't OUTLINED yet, so they sit slightly pasted-on against
//     the inked face — give them the FACE_OL treatment.
//   • More asymmetry dials available: a subtle whole-head tilt, mouth-corner
//     asymmetry. (Done so far: jaw shift, eye height + slant, brow raise, nose.)
//   • Archetype polish: a real apron collar for barista, a jersey number for the
//     baller, light face-tats option for non-"excessive" street roles.
// ───────────────────────────────────────────────────────────────────────────

// ── roles ──────────────────────────────────────────────────────────────────
enum { R_MOBSTER, R_GANGSTER, R_CORNERBOY, R_DEALER, R_PIMP, R_GIRL, R_FIEND, R_CIVILIAN,
       R_BEGGAR, R_LABORER, R_DRUNK, R_SENIOR, R_BALLER, R_SUIT_BIZ, R_BARISTA, R_HISTORIAN, R_TEEN,
       NROLES };
static const char *ROLE_NAME[NROLES] = {
    "MADE MAN", "GANGSTER", "CORNER BOY", "DEALER", "PIMP", "WORKING GIRL", "FIEND", "CIVILIAN",
    "BEGGAR", "LABORER", "DRUNK", "SENIOR", "BALLER", "BUSINESSMAN", "BARISTA", "HISTORIAN", "TEENAGER"
};

// ── expressions ──────────────────────────────────────────────────────────────
enum { X_NEUTRAL, X_SMUG, X_ANGRY, X_WARY, X_LAUGH, X_NERVOUS };
#define X_SAD_FALLBACK X_NERVOUS    // no distinct "sad" face — read as nervous

// ── hair styles ──────────────────────────────────────────────────────────────
enum { H_BALD, H_SHORT, H_FADE, H_AFRO, H_CORNROWS, H_SLICK, H_LONG, H_BUN, H_BOB };

// ── facial hair (men) ─────────────────────────────────────────────────────────
enum { F_NONE, F_STUBBLE, F_MUSTACHE, F_GOATEE, F_FULL };

// ── nose shapes ─────────────────────────────────────────────────────────────────
enum { NS_BUTTON, NS_STRAIGHT, NS_WIDE, NS_ROMAN, NS_POINTY };

// ── face shapes ─────────────────────────────────────────────────────────────────
enum { FS_ROUND, FS_LONG, FS_SQUARE, FS_HEAVY, FS_SLIM, FS_GAUNT };

// ── accessory bits ─────────────────────────────────────────────────────────────
#define A_SHADES   (1<<0)
#define A_CHAIN    (1<<1)
#define A_HOOPS    (1<<2)
#define A_STUD     (1<<3)
#define A_BANDANA  (1<<4)
#define A_DURAG    (1<<5)
#define A_CAP      (1<<6)
#define A_SCARF    (1<<7)
#define A_CIG      (1<<8)
#define A_GOLDTOOTH (1<<9)
#define A_SCAR     (1<<10)
#define A_GRILL    (1<<11)   // full gold grill
#define A_FUR      (1<<12)   // pimp fur collar
#define A_LIPSTICK (1<<13)
#define A_SHADOW   (1<<14)   // eyeshadow / makeup
#define A_BEAUTY   (1<<15)   // beauty mark / mole
#define A_FACETAT  (1<<16)   // face/neck tattoos (teardrop, cheek ink, brow slit)
#define A_GLASSES  (1<<17)   // real spectacles (not shades) — senior/historian/biz/barista
#define A_HEADBAND (1<<18)   // sports headband — baller

// ── clothing collar style ──────────────────────────────────────────────────────
enum { C_SUIT, C_HOODIE, C_TRACKSUIT, C_WIFEBEATER, C_STRAPTOP, C_FURCOAT, C_TEE };

// ── skin ramp: pale → very dark — 3 planar tones (lit highlight / base / shadow)
// for the Hotline-Miami hard-shading look under a GTA-style warm upper-left key.
typedef struct { int hi, base, shade; } Skin;
static const Skin SKINS[] = {
    { CLR_WHITE,          CLR_LIGHT_PEACH,    CLR_PEACH          },  // 0 pale
    { CLR_LIGHT_PEACH,    CLR_PEACH,          CLR_DARK_PEACH     },  // 1 fair
    { CLR_PEACH,          CLR_MEDIUM_GREY,    CLR_DARK_BROWN     },  // 2 olive / tan
    { CLR_MEDIUM_GREY,    CLR_BROWN,          CLR_DARK_BROWN     },  // 3 brown
    { CLR_BROWN,          CLR_DARK_BROWN,     CLR_BROWNISH_BLACK },  // 4 dark brown
    { CLR_DARK_BROWN,     CLR_BROWNISH_BLACK, CLR_BLACK          },  // 5 very dark
};
#define NSKIN 6
#define FACE_OL CLR_BLACK     // the heavy ink outline (Hotline-Miami trait)

// ── hair colors {base, dark} ────────────────────────────────────────────────────
typedef struct { int base, dark; } Hair;
static const Hair HAIRS[] = {
    { CLR_BROWNISH_BLACK, CLR_BLACK },        // 0 black
    { CLR_DARK_BROWN,  CLR_BROWNISH_BLACK },  // 1 dark brown
    { CLR_BROWN,       CLR_DARK_BROWN },      // 2 brown
    { CLR_LIGHT_YELLOW,CLR_ORANGE },          // 3 blonde
    { CLR_LIGHT_GREY,  CLR_DARK_GREY },       // 4 grey
    { CLR_WHITE,       CLR_LIGHT_GREY },      // 5 bleached
    { CLR_DARK_RED,    CLR_DARKER_PURPLE },   // 6 dyed red
};
#define NHAIR 7

// ── an NPC ─────────────────────────────────────────────────────────────────────
typedef struct {
    int role;
    int sex;            // 0 m, 1 f
    int skin;           // index into SKINS
    int face;           // 0 round, 1 long, 2 square, 3 heavy
    int hair, haircol;
    int facial;         // facial-hair style (men)
    int nose;           // nose shape
    int babe;           // women only — got the glamour roll (not all do)
    int wrinkly;        // weathered face — forehead lines, crow's feet, folds
    int heavytat;       // excessive face tattoos (the fully-inked look)
    int collar;         // clothing
    int shirt;          // collar color
    int acc;            // accessory bitmask
    int expr;
    int eyecol;
    char name[28];
    int line;           // which bark
} Npc;

static Npc me;
static int  roll_frame;     // frame() at the moment of the roll
static int  tick_on = 1;
static int  shown_chars;    // typewriter cursor

// ── helper: pick a random element ────────────────────────────────────────────────
// (chance(pct) is a studio.h builtin — a weighted coin flip)
static int pick(const int *arr, int n) { return arr[rnd(n)]; }

// a tiny deterministic LCG — for per-face detail (stubble specks, scar side, tattoo
// placement) that must be STABLE per person, not re-rolled every frame by rnd()
static unsigned prng(unsigned *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }

// ── names ─────────────────────────────────────────────────────────────────────
static const char *IT_FIRST[] = { "Tony","Sal","Vinnie","Paulie","Carmine","Rocco","Frankie","Nicky","Joey","Sonny" };
static const char *IT_LAST[]  = { "Salerno","Gambino","Moretti","Russo","Bruno","Carbone","Lupo","Tagliaferro","Vitale","Cuneo" };
static const char *IT_NICK[]  = { "the Hammer","Knuckles","Big Tony","Fingers","Cheeks","the Ox","Two-Times","the Nose" };
static const char *ST_PREFIX[]= { "Lil'","Big","Young","Mad","Slim","Black" };
static const char *ST_NAME[]  = { "Smokey","Reese","Marlo","Bodie","Tre","D-Loc","Cutty","Wee-Bey","Snoop","Avon","Stringer","Dukie" };
static const char *GIRL_NAME[]= { "Candy","Roxy","Destiny","Mercedes","Lola","Cinnamon","Jade","Star","Bambi","Diamond" };
static const char *CIV_FIRST[]= { "Marcus","Trevor","Deion","Wesley","Andre","Lamar","Carl","Reggie","Dwayne","Otis" };
static const char *FIEND_NAME[]={ "Bubbles","Sherm","Crackhead Bob","Twitch","Greasy Pete","Skeeter","Ronnie","Fester" };
static const char *BUM_NAME[]  = { "Old Pete","Shakes","Lucky","One-Eye","Doc","Stumbles","Gus","Boxcar","Tex","Rags" };
static const char *WORK_NAME[] = { "Hank","Big Joe","Mike","Earl","Buddy","Frank","Dale","Chuck","Manny","Sal" };
static const char *SEN_M[]     = { "Walter","Herb","Stan","Arthur","Mort","Cliff","Norman","Leonard","Sy","Murray" };
static const char *SEN_F[]     = { "Edith","Mabel","Doris","Agnes","Ruth","Gloria","Pearl","Estelle","Mildred","Vera" };
static const char *BALL_NAME[] = { "Tyrone","Jamal","DeShawn","Magic","Reggie","Pippen","Marcus","Darnell","Hakeem","AI" };
static const char *BIZ_NAME[]  = { "Brad","Chad","Sterling","Preston","Trent","Blake","Grant","Mr. Klein","Hunter","Brock" };
static const char *CAFE_NAME[] = { "Dylan","Aiden","River","Sky","Juno","Wren","Felix","Oat","Sage","Birch" };
static const char *HIST_NAME[] = { "Prof. Crane","Dr. Aldous","Bartholomew","Edmund","Cornelius","Dr. Voss","Reginald","Prof. Hale" };
static const char *TEEN_NAME[] = { "Kayden","Brayden","Tyler","Mads","Zoe","Skylar","Jayden","Chloe","Hunter","Brooklyn" };

static void cpy(char *o, const char *s) { int p = 0; while (*s) o[p++] = *s++; o[p] = 0; }

static void make_name(Npc *n) {
    char *o = n->name;
    if (n->role == R_MOBSTER) {
        // First "the Nick" Last  — sometimes
        const char *f = IT_FIRST[rnd(10)], *l = IT_LAST[rnd(10)];
        if (chance(45)) { const char *nk = IT_NICK[rnd(8)];
            int p=0; while(*f) o[p++]=*f++; o[p++]=' '; o[p++]='"';
            while(*nk) o[p++]=*nk++; o[p++]='"'; o[p++]=' ';
            while(*l) o[p++]=*l++; o[p]=0;
        } else {
            int p=0; while(*f) o[p++]=*f++; o[p++]=' '; while(*l) o[p++]=*l++; o[p]=0;
        }
    } else if (n->role == R_GIRL) {
        const char *g = GIRL_NAME[rnd(10)]; int p=0; while(*g) o[p++]=*g++; o[p]=0;
    } else if (n->role == R_FIEND) {
        const char *g = FIEND_NAME[rnd(8)]; int p=0; while(*g) o[p++]=*g++; o[p]=0;
    } else if (n->role == R_CIVILIAN) {
        const char *f = CIV_FIRST[rnd(10)]; int p=0; while(*f) o[p++]=*f++; o[p]=0;
    } else if (n->role == R_BEGGAR || n->role == R_DRUNK) {
        cpy(o, BUM_NAME[rnd(10)]);
    } else if (n->role == R_LABORER) {
        cpy(o, WORK_NAME[rnd(10)]);
    } else if (n->role == R_SENIOR) {
        cpy(o, n->sex ? SEN_F[rnd(10)] : SEN_M[rnd(10)]);
    } else if (n->role == R_BALLER) {
        cpy(o, BALL_NAME[rnd(10)]);
    } else if (n->role == R_SUIT_BIZ) {
        cpy(o, BIZ_NAME[rnd(10)]);
    } else if (n->role == R_BARISTA) {
        cpy(o, CAFE_NAME[rnd(10)]);
    } else if (n->role == R_HISTORIAN) {
        cpy(o, HIST_NAME[rnd(8)]);
    } else if (n->role == R_TEEN) {
        cpy(o, TEEN_NAME[rnd(10)]);
    } else {
        // street alias: maybe a prefix
        int p=0;
        if (chance(55)) { const char *pf = ST_PREFIX[rnd(6)]; while(*pf) o[p++]=*pf++; o[p++]=' '; }
        const char *g = ST_NAME[rnd(12)]; while(*g) o[p++]=*g++; o[p]=0;
    }
}

// ── barks (per role) — picked at roll, expression set from the line ─────────────
typedef struct { const char *l1, *l2; int expr; } Bark;

static const Bark BK_MOBSTER[] = {
    {"You come to ME, on the day of my daughter's wedding?","",X_SMUG},
    {"This thing of ours? You don't talk about it.","Capisce?",X_ANGRY},
    {"Sit down. Have a cannoli. We're family now.","",X_NEUTRAL},
    {"Somebody touched my collection. That's a problem.","A big problem.",X_ANGRY},
    {"I know people. I know EVERYBODY.","",X_SMUG},
};
static const Bark BK_GANGSTER[] = {
    {"This whole block? It belongs to me now.","",X_SMUG},
    {"You rollin' through my hood without a word?","Bad move.",X_ANGRY},
    {"Loyalty over everything. You with us or not?","",X_WARY},
    {"They tried to take the corner. They learned.","",X_SMUG},
    {"Money, power, respect. In that order.","",X_NEUTRAL},
};
static const Bark BK_CORNERBOY[] = {
    {"Yo, I ain't see nothin', officer. For real.","",X_WARY},
    {"Lookout shift again? Man, I'm tired.","",X_NERVOUS},
    {"Pssst. You good? You lookin'?","",X_WARY},
    {"Five-O! Everybody walk it off, walk it off!","",X_NERVOUS},
    {"I'm tryna get up the chain, you feel me?","",X_NEUTRAL},
};
static const Bark BK_DEALER[] = {
    {"First one's a discount. After that, retail.","",X_SMUG},
    {"You want the good stuff or the cheap stuff?","",X_NEUTRAL},
    {"My product speaks for itself, baby.","",X_SMUG},
    {"You owe me. And I always collect.","",X_ANGRY},
    {"Cash only. I don't do tabs no more.","",X_WARY},
};
static const Bark BK_PIMP[] = {
    {"It's all about the presentation, playa.","",X_SMUG},
    {"This coat? Genuine. Like my whole operation.","",X_SMUG},
    {"You disrespect my girls, you disrespect ME.","",X_ANGRY},
    {"Pimpin' ain't easy, but somebody gotta do it.","",X_LAUGH},
};
static const Bark BK_GIRL[] = {
    {"You lookin' for a good time, sugar?","",X_SMUG},
    {"Fifty up front, honey. I don't do favors.","",X_NEUTRAL},
    {"Be a gentleman or beat it.","",X_WARY},
    {"Cold night to be workin' this corner.","",X_NERVOUS},
    {"You got that look. I like that look.","",X_LAUGH},
};
static const Bark BK_FIEND[] = {
    {"Hey hey hey, I'll watch ya car real good!","Real good!",X_NERVOUS},
    {"Spare a dollar? Quarter? Anything, man.","",X_NERVOUS},
    {"I got information. Good information. Cheap.","",X_WARY},
    {"I used to be somebody, ya know. SOMEBODY.","",X_SAD_FALLBACK},
    {"Just need a little somethin' to take the edge off.","",X_NERVOUS},
};
static const Bark BK_CIV[] = {
    {"I don't want any trouble, alright?","",X_NERVOUS},
    {"This neighborhood used to be safe.","",X_SAD_FALLBACK},
    {"You didn't see me. I wasn't here.","",X_WARY},
    {"Just tryin' to get home in one piece.","",X_NERVOUS},
    {"Whatever you're selling, I ain't buying.","",X_NEUTRAL},
};

static const Bark BK_BEGGAR[] = {
    {"Spare some change? God bless ya.","",X_NERVOUS},
    {"Ain't eaten since yesterday, friend.","",X_SAD_FALLBACK},
    {"I'll watch your car real good. Real good.","",X_NERVOUS},
    {"Used to have a house. A car. A wife.","",X_SAD_FALLBACK},
    {"Anything helps. Even a smile.","",X_NEUTRAL},
};
static const Bark BK_LABORER[] = {
    {"Twelve-hour shift and the boss still ain't happy.","",X_NEUTRAL},
    {"Mind the wet cement, pal.","",X_NEUTRAL},
    {"Honest day's work for a dishonest day's pay.","",X_WARY},
    {"This whole block? My crew poured it.","",X_SMUG},
    {"Clock out at five. Beer's at five-oh-one.","",X_LAUGH},
};
static const Bark BK_DRUNK[] = {
    {"Heyyy... I know you. I KNOW you, right?","",X_LAUGH},
    {"One more for the road, barkeep!","",X_LAUGH},
    {"I'm not drunk, YOU'RE... you're blurry.","",X_NERVOUS},
    {"The room's spinnin' but I'm standin' still.","",X_LAUGH},
    {"Lemme tell ya 'bout my ex-wife...","",X_SAD_FALLBACK},
};
static const Bark BK_SENIOR[] = {
    {"Back in MY day, this was all orange groves.","",X_NEUTRAL},
    {"You kids and your telephones.","",X_WARY},
    {"Get off my lawn! ...please.","",X_ANGRY},
    {"I've seen this city change three times over.","",X_NEUTRAL},
    {"Would you like a butterscotch, dear?","",X_LAUGH},
};
static const Bark BK_BALLER[] = {
    {"Game point. Check it up.","",X_SMUG},
    {"Ain't nobody on this court can guard me.","",X_SMUG},
    {"You want next? Get in line.","",X_NEUTRAL},
    {"Ball is life, my man. Ball is life.","",X_LAUGH},
    {"And ONE! Put that on the highlight reel.","",X_LAUGH},
};
static const Bark BK_BIZ[] = {
    {"Let's circle back and touch base offline.","",X_NEUTRAL},
    {"Time is money, and you're wasting both.","",X_WARY},
    {"I closed three deals before your coffee.","",X_SMUG},
    {"Synergy. That's what this city needs.","",X_SMUG},
    {"My lawyer will be in contact.","",X_ANGRY},
};
static const Bark BK_BARISTA[] = {
    {"Oat milk, half-caff, extra hot? Coming up.","",X_NEUTRAL},
    {"We actually source these beans ourselves.","",X_SMUG},
    {"Name for the cup? ...I'll just guess.","",X_LAUGH},
    {"That'll be seven-fifty for the small.","",X_NEUTRAL},
    {"The wifi password is on the chalkboard.","",X_NEUTRAL},
};
static const Bark BK_HISTORIAN[] = {
    {"This district predates the founding, you know.","",X_NEUTRAL},
    {"Ah, but the archives tell a different story.","",X_SMUG},
    {"Those who forget the past... well. You know.","",X_WARY},
    {"I've a monograph on precisely this matter.","",X_NEUTRAL},
    {"Fascinating. Truly fascinating.","",X_LAUGH},
};
static const Bark BK_TEEN[] = {
    {"Ugh, this is SO boring. Can we go?","",X_NERVOUS},
    {"Whatever. You wouldn't get it.","",X_WARY},
    {"My mom's gonna kill me. Don't tell her.","",X_NERVOUS},
    {"Bet. That's actually kinda fire though.","",X_SMUG},
    {"Can I get a ride? Gas money's on me. Maybe.","",X_NEUTRAL},
};

static const Bark *bark_set(int role, int *count) {
    switch (role) {
        case R_MOBSTER:  *count = 5; return BK_MOBSTER;
        case R_GANGSTER: *count = 5; return BK_GANGSTER;
        case R_CORNERBOY:*count = 5; return BK_CORNERBOY;
        case R_DEALER:   *count = 5; return BK_DEALER;
        case R_PIMP:     *count = 4; return BK_PIMP;
        case R_GIRL:     *count = 5; return BK_GIRL;
        case R_FIEND:    *count = 5; return BK_FIEND;
        case R_BEGGAR:   *count = 5; return BK_BEGGAR;
        case R_LABORER:  *count = 5; return BK_LABORER;
        case R_DRUNK:    *count = 5; return BK_DRUNK;
        case R_SENIOR:   *count = 5; return BK_SENIOR;
        case R_BALLER:   *count = 5; return BK_BALLER;
        case R_SUIT_BIZ: *count = 5; return BK_BIZ;
        case R_BARISTA:  *count = 5; return BK_BARISTA;
        case R_HISTORIAN:*count = 5; return BK_HISTORIAN;
        case R_TEEN:     *count = 5; return BK_TEEN;
        default:         *count = 5; return BK_CIV;
    }
}

// ── roll a person ───────────────────────────────────────────────────────────────
static void roll_npc(Npc *n, int forced_role) {
    n->role = (forced_role >= 0) ? forced_role : rnd(NROLES);
    n->acc = 0;
    n->babe = 0;
    // face shape — biased toward leaner faces; slim/long are the common case now
    n->face = pick((const int[]){FS_ROUND, FS_LONG, FS_LONG, FS_SLIM, FS_SLIM, FS_SLIM, FS_SQUARE, FS_HEAVY}, 8);
    // nose shape
    n->nose = rnd(5);
    n->eyecol = pick((const int[]){CLR_DARK_BROWN, CLR_BROWNISH_BLACK, CLR_BLUE_GREEN, CLR_DARK_GREY, CLR_BROWN}, 5);

    // sex: most roles are men; working girl always woman
    if (n->role == R_GIRL) n->sex = 1;
    else if (n->role == R_CIVILIAN) n->sex = chance(45) ? 1 : 0;
    else n->sex = chance(15) ? 1 : 0;

    // skin tone — biased per role for flavor, but anyone can be anything
    if (n->role == R_MOBSTER)      n->skin = pick((const int[]){0,1,1,2,2}, 5);   // pale/olive
    else if (n->role == R_FIEND)   n->skin = pick((const int[]){0,1,2,3,4}, 5);
    else                            n->skin = rnd(NSKIN);

    // hair color: mostly natural, mobsters can grey, gangsters can bleach
    if (n->skin >= 3)              n->haircol = pick((const int[]){0,0,1}, 3);    // dark skin → black/dk brown
    else                            n->haircol = rnd(NHAIR);
    if (n->role == R_MOBSTER && chance(35)) n->haircol = 4;                        // greying made man
    if (n->role == R_GANGSTER && chance(20)) n->haircol = 5;                       // bleached tips

    // hair style
    if (n->sex) n->hair = pick((const int[]){H_LONG, H_LONG, H_BUN, H_BOB, H_CORNROWS}, 5);
    else        n->hair = pick((const int[]){H_SHORT, H_FADE, H_BALD, H_AFRO, H_CORNROWS, H_SLICK}, 6);

    // facial hair (men)
    n->facial = F_NONE;
    if (!n->sex) n->facial = pick((const int[]){F_NONE,F_STUBBLE,F_STUBBLE,F_MUSTACHE,F_GOATEE,F_FULL}, 6);

    // ── role loadouts ─────────────────────────────────────────────
    switch (n->role) {
    case R_MOBSTER:
        n->collar = C_SUIT;
        if (!n->sex) n->hair = chance(70) ? H_SLICK : H_SHORT;
        if (chance(70) && !n->sex) n->facial = chance(50) ? F_MUSTACHE : F_NONE;
        if (chance(35)) n->acc |= A_CIG;
        if (chance(55)) n->acc |= A_CHAIN;
        if (chance(20)) n->acc |= A_SCAR;
        n->shirt = pick((const int[]){CLR_DARK_GREY, CLR_BLACK, CLR_DARKER_BLUE, CLR_DARKER_PURPLE}, 4);
        break;
    case R_GANGSTER:
        n->collar = chance(50) ? C_TRACKSUIT : C_TEE;
        if (chance(45)) n->acc |= A_BANDANA;
        else if (chance(40)) n->acc |= A_DURAG;
        else if (chance(40)) n->acc |= A_CAP;
        if (chance(75)) n->acc |= A_CHAIN;
        if (chance(45)) n->acc |= A_SHADES;
        if (chance(30)) n->acc |= A_GOLDTOOTH;
        if (chance(25)) n->acc |= A_SCAR;
        if (chance(35)) n->acc |= A_FACETAT;            // teardrops / cheek ink
        if (chance(20) && !n->sex) n->acc |= A_STUD;
        n->shirt = pick((const int[]){CLR_RED, CLR_BLUE, CLR_DARK_GREEN, CLR_WHITE, CLR_DARK_PURPLE}, 5);
        break;
    case R_CORNERBOY:
        n->collar = C_HOODIE;
        n->face = chance(60) ? FS_ROUND : FS_SLIM;      // younger / leaner
        if (chance(45)) n->acc |= A_DURAG;
        else if (chance(50)) n->acc |= A_CAP;
        if (chance(35)) n->acc |= A_CHAIN;
        if (chance(20)) n->acc |= A_FACETAT;
        if (chance(20) && !n->sex) n->acc |= A_STUD;
        n->facial = chance(70) ? F_NONE : F_STUBBLE;   // young
        n->shirt = pick((const int[]){CLR_DARK_GREY, CLR_DARK_GREEN, CLR_BROWN, CLR_DARK_BLUE}, 4);
        break;
    case R_DEALER:
        n->collar = chance(50) ? C_TRACKSUIT : C_TEE;
        if (chance(65)) n->acc |= A_SHADES;
        if (chance(80)) n->acc |= A_CHAIN;
        if (chance(45)) n->acc |= A_GOLDTOOTH;
        if (chance(30)) n->acc |= A_GRILL;
        if (chance(30)) n->acc |= A_CIG;
        if (chance(30)) n->acc |= A_FACETAT;
        if (chance(25) && !n->sex) n->acc |= A_STUD;
        n->shirt = pick((const int[]){CLR_BLACK, CLR_RED, CLR_DARK_PURPLE, CLR_DARKER_BLUE}, 4);
        break;
    case R_PIMP:
        n->collar = C_FURCOAT;
        n->acc |= A_FUR;
        if (chance(80)) n->acc |= A_CHAIN;
        if (chance(60)) n->acc |= A_SHADES;
        if (chance(70)) n->acc |= A_CAP;     // wide-brim → drawn as fur hat
        if (chance(55)) n->acc |= A_GOLDTOOTH;
        if (chance(15)) n->acc |= A_FACETAT;
        if (chance(35)) n->facial = F_GOATEE;
        n->shirt = pick((const int[]){CLR_DARK_PURPLE, CLR_DARK_RED, CLR_DARK_GREEN, CLR_INDIGO}, 4);
        break;
    case R_GIRL:
        n->collar = C_STRAPTOP;
        n->hair = pick((const int[]){H_LONG, H_LONG, H_BUN, H_BOB}, 4);
        n->acc |= A_LIPSTICK | A_SHADOW;
        if (chance(75)) n->acc |= A_HOOPS;
        if (chance(30)) n->acc |= A_CHAIN;
        if (chance(20)) n->acc |= A_SCARF;
        n->babe = chance(55);                           // some are stunners, some rough/older
        if (!n->babe && chance(20)) n->acc |= A_FACETAT;
        n->shirt = pick((const int[]){CLR_PINK, CLR_RED, CLR_DARK_PURPLE, CLR_BLUE, CLR_LIME_GREEN}, 5);
        break;
    case R_FIEND:
        n->collar = chance(50) ? C_WIFEBEATER : C_TEE;
        n->hair = pick((const int[]){H_SHORT, H_AFRO, H_BALD}, 3);
        n->face = chance(55) ? FS_GAUNT : n->face;      // sunken, hollow
        n->facial = chance(60) ? F_STUBBLE : F_FULL;   // unkempt
        if (chance(40)) n->acc |= A_CAP;
        if (chance(35)) n->acc |= A_SCAR;
        if (chance(20)) n->acc |= A_FACETAT;
        n->shirt = pick((const int[]){CLR_DARK_GREY, CLR_BROWN, CLR_DARK_GREEN, CLR_LIGHT_GREY}, 4);
        break;
    case R_BEGGAR:
        n->collar = chance(50) ? C_HOODIE : C_TEE;       // layered, ragged
        n->hair = pick((const int[]){H_SHORT, H_BALD, H_AFRO}, 3);
        n->facial = chance(75) ? F_FULL : F_STUBBLE;     // unkempt beard
        if (chance(60)) n->acc |= A_CAP;                 // a knit beanie
        if (chance(30)) n->acc |= A_SCAR;
        n->shirt = pick((const int[]){CLR_DARK_GREY, CLR_BROWN, CLR_DARK_GREEN, CLR_DARK_BROWN}, 4);
        break;
    case R_LABORER:
        n->collar = C_TEE;
        n->hair = pick((const int[]){H_SHORT, H_BALD, H_SLICK}, 3);
        n->facial = chance(60) ? F_STUBBLE : (chance(50)?F_MUSTACHE:F_NONE);
        if (chance(70)) n->acc |= A_CAP;                 // hard hat / ball cap
        n->shirt = pick((const int[]){CLR_ORANGE, CLR_YELLOW, CLR_TRUE_BLUE, CLR_DARK_RED, CLR_DARK_GREEN}, 5);
        break;
    case R_DRUNK:
        n->collar = chance(50) ? C_TEE : C_HOODIE;
        n->hair = pick((const int[]){H_SHORT, H_BALD, H_SLICK, H_AFRO}, 4);
        n->facial = chance(70) ? F_STUBBLE : F_FULL;
        n->nose = chance(60) ? NS_WIDE : NS_ROMAN;       // a boozer's nose
        if (chance(35)) n->acc |= A_CAP;
        n->shirt = pick((const int[]){CLR_DARK_RED, CLR_DARK_GREEN, CLR_BROWN, CLR_INDIGO}, 4);
        break;
    case R_SENIOR:
        n->sex = chance(45) ? 1 : 0;
        n->collar = chance(50) ? C_SUIT : C_TEE;         // cardigan-ish / shirt
        n->hair = n->sex ? pick((const int[]){H_BUN, H_BOB, H_SHORT}, 3)
                         : pick((const int[]){H_BALD, H_BALD, H_SHORT}, 3);
        n->haircol = 4;                                  // grey/white
        if (!n->sex) n->facial = chance(40) ? F_MUSTACHE : F_NONE;
        if (chance(70)) n->acc |= A_GLASSES;
        n->shirt = pick((const int[]){CLR_LIGHT_GREY, CLR_INDIGO, CLR_DARK_GREEN, CLR_BROWN, CLR_DARK_PURPLE}, 5);
        break;
    case R_BALLER:
        n->collar = C_WIFEBEATER;                        // a jersey/tank
        n->face = chance(60) ? FS_SLIM : FS_LONG;
        n->hair = pick((const int[]){H_FADE, H_SHORT, H_CORNROWS, H_BALD}, 4);
        n->facial = chance(60) ? F_NONE : F_STUBBLE;
        n->acc |= A_HEADBAND;
        if (chance(40)) n->acc |= A_CHAIN;
        if (chance(25)) n->acc |= A_STUD;
        n->shirt = pick((const int[]){CLR_RED, CLR_TRUE_BLUE, CLR_DARK_PURPLE, CLR_WHITE, CLR_DARK_GREEN}, 5);
        break;
    case R_SUIT_BIZ:
        n->collar = C_SUIT;
        n->hair = chance(60) ? H_SLICK : H_SHORT;
        n->facial = chance(75) ? F_NONE : F_STUBBLE;     // clean-cut
        if (chance(35)) n->acc |= A_GLASSES;
        n->shirt = pick((const int[]){CLR_TRUE_BLUE, CLR_DARK_RED, CLR_DARK_PURPLE, CLR_INDIGO}, 4);
        break;
    case R_BARISTA:
        n->sex = chance(40) ? 1 : 0;
        n->collar = C_TEE;
        n->hair = n->sex ? pick((const int[]){H_BUN, H_LONG, H_BOB}, 3)
                         : pick((const int[]){H_BUN, H_SHORT, H_SLICK}, 3);   // man-bun
        if (!n->sex) n->facial = chance(70) ? F_FULL : F_GOATEE;   // hipster beard
        if (chance(45)) n->acc |= A_GLASSES;
        if (chance(35)) n->acc |= A_CAP;                 // beanie
        if (n->sex && chance(50)) n->acc |= A_HOOPS;
        n->shirt = pick((const int[]){CLR_DARK_GREEN, CLR_DARK_RED, CLR_BROWN, CLR_MEDIUM_GREEN, CLR_INDIGO}, 5);
        break;
    case R_HISTORIAN:
        n->sex = chance(35) ? 1 : 0;
        n->collar = C_SUIT;                              // tweed
        n->hair = n->sex ? pick((const int[]){H_BUN, H_BOB}, 2)
                         : pick((const int[]){H_BALD, H_SHORT, H_SLICK}, 3);
        if (chance(55)) n->haircol = 4;                  // greying
        if (!n->sex) n->facial = chance(60) ? F_FULL : F_GOATEE;
        n->acc |= A_GLASSES;                             // always bespectacled
        n->shirt = pick((const int[]){CLR_DARK_GREEN, CLR_BROWN, CLR_DARK_RED, CLR_INDIGO}, 4);
        break;
    case R_TEEN:
        n->sex = chance(45) ? 1 : 0;
        n->collar = chance(55) ? C_HOODIE : C_TEE;
        n->face = chance(60) ? FS_ROUND : FS_SLIM;       // young, smooth
        n->facial = F_NONE;
        if (n->sex) { n->acc |= A_SHADOW; if (chance(40)) n->acc |= A_HOOPS; n->babe = chance(35); }
        if (chance(45)) n->acc |= A_CAP;
        if (chance(20)) n->acc |= A_GLASSES;
        n->shirt = pick((const int[]){CLR_RED, CLR_TRUE_BLUE, CLR_LIME_GREEN, CLR_PINK, CLR_DARK_PURPLE, CLR_ORANGE}, 6);
        break;
    default: // civilian
        n->collar = chance(40) ? C_TEE : C_HOODIE;
        if (n->sex) { n->acc |= A_SHADOW; if (chance(50)) n->acc |= A_HOOPS; n->babe = chance(30); }
        if (chance(20)) n->acc |= A_CAP;
        if (chance(25)) n->acc |= A_GLASSES;
        n->shirt = pick((const int[]){CLR_BLUE, CLR_DARK_GREEN, CLR_LIGHT_GREY, CLR_ORANGE, CLR_INDIGO}, 5);
        break;
    }
    if (n->sex && (n->acc & A_BANDANA)) n->acc &= ~A_BANDANA;  // tidy

    // ── the glamour roll (women only — NOT everyone) ──────────────────────
    if (n->babe) {
        n->face   = pick((const int[]){FS_SLIM, FS_SLIM, FS_ROUND, FS_LONG}, 4);  // soft, lean
        n->nose   = pick((const int[]){NS_BUTTON, NS_BUTTON, NS_POINTY, NS_STRAIGHT}, 4);
        n->hair   = pick((const int[]){H_LONG, H_LONG, H_BOB, H_BUN}, 4);
        n->haircol= pick((const int[]){0, 1, 2, 3}, 4);     // no grey
        n->acc   |= A_LIPSTICK | A_SHADOW;
        n->acc   &= ~(A_SCAR | A_FACETAT);                  // flawless
        if (chance(45)) n->acc |= A_BEAUTY;                 // a Monroe mole
    } else if (n->sex) {
        if (chance(40)) n->nose = NS_WIDE;                  // plainer features
        if (chance(25)) n->haircol = 4;                     // older / greying
    }

    // ── weathered / wrinkly (some faces, never the babes) ────────────────
    n->wrinkly = 0;
    if (n->role == R_SENIOR) n->wrinkly = 1;                // always
    else if (!n->babe && n->role != R_TEEN && n->role != R_BALLER) {
        int p = 8;                                          // most are smooth
        if (n->haircol == 4)                 p = 75;        // greying → old
        if (n->role == R_MOBSTER)            p += 25;       // veteran made men
        if (n->role == R_FIEND || n->role == R_BEGGAR || n->role == R_DRUNK) p += 45; // hard-living
        if (n->role == R_HISTORIAN)          p += 45;       // bookish & aged
        if (n->role == R_LABORER)            p += 25;       // weathered outdoors
        if (n->face == FS_HEAVY || n->face == FS_GAUNT) p += 20;
        n->wrinkly = chance(p);
    }

    // ── excessive face tattoos (the fully-inked look) — a few, on the street ──
    n->heavytat = 0;
    if ((n->acc & A_FACETAT) && !n->babe) {
        int p = (n->role==R_GANGSTER || n->role==R_DEALER) ? 40
              : (n->role==R_CORNERBOY || n->role==R_FIEND) ? 30 : 0;
        n->heavytat = chance(p);
    }

    // pick a bark
    int nb; bark_set(n->role, &nb);
    n->line = rnd(nb);
    const Bark *bs = bark_set(n->role, &nb);
    n->expr = bs[n->line].expr;

    make_name(n);
}

// ── geometry of the current head (set by draw_portrait, used by accessories) ────
static int FCX, FCY, HW, VH;   // center x/y, half-width, half-height (top half of head)
static int JW, JH;             // jaw half-width / half-height (set per face shape)
static int JAW_DX;             // jaw pushed off-centre for facial asymmetry

static void hair_back(Npc *n);
static void hair_front(Npc *n);
static int  pick_cap(Npc *n);

// Draw the whole head SILHOUETTE (skull + jaw + chin) as one mass, translated by
// (dx,dy) and grown by `grow` px, in one colour. Calling it at a few offsets/sizes
// stacks shadow → base → highlight so the planar shading conforms to the head
// shape (no spilling, no blobs) — the core of the Hotline-Miami face look.
static void face_mass(Npc *n, int dx, int dy, int grow, int color) {
    int jx = FCX + dx + JAW_DX;   // jaw shifted off-centre → asymmetric face shape
    ovalfill(FCX + dx, FCY + dy, HW + grow, VH + grow, color);
    ovalfill(jx, FCY + 24 + dy, JW + grow, JH + grow, color);
    if (n->face == FS_SQUARE || n->face == FS_HEAVY)
        ovalfill(jx, FCY + 22 + dy, JW + 2 + grow, JH - 4 + grow, color);
    if (n->sex || n->face == FS_SLIM || n->face == FS_GAUNT)
        trifill(jx - JW + 2, FCY + 22 + dy,
                jx + JW - 2, FCY + 22 + dy,
                jx,          FCY + JH + 24 + dy, color);
}

// tattoo content — three flavours, all drawn in the in-game bitmap fonts:
//   WORDS    — gang/criminal lettering, tilted
//   SYMS     — CP437 control-code glyphs (☺☻♥♦♣♠♫☼ arrows ‼ § ¶ …); codepoints
//              1–31 are single-byte UTF-8 so they render as those glyphs (skip
//              10/13 = newline/CR). Cryptic occult/gang ink.
//   ART      — tiny multi-line ASCII-art motifs (crosses, skull, dagger, RIP)
static const char *TAT_WORDS[] = {
    "MONEY","LOYAL","PAID","SOLO","213","RIP","MOB","CASH","KING","PAIN",
    "HUSTLE","BLESSED","OUTLAW","TRUST","REST","ACE","SUR","NORTE","DEATH","HEAT",
};
#define NTAT 20
static const char *TAT_SYMS[] = {
    "\x06\x05",      // ♠♣  card suits
    "\x03\x04",      // ♥♦
    "\x0F",          // ☼   sun / compass
    "\x02",          // ☻   dark smiley
    "\x0E\x0E",      // ♫♫  notes
    "\x1E\x1F",      // ▲▼
    "\x18\x19",      // ↑↓
    "\x13",          // ‼
    "\x14\x15",      // ¶§
    "\x0B\x0C",      // ♂♀
};
#define NSYM 10
static const char *TAT_ART[] = {
    " + \n+++\n + ",          // cross
    "\x02\n/|\\",             // ☻ skull over shoulders
    "RIP\n___",               // headstone
    "/\\\n\\/",               // diamond/blade
    ">+<",                    // single-line dagger-ish
    "[\x06]",                 // [♠] boxed spade
};
#define NART 6
static void tat_text(const char *s, int fnt, int x, int y, float deg, int col) {
    font(fnt);
    print_rot_scaled(s, x, y, deg, 1, col, 1);
    font(FONT_NORMAL);
}
// multi-line ascii-art tattoo (own newline split — drawn flat, centred-ish on x)
static void tat_art(const char *s, int fnt, int x, int y, int col) {
    font(fnt);
    int lh = (fnt == FONT_TINY) ? 6 : 7;
    char ln[16]; int li = 0, row = 0;
    for (int i = 0;; i++) {
        char c = s[i];
        if (c == '\n' || c == 0) {
            ln[li] = 0;
            print(ln, x - text_width(ln)/2, y + row*lh, col);
            li = 0; row++;
            if (c == 0) break;
        } else if (li < 15) ln[li++] = c;
    }
    font(FONT_NORMAL);
}

// gritted teeth — a dark mouth band with vertical light teeth (the HM grimace)
static void teeth(int cx, int cy, int w, int gold) {
    rectfill(cx - w, cy - 3, w*2, 6, FACE_OL);                 // dark gum/lip frame
    rectfill(cx - w + 1, cy - 1, w*2 - 2, 3, gold ? CLR_YELLOW : CLR_LIGHT_PEACH);
    for (int t = -w + 2; t < w - 1; t += 3)
        line(cx + t, cy - 1, cx + t, cy + 1, FACE_OL);         // tooth gaps
}

// ── portrait ─────────────────────────────────────────────────────────────────
static void draw_portrait(Npc *n, int talking, int mouth_open) {
    int sk = SKINS[n->skin].base, skd = SKINS[n->skin].shade;
    int hc = HAIRS[n->haircol].base, hcd = HAIRS[n->haircol].dark;

    // head metrics by face shape
    int bobx = (int)(sinf(frame()*0.04f)*1.5f);
    FCX = 160 + bobx;
    FCY = 78;
    switch (n->face) {
        case FS_ROUND:  HW = 45; VH = 55; break;
        case FS_LONG:   HW = 39; VH = 64; break;
        case FS_SQUARE: HW = 50; VH = 54; break;
        case FS_HEAVY:  HW = 54; VH = 58; break;
        case FS_SLIM:   HW = 37; VH = 60; break;   // narrow, lean
        case FS_GAUNT:  HW = 35; VH = 62; break;   // hollow, sunken
        default:        HW = 45; VH = 56; break;
    }
    if (n->sex) { HW -= 3; VH -= 1; }

    // per-face seed (deterministic from genes, NOT frame) — drives stable detail
    // (stubble/scar/tattoo placement) AND the facial asymmetry below
    unsigned seed = (unsigned)(n->skin*131 + n->face*17 + n->nose*101
                               + n->haircol*7 + n->role*23 + n->shirt*3 + 1);
    // ── subtle facial ASYMMETRY — real faces (and HM/GTA portraits) aren't
    //    mirror-symmetric. Small, deterministic per-face nudges. ───────────
    int asx     = (seed & 1) ? 1 : -1;             // overall lean
    JAW_DX      = asx * (1 + (int)((seed >> 1) & 1));   // jaw 1-2px to one side
    int eldy    = ((seed >> 2) & 1) ? asx : 0;     // eyes tilt: one up, one down
    int erdy    = -eldy;
    int browL   = asx > 0 ? 2 : ((seed >> 4) & 1); // one brow raised higher
    int browR   = asx < 0 ? 2 : ((seed >> 5) & 1);
    int nosedx  = asx * (int)((seed >> 6) & 1);    // nose off-centre 0-1px

    // ── shoulders / clothing (behind, runs off the bottom) ──────────────
    int shy = FCY + VH + 28;
    int shc = n->shirt;
    ovalfill(FCX, shy + 40, HW + 58, 60, shc);
    // collar detail
    if (n->collar == C_SUIT) {
        // dark jacket + white shirt V + tie
        ovalfill(FCX, shy + 40, HW + 58, 60, CLR_BLACK);
        trifill(FCX - 26, shy, FCX + 26, shy, FCX, shy + 40, CLR_WHITE);    // shirt V
        trifill(FCX - 14, shy + 2, FCX + 14, shy + 2, FCX, shy + 34, n->shirt); // tie
        // lapels
        trifill(FCX - 30, shy - 4, FCX - 4, shy + 6, FCX - 26, shy + 40, CLR_DARK_GREY);
        trifill(FCX + 30, shy - 4, FCX + 4, shy + 6, FCX + 26, shy + 40, CLR_DARK_GREY);
    } else if (n->collar == C_HOODIE) {
        ovalfill(FCX, shy + 44, HW + 56, 56, shc);
        // hood bunched at the neck
        ovalfill(FCX - HW - 6, shy + 6, 22, 30, shc);
        ovalfill(FCX + HW + 6, shy + 6, 22, 30, shc);
        // drawstrings
        line(FCX - 8, shy + 6, FCX - 8, shy + 30, CLR_WHITE);
        line(FCX + 8, shy + 6, FCX + 8, shy + 30, CLR_WHITE);
    } else if (n->collar == C_TRACKSUIT) {
        // zip-up with two color stripes on the shoulders
        rectfill(FCX - 2, shy - 2, 4, 50, CLR_DARK_GREY);   // zipper line
        for (int s = -1; s <= 1; s += 2) {
            line(FCX + s*30, shy + 4, FCX + s*52, shy + 40, CLR_WHITE);
            line(FCX + s*34, shy + 4, FCX + s*56, shy + 40, CLR_WHITE);
        }
    } else if (n->collar == C_WIFEBEATER) {
        // bare shoulders + thin white straps
        ovalfill(FCX, shy + 40, HW + 58, 60, sk);
        ovalfill(FCX, shy + 40, HW + 58, 60, skd);       // shaded chest
        trifill(FCX - 30, shy - 6, FCX - 10, shy - 6, FCX - 18, shy + 50, CLR_WHITE);
        trifill(FCX + 30, shy - 6, FCX + 10, shy - 6, FCX + 18, shy + 50, CLR_WHITE);
    } else if (n->collar == C_STRAPTOP) {
        ovalfill(FCX, shy + 38, HW + 56, 58, sk);        // bare shoulders
        rectfill(FCX - HW - 30, shy + 30, (HW+30)*2, 40, n->shirt);  // low top
        trifill(FCX - 22, shy + 30, FCX + 22, shy + 30, FCX, shy + 50, sk); // neckline V
    } else if (n->collar == C_FURCOAT) {
        ovalfill(FCX, shy + 44, HW + 64, 58, n->shirt);
        // fur trim — fuzzy oval band
        for (int i = -HW - 40; i <= HW + 40; i += 6)
            circfill(FCX + i, shy + 6 + (i*i)/600, 7, n->haircol==4?CLR_LIGHT_GREY:CLR_WHITE);
    } else { // TEE
        ovalfill(FCX, shy + 42, HW + 54, 56, shc);
        trifill(FCX - 18, shy - 2, FCX + 18, shy - 2, FCX, shy + 22, sk); // crew neck skin
    }

    // gold rope chain sits on the chest (drawn here so collar covers its ends)
    if (n->acc & A_CHAIN) {
        for (int a = -60; a <= 60; a += 7) {
            int cxp = FCX + a;
            int cyp = shy + 8 + (a*a)/90;
            circfill(cxp, cyp, 2, CLR_YELLOW);
            pset(cxp, cyp - 1, CLR_LIGHT_YELLOW);
        }
        if (n->role == R_PIMP || (n->acc & A_GRILL))   // dollar-sign medallion
            { circfill(FCX, shy + 48, 6, CLR_YELLOW); print("$", FCX-2, shy+44, CLR_DARK_ORANGE); }
    }

    // ── hair behind the head ──────────────────────────────────────────
    hair_back(n);

    // ── ears (+ earrings) ─────────────────────────────────────────────
    circfill(FCX - HW + 2, FCY + 14, 7, sk);
    circfill(FCX + HW - 2, FCY + 14, 7, sk);
    circfill(FCX - HW + 2, FCY + 14, 3, skd);
    circfill(FCX + HW - 2, FCY + 14, 3, skd);
    if (n->acc & A_HOOPS) {
        circ(FCX - HW + 1, FCY + 22, 5, CLR_YELLOW);
        circ(FCX + HW - 1, FCY + 22, 5, CLR_YELLOW);
    } else if (n->acc & A_STUD) {
        circfill(FCX + HW - 2, FCY + 19, 2, CLR_YELLOW);
    }

    // ── head — outlined, hard planar shading (HM look, GTA upper-left key) ──
    int skhi = SKINS[n->skin].hi;
    // jaw / chin metrics — wide & square for heavy, narrow & pointed for slim/women
    JW = HW - 8;
    if (n->face == FS_SQUARE)                            JW = HW - 3;
    else if (n->face == FS_LONG)                         JW = HW - 13;
    else if (n->face == FS_SLIM || n->face == FS_GAUNT)  JW = HW - 11;
    if (n->sex) JW -= 3;                                 // softer feminine chin
    JH = VH - (n->face==FS_LONG ? 8 : 16);
    // 1) heavy ink outline (the silhouette grown +2, behind everything)
    face_mass(n, 0, 0, 2, FACE_OL);
    // 2) shadow side — the whole face filled in the darkest skin tone
    face_mass(n, 0, 0, 0, skd);
    // 3) base/lit skin — same mass nudged up-left & shrunk, so shadow stays as a
    //    crescent on the lower-right rim and the lit edge loses its outline
    face_mass(n, -3, -2, -2, sk);
    // 4) highlight — a forehead catch-light up-left (NOT a half-face wash)
    ovalfill(FCX - 8, FCY - VH/2 - 2, HW - 18, VH/3, skhi);
    // gaunt: sunken hollows carved under the cheekbones (symmetric → reads gaunt)
    if (n->face == FS_GAUNT) {
        ovalfill(FCX - HW + 11, FCY + 16, 7, 13, skd);
        ovalfill(FCX + HW - 12, FCY + 16, 7, 13, skd);
    }
    // cheekbone catch-light hint (upper-left cheek)
    ovalfill(FCX - HW/2, FCY + 4, 6, 4, skhi);
    // blush — a soft rosy cheek for the babes (subtle; reads only on pale skin)
    if (n->babe && n->skin <= 1) {
        ovalfill(FCX - HW/2 + 2, FCY + 15, 5, 2, CLR_DARK_PEACH);
        ovalfill(FCX + HW/2 - 2, FCY + 15, 5, 2, CLR_DARK_PEACH);
    }
    int jw = JW, jh = JH;   // local aliases for the feature code below

    // ── hair in front / hairline ──────────────────────────────────────
    hair_front(n);

    // ── eyebrows ──────────────────────────────────────────────────────
    int ey = FCY - 4;
    int exo = HW - 18;               // eye x offset
    int lx = FCX - exo, rx = FCX + exo;
    int browc = (n->haircol==5) ? CLR_LIGHT_GREY : hcd;
    int brow_y = ey - 12;
    int brow_drop = 0, brow_in = 0;
    if (n->expr == X_ANGRY)  { brow_drop = 3; brow_in = 4; }
    if (n->expr == X_SMUG)   { brow_drop = -1; }
    if (n->expr == X_WARY)   { brow_drop = 2; }
    if (n->expr == X_NERVOUS){ brow_drop = -3; }
    int lby = brow_y + brow_drop - browL;   // left brow (asymmetric raise)
    int rby = brow_y + brow_drop - browR;   // right brow
    if (!(n->acc & A_SHADES)) {
        if (n->sex) {
            // thin arched brows — two short strokes peaking in the middle
            line(lx - 8, lby + 3, lx,     lby - brow_in, browc);
            line(lx,     lby - brow_in, lx + 7, lby + 2, browc);
            line(rx + 8, rby + 3, rx,     rby - brow_in, browc);
            line(rx,     rby - brow_in, rx - 7, rby + 2, browc);
        } else {
            for (int t = 0; t < 3; t++) {
                line(lx - 9, lby + t, lx + 7, lby + t - brow_in, browc);
                line(rx + 9, rby + t, rx - 7, rby + t - brow_in, browc);
            }
        }
    }

    // ── eyes ──────────────────────────────────────────────────────────
    int blink = (frame() % 220) < 6;     // occasional full blink
    // occasional idle SQUINT — eyes narrow to near-slits for a beat (a hard
    // stare / sizing-you-up look), offset from the blink so they don't collide
    int idle_squint = !blink && (frame() % 330) >= 300;
    int squint = (n->expr == X_ANGRY || n->expr == X_SMUG || n->expr == X_WARY);
    int ew = 8, eh = squint ? 3 : 6;
    if (n->sex) { eh = squint ? 4 : 7; }              // women: rounder (taller) eyes, same width
    if (idle_squint) eh = 2;                          // narrowed to a menacing slit
    int irc = eh;                                     // iris ~fills the eye height (almond, not buggy)
    if (n->babe) irc = eh + 1;                        // big doe eyes for the babes
    int pup = n->babe ? 3 : 2;
    int ley = ey + eldy, rey = ey + erdy;   // eyes sit at slightly different heights
    // per-eye ROTATION — outer corner lifted by a few px; the two eyes tilt by
    // DIFFERENT amounts so the face isn't mirror-symmetric
    int tl = 1 + (int)((seed >> 8) & 1);    // left eye tilt 1-2px
    int tr = (int)((seed >> 9) % 3);        // right eye tilt 0-2px
    if (blink || idle_squint) { tl = 0; tr = 0; }
    if (!(n->acc & A_SHADES)) {
        // sunken sockets — a darker recess under the brow ridge (HM intensity)
        ovalfill(lx, ley - 1, ew + 3, eh + 3, skd);
        ovalfill(rx, rey - 1, ew + 3, eh + 3, skd);
        if (n->acc & A_SHADOW) {  // eyeshadow over the socket
            ovalfill(lx, ley - 3, ew + 2, 4, n->babe ? CLR_DARK_PURPLE : CLR_DARKER_PURPLE);
            ovalfill(rx, rey - 3, ew + 2, 4, n->babe ? CLR_DARK_PURPLE : CLR_DARKER_PURPLE);
        }
        if (blink) {
            line(lx - ew, ley, lx + ew, ley, FACE_OL);
            line(rx - ew, rey, rx + ew, rey, FACE_OL);
        } else {
            ovalfill(lx, ley, ew, eh, CLR_WHITE);
            ovalfill(rx, rey, ew, eh, CLR_WHITE);
            // shave the white into a slanted almond (socket-tone masks): for each
            // eye, cut the inner-top and outer-bottom corners along the tilt
            trifill(lx - 1, ley - eh - 1, lx + ew + 1, ley - eh - 1, lx + ew + 1, ley - eh + tl, skd);
            trifill(lx + 1, ley + eh + 1, lx - ew - 1, ley + eh + 1, lx - ew - 1, ley + eh - tl, skd);
            trifill(rx + 1, rey - eh - 1, rx - ew - 1, rey - eh - 1, rx - ew - 1, rey - eh + tr, skd);
            trifill(rx - 1, rey + eh + 1, rx + ew + 1, rey + eh + 1, rx + ew + 1, rey + eh - tr, skd);
            int look = (n->expr==X_WARY) ? 2 : 0;   // shifty eyes glance aside
            // iris fills the eye (almond), with a glowing inner ring + bright catchlight
            circfill(lx + 1 + look, ley, irc, n->eyecol);
            circfill(rx + 1 + look, rey, irc, n->eyecol);
            circfill(lx + 1 + look, ley, pup, CLR_BLACK);
            circfill(rx + 1 + look, rey, pup, CLR_BLACK);
            pset(lx + look - 1, ley - 1, CLR_WHITE); pset(lx + look, ley - 1, CLR_WHITE);
            pset(rx + look - 1, rey - 1, CLR_WHITE); pset(rx + look, rey - 1, CLR_WHITE);
        }
        // heavy dark upper lid (2px), slanted by the per-eye tilt (outer up)
        line(lx - ew - 1, ley - eh - tl, lx + ew, ley - eh + tl, FACE_OL);
        line(lx - ew,     ley - eh + 1 - tl, lx + ew, ley - eh + 1 + tl, FACE_OL);
        line(rx + ew + 1, rey - eh - tr, rx - ew, rey - eh + tr, FACE_OL);
        line(rx + ew,     rey - eh + 1 - tr, rx - ew, rey - eh + 1 + tr, FACE_OL);
        // tired lower-lid / bag line — slanted to match
        line(lx - ew + 1, ley + eh + 1 - tl, lx + ew - 1, ley + eh + 1 + tl, skd);
        line(rx - ew + 1, rey + eh + 1 + tr, rx + ew - 1, rey + eh + 1 - tr, skd);
        // mascara lashes — outer-corner flick (women), lifted with the tilt
        if (n->sex) {
            line(lx - ew, ley - eh - tl + 1, lx - ew - 3, ley - eh - tl - 2, CLR_BLACK);
            line(rx + ew, rey - eh - tr + 1, rx + ew + 3, rey - eh - tr - 2, CLR_BLACK);
        }
    }

    // ── nose — a real protruding form: shadow on the right, lit bridge on the
    //    left (upper-left key), dark nostrils underneath (HM/GTA dimensionality)
    int ny = FCY + 14;
    int nx = FCX + nosedx;                   // nose off-centre (asymmetry)
    switch (n->nose) {
    case NS_BUTTON:                          // small, short, upturned
        ovalfill(nx + 1, ny + 1, 3, 4, skd);
        ovalfill(nx - 1, ny, 2, 3, skhi);
        pset(nx - 2, ny + 4, FACE_OL); pset(nx + 2, ny + 4, FACE_OL);
        break;
    case NS_WIDE:                            // broad, flared nostrils
        ovalfill(nx + 1, ny + 1, 8, 6, skd);
        ovalfill(nx - 2, ny, 4, 6, skhi);
        line(nx - 7, ny + 4, nx - 5, ny + 6, FACE_OL);   // nostril wings
        line(nx + 7, ny + 4, nx + 5, ny + 6, FACE_OL);
        pset(nx - 5, ny + 4, FACE_OL); pset(nx + 5, ny + 4, FACE_OL);
        break;
    case NS_ROMAN:                           // long, a bump on the bridge
        ovalfill(nx + 1, ny - 2, 4, 13, skd);
        ovalfill(nx - 1, ny - 4, 2, 12, skhi);
        ovalfill(nx + 2, ny - 5, 2, 4, skd);     // the bump
        pset(nx - 3, ny + 8, FACE_OL); pset(nx + 3, ny + 8, FACE_OL);
        line(nx - 3, ny + 8, nx + 3, ny + 9, FACE_OL);
        break;
    case NS_POINTY:                          // narrow, refined, longer tip
        ovalfill(nx + 1, ny, 2, 10, skd);
        ovalfill(nx - 1, ny - 1, 2, 8, skhi);
        ovalfill(nx, ny + 6, 3, 3, skd);         // little tip
        pset(nx - 2, ny + 7, FACE_OL); pset(nx + 2, ny + 7, FACE_OL);
        break;
    default:                                 // NS_STRAIGHT — the classic
        ovalfill(nx + 1, ny, 5, 7, skd);
        ovalfill(nx - 2, ny - 1, 3, 6, skhi);
        line(nx - 3, ny + 5, nx + 3, ny + 6, FACE_OL);   // under-tip shadow
        pset(nx - 3, ny + 5, FACE_OL); pset(nx + 3, ny + 5, FACE_OL);
        break;
    }
    // drunk flush — rosy cheeks + a reddened nose (reads on lighter skin)
    if (n->role == R_DRUNK && n->skin <= 3) {
        ovalfill(FCX - HW/2, FCY + 12, 7, 4, CLR_DARK_PEACH);
        ovalfill(FCX + HW/2, FCY + 12, 7, 4, CLR_DARK_PEACH);
        ovalfill(nx, ny + 3, 3, 3, CLR_DARK_PEACH);
    }
    // teenage acne — a few small red spots scattered on the cheeks/forehead
    if (n->role == R_TEEN) {
        unsigned s = seed ^ 0x51A3u;
        for (int i = 0; i < 5; i++) {
            int ax = FCX - HW + 8 + (int)(prng(&s) % (HW*2 - 16));
            int ay = FCY - 6 + (int)(prng(&s) % 32);
            pset(ax, ay, CLR_DARK_PEACH);
        }
    }

    // ── mouth ─────────────────────────────────────────────────────────
    int my = FCY + 30;
    int lip = (n->acc & A_LIPSTICK) ? (n->shirt==CLR_RED?CLR_DARK_RED:CLR_RED) : -1;
    int mw = 12;
    if (talking && mouth_open) {
        ovalfill(FCX, my, mw - 2, 5, CLR_DARKER_PURPLE);     // open mouth
        if (n->acc & A_GRILL)      rectfill(FCX-8, my-3, 16, 3, CLR_YELLOW);
        else                        ovalfill(FCX, my - 2, mw - 4, 2, CLR_WHITE); // teeth
        if (lip >= 0) { ovalfill(FCX, my - 4, mw, 2, lip); ovalfill(FCX, my + 4, mw, 2, lip); }
    } else if (n->sex && lip >= 0 && n->expr != X_LAUGH) {
        // fuller painted lips — the pout
        int lw = mw - 2;
        if (n->expr == X_SMUG)  { line(FCX - lw, my + 1, FCX + lw, my - 2, skd); }
        ovalfill(FCX, my - 1, lw, n->babe ? 3 : 2, lip);       // upper lip
        ovalfill(FCX, my + 2, lw + 1, n->babe ? 3 : 2, lip);   // fuller lower lip
        line(FCX - lw + 1, my, FCX + lw - 1, my, skd);         // the seam
        pset(FCX - 2, my - 1, CLR_PINK);                       // gloss highlight
        pset(FCX + 2, my + 2, CLR_PINK);
    } else {
        if (n->expr == X_LAUGH) {
            ovalfill(FCX, my, mw, 4, CLR_DARKER_PURPLE);
            ovalfill(FCX, my - 2, mw - 2, 2, (n->acc&A_GRILL)?CLR_YELLOW:CLR_WHITE);
            if (lip >= 0) { ovalfill(FCX, my - 3, mw, 2, lip); ovalfill(FCX, my + 3, mw, 2, lip); }
        } else if (n->expr == X_SMUG) {
            // smirk — line tilted up one side
            line(FCX - mw, my + 2, FCX + mw, my - 2, lip>=0?lip:skd);
            line(FCX - mw, my + 3, FCX + mw - 2, my - 1, lip>=0?lip:skd);
        } else if (n->expr == X_ANGRY) {
            if (!n->sex) teeth(FCX, my, mw - 2, n->acc & (A_GRILL|A_GOLDTOOTH));  // clenched
            else { int lc=lip>=0?lip:skd; line(FCX - mw, my - 2, FCX + mw, my + 1, lc); line(FCX - mw, my - 1, FCX + mw, my + 2, lc); }
        } else if (n->expr == X_NERVOUS) {
            line(FCX - mw + 2, my + 2, FCX + mw - 2, my + 2, lip>=0?lip:skd);
        } else {
            line(FCX - mw, my, FCX + mw, my, lip>=0?lip:skd);
            if (lip >= 0) { ovalfill(FCX, my - 1, mw, 2, lip); ovalfill(FCX, my + 2, mw, 2, lip); }
        }
        // gold tooth glint on a closed smug/laugh mouth
        if ((n->acc & A_GOLDTOOTH) && (n->expr==X_SMUG))
            pset(FCX + 4, my, CLR_YELLOW);
    }

    // ── wrinkles — weathered faces get forehead lines, crow's feet, folds ──
    if (n->wrinkly) {
        int fh_y = brow_y - 6;                               // forehead, above the brows
        for (int w = 0; w < 2; w++) {                        // two forehead furrows
            int wy = fh_y - w*4;
            line(FCX - HW + 16, wy, FCX - 4, wy - 1, skd);
            line(FCX - 4, wy - 1, FCX + HW - 16, wy, skd);
        }
        line(FCX - 3, fh_y, FCX - 3, fh_y - 7, skd);         // a vertical frown line
        // crow's feet at the outer eye corners
        for (int c = -1; c <= 1; c++) {
            line(lx - ew, ey + c*2, lx - ew - 4, ey + c*3 - 1, skd);
            line(rx + ew, ey + c*2, rx + ew + 4, ey + c*3 - 1, skd);
        }
        // nasolabial folds — nose wing down to the mouth corner
        line(FCX - 7, ny + 4, FCX - mw - 1, my + 1, skd);
        line(FCX + 7, ny + 4, FCX + mw + 1, my + 1, skd);
    }


    // ── facial hair ───────────────────────────────────────────────────
    if (!n->sex) {
        int fh = hcd;
        if (n->facial == F_STUBBLE) {
            unsigned s = seed;
            for (int i = 0; i < 70; i++) {
                int px = FCX - jw + (int)(prng(&s) % (jw*2));
                int py = my - 6 + (int)(prng(&s) % 28);
                if ((px-FCX)*(px-FCX) + (py-(FCY+30))*(py-(FCY+30)) < jw*jw)
                    pset(px, py, fh);
            }
        } else if (n->facial == F_MUSTACHE) {
            ovalfill(FCX, my - 6, 14, 3, fh);
        } else if (n->facial == F_GOATEE) {
            ovalfill(FCX, my - 6, 12, 3, fh);                 // 'stache
            ovalfill(FCX, my + 10, 9, 8, fh);                 // chin patch
        } else if (n->facial == F_FULL) {
            ovalfill(FCX, my + 6, jw - 2, 20, fh);            // beard mass
            ovalfill(FCX, my - 6, 14, 3, fh);                 // 'stache
            trifill(FCX - jw, my - 4, FCX - jw + 8, my - 8, FCX - jw + 4, my + 14, fh); // L sideburn
            trifill(FCX + jw, my - 4, FCX + jw - 8, my - 8, FCX + jw - 4, my + 14, fh); // R sideburn
            // keep the mouth visible
            line(FCX - mw, my, FCX + mw, my, CLR_DARKER_PURPLE);
        }
    }

    // ── scar — a short slash on the CHEEK (below the eye, never over it) ──
    if (n->acc & A_SCAR) {
        int dir = (seed & 1) ? -1 : 1;
        int sx = FCX + dir*(HW - 16), sy = FCY + 12;
        line(sx, sy, sx + dir*2, sy + 12, skhi);            // raised pale scar line
        for (int k = 2; k <= 10; k += 4)                    // stitch ticks across it
            line(sx - 2, sy + k, sx + 2, sy + k, skhi);
    }

    // ── face tattoos — INKED TEXT (rotated words in a small font) + teardrops ──
    if (n->acc & A_FACETAT) {
        int tcol = (n->skin >= 3) ? CLR_DARK_GREY : CLR_DARKER_BLUE;   // faded ink
        int side = (seed & 2) ? 1 : -1;
        int tx = FCX + side * exo;                  // teardrops under one eye
        int drops = n->heavytat ? 3 : 1 + (int)((seed >> 2) & 1);
        for (int d = 0; d < drops; d++) {
            int dy = ey + 9 + d*6;
            trifill(tx - 2, dy, tx + 2, dy, tx, dy + 4, tcol);
            pset(tx, dy + 1, sk);                   // hollow centre (outline-teardrop)
        }
        // cheek ink (opposite the teardrops): a word OR cryptic symbols, tilted
        if (seed & 32) tat_text(TAT_SYMS[seed % NSYM], FONT_NORMAL,
                                FCX - side*(HW - 17), FCY + 13, -side * 14, tcol);
        else           tat_text(TAT_WORDS[seed % NTAT], FONT_TINY,
                                FCX - side*(HW - 17), FCY + 13, -side * 14, tcol);
        // brow slit on the other brow (men, when not behind shades)
        if (!n->sex && !(n->acc & A_SHADES) && (seed & 4))
            rectfill(FCX - side*exo - 1, brow_y - 1 + brow_drop, 3, 5, sk);

        // ── EXCESSIVE ink (fully-inked face): words + symbols + ascii art ──
        if (n->heavytat) {
            // forehead statement — cryptic symbols or a word (where skin shows)
            if (!(n->acc & (A_BANDANA|A_DURAG|A_SCARF))) {
                if (seed & 64) tat_text(TAT_SYMS[(seed >> 5) % NSYM], FONT_NORMAL,
                                        FCX, brow_y - 10, (seed & 16) ? 5 : -5, tcol);
                else           tat_text(TAT_WORDS[(seed >> 5) % NTAT], FONT_TINY,
                                        FCX, brow_y - 9, (seed & 16) ? 5 : -5, tcol);
            }
            // a bolder word low on the jaw
            tat_text(TAT_WORDS[(seed >> 8) % NTAT], FONT_SMALL,
                     FCX + side*(HW - 20), FCY + 30, side * 18, tcol);
            // a little ASCII-ART motif on the temple, above the eye (flat)
            tat_art(TAT_ART[(seed >> 10) % NART], FONT_TINY,
                    FCX + side*(HW - 10), FCY - 14, tcol);
            // chin numerals
            tat_text("213", FONT_TINY, FCX, my + 12, 0, tcol);
        }
    }

    // ── beauty mark (a Monroe mole — babes) ────────────────────────────
    if (n->acc & A_BEAUTY)
        circfill(FCX - mw - 1, my - 4, 1, CLR_BROWNISH_BLACK);

    // a single bead of sweat trickling down one temple (fiends / nervous) —
    // a pale droplet that slides smoothly and resets, NOT bright blue dots
    if (n->role == R_FIEND || n->expr == X_NERVOUS) {
        int phase = frame() % 150;                 // ~2.5s cycle, mostly dry
        if (phase < 70) {
            int bx = FCX - HW + 15;
            int by = FCY - 14 + phase/2;           // slides down the temple
            ovalfill(bx, by, 2, 3, CLR_LIGHT_GREY);
            pset(bx - 1, by - 1, CLR_WHITE);       // highlight makes it read as a wet bead
        }
    }

    // ── shades (over the eyes, late) ──────────────────────────────────
    if (n->acc & A_SHADES) {
        rectfill(lx - 11, ey - 6, 22, 13, CLR_BLACK);
        rectfill(rx - 11, ey - 6, 22, 13, CLR_BLACK);
        line(lx + 11, ey - 2, rx - 11, ey - 2, CLR_BLACK);   // bridge
        // shine streak
        line(lx - 8, ey - 4, lx - 3, ey + 1, CLR_DARK_GREY);
        line(rx - 8, ey - 4, rx - 3, ey + 1, CLR_DARK_GREY);
    }

    // ── spectacles (clear lenses, frames only — senior/historian/biz/etc.) ──
    if ((n->acc & A_GLASSES) && !(n->acc & A_SHADES)) {
        int gc = (n->skin >= 4) ? CLR_LIGHT_GREY : CLR_BROWNISH_BLACK;
        rect(lx - ew - 1, ley - eh - 2, 2*ew + 3, 2*eh + 4, gc);   // left frame
        rect(rx - ew - 1, rey - eh - 2, 2*ew + 3, 2*eh + 4, gc);   // right frame
        line(lx + ew + 2, ley - 1, rx - ew - 2, rey - 1, gc);      // bridge
        line(lx - ew - 1, ley - 1, lx - HW + 3, ley - 3, gc);      // left temple arm
        line(rx + ew + 1, rey - 1, rx + HW - 3, rey - 3, gc);      // right temple arm
        pset(lx + ew - 1, ley - eh, CLR_WHITE);                    // lens glint
        pset(rx + ew - 1, rey - eh, CLR_WHITE);
    }

    // ── sports headband across the forehead (baller) ──────────────────
    if (n->acc & A_HEADBAND) {
        int bandc = (n->shirt == CLR_WHITE) ? CLR_RED : CLR_WHITE;
        int by0 = FCY - VH + 18;
        rectfill(FCX - HW + 4, by0, (HW - 4)*2, 6, bandc);
        line(FCX - HW + 4, by0, FCX + HW - 4, by0, FACE_OL);
        line(FCX - HW + 4, by0 + 5, FCX + HW - 4, by0 + 5, FACE_OL);
    }

    // ── cigarette + smoke ─────────────────────────────────────────────
    if (n->acc & A_CIG) {
        int cgx = FCX + mw - 2, cgy = my + 2;
        rectfill(cgx, cgy, 12, 2, CLR_WHITE);
        rectfill(cgx + 11, cgy, 2, 2, CLR_ORANGE);          // ember
        // rising smoke
        for (int s = 0; s < 8; s++) {
            int t = (frame() + s*7);
            int sx = cgx + 13 + (int)(sinf(t*0.08f + s)*3) + s;
            int sy = cgy - 2 - s*4 - (t/4)%4;
            if (sy > FCY - VH - 30) pset(sx, sy, CLR_LIGHT_GREY);
        }
    }
}

// ── hair: behind the head ────────────────────────────────────────────────────
static void hair_back(Npc *n) {
    int hc = HAIRS[n->haircol].base, hcd = HAIRS[n->haircol].dark;
    if (n->acc & (A_DURAG | A_BANDANA | A_SCARF)) return;   // covered
    switch (n->hair) {
        case H_AFRO:
            circfill(FCX, FCY - 8, HW + 16, hc);
            circfill(FCX, FCY - 8, HW + 16, hcd);
            circfill(FCX - 4, FCY - 12, HW + 12, hc);
            break;
        case H_LONG:
            ovalfill(FCX, FCY + 18, HW + 12, VH + 6, hc);
            ovalfill(FCX - HW - 6, FCY + 24, 14, VH, hc);
            ovalfill(FCX + HW + 6, FCY + 24, 14, VH, hc);
            break;
        case H_BUN:
            ovalfill(FCX, FCY + 4, HW + 6, VH, hc);
            circfill(FCX, FCY - VH + 2, 12, hc);
            circfill(FCX, FCY - VH + 2, 12, hcd);
            break;
        case H_BOB:
            ovalfill(FCX, FCY + 6, HW + 9, VH + 2, hc);
            break;
        case H_CORNROWS:
        case H_SHORT:
        case H_FADE:
        case H_SLICK:
            ovalfill(FCX, FCY - 6, HW + 4, VH, hc);
            break;
        default: break;  // bald
    }
}

// ── hair: in front (hairline, fade shape, headwear) ──────────────────────────
static void hair_front(Npc *n) {
    int hc = HAIRS[n->haircol].base, hcd = HAIRS[n->haircol].dark;
    int topY = FCY - VH;

    // headwear takes over the crown
    if (n->acc & A_DURAG) {
        ovalfill(FCX, FCY - VH + 14, HW + 2, 22, CLR_BLACK);
        rectfill(FCX - HW, FCY - VH + 14, HW*2, 16, CLR_BLACK);
        // tail down the back-left
        trifill(FCX - HW + 4, FCY - VH + 22, FCX - HW - 2, FCY - VH + 22, FCX - HW - 10, FCY + 6, CLR_BLACK);
        return;
    }
    if (n->acc & A_BANDANA) {
        int bc = (n->shirt==CLR_RED)?CLR_RED:(n->role==R_GANGSTER?CLR_BLUE:CLR_RED);
        rectfill(FCX - HW, FCY - VH + 16, HW*2, 12, bc);
        ovalfill(FCX, FCY - VH + 16, HW, 10, bc);
        // knotted tails on the side
        trifill(FCX + HW - 2, FCY - VH + 22, FCX + HW + 8, FCY - VH + 18, FCX + HW + 6, FCY - VH + 30, bc);
        // paisley dots
        for (int i = -HW + 8; i < HW - 8; i += 10) pset(FCX + i, FCY - VH + 22, CLR_WHITE);
        return;
    }
    if (n->acc & A_SCARF) {
        int bc = n->shirt;
        ovalfill(FCX, FCY - VH + 18, HW + 4, 26, bc);
        rectfill(FCX - HW - 4, FCY - VH + 18, (HW+4)*2, 18, bc);
        return;
    }
    if (n->acc & A_CAP) {
        int capc = (n->role==R_PIMP) ? n->shirt : pick_cap(n);
        if (n->role == R_PIMP) {
            // wide-brim fur/felt hat
            ovalfill(FCX, FCY - VH + 12, HW + 22, 8, capc);   // brim
            ovalfill(FCX, FCY - VH + 2, HW - 2, 14, capc);    // crown
            ovalfill(FCX, FCY - VH + 6, HW + 2, 4, CLR_DARK_GREY); // band
            line(FCX + HW - 6, FCY - VH + 2, FCX + HW + 6, FCY - VH - 12, CLR_RED); // feather
        } else if (n->role == R_LABORER) {
            // hard hat — domed shell + wide brim + a ridge
            int hc2 = CLR_YELLOW;
            ovalfill(FCX, FCY - VH + 14, HW + 12, 7, hc2);    // brim
            ovalfill(FCX, FCY - VH + 4, HW - 4, 14, hc2);     // shell
            line(FCX, FCY - VH - 7, FCX, FCY - VH + 10, CLR_DARK_ORANGE); // crest ridge
        } else if (n->role == R_BEGGAR || n->role == R_BARISTA ||
                   n->role == R_TEEN   || n->role == R_SENIOR) {
            // knit beanie — a rounded cap hugging the crown, with a folded brim
            ovalfill(FCX, FCY - VH + 8, HW - 1, 16, capc);
            rectfill(FCX - HW + 2, FCY - VH + 14, (HW-2)*2, 6, capc);   // folded band
            for (int i = -HW + 4; i < HW - 4; i += 4)                   // knit ribbing
                line(FCX + i, FCY - VH + 14, FCX + i, FCY - VH + 19, pick_cap(n)==capc?CLR_BLACK:capc);
        } else {
            // snapback: dome + flat brim to one side
            ovalfill(FCX, FCY - VH + 12, HW + 2, 16, capc);
            rectfill(FCX - HW, FCY - VH + 16, HW*2, 12, capc);
            rectfill(FCX - HW - 14, FCY - VH + 22, 22, 5, capc);  // brim
            rectfill(FCX - 3, FCY - VH + 12, 6, 5, CLR_WHITE);    // little logo
        }
        return;
    }

    // bare hairstyles — front hairline
    switch (n->hair) {
        case H_BALD:
            // shaved dome highlight
            ovalfill(FCX, FCY - VH + 18, HW - 6, 8, SKINS[n->skin].base);
            pset(FCX - 8, FCY - VH + 16, CLR_WHITE);
            break;
        case H_SHORT:
            ovalfill(FCX, FCY - VH + 10, HW - 2, 18, hc);
            rectfill(FCX - HW + 4, FCY - VH + 12, (HW-4)*2, 8, hc);
            break;
        case H_FADE:
            // high-top fade: flat block on top, tight shaved sides
            rectfill(FCX - HW + 8, FCY - VH - 6, (HW-8)*2, 26, hc);
            rectfill(FCX - HW + 6, FCY - VH + 14, (HW-6)*2, 6, hcd);   // line-up
            // shaved sides (skin shows) — narrow the block at temples
            trifill(FCX - HW + 2, FCY - VH + 8, FCX - HW + 10, FCY - VH + 8, FCX - HW + 10, FCY + 4, SKINS[n->skin].base);
            trifill(FCX + HW - 2, FCY - VH + 8, FCX + HW - 10, FCY - VH + 8, FCX + HW - 10, FCY + 4, SKINS[n->skin].base);
            break;
        case H_AFRO:
            // front already huge from back layer; add line-up
            rectfill(FCX - HW + 4, FCY - VH + 16, (HW-4)*2, 4, hcd);
            break;
        case H_CORNROWS:
            ovalfill(FCX, FCY - VH + 12, HW - 2, 16, hcd);
            for (int i = -HW + 8; i <= HW - 8; i += 8) {
                line(FCX + i, FCY - VH + 4, FCX + i, FCY + 2, hc);
                pset(FCX + i, FCY - VH + 8, CLR_BLACK);
            }
            break;
        case H_SLICK:
            ovalfill(FCX, FCY - VH + 10, HW - 2, 16, hc);
            for (int i = -HW + 6; i <= HW - 6; i += 5)            // combed-back lines
                line(FCX + i, FCY - VH + 4, FCX + i + 3, FCY - VH + 18, hcd);
            pset(FCX - HW + 12, FCY - VH + 8, CLR_WHITE);
            break;
        case H_LONG:
            ovalfill(FCX, FCY - VH + 12, HW - 2, 18, hc);
            // center part
            line(FCX, FCY - VH + 6, FCX, FCY - VH + 22, hcd);
            break;
        case H_BUN:
            ovalfill(FCX, FCY - VH + 12, HW - 4, 14, hc);
            line(FCX, FCY - VH + 6, FCX, FCY - VH + 20, hcd);
            break;
        case H_BOB:
            ovalfill(FCX, FCY - VH + 12, HW, 18, hc);
            // side bangs
            trifill(FCX - HW + 2, FCY - VH + 14, FCX - 4, FCY - VH + 14, FCX - HW + 6, FCY + 4, hc);
            break;
    }
}

static int pick_cap(Npc *n) {
    static const int caps[] = { CLR_RED, CLR_DARK_BLUE, CLR_BLACK, CLR_DARK_GREEN, CLR_WHITE, CLR_ORANGE };
    return caps[(n->skin + n->face + n->line) % 6];
}

// ── conversation box ─────────────────────────────────────────────────────────
static void draw_box(Npc *n) {
    int nb; const Bark *bs = bark_set(n->role, &nb);
    const Bark *b = &bs[n->line];

    int bx = 8, by = 150, bw = SCREEN_W - 16, bh = 44;
    // panel
    rectfill(bx, by, bw, bh, CLR_DARKER_BLUE);
    rect(bx, by, bw, bh, CLR_WHITE);
    rect(bx + 1, by + 1, bw - 2, bh - 2, CLR_DARK_GREY);

    // name plate tab
    int npw = text_width(n->name) + 12;
    rectfill(bx + 6, by - 9, npw, 12, CLR_RED);
    rect(bx + 6, by - 9, npw, 12, CLR_WHITE);
    print(n->name, bx + 12, by - 7, CLR_WHITE);
    // role label (right tab)
    int rw = text_width(ROLE_NAME[n->role]) + 10;
    rectfill(bx + bw - rw - 6, by - 9, rw, 12, CLR_DARK_GREEN);
    rect(bx + bw - rw - 6, by - 9, rw, 12, CLR_WHITE);
    print(ROLE_NAME[n->role], bx + bw - rw - 1, by - 7, CLR_WHITE);

    // typewritten body
    font(FONT_SMALL);
    char buf[96];
    int total = 0;
    // assemble both lines into one buffer separated by '\n' marker we handle manually
    const char *parts[2] = { b->l1, b->l2 };
    int ci = 0;
    for (int p = 0; p < 2 && parts[p][0]; p++)
        for (int i = 0; parts[p][i]; i++) total++;

    int show = shown_chars;
    // line 1
    int cx = bx + 8, cyt = by + 8;
    int drawn = 0;
    for (int p = 0; p < 2 && parts[p][0]; p++) {
        char tmp[80]; int k = 0;
        for (int i = 0; parts[p][i] && drawn < show; i++) { tmp[k++] = parts[p][i]; drawn++; }
        tmp[k] = 0;
        print(tmp, cx, cyt + p*12, CLR_LIGHT_PEACH);
    }
    font(FONT_NORMAL);

    // blinking advance arrow when done
    if (shown_chars >= total && (frame()/20)%2)
        print("\x10", bx + bw - 12, by + bh - 10, CLR_YELLOW);

    // hint line
    font(FONT_TINY);
    print("SPACE new  Z line  1-9,a-h role", bx + 6, by + bh + 2, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

// ── background mood (tinted per role) ──────────────────────────────────────────
static void draw_bg(Npc *n) {
    int top, bot;
    switch (n->role) {
        case R_MOBSTER: top = CLR_DARK_RED;    bot = CLR_BROWNISH_BLACK; break;  // warm restaurant
        case R_GIRL:    top = CLR_DARK_PURPLE; bot = CLR_DARKER_PURPLE;  break;  // pink neon
        case R_PIMP:    top = CLR_DARK_PURPLE; bot = CLR_DARKER_BLUE;    break;
        case R_FIEND:
        case R_BEGGAR:  top = CLR_DARKER_GREY; bot = CLR_BLACK;          break;  // grim alley
        case R_DRUNK:   top = CLR_DARK_BROWN;  bot = CLR_BROWNISH_BLACK; break;  // dim bar
        case R_SUIT_BIZ:top = CLR_BLUE_GREEN;  bot = CLR_DARKER_BLUE;    break;  // cool office
        case R_BARISTA: top = CLR_BROWN;       bot = CLR_DARK_BROWN;     break;  // warm cafe
        case R_HISTORIAN:top= CLR_DARK_BROWN;  bot = CLR_BROWNISH_BLACK; break;  // study / library
        case R_BALLER:  top = CLR_DARK_ORANGE; bot = CLR_DARK_RED;       break;  // sunset court
        case R_SENIOR:  top = CLR_INDIGO;      bot = CLR_DARKER_PURPLE;  break;
        case R_LABORER: top = CLR_DARK_ORANGE; bot = CLR_BROWNISH_BLACK; break;  // dusty site
        default:        top = CLR_DARKER_BLUE; bot = CLR_BLACK;          break;  // night street
    }
    for (int y = 0; y < 150; y++) {
        int c = (y < 60) ? top : (y < 105 ? bot : CLR_BLACK);
        line(0, y, SCREEN_W, y, c);
    }
    // STATIC ordered-dither seams (deterministic in x,y — no per-frame flicker)
    for (int x = 0; x < SCREEN_W; x += 2) {
        pset(x,     57, bot);  pset(x + 1, 59, bot);          // top → bot
        pset(x + 1, 101, CLR_BLACK);  pset(x, 103, CLR_BLACK); // bot → black
    }
    // building silhouettes / lit windows for street roles
    if (n->role != R_MOBSTER) {
        rectfill(8, 70, 40, 80, CLR_BLACK);
        rectfill(272, 60, 44, 90, CLR_BLACK);
        for (int wy = 78; wy < 140; wy += 14)
            for (int wx = 14; wx < 40; wx += 12)
                if ((wx + wy + n->skin) % 3) rectfill(wx, wy, 6, 7, CLR_DARK_ORANGE);
        for (int wy = 70; wy < 140; wy += 16)
            for (int wx = 278; wx < 312; wx += 12)
                if ((wx + wy) % 2) rectfill(wx, wy, 6, 8, CLR_YELLOW);
    } else {
        // hanging restaurant light
        line(160, 0, 160, 18, CLR_DARK_GREY);
        circfill(160, 22, 6, CLR_LIGHT_YELLOW);
    }
    // vignette
    for (int i = 0; i < 4; i++) {
        line(0, i, SCREEN_W, i, CLR_BLACK);
        line(i, 0, i, 150, CLR_BLACK);
        line(SCREEN_W-1-i, 0, SCREEN_W-1-i, 150, CLR_BLACK);
    }
}

// ── lifecycle ────────────────────────────────────────────────────────────────
static int bark_total(Npc *n) {
    int nb; const Bark *bs = bark_set(n->role, &nb);
    const Bark *b = &bs[n->line];
    int t = 0; for (int i=0;b->l1[i];i++) t++; for (int i=0;b->l2[i];i++) t++;
    return t;
}

static void new_line(int reroll_person) {
    if (reroll_person) roll_npc(&me, -1);
    else {
        int nb; bark_set(me.role, &nb);
        me.line = rnd(nb);
        const Bark *bs = bark_set(me.role, &nb);
        me.expr = bs[me.line].expr;
    }
    roll_frame = frame();
    shown_chars = 0;
}

void init(void) {
    new_line(1);    // roll the first person off the corner
}

void update(void) {
    if (btnp(0, BTN_B)) tick_on = !tick_on;
    if (keyp(KEY_SPACE) || mouse_pressed(0)) new_line(1);
    if (btnp(0, BTN_A)) new_line(0);
    // force a role: keys 1-9 → roles 0-8, then A-H → roles 9-16
    for (int k = 0; k < NROLES; k++) {
        int kc = (k < 9) ? ('1' + k) : ('A' + (k - 9));
        if (keyp(kc)) { roll_npc(&me, k); roll_frame = frame(); shown_chars = 0; }
    }

    // advance typewriter
    int total = bark_total(&me);
    int target = (frame() - roll_frame) / 2;
    if (target > total) target = total;
    if (target > shown_chars) {
        shown_chars = target;
        if (tick_on && (shown_chars % 2 == 0)) hit(74, INSTR_NOISE, 1, 18);
    }
}

// ── mini avatar — the SAME face at ~100px, as it'd appear as an in-game ID/
// contact icon. zoom_rect() copies the rendered head region (frame-so-far)
// scaled down into a framed card; no re-draw, no second code path to drift.
static void draw_avatar(Npc *n) {
    // tight capture box around the head + a little neck/collar
    int top    = FCY - VH - 18; if (top < 2) top = 2;
    int bottom = FCY + VH + 22;
    int sx = FCX - HW - 12, sy = top;
    int sw = 2 * (HW + 12), sh = bottom - sy;
    int dh = 100, dw = sw * dh / sh;
    if (dw > 82) dw = 82;                 // keep the card clear of the big head's pixels
    int dx = 10, dy = 18;
    // ID card frame
    rectfill(dx - 4, dy - 4, dw + 8, dh + 8, CLR_DARKER_BLUE);
    rect(dx - 4, dy - 4, dw + 8, dh + 8, CLR_LIGHT_GREY);
    zoom_rect(sx, sy, sw, sh, dx, dy, dw, dh);    // the same face, shrunk
    rect(dx - 1, dy - 1, dw + 2, dh + 2, CLR_DARK_GREY);
    font(FONT_TINY);
    print_centered("100px", dx + dw/2, dy + dh + 3, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
}

void draw(void) {
    draw_bg(&me);
    int total = bark_total(&me);
    int talking = shown_chars < total;
    int mouth_open = (frame()/4) % 2;
    draw_portrait(&me, talking, mouth_open);
    draw_avatar(&me);
    draw_box(&me);
}
