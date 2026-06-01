#include "studio.h"
#include <string.h>
#include <stddef.h>

// BOGGLE — drag across adjacent letters to spell words, beat the clock.
//   mouse_pressed/down/released  trace a chain of 8-way-adjacent cells
//   embedded dictionary (binary-search) validates words of length 3+
//   dt() countdown round + bar() HUD + save_int("best") high score
// Drag the mouse across adjacent letters; release to submit. R = restart.

// ---- embedded dictionary (sorted, all length 3-8, common short words) ----
static const char *DICT[] = {
    "ace","act","add","ado","age","ago","aid","ail","aim","air","ale","all","amp",
    "and","ant","any","ape","apt","arc","are","ark","arm","art","ash","ask","ate",
    "awe","axe","aye","bad","bag","ban","bar","bat","bay","bed","bee","beg","bet",
    "bid","big","bin","bit","boa","bog","boo","bow","box","boy","bra","bud","bug",
    "bun","bus","but","buy","cab","cad","cam","can","cap","car","cat","cob","cod",
    "cog","con","coo","cop","cot","cow","cry","cub","cue","cup","cur","cut","dab",
    "dad","dam","day","den","dew","did","die","dig","dim","din","dip","doe","dog",
    "don","dot","dry","dub","due","dug","ear","eat","ebb","eel","egg","ego","elf",
    "elk","elm","end","eon","era","eve","ewe","eye","fad","fan","far","fat","fax",
    "fed","fee","fen","few","fib","fig","fin","fir","fit","fix","fly","foe","fog",
    "for","fox","fry","fun","fur","gag","gap","gas","gel","gem","get","gig","gin",
    "gnu","goa","gob","god","goo","got","gum","gun","gut","guy","gym","had","hag",
    "ham","has","hat","hay","hem","hen","her","hew","hex","hey","hid","him","hip",
    "his","hit","hoe","hog","hop","hot","how","hub","hue","hug","hum","hut","ice",
    "icy","ill","imp","ink","inn","ion","ire","irk","its","ivy","jab","jag","jam",
    "jar","jaw","jay","jet","jig","job","jog","jot","joy","jug","jut","keg","ken",
    "key","kid","kin","kit","lab","lad","lag","lap","law","lax","lay","led","leg",
    "let","lid","lie","lip","lit","lob","log","lop","lot","low","lug","lye","mac",
    "mad","man","map","mar","mat","maw","may","men","met","mew","mid","mix","mob",
    "mod","mom","mop","mow","mud","mug","mum","nab","nag","nap","nay","net","new",
    "nib","nil","nip","nit","nob","nod","nor","not","now","nub","nun","nut","oak",
    "oar","oat","odd","ode","off","oft","ohm","oil","old","one","opt","orb","ore",
    "our","out","owe","owl","own","pad","pal","pan","pap","par","pat","paw","pay",
    "pea","peg","pen","pep","per","pet","pew","pie","pig","pin","pit","ply","pod",
    "pop","pot","pow","pro","pry","pub","pug","pun","pup","pus","put","rag","ram",
    "ran","rap","rat","raw","ray","red","ref","rep","rib","rid","rig","rim","rip",
    "rob","rod","roe","rot","row","rub","rue","rug","rum","run","rut","rye","sac",
    "sad","sag","sap","sat","saw","say","sea","see","set","sew","she","shy","sic",
    "sin","sip","sir","sit","six","ski","sky","sly","sob","sod","son","sop","sow",
    "soy","spa","spy","sty","sub","sue","sum","sun","sup","tab","tad","tag","tan",
    "tap","tar","tat","tax","tea","ted","tee","ten","the","thy","tic","tie","tin",
    "tip","toe","tog","tom","ton","too","top","tot","tow","toy","try","tub","tug",
    "tun","tut","two","urn","use","van","vat","vet","via","vie","vim","vow","wad",
    "wag","wan","war","was","wax","way","web","wed","wee","wet","who","why","wig",
    "win","wit","woe","wok","won","woo","wow","wry","yak","yam","yap","yaw","yea",
    "yen","yes","yet","yew","yip","you","zag","zap","zed","zen","zig","zip","zit",
    "zoo",
    "able","acid","acre","aero","aged","ahoy","aide","ajar","akin","alas","ally",
    "aloe","also","alto","amid","amir","ammo","amok","amps","anew","anti","ants",
    "apex","apse","aqua","arch","arcs","area","aria","arid","arms","army","arts",
    "ashy","atom","aunt","aura","auto","aver","avid","avow","away","awed","axes",
    "axis","axle","babe","back","bade","bail","bait","bake","bald","bale","balk",
    "ball","balm","band","bane","bang","bank","barb","bard","bare","bark","barn",
    "base","bash","bask","bass","bath","bats","bawl","bays","bead","beak","beam",
    "bean","bear","beat","beau","beck","beds","beef","been","beer","bees","beet",
    "bell","belt","bend","bent","best","beta","bevy","bias","bide","bike","bile",
    "bilk","bill","bind","bird","bite","bits","blab","blah","bled","blip","blob",
    "bloc","blot","blow","blue","blur","boar","boas","boat","bode","body","bogs",
    "boil","bold","bolt","bomb","bond","bone","bong","bony","book","boom","boon",
    "boor","boos","boot","bore","born","bose","boss","both","bout","bowl","bows",
    "boys","brag","bran","bras","brat","bray","bred","brew","brig","brim","brow",
    "buck","buds","buff","bugs","bulb","bulk","bull","bump","bums","bung","bunk",
    "buns","buoy","burn","burp","burr","bury","bush","bust","busy","butt","buys",
    "buzz","byte","cabs","cafe","cage","cake","calf","call","calm","came","camp",
    "cane","cans","cape","caps","card","care","carp","cars","cart","case","cash",
    "cask","cast","cats","cave","cell","cent","chap","char","chat","chef","chew",
    "chic","chin","chip","chop","chow","chub","chug","chum","cite","city","clad",
    "clam","clan","clap","claw","clay","clef","clip","clod","clog","clop","clot",
    "club","clue","coal","coat","coax","cobs","cock","coda","code","coed","cogs",
    "coil","coin","coke","cola","cold","colt","coma","comb","come","cone","conk",
    "cons","cook","cool","coop","coos","cope","cops","copy","cord","core","cork",
    "corn","cost","cosy","cots","couch","cove","cows","cozy","crab","crag","cram",
    "crew","crib","crop","crow","crud","crux","cube","cubs","cued","cues","cuff",
    "cull","cult","cups","curb","curd","cure","curl","curs","curt","cusp","cuts",
    "dabs","dais","dale","dame","damn","damp","dams","dare","dark","darn","dart",
    "dash","data","date","daub","dawn","days","daze","dead","deaf","deal","dean",
    "dear","debt","deck","deed","deem","deep","deer","deft","defy","dell","demo",
    "dens","dent","deny","desk","dews","dewy","dial","dice","dies","diet","digs",
    "dike","dill","dime","dine","ding","dins","dint","dips","dire","dirt","disc",
    "dish","disk","ditz","dive","dock","docs","does","doff","doge","dogs","dole",
    "doll","dolt","dome","done","dons","doom","door","dope","dork","dorm","dose",
    "dote","dots","dour","dove","down","doze","drab","drag","dram","draw","dray",
    "dreg","drew","drip","drop","drub","drug","drum","dual","dub","duck","duct",
    "dude","duds","duel","dues","duet","duff","dugs","duke","dull","duly","dumb",
    "dump","dune","dung","dunk","duns","dupe","dusk","dust","duty","dyed","dyes",
    "each","earl","earn","ears","ease","east","easy","eats","ebbs","echo","edge",
    "edgy","edit","eels","eery","eggs","egos","eke","elks","ells","elms","else",
    "emit","emus","ends","envy","eons","epic","eras","euro","even","ever","eves",
    "evil","ewes","exam","exes","exit","expo","eyed","eyes","face","fact","fade",
    "fads","fail","fair","fake","fall","fame","fang","fans","fare","farm","fast",
    "fate","fats","faun","fawn","faze","fear","feat","feed","feel","fees","feet",
    "fell","felt","fend","fern","fest","feud","fiat","fibs","figs","file","fill",
    "film","find","fine","fink","fins","fire","firm","firs","fish","fist","fits",
    "five","fizz","flag","flak","flap","flat","flaw","flax","flay","flea","fled",
    "flee","flew","flex","flip","flit","floe","flog","flop","flow","flue","flux",
    "foal","foam","fobs","foci","foes","fogs","foil","fold","folk","fond","font",
    "food","fool","foot","ford","fore","fork","form","fort","foul","four","fowl",
    "foxy","frat","fray","free","fret","frog","from","fuel","full","fume","fund",
    "funk","furl","furs","fury","fuse","fuss","fuzz","gabs","gads","gaff","gage",
    "gags","gain","gait","gala","gale","gall","game","gang","gaol","gape","gaps",
    "garb","gash","gasp","gate","gave","gawk","gaze","gear","geek","gees","geld",
    "gels","gems","gene","gent","germ","gift","gigs","gild","gill","gilt","gird",
    "girl","gist","give","glad","glee","glen","glib","glob","glow","glue","glum",
    "glut","gnat","gnaw","goad","goal","goat","gobs","gods","goer","goes","gold",
    "golf","gone","gong","good","goof","goon","goop","gore","gory","gosh","gout",
    "gown","grab","gram","gray","grew","grey","grid","grim","grin","grip","grit",
    "grow","grub","gulf","gull","gulp","gums","guns","guru","gush","gust","guts",
    "guys","gyms","hack","haft","hags","hail","hair","hake","hale","half","hall",
    "halo","halt","hams","hand","hang","hank","hard","hare","hark","harm","harp",
    "hash","hate","hats","haul","have","hawk","hays","haze","hazy","head","heal",
    "heap","hear","heat","heck","heed","heel","heft","heir","held","hell","helm",
    "help","hemp","hems","hens","herb","herd","here","hero","hers","hewn","hews",
    "hick","hide","high","hike","hill","hilt","hind","hint","hips","hire","hiss",
    "hits","hive","hoax","hobo","hock","hods","hoed","hoes","hogs","hold","hole",
    "holy","home","hone","hood","hoof","hook","hoop","hoot","hope","hops","horn",
    "hose","host","hots","hour","hove","howl","hubs","hued","hues","huff","huge",
    "hugs","hula","hulk","hull","hump","hums","hung","hunk","hunt","hurl","hurt",
    "hush","husk","huts","hymn","hype","iamb","ibex","ibis","iced","ices","icky",
    "icon","idea","idle","idly","idol","iffy","ilks","ills","imps","inch","info",
    "inks","inky","inns","into","ions","iota","ired","ires","iris","irks","iron",
    "isle","itch","item","jabs","jack","jade","jail","jamb","jams","jars","java",
    "jaws","jays","jazz","jean","jeep","jeer","jell","jerk","jest","jets","jibe",
    "jigs","jilt","jinx","jive","jobs","jock","jogs","john","join","joke","jolt",
    "josh","jots","joys","judo","jugs","juju","juke","jump","junk","jury","just",
    "jute","juts","kale","kart","kayo","keel","keen","keep","kegs","kelp","kept",
    "keys","khan","kick","kids","kiln","kilt","kind","king","kink","kins","kiss",
    "kite","kits","kiwi","knee","knew","knit","knob","knot","know","koala","kohl",
    "kris","labs","lace","lack","lacy","lads","lady","lags","laid","lair","lake",
    "lamb","lame","lamp","land","lane","laps","lard","lark","lash","lass","last",
    "late","lath","laud","lava","lawn","laws","lays","laze","lazy","lead","leaf",
    "leak","lean","leap","lear","leas","leek","leer","lees","left","legs","lend",
    "lens","lent","less","lest","lets","levy","liar","lice","lick","lids","lied",
    "lief","lien","lies","lieu","life","lift","like","lilt","lily","limb","lime",
    "limp","line","link","lint","lion","lips","lisp","list","lits","live","load",
    "loaf","loam","loan","lobe","lobs","loci","lock","loci","lode","loft","logo",
    "logs","loin","loll","lone","long","look","loom","loon","loop","loot","lope",
    "lops","lord","lore","lorn","lose","loss","lost","lots","loud","lout","love",
    "lows","luau","luck","lude","lugs","lull","lumber","lump","lunar","lung","lure",
    "lurk","lush","lust","lute","lynx","lyre","mace","made","maid","mail","maim",
    "main","make","male","mall","malt","mama","mane","many","maps","mare","mark",
    "mars","mart","mash","mask","mass","mast","mate","math","mats","matt","maul",
    "maws","maze","mead","meal","mean","meat","meek","meet","meld","melt","memo",
    "mend","menu","meow","mere","mesh","mess","mete","mews","mica","mice","mild",
    "mile","milk","mill","mils","mime","mind","mine","mini","mink","mint","minx",
    "mire","miss","mist","mite","mitt","moan","moat","mobs","mock","mode","mods",
    "mojo","mold","mole","moll","molt","mom","moms","monk","mood","moon","moor",
    "moos","moot","mope","mops","more","morn","moss","most","mote","moth","move",
    "mows","much","muck","muff","mugs","mule","mull","mums","muse","mush","musk",
    "must","mute","mutt","myth","nabs","nags","nail","name","nape","naps","narc",
    "nary","nave","navy","nays","near","neat","neck","need","neon","nerd","nest",
    "nets","newt","next","nibs","nice","nick","nigh","nile","nine","nips","nits",
    "node","nods","noel","noes","none","nook","noon","noose","norm","nose","nosy",
    "note","noun","nova","nubs","nuke","null","numb","nuns","nurse","nuts","oaf",
    "oafs","oaks","oars","oath","oats","obey","oboe","odds","odes","odor","ogle",
    "ogre","oils","oily","oink","okay","okra","oldie","omen","omit","once","ones",
    "only","onto","onus","onyx","ooze","oozy","opal","open","opts","opus","oral",
    "orbs","ores","organ","oust","ouch","ours","oust","outs","oval","oven","over",
    "owed","owes","owls","owns","oxen","pace","pack","pact","pads","page","paid",
    "pail","pain","pair","pale","pall","palm","pals","pane","pang","pans","pant",
    "papa","paps","pare","park","parr","pars","part","pass","past","pate","path",
    "pats","pave","pawn","paws","pays","peak","peal","pear","peas","peat","peck",
    "peek","peel","peep","peer","pegs","pelt","pens","pent","peon","peps","perk",
    "perm","pert","peso","pest","pets","pews","pick","pied","pier","pies","pigs",
    "pike","pile","pill","pimp","pine","ping","pink","pins","pint","pion","pipe",
    "pips","pita","pith","pits","pity","plan","play","plea","pled","plod","plop",
    "plot","plow","ploy","plug","plum","plus","pock","pods","poem","poet","poke",
    "poky","pole","poll","polo","pomp","pond","pone","pong","pony","pooh","pool",
    "poop","poor","pope","pops","pore","pork","porn","port","pose","posh","post",
    "posy","pots","pour","pout","pram","pray","prep","prey","prim","prod","prom",
    "prop","prow","pub","pubs","puck","puff","pugs","puke","pull","pulp","puma",
    "pump","punk","puns","punt","puny","pupa","pups","pure","purl","purr","push",
    "puts","putt","quad","quay","quid","quip","quit","quiz","race","rack","racy",
    "raft","rage","rags","raid","rail","rain","rake","ramp","rams","rang","rank",
    "rant","rape","raps","rapt","rare","rash","rasp","rate","rats","rave","raw",
    "rays","raze","razz","read","real","ream","reap","rear","redo","reds","reed",
    "reef","reek","reel","refs","rein","rely","rend","rent","rest","ribs","rice",
    "rich","rick","ride","rids","rife","riff","rift","rigs","rile","rill","rime",
    "rims","rind","ring","rink","riot","ripe","rips","rise","risk","rite","road",
    "roam","roar","robe","robs","rock","rode","rods","roe","roes","roil","role",
    "roll","romp","rood","roof","rook","room","root","rope","rosy","rote","rots",
    "rove","rows","rube","rubs","ruby","rude","rued","rues","ruff","rugs","ruin",
    "rule","rump","rums","rune","rung","runs","runt","ruse","rush","rust","ruts",
    "sack","sacs","safe","saga","sage","sago","sags","said","sail","sake","sale",
    "salt","same","sand","sane","sang","sank","saps","sari","sash","sass","sate",
    "save","saws","says","scab","scam","scan","scar","scat","scow","scud","scum",
    "seal","seam","sear","seas","seat","sect","seed","seek","seem","seen","seep",
    "seer","sees","self","sell","semi","send","sent","sept","sera","serf","sets",
    "sewn","sews","sext","shad","shag","sham","shed","shes","shew","shim","shin",
    "ship","shod","shoe","shoo","shop","shot","show","shun","shut","sick","side",
    "sift","sigh","sign","silk","sill","silo","silt","sing","sink","sins","sips",
    "sire","sirs","site","sits","size","skew","skid","skim","skin","skip","skis",
    "skit","slab","slag","slam","slap","slat","slaw","slay","sled","slew","slid",
    "slim","slip","slit","slob","sloe","slog","slop","slot","slow","slug","slum",
    "slur","slut","smog","smug","smut","snag","snap","snip","snit","snob","snot",
    "snow","snub","snug","soak","soap","soar","sobs","sock","soda","sofa","soft",
    "soil","sold","sole","solo","some","song","sons","soon","soot","sops","sora",
    "sore","sort","souk","soul","soup","sour","sown","sows","soya","spam","span",
    "spar","spas","spat","spay","sped","spew","spin","spit","spot","spry","spud",
    "spun","spur","stab","stag","star","stay","stem","step","stew","stir","stop",
    "stow","stub","stud","stun","stye","subs","such","suds","sued","sues","suet",
    "suit","sulk","sumo","sump","sums","sung","sunk","suns","sups","sure","surf",
    "swab","swag","swam","swan","swap","swat","sway","swig","swim","swum","sync",
    "tabs","tabu","tack","taco","tact","tads","tags","tail","take","talc","tale",
    "talk","tall","tame","tamp","tang","tank","tans","tape","taps","tare","tarn",
    "taro","tarp","tars","tart","task","taut","taxi","teak","teal","team","tear",
    "teas","teat","teem","teen","tees","tell","temp","tend","tens","tent","term",
    "tern","test","text","than","that","thaw","thee","them","then","they","thin",
    "this","thou","thud","thug","thus","tick","tics","tide","tidy","tied","tier",
    "ties","tiff","tile","till","tilt","time","tine","ting","tins","tint","tiny",
    "tips","tire","toad","toed","toes","toga","togs","toil","told","toll","tomb",
    "tome","toms","tone","tong","tons","took","tool","toot","tope","tops","tore",
    "torn","tort","tosh","toss","tote","tots","tour","tout","town","tows","toys",
    "trad","tram","trap","tray","tree","trek","trim","trio","trip","trod","trot",
    "troy","true","tsar","tuba","tube","tubs","tuck","tufa","tuff","tuft","tugs",
    "tuna","tune","turf","turn","tusk","tutu","twig","twin","twit","tyke","type",
    "typo","tyre","tyro","ugly","ulna","undo","unit","unto","upon","urea","urge",
    "urns","used","user","uses","vain","vale","vamp","vane","vans","vary","vase",
    "vast","vats","veal","veer","veil","vein","veld","vend","vent","verb","very",
    "vest","veto","vets","vial","vibe","vice","vied","vies","view","vile","vine",
    "viol","visa","vise","void","vole","volt","vote","vows","wade","wads","waft",
    "wage","wags","waif","wail","wait","wake","wale","walk","wall","wand","wane",
    "want","ward","ware","warm","warn","warp","wars","wart","wary","wash","wasp",
    "watt","wave","wavy","waxy","ways","weak","weal","wean","wear","webs","weds",
    "weed","week","weep","weft","weir","weld","well","welt","wend","went","wept",
    "were","west","wets","wham","what","when","whet","whey","whim","whip","whir",
    "whit","whiz","whoa","whom","wick","wide","wife","wigs","wild","wile","will",
    "wilt","wily","wimp","wind","wine","wing","wink","wins","wipe","wire","wiry",
    "wise","wish","wisp","wist","wits","wive","woes","woke","woks","wold","wolf",
    "womb","wons","wont","wood","woof","wool","woos","word","wore","work","worm",
    "worn","wort","wove","wows","wrap","wren","writ","yack","yaks","yams","yang",
    "yank","yaps","yard","yarn","yawl","yawn","yaws","yeah","year","yeas","yell",
    "yelp","yens","yews","yips","yock","yoga","yogi","yoke","yolk","yore","your",
    "yowl","yoyo","yule","yurt","zags","zany","zaps","zeal","zebra","zero","zest",
    "zigs","zinc","zing","zip","zips","zits","zone","zonk","zoom","zoos",
};
static const int DICT_N = (int)(sizeof(DICT)/sizeof(DICT[0]));

