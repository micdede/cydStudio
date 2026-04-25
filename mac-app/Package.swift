// swift-tools-version: 5.9
//
// cydStudio macOS app — SwiftUI, SPM-based.
//
// Open in Xcode: `open Package.swift` from this directory.
// Or run from CLI: `swift run` (debug build, fast iteration).
// Release build:  `swift build -c release` → .build/release/CydStudio
//
// macOS 14 baseline lets us use NWBrowser, modern SwiftUI tables, and
// Observation without backports.
import PackageDescription

let package = Package(
    name: "CydStudio",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "CydStudio", targets: ["CydStudio"])
    ],
    targets: [
        .executableTarget(
            name: "CydStudio",
            resources: [
                .copy("Resources/examples"),
                .copy("Resources/schema"),
                .copy("Resources/firmware"),
            ]
        )
    ]
)
