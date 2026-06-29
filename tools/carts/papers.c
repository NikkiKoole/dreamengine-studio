/* de:meta
{
  "title": "papers, please",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "title-play-gameover-loop",
    "save-load-persistence"
  ],
  "lineage": "Homage to Lucas Pope's Papers, Please (2013); novel in distilling the multi-document cross-checking loop and escalating rule set into a ~200-line single-file cart.",
  "genre": "simulation",
  "homage": "Papers, Please (2013)",
  "description": "A document-inspection game after Lucas Pope's classic. An applicant hands over a passport (and, on later days, an entry permit); cross-check their fields against the day's posted RULEBOOK and stamp APPROVE or DENY. Catch expired passports, banned nations, missing permits, mismatched names and expired permits — the rules tighten each day, so a clean entrant one day is a denial the next. Five mistakes and you're terminated. A approves, B denies. Glory to Arstotzka."
}
de:meta */
#include "studio.h"

// PAPERS, PLEASE  — border checkpoint inspector
// An applicant hands over documents. Cross-check them against the day's posted
// RULES, then stamp: A = APPROVE, B = DENY. Deny anyone who breaks a rule;
// approve anyone clean. The rules tighten each day. Five mistakes and you're out.
// Glory to Arstotzka.

#define QUOTA   6          // applicants per day
#define MAX_STRIKE 5

enum { PROCESS, RESULT, DAYINTRO, OVER };

static const char *FIRST[8] = { "JAN", "ELENA", "IVAN", "MARIA", "PETR", "ANYA", "DMITRI", "OLGA" };
static const char *LAST[8]  = { "KOSTAVA", "VONEL", "SAROYAN", "DURITSA", "MORATSKY", "LASZLO", "RYBAK", "KOVAC" };
static const char *NATION[6]= { "ARSTOTZKA", "KOLECHIA", "OBRISTAN", "REPUBLIA", "ANTEGRIA", "IMPOR" };
static const int   SKIN[6]  = { CLR_PEACH, CLR_LIGHT_PEACH, CLR_DARK_PEACH, CLR_PEACH, CLR_LIGHT_PEACH, CLR_DARK_PEACH };
static const int   HAT[6]   = { CLR_DARK_RED, CLR_DARK_GREEN, CLR_INDIGO, CLR_DARK_BLUE, CLR_BROWN, CLR_DARK_PURPLE };

// rules active by day
static bool r_banned, r_permit, r_names, r_pexp;
static int  banned;          // banned nation index (when r_banned)

// current applicant
static int  a_first, a_last, a_nation;
static int  a_dob, a_exp;            // dates as YYYYMMDD
static bool a_haspermit, a_namebad;
static int  a_permit_last, a_pexp;
static bool a_valid;                 // derived truth
static char a_reason[40];

static int  cyear, cmon, cday;       // current date
static int  day, processed, score, strikes, hiscore;
static int  state, last_correct;
static float timer_until;
static int  stamp;                    // 0 none, 1 approved, 2 denied

// ====================================================================

static int cur_date() { return cyear * 10000 + cmon * 100 + cday; }
static const char *fmt_date(int v) { return str("%d.%02d.%02d", v / 10000, (v / 100) % 100, v % 100); }

static int gen_date(int y0, int y1) {   // random date with year in [y0,y1]
    int y = rnd_between(y0, y1 + 1);
    return y * 10000 + rnd_between(1, 13) * 100 + rnd_between(1, 29);
}

static void set_rules() {
    r_banned = day >= 2;
    r_permit = day >= 3;
    r_names  = day >= 3;
    r_pexp   = day >= 4;
    banned   = (day + 1) % 6;          // a rotating banned nation
    if (banned == 0) banned = 1;       // never ban Arstotzka itself
}

static void set_reason(const char *s) { int i = 0; while (s[i] && i < 39) { a_reason[i] = s[i]; i++; } a_reason[i] = 0; }

static void derive_truth() {
    a_valid = false;
    if (a_exp < cur_date())                                set_reason("EXPIRED PASSPORT");
    else if (r_banned && a_nation == banned)               set_reason("BANNED NATION");
    else if (r_permit && !a_haspermit)                     set_reason("NO ENTRY PERMIT");
    else if (r_permit && a_haspermit && a_namebad)         set_reason("NAME MISMATCH");
    else if (r_pexp && a_haspermit && a_pexp < cur_date()) set_reason("PERMIT EXPIRED");
    else { a_valid = true; a_reason[0] = 0; }
}