// weighted letter bag (scrabble-ish frequency) for playable boards
static const char BAG[] =
    "aaaaaaaaa""bb""cc""dddd""eeeeeeeeeeee""ff""ggg""hh""iiiiiiiii"
    "j""k""llll""mm""nnnnnn""oooooooo""pp""q""rrrrrr""ssss""tttttt"
    "uuuu""vv""www""x""yy""z";
static const int BAG_N = (int)(sizeof(BAG)-1);

// ---- layout ----
#define GRID   4
#define CELLPX 44
#define GAP    6
static int gridX, gridY;   // top-left of the board on screen

// ---- state ----
static char board[GRID][GRID];
static int  chain[GRID*GRID];   // packed cell indices (r*GRID+c) in trace order
static int  chainLen;
static int  tracing;
static float roundT;            // seconds remaining
#define ROUND_LEN 90.0f
static int  score, best, found;
static char lastWord[20];       // last submitted word
static int  lastGood;           // 1=accepted, 0=rejected, -1=none
static float flashT;            // flash timer (color tint)
static int  flashColor;
static int  over;               // round-over flag

// recently-found word ribbon (most recent first)
#define FOUND_MAX 40
static char foundList[FOUND_MAX][10];
static int  foundCount;

// linear scan — the list is grouped by length, not globally sorted, and lookups
// only happen on word submit (rare), so a scan of ~2500 entries is plenty fast.
static int dict_has(const char *w) {
    for (int i = 0; i < DICT_N; i++)
        if (strcmp(w, DICT[i]) == 0) return 1;
    return 0;
}

