/* de:meta
{
  "title": "Football Manager",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "inventory-system"
  ],
  "lineage": "Inspired by FC Mobile / Football Manager lite; novel in combining a drag-to-arrange formation pitch, a procedurally refreshing transfer market, and a Poisson-ish strength-ratio match simulator feeding a league table.",
  "genre": "sports",
  "homage": "Football Manager (1982)",
  "description": "An FC-Mobile-style manager mode played as a three-season CLIMB: you take Feyenoord from the regional EERSTE DIVISIE up through the EREDIVISIE (the best Dutch clubs) and into the CHAMPIONS LEAGUE (the giants of Europe). Each tier is its own 12-club, 10-round league; finish 1st to be promoted, and win the Champions League to be crowned CHAMPION OF EUROPE. Miss out and you replay the tier — but your squad and cash carry over, so the long, scrollable transfer market (a 30-deep shortlist of real 2026-era stars, the world-class names carrying 90+ ratings at a premium price — Yamal, Mbappe, Haaland, Courtois) is how you bankroll the climb. Run your XI on a drag-to-arrange formation pitch with a bench of spares, switchable 4-4-2 / 4-3-3 shapes and an out-of-position penalty. Before each match a SCOUT REPORT previews the next opponent — their style (attacking / defensive / balanced), strength and danger man — and your formation counters their style (park the bus vs attackers, go wide vs a low block), so you can reshape the side before you KICK OFF. Fielded players can pick up INJURIES that sideline them for a few rounds (unfieldable and worth less), forcing you to reshuffle. Hit kick off and the engine auto-sims a scoreline from your strength versus the opponent's, ticking goals up on a scoreboard before slamming home WIN/DRAW/DEFEAT while the table re-sorts live on points and goal difference. Player names are pooled by position, and the HUD, market and standings use the compact 4×6 font to fit the bigger rosters. Drag players between pitch and bench (hover a chip to see the player's name + rating), click 4-4-2 / 4-3-3 to switch shape, click a price to BUY or a value to SELL, and click PLAY MATCH (TAB cycles SQUAD/MARKET/LEAGUE, ENTER plays)."
}
de:meta */
#include "studio.h"
#include <stddef.h>
#include <string.h>

// SOCCERMGR — an FC-Mobile-style football manager mode.
// You run a squad: a starting XI laid out on a formation pitch + a bench,
// every player a position and a rating. Drag bodies between the pitch slots
// and the bench to set your team, switch the shape (4-4-2 / 4-3-3), wheel and
// deal in a TRANSFER MARKET against a cash budget, then hit PLAY MATCH — the
// engine auto-sims a scoreline from your starting-XI strength vs the opponent's
// and feeds the result into a 12-club league table over a 10-round season.
//
// CAMPAIGN: a three-tier climb. Win your division (finish 1st) to be promoted —
// EERSTE DIVISIE (regional Dutch clubs) -> EREDIVISIE (the best of Holland) ->
// CHAMPIONS LEAGUE (the giants of Europe). Win the Champions League and you're
// CHAMPION OF EUROPE. Miss out and you replay the tier next season — your squad
// and cash carry over, so the transfer market is how you climb. Three good
// seasons is the fast track to the top.
//
// It's a menu + table game, mouse-first. No real-time match, no training.
//
// PRE-MATCH: PLAY MATCH opens a SCOUT REPORT on the next opponent — their style
// (attacking / defensive / balanced), strength, and danger man — plus the
// matchup verdict. Your FORMATION counters their style (park the bus vs
// attackers, go wide vs a low block), so you can go BACK, reshape the side, and
// come back before you KICK OFF. INJURIES: a fielded player can pick up a knock,
// sits out a few rounds (can't be fielded, worth less to sell), and frees a slot
// you must fill from the bench.
//
// MOUSE: drag a player to move him between a formation slot and the bench;
// click 4-4-2 / 4-3-3 to switch shape; in the MARKET click BUY on a listed
// player or SELL on one of yours; click PLAY MATCH for the preview, then KICK
// OFF; click LEAGUE to see the table. Keys: TAB cycles SQUAD/MARKET/TABLE,
// ENTER opens the preview / kicks off / advances.
//
// TODO — inspiration: SENSIBLE WORLD OF SOCCER (the touchstone for this cart).
// Cool things SWOS did that we could steal:
//   - PLAYABLE arcade matches: tiny top-down "blobby" players + after-touch
//     (hold a direction after you shoot to curve the ball). The manager+play
//     hybrid is the soul — right now we only auto-sim.
//   - A whole-world DB with the full promotion/relegation pyramid (it had the
//     entire football world). We have 3 tiers; the climb could be deeper.
//   - A long CAREER (~20 seasons): get headhunted by bigger clubs when you do
//     well; contracts; trophies cabinet.
//   - Per-player SKILLS, not one rating (pace / tackling / shooting / heading),
//     plus YOUTH players coming through the academy.
//   - Match feedback as a GOALSCORER ticker / highlights reel, not just a score.
//   - Tactics screen you drag (we have the formation pitch — extend it).

// ---------------------------------------------------------------------------
// positions
enum { POS_GK, POS_DEF, POS_MID, POS_FWD, NPOS };
static const char *POSL[NPOS] = { "GK", "DEF", "MID", "FWD" };
static const int   POSC[NPOS] = { CLR_ORANGE, CLR_BLUE, CLR_GREEN, CLR_RED };

// ---------------------------------------------------------------------------
// players — one pool. slots 0..10 = starting XI, 11.. = bench. -1 rating = empty.
#define MAXP 22
// injured > 0 = rounds left on the sidelines (can't be fielded, worth less)
typedef struct { char name[16]; int pos; int rating; int kit; int injured; } Player;
static Player squad[MAXP];     // your roster (filled 0..nplayers-1)
static int    nplayers;

// formation: which player index (into squad[]) sits in each of the 11 XI slots,
// plus bench list. We keep XI[11] (player idx or -1) and BENCH order implicit:
// a player is "starting" if his index appears in XI, else he's on the bench.
static int XI[11];             // player index per pitch slot, -1 = empty slot

// formation shapes: slot -> position requirement + pitch coords (0..1 of pitch box)
enum { F_442, F_433, NFORM };
static const char *FORML[NFORM] = { "4-4-2", "4-3-3" };
// per shape: position of each of 11 slots
static const int FPOS[NFORM][11] = {
    { POS_GK, POS_DEF,POS_DEF,POS_DEF,POS_DEF, POS_MID,POS_MID,POS_MID,POS_MID, POS_FWD,POS_FWD },
    { POS_GK, POS_DEF,POS_DEF,POS_DEF,POS_DEF, POS_MID,POS_MID,POS_MID,        POS_FWD,POS_FWD,POS_FWD },
};
// normalized pitch coords (x,y in 0..100) per slot
static const int FX[NFORM][11] = {
    { 50,  18,38,62,82,  18,38,62,82,  38,62 },
    { 50,  18,38,62,82,  30,50,70,     22,50,78 },
};
static const int FY[NFORM][11] = {
    { 90,  68,72,72,68,  46,50,50,46,  22,22 },
    { 90,  68,72,72,68,  48,44,48,     22,18,22 },
};
static int formation;

// ---------------------------------------------------------------------------
// transfer market — a refreshing shortlist of buyable players
#define NMARKET 30                // a long shortlist — the buy column scrolls
static Player market[NMARKET];
static int    mprice[NMARKET];    // asking price
static bool   msold[NMARKET];
static int    mkt_scroll;         // index of the first buy row shown (0..NMARKET-MKT_VIS)

// ---------------------------------------------------------------------------
// league — you + 11 AI clubs. Each rival has a playing STYLE (scoutable, and it
// decides which formation counters them) and a named danger man for the dossier.
#define NCLUB 12
enum { STYLE_BALANCED, STYLE_ATTACK, STYLE_DEFENCE, NSTYLE };
static const char *STYLEL[NSTYLE] = { "BALANCED", "ATTACKING", "DEFENSIVE" };
typedef struct {
    char name[12]; int str; int W,D,L,GF,GA,pts;
    int  style; char starnm[16]; int starrating;     // scouting dossier
} Club;
static Club club[NCLUB];          // club[0] = YOU
#define YOU 0

