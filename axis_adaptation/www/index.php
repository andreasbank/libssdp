<?php
require_once('SqlConnection.php');
require_once('CapabilityManager.php');
$request_start = microtime(true);

/* MySQL credentials */
$host     = 'localhost';
$database = 'abused';
$username = 'abused';
$password = 'abusedpass';

/* Info for extracting specific information of
   the device in the case where it is an AXIS device */
$axis_device_credentials = array(
  array(
    'username' => 'root',
    'password' => 'pass'),
  array(
    'username' => 'camroot',
    'password' => 'password'));
$axis_proxy_address = 'wwwproxy.se.axis.com';
$axis_proxy_port = 3128;


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
// %'.020d
/* Sort the results */
function sort_results(&$arr, $sort_by) {
  foreach($arr as $k => $v) {
    $id_array[$k] = $v['id'];
    $ipv4_array[$k] = $v['ipv4'];
    //$ipv6_array[$k] = $v['ipv6'];
    $model_name_array[$k] = $v['model_name'];
    $firmware_version_array[$k] = $v['firmware_version'];
    $capabilities_array[$k] = $v['capabilities'];
    $last_update_array[$k] = sprintf("%d", (time() - strtotime($v['last_update'])));
    $last_upnp_message_array[$k] = sprintf("%d", (time() - strtotime($v['last_upnp_message'])));
    $locked_by_array[$k] = $v['locked_by'];
    $locked_date_array[$k] = sprintf("%d", strtotime($v['locked_date']));
  }

  switch($sort_by) {
  case 'id':
  case 'ipv4':
  //case 'ipv6':
  case 'model_name':
  case 'firmware_version':
  case 'capabilities':
  case 'last_update':
  case 'last_upnp_message':
  case 'locked_by':
  case 'locked_date':
    $sort_by_array = ${sprintf("%s%s", $sort_by, '_array')};
    break;
  default:
    return $arr;
    break;
  }

  $sorted_array = array_multisort($sort_by_array, $arr);
  return $sorted_array;
}

function list_devices($sql,
                      $capability,
                      $model_name,
                      $firmware_version,
                      $capability_state,
                      $locked_by,
                      $device_id,
                      $age,
                      $ip,
                      $sort_by) {

  $query = sprintf("call list_devices(%s, %s, %s, %s, %d, %s, %s, %s);",
                   ($capability ? sprintf("'%s'", $capability) : 'NULL'),
                   ($model_name ? sprintf("'%s'", $model_name) : 'NULL'),
                   ($firmware_version ? sprintf("'%s'", $firmware_version) : 'NULL'),
                   ($capability_state ? sprintf("'%s'", $capability_state) : 'NULL'),
                   $age * 60,
                   ($locked_by ? sprintf("'%s'", $locked_by) : 'NULL'),
                   ($device_id ? sprintf("'%s'", $device_id) : 'NULL'),
                   ($ip ? sprintf("'%s'", $ip) : 'NULL'));

  $results = $sql->call($query);

  /* Sort the results if $sort_by is set */
  if(!empty($sort_by)) {
    sort_results($results, $sort_by);
  }

  return $results;

}

function move_to_portal($sql,
                        array $device_ids,
                        $age,
                        array $portal_ips,
                        $portal_admin_username,
                        $portal_admin_password,
                        array $device_credentials) {

  foreach($device_ids as $device_id) {
    $devices = list_devices($sql,
                            null,
                            null,
                            null,
                            null,
                            null,
                            $device_id,
                            $age,
                            null,
                            null);

    if(count($devices) < 1 || count($devices[0]) < 1) {
      throw new Exception(sprintf("No device with the ID '%s' present (NOTE: age is set to %d minutes)",
                                  $device_id,
                                  $age));
    }

    $cm = new CapabilityManager($devices[0][0]['ipv4'],
                                $credentials);

    /* Set the new remote service in the device */
    $cm->move_to_portal($portal_ips,
                        $portal_admin_username,
                        $portal_admin_password);

  }
}

