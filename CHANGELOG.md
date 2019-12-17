# PLCrashReporter Change Log

## Version 1.5.0 (Under development)

* Drop support old versions of Xcode and iOS. The minimal versions are Xcode 10 and iOS 8 now.
* Remove `UIKit` dependency on iOS.
* Fix arm64e crash report text formatting.
* Fix possible crash `plcrash_log_writer_set_exception` method when `NSException` instances have a `nil` reason.

___

## Version 1.4.0

* Support macOS 10.15 and XCode 11 and drop support for macOS 10.6.
* Add support for tvOS apps.
* Update `protobuf-c` to version 1.3.2. `protoc-c` code generator binary has been removed from the repo, so it should be installed separately now (`brew install protobuf-c`). `protoc-c` C library is included as a git submodule, please make sure that it's initialized after update (`git submodule update --init`).
* Remove outdated "Google Toolbox for Mac" dependency.
* The sources aren't distributed in the release archive anymore. Please use GitHub snapshot instead.
* Distribute static libraries in a second archive aside the frameworks archive.
* Fix minor bugs in runtime symbolication: use correct bit-mask for the data pointer and correctly reset error code if no categories for currently symbolicating class.
* Add preview support for the arm64e CPU architecture.
* Support for arm64e devices that run an arm64 slice (which is the default for apps that were compiled with Xcode 10 or earlier).
* Remove support for armv6 CPU architecture as it is no longer supported.
* Improve namespacing to avoid symbol collisions when integrating PLCrashReporter.
* Fix a crash that occurred on macOS where PLCrashReporter would be caught in an endless loop handling signals. 
* Make it possible to not add an uncaught exception handler via `shouldRegisterUncaughtExceptionHandler` property on `PLCrashReporterConfig`. This scenario is important when using PLCrashReporter inside managed runtimes, i.e. for a Xamarin app. This is not a breaking change and behavior will not change if you use PLCrashReporter.
