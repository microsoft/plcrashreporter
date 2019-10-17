# PLCrashReporter Change Log

## Version "next"

* Support macOS 10.15 and XCode 11.
* Update `protobuf-c` to version 1.3.2. `protoc-c` code generator binary has been removed from the repo, so it should be installed separately now (`brew install protobuf-c`). `protoc-c` C library is included as a git submodule, please make sure that it's initialized after update (`git submodule update --init`).
* Remove outdated "Google Toolbox for Mac" dependency.
* The sources aren't distributed in the release archive anymore. Please use GitHub snapshot instead.
* Distribute static libraries in a second archive aside the frameworks archive.
* Fixed minor bugs in runtime symbolication: use correct bit-mask for the data pointer and correctly reset error code if no categories for currently symbolicating class.

___

## Version 1.2.3-rc1

* Add preview support for the arm64e CPU architecture.
* Support for arm64e devices that run an arm64 slice (which is the default for apps that were compiled with Xcode 10 or earlier).

___

## Version 1.2.2

* Make sure PLCrashReporter builds fine using Xcode 8.3.2.
* Add support for tvOS apps.
* Remove support for armv6 CPU architecture as it is no longer supported in Xcode 8.
* Improve namespacing to avoid symbol collisions when integrating PLCrashReporter.
* Fix a crash that occurred on macOS where PLCrashReporter would be caught in an endless loop handling signals. 855964ab7ee40bbe14e037533c1c76f76a5e8c59
* Make it possible to not add an uncaught exception handler, a scenario that is important when using PLCrashReporter inside managed runtimes, i.e. for a Xamarin app. This not a breaking change and behavior will not change if you use PLCrashReporter. 597165a432ed9545bf0cffb0ac48547f8ea98d89
* Drop support for macOS 10.6.
