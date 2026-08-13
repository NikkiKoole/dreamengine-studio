// The UMBRELLA HEADER of the TinyjamAUKernel framework — how the framework's Swift sees the C engine.
//
// It replaces AU-Bridging-Header.h, which the appex used. A bridging header is not a style choice
// here: `error: using bridging headers with framework targets is unsupported`. The documented
// substitute for a mixed-language FRAMEWORK is exactly this — include the C headers in the
// framework's public umbrella header (with DEFINES_MODULE=YES) and the target's own Swift files see
// those declarations with no import statement, the same way they saw the bridging header.
//
// WHY THERE IS A FRAMEWORK AT ALL, since it is the whole point of the target: an AUv3 can only be
// loaded IN-PROCESS if the code lives in a bundle the system can load, named by the extension's
// `AudioComponentBundle` key. An `.appex` is an executable, not a loadable bundle, so pointing that
// key at the appex (what we did until 2026-08-13) means a host asking for in-process loading is
// silently given an out-of-process one instead — measured: `.loadInProcess` still answered from
// another pid. In-process is the case where `createAudioUnit` runs once and the view controller the
// host is handed holds THAT audio unit, which is what the orphaned-panel defect needs.
// See docs/design/ios-plan.md.
//
// Deliberately thin: only engine.h, the same one line the bridging header had. engine.h is a
// standalone C contract that does NOT drag in studio.h — which matters more here than it did for a
// bridging header, because a module must be self-contained and studio.h declares echo() and
// filter(), the names that collide with curses.h in the macOS SDK's Darwin module.
#import "engine.h"
