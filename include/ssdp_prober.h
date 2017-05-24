#ifndef __SSDP_PROBER_H__
#define __SSDP_PROBER_H__

#include <sys/socket.h> /* struct sockaddr_storage */

#include "common_definitions.h"
#include "configuration.h"

/* A container struct for the SSDP prober */
typedef struct ssdp_prober_s {
  SOCKET sock;
  struct sockaddr_storage forwarder;
} ssdp_prober_s;

/**
 * Create a standard SSDP probe message.
 */
const char *ssdp_probe_message_create(void);

/**
 * Close the passed SSDP prober.
 *
 * @param listener The SSDP prober to close.
 */
void ssdp_prober_close(ssdp_prober_s *prober);

/**
 * Return the underlaying socket from a SSDP prober.
 *
 * @param prober The prober to get the socket from.
 *
 * @return The underlaying socket.
 */
SOCKET ssdp_prober_get_sock(ssdp_prober_s *prober);

/**
 * Return the underlaying forwarder from a SSDP prober.
 *
 * @param prober The prober to get the forwarder from.
 *
 * @return The underlaying forwarder.
 */
struct sockaddr_storage *ssdp_prober_get_forwarder(ssdp_prober_s *prober);

/**
 * Create a SSDP prober. It is used to probe/scan the network in order to
 * discover the existing SSDP-capable devices on the network. Sets errno to the
 * error number on failure.
 *
 * @param prober The prober to init.
 * @param conf the global configuration.
 *
 * @return A SSDP prober.
 */
int ssdp_prober_init(ssdp_prober_s *prober, configuration_s *conf);

/**
 * Tells the SSDP prober to probe/scan for SSDP-capable nodes.
 *
 * @param prober The prober to start.
 * @param conf The global configuration to use.
 *
 * 0 on success, non-0 value on error.
 */
int ssdp_prober_start(ssdp_prober_s *prober, configuration_s *conf);

#endif /* __SSDP_PROBER_H__ */
