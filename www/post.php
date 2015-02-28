<?php
/*
  Configure MySQL with:
  CREATE DATABASE 'abused';

  CREATE TABLE `capabilities` (`id` INT NOT NULL AUTO_INCREMENT,
                               `name` VARCHAR(255) NOT NULL UNIQUE,
                               PRIMARY KEY(`id`)) ENGINE=InnoDB;

  CREATE TABLE `model_firmware` (`id` INT NOT NULL AUTO_INCREMENT,
                                 `model_name` VARCHAR(255) NOT NULL,
                                 `firmware_version` VARCHAR(255) NOT NULL,
                                 UNIQUE KEY(`model_name`, `firmware_version`),
                                 PRIMARY KEY(`id`)) ENGINE=InnoDB;

  CREATE TABLE `model_firmware_capability` (`model_firmware_id` INT NOT NULL,
                                            `capability_id` INT NOT NULL,
                                            FOREIGN KEY (`model_firmware_id`)
                                              REFERENCES `model_firmware`(`id`),
                                            FOREIGN KEY (`capability_id`)
                                              REFERENCES `capability`(`id`),
                                            PRIMARY KEY(`model_firmware_id`, `capability_id`)) ENGINE=InnoDB;
 
  CREATE TABLE `devices` (`id` VARCHAR(255) NOT NULL,
                          `mac` VARCHAR(255) NOT NULL,
                          `ipv4` VARCHAR(15),
                          `ipv6` VARCHAR(46),
                          `friendly_name` VARCHAR(255),
                          `model_firmware_id` INT,
                          `last_update` DATETIME NOT NULL,
                          `last_upnp_message` ENUM('hello', 'alive', 'bye') NOT NULL,
                          FOREIGN KEY (`model_firmware_id`) REFERENCES `model_firmware` (`id`),
                          PRIMARY KEY (`id`)) ENGINE=InnoDB;

  CREATE TABLE `locked_devices`(`device_id` VARCHAR(255) NOT NULL,
                                `locked` TINYINT(1) DEFAULT 1,
                                `locked_date` DATETIME NOT NULL,
                                `locked_by` VARCHAR(255) NOT NULL,
                                FOREIGN KEY (`device_id`)
                                  REFERENCES `devices`(`id`)
                                  ON DELETE CASCADE
                                  ON UPDATE CASCADE,
                                PRIMARY KEY (`device_id`, `state_date`)) ENGINE=InnoDB;

  CREATE USER 'abused'@'%' IDENTIFIED BY 'abusedpass';

  GRANT SELECT, INSERT, UPDATE ON `abused`.`devices` TO 'abused'@'%';

  GRANT SELECT, INSERT, UPDATE ON `abused`.`locked_devices` TO 'abused'@'%';

  DELIMITER //
  CREATE PROCEDURE `add_or_update_device`(IN `v_id` VARCHAR(255),
                                          IN `v_mac` VARCHAR(17),
                                          IN `v_ipv4` VARCHAR(15),
                                          IN `v_ipv6` VARCHAR(46),
                                          IN `v_friendly_name` VARCHAR(255),
                                          IN `v_model_firmware_id` INT,
                                          IN `v_last_upnp_message` VARCHAR(5))
    SQL SECURITY INVOKER
  BEGIN
    INSERT INTO `devices` VALUES(v_id,
                                 v_mac,
                                 v_ipv4,
                                 v_ipv6,
                                 v_friendly_name,
                                 v_model_firmware_id,
                                 NOW(),
                                 v_last_upnp_message)
      ON DUPLICATE KEY UPDATE
        `mac`=v_mac,
        `ipv4`=v_ipv4,
        `ipv6`=v_ipv6,
        `friendly_name`=v_friendly_name,
        `model_firmware_id`=v_model_firmware_id,
        `last_update`=NOW(),
        `last_upnp_message`=v_last_upnp_message;
  END//
  DELIMITER ;

  DELIMITER //
  CREATE PROCEDURE `add_model_firmware_if_not_exist`(IN `v_model_name` VARCHAR(255),
                                        IN `v_firmware_version` VARCHAR(255),
                                        INOUT `return_id` INT)
    SQL SECURITY INVOKER
  BEGIN
    SET return_id=null;
    SELECT `id` INTO return_id
      FROM `model_firmware`
      WHERE `model_name`=v_model_name
        AND `firmware_version`=v_firmware_version;
    IF return_id IS NULL THEN
      INSERT INTO `model_firmware` (`model_name`, `firmware_version`)
        VALUES(v_model_name, v_firmware_version);
      SELECT `id` INTO return_id
        FROM `model_firmware`
        WHERE `model_name`=v_model_name
          AND `firmware_version`=v_firmware_version;
    END IF;
  END
  DELIMITER ;

  DELIMITER //
  CREATE PROCEDURE `delete_inactive_devices`(IN `inactive_seconds` INT)
    SQL SECURITY INVOKER
  BEGIN
    DELETE FROM `devices`
      WHERE `last_update`<(SELECT NOW()-INTERVAL inactive_seconds SECOND);
  END//
  DELIMITER ;

  DELIMITER //
  CREATE PROCEDURE `is_device_locked_internal`(IN `v_device_id` VARCHAR(255),
                                               INOUT `is_locked` TINYINT(1))
    SQL SECURITY INVOKER
  BEGIN
    SELECT `locked` INTO is_locked
      FROM `locked_devices`
      WHERE `device_id`=v_device_id
        AND `locked`=1;
  END//
  DELIMITER ;

  DELIMITER //
  CREATE PROCEDURE `lock_device`(IN `v_device_id` VARCHAR(255),
                                 IN `v_locked_by` VARCHAR(255))
    SQL SECURITY INVOKER
  BEGIN 
    DECLARE is_locked INT DEFAULT 0;
    CALL `is_device_locked_internal`(v_device_id, is_locked);
    IF is_locked=1 THEN
      SELECT 0 AS `success`;
    ELSE
      INSERT INTO `locked_devices`
        VALUES(v_device_id, 1, NOW(), v_locked_by);
      SELECT 1 AS `success`;
    END IF;
  END//
  DELIMITER ;

  DELIMITER //
  CREATE PROCEDURE `unlock_device`(IN `v_device_id` VARCHAR(255))
    SQL SECURITY INVOKER
  BEGIN
    DELETE FROM `locked_devices`
    WHERE `locked`=1
    AND `device_id`=v_device_id;
  END//
  DELIMITER ;

  GRANT EXECUTE ON PROCEDURE abused.add_device TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.is_device_locked TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.delete_inactive_devices TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.is_device_locked_internal TO 'abused'@'%';
 */

