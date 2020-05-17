# Backtrace-PLCrashReporter

<p align="center">
    <img src="https://img.shields.io/badge/platform-iOS%2010%2B%20%7C%20tvOS%2010%2B%20%7C%20macOS%2010.10%2B-blue.svg" alt="Supported platforms"/>
    <a href="https://cocoapods.org/pods/Backtrace-PLCrashReporter"><img src="https://img.shields.io/cocoapods/v/Backtrace-PLCrashReporter.svg?style=flat" alt="CocoaPods compatible" /></a>
</p>

PLCrashReporter is a reliable open source library that provides an in-process live crash reporting framework for use on iOS, macOS and tvOS. The library detects crashes and generates reports to help your investigation and troubleshooting with the information of application, system, process, thread, etc. as well as stack traces.

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
- Minimum supported platforms: iOS 10, macOS 10.10, tvOS 10.

## Installation

### via CocoaPods

To use [CocoaPods](https://cocoapods.org) just add this to your Podfile:

```ruby
pod 'Backtrace-PLCrashReporter'
```

**Note:** It is required to specify `use_frameworks!` in your Podfile.

## Building

### Prerequisites

- A Mac running macOS compliant with Xcode requirements
- Xcode 10 or above

### Setup

Setup script will make sure all required dependencies are installed, including: RubyGems, Homebrew packages, Git submodules, etc. The goal is to make sure all required dependencies are installed.
Also, it is used to set up a project in an initial state. 

- Open a new window for your Terminal.
- Go to PLCrashReporter's root folder and run

    ```bash
    sh scripts/setup.sh
    ```

#### Third party tools
- [Doxygen](https://github.com/doxygen/doxygen) to generate the documentation.
- [GraphViz](https://www.graphviz.org/download/) to generate the documentation. 
- [protobuf-c](https://github.com/protobuf-c/protobuf-c) to convert Protocol Buffer `.proto` files to C descriptor code.
- [XcodeGen](https://github.com/yonaskolb/XcodeGen) to generate Xcode project.

### To validate pod
CI build script will validate pod specifications. 

- Open a new window for your Terminal.
- Go to PLCrashReporter's root folder and run

    ```bash
    sh scripts/cibuild.sh
    ```