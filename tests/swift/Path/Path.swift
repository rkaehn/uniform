import SwiftUI
#if canImport(UIKit)
import UIKit
#endif

@available(macOS 13.0, iOS 16.0, *)
@objc(SwiftUIShapesBridge)
public class SwiftUIShapesBridge: NSObject {
    @objc(renderWithWidth:height:prims:primCount:background:tint:primUnions:variant:appearance:patternRects:patternRectCount:edrRects:edrRectCount:toPxFromPt:)
    public static func render(width: UInt32, height: UInt32, prims: UnsafePointer<ui_sdf_prim>?, primCount: UInt32, background: UnsafePointer<Float>, tint: UnsafePointer<Float>, primUnions: UnsafePointer<UInt32>?, variant: UInt32, appearance: UInt32, patternRects: UnsafePointer<ui_test_glass_pattern_rect>?, patternRectCount: UInt32, edrRects: UnsafePointer<ui_test_glass_pattern_rect>?, edrRectCount: UInt32, toPxFromPt: CGFloat) {
        let prims = prims.map { Array(UnsafeBufferPointer(start: $0, count: Int(primCount))) } ?? []
        let background = SIMD4<Float>(background[0], background[1], background[2], background[3])
        let tint = SIMD4<Float>(tint[0], tint[1], tint[2], tint[3])
        let unions: [UInt32]? = primUnions.map { Array(UnsafeBufferPointer(start: $0, count: Int(primCount))) }
        let pattern = patternRects.map { Array(UnsafeBufferPointer(start: $0, count: Int(patternRectCount))) } ?? []
        let edr = edrRects.map { Array(UnsafeBufferPointer(start: $0, count: Int(edrRectCount))) } ?? []

        assert(Thread.isMainThread)
        MainActor.assumeIsolated {
            renderOnMain(width: width, height: height, prims: prims, background: background, tint: tint, unions: unions, variant: variant, appearance: appearance, pattern: pattern, edr: edr, toPxFromPt: toPxFromPt)
        }
    }

    @objc(showHDRImageWithWidth:height:rgba:)
    public static func showHDRImage(width: UInt32, height: UInt32, rgba: UnsafePointer<Float>) {
        #if canImport(UIKit)
        assert(Thread.isMainThread)
        guard #available(iOS 17.0, *) else { return }

        let w = Int(width)
        let h = Int(height)
        let bytesPerRow = w * 4 * MemoryLayout<Float>.size
        let data = Data(bytes: rgba, count: h * bytesPerRow)
        guard let space = CGColorSpace(name: CGColorSpace.extendedDisplayP3),
              let provider = CGDataProvider(data: data as CFData),
              let image = CGImage(width: w, height: h, bitsPerComponent: 32, bitsPerPixel: 128,
                                  bytesPerRow: bytesPerRow, space: space,
                                  bitmapInfo: [CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue),
                                               .floatComponents, .byteOrder32Little],
                                  provider: provider, decode: nil, shouldInterpolate: false,
                                  intent: .defaultIntent) else {
            print("glass snapshot: HDR image creation failed")
            return
        }

