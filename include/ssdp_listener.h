#ifndef __SSDP_LISTENER_H__
#define __SSDP_LISTENER_H__

#include <sys/socket.h>

#include "configuration.h"

/* An opaque struct for the SSDP listener */
typedef struct ssdp_listener_s ssdp_listener_s;

/**
 * Create a UPNP listener. Sets errno to the error number on failure.
 *
 * @param conf the global configuration.
 *
 * @return The SSDP listener.
 */
ssdp_listener_s *create_ssdp_listener(configuration_s *conf);

/**
 * Destroy the passed SSDP listener.
 *
 * @param listener The SSDP listener to destroy.
 */
void destroy_ssdp_listener(ssdp_listener_s *listener);

/**
 * Read from the listener. Blocks untill timeout if no data to read.
 * ...
 */
int read_ssdp_listener(ssdp_listener_s *listener, char *buffer,
    size_t buffer_len, struct sockaddr_storage *client_addr);

#endif /* __SSDP_LISTENER_H__ */