static int already_found(const char *w) {
    for (int i = 0; i < foundCount; i++)
        if (strcmp(w, foundList[i]) == 0) return 1;
    return 0;
}

static void new_board(void) {
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++)
            board[r][c] = BAG[rnd(BAG_N)];
    chainLen = 0; tracing = 0;
}

static void start_round(void) {
    new_board();
    roundT = ROUND_LEN;
    score = 0; found = 0; foundCount = 0;
    lastWord[0] = 0; lastGood = -1;
    flashT = 0; over = 0;
}

void init(void) {
    gridX = (SCREEN_W - (GRID*CELLPX + (GRID-1)*GAP)) / 2;
    gridY = 40;
    best = load_int("best", 0);
    // a little plucky chime instrument for accepted words
    instrument(5, INSTR_TRI, 4, 60, 4, 180);
    start_round();
}

// which cell is the pointer over? returns packed index or -1
static int cell_at(int mx, int my) {
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++) {
            int x = gridX + c*(CELLPX+GAP);
            int y = gridY + r*(CELLPX+GAP);
            if (point_in_box(mx, my, x, y, CELLPX, CELLPX)) return r*GRID + c;
        }
    return -1;
}

static int in_chain(int cell) {
    for (int i = 0; i < chainLen; i++) if (chain[i] == cell) return 1;
    return 0;
}