// ---------------------------------------------------------------------------
// state
enum { TITLE, SQUAD, MARKET, RESULT, TABLE, OVER, PREVIEW };
static int state;
static int cash;
static int round_;                 // 1..NROUNDS
#define NROUNDS 10
static int division;               // current tier, D_EERSTE .. D_CL
static int season;                 // total seasons played this campaign (1-based)
static int end_pos;                // your finishing rank when a season ends
static int end_outcome;            // 0 missed promotion · 1 promoted · 2 won the lot
static int best;                   // career best: 0 none · 1 won Eerste · 2 won Ere · 3 European champion

// match result animation
static int   res_my, res_op;       // your goals / opp goals this match
static int   res_opp;              // opponent club index
static float res_t;                // 0..1 reveal
static int   res_shown_my, res_shown_op;   // ticking score
static float flashT; static int flashColor;

// drag state
static int   drag_from;            // XI slot being dragged (0..10), or -1
static int   drag_bench;           // bench player index being dragged, or -1
static int   drag_dx, drag_dy;
static bool  sb_drag;              // dragging the transfer-list scrollbar thumb

// ---------------------------------------------------------------------------
// names — real 2026-era European stars, pooled by position so a GK gets a GK
// name, and split into two TIERS: ELITE (world-class — earn a 87..95 rating and
// a premium price) and REGULAR (the rest, 60..84). Surnames / recognizable short
// forms; kept to fit the 15-char name buffer.
enum { T_REG, T_ELITE, NTIER };
static const char *ELITE_GK[]  = { "Courtois","Donnarumma","Maignan","Alisson","Ederson","Oblak","TerStegen","Neuer" };
static const char *REG_GK[]    = { "Onana","Raya","Sommer","Lunin","Sels","Verbruggen","Provedel","Kobel","Martinez","Bounou","Ramsdale","Vicario","Bizot" };
static const char *ELITE_DEF[] = { "VanDijk","Saliba","Hakimi","Rudiger","Dias","Gvardiol","Kounde","Bastoni","Walker","Konate","Militao","Marquinhos" };
static const char *REG_DEF[]   = { "Cubarsi","Araujo","Hancko","Hato","Bremer","Theo","Dumfries","Calafiori","Tah","Upamecano","Pavard","Geertruida","Smal","Akanji","Robertson","Davies","DeLigt","Timber","Frimpong","Schar","Hummels","Tomori","Kerkez" };
static const char *ELITE_MID[] = { "Pedri","Bellingham","Rodri","Wirtz","Musiala","Vitinha","Valverde","DeJong","Rice","Kimmich","BrunoF.","Odegaard","Camavinga","Tchouameni" };
static const char *REG_MID[]   = { "Gavi","Veerman","Saibari","JoaoNeves","Zaire","Modric","Fermin","Til","Gravenberch","Reijnders","Koopmeiner","Schouten","Mainoo","Eze","Gundogan","MacAlister","Wieffer","Stengs" };
static const char *ELITE_FWD[] = { "Yamal","Mbappe","Vinicius","Haaland","Salah","Kane","Lewandowski","Osimhen","Lautaro","Leao","Saka","Foden","Raphinha","Dembele","Kvara" };
static const char *REG_FWD[]   = { "Olmo","Brobbey","Pepi","Ueda","NicoWill.","Gyokeres","Isak","Rashford","Doue","Barcola","Gakpo","Depay","Simons","Lang","Nunez","Jackson","Havertz","Openda","Weghorst","Kluivert","Garnacho","Hojlund","Sancho" };
static const char *const *POOL[NTIER][NPOS] = {
    { REG_GK,   REG_DEF,   REG_MID,   REG_FWD   },
    { ELITE_GK, ELITE_DEF, ELITE_MID, ELITE_FWD },
};
#define PCNT(a) ((int)(sizeof(a) / sizeof *(a)))
static const int POOLN[NTIER][NPOS] = {
    { PCNT(REG_GK),   PCNT(REG_DEF),   PCNT(REG_MID),   PCNT(REG_FWD)   },
    { PCNT(ELITE_GK), PCNT(ELITE_DEF), PCNT(ELITE_MID), PCNT(ELITE_FWD) },
};
// CAMPAIGN: you manage Feyenoord on a three-season climb. Each division is its
// own 12-club league (you = club[0] + 11 rivals). Win it (finish 1st) to go up.
//   EERSTE DIVISIE   — regional Dutch clubs, weak: you should dominate
//   EREDIVISIE       — the best Dutch clubs, mid-strength: a real fight
//   CHAMPIONS LEAGUE — the giants of Europe: only an upgraded squad survives
enum { D_EERSTE, D_ERE, D_CL, NDIV };
static const char *DIVNM[NDIV] = { "EERSTE DIVISIE", "EREDIVISIE", "CHAMPIONS LEAGUE" };
#define NRIVAL (NCLUB - 1)
static const char *RIVALS[NDIV][NRIVAL] = {
    { "WILLEM II","RODA JC","ADO","CAMBUUR","GRAAFSCHAP","FC EMMEN","VVV VENLO","MVV","EINDHOVEN","TOP OSS","DORDRECHT" },
    { "AJAX","PSV","AZ ALKMAAR","TWENTE","UTRECHT","GO AHEAD","SPARTA","NEC","HEERENVEEN","FORTUNA","GRONINGEN" },
    { "REAL MADRID","MAN CITY","BAYERN","BARCELONA","LIVERPOOL","ARSENAL","PARIS SG","INTER","ATLETICO","NAPOLI","DORTMUND" },
};
static const int RSTR[NDIV][NRIVAL] = {
    { 760, 720, 700, 690, 680, 670, 660, 650, 640, 620, 600 },
    { 840, 830, 800, 780, 770, 740, 730, 720, 715, 705, 700 },
    { 920, 910, 895, 885, 870, 855, 845, 835, 820, 805, 790 },
};

// is this name already on the books? avoids two Mbappes on screen at once.
// scans your squad + the market rows filled so far this refresh (0..upto).
static bool name_used(const char *n, int upto) {
    for (int i = 0; i < nplayers; i++) if (strcmp(squad[i].name, n) == 0) return true;
    for (int i = 0; i < upto; i++)      if (strcmp(market[i].name, n) == 0) return true;
    return false;
}

static void pick_name(char *out, int pos, int tier, int upto) {
    const char *const *pool = POOL[tier][pos]; int n = POOLN[tier][pos];
    const char *cand = pool[rnd(n)];
    for (int t = 0; t < 24 && name_used(cand, upto); t++) cand = pool[rnd(n)];
    int k = 0; while (cand[k] && k < 15) { out[k] = cand[k]; k++; } out[k] = 0;
}

// random player of a position; elite -> a world-class name + 87..95 rating,
// otherwise a regular name + 60..84. upto = market rows already filled this pass
// (for de-duping the shortlist), 0 when generating your squad.
static void gen_player(Player *p, int pos, bool elite, int upto) {
    pick_name(p->name, pos, elite ? T_ELITE : T_REG, upto);
    p->pos = pos;
    p->rating = elite ? rnd_between(87, 96) : rnd_between(60, 85);
    p->kit = 0;
    p->injured = 0;
}

// ---------------------------------------------------------------------------
// helpers
static bool hover(int x, int y, int w, int h) { return point_in_box(mouse_x(), mouse_y(), x, y, w, h); }
static bool clicked(int x, int y, int w, int h) { return mouse_pressed(MOUSE_LEFT) && hover(x, y, w, h); }
static int  rating_col(int r) { return r >= 85 ? CLR_GREEN : r >= 75 ? CLR_LIME_GREEN : r >= 65 ? CLR_YELLOW : CLR_ORANGE; }
// resale value — knocked down 40% while a player is injured
static int  sell_value(int pidx) {
    int r = squad[pidx].rating, v = (r - 55) * (r - 55) * 3 + 150;
    return squad[pidx].injured > 0 ? v * 6 / 10 : v;
}

