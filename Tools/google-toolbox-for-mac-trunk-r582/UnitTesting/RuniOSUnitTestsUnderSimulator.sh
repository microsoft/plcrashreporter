#!/bin/bash
#  RuniOSUnitTestsUnderSimulator.sh.sh
#  Copyright 2008-2012 Google Inc.
#
#  Licensed under the Apache License, Version 2.0 (the "License"); you may not
#  use this file except in compliance with the License.  You may obtain a copy
#  of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
#  License for the specific language governing permissions and limitations under
#  the License.
#
#  Runs all unittests through the iOS simulator. We don't handle running them
#  on the device. To run on the device just choose "run".

set -o errexit
set -o nounset
# Uncomment the next line to trace execution.
#set -o verbose

# GTM_DEVICE_TYPE -
#   Set to 'iPhone' or 'iPad' to control which simulator is used.
GTM_DEVICE_TYPE=${GTM_DEVICE_TYPE:=iPhone}

# GTM_SIMULATOR_SDK_VERSION
#   Set to something like 5.1 to use a specific SDK version (the needed
#   simulator support must be installed).  Use 'default' to get the dev tools
#   default value.
GTM_SIMULATOR_SDK_VERSION=${GTM_SIMULATOR_SDK_VERSION:=default}

# GTM_SIMULATOR_START_TIMEOUT
#   Controls the simulator startup timeout. Things like machine load, running
#   on a VM, etc.; can cause the startup to take longer.
GTM_SIMULATOR_START_TIMEOUT=${GTM_SIMULATOR_START_TIMEOUT:=60}

# TODO(tvl): Add a variable for the simulator user dir.

# GTM_ENABLE_LEAKS -
#   Set to a non-zero value to turn on the leaks check. You will probably want
#   to disable zombies, otherwise you will get a lot of false positives.

# GTM_DISABLE_TERMINATION
#   Set to a non-zero value so that the app doesn't terminate when it's finished
#   running tests. This is useful when using it with external tools such
#   as Instruments.

# GTM_LEAKS_SYMBOLS_TO_IGNORE
#   List of comma separated symbols that leaks should ignore. Mainly to control
#   leaks in frameworks you don't have control over.
#   Search this file for GTM_LEAKS_SYMBOLS_TO_IGNORE to see examples.
#   Please feel free to add other symbols as you find them but make sure to
#   reference Radars or other bug systems so we can track them.

# GTM_REMOVE_GCOV_DATA
#   Before starting the test, remove any *.gcda files for the current run so
#   you won't get errors when the source file has changed and the data can't
#   be merged.
#
GTM_REMOVE_GCOV_DATA=${GTM_REMOVE_GCOV_DATA:=0}

# GTM_TEST_AFTER_BUILD
#   When set to 1, tests are run only when TEST_AFTER_BUILD is set to "YES".
#   This can be used to have tests run as an after build step when running
#   from the command line, but not when running from within XCode.
GTM_USE_TEST_AFTER_BUILD=${GTM_USE_TEST_AFTER_BUILD:=0}

readonly ScriptDir=$(dirname "$(echo $0 | sed -e "s,^\([^/]\),$(pwd)/\1,")")
readonly ScriptName=$(basename "$0")
readonly ThisScript="${ScriptDir}/${ScriptName}"
readonly SimExecutable="${ScriptDir}/iossim"

# Variables that follow Xcode unittesting conventions
readonly TEST_BUNDLE_PATH="${TEST_BUNDLE_PATH:=${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.${WRAPPER_EXTENSION}}"
TEST_HOST="${TEST_HOST:=}"
XCODE_VERSION_MINOR=${XCODE_VERSION_MINOR:=0000}

# Old Xcode versions don't set TEST_AFTER_BUILD, default it to avoid errors
# for unset variables.
TEST_AFTER_BUILD=${TEST_AFTER_BUILD:=NO}

# Some helpers.

GTMXcodeNote() {
  echo "${ThisScript}:${1}: note: GTM ${2}"
}

GTMXcodeWarning() {
  echo "${ThisScript}:${1}: warning: GTM ${2}"
}

GTMXcodeError() {
  echo "${ThisScript}:${1}: error: GTM ${2}"
}

GTMFakeUnitTestingMsg() {
  echo "${DEVELOPER_DIR}/Tools/RunPlatformUnitTests.include:${1}: ${2}: ${3}"
}

GTMKillSimulator() {
  # If there is something killed, sleep for few seconds to let the simulator
  # spin down so it isn't still seen as running when the next thing tries to
  # launch it.
  /usr/bin/killall "iPhone Simulator" 2> /dev/null && sleep 2 || true
}

# Honor TEST_AFTER_BUILD if requested.
if [[ "$GTM_USE_TEST_AFTER_BUILD" == 1 && "$TEST_AFTER_BUILD" == "NO" ]]; then
  GTMXcodeNote ${LINENO} "Skipping running of unittests since TEST_AFTER_BUILD=NO."
  exit 0
fi

