/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008-2009 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#import "SenTestCompat.h"

#import "PLCrashReport.h"
#import "PLCrashReporter.h"
#import "PLCrashFrameWalker.h"
#import "PLCrashTestThread.h"

@interface PLCrashReporterTests : SenTestCase
@end

@implementation PLCrashReporterTests

- (void) testSingleton {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
    STAssertNotNil([PLCrashReporter sharedReporter], @"Returned nil singleton instance");
    STAssertTrue([PLCrashReporter sharedReporter] == [PLCrashReporter sharedReporter], @"Crash reporter did not return singleton instance");
#pragma clang diagnostic pop
}

/**
 * Test generation of a 'live' crash report for a specific thread.
 */
- (void) testGenerateLiveReportWithThread {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    //plcrash_test_thread_stop(&thr);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


- (void) testGenerateLiveReportWithThread1 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread2 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


- (void) testGenerateLiveReportWithThread3 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread4 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread5 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread6 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


/** */

- (void) testGenerateLiveReportWithThread11 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread21 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


- (void) testGenerateLiveReportWithThread31 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread41 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread51 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread61 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

/** */

- (void) testGenerateLiveReportWithThread111 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread211 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


- (void) testGenerateLiveReportWithThread311 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread411 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread511 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread611 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

/***/

- (void) testGenerateLiveReportWithThread1111 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    ////plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    ////plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread22 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    //plcrash_test_thread_stop(&thr);
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


- (void) testGenerateLiveReportWithThread32 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread42 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread52 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread62 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread47 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


- (void) testGenerateLiveReportWithThread417 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread427 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


- (void) testGenerateLiveReportWithThread43 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread44 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread45 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread46 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


/** */

- (void) testGenerateLiveReportWithThread4117 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread421 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


- (void) testGenerateLiveReportWithThread431 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread441 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread451 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread461 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

/** */

- (void) testGenerateLiveReportWithThread4111 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread4211 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


- (void) testGenerateLiveReportWithThread4311 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread4411 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread4511 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread4611 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

/***/

- (void) testGenerateLiveReportWithThread41111 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread422 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}


- (void) testGenerateLiveReportWithThread432 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread442 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread452 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

- (void) testGenerateLiveReportWithThread462 {
    NSError *error;
    NSData *reportData;
    plcrash_test_thread_t thr;

    /* Spawn a thread and generate a report for it */
    //plcrash_test_thread_spawn(&thr);
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    reportData = [reporter generateLiveReportWithThread: pthread_mach_thread_np(thr.thread)
                                                  error: &error];
    //plcrash_test_thread_stop(&thr);
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    NSLog(@"Data length : %lu", reportData.length);
    
    STAssertTrue(reportData.length > 0, @"lengh should be > 0");

    /* Try parsing the result */
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    /* Sanity check the signal info */
    if (report) {
        STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
        STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
    }
}

/**
 * Test generation of a 'live' crash report.
 */
- (void) testGenerateLiveReport {
    NSError *error;
    PLCrashReporter *reporter = [[PLCrashReporter alloc] initWithConfiguration: [PLCrashReporterConfig defaultConfiguration]];
    NSData *reportData = [reporter generateLiveReportAndReturnError: &error];
    STAssertNotNil(reportData, @"Failed to generate live report: %@", error);
    
    PLCrashReport *report = [[PLCrashReport alloc] initWithData: reportData error: &error];
    STAssertNotNil(report, @"Could not parse geneated live report: %@", error);

    STAssertEqualStrings([[report signalInfo] name], @"SIGTRAP", @"Incorrect signal name");
    STAssertEqualStrings([[report signalInfo] code], @"TRAP_TRACE", @"Incorrect signal code");
}

@end