static void coin(void) { hit(81, INSTR_SQUARE, 3, 40); schedule(45, 88, INSTR_SQUARE, 3); }
static void nope(void) { note(45, INSTR_SQUARE, 2); }
static void whistle(void){ note(96, INSTR_TRI, 3); schedule(90, 99, INSTR_TRI, 3); }

// is a player index currently in the starting XI? returns slot or -1
static int xi_slot_of(int pidx) { for (int s = 0; s < 11; s++) if (XI[s] == pidx) return s; return -1; }

// strength of starting XI: sum of ratings, with a penalty when a slot's
// occupant doesn't match the required position (out of position), empty slots = 0.
static int xi_strength(void) {
    int total = 0;
    for (int s = 0; s < 11; s++) {
        int pidx = XI[s];
        if (pidx < 0) continue;
        int r = squad[pidx].rating;
        if (squad[pidx].pos != FPOS[formation][s]) r = r * 8 / 10;   // -20% out of position
        total += r;
    }
    return total;
}
static int xi_filled(void) { int n = 0; for (int s = 0; s < 11; s++) if (XI[s] >= 0) n++; return n; }

// bench = squad players not in the XI
static int bench_list(int *out) {
    int n = 0;
    for (int i = 0; i < nplayers; i++) if (xi_slot_of(i) < 0) out[n++] = i;
    return n;
}

// ---------------------------------------------------------------------------
static void refresh_market(void) {
    for (int i = 0; i < NMARKET; i++) {
        int pos = i == 0 ? POS_GK : 1 + rnd(NPOS - 1);
        bool elite = (i < 3) || (rnd(100) < 25);   // top 3 are marquee names; ~25% of the rest too
        gen_player(&market[i], pos, elite, i);
        // price scales steeply with rating — the world-class names cost a fortune
        int r = market[i].rating;
        mprice[i] = (r - 55) * (r - 55) * 4 + 200;
        msold[i] = false;
    }
    mkt_scroll = 0;
}

// (re)seed the table for the CURRENT division: you (club[0]) + its 11 rivals,
// all records zeroed. club[0].str is set from your squad each match.
static void new_league(void) {
    static const char *YOUNM = "FEYENOORD";
    int n = 0; while (YOUNM[n] && n < 11) { club[0].name[n] = YOUNM[n]; n++; } club[0].name[n] = 0;
    club[0].str = 0;
    for (int c = 1; c < NCLUB; c++) {
        const char *nm = RIVALS[division][c - 1];
        int k = 0; while (nm[k] && k < 11) { club[c].name[k] = nm[k]; k++; } club[c].name[k] = 0;
        club[c].str = RSTR[division][c - 1];
        club[c].style = rnd(NSTYLE);
        // danger man: an elite name, rated near the club's overall strength
        int spos = rnd(NPOS);
        const char *const *pool = POOL[T_ELITE][spos]; int pn = POOLN[T_ELITE][spos];
        const char *sn = pool[rnd(pn)];
        int j = 0; while (sn[j] && j < 15) { club[c].starnm[j] = sn[j]; j++; } club[c].starnm[j] = 0;
        int sr = club[c].str / 10 + rnd_between(0, 6);             // ~rating from club strength
        club[c].starrating = sr < 70 ? 70 : sr > 96 ? 96 : sr;
    }
    for (int c = 0; c < NCLUB; c++)
        club[c].W = club[c].D = club[c].L = club[c].GF = club[c].GA = club[c].pts = 0;
}

// start a fresh season in the current division (keeps your squad + cash).
static void start_season(void) {
    round_ = 1;
    for (int i = 0; i < nplayers; i++) squad[i].injured = 0;   // pre-season: everyone fit
    refresh_market();
    new_league();
    drag_from = drag_bench = -1;
    res_t = 0;
}

static void new_game(void) {
    nplayers = 0;
    // starting roster: 1 GK, 5 DEF, 6 MID, 4 FWD = 16. A humble lower-league
    // squad — regular names, modest ratings; you buy the stars as you climb.
    int plan[][2] = { {POS_GK,1},{POS_DEF,5},{POS_MID,6},{POS_FWD,4} };
    for (int k = 0; k < 4; k++)
        for (int j = 0; j < plan[k][1] && nplayers < MAXP; j++) {
            gen_player(&squad[nplayers], plan[k][0], false, 0);
            squad[nplayers].rating = rnd_between(62, 77);   // keep the start modest
            nplayers++;
        }

    formation = F_442;
    // auto-fill XI: best matching-position player per slot, then any
    for (int s = 0; s < 11; s++) XI[s] = -1;
    bool used[MAXP] = { false };
    for (int s = 0; s < 11; s++) {
        int want = FPOS[formation][s], bestp = -1, bestr = -1;
        for (int i = 0; i < nplayers; i++)
            if (!used[i] && squad[i].pos == want && squad[i].rating > bestr) { bestr = squad[i].rating; bestp = i; }
        if (bestp < 0)
            for (int i = 0; i < nplayers; i++)
                if (!used[i] && squad[i].rating > bestr) { bestr = squad[i].rating; bestp = i; }
        if (bestp >= 0) { XI[s] = bestp; used[bestp] = true; }
    }

    cash = 4000;
    division = D_EERSTE;
    season = 1;
    start_season();
}

void init(void) {
    best = load(0);
    colorkey(0);                  // baked title wordmarks: palette-0 padding = transparent
    instrument(5, INSTR_SAW, 6, 90, 4, 120); instrument_filter(5, FILTER_LOW, 1200, 4); // crowd-ish stab
    new_game();
    state = TITLE;
}

// ---------------------------------------------------------------------------
// switching formation: remap XI to keep players where positions still match,
// drop overflow (slot count changes only in arrangement, still 11 slots).
static void set_formation(int f) {
    if (f == formation) return;
    int keep[11]; for (int s = 0; s < 11; s++) keep[s] = XI[s];
    formation = f;
    // simple: keep the same player-per-slot mapping (both shapes are 11 slots),
    // the position-match penalty will just shift. That's the FC-mobile feel.
    for (int s = 0; s < 11; s++) XI[s] = keep[s];
    sfx(-1); coin();
}

// ---------------------------------------------------------------------------
// play one match: poisson-ish scoreline from strength ratio
static int sim_goals(int my_str, int op_str) {
    float ratio = (float)my_str / (float)(op_str + 1);
    float xg = clamp(0.7f + (ratio - 1.0f) * 2.2f, 0.15f, 4.2f);
    int g = 0;
    for (int k = 0; k < 7; k++) if (rnd_float() < xg / 7.0f * 1.6f) g++;   // up to ~7 chances
    return g;
}

static void apply_result(int my, int op, int opp) {
    Club *me = &club[YOU], *o = &club[opp];
    me->GF += my; me->GA += op; o->GF += op; o->GA += my;
    if (my > op)      { me->W++; o->L++; me->pts += 3; cash += 800; }
    else if (my < op) { me->L++; o->W++; o->pts += 3;  cash += 250; }
    else              { me->D++; o->D++; me->pts++; o->pts++; cash += 450; }

    // other AI clubs also play each other (round-robin-ish), advance the table
    for (int c = 1; c < NCLUB; c++) {
        if (c == opp) continue;
        int d = 1 + ((c + round_) % (NCLUB - 1));
        if (d == c) d = 1 + (d % (NCLUB - 1));
        if (d <= c) continue;            // each pair once
        int a = sim_goals(club[c].str, club[d].str);
        int b = sim_goals(club[d].str, club[c].str);
        club[c].GF += a; club[c].GA += b; club[d].GF += b; club[d].GA += a;
        if (a > b)      { club[c].W++; club[d].L++; club[c].pts += 3; }
        else if (b > a) { club[d].W++; club[c].L++; club[d].pts += 3; }
        else            { club[c].D++; club[d].D++; club[c].pts++; club[d].pts++; }
    }
}

static int upcoming_opp(void) { return 1 + ((round_ - 1) % (NCLUB - 1)); }

// how your shape fares against the opponent's style: park the bus (4-4-2) blunts
// attackers; width (4-3-3) breaks down a low block. Added to your match strength.
static int formation_edge(int style) {
    if (style == STYLE_ATTACK)  return formation == F_442 ? 35 : -25;
    if (style == STYLE_DEFENCE) return formation == F_433 ? 35 : -10;
    return 0;
}

