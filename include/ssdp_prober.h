/** \file ssdp_prober.h
 * The SSDP prober header file.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __SSDP_PROBER_H__
#define __SSDP_PROBER_H__

#include <sys/socket.h> /* struct sockaddr_storage */

#include "common_definitions.h"
#include "configuration.h"

/** A container struct for the SSDP prober. */
typedef struct ssdp_prober_s {
  SOCKET sock;
  struct sockaddr_storage forwarder;
} ssdp_prober_s;

/**
 * Create a standard SSDP probe message.
 *
 * @return A string containing a SSDP search/probe message.
 */
const char *ssdp_probe_message_create(void);

/**
 * Close the passed SSDP prober.
 *
 * @param listener The SSDP prober to close.
 */
void ssdp_prober_close(ssdp_prober_s *prober);

/**
 * Create a SSDP prober. It is used to probe/scan the network in order to
 * discover the existing SSDP-capable devices on the network.
 *
 * @param prober The prober to init.
 * @param conf the global configuration.
 *
 * @return 0 on success, non-0 value on error.
 */
int ssdp_prober_init(ssdp_prober_s *prober, configuration_s *conf);

/**
 * Tells the SSDP prober to probe/scan for SSDP-capable nodes.
 *
 * @param prober The prober to start.
 * @param conf The global configuration to use.
 *
 * @return 0 on success, non-0 value on error.
 */
int ssdp_prober_start(ssdp_prober_s *prober, configuration_s *conf);

#endif /* __SSDP_PROBER_H__ */