static void gen_applicant() {
    a_first = rnd(8); a_last = rnd(8);
    a_nation = rnd_between(1, 6);                 // default: not Arstotzka, and (re-rolled below) not banned
    a_dob  = gen_date(cyear - 60, cyear - 18);
    a_exp  = gen_date(cyear + 1, cyear + 4);      // valid by default
    a_haspermit  = r_permit;                      // present when required
    a_namebad    = false;
    a_permit_last = a_last;
    a_pexp = gen_date(cyear + 1, cyear + 3);

    if (a_nation == banned) a_nation = (banned % 5) + 1;   // start clean

    if (chance(50)) {                              // inject one violation the rules can catch
        int opts[5], n = 0;
        opts[n++] = 0;                             // expired passport (always a rule)
        if (r_banned) opts[n++] = 1;
        if (r_permit) { opts[n++] = 2; opts[n++] = 3; }
        if (r_pexp)   opts[n++] = 4;
        switch (opts[rnd(n)]) {
            case 0: a_exp = gen_date(cyear - 4, cyear - 1); break;
            case 1: a_nation = banned; break;
            case 2: a_haspermit = false; break;
            case 3: a_namebad = true; a_permit_last = (a_last + 1 + rnd(6)) % 8; break;
            case 4: a_pexp = gen_date(cyear - 3, cyear - 1); break;
        }
    }
    derive_truth();
    stamp = 0;
}

static void next_day() {
    day++; processed = 0;
    cday++; if (cday > 28) { cday = 1; cmon++; if (cmon > 12) { cmon = 1; cyear++; } }
    set_rules();
    state = DAYINTRO; timer_until = now() + 2.6f;
}

static void new_game() {
    cyear = 1982; cmon = 11; cday = 23;
    day = 1; processed = 0; score = 0; strikes = 0;
    set_rules();
    gen_applicant();
    state = PROCESS;
}

void init() { hiscore = load(0); new_game(); }

static void decide(bool approve) {
    last_correct = (approve == a_valid);
    stamp = approve ? 1 : 2;
    if (last_correct) { score++; note(76, INSTR_SINE, 3); }
    else {
        strikes++; note(40, INSTR_SAW, 4);
        if (strikes >= MAX_STRIKE) { state = OVER; if (score > hiscore) { hiscore = score; save(0, hiscore); } return; }
    }
    state = RESULT; timer_until = now() + 1.5f;
}

void update() {
    if (state == OVER) { if (btnp(0, BTN_A) || btnp(0, BTN_B)) new_game(); return; }
    if (state == DAYINTRO) { if (now() > timer_until || btnp(0, BTN_A) || btnp(0, BTN_B)) { gen_applicant(); state = PROCESS; } return; }
    if (state == RESULT) {
        if (now() > timer_until) {
            processed++;
            if (processed >= QUOTA) next_day();
            else { gen_applicant(); state = PROCESS; }
        }
        return;
    }
    // PROCESS
    if (btnp(0, BTN_A)) decide(true);
    if (btnp(0, BTN_B)) decide(false);
}

// ====================================================================

static void draw_applicant(int x, int y) {
    int blnk = (frame() / 50) % 12 == 0;
    rectfill(x - 12, y + 10, 24, 16, CLR_DARK_GREY);     // shoulders
    circfill(x, y, 9, SKIN[a_nation]);                    // head
    rectfill(x - 10, y - 9, 20, 4, HAT[a_nation]);        // hat
    rectfill(x - 7, y - 12, 14, 4, HAT[a_nation]);
    if (!blnk) { pset(x - 3, y - 1, CLR_BLACK); pset(x + 3, y - 1, CLR_BLACK); }
    line(x - 3, y + 4, x + 3, y + 4, CLR_DARK_BROWN);     // mouth
}

