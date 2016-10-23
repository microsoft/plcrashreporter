Pod::Spec.new do |s|
  s.name             = 'PLCrashReporter-DynamicFramework'
  s.version          = '1.3.0.0'
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
  #s.osx.deployment_target = '10.8'
  
  #s.source_files = '${SDKROOT}/usr/include/mach/mach_exc.defs' # osx needed
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
  CMD

  #mig -arch "${CURRENT_ARCH}" -header "${DERIVED_FILE_DIR}/${CURRENT_ARCH}/mach_exc.h" -server /dev/null -user "${DERIVED_FILE_DIR}/${CURRENT_ARCH}/${INPUT_FILE_BASE}User.c" "${SDKROOT}/usr/include/mach/mach_exc.defs"

  s.library = 'c++'
  #s.osx.library = 'macho'
end
