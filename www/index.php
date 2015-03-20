<?php

function randomPleasingColor() {
  $red = rand(128, 256);
  $green = rand(128, 256);
  $blue = rand(128, 256);

  return sprintf("rgb(%d, %d, %d)", $red, $green, $blue);
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

$host     = 'localhost';
$database = 'abused';
$username = 'abused';
$password = 'abusedpass';

$h_sql = new mysqli($host, $username, $password, $database);
if ($h_sql->connect_errno) {
  printf("Failed to connect to MySQL: %s", $h_sql->connect_error);
  exit(1);
}

switch($action) {

/* List devices */
case 'list':
  $res = $h_sql->query('SELECT d.id, d.ipv4, mf.model_name, mf.firmware_version, d.last_update
                        FROM `devices` d, `model_firmware` mf
                        WHERE d.model_firmware_id=mf.id;');
  if(false === $res) {
    printf("Failed to query MySQL: %s", $h_sql->error);
    exit(1);
  }
  header('HTTP/1.0 200 Ok');
  $results = array();
  while($row = $res->fetch_assoc()) {
    $results[] = $row;
  }
  printf("<!DOCTYPE html>\n");
  printf("<html>\n<head>\n\t<title>List of devices</title>\n</head>\n<body>\n");
  printf("<table style=\"background-color: %s; padding: 1em; border: solid 2px black; border-radius: 1em;\">\n", randomPleasingColor());
  printf("\t<tr>\n");
  printf("\t\t<td style=\"font-weight: bold; border-bottom: solid 2px black;\">id</td>\n");
  printf("\t\t<td style=\"font-weight: bold; border-bottom: solid 2px black;\">ip</td>\n");
  printf("\t\t<td style=\"font-weight: bold; border-bottom: solid 2px black;\">model</td>\n");
  printf("\t\t<td style=\"font-weight: bold; border-bottom: solid 2px black;\">fw</td>\n");
  printf("\t\t<td style=\"font-weight: bold; border-bottom: solid 2px black;\">last updated</td>\n");
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
  $res = $h_sql->query(sprintf("SELECT d.id, d.ipv4, mf.model_name, mf.firmware_version, d.last_update
                                FROM `devices` d,
                                     `model_firmware` mf,
                                     `model_firmware_capability` mfc,
                                     `capabilities` c
                                WHERE d.model_firmware_id=mf.id
                                AND d.model_firmware_id=mfc.model_firmware_id
                                AND mfc.capability_id=c.id
                                AND c.name='%s';", $capability));
  if(false === $res) {
    printf("Failed to query MySQL: %s", $h_sql->error);
    exit(1);
  }
  header('HTTP/1.0 200 Ok');
  $results = array();
  while($row = $res->fetch_assoc()) {
    $results[] = $row;
  }
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

