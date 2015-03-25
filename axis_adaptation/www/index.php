<?php
require_once('SqlConnection.php');

function random_pleasing_colors() {
  $red = rand(128, 236);
  $green = rand(128, 236);
  $blue = rand(128, 236);

  return array(sprintf("rgb(%d, %d, %d)", $red, $green, $blue),
               sprintf("rgb(%d, %d, %d)", $red + 20, $green + 20, $blue + 20));
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

  $firmware_version = null;
  if(isset($_post['firmware_version']) && !empty($_post['firmware_version'])) {
    $firmware_version = $_post['firmware_version'];
  }
  else if(isset($_get['firmware_version']) && !empty($_get['firmware_version'])) {
    $firmware_version = $_get['firmware_version'];
  }

  $user = NULL;
  if(isset($_POST['user']) && !empty($_POST['user'])) {
    $user = $_POST['user'];
  }
  else if(isset($_GET['user']) && !empty($_GET['user'])) {
    $user = $_GET['user'];
  }

  $age = null;
  if(isset($_POST['age']) && !empty($_POST['age'])) {
    $age = $_POST['age'];
  }
  else if(isset($_GET['age']) && !empty($_GET['age'])) {
    $age = $_GET['age'];
  }

  try {
    $results = $sql->call(sprintf("call list_devices(%s, %s, %s);",
                                  ($capability ? sprintf("'%s'", $capability) : 'NULL'),
                                  ($model_name ? sprintf("'%s'", $model_name) : 'NULL'),
                                  ($firmware_version ? sprintf("'%s'", $firmware_version) : 'NULL')));
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

/* List devices in a minimal GUI */
case 'gui_list':
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

  $firmware_version = null;
  if(isset($_post['firmware_version']) && !empty($_post['firmware_version'])) {
    $firmware_version = $_post['firmware_version'];
  }
  else if(isset($_get['firmware_version']) && !empty($_get['firmware_version'])) {
    $firmware_version = $_get['firmware_version'];
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

  $age = null;
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

  /* Capture the current location to add to all links */
  $current_request = $_SERVER['REQUEST_URI'];
  $current_script = $_SERVER['SCRIPT_NAME'];

  /* Create the random colors for the table */
  $random_colors = random_pleasing_colors();

  /* Print out the whole html document */
  header('HTTP/1.0 200 Ok');
  printf("<!DOCTYPE html>\n");
  printf("<html>\n<head>\n\t<title>List of devices</title>\n");
  printf("\t<style type=\"text/css\">\n");
  printf("\t\thtml {\n\t\tfont-family: Verdana, Sans-serif;\n\t\t}\n");
  printf("\t</style>\n");
  printf("</head>\n<body>\n");
  printf("<table style=\"border-spacing: 0; background-color: %s; padding: 1em; box-shadow: 2px 2px 2px black; border-radius: 1em;\">\n", $random_colors[0]);
  printf("\t<tr>\n");
  printf("\t\t<td style=\"padding: 0 .5em; padding-top: .5em; border-top-left-radius: .5em; border-top-right-radius: .5em; background-color: white; text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">ID</td>\n");
  printf("\t\t<td style=\"padding: 0 .5em; text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">IP (v4)</td>\n");
  printf("\t\t<td style=\"padding: 0 .5em; text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Model</td>\n");
  printf("\t\t<td style=\"padding: 0 .5em; text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Firmware version</td>\n");
  printf("\t\t<td style=\"padding: 0 .5em; text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Capabilities</td>\n");
  printf("\t\t<td style=\"padding: 0 .5em; text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Last updated</td>\n");
  printf("\t\t<td style=\"padding: 0 .5em; text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Locked by</td>\n");
  printf("\t\t<td style=\"padding: 0 .5em; text-shadow: 1px 1px 2px grey; font-weight: bold; border-bottom: solid 2px black;\">Locked date</td>\n");
  printf("\t</tr>\n");
  $results_size = count($results);
  $results_index = 0;
  foreach($results as $result) {
    $lock_style = 'color: green;';
    $lock_link = sprintf("%s?action=lock_device_by_id&amp;device_id=%s&amp;user=%s&amp;age=%s&amp;url=%s",
                         $current_script,
                         $result['id'],
                         $user,
                         $age,
                         urlencode($current_request));
    if(!empty($result['locked_by'])) {
      $lock_style = 'color: red;';
      $lock_link = sprintf("%s?action=unlock_device&amp;device_id=%s&amp;url=%s",
                           $current_script,
                           $result['id'],
                           urlencode($current_request));
    }
    $round_border_bottom_left = '';
    $round_border_bottom_right = '';
    $bottom_padding = '';
    if($results_index++ == $results_size - 1) {
      $round_border_bottom_left = 'border-bottom-left-radius: .5em; ';
      $round_border_bottom_right = 'border-bottom-right-radius: .5em; ';
      $bottom_padding = ' padding-bottom: .5em;';
    }
    printf("\t<tr>\n");
    printf("\t\t<td style=\"%s%sbackground-color: white; padding: 0 .5em;%s\">\n",
           $round_border_bottom_left,
           $round_border_bottom_right,
           $bottom_padding);
    printf("\t\t\t<div style=\"%s cursor: pointer;\" onclick=\"javascript:window.location='%s'\">%s</div>\n",
           $lock_style,
           $lock_link,
           $result['id']);
    printf("\t\t</td>\n");
    printf("\t\t<td style=\"%s%spadding: 0 .5em;%s\"><a target=\"_blank\" href=\"http://%s\">%s</a></td>\n",
           $round_border_bottom_left,
           ($results_index % 2 ? sprintf("background-color: %s;", $random_colors[1]) : ''),
           $bottom_padding,
           $result['ipv4'],
           $result['ipv4']);
    printf("\t\t<td style=\"%spadding: 0 .5em;%s\">%s</td>\n",
           ($results_index % 2 ? sprintf("background-color: %s;", $random_colors[1]) : ''),
           $bottom_padding,
           $result['model_name']);
    printf("\t\t<td style=\"%spadding: 0 .5em;%s\">%s</td>\n",
           ($results_index % 2 ? sprintf("background-color: %s;", $random_colors[1]) : ''),
           $bottom_padding,
           $result['firmware_version']);
    printf("\t\t<td style=\"%spadding: 0 .5em;%s\">%s</td>\n",
           ($results_index % 2 ? sprintf("background-color: %s;", $random_colors[1]) : ''),
           $bottom_padding,
           $result['capabilities']);
    printf("\t\t<td style=\"%spadding: 0 .5em;%s\">%s</td>\n",
           ($results_index % 2 ? sprintf("background-color: %s;", $random_colors[1]) : ''),
           $bottom_padding,
           $result['last_update']);
    printf("\t\t<td style=\"%spadding: 0 .5em;%s\">%s</td>\n",
           ($results_index % 2 ? sprintf("background-color: %s;", $random_colors[1]) : ''),
           $bottom_padding,
           $result['locked_by']);
    printf("\t\t<td style=\"%s%spadding: 0 .5em;%s\">%s</td>\n",
           $round_border_bottom_right,
           ($results_index % 2 ? sprintf("background-color: %s;", $random_colors[1]) : ''),
           $bottom_padding,
           $result['locked_date']);
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

  $url = null;
  if(isset($_POST['url']) && !empty($_POST['url'])) {
    $url = urldecode($_POST['url']);
  }
  else if(isset($_GET['url']) && !empty($_GET['url'])) {
    $url = urldecode($_GET['url']);
  }

  try {
    $results = $sql->call(sprintf("call lock_device_by_id('%s', '%s')", $device_id, $user));
    header('Content-type: application/json; charset=utf-8');
    if(!empty($url)) {
      header(sprintf("Location: %s", $url));
    }
    else if(is_array($results) && count($results) > 0) {
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

  $url = null;
  if(isset($_POST['url']) && !empty($_POST['url'])) {
    $url = urldecode($_POST['url']);
  }
  else if(isset($_GET['url']) && !empty($_GET['url'])) {
    $url = $_GET['url'];
  }

  $age = null;
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
    if(!empty($url)) {
      header(sprintf("Location: %s", $url));
    }
    else if(is_array($results) && count($results) > 0) {
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

  $url = null;
  if(isset($_POST['url']) && !empty($_POST['url'])) {
    $url = urldecode($_POST['url']);
  }
  else if(isset($_GET['url']) && !empty($_GET['url'])) {
    $url = $_GET['url'];
  }

  try {
    $results = $sql->call(sprintf("call unlock_device('%s')", $device_id));
  }
  catch(Exception $e) {
    header('HTTP/1.0 500');
    printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
    exit(1);
  }
  if(!empty($url)) {
    header(sprintf("Location: %s", $url));
  }
  else {
    printf("OK\n");
  }
  break;

/* Inform that the action is invalid */
default:
  printf("Invalid action.");
  exit(0);
}

