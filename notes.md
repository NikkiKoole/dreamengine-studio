Hi since i'm out of tokens i will do a run through all my carts, and note down things we should improve :
the goals is basically to have every cart be as good as it can be without thinking about whole new features, basically just polish. Some carts were made when we didnt have the same api we do now, so some things can be done much better now :


also another note, before mass changing all the carts and tripping over our everlasting issue with multiple agents working in index.json, we need a startegy to fix this, i think the idea became to add the stuff we have in the json in the doc header of the cart itself, but im not entirely sure how good of an idea is that , when we wnat to look through the json to find anything or search in the editor. 
well we probably need to atack this first before starting mass editing.. lets brainstorm and test this 

another idea i'm having while making this list, maybe we need another tag on the carts, where we can mark things left todo

another note: im seeing some carts that have other resolutions some square , tall etc, we need to mark this on the josn/header so we can find them separately

another thought, when we started this repo and had a few carts we added tags for games, i kinda feel we wnat way more tgs so we canmore easily find games , like text adverture, or vertical oriented or well you know lets analize the list and see what sort of tags we might want more

also:
showcase     = belongs on the public gallery
teaching     = useful because it explains one thing clearly
pillar       = future-system seed
archive      = historically important, not for browsing
retired      = superseded by a better cart
internal     = API/probe/debug only

Best of DreamEngine
Learn the API
Sound Toys
Living Worlds
Arcade Homages
Technical Labs
Retired / Historical



another thing i noticed, when i play/run a cart and escape the thumb is empty and needs a second or soe to show up again, why is taht? we havent baked a new screnshot fro the cart?

ok and sice we have a slightly new north star, our collaboration is first, being tutorially not anymore, first off check and reason to think if we need more tools that will help you do this work below.

basically im walking through the carts selecting 'recently updated' backwards. (more note bass is an outlier becasue i just built it)

ok the carts
# more note bass: 
the knobs should function and be positioned like in one note bass, also the xy pads should be more rectangular and as alrge as can be, we will have some space left to thnk about more pads . also we should make starting a ui interction by dragging not trigger another interactoin when we move over. we have a cart where we do this correctly. so basically when you roatte a knob or start a xy pad and move your pressed mouse over something else that shoulds react . you have to START the interaction there.  (we fixed this in dubdesk, all our muscial carts with many knobs and sliders and xy pads should behave liek this )


another note, we probably need to analize and mark every cart what sort of touch joystick it would ideally get. (if any)

# falling sand: 
there should be cure sprite buttons to select what you will draw, it should also work on mouse

# boids: 
mobile needs onscreen joystick, we will get a lot of tehse, maybe dont do it directly but mark and think how we will suport various gameplays on touch.

# verlet rope:
needs mouse to position root, lets the red thing be draggable too, maybe adda knob to change rope length and make z an x button cure pixel buttons. 

# ripple tank
mouse enable to drop stones in water too, maybe add some sfx

# a* pathfinding:
add cute pixel buttons, support mouse

# maze generator: 
add cute pixel button (maybe a bigger one then we have now, 32x32 or 24x24 , lets discuss api or do it just in this cart  for that)

# lil life:
the counters only go down, never up, he always has a z even though appraently he's doing things, i couldnt find out what buttons to use for building? can we think of mouse interaction here? 

# double pendulum: 
add another big cute button (maze wants that too) to start again 

# doom fire:
how to support on mobile, onscreen joystick ?

# sorting visualizer:
labels offscreen , maybe add some cute pixelbuttons

# pathfinding :
support mouse, drag start and end, draw button fro walls and eraser , cute pixel buttons for the various path algos
dont auto restart when done. 

# l system plants :
when i grow the plant (next iteration) it gets more branches but vertically it kinda shrinks. i think we want to grow the plant form small and not many brances to bigger with more branches? now it ffeels a bit backward.
also lets add cute pixel buttons for the various plant algos 
changing the angle sometimes for no clear reason changes the size of the plant too

# tiny city: 
can we do some smarter tile neighbor checking so we can roads oriented the right way depending on its neighbors

# le petomane:
mouse support, sliders should work in mouse/toch too,
clicking on th ecushion shoud rip too, maybe add cute pixel buttons for the presets, maybe add some reverb/echo to make it even greater

# music garden: 
all good

# juice:
make the text label buttons also clickable by mouse

