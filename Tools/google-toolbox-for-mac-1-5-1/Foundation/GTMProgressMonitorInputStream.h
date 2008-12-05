//
//  GTMProgressMonitorInputStream.m
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

#import <Foundation/Foundation.h>

// The monitored input stream calls back into the monitor delegate
// with the number of bytes and total size
//
// - (void)inputStream:(GTMProgressMonitorInputStream *)stream 
//   hasDeliveredByteCount:(unsigned long long)numberOfBytesRead 
//        ofTotalByteCount:(unsigned long long)dataLength;

@interface GTMProgressMonitorInputStream : NSInputStream {
  
  NSInputStream *inputStream_; // encapsulated stream that does the work
  
  unsigned long long dataSize_;     // size of data in the source 
  unsigned long long numBytesRead_; // bytes read from the input stream so far
  
  __weak id monitorDelegate_;    // WEAK, not retained
  SEL monitorSelector_;  
  
  __weak id monitorSource_;     // WEAK, not retained
}

// Length is passed to the progress callback; it may be zero if the progress
// callback can handle that (mainly meant so the monitor delegate can update the
// bounds/position for a progress indicator.
+ (id)inputStreamWithStream:(NSInputStream *)input 
                     length:(unsigned long long)length;

- (id)initWithStream:(NSInputStream *)input 
              length:(unsigned long long)length;

// The monitor is called when bytes have been read
//
// monitorDelegate should respond to a selector with a signature matching:
//
// - (void)inputStream:(GTMProgressMonitorInputStream *)stream 
//   hasDeliveredBytes:(unsigned long long)numReadSoFar
//        ofTotalBytes:(unsigned long long)total
//
// |total| will be the length passed when this GTMProgressMonitorInputStream was
// created.

- (void)setMonitorDelegate:(id)monitorDelegate // not retained
                  selector:(SEL)monitorSelector;
- (id)monitorDelegate;
- (SEL)monitorSelector;

// The source argument lets the delegate know the source of this input stream.
// this class does nothing w/ this, it's just here to provide context to your
// monitorDelegate.
- (void)setMonitorSource:(id)source; // not retained
- (id)monitorSource;

@end

