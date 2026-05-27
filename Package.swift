// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "claude-light",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(name: "claude-light", targets: ["ClaudeLight"])
    ],
    targets: [
        .executableTarget(
            name: "ClaudeLight",
            path: "Sources/ClaudeLight"
        )
    ]
)