void draw() {
    cls(CLR_DARKER_PURPLE);

    // top bar
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(str("DAY %d", day), 4, 2, CLR_LIGHT_GREY);
    print_centered("CHECKPOINT", SCREEN_W/2, 2, CLR_LIGHT_YELLOW);
    print_right(str("SCORE %d  X%d/%d", score, strikes, MAX_STRIKE), SCREEN_W - 4, 2, strikes >= 3 ? CLR_RED : CLR_LIGHT_GREY);

    if (state == DAYINTRO) {
        print_centered(str("-- DAY %d --", day), SCREEN_W/2, 70, CLR_LIGHT_YELLOW);
        print_centered(str("DATE %s", fmt_date(cur_date())), SCREEN_W/2, 84, CLR_WHITE);
        if (day == 1)      print_centered("ENTRANTS NEED A PASSPORT", SCREEN_W/2, 100, CLR_LIGHT_GREY);
        else if (day == 2) print_centered(str("NOW DENYING: %s", NATION[banned]), SCREEN_W/2, 100, CLR_RED);
        else if (day == 3) print_centered("NOW: ENTRY PERMIT + NAMES MATCH", SCREEN_W/2, 100, CLR_RED);
        else if (day == 4) print_centered("NOW: CHECK PERMIT EXPIRY", SCREEN_W/2, 100, CLR_RED);
        else               print_centered(str("DENYING: %s", NATION[banned]), SCREEN_W/2, 100, CLR_RED);
        print_centered("press A", SCREEN_W/2, 130, CLR_DARK_GREY);
        return;
    }

    draw_applicant(30, 40);
    print(a_valid && false ? "" : "\"MAY I ENTER?\"", 6, 80, CLR_LIGHT_GREY);

    // passport
    int px = 66, py = 16;
    rectfill(px, py, 130, 72, CLR_DARK_RED);
    rect(px, py, 130, 72, CLR_LIGHT_YELLOW);
    print("PASSPORT", px + 36, py + 4, CLR_LIGHT_YELLOW);
    rectfill(px + 6, py + 16, 28, 32, SKIN[a_nation]);    // photo
    rect(px + 6, py + 16, 28, 32, CLR_BLACK);
    circfill(px + 20, py + 28, 6, HAT[a_nation]);
    print(str("%s", FIRST[a_first]), px + 38, py + 17, CLR_WHITE);
    print(str("%s", LAST[a_last]), px + 38, py + 26, CLR_WHITE);
    print(str("%s", NATION[a_nation]), px + 38, py + 38, a_nation == banned && r_banned ? CLR_RED : CLR_LIGHT_YELLOW);
    print(str("DOB %s", fmt_date(a_dob)), px + 6, py + 52, CLR_WHITE);
    print(str("EXP %s", fmt_date(a_exp)), px + 6, py + 61, a_exp < cur_date() ? CLR_RED : CLR_WHITE);

    // entry permit
    if (r_permit) {
        int qy = py + 76;
        if (a_haspermit) {
            rectfill(px, qy, 130, 32, CLR_TRUE_BLUE);
            rect(px, qy, 130, 32, CLR_WHITE);
            print("ENTRY PERMIT", px + 28, qy + 3, CLR_WHITE);
            print(str("%s %s", FIRST[a_first], LAST[a_permit_last]), px + 6, qy + 13, CLR_LIGHT_PEACH);
            if (r_pexp) print(str("EXP %s", fmt_date(a_pexp)), px + 6, qy + 22, a_pexp < cur_date() ? CLR_RED : CLR_LIGHT_PEACH);
        } else {
            print("(no entry permit)", px + 18, qy + 12, CLR_DARK_GREY);
        }
    }

    // rules
    int rx = 204, ry = 16;
    rectfill(rx, ry, SCREEN_W - rx - 2, 120, CLR_BLACK);
    rect(rx, ry, SCREEN_W - rx - 2, 120, CLR_LIGHT_GREY);
    print("RULEBOOK", rx + 18, ry + 3, CLR_LIGHT_YELLOW);
    print(str("DATE %s", fmt_date(cur_date())), rx + 4, ry + 14, CLR_WHITE);
    int ly = ry + 28;
    print("- PASSPORT REQ", rx + 4, ly, CLR_LIGHT_GREY); ly += 9;
    print("- NO EXPIRED", rx + 4, ly, CLR_LIGHT_GREY); ly += 9;
    if (r_banned) { print(str("- BAN %s", NATION[banned]), rx + 4, ly, CLR_RED); ly += 9; }
    if (r_permit) { print("- ENTRY PERMIT", rx + 4, ly, CLR_LIGHT_GREY); ly += 9; }
    if (r_names)  { print("- NAMES MATCH", rx + 4, ly, CLR_LIGHT_GREY); ly += 9; }
    if (r_pexp)   { print("- PERMIT VALID", rx + 4, ly, CLR_LIGHT_GREY); ly += 9; }

    // decision prompt / stamp
    print_centered(str("APPLICANT %d/%d", processed + 1, QUOTA), SCREEN_W/2, 150, CLR_DARK_GREY);
    print("A: APPROVE", 30, 165, CLR_GREEN);
    print_right("DENY :B", SCREEN_W - 30, 165, CLR_RED);

    if (stamp) {
        int col = stamp == 1 ? CLR_GREEN : CLR_RED;
        rect(96, 44, 70, 22, col); rect(97, 45, 68, 20, col);
        print(stamp == 1 ? "APPROVED" : " DENIED", 104, 51, col);
    }
    if (state == RESULT)
        print_centered(last_correct ? "CORRECT" : str("WRONG - %s", a_reason), SCREEN_W/2, 182, last_correct ? CLR_GREEN : CLR_RED);

    if (state == OVER) {
        rectfill(SCREEN_W / 2 - 74, SCREEN_H / 2 - 24, 148, 50, CLR_BLACK);
        rect    (SCREEN_W / 2 - 74, SCREEN_H / 2 - 24, 148, 50, CLR_RED);
        print_centered("TERMINATED", SCREEN_W/2, SCREEN_H / 2 - 14, CLR_RED);
        print_centered(str("processed %d  (day %d)", score, day), SCREEN_W/2, SCREEN_H / 2 - 1, CLR_YELLOW);
        print_centered("Z to report for duty", SCREEN_W/2, SCREEN_H / 2 + 13, CLR_LIGHT_GREY);
    }
}
