import SwiftUI

// Phase 2 — the real dreamengine (a cart: omnichord) hosted on iOS via CanvasView.
// CanvasView owns the lifecycle: de_init() then AudioEngine.start() (so sound_init runs
// before the audio thread pulls de_audio_render). App just loads StoreKit entitlements.
@main
struct TinyjamHelloApp: App {
    var body: some Scene {
        WindowGroup { ContentView() }
    }
}

struct ContentView: View {
    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            CanvasViewRep().ignoresSafeArea()
        }
        .onAppear {
            Store_Init()                 // spike 4 — load products + entitlements
        }
    }
}
