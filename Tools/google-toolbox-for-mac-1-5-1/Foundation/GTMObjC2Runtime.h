//
//  GTMObjC2Runtime.h
//
//  Copyright 2007-2008 Google Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not
//  use this file except in compliance with the License.  You may obtain a copy
//  of the License at
// 
//  http://www.apache.org/licenses/LICENSE-2.0
// 
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
//  License for the specific language governing permissions and limitations under
//  the License.
//

#import <objc/objc-api.h>
#import "GTMDefines.h"

// These functions exist for code that we want to compile on both the < 10.5 
// sdks and on the >= 10.5 sdks without warnings. It basically reimplements 
// certain parts of the objc2 runtime in terms of the objc1 runtime. It is not 
// a complete implementation as I've only implemented the routines I know we 
// use. Feel free to add more as necessary.
// These functions are not documented because they conform to the documentation
// for the ObjC2 Runtime.

#if OBJC_API_VERSION >= 2  // Only have optional and req'd keywords in ObjC2.
#define AT_OPTIONAL @optional
#define AT_REQUIRED @required
#else
#define AT_OPTIONAL
#define AT_REQUIRED
#endif

// The file objc-runtime.h was moved to runtime.h and in Leopard, objc-runtime.h 
// was just a wrapper around runtime.h. For the iPhone SDK, this objc-runtime.h
// is removed in the iPhoneOS2.0 SDK.
//
// The |Object| class was removed in the iPhone2.0 SDK too.
#if GTM_IPHONE_SDK
#import <objc/runtime.h>
#else
#import <objc/objc-runtime.h>
#import <objc/Object.h>
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
#import "objc/Protocol.h"

Class object_getClass(id obj);
const char *class_getName(Class cls);
BOOL class_conformsToProtocol(Class cls, Protocol *protocol);
Class class_getSuperclass(Class cls);
Method *class_copyMethodList(Class cls, unsigned int *outCount);
SEL method_getName(Method m);
void method_exchangeImplementations(Method m1, Method m2);
IMP method_getImplementation(Method method);
IMP method_setImplementation(Method method, IMP imp);
struct objc_method_description protocol_getMethodDescription(Protocol *p,
                                                             SEL aSel,
                                                             BOOL isRequiredMethod,
                                                             BOOL isInstanceMethod);
#endif  // OBJC2_UNAVAILABLE
