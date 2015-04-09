<?php
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *	Copyright(C) 2013 by Andreas Bank, andreas.mikael.bank@gmail.com
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class AxisSyslog {

	public $controller = null;
	public $ip = "";
	public $username = "";
	public $password = "";
	public $syslog_ip = "";
	private $telnet = 0;
	private $use_ssh = null;
	private $ssh = null;

	public function __construct($controller, $ip, $username, $password, $syslog_ip, $use_ssh = false, $use_rsyslog = false) {
		$this->controller = $controller;
		$this->ip = $ip;
		$this->username = $username;
		$this->password = $password;
		$this->syslog_ip = $syslog_ip;
		$this->use_ssh = $use_ssh;
		$this->use_rsyslog = $use_rsyslog;
		if($use_ssh)
			$this->ssh = new SshClient($ip, $username, $password);
		else
			$this->telnet = new AxisTelnet();
	}

	/***
	 * Connects (opens) the SSH session
	 */
	public function connect() {
		if(!$this->use_ssh) {
			if(!$this->telnet->is_connected())
				$this->telnet->connect($this->ip, $this->username, $this->password);
			sleep(2);
		} else {
			$this->ssh->begin_execution();
		}
	}

	/***
	 * Disconnects (closes) the SSH session
	 */
	public function disconnect() {
		if(!$this->use_ssh) {
			if($this->telnet->is_connected())
				$this->telnet->disconnect();
		} else {
			$this->ssh->end_execution();
		}
	}

	/***
	 * Restarts the syslog daemon
	 */
	public function restart() {
		if($this->use_ssh) {
			if($this->use_rsyslog) {
				$this->ssh->execute("killall rsyslogd;");
				$this->ssh->execute("rsyslogd;");
			} else {
				$this->ssh->execute("/etc/init.d/sysklogd restart;");
			}
		} else {
			if($this->telnet->is_connected())
				$this->telnet->write("/etc/init.d/sysklogd restart", $dump);
		}
	}

	/***
	 * Enables the syslog daemon to send to the specified IP address
	 *
	 * @throws 101: already configured
	 */
	public function enable() {
		try {
			$this->connect();
		} catch(Exception $e) {
			if($e->getCode() != 1)
				throw $e;
		}
		if(!$this->use_ssh) {
			if(!$this->telnet->is_connected()) {
				// error, most probably disabled telnet
				flush();
				$this->controller->enableTelnet($this->ip, $this->username, $this->password);
				sleep(5);
				try {
					$this->connect();
				} catch(Exception $e) {
					throw $e;
				}
			}
		} else {
			if(!$this->controller->is_device_ssh_enabled($this->ip, $this->username, $this->password)) {
				$this->controller->enable_device_ssh($this->ip, $this->username, $this->password);
				sleep(5);
			}
		}
		$return = "";
		if($this->use_ssh) {
			if($this->use_rsyslog) {
				$return = $this->ssh->execute(sprintf("grep %s /etc/rsyslog.d/40-remote_log.conf", $this->syslog_ip));
			}
			else {
				$return = $this->ssh->execute(sprintf("grep %s /etc/syslog.conf", $this->syslog_ip));
			}
		} else {
			$this->telnet->write(sprintf("grep %s /etc/syslog.conf", $this->syslog_ip), $return);
		}
		if(strlen($return) > 0 && strpos($return, "[root@") === false) {
			$this->disconnect();
			throw new Exception(sprintf("The syslog is already configured to send to the specified IP (%s)", substr($return, 0, -1)), 0);
		}
		else {
			$cmd = "echo \"*.* @".$this->syslog_ip."\" >> ";
			if($this->use_ssh) {
				if($this->use_rsyslog) {
					// TODO: check for error (eg. file does not exist, etc etc)
					$result = $this->ssh->execute(sprintf("%s/etc/rsyslog.d/40-remote_log.conf", $cmd));
				}
				else {
					$this->ssh->execute(sprintf("%s/etc/syslog.conf", $cmd));
				}
			} else {
				$this->telnet->write(sprintf("%s/etc/syslog.conf", $cmd), $dump);
			}
			$this->restart();
		}
		$this->disconnect();
	}
}
?>
