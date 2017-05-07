#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>

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

// TODO: remove ip_size ?
BOOL parse_url(const char *url, char *ip, int ip_size, int *port, char *rest,
    int rest_size) {

  /* Check if HTTPS */
  if(!url || strlen(url) < 8) {
    PRINT_ERROR("The argument 'url' is not set or is empty");
    return FALSE;
  }

  PRINT_DEBUG("passed url: %s", url);

  if(*(url + 4) == 's') {
    /* HTTPS is not supported at the moment, skip */
    PRINT_DEBUG("HTTPS is not supported, skipping");
    return FALSE;
  }

  const char *ip_begin = strchr(url, ':') + 3;              // http[s?]://^<ip>:<port>/<rest>
  PRINT_DEBUG("ip_begin: %s", ip_begin);
  BOOL is_ipv6 = *ip_begin == '[' ? TRUE : FALSE;
  PRINT_DEBUG("is_ipv6: %s", (is_ipv6 ? "TRUE" : "FALSE"));

  char *rest_begin = NULL;
  if(is_ipv6) {
    if(!(rest_begin = strchr(ip_begin, ']')) || !(rest_begin = strchr(rest_begin, '/'))) {
      // [<ipv6>]^:<port>/<rest>             && :<port>^/<rest>
      PRINT_ERROR("Error: (IPv6) rest_begin is NULL\n");
      return FALSE;
    }
    PRINT_DEBUG("rest_begin: %s", rest_begin);
  }
  else {
    if(!(rest_begin = strchr(ip_begin, '/'))) {                // <ip>:<port>^/<rest>
      PRINT_ERROR("Error: rest_begin is NULL\n");
      return FALSE;
    }
  }

  if(rest_begin != NULL) {
    strcpy(rest, rest_begin);
  }

  char *working_str = (char *)malloc(sizeof(char) * 256); // temporary string buffer
  char *ip_with_brackets = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE + 2); // temporary IP buffer
  memset(working_str, '\0', 256);
  memset(ip_with_brackets, '\0', IPv6_STR_MAX_SIZE + 2);
  PRINT_DEBUG("size to copy: %d", (int)(rest_begin - ip_begin));
  strncpy(working_str, ip_begin, (size_t)(rest_begin - ip_begin)); // "<ip>:<port>"
  PRINT_DEBUG("working_str: %s", working_str);
  char *port_begin = strrchr(working_str, ':');                 // "<ip>^:<port>^"
  PRINT_DEBUG("port_begin: %s", port_begin);
  if(port_begin != NULL) {
    *port_begin = ' ';                                           // "<ip>^ <port>^"
    // maybe memset instead of value assignment ?
    sscanf(working_str, "%s %d", ip_with_brackets, port);      // "<%s> <%d>"
    PRINT_DEBUG("port: %d", *port);
    if(strlen(ip_with_brackets) < 1) {
      PRINT_ERROR("Malformed header_location detected in: '%s'", url);
      free(working_str);
      free(ip_with_brackets);
      return FALSE;
    }
    if(*ip_with_brackets == '[') {
      PRINT_DEBUG("ip_with_brackets: %s", ip_with_brackets);
      //strncpy(ip, ip_with_brackets + 1, strlen(ip_with_brackets) - 1); // uncomment in case we dont need brackets
    }
    else {
      //strcpy(ip, ip_with_brackets); // uncomment in case we dont need brackets
    }
      strcpy(ip, ip_with_brackets);
      PRINT_DEBUG("ip: %s", ip);
  }

  if(rest_begin == NULL) {
    PRINT_ERROR("Malformed header_location detected in: '%s'", url);
    free(working_str);
    free(ip_with_brackets);
    return FALSE;
  }

  free(working_str);
  free(ip_with_brackets);
  return TRUE;
}

