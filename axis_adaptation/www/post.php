<?php
require_once('CapabilityManager.php');
require_once('AbusedResult.php');
require_once('SqlConnection.php');

/* Get the XML from the POST data */
$raw_xml = file_get_contents('php://input');

/* MySQL credentials */
$host     = 'localhost';
$database = 'abused';
$username = 'abused';
$password = 'abusedpass';

/* Parse the XML into an array of AbusedResult objects */
$abused_results = AbusedResult::parse_xml($raw_xml);

/* Info for extracting specific information of
   the device in the case where it is an AXIS device */
$axis_device_credentials = array(
  array(
    'username' => 'root',
    'password' => 'pass'),
  array(
    'username' => 'camroot',
    'password' => 'password'));

/* Connect to MySQL */
try {
  $sqlconnection = new SqlConnection($host, $database, $username, $password);
}
catch(Exception $e) {
  $err_msg = sprintf("Error: [%d] %s", $e->getCode(), $e->getMessage());
  error_log($err_msg);
  exit(1);
}

/* Insert or update for each AbusedResult we have received and parsed */
foreach($abused_results as $abused_result) {
  try {
    $mac = $abused_result->get_mac();
    /* Igore messages that dont have a mac
       (devices that aren't on the local net)*/
    if(empty($mac)) {
      continue;
    }

    /* If ID is not a MAC address then use the
       MAC address as ID (for AXIS compatibility)*/
    $id = $abused_result->get_serial_number();
    if(!$abused_result->is_mac_address(str_replace('I', '', $id))) {
      $id = preg_replace(array('/^0:/','/:0:/'),
                         array(':00:', '00:'),
                         $mac);
      $id = str_replace(':', '', $id);
      $id = strtoupper($id);
    }
    $ipv4 = $abused_result->get_ip();
    $ipv6 = NULL;
    if(!AbusedResult::is_ipv4_address($ipv4)) {
      if(AbusedResult::is_ipv6($ipv4)) {
        $ipv6 = $ipv4;
        $ipv4 = NULL;
      }
      else {
        /* Not an IP address, skip packet*/
        continue;
      }
    }

    $friendly_name = $abused_result->get_custom_field_friendlyName();

    $ssdp_message_type = NULL;
    try {
      /* Fetch the NTS header */
      $ssdp_message_type = $abused_result->get_header_nts();
      /* Explanation: Header 'NTS' is the type
         of ssdp message (ssdp::[hello|alive|bye]) */
      /* Split 'ssdp::<type>' into 'ssdp::' and '<type>'
         and only use the latter */
      $ssdp_message_type = explode(':', $ssdp_message_type)[1];
    } catch(Exception $e) {
      /* If message doesn't contain the ssdp type
         then it is broken, skip to next message */
      error_log('Message is broken, skipping');
      continue;
    }

    $model_name = $abused_result->get_custom_field_modelName();

    $cm = null;
    $model_firmware_id = null;

    /* If it is a QNAP (excluding 'TwonkyMedia Server' version
       '6.0.34', it is Iomega which has 2 instances of UPnP) */
    if(!(false === stripos($model_name, 'TwonkyMedia Server')) &&
       !(false === stripos($abused_result->get_custom_field_manufacturer(), 'PacketVideo')) &&
       (false === stripos($abused_result->get_custom_field_modelNumber(), '6.0.34'))) {

      /* Fetch the firmware version from the device */
      $firmware_version = $abused_result->get_custom_field_modelNumber();

      /* Create or retrieve existing ID for the model-firmware combination */
      $model_firmware_id = $sqlconnection->call(sprintf("call add_model_firmware_if_not_exist('%s', '%s')",
                                   $model_name,
                                   $firmware_version))[0][0]['id'];

      /* See if model-firmware combination has
         been probed for capabilities */
      $is_probed = $sqlconnection->call(sprintf("call is_model_firmware_probed('%s')", 
                                             $model_firmware_id))[0][0]['probed'] == 'yes' ? true : false;

      /* If device has not been probed for capabilities
         then probe it and fill the database */
      if(!$is_probed) {
        $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'nas\');')[0][0]['id'];
        $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
      }

    }

    /* If it is a AxisSTS NAS */
    // TODO: Remove when AVHS 2.24 is EOLd
    else if((!(false === stripos($model_name, 'lenovo ix')) ||
             !(false === stripos($model_name, 'iomega storcenter')))) { // TODO: test iomega NAS

      /* Fetch the firmware version from the device */
      $firmware_version = $abused_result->get_custom_field_modelNumber();

      /* Create or retrieve existing ID for the model-firmware combination */
      $model_firmware_id = $sqlconnection->call(sprintf("call add_model_firmware_if_not_exist('%s', '%s')",
                                   $model_name,
                                   $firmware_version))[0][0]['id'];

      /* See if model-firmware combination has
         been probed for capabilities */
      $is_probed = $sqlconnection->call(sprintf("call is_model_firmware_probed('%s')", 
                                             $model_firmware_id))[0][0]['probed'] == 'yes' ? true : false;

      /* If device has not been probed for capabilities
         then probe it and fill the database */
      if(!$is_probed) {
        $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'stsnas\');')[0][0]['id'];
        $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
      }

    }

    /* Else if it is an AXIS device */
    else if(!(false === stripos($model_name, 'axis'))) {

      $cm = new CapabilityManager(($ipv4 ? $ipv4 : $ipv6),
                                  null,
                                  $axis_device_credentials);

      /* Fetch the firmware version from the device */
      try {
        $firmware_version = $cm->get_firmware_version();
      }
      catch(Exception $e) {
        /* If the credentials were wrong try with the backup ones  */
        if($e->getCode() == 401) {
          $cm->set_credentials($axis_device_credentials);
          $firmware_version = $cm->get_firmware_version();
        }
        else {
          throw $e;
        }
      }

      /* Create or retrieve existing ID for the model-firmware combination */
      $model_firmware_id = $sqlconnection->call(sprintf("call add_model_firmware_if_not_exist('%s', '%s')",
                                   $model_name,
                                   $firmware_version))[0][0]['id'];

      /* See if model-firmware combination has
         been probed for capabilities */
      $is_probed = $sqlconnection->call(sprintf("call is_model_firmware_probed('%s')", 
                                             $model_firmware_id))[0][0]['probed'] == 'yes' ? true : false;
  
      /* If device has not been probed for capabilities
         then probe it and fill the database */
      if(!$is_probed) {
        if($cm->has_local_storage()) {
          $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'local_storage\');')[0][0]['id'];
          $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
        }
        if($cm->has_pir()) {
          $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'pir\');')[0][0]['id'];
          $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
        }
        if($cm->has_light_control()) {
          $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'light_control\');')[0][0]['id'];
          $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
        }
        if($cm->has_audio()) {
          $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'audio\');')[0][0]['id'];
          $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
        }
        if($cm->has_mic()) {
          $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'mic\');')[0][0]['id'];
          $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
        }
        if($cm->has_ptz()) {
          $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'ptz\');')[0][0]['id'];
          $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
        }
        if($cm->has_dptz()) {
          $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'dptz\');')[0][0]['id'];
          $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
        }
        if($cm->has_speaker()) {
          $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'speaker\');')[0][0]['id'];
          $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
        }
        if($cm->has_wlan()) {
          $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'wlan\');')[0][0]['id'];
          $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
        }
        if($cm->has_status_led()) {
          $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'status_led\');')[0][0]['id'];
          $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
        }
        if($cm->has_ir()) {
          $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'ir\');')[0][0]['id'];
          $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                       $model_firmware_id,
                                       $capability_id));
        }
      }
    }

    /* Else if device type isn't supported then skip it */
    else {
      continue;
    }

    $query = sprintf("call add_or_update_device('%s', '%s', '%s', '%s', '%s', '%s', '%s')",
                     $id,
                     $mac,
                     $ipv4,
                     $ipv6,
                     $friendly_name,
                     $model_firmware_id,
                     $ssdp_message_type);

    $sqlconnection->call($query);

    /* The following things have to be done AFTER an AXIS device has been
       registered (ex. because of a FOREIGN KEY to the device ID) */
    if(!(false === stripos($model_name, 'axis'))) {

      /* Always check SD_DISK state if the device has SD_DISK support */
      if($cm->has_sd_disk()) {
        $capability_id = $sqlconnection->call('call add_capability_if_not_exist(\'sd_disk\');')[0][0]['id'];
        $sqlconnection->call(sprintf("call add_capability_to_model_firmware('%s', '%s');",
                                     $model_firmware_id,
                                     $capability_id));

        $capability_state = $cm->get_sd_disk_status();
        $query = sprintf("call add_or_update_device_capability_state('%s', '%s', '%s');",
                         $id,
                         $capability_id,
                         $capability_state);
        $sqlconnection->call($query);
      }
    }

  } catch(Exception $e) {
    /* Print and continue */
    if ($e->getCode() != 401 && false == strpos($e->getMessage(), 'does not exist')) {
      $err_msg = sprintf("Error: [%d] %s", $e->getCode(), $e->getMessage());
      error_log($err_msg);
    }
    continue;
  }
}

