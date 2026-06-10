// Lean fix for the WASM_WORKERS build: libc-ww's clock_nanosleep references
// emscripten_thread_sleep, which bare WASM_WORKERS doesn't provide. raylib on web is
// rAF-driven and never sleeps, so a no-op is safe (verified: CPU stays low). See
// docs/design/audio-threading.md Stage 0.
addToLibrary({ emscripten_thread_sleep: function (ms) {} });
