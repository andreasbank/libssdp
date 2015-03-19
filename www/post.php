<?php
/*
  Configure MySQL with:
  CREATE DATABASE `abused`;

  CREATE TABLE `capabilities` (`id` INT NOT NULL AUTO_INCREMENT,
                               `name` VARCHAR(255) NOT NULL UNIQUE,
                               `group` VARCHAR(255) NOT NULL UNIQUE,
                               PRIMARY KEY(`id`)) ENGINE=InnoDB;

  CREATE TABLE `model_firmware` (`id` INT NOT NULL AUTO_INCREMENT,
                                 `model_name` VARCHAR(255) NOT NULL,
                                 `firmware_version` VARCHAR(255) NOT NULL,
                                 `last_updated` DATETIME,
                                 UNIQUE KEY(`model_name`, `firmware_version`),
                                 PRIMARY KEY(`id`)) ENGINE=InnoDB;

  CREATE TABLE `model_firmware_capability` (`model_firmware_id` INT NOT NULL,
                                            `capability_id` INT NOT NULL,
                                            FOREIGN KEY (`model_firmware_id`)
                                              REFERENCES `model_firmware`(`id`),
                                            FOREIGN KEY (`capability_id`)
                                              REFERENCES `capabilities`(`id`),
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
                                `locked_by` VARCHAR(255) NOT NULL,
                                `locked_date` DATETIME NOT NULL,
                                FOREIGN KEY (`device_id`)
                                  REFERENCES `devices`(`id`)
                                  ON DELETE CASCADE
                                  ON UPDATE CASCADE,
                                PRIMARY KEY (`device_id`, `locked_date`)) ENGINE=InnoDB;

  CREATE USER 'abused'@'%' IDENTIFIED BY 'abusedpass';

  GRANT SELECT, INSERT, UPDATE ON `abused`.`devices` TO 'abused'@'%';

  GRANT SELECT, INSERT, UPDATE ON `abused`.`locked_devices` TO 'abused'@'%';

  DELIMITER //
  CREATE PROCEDURE `add_capability_if_not_exist`(IN `v_capability_name` VARCHAR(255),
                                                 IN `v_capability_group` VARCHAR(255))
    SQL SECURITY INVOKER
  BEGIN
      DECLARE found_id INT DEFAULT NULL;
      SELECT `id` INTO found_id
        FROM `capabilities`
        WHERE `name`=v_capability_name
          AND `group`=v_capability_group;
      IF found_id IS NULL THEN
        INSERT INTO `capabilities` (`name`, `group`)
          VALUES(v_capability_name, v_capability_group);
      END IF;
      SELECT `id`
        FROM `capabilities`
        WHERE `name`=v_capability_name
          AND `group`=v_capability_group;
  
  END//
  DELIMITER ;

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
                                        IN `v_firmware_version` VARCHAR(255))
    SQL SECURITY INVOKER
  BEGIN
    DECLARE found_id INT DEFAULT NULL;
    SELECT `id` INTO found_id
      FROM `model_firmware`
      WHERE `model_name`=v_model_name
        AND `firmware_version`=v_firmware_version;
    IF found_id IS NULL THEN
      INSERT INTO `model_firmware` (`model_name`, `firmware_version`)
        VALUES(v_model_name, v_firmware_version);
    END IF;
    SELECT `id`
    FROM `model_firmware`
    WHERE `model_name`=v_model_name
    AND `firmware_version`=v_firmware_version;
  END//
  DELIMITER ;

  DELIMITER ;
  CREATE PROCEDURE `is_model_firmware_probed`(IN `v_model_firmware_id` INT)
    SQL SECURITY INVOKER
  BEGIN
    DECLARE v_count INT DEFAULT 0;
    SELECT COUNT(*) INTO v_count
    FROM `model_firmware_capabilities`
    WHERE `model_firmware_id`=v_model_firmware_id;
    IF v_count > 0 THEN
      SELECT 'yes' AS `Probed`;
    ELSE
      SELECT 'no' AS `Probed`;
    END IF;
  END//
  DELIMITER ;

  DELIMITER //
  CREATE PROCEDURE `add_capability_to_model_firmware` (IN `v_capability_id` INT,
                                                       IN `v_model_firmware` INT)
    SQL SECURITY INVOKER
  BEGIN
    INSERT INTO `model_firmware_capability`
    VALUES (
      v_capability, v_model_firmware
    );
  END//
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
  private $serial_number;
  private $mac;
  private $ip;
  private $request_protocol;
  private $request;
  private $datetime;
  private $custom_fields_size;
  private $custom_fields;
  private $upnp_headers_size;
  private $upnp_headers;

  public function __construct($raw_xml, $serial_number, $mac, $ip, $request_protocol, $request, $datetime, $custom_fields_size, $custom_fields, $upnp_headers_size, $upnp_headers) {
    $this->raw_xml = $raw_xml;
    $this->serial_number = $serial_number;
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

  private $ip = '';
  private $username = '';
  private $password = '';
  private $timeout = 5;

  public function __construct($ip, $username = 'root', $password = 'pass') {
    $this->ip = $ip;
    $this->username = $username;
    $this->password = $password;
  }

  /**
   * Set the timeout for the connections
   */
  public function set_timeout($timeout) {
    if(!is_int($timeout) || (empty($timeout) && $timeout != 0) ||
       $timeout < 0 || $timeout > 300) {
      throw new Exception('Timeout must be a positive integer');
    }
    $this->timeout = $timeout;
  }

  /**
   *  Generates a new nonce
   */
  public function generate_nonce($bits = 256) {
    $bytes = ceil($bits / 8);
    $cnonce = '';
    $random_string = '';
    for($i = 0; $i < $bytes; $i++) {
      $random_string = sprintf("%s%s", $random_string, chr(mt_rand(0, 255)));
    }
    $cnonce = hash('sha512', $random_string);
    return $cnonce;
  }

  /* Build a DIGEST authentication header */
  public function build_digest_auth_header($headers, $uri) {
    /*
    response is something like this:
    HTTP/1.0 401 Unauthorized
    Server: HTTPd/0.9
    Date: Sun, 1 Mar 2015 20:26:47 GMT
    WWW-Authenticate: Digest realm="testrealm@host.com",
                             qop="auth,auth-int",
                             nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093",
                             opaque="5ccc069c403ebaf9f0171e9517f40e41"
    */

    /* Extract the WWW-Authenticate header */
    $header = null;
    foreach($headers as $header_) {
      if(strstr($header_, 'WWW-Authenticate: Digest') != false) {
        $header = $header_;
        break;
      }
    }

    $auth_args = array();

    /* Extract the digest args from the WWW-Authenticate header */
    $match_success = preg_match("/WWW-Authenticate:\sDigest\s(.*)/", $header, $matches);
    if($match_success) {

      /* Clean the digest authorization arguments
         and create an associative array */
      $raw_arguments = split(',', $matches[1]);
      foreach($raw_arguments as $raw_argument) {
        $argument = split('=', trim($raw_argument), 2);
        if(!empty($argument[1]) && !empty($argument[0])) {
          $auth_args[$argument[0]] = str_replace('"', '', $argument[1]);
        }
      }

    }

    /* Check if 'auth' (normal digest) authentication
       method is supported and if authenticating against
       a 'realm' */
    if(!in_array('auth', split(',', $auth_args['qop'])) ||
       !in_array('realm', array_keys($auth_args))) {
      throw new Exception('The server requested a unsupported digest authentication method');
    }

    /* Set our args; RFC2069: qop=auth */
    $auth_args['qop'] = 'auth';
    $nc = 1;
    $cnonce = $this->generate_nonce();

    /* Build the header response field according to
       http://tools.ietf.org/html/rfc2069,
       note that we only implement needed features
       and make alot of assumptions (as using 'realm') */
    $ha1 = md5(sprintf("%s:%s:%s", $this->username, $auth_args['realm'], $this->password));
    $ha2 = md5(sprintf("GET:%s", $uri));
    $response = md5(sprintf("%s:%s:%d:%s:%s:%s",
                            $ha1,
                            $auth_args['nonce'],
                            $nc,
                            $cnonce,
                            $auth_args['qop'],
                            $ha2));

    $auth_digest = sprintf("Authorization: Digest username=\"%s\"", $this->username);
    $auth_digest = sprintf("%s, realm=\"%s\"", $auth_digest, $auth_args['realm']);
    $auth_digest = sprintf("%s, nonce=\"%s\"", $auth_digest, $auth_args['nonce']);
    $auth_digest = sprintf("%s, uri=\"%s\"", $auth_digest, $uri);
    $auth_digest = sprintf("%s, qop=%s", $auth_digest, $auth_args['qop']);
    $auth_digest = sprintf("%s, nc=%d", $auth_digest, $nc);
    $auth_digest = sprintf("%s, cnonce=\"%s\"", $auth_digest, $cnonce);
    $auth_digest = sprintf("%s, response=\"%s\"", $auth_digest, $response);
    if(array_key_exists('opaque', $auth_args)) {
      $auth_digest = sprintf("%s, response=\"%s\"", $auth_digest, $auth_args['opaque']);
    }

    return $auth_digest;
  }

  /**
   * Build basic authentication header
   */
  public function build_basic_auth_header() {
    $auth_basic = sprintf("Authorization: Basic %s",
                          base64_encode(sprintf("%s:%s",
                                                $this->username,
                                                $this->password)));
    return $auth_basic;
  }

  /**
   * Build the request context
   */
  public function build_context($auth_header = null) {

    /* If no specific authentication header given
       build a basic authentication header and use it*/
    if(empty($auth_header)) {
      $auth_header = $this->build_basic_auth_header();
    }

    $context = stream_context_create(array(
                                       'http' => array(
                                         'header'  => $auth_header,
                                         'timeout' => $this->timeout
                                       )));
    return $context;
  }

  /**
   * Send a http(s) request
   */
  public function send_request($url) {
    $result = null;

    /* Extract transport method [http|https] */
    $transport = strstr($url, '://', true);
    if($transport != 'http' && $transport != 'https') {
      throw new Exception('Wrong transport method, only HTTP or HTTPS is allowed');
    }

    $context = $this->build_context();

    //*********************
    // KEEP IN CASE OF ALIENS
    //if($transport == 'https') {
    //  stream_context_set_option($context, 'ssl', 'verify_host', false);
    //  stream_context_set_option($context, 'ssl', 'allow_self_signed', true);
    //  stream_context_set_option($context, 'ssl', 'verify_peer', false);
    //}
    //********************

    /* Try with basic authentication */
    $result = @file_get_contents($url, false, $context);

    /* Check if digest is needed */
    if($result === false &&
       isset($http_response_header) &&
       strpos($http_response_header[0], '401')) {

      /* Retry using digest authentication */
      $context = $this->build_context($this->build_digest_auth_header($http_response_header,
                                                                      $url));
      $result = @file_get_contents($url, false, $context);
    }

    if($result === false) {
      if(isset($http_response_header) && strpos($http_response_header[0], '401')) {
        throw new Exception("Wrong credentials", 401);
      }
      throw new Exception("Connection failed", 998);
    }

    return $result;
  }

  /**
   * Sets a parameter in the camera
   *
   * @param parameter The parameter to be set
   * @param value The value the parameter should be set to
   * @param donotEncode Whether to URLEncode the parameter
   * @param transport The type of thransport to use [http|https]
   *
   * @return Returns the parameter's current value as a string
   */
  public function set_axis_device_parameter($parameter,
                                       $value,
                                       $donotEncode = false,
                                       $transport = 'http') {
    $result = null;
    $url = sprintf("%s://%s/axis-cgi/param.cgi?action=update&%s=%s",
                   $transport,
                   $this->ip,
                   $parameter,
                   ($donotEncode?$value:rawurlencode($value)));

    $result = $this->send_request($url);

    /* Check if the device did not set the parameter */
    if($result != 'OK') {
      throw new Exception(sprintf("Did not receive OK from device (result=\"%s\"", $result), 1);
    }

    return true;
  }

  /**
   * Retrieves a parameter from the device
   *
   * @param parameter The parameter group
   * @param transport The type of thransport to use [http|https]
   *
   * @return Returns the parameter's current value as a string
   */
  public function get_axis_device_parameter($parameter, $transport = 'http') {
    $result = null;
    $url = sprintf("%s://%s/axis-cgi/param.cgi?action=list%s",
                   $transport,
                   $this->ip,
                   ($parameter ? sprintf("&group=%s", $parameter) : ''));

    $result = $this->send_request($url);

    /* Check if an error ocurred */
    if(1 == preg_match('/^# Error:/', $result)) {
      throw new Exception($result, 999);
    }

    /* Return the result excluding the group string */
    return trim(substr($result, strlen($parameter) + 1));
  }
 
 
  /**
   * Enables/disables the SSH server on the device
   *
   * @param enable True to enable or false to disable the SSH server
   *
   * @return Returns true on success and false on failure
   */
  public function set_axis_device_ssh($enable) {
    if($enable) {
      $enable = "yes";
    } else {
      $enable = "no";
    }
    if(!$this->is_axis_device_ssh_capable()) {
      throw new Exception('The device is not SSH capable', 1);
    }
    $this->set_axis_device_parameter('Network.SSH.Enabled', $enable);
    return true;
  }
  
  /**
   * Enables the SSH server on the device
   *
   * @return Returns true on success and false on failure
   */
  public function enable_axis_device_ssh() {
    return $this->set_axis_device_ssh(true);
  }
  
  /**
   * Disables the SSH server on the device
   *
   * @return Returns true on success and false on failure
   */
  public function disable_axis_device_ssh() {
    return $this->set_axis_device_ssh(false);
  }
  
  /**
   * Checks if the device firmware is SSH capable (>=LFP5)
   *
   * @return Returns true on success and false on failure
   */
  public function is_axis_device_ssh_capable() {
    try {
      $parameter = 'Network.SSH.Enabled';
      $this->get_axis_device_parameter($parameter);
    } catch(Exception $e) {
      if(999 != $e->getCode()) {
        throw $e;
      }
      else {
        return false;
      }
    }
    return true;
  }
  
  /**
   * Checks if the device SSH server is enabled
   *
   * @return Returns true on success and false on failure
   */
  public function is_axis_device_ssh_enabled() {
    $parameter = 'Network.SSH.Enabled';
    $result = $this->get_axis_device_parameter($parameter);

    if('no' == $result)
      return false;
    return true;
  }

  public function has_ir() {
    try {
      $this->get_axis_device_parameter('Layout.IRIlluminationEnabled');
    } catch(Exception $e) {
      return false;
    }
    return true;
  }

  public function has_pir() {
    for($i = 0; $i < 4; $i++) {
      try {
        $this->get_axis_device_parameter(sprintf("IOPort.%d.Input.PIR", $i));
        return true;
      } catch(Exception $e) {
        /* Do nothing */
      }
    }
    return false;
  }

  public function has_light_control() {
    try {
      $this->get_axis_device_parameter('Properties.LightControl');
    } catch(Exception $e) {
      return false;
    }
    return true;
  }

  public function has_status_led() {
    try {
      $this->get_axis_device_parameter('StatusLED');
    } catch(Exception $e) {
      return false;
    }
    return true;
  }

  public function has_ptz() {
    try {
      $this->get_axis_device_parameter('PTZ');
    } catch(Exception $e) {
      return false;
    }
    return true;
  }

  public function has_dptz() {
    try {
      $this->get_axis_device_parameter('Properties.PTZ.DigitalPTZ');
    } catch(Exception $e) {
      return false;
    }
    return true;
  }

  public function has_audio() {
    try {
      $this->get_axis_device_parameter('Properties.Audio');
    } catch(Exception $e) {
      return false;
    }
    return true;
  }

  public function has_speaker() {
    try {
      $this->get_axis_device_parameter('AudioSource.A0.OutputGain');
    } catch(Exception $e) {
      return false;
    }
    return true;
  }

  public function has_mic() {
    try {
      $input_type = $this->get_axis_device_parameter('AudioSource.A0.InputType');
      if(!in_array($input_type, array( 'mic', 'internal' ))) {
        return false;
      }
    } catch(Exception $e) {
      return false;
    }
    return true;
  }

  public function has_wlan() {
    try {
      $this->get_axis_device_parameter('Network.Wireless');
    } catch(Exception $e) {
      return false;
    }
    return true;
  }

  public function has_local_storage() {
    try {
      $this->get_axis_device_parameter('Properties.LocalStorage.LocalStorage');
    } catch(Exception $e) {
      return false;
    }
    return true;
  }

  public function has_sd_disk() {
    try {
      $this->get_axis_device_parameter('Properties.LocalStorage.SDCard');
    } catch(Exception $e) {
      return false;
    }
    return true;
  }

}

