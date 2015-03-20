<?php
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

  public function __construct($raw_xml,
                              $serial_number,
                              $mac,
                              $ip,
                              $request_protocol,
                              $request,
                              $datetime,
                              $custom_fields_size,
                              $custom_fields,
                              $upnp_headers_size,
                              $upnp_headers) {
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

    /* Parse out serialNumber */
    $serial_number = '';
    if(array_key_exists('serialNumber', $custom_fields)) {
      $serial_number = trim((string) $custom_fields['serialNumber']);
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
      $serial_number,
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
      /* If it is a request for a header */
      elseif(0 === strpos($matches[1], 'header_')) {

        /* Try to locate the header and return it's value */
        foreach($this->upnp_headers as $h_name => $h_value) {
          if($h_name == substr($matches[1], 7)) {
            return $h_value;
          }
        }
        throw new Exception(sprintf("Header '%s' does not exist", $matches[1]));
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
