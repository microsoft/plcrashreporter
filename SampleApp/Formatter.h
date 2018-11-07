//
//  Formatter.h
//  SampleApp
//
//  Created by KangYongseok on 07/11/2018.
//

#import <Foundation/Foundation.h>
@import CrashReporter;

@interface Formatter : NSObject
+ (NSString *)formatStackFrame: (PLCrashReportStackFrameInfo *) frameInfo
                    frameIndex: (NSUInteger) frameIndex
                        report: (PLCrashReport *) report
                          lp64: (BOOL) lp64;
@end
