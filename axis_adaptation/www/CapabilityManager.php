<?php
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

  public function set_credentials($username, $password) {
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

  public function parse_axis_device_xml($url, $parameter, $transport = 'http') {
    $result = null;
    $url = sprintf("%s://%s/%s",
                   $transport,
                   $this->ip,
                   $url);

    $result = $this->send_request($url);

    /* Check if an error ocurred */
    if(1 == preg_match('/^# Error:/', $result)) {
      throw new Exception($result, 999);
    }

    $pattern = sprintf("/%s=\"([A-Za-z]+)\"/", $parameter);

    $parsed_success = preg_match($pattern,
                                 $result,
                                 $parsed_matches);

    if(!$parsed_success || count($parsed_matches) < 1) {
      return 'unknown';
    }

    /* Return the result */
    return $parsed_matches[1];
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

  public function get_firmware_version() {
    $fw_version = $this->get_axis_device_parameter('Properties.Firmware.Version');
    return $fw_version;
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
        if('PIR sensor' == $this->get_axis_device_parameter(sprintf("IOPort.I%d.Input.Name", $i))) {
          return true;
        }
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

  public function has_ptz_driver_installed() {
    try {
      if(1 == explode('=', $this->get_axis_device_parameter('PTZ.CamPorts'))[1]) {
        return true;
      }
    } catch(Exception $e) {
      /* Do nothing */
    }
    return false;
  }

  public function has_ptz() {
    try {
      if(('true' == $this->get_axis_device_parameter('PTZ.ImageSource.I0.PTZEnabled')) &&
         !$this->has_dptz() &&
         $this->has_ptz_driver_installed()) {
        return true;
      }
    } catch(Exception $e) {
      /* Do nothing */
    }
    return false;
  }

  public function has_dptz() {
    try {
      if('yes' == $this->get_axis_device_parameter('Properties.PTZ.DigitalPTZ')) {
        return true;
      }
    } catch(Exception $e) {
      /* Do nothing */
    }
    return false;
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

  public function get_sd_disk_status() {
    try {
      $status = $this->parse_axis_device_xml('axis-cgi/disks/list.cgi?diskid=SD_DISK&type=xml', 'status');
    } catch(Exception $e) {
      $status = 'unknown';
    }
    return $status;
  }

}
