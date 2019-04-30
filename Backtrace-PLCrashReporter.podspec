
Pod::Spec.new do |s|

  s.name         = "Backtrace-PLCrashReporter"
  s.version      = "1.5.0"
  s.summary      = "Reliable, open-source crash reporting for iOS, tvOS and macOS."
  s.description      = <<-DESC
                      Plausible CrashReporter provides an in-process crash reporting
                      framework for use on both iOS, tvOS and macOS, and powers many of
                      the crash reporting services available for iOS, tvOS, macOS, including
                      Backtrace, HockeyApp, Flurry, Crittercism and FoglightAPM.
                     DESC

  s.homepage     = "https://github.com/backtrace-labs/plcrashreporter"

  s.license          = { :type => 'MIT', :file => 'LICENSE' }
  s.author           = { 'Plausible Labs Cooperative, Inc.' => 'contact@plausible.coop' }

  s.ios.deployment_target = "10.0"
  s.tvos.deployment_target = "10.0"
  s.osx.deployment_target = "10.10"

  s.source       = { :git => "https://github.com/backtrace-labs/plcrashreporter.git", :tag => "#{s.version}" }

  s.source_files  = "Source/**/*.{h,hpp,c,cpp,m,mm,s}",
                    "Dependencies/protobuf-2.0.3/src/*.{h,c}"
  s.exclude_files = "**/*Tests*",
                    "**/*TestCase*",
                    "**/*test.*",
                    "**/*_test_*",
                    "**/*main.m"

  s.public_header_files = "Source/PLCrashReport*.h",
                          "Source/PLCrashNamespace*.h",
                          "Source/PLCrashMacros.h",
                          "Source/PLCrashFeatureConfig.h",
                          "Source/CrashReporter.h"
  s.preserve_paths = "Dependencies/**"


  s.resources = "Resources/*.proto"
  s.pod_target_xcconfig = {
    "GCC_PREPROCESSOR_DEFINITIONS" => "PLCR_PRIVATE"
  }
  s.libraries = "c++"
  s.requires_arc = false

  s.prefix_header_contents = "#import \"PLCrashNamespace.h\""

  s.prepare_command =
  <<-CMD
    cd "Resources" && "../Dependencies/protobuf-2.0.3/bin/protoc-c" --c_out="../Source" "crash_report.proto" && cd ..
    find . \\( -iname '*.h' -o -iname '*.hpp' -o -iname '*.c' -o -iname '*.cc' -o -iname '*.cpp' -o -iname '*.m' -o -iname '*.mm' \\) -exec sed -i '' -e 's/#include <google\\/protobuf-c\\/protobuf-c.h>/#include "..\\/Dependencies\\/protobuf-2.0.3\\/include\\/google\\/protobuf-c\\/protobuf-c.h"/g' {} \\;
    find . \\( -iname '*.h' -o -iname '*.hpp' -o -iname '*.c' -o -iname '*.cc' -o -iname '*.cpp' -o -iname '*.m' -o -iname '*.mm' \\) -exec sed -i '' -e 's/#import "CrashReporter\\/CrashReporter.h"/#import "CrashReporter.h"/g' {} \\;
    SDKROOT=$(xcrun --sdk macosx --show-sdk-path)
    mig -arch "i386" -header "Source/mach_exc_i386.h" -server /dev/null -user "Source/mach_exc_i386User.inc" "${SDKROOT}/usr/include/mach/mach_exc.defs"
    mig -arch "x86_64" -header "Source/mach_exc_x86_64.h" -server /dev/null -user "Source/mach_exc_x86_64User.inc" "${SDKROOT}/usr/include/mach/mach_exc.defs"
    echo '#ifdef __LP64__'                       > Source/mach_exc.h
    echo '#include "mach_exc_x86_64.h"'         >> Source/mach_exc.h
    echo '#else'                                >> Source/mach_exc.h
    echo '#include "mach_exc_i386.h"'           >> Source/mach_exc.h
    echo '#endif'                               >> Source/mach_exc.h
    FILE_86=$(cat Source/mach_exc_i386User.inc)
    FILE_64=$(cat Source/mach_exc_x86_64User.inc)
    echo '#import "PLCrashFeatureConfig.h"'      > Source/mach_exc.c
    echo '#if PLCRASH_FEATURE_MACH_EXCEPTIONS'  >> Source/mach_exc.c
    echo '#ifdef __LP64__'                      >> Source/mach_exc.c
    echo "$FILE_64"                             >> Source/mach_exc.c
    echo '#else'                                >> Source/mach_exc.c
    echo "$FILE_86"                             >> Source/mach_exc.c
    echo '#endif'                               >> Source/mach_exc.c
    echo '#endif'                               >> Source/mach_exc.c

  CMD
end