# particles: 
make the 2 text buttons into cute pixel buttons
also a general note, we started this repo with arrows and ab buttons (gamepad like) i still want everything also working on gamepad (or atleast possible) so we have to think a bit about this, sometimes we only want 2 onscreen buttons ()for mouse/touch but still have ab trgger them too.
another thing about particles, but that might be a next version, this is just 1 sort of firey, fountain effect, i would liek us to think about a more extravagant particle ssytem for other effects.

# 17 hypotrochoid:
lets add a single big cute pixel button to start the next , also display the name of the thing onscreen.

# 12 high score:
fine as is, we could support mouse/touch maybe ? 

# 11 noise: 
fine

# 10 build a world:
lets add onscreen joystick
also about this, i feel i want to add the joystick in code, not via the settings, so when this cart is commiled and showsn on a touch device it uses onscreen joystick (still we need ways to in this case just show the poke only no buttons)

# 9 enemies;
on screen joystick, way to get past intro page on touch 

# 8 catch the star:
i dont see the star to catch, (onscreen joystick)

# 7 keeping score:
maybe istead of onscreen buttons we will use thisone to trigger touch overlay buttons ? instead of zx ? what do you think

# 6 sound and music:
fine

# 5b colorkey:
the sprite has a weird black line 
make the text button clickable by mouse/touch

# 5 draw your character:
needs touch joystick onscreen

# 4 press a button:
remind me to actually start hooking up a gamepad controller and start testing if that works too

# 3 things that move:
fine as is

# 1 hello screen:
fine

# instruments:
add an onscreen toggle to stop/start the auto play
maket the 4 instrument blocks , clickable/ you just play when clicked

# lfo :
same as instruments above

# 20 effects
fade denst go all the way to black
lets make these buttons cute pixel buttons, also supported by gamepad

# world pointer:
lets also support mouse world drag perhaps only when mouse middle button pressed to not get inthe way of flag adding.
maybe we can add onscreen buttons for rotate and clear, rotat ebuttons might be interreting to see if can differ between click button and keeping it pressed and triggering continuosly button

# arcs :
fine

# infiniminer:
this needs some attention i feel i cant rotate correctly, and am looking at theinside of voxels/boxes the whole time. i also dont relaly like the dithering we use. maybe we should use our trutype colors for this one to tint the blocks better ?

# textured3d:
this needs amousesupport zoom wheel, drag rotate, i want the text labels clickable. and would also like the mouse to be able to change rotation 

# solid 3d:  
also in need of better mouse support, labels fall of

# 3d wireframe:
same as the other 2 above, also the l/r spin is weird, it sort of makes the spinning stop but i cant start it agaon.

basically the 3 carts above need better mosue support, we want a toggle onscreen to turn on/off the auto rttaing and want the mouse to be able to wheel to zoom, and drag to rotate
this might also be the moment to really test this on touch, lets atleast leave seams we need so zooming can be triggerd by pinch maybe

# 16 spirograph:
fine

# ragdoll:
mark this one as needing better invetsigaing touch.

# physics playground:
same as ragdoll, we need to brainstorm how to have a palette of drawable AND touch support, maybe a toggle onscreen that lets us set the active tool, add something, or just move, and then touch/mouse alswasy just does that . so we can get away from alwasy needing to have a z,x a,b button pair which we dont alwsy

# trifill stress
feels kinda fine. mayeb also textlabels that need to support mosue. i think we were invetsiagting this at one point, i even think , yes we have the cart 'UI kit'

# fonts :
fine

# deluxe paint:
can we toggle the cycle off by default, this tool could be improved a lot we have a lot of new commands nowadays but that si a bgger task ouside this list.

# 24 moving patterns
make button label clickable 

# 2 shapes and colors:
fine

# bloom
the new planst all sort of appear around the middle of the screen never at the beginning and end. lets also add cute on screen a b buttons .

# merchant
clicking on a product (spice/silk) should not buy that, we have the buy button for that . also the houses need some work.

# swarmturf :
fine!

# sand burrow :
your character sometimes suddenly is back on top of the sand.
needs some touch/mouse support to also steer him ? (maybe onscreen joystick)

# rollswarm
needs onscreen joystick for touch 
feesl a bit random, maybe some variation in enemy speed would help

# beatcript
needs onscrren restart clickable label, want dpad onscreen for touch (not joystick)