        MainActor.assumeIsolated {
            let scale = RenderWindow.shared.screenScale
            let size = CGSize(width: CGFloat(w) / scale, height: CGFloat(h) / scale)
            let view = AnyView(
                Image(uiImage: UIImage(cgImage: image, scale: scale, orientation: .up))
                    .interpolation(.none)
                    .allowedDynamicRange(.high)
                    .frame(width: size.width, height: size.height)
            )
            RenderWindow.shared.show(view, size: size, scale: scale, style: .dark)
        }
        #endif
    }

    @objc(presentStyledWithWidth:height:nodes:nodeCount:background:toPxFromPt:)
    public static func presentStyled(width: UInt32, height: UInt32, nodes: UnsafePointer<ui_test_style_node>, nodeCount: UInt32, background: UnsafePointer<Float>, toPxFromPt: CGFloat) {
        #if canImport(UIKit)
        let nodes = Array(UnsafeBufferPointer(start: nodes, count: Int(nodeCount)))
        let background = SIMD4<Float>(background[0], background[1], background[2], background[3])
        assert(Thread.isMainThread)
        guard #available(iOS 16.4, *) else { return }
        MainActor.assumeIsolated {
            let size = CGSize(width: CGFloat(width) / toPxFromPt, height: CGFloat(height) / toPxFromPt)
            let content = styledView(nodes: nodes, background: background, size: size).frame(width: size.width, height: size.height)
            RenderWindow.shared.show(AnyView(content), size: size, scale: toPxFromPt, style: .light)
        }
        #endif
    }

    private static func styleColor(_ c: SIMD4<Float>) -> Color {
        Color(.sRGB, red: Double(c.x), green: Double(c.y), blue: Double(c.z), opacity: Double(c.w))
    }

    private static func styleLayers(_ tuple: (ui_test_style_layer, ui_test_style_layer, ui_test_style_layer, ui_test_style_layer)) -> [ui_test_style_layer] {
        [tuple.0, tuple.1, tuple.2, tuple.3].filter { $0.type != 0 }
    }

    private static func layerOpacity(_ l: ui_test_style_layer) -> Double {
        l.opacity > 0.0 ? Double(l.opacity) : 1.0
    }

    private static func layerStyle(_ l: ui_test_style_layer) -> AnyShapeStyle {
        if l.type == 2 {
            let points: (UnitPoint, UnitPoint)
            switch l.direction {
            case 1: points = (.trailing, .leading)
            case 2: points = (.top, .bottom)
            case 3: points = (.bottom, .top)
            default: points = (.leading, .trailing)
            }
            let colors = [styleColor(l.color0), styleColor(l.color1)]
            if #available(macOS 14.0, iOS 17.0, *) {
                return AnyShapeStyle(.linearGradient(Gradient(colors: colors).colorSpace(.device), startPoint: points.0, endPoint: points.1))
            }
            return AnyShapeStyle(LinearGradient(colors: colors, startPoint: points.0, endPoint: points.1))
        }
        return AnyShapeStyle(styleColor(l.color0))
    }

    private static func styledStroke<S: InsettableShape>(_ shape: S, _ l: ui_test_style_layer, align: UInt32, thickness: Double) -> AnyView {
        let style = layerStyle(l)
        let view: AnyView
        switch align {
        case 1: view = AnyView(shape.inset(by: -0.5 * thickness).strokeBorder(style, lineWidth: thickness))
        case 2: view = AnyView(shape.inset(by: -thickness).strokeBorder(style, lineWidth: thickness))
        default: view = AnyView(shape.strokeBorder(style, lineWidth: thickness))
        }
        return AnyView(view.opacity(layerOpacity(l)))
    }

    private static func styledNode<S: InsettableShape>(_ shape: S, _ n: ui_test_style_node) -> AnyView {
        let size = CGSize(width: n.size.x, height: n.size.y)

        var view = AnyView(ZStack(alignment: .topLeading) {
            ForEach(Array(styleLayers(n.fill).enumerated()), id: \.offset) { _, l in
                shape.fill(layerStyle(l)).opacity(layerOpacity(l))
            }
            if n.has_child {
                let child = Rectangle()
                    .fill(styleColor(n.child_color))
                    .frame(width: n.child_size.x, height: n.child_size.y)
                    .offset(x: n.child_origin.x, y: n.child_origin.y)
                if n.clip {
                    ZStack(alignment: .topLeading) { child }
                        .frame(width: size.width, height: size.height, alignment: .topLeading)
                        .clipShape(shape)
                } else {
                    child
                }
            }
            if n.outline_thickness > 0.0 {
                ForEach(Array(styleLayers(n.outline).enumerated()), id: \.offset) { _, l in
                    styledStroke(shape, l, align: n.outline_align, thickness: n.outline_thickness)
                }
            }
        }.frame(width: size.width, height: size.height, alignment: .topLeading))

        if n.has_shadow {
            view = AnyView(view.shadow(color: styleColor(n.shadow_color), radius: n.shadow_radius, x: n.shadow_offset.x, y: n.shadow_offset.y))
        }
        if n.blur > 0.0 {
            view = AnyView(view.blur(radius: n.blur_radius_swiftui))
        }
        if n.node_opacity > 0.0 && n.node_opacity < 1.0 {
            view = AnyView(view.compositingGroup().opacity(Double(n.node_opacity)))
        }
        return AnyView(view.offset(x: n.origin.x, y: n.origin.y))
    }

    private static func styledView(nodes: [ui_test_style_node], background: SIMD4<Float>, size: CGSize) -> AnyView {
        AnyView(ZStack(alignment: .topLeading) {
            styleColor(background)
            ForEach(Array(nodes.enumerated()), id: \.offset) { _, n in
                if n.circle {
                    styledNode(Circle(), n)
                } else if n.radius > 0.0 {
                    styledNode(RoundedRectangle(cornerRadius: n.radius, style: n.continuous ? .continuous : .circular), n)
                } else {
                    styledNode(Rectangle(), n)
                }
            }
        }.frame(width: size.width, height: size.height))
    }

    @objc(dismissScene)
    public static func dismissScene() {
        #if canImport(UIKit)
        assert(Thread.isMainThread)
        if #available(iOS 16.4, *) {
            MainActor.assumeIsolated {
                RenderWindow.shared.dismiss()
            }
        }
        #endif
    }

    @MainActor
    private static func renderOnMain(width: UInt32, height: UInt32, prims: [ui_sdf_prim], background: SIMD4<Float>, tint: SIMD4<Float>, unions: [UInt32]?, variant: UInt32, appearance: UInt32, pattern: [ui_test_glass_pattern_rect], edr: [ui_test_glass_pattern_rect], toPxFromPt: CGFloat) {
        let size = CGSize(width: CGFloat(width) / toPxFromPt, height: CGFloat(height) / toPxFromPt)
        let content = view(prims: prims, background: background, tint: tint, unions: unions, variant: variant, appearance: appearance, pattern: pattern, edr: edr, size: size, scale: toPxFromPt)
            .frame(width: size.width, height: size.height)

        #if canImport(UIKit)
        if #available(iOS 16.4, *) {
            let style: UIUserInterfaceStyle = appearance == 1 ? .light : .dark
            RenderWindow.shared.show(AnyView(content), size: size, scale: toPxFromPt, style: style)
        }
        #endif
    }

    #if canImport(UIKit)
    fileprivate static func edrImage(rects: [ui_test_glass_pattern_rect], size: CGSize, scale: CGFloat) -> UIImage? {
        let w = Int(size.width * scale)
        let h = Int(size.height * scale)
        let bytesPerRow = w * 4 * MemoryLayout<Float>.size
        guard let space = CGColorSpace(name: CGColorSpace.extendedLinearSRGB),
              let ctx = CGContext(data: nil, width: w, height: h, bitsPerComponent: 32, bytesPerRow: bytesPerRow, space: space,
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue | CGBitmapInfo.floatComponents.rawValue | CGBitmapInfo.byteOrder32Little.rawValue) else {
            return nil
        }
        ctx.clear(CGRect(x: 0, y: 0, width: w, height: h))
        ctx.translateBy(x: 0.0, y: CGFloat(h))
        ctx.scaleBy(x: scale, y: -scale)
        for r in rects {
            let components: [CGFloat] = [CGFloat(r.color.x), CGFloat(r.color.y), CGFloat(r.color.z), CGFloat(r.color.w)]
            guard let color = CGColor(colorSpace: space, components: components) else {
                continue
            }
            ctx.setFillColor(color)
            ctx.fill(CGRect(x: r.origin.x, y: r.origin.y, width: r.size.x, height: r.size.y))
        }
        guard var image = ctx.makeImage() else {
            return nil
        }
        let headroom = rects.map { max($0.color.x, $0.color.y, $0.color.z) }.max() ?? 1.0
        if #available(iOS 18.0, *), headroom > 1.0, let provider = image.dataProvider,
           let tagged = CGImage(headroom: headroom, width: image.width, height: image.height, bitsPerComponent: image.bitsPerComponent,
                                bitsPerPixel: image.bitsPerPixel, bytesPerRow: image.bytesPerRow, space: space, bitmapInfo: image.bitmapInfo,
                                provider: provider, decode: nil, shouldInterpolate: false, intent: .defaultIntent) {
            image = tagged
        }
        return UIImage(cgImage: image, scale: scale, orientation: .up)
    }
    #endif

    @available(macOS 26.0, iOS 26.0, *)
    fileprivate static func material(_ t: SIMD4<Float>, variant: UInt32) -> Glass {
        var m = variant == 1 ? Glass.regular : Glass.clear
        if t.w > 0.0 {
            m = m.tint(Color(.sRGB, red: Double(t.x), green: Double(t.y), blue: Double(t.z), opacity: Double(t.w)))
        }
        return m
    }

    private static func view(prims: [ui_sdf_prim], background: SIMD4<Float>, tint: SIMD4<Float>, unions: [UInt32]?, variant: UInt32, appearance: UInt32, pattern: [ui_test_glass_pattern_rect], edr: [ui_test_glass_pattern_rect], size: CGSize, scale: CGFloat) -> AnyView {
        let backgroundColor = Color(.sRGB, red: Double(background.x), green: Double(background.y), blue: Double(background.z), opacity: Double(background.w))
        let scheme: ColorScheme = appearance == 1 ? .light : .dark

        let patternCanvas = pattern.isEmpty ? nil : Canvas(rendersAsynchronously: false) { context, _ in
            for r in pattern {
                let rect = CGRect(x: r.origin.x, y: r.origin.y, width: r.size.x, height: r.size.y)
                let color = Color(.sRGB, red: Double(r.color.x), green: Double(r.color.y), blue: Double(r.color.z), opacity: Double(r.color.w))
                context.fill(Path(rect), with: .color(color))
            }
        }

        var edrUnderlay: AnyView? = nil
        #if canImport(UIKit)
        if #available(iOS 17.0, *), edr.isEmpty == false, let image = edrImage(rects: edr, size: size, scale: scale) {
            edrUnderlay = AnyView(Image(uiImage: image)
                .interpolation(.none)
                .allowedDynamicRange(.high)
                .frame(width: size.width, height: size.height))
        }
        #endif

        if #available(macOS 26.0, iOS 26.0, *) {
            if prims.isEmpty {
                return AnyView(ZStack {
                    backgroundColor
                    if let patternCanvas { patternCanvas }
                    if let edrUnderlay { edrUnderlay }
                }.environment(\.colorScheme, scheme))
            }

            let glass: AnyView
            if let unions {
                glass = AnyView(UnionGlassView(prims: prims, groups: unions, tint: tint, variant: variant))
            } else {
                glass = AnyView(Color.clear.glassEffect(material(tint, variant: variant), in: FixedShape(path: unionPath(prims))))
            }
            if background.w > 0.0 || patternCanvas != nil || edrUnderlay != nil {
                return AnyView(ZStack {
                    backgroundColor
                    if let patternCanvas { patternCanvas }
                    if let edrUnderlay { edrUnderlay }
                    glass
                }.environment(\.colorScheme, scheme))
            }
            return AnyView(glass.environment(\.colorScheme, scheme))
        }
        let shape = unionPath(prims)

        return AnyView(ZStack {
            backgroundColor
            if let patternCanvas { patternCanvas }
            Canvas(rendersAsynchronously: false) { context, _ in
                context.fill(shape, with: .color(.white))
            }
        })
    }

    @objc(presentCompareWithWidth:height:items:itemCount:background:toPxFromPt:)
    public static func presentCompare(width: UInt32, height: UInt32, items: UnsafePointer<ui_test_sui_item>?, itemCount: UInt32, background: UnsafePointer<Float>, toPxFromPt: CGFloat) {
        #if canImport(UIKit)
        let items = items.map { Array(UnsafeBufferPointer(start: $0, count: Int(itemCount))) } ?? []
        let bg = SIMD4<Float>(background[0], background[1], background[2], background[3])
        assert(Thread.isMainThread)
        guard #available(iOS 16.4, *) else { return }
        MainActor.assumeIsolated {
            let size = CGSize(width: CGFloat(width) / toPxFromPt, height: CGFloat(height) / toPxFromPt)
            let content = compareView(items: items, background: bg).frame(width: size.width, height: size.height)
            RenderWindow.shared.show(AnyView(content), size: size, scale: toPxFromPt, style: .light)
        }
        #endif
    }

    private static func compareColor(_ v: SIMD4<Float>) -> Color {
        Color(.sRGB, red: Double(v.x), green: Double(v.y), blue: Double(v.z), opacity: Double(v.w))
    }

    @available(macOS 13.3, iOS 16.4, *)
    private static func comparePath(_ item: ui_test_sui_item, inset: Double = 0.0) -> Path {
        let rect = CGRect(x: item.origin.x + inset, y: item.origin.y + inset,
                          width: item.size.x - 2.0 * inset, height: item.size.y - 2.0 * inset)
        switch item.kind {
        case ui_test_sui_kind_ellipse:
            return Ellipse().path(in: rect)
        case ui_test_sui_kind_path:
            var path = Path()
            for component in UnsafeBufferPointer(start: ui_path_get_components(item.path), count: Int(ui_path_get_count(item.path))) {
                switch component.type {
                case .move:
                    path.move(to: CGPoint(x: Double(component.move.p.x), y: Double(component.move.p.y)))
                case .line:
                    path.addLine(to: CGPoint(x: Double(component.line.p.x), y: Double(component.line.p.y)))
                case .cubic:
                    path.addCurve(to: CGPoint(x: Double(component.cubic.p.x), y: Double(component.cubic.p.y)),
                                  control1: CGPoint(x: Double(component.cubic.c1.x), y: Double(component.cubic.c1.y)),
                                  control2: CGPoint(x: Double(component.cubic.c2.x), y: Double(component.cubic.c2.y)))
                default:
                    path.closeSubpath()
                }
            }
            return path
        default:
            let r = item.radius
            let shape = UnevenRoundedRectangle(topLeadingRadius: max(r.x - inset, 0.0),
                                               bottomLeadingRadius: max(r.z - inset, 0.0),
                                               bottomTrailingRadius: max(r.w - inset, 0.0),
                                               topTrailingRadius: max(r.y - inset, 0.0),
                                               style: item.circular ? .circular : .continuous)
            return shape.path(in: rect)
        }
    }

    @available(macOS 13.3, iOS 16.4, *)
    private static func compareTransformed(_ base: AnyView, item: ui_test_sui_item, anchor: CGPoint) -> AnyView {
        if item.transform_x == .zero && item.transform_y == .zero && item.transform_t == .zero {
            return base
        }
        let m = CGAffineTransform(a: item.transform_x.x, b: item.transform_x.y,
                                  c: item.transform_y.x, d: item.transform_y.y,
                                  tx: item.transform_t.x, ty: item.transform_t.y)
        let anchored = CGAffineTransform(translationX: -anchor.x, y: -anchor.y)
            .concatenating(m)
            .concatenating(CGAffineTransform(translationX: anchor.x, y: anchor.y))
        return AnyView(base.transformEffect(anchored))
    }

    @available(macOS 13.3, iOS 16.4, *)
    private static func compareTransformed(_ base: AnyView, item: ui_test_sui_item) -> AnyView {
        compareTransformed(base, item: item, anchor: CGPoint(x: item.origin.x, y: item.origin.y))
    }

    @available(macOS 13.3, iOS 16.4, *)
    private static func compareDecorated(_ base: AnyView, item: ui_test_sui_item, opacity: Float) -> AnyView {
        var view = base
        if item.shadow.w > 0.0 {
            view = AnyView(view.shadow(color: compareColor(item.shadow), radius: CGFloat(item.shadow_radius),
                                       x: item.shadow_offset.x, y: item.shadow_offset.y))
        }
        if item.blur > 0.0 {
            view = AnyView(view.blur(radius: CGFloat(item.blur)))
        }
        if opacity > 0.0 {
            view = AnyView(view.compositingGroup().opacity(Double(opacity)))
        }
        return view
    }

    @available(macOS 13.3, iOS 16.4, *)
    private static func compareItemView(_ item: ui_test_sui_item) -> AnyView {
        var layers: [AnyView] = []
        if item.kind == ui_test_sui_kind_path && item.path_thickness > 0.0 {
            layers.append(AnyView(FixedShape(path: comparePath(item))
                .stroke(compareColor(item.fill), style: StrokeStyle(lineWidth: CGFloat(item.path_thickness), lineCap: .round, lineJoin: .round))))
        } else if item.fill.w > 0.0 {
            layers.append(AnyView(FixedShape(path: comparePath(item))
                .fill(compareColor(item.fill), style: FillStyle(eoFill: true))))
        }
        if item.stroke_width > 0.0 {
            let width = Double(item.stroke_width)
            let inset = item.stroke_align == .inside ? width * 0.5
                : item.stroke_align == .outside ? -width * 0.5 : 0.0
            layers.append(AnyView(FixedShape(path: comparePath(item, inset: inset))
                .stroke(compareColor(item.stroke), style: StrokeStyle(lineWidth: width, lineCap: .round, lineJoin: .round))))
        }
        let view = layers.count == 1 ? layers[0] : AnyView(ZStack { ForEach(0..<layers.count, id: \.self) { layers[$0] } })
        return compareDecorated(compareTransformed(view, item: item), item: item, opacity: item.opacity)
    }

    @available(macOS 13.3, iOS 16.4, *)
    private static func compareView(items: [ui_test_sui_item], background: SIMD4<Float>) -> AnyView {
        var pieces: [AnyView] = []
        var i = 0
        while i < items.count {
            let item = items[i]
            if item.group == 0 {
                pieces.append(compareItemView(item))
                i += 1
                continue
            }
            var members: [ui_test_sui_item] = []
            while i < items.count && items[i].group == item.group {
                members.append(items[i])
                i += 1
            }
            members[0].shadow.w = 0.0
            members[0].blur = 0.0
            members[0].opacity = 0.0
            members[0].transform_x = .zero
            members[0].transform_y = .zero
            members[0].transform_t = .zero
            var view = AnyView(ZStack { ForEach(0..<members.count, id: \.self) { compareItemView(members[$0]) } })
            if item.group_clip {
                view = AnyView(view.clipShape(FixedShape(path: comparePath(item))))
                view = compareTransformed(view, item: item)
            } else {
                view = compareTransformed(view, item: item, anchor: .zero)
            }
            view = compareDecorated(view, item: item, opacity: item.group_opacity)
            pieces.append(view)
        }
        return AnyView(ZStack {
            compareColor(background)
            ForEach(0..<pieces.count, id: \.self) { pieces[$0] }
        }.environment(\.colorScheme, .light))
    }

    fileprivate static func unionPath(_ prims: [ui_sdf_prim]) -> Path {
        var result = Path()
        for prim in prims {
            let p = path(for: prim)
            if #available(macOS 14.0, iOS 17.0, *) {
                result = result.union(p)
            } else {
                result.addPath(p)
            }
        }
        return result
    }

    fileprivate static func path(for prim: ui_sdf_prim) -> Path {
        switch prim.type {
        case ui_sdf_prim_type_circle:
            let circle = prim.circle
            let rect = CGRect(x: circle.center.x - circle.radius, y: circle.center.y - circle.radius,
                              width: 2.0 * circle.radius, height: 2.0 * circle.radius)
            return Circle().path(in: rect)
        default:
            let rect = prim.rect
            return RoundedRectangle(cornerRadius: rect.radius, style: rect.continuous ? .continuous : .circular)
                .path(in: CGRect(x: rect.origin.x, y: rect.origin.y, width: rect.width, height: rect.height))
        }
    }
}

