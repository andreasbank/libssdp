/** \file configuration.h
 * The program/lib configuration container and related functions.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include "common_definitions.h"
#include "net_definitions.h"

/** A container for the program/lib configuration. */
typedef struct configuration_struct {
  /** Interface to use. */
  char                interface[IPv6_STR_MAX_SIZE];
  /** Interface IP to use. */
  char                ip[IPv6_STR_MAX_SIZE];
  /** Run the program as a daemon ignoring control signals and terminal. */
  BOOL                run_as_daemon;
  /**
   * Run the program as a server, open a port and wait for incoming
   * connection (requests).
   */
  BOOL                run_as_server;
  /** Switch for enabling listening for UPnP (SSDP) notifications. */
  BOOL                listen_for_upnp_notif;
  /** Switch to perform an active UPnP (SSDP) scan. */
  BOOL                scan_for_upnp_devices;
  /** The address to forward the results to. */
  char               *forward_address;
  /** Switch to enable forwarding of the scan results. */
  BOOL                forward_enabled;
  /** Don't try to fetch device info from the url in the "Location" header. */
  BOOL                fetch_info;
  /** The size of the ssdp_cache list. */
  BOOL                ssdp_cache_size;
  /** Convert to JSON before forwarding. */
  BOOL                json_output;
  /** Convert to XML before forwarding. */
  BOOL                xml_output;
  /**
   * Time-To-Live value, how many routers the multicast shall propagate
   * through.
   */
  unsigned char       ttl;
  /** Automatically add a filter to ignore M-SEARCH messages. */
  BOOL                ignore_search_msgs;
  /** Grep-like filter string. */
  char               *filter;
  /** Force the usage of the IPv4 protocol. */
  BOOL                use_ipv4;
  /** Force the usage of the IPv6 protocol. */
  BOOL                use_ipv6;
  /** Minimize informative output. */
  BOOL                quiet_mode;
  /** The time to wait for a device to answer a query. */
  int                 upnp_timeout;
  /** Enable multicast loopback traffic. */
  BOOL                enable_loopback;
} configuration_s;

/**
 * Set the default configuration to a configuration struct.
 *
 * @param conf The configuration to set default values in.
 */
void set_default_configuration(configuration_s *conf);

/**
 * Print the usage help text.
 */
void usage(void);

/**
 * Parses the given arguments and fills the given configuration accordingly.
 *
 * @param argc The number of arguments to parse.
 * @param argv The arguments to parse
 * @param conf The configuration to fill.
 *
 * @return 0 on success, non 0 on failure.
 */
int parse_args(int argc, char * const *argv, configuration_s *conf);

#endif /* __CONFIGURATION_H__ */