# lil farm
the topbar is overlapping part of the gameworld
the feedback text is gone very quickly
its unclear how to harvest ?
hoe seed can needs to be cute onscreen pixel buttons. 
and this needs to have gamepad suport in some way. need to brainstorm

# deckbuilder 
fine

# streetfighter
needs 2 rounds, like streetfighetr has, and we also need to check this with gamepad.

# pool
this feels  abit backward, on touch and also mouse i want to mimic how you drag the stick bend the bal, now you aim where you will shoot, if that makes sense, so it needs to be inverted, lets discuss this a bit

# qbert 
the choice of buttons to steer is awkward, but im not really sure how the original worked, also the piramid is rendered in a weird way/order, 

# boulder dash
love this one!

# dig dug
the thing i persoanlly liek about the original of dig dug is the music too, so lets get a rhytm in an a  melody that sort of stops playing when you stop moving. the game feels unfisninhed, i dont really know what im supposed to do ? there is nothing to collect ?

# defender 
needs touch onscreen joystcik

# centipede 
pretty good, probably needs onscreen joystick for touch, maybe we can make the beep a bit less ,  drives me crazy after 10 seconds

# missile command 
feels nice

# minesweeper
feels ncie

# boggle 
lets use our comic font for this one, also make it more juicy with some text effects we have

# hangman
lest make the library of words it can be a bit bigger also less nerdy. lets use comic font for the big text, text fx . and make the hanged person look a lot better, also make him look dead when lost (x x eyes)
make the gallow look better too with a bit more clunky lines and perhabs some dithering or something

# wordle
when you pick a wrong word, or just some letters, you should loose a row too. now you can keep on trying as long as the word/even wrong word , is unrecognized

# columns
when you delete some blocks would be nice if we tween the blocks in place, instead of just jumping them in.
needs onscren joystick + ab

# puyo
ui labels are overlapping al over the place
needs start panel. 
when blokcs drop and touch the groudn they overlap and then jump back. needs touch joystick

# dr mario
feels nice, win panel needs a bit more space, text is overlapping, touch joystci

# bejeweled 
nice!

# battleship
needs on screen pixel button so we can play on touch (rotate)
needs a win/lose panel at the end
maybe we could do some spritework for the ships ?

# connect 4
text is overlaping in the bottom
when i place a dics, i wnat to see it slide down, now it just appears . maybe have a little rhythm section playing jazzy brush + upright  in the background.

# othello
maybe we can add a sort of cheat mode, when i hover a place i could click to faintly see the effect of doing that move?

# worms
the exposions need some juice, we have particels, look at boom for inspiration, smoke dithering , etc. 
also the sound wehn moving is annoying, jyst make some shuffling sound. a simple chiptune in the background would be nice

# monkey island
text is overlapping going offscreen
we can use smaller font 
we need to sort on y so character can walk in front or back of things, and maybe we can make a very simple muse-like musical thing that repsonds to what we are doing, monkey island is famous for it and we have all the tools we need for that 
win state fades the screen to half or something, 

# leusure suit larry 
text is overlapping going offscreen
wen talking to ladys we want to see close up, also make bgger face respond to my words.

a slightly bigger task, but i would really loike if we can go outside, wander on the street aimlessly, get driven over by a car and or get in a fight in an alley

jukebox should reall play a song. we can do that nowadays, we have many radio carts 

win screen fades to half (like monkey island)

# the incredble machine
lets make r and c buttons cute pixel buttons we can touch click 
can we have a little blip sound effect when the ball hits the ground

# transport tycoon
gosh yes, hard to say what this cart proves, the least fune game thus far.. haha lets keep it but mark it as needing brainstorimng, we have many other carts and systems since then and could maybe add some cool systems here.

# california games half pipe
the guy leaves a trail, 
its unclear how to steer, do i press z? mouse sends hi flying, he is roatted the wrong way around. if all that works we might want to add a tiny bit of verlet fun to make it look nicer? its a promising cart but very unplayable right now

# summer games 
i need to alternate click z,x button but x takes me back to begin. cant get past that right now, need to look later again

# winter games
winter game is also getting keys messed up. z is needed to go back but doenst trigger that . 

# zak mckracken
same z layering as the other adventure games, this also has some color choices that make things hard to see, the fade on the end (same as other adventure games)


# civ tiny 
text overflows 
i think we need to pick if we reseacrh OR build warriors OR settlers, but i dont know if we shoud go in these details for this game.  also founding 3 cities is a werid win state, fade screen on the end, and would be maybe better with an enemy (like 4x) 

