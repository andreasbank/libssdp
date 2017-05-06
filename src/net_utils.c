#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "common_definitions.h"
#include "log.h"
#include "net_definitions.h"
#include "net_utils.h"
#include "string_utils.h"

BOOL parse_address(const char *raw_address, struct sockaddr_storage **pp_address) {
  char *ip;
  struct sockaddr_storage *address = NULL;
  int colon_pos = 0;
  int port = 0;
  BOOL is_ipv6;

  PRINT_DEBUG("parse:address()");

  if(strlen(raw_address) < 1) {
    PRINT_ERROR("No valid IP address specified");
    return FALSE;
  }

  /* Allocate the input address */
  *pp_address = (struct sockaddr_storage *)malloc(sizeof(struct sockaddr_storage));

  /* Get rid of the "pointer to a pointer" */
  address = *pp_address;
  memset(address, 0, sizeof(struct sockaddr_storage));
  colon_pos = strpos(raw_address, ":");

  if(colon_pos < 1) {
    PRINT_ERROR("No valid port specified (%s)\n", raw_address);
    return FALSE;
  }

  ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
  memset(ip, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);

  /* Get rid of [] if IPv6 */
  strncpy(ip, strchr(raw_address, '[') + 1, strchr(raw_address, '[') - strchr(raw_address, ']'));

  is_ipv6 = inet_pton(AF_INET6, ip, address);
  PRINT_DEBUG("is_ipv6 == %s", is_ipv6 ? "TRUE" : "FALSE");
  if(is_ipv6) {
    address->ss_family = AF_INET6;
    port = atoi(strrchr(raw_address, ':') + 1);
  }
  else {
    memset(ip, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);
    strncpy(ip, raw_address, colon_pos);
    if(!inet_pton(AF_INET, ip, &(((struct sockaddr_in *)address)->sin_addr))) {
      PRINT_ERROR("No valid IP address specified (%s)\n", ip);
      return FALSE;
    }
    address->ss_family = AF_INET;
    port = atoi(raw_address + colon_pos + 1);
  }

  if(port < 80 || port > PORT_MAX_NUMBER) {
    PRINT_ERROR("No valid port specified (%d)\n", port);
    return FALSE;
  }

  if(is_ipv6) {
    ((struct sockaddr_in6 *)address)->sin6_port = htons(port);
  }
  else {
    ((struct sockaddr_in *)address)->sin_port = htons(port);
  }

  free(ip);

  return TRUE;
}

