/*
 * Author: Andreas Linde <mail@andreaslinde.de>
 *
 * Copyright (c) 2009 Andreas Linde. All rights reserved.
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


#import <CrashReporter/CrashReporter.h>
#import "StartupViewController.h"
#import "CrashReporterDemoAppDelegate.h"

#define USER_AGENT @"CrashReporterDemo/1.3.0"

#define CRASH_URL @"http://www.yourdomainname.com/crash.php"

#define CRASH_TYPE_UNKNOWN 0							// no idea what this bug is about
#define CRASH_TYPE_NEW 1									// the bug is new, maybe we need that flag one day
#define CRASH_TYPE_FIXED_NEXTRELEASE 2		// the bug is fixed and the bugfix will be available in the next release
#define CRASH_TYPE_FIXED_SENDTOAPPLE 3		// the bug is fixed and the new release already has been sent to apple for approval
#define CRASH_TYPE_FIXED_AVAILABLE 4			// the bug is fixed and a new version is available, before showing the message check if the user already has updated!


@interface StartupViewController (private)

- (void) askSendCrash;
- (void) sendCrashData;
- (void) finishSendCrashData;

- (void) showCrashStatusMessage:(int)type;

- (NSMutableURLRequest *)requestWithURL:(NSURL *)theURL;

- (int) postXML:(NSString*)xml toURL:(NSURL*)url;
- (void) parseXMLFileAtURL:(NSString *)url parseError:(NSError **)error;
@end

@implementation StartupViewController

@synthesize hudView;
@synthesize label;
@synthesize loadingIndicator;
@synthesize hudBox;

@synthesize _crashLogAppVersion;

- (void)viewDidLoad
{
	_memoryCrash = NO;
	_crashIdenticalCurrentVersion = YES;
	_crashLogAppVersion = @"";
	
	_alertType = 10;
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning]; // Releases the view if it doesn't have a superview
    // Release anything that's not essential, such as cached data
}


- (void)dealloc {
    [super dealloc];
}


- (void)showHUD:(NSString *)title
{
	[self.label setText: title];
	[self.loadingIndicator startAnimating];
	[self.hudView setHidden:NO];
}


- (void)removeHUD
{
	[self.hudView setHidden:YES];
	[self.loadingIndicator stopAnimating];
}


#pragma mark CrashLog

- (void) startCrashData
{
	CrashReporterDemoAppDelegate *appDelegate = (CrashReporterDemoAppDelegate *)[[UIApplication sharedApplication] delegate];
	if (![appDelegate isAutoSendCrashData])
	{
		[self askSendCrash];
	} else {
		[self sendCrashData];
	}	
}

- (void) askSendCrash
{
	_alertType = 10;
	_alertView = [[UIAlertView alloc] initWithTitle:NSLocalizedString(@"CrashDataFoundTitle", @"Title showing in the alert box when crash report data has been found")
																											message:NSLocalizedString(@"CrashDataFoundDescription", @"Description explaining that crash data has been found and ask the user if the data might be uplaoded to the developers server")
																										 delegate:self
																						cancelButtonTitle:NSLocalizedString(@"No", @"")
																						otherButtonTitles:NSLocalizedString(@"Yes", @""), 
																															NSLocalizedString(@"Always", @""), nil];
	
	[_alertView show];
	[_alertView release];	
}


- (NSString *) convert
{
	CrashReporterDemoAppDelegate *appDelegate = (CrashReporterDemoAppDelegate *)[[UIApplication sharedApplication] delegate];
	NSString *xmlString = @"";
	
	NSError *error;

	// Try loading the crash report
	NSData *data  = [NSData dataWithData:[appDelegate getCrashData]];
	if (data == nil) {
		NSLog(@"Could not load crash report: %@", error);
		goto finish;
	}
	
	/* Decode it */
	PLCrashReport *crashLog = [[PLCrashReport alloc] initWithData: data error: &error];
	if (crashLog == nil) {
		goto finish;
	}
	
	/* Header */
	
	/* Map to apple style OS nane */
	const char *osName;
	switch (crashLog.systemInfo.operatingSystem) {
		case PLCrashReportOperatingSystemiPhoneOS:
			osName = "iPhone OS";
			break;
		case PLCrashReportOperatingSystemiPhoneSimulator:
			osName = "Mac OS X";
			break;
		default:
			osName = "iPhone OS";
			break;
	}
	
	/* Map to Apple-style code type */
	const char *codeType;
	switch (crashLog.systemInfo.architecture) {
		case PLCrashReportArchitectureARM:
			codeType = "ARM";
			break;
		default:
			codeType = "ARM";
			break;
	}
	
	NSString *currentVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
	_crashLogAppVersion = [NSString stringWithFormat:@"%@", crashLog.applicationInfo.applicationVersion];
	if ([currentVersion compare:crashLog.applicationInfo.applicationVersion] != NSOrderedSame)
	{
		_crashIdenticalCurrentVersion = NO;
	}
	
	xmlString = [NSString stringWithFormat:@"%@Incident Identifier: [TODO]\n", xmlString];
	xmlString = [NSString stringWithFormat:@"%@CrashReporter Key:   [TODO]\n", xmlString];
	xmlString = [NSString stringWithFormat:@"%@Process:         [TODO]\n", xmlString];
	xmlString = [NSString stringWithFormat:@"%@Path:            [TODO]\n", xmlString];
	xmlString = [NSString stringWithFormat:@"%@Identifier:      %s\n", xmlString, [crashLog.applicationInfo.applicationIdentifier UTF8String]];
	xmlString = [NSString stringWithFormat:@"%@Version:         %s\n", xmlString, [crashLog.applicationInfo.applicationVersion UTF8String]];
	xmlString = [NSString stringWithFormat:@"%@Code Type:       %s\n", xmlString, codeType];
	xmlString = [NSString stringWithFormat:@"%@Parent Process:  [TODO]\n", xmlString];
	
	xmlString = [NSString stringWithFormat:@"%@\n", xmlString];
	
	/* System info */
	xmlString = [NSString stringWithFormat:@"%@Date/Time:       %s\n", xmlString, [[crashLog.systemInfo.timestamp description] UTF8String]];
	xmlString = [NSString stringWithFormat:@"%@OS Version:      %s %s\n", xmlString, osName, [crashLog.systemInfo.operatingSystemVersion UTF8String]];
	xmlString = [NSString stringWithFormat:@"%@Report Version:  103\n", xmlString];
	
	xmlString = [NSString stringWithFormat:@"%@\n", xmlString];
	
	/* Exception code */
	xmlString = [NSString stringWithFormat:@"%@Exception Type:  %s\n", xmlString, [crashLog.signalInfo.name UTF8String]];
	xmlString = [NSString stringWithFormat:@"%@Exception Codes: %s at 0x%" PRIx64 "\n", xmlString, [crashLog.signalInfo.code UTF8String], crashLog.signalInfo.address];
	
	for (PLCrashReportThreadInfo *thread in crashLog.threads) {
		if (thread.crashed) {
			xmlString = [NSString stringWithFormat:@"%@Crashed Thread:  %d\n", xmlString, thread.threadNumber];
			break;
		}
	}
	
	xmlString = [NSString stringWithFormat:@"%@\n", xmlString];
	
	/* Threads */
	for (PLCrashReportThreadInfo *thread in crashLog.threads) {
		if (thread.crashed)
			xmlString = [NSString stringWithFormat:@"%@Thread %d Crashed:\n", xmlString, thread.threadNumber];
		else
			xmlString = [NSString stringWithFormat:@"%@Thread %d:\n", xmlString, thread.threadNumber];
		for (NSUInteger frame_idx = 0; frame_idx < [thread.stackFrames count]; frame_idx++) {
			PLCrashReportStackFrameInfo *frameInfo = [thread.stackFrames objectAtIndex: frame_idx];
			PLCrashReportBinaryImageInfo *imageInfo;
			
			/* Base image address containing instrumention pointer, offset of the IP from that base
			 * address, and the associated image name */
			uint64_t baseAddress = 0x0;
			uint64_t pcOffset = 0x0;
			const char *imageName = "\?\?\?";
			
			imageInfo = [crashLog imageForAddress: frameInfo.instructionPointer];
			if (imageInfo != nil) {
				imageName = [[imageInfo.imageName lastPathComponent] UTF8String];
				baseAddress = imageInfo.imageBaseAddress;
				pcOffset = frameInfo.instructionPointer - imageInfo.imageBaseAddress;
			}
			
			xmlString = [NSString stringWithFormat:@"%@%-4d%-36s0x%08" PRIx64 " 0x%" PRIx64 " + %" PRId64 "\n", xmlString, 
									 frame_idx, imageName, frameInfo.instructionPointer, baseAddress, pcOffset];
		}
		xmlString = [NSString stringWithFormat:@"%@\n", xmlString];
	}
	
	/* Images */
	xmlString = [NSString stringWithFormat:@"%@Binary Images:\n", xmlString];
	for (PLCrashReportBinaryImageInfo *imageInfo in crashLog.images) {
		NSString *uuid;
		/* Fetch the UUID if it exists */
		if (imageInfo.hasImageUUID)
			uuid = imageInfo.imageUUID;
		else
			uuid = @"???";
    
		/* base_address - terminating_address file_name identifier (<version>) <uuid> file_path */
		xmlString = [NSString stringWithFormat:@"%@0x%" PRIx64 " - 0x%" PRIx64 "  %s \?\?\? (\?\?\?) <%s> %s\n", xmlString,
								 imageInfo.imageBaseAddress,
								 imageInfo.imageBaseAddress + imageInfo.imageSize,
								 [[imageInfo.imageName lastPathComponent] UTF8String],
								 [uuid UTF8String],
								 [imageInfo.imageName UTF8String]];
	}
	