// 8-way grid adjacency between two packed cells
static int adjacent(int a, int b) {
    int dr = abs(a/GRID - b/GRID);
    int dc = abs(a%GRID - b%GRID);
    return (dr <= 1 && dc <= 1 && (dr || dc));
}

static void submit(void) {
    if (chainLen >= 3) {
        char w[20];
        for (int i = 0; i < chainLen; i++) w[i] = board[chain[i]/GRID][chain[i]%GRID];
        w[chainLen] = 0;
        strcpy(lastWord, w);
        if (already_found(w)) {
            lastGood = 0; flashColor = CLR_ORANGE; flashT = 0.35f;
            note(60, INSTR_TRI, 3);
        } else if (dict_has(w)) {
            lastGood = 1; flashColor = CLR_GREEN; flashT = 0.45f;
            score += chainLen; found++;
            if (foundCount < FOUND_MAX) strcpy(foundList[foundCount++], w);
            // ascending chime, longer word = brighter
            int n = 60 + chainLen * 3;
            note(n, 5, 5); schedule(70, n + 4, 5, 4); schedule(140, n + 7, 5, 4);
            if (score > best) { best = score; save_int("best", best); }
        } else {
            lastGood = 0; flashColor = CLR_RED; flashT = 0.35f;
            shake(3); note(48, INSTR_SAW, 4);
        }
    } else if (chainLen > 0) {
        lastGood = 0; flashColor = CLR_RED; flashT = 0.2f;
        note(50, INSTR_SAW, 3);
    }
    chainLen = 0; tracing = 0;
}

