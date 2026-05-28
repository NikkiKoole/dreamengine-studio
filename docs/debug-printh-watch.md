# Debug tools — `printh()` + `watch()`

**Status:** design sketched, not implemented. Pick up here.
**Date drafted:** 2026-05-29

## Why this is the next move

For the target audience (teens learning C), debugging is the single biggest force multiplier. Right now if a cart misbehaves there's nowhere to talk back — `printf` is swallowed, the only way to inspect state is `print()` to the game canvas with hand-rolled `snprintf` boilerplate.

Two small features fix 80% of that:

- **`printh(fmt, ...)`** — printf into a panel in the editor (not the game window). "print-to-host", name borrowed from PICO-8, also dodges the conflict with `<math.h>`'s `log()`.
- **`watch(name, fmt, ...)`** — register a named value, runtime draws a live overlay in the corner of the game window. Updated every frame.

Both ship together because they answer the same two beginner questions: *"what is this variable right now?"* and *"did this branch even run?"*

---

## API additions (`studio.h`)

```c
// debug
void printh(const char *fmt, ...);                       // printf to the editor log panel
void watch(const char *name, const char *fmt, ...);      // live overlay entry
void watch_visible(bool on);                             // hide/show overlay (default: on)
```

`watch()` is printf-style so one function covers any type:

```c
watch("x",   "%d", x);
watch("pos", "(%d, %d)", x, y);
watch("vel", "%.2f", vx);
```

---

## `printh` — the log panel

### Runtime (`studio.c`)

```c
void printh(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);   // line-buffer through the pipe
}
```

Stderr because raylib already writes its TRACELOG noise to stdout — stderr stays clean for cart output.

### `main.cjs` — capture and forward

Change the `spawn`:

```js
// was: stdio: 'ignore'
const proc = spawn(CART_BIN, [], {
  detached: false,                            // editor closing kills the cart
  stdio:    ['ignore', 'pipe', 'pipe'],
  cwd:      BUILD_DIR,
})

proc.stderr.on('data', chunk => {
  win.webContents.send('cart:log', chunk.toString())
})
proc.stdout.on('data', chunk => {
  win.webContents.send('cart:log', chunk.toString())   // raylib trace + cart printf
})
proc.on('exit', code => win.webContents.send('cart:exit', code))
```

### `preload.cjs`

```js
contextBridge.exposeInMainWorld('studio', {
  // ... existing ...
  onLog:  cb => ipcRenderer.on('cart:log',  (_, s) => cb(s)),
  onExit: cb => ipcRenderer.on('cart:exit', (_, code) => cb(code)),
})
```

### Renderer-side panel

New bottom-docked panel below the existing build log:

```
┌─────────────────────────────────────────────┐
│ runtime log              [clear]   [×]      │
├─────────────────────────────────────────────┤
│  player started at (152, 92)                │
│  beat 1                                     │
│  collision with sprite 3 at (40, 60)        │
│  ─── cart exited (code 0) ───              │
└─────────────────────────────────────────────┘
```

Behaviors:
- Auto-scrolls to bottom on new lines
- Buffer cap ~1000 lines (drop oldest)
- Clears on each ▶ run
- Exit banner shows clean-exit vs crash
- `×` collapses; reopens automatically when new output arrives

---

## `watch` — the overlay

### Runtime state

```c
typedef struct {
    char  name[24];
    char  value[40];
    int   age;       // frames since last touched
} WatchEntry;

#define WATCH_MAX 16
static WatchEntry watches[WATCH_MAX];
static int        watch_count = 0;
static bool       watch_show  = true;   // toggle with F1
```

### `watch()` implementation

```c
void watch(const char *name, const char *fmt, ...) {
    int slot = -1;
    for (int i = 0; i < watch_count; i++) {
        if (strcmp(watches[i].name, name) == 0) { slot = i; break; }
    }
    if (slot < 0) {
        if (watch_count >= WATCH_MAX) return;   // silently drop overflow
        slot = watch_count++;
        snprintf(watches[slot].name, sizeof(watches[slot].name), "%s", name);
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(watches[slot].value, sizeof(watches[slot].value), fmt, ap);
    va_end(ap);
    watches[slot].age = 0;
}
```