finish:
	if ([xmlString length] == 0)
	{
		_memoryCrash = YES;
		xmlString = [NSString stringWithFormat:@"Memory Warning!"];
	}
	
	return xmlString;
}


- (void) sendCrashData
{
	CrashReporterDemoAppDelegate *appDelegate = (CrashReporterDemoAppDelegate *)[[UIApplication sharedApplication] delegate];

	NSString *device = @"";
	NSString *contact = @"";
		
	NSString *log = [self convert];
	
	NSString *xml = [NSString stringWithFormat:@"<crashlog><version>%@</version><crashappversion>%@</crashappversion><startmemory>%u</startmemory><endmemory>%u</endmemory><contact>%@</contact><log><![CDATA[%@]]></log></crashlog>", 
									 [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"],
									 _crashLogAppVersion,
									 [appDelegate getLastStartupFreeMemory], 
									 [appDelegate getLastShutdownFreeMemory], 
									 device, 
									 contact, 
									 log];
	int result = [self postXML:xml toURL:[NSURL URLWithString:CRASH_URL]];	
	
	[self removeHUD];

	if (result < CRASH_TYPE_FIXED_NEXTRELEASE || _memoryCrash || !_crashIdenticalCurrentVersion)
	{
		[appDelegate finishCrashLog];
	} else if (!_memoryCrash && _crashIdenticalCurrentVersion) {
		// show the user some more info about the crash he/she had
		[self showCrashStatusMessage:result];
	}
}


- (void) showCrashStatusMessage:(int)type
{
	// the bug has already been fixed and it will be included in the next release
	if (type == CRASH_TYPE_FIXED_NEXTRELEASE)
	{
		_alertType = 20;
		_alertView = nil;
		_alertView = [[UIAlertView alloc] initWithTitle:NSLocalizedString(@"CrashResponseTitle", @"Title for the alertview giving feedback about the crash")
																						message:NSLocalizedString(@"CrashResponseNextRelease", @"Full text telling the bug is fixed and will be available in the next release")
																					 delegate:self
																	cancelButtonTitle:NSLocalizedString(@"Ok", @"")
																	otherButtonTitles:nil
									];
		[_alertView show];
		[_alertView release];	
	} else if (type == CRASH_TYPE_FIXED_SENDTOAPPLE) {
		// the bugfix release has been send to apple
		_alertType = 21;
		_alertView = nil;
		_alertView = [[UIAlertView alloc] initWithTitle:NSLocalizedString(@"CrashResponseTitle", @"Title for the alertview giving feedback about the crash")
																						message:NSLocalizedString(@"CrashResponseWaitingApple", @"Full text telling the bug is fixed and the new release is waiting at Apple")
																					 delegate:self
																	cancelButtonTitle:NSLocalizedString(@"Ok", @"")
																	otherButtonTitles:nil
									];
		[_alertView show];
		[_alertView release];	
	} else if (type == CRASH_TYPE_FIXED_AVAILABLE) {
		// the bugfix release is available
		_alertType = 22;
		_alertView = nil;
		_alertView = [[UIAlertView alloc] initWithTitle:NSLocalizedString(@"CrashResponseTitle", @"Title for the alertview giving feedback about the crash")
																						message:NSLocalizedString(@"CrashResponseAvailable", @"Full text telling the bug is fixed and an update is available in the AppStore")
																					 delegate:self
																	cancelButtonTitle:NSLocalizedString(@"Ok", @"")
																	otherButtonTitles:nil
									];
		[_alertView show];
		[_alertView release];	
	}
}


#pragma mark postxml

- (NSMutableURLRequest *)requestWithURL:(NSURL *)theURL
{ 
	NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:theURL]; 
	[req setCachePolicy: NSURLRequestReloadIgnoringLocalCacheData];
	[req setValue:USER_AGENT forHTTPHeaderField:@"User-Agent"];
	[req setTimeoutInterval: 15];
	return req; 
} 

