// swift-tools-version:5.0

import PackageDescription

let package = Package(
    name: "PLCrashReporter",
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
                "Source/PLCrashReport.proto"
            ],
            sources: [
                "Source",
                "Dependencies/protobuf-c"
            ],
            cSettings: [
                .define("PLCR_PRIVATE"),
                .define("PLCF_RELEASE_BUILD"),
                .define("PLCRASHREPORTER_PREFIX", to: ""),
                .headerSearchPath("Dependencies/protobuf-c")
            ],
            linkerSettings: [
                .linkedFramework("Foundation")
            ]
        )
    ]
)
