<?
/**
 * @file
 * @author  Andreas Bank <andreas.mikael.bank@gmail.com>
 * @version 1.0
 *
 * @section COPYRIGHT
 *
 * Copyright (C) 2013 by Andreas Bank, andreas.mikael.bank@gmail.com (andreas.bank@axis.com)
 *
 * @section DESCRIPTION
 *
 * SshClient.php
 * A SSH client class.
 *
 * @section LAST EDITED
 *
 * 2013-08-28
 */

class SshClient {
	public $ip = null;
	public $username = null;
	public $password = null;
	public $port = null;
	public $connection = null;
	private $executionInProgress = false;

	public function __construct($ip, $username, $password, $port = 22) {
		$this->ip = $ip;
		$this->username = $username;
		$this->password = $password;
		$this->port = $port;
	}

	/**
	 * Connects and authenticates to a SSH server
	 */
	public function connect() {
		try {
			if($this->connection != null)
				throw new Exception("Cannot connect, a connection already exists", 1);
			$this->connection = ssh2_connect($this->ip, $this->port);
			if(!$this->connection) {
				throw new Exception("Connection failed", 1);
			}
			$auth = @ssh2_auth_password($this->connection, $this->username, $this->password);
			if(!$auth) {
				throw new Exception("Wrong credentials", 1);
			}
		} catch(Exception $e) {
			$this->executionInProgress = false;
			$this->connection = null;
			throw new Exception("SshClient::connect(): ".$e->getMessage(), $e->getCode());
		}
	}

	/**
	 * Disconnects from the connected SSH server
	 */
	 public function disconnect() {
	 	// not really anything to disconnect :S
		$this->connection = null;
	}

	/**
	 * Sends a command to be executed to the SSH server.
	 * Can be used with beginExecution()/endExecution()
	 * to send multiple commands in a single session
	 *
	 * @param cmd The command to be executed
	 *
	 * @return The output of the command
	 */
	public function execute($cmd) {
		try {
			if(!$this->executionInProgress) {
				$this->connect();
			}
			$stream = ssh2_exec($this->connection, $cmd);
			$stream_out = ssh2_fetch_stream($stream, SSH2_STREAM_STDIO);
			$stream_err = ssh2_fetch_stream($stream, SSH2_STREAM_STDERR);
			stream_set_blocking($stream_out, true);
			stream_set_blocking($stream_err, true);
			$output = stream_get_contents($stream_out);
			$error = stream_get_contents($stream_err);
			fclose($stream_out);
			if(!$this->executionInProgress)
				$this->disconnect();
			if(empty($error)) {
				return $output;
			}
			return $error;
		} catch(Exception $e) {
			throw new Exception("SshClient::execute(): ".$e->getMessage(), $e->getCode());
		}
	}

	/**
	 * Begins a SSH session in which multiple calls to execute()
	 * can be made within a single connection.
	 * Connections started with this method should be ended
	 * using endExecution().
	 */
	public function beginExecution() {
		try {
			$this->executionInProgress = true;
			$this->connect();
		} catch(Exception $e) {
			throw new Exception("SshClient::beginExecution(): ".$e->getMessage(), $e->getCode());
		}
	}

	/**
	 * Ends a SSH session started with beginExecution()
	 */
	public function endExecution() {
		try {
		$this->executionInProgress = false;
		$this->disconnect();
		} catch(Exception $e) {
			throw new Exception("SshClient::endExecution(): ".$e->getMessage(), $e->getCode());
		}
	}
}
?>