# Only support simulator builds.
if [[ "${PLATFORM_NAME}" != "iphonesimulator" ]]; then
  GTMXcodeNote ${LINENO} "Skipping running of unittests for device build."
  exit 0
fi

# Make sure the iossim executable exists and is executable.
if [[ ! -x "${SimExecutable}" ]]; then
  GTMXcodeError ${LINENO} "Unable to run tests: ${SimExecutable} was not found/executable."
  exit 1
fi

# Make sure the test bundle (and test host) exists.
if [[ ! -e "${TEST_BUNDLE_PATH}" ]]; then
  GTMXcodeError ${LINENO} "Unable to find test bundle: ${TEST_BUNDLE_PATH}"
  exit 1
fi
if [[ -n "${TEST_HOST}" ]]; then
  if [[ ! -e "${TEST_HOST}" ]]; then
    GTMXcodeError ${LINENO} "Unable to find test host: ${TEST_HOST}"
    exit 1
  fi
fi

# Sanity check Xcode version.
if [[ "${XCODE_VERSION_MINOR}" -lt "0430" ]]; then
  GTMXcodeWarning ${LINENO} "Unit testing process not supported on Xcode < 4.3 (${XCODE_VERSION_MINOR})"
fi

# We kill the iPhone Simulator because otherwise we run into issues where
# the unittests fail becuase the simulator is currently running, and
# at this time the iOS SDK won't allow two simulators running at the same
# time.
GTMKillSimulator

if [[ $GTM_REMOVE_GCOV_DATA -ne 0 ]]; then
  if [[ "${OBJECT_FILE_DIR}-${CURRENT_VARIANT}" != "-" ]]; then
    if [[ -d "${OBJECT_FILE_DIR}-${CURRENT_VARIANT}" ]]; then
      GTMXcodeNote ${LINENO} "Removing any .gcda files"
      (cd "${OBJECT_FILE_DIR}-${CURRENT_VARIANT}" && \
          find . -type f -name "*.gcda" -print0 | xargs -0 rm -f )
    fi
  fi
fi

# 6251475 iPhone Simulator leaks @ CFHTTPCookieStore shutdown if
#         CFFIXED_USER_HOME empty
GTM_LEAKS_SYMBOLS_TO_IGNORE="CFHTTPCookieStore"

#
# Build up the command line to run.
#

GTM_TEST_COMMAND=(
  "${SimExecutable}"
    "-d" "${GTM_DEVICE_TYPE}"
    "-t" "${GTM_SIMULATOR_START_TIMEOUT}"
)
if [[ "${GTM_SIMULATOR_SDK_VERSION}" != "default" ]] ; then
  GTM_TEST_COMMAND+=( "-s" "${GTM_SIMULATOR_SDK_VERSION}" )
fi
if [[ -n "${TEST_HOST}" ]]; then
  # When using a test host, it is usually set to the executable within the app
  # bundle, back up one to point at the bundle.
  TEST_HOST_FILENAME=$(basename "${TEST_HOST}")
  TEST_HOST_EXTENSION="${TEST_HOST_FILENAME##*.}"
  if [[ "${TEST_HOST_EXTENSION}" != "app" ]] ; then
    TEST_HOST=$(dirname "${TEST_HOST}")
  fi
  # Yes the DYLD_INSERT_LIBRARIES value below looks odd, that is found from
  # looking at what Xcode sets when it invokes unittests directly.
  GTM_TEST_COMMAND+=(
    "-e" "DYLD_INSERT_LIBRARIES=/../../Library/PrivateFrameworks/IDEBundleInjection.framework/IDEBundleInjection"
    "-e" "XCInjectBundle=${TEST_BUNDLE_PATH}"
    "-e" "XCInjectBundleInto=${TEST_HOST}"
    "${TEST_HOST}"
  )
else
  GTM_TEST_COMMAND+=( "${TEST_BUNDLE_PATH}" )
fi
GTM_TEST_COMMAND+=(
    "-NSTreatUnknownArgumentsAsOpen" "NO"
    "-ApplePersistenceIgnoreState" "YES"
    "-SenTest" "All"
  )
# There was a test host, add the test bundle at the end as an arg to the app.
if [[ -n "${TEST_HOST}" ]]; then
  GTM_TEST_COMMAND+=( "${TEST_BUNDLE_PATH}" )
fi

# These two lines seem to fake out Xcode just enough that its log parser acts
# as though Xcode were running the unit test via the UI. This prevents false
# failures based on lines including "error" and such (which tends to happen in
# raw NSLogs in code).
GTMFakeUnitTestingMsg ${LINENO} "note" "Started tests for architectures 'i386'"
GTMFakeUnitTestingMsg ${LINENO} "note" "Running tests for architecture 'i386' (GC OFF)"

set +e
"${GTM_TEST_COMMAND[@]}"
TEST_HOST_RESULT=$?
set -e

GTMKillSimulator

if [[ ${TEST_HOST_RESULT} -ne 0 ]]; then
  GTMXcodeError ${LINENO} "Tests failed."
  exit 1
fi

exit 0
