#ifndef __SSDP_LISTENER_H__
#define __SSDP_LISTENER_H__

#include <sys/socket.h> /* struct sockaddr_storage */

#include "common_definitions.h"
#include "configuration.h"

/* A container struct for the SSDP listener */
typedef struct ssdp_listener_s {
  SOCKET sock;
  struct sockaddr_storage forwarder;
} ssdp_listener_s;

/**
 * Create a passive (multicast) SSDP listener. This type of listener is used to
 * passively listen to SSDP messages sent over a network. Sets errno to the
 * error number on failure.
 *
 * @param listener The listener to init.
 * @param conf the global configuration.
 *
 * @return A SSDP listener.
 */
int ssdp_passive_listener_init(ssdp_listener_s *listener,
    configuration_s *conf);

/**
 * Create an active (unicast) SSDP listener. This type of listener is used to
 * listen to SSDP query responses. Sets errno to the error number on failure.
 *
 * @param listener The listener to init.
 * @param conf The global configuration.
 * @param port The port to listen at.
 *
 * @return A SSDP listener.
 */
int ssdp_active_listener_init(ssdp_listener_s *listener, configuration_s *conf,
    int port);

/**
 * Destroy the passed SSDP listener.
 *
 * @param listener The SSDP listener to destroy.
 */
void ssdp_listener_close(ssdp_listener_s *listener);

/**
 * Read from the listener. Blocks untill timeout if no data to read.
 *
 * @param listener The listener to read from.
 * @param recv_node Information about the node (client) that sent data.
 */
void ssdp_listener_read(ssdp_listener_s *listener,
    ssdp_recv_node_s *recv_node);

/**
 * Return the underlaying socket from a SSDP listener.
 *
 * @param prober The listener to get the socket from.
 *
 * @return The underlaying socket.
 */
SOCKET ssdp_listener_get_sock(ssdp_listener_s *listener);

/**
 * Return the underlaying forwarder from a SSDP listener.
 *
 * @param prober The listener to get the forwarder from.
 *
 * @return The underlaying forwarder.
 */
struct sockaddr_storage *ssdp_listener_get_forwarder(ssdp_listener_s *listener);

/**
 * Tells the SSDP listener to start listening to SSDP messages.
 *
 * @param listener The SSDP listener to start.
 * @param conf The global configuration to use.
 *
 * Non-0 value on error. This function does not return if no error ocurrs.
 */
int ssdp_listener_start(ssdp_listener_s *listener, configuration_s *conf);

#endif /* __SSDP_LISTENER_H__ */
