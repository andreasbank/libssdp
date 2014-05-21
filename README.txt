COPYRIGHT NOTICE:
  Copyright(C) by Andreas Bank, andreas.mikael.bank@gmail.com

[A]used is  [B]anks [U]ber-[S]exy-[E]dition [D]aemon"

BUILD & INSTALL:

  1. Download both the Makefile and abused.c in one (same) directory
  2. In a terminal run `make´
  3. Then run `chmod 755 abused´
  4. At last run `sudo cp <path-to-download-directory>/abused /usr/local/bin´


USAGE:

  abused [OPTIONS] [interface]

	-C <file.conf>    Configuration file to use
	-i                Interface to use, default is all
	-t                TTL value (routers to hop), default is 1
	-s                Show scanned interfaces at start
	-l                Include link-local addresses
	-b                Print only Bonjour friendly name, not full url
	-c                Do not use color when printing
	-r                Use red color when printing (default color is green)
	-o                Print each devices information in one line
	-f <string>       Filter for capturing, 'grep'-like effect. Also works
	                  for -u and -U where you can specify a list of
                    comma separated filters
	-S                Run as a server,
	                  listens on port 43210 and returns a
	                  bonjour scan result (formatted list) for AXIS devices
	                  upon receiving the string 'abused'
	-d                Run as a UNIX daemon,
	                  only works in combination with -S
	-u                Listen for local UPnP (SSDP) notifications
	-U                Perform an active search for UPnP devices
	-a <ip>:<port>    Forward the events to the specified ip and port,
	                  also works in combination with -u.
	-R                Output unparsed raw data
	-F                Do not try to parse the "Location" header to get more device info
	-x                Convert results to XML before use or output
	-4                Force the use of the IPv4 protocol
	-6                Force the use of the IPv6 protocol

