<?php
require_once('Controller.php');

function parse_http_args() {

  $args = array();

  $args['action'] = null;
  if(isset($_POST['action']) && !empty($_POST['action'])) {
    $args['action'] = $_POST['action'];
  }
  else if(isset($_GET['action']) && !empty($_GET['action'])) {
    $args['action'] = $_GET['action'];
  }
  else {
    throw new Exception('No action specified', 0);
  }

  $args['capability'] = null;
  if(isset($_POST['capability']) && !empty($_POST['capability'])) {
    $args['capability'] = $_POST['capability'];
  }
  else if(isset($_GET['capability']) && !empty($_GET['capability'])) {
    $args['capability'] = $_GET['capability'];
  }

  $args['model_name'] = null;
  if(isset($_POST['model_name']) && !empty($_POST['model_name'])) {
    $args['model_name'] = $_POST['model_name'];
  }
  else if(isset($_GET['model_name']) && !empty($_GET['model_name'])) {
    $args['model_name'] = $_GET['model_name'];
  }

  $args['firmware_version'] = null;
  if(isset($_POST['firmware_version']) && !empty($_POST['firmware_version'])) {
    $args['firmware_version'] = $_POST['firmware_version'];
  }
  else if(isset($_GET['firmware_version']) && !empty($_GET['firmware_version'])) {
    $args['firmware_version'] = $_GET['firmware_version'];
  }

  $args['capability_state'] = null;
  if(isset($_POST['capability_state']) && !empty($_POST['capability_state'])) {
    $args['capability_state'] = $_POST['capability_state'];
  }
  else if(isset($_GET['capability_state']) && !empty($_GET['capability_state'])) {
    $args['capability_state'] = $_GET['capability_state'];
  }

  $args['locked_by'] = null;
  if(isset($_POST['locked_by']) && !empty($_POST['locked_by'])) {
    $args['locked_by'] = $_POST['locked_by'];
  }
  else if(isset($_GET['locked_by']) && !empty($_GET['locked_by'])) {
    $args['locked_by'] = $_GET['locked_by'];
  }

  $args['device_ids'] = null;
  if(isset($_POST['device_ids']) && !empty($_POST['device_ids'])) {
    $args['device_ids'] = $_POST['device_ids'];
  }
  else if(isset($_GET['device_ids']) && !empty($_GET['device_ids'])) {
    $args['device_ids'] = $_GET['device_ids'];
  }

  $args['device_id'] = null;
  if(isset($_POST['device_id']) && !empty($_POST['device_id'])) {
    $args['device_id'] = $_POST['device_id'];
  }
  else if(isset($_GET['device_id']) && !empty($_GET['device_id'])) {
    $args['device_id'] = $_GET['device_id'];
  }

  /* 16 minutes */
  $args['age'] = 16;
  if(isset($_POST['age']) && !empty($_POST['age'])) {
    $args['age'] = $_POST['age'];
  }
  else if(isset($_GET['age']) && !empty($_GET['age'])) {
    $args['age'] = $_GET['age'];
  }

  $args['ip'] = null;
  if(isset($_POST['ip']) && !empty($_POST['ip'])) {
    $args['ip'] = $_POST['ip'];
  }
  else if(isset($_GET['ip']) && !empty($_GET['ip'])) {
    $args['ip'] = $_GET['ip'];
  }

  $args['sort_by'] = null;
  if(isset($_POST['sort_by']) && !empty($_POST['sort_by'])) {
    $args['sort_by'] = $_POST['sort_by'];
  }
  else if(isset($_GET['sort_by']) && !empty($_GET['sort_by'])) {
    $args['sort_by'] = $_GET['sort_by'];
  }

  $args['portal_ips'] = null;
  if(isset($_POST['portal_ips']) && !empty($_POST['portal_ips'])) {
    $args['portal_ips'] = $_POST['portal_ips'];
  }
  else if(isset($_GET['portal_ips']) && !empty($_GET['portal_ips'])) {
    $args['portal_ips'] = $_GET['portal_ips'];
  }

  $args['user'] = null;
  if(isset($_POST['user']) && !empty($_POST['user'])) {
    $args['user'] = $_POST['user'];
  }
  else if(isset($_GET['user']) && !empty($_GET['user'])) {
    $args['user'] = $_GET['user'];
  }

  $args['url'] = null;
  if(isset($_POST['url']) && !empty($_POST['url'])) {
    $args['url'] = urldecode($_POST['url']);
  }
  else if(isset($_GET['url']) && !empty($_GET['url'])) {
    $args['url'] = urldecode($_GET['url']);
  }

  return $args;
}

