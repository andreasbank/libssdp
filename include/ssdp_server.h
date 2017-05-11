#ifndef __SSDP_SERVER_H__
#define __SSDP_SERVER_H__

#include <sys/socket.h>

#include "configuration.h"

/* An opaque struct for the ssdp server */
typedef struct ssdp_server ssdp_server;

/**
 * Create a UPNP listener. Sets errno to the error number on failure.
 *
 * @param conf the global configuration.
 *
 * @return The SSDP server.
 */
ssdp_server *create_ssdp_server(configuration_s *conf);

/**
 * Destroy the passed SSDP server.
 *
 * @param server The SSDP server to destroy.
 */
void destroy_ssdp_server(ssdp_server *server);

/**
 * Read from the server. Blocks untill timeout if no data to read.
 * ...
 */
int read_ssdp_server(ssdp_server *server, char *buffer, size_t buffer_len,
    struct sockaddr_storage *client_addr);

#endif /* __SSDP_SERVER_H__ */
