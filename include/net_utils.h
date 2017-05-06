#ifndef __NET_UTILS_H__
#define __NET_UTILS_H__

#include <sys/socket.h>

/**
 * Parse a given ip:port combination into an internet address structure
 *
 * @param char *raw_address The <ip>:<port> string combination to be parsed
 * @param sockaddr_storage ** The IP internet address structute to be allocated and filled
 *
 * @return BOOL TRUE on success
 */
BOOL parse_address(const char *raw_address, struct sockaddr_storage **pp_address);

#endif /* __NET_UTILS_H__ */
