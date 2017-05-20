#ifndef __SSDP_LISTENER_H__
#define __SSDP_LISTENER_H__

#include <sys/socket.h>

#include "common_definitions.h"
#include "configuration.h"

#define SSDP_RECV_DATA_LEN 2048 /* 2KiB */

/* An opaque struct for the SSDP listener */
typedef struct ssdp_listener_s ssdp_listener_s;

/* Info about the node that we received a mesage from */
typedef struct ssdp_recv_node_s {
  char from_ip[IPv6_STR_MAX_SIZE];
  char from_mac[MAC_STR_MAX_SIZE];
  int recv_bytes;
  char recv_data[SSDP_RECV_DATA_LEN];
} ssdp_recv_node_s;

/**
 * Create a passive (multicast) SSDP listener. This type of listener is used to
 * passively listen to SSDP messages sent over a network. Sets errno to the
 * error number on failure.
 *
 * @param conf the global configuration.
 *
 * @return A SSDP listener.
 */
ssdp_listener_s *create_ssdp_passive_listener(configuration_s *conf);

/**
 * Create an active (unicast) SSDP listener. This type of listener is used to
 * listen to SSDP query responses. Sets errno to the error number on failure.
 *
 * @param conf The global configuration.
 * @param port The port to listen at.
 *
 * @return A SSDP listener.
 */
ssdp_listener_s *create_ssdp_active_listener(configuration_s *conf, int port);

/**
 * Destroy the passed SSDP listener.
 *
 * @param listener The SSDP listener to destroy.
 */
void destroy_ssdp_listener(ssdp_listener_s *listener);

/**
 * Read from the listener. Blocks untill timeout if no data to read.
 *
 * @param listener The listener to read from.
 * @param recv_node Information about the node (client) that sent data.
 */
void read_ssdp_listener(ssdp_listener_s *listener,
    ssdp_recv_node_s *recv_node);

/**
 * Return the socket from a listener.
 *
 * @param The listener.
 *
 * @return The listener's socket.
 */
SOCKET get_sock_from_listener(ssdp_listener_s *listener);

#endif /* __SSDP_LISTENER_H__ */
