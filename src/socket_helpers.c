#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>

#include "log.h"
#include "net_definitions.h"
#include "net_utils.h"
#include "socket_helpers.h"
#include "ssdp_static_defs.h"

/* Set send timeout */
int set_send_timeout(SOCKET sock, int timeout) {
  struct timeval stimeout;
  stimeout.tv_sec = timeout;
  stimeout.tv_usec = 0;

  PRINT_DEBUG("Setting send-timeout to %d", (int)stimeout.tv_sec);

  if(setsockopt(sock,
                SOL_SOCKET,
                SO_SNDTIMEO,
                (char *)&stimeout,
                sizeof(stimeout)) < 0) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return 1;
  }
  return 0;
}

/* Set receive timeout */
int set_receive_timeout(SOCKET sock, int timeout) {
  struct timeval rtimeout;
  rtimeout.tv_sec = timeout;
  rtimeout.tv_usec = 0;

  PRINT_DEBUG("Setting receive-timeout to %d", (int)rtimeout.tv_sec);

  if(setsockopt(sock,
                SOL_SOCKET,
                SO_RCVTIMEO,
                (char *)&rtimeout,
                sizeof(rtimeout)) < 0) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return 1;
  }

  return 0;
}

/* Enable socket to receive from an already used port */
int set_reuseaddr(SOCKET sock) {
  int reuse = 1;
  PRINT_DEBUG("Setting reuseaddr");
  if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return 1;
  }
  /* linux >= 3.9
  if(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return 1;
  }
  */

  return 0;
}

/* Set socket keepalive */
int set_keepalive(SOCKET sock, BOOL keepalive) {
  PRINT_DEBUG("Setting keepalive to %d", keepalive);
  if(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&keepalive, sizeof(BOOL)) < 0) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return 1;
  }

  return 0;
}

/* Set TTL */
int set_ttl(SOCKET sock, int family, int ttl) {
    PRINT_DEBUG("Setting TTL to %d", ttl);
    if(setsockopt(sock,
                  (family == AF_INET ? IPPROTO_IP : IPPROTO_IPV6),
                  (family == AF_INET ? IP_MULTICAST_TTL : IPV6_MULTICAST_HOPS),
                  (char *)&ttl,
                  sizeof(ttl)) < 0) {
      PRINT_ERROR("(%d) %s", errno, strerror(errno));
      return 1;
    }

    return 0;
}

/* Disable multicast loopback traffic */
int disable_multicast_loopback(SOCKET sock, int family) {
  unsigned char loop = FALSE;
  PRINT_DEBUG("Disabling loopback multicast traffic");
  if(setsockopt(sock,
                family == AF_INET ? IPPROTO_IP :
                                    IPPROTO_IPV6,
                family == AF_INET ? IP_MULTICAST_LOOP :
                                     IPV6_MULTICAST_LOOP,
                &loop,
                sizeof(loop)) < 0) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return 1;
  }

  return 0;
}

