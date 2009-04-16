<?php

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
	
	
$allowed_args = ',xmlstring,';

$server = 'hostname';
$loginsql = 'user';
$passsql = 'password';
$base = 'datbase';
$dbcrashtable = 'app_crash';
$dbanalyzetable = 'app_crash_analyzed';
$dbversiontable = 'app_versions';

/* Verbindung aufbauen, auswählen einer Datenbank */
$link = mysql_connect($server, $loginsql, $passsql)
    or die("<?xml version=\"1.0\" encoding=\"UTF-8\"?><result>0</result>");
mysql_select_db($base) or die("<?xml version=\"1.0\" encoding=\"UTF-8\"?><result>0</result>");

foreach(array_keys($_POST) as $k) {
    $temp = ",$k,";
    if(strpos($allowed_args,$temp) !== false) { $$k = $_POST[$k]; }
}
if (!isset($xmlstring)) $xmlstring = "";

// simulate memory problem
//$xmlstring = "<crashlog><version>1.2.2.1</version><crashappversion>1.2.2.1</crashappversion><startmemory>10000</startmemory><endmemory>5000</endmemory><contact>no contact</contact><log><![CDATA[Memory Warning!]]></log></crashlog>";

// simulate any other crash
//$xmlstring = "<crashlog><version>1.2.2.1</version><crashappversion>1.2.2.1</crashappversion><startmemory>10000</startmemory><endmemory>5000</endmemory><contact>no contact</contact><log><![CDATA[0x000b0419 0x1000 + 717849]]></log></crashlog>";

if ($xmlstring == "") die("<?xml version=\"1.0\" encoding=\"UTF-8\"?><result>0</result>");

$reader = new XMLReader();

$reader->XML($xmlstring);

$version = "";
$crashappversion = "";
$log = "";
$contact = "";
$startmemory = "";
$endmemory = "";

// 0	not analyzed
// 1	new bug, dev has to take a look at it
$done = "0";

// 0	no idea what this bug is about
// 1	the bug is new, maybe we need that flag one day
// 2	the bug is fixed and the bugfix will be available in the next release
// 3	the bug is fixed and the new release already has been sent to apple for approval
// 4	the bug is fixed and a new version is available, before showing the message check if the user already has updated!
$bugstatus = 0;

function reading($reader, $tag)
{
  $input = "";
	while ($reader->read())
	{
    if ($reader->nodeType == XMLReader::TEXT
        || $reader->nodeType == XMLReader::CDATA
        || $reader->nodeType == XMLReader::WHITESPACE
        || $reader->nodeType == XMLReader::SIGNIFICANT_WHITESPACE)
    {
      $input .= $reader->value;
    }
    else if ($reader->nodeType == XMLReader::END_ELEMENT
        && $reader->name == $tag)
    {
      break;
    }
  }
	return $input;
}


define('VALIDATE_NUM',          '0-9');
define('VALIDATE_ALPHA_LOWER',  'a-z');
define('VALIDATE_ALPHA_UPPER',  'A-Z');
define('VALIDATE_ALPHA',        VALIDATE_ALPHA_LOWER . VALIDATE_ALPHA_UPPER);
define('VALIDATE_SPACE',        '\s');
define('VALIDATE_PUNCTUATION',  VALIDATE_SPACE . '\.,;\:&"\'\?\!\(\)');


/**
 * Validate a string using the given format 'format'
 *
 * @param string $string  String to validate
 * @param array  $options Options array where:
 *                          'format' is the format of the string
 *                              Ex:VALIDATE_NUM . VALIDATE_ALPHA (see constants)
 *                          'min_length' minimum length
 *                          'max_length' maximum length
 *
 * @return boolean true if valid string, false if not
 *
 * @access public
 */
function ValidateString($string, $options)
{
	$format     = null;
	$min_length = 0;
	$max_length = 0;
	
	if (is_array($options)) {
		extract($options);
	}
	
	if ($format && !preg_match("|^[$format]*\$|s", $string)) {
		return false;
	}
	
	if ($min_length && strlen($string) < $min_length) {
		return false;
	}
	
	if ($max_length && strlen($string) > $max_length) {
		return false;
	}
	
	return true;
}