try {

  $controller = new Controller();

  $args = parse_http_args();

  switch($args['action']) {
  
  /* List devices */
  case 'list_devices':
  
    $results = $controller->list_devices($args['capability'],
                                         $args['model_name'],
                                         $args['firmware_version'],
                                         $args['capability_state'],
                                         $args['age'],
                                         $args['locked_by'],
                                         $args['device_id'],
                                         $args['ip'],
                                         $args['sort_by']);

    if(is_array($results) && count($results) > 0) {
      $json_results = json_encode($results[0]);
  
      printf("%s", $json_results);
    }
    else {
      printf("[]");
    }
    break;
  
  /* Restart AVHS */
  case 'restart_avhs':
  
    if(!isset($args['device_ids']) || empty($args['device_ids'])) {
      throw new Exception('Missing arguments \'device_ids[]\'', 0);
    }
  
    $controller->restart_avhs($args['device_ids'],
                        $args['age']);
  
    if(empty($args['url'])) {
      header('Content-type: application/json; charset=utf-8');
      printf('{\'result\':\'OK\'}');
    }
    else {
      header(sprintf("Location: %s", $args['url']));
    }
  
    break;
  
  /* Enable AVHS */
  case 'enable_avhs':
  
    if(!isset($args['device_ids']) || empty($args['device_ids'])) {
      throw new Exception('Missing arguments \'device_ids[]\'', 0);
    }
  
    $controller->enable_avhs($args['device_ids'],
                             $args['age']);
  
    if(empty($args['url'])) {
      header('Content-type: application/json; charset=utf-8');
      printf('{\'result\':\'OK\'}');
    }
    else {
      header(sprintf("Location: %s", $args['url']));
    }
    break;
  
  /* Factory-default a list of devices */
  case 'factory_default':
  
    if(!isset($args['device_ids']) || empty($args['device_ids'])) {
      throw new Exception('Missing arguments \'device_ids[]\'', 0);
    }
 
    $controller->factory_default($args['device_ids'],
                                 $args['age']);
  
    if(empty($args['url'])) {
      header('Content-type: application/json; charset=utf-8');
      printf('{\'result\':\'OK\'}');
    }
    else {
      header(sprintf("Location: %s", $args['url']));
    }
  
    break;
  
  /* Move device to portal */
  case 'dispatch_devices':
  
    if(!isset($args['device_ids']) || empty($args['device_ids'])) {
      throw new Exception('Missing arguments \'device_ids[]\'', 0);
    }
  
    if(!isset($args['portal_ips']) || empty($args['portal_ips'])) {
      throw new Exception('Missing argument \'portal_ips\'', 0);
    }
  
    $controller->dispatch_devices($args['device_ids'],
                                  $args['age'],
                                  explode(',', $args['portal_ips']));
  
    if(empty($args['url'])) {
      header('Content-type: application/json; charset=utf-8');
      printf('{\'result\':\'OK\'}');
    }
    else {
      header(sprintf("Location: %s", $args['url']));
    }
  
    break;
  
  /* Fetch device info */
  case 'device_info':
  
    if(!isset($args['device_ids']) || empty($args['device_ids'])) {
      throw new Exception('Missing arguments \'device_ids[]\'', 0);
    }
  
    $result = $controller->device_info($args['device_id'],
                                 $args['age']);
  
    printf("%s", json_encode($result));
  
    break;
  
  /* Fetch device info */
  case 'gui_device_info':
  
    if(!isset($args['device_id']) || empty($args['device_id'])) {
      throw new Exception('Missing arguments \'device_ids[]\'', 0);
    }
  
    $result = $controller->device_info($args['device_id'],
                                       $args['age']);
  
    printf("<table border=\"1\"><tr><td style=\"font-weight: bold;\">ID</td><td style=\"font-weight: bold;\">Remote service</td><td style=\"font-weight: bold;\">OAK</td><td style=\"font-weight: bold;\">Is on dispatcher</td></tr>");
    printf("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr></table>\n",
           $result['id'],
           str_replace(",", "<br />\n", $result['remote_service']),
           $result['oak'],
           $result['on_dispatcher']);
    break;
  
  /* Lock the device if possible */
  case 'lock_device':
  
    if(!isset($args['user']) || empty($args['user'])) {
      throw new Exception('No user specified', 0);
    }
  
    $result = $controller->lock_device($args['user'],
                                       $args['capability'],
                                       $args['model_name'],
                                       $args['firmware_version'],
                                       $args['capability_state'],
                                       $args['device_id'],
                                       $args['age']);
  
    if(empty($args['url'])) {
      header('Content-type: application/json; charset=utf-8');
      if(is_array($result) && count($result) > 0) {
        printf("%s", json_encode($result));
      }
      else {
        printf("[]");
      }
    }
    else {
      header(sprintf("Location: %s", $args['url']));
    }
    break;
  
  /* Unlock a device by its ID */
  case 'unlock_device':
  
    if(!isset($args['device_id']) || empty($args['device_id'])) {
      throw new Exception('Missing arguments \'device_ids[]\'', 0);
    }
  
    $result = $controller->unlock_device($args['device_id']);
  
    if(empty($args['url'])) {
      printf("%s", $result);
    }
    else {
      header(sprintf("Location: %s", $args['url']));
    }
  
    break;
  
  /* Inform that the action is invalid */
  default:
    printf('Invalid action');
    exit(0);
  }

}
catch(Exception $e) {
  header('HTTP/1.0 500');
  printf("Error [%d]: %s", $e->getCode(), $e->getMessage());
  exit(1);
}
