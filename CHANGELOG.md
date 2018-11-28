# PLCrashReporter Change Log

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