void update(void) {
    if (flashT > 0) flashT -= dt();

    if (over) {
        if (keyp('R') || keyp(KEY_ENTER) || mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE))
            start_round();
        return;
    }

    roundT -= dt();
    if (roundT <= 0) {
        roundT = 0; over = 1; tracing = 0; chainLen = 0;
        note(72, INSTR_SINE, 4); schedule(150, 67, INSTR_SINE, 4); schedule(300, 60, INSTR_SINE, 4);
        return;
    }

    int mx = mouse_x(), my = mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {
        int cell = cell_at(mx, my);
        if (cell >= 0) { tracing = 1; chainLen = 0; chain[chainLen++] = cell; hit(64, 5, 3, 60); }
    }

    if (tracing && mouse_down(MOUSE_LEFT)) {
        int cell = cell_at(mx, my);
        if (cell >= 0 && !in_chain(cell) && chainLen > 0 && adjacent(chain[chainLen-1], cell)) {
            chain[chainLen++] = cell;
            hit(64 + chainLen, 5, 3, 50);   // rising click per added letter
        }
    }

    if (tracing && mouse_released(MOUSE_LEFT)) submit();

    // R reshuffles mid-round too (fresh letters, keeps the clock)
    if (keyp('R')) { new_board(); lastWord[0] = 0; lastGood = -1; }
}

