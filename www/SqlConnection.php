<?php
class SqlConnection {

  /* MySQL host, database and credentials */
  private $host;
  private $database;
  private $username;
  private $password;
  public $connection = NULL;

  public function __construct($host, $database, $username, $password) {
    $this->host = $host;
    $this->database = $database;
    $this->username = $username;
    $this->password = $password;
    $this->connection = new mysqli($host, $username, $password, $database);
    if($this->connection->connect_errno) {
      throw new Exception(sprintf("Failed to connect to MySQL: %s",
                                  $this->connection->connect_error));
    }
  }

  public function __destruct() {
    $this->connection->close();
  }

  public function query($query) {

    $res = $this->connection->query($query);
    if(false === $res) {
      if($res) {
        $res->free();
      }
      throw new Exception(sprintf("Failed to query MySQL: %s",
                                  $this->connection->error));
    }
    $result = $this->resultset_to_array($res);
    $res->free();
    return $result;
  }

  public function call($query) {

    $res = $this->connection->multi_query($query);
    if(false === $res) {
      throw new Exception(sprintf("Failed to call stored procedure on MySQL: %s",
                                  $this->connection->error));
    }

    /* Go through all the returned result-sets */
    $results = array();
    do {
      if($result = $this->connection->use_result()) {
        $results[] = $this->resultset_to_array($result);
        $result->free();
      }
    } while($this->connection->more_results() && $this->connection->next_result());
    return $results;
  }

  public function resultset_to_array($resultset) {
    $results = array();
    while($row = $resultset->fetch_assoc()) {
      $results[] = $row;
    }
    return $results;
  }

}
