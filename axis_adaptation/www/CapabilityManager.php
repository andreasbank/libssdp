<?php
class CapabilityManager {

  private $ip = '';
  private $id = null;
  private $credentials = null;
  private $timeout = 5;
  private $proxy_address = null;
  private $proxy_port = null;

  public function __construct($ip,
                              $id,
                              array $credentials = array(
                                array(
                                  'username' => 'root',
                                  'password' => 'pass'),
                                array(
                                  'username' => 'camroot',
                                  'password' => 'password')),
                              $proxy_address = null,
                              $proxy_port = null) {
    $this->ip = $ip;
    $this->id = $id;
    if(isset($credentials) && !isset($credentials[0]['username'])) {
      throw new Exception('Wrong array structure for \'credentials\' (should be an array of arrays)');
    }
    $this->credentials = $credentials;
    $this->proxy_address = $proxy_address;
    $this->proxy_port = $proxy_port;
  }

  public function set_credentials(array $credentials) {
    if(!isset($credentials[0]['username'])) {
      throw new Exception('Wrong array structure for \'credentials\' (should be an array of arrays)');
    }
    $this->credentials = $credentials;
  }

  public function set_proxy($proxy_address, $proxy_port) {
    $this->proxy_address = $proxy_address;
    $this->proxy_port = $proxy_port;
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
  public function build_digest_auth_header($headers,
                                           $uri,
                                           $username = null,
                                           $password = null) {
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

    if(empty($username) && isset($this->credentials[0]['username'])) {
      $username = $this->credentials[0]['username'];
      $password = $this->credentials[0]['password'];
    }
    else if(empty($username)) {
      throw new Exception('No credentials given, cannot create digest authorization header', 0);
    }

    /* Extract the WWW-Authenticate header */
    $header = null;
    foreach($headers as $header_) {
      if(strstr($header_, 'WWW-Authenticate: Digest') != false) {
        $header = $header_;
        break;
      }
    }

    if(empty($header)) {
      throw new Exception('Digest authorization has not been requested', 0);
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
      throw new Exception('The server requested a unsupported digest authentication method', 0);
    }

    /* Set our args; RFC2069: qop=auth */
    $auth_args['qop'] = 'auth';
    $nc = 1;
    $cnonce = $this->generate_nonce();

    /* Build the header response field according to
       http://tools.ietf.org/html/rfc2069,
       note that we only implement needed features
       and make alot of assumptions (as using 'realm') */
    $ha1 = md5(sprintf("%s:%s:%s", $username, $auth_args['realm'], $password));
    $ha2 = md5(sprintf("GET:%s", $uri));
    $response = md5(sprintf("%s:%s:%d:%s:%s:%s",
                            $ha1,
                            $auth_args['nonce'],
                            $nc,
                            $cnonce,
                            $auth_args['qop'],
                            $ha2));

    $auth_digest = sprintf("Authorization: Digest username=\"%s\"", $username);
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
  public function build_basic_auth_header($username = null,
                                          $password = null) {

    if(empty($username) && isset($this->credentials[0]['username'])) {
      $username = $this->credentials[0]['username'];
      $password = $this->credentials[0]['password'];
    }
    else if(empty($username)){
      throw new Exception('No credentials given, cannot create basic authorization header', 0);
    }

    $auth_basic = sprintf("Authorization: Basic %s",
                          base64_encode(sprintf("%s:%s",
                                                $username,
                                                $password)));
    return $auth_basic;
  }

  /**
   * Build the request context
   */
  public function build_context($auth_header) {

    $context = array(
                 'http' => array(
                   'timeout' => $this->timeout
                 ));

    if(!empty($auth_header)) {
      $context['http'] = array_merge($context['http'],
                  array('header'  => $auth_header));
    }

    if(!empty($this->proxy_address) && !empty($this->proxy_port)) {
      $context['http'] = array_merge($context['http'],
                                     array('proxy' => sprintf("tcp://%s:%d",
                                                              $this->proxy_address,
                                                              $this->proxy_port)));
      $context['http'] = array_merge($context['http'],
                                     array('request_fulluri' => true));
    }

    $context = stream_context_create($context);

    return $context;
  }

  /**
   * Send a http(s) request
   */
  public function send_request($url,
                               $username = null,
                               $password = null) {
    $result = null;
    $credentials = null;

    /* Extract transport method [http|https] */
    $transport = strstr($url, '://', true);
    if($transport != 'http' && $transport != 'https') {
      throw new Exception('Wrong transport method, only HTTP or HTTPS is allowed');
    }

    if(!empty($username) && !empty($password)) {
      $credentials = array(
        array(
          'username' => $username,
          'password' => $password
        )
      );
    }
    else if(!empty($this->credentials)) {
      $credentials = $this->credentials;
    }
    else {
      $credentials = array(null);
    }

    $i = 0;
    foreach($credentials as $single_credentials) {

      try {

        $i++;
        if(isset($single_credentials['username'])) {
          $context = $this->build_context(
            $this->build_basic_auth_header($single_credentials['username'],
                                           $single_credentials['password']));
        }
        else {
          $context = $this->build_context();
        }

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
        // TODO: also check if string 'digest' is
        //       present in any of the headers in the if-statement
        if($result === false &&
           isset($http_response_header) &&
           strpos($http_response_header[0], '401') &&
           isset($single_credentials['username'])) {

          /* Retry using digest authentication */
          $context = $this->build_context(
            $this->build_digest_auth_header($http_response_header,
                                            $url,
                                            $single_credentials['username'],
                                            $single_credentials['password']));
          $result = @file_get_contents($url, false, $context);
        }

        if($result === false) {
          if(isset($http_response_header) &&
             strpos($http_response_header[0], '401')) {
            if(isset($single_credentials['username'])) {
              throw new Exception('Authorization required', 401);
            }
            else {
              throw new Exception('Wrong credentials', 401);
            }
          }
          throw new Exception(sprintf("Connection failed %s(%s, proxy: %s)",
                                      (isset($http_response_header[0]) ?
                                         sprintf("[HTTP error code: %s] ", $http_response_header[0]) : ''),
                                      $url,
                                      (empty($this->proxy_address) ? 'none' :
                                         sprintf("tcp://%s:%s", $this->proxy_address, $this->proxy_port))),
                              998);
        }
        break;
      }
      catch(Exception $e) {
        if(($e->getCode() == 401 && false != strpos($e->getMessage(), 'Authorization required')) ||
           count($credentials) == $i) {
          throw $e;
        }
        /* Else continue */
      }
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

  public function enable_avhs($restart = false) {
    if($restart) {
      $this->set_axis_device_parameter('RemoteService.Enabled', 'no');
    }
    $this->set_axis_device_parameter('RemoteService.Enabled', 'yes');
  }


  public function has_been_dispatched() {
  
    /* Fetch the remote service parameter from the device */
    $remote_service = $this->get_remote_service();

    if(!(false === strpos($remote_service, 'dispatch'))) {
      return false;
    }
    return true;
  }

  public function dispatch_device(array $portal_ips,
                                  $portal_admin_username,
                                  $portal_admin_password) {


    $this->enable_avhs();
    $is_dispatched = $this->has_been_dispatched();
    $on_dispatcher = '';
    $oak = $this->get_oak($on_dispatcher);
    $on_dispatcher = (empty($on_dispatcher) ? false : true);

    if(!$is_dispatched) {

      /* Wait 20 seconds for the device to
         connect to the dispatcher */
      for($i = 0; $i < 20; $i++) {
        if($on_dispatcher) {
          break;
        }
        sleep(1);
      }

      if(!$on_dispatcher) {
        throw new Exception('Device did not connect to dispatcher in the allowed time interval (20 seconds)', 0);
      }

      /* Add the device to the portal */
      $this->add_device_to_portal($portal_ips[0],
                                  $portal_admin_username,
                                  $portal_admin_password,
                                  $oak);
    }
    else {
      try {
        $portal_ips = implode(',', $portal_ips);
        $this->set_axis_device_parameter('RemoteService.ServerList', $portal_ips);
      }
      catch(Exception $e) {
        $this->set_axis_device_parameter('RemoteService.RemoteServerURL', $portal_ips);
      }
      $this->enable_avhs(true);
    }

    // TODO: make it check if device is on portal?
  }

  public function add_device_to_portal($portal_ip,
                                       $portal_admin_username,
                                       $portal_admin_password,
                                       $oak) {

    $this->send_request(sprintf("http://%s/admin/device?a=add&u=%s&p=%s&deviceid=%s&method=oak&key=%s",
                                $portal_ip,
                                $portal_admin_username,
                                $portal_admin_password,
                                $this->id,
                                $oak));
  }

  public function factory_default() {

    $result = $this->send_request(sprintf("http://%s/axis-cgi/admin/hardfactorydefault.cgi", $this->ip));

  }

  /**
   *  Enables telnet on the device.
   */
  public function enable_telnet() {
    $ip = null;

    $ftpObject = new AxisFtp($this->ip, $this->username, $password);
    $path = "/tmp/";

    if(!$this->isDeviceSshCapable()) {
      $ftpObject->modify_device_files(sprintf("%sinittab", $path), "/etc/inittab", "/#tnet/", "tnet");
    }
    $ftpObject->modify_device_files(sprintf("%sftpd", $path), "/etc/init.d/ftpd", "/_OPTIONS/", "_OPTIONS\n\t\t/usr/sbin/telnetd");

    $param = 'Network.FTP.Enabled='; // yes|no

    $this->set_axis_device_parameter($param, 'no');
    $this->set_axis_device_parameter($param, 'yes');
  }

  public function get_remote_service() {
    try {
      $server_list = $this->get_axis_device_parameter('RemoteService.ServerList');
    }
    catch(Exception $e) {
      $server_list = $this->get_axis_device_parameter('RemoteService.RemoteServerURL');
    }
    return $server_list;
  }

  public function get_oak(&$server,
                          $dispatcher_ip = 'dispatchse1.avhs.axis.com',
                          $dispatcher_username = 'qa',
                          $dispatcher_password = '4Mpq%Y>y=_kTY)$(R}h9',
                          $transport = 'https') {
    $result = null;

    $url = sprintf("%s://%s/dispatcher/admin/qa.php?cmd=info&cameraid=%s",
                   $transport,
                   $dispatcher_ip,
                   $this->id);

    $result = $this->send_request($url,
                                  $dispatcher_username,
                                  $dispatcher_password);

    $server = substr($result, 7, strpos($result, 'key=') - 8);

    /* Return the result */
    $result = substr($result, strpos($result, 'key=') + 4 - strlen($result));
    return trim($result);
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
