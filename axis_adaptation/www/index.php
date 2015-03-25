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
  try {
    $results = $sql->query('SELECT d.id, d.ipv4, mf.model_name, mf.firmware_version, d.last_update
                            FROM `devices` d, `model_firmware` mf
                            WHERE d.model_firmware_id=mf.id;');
//    $locked_results = $sql->query('SELECT ld.device_id, ld.locked_by, ld.locked_date
//                                   FROM `locked_evices` ld
//                                   WHERE ld.locked=1;');
  }
  catch(Exception $e) {
    header('HTTP/1.0 500');
    printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
    exit(1);
  }
  header('HTTP/1.0 200 Ok');
  printf("<!DOCTYPE html>\n");
  printf("<html>\n<head>\n\t<title>List of devices</title>\n</head>\n<body>\n");
  printf("<table style=\"background-color: %s; padding: 1em; border: solid 2px black; border-radius: 1em;\">\n", randomPleasingColor());
  printf("\t<tr>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">id</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">ip</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">model</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">fw</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">last updated</td>\n");
  printf("\t</tr\n");
  foreach($results as $result) {
    printf("\t<tr>\n");
    printf("\t\t<td style=\"padding-right: 1em;\">%s</td>\n", $result['id']);
    printf("\t\t<td style=\"padding-right: 1em;\"><a target=\"_blank\" href=\"http://%s\">%s</a></td>\n", $result['ipv4'], $result['ipv4']);
    printf("\t\t<td style=\"padding-right: 1em;\">%s</td>\n", $result['model_name']);
    printf("\t\t<td style=\"padding-right: 1em;\">%s</td>\n", $result['firmware_version']);
    printf("\t\t<td>%s</td>\n", $result['last_update']);
    printf("\t</tr>\n");
  }
  printf("</table>\n");
  printf("</body>\n</html>\n");
  break;

/* List devices for given capability */
case 'list_by_capability':
  $capability = NULL;
  if(isset($_POST['capability']) && !empty($_POST['capability'])) {
    $capability = $_POST['capability'];
  }
  else if(isset($_GET['capability']) && !empty($_GET['capability'])) {
    $capability = $_GET['capability'];
  }
  else {
    printf("No capability specified.");
    exit(0);
  }
  try {
    $results = $sql->query(sprintf("SELECT d.id, d.ipv4, mf.model_name, mf.firmware_version, d.last_update
                                    FROM `devices` d,
                                         `model_firmware` mf,
                                         `model_firmware_capability` mfc,
                                         `capabilities` c
                                    WHERE d.model_firmware_id=mf.id
                                    AND d.model_firmware_id=mfc.model_firmware_id
                                    AND mfc.capability_id=c.id
                                    AND c.name='%s';", $capability));
  }
  catch(Exception $e) {
    header('HTTP/1.0 500');
    printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
    exit(1);
  }
  header('HTTP/1.0 200 Ok');
  printf("<!DOCTYPE html>\n");
  printf("<html>\n<head>\n\t<title>List devices with capability '%s'</title>\n</head>\n<body>\n", $capability);
  printf("<table style=\"background-color: %s; padding: 1em; border: solid 2px black; border-radius: 1em;\">\n", randomPleasingColor());
  printf("\t<tr>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">id</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">ip</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">model</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">fw</td>\n");
  printf("\t\t<td style=\"text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">last updated</td>\n");
  printf("\t</tr\n");
  foreach($results as $result) {
    printf("\t<tr>\n");
    printf("\t\t<td style=\"padding-right: 1em;\">%s</td>\n", $result['id']);
    printf("\t\t<td style=\"padding-right: 1em;\"><a target=\"_blank\" href=\"http://%s\">%s</a></td>\n", $result['ipv4'], $result['ipv4']);
    printf("\t\t<td style=\"padding-right: 1em;\">%s</td>\n", $result['model_name']);
    printf("\t\t<td style=\"padding-right: 1em;\">%s</td>\n", $result['firmware_version']);
    printf("\t\t<td>%s</td>\n", $result['last_update']);
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
    printf("%s", json_encode($results[0]));
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
    printf("%s", json_encode($results[0]));
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

