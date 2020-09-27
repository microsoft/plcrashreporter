
Pod::Spec.new do |s|

  s.name         = "Backtrace-PLCrashReporter"
  s.version      = "1.5.4"
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

  s.ios.deployment_target =   "10.0"
  s.tvos.deployment_target =  "10.0"
  s.osx.deployment_target =   "10.10"

  s.source       = { :git => "https://github.com/backtrace-labs/plcrashreporter.git", :tag => "#{s.version}" }

  s.source_files  = "Sources/**/*.{h,hpp,c,cpp,m,mm,s}",
                    "Dependencies/protobuf-c/protobuf-c/*.{h,c}"
  
  s.public_header_files = "Sources/include"
  s.preserve_paths = "Dependencies/**"

  s.pod_target_xcconfig = {
    "GCC_PREPROCESSOR_DEFINITIONS" => "PLCR_PRIVATE PLCF_RELEASE_BUILD"
  }
  s.libraries = "c++"
  s.requires_arc = false

  s.prefix_header_contents = '#import "PLCrashNamespace.h"'
end
