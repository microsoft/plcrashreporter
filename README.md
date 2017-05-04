#PLCrashReporter

***

#Difference from official version

***Added method to get key string from crash log:***

```
@interface PLCrashReportTextFormatter : NSObject <PLCrashReportFormatter>
...
//Get key string for calc crash report id
+ (NSString *)keyStringForCrashReport:(PLCrashReport *)report
                       withTextFormat:(PLCrashReportTextFormat)textFormat
                               option:(PLCrashReportFormatterKeyStringOption)option;
...
@end
```
***Added method to get finger print of crash log***

```
@interface PLCrashReport : NSObject
...
/**
 Calculate SHA1 string of crash key string to distinguish between different crash logs.
 May use this as primary key to store crash log on server.
 */
- (NSString *)fingerPrint;
- (NSString *)fingerPrintWithOption:(PLCrashReportFingerPrintOption)option;
...
@end
```

***Added method to export crash report to text***

```
@interface PLCrashReport : NSObject
...
/*
 Export crash report to text in apple crash log format.
 */
- (NSString *)exportCrashReportString;
...
@end
```
