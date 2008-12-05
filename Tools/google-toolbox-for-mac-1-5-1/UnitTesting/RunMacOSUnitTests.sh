#
#  RunUnitTests.sh
#  Copyright 2008 Google Inc.
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
#  Run the unit tests in this test bundle.
#  Set up some env variables to make things as likely to crash as possible.
#  See http://developer.apple.com/technotes/tn2004/tn2124.html for details.
#

export MallocScribble=YES
export MallocPreScribble=YES
export MallocGuardEdges=YES
# CFZombieLevel disabled because it doesn't play well with the 
# security framework
# export CFZombieLevel=3
export NSAutoreleaseFreedObjectCheckEnabled=YES
export NSZombieEnabled=YES
export OBJC_DEBUG_FRAGILE_SUPERCLASSES=YES

# If we have debug libraries on the machine, we'll use them
# unless a target has specifically turned them off
if [ ! $GTM_NO_DEBUG_FRAMEWORKS ]; then
  if [ -f "/System/Library/Frameworks/CoreFoundation.framework/Versions/Current/CoreFoundation_debug" ]; then
    echo ---- Using _debug frameworks ----
    export DYLD_IMAGE_SUFFIX=_debug
  fi
fi

"${SYSTEM_DEVELOPER_DIR}/Tools/RunUnitTests"
