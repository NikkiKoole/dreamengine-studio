# Debug tools — `printh()` + `watch()`

**Status:** ✅ implemented 2026-05-29 (went with the recommended leans on all 5 open decisions). Followed the design below as written; one addition — `SetTraceLogLevel(LOG_WARNING)` in `main()` keeps raylib's INFO chatter out of the new log panel. Crash-capture (SIGSEGV/FPE/ABRT/BUS) is also DONE: the handler dumps the last `watch()` values to stderr via async-signal-safe `write()`, then re-raises so the editor still sees the real signal — verified live (signal 11 → "last watched values: x = 60" → red banner).

> **Notes for the next session:** `-fno-delete-null-pointer-checks` is now in both compile paths in `main.cjs`, so a plain beginner null *write* (`*p = 42;`) reliably faults as SIGSEGV (caught) instead of being optimized into a SIGTRAP or elided. Still platform-specific: arm64 integer divide-by-zero does NOT trap (returns 0), so SIGFPE won't fire on Apple Silicon — a `volatile` null read/write is the dependable test crash.
>
> The runtime-log panel auto-hides after 3s on a clean exit (code 0) with no `printh` output — mirroring the build log. It stays open if the cart printed anything or crashed.
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

## Open decisions — ✅ all resolved (shipped with the leans)

1. **Name: `printh`?** → **`printh`** shipped (PICO-8 match, dodges `<math.h>`'s `log`).
2. **Log panel: tab or docked panel?** → **docked panel**, but bottom-docked (not below the build log) with its own `clear` + `×`. Auto-hides 3s after a clean, silent exit, mirroring the build log; reopens on new output.
3. **`watch()` position: configurable?** → **fixed top-left** for v1. `watch_position(x, y)` still deferred.
4. **Strip ANSI escapes?** → **yes**, regex on the renderer side (`stripAnsi` in shell.js).
5. **Default `watch_visible`?** → **`true`**; F1 toggles; cart can call `watch_visible(false)` for clean screenshots.

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

## Crash capture — ✅ done

Shipped alongside the main feature (the design above anticipated it as a follow-on; it landed in the same pass):

- **studio.c**: `signal()` registers a handler for **SIGSEGV / SIGFPE / SIGABRT / SIGBUS**. The handler writes `*** cart crashed: signal N ***` + the last `watch()` values to stderr using async-signal-safe `write()` (not `fprintf`, which isn't safe in a handler), then restores `SIG_DFL` and **re-raises** so the process dies for real and the editor sees the actual signal.
- **main.cjs**: forwards `{ code, signal }` on exit.
- **shell.js**: a non-zero/signal exit turns the banner red — `─── cart crashed (SIGSEGV) ───`.

`watch()` + crash capture compose exactly as hoped — verified live: a null deref produced `signal 11` plus `last watched values: x = 60`. A mystifying silent failure becomes a debuggable moment.

**Platform note:** the build now passes `-fno-delete-null-pointer-checks`, so a plain beginner null *write* (`*p = 42;`) reliably faults instead of being optimized into a SIGTRAP or elided. arm64 integer divide-by-zero still does NOT trap (returns 0) — SIGFPE won't fire on Apple Silicon, so a `volatile` null read/write is the dependable test crash.

---

## Smoke test (as shipped — for regression checks)

Run inside the Electron app (`npm start`); the panel/overlay only work there. Note: changing `main.cjs` needs an Electron restart.

- `printh("hello %d", 42)` in `update()` → line in the bottom runtime log panel.
- `watch("frame", "%d", beat())` → ticks in the top-left of the game window.
- Press **F1** → overlay hides/shows.
- Force a crash with a `volatile` null read — `volatile int *p = 0; int v = *p; watch("v","%d",v);` → game window closes, panel shows `*** cart crashed: signal 11 ***` + last watched values, and a red `─── cart crashed (SIGSEGV) ───` banner. (A plain `*p = 42;` write also crashes now, thanks to `-fno-delete-null-pointer-checks`.)
- A cart that prints nothing and exits cleanly → panel flashes the green exit banner, then auto-hides after 3s.

## Still open / future

- `watch_position(x, y)` — overlay is fixed top-left for now.
- Whether to add `-fno-delete-null-pointer-checks` was resolved (yes); no other build-flag changes pending.
