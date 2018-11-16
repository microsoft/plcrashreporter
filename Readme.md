# PLCrashReporter - Reliable, open source crash reporter for iOS, macOS and tvOS

## 1. Information about this fork

This is a fork of the [official PLCrashReporter repository](https://github.com/plausiblelabs/plcrashreporter). It is based on [PLCrashReporter 1.2.0](https://github.com/plausiblelabs/plcrashreporter/releases/tag/1.2) (commit [42ce50ad93955eb2b29488fefd6f0a29329a6446](https://github.com/plausiblelabs/plcrashreporter/commit/42ce50ad93955eb2b29488fefd6f0a29329a6446)) with additional fixes and changes.
It is used in the following SDKs:

* [AppCenter-SDK-Mac](https://github.com/Microsoft/AppCenter-SDK-Apple)
* [HockeySDK-iOS](https://github.com/BitStadium/HockeySDK-iOS)
* [HockeySDK-Mac](https://github.com/BitStadium/HockeySDK-Mac)
* [HockeySDK-tvOS](https://github.com/BitStadium/HockeySDK-tvOS)

> [Note!] Please note that this fork is based on 1.2.0 of PLCrashReporter and not 1.3.0 which is currently in `master` of the official repository.

## 1.1 Differences between this fork and the official repository

* Added support for tvOS.
* Added support for the arm64e CPU architecture.
* Drop support for armv6 CPU architecture.
* Improved namespacing to avoid symbol collisions when integrating PLCrashReporter.
* Updated API to allow to not add an uncaught exception handler when configuring PLCrashReporter. This is important for scenarios where PLCrashReporter is used in a managed runtime, i.e. a Xamarin application.

## 1.2 Building PLCrashReporter locally

To build PLCrashReporter, we recommend building from the command line.

* Open the Xcode project.
* Open `PLCrashNamespace.h` and set  `#define PLCRASHREPORTER_PREFIX` to your class prefix (i.e. `MS`).
* Close Xcode and open a Terminal
* Go to PlCrashReporter's root folder and run `xcodebuild PL_ALLOW_LOCAL_MODS=1 BITCODE_GENERATION_MODE=bitcode OTHER_CFLAGS="-fembed-bitcode" -configuration Release -target 'Disk Image'` to create binaries for all platforms
* Verify that we have a bitcode enabled framework by running `otool -l build/Release-appletv/CrashReporter.framework/Versions/A/CrashReporter | grep __LLVM`

## 2. Contributing

We are looking forward to your contributions via pull requests.

To contribute to PLCrashReporter, you need:

* A mac that is capable of running Xcode 8 and Xcode 10 (macOS Mojave cannot run Xcode 8 and is not recommended at this time).
* Install Doxygen to create documentation. See [the official Doxygen repository](https://github.com/doxygen/doxygen) for more information how to install it or use [homebrew]().

### 2.1 Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## 3. Contact

This fork of the [official PLCrashReporter repository](https://github.com/plausiblelabs/plcrashreporter) is maintained by the team of [Visual Studio App Center](https://appcenter.ms). For general information about PLCrashReporter, please visit [the official PLCrashReporter website](http://plcrashreporter.org).

### 3.1 Intercom

If you have further questions, want to provide feedback or you are running into issues, log in to the [App Center portal](https://appcenter.ms) and use the blue Intercom button on the bottom right to start a conversation with us.

### 3.2 Twitter
We're on Twitter as [@vsappcenter](https://www.twitter.com/vsappcenter).