class AbusedResult {
  private $raw_xml;
  private $mac;
  private $ip;
  private $request_protocol;
  private $request;
  private $datetime;
  private $custom_fields_size;
  private $custom_fields;
  private $upnp_headers_size;
  private $upnp_headers;

  function __construct($raw_xml, $mac, $ip, $request_protocol, $request, $datetime, $custom_fields_size, $custom_fields, $upnp_headers_size, $upnp_headers) {
    $this->raw_xml = $raw_xml;
    $this->mac = $mac;
    $this->ip = $ip;
    $this->request_protocol = $request_protocol;
    $this->request = $request;
    $this->datetime = $datetime;
    $this->custom_fields_size = $custom_fields_size;
    $this->custom_fields = $custom_fields;
    $this->upnp_headers_size = $upnp_headers_size;
    $this->upnp_headers = $upnp_headers;
  }

  public function set_raw_xml($raw_xml) {
    if($this->raw_xml == null) {
      $this->raw_xml = $raw_xml;
    }
  }

  public static function parse_xml($raw_xml) {
    $parsed_xml = simplexml_load_string($raw_xml);
    if(false === $parsed_xml) {
      $msg = "Error: Failed to parse XML\n";
      printf($msg);
      error_log($msg);
      foreach(libxml_get_errors() as $error) {
        printf($error->message);
        error_log($error->message);
      }
      exit(1);
    }

    $abused_results = array();
    for($i = 0; $i < count($parsed_xml->message); $i++) {
      $abused_result = AbusedResult::parse_abused_message($parsed_xml->message[$i]);
      $abused_result->set_raw_xml($raw_xml);
      $abused_results[] = $abused_result;
    }

    return $abused_results;
  }

