<?php
require_once('SqlConnection.php');
require_once('CapabilityManager.php');

class Controller {

  /* MySQL credentials */
  private $host     = 'localhost';
  private $database = 'abused';
  private $username = 'abused';
  private $password = 'abusedpass';

  /* Info for extracting specific information of
     the device in the case where it is an AXIS device */
  private $device_credentials = array(
    array(
      'username' => 'root',
      'password' => 'pass'),
    array(
      'username' => 'camroot',
      'password' => 'password'));
  private $proxy_address = 'wwwproxy.se.axis.com';
  private $proxy_port = 3128;
  private $avhs_admin_username = 'masteradmin';
  private $avhs_admin_password = 'p';

  /* MySQL handle */
  private $sql = null;

  // TODO: take inputs before we try to connect
  public function Controller() {
    try {
      $this->sql = new SqlConnection($this->host, $this->database, $this->username, $this->password);
    }
    catch(Exception $e) {
      printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
      exit(1);
    }
  }

  public function call($query) {
    return $this->sql->call($query);
  }

  public function query($query) {
    return $this->sql->query($query);
  }

  /* Sort the results */
  public function sort_results(&$arr, $sort_by) {
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

  public function list_devices($capability,
                               $model_name,
                               $firmware_version,
                               $capability_state,
                               $age,
                               $locked_by,
                               $device_id,
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

    $results = $this->sql->call($query);

    /* Sort the results if $sort_by is set */
    if(!empty($sort_by)) {
      $this->sort_results($results, $sort_by);
    }

    return $results;

  }

  public function restart_avhs(array $device_ids,
                               $age) {
    $this->enable_avhs($device_ids,
                       $age,
                       true);

  }

  public function enable_avhs(array $device_ids,
                              $age,
                              $restart = false) {

    foreach($device_ids as $device_id) {
      $devices = $this->list_devices(null, // capability
                                     null, // model_name
                                     null, // firmware_name
                                     null, // capability_state
                                     $age, // age
                                     null, // locked_by
                                     $device_id, // device_id
                                     null, // ip
                                     null); // sort_by

      if(count($devices) < 1 || count($devices[0]) < 1) {
        throw new Exception(sprintf("No device with the ID '%s' present (NOTE: age is set to %d minutes)",
                                    $device_id,
                                    $age));
      }

      $cm = new CapabilityManager($devices[0][0]['ipv4'],
                                  $devices[0][0]['id'],
                                  $this->device_credentials);

      /* Factory-default the device */
      $cm->enable_avhs($restart);

    }
  }

  public function factory_default(array $device_ids,
                                  $age) {

    foreach($device_ids as $device_id) {
      $devices = $this->list_devices(null, // capability
                                     null, // model_name
                                     null, // firmware_name
                                     null, // capability_state
                                     $age, // age
                                     null, // locked_by
                                     $device_id, // device_id
                                     null, // ip
                                     null); // sort_by

      if(count($devices) < 1 || count($devices[0]) < 1) {
        throw new Exception(sprintf("No device with the ID '%s' present (NOTE: age is set to %d minutes)",
                                    $device_id,
                                    $age));
      }

      $cm = new CapabilityManager($devices[0][0]['ipv4'],
                                  $devices[0][0]['id'],
                                  $this->device_credentials);

      /* Factory-default the device */
      $cm->factory_default();

    }
  }

  public function dispatch_devices(array $device_ids,
                                   $age,
                                   array $portal_ips) {

    foreach($device_ids as $device_id) {
      $devices = $this->list_devices(null, // capability
                                     null, // model_name
                                     null, // firmware_name
                                     null, // capability_state
                                     $age, // age
                                     null, // locked_by
                                     $device_id, // device_id
                                     null, // ip
                                     null); // sort_by

      if(count($devices) < 1 || count($devices[0]) < 1) {
        throw new Exception(sprintf("No device with the ID '%s' present (NOTE: age is set to %d minutes)",
                                    $device_id,
                                    $age));
      }

        $cm = new CapabilityManager($devices[0][0]['ipv4'],
                                    $devices[0][0]['id'],
                                    $this->device_credentials);

        /* Set the new remote service in the device */
        $cm->dispatch_device($portal_ips,
                             $this->avhs_admin_username,
                             $this->avhs_admin_password);

      }
    }

  public function device_info($device_id,
                              $age) {

    $devices = $this->list_devices(null, // capability
                                   null, // model_name
                                   null, // firmware_name
                                   null, // capability_state
                                   $age, // age
                                   null, // locked_by
                                   $device_id, // device_id
                                   null, // ip
                                   null); // sort_by

    if(count($devices) < 1 || count($devices[0]) < 1) {
      throw new Exception(sprintf("No device with the ID '%s' present (NOTE: age is set to %d minutes)",
                                  $device_id,
                                  $age));
    }

    $cm = new CapabilityManager($devices[0][0]['ipv4'],
                                $devices[0][0]['id'],
                                $this->device_credentials);

    $remote_service = $cm->get_remote_service();

    $cm->set_proxy($this->proxy_address, $this->proxy_port);
    $server = '';
    $oak = $cm->get_oak($server);
    $cm->set_proxy(null, null);

    return array('id' => $device_id,
                 'remote_service' => $remote_service,
                 'oak' => $oak,
                 'on_dispatcher' => (!(false === strpos($server, 'dispatcher')) ? 'yes' : 'no'));

  }

  public function has_device_been_dispatched($device_id,
                                             $age) {

    $remote_service = $this->device_info($device_id,
                                         $age);

    $remote_service = $remote_service['remote_service'];

    if(!(false === strpos($remote_service, 'dispatch'))) {
      return false;
    }
    return true;
  }

  public function lock_device($user,
                              $capability,
                              $model_name,
                              $firmware_version,
                              $capability_state,
                              $device_id,
                              $age) {
    
    $results = $this->sql->call(sprintf("call lock_device(%s, %s, %s, %s, '%s', %d, %s)",
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

  public function unlock_device($device_id) {
    $this->sql->call(sprintf("call unlock_device('%s')", $device_id));
  }
}

