// swift-tools-version:5.9

import PackageDescription

let package = Package(
    name: "PLCrashReporter",
    defaultLocalization: "en",
    platforms: [
        .iOS(.v12),
        .macOS(.v10_13),
        .tvOS(.v12)
    ],
    products: [
        .library(name: "CrashReporter", targets: ["CrashReporter"])
    ],
    targets: [
        .target(
            name: "CrashReporter",
            path: "",
            exclude: [
                "Source/PLCrashReport.proto",
                "Tools/CrashViewer/",
                "Other Sources/Crash Demo/",
                "Dependencies/protobuf-c/generate-pb-c.sh",
            ],
            sources: [
                "Source",
                "Dependencies/protobuf-c"
            ],
            resources: [.process("Resources/PrivacyInfo.xcprivacy")],
            cSettings: [
                .define("PLCR_PRIVATE"),
                .define("PLCF_RELEASE_BUILD"),
                .define("PLCRASHREPORTER_PREFIX", to: ""),
                .define("SWIFT_PACKAGE"), // Should be defined by default, Xcode 11.1 workaround.
                .headerSearchPath("Dependencies/protobuf-c")
            ],
            swiftSettings: [
                .interoperabilityMode(.Cxx),
            ],
            linkerSettings: [
                .linkedFramework("Foundation")
            ]
        )
    ]
)
