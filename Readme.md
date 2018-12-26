# PLCrashReporter - A reliable, open source crash reporter for iOS, macOS, and tvOS

## 1. Information about this fork

This is a fork of the [official PLCrashReporter repository](https://github.com/plausiblelabs/plcrashreporter). It is maintained by the [App Center](https://appcenter.ms) team and based on PLCrashReporter 1.2.1 (commit [fda23306](https://github.com/Microsoft/PLCrashReporter/tree/fda233062b5586f5d01cc527af643168665226c0)) with additional fixes and changes.
It was created for use in the following SDKs:

* [AppCenter-SDK-Apple for iOS and macOS](https://github.com/Microsoft/AppCenter-SDK-Apple)
* [HockeySDK-iOS](https://github.com/BitStadium/HockeySDK-iOS)
* [HockeySDK-Mac](https://github.com/BitStadium/HockeySDK-Mac)
* [HockeySDK-tvOS](https://github.com/BitStadium/HockeySDK-tvOS)

> **Please note that this fork is based on version 1.2.1 of PLCrashReporter and not 1.3 which is currently in `master` of the official repository.**

The focus of this fork is to add new features to PLCrashReporter (e.g. support for tvOS or the new arm64e CPU architecture). To keep changes to a minimum, this fork deliberately does not address compile-time warnings. That said, the fork has been battle-tested in the SDKs mentioned above.
At this time, we are hoping to contribute our changes to the official PLCrashReporter repository and are talking to the team at Plausible Labs.  

## 1.1 Differences between this fork and the official repository

Please check out our [change log](CHANGELOG.md).

## 1.2 Using PLCrashReporter

The easiest way to use PLCrashReporter is by using [AppCenter](https://appcenter.ms). However, if you want to use PLCrashReporter directly, grab the latest release at [https://github.com/Microsoft/PLCrashReporter/releases](https://github.com/Microsoft/PLCrashReporter/releases) and check out the [official PLCrashReporter documentation](https://www.plcrashreporter.org/documentation).

## 1.3 Building PLCrashReporter

To build PLCrashReporter, we recommend using the command line as the PLCrashReporter project has issues when compiling some of its targets in Xcode due to Xcode 10's new build system (check out the [Xcode 10 release notes](https://developer.apple.com/documentation/xcode_release_notes/xcode_10_release_notes/build_system_release_notes_for_xcode_10) for more information about the new build system]).

### 1.3.1 Prerequisites

* A Mac
* Xcode 10.1
* Doxygen to generate the documentation. See [the official Doxygen repository](https://github.com/doxygen/doxygen) for more information or use [Homebrew](https://brew.sh) to install it.
* GraphViz to generate the documentation. See [the official GraphViz website](https://www.graphviz.org/download/) for more information or use [Homebrew](https://brew.sh) to install it.

### 1.3.2 How to build PLCrashReporter with Xcode 10.1

* Open `CrashReporter.xcodeproj` in Xcode 10.1.
* Open `PLCrashNamespace.h` and set  `#define PLCRASHREPORTER_PREFIX` to your class prefix (i.e. `MS`).
* Open a new window for your Terminal.
* Go to PlCrashReporter's root folder and run

    ```bash
    xcodebuild PL_ALLOW_LOCAL_MODS=1 BITCODE_GENERATION_MODE=bitcode OTHER_CFLAGS="-fembed-bitcode" -configuration Release -target 'Disk Image'
    ```

    to create binaries for all platforms.
* Verify that your iOS and tvOS binaries have Bitcode enabled by running `otool -l build/Release-appletv/CrashReporter.framework/Versions/A/CrashReporter | grep __LLVM` (adjust the path to the binary as necessary). If you see some output, it means the binary is Bitcode enabled.

### 1.3.3 How to build PLCrashReporter if you care about Xcode backward compatibility

As [Bitcode](http://llvm.org/docs/BitCodeFormat.html) versions are not backward compatible, it is required to build an SDK or component with the minimum Xcode version that the SDK needs to support. In the past, this meant that you would simply build PLCrashReporter with the oldest Xcode version that you care about. With the introduction of the arm64e CPU architecture in Fall 2018, things got more complicated.
To ensure PLCrashReporter supports apps that use Xcode 8.3.3, it needs to be built using Xcode 8.3.3. At the same time, PLCrashReporter 1.2.3-rc1 and later support the arm64e CPU architecture. The arm64e architecture can only be built with Xcode 10.1 and later and is currently in preview (check out the [Xcode 10.1 release notes](https://developer.apple.com/documentation/xcode_release_notes/xcode_10_1_release_notes) for more information). To reconcile both backward compatibility with Xcode 8 and support for arm64e CPUs, you need to build all architecture slices of PLCrashReporter-iOS using Xcode 8.3.3 and then merge them using the `lipo`-tool with an arm64e slice that was built with Xcode 10.1 and up. To make this easier, we have updated the script that creates the CrashReporter-iOS binary.

#### 1.3.3.1 Additional prerequisites

* Install the prerequisites as explained in 1.3.1 above.
* Install Xcode 10.1 and the oldest Xcode version that you care about, i.e. Xcode 8.3.3 in parallel (information on how to do that can be found at [https://medium.com/@hacknicity/working-with-multiple-versions-of-xcode-e331c01aa6bc](https://medium.com/@hacknicity/working-with-multiple-versions-of-xcode-e331c01aa6bc).
* Make sure you are using the old version of Xcode by running `xcode-select -p`. It should point to your oldest Xcode version. If it points to a newer version, e.g. Xcode 10.1, use `sudo xcode-select -s PATH_TO_OLD_XCODE` to switch to the old Xcode version.

#### 1.3.3.2 Build PLCrashReporter in a way that is backward compatible with older Xcode versions

* Open `CrashReporter.xcodeproj` in Xcode 10.1.
* Open `PLCrashNamespace.h` and set  `#define PLCRASHREPORTER_PREFIX` to your class prefix (i.e. `MS`).
* Open your Terminal.
* Go to PLCrashReporter's root folder and run

    ```bash
    xcodebuild PL_ARM64E_XCODE_PATH="Path to your Xcode 10.1 installation" PL_ALLOW_LOCAL_MODS=1 BITCODE_GENERATION_MODE=bitcode OTHER_CFLAGS="-fembed-bitcode" -configuration Release -target 'Disk Image'
    ```

    This will create binaries for all platforms and adds support for arm64e to PLCrashReporter-iOS. Note the environment variable `PL_ARM64E_XCODE_PATH`. Make sure to set it to your latest Xcode version that supports arm64e, currently Xcode 10.1.
* Verify that your iOS and tvOS binaries have Bitcode enabled by running `otool -l build/Release-appletv/CrashReporter.framework/Versions/A/CrashReporter | grep __LLVM` (adjust the path to the binary as necessary). If you see some output, it means the binary is Bitcode enabled.

## 2. Contributing

We are looking forward to your contributions via pull requests.

To contribute to PLCrashReporter, you need the tools mentioned in 1.3.3.1 above to build PLCrashReporter for all architectures.

### 2.1 Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## 3. Contact

This fork of the [official PLCrashReporter repository](https://github.com/plausiblelabs/plcrashreporter) is maintained by the team of [Visual Studio App Center](https://appcenter.ms). For general information about PLCrashReporter, please visit [the official PLCrashReporter website](http://plcrashreporter.org).

### 3.1 Twitter

We're on Twitter as [@vsappcenter](https://www.twitter.com/vsappcenter).
