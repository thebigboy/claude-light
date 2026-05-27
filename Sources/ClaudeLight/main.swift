import AppKit
import Foundation

enum ClaudeState: String {
    case idle
    case thinking
    case working
    case error

    var color: NSColor {
        switch self {
        case .idle:
            return NSColor(calibratedRed: 1.0, green: 0.08, blue: 0.06, alpha: 1.0)
        case .thinking:
            return NSColor(calibratedRed: 1.0, green: 0.74, blue: 0.05, alpha: 1.0)
        case .working:
            return NSColor(calibratedRed: 0.18, green: 0.86, blue: 0.28, alpha: 1.0)
        case .error:
            return NSColor(calibratedRed: 1.0, green: 0.24, blue: 0.02, alpha: 1.0)
        }
    }

    var activeAlpha: CGFloat {
        switch self {
        case .idle, .error:
            return 1.0
        case .thinking, .working:
            return 0.86
        }
    }
}

struct StatePayload: Decodable {
    let state: String
}

@MainActor
final class LightView: NSView {
    private var animationStart = Date()
    private var animationTimer: Timer?

    var state: ClaudeState = .idle {
        didSet {
            animationStart = Date()
            updateAnimationTimer()
            needsDisplay = true
        }
    }

    override var isFlipped: Bool { true }

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        wantsLayer = true
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        wantsLayer = true
    }

    override func draw(_ dirtyRect: NSRect) {
        NSColor.clear.setFill()
        dirtyRect.fill()

        drawHousing()
        drawLight(.idle, at: 0)
        drawLight(.thinking, at: 1)
        drawLight(.working, at: 2)
    }

    private func drawHousing() {
        let housingRect = bounds.insetBy(dx: 4, dy: 4)
        let path = NSBezierPath(roundedRect: housingRect, xRadius: 14, yRadius: 14)
        NSColor.black.withAlphaComponent(0.72).setFill()
        path.fill()

        NSColor.white.withAlphaComponent(0.12).setStroke()
        path.lineWidth = 1
        path.stroke()
    }

    private func drawLight(_ lightState: ClaudeState, at index: Int) {
        let diameter: CGFloat = 28
        let gap: CGFloat = 8
        let top = (bounds.height - diameter * 3 - gap * 2) / 2
        let rect = NSRect(
            x: (bounds.width - diameter) / 2,
            y: top + CGFloat(index) * (diameter + gap),
            width: diameter,
            height: diameter
        )

        let isActive = state == lightState
        let pulse = pulseScale(for: lightState, isActive: isActive)
        let glowAlpha = isActive ? pulse : 0.18
        let fillAlpha = isActive ? lightState.activeAlpha : 0.16
        let color = lightState.color

        let shadow = NSShadow()
        shadow.shadowBlurRadius = isActive ? 10 + 8 * pulse : 3
        shadow.shadowOffset = .zero
        shadow.shadowColor = color.withAlphaComponent(glowAlpha)
        shadow.set()

        color.withAlphaComponent(fillAlpha).setFill()
        NSBezierPath(ovalIn: rect).fill()

        NSColor.white.withAlphaComponent(0.5).setFill()
        let highlight = NSRect(
            x: rect.minX + rect.width * 0.25,
            y: rect.minY + rect.height * 0.2,
            width: rect.width * 0.22,
            height: rect.height * 0.22
        )
        NSBezierPath(ovalIn: highlight).fill()

        NSColor.black.withAlphaComponent(0.35).setStroke()
        let rim = NSBezierPath(ovalIn: rect)
        rim.lineWidth = 1
        rim.stroke()
    }

    private func pulseScale(for lightState: ClaudeState, isActive: Bool) -> CGFloat {
        guard isActive, lightState == .thinking || lightState == .working else {
            return isActive ? 0.85 : 0.18
        }

        let elapsed = Date().timeIntervalSince(animationStart)
        let wave = (sin(elapsed * 2.6) + 1) / 2
        return 0.45 + CGFloat(wave) * 0.45
    }

    private func updateAnimationTimer() {
        animationTimer?.invalidate()

        guard state == .thinking || state == .working else {
            animationTimer = nil
            return
        }

        animationTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 30.0, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.needsDisplay = true
            }
        }
    }
}

@MainActor
final class FloatingLightController: NSObject {
    private let stateURL: URL
    private let lightView = LightView(frame: NSRect(x: 0, y: 0, width: 48, height: 124))
    private var window: NSWindow!
    private var source: DispatchSourceFileSystemObject?
    private var directorySource: DispatchSourceFileSystemObject?

    init(stateURL: URL) {
        self.stateURL = stateURL
        super.init()
        createWindow()
        ensureStateFile()
        loadState()
        watchStateFile()
        watchStateDirectory()
    }

    private func createWindow() {
        let frame = NSRect(x: 80, y: 80, width: 48, height: 124)
        window = NSWindow(
            contentRect: frame,
            styleMask: [.borderless],
            backing: .buffered,
            defer: false
        )
        window.backgroundColor = .clear
        window.isOpaque = false
        window.hasShadow = false
        window.level = .floating
        window.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .stationary]
        window.contentView = lightView
        window.isMovableByWindowBackground = true
        window.ignoresMouseEvents = false
        window.makeKeyAndOrderFront(nil)
    }

    private func ensureStateFile() {
        let directory = stateURL.deletingLastPathComponent()
        try? FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        if !FileManager.default.fileExists(atPath: stateURL.path) {
            try? #"{"state":"idle"}"#.write(to: stateURL, atomically: true, encoding: .utf8)
        }
    }

    private func loadState() {
        guard let data = try? Data(contentsOf: stateURL),
              let payload = try? JSONDecoder().decode(StatePayload.self, from: data),
              let state = ClaudeState(rawValue: payload.state) else {
            lightView.state = .error
            return
        }
        lightView.state = state
    }

    private func watchStateFile() {
        source?.cancel()

        let descriptor = open(stateURL.path, O_EVTONLY)
        guard descriptor >= 0 else {
            return
        }

        let nextSource = DispatchSource.makeFileSystemObjectSource(
            fileDescriptor: descriptor,
            eventMask: [.write, .delete, .rename],
            queue: .main
        )
        nextSource.setEventHandler { [weak self] in
            guard let self else { return }
            self.loadState()
            if self.source?.data.contains(.delete) == true || self.source?.data.contains(.rename) == true {
                self.watchStateFile()
            }
        }
        nextSource.setCancelHandler {
            close(descriptor)
        }
        nextSource.resume()
        source = nextSource
    }

    private func watchStateDirectory() {
        let directory = stateURL.deletingLastPathComponent()
        let descriptor = open(directory.path, O_EVTONLY)
        guard descriptor >= 0 else {
            return
        }

        let nextSource = DispatchSource.makeFileSystemObjectSource(
            fileDescriptor: descriptor,
            eventMask: [.write],
            queue: .main
        )
        nextSource.setEventHandler { [weak self] in
            self?.loadState()
            self?.watchStateFile()
        }
        nextSource.setCancelHandler {
            close(descriptor)
        }
        nextSource.resume()
        directorySource = nextSource
    }
}

let app = NSApplication.shared
app.setActivationPolicy(.accessory)

let home = FileManager.default.homeDirectoryForCurrentUser
let stateURL = home
    .appendingPathComponent(".claude-light", isDirectory: true)
    .appendingPathComponent("state.json")

let controller = FloatingLightController(stateURL: stateURL)
_ = controller
app.run()