### Frame-end housekeeping (in main loop, after `draw()`)

```c
// age all entries; drop any that haven't been touched in 60 frames (~1 sec)
for (int i = watch_count - 1; i >= 0; i--) {
    if (++watches[i].age > 60) {
        watches[i] = watches[--watch_count];   // swap-remove
    }
}
```

Means conditional watches naturally disappear when their branch stops firing — no cleanup needed in the cart.

### Overlay rendering (window space, like touch controls)

```c
static void draw_watch_overlay(void) {
    if (!watch_show || watch_count == 0) return;
    int pad = 8, lh = 14;
    int w = 220, h = pad*2 + watch_count * lh;

    DrawRectangle(8, 8, w, h, (Color){ 0, 0, 0, 180 });
    DrawRectangleLines(8, 8, w, h, (Color){ 200, 200, 200, 120 });

    for (int i = 0; i < watch_count; i++) {
        char line[80];
        snprintf(line, sizeof(line), "%s: %s", watches[i].name, watches[i].value);
        DrawTextEx(game_font, line,
            (Vector2){ 8 + pad, 8 + pad + i * lh },
            12, 1, WHITE);
    }
}
```

Call from main loop right before `EndDrawing()`, after `draw_touch_overlay()`.

### Toggle

```c
// in main loop, near poll_virtual_touches():
if (IsKeyPressed(KEY_F1)) watch_show = !watch_show;
```

---

## Open decisions (resolve before writing code)

1. **Name: `printh`?** Alternatives: `say`, `trace`, `dbg`. `log` is perfect-but-collides with `<math.h>`. Lean **`printh`** (PICO-8 match, charming pun).
2. **Log panel: tab or docked panel?** Lean **docked panel** below build log (always visible, has its own `×` collapse like the search panel).
3. **`watch()` position: configurable?** **No, fixed top-left for v1.** Add `watch_position(x, y)` later if needed.
4. **Strip ANSI escapes from raylib's stdout?** **Yes** — tiny regex on the renderer side, ~5 lines.
5. **Default `watch_visible`: `true` or `false`?** Lean **`true`** — learner sees watches immediately. Cart can hide for screenshots.

---

## Estimated effort

| piece | files | LOC | notes |
|---|---|---|---|
| `studio.h` declarations | 1 | 4 | trivial |
| `studio.c` impl | 1 | ~80 | most of the work |
| `main.cjs` stdio pipe + IPC | 1 | ~15 | replace `stdio: 'ignore'` |
| `preload.cjs` | 1 | 2 | onLog / onExit |
| log panel DOM + CSS | shell.js + .css | ~60 | scroll, clear, ansi strip |
| `studioDocs.js` + `shell.js` help section | 2 | ~10 | 3 doc entries + section |

**~170 lines across 6 files. Roughly half a day with polish.**

---

## Follow-on: crash capture

Once the stderr pipe is wired, crash capture is ~20 more lines:

- **studio.c**: `signal(SIGSEGV, crash_handler)` → writes "cart crashed: signal N" + the last `watch()` values to stderr → exit.
- **shell.js**: when `cart:exit` arrives with `code !== 0`, the log-panel exit banner turns red: `─── cart crashed (signal 11) ───`.

`watch()` + crash capture compose beautifully — you get the variables' last values alongside the crash. That converts a mystifying silent failure into a debuggable moment.

---

## When picking this up

1. Resolve open decisions 1–5 above (or just go with the leans).
2. Order to build:
   1. `studio.h` declarations
   2. `studio.c` impls (`printh`, `watch`, `watch_visible`, overlay, F1 toggle, frame-end aging)
   3. `main.cjs` spawn change + IPC
   4. `preload.cjs` `onLog` / `onExit`
   5. log panel DOM/CSS in shell.js + shell.css
   6. docs (studioDocs + shell sections)
3. Smoke test:
   - Add `printh("hello %d", 42)` to the starter cart's `update()` — see it in the log panel.
   - Add `watch("frame", "%d", beat())` — see it tick in the corner.
   - Press F1 — overlay hides.
   - Force a crash (`int *p = 0; *p = 1;`) — banner should turn red. *(Crash banner needs the follow-on PR.)*
