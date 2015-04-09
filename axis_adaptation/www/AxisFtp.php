<?php
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Copyright(C) 2013 by Andreas Bank, andreas.mikael.bank@gmail.com
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class AxisFtp {
  public $ftp_server = '';
  public $ftp_username = 'root';
  public $ftp_password = 'pass';
  public $conn_id = null;
  public function __construct($ip, $username, $password) {
    $this->ftp_server = $ip;
    $this->ftp_username = $username;
    $this->ftp_password = $password;
  }

  /**
   * modify_device_files()
   * Customized method for AXIS cameras.
   */
  public function modify_device_files($local_file, $remote_file, $preg_pattern, $preg_substitute) {
    if(file_exists($local_file))
      unlink($local_file);
    $conn_id = @ftp_connect($this->ftp_server);
    if($conn_id == false) {
      throw new Exception("Ftp connection failed! Check IP and try again.", 1);
    }
    if(!@ftp_login($conn_id, $this->ftp_username, $this->ftp_password)) {
      throw new Exception("Ftp login failed! Check credentials and try again.", 1);
    }
    if(@ftp_get($conn_id, $local_file, $remote_file, FTP_BINARY) == FTP_FAILED) {
      @ftp_close($conn_id);
      throw new Exception("Failed to download file (".$remote_file.")!", 1);
    }
    $local_file_contents = file_get_contents($local_file);
    $local_file_contents = preg_replace($preg_pattern, $preg_substitute, $local_file_contents);
    $local_fileId = fopen($local_file, "r+");
    fwrite($local_fileId, $local_file_contents);
    fclose($local_fileId);
    if(@ftp_put($conn_id, $remote_file, $local_file, FTP_BINARY) == FTP_FAILED) {
      ftp_close($conn_id);
      throw new Exception("Failed to upload file (".$local_file.")!", 1);
    }
    ftp_close($conn_id);
  }

  /**
   * upload()
   *
   */
  public function upload($remote_file, $local_file) {
    $conn_id = ftp_connect($this->ftp_server);
    if($conn_id == false) {
      return 1;
      exit(1);
    }
    if(!ftp_login($conn_id, $this->ftp_username, $this->ftp_password)) {
      return 2;
      exit(1);
    }
    if(ftp_put($conn_id, $remote_file, $local_file, FTP_BINARY) == FTP_FAILED) {
      ftp_close($conn_id);
      return 3;
      exit(1);
    }
    return 0;
    ftp_close($conn_id);
  }

  /**
   * upload_asyn()
   *
   */
  public function upload_asyn($remote_file, $local_file) {
    $conn_id = ftp_connect($this->ftp_server);
    if($conn_id == false) {
      return 1;
    }
    if(!ftp_login($conn_id, $this->ftp_username, $this->ftp_password)) {
      return 2;
    }
    //ftp_chdir($conn_id, "andreas"); // tmp for testing purpouses
    $check = ftp_nb_put($conn_id, $remote_file, $local_file, FTP_BINARY);
    $i = -1;
    while ($check == FTP_MOREDATA) {
      if($i++>1000) $i = 0;
      flush();
      if($i%100 == 0) echo ".";
      $check = ftp_nb_continue($conn_id);
    }
    echo "<br />\n";
    if ($check != FTP_FINISHED) {
      return 3;
    }
    return 0;
  }
}
