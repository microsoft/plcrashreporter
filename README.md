[![CocoaPods](https://img.shields.io/cocoapods/v/PLCrashReporter.svg)](https://cocoapods.org/pods/PLCrashReporter)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg)](https://github.com/Carthage/Carthage)
[![SwiftPM compatible](https://img.shields.io/badge/SwiftPM-compatible-brightgreen.svg)](https://swift.org/package-manager)

# PLCrashReporter

PLCrashReporter is a reliable open source library that provides an in-process live crash reporting framework for use on iOS, macOS and tvOS. The library detects crashes and generates reports to help your investigation and troubleshooting with the information of application, system, process, thread, etc. as well as stack traces.

The easiest way to use PLCrashReporter is by using [AppCenter](https://appcenter.ms). However, if you want to use PLCrashReporter directly, grab the latest release at [releases page](https://github.com/microsoft/plcrashreporter/releases).

## Features

- Uses only supported and public APIs/ABIs for crash reporting.
- The most accurate stack unwinding available, using DWARF and Apple Compact Unwind frame data.
- First released in 2008, and used in hundreds of thousands of apps. PLCrashReporter has seen a tremendous amount of user testing.
- Does not interfere with debugging in lldb/gdb
- Backtraces for all active threads are provided.
- Provides full register state for the crashed thread.

## Prerequisites

- Xcode 11 or above.
- Minimum supported platforms: iOS 11, macOS 10.9, tvOS 11, Mac Catalyst 13.0.

## Decoding Crash Reports

Crash reports are output as protobuf-encoded messages, and may be decoded using the CrashReporter library or any [Google Protocol Buffers](https://developers.google.com/protocol-buffers/) decoder.

In addition to the in-library decoding support, you may use the included `plcrashutil` binary to convert crash reports to apple's standard iPhone text format:

```ruby
plcrashutil convert --format=iphone example_report.plcrash
```

You can use `atos` command-line tool to symbolicate the output. For more information about this tool, see [Adding Identifiable Symbol Names to a Crash Report](https://developer.apple.com/documentation/Xcode/adding-identifiable-symbol-names-to-a-crash-report).
Future library releases may include built-in re-usable formatters, for outputting alternative formats directly from the phone.

## Adding PLCrashReporter to your project

PLCrashReporter can be added to your app via [CocoaPods](https://guides.cocoapods.org/using/using-cocoapods.html), [Carthage](https://github.com/Carthage/Carthage#quick-start), [Swift Package Manager](https://developer.apple.com/documentation/xcode/adding_package_dependencies_to_your_app), or by manually adding the binaries to your project.

### Integration via Cocoapods

1. Add the following line to your `Podfile`:
    ```ruby
    pod 'PLCrashReporter'
    ```
1. Run `pod install` to install your newly defined pod and open the project's `.xcworkspace`.

### Integration via Swift Package Manager

1. From the Xcode menu, click **File** > **Swift Packages** > **Add Package Dependency**.
1. In the dialog that appears, enter the repository URL: https://github.com/microsoft/plcrashreporter.git.
1. In Version, select **Up to Next Major** and take the default option.

### Integration via Carthage

1. Add the following line to your `Cartfile`:
    ```ruby
    github "microsoft/plcrashreporter"
    ```
1. Run `carthage update --use-xcframeworks` to fetch dependencies.
2. In Xcode, open your application target's **General** settings tab.  Drag and drop **CrashReporter.xcframework** from the **Carthage/Build** folder into the `Frameworks, Libraries and Embedded Content` section.  For iOS and tvOS, set `Embed` to `Do not embed`.  For macoS, set `Embed` to `Embed and Sign`.

> **NOTE:**
> Carthage integration doesn't build the dependency correctly in Xcode 12 with flag "--no-use-binaries" or from a specific branch. To make it work, refer to [this instruction](https://github.com/Carthage/Carthage/blob/master/Documentation/Xcode12Workaround.md).

### Integration by copying the binaries into your project

1. Download the [PLCrashReporter](https://github.com/Microsoft/plcrashreporter/releases) frameworks provided as a zip file.
2. Unzip the file and you'll see a folder called **PLCrashReporter** that contains subfolders for all supported platforms.
3. Add PLCrashReporter to the project in Xcode:
   * Make sure the Project Navigator is visible (⌘+1).
   * Now drag & drop **PLCrashReporter.framework** (or **PLCrashReporter.xcframework**) from the Finder into Xcode's Project Navigator.
   * A dialog will appear, make sure your app target is checked and click **Finish**.
    > **NOTE:**
    > PLCrashReporter xcframework contains static binaries for iOS and tvOS, and dynamic binaries for macOS. When adding the framework to your project make sure that in `Frameworks, Libraries and Embedded Content` section `Embed` is selected to `Do not embed` for iOS and tvOS and `Embed and Sign` for macOS. `PLCrashReporter-Static-{version}.zip` is an exception - it contains static frameworks for all platforms.

## Example

The following example shows a way how to initialize crash reporter. Please note that enabling in-process crash reporting will conflict with any attached debuggers so make sure the **debugger isn't attached** when you crash the app.

### Objective-c

```objc
@import CrashReporter;

...

// Uncomment and implement isDebuggerAttached to safely run this code with a debugger.
// See: https://github.com/microsoft/plcrashreporter/blob/2dd862ce049e6f43feb355308dfc710f3af54c4d/Source/Crash%20Demo/main.m#L96
// if (![self isDebuggerAttached]) {

// It is strongly recommended that local symbolication only be enabled for non-release builds.
// Use PLCrashReporterSymbolicationStrategyNone for release versions.
PLCrashReporterConfig *config = [[PLCrashReporterConfig alloc] initWithSignalHandlerType: PLCrashReporterSignalHandlerTypeMach
                                                                   symbolicationStrategy: PLCrashReporterSymbolicationStrategyAll];
PLCrashReporter *crashReporter = [[PLCrashReporter alloc] initWithConfiguration: config];

// Enable the Crash Reporter.
NSError *error;
if (![crashReporter enableCrashReporterAndReturnError: &error]) {
    NSLog(@"Warning: Could not enable crash reporter: %@", error);
}
// }
```

Checking collected crash report can be done in the following way:

```objc
if ([crashReporter hasPendingCrashReport]) {
    NSError *error;

    // Try loading the crash report.
    NSData *data = [crashReporter loadPendingCrashReportDataAndReturnError: &error];
    if (data == nil) {
        NSLog(@"Failed to load crash report data: %@", error);
        return;
    }

    // Retrieving crash reporter data.
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: data error: &error];
    if (report == nil) {
        NSLog(@"Failed to parse crash report: %@", error);
        return;
    }

    // We could send the report from here, but we'll just print out some debugging info instead.
    NSString *text = [PLCrashReportTextFormatter stringValueForCrashReport: report withTextFormat: PLCrashReportTextFormatiOS];
    NSLog(@"%@", text);

    // Purge the report.
    [crashReporter purgePendingCrashReport];
}
```

### Swift

```swift 
import CrashReporter

...
// Uncomment and implement isDebuggerAttached to safely run this code with a debugger.
// See: https://github.com/microsoft/plcrashreporter/blob/2dd862ce049e6f43feb355308dfc710f3af54c4d/Source/Crash%20Demo/main.m#L96
// if (!isDebuggerAttached()) {

  // It is strongly recommended that local symbolication only be enabled for non-release builds.
  // Use [] for release versions.
  let config = PLCrashReporterConfig(signalHandlerType: .mach, symbolicationStrategy: .all)
  guard let crashReporter = PLCrashReporter(configuration: config) else {
    print("Could not create an instance of PLCrashReporter")
    return
  }

  // Enable the Crash Reporter.
  do {
    try crashReporter.enableAndReturnError()
  } catch let error {
    print("Warning: Could not enable crash reporter: \(error)")
  }
// }
```

Checking collected crash report can be done in the following way:

```swift
  // Try loading the crash report.
  if crashReporter.hasPendingCrashReport() {
    do {
      let data = try crashReporter.loadPendingCrashReportDataAndReturnError()

      // Retrieving crash reporter data.
      let report = try PLCrashReport(data: data)

      // We could send the report from here, but we'll just print out some debugging info instead.
      if let text = PLCrashReportTextFormatter.stringValue(for: report, with: PLCrashReportTextFormatiOS) { 
        print(text)
      } else {
        print("CrashReporter: can't convert report to text")
      }
    } catch let error {
      print("CrashReporter failed to load and parse with error: \(error)")
    }
  }

  // Purge the report.
  crashReporter.purgePendingCrashReport()
```

## Building

### Prerequisites

- A Mac running macOS compliant with Xcode requirements.
- Xcode 11 or above.

Also, next optional tools are used to build additional resources:

- Doxygen to generate the documentation. See [the official Doxygen repository](https://github.com/doxygen/doxygen) for more information or use [Homebrew](https://brew.sh) to install it.
- GraphViz to generate the documentation. See [the official GraphViz website](https://www.graphviz.org/download/) for more information or use [Homebrew](https://brew.sh) to install it.
- `protobuf-c` to convert Protocol Buffer `.proto` files to C descriptor code. See [the official protobuf-c repository](https://github.com/protobuf-c/protobuf-c) for more information or use [Homebrew](https://brew.sh) to install it.

### Building

- Open a new window for your Terminal.
- Go to PLCrashReporter's root folder and run

    ```bash
    xcodebuild -configuration Release -target 'CrashReporter'
    ```

    to create binaries for all platforms.

## Contributing

We are looking forward to your contributions via pull requests.

To contribute to PLCrashReporter, you need the tools mentioned above to build PLCrashReporter for all architectures and `protobuf-c` to convert Protocol Buffer `.proto` files to C descriptor code.

### Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