- (NSString *)getBoundary
{
	return @"----FOO";
}


- (int)postXML:(NSString*)xml toURL:(NSURL*)url
{
	NSMutableURLRequest * request = [NSMutableURLRequest requestWithURL:url];
	
	[request setCachePolicy: NSURLRequestReloadIgnoringLocalCacheData];
	[request setValue:USER_AGENT forHTTPHeaderField:@"User-Agent"];
	[request setTimeoutInterval: 15];
	[request setHTTPMethod:@"POST"];
	NSString *contentType = [NSString stringWithFormat:@"multipart/form-data, boundary=%@", [self getBoundary]];
	[request setValue:contentType forHTTPHeaderField:@"Content-type"];
	
	NSMutableData *postBody =  [NSMutableData data];
	[postBody appendData:[[NSString stringWithFormat:@"--%@\r\n", [self getBoundary]] dataUsingEncoding:NSUTF8StringEncoding]];
	[postBody appendData:[@"Content-Disposition: form-data; name=\"xmlstring\"\r\n\r\n" dataUsingEncoding:NSUTF8StringEncoding]];
	[postBody appendData:[xml dataUsingEncoding:NSUTF8StringEncoding]];
	[postBody appendData:[[NSString stringWithFormat:@"\r\n--%@\r\n", [self getBoundary]] dataUsingEncoding:NSUTF8StringEncoding]];
	[request setHTTPBody:postBody];
	
	_serverResult = CRASH_TYPE_UNKNOWN;
	
	NSHTTPURLResponse *response;
	NSError *errmsg = NULL;

	NSData *theResponseData = [NSURLConnection sendSynchronousRequest:request returningResponse:&response error:&errmsg];
	int statusCode = [response statusCode];
	
	if (statusCode >= 200 && statusCode < 400)
	{
		//	NSLog(@"%@",url);
		NSXMLParser *parser = [[NSXMLParser alloc] initWithData:theResponseData];
		// Set self as the delegate of the parser so that it will receive the parser delegate methods callbacks.
		[parser setDelegate:self];
		// Depending on the XML document you're parsing, you may want to enable these features of NSXMLParser.
		[parser setShouldProcessNamespaces:NO];
		[parser setShouldReportNamespacePrefixes:NO];
		[parser setShouldResolveExternalEntities:NO];
		
		[parser parse];
		
		[parser release];		
	}
	
	return _serverResult;
}


