<?php
/**
 * Configure MySQL with:
 * CREATE USER 'abused'@'%' IDENTIFIED BY 'abusedpass';
 * GRANT SELECT, INSERT, UPDATE ON devicemanagement.devices TO 'abused'@'%';
 * GRANT SELECT, INSERT, UPDATE ON devicemanagement.address TO 'abused'@'%';
 */

$host     = 'localhost';
$database = 'devicemanagement';
$username = 'abused';
$password = 'abusedpass';
$query = 'SELECT * FROM `devices` d LEFT OUTER JOIN `address` ON d.macaddress=address.macaddress';

$h_sql = new mysqli($host, $username, $password, $database);
if ($h_sql->connect_errno) {
  printf("Failed to connect to MySQL: %s", $h_sql->connect_error);
  exit(1);
}

$res = $h_sql->query($query);
if(false === $res) {
  printf("Failed to query MySQL: %s", $h_sql->error);
  exit(1);
}

$results = array();

while($row = $res->fetch_assoc()) {
  $results[] = $row;
}
echo "<pre>\n";
var_dump($results);
echo "</pre>\n";
