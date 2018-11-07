//
//  ViewController.swift
//  SampleApp
//
//  Created by KangYongseok on 07/11/2018.
//  Copyright © 2018 Yongseok Kang. All rights reserved.
//

import UIKit
import CrashReporter
import os.log

class ViewController: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        
        os_log("crash report create", log: OSLog.default, type: .debug)
    }

    
}