function device_info($sql,
                     $device_id,
                     $device_credentials,
                     $age,
                     $proxy_address,
                     $proxy_port) {

  $devices = list_devices($sql,
                          null,
                          null,
                          null,
                          null,
                          null,
                          $device_id,
                          $age,
                          null,
                          null);

  if(count($devices) < 1 || count($devices[0]) < 1) {
    throw new Exception(sprintf("No device with the ID '%s' present (NOTE: age is set to %d minutes)",
                                $device_id,
                                $age));
  }

  $cm = new CapabilityManager($devices[0][0]['ipv4'],
                              $device_credentials);

  $remote_service = null;
  $oak = null;

  /* Fetch the remote service parameter from the device */
  $remote_service = $cm->get_remote_service();

  /* Fetch the device OAK from the dispatcher */
  $cm->set_proxy($proxy_address, $proxy_port);
  $oak = $cm->get_oak($device_id);
  $cm->set_proxy(null, null);

  return array('id' => $device_id, 'remote_service' => $remote_service, 'oak' => $oak);

}

function gui_list($user,
                  $sql,
                  $capability,
                  $model_name,
                  $firmware_version,
                  $capability_state,
                  $locked_by,
                  $device_id,
                  $age,
                  $ip,
                  $sort_by,
                  $current_script,
                  $current_request) {

  $query = sprintf("call list_devices(%s, %s, %s, %s, %d, %s, %s, %s);",
                   ($capability ? sprintf("'%s'", $capability) : 'NULL'),
                   ($model_name ? sprintf("'%s'", $model_name) : 'NULL'),
                   ($firmware_version ? sprintf("'%s'", $firmware_version) : 'NULL'),
                   ($capability_state ? sprintf("'%s'", $capability_state) : 'NULL'),
                   $age * 60,
                   ($locked_by ? sprintf("'%s'", $locked_by) : 'NULL'),
                   ($device_id ? sprintf("'%s'", $device_id) : 'NULL'),
                   ($ip ? sprintf("'%s'", $ip) : 'NULL'));

  $results = $sql->call($query);

  if(is_array($results) && count($results) > 0) {
    $results = $results[0];

    /* Sort the results if $sort_by is set */
    if(!empty($sort_by)) {
      sort_results($results, $sort_by);
    }

  }
  else {
    $results = NULL;
  }

  /* Create the random colors for the table */
  $random_color = random_pleasing_color();

  /* Get results columns for sort_by select box */
  $result_columns = array();
  if(count($results) > 0) {
    foreach($results[0] as $key => $value) {
      $result_columns[] = $key;
    }
  }

  /* Print out the whole html document */
  header('HTTP/1.0 200 Ok');
  printf("<!DOCTYPE html>\n");
  printf("<html>\n<head>\n\t<title>List of devices</title>\n");
  printf("\t<link rel=\"stylesheet\" type=\"text/css\" href=\"gui.css\">\n");
  printf("\t<script type=\"text/javascript\" src=\"gui.js\"></script>");
  printf("</head>\n<body>\n<table style=\"margin-left: auto; margin-right: auto;\">\n");
  printf("<tr>\n<td style=\"font-size: .8em; color: gray; padding-left: 2em;\">\n");
  printf("Logged in as '%s'\n", $user);
  printf("</td>\n</tr>\n<tr>\n<td>\n");
  printf("<table class=\"round_table\" style=\"background-color: %s\">\n", $random_color);
  printf("\t<tr>\n");
  printf("\t\t<td colspan=\"8\" style=\"padding-bottom: 2em;\">\n");
  printf("\t\t\t<form methon=\"get\" action=\"\">\n");
  printf("\t\t\t\t<table style=\"margin-left: auto; margin-right: auto;\">\n");
  printf("\t\t\t\t\t<tr>\n");
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
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tCapability:\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"capability\" value=\"%s\" />\n", $capability);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td rowspan=\"3\" style=\"text-align: center;\">\n");
  printf("\t\t\t\t\t\t\t<input type=\"hidden\" name=\"action\" value=\"gui_list\" />\n");
  printf("\t\t\t\t\t\t\t<input type=\"hidden\" name=\"user\" value=\"%s\" />\n", $user);
  printf("\t\t\t\t\t\t\t<input type=\"submit\" class=\"apply_button\" style=\"border: none; background-color: transparent; box-shadow: none; margin-left: 1em; margin-right: 2em;\" title=\"Apply filters\" value=\"\" />\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t</tr>\n");
  printf("\t\t\t\t\t<tr>\n");
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
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tFirmware version:\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"firmware_version\" value=\"%s\" />\n", $firmware_version);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t</tr>\n");
  printf("\t\t\t\t\t<tr>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tOccupant:\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"locked_by\" value=\"%s\" />\n", $locked_by);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tAge (minutes):\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<input type=\"input\" name=\"age\" value=\"%s\" />\n", $age);
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td class=\"title_td_header_table\">\n");
  printf("\t\t\t\t\t\t\tSort by:\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t\t<td>\n");
  printf("\t\t\t\t\t\t\t<select style=\"width: 100%%;\" name=\"sort_by\">\n");
  printf("\t\t\t\t\t\t\t\t<option %svalue=\"\">none</option>\n",
         (empty($sort_by) ? 'selected="selected" ' : ''));
  foreach($result_columns as $option) {
    printf("\t\t\t\t\t\t\t\t<option %svalue=\"%s\">%s</option>\n",
           ($option == $sort_by ? 'selected="selected" ' : ''),
           $option,
           $option);
  }
  printf("\t\t\t\t\t\t\t</select>\n");
  printf("\t\t\t\t\t\t</td>\n");
  printf("\t\t\t\t\t</tr>\n");
  printf("\t\t\t\t</table>\n");
  printf("\t\t\t</form>\n");
  printf("\t\t</td>\n");
  printf("\t</tr>\n");
  printf("\t<tr>\n");
  printf("\t\t<td>\n");
  printf("\t\t\t<form id=\"group_action_form\" method=\"post\" action=\"\">\n");
  printf("\t\t\t<table class=\"no_cell_space\" style=\"margin: none; width: 100%%;\">\n");
  printf("\t\t\t\t<tr>\n");
  printf("\t\t\t\t\t<td class=\"title_td_result_table\">&nbsp;</td>\n");
  printf("\t\t\t\t\t<td class=\"title_td_result_table border_top_left_radius border_top_right_radius white_bg\">ID</td>\n");
  printf("\t\t\t\t\t<td class=\"title_td_result_table\">IP (v4)</td>\n");
  printf("\t\t\t\t\t<td class=\"title_td_result_table\">Model</td>\n");
  printf("\t\t\t\t\t<td class=\"title_td_result_table\">Firmware version</td>\n");
  printf("\t\t\t\t\t<td class=\"title_td_result_table\">Capabilities</td>\n");
  printf("\t\t\t\t\t<td class=\"title_td_result_table\">Updated (UPnP)</td>\n");
  printf("\t\t\t\t\t<td class=\"title_td_result_table\">Occupant</td>\n");
  printf("\t\t\t\t</tr>\n");
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
    printf("\t\t\t\t<tr>\n");
    printf("\t\t\t\t\t<td class=\"cell_padding%s%s%s%s\"><input name=\"checkbox_%s\" type=\"checkbox\" value=\"%s\" onchange=\"javascript:group_action_visibility();\" /></td>\n",
           $round_border_bottom_left,
           $round_border_bottom_right,
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           $result['id'], // TODO: make a 'clean_name' function that removes weird chars
           $result['id']);

    printf("\t\t\t\t\t<td class=\"white_bg cell_padding%s%s%s\">\n",
           $round_border_bottom_left,
           $round_border_bottom_right,
           $bottom_padding);
    printf("\t\t\t\t\t\t<div class=\"%s\" onclick=\"javascript:window.location='%s'\">%s</div>\n",
           $lock_style,
           $lock_link,
           $result['id']);
    printf("\t\t\t\t\t</td>\n");

    printf("\t\t\t\t\t<td class=\"cell_padding%s%s%s\"><a target=\"_blank\" href=\"http://%s/admin/maintenance.shtml\">%s</a>&nbsp;<a target=\"_blank\" href=\"?action=gui_device_info&device_id=%s&age=$age\"><img style=\"height: 1em;\" src=\"info.png\" alt=\"[i]\" /></a></td>\n",
           $round_border_bottom_left,
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           $result['ipv4'],
           $result['ipv4'],
           $result['id']);

    printf("\t\t\t\t\t<td class=\"cell_padding%s%s\">%s</td>\n",
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           $result['model_name']);

    printf("\t\t\t\t\t<td class=\"cell_padding%s%s\">%s</td>\n",
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           $result['firmware_version']);

    printf("\t\t\t\t\t<td class=\"cell_padding%s%s\">\n%s\n\t\t\t\t</td>\n",
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           get_capabilities_icons($result['capabilities'], 5));

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
    printf("\t\t\t\t\t<td class=\"cell_padding%s%s\">%s</td>\n",
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
    printf("\t\t\t\t\t<td class=\"cell_padding%s%s%s\">%s</td>\n",
           $round_border_bottom_right,
           $bottom_padding,
           ($results_index % 2 ? ' lighter_bg' : ''),
           $occupant_and_time_lapsed);

    printf("\t\t\t\t</tr>\n");
  }
  printf("\t\t\t\t<tr id=\"group_action_field\" style=\"display: none;\">\n");
  printf("\t\t\t\t\t<td colspan=\"8\" style=\"padding-top: 1em; padding-left: .5em;\">\n");
  printf("\t\t\t\t\t\t<img style=\"height: 2em; vertical-align: -0.3em;\" src=\"down_right_arrow.png\" alt=\"->\">\n");
  printf("\t\t\t\t\t\t<input type=\"hidden\" name=\"url\" value=\"%s\">\n", urlencode($current_request));
  printf("\t\t\t\t\t\t<select name=\"action\">\n");
  printf("\t\t\t\t\t\t\t<option value=\"move_to_portal\">Move to portal</option>\n");
  printf("\t\t\t\t\t\t</select>\n");
  printf("\t\t\t\t\t\t<input type=\"submit\" value=\"Go\" />\n");
  printf("\t\t\t\t\t</td>\n");
  printf("\t\t\t\t</tr>\n");
  printf("\t\t\t</table>\n\t\t\t</form>\n\t\t</td>\n\t</tr>\n\n");
  printf("</table>\n</td>\n</tr>\n\n");
  printf("<tr>\n<td style=\"font-size: .8em; color: gray; padding: 1em; text-align: center;\">found %s matches (%ss)</td>\n</tr></table>",
         count($results),
         request_time());
  printf("</body>\n</html>\n");
}