  public static function parse_abused_message(SimpleXMLElement $message) {

    /* Gather all custom_fields */
    $custom_fields = array();
    $custom_field_array = $message->custom_fields->custom_field;
    $custom_fields_size = count($custom_field_array);
    for($i = 0; $i < $custom_fields_size; $i++) {
      $custom_field_attributes = $custom_field_array[$i]->attributes();
      $custom_field_name = trim((string) $custom_field_attributes['name']);
      $custom_field_value = trim((string) $custom_field_array[$i]);
      $custom_fields[$custom_field_name] = $custom_field_value;
    }

    /* Gather all headers */
    $headers = array();
    $header_array = $message->headers->header;
    $headers_size = count($header_array);
    for($i = 0; $i < $headers_size; $i++) {
      $header_attributes = $header_array[$i]->attributes();
      $header_name = trim((string) $header_attributes['typeStr']);
      $header_value = trim((string) $header_array[$i]);
      $headers[$header_name] = $header_value;
    }

    /* Get request protocol */
    $request_attributes = $message->request->attributes();
    $request_protocol = trim((string) $request_attributes['protocol']);

    /* Build AbusedResult object */
    $abused_result = new AbusedResult(
      null,
      trim((string) $message->mac),
      trim((string) $message->ip),
      $request_protocol,
      trim((string) $message->request),
      trim((string) $message->datetime),
      $custom_fields_size,
      $custom_fields,
      $headers_size,
      $headers
    );

    return $abused_result;
  }

  /* Make magic getters for all fields
     eg. object->get_mac() returns the field AbusedResult::mac */
  public function __call($name, $args) {
    $match = preg_match("/^get_([A-Za-z0-9_]+)$/", $name, $matches);
    if($match) {

      /* If it is a request for a custom field */
      if(0 === strpos($matches[1], 'custom_field_')) {

        /* Try to locate the custom field and return it's value */
        foreach($this->custom_fields as $cf_name => $cf_value) {
          if($cf_name == substr($matches[1], 13)) {
            return $cf_value;
          }
        }
        throw new Exception(sprintf("Custom field '%s' does not exist", $matches[1]));
      }
      else {

        /* Try to call a method with the matched name */
        try {
          $funct_name = $matches[1];
          return $this->$funct_name;
        } catch(Exception $e) {
          throw new Exception(sprintf("Field '%s' does not exist", $matches[1]));
        }

      }

    }
    else {
      throw new Exception(sprintf("Method not found ('%s')", $name));
    }
  }

  /**
   * Checks if the passed argument is a valid MAC address
   *
   * @param $mac The MAC address to be checked
   *
   * @return Returns true if it is a valid MAC address, otherwise returns false
   */
  public static function is_mac_address($mac) {
    if(preg_match("/^[0-9a-fA-F]{2}(?=([:.]?))(?:\\1[0-9a-fA-F]{2}){5}$/", $mac))
      return true;
    return false;
  }
  
  /**
   * Checks if the passed argument is a valid IPv4 address
   *
   * @param $ip The IPv4 address to be checked
   *
   * @return Returns true if it is a valid IPv4 address, otherwise returns false
   */
  public static function is_ipv4_address($ip) {
    if(preg_match("/^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/", $ip)) {
      $ipParts=explode(".",$ip);
      foreach($ipParts as $ipPart) {
        if(intval($ipPart)<0 || intval($ipPart)>255)
          return false;
      }
      return true;
    }
    return false;
  }
  
