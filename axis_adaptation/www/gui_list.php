<?php
require_once('Controller.php');

$request_start = microtime(true);

function request_time($request_start) {
  return round((microtime(true) - $request_start), 4);
}

function random_pleasing_color() {
  $red = rand(128, 236);
  $green = rand(128, 236);
  $blue = rand(128, 236);

  return sprintf("rgb(%d, %d, %d)", $red, $green, $blue);
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

try {

  $controller = new Controller();

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

  $results = $controller->list_devices($capability,
                                       $model_name,
                                       $firmware_version,
                                       $capability_state,
                                       $age,
                                       $locked_by,
                                       $device_id,
                                       $ip,
                                       $sort_by);

  if(isset($results[0])) {
    $results = $results[0];
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
?>
<!DOCTYPE html>
<html>
<head>
  <title>List of devices</title>
  <link rel="stylesheet" type="text/css" href="gui.css" />
  <script type="text/javascript" src="gui.js"></script>
</head>
<body>
  <table style="margin-left: auto; margin-right: auto;">
    <tr>
      <td style="font-size: .8em; color: gray; padding-left: 2em;">
        Logged in as '<?=$user?>'
      </td>
    </tr>
    <tr>
      <td>
        <table class="round_table" style="background-color: <?=$random_color?>">
          <tr>
            <td colspan="8" style="padding-bottom: 2em;">
              <form methon="get" action="">
                <table style="margin-left: auto; margin-right: auto;">
                  <tr>
                    <td class="title_td_header_table">
                      ID: 
                    </td>
                    <td>
                      <input type="input" name="device_id" value="<?=$device_id?>" />
                    </td>
                    <td class="title_td_header_table">
                      IP (v4):
                    </td>
                    <td>
                      <input type="input" name="ip" value="<?=$ip?>" />
                    </td>
                    <td class="title_td_header_table">
                      Capability:
                    </td>
                    <td>
                      <input type="input" name="capability" value="<?=$capability?>" />
                    </td>
                    <td rowspan="3" style="text-align: center;">
                      <input type="hidden" name="user" value="<?=$user?>" />
                      <input type="submit" class="apply_button" style="border: none; background-color: transparent; box-shadow: none; margin-left: 1em; margin-right: 2em;" title="Apply filters" value="" />
                    </td>
                  </tr>
                  <tr>
                    <td class="title_td_header_table">
                      <span title="For capability 'sd_disk' state can be: OK, connected, disconnected or failed">Capability state:</span>
                    </td>
                    <td>
                      <input type="input" name="capability_state" value="<?=$capability_state?>" />
                    </td>
                    <td class="title_td_header_table">
                      Model name:
                    </td>
                    <td>
                      <input type="input" name="model_name" value="<?=$model_name?>" />
                    </td>
                    <td class="title_td_header_table">
                      Firmware version:
                    </td>
                    <td>
                      <input type="input" name="firmware_version" value="<?=$firmware_version?>" />
                    </td>
                  </tr>
                  <tr>
                    <td class="title_td_header_table">
                      Occupant:
                    </td>
                    <td>
                      <input type="input" name="locked_by" value="<?=$locked_by?>" />
                    </td>
                    <td class="title_td_header_table">
                      Age (minutes):
                    </td>
                    <td>
                      <input type="input" name="age" value="<?=$age?>" />
                    </td>
                    <td class="title_td_header_table">
                      Sort by:
                    </td>
                    <td>
                      <select style="width: 100%;" name="sort_by">
                        <option <?=(empty($sort_by) ? 'selected="selected" ' : '')?>value="">none</option>
<?php
  foreach($result_columns as $option) {
    printf("                        <option %svalue=\"%s\">%s</option>\n",
           ($option == $sort_by ? 'selected="selected" ' : ''),
           $option,
           $option);
  }
?>
                      </select>
                    </td>
                  </tr>
                </table>
              </form>
            </td>
          </tr>
          <tr>
            <td>
              <form id="group_action_form" method="post" action="./">
                <table class="no_cell_space" style="margin: none; width: 100%;">
                  <tr>
                    <td class="title_td_result_table">&nbsp;</td>
                    <td class="title_td_result_table border_top_left_radius border_top_right_radius white_bg">ID</td>
                    <td class="title_td_result_table">IP (v4)</td>
                    <td class="title_td_result_table">Model</td>
                    <td class="title_td_result_table">Firmware version</td>
                    <td class="title_td_result_table">Capabilities</td>
                    <td class="title_td_result_table">Updated (UPnP)</td>
                    <td class="title_td_result_table">Occupant</td>
                  </tr>
<?php
  $results_size = count($results);
  $results_index = 0;
  foreach($results as $result) {
    $lock_style = 'div_device_bookable';
    $lock_link = sprintf("./?action=lock_device&amp;device_id=%s&amp;user=%s&amp;age=%s&amp;url=%s",
                         $result['id'],
                         $user,
                         $age,
                         urlencode($current_request));
    if(!empty($result['locked_by'])) {
      $lock_style = 'div_device_booked';
      $lock_link = sprintf("./?action=unlock_device&amp;device_id=%s&amp;url=%s",
                           $result['id'],
                           urlencode($current_request));
    }
    $round_border_bottom_left = '';
    $round_border_bottom_right = '';
    $bottom_padding = '';
    $lighter_bg = ' lighter_bg_gradient ';
    if($results_index++ == $results_size - 1) {
      $round_border_bottom_left = ' border_bottom_left_radius';
      $round_border_bottom_right = ' border_bottom_right_radius';
      $bottom_padding = ' bottom_cell';
      $lighter_bg = ' lighter_bg_gradient_last ';
    }
?>
                  <tr>
<?php
    $class = sprintf("%s%s%s%s",
                     $round_border_bottom_left,
                     $round_border_bottom_right,
                     $bottom_padding,
                     ($results_index % 2 ? $lighter_bg : ''));
?>
                    <td class="cell_padding<?=$class?>">
                      <input name="device_ids[]" type="checkbox" value="<?=$result['id']?>" onchange="javascript:group_action_visibility();" />
                    </td>
<?php
    $class = sprintf("%s%s%s",
                     $round_border_bottom_left,
                     $round_border_bottom_right,
                     $bottom_padding);
?>
                    <td class="white_bg cell_padding<?=$class?>">
                      <div class="<?=$lock_style?>" onclick="javascript:window.location='<?=$lock_link?>'"><?=$result['id']?></div>
                    </td>
<?php
    $class = sprintf("%s%s%s",
                     $round_border_bottom_left,
                     $bottom_padding,
                     ($results_index % 2 ? $lighter_bg : ''));
?>
                    <td class="cell_padding<?=$class?>">
                      <a target="_blank" href="http://<?=$result['ipv4']?>/admin/maintenance.shtml"><?=$result['ipv4']?></a>
                      &nbsp;
                      <a target="_blank" href="./?action=gui_device_info&device_id=<?=$result['id']?>&age=<?=$age?>">
                        <img style="height: 1em;" src="info.png" alt="[i]" />
                      </a>
                    </td>
<?php
    $class = sprintf("%s%s",
                     $bottom_padding,
                     ($results_index % 2 ? $lighter_bg : ''));
?>
                    <td class="cell_padding<?=$class?>"><?=$result['model_name']?></td>
                    <td class="cell_padding<?=$class?>"><?=$result['firmware_version']?></td>
                    <td class="cell_padding<?=$class?>">
<?=get_capabilities_icons($result['capabilities'], 6)?>
                    </td>
<?php
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
    $class = sprintf("%s%s",
                     $bottom_padding,
                     ($results_index % 2 ? $lighter_bg : ''));
?>
                    <td class="cell_padding<?=$class?>"><?=$time_lapsed?></td>
<?php
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
    $class = sprintf("%s%s%s",
                     $round_border_bottom_right,
                     $bottom_padding,
                     ($results_index % 2 ? $lighter_bg : ''));
?>
                    <td class="cell_padding<?=$class?>"><?=$occupant_and_time_lapsed?></td>
                  </tr>
<?php
  }
?>
                  <tr id="group_action_field" style="display: none;">
                    <td colspan="8" style="padding-top: 1em; padding-left: .5em;">
                      <img style="height: 2em; vertical-align: -0.3em;" src="down_right_arrow.png" alt="->">
                      <input type="hidden" id="portal_ips" name="portal_ips" value="">
                      <input type="hidden" name="url" value="<?=urlencode($current_request)?>">
                      <select id="select_action" name="action">
                        <option value="dispatch_devices">Dispatch to portal</option>
                        <option value="enable_avhs">Enable AVHS</option>
                        <option value="restart_avhs">Restart AVHS</option>
                        <option value="factory_default">Factory default</option>
                      </select>
                      <input type="submit" value="Go" onclick="return dropdown_check();" />
                    </td>
                  </tr>
                </table>
              </form>
            </td>
          </tr>
        </table>
      </td>
    </tr>
    <tr>
      <td style="font-size: .8em; color: gray; padding: 1em; text-align: center;">
        found <?=count($results)?> matches (<?=request_time($request_start)?>s)
      </td>
    </tr>
  </table>
  <script type="text/javascript">group_action_visibility();</script>
  </body>
</html>
<?php
}
catch(Exception $e) {

  header('HTTP/1.0 500');
  printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
  exit(1);

}