@available(macOS 13.0, iOS 16.0, *)
private struct FixedShape: Shape {
    let path: Path

    func path(in rect: CGRect) -> Path {
        path
    }
}

@available(macOS 26.0, iOS 26.0, *)
private struct UnionGlassView: View {
    let prims: [ui_sdf_prim]
    let groups: [UInt32]
    let tint: SIMD4<Float>
    let variant: UInt32
    @Namespace private var unionNamespace

    private var groupIds: [UInt32] {
        var seen: [UInt32] = []
        for g in groups where seen.contains(g) == false {
            seen.append(g)
        }
        return seen
    }

    var body: some View {
        GlassEffectContainer {
            ZStack {
                ForEach(groupIds, id: \.self) { g in
                    let members = prims.indices.filter { groups[$0] == g }
                    Color.clear
                        .glassEffect(SwiftUIShapesBridge.material(tint, variant: variant),
                                     in: FixedShape(path: SwiftUIShapesBridge.unionPath(members.map { prims[$0] })))
                        .glassEffectUnion(id: g, namespace: unionNamespace)
                }
            }
        }
    }
}

#if canImport(UIKit)
@available(iOS 16.4, *)
@MainActor
private final class RenderWindow {
    static let shared = RenderWindow()

    private let window: UIWindow
    private let host: UIHostingController<AnyView>