  /**
   * Check if a given IPv6 address is valid
   *
   * @param $ip The IPv6 address to be checked
   *
   * @return Returns true if the given IPv6 is valid, otherwise returns false
   */
  public static function is_ipv6_address($ip) {
    /* Exit for localhost */
    if (strlen($ip) < 3)
      return $ip == '::';
    /* Check if end part is in IPv4 format */
    if (strpos($ip, '.')) {
      $lastcolon = strrpos($ip, ':');
      if (!($lastcolon && $this->is_ipv4_address(substr($ip, $lastcolon + 1))))
        return false;
      /* Replace the IPv4 part with IPv6 dummy part */
      $ip = substr($ip, 0, $lastcolon) . ':0:0';
    }
    /* Check for uncompressed address format */
    if (strpos($ip, '::') === false) {
      return preg_match('/^(?:[a-f0-9]{1,4}:){7}[a-f0-9]{1,4}$/i', $ip);
        }
    //count colons for compressed format
    if (substr_count($ip, ':') < 8) {
      return preg_match('/^(?::|(?:[a-f0-9]{1,4}:)+):(?:(?:[a-f0-9]{1,4}:)*[a-f0-9]{1,4})?$/i', $ip);
        }
    return false;
  }
  
  /**
   * Check if a given IP address is valid
   *
   * @param $ip The IP address to be checked
   *
   * @return Returns true if the gevin IP is valid, otherwise returns false
   */
  public static function is_ip_address($ip) {
    return $this->is_ipv4_address($ip) || $this->is_ipv6_address($ip);
  }
}


class CapabilityManager {

  $ip = '';
  $username = '';
  $password = '';

  public function __construct($ip, $username = 'root', $password = 'pass') {
    $this->ip = $ip;
    $this->username = $username;
    $this->password = $password;
  }

  /**
   * Sets a parameter in the camera
   *
   * @param value The value the parameter should be set to
   *
   * @return Returns the parameter's current value as a string
   */
  public function setDeviceParameter($parameter, $value, $donotEncode = false) {
    $result = null;
    $context = stream_context_create(array(
    'http' => array(
        'header'  => sprintf("Authorization: Basic %s",
                             base64_encode(sprintf("%s:%s",
                                                   $this->username,
                                                   $this->password))),
        'timeout' => '5'
      )
    ));

//*********************
//STUFF NEEDED TO GET DIGEST WORKING!
//stream_context_set_option($context, 'ssl', 'verify_host', false);
//********************

    $result = @file_get_contents("http://".$this->ip."/axis-cgi/param.cgi?action=update&".$parameter."=".($donotEncode?$value:rawurlencode($value)), false, $context);
    if($result === false) {
      if(isset($http_response_header) && strpos($http_response_header[0], "401"))
        throw new Exception("Wrong credentials", 1);
      throw new Exception("Connection failed", 1);
    }
    if($result!="OK") {
      throw new Exception("Did not receive OK from device (result=\"".$result."\")", 1);
    }
    return true;
  }

