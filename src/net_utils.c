/** \file net_utils.c
 * A collection of network utilities.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#if defined BSD || defined __APPLE__
#include <netinet/if_ether.h>
#include <sys/sysctl.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <sys/file.h>
#endif

#include "common_definitions.h"
#include "log.h"
#include "net_definitions.h"
#include "net_utils.h"
#include "string_utils.h"

int parse_address(const char *raw_address,
    struct sockaddr_storage *address) {
  int ret = 0;
  char *ip = NULL;
  int colon_pos = 0;
  int port = 0;
  BOOL is_ipv6;

  PRINT_DEBUG("parse_address()");

  if(!raw_address || strlen(raw_address) < 1) {
    PRINT_ERROR("No valid IP address specified");
    goto err;
  }

  /* Allocate the input address */
  memset(address, 0, sizeof(struct sockaddr_storage));
  colon_pos = strpos(raw_address, ":");

  if(colon_pos < 1) {
    PRINT_ERROR("No valid port specified (%s)\n", raw_address);
    goto err;
  }

  ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
  memset(ip, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);

  /* Get rid of [] if IPv6 */
  strncpy(ip, strchr(raw_address, '[') + 1,
      strchr(raw_address, '[') - strchr(raw_address, ']'));

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
      goto err;
    }
    address->ss_family = AF_INET;
    port = atoi(raw_address + colon_pos + 1);
  }

  if(port < 80 || port > PORT_MAX_NUMBER) {
    PRINT_ERROR("No valid port specified (%d)\n", port);
    goto err;
  }

  if(is_ipv6) {
    ((struct sockaddr_in6 *)address)->sin6_port = htons(port);
  }
  else {
    ((struct sockaddr_in *)address)->sin_port = htons(port);
  }

  goto success;

err:
  ret = 1;

success:
  if (ip)
    free(ip);

  return ret;
}

// TODO: remove ip_size ?
// TODO: make work with dnames also.
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

int find_interface(struct sockaddr_storage *saddr, const char *interface,
    const char *address) {
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
  if (!interface) {
    PRINT_ERROR("No interface string-buffer available");
    return -1;
  }

  if (!address) {
    PRINT_ERROR("No IP address string-buffer available");
    return -1;
  }

  if (!saddr) {
    PRINT_ERROR("No address structure-buffer available");
    return -1;
  }

  /* Check if address is IPv6*/
  is_ipv6 = inet_pton(AF_INET6,
                      address,
                      (void *)&saddr6->sin6_addr) > 0;

  /* If address not set or is bind-on-all
     then set a bindall address in the struct */
  if (((strlen(interface) == 0) && (strlen(address) == 0)) ||
    (strcmp("0.0.0.0", address) == 0) || (strcmp("::", address) == 0)) {
    if (is_ipv6) {
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
  if (getifaddrs(&interfaces) < 0) {
    PRINT_ERROR("Could not find any interfaces\n");
    return -1;
  }

  compare_address = malloc(sizeof(char) * IPv6_STR_MAX_SIZE);

  /* Loop through the interfaces*/
  for (ifa = interfaces; ifa && ifa->ifa_next; ifa = ifa->ifa_next) {

    /* Helpers */
    struct sockaddr_in6 *ifaddr6 = (struct sockaddr_in6 *)ifa->ifa_addr;
    struct sockaddr_in *ifaddr4 = (struct sockaddr_in *)ifa->ifa_addr;
    int ss_family = ifa->ifa_addr->sa_family;

    memset(compare_address, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);

    /* Extract the IP address in printable format */
    if (is_ipv6 && ss_family == AF_INET6) {
      PRINT_DEBUG("Extracting an IPv6 address");
      get_ip_from_sock_address((struct sockaddr_storage *)ifaddr6,
          compare_address);
      if (compare_address[0] == '\0') {
        PRINT_ERROR("Could not extract printable IPv6 for the interface %s: "
            "(%d) %s", ifa->ifa_name, errno, strerror(errno));
        return -1;
      }
    }
    else if (!is_ipv6 && ss_family == AF_INET) {
      PRINT_DEBUG("Extracting an IPv4 address");
      get_ip_from_sock_address((struct sockaddr_storage *)ifaddr4,
          compare_address);
      if (compare_address[0] == '\0') {
        PRINT_ERROR("Could not extract printable IPv4 for the interface %s: "
            "(%d) %s", ifa->ifa_name, errno, strerror(errno));
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
        (strcmp("::", address) == 0) || (strlen(address) == 0)) ? TRUE : FALSE;

    if ((if_present && (strcmp(interface, ifa->ifa_name) == 0) &&
        addr_present && (strcmp(address, compare_address) == 0)) ||
        (if_present && (strcmp(interface, ifa->ifa_name) == 0) &&
        addr_is_bindall) ||
        (!if_present &&
        addr_present && (strcmp(address, compare_address) == 0)) ||
        (!if_present &&
        addr_is_bindall)) {

      /* Set the interface index to be returned*/
      ifindex = if_nametoindex(ifa->ifa_name);

      PRINT_DEBUG("Matched interface (with index %d) name '%s' with %s "
          "address %s", ifindex, ifa->ifa_name,
          (ss_family == AF_INET ? "IPv4" : "IPv6"), compare_address);

      /* Set the appropriate address in the sockaddr struct */
      if (!is_ipv6 && ss_family == AF_INET) {
        saddr4->sin_addr = ifaddr4->sin_addr;
        PRINT_DEBUG("Setting IPv4 saddr");
      }
      else if (is_ipv6 && ss_family == AF_INET6){
        saddr6->sin6_addr = ifaddr6->sin6_addr;
        PRINT_DEBUG("Setting IPv6 saddr");
      }

      /* Set family to IP version*/
      saddr->ss_family = ss_family;

      break;
    } else {
      PRINT_DEBUG("Match failed, trying next interface (if any)");
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

#if defined BSD || defined __APPLE__
// TODO: fix for IPv6
/* MacOS X variant of the function */
char *get_mac_address_from_socket(const SOCKET sock,
    const struct sockaddr_storage *ss_ip, const char *ip, char *mac_buffer) {
  char *mac_string = mac_buffer ? mac_buffer :
      malloc(sizeof(char) * MAC_STR_MAX_SIZE);
  memset(mac_string, '\0', MAC_STR_MAX_SIZE);

  PRINT_DEBUG("Using BSD style MAC discovery (sysctl)");
  int sysctl_flags[] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS,
      RTF_LLINFO };
  char *arp_buffer = NULL;
  char *arp_buffer_end = NULL;
  char *arp_buffer_next = NULL;
  struct rt_msghdr *rtm = NULL;
  struct sockaddr_inarp *sin_arp = NULL;
  struct sockaddr_dl *sdl = NULL;
  size_t arp_buffer_size;
  BOOL found_arp = FALSE;
  struct sockaddr_storage *local_ss_ip = NULL; /* Used if ss_ip is NULL */

  /* Choose IP source */
  if (ss_ip) {
    local_ss_ip = (struct sockaddr_storage *)ss_ip;
  } else if (ip) {
    local_ss_ip = malloc(sizeof *local_ss_ip);
    memset(local_ss_ip, 0, sizeof *local_ss_ip);
    if (!set_ip_and_port_in_sock_address(ip, 0, local_ss_ip)) {
      PRINT_ERROR("get_mac_address_from_socket(): Failed to get MAC from given"
          " IP (%s)", ip);
      goto err;
    }
  } else {
    PRINT_ERROR("Neither IP nor SOCKADDR given, MAC not fetched");
    goto err;
  }

  /* See how much we need to allocate */
  if (sysctl(sysctl_flags, 6, NULL, &arp_buffer_size, NULL, 0) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): sysctl(): Could not determine "
        "needed size");
    goto err;
  }

  /* Allocate needed memmory */
  if ((arp_buffer = malloc(arp_buffer_size)) == NULL) {
    PRINT_ERROR("get_mac_address_from_socket(): Failed to allocate memory for "
        "the arp table buffer");
    goto err;
  }

  /* Fetch the arp table */
  if(sysctl(sysctl_flags, 6, arp_buffer, &arp_buffer_size, NULL, 0) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): sysctl(): Could not retrieve "
        "arp table");
    goto err;
  }

  /* Loop through the arp table/list */
  arp_buffer_end = arp_buffer + arp_buffer_size;
  for(arp_buffer_next = arp_buffer; arp_buffer_next < arp_buffer_end;
      arp_buffer_next += rtm->rtm_msglen) {

    /* See it through another perspective */
    rtm = (struct rt_msghdr *)arp_buffer_next;

    /* Skip to the address portion */
    sin_arp = (struct sockaddr_inarp *)(rtm + 1);
    sdl = (struct sockaddr_dl *)(sin_arp + 1);

    /* Check if this address is the one we are looking for */
    if (((struct sockaddr_in *)local_ss_ip)->sin_addr.s_addr !=
        sin_arp->sin_addr.s_addr) {
      continue;
    }
    found_arp = TRUE;

    /* Then proudly save the MAC to a string */
    if (sdl->sdl_alen) {
      unsigned char *cp = (unsigned char *)LLADDR(sdl);
      sprintf(mac_string, "%x:%x:%x:%x:%x:%x", cp[0], cp[1], cp[2], cp[3],
          cp[4], cp[5]);
    }

  }

  free(arp_buffer);
  if(local_ss_ip && !ss_ip)
    free(local_ss_ip);

  PRINT_DEBUG("Determined MAC string: %s", mac_string);
  return mac_string;

err:
  if (arp_buffer)
    free(arp_buffer);
  if (!mac_buffer)
    free(mac_string);
  if (local_ss_ip && !ss_ip)
    free(local_ss_ip);

  return NULL;
}

#else
/* Linux variant of the function */
char *get_mac_address_from_socket(const SOCKET sock,
    const struct sockaddr_storage *ss_ip, const char *ip, char *mac_buffer) {
  char *mac_string = mac_buffer ? mac_buffer :
      malloc(sizeof(char) * MAC_STR_MAX_SIZE);
  memset(mac_string, '\0', MAC_STR_MAX_SIZE);

  /* Linux (and rest) solution */
  PRINT_DEBUG("Using Linux style MAC discovery (ioctl SIOCGARP)");
  struct arpreq arp;
  unsigned char *mac = NULL;

  memset(&arp, '\0', sizeof(struct arpreq));
  sa_family_t ss_fam = AF_INET;

  /* Sanity check */
  if(!ss_ip && !ip){
    PRINT_ERROR("Neither IP string nor sockaddr given, MAC not fetched");
    if (!mac_buffer)
      free(mac_string);
    return NULL;
  }

  /* Check if IPv6 */
  if ((ss_ip && ss_ip->ss_family == AF_INET6)
  || (ip && is_address_ipv6(ip))) {
    ss_fam = AF_INET6;
    PRINT_DEBUG("Looking for IPv6-MAC association");
  }
  else if ((ss_ip && ss_ip->ss_family == AF_INET)
  || (ip && is_address_ipv4(ip))) {
    PRINT_DEBUG("Looking for IPv4-MAC association");
  }
  else {
    PRINT_ERROR("ss_ip or ip variable error");
  }

  /* Assign address family for arpreq */
  ((struct sockaddr_storage *)&arp.arp_pa)->ss_family = ss_fam;

  /* Fill the fields */
  if (ss_ip) {
    if(ss_fam == AF_INET) {
      struct in_addr *sin_a = &((struct sockaddr_in *)&arp.arp_pa)->sin_addr;
      *sin_a = ((struct sockaddr_in *)ss_ip)->sin_addr;
    }
    else {
      struct in6_addr *sin6_a =
          &((struct sockaddr_in6 *)&arp.arp_pa)->sin6_addr;
      *sin6_a = ((struct sockaddr_in6 *)ss_ip)->sin6_addr;
    }
  }
  else if (ip) {
    if (!set_ip_and_port_in_sock_address(ip, 0,
      (struct sockaddr_storage *)&arp.arp_pa)) {
    PRINT_ERROR("Failed to get MAC from given IP (%s): %s", ip,
        strerror(errno));
    if (!mac_buffer)
      free(mac_string);
    return NULL;
    }
  }

  /* Go through all interfaces' arp-tables and search for the MAC */
  /* Possible explanation: Linux arp-tables are if-name specific */
  struct ifaddrs *interfaces = NULL, *ifa = NULL;
  if (getifaddrs(&interfaces) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): getifaddrs():Could not "
        "retrieve interfaces");
    if (!mac_buffer)
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

    /* Ask for the arp-table */
    if(ioctl(sock, SIOCGARP, &arp) < 0) {

      /* Handle failure */
      if(errno == 6) {
        /* if error is "Unknown device or address" then continue/try
           next interface */
        PRINT_DEBUG("get_mac_address_from_socket(): ioctl(): (%d) %s", errno,
            strerror(errno));
        continue;
      }
      else {
        PRINT_ERROR("get_mac_address_from_socket(): ioctl(): (%d) %s", errno,
            strerror(errno));
        continue;
      }
    }

    mac = (unsigned char *)&arp.arp_ha.sa_data[0];

  }

  freeifaddrs(interfaces);

  if(!mac) {
    PRINT_DEBUG("mac is NULL");
    if (!mac_buffer)
      free(mac_string);
    return NULL;
  }

  sprintf(mac_string, "%x:%x:%x:%x:%x:%x", *mac, *(mac + 1), *(mac + 2),
      *(mac + 3), *(mac + 4), *(mac + 5));
  mac = NULL;

  PRINT_DEBUG("Determined MAC string: %s", mac_string);
  return mac_string;
}
#endif

BOOL is_address_multicast(const char *address) {
  char *str_first = NULL;
  int int_first = 0;
  BOOL is_ipv6;
  if (address != NULL) {
    is_ipv6 = is_address_ipv6(address);

    if (is_ipv6) {
      struct in6_addr ip6_ll;
      struct in6_addr ip6_addr;
      inet_pton(AF_INET6, "ff02::c", &ip6_ll);
      inet_pton(AF_INET6, address, &ip6_addr);
      if (ip6_ll.s6_addr == ip6_addr.s6_addr) {
        return TRUE;
      }
      return FALSE;
    }
    else {
      if (strcmp(address, "0.0.0.0") == 0)
        return TRUE;

      str_first = malloc(4);
      memset(str_first, '\0', 4);
      strncpy(str_first, address, 3);
      int_first = atoi(str_first);
      free(str_first);
      if (int_first > 223 && int_first < 240) {
        return TRUE;
      }
      return FALSE;
    }
  }

  printf("Supplied address ('%s') is not valid\n", address);

  return FALSE;
}

char *get_ip_from_sock_address(const struct sockaddr_storage *saddr,
    char *ip_buffer) {
  void *sock_addr;
  size_t ip_size = IPv4_STR_MAX_SIZE;
  char *ip = ip_buffer ? ip_buffer : NULL;

  if (!saddr) {
    PRINT_DEBUG("Socket address is empty");
    goto err;
  }

  if (saddr->ss_family == AF_INET) {
    sock_addr = (void *)&((struct sockaddr_in *)saddr)->sin_addr;
  } else if (saddr->ss_family == AF_INET6) {
    sock_addr = (void *)&((struct sockaddr_in6 *)saddr)->sin6_addr;
    ip_size = IPv6_STR_MAX_SIZE;
  } else {
    PRINT_WARN("Not an IP socket address");
    goto err;
  }

  if (!ip)
    ip = malloc(ip_size);

  if (inet_ntop(saddr->ss_family, sock_addr, ip, ip_size) == NULL) {
    PRINT_ERROR("Erroneous sock address");
    if (!ip_buffer)
      free(ip);
    else
      ip_buffer[0] = '\0';

    ip = NULL;
  }

err:
  return ip;
}

inline BOOL is_address_ipv6(const char *ip) {
  struct sockaddr_in6 saddr6;

  return inet_pton(AF_INET6, ip, (void *)&saddr6) > 0;
}

BOOL is_address_ipv6_ex(const char *ip, struct sockaddr_in6 *saddr6) {
  BOOL is_ipv6 = inet_pton(AF_INET6, ip, (void *)&saddr6->sin6_addr) > 0;

  if (is_ipv6)
    saddr6->sin6_family = AF_INET6;

  return is_ipv6;
}

inline BOOL is_address_ipv4(const char *ip) {
  struct in_addr saddr;

  return inet_pton(AF_INET, ip, (void *)&saddr) > 0;
}

BOOL is_address_ipv4_ex(const char *ip, struct sockaddr_in *saddr) {
  BOOL is_ipv4 = inet_pton(AF_INET, ip, (void *)&saddr->sin_addr) > 0;

  if (is_ipv4)
    saddr->sin_family = AF_INET;

  return is_ipv4;
}

BOOL set_ip_and_port_in_sock_address(const char *ip, int port,
    struct sockaddr_storage *saddr) {
  int res = 0;
  void *sock_addr;
  in_port_t *sock_port;

  if (!ip) {
    PRINT_DEBUG("IP address is empty");
    goto err;
  }

  if (port < 1 || port > 65535) {
    PRINT_DEBUG("Invalid port number");
    goto err;
  }

  if (!saddr) {
    PRINT_DEBUG("Socket address is empty");
    goto err;
  }

  if (is_address_ipv6_ex(ip, (struct sockaddr_in6 *)saddr)) {
    sock_addr = (void *)&((struct sockaddr_in6 *)saddr)->sin6_addr;
    sock_port = (in_port_t *)&((struct sockaddr_in6 *)saddr)->sin6_port;
  } else if (is_address_ipv4_ex(ip, (struct sockaddr_in *)saddr)) {
    sock_addr = (void *)&((struct sockaddr_in *)saddr)->sin_addr;
    sock_port = (in_port_t *)&((struct sockaddr_in *)saddr)->sin_port;
  } else {
    PRINT_ERROR("Erroneous IP address");
    goto err;
  }

  res = inet_pton(saddr->ss_family, ip, sock_addr);
  if (res > 0)
    *sock_port = htons(port);

err:
  return res > 0;
}

int get_port_from_sock_address(const struct sockaddr_storage *saddr) {
  int port = 0;

  if (saddr->ss_family == AF_INET) {
    port = ntohs(((struct sockaddr_in *)saddr)->sin_port);
  } else if (saddr->ss_family == AF_INET6) {
    port = ntohs(((struct sockaddr_in6 *)saddr)->sin6_port);
  } else {
    PRINT_ERROR("Erroneous socket address");
  }

  return port;
}
