#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include "common_definitions.h"
#include "net_definitions.h"

typedef struct configuration_struct {
  char                interface[IPv6_STR_MAX_SIZE];   /* Interface to use */
  char                ip[IPv6_STR_MAX_SIZE];          /* Interface IP to use */
  BOOL                run_as_daemon;                  /* Run the program as a daemon ignoring control signals and terminal */
  BOOL                run_as_server;                  /* Run the program as a server, open a port and wait for incoming connection (requests) */
  BOOL                listen_for_upnp_notif;          /* Switch for enabling listening for UPnP (SSDP) notifications */
  BOOL                scan_for_upnp_devices;          /* Switch to perform an active UPnP (SSDP) scan */
  BOOL                forward_enabled;                /* Switch to enable forwarding of the scan results */
  BOOL                fetch_info;                     /* Don't try to fetch device info from the url in the "Location" header */
  BOOL                ssdp_cache_size;                /* The size of the ssdp_cache list */
  BOOL                json_output;                    /* Convert to JSON before forwarding */
  BOOL                xml_output;                     /* Convert to XML before forwarding */
  unsigned char       ttl;                            /* Time-To-Live value, how many routers the multicast shall propagate through */
  BOOL                ignore_search_msgs;             /* Automatically add a filter to ignore M-SEARCH messages*/
  char               *filter;                         /* Grep-like filter string */
  BOOL                use_ipv4;                       /* Force the usage of the IPv4 protocol */
  BOOL                use_ipv6;                       /* Force the usage of the IPv6 protocol */
  BOOL                quiet_mode;                     /* Minimize informative output */
  int                 upnp_timeout;                   /* The time to wait for a device to answer a query */
  BOOL                enable_loopback;                /* Enable multicast loopback traffic */
} configuration_s;

void set_default_configuration(configuration_s *);
void usage(void);
int parse_args(int, char * const *, configuration_s *);

#endif /* __CONFIGURATION_H__ */