// PLAY MATCH first opens the dossier — you can still go BACK and reshape the team.
static void goto_preview(void) {
    if (xi_filled() < 11) { nope(); return; }
    res_opp = upcoming_opp();
    state = PREVIEW;
}

// actually play it: scoreline from strength (with the formation edge vs the
// opponent's style), then a post-match injury roll on the XI.
static void kickoff(void) {
    if (xi_filled() < 11) { nope(); return; }
    res_opp = upcoming_opp();
    club[YOU].str = xi_strength() + formation_edge(club[res_opp].style);
    res_my = sim_goals(club[YOU].str, club[res_opp].str);
    res_op = sim_goals(club[res_opp].str, club[YOU].str);
    apply_result(res_my, res_op, res_opp);
    // ~30% chance a fielded player picks up a knock (out 1..4 rounds); he's
    // pulled from the XI so you must reshuffle before the next match.
    if (rnd(100) < 30) {
        int s = rnd(11);
        if (XI[s] >= 0) { squad[XI[s]].injured = rnd_between(1, 5); XI[s] = -1; }
    }
    res_t = 0; res_shown_my = res_shown_op = 0;
    state = RESULT;
    whistle();
}

static int league_pos(void) {     // your rank 1..NCLUB by pts then GD
    int rank = 1;
    int mygd = club[YOU].GF - club[YOU].GA;
    for (int c = 0; c < NCLUB; c++) {
        if (c == YOU) continue;
        int gd = club[c].GF - club[c].GA;
        if (club[c].pts > club[YOU].pts || (club[c].pts == club[YOU].pts && gd > mygd)) rank++;
    }
    return rank;
}

// ---------------------------------------------------------------------------
// layout constants
#define HUD_H 12
// pitch box
#define PX 8
#define PY 18
#define PW 184
#define PH 168
// bench panel
#define BNX 198
#define BNY 18
#define BNW 116
// formation slot pixel center
static int slot_px(int s) { return PX + 14 + FX[formation][s] * (PW - 28) / 100; }
static int slot_py(int s) { return PY + 12 + FY[formation][s] * (PH - 24) / 100; }
// tab bar at very bottom
#define TBY 190
#define TBH 10
// MARKET rows — small font lets us pack the shortlist. Single source of truth
// for hit-test (update) AND draw, so the price buttons line up.
#define MKT_Y0   28
#define MKT_DY   15
#define MKT_ROWY(i) (MKT_Y0 + (i) * MKT_DY)
#define MKT_VIS  10                // buy rows visible at once (the list scrolls)
#define MKT_SELL 9                 // sell rows shown (bench spares)
// BUY-row columns (left panel) — narrowed to leave a scrollbar strip on the right
#define BUY_X     6                //   row rect left
#define BUY_W     140              //   row rect width  (ends at 146)
#define BUY_BTNX  100              //   price/BUY button left
#define BUY_BTNW  40
// scrollbar for the buy list: ▲ button, track, ▼ button
#define SB_X     148
#define SB_W     10
#define SB_AH    12                // arrow-button height
#define SB_TOP   MKT_Y0                       // 28
#define SB_BOT   (MKT_ROWY(MKT_VIS) - 1)      // bottom of the last visible row
#define SB_TY0   (SB_TOP + SB_AH)             // track top (below ▲)
#define SB_TY1   (SB_BOT - SB_AH)             // track bottom (above ▼)
// BENCH rows on the SQUAD screen — also small font, more subs reachable.
#define BN_ROW0  (BNY + 16)
#define BN_DY    13
#define BN_VIS   9

// ---------------------------------------------------------------------------
void update(void) {
    if (flashT > 0) flashT -= dt();

    if (state == TITLE) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_ENTER) || btnp(0, BTN_A)) state = SQUAD;
        return;
    }
    if (state == OVER) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_ENTER) || btnp(0, BTN_A)) {
            if (end_outcome == 2) { new_game(); state = TITLE; }   // won it all → fresh campaign
            else {                                                 // promoted or replay this tier
                if (end_outcome == 1) division++;
                season++;
                start_season();
                state = SQUAD;
            }
        }
        return;
    }
    if (state == RESULT) {
        res_t = clamp(res_t + dt() * 0.7f, 0, 1.4f);
        int tm = (int)lerp(0, res_my, clamp(res_t * 1.5f, 0, 1));
        int to = (int)lerp(0, res_op, clamp(res_t * 1.5f, 0, 1));
        if (tm != res_shown_my) { res_shown_my = tm; note(72, INSTR_SQUARE, 4); }
        if (to != res_shown_op) { res_shown_op = to; note(60, INSTR_SAW, 3); }
        if (res_t >= 1.2f && (mouse_pressed(MOUSE_LEFT) || keyp(KEY_ENTER) || btnp(0, BTN_A))) {
            if (round_ >= NROUNDS) {                               // season's done — judge it
                end_pos = league_pos();
                // top 2 go up from the Dutch tiers; only the winner takes Europe
                if (division == D_CL) end_outcome = (end_pos == 1) ? 2 : 0;
                else                  end_outcome = (end_pos <= 2) ? 1 : 0;
                if (end_outcome >= 1) {                            // record the career best
                    int b = (end_outcome == 2) ? 3 : division + 1;
                    if (b > best) { best = b; save(0, best); }
                }
                state = OVER;
            }
            else {                                                 // next round
                round_++;
                for (int i = 0; i < nplayers; i++) if (squad[i].injured > 0) squad[i].injured--;
                refresh_market(); state = TABLE;
            }
        }
        return;
    }
    if (state == PREVIEW) {                                         // the pre-match dossier
        if (clicked(16, 176, 100, 16) || keyp(KEY_TAB) || mouse_pressed(MOUSE_RIGHT)) { state = SQUAD; return; }
        if (clicked(204, 176, 100, 16) || keyp(KEY_ENTER) || btnp(0, BTN_A)) { kickoff(); return; }
        return;
    }

    // tab bar (shared by SQUAD / MARKET / TABLE)
    int tbw = SCREEN_W / 3;
    if (clicked(0, TBY, tbw, TBH)) state = SQUAD;
    if (clicked(tbw, TBY, tbw, TBH)) state = MARKET;
    if (clicked(tbw * 2, TBY, SCREEN_W - tbw * 2, TBH)) state = TABLE;
    if (keyp(KEY_TAB)) state = state == SQUAD ? MARKET : state == MARKET ? TABLE : SQUAD;
    if (keyp(KEY_ENTER) && xi_filled() == 11) { goto_preview(); return; }

    if (state == SQUAD) {
        // formation buttons
        if (clicked(BNX, 158, 56, 12)) set_formation(F_442);
        if (clicked(BNX + 60, 158, 56, 12)) set_formation(F_433);
        // play match button — opens the preview first
        if (clicked(BNX, 173, BNW, 14) && xi_filled() == 11) { goto_preview(); return; }

        int bench[MAXP]; int nb = bench_list(bench);

        // begin a drag (pitch slot or bench row)
        if (mouse_pressed(MOUSE_LEFT) && drag_from < 0 && drag_bench < 0) {
            for (int s = 0; s < 11; s++) {
                int cx = slot_px(s), cy = slot_py(s);
                if (XI[s] >= 0 && point_in_box(mouse_x(), mouse_y(), cx - 9, cy - 9, 18, 18)) {
                    drag_from = s; drag_dx = mouse_x() - cx; drag_dy = mouse_y() - cy; break;
                }
            }
            if (drag_from < 0) {
                for (int i = 0; i < nb && i < BN_VIS; i++) {
                    int ry = BN_ROW0 + i * BN_DY;
                    if (point_in_box(mouse_x(), mouse_y(), BNX, ry, BNW, BN_DY)) { drag_bench = bench[i]; break; }
                }
            }
        }

        // drop
        if (mouse_released(MOUSE_LEFT) && (drag_from >= 0 || drag_bench >= 0)) {
            // find target slot under cursor
            int target = -1;
            for (int s = 0; s < 11; s++)
                if (point_in_box(mouse_x(), mouse_y(), slot_px(s) - 11, slot_py(s) - 11, 22, 22)) { target = s; break; }
            bool over_bench = hover(BNX, BNY + 14, BNW, 134 - 14);

            if (drag_from >= 0) {              // dragging an XI player
                if (target >= 0 && target != drag_from) {       // swap two slots
                    int tmp = XI[target]; XI[target] = XI[drag_from]; XI[drag_from] = tmp; coin();
                } else if (over_bench) {       // send to bench
                    XI[drag_from] = -1; coin();
                }
            } else if (drag_bench >= 0) {      // dragging a bench player onto a slot
                if (target >= 0) {
                    if (squad[drag_bench].injured > 0) nope();   // can't field an injured player
                    else { XI[target] = drag_bench; coin(); }    // takes slot; occupant to bench
                }
            }
            drag_from = drag_bench = -1;
        }
        return;
    }

    if (state == MARKET) {
        int bench[MAXP]; int nb = bench_list(bench);
        int maxscroll = NMARKET - MKT_VIS;

        // --- scroll the buy list: wheel (over the left panel), arrow keys,
        //     ▲/▼ buttons, click-the-track-to-page, and a draggable thumb ---
        float wh = mouse_wheel();
        if (wh > 0.1f && mouse_x() < 160) mkt_scroll--;
        if (wh < -0.1f && mouse_x() < 160) mkt_scroll++;
        if (keyp(KEY_UP))   mkt_scroll--;
        if (keyp(KEY_DOWN)) mkt_scroll++;
        if (clicked(SB_X, SB_TOP, SB_W, SB_AH)) mkt_scroll--;          // ▲
        if (clicked(SB_X, SB_BOT - SB_AH, SB_W, SB_AH)) mkt_scroll++;  // ▼
        int trackH = SB_TY1 - SB_TY0;
        int thumbH = trackH * MKT_VIS / NMARKET; if (thumbH < 10) thumbH = 10;
        int thumbY = SB_TY0 + (maxscroll ? (trackH - thumbH) * mkt_scroll / maxscroll : 0);
        if (mouse_pressed(MOUSE_LEFT) && hover(SB_X, thumbY, SB_W, thumbH)) sb_drag = true;
        else if (mouse_pressed(MOUSE_LEFT) && hover(SB_X, SB_TY0, SB_W, trackH))  // page on track click
            mkt_scroll += (mouse_y() < thumbY ? -MKT_VIS : MKT_VIS);
        if (!mouse_down(MOUSE_LEFT)) sb_drag = false;
        if (sb_drag) {
            int span = trackH - thumbH;
            mkt_scroll = span > 0 ? (mouse_y() - SB_TY0 - thumbH / 2) * maxscroll / span : 0;
        }
        if (mkt_scroll < 0) mkt_scroll = 0;
        if (mkt_scroll > maxscroll) mkt_scroll = maxscroll;

        // buy buttons (only the visible window; row -> market index = mkt_scroll+row)
        for (int row = 0; row < MKT_VIS; row++) {
            int i = mkt_scroll + row; if (i >= NMARKET) break;
            int ry = MKT_ROWY(row);
            if (!msold[i] && clicked(BUY_BTNX, ry, BUY_BTNW, MKT_DY - 1)) {
                if (cash >= mprice[i] && nplayers < MAXP) {
                    cash -= mprice[i]; squad[nplayers++] = market[i]; msold[i] = true; coin();
                } else nope();
            }
        }
        // sell list: bench players (can't sell from XI directly — must bench first)
        for (int i = 0; i < nb && i < MKT_SELL; i++) {
            int ry = MKT_ROWY(i), pidx = bench[i];
            int val = sell_value(pidx);
            if (clicked(258, ry, 42, MKT_DY - 1)) {
                if (nplayers > 11) {
                    cash += val;
                    // remove pidx from squad, compact, fix XI indices
                    for (int j = pidx; j < nplayers - 1; j++) squad[j] = squad[j + 1];
                    nplayers--;
                    for (int s = 0; s < 11; s++) {
                        if (XI[s] == pidx) XI[s] = -1;
                        else if (XI[s] > pidx) XI[s]--;
                    }
                    coin();
                } else nope();
            }
        }
        return;
    }

    if (state == TABLE) {
        // standings only — play via ENTER or the SQUAD tab's PLAY MATCH button
        return;
    }
}

