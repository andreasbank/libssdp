<?php
$action = NULL;
if(isset($_GET['action']) && !empty($_GET['action'])) {
  $action = $_POST['action'];
}
else if(isset($_GET['action']) && !empty($_GET['action'])) {
  $action = $_GET['action'];
}
else {
  printf("No action specified.");
  exit(0);
}

$host     = 'localhost';
$database = 'abused';
$username = 'abused';
$password = 'abusedpass';
$query    = 'SELECT * FROM `devices`i';

$h_sql = new mysqli($host, $username, $password, $database);
if ($h_sql->connect_errno) {
  printf("Failed to connect to MySQL: %s", $h_sql->connect_error);
  exit(1);
}

switch($action) {

/* Lock the device if possible */
case 'lock_device':
  // TODO: do it.
  //header('HTTP/1.0 200 Ok');
  //header('HTTP/1.0 409 Resource is in use');
  //header('HTTP/1.0 410 Resource is no longer present');
  //header('HTTP/1.0 404 Not Found');
  break;

/* Inform that the action is invalid */
default:
  printf("Invalid action.");
  exit(0);
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

foreach($results as $result) {
  printf("%s", $row);
}
