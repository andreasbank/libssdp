<?php
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *	Copyright (C) 2015 by Andreas Bank, andreas.mikael.bank@gmail.com
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

$htmlHeader = "<!DOCTYPE html>\n<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"sv\" xml:lang=\"sv\">\n" .
				"<head>\n\t<title>Uninstall the ABUSED DB</title>\n".
				"\t<style type=\"text/css\">\n\ttd {\n\t\ttext-align:left;\n\t}\n\t</style>\n</head>\n<body style=\"text-align: center;\">\n";
$htmlFooter = "</body>\n</html>";

if(isset($_POST['clearMysql'])) {
	$errorMessage = "\nan error occurred:<br />\n";
	if(@mysql_connect($_POST['mysqlHost'], $_POST['mysqlUsername'], $_POST['mysqlPassword']) == false)
		echo $htmlHeader.$errorMessage." (connect) ".mysql_error()."\n".$htmlFooter;
	else if(@mysql_query("drop database `".$_POST['mysqlDatabase']."`") == false)
		echo $htmlHeader.$errorMessage."(drop database) ".mysql_error()."\n".$htmlFooter;
	else {
		echo "<span style=\"color:green;\">Success! All data has been removed!</span>";
		mysql_close();
	}
}
else {
	echo $htmlHeader;
?>
	<p>
		Welcome to the uninstallation of the ABUSED DB!<br />
		The uninstallation requires read/write privilegies<br />
		to the MySQL server.<br />
		This process will remove the ABUSED database and all its data.<br />
		<span style="font-style: italic; color: red;"><b>WARNING:</b><br />
		This process cannot be undone!</span>
	</p>
	<h1>Step 1 - MySQL information:</h1>
	<form method="post" action="uninstall.php">
	<table style="margin: auto; border: solid 1px black;">
		<tr>
			<td>MySQL address:</td>
			<td><input type="input" id="h" name="mysqlHost" value="localhost" /></td>
		</tr>
		<tr>
			<td>MySQL username:</td>
			<td><input type="input" id="u" name="mysqlUsername" value="root" /></td>
		</tr>
		<tr>
			<td>MySQL password:</td>
			<td><input type="password" id="p" name="mysqlPassword" value="" /></td>
		</tr>
		<tr>
			<td>Databasens name:</td>
			<td><input type="input" id="p" name="mysqlDatabase" value="abused" /></td>
		</tr>
		<tr>
			<td colspan="2" style="text-align: center;">
				<input type="submit" name="clearMysql" value="Uninstall" />
			</td>
		</tr>
	</table>
	</form>
<?php
	echo $htmlFooter;
}
?>