// ===========================================================================
// drawing
// ---------------------------------------------------------------------------
static void draw_hud(void) {
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_DARKER_BLUE);
    print(str("$%d", cash), 4, 2, CLR_YELLOW);
    // division + round in the small font so the long tier names fit between
    // the cash (left) and strength (right) readouts
    font(FONT_SMALL);
    print_centered(str("%s  R%d/%d", DIVNM[division], round_, NROUNDS), SCREEN_W/2, 3, CLR_LIGHT_YELLOW);
    font(FONT_NORMAL);
    print_right(str("STR %d", xi_strength()), 316, 2, CLR_LIME_GREEN);
}

static void draw_tabs(void) {
    const char *lab[3] = { "SQUAD", "MARKET", "LEAGUE" };
    int active = state == SQUAD ? 0 : state == MARKET ? 1 : 2;
    int tbw = SCREEN_W / 3;
    for (int i = 0; i < 3; i++) {
        int x = i * tbw, w = (i == 2 ? SCREEN_W - x : tbw);
        bool hv = hover(x, TBY, w, TBH);
        rectfill(x, TBY, w, TBH, i == active ? CLR_DARK_GREEN : hv ? CLR_DARK_GREY : CLR_BLACK);
        print(lab[i], x + (w - text_width(lab[i])) / 2, TBY + 2, i == active ? CLR_WHITE : CLR_LIGHT_GREY);
        line(x, TBY, x, TBY + TBH, CLR_DARKER_BLUE);
    }
}

// draw a player chip (a little kit jersey + number-ish) at center cx,cy
static void draw_chip(int cx, int cy, Player *p, bool small, bool ghost) {
    int r = small ? 7 : 8;
    int kit = POSC[p->pos];
    if (ghost) { fillp(FILL_CHECKER, -1); }
    circfill(cx, cy + 1, r, CLR_BLACK);                 // shadow
    circfill(cx, cy, r, kit);
    circ(cx, cy, r, CLR_WHITE);
    if (ghost) fillp_reset();
    // rating number
    const char *rt = str("%d", p->rating);
    print(rt, cx - text_width(rt) / 2, cy - 3, CLR_WHITE);
}

static void draw_pitch(void) {
    // turf
    rectfill(PX, PY, PW, PH, CLR_DARK_GREEN);
    for (int i = 0; i < PH; i += 16) rectfill(PX, PY + i, PW, 8, CLR_MEDIUM_GREEN);
    rect(PX + 3, PY + 3, PW - 6, PH - 6, CLR_LIGHT_GREY);
    line(PX + 3, PY + PH / 2, PX + PW - 3, PY + PH / 2, CLR_LIGHT_GREY);
    circ(PX + PW / 2, PY + PH / 2, 16, CLR_LIGHT_GREY);
    // goal boxes
    rect(PX + PW / 2 - 26, PY + 3, 52, 18, CLR_LIGHT_GREY);
    rect(PX + PW / 2 - 26, PY + PH - 21, 52, 18, CLR_LIGHT_GREY);

    // empty-slot markers + chips
    for (int s = 0; s < 11; s++) {
        int cx = slot_px(s), cy = slot_py(s);
        if (s == drag_from) continue;                   // drawn floating
        if (XI[s] < 0) {
            circ(cx, cy, 8, FPOS[formation][s] == POS_GK ? CLR_ORANGE : CLR_DARKER_GREY);
            print(POSL[FPOS[formation][s]], cx - text_width(POSL[FPOS[formation][s]]) / 2, cy - 3, CLR_DARK_GREY);
        } else {
            bool wrong = squad[XI[s]].pos != FPOS[formation][s];
            draw_chip(cx, cy, &squad[XI[s]], false, false);
            if (wrong && blink(20)) circ(cx, cy, 10, CLR_RED);    // out-of-position warning
        }
    }
}

