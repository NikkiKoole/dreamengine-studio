# Learn to Make Games with Dreamengine

**For beginners — no experience needed!**

You're about to write real code that makes games. Not Scratch blocks, not drag-and-drop — actual C code that your computer compiles and runs. By the end of these tutorials you'll have made your own game from scratch.

Each tutorial is a cart you can load, run, and change. Break things. See what happens. That's how programmers learn.

---

## The Tutorials

### Part 1 — Hello, Screen!
*Your very first program*

You'll make the screen light up with a color and put some words on it. It sounds small but this is the hardest step — after this, everything else builds on it.

- What `draw()` is and why it exists
- Clearing the screen with `cls()`
- Writing text with `print()`
- Picking a color

---

### Part 2 — Shapes and Colors
*Drawing with code*

Forget pencils — you'll draw with numbers. Squares, circles, lines, and single pixels, all in 32 colors.

- The color palette (32 colors, all named)
- Rectangles: `rect()` and `rectfill()`
- Circles: `circ()` and `circfill()`
- Lines: `line()`
- Single pixels: `pset()`
- The coordinate system (0,0 is the top-left corner)

---

### Part 3 — Things That Move
*Variables and the game loop*

Right now everything is frozen. In this tutorial you'll learn how to make things move across the screen every frame.

- What a variable is (a box that holds a number)
- `update()` — the function that runs 60 times a second
- Moving something by changing its x and y every frame
- What happens at the edges of the screen

---

### Part 4 — Press a Button
*Taking control*

Time to control something yourself. You'll move a character around the screen using the keyboard.

- Reading the keyboard with `btn()`
- The six buttons: up, down, left, right, A, B
- Moving your character left/right/up/down
- Keeping your character on screen (clamping)

---

### Part 5 — Draw Your Character
*Sprites*

Words and shapes are fine, but now you'll draw your own pixel art character in the sprite editor and put them in your game.

- Using the sprite editor
- Drawing a 16×16 character
- `spr()` — drawing a sprite by its number
- Facing left and right with `sprf()`

---

### Part 6 — Sounds and Music
*Making noise*

Games need sound! You'll add a jump sound, a coin sound, and some background music with just one line of code each.

- Playing a sound effect with `sfx()`
- Playing music with `music()`
- Playing a single note with `note()`
- Matching sound to things that happen (jumping, collecting)

---

### Part 7 — Keeping Score
*Numbers on screen*

Every game needs a score. You'll learn how to track a number and show it on screen.

- Keeping a score variable
- Printing numbers with `print()`
- Making the score go up when something happens

---

### Part 8 — Catch the Star
*Your first complete game*

Put everything together into a real game: a player that moves, a star that appears in a random spot, a score that goes up when you catch it, and a sound that plays each time.

- Putting `update()` and `draw()` together
- Simple collision (are two things in the same place?)
- Randomness: making the star appear somewhere new each time
- A complete game loop: start → play → score

---

### Part 9 — Enemies!
*Something that chases you*

Add a simple enemy that moves towards your player. Now there's danger — and a game over.

- Moving one thing towards another
- Checking if the player and enemy are touching
- A game over screen
- Restarting the game

---

### Part 10 — Build a World
*The map editor*

Your game world can be bigger than the screen. You'll paint a tiled map and scroll through it.

- Painting tiles in the map editor
- Drawing the map with `map()`
- Scrolling the camera with `camera()`
- Checking what tile is at a position (for collision with walls)

---

## What's Next?

After these tutorials you'll know enough to make almost anything. Some ideas to try on your own:

- A maze game
- A top-down adventure
- A platformer
- A two-player game (player 0 uses WASD, player 1 uses arrow keys)

The API reference lists every function available — hover over any function name in the editor to see what it does.