while ($reader->read())
{
  if ($reader->name == "crashlog")
  {
		$log = "";
		$version = "";
		$crashappversion = "";
		$contact = "";
		$startmemory = "";
		$endmemory = "";
		$done = "0";
  } else
	if ($reader->name == "crashappversion" && $reader->nodeType == XMLReader::ELEMENT)
	{
  	$crashappversion = reading($reader, "crashappversion");
		if( !ValidateString( $crashappversion, array('format'=>VALIDATE_NUM . VALIDATE_SPACE . VALIDATE_PUNCTUATION) ) ) die("<?xml version=\"1.0\" encoding=\"UTF-8\"?><result>0</result>");
  } else
	if ($reader->name == "version" && $reader->nodeType == XMLReader::ELEMENT)
	{
  	$version = reading($reader, "version");
  } else
	if ($reader->name == "startmemory" && $reader->nodeType == XMLReader::ELEMENT)
	{
  	$startmemory = reading($reader, "startmemory");
  } else
	if ($reader->name == "endmemory" && $reader->nodeType == XMLReader::ELEMENT)
	{
  	$endmemory = reading($reader, "endmemory");
  } else
	if ($reader->name == "contact" && $reader->nodeType == XMLReader::ELEMENT)
	{
  	$contact = reading($reader, "contact");
  } else
	if ($reader->name == "log" && $reader->nodeType == XMLReader::ELEMENT)
	{
  	$log = reading($reader, "log");

  	// last element, now add it to the database
		if ($log != "" && $version != "")
		{
			// check if the crash is because of a memory problem, then go back right away
			$pos = strpos($log, "Memory Warning!");
			if ($pos !== false)
			{
				$bugstatus = 0;
			} else {
				// get all the known bug patterns for the current app version
				$query = "SELECT id, pattern, fix, affected FROM ".$dbanalyzetable." WHERE affected like '%".mysql_real_escape_string($crashappversion)."%'";
				$result = mysql_query($query) or die("<?xml version=\"1.0\" encoding=\"UTF-8\"?><result>0</result>");

				$match = false;
				$matchid = "";

				// search the log file for each pattern
				while ($row = mysql_fetch_row($result))
				{
					$pos = strpos($log, $row[1]);

					// is the pattern found?
					if ($pos !== false)
					{
						// yes, found
						$match = true;
						
						// mark the crashlog as done, so the developer knows the bug isn't new
						$done = "1";
						
						// get the pattern id, to increase the amount of occurrances of this bug
						$matchid = $row[0];
						
						if ($row[2] == $row[3]) {
							// if the fix and affected version string is identical, then it the upcoming version number is not defined, so assume status 2
							$bugstatus = 2;
						} else if ($row[2] != "") {
							// default if the version is not found
							$bugstatus = 0;
							
							// pattern has been found, now look for the status of the fix version
							$query = "SELECT status FROM ".$dbversiontable." WHERE version = '".mysql_real_escape_string($row[2])."'";
							$result2 = mysql_query($query) or die("<?xml version=\"1.0\" encoding=\"UTF-8\"?><result>0</result>");
							
							// get the status
							while ($row2 = mysql_fetch_row($result2))
							{
								// get the bug status defined in the database
								$bugstatus = $row2[0];
							}

							/* Freigeben des Resultsets */
							mysql_free_result($result2);
						} else {
							// default if everything goes wrong, probably the fix version is not yet known, so it is not clear when a fix will come
							$bugstatus = 0;
						}						
					}
				}

				/* Freigeben des Resultsets */
				mysql_free_result($result);
				
				if ($matchid != "")
				{
					// update the amount of bug occurances of the found item
					$query = "UPDATE ".$dbanalyzetable." SET amount=amount+1 WHERE id=".$matchid;
					$result = mysql_query($query) or die("<?xml version=\"1.0\" encoding=\"UTF-8\"?><result>0</result>");
				}
			}
			// if the bug was found in a pattern, then store the id of the pattern, otherwise mark it as not linked via -1 value
			if ($matchid == "")
			{
				$matchid = -1;
			}
			$query = "INSERT INTO ".$dbcrashtable." (contact, version, crashappversion, startmemory, endmemory, log, done, analyzed) values ('".$contact."', '".$version."', '".$crashappversion."', '".$startmemory."', '".$endmemory."', '".$log."', '".$done."', '".$matchid."')";
			$result = mysql_query($query) or die("<?xml version=\"1.0\" encoding=\"UTF-8\"?><result>0</result>");
		}
  }
}
$reader->close();

/* schliessen der Verbinung */
mysql_close($link);

/* Ausgabe der Ergebnisse in XML */
$xw = new xmlWriter();
$xw->openMemory();
$xw->startDocument('1.0','UTF-8');
$xw->writeElement ('result', $bugstatus);
$xw->endElement();
echo $xw->outputMemory(true);
?>