int find_interface(struct sockaddr_storage *saddr, char *interface, char *address) {
  // TODO: for porting to Windows see http://msdn.microsoft.com/en-us/library/aa365915.aspx
  struct ifaddrs *interfaces, *ifa;
  struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *)saddr;
  struct sockaddr_in *saddr4 = (struct sockaddr_in *)saddr;
  char *compare_address = NULL;
  int ifindex = -1;
  BOOL is_ipv6 = FALSE;

  PRINT_DEBUG("find_interface(%s, \"%s\", \"%s\")",
              saddr == NULL ? NULL : "saddr",
              interface,
              address);

  /* Make sure we have the buffers allocated */
  if(interface == NULL) {
    PRINT_ERROR("No interface string-buffer available");
    return -1;
  }

  if(address == NULL) {
    PRINT_ERROR("No IP address string-buffer available");
    return -1;
  }

  if(saddr == NULL) {
    PRINT_ERROR("No address structure-buffer available");
    return -1;
  }

  /* Check if address is IPv6*/
  is_ipv6 = inet_pton(AF_INET6,
                      address,
                      (void *)&saddr6->sin6_addr) > 0;

  /* If address not set or is bind-on-all
     then set a bindall address in the struct */
  if(((strlen(interface) == 0) && (strlen(address) == 0)) ||
    (strcmp("0.0.0.0", address) == 0) || (strcmp("::", address) == 0)) {
    if(is_ipv6) {
      saddr->ss_family = AF_INET6;
      saddr6->sin6_addr = in6addr_any;
      PRINT_DEBUG("find_interface(): Matched all addresses (::)\n");
    }
    else {
      saddr->ss_family = AF_INET;
      saddr4->sin_addr.s_addr = htonl(INADDR_ANY);
      PRINT_DEBUG("find_interface(): Matched all addresses (0.0.0.0)");
    }
    return 0;
  }

  /* Try to query for all devices on the system */
  if(getifaddrs(&interfaces) < 0) {
    PRINT_ERROR("Could not find any interfaces\n");
    return -1;
  }

  compare_address = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);

  /* Loop through the interfaces*/
  for(ifa=interfaces; ifa&&ifa->ifa_next; ifa=ifa->ifa_next) {

    /* Helpers */
    struct sockaddr_in6 *ifaddr6 = (struct sockaddr_in6 *)ifa->ifa_addr;
    struct sockaddr_in *ifaddr4 = (struct sockaddr_in *)ifa->ifa_addr;
    int ss_family = ifa->ifa_addr->sa_family;

    memset(compare_address, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);

    /* Extract the IP address in printable format */
    if(is_ipv6 && ss_family == AF_INET6) {
      PRINT_DEBUG("Extracting a IPv6 address");
      if(inet_ntop(AF_INET6,
                (void *)&ifaddr6->sin6_addr,
                compare_address,
                IPv6_STR_MAX_SIZE) == NULL) {
        PRINT_ERROR("Could not extract printable IPv6 for the interface %s: (%d) %s",
                    ifa->ifa_name,
                    errno,
                    strerror(errno));
        return -1;
      }
    }
    else if(!is_ipv6 && ss_family == AF_INET) {
      PRINT_DEBUG("Extracting a IPv4 address");
      if(inet_ntop(AF_INET,
                (void *)&ifaddr4->sin_addr,
                compare_address,
                IPv6_STR_MAX_SIZE) == NULL) {
        PRINT_ERROR("Could not extract printable IPv4 for the interface %s: (%d) %s",
                    ifa->ifa_name,
                    errno,
                    strerror(errno));
        return -1;
      }
    }
    else {
      continue;
    }

    /* Check if this is the desired interface and/or address
       5 possible scenarios:
       interface and address given
       only interface given (address is automatically bindall)
       only address given
       none given (address is automatically bindall) */
    BOOL if_present = strlen(interface) > 0 ? TRUE : FALSE;
    BOOL addr_present = strlen(address) > 0 ? TRUE : FALSE;
    BOOL addr_is_bindall = ((strcmp("0.0.0.0", address) == 0) ||
                            (strcmp("::", address) == 0) ||
                            (strlen(address) == 0)) ?
                            TRUE :
                            FALSE;

    if((if_present && (strcmp(interface, ifa->ifa_name) == 0) &&
       addr_present && (strcmp(address, compare_address))) ||
      (if_present && (strcmp(interface, ifa->ifa_name) == 0) &&
       addr_is_bindall) ||
      (!if_present &&
       addr_present && (strcmp(address, compare_address) == 0)) ||
      (!if_present &&
       addr_is_bindall)) {

      /* Set the interface index to be returned*/
      ifindex = if_nametoindex(ifa->ifa_name);

      PRINT_DEBUG("Matched interface (with index %d) name '%s' with %s address %s", ifindex, ifa->ifa_name, (ss_family == AF_INET ? "IPv4" : "IPv6"), compare_address);

      /* Set the appropriate address in the sockaddr struct */
      if(!is_ipv6 && ss_family == AF_INET) {
        saddr4->sin_addr = ifaddr4->sin_addr;
        PRINT_DEBUG("Setting IPv4 saddr...");
      }
      else if(is_ipv6 && ss_family == AF_INET6){
        saddr6->sin6_addr = ifaddr6->sin6_addr;
        PRINT_DEBUG("Setting IPv6 saddr");
      }

      /* Set family to IP version*/
      saddr->ss_family = ss_family;

      break;
    }

  }

  /* Free getifaddrs allocations */
  if(interfaces) {
    freeifaddrs(interfaces);
  }

  /* free our compare buffer */
  if(compare_address != NULL) {
    free(compare_address);
  }

  return ifindex;
}

