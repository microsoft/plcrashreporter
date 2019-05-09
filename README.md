# PLCrashReporter

Reliable, open-source crash reporting for iOS and Mac OS X.

## Features

- Uses only supported and public APIs/ABIs for crash reporting.
- The most accurate stack unwinding available, using DWARF and Apple Compact Unwind frame data.
- First released in 2008, and used in hundreds of thousands of apps. PLCrashReporter has seen a tremendous amount of user testing.
- Does not interfere with debugging in lldb/gdb
- Easy to integrate with existing or custom crash reporting services.
- Backtraces for all active threads are provided.
- Provides full register state for the crashed thread.

## Decoding Crash Reports
Crash reports are output as protobuf-encoded messages, and may be decoded using the CrashReporter library or any Google Protobuf decoder.

In addition to the in-library decoding support, you may use the included plcrashutil binary to convert crash reports to apple's standard iPhone text format. This may be passed to the symbolicate tool.

`./bin/plcrashutil convert --format=iphone example_report.plcrash | symbolicatecrash`
Future library releases may include built-in re-usable formatters, for outputting alternative formats directly from the phone.

## Building
To build an embeddable framework:

`user@max:~/plcrashreporter-trunk> xcodebuild -configuration Release -target 'Disk Image'`

This will output a new release disk image containing an embeddable Mac OS X framework and an iOS static framework in `build/Release/PLCrashReporter-{version}.dmg`