#pragma mark UIAlertViewDelegate

- (void)alertView:(UIAlertView *)alertView willDismissWithButtonIndex:(NSInteger)buttonIndex
{
	CrashReporterDemoAppDelegate *appDelegate = (CrashReporterDemoAppDelegate *)[[UIApplication sharedApplication] delegate];
	switch (_alertType)
	{
		case 10:
			if (buttonIndex == 1)
			{
				[self showHUD:NSLocalizedString(@"Sending", @"Text showing in a processing box that the crash data is being uploaded to the server")];
			}	else if (buttonIndex == 2) {
				[appDelegate setAutoSendCrashData:YES];
				[self showHUD:NSLocalizedString(@"Sending", @"Text showing in a processing box that the crash data is being uploaded to the server")];
			}			
			break;
	}	
}

// invoke the selected action from the actionsheet for a location element
- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex
{
	CrashReporterDemoAppDelegate *appDelegate = (CrashReporterDemoAppDelegate *)[[UIApplication sharedApplication] delegate];
	switch (_alertType)
	{
		case 10:
			if (buttonIndex == 1 || buttonIndex == 2)
			{
				[self sendCrashData];
			} else {
				[appDelegate finishCrashLog];
			}
			break;
		case 20:
		case 21:
		case 22:
			[appDelegate finishCrashLog];
			break;			
	}	
}