/* Join the multicast group on required interfaces */
int join_multicast_group(SOCKET sock, char *multicast_group, char *interface_ip) {
  struct ifaddrs *ifa, *interfaces = NULL;
  BOOL is_bindall = FALSE;
  BOOL is_mc_ipv6 = FALSE;
  BOOL is_ipv6 = FALSE;
  struct sockaddr_storage sa_interface;

  PRINT_DEBUG("join_multicast_group(%d, \"%s\", \"%s\")", sock,
      multicast_group, interface_ip);

  /* Check if multicast_group is a IPv6 address*/
  if (inet_pton(AF_INET6, interface_ip,
      (void *)&((struct sockaddr_in6 *)&sa_interface)->sin6_addr) > 0) {
    is_mc_ipv6 = TRUE;
  }
  else {
    /* Check if multicast_group is a IPv4 address */
    int res;

    if ((res = inet_pton(AF_INET, interface_ip,
        (void *)&((struct sockaddr_in *)&sa_interface)->sin_addr)) < 1) {
      if (res == 0) {
        PRINT_ERROR("interface_ip is NULL");
      } else {
        PRINT_ERROR("Given multicast group address '%s' is neither an IPv4 nor"
            " IPv6, cannot continue (%d)", interface_ip);
      }
      return 1;
    }
  }

  PRINT_DEBUG("Multicast group address is IPv%d", (is_mc_ipv6 ? 6 : 4));

  /* If interface_ip is empty string set it to "0.0.0.0" */
  if(interface_ip != NULL && strlen(interface_ip) == 0) {
    if(is_mc_ipv6) {
      strncpy(interface_ip, "::", IPv6_STR_MAX_SIZE);
    }
    else {
      strncpy(interface_ip, "0.0.0.0", IPv6_STR_MAX_SIZE);
    }
    PRINT_DEBUG("interface_ip was empty, set to '%s'", interface_ip);
  }

  /* Check if interface_ip is a IPv6 address*/
  if (inet_pton(AF_INET6, interface_ip,
      (void *)&((struct sockaddr_in6 *)&sa_interface)->sin6_addr) > 0) {
    is_ipv6 = TRUE;
  }
  else {
    /* Check if interface_ip is a IPv4 address */
    int res;

    if ((res = inet_pton(AF_INET, interface_ip,
        (void *)&((struct sockaddr_in *)&sa_interface)->sin_addr)) < 1) {
      if (res == 0) {
        PRINT_ERROR("interface_ip is NULL");
      } else {
        PRINT_ERROR("Given interface address '%s' is neither an IPv4 nor IPv6,"
            " cannot continue", interface_ip);
      }
      return 1;
    }
  }

  PRINT_DEBUG("Interface address is IPv%d", (is_mc_ipv6 ? 6 : 4));

  /* Check if the address is a bind-on-all address*/
  if(strcmp("0.0.0.0", interface_ip) == 0 ||
     strcmp("::", interface_ip) == 0) {
    is_bindall = TRUE;
  }

  PRINT_DEBUG("Interface address is %s", (is_bindall ? "a bind-all address" :
      interface_ip));

  /* Get all interfaces and IPs */
  if(getifaddrs(&interfaces) < 0) {
    PRINT_ERROR("Could not find any interfaces: (%d) %s\n", errno,
        strerror(errno));
    return 1;
  }

  /* DEBUG BEGIN */
  PRINT_DEBUG("List of available interfaces and IPs:");
  PRINT_DEBUG("********************");
  for (ifa = interfaces; ifa != NULL; ifa = ifa->ifa_next) {
    struct in_addr *ifaddr4 =
        (struct in_addr *)&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
    struct in6_addr *ifaddr6 =
        (struct in6_addr *)&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
    char ip[IPv6_STR_MAX_SIZE];
    if(ifa->ifa_addr->sa_family != AF_INET &&
       ifa->ifa_addr->sa_family != AF_INET6) {
      PRINT_DEBUG("Not an internet address, skipping interface.");
      continue;
    }
    if(inet_ntop(ifa->ifa_addr->sa_family,
                 ifa->ifa_addr->sa_family == AF_INET ? (void *)ifaddr4 :
                                        (void *)ifaddr6,
                 ip,
                 IPv6_STR_MAX_SIZE) == NULL) {
      PRINT_ERROR("Failed to extract address, skipping interface: (%d) %s",
                  errno,
                  strerror(errno));
      continue;
    }
    PRINT_DEBUG("IF: %s; IP: %s", ifa->ifa_name, ip);
  }
  PRINT_DEBUG("********************");
  /* DEBUG END */

  PRINT_DEBUG("Start looping through available interfaces and IPs");

  /* Loop throgh all the interfaces */
  for (ifa = interfaces; ifa != NULL; ifa = ifa->ifa_next) {

    /* Skip loopback addresses */
    if(ifa->ifa_flags & IFF_LOOPBACK) {
      PRINT_DEBUG("Loopback address detected, skipping");
      continue;
    }

    /* Helpers */
    struct in_addr *ifaddr4 =
        (struct in_addr *)&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
    struct in6_addr *ifaddr6 =
        (struct in6_addr *)&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
    int ss_family = ifa->ifa_addr->sa_family;

    /* Skip if not a required type of IP address */
    if((!is_ipv6 && ss_family != AF_INET) ||
       (is_ipv6 && ss_family != AF_INET6)) {
      PRINT_DEBUG("Skipping interface (%s) address, wrong type (%d)",
                  ifa->ifa_name,
                  ss_family);
      continue;
    }

    /* Extract IP in string format */
    char ip[IPv6_STR_MAX_SIZE];
    if(inet_ntop(ss_family,
                 ss_family == AF_INET ? (void *)ifaddr4 :
                                        (void *)ifaddr6,
                 ip,
                 IPv6_STR_MAX_SIZE) == NULL) {
      PRINT_ERROR("Failed to extract address, will skip current interface:"
          " (%d) %s", errno, strerror(errno));
      continue;
    }

    /* If not using a bindall IP then skip all
       IP that do not match the desired IP */
    if(!is_bindall && strcmp(ip, interface_ip) != 0) {
      PRINT_DEBUG("Skipping interface (%s) address, wrong IP (%s != %s)",
                  ifa->ifa_name,
                  ip,
                  interface_ip);
      continue;
    }

    PRINT_DEBUG("Found candidate interface %s, type %d, IP %s", ifa->ifa_name,
        ss_family, ip);

    struct ip_mreq mreq;
    struct ipv6_mreq mreq6;

    if(!is_ipv6) {
      memset(&mreq, 0, sizeof(struct ip_mreq));
      mreq.imr_interface.s_addr = ifaddr4->s_addr;
    }
    else {
      memset(&mreq6, 0, sizeof(struct ipv6_mreq));
      mreq6.ipv6mr_interface = if_nametoindex(ifa->ifa_name);
    }

    int res;
    if((res = inet_pton(ss_family,
                 multicast_group,
                 (is_ipv6 ? (void *)&mreq6.ipv6mr_multiaddr :
                            (void *)&mreq.imr_multiaddr))) < 1) {
      if (res == 0) {
        PRINT_ERROR("interface_ip is NULL");
      } else {
        PRINT_ERROR("Incompatible multicast group");
      }
      return 1;
    }

    #ifdef DEBUG___
    {
      PRINT_DEBUG("%s", (is_ipv6 ? "IPV6_ADD_MEMBERSHIP" : "IP_ADD_MEMBERSHIP"));
      char a[IPv6_STR_MAX_SIZE];
      if(is_ipv6) {
        PRINT_DEBUG("mreq6.ipv6mr_interface: %d", mreq6.ipv6mr_interface);
      }
      else {
        inet_ntop(ss_family, (void *)&mreq.imr_interface, a, IPv6_STR_MAX_SIZE);
        PRINT_DEBUG("mreq.imr_interface: %s", a);
      }
      inet_ntop(ss_family,
                is_ipv6 ? (void *)&mreq6.ipv6mr_multiaddr :
                          (void *)&mreq.imr_multiaddr,
                a,
                IPv6_STR_MAX_SIZE);
      PRINT_DEBUG("mreq%smr_multiaddr: %s", (is_ipv6 ? "6.ipv6" : ".i"), a);
    }
    #endif

    PRINT_DEBUG("Joining multicast group '%s' on interface IP '%s'", multicast_group, ip);

    if(setsockopt(sock,
                  is_ipv6 ? IPPROTO_IPV6 :
                            IPPROTO_IP,
                  is_ipv6 ? IPV6_ADD_MEMBERSHIP :
                            IP_ADD_MEMBERSHIP,
                  is_ipv6 ? (void *)&mreq6 :
                            (void *)&mreq,
                  is_ipv6 ? sizeof(struct ipv6_mreq) :
                            sizeof(struct ip_mreq)) < 0) {
      PRINT_ERROR("(%d) %s", errno, strerror(errno));
      close(sock);
      freeifaddrs(interfaces);
      return 1;
    }
  }

  PRINT_DEBUG("Finished looping through interfaces and IPs");

  /* Free ifaddrs interfaces */
  if(interfaces != NULL) {
    freeifaddrs(interfaces);
    interfaces = NULL;
  }

  return 0;
}

