import SwiftUI

// spike 0 — toolchain proof. This is the throwaway hello-world that verified the
// agentic build loop (xcodegen → xcodebuild → simulator). The real app replaces
// this with a UIViewController hosting the software-canvas framebuffer (spike 1);
// see ../docs/design/ios-plan.md.
@main
struct TinyjamHelloApp: App {
    var body: some Scene {
        WindowGroup { ContentView() }
    }
}

struct ContentView: View {
    var body: some View {
        ZStack {
            Color(red: 1.0, green: 0.43, blue: 0.71).ignoresSafeArea()  // shell.css run-button pink
            VStack(spacing: 12) {
                Text("Tinyjam")
                    .font(.system(size: 56, weight: .heavy, design: .rounded))
                    .foregroundColor(.white)
                Text("hello, iOS 🎵")
                    .font(.title2)
                    .foregroundColor(.white.opacity(0.9))
                Text("spike 0 — toolchain proof")
                    .font(.footnote)
                    .foregroundColor(.white.opacity(0.7))
            }
        }
    }
}
