	/*
	 * Author: Andreas Linde <mail@andreaslinde.de>
	 *
	 * Copyright (c) 2009 Andreas Linde. All rights reserved.
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
	 
These are the main features of this demo project:
- (Automatically) send crash reports to a developers database
- Let the user decide per crash to send data or always
- The user has the option to provide additional information in the settings, like email address for contacting the user
- Give the user immediate feedback if the crash is known and will be fixed in the next update, or if the update is already waiting at Apple for approval, or if the update is already available to install

These are the main features on backend side for the developer:
- Maintain app versions and their status in a table (upcoming, in approval, available, ...)
- Maintain crash reports and sort them by using simple patterns. Automatically know how many times a bug has occured and easily filter the new ones in the DB
- Assign bugfix versions for each bug pattern

Server side files:
- database.txt contains all the default tables
- crash.php is the file that is invoked by the iPhone app
- crash_update.php has to be invoked once a new pattern has been added, so the non-checked bug reports are processed and assigned to the new pattern if they match

Feel free to add enhancements, maybe even a web front-end for managing the data in the database and provide them back to the community!

Thanks
Andreas Linde
http://www.andreaslinde.com/
http://www.buzzworks.de/