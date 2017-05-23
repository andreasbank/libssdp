#ifndef __SSDP_COMMON_H__
#define __SSDP_COMMON_H__

#include <sys/socket.h>

#include "configuration.h"

/**
 * Print forwarding status.
 *
 * @param conf The global configuration to use.
 * @param forwarder The address of the forward recipient.
 */
void print_forwarder(configuration_s *conf,
    struct sockaddr_storage *forwarder);

#endif /* __SSDP_COMMON_H__ */
