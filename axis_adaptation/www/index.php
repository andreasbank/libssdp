<?php
require_once('SqlConnection.php');

function randomPleasingColor() {
  $red = rand(128, 256);
  $green = rand(128, 256);
  $blue = rand(128, 256);

  return sprintf("rgb(%d, %d, %d)", $red, $green, $blue);
}

function get_device_locked_info($locked_list, $device_id) {
  foreach($locked_list as $locked_device) {
    if($locked_device['device_id'] == $device_id) {
      return $locked_device;
    }
  }
  return null;
}

/* MySQL credentials */
$host     = 'localhost';
$database = 'abused';
$username = 'abused';
$password = 'abusedpass';

/* Connect to MySQL */
try {
  $sql = new SqlConnection($host, $database, $username, $password);
}
catch(Exception $e) {
  printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
  exit(1);
}

$action = NULL;
if(isset($_POST['action']) && !empty($_POST['action'])) {
  $action = $_POST['action'];
}
else if(isset($_GET['action']) && !empty($_GET['action'])) {
  $action = $_GET['action'];
}
else {
  printf("No action specified.");
  exit(0);
}

switch($action) {

/* List devices */
case 'list':
  $capability = NULL;
  if(isset($_POST['capability']) && !empty($_POST['capability'])) {
    $capability = $_POST['capability'];
  }
  else if(isset($_GET['capability']) && !empty($_GET['capability'])) {
    $capability = $_GET['capability'];
  }

  $model_name = NULL;
  if(isset($_POST['model_name']) && !empty($_POST['model_name'])) {
    $model_name = $_POST['model_name'];
  }
  else if(isset($_GET['model_name']) && !empty($_GET['model_name'])) {
    $model_name = $_GET['model_name'];
  }

  $firmware_version = NULL;
  if(isset($_POST['firmware_version']) && !empty($_POST['firmware_version'])) {
    $firmware_version = $_POST['firmware_version'];
  }
  else if(isset($_GET['firmware_version']) && !empty($_GET['firmware_version'])) {
    $firmware_version = $_GET['firmware_version'];
  }

  try {
    $results = $sql->call(sprintf("call list_devices(%s, %s, %s);",
                                  ($capability ? sprintf("'%s'", $capability) : 'NULL'),
                                  ($model_name ? sprintf("'%s'", $model_name) : 'NULL'),
                                  ($firmware_version ? sprintf("'%s'", $firmware_version) : 'NULL')));
    if(is_array($results) && count($results) > 0) {
      $results = $results[0];
    }
    else {
      $results = NULL;
    }
  }
  catch(Exception $e) {
    header('HTTP/1.0 500');
    printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
    exit(1);
  }
  header('HTTP/1.0 200 Ok');
  printf("<!DOCTYPE html>\n");
  printf("<html>\n<head>\n\t<title>List of devices</title>\n</head>\n<body>\n");
  printf("<table style=\"border-spacing: 0; background-color: %s; padding: 1em; border: solid 2px black; border-radius: 1em;\">\n", randomPleasingColor());
  printf("\t<tr>\n");
  printf("\t\t<td style=\"border-top-left-radius: .5em; border-top-right-radius: .5em; background-color: white; text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">ID</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">IP (v4)</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Model</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Firmware version</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Capabilities</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Last updated</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Locked by</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Locked date</td>\n");
  printf("\t</tr>\n");
  $results_size = count($results);
  $results_index = 0;
  foreach($results as $result) {
    $round_borders = '';
    $bottom_padding = '';
    if($results_index++ == $results_size - 1) {
      $round_borders = 'border-bottom-left-radius: .5em; border-bottom-right-radius: .5em; ';
      $bottom_padding = ' padding-bottom: .5em;';
    }
    printf("\t<tr>\n");
    printf("\t\t<td style=\"%sbackground-color: white; padding: 0 .5em;\">%s</td>\n", $round_borders, $result['id']);
    printf("\t\t<td style=\"padding-right: 1em;\"><a target=\"_blank\" href=\"http://%s\">%s</a></td>\n", $result['ipv4'], $result['ipv4']);
    printf("\t\t<td style=\"padding: 0 .5em;\">%s</td>\n", $result['model_name']);
    printf("\t\t<td style=\"padding: 0 .5em;\">%s</td>\n", $result['firmware_version']);
    printf("\t\t<td style=\"padding: 0 .5em;\">%s</td>\n", $result['capabilities']);
    printf("\t\t<td style=\"padding: 0 .5em;\">%s</td>\n", $result['last_update']);
    printf("\t\t<td style=\"padding: 0 .5em;\">%s</td>\n", $result['locked_by']);
    printf("\t\t<td style=\"padding: 0 .5em;%s\">%s</td>\n", $bottom_padding, $result['locked_date']);
    printf("\t</tr>\n");
  }
  printf("</table>\n");
  printf("</body>\n</html>\n");
  break;

/* Lock a device by its ID if possible */
case 'lock_device_by_id':
  $device_id = NULL;
  if(isset($_POST['device_id']) && !empty($_POST['device_id'])) {
    $device_id = $_POST['device_id'];
  }
  else if(isset($_GET['device_id']) && !empty($_GET['device_id'])) {
    $device_id = $_GET['device_id'];
  }
  else {
    printf("No device ID specified.");
    exit(0);
  }
  $user = NULL;
  if(isset($_POST['user']) && !empty($_POST['user'])) {
    $user = $_POST['user'];
  }
  else if(isset($_GET['user']) && !empty($_GET['user'])) {
    $user = $_GET['user'];
  }
  else {
    printf("No user specified.");
    exit(0);
  }
  try {
    $results = $sql->call(sprintf("call lock_device_by_id('%s', '%s')", $device_id, $user));
    header('Content-type: application/json; charset=utf-8');
    if(is_array($results) && count()) {
      printf("%s", json_encode($results[0]));
    }
    else {
      printf("[]");
    }
  }
  catch(Exception $e) {
    header('HTTP/1.0 500');
    printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
    exit(1);
  }
  break;

/* Lock the device if possible */
case 'lock_device':
  $capability = NULL;
  if(isset($_POST['capability']) && !empty($_POST['capability'])) {
    $capability = $_POST['capability'];
  }
  else if(isset($_GET['capability']) && !empty($_GET['capability'])) {
    $capability = $_GET['capability'];
  }

  $model_name = NULL;
  if(isset($_POST['model_name']) && !empty($_POST['model_name'])) {
    $model_name = $_POST['model_name'];
  }
  else if(isset($_GET['model_name']) && !empty($_GET['model_name'])) {
    $model_name = $_GET['model_name'];
  }

  $firmware_version = NULL;
  if(isset($_POST['firmware_version']) && !empty($_POST['firmware_version'])) {
    $firmware_version = $_POST['firmware_version'];
  }
  else if(isset($_GET['firmware_version']) && !empty($_GET['firmware_version'])) {
    $firmware_version = $_GET['firmware_version'];
  }

  $user = NULL;
  if(isset($_POST['user']) && !empty($_POST['user'])) {
    $user = $_POST['user'];
  }
  else if(isset($_GET['user']) && !empty($_GET['user'])) {
    $user = $_GET['user'];
  }
  else {
    printf("No user specified.");
    exit(0);
  }

  $age = NULL;
  if(isset($_POST['age']) && !empty($_POST['age'])) {
    $age = $_POST['age'];
  }
  else if(isset($_GET['age']) && !empty($_GET['age'])) {
    $age = $_GET['age'];
  }
  else {
    printf("No age specified.");
    exit(0);
  }

  try {
    $results = $sql->call(sprintf("call lock_device(%s, %s, %s, '%s', %d)",
                                  ($capability ? sprintf("'%s'", $capability): 'NULL'),
                                  ($model_name ? sprintf("'%s'", $model_name) : 'NULL'),
                                  ($firmware_version ? sprintf("'%s'", $firmware_version) : 'NULL'),
                                  $user,
                                  $age));
    header('Content-type: application/json; charset=utf-8');
    if(is_array($results) && count($results) > 0) {
      printf("%s", json_encode($results[0]));
    }
    else {
      printf("[]");
    }
  }
  catch(Exception $e) {
    header('HTTP/1.0 500');
    printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
    exit(1);
  }
  break;

/* Unlock a device by its ID */
case 'unlock_device':
  $device_id = NULL;
  if(isset($_POST['device_id']) && !empty($_POST['device_id'])) {
    $device_id = $_POST['device_id'];
  }
  else if(isset($_GET['device_id']) && !empty($_GET['device_id'])) {
    $device_id = $_GET['device_id'];
  }
  else {
    printf("No device ID specified.");
    exit(0);
  }
  try {
    $results = $sql->call(sprintf("call unlock_device('%s')", $device_id));
  }
  catch(Exception $e) {
    header('HTTP/1.0 500');
    printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
    exit(1);
  }
  printf("OK\n");
  break;

/* Inform that the action is invalid */
default:
  printf("Invalid action.");
  exit(0);
}

