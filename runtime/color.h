#ifndef DE_COLOR_H
#define DE_COLOR_H

// ============================================================================
// color.h — the engine's universal 32-bit RGBA color type (platform seam, phase A).
//
//   - Raylib backends (desktop/web): DeColor is Raylib's Color.
//   - DE_NO_RAYLIB backends (iOS/Switch): DeColor is the shim's Color
//     (raylib_compat.h) — same {unsigned char r,g,b,a} layout.
//
// Either way DeColor is a zero-cost alias of whatever "Color" the active backend
// defines, so the engine core speaks one color type and the backend's draw calls
// take it directly. Set -DDE_NO_RAYLIB on a backend with no Raylib.
// ============================================================================

#ifdef DE_NO_RAYLIB
#include "raylib_compat.h"
#else
#include "raylib.h"
#endif

typedef Color DeColor;

#endif // DE_COLOR_H