class SqlConnection {

  /* MySQL host, database and credentials */
  private $host     = 'localhost';
  private $database = 'abused';
  private $username = 'abused';
  private $password = 'abusedpass';
  private $connection = NULL;

  public function __construct($host, $database, $username, $password) {
    $this->host = $host;
    $this->database = $database;
    $this->username = $username;
    $this->password = $password;
    $this->connection = new mysqli($host, $username, $password, $database);
    if($this->connection->connect_errno) {
      throw new Exception(sprintf("Failed to connect to MySQL: %s", $h_sql->connect_error));
    }
  }

  public function __destruct() {
    $this->connection->close();
  }

  public function query($query) {

    $res = $this->connection->query($query);
    if(false === $res) {
      $msg = sprintf("Failed to query MySQL: %s", $h_sql->error);
      $res->free();
      throw new Exception(sprintf("%s", $msg));
    }
    $result = $this->resultset_to_array($res);
    $res->free();
    return $result;
  }

  public function resultset_to_array($resultset) {
    $results = array();
    while($row = $resultset->fetch_assoc()) {
      $results[] = $row;
    }
    return $results;
  }

}

// *********** CapabilityManager TEST **************
//$cm = null;
//$cm = new CapabilityManager('192.168.0.41', 'root', 'pass');
//try {
//  $can_ssh = $cm->is_axis_device_ssh_capable();
//  if($can_ssh) {
//    $is_enabled = $cm->is_axis_device_ssh_enabled();
//    printf("SSH enabled: %s<br />\n", ($is_enabled ? 'yes' : 'no'));
//  }
//  else {
//    printf("The device is not SSH capable<br />\n");
//  }
//  $fw_ver = $cm->get_axis_device_parameter('Properties.Firmware.Version', 'https');
//  printf("Firmware version: %s<br />\n", $fw_ver);
//} catch(Exception $e) {
//  printf("%s", $e->getMessage());
//}
//if($cm->has_ir()) {
//  printf("Has support for IR illumination<br />\n");
//}
//else {
//  printf("Does not have support for IR illumination<br />\n");
//}
//exit(0);
// *******************************

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

/* Connect to MySQL */
$sqconnectionl = new SqlConnection($host, $database, $username, $password);

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
    if(!$abused_result->is_mac_address(id)) {
      $id = str_replace(':', '', $mac);
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
    $model_name = $abused_result->get_custom_field_modelName();
    $friendly_name = $abused_result->get_custom_field_friendlyName();

    /* See if model-firmware combination has
       been probed for capabilities */

  } catch(Exception $e) {
    /* Do noting */
    continue;
  }
  $query = sprintf("call add_or_update_device('%s', '%s', '%s', '%s', '%s', '%s', '%s')",
                   $id,
                   $mac,
                   $ipv4,
                   $ipv6,
                   $model_name,
                   $friendly_name,
                   $model_number);

  $sqlconnection->query($query);
}

