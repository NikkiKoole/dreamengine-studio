/* de:meta
{
  "title": "shakespearean insults",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "Procedural text from three word lists. Pick one word from each column and glue them into Thou [A] [B] [C]! With ~50 words per column thats over a hundred thousand insults from one tiny program — that multiplication is combinatorics, and the picking is just rnd() indexing into arrays, the same trick behind procedural names and loot. Z hurls a fresh one."
}
de:meta */
#include "studio.h"

// SHAKESPEAREAN INSULT KIT — procedural text from three word lists.
//
// Pick one word from each column and glue them together: "Thou [A] [B] [C]!"
// With ~50 words per column that's 50 x 50 x 50 = over a hundred thousand
// insults from one tiny program. That multiplication is COMBINATORICS, and
// picking the words is just rnd() indexing into arrays — the same trick behind
// procedural names, levels, and loot in real games.
//
//   Z   hurl a fresh insult

static const char *C1[] = {
    "artless","bawdy","beslubbering","bootless","churlish","cockered","clouted",
    "craven","currish","dankish","dissembling","droning","errant","fawning",
    "fobbing","frothy","gleeking","goatish","gorbellied","impertinent","jarring",
    "loggerheaded","lumpish","mammering","mangled","mewling","paunchy","pribbling",
    "puking","puny","quailing","rank","reeky","roguish","ruttish","saucy","spongy",
    "surly","tottering","unmuzzled","vain","villainous","warped","wayward","yeasty",
};
static const char *C2[] = {
    "base-court","bat-fowling","beef-witted","beetle-headed","boil-brained",
    "clapper-clawed","clay-brained","common-kissing","crook-pated","dismal-dreaming",
    "dizzy-eyed","dog-hearted","dread-bolted","earth-vexing","elf-skinned",
    "fat-kidneyed","fen-sucked","flap-mouthed","fly-bitten","folly-fallen",
    "fool-born","full-gorged","guts-griping","half-faced","hasty-witted",
    "hedge-born","hell-hated","idle-headed","ill-nurtured","knotty-pated",
    "milk-livered","motley-minded","onion-eyed","plume-plucked","pottle-deep",
    "pox-marked","reeling-ripe","rough-hewn","rude-growing","rump-fed",
    "shard-borne","sheep-biting","spur-galled","swag-bellied","toad-spotted",
    "unchin-snouted","weather-bitten",
};
static const char *C3[] = {
    "apple-john","baggage","barnacle","bladder","boar-pig","bugbear","bum-bailey",
    "canker-blossom","clack-dish","clotpole","coxcomb","codpiece","death-token",
    "dewberry","flap-dragon","flax-wench","flirt-gill","foot-licker","fustilarian",
    "giglet","gudgeon","haggard","harpy","hedge-pig","horn-beast","hugger-mugger",
    "joithead","lewdster","lout","maggot-pie","malt-worm","mammet","measle","minnow",
    "miscreant","moldwarp","mumble-news","nut-hook","pigeon-egg","pignut","puttock",
    "pumpion","ratsbane","scut","skainsmate","strumpet","varlet","vassal","wagtail",
    "whey-face",
};
#define N1 (int)(sizeof(C1) / sizeof(C1[0]))
#define N2 (int)(sizeof(C2) / sizeof(C2[0]))
#define N3 (int)(sizeof(C3) / sizeof(C3[0]))

static int i1 = 2, i2 = 9, i3 = 17;   // a tasty default for the thumbnail
static int spin = 0;

void update(void) {
    if (spin > 0) {
        spin--;
        if (spin % 2 == 0) { i1 = rnd(N1); i2 = rnd(N2); i3 = rnd(N3); hit(70, INSTR_SQUARE, 1, 18); }
        if (spin == 0) hit(76, INSTR_SINE, 4, 120);   // the "ding" when it settles
    }
    if (btnp(0, BTN_A) && spin == 0) spin = 22;
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);
    print_centered("SHAKESPEAREAN INSULT KIT", SCREEN_W/2, 8, CLR_LIGHT_GREY);

    print_centered("Thou", SCREEN_W/2, 44, CLR_MAUVE);
    print_centered(C1[i1], SCREEN_W/2, 64, CLR_LIGHT_YELLOW);
    print_centered(C2[i2], SCREEN_W/2, 82, CLR_ORANGE);
    print_centered(str("%s!", C3[i3]), SCREEN_W/2, 100, CLR_PINK);

    print_centered(str("1 of %d insults", N1 * N2 * N3), SCREEN_W/2, 130, CLR_DARK_GREY);
    print_centered("Z: hurl another", SCREEN_W/2, SCREEN_H - 11, CLR_LIGHT_GREY);
}
