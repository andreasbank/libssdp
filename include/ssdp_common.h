#ifndef __SSDP_COMMON_H__
#define __SSDP_COMMON_H__

#include <sys/socket.h>

#include "configuration.h"

#define SSDP_RECV_DATA_LEN 2048 /* 2KiB */

/* Info about the node that we received a mesage from */
typedef struct ssdp_recv_node_s {
  char from_ip[IPv6_STR_MAX_SIZE];
  char from_mac[MAC_STR_MAX_SIZE];
  int recv_bytes;
  char recv_data[SSDP_RECV_DATA_LEN];
} ssdp_recv_node_s;

/**
 * Print forwarding status (after being parsed into a network-order address).
 *
 * @param conf The global configuration to use.
 * @param forwarder The address of the forward recipient.
 */
void print_forwarder(configuration_s *conf,
    struct sockaddr_storage *forwarder);

#endif /* __SSDP_COMMON_H__ */