# jumpstar
same fade end issue, this happens a lot,
onscreen joystick for touch

# football manager
some overflwoing text, maybe use smaller font sometimes 
nice game, might be somewthing to grow further

# bones
lets use smaller font  so we can write body parts less cryptic, i couldnt find anymore how to change the rig, i know we could. ah tab shou dhave that onscreen to, maybe have a little help panel. this cart needs some work to make more pleasnt to use, its a proising tool we could start using , but we wont now. ideas aplenty for this, basically about what sort of shapes/colors the body part will have etc.

# smooch lounge
the dancer is sort of very weird, it renders in 2 places at once. it wudl also be very cool if the thinsg we rhytmically did would aslo make sesne for the song, the song in the background can use some cleanup too, lets do a jazzy combo, brushy snare, upright bass, and some pentatonic lead i can play ?  it also needs an intro panel and maybe we could look for a ttf font to make into the title ? 

# pinball
plays good, i think i would like it better if we would make this screen 320x240 flipped so 240x320, also some fatter pixel fun, not the single thickness lines, and we ought to use more polygon stuff instead of those blobby approaches. 
i like the sound and bg music, and the machine plays decent.

# sky strike
myabe the title screen can use a better font, but it plays nice! very good!

# composer mario paint sound
some overlapping labels, pretty good!

# operation wolf
nie game, i dont know how the original handled it but it feels weird when you completely run out of amm, but its a good mechanic to not have unlimited either, i thinki ts fine!

# excitebike
pretty good, dont really know what to do other thehn keeping gas pressed though ;)

# outrun
trees & the cars flicker, adn i kinda feel i want to use real polygons for car drawing, dont like the various resolutions. 
also on a crash i think the roll is weird, maybe we can think of something better ? can we also have a visible guy and girl in the car? make it loks like a pico32  testarossa

# xcom
nice game, has some overlapping text

# doom
looks like i can only shoot once? 


# micro 4x
nice game!, i was most addicted to this in the first weeks! 

# galaga
fine, needs onscreen joystick for touch
sfx are a bit annying

# m.u.l.e
i dont udnerstand this gaem, i cant remeber that the original mule ws this sort of rhytm game?

# solvalou xevious
i odnt like how the terrain is thi noisy random stuff, can t we keep track of what tile goes where?

# joust
pretty good, the enemies and background shoudl prolly not have same colour though

# papers please
use small font where needed
maybe some text effects too ()

# lil hero zelda
haha well, gosh, what can i say. what do you think?

# bubble bobble
pretty good, except when i let a captured enemy in bubble go to the top there is no way to catch him anymore, in the real game the kind of had away to get nback in the game

# bomberman
nice game, would like to have better sprite work for the fire though (neighbour aware tiling technicques) 

# pang
prety good, we can improve the sound, have some echo, let the buncing balls make a nice sound, maybe a sweet little minimal melody in the back

# onearmed bandit
i want to see the spining things, also i want to see what i just didnt reach basically i wnat to see 3 rows, top and bottom one a bit cutoff and really have the effect of the spin. also some better sfx, use some fm sythn and bells 

# shakespearean insults
very nice, lets make a click on the screen also trigger a new one, and lets play more with the text (like in sloppy text)

# napoleon radio 
this has some issue where the solo part gets cut of by anothe rinstrument


# tower defense
its fine, probaly needs onscreen joystick for touch, or well maybe cute pixel button to sleect tool with is enough.
# jungle run
cant get off teh rope

# street brawler
this game seems identical to streetfigher

# fire!
its impossibl eto catch the second baby in time, also the original has just a few discrete steps, which enables more complex bouncyness. 

# tetris
nice!

# escape the cave
ok very simpe still needs z odering like the other advenrture games

# rogue
nice, probably onscreen josytick for touch


# lil blob (pet)
outline doesnt align with the shape
needs moue/touch support

# sokoban
som text overlfow issues, this could do with some more levels look up originla sokoban levels or make inetresying puzzles

# sopwith camel
feels weird, can we look up how the original worked more rprecsley 
# paper.io
works nice!

# burger time
plays nice!

# pac-man
yeah we have a little issue with our exact level layout, we ought to miimic the level layout of level 1 pacman exatcly 

# vampire survivors
nice put in onscreen joystick

# katamari
we shou dtry to really use this cart to showcase all the great drawing capabilities we have, all the objects can be way more interresting
