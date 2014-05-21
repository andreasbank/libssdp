<?php
$sock = fsockopen("localhost", 43210, $errno, $errstr, 10.0);
if($sock == null) {
  echo "error\n";
  exit();
}
fwrite($sock, "abused");
echo "Data sent\n";
$data="";
echo "Waiting for data...\n";
while (!feof($sock)) $data.=fread($sock, 512);
echo "Data received: ".$data."\n";
fclose($sock);
?>
