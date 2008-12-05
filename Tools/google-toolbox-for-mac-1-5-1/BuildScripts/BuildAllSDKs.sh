#!/bin/sh
# BuildAllSDKs.sh
#
# This script builds both the Tiger and Leopard versions of the requested
# target in the current basic config (debug, release, debug-gcov). This script
# should be run from the same directory as the GTM Xcode project file.
#
# Copyright 2006-2008 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy
# of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.

PROJECT_TARGET="$1"

XCODEBUILD="${DEVELOPER_BIN_DIR}/xcodebuild"
REQUESTED_BUILD_STYLE=$(echo "${BUILD_STYLE}" | sed "s/.*OrLater-\(.*\)/\1/")
# See if we were told to clean instead of build.
PROJECT_ACTION="build"
if [ "${ACTION}" == "clean" ]; then
  PROJECT_ACTION="clean"
fi

# helper for doing a build
function doIt {
  local myProject=$1
  local myTarget=$2
  local myConfig=$3
  echo "note: Starting ${PROJECT_ACTION} of ${myTarget} from ${myProject} in ${myConfig}"
  ${XCODEBUILD} -project "${myProject}" \
    -target "${myTarget}" \
    -configuration "${myConfig}" \
    "${PROJECT_ACTION}"
  buildResult=$?
  if [ $buildResult -ne 0 ]; then
    echo "Error: ** ${PROJECT_ACTION} Failed **"
    exit $buildResult
  fi
  echo "note: Done ${PROJECT_ACTION}"
}

# now build tiger and then leopard
doIt GTM.xcodeproj "${PROJECT_TARGET}" "TigerOrLater-${REQUESTED_BUILD_STYLE}"
doIt GTM.xcodeproj "${PROJECT_TARGET}" "LeopardOrLater-${REQUESTED_BUILD_STYLE}"

# TODO(iphone if right tool chain?)
