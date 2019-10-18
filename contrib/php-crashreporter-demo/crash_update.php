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
	 
 $server = 'hostname';
$loginsql = 'user';
$passsql = 'password';
$base = 'datbase';
$dbcrashtable = 'app_crash';
$dbanalyzetable = 'app_crash_analyzed';
$dbversiontable = 'app_versions';

/* Verbindung aufbauen, auswählen einer Datenbank */
$link = mysql_connect($server, $loginsql, $passsql)
    or die("Error connecting");
mysql_select_db($base) or die("Error selecting database");

// for each pattern go through all crash reports that are not done and do not have an id in analyzed field
$query = "SELECT id, pattern, affected from ".$dbanalyzetable;
$result = mysql_query($query) or die("Error SQL 1");
echo $query."<br/>";
while ($row = mysql_fetch_row($result))
{
	$pattern_id = $row[0];
	$pattern_string = $row[1];
	$pattern_affected = $row[2];
	
	$query2 = "SELECT id FROM ".$dbcrashtable." WHERE done = 0 and analyzed = 0 and version like '".$pattern_affected."' and log like '%".$pattern_string."%'";
	$result2 = mysql_query($query2) or die("Error SQL 2");
echo $query2."<br/>";
	
	// search the log file for each pattern
	while ($row2 = mysql_fetch_row($result2))
	{
		$crash_id = $row2[0];
		
		// update the amount of bug occurances of the found item
		$query3 = "UPDATE ".$dbanalyzetable." SET amount=amount+1 WHERE id=".$pattern_id;
		$result3 = mysql_query($query3) or die("Error SQL 3");
		echo $query3."<br/>";
		
		// update the amount of bug occurances of the found item
		$query4 = "UPDATE ".$dbcrashtable." SET done=1, analyzed = ".$pattern_id." WHERE id=".$crash_id;
		$result4 = mysql_query($query4) or die("Error SQL 4");

		echo $query4."<br/>";
	}
	
	/* Freigeben des Resultsets */
	mysql_free_result($result2);
}

/* Freigeben des Resultsets */
mysql_free_result($result);

/* schliessen der Verbinung */
mysql_close($link);
?>
