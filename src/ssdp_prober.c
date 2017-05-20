#include <stdlib.h>

#define PROBE_MSG \
  "M-SEARCH * HTTP/1.1\r\n" \
  "Host:239.255.255.250:1900\r\n" \
  "ST:urn:axis-com:service:BasicService:1\r\n" \
  "Man:\"ssdp:discover\"\r\n" \
  "MX:0\r\n\r\n"

const char *create_ssdp_probe_message(void) {
  return PROBE_MSG;
}