    private init() {
        let scene = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first
        window = scene.map { UIWindow(windowScene: $0) } ?? UIWindow(frame: .zero)

        host = UIHostingController(rootView: AnyView(EmptyView()))
        host.view.backgroundColor = .clear
        host.safeAreaRegions = []

        window.rootViewController = host
        window.windowLevel = UIWindow.Level(rawValue: UIWindow.Level.normal.rawValue + 1.0)
        window.isUserInteractionEnabled = false
        window.isHidden = true
    }

    static let beaconHoldS: TimeInterval = 0.9
    static let beaconCellPt: CGFloat = 2.0
    static let launchSettleS: TimeInterval = 2.0

    var screenScale: CGFloat {
        window.traitCollection.displayScale
    }

    private var didVerifyDisplay = false
    private var didLaunchSettle = false
    private var edrHolder: UIWindow?
    private var userBrightness: CGFloat?

    private var frameInterval: TimeInterval {
        1.0 / TimeInterval(max(window.windowScene?.screen.maximumFramesPerSecond ?? 60, 60))
    }

    private func spinRunLoop(until deadline: TimeInterval) {
        while true {
            let remaining = deadline - CACurrentMediaTime()
            if remaining <= 0 {
                break
            }
            let slice = min(remaining, frameInterval)
            if RunLoop.current.run(mode: .default, before: Date(timeIntervalSinceNow: slice)) == false {
                Thread.sleep(forTimeInterval: slice)
            }
        }
    }

