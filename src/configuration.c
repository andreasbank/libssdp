/** \file configuration.c
 * The program/lib configuration
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "common_definitions.h"
#include "configuration.h"
#include "log.h"
#include "net_utils.h"
#include "ssdp_message.h"
#include "string.h"

void set_default_configuration(configuration_s *c) {

  /* Default configuration */
  memset(c->interface, '\0', IPv6_STR_MAX_SIZE);
  memset(c->ip, '\0', IPv6_STR_MAX_SIZE);
  c->run_as_daemon         = FALSE;
  c->run_as_server         = FALSE;
  c->listen_for_upnp_notif = FALSE;
  c->scan_for_upnp_devices = FALSE;
  c->forward_address       = NULL;
  c->ssdp_cache_size       = 10;
  c->fetch_info            = TRUE;
  c->json_output           = FALSE;
  c->xml_output            = FALSE;
  c->ttl                   = 64;
  c->filter                = NULL;
  c->ignore_search_msgs    = TRUE;
  c->use_ipv4              = FALSE;
  c->use_ipv6              = FALSE;
  c->quiet_mode            = FALSE;
  c->upnp_timeout          = MULTICAST_TIMEOUT;
  c->enable_loopback       = FALSE;
}

void usage(void) {
  printf("[A]bused is [B]anks [U]ber-[S]exy-[E]dition [D]aemon\"\n\n");
  printf("USAGE: abused [OPTIONS]\n");
  printf("OPTIONS:\n");
  //printf("\t-C <file.conf>    Configuration file to use\n");
  printf("\t-i                Interface to use, default is all\n");
  printf("\t-I                Interface IP address to use, default is a bind-all address\n");
  printf("\t-t                TTL value (routers to hop), default is 1\n");
  printf("\t-f <string>       Filter for capturing, 'grep'-like effect. Also works\n");
  printf("\t                  for -u and -U where you can specify a list of\n");
  printf("\t                  comma separated filters\n");
  printf("\t-M                Don't ignore UPnP M-SEARCH messages\n");
  printf("\t-S                Run as a server,\n");
  printf("\t                  listens on port 43210 and returns a\n");
  printf("\t                  UPnP scan result (formatted list) for AXIS devices\n");
  printf("\t                  upon receiving the string 'abused'\n");
  printf("\t-d                Run as a UNIX daemon,\n");
  printf("\t                  only works in combination with -S or -a\n");
  printf("\t-u                Listen for local UPnP (SSDP) notifications\n");
  printf("\t-U                Perform an active search for UPnP devices\n");
  printf("\t-a <ip>:<port>    Forward the events to the specified ip and port,\n");
  printf("\t                  also works in combination with -u.\n");
  printf("\t-F                Do not try to parse the \"Location\" header and fetch device info\n");
  //printf("\t-j                Convert results to JSON\n");
  printf("\t-x                Convert results to XML\n");
  //printf("\t-4                Force the use of the IPv4 protocol\n");
  //printf("\t-6                Force the use of the IPv6 protocol\n");
  printf("\t-q                Be quiet!\n");
  printf("\t-T                The time to wait for a devices answer a search query\n");
  printf("\t-L                Enable multicast loopback traffic\n");
}

int parse_args(const int argc, char * const *argv, configuration_s *conf) {
  int opt;

  while ((opt = getopt(argc, argv, "C:i:I:t:f:MSduUma:RFc:jx64qT:L")) > 0) {
    char *pend = NULL;

    switch (opt) {

    case 'C':
      /*if(!parse_configuration_file(conf, optarg)) {
        printf("Parsing of '%s' failed!, optarg);
      };*/
      break;

    case 'i':
      strncpy(conf->interface, optarg, IPv6_STR_MAX_SIZE);
      break;

    case 'I':
      strncpy(conf->ip, optarg, IPv6_STR_MAX_SIZE);
      break;

    case 't':
      // TODO: errorhandling
      pend = NULL;
      conf->ttl = (unsigned char)strtol(optarg, &pend, 10);
      break;

    case 'f':
      conf->filter = optarg;
      break;

    case 'M':
      conf->ignore_search_msgs = FALSE;
      break;

    case 'S':
      conf->run_as_server = TRUE;
      break;

    case 'd':
      conf->run_as_daemon = TRUE;
      break;

    case 'u':
      conf->listen_for_upnp_notif = TRUE;
      break;

    case 'U':
      conf->scan_for_upnp_devices = TRUE;
      break;

    case 'a':
      conf->forward_address = optarg;
      break;

    case 'F':
      conf->fetch_info = FALSE;
      break;

    case 'c':
      // TODO: errorhandling
      pend = NULL;
      conf->ssdp_cache_size = (int)strtol(optarg, &pend, 10);
      break;

    case 'j':
      conf->xml_output = FALSE;
      conf->json_output = TRUE;
      break;

    case 'x':
      conf->json_output = FALSE;
      conf->xml_output = TRUE;
      break;

    case '4':
      conf->use_ipv6 = FALSE;
      conf->use_ipv4 = TRUE;
      break;

    case '6':
      conf->use_ipv4 = FALSE;
      conf->use_ipv6 = TRUE;
      break;

    case 'q':
      conf->quiet_mode = TRUE;
      break;

    case 'T':
      pend = NULL;
      conf->upnp_timeout = (int)strtol(optarg, &pend, 10);
      break;

    case 'L':
      conf->enable_loopback = TRUE;
      break;

    default:
      usage();
      return 1;
    }
  }

  return 0;
}