  /**
   * Retrieves a parameter from the device
   *
   * @param parameter The parameter group
   *
   * @return Returns the parameter's current value as a string
   */
  public function get_axis_device_parameter($parameter) {
    $result = null;
    $context = stream_context_create(array(
      'http' => array(
          'header'  => sprintf("Authorization: Basic %s",
                               base64_encode(sprintf("%s:%s",
                                                     $this->username,
                                                     $this->password))),
          'timeout' => '5'
        )
      ));

    $result = @file_get_contents("http://".$this->ip."/axis-cgi/param.cgi?action=list".($parameter?"&group=".$parameter:""), false, $context);

    if(false === $result) {
      if(isset($http_response_header) && strpos($http_response_header[0], "401")) {
        throw new Exception("Wrong credentials", 1);
      }
      throw new Exception("Connection failed", 1);
    } else if(preg_match("/^# Error:/", $result)) {
      throw new Exception($result, 1);
    }
    return trim(substr($result, strlen($parameter)+1));
  }
  
  
  /**
   * Enables/disables the SSH server on the device
   *
   * @param enable True to enable or false to disable the SSH server
   *
   * @return Returns true on success and false on failure
   */
  public function set_device_ssh($enable) {
    if($enable) {
      $enable = "yes";
    } else {
      $enable = "no";
    }
    if($this->is_device_ssh_capable()) {
      throw new Exception("The device is not SSH capable", 1);
    }
    $this->set_device_parameter("Network.SSH.Enabled", $enable);
    return true;
  }
  
  /**
   * Enables the SSH server on the device
   *
   * @return Returns true on success and false on failure
   */
  public function enable_device_ssh() {
    return $this->set_device_ssh(true);
  }
  
  /**
   * Disables the SSH server on the device
   *
   * @return Returns true on success and false on failure
   */
  public function disable_device_ssh() {
    return $this->set_device_ssh(false);
  }
  
  /**
   * Checks if the device firmware is SSH capable (>=LFP5)
   *
   * @return Returns true on success and false on failure
   */
  public function is_device_ssh_capable() {
    try {
      $parameter = 'Network.SSH.Enabled';
      $this->get_device_parameter($parameter);
    } catch(Exception $e) {
      if(preg_match("/# Error:/", $e->getMessage()))
        return false;
      throw $e;
    }
    return true;
  }
  
  /**
   * Checks if the device SSH server is enabled
   *
   * @return Returns true on success and false on failure
   */
  public function is_device_ssh_enabled() {
    try {
      $parameter = "Network.SSH.Enabled";
      $result = $this->get_device_parameter($parameter);
    } catch(Exception $e) {
      if(preg_match("/# Error:/", $e->getMessage()))
        return false;
      throw $e;
    }
    if('no' == $result)
      return false;
    return true;
  }

  public function get_capability() {
    
  }

}

/* Get the XML from the POST data */
$raw_xml = file_get_contents('php://input');

/* Parse the XML into an array of AbusedResult objects */
$abused_results = AbusedResult::parse_xml($raw_xml);

/* Info for extracting specific information of
   the device in case it is an AXIS device */
$axis_device_default_username = 'root';
$axis_device_default_password = 'pass';
$axis_device_tfw_username     = 'tfwroot';
$axis_device_tfw_password     = 'tfwpass';

/* MySQL host, database and credentials */
$host     = 'localhost';
$database = 'abused';
$username = 'abused';
$password = 'abusedpass';

/* Connect to MySQL */
$h_sql = new mysqli($host, $username, $password, $database);
if ($h_sql->connect_errno) {
  printf("Failed to connect to MySQL: %s", $h_sql->connect_error);
  exit(1);
}

/* Insert or update for each AbusedResult we have received and parsed */
foreach($abused_results as $abused_result) {
  try {
    $id = $abused_result->get_mac();
    $mac = $abused_result->get_mac();
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
    $model_name = $abused_result->get_custom_field_modelName();
    $friendly_name = $abused_result->get_custom_field_friendlyName();
    $model_number = $abused_result->get_custom_field_modelNumber();
  } catch(Exception $e) {
    /* Do noting */
    continue;
  }
  $query = sprintf("call add_device('%s', '%s', '%s', '%s', '%s', '%s', '%s')",
                   $id,
                   $mac,
                   $ipv4,
                   $ipv6,
                   $model_name,
                   $friendly_name,
                   $model_number);

  $res = $h_sql->query($query);

  if(false === $res) {
    $msg = sprintf("Failed to query MySQL: %s", $h_sql->error); 
    printf("%s", $msg);
    error_log($msg);
  }
}

