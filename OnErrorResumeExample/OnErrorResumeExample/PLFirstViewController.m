//
//  PLFirstViewController.m
//  OnErrorResumeExample
//
//  Created by Landon Fuller on 3/28/14.
//  Copyright (c) 2014 Plausible Labs. All rights reserved.
//

#import "PLFirstViewController.h"

@interface PLFirstViewController ()

@end

@implementation PLFirstViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
}

- (IBAction) crashMe: (id) sender {
    /* NULL dereference! */
//    ((volatile char *) NULL)[0] = 0xFF;
    
    /* CFRelease assertion! */
    CFRelease(NULL);
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
