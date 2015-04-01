<?php
require_once('SqlConnection.php');
$request_start = microtime(true);

function request_time() {
  return round((microtime(true) - $GLOBALS['request_start']), 4);
}

function random_pleasing_color() {
  $red = rand(128, 236);
  $green = rand(128, 236);
  $blue = rand(128, 236);

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

/* Get time lapsed in human-readable format */
function time_lapsed($datetime, $add_ago = true) {

    $datetime = time() - strtotime($datetime);
    $tokens = array (
        31536000 => 'year',
        2592000 => 'month',
        604800 => 'week',
        86400 => 'day',
        3600 => 'hour',
        60 => 'minute',
        1 => 'second'
    );

    foreach($tokens as $unit => $text) {
        if($datetime < $unit) continue;
        $units = floor($datetime / $unit);
        return sprintf("%s %s%s%s",
                       $units,
                       $text,
                       (($units > 1) ? 's' : ''),
                       ($add_ago ? ' ago' : ''));
    }
}

function get_tabs($count) {
  $tabs = '';

  for($i = 0; $i < $count; $i++) {
    $tabs = sprintf("%s\t", $tabs);
  }

  return $tabs;
}

/**
 * Builds the html code needed to represent the capabilities as icons
 */
function get_capabilities_icons($capabilities, $tabs_count = 0) {

  $html = '';
  $tabs = get_tabs($tabs_count);
  $capabilities_array = explode(', ', $capabilities);
  $capabilities_size = count($capabilities_array);

  for($i = 0; $i < $capabilities_size; $i++) {
    $c = $capabilities_array[$i];

    switch($c) {

    case 'sd_disk (OK)':
    case 'sd_disk (connected)':
      $c = 'sd_disk';
      break;

    case 'sd_disk (disconnected)':
      $c = 'sd_disk_gray';
      break;

    default:
      break;
    }

    $title = str_replace('_', ' ', sprintf("%s%s", strtoupper($c[0]), substr($c, 1)));
    if($title == 'Sd disk') {
      $title = 'SD DISK (SD DISK inserted)';
    }
    else if($title == 'Sd disk gray') {
      $title = 'SD DISK (No SD DISK inserted)';
    }
    if($title == 'Mic') {
      $title = 'Microphone or Line-in';
    }

    $html = sprintf("%s%s<div title=\"%s\" class=\"capability_icon_%s\">&nbsp;</div>%s",
                    $html,
                    $tabs,
                    $title,
                    $c,
                    ($i + 1 == $capabilities_size ? '' : "\n"));
  }

  return $html;
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
case 'list_devices':
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

  $capability_state = NULL;
  if(isset($_POST['capability_state']) && !empty($_POST['capability_state'])) {
    $capability_state = $_POST['capability_state'];
  }
  else if(isset($_GET['capability_state']) && !empty($_GET['capability_state'])) {
    $capability_state = $_GET['capability_state'];
  }

  $locked_by = NULL;
  if(isset($_POST['locked_by']) && !empty($_POST['locked_by'])) {
    $locked_by = $_POST['locked_by'];
  }
  else if(isset($_GET['locked_by']) && !empty($_GET['locked_by'])) {
    $locked_by = $_GET['locked_by'];
  }

  $device_id = null;
  if(isset($_POST['device_id']) && !empty($_POST['device_id'])) {
    $device_id = $_POST['device_id'];
  }
  else if(isset($_GET['device_id']) && !empty($_GET['device_id'])) {
    $device_id = $_GET['device_id'];
  }

  $age = 3600;
  if(isset($_POST['age']) && !empty($_POST['age'])) {
    $age = $_POST['age'];
  }
  else if(isset($_GET['age']) && !empty($_GET['age'])) {
    $age = $_GET['age'];
  }

  $ip = null;
  if(isset($_POST['ip']) && !empty($_POST['ip'])) {
    $ip = $_POST['ip'];
  }
  else if(isset($_GET['ip']) && !empty($_GET['ip'])) {
    $ip = $_GET['ip'];
  }

  try {
    $query = sprintf("call list_devices(%s, %s, %s, %s, %d, %s, %s, %s);",
                     ($capability ? sprintf("'%s'", $capability) : 'NULL'),
                     ($model_name ? sprintf("'%s'", $model_name) : 'NULL'),
                     ($firmware_version ? sprintf("'%s'", $firmware_version) : 'NULL'),
                     ($capability_state ? sprintf("'%s'", $capability_state) : 'NULL'),
                     $age,
                     ($locked_by ? sprintf("'%s'", $locked_by) : 'NULL'),
                     ($device_id ? sprintf("'%s'", $device_id) : 'NULL'),
                     ($ip ? sprintf("'%s'", $ip) : 'NULL'));

    $results = $sql->call($query);

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
  if(isset($_POST['firmware_version']) && !empty($_POST['firmware_version'])) {
    $firmware_version = $_POST['firmware_version'];
  }
  else if(isset($_GET['firmware_version']) && !empty($_GET['firmware_version'])) {
    $firmware_version = $_GET['firmware_version'];
  }

  $capability_state = NULL;
  if(isset($_POST['capability_state']) && !empty($_POST['capability_state'])) {
    $capability_state = $_POST['capability_state'];
  }
  else if(isset($_GET['capability_state']) && !empty($_GET['capability_state'])) {
    $capability_state = $_GET['capability_state'];
  }

  $locked_by = NULL;
  if(isset($_POST['locked_by']) && !empty($_POST['locked_by'])) {
    $locked_by = $_POST['locked_by'];
  }
  else if(isset($_GET['locked_by']) && !empty($_GET['locked_by'])) {
    $locked_by = $_GET['locked_by'];
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

  $device_id = null;
  if(isset($_POST['device_id']) && !empty($_POST['device_id'])) {
    $device_id = $_POST['device_id'];
  }
  else if(isset($_GET['device_id']) && !empty($_GET['device_id'])) {
    $device_id = $_GET['device_id'];
  }

  /* 16 minutes */
  $age = 960;
  if(isset($_POST['age']) && !empty($_POST['age'])) {
    $age = $_POST['age'];
  }
  else if(isset($_GET['age']) && !empty($_GET['age'])) {
    $age = $_GET['age'];
  }

  $ip = null;
  if(isset($_POST['ip']) && !empty($_POST['ip'])) {
    $ip = $_POST['ip'];
  }
  else if(isset($_GET['ip']) && !empty($_GET['ip'])) {
    $ip = $_GET['ip'];
  }

  try {
    $query = sprintf("call list_devices(%s, %s, %s, %s, %d, %s, %s, %s);",
                     ($capability ? sprintf("'%s'", $capability) : 'NULL'),
                     ($model_name ? sprintf("'%s'", $model_name) : 'NULL'),
                     ($firmware_version ? sprintf("'%s'", $firmware_version) : 'NULL'),
                     ($capability_state ? sprintf("'%s'", $capability_state) : 'NULL'),
                     $age,
                     ($locked_by ? sprintf("'%s'", $locked_by) : 'NULL'),
                     ($device_id ? sprintf("'%s'", $device_id) : 'NULL'),
                     ($ip ? sprintf("'%s'", $ip) : 'NULL'));

    $results = $sql->call($query);

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
  $random_color = random_pleasing_color();

  /* Print out the whole html document */
  header('HTTP/1.0 200 Ok');
  printf("<!DOCTYPE html>\n");
  printf("<html>\n<head>\n\t<title>List of devices</title>\n");
  printf("\t<link rel=\"stylesheet\" type=\"text/css\" href=\"gui.css\">\n");
  printf("</head>\n<body>\n");
  printf("<table class=\"round_table\" style=\"background-color: %s\">\n", $random_color);
  printf("\t<tr>\n");
  printf("\t\t<td colspan=\"7\" style=\"padding-bottom: 2em;\">\n");
  printf("\t\t\t<form methon=\"get\" action=\"\">\n");
  printf("\t\t\t\t<table style=\"margin-left: auto; margin-right: auto;\">\n");
  printf("\t\t\t\t\t<tr>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tLogged in as:\n");
  printf("\t\t\t\t\t  </td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"user\" readonly=\"true\" value=\"%s\" />\n", $user);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tID: \n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"device_id\" value=\"%s\" />\n", $device_id);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tIP (v4):\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"ip\" value=\"%s\" />\n", $ip);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td rowspan=\"3\" style=\"text-align: center;\">\n");
  printf("\t\t\t\t\t\t\t<input type=\"hidden\" name=\"action\" value=\"gui_list\" />\n");
  printf("\t\t\t\t\t\t\t<input type=\"submit\" class=\"apply_button\" style=\"border: none; background-color: transparent; box-shadow: none; margin-left: 1em; margin-right: 2em;\" title=\"Apply filters\" value=\"\" />\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t</tr>\n");
  printf("\t\t\t\t\t<tr>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tCapability:\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"capability\" value=\"%s\" />\n", $capability);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\t<span title=\"For capability 'sd_disk' state can be: OK, connected, disconnected or failed\">Capability state:</span>\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"capability_state\" value=\"%s\" />\n", $capability_state);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tModel name:\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"model_name\" value=\"%s\" />\n", $model_name);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t</tr>\n");
  printf("\t\t\t\t\t<tr>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tFirmware version:\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"firmware_version\" value=\"%s\" />\n", $firmware_version);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tOccupant:\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"locked_by\" value=\"%s\" />\n", $locked_by);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tAge (seconds):\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"age\" value=\"%s\" />\n", $age);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t</tr>\n");
  printf("\t\t\t\t</table>\n");
  printf("\t\t\t</form>\n");
  printf("\t\t</td>\n");
  printf("\t</tr>\n");
  printf("\t<tr>\n");
  printf("\t\t<td class=\"title_td_result_table border_top_left_radius border_top_right_radius white_bg\">ID</td>\n");
  printf("\t\t<td class=\"title_td_result_table\">IP (v4)</td>\n");
  printf("\t\t<td class=\"title_td_result_table\">Model</td>\n");
  printf("\t\t<td class=\"title_td_result_table\">Firmware version</td>\n");
  printf("\t\t<td class=\"title_td_result_table\">Capabilities</td>\n");
  printf("\t\t<td class=\"title_td_result_table\">Updated (UPnP)</td>\n");
  printf("\t\t<td class=\"title_td_result_table\">Occupant</td>\n");
  printf("\t</tr>\n");
  $results_size = count($results);
  $results_index = 0;
  foreach($results as $result) {
    $lock_style = 'div_device_bookable';
    $lock_link = sprintf("%s?action=lock_device&amp;device_id=%s&amp;user=%s&amp;age=%s&amp;url=%s",
                         $current_script,
                         $result['id'],
                         $user,
                         $age,
                         urlencode($current_request));
    if(!empty($result['locked_by'])) {
      $lock_style = 'div_device_booked';
      $lock_link = sprintf("%s?action=unlock_device&amp;device_id=%s&amp;url=%s",
                           $current_script,
                           $result['id'],
                           urlencode($current_request));
    }
    $round_border_bottom_left = '';
    $round_border_bottom_right = '';
    $bottom_padding = '';
    if($results_index++ == $results_size - 1) {
      $round_border_bottom_left = ' border_bottom_left_radius';
      $round_border_bottom_right = ' border_bottom_right_radius';
      $bottom_padding = ' bottom_cell';
    }
    printf("\t<tr>\n");
    printf("\t\t<td class=\"white_bg cell_padding%s%s%s\">\n",
           $round_border_bottom_left,
           $round_border_bottom_right,
           $bottom_padding);
    printf("\t\t\t<div class=\"%s\" onclick=\"javascript:window.location='%s'\">%s</div>\n",
           $lock_style,
           $lock_link,
           $result['id']);
    printf("\t\t</td>\n");

    printf("\t\t<td class=\"cell_padding%s%s%s\"><a target=\"_blank\" href=\"http://%s\">%s</a></td>\n",
           $round_border_bottom_left,
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           $result['ipv4'],
           $result['ipv4']);

    printf("\t\t<td class=\"cell_padding%s%s\">%s</td>\n",
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           $result['model_name']);

    printf("\t\t<td class=\"cell_padding%s%s\">%s</td>\n",
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           $result['firmware_version']);

    printf("\t\t<td class=\"cell_padding%s%s\">\n%s\n\t\t</td>\n",
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           get_capabilities_icons($result['capabilities'], 3));

    $time_lapsed = '&nbsp;';
    if($result['last_update']) {
      $time_lapsed = time_lapsed($result['last_update']);
      if(empty($time_lapsed)) {
        $time_lapsed = 'now';
      }
      else {
        $matched = preg_match('/(\d*) (.*) ago/', $time_lapsed, $matches);
        if($matched) {
          $time_lapsed_color = null;
          if($matches[1] == '15' && $matches[2] == 'minutes') {
            $time_lapsed_color = 'orange_color';
          }
          else if(($matches[1] > 15 && $matches[2] == 'minutes') ||
                  false === strpos($matches[2], 'second') &&
                  false === strpos($matches[2], 'minute')) {
            $time_lapsed_color = 'red_color';
          }
        }
        $time_lapsed = sprintf("<div title=\"%s (last UPnP message was '%s')\" class=\"%s%s\">%s</div>",
                               $result['last_update'],
                               $result['last_upnp_message'],
                               ($time_lapsed_color ? 'text_center_shadow ' : ''),
                               $time_lapsed_color,
                               $time_lapsed);
      }
    }
    printf("\t\t<td class=\"cell_padding%s%s\">%s</td>\n",
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           $time_lapsed);

    $occupant_and_time_lapsed = '&nbsp';
    if($result['locked_date']) {
      $time_lapsed_color = '';
      $time_lapsed = time_lapsed($result['locked_date']);
      if(empty($time_lapsed)) {
        $time_lapsed = 'now';
      }
      else {
        $matched = preg_match('/(\d*) (.*) ago/', $time_lapsed, $matches);
        $time_lapsed_color = '';
        if($matched) {
          if($matches[1] > '1' && $matches[1] < 6 && $matches[2] == 'days') {
            $time_lapsed_color = 'orange_color';
          }
          else if(($matches[1] > 5 && $matches[2] == 'days') ||
                  false === strpos($matches[2], 'second') &&
                  false === strpos($matches[2], 'minute') &&
                  false === strpos($matches[2], 'hour') &&
                  false === strpos($matches[2], 'day')) {
            $time_lapsed_color = 'red_color';
          }
        }
      }
      $occupant_and_time_lapsed = sprintf("<div title=\"%s (%s)\" class=\"%s%s\">%s</div>",
                             $time_lapsed,
                             $result['locked_date'],
                             ($time_lapsed_color ? 'text_center_shadow ' : ''),
                             $time_lapsed_color,
                             $result['locked_by']);
    }
    printf("\t\t<td class=\"cell_padding%s%s\">%s</td>\n",
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           $occupant_and_time_lapsed);

    printf("\t</tr>\n");
  }
  printf("</table>\n");
  printf("<div style=\"font-size: .8em; color: gray; padding: 1em; text-align: center;\">found %s matches (%ss)</div>\n",
         count($results),
         request_time());
  printf("</body>\n</html>\n");
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

  $capability_state = NULL;
  if(isset($_POST['capability_state']) && !empty($_POST['capability_state'])) {
    $capability_state = $_POST['capability_state'];
  }
  else if(isset($_GET['capability_state']) && !empty($_GET['capability_state'])) {
    $capability_state = $_GET['capability_state'];
  }

  $device_id = null;
  if(isset($_POST['device_id']) && !empty($_POST['device_id'])) {
    $device_id = $_POST['device_id'];
  }
  else if(isset($_GET['device_id']) && !empty($_GET['device_id'])) {
    $device_id = $_GET['device_id'];
  }

  /* 16 minutes */
  $age = 960;
  if(isset($_POST['age']) && !empty($_POST['age'])) {
    $age = $_POST['age'];
  }
  else if(isset($_GET['age']) && !empty($_GET['age'])) {
    $age = $_GET['age'];
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

  try {
    $results = $sql->call(sprintf("call lock_device(%s, %s, %s, %s, '%s', %d, %s)",
                                  ($capability ? sprintf("'%s'", $capability): 'NULL'),
                                  ($model_name ? sprintf("'%s'", $model_name) : 'NULL'),
                                  ($firmware_version ? sprintf("'%s'", $firmware_version) : 'NULL'),
                                  ($capability_state ? sprintf("'%s'", $capability_state): 'NULL'),
                                  $user,
                                  $age,
                                  ($device_id ? sprintf("'%s'", $device_id) : 'NULL')));
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

