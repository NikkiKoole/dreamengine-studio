import UIKit
import SwiftUI

// Hosts the C software-canvas framebuffer on screen. A CADisplayLink ticks once per
// vsync — THAT is the inverted loop: iOS drives us, we never own main(). Each tick we
// call de_update(t), wrap the RGBA buffer in a CGImage, and hand it to the layer.
// Nearest-neighbour scaling keeps the lo-fi pixels crisp; resizeAspect letterboxes.
final class CanvasView: UIView {
    private var link: CADisplayLink?
    private var start: CFTimeInterval = 0
    private let w = Int(de_width())
    private let h = Int(de_height())
    private let cs = CGColorSpaceCreateDeviceRGB()

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .black
        layer.magnificationFilter = .nearest
        layer.contentsGravity = .resizeAspect
        de_ready()
        start = CACurrentMediaTime()
        let l = CADisplayLink(target: self, selector: #selector(tick))
        l.add(to: .main, forMode: .common)
        link = l
    }
    required init?(coder: NSCoder) { fatalError("not used") }

    @objc private func tick() {
        de_update(CACurrentMediaTime() - start)
        guard let base = de_framebuffer() else { return }
        let count = w * h * 4
        let info = CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
        guard let provider = CGDataProvider(data: Data(bytes: base, count: count) as CFData),
              let img = CGImage(width: w, height: h, bitsPerComponent: 8, bitsPerPixel: 32,
                                bytesPerRow: w * 4, space: cs, bitmapInfo: info, provider: provider,
                                decode: nil, shouldInterpolate: false, intent: .defaultIntent)
        else { return }
        layer.contents = img
    }

    deinit { link?.invalidate() }
}

struct CanvasViewRep: UIViewRepresentable {
    func makeUIView(context: Context) -> CanvasView { CanvasView(frame: .zero) }
    func updateUIView(_ uiView: CanvasView, context: Context) {}
}