static void draw_bench(void) {
    rectfill(BNX, BNY, BNW, 134, CLR_DARKER_PURPLE);
    rect(BNX, BNY, BNW, 134, CLR_DARK_PURPLE);
    print("BENCH", BNX + 4, BNY + 3, CLR_LIGHT_YELLOW);

    int bench[MAXP]; int nb = bench_list(bench);
    font(FONT_SMALL);
    for (int i = 0; i < nb && i < BN_VIS; i++) {
        int pidx = bench[i]; if (pidx == drag_bench) continue;
        int ry = BN_ROW0 + i * BN_DY;
        bool hv = hover(BNX, ry, BNW, BN_DY);
        rectfill(BNX + 2, ry, BNW - 4, BN_DY - 1, hv ? CLR_DARKER_GREY : CLR_BLACK);
        bool inj = squad[pidx].injured > 0;
        rectfill(BNX + 4, ry + 2, 6, 8, POSC[squad[pidx].pos]);
        print(POSL[squad[pidx].pos], BNX + 13, ry + 3, CLR_LIGHT_GREY);
        print(squad[pidx].name, BNX + 32, ry + 3, inj ? CLR_RED : CLR_WHITE);
        if (inj) print_right(str("INJ%d", squad[pidx].injured), BNX + BNW - 4, ry + 3, CLR_RED);
        else     print_right(str("%d", squad[pidx].rating), BNX + BNW - 4, ry + 3, rating_col(squad[pidx].rating));
    }
    if (nb == 0) print("(empty)", BNX + 6, BN_ROW0 + 2, CLR_DARK_GREY);
    if (nb > BN_VIS) print(str("+%d more", nb - BN_VIS), BNX + 6, BN_ROW0 + BN_VIS * BN_DY, CLR_DARK_GREY);
    font(FONT_NORMAL);

    // formation buttons
    for (int f = 0; f < NFORM; f++) {
        int x = BNX + f * 60, w = 56;
        bool hv = hover(x, 158, w, 12);
        rectfill(x, 158, w, 12, f == formation ? CLR_TRUE_BLUE : hv ? CLR_DARK_GREY : CLR_BLACK);
        rect(x, 158, w, 12, CLR_BLUE);
        print(FORML[f], x + (w - text_width(FORML[f])) / 2, 160, f == formation ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    // PLAY MATCH — names the next opponent; opens the preview dossier
    bool ready = xi_filled() == 11;
    bool hv = hover(BNX, 173, BNW, 14);
    int bcx = BNX + BNW / 2;
    rectfill(BNX, 173, BNW, 14, ready ? (hv ? CLR_LIME_GREEN : CLR_DARK_GREEN) : CLR_DARKER_GREY);
    rect(BNX, 173, BNW, 14, ready ? CLR_GREEN : CLR_DARK_GREY);
    font(FONT_SMALL);
    if (ready) {
        print_centered("PLAY MATCH >", bcx, 175, CLR_WHITE);
        print_centered(str("vs %s", club[upcoming_opp()].name), bcx, 181, CLR_LIGHT_YELLOW);
    } else {
        print_centered(str("XI %d/11 - need a full side", xi_filled()), bcx, 175, CLR_LIGHT_GREY);
        print_centered("drag subs in from the bench", bcx, 181, CLR_LIGHT_GREY);
    }
    font(FONT_NORMAL);
}

static void draw_floating_chip(void) {
    if (drag_from >= 0 && XI[drag_from] >= 0)
        draw_chip(mouse_x() - drag_dx, mouse_y() - drag_dy, &squad[XI[drag_from]], false, false);
    if (drag_bench >= 0)
        draw_chip(mouse_x(), mouse_y(), &squad[drag_bench], false, false);
}

// hover a pitch chip (when not dragging) to reveal who it is: name + pos + rating
static void draw_pitch_tooltip(void) {
    if (drag_from >= 0 || drag_bench >= 0) return;
    for (int s = 0; s < 11; s++) {
        if (XI[s] < 0) continue;
        int cx = slot_px(s), cy = slot_py(s);
        if (!point_in_box(mouse_x(), mouse_y(), cx - 9, cy - 9, 18, 18)) continue;
        Player *p = &squad[XI[s]];
        circ(cx, cy, 10, CLR_LIGHT_YELLOW);                 // highlight the chip
        font(FONT_SMALL);
        const char *info = str("%s  %d", POSL[p->pos], p->rating);
        int wn = text_width(p->name), wi = text_width(info);
        int w = (wn > wi ? wn : wi) + 8;
        int bx = cx - w / 2, by = cy - 23;
        if (bx < PX + 1) bx = PX + 1;
        if (bx + w > PX + PW - 1) bx = PX + PW - 1 - w;
        if (by < PY + 1) by = cy + 12;                      // flip below the chip near the top edge
        rectfill(bx, by, w, 16, CLR_BLACK);
        rect(bx, by, w, 16, CLR_LIGHT_YELLOW);
        print(p->name, bx + 4, by + 2, CLR_WHITE);
        print(info, bx + 4, by + 9, rating_col(p->rating));
        font(FONT_NORMAL);
        break;
    }
}

static void draw_squad(void) {
    cls(CLR_BLACK);
    draw_pitch();
    draw_bench();
    draw_pitch_tooltip();
    draw_floating_chip();
    draw_hud();
    draw_tabs();
    if (drag_from >= 0 || drag_bench >= 0)
        print_centered("drop on a slot, or the bench", SCREEN_W/2, PY + PH + 1, CLR_DARK_GREY);
    else
        print("drag a chip to move - hover to see who", PX + 2, PY + PH + 1, CLR_DARK_GREY);
}

// little filled triangles for the scrollbar arrow buttons
static void tri_up(int cx, int cy, int col) { for (int r = 0; r <= 3; r++) line(cx - r, cy + r, cx + r, cy + r, col); }
static void tri_dn(int cx, int cy, int col) { for (int r = 0; r <= 3; r++) line(cx - (3 - r), cy + r, cx + (3 - r), cy + r, col); }

static void draw_market(void) {
    cls(CLR_BLACK);
    print(str("TRANSFER LIST (%d)", NMARKET), 6, 16, CLR_LIGHT_YELLOW);
    print("SELL (bench only)", 168, 16, CLR_LIGHT_YELLOW);
    line(160, 28, 160, 186, CLR_DARK_GREY);

    int maxscroll = NMARKET - MKT_VIS;

    // BUY list (left column, scrolling window): chip | pos | name | rating | price-BUY
    for (int row = 0; row < MKT_VIS; row++) {
        int i = mkt_scroll + row; if (i >= NMARKET) break;
        int ry = MKT_ROWY(row);
        bool can = !msold[i] && cash >= mprice[i] && nplayers < MAXP;
        rectfill(BUY_X, ry, BUY_W, MKT_DY - 1, msold[i] ? CLR_DARKER_GREY : CLR_BROWNISH_BLACK);
        rectfill(BUY_X + 2, ry + 2, 6, MKT_DY - 5, POSC[market[i].pos]);
        font(FONT_SMALL);
        print(POSL[market[i].pos], BUY_X + 10, ry + 3, CLR_LIGHT_GREY);
        print(market[i].name, BUY_X + 30, ry + 3, CLR_WHITE);
        print_right(str("%d", market[i].rating), BUY_BTNX - 4, ry + 3, rating_col(market[i].rating));
        font(FONT_NORMAL);
        if (msold[i]) {
            font(FONT_SMALL); print("SOLD", BUY_BTNX + 8, ry + 3, CLR_DARK_GREY); font(FONT_NORMAL);
        } else {
            bool hv = hover(BUY_BTNX, ry, BUY_BTNW, MKT_DY - 1);
            rectfill(BUY_BTNX, ry, BUY_BTNW, MKT_DY - 1, can ? (hv ? CLR_LIME_GREEN : CLR_DARK_GREEN) : CLR_DARKER_GREY);
            rect(BUY_BTNX, ry, BUY_BTNW, MKT_DY - 1, can ? CLR_GREEN : CLR_DARK_GREY);
            const char *pl = str("%d", mprice[i]);
            print(pl, BUY_BTNX + (BUY_BTNW - text_width(pl)) / 2, ry + 4, CLR_WHITE);
        }
    }

    // scrollbar: track + ▲/▼ arrow buttons + proportional thumb
    int trackH = SB_TY1 - SB_TY0;
    int thumbH = trackH * MKT_VIS / NMARKET; if (thumbH < 10) thumbH = 10;
    int thumbY = SB_TY0 + (maxscroll ? (trackH - thumbH) * mkt_scroll / maxscroll : 0);
    int cx = SB_X + SB_W / 2;
    rectfill(SB_X, SB_TOP, SB_W, SB_BOT - SB_TOP, CLR_BROWNISH_BLACK);
    rectfill(SB_X, SB_TOP, SB_W, SB_AH, mkt_scroll > 0 ? (hover(SB_X, SB_TOP, SB_W, SB_AH) ? CLR_DARK_GREEN : CLR_DARKER_GREY) : CLR_BROWNISH_BLACK);
    rectfill(SB_X, SB_BOT - SB_AH, SB_W, SB_AH, mkt_scroll < maxscroll ? (hover(SB_X, SB_BOT - SB_AH, SB_W, SB_AH) ? CLR_DARK_GREEN : CLR_DARKER_GREY) : CLR_BROWNISH_BLACK);
    tri_up(cx, SB_TOP + 4, mkt_scroll > 0 ? CLR_WHITE : CLR_DARK_GREY);
    tri_dn(cx, SB_BOT - SB_AH + 5, mkt_scroll < maxscroll ? CLR_WHITE : CLR_DARK_GREY);
    rectfill(SB_X + 1, thumbY, SB_W - 2, thumbH, sb_drag ? CLR_LIME_GREEN : CLR_LIGHT_GREY);

    // SELL column (bench players)
    int bench[MAXP]; int nb = bench_list(bench);
    for (int i = 0; i < nb && i < MKT_SELL; i++) {
        int ry = MKT_ROWY(i), pidx = bench[i];
        int val = sell_value(pidx);
        bool inj = squad[pidx].injured > 0;
        rectfill(166, ry, 146, MKT_DY - 1, CLR_DARKER_PURPLE);
        rectfill(168, ry + 2, 6, MKT_DY - 5, POSC[squad[pidx].pos]);
        font(FONT_SMALL);
        print(squad[pidx].name, 178, ry + 3, inj ? CLR_RED : CLR_WHITE);
        if (inj) print_right(str("INJ%d", squad[pidx].injured), 252, ry + 3, CLR_RED);
        else     print_right(str("%d", squad[pidx].rating), 252, ry + 3, rating_col(squad[pidx].rating));
        font(FONT_NORMAL);
        // sell button at x=258, w=42
        bool can = nplayers > 11;
        bool hs = hover(258, ry, 42, MKT_DY - 1);
        rectfill(258, ry, 42, MKT_DY - 1, can ? (hs ? CLR_DARK_RED : CLR_DARKER_GREY) : CLR_DARKER_GREY);
        rect(258, ry, 42, MKT_DY - 1, can ? CLR_RED : CLR_DARK_GREY);
        const char *sl = str("%d", val);
        print(sl, 258 + (42 - text_width(sl)) / 2, ry + 4, CLR_WHITE);
    }
    if (nb == 0) print("(no spares to sell)", 168, MKT_Y0 + 2, CLR_DARK_GREY);
    if (nb > MKT_SELL) { font(FONT_SMALL); print(str("+%d more on bench", nb - MKT_SELL), 168, MKT_ROWY(MKT_SELL) + 2, CLR_DARK_GREY); font(FONT_NORMAL); }

    font(FONT_SMALL);
    print("wheel / arrows / drag the bar to scroll  -  click a price to buy or sell", 6, 183, CLR_DARK_GREY);
    font(FONT_NORMAL);
    draw_hud();
    draw_tabs();
}

static void draw_table(void) {
    cls(CLR_DARKER_BLUE);
    print_centered(str("%s  -  TABLE", DIVNM[division]), SCREEN_W/2, 14, CLR_LIGHT_YELLOW);
    // header — small font; the 12 clubs only fit when the rows are tight
    int y0 = 24;
    font(FONT_SMALL);
    print("# CLUB", 10, y0, CLR_DARK_GREY);
    print("P",   140, y0, CLR_DARK_GREY);
    print("W D L", 160, y0, CLR_DARK_GREY);
    print("GD",  236, y0, CLR_DARK_GREY);
    print_right("PTS", 308, y0, CLR_DARK_GREY);

    // sort clubs by pts then GD (indices)
    int ord[NCLUB]; for (int i = 0; i < NCLUB; i++) ord[i] = i;
    for (int a = 0; a < NCLUB; a++)
        for (int b = a + 1; b < NCLUB; b++) {
            int ga = club[ord[a]].GF - club[ord[a]].GA, gb = club[ord[b]].GF - club[ord[b]].GA;
            bool swap = club[ord[b]].pts > club[ord[a]].pts ||
                        (club[ord[b]].pts == club[ord[a]].pts && gb > ga);
            if (swap) { int t = ord[a]; ord[a] = ord[b]; ord[b] = t; }
        }
    int promo = (division == D_CL) ? 1 : 2;     // promotion places this tier
    for (int i = 0; i < NCLUB; i++) {
        int c = ord[i], ry = y0 + 10 + i * 12;
        bool me = c == YOU, up = i < promo;
        if (me) rectfill(6, ry - 1, 306, 11, CLR_DARK_GREEN);
        else if (i & 1) rectfill(6, ry - 1, 306, 11, CLR_BLACK);   // zebra rows aid scanning
        if (up) rectfill(6, ry - 1, 2, 11, CLR_LIME_GREEN);        // promotion-zone marker
        int gd = club[c].GF - club[c].GA;
        int col = me ? CLR_WHITE : CLR_LIGHT_GREY;
        print(str("%d", i + 1), 10, ry, i == 0 ? CLR_YELLOW : up ? CLR_LIME_GREEN : col);
        print(club[c].name, 24, ry, col);
        print(str("%d", club[c].W + club[c].D + club[c].L), 140, ry, col);
        print(str("%d %d %d", club[c].W, club[c].D, club[c].L), 160, ry, col);
        print(str("%+d", gd), 236, ry, gd >= 0 ? CLR_LIME_GREEN : CLR_ORANGE);
        print_right(str("%d", club[c].pts), 308, ry, me ? CLR_YELLOW : col);
    }
    font(FONT_NORMAL);

    bool ready = xi_filled() == 11;
    if (!ready)
        print_centered("set your XI first to play", SCREEN_W/2, 181, CLR_ORANGE);
    else {
        const char *goal = division == D_CL ? "WIN to be CHAMPION OF EUROPE"
                                            : "TOP 2 PROMOTE - ENTER to play";
        print_centered(goal, SCREEN_W/2, 181, CLR_LIME_GREEN);
    }

    draw_hud();
    draw_tabs();
}

static void draw_result(void) {
    cls(CLR_DARK_GREEN);
    for (int i = 0; i < SCREEN_H; i += 16) rectfill(0, i, SCREEN_W, 8, CLR_MEDIUM_GREEN);
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();

    print_centered("FULL TIME", SCREEN_W/2, 30, CLR_LIGHT_YELLOW);
    // scoreboard
    int bx = 50, by = 60, bw = 220, bh = 60;
    rectfill(bx, by, bw, bh, CLR_BLACK);
    rect(bx, by, bw, bh, CLR_LIGHT_YELLOW);
    print(club[YOU].name, bx + 12, by + 10, CLR_WHITE);
    print_right(club[res_opp].name, bx + bw - 12, by + 10, CLR_WHITE);
    print_scaled(str("%d", res_shown_my), bx + 56, by + 28, CLR_LIME_GREEN, 3);
    print_scaled("-", bx + bw / 2 - 4, by + 28, CLR_LIGHT_GREY, 3);
    print_scaled(str("%d", res_shown_op), bx + bw - 64, by + 28, CLR_RED, 3);

    if (res_t >= 1.0f) {
        const char *verdict = res_my > res_op ? "WIN!" : res_my < res_op ? "DEFEAT" : "DRAW";
        int vc = res_my > res_op ? CLR_GREEN : res_my < res_op ? CLR_RED : CLR_YELLOW;
        print_centered(verdict, SCREEN_W/2, 134, vc);
        print_centered(str("league position: %d", league_pos()), SCREEN_W/2, 150, CLR_LIGHT_YELLOW);
        if (blink(22)) print_centered(round_ >= NROUNDS ? "click for season end" : "click to continue", SCREEN_W/2, 168, CLR_WHITE);
    }
}

// draw a baked wordmark (sheet region 0,sy → 128×32) into a dest rect, fronted
// by a long EXTRUDED drop shadow: the ink (and its AA edge) is pal()-recolored
// to `sh` and stamped on a `depth`-step down-right diagonal, then the crisp
// wordmark lands on top. Keep dw:dh ≈ 4:1 to preserve the 128×32 aspect.
static void wordmark(int sy, int dx, int dy, int dw, int dh,
                     int ink, int aa, int sh, int depth) {
    pal(ink, sh); pal(aa, sh);
    for (int k = depth; k >= 1; k--) sspr(0, sy, 128, 32, dx + k, dy + k, dw, dh);
    pal_reset();
    sspr(0, sy, 128, 32, dx, dy, dw, dh);
}

static void draw_title(void) {
    cls(CLR_DARK_GREEN);
    for (int i = 0; i < SCREEN_H; i += 16) rectfill(0, i, SCREEN_W, 8, CLR_MEDIUM_GREEN);
    // baked Futura Condensed wordmarks, each on a big extruded shadow
    wordmark(0,  12, 18, 296, 74, CLR_WHITE,        CLR_LIGHT_GREY, CLR_DARKER_BLUE, 6);  // FOOTBALL
    wordmark(32, 64, 94, 192, 48, CLR_LIGHT_YELLOW, CLR_ORANGE,     CLR_DARK_BROWN,  5);  // MANAGER
    print_centered("3 seasons: EERSTE DIVISIE - EREDIVISIE - EUROPE", SCREEN_W/2, 150, CLR_LIGHT_GREY);
    const char *bl = best == 0 ? "none yet" : best == 1 ? "won the Eerste Divisie"
                   : best == 2 ? "won the Eredivisie" : "CHAMPION OF EUROPE";
    print_centered(str("career best: %s", bl), SCREEN_W/2, 164, CLR_YELLOW);
    print_centered("click / ENTER to kick off", SCREEN_W/2, 180, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

static void draw_over(void) {
    cls(CLR_DARKER_BLUE);
    rectfill(34, 32, 252, 134, CLR_BLACK);
    rect(34, 32, 252, 134, CLR_LIGHT_YELLOW);
    print_centered(str("%s  -  SEASON %d", DIVNM[division], season), SCREEN_W/2, 42, CLR_LIGHT_GREY);

    if (end_outcome == 2) {                                   // won the Champions League
        print_scaled("CHAMPIONS", (SCREEN_W - text_width("CHAMPIONS") * 2) / 2, 56, CLR_GREEN, 2);
        print_centered("OF EUROPE!", SCREEN_W/2, 80, CLR_LIME_GREEN);
    } else if (end_outcome == 1) {                            // promoted
        print_scaled("PROMOTED!", (SCREEN_W - text_width("PROMOTED!") * 2) / 2, 58, CLR_LIME_GREEN, 2);
        print_centered(str("up to the %s", DIVNM[division + 1]), SCREEN_W/2, 82, CLR_LIGHT_YELLOW);
    } else {                                                  // missed out — replay the tier
        const char *f = str("FINISHED #%d", end_pos);
        print_scaled(f, (SCREEN_W - text_width(f) * 2) / 2, 58, CLR_ORANGE, 2);
        print_centered(str("another go at the %s", DIVNM[division]), SCREEN_W/2, 82, CLR_LIGHT_YELLOW);
    }

    print_centered(str("%dW %dD %dL", club[YOU].W, club[YOU].D, club[YOU].L), SCREEN_W/2, 102, CLR_WHITE);
    print_centered(str("%d points    %+d GD", club[YOU].pts, club[YOU].GF - club[YOU].GA), SCREEN_W/2, 116, CLR_LIGHT_GREY);
    print_centered(end_outcome == 2 ? "click for a new campaign" : "click to continue",
                   SCREEN_W/2, 148, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

// pre-match dossier: scout the opponent, judge the matchup, then KICK OFF (or
// go BACK to reshape the side). The formation edge here decides the tactical tip.
static void draw_preview(void) {
    Club *o = &club[res_opp];
    int edge = formation_edge(o->style);
    int mystr = xi_strength() + edge;
    int diff  = mystr - o->str;
    int scol  = o->style == STYLE_ATTACK ? CLR_RED : o->style == STYLE_DEFENCE ? CLR_BLUE : CLR_YELLOW;

    cls(CLR_DARKER_BLUE);
    print_centered("MATCH PREVIEW", SCREEN_W / 2, 8, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print_centered(str("%s   ROUND %d/%d", DIVNM[division], round_, NROUNDS), SCREEN_W / 2, 18, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    // opponent scout report
    rectfill(20, 28, 280, 56, CLR_BLACK); rect(20, 28, 280, 56, scol);
    font(FONT_SMALL); print("SCOUT REPORT", 26, 31, CLR_DARK_GREY); font(FONT_NORMAL);
    print_scaled(o->name, (SCREEN_W - text_width(o->name) * 2) / 2, 40, CLR_WHITE, 2);
    font(FONT_SMALL);
    print(str("style: %s", STYLEL[o->style]), 26, 61, scol);
    print_right(str("strength %d", o->str), 294, 61, CLR_LIME_GREEN);
    print(str("danger man: %s (%d)", o->starnm, o->starrating), 26, 71, CLR_LIGHT_YELLOW);
    font(FONT_NORMAL);

    // your side + the matchup edge
    print(str("YOUR %s", FORML[formation]), 26, 92, CLR_WHITE);
    print_right(str("strength %d  (%+d)", mystr, edge), 294, 92, edge >= 0 ? CLR_LIME_GREEN : CLR_ORANGE);
    const char *tip = edge > 0 ? "your shape counters them - press on"
                    : edge < 0 ? "risky shape - they can exploit it"
                               : "an even tactical battle";
    print_centered(tip, SCREEN_W / 2, 106, edge > 0 ? CLR_GREEN : edge < 0 ? CLR_ORANGE : CLR_LIGHT_GREY);

    const char *v = diff > 60 ? "FAVOURITES" : diff < -60 ? "UNDERDOGS" : "EVENLY MATCHED";
    int vc = diff > 60 ? CLR_GREEN : diff < -60 ? CLR_RED : CLR_YELLOW;
    print_scaled(v, (SCREEN_W - text_width(v) * 2) / 2, 122, vc, 2);
    print_centered("go BACK to switch formation or swap players", SCREEN_W / 2, 150, CLR_DARK_GREY);

    // buttons
    bool bh = hover(16, 176, 100, 16), kh = hover(204, 176, 100, 16);
    rectfill(16, 176, 100, 16, bh ? CLR_DARK_GREY : CLR_BLACK); rect(16, 176, 100, 16, CLR_LIGHT_GREY);
    print_centered("< BACK", 66, 180, CLR_WHITE);
    rectfill(204, 176, 100, 16, kh ? CLR_LIME_GREEN : CLR_DARK_GREEN); rect(204, 176, 100, 16, CLR_GREEN);
    print_centered("KICK OFF >", 254, 180, CLR_WHITE);
}

static void draw_flash(void) {
    if (flashT <= 0) return;
    fillp(FILL_CHECKER, -1);
    rectfill(0, 0, SCREEN_W, SCREEN_H, flashColor);
    fillp_reset();
}

void draw(void) {
    switch (state) {
        case TITLE:   draw_title();   break;
        case SQUAD:   draw_squad();   break;
        case MARKET:  draw_market();  break;
        case TABLE:   draw_table();   break;
        case PREVIEW: draw_preview(); break;
        case RESULT:  draw_result();  break;
        case OVER:    draw_over();    break;
    }
    draw_flash();
}