    private func holdEDRHeadroom(on scene: UIWindowScene) {
        guard #available(iOS 26.0, *) else {
            return
        }
        let side: CGFloat = 16.0
        let patch = ui_test_glass_pattern_rect(origin: SIMD2<Double>(0.0, 0.0), size: SIMD2<Double>(Double(side), Double(side)), color: SIMD4<Float>(4.0, 4.0, 4.0, 1.0))
        guard let image = SwiftUIShapesBridge.edrImage(rects: [patch], size: CGSize(width: side, height: side), scale: screenScale)?.cgImage else {
            return
        }
        let controller = UIViewController()
        controller.view.backgroundColor = .clear
        controller.view.layer.contents = image
        controller.view.layer.preferredDynamicRange = .high

        let bounds = scene.coordinateSpace.bounds
        let holder = UIWindow(windowScene: scene)
        holder.rootViewController = controller
        holder.frame = CGRect(x: bounds.maxX - side, y: bounds.maxY - side, width: side, height: side)
        holder.windowLevel = window.windowLevel
        holder.isUserInteractionEnabled = false
        holder.isHidden = false
        holder.layoutIfNeeded()
        CATransaction.flush()
        edrHolder = holder
    }

    private func prepareDisplay() {
        guard didVerifyDisplay == false, let scene = window.windowScene else {
            return
        }
        didVerifyDisplay = true
        let screen = scene.screen

        var required: CGFloat = 2.0
        var wait: TimeInterval = 5.0
        if screen.currentEDRHeadroom < required && screen.potentialEDRHeadroom >= 3.0 {
            print("glass snapshot: edr current \(screen.currentEDRHeadroom) at brightness \(screen.brightness) without Reference Mode, dimming the display and holding EDR content in its bottom-right corner")
            userBrightness = screen.brightness
            screen.brightness = 0.0
            holdEDRHeadroom(on: scene)
            required = 3.0
            wait = 10.0
        }
        let deadline = CACurrentMediaTime() + wait
        while screen.currentEDRHeadroom < required && CACurrentMediaTime() < deadline {
            spinRunLoop(until: CACurrentMediaTime() + frameInterval)
        }
        print("glass snapshot: edr current \(screen.currentEDRHeadroom) potential \(screen.potentialEDRHeadroom) brightness \(screen.brightness)")
        precondition(screen.currentEDRHeadroom >= required,
                     "glass snapshot: EDR headroom \(screen.currentEDRHeadroom) < \(required) (potential \(screen.potentialEDRHeadroom)) — Reference Mode off or display SDR-clipped; captures of EDR content clip at white in this state")
    }

    private func present(_ view: AnyView, size: CGSize, style: UIUserInterfaceStyle) {
        UIView.performWithoutAnimation {
            window.overrideUserInterfaceStyle = style
            window.frame = CGRect(origin: .zero, size: size)
            host.rootView = view
            host.view.frame = CGRect(origin: .zero, size: size)
            window.isHidden = false
            window.layoutIfNeeded()
        }
        CATransaction.flush()
    }

    func show(_ view: AnyView, size: CGSize, scale: CGFloat, style: UIUserInterfaceStyle) {
        let firstPresentation = didLaunchSettle == false
        if firstPresentation {
            didLaunchSettle = true
            spinRunLoop(until: CACurrentMediaTime() + Self.launchSettleS)
        }
        prepareDisplay()
        precondition(screenScale == scale,
                     "glass snapshot: scene px/pt \(scale) != device display scale \(screenScale) — references must be captured at exactly the scale the scenes are built and GPU-rendered at")

        let cell = Self.beaconCellPt
        let beaconColors: [Color] = [
            Color(.sRGB, red: 1, green: 0, blue: 0, opacity: 1),
            Color(.sRGB, red: 0, green: 1, blue: 0, opacity: 1),
            Color(.sRGB, red: 0, green: 0, blue: 1, opacity: 1),
            Color(.sRGB, red: 0, green: 1, blue: 1, opacity: 1),
            Color(.sRGB, red: 1, green: 0, blue: 1, opacity: 1),
            Color(.sRGB, red: 1, green: 1, blue: 0, opacity: 1),
            Color(.sRGB, red: 0, green: 0, blue: 0, opacity: 1),
            Color(.sRGB, red: 1, green: 1, blue: 1, opacity: 1),
        ]
        let beaconView = AnyView(ZStack(alignment: .topLeading) {
            Color(.sRGB, red: 0, green: 0, blue: 0, opacity: 1)
            Canvas(rendersAsynchronously: false) { context, _ in
                for (i, color) in beaconColors.enumerated() {
                    context.fill(Path(CGRect(x: Double(i) * cell, y: 0.0, width: cell, height: cell)), with: .color(color))
                }
            }
        }.frame(width: size.width, height: size.height))
        let beaconShownAt = CACurrentMediaTime()
        present(beaconView, size: size, style: style)
        spinRunLoop(until: beaconShownAt + (firstPresentation ? 3.0 : Self.beaconHoldS))

        present(view, size: size, style: style)
        print("glass snapshot: present \(Int((size.width * scale).rounded()))x\(Int((size.height * scale).rounded()))px, edr \(window.windowScene?.screen.currentEDRHeadroom ?? 0.0)")
    }

    func dismiss() {
        window.isHidden = true
        edrHolder?.isHidden = true
        edrHolder = nil
        if let brightness = userBrightness {
            window.windowScene?.screen.brightness = brightness
            userBrightness = nil
        }
        CATransaction.flush()
    }
}
#endif
