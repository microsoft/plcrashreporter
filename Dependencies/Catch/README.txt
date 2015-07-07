catch.hpp
  Description: A single-header C++ unit testing library.
  Version: v1.2.1 downloaded from https://github.com/philsquared/Catch
  License:
        Boost Software License (Liberal)
        The catch code is used by the PLCrashReporter unit tests; it is not incorporated into the
        PLCrashReporter library.
  Modifications:
   - Adding missing CATCH_CONFIG_NO_CPP11 guard on unique_ptr usage.

XCTestRunner.mm
  Description: Xcode integration for catch; registers test cases with XCTest.
  Version: Revision 56a67045d795278d1e29b0fa4792a2024b747b1a fetched from https://github.com/philsquared/Catch/pull/454
  License:
        MIT License
        The test runner code is only used by the PLCrashReporter unit tests; it is not incorporated into the
        PLCrashReporter library.
  Modifications:
   - Replaced direct inclusion of "catch.hpp" with our own "PLCrashCatchTest.hpp" shim.