// swift-tools-version:5.3

import PackageDescription

let package = Package(
    name: "PLCrashReporter",
    defaultLocalization: "en",
    platforms: [
        .iOS(.v11),
        .macOS(.v10_10),
        .tvOS(.v11)
    ],
    products: [
        .library(name: "CrashReporter", targets: ["CrashReporter"])
    ],
    targets: [
        .target(
            name: "CrashReporter",
            path: "",
            exclude: [
                "Source/dwarf_opstream.hpp",
                "Source/dwarf_stack.hpp",
                "Source/PLCrashAsyncDwarfCFAState.hpp",
                "Source/PLCrashAsyncDwarfCIE.hpp",
                "Source/PLCrashAsyncDwarfEncoding.hpp",
                "Source/PLCrashAsyncDwarfExpression.hpp",
                "Source/PLCrashAsyncDwarfFDE.hpp",
                "Source/PLCrashAsyncDwarfPrimitives.hpp",
                "Source/PLCrashAsyncLinkedList.hpp",
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
            linkerSettings: [
                .linkedFramework("Foundation")
            ]
        )
    ]
)