function lock_device($sql,
                     $user,
                     $capability,
                     $model_name,
                     $firmware_version,
                     $capability_state,
                     $device_id,
                     $age) {
  
  $results = $sql->call(sprintf("call lock_device(%s, %s, %s, %s, '%s', %d, %s)",
                                ($capability ? sprintf("'%s'", $capability): 'NULL'),
                                ($model_name ? sprintf("'%s'", $model_name) : 'NULL'),
                                ($firmware_version ? sprintf("'%s'", $firmware_version) : 'NULL'),
                                ($capability_state ? sprintf("'%s'", $capability_state): 'NULL'),
                                $user,
                                $age * 60,
                                ($device_id ? sprintf("'%s'", $device_id) : 'NULL')));

  if(is_array($results) && count($results) > 0) {
    $results =  $results[0];
  }

  return $results;
}

function unlock_device($sql, $device_id, $url) {

  $sql->call(sprintf("call unlock_device('%s')", $device_id));

  return 'OK';
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

  /* 16 minutes */
  $age = 16;
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

  $sort_by = null;
  if(isset($_POST['sort_by']) && !empty($_POST['sort_by'])) {
    $sort_by = $_POST['sort_by'];
  }
  else if(isset($_GET['sort_by']) && !empty($_GET['sort_by'])) {
    $sort_by = $_GET['sort_by'];
  }

  try {
    $results = list_devices($sql,
                            $capability,
                            $model_name,
                            $firmware_version,
                            $capability_state,
                            $locked_by,
                            $device_id,
                            $age,
                            $ip,
                            $sort_by);

    if(is_array($results) && count($results) > 0) {
      $json_results = json_encode($results[0]);

      printf("%s", $json_results);
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

/* Fetch device info */
case 'device_info':

  try {
    $device_id = null;
    if(isset($_POST['device_id']) && !empty($_POST['device_id'])) {
      $device_id = $_POST['device_id'];
    }
    else if(isset($_GET['device_id']) && !empty($_GET['device_id'])) {
      $device_id = $_GET['device_id'];
    }
    else {
      throw new Exception('Missing argument \'ID\'', 0);
    }

    $age = 16;
    if(isset($_POST['age']) && !empty($_POST['age'])) {
      $age = $_POST['age'];
    }
    else if(isset($_GET['age']) && !empty($_GET['age'])) {
      $age = $_GET['age'];
    }

    $result = device_info($sql,
                          $device_id,
                          $axis_device_credentials,
                          $age,
                          $axis_proxy_address,
                          $axis_proxy_port);

    printf("%s", json_encode(array($result['remote_service'], $result['oak'])));
  }
  catch(Exception $e) {
    header('HTTP/1.0 500');
    printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
    exit(1);
  }

  break;

/* Fetch device info */
case 'gui_device_info':

  try {
    $device_id = null;
    if(isset($_POST['device_id']) && !empty($_POST['device_id'])) {
      $device_id = $_POST['device_id'];
    }
    else if(isset($_GET['device_id']) && !empty($_GET['device_id'])) {
      $device_id = $_GET['device_id'];
    }
    else {
      throw new Exception('Missing argument \'ID\'', 0);
    }

    $age = 16;
    if(isset($_POST['age']) && !empty($_POST['age'])) {
      $age = $_POST['age'];
    }
    else if(isset($_GET['age']) && !empty($_GET['age'])) {
      $age = $_GET['age'];
    }

    $result = device_info($sql,
                          $device_id,
                          $axis_device_credentials,
                          $age,
                          $axis_proxy_address,
                          $axis_proxy_port);

    printf("<table border=\"1\"><tr><td style=\"font-weight: bold;\">ID</td><td style=\"font-weight: bold;\">Remote service</td><td style=\"font-weight: bold;\">OAK</td></tr>");
    printf("<tr><td>%s</td><td>%s</td><td>%s</td></tr></table>\n",
           $result['id'],
           str_replace(",", "<br />\n", $result['remote_service']),
           $result['oak']);
  }
  catch(Exception $e) {
    header('HTTP/1.0 500');
    printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
    exit(1);
  }

  break;

/* List devices in a minimal GUI */
case 'gui_list':

  try {
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
      throw new Exception('Missing argument \'user\'');
    }

    $device_id = null;
    if(isset($_POST['device_id']) && !empty($_POST['device_id'])) {
      $device_id = $_POST['device_id'];
    }
    else if(isset($_GET['device_id']) && !empty($_GET['device_id'])) {
      $device_id = $_GET['device_id'];
    }

    /* 16 minutes */
    $age = 16;
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

    $sort_by = null;
    if(isset($_POST['sort_by']) && !empty($_POST['sort_by'])) {
      $sort_by = $_POST['sort_by'];
    }
    else if(isset($_GET['sort_by']) && !empty($_GET['sort_by'])) {
      $sort_by = $_GET['sort_by'];
    }

    /* Capture the current location to add to all links */
    $current_request = $_SERVER['REQUEST_URI'];
    $current_script = $_SERVER['SCRIPT_NAME'];

    gui_list($user,
             $sql,
             $capability,
             $model_name,
             $firmware_version,
             $capability_state,
             $locked_by,
             $device_id,
             $age,
             $ip,
             $sort_by,
             $current_script,
             $current_request);
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
  $age = 16;
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
    $result = lock_device($sql,
                          $user,
                          $capability,
                          $model_name,
                          $firmware_version,
                          $capability_state,
                          $device_id,
                          $age);

    if(empty($url)) {
      header('Content-type: application/json; charset=utf-8');
      if(is_array($results) && count($results) > 0) {
        printf("%s", json_encode($results));
      }
      else {
        printf("[]");
      }
      printf("%s", $result);
    }
    else {
      header(sprintf("Location: %s", $url));
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

  $url = null;
  if(isset($_POST['url']) && !empty($_POST['url'])) {
    $url = urldecode($_POST['url']);
  }
  else if(isset($_GET['url']) && !empty($_GET['url'])) {
    $url = $_GET['url'];
  }

  try {

    $device_id = NULL;
    if(isset($_POST['device_id']) && !empty($_POST['device_id'])) {
      $device_id = $_POST['device_id'];
    }
    else if(isset($_GET['device_id']) && !empty($_GET['device_id'])) {
      $device_id = $_GET['device_id'];
    }
    else {
      throw new Exception('Missing argument \'ID\'');
    }

    $result = unlock_device($sql, $device_id);

    if(empty($url)) {
      printf("%s", $result);
    }
    else {
      header(sprintf("Location: %s", $url));
    }

  }
  catch(Exception $e) {
    header('HTTP/1.0 500');
    printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
    exit(1);
  }

  break;

/* Inform that the action is invalid */
default:
  printf('Invalid action');
  exit(0);
}

