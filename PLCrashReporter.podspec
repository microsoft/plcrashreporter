Pod::Spec.new do |spec|
  spec.name       = 'PLCrashReporter'
  spec.version    = '1.4.0'
  spec.summary    = 'Reliable, open-source crash reporting for iOS, macOS and tvOS.'

  spec.homepage   = 'https://github.com/microsoft/plcrashreporter'
  spec.license    = { :type => 'MIT', :file => 'LICENSE.txt' }
  spec.authors    = { 'Plausible Labs Cooperative, Inc.'  => 'contact@plausible.coop',
                      'Microsoft'                         => 'appcentersdk@microsoft.com' }

  spec.source     = { :http     => "https://github.com/microsoft/plcrashreporter/releases/download/#{spec.version}/PLCrashReporter-#{spec.version}.zip",
                      :flatten  => true }

  spec.ios.deployment_target    = '8.0'
  spec.ios.vendored_frameworks  = "iOS Framework/CrashReporter.framework"

  spec.osx.deployment_target    = '10.7'
  spec.osx.vendored_frameworks  = "Mac OS X Framework/CrashReporter.framework"
  
  spec.tvos.deployment_target   = '9.0'
  spec.tvos.vendored_frameworks = "tvOS Framework/CrashReporter.framework"
end
