# PLCrashReporter

Reliable, open-source crash reporting for iOS, Mac OS X and tvOS.

The easiest way to use PLCrashReporter is by using [AppCenter](https://appcenter.ms). However, if you want to use PLCrashReporter directly, grab the latest release at [https://github.com/Microsoft/PLCrashReporter/releases](https://github.com/Microsoft/PLCrashReporter/releases) and check out the [official PLCrashReporter documentation](https://www.plcrashreporter.org/documentation).

## Features

- Uses only supported and public APIs/ABIs for crash reporting.
- The most accurate stack unwinding available, using DWARF and Apple Compact Unwind frame data.
- First released in 2008, and used in hundreds of thousands of apps. PLCrashReporter has seen a tremendous amount of user testing.
- Does not interfere with debugging in lldb/gdb
- Easy to integrate with existing or custom crash reporting services.
- Backtraces for all active threads are provided.
- Provides full register state for the crashed thread.

## Prerequisites

- Xcode 10 or above.
- Minimum supported platforms: iOS 8, macOS 10.7, tvOS 9.

## Decoding Crash Reports

Crash reports are output as protobuf-encoded messages, and may be decoded using the CrashReporter library or any Google Protobuf decoder.

In addition to the in-library decoding support, you may use the included plcrashutil binary to convert crash reports to apple's standard iPhone text format. This may be passed to the symbolicate tool.

`./bin/plcrashutil convert --format=iphone example_report.plcrash | symbolicatecrash`
Future library releases may include built-in re-usable formatters, for outputting alternative formats directly from the phone.

## Building

### Prerequisites

- A Mac running macOS compliant with Xcode requirements
- Xcode 10.1 or above
- Doxygen to generate the documentation. See [the official Doxygen repository](https://github.com/doxygen/doxygen) for more information or use [Homebrew](https://brew.sh) to install it.
- GraphViz to generate the documentation. See [the official GraphViz website](https://www.graphviz.org/download/) for more information or use [Homebrew](https://brew.sh) to install it.
- `protobuf-c` to convert Protocol Buffer `.proto` files to C descriptor code. See [the official protobuf-c repository](https://github.com/protobuf-c/protobuf-c) for more information or use [Homebrew](https://brew.sh) to install it.

### To build

- Open a new window for your Terminal.
- Go to PLCrashReporter's root folder and run

    ```bash
    xcodebuild BITCODE_GENERATION_MODE=bitcode OTHER_CFLAGS="-fembed-bitcode" -configuration Release -target 'Disk Image'
    ```

    to create binaries for all platforms.
- Verify that your iOS and tvOS binaries have Bitcode enabled by running `otool -l build/Release-appletv/CrashReporter.framework/Versions/A/CrashReporter | grep __LLVM` (adjust the path to the binary as necessary). If you see some output, it means the binary is Bitcode enabled.

## Contributing

We are looking forward to your contributions via pull requests.

To contribute to PLCrashReporter, you need the tools mentioned above to build PLCrashReporter for all architectures.

### Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

### Checking out the repository

PLCrashReporter has a dependency on [Protocol Buffers implementation in C](https://github.com/protobuf-c/protobuf-c) as a git submodule. Use below command to clone PLCrashReporter repository or update the repository if you have already cloned it.

```bash
git clone --recursive https://github.com/microsoft/plcrashreporter.git
```

```bash
git submodule update --init --recursive
```