#pragma mark NSXMLParser

- (void)parserDidStartDocument:(NSXMLParser *)parser
{
}


- (void)parserDidEndDocument:(NSXMLParser *)parser
{
}


- (void)parseXMLFileAtURL:(NSString *)url parseError:(NSError **)error
{	
	NSXMLParser *parser = [[NSXMLParser alloc] initWithContentsOfURL:[NSURL URLWithString:url]];
	// Set self as the delegate of the parser so that it will receive the parser delegate methods callbacks.
	[parser setDelegate:self];
	// Depending on the XML document you're parsing, you may want to enable these features of NSXMLParser.
	[parser setShouldProcessNamespaces:NO];
	[parser setShouldReportNamespacePrefixes:NO];
	[parser setShouldResolveExternalEntities:NO];
	
	[parser parse];
	
	NSError *parseError = [parser parserError];
	if (parseError && error) {
		*error = parseError;
	}
	
	[parser release];
}


- (void)parser:(NSXMLParser *)parser didStartElement:(NSString *)elementName namespaceURI:(NSString *)namespaceURI qualifiedName:(NSString *)qName attributes:(NSDictionary *)attributeDict
{
	if (qName)
	{
		elementName = qName;
	}
	
	if ([elementName isEqualToString:@"result"]) {
		_contentOfProperty = [NSMutableString string];
	}
}


- (void)parser:(NSXMLParser *)parser didEndElement:(NSString *)elementName namespaceURI:(NSString *)namespaceURI qualifiedName:(NSString *)qName
{     
	if (qName)
	{
		elementName = qName;
	}
	
	if ([elementName isEqualToString:@"result"]) {
		_serverResult = [_contentOfProperty intValue];
	}
}


- (void)parser:(NSXMLParser *)parser foundCharacters:(NSString *)string
{
	if (_contentOfProperty)
	{
		// If the current element is one whose content we care about, append 'string'
		// to the property that holds the content of the current element.
		if (string != nil)
		{
			[_contentOfProperty appendString:string];
		}
	}
}


@end
