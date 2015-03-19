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

  GRANT SELECT, INSERT, UPDATE ON `abused`.`capabilities` TO 'abused'@'%';

  GRANT SELECT, INSERT, UPDATE ON `abused`.`model_firmware` TO 'abused'@'%';

  GRANT SELECT, INSERT, UPDATE ON `abused`.`model_firmware_capability` TO 'abused'@'%';

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
      INSERT INTO `model_firmware` (`model_name`, `firmware_version`, `last_updated`)
      VALUES(v_model_name, v_firmware_version), NOW();
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
    FROM `model_firmware_capability`
    WHERE `model_firmware_id`=v_model_firmware_id;
    IF v_count > 0 THEN
      SELECT 'yes' AS `probed`;
    ELSE
      SELECT 'no' AS `probed`;
    END IF;
  END//
  DELIMITER ;

  DELIMITER //
  CREATE PROCEDURE `add_capability_to_model_firmware` (IN `v_capability_id` INT,
                                                       IN `v_model_firmware_id` INT)
    SQL SECURITY INVOKER
  BEGIN
    INSERT INTO `model_firmware_capability`
    VALUES (
      v_capability_id, v_model_firmware_id
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

  DELIMITER //
  CREATE PROCEDURE `is_device_locked` (IN `v_device_id` INT)
    SQL SECURITY DEFINER
  BEGIN
    DECLARE is_locked TINYINT DEFAULT 0;
  
    SELECT `locked`
    INTO is_locked
    FROM `locked_devices`
    WHERE `device_id` = v_device_id
    AND `locked` =1;
    IF is_locked =1 THEN
      SELECT 'yes' AS `locked` ;
    ELSE
      SELECT 'no' AS `locked` ;
    END IF ;
  END
  DELIMITER ;

  GRANT EXECUTE ON PROCEDURE abused.add_capability_if_not_exist TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.add_model_firmware_if_not_exist TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.is_model_firmware_probed TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.add_capability_to_model_firmware TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.add_or_update_device TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.is_device_locked TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.delete_inactive_devices TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.is_device_locked_internal TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.lock_device TO 'abused'@'%';

  GRANT EXECUTE ON PROCEDURE abused.unlock_device TO 'abused'@'%';
 */

require_once('CapabilityManager.php');
require_once('AbusedResult.php');
require_once('SqlConnection.php');

$test_xml = <<<EOT
<?xml version="1.0" encoding="utf-8"?>
<root>
  <message length="458">
    <mac>
      0:40:8c:d5:a6:87
    </mac>
    <ip>
      192.168.0.41
    </ip>
    <request protocol="HTTP/1.1">
      NOTIFY *
    </request>
    <datetime>
      2015-03-19 12:24:26
    </datetime>
    <custom_fields count="7">
      <custom_field name="serialNumber">
        00408CD5A687
      </custom_field>
      <custom_field name="friendlyName">
        AXIS P3367 - 00408CD5A687
      </custom_field>
      <custom_field name="manufacturer">
        AXIS
      </custom_field>
      <custom_field name="manufacturerURL">
        http://www.axis.com/
      </custom_field>
      <custom_field name="modelName">
        AXIS P3367
      </custom_field>
      <custom_field name="modelNumber">
        P3367
      </custom_field>
      <custom_field name="modelURL">
        http://www.axis.com/
      </custom_field>
    </custom_fields>
    <headers count="10">
      <header typeInt="1" typeStr="host">
         239.255.255.250:1900
      </header>
      <header typeInt="0" typeStr="CACHE-CONTROL">
         max-age=1800
      </header>
      <header typeInt="6" typeStr="location">
         http://192.168.0.41:51040/rootdesc1.xml
      </header>
      <header typeInt="8" typeStr="opt">
         "http://schemas.upnp.org/upnp/1/0/"; ns=01
      </header>
      <header typeInt="9" typeStr="01-nls">
         0fb26636-1dd2-11b2-a53c-f62ae0ef4d8c
      </header>
      <header typeInt="10" typeStr="nt">
         urn:axis-com:service:BasicService:1
      </header>
      <header typeInt="11" typeStr="nts">
         ssdp:alive
      </header>
      <header typeInt="12" typeStr="server">
         Linux/2.6.35, UPnP/1.0, Portable SDK for UPnP devices/1.6.18
      </header>
      <header typeInt="13" typeStr="x-user-agent">
         redsonic
      </header>
      <header typeInt="14" typeStr="usn">
         uuid:Upnp-BasicDevice-1_0-00408CD5A687::urn:axis-com:service:BasicService:1
      </header>
    </headers>
  </message>
</root>
EOT;

/* Get the XML from the POST data */
//$raw_xml = file_get_contents('php://input');
// TESTING WITH DUMMY XML DATA
$raw_xml = $test_xml;

/* MySQL credentials */
$host     = 'localhost';
$database = 'abused';
$username = 'abused';
$password = 'abusedpass';

/* Parse the XML into an array of AbusedResult objects */
$abused_results = AbusedResult::parse_xml($raw_xml);

/* Info for extracting specific information of
   the device in case it is an AXIS device */
$axis_device_default_username = 'root';
$axis_device_default_password = 'pass';
$axis_device_tfw_username     = 'tfwroot';
$axis_device_tfw_password     = 'tfwpass';

/* Connect to MySQL */
$sqlconnection = new SqlConnection($host, $database, $username, $password);

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
    if(!$abused_result->is_mac_address($id)) {
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
      continue;
    }

    $model_name = $abused_result->get_custom_field_modelName();
    $cm = new CapabilityManager(($ipv4 ? $ipv4 : $ipv6),
                                $axis_device_default_username,
                                $axis_device_default_password);

    /* Fetch the firmware version from the device */
    $firmware_version = $cm->get_firmware_version();

    /* Create or retrieve existing ID for the model-firmware combination */
    $model_firmware_id = $sqlconnection->call(sprintf("call add_model_firmware_if_not_exist('%s', '%s')",
                                 $model_name,
                                 $firmware_version))[0][0]['id'];

    /* See if model-firmware combination has
       been probed for capabilities */
    $is_probed = $sqlconnection->call(sprintf("call is_model_firmware_probed('%s')", 
                                           $model_firmware_id))[0][0]['probed'];

    echo "Model-firmware id: ".$model_firmware_id."<br />\n";
    echo "Is probed: ".$is_probed."<br />\n";

  } catch(Exception $e) {
    /* Do noting */
    echo $e->getMessage();
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
}