/**
 * Creates a connection to a given host
 * TODO: >>>TO BE DEPRECATED SOON<<<
 */
SOCKET setup_socket(BOOL is_ipv6, BOOL is_udp, BOOL is_multicast,
    char *interface, char *if_ip, struct sockaddr_storage *sa,
    const char *ip, int port, BOOL is_server, int queue_length, BOOL keepalive,
    int ttl, BOOL loopback) {
  SOCKET sock;
  int protocol;
  int ifindex = 0;
  struct ip_mreq mreq;
  struct ipv6_mreq mreq6;
  struct sockaddr_storage *saddr;

  saddr = (struct sockaddr_storage *)malloc(sizeof(struct sockaddr_storage));
  memset(saddr, 0, sizeof(struct sockaddr_storage));
  memset(&mreq, 0, sizeof(mreq));
  memset(&mreq6, 0, sizeof(mreq6));

  /* If 'sa' (sockaddr) given instead of 'if_ip' (char *)
     then fill 'if_ip' with the IP from the sockaddr */
  if(sa != NULL) {
    inet_ntop(sa->ss_family, sa->ss_family == AF_INET ?
        (void *)&((struct sockaddr_in *)sa)->sin_addr :
        (void *)&((struct sockaddr_in6 *)sa)->sin6_addr, if_ip,
        IPv6_STR_MAX_SIZE);
  }

  /* Find the index of the interface */
  ifindex = find_interface(saddr, interface, if_ip);
  if(ifindex < 0) {
    PRINT_ERROR("The requested interface '%s' could not be found\n", interface);
    free(saddr);
    return SOCKET_ERROR;
  }

  #ifdef DEBUG___
  {
    char a[100];
    memset(a, '\0', 100);
    if(!inet_ntop(saddr->ss_family, (saddr->ss_family == AF_INET ?
        (void *)&((struct sockaddr_in *)saddr)->sin_addr :
        (void *)&((struct sockaddr_in6 *)saddr)->sin6_addr), a, 100)) {
      PRINT_ERROR("setup_socket(); inet_ntop(): (%d) %s", errno,
          strerror(errno));
    }
    PRINT_DEBUG("find_interface() returned: saddr->sin_addr: %s", a);
  }
  #endif

  /* If muticast requested, check if it really is a multicast address */
  if (is_multicast && (ip != NULL && strlen(ip) > 0)) {
    if(!is_address_multicast(ip)) {
      PRINT_ERROR("The specified IP (%s) is not a multicast address\n", ip);
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      return SOCKET_ERROR;
    }
  }

  /* Set protocol version */
  if (is_ipv6) {
    saddr->ss_family = AF_INET6;
    protocol = IPPROTO_IPV6;
  }
  else {
    saddr->ss_family = AF_INET;
    protocol = IPPROTO_IP;
  }

  /* init socket */
  sock = socket(saddr->ss_family, is_udp ? SOCK_DGRAM : SOCK_STREAM, protocol);

  if(sock < 0) {
    PRINT_ERROR("Failed to create socket. (%d) %s", errno, strerror(errno));
    free(saddr);
    if(sa != NULL) {
      free(interface);
    }
    return SOCKET_ERROR;
  }

  /* Set reuseaddr */
  if(set_reuseaddr(sock)) {
    free(saddr);
    if(sa != NULL) {
      free(interface);
    }
    return SOCKET_ERROR;
  }

  /* Set keepalive */
  if(set_keepalive(sock, keepalive)) {
    free(saddr);
    if(sa != NULL) {
      free(interface);
    }
    return SOCKET_ERROR;
  }

  /* Set TTL */
  if(!is_server && is_multicast) {
    if(set_ttl(sock, (is_ipv6 ? AF_INET6 : AF_INET), ttl) < 0) {
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      return SOCKET_ERROR;
    }
  }

  /* Setup address structure containing address information to use to connect */
  if(is_ipv6) {
    PRINT_DEBUG("is_ipv6 == TRUE");
    struct sockaddr_in6 * saddr6 = (struct sockaddr_in6 *)saddr;
    saddr6->sin6_port = htons(port);
    if(is_udp && is_multicast) {
      PRINT_DEBUG("  is_udp == TRUE && is_multicast == TRUE");
      if(ip == NULL || strlen(ip) < 1) {
        inet_pton(saddr->ss_family, SSDP_ADDR6_LL, &mreq6.ipv6mr_multiaddr);
      }
      else {
        inet_pton(saddr->ss_family, ip, &mreq6.ipv6mr_multiaddr);
      }
      if(interface != NULL && strlen(interface) > 0) {
        mreq6.ipv6mr_interface = (unsigned int)ifindex;
      }
      else {
        mreq6.ipv6mr_interface = (unsigned int)0;
      }
      //saddr6->sin6_addr =  mreq6.ipv6mr_multiaddr;

      #ifdef DEBUG___
      {
        if(!is_server) {
          PRINT_DEBUG("    IPV6_MULTICAST_IF");
        }
        char a[100];
        PRINT_DEBUG("    mreq6->ipv6mr_interface: %d", ifindex);
        inet_ntop(saddr->ss_family, (void *)&mreq6.ipv6mr_multiaddr, a, 100);
        PRINT_DEBUG("    mreq6->ipv6mr_multiaddr: %s", a);
      }
      #endif

      if(!is_server && setsockopt(sock,
                                  protocol,
                                  IPV6_MULTICAST_IF,
                                  &mreq6.ipv6mr_interface,
                                  sizeof(mreq6.ipv6mr_interface)) < 0) {
        PRINT_ERROR("setsockopt() IPV6_MULTICAST_IF: (%d) %s", errno,
            strerror(errno));
        free(saddr);
        if(sa != NULL) {
          free(interface);
        }
        return SOCKET_ERROR;
      }
    }
  }
  else {
    PRINT_DEBUG("is_ipv6 == FALSE");
    struct sockaddr_in *saddr4 = (struct sockaddr_in *)saddr;
    saddr4->sin_port = htons(port);
    if(is_udp && is_multicast) {
      PRINT_DEBUG("  is_udp == TRUE && is_multicast == TRUE");
      if(ip == NULL || strlen(ip) < 1) {
        inet_pton(saddr->ss_family, SSDP_ADDR, &mreq.imr_multiaddr);
      }
      else {
        inet_pton(saddr->ss_family, ip, &mreq.imr_multiaddr);
      }
      if((if_ip != NULL && strlen(if_ip) > 0) || 
          (interface != NULL && strlen(interface) > 0)) {
         mreq.imr_interface.s_addr = saddr4->sin_addr.s_addr;
      }
      else {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
      }
      //saddr4->sin_addr =  mreq.imr_multiaddr;
      #ifdef DEBUG___
      {
        if(!is_server) {
          PRINT_DEBUG("    IP_MULTICAST_IF");
        }
        char a[100];
        inet_ntop(saddr->ss_family, (void *)&mreq.imr_interface, a, 100);
        PRINT_DEBUG("    mreq->imr_interface: %s", a);
        inet_ntop(saddr->ss_family, (void *)&mreq.imr_multiaddr, a, 100);
        PRINT_DEBUG("    mreq->imr_multiaddr: %s", a);
      }
      #endif
      if (!is_server && setsockopt(sock, protocol, IP_MULTICAST_IF,
          &mreq.imr_interface, sizeof(mreq.imr_interface)) < 0) {
        PRINT_ERROR("setsockopt() IP_MULTICAST_IF: (%d) %s", errno,
            strerror(errno));
        free(saddr);
        if(sa != NULL) {
          free(interface);
        }
        return SOCKET_ERROR;
      }
    }
  }

  /* Enable/disable loopback multicast */
  if(is_server && is_multicast && !loopback &&
     disable_multicast_loopback(sock, saddr->ss_family)) {
    free(saddr);
    if(sa != NULL) {
      free(interface);
    }
    return SOCKET_ERROR;
  }

  /* If server requested, bind the socket to the given address and port*/
  // TODO: Fix for IPv6
  if(is_server) {

    struct sockaddr_in bindaddr;
    bindaddr.sin_family = saddr->ss_family;
    bindaddr.sin_addr.s_addr = mreq.imr_multiaddr.s_addr;
    bindaddr.sin_port = ((struct sockaddr_in *)saddr)->sin_port;

    #ifdef DEBUG___
    PRINT_DEBUG("is_server == TRUE");
    char a[100];
    inet_ntop(bindaddr.sin_family,
        (void *)&((struct sockaddr_in *)&bindaddr)->sin_addr, a, 100);
    PRINT_DEBUG("  bind() to: saddr->sin_family: %d(%d)",
        ((struct sockaddr_in *)&bindaddr)->sin_family, AF_INET);
    PRINT_DEBUG("  bind() to: saddr->sin_addr: %s (port %d)", a,
        ntohs(((struct sockaddr_in *)&bindaddr)->sin_port));
    #endif

    if(bind(sock, (struct sockaddr *)&bindaddr,
        (bindaddr.sin_family == AF_INET ? sizeof(struct sockaddr_in) :
        sizeof(struct sockaddr_in6))) < 0) {
      PRINT_ERROR("setup_socket(); bind(): (%d) %s", errno, strerror(errno));
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      return SOCKET_ERROR;
    }
    if(!is_udp) {
      PRINT_DEBUG("  is_udp == FALSE");
      if (listen(sock, queue_length) == SOCKET_ERROR) {
        PRINT_ERROR("setup_socket(); listen(): (%d) %s", errno,
            strerror(errno));
        close(sock);
        free(saddr);
        if(sa != NULL) {
          free(interface);
        }
        return SOCKET_ERROR;
      }
    }
  }

  /* Make sure we have a string IP before joining multicast groups */
  /* NOTE: Workaround until refactoring is complete */
  char iface_ip[IPv6_STR_MAX_SIZE];
  if(if_ip == NULL || strlen(if_ip) < 1) {
    if(inet_ntop(is_ipv6 ? AF_INET6 :
                           AF_INET,
                 is_ipv6 ? (void *)&((struct sockaddr_in6 *)saddr)->sin6_addr :
                           (void *)&((struct sockaddr_in *)saddr)->sin_addr,
                 iface_ip,
                 IPv6_STR_MAX_SIZE) == NULL) {
      PRINT_ERROR("Failed to get string representation of the interface IP address: (%d) %s", errno, strerror(errno));
      free(saddr);
      return SOCKET_ERROR;
    }
  }
  else {
    strcpy(iface_ip, if_ip);
  }

  /* Join the multicast group on required interfaces */
  if(is_server && is_multicast && join_multicast_group(sock,
                          is_ipv6 ? SSDP_ADDR6_SL :
                                    SSDP_ADDR,
                          iface_ip)) {
    PRINT_ERROR("Failed to join required multicast group");
    return SOCKET_ERROR;
  }

  free(saddr);

  return sock;
}

