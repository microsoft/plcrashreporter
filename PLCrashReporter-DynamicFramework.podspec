Pod::Spec.new do |s|
  s.name             = 'PLCrashReporter-DynamicFramework'
  s.version          = '1.3.0.1'
  s.summary          = 'Reliable, open-source crash reporting for iOS and Mac OS X.'

  s.description      = <<-DESC
                        Plausible CrashReporter provides an in-process crash reporting 
                        framework for use on both iOS and Mac OS X, and powers many of 
                        the crash reporting services available for iOS, including 
                        HockeyApp, Flurry, Crittercism and FoglightAPM.
                       DESC

  s.homepage         = 'https://github.com/plausiblelabs/plcrashreporter'
  s.license          = { :type => 'MIT', :file => 'LICENSE' }
  s.author           = { 'Plausible Labs Cooperative, Inc.' => 'contact@plausible.coop' }
  s.source           = { :git => 'https://github.com/plausiblelabs/plcrashreporter.git', :commit => "5347b1e1e5f2e134f624dd0c47de74b8b40a57eb" }

  s.ios.deployment_target = '7.0'
  s.osx.deployment_target = '10.8'
  
  s.source_files = 'Source/**/*.{h,hpp,c,cc,cpp,m,mm,s}', 'Dependencies/protobuf-2.0.3/src/*'
  s.exclude_files = '**/*Tests.*', '**/*_test_*', '**/*TestCase.*', '**/*test.*', '**/*main.m'
  s.public_header_files = 'Source/**/*.h', 'Dependencies/**/*.h'
  s.prefix_header_contents = '#import "PLCrashNamespace.h"'
  s.header_mappings_dir = '.'
  s.requires_arc = false
  
  s.resources = 'Resources/*.proto'
  s.preserve_paths = 'Dependencies/**'
  
  s.pod_target_xcconfig = { 
    'GCC_PREPROCESSOR_DEFINITIONS' => 'PLCR_PRIVATE',
  }
  s.xcconfig = {
    'CLANG_ALLOW_NON_MODULAR_INCLUDES_IN_FRAMEWORK_MODULES' => 'YES',
  }
  s.prepare_command = <<-CMD
    cd "Resources" && "../Dependencies/protobuf-2.0.3/bin/protoc-c" --c_out="../Source" "crash_report.proto" && cd ..
    find . \\( -iname '*.h' -o -iname '*.hpp' -o -iname '*.c' -o -iname '*.cc' -o -iname '*.cpp' -o -iname '*.m' -o -iname '*.mm' \\) -exec sed -i '' -e 's/#include <google\\/protobuf-c\\/protobuf-c.h>/#include "..\\/Dependencies\\/protobuf-2.0.3\\/src\\/protobuf-c.h"/g' {} \\;
    find . \\( -iname '*.h' -o -iname '*.hpp' -o -iname '*.c' -o -iname '*.cc' -o -iname '*.cpp' -o -iname '*.m' -o -iname '*.mm' \\) -exec sed -i '' -e 's/#import "CrashReporter\\/CrashReporter.h"/#import "CrashReporter.h"/g' {} \\;

    SDKROOT=$(xcrun --sdk macosx --show-sdk-path)
    mig -arch "i386" -header "Source/mach_exc_i386.h" -server /dev/null -user "Source/mach_exc_i386User.inc" "${SDKROOT}/usr/include/mach/mach_exc.defs"
    mig -arch "x86_64" -header "Source/mach_exc_x86_64.h" -server /dev/null -user "Source/mach_exc_x86_64User.inc" "${SDKROOT}/usr/include/mach/mach_exc.defs"

    echo '#ifdef __LP64__'               > Source/mach_exc.h
    echo '#include "mach_exc_x86_64.h"' >> Source/mach_exc.h
    echo '#else'                        >> Source/mach_exc.h
    echo '#include "mach_exc_i386.h"'   >> Source/mach_exc.h
    echo '#endif'                       >> Source/mach_exc.h

    FILE_86=$(cat Source/mach_exc_i386User.inc)
    FILE_64=$(cat Source/mach_exc_x86_64User.inc)
    echo '#ifdef __LP64__'  > Source/mach_exc.c
    echo "$FILE_64"        >> Source/mach_exc.c
    echo '#else'           >> Source/mach_exc.c
    echo "$FILE_86"        >> Source/mach_exc.c
    echo '#endif'          >> Source/mach_exc.c
  CMD

  s.library = 'c++'
end