// TODO: fix for IPv6
char *get_mac_address_from_socket(const SOCKET sock,
    struct sockaddr_storage *sa_ip, char *ip) {
  char *mac_string = (char *)malloc(sizeof(char) * MAC_STR_MAX_SIZE);
  memset(mac_string, '\0', MAC_STR_MAX_SIZE);

  #if defined BSD || defined APPLE
  /* xxxBSD or MacOS solution */
  PRINT_DEBUG("Using BSD style MAC discovery (sysctl)");
  int sysctl_flags[] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO };
  char *arp_buffer = NULL;
  char *arp_buffer_end = NULL;
  char *arp_buffer_next = NULL;
  struct rt_msghdr *rtm = NULL;
  struct sockaddr_inarp *sin_arp = NULL;
  struct sockaddr_dl *sdl = NULL;
  size_t arp_buffer_size;
  BOOL found_arp = FALSE;
  struct sockaddr_storage *tmp_sin = NULL;

  /* Sanity check, choose IP source */
  if(NULL == sa_ip && NULL != ip) {
    tmp_sin = (struct sockaddr_storage *)malloc(sizeof(struct sockaddr_storage));
    memset(tmp_sin, '\0', sizeof(struct sockaddr_storage));
    if(!inet_pton(sa_ip->ss_family, ip, (sa_ip->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)sa_ip)->sin_addr : (void *)&((struct sockaddr_in6 *)sa_ip)->sin6_addr))) {
      PRINT_ERROR("get_mac_address_from_socket(): Failed to get MAC from given IP (%s)", ip);
      free(mac_string);
      free(tmp_sin);
      return NULL;
    }
    sa_ip = tmp_sin;
  }
  else if(NULL == sa_ip) {
    PRINT_ERROR("Neither IP nor SOCKADDR given, MAC not fetched");
    free(mac_string);
    return NULL;
  }

  /* See how much we need to allocate */
  if(sysctl(sysctl_flags, 6, NULL, &arp_buffer_size, NULL, 0) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): sysctl(): Could not determine neede size");
    free(mac_string);
    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
    return NULL;
  }

  /* Allocate needed memmory */
  if((arp_buffer = malloc(arp_buffer_size)) == NULL) {
    PRINT_ERROR("get_mac_address_from_socket(): Failed to allocate memory for the arp table buffer");
    free(mac_string);
    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
    return NULL;
  }

  /* Fetch the arp table */
  if(sysctl(sysctl_flags, 6, arp_buffer, &arp_buffer_size, NULL, 0) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): sysctl(): Could not retrieve arp table");
    free(arp_buffer);
    free(mac_string);
    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
    return NULL;
  }

  /* Loop through the arp table/list */
  arp_buffer_end = arp_buffer + arp_buffer_size;
  for(arp_buffer_next = arp_buffer; arp_buffer_next < arp_buffer_end; arp_buffer_next += rtm->rtm_msglen) {

    /* See it through another perspective */
    rtm = (struct rt_msghdr *)arp_buffer_next;

    /* Skip to the address portion */
    sin_arp = (struct sockaddr_inarp *)(rtm + 1);
    sdl = (struct sockaddr_dl *)(sin_arp + 1);

    /* Check if address is the one we are looking for */
    if(((struct sockaddr_in *)sa_ip)->sin_addr.s_addr != sin_arp->sin_addr.s_addr) {
      continue;
    }
    found_arp = TRUE;

    /* The proudly save the MAC to a string */
    if (sdl->sdl_alen) {
      unsigned char *cp = (unsigned char *)LLADDR(sdl);
      sprintf(mac_string, "%x:%x:%x:%x:%x:%x", cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
    }

    free(arp_buffer);

    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
  }
  #else
  /* Linux (and rest) solution */
  PRINT_DEBUG("Using Linux style MAC discovery (ioctl SIOCGARP)");
  struct arpreq arp;
  unsigned char *mac = NULL;

  memset(&arp, '\0', sizeof(struct arpreq));
  sa_family_t ss_fam = AF_INET;
  struct in_addr *sin_a = NULL;
  struct in6_addr *sin6_a = NULL;
  BOOL is_ipv6 = FALSE;

  /* Sanity check */
  if(sa_ip == NULL && ip == NULL){
    PRINT_ERROR("Neither IP string nor sockaddr given, MAC not fetched");
    free(mac_string);
    return NULL;
  }

  /* Check if IPv6 */
  struct sockaddr_in6 *tmp = (struct sockaddr_in6 *)malloc(sizeof(struct sockaddr_in6));
  if((sa_ip != NULL && sa_ip->ss_family == AF_INET6)
  || (ip != NULL && inet_pton(AF_INET6, ip, tmp))) {
    is_ipv6 = TRUE;
    ss_fam = AF_INET6;
    PRINT_DEBUG("Looking for IPv6-MAC association");
  }
  else if((sa_ip != NULL && sa_ip->ss_family == AF_INET)
  || (ip != NULL && inet_pton(AF_INET, ip, tmp))) {
    PRINT_DEBUG("Looking for IPv4-MAC association");
  }
  else {
    PRINT_ERROR("sa_ip or ip variable error");
  }
  free(tmp);
  tmp = NULL;

  /* Assign address family for arpreq */
  ((struct sockaddr_storage *)&arp.arp_pa)->ss_family = ss_fam;

  /* Point the needed pointer */
  if(!is_ipv6){
    sin_a = &((struct sockaddr_in *)&arp.arp_pa)->sin_addr;
  }
  else if(is_ipv6){
    sin6_a = &((struct sockaddr_in6 *)&arp.arp_pa)->sin6_addr;
  }

  /* Fill the fields */
  if(sa_ip != NULL) {
    if(ss_fam == AF_INET) {
      *sin_a = ((struct sockaddr_in *)sa_ip)->sin_addr;
    }
    else {
      *sin6_a = ((struct sockaddr_in6 *)sa_ip)->sin6_addr;
    }
  }
  else if(ip != NULL) {

    if(!inet_pton(ss_fam,
        ip,
        (ss_fam == AF_INET ? (void *)sin_a: (void *)sin6_a))) {
    PRINT_ERROR("Failed to get MAC from given IP (%s)", ip);
    PRINT_ERROR("Error %d: %s", errno, strerror(errno));
    free(mac_string);
    return NULL;
    }
  }

  /* Go through all interfaces' arp-tables and search for the MAC */
  /* Possible explanation: Linux arp-tables are if-name specific */
  struct ifaddrs *interfaces = NULL, *ifa = NULL;
  if(getifaddrs(&interfaces) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): getifaddrs():Could not retrieve interfaces");
    free(mac_string);
    return NULL;
  }

  /* Start looping through the interfaces*/
  for(ifa = interfaces; ifa && ifa->ifa_next; ifa = ifa->ifa_next) {

    /* Skip the loopback interface */
    if(strcmp(ifa->ifa_name, "lo") == 0) {
      PRINT_DEBUG("Skipping interface 'lo'");
      continue;
    }

    /* Copy current interface name into the arpreq structure */
    PRINT_DEBUG("Trying interface '%s'", ifa->ifa_name);
    strncpy(arp.arp_dev, ifa->ifa_name, 15);

    ((struct sockaddr_storage *)&arp.arp_ha)->ss_family = ARPHRD_ETHER;

    /* Ask for thE arp-table */
    if(ioctl(sock, SIOCGARP, &arp) < 0) {

      /* Handle failure */
      if(errno == 6) {
        /* if error is "Unknown device or address" then continue/try next interface */
        PRINT_DEBUG("get_mac_address_from_socket(): ioctl(): (%d) %s", errno, strerror(errno));
        continue;
      }
      else {
        PRINT_ERROR("get_mac_address_from_socket(): ioctl(): (%d) %s", errno, strerror(errno));
        continue;
      }
    }

    mac = (unsigned char *)&arp.arp_ha.sa_data[0];

  }

  freeifaddrs(interfaces);

  if(!mac) {
    PRINT_DEBUG("mac is NULL");
    free(mac_string);
    return NULL;
  }

  sprintf(mac_string, "%x:%x:%x:%x:%x:%x", *mac, *(mac + 1), *(mac + 2), *(mac + 3), *(mac + 4), *(mac + 5));
  mac = NULL;
  #endif

  PRINT_DEBUG("Determined MAC string: %s", mac_string);
  return mac_string;
}

BOOL is_address_multicast(const char *address) {
  char *str_first = NULL;
  int int_first = 0;
  BOOL is_ipv6;
  if(address != NULL) {
    // TODO: Write a better IPv6 discovery mechanism
    is_ipv6 = strchr(address, ':') != NULL ? TRUE : FALSE;
    if(strcmp(address, "0.0.0.0") == 0) {
      return TRUE;
    }
    if(is_ipv6) {
      struct in6_addr ip6_ll;
      struct in6_addr ip6_addr;
      inet_pton(AF_INET6, "ff02::c", &ip6_ll);
      inet_pton(AF_INET6, address, &ip6_addr);
      if(ip6_ll.s6_addr == ip6_addr.s6_addr) {
        return TRUE;
      }
      return FALSE;
    }
    else {
      str_first = (char *)malloc(sizeof(char) * 4);
      memset(str_first, '\0', sizeof(char) * 4);
      strncpy(str_first, address, 3);
      int_first = atoi(str_first);
      free(str_first);
      if(int_first > 223 && int_first < 240) {
        return TRUE;
      }
      return FALSE;
    }
  }

  printf("Supplied address ('%s') is not valid\n", address);

  return FALSE;
}