static void cell_center(int cell, int *cx, int *cy) {
    int r = cell/GRID, c = cell%GRID;
    *cx = gridX + c*(CELLPX+GAP) + CELLPX/2;
    *cy = gridY + r*(CELLPX+GAP) + CELLPX/2;
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    // title
    print_scaled("BOGGLE", 8, 6, CLR_LIGHT_YELLOW, 2);
    print_right(str("BEST %d", best), SCREEN_W - 8, 8, CLR_LIGHT_GREY);
    print_right(str("SCORE %d", score), SCREEN_W - 8, 20, CLR_WHITE);

    // timer bar
    bar(8, 26, 120, 6, roundT/ROUND_LEN, CLR_GREEN, CLR_DARKER_GREY);
    print(str("%2d s", (int)roundT), 132, 25, roundT < 10 ? CLR_RED : CLR_LIGHT_GREY);

    int hover = (!over) ? cell_at(mouse_x(), mouse_y()) : -1;

    // tiles
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++) {
            int cell = r*GRID + c;
            int x = gridX + c*(CELLPX+GAP);
            int y = gridY + r*(CELLPX+GAP);
            int sel = in_chain(cell);
            int tcol = CLR_DARKER_BLUE;
            if (sel) {
                if (flashT > 0 && lastGood == 1) tcol = CLR_DARK_GREEN; else tcol = CLR_TRUE_BLUE;
            } else if (cell == hover && !tracing) tcol = CLR_BLUE_GREEN;
            // shadow + tile face
            rectfill(x+2, y+3, CELLPX, CELLPX, CLR_BROWNISH_BLACK);
            rectfill(x, y, CELLPX, CELLPX, tcol);
            rect(x, y, CELLPX, CELLPX, sel ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
            // letter (uppercase)
            char ch = board[r][c] - 32;
            char s[2] = { ch, 0 };
            int lc = sel ? CLR_WHITE : CLR_LIGHT_PEACH;
            print_scaled(s, x + (CELLPX - text_width(s)*3)/2, y + (CELLPX - 21)/2, lc, 3);
        }

    // trace line over the chain
    if (chainLen > 1) {
        int px, py;
        cell_center(chain[0], &px, &py);
        for (int i = 1; i < chainLen; i++) {
            int cx, cy; cell_center(chain[i], &cx, &cy);
            line(px, py, cx, cy, CLR_YELLOW);
            line(px, py+1, cx, cy+1, CLR_ORANGE);
            px = cx; py = cy;
        }
    }
    for (int i = 0; i < chainLen; i++) {
        int cx, cy; cell_center(chain[i], &cx, &cy);
        circfill(cx, cy, 3, CLR_YELLOW);
    }

    // current word being traced
    int wordY = gridY + GRID*(CELLPX+GAP) + 6;
    if (tracing && chainLen > 0) {
        char w[20];
        for (int i = 0; i < chainLen; i++) w[i] = board[chain[i]/GRID][chain[i]%GRID] - 32;
        w[chainLen] = 0;
        print_centered(w, wordY, chainLen >= 3 ? CLR_GREEN : CLR_LIGHT_GREY);
    } else if (lastGood >= 0 && lastWord[0]) {
        // feedback on the last submission
        char up[20]; int i;
        for (i = 0; lastWord[i]; i++) up[i] = lastWord[i] - 32; up[i] = 0;
        int col = lastGood == 1 ? CLR_GREEN : CLR_RED;
        print_centered(str("%s %s", up, lastGood == 1 ? str("+%d", (int)strlen(lastWord)) : "X"),
                       wordY, (flashT > 0) ? flashColor : col);
    } else {
        print_centered("drag across adjacent letters", wordY, CLR_DARK_GREY);
    }

    // found-words count + recent ribbon
    print(str("WORDS %d", found), 8, SCREEN_H - 26, CLR_LIGHT_YELLOW);
    {
        int rx = 70, n = 0;
        for (int i = foundCount - 1; i >= 0 && n < 5; i--, n++) {
            char up[10]; int j;
            for (j = 0; foundList[i][j]; j++) up[j] = foundList[i][j] - 32; up[j] = 0;
            int w = text_width(up);
            if (rx + w > SCREEN_W - 6) break;
            print(up, rx, SCREEN_H - 26, CLR_MEDIUM_GREEN);
            rx += w + 8;
        }
    }
    print_centered("hold + drag adjacent letters  -  release to submit  -  R reshuffle",
                   SCREEN_H - 12, CLR_DARK_GREY);

    // round over overlay
    if (over) {
        fade(0.55f);
        int bw = 220, bh = 96, bx = (SCREEN_W-bw)/2, by = (SCREEN_H-bh)/2;
        rectfill(bx, by, bw, bh, CLR_DARKER_PURPLE);
        rect(bx, by, bw, bh, CLR_LIGHT_YELLOW);
        print_scaled("ROUND OVER", bx + (bw - text_width("ROUND OVER")*2)/2, by + 10, CLR_LIGHT_YELLOW, 2);
        print_centered(str("%d words   %d points", found, score), by + 40, CLR_WHITE);
        if (score >= best && score > 0)
            print_centered("new best!", by + 54, CLR_GREEN);
        else
            print_centered(str("best %d", best), by + 54, CLR_LIGHT_GREY);
        if (blink(20))
            print_centered("press R to play again", by + 74, CLR_YELLOW);
    }
}
