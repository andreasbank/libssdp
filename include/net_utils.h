#ifndef __NET_UTILS_H__
#define __NET_UTILS_H__

#include <sys/socket.h>

#include "common_definitions.h"

/**
 * Parse a given ip:port combination into an internet address structure
 *
 * @param char *raw_address The <ip>:<port> string combination to be parsed
 * @param sockaddr_storage ** The IP internet address structute to be allocated and filled
 *
 * @return BOOL TRUE on success
 */
BOOL parse_address(const char *raw_address, struct sockaddr_storage **pp_address);

BOOL parse_url(const char *url, char *ip, int ip_size, int *port, char *rest,
    int rest_size);

/**
* Tries to find a interface with the specified name or IP address
*
* @param struct sockaddr_storage *saddr A pointer to the buffer of the address that should be updated
* @param char * address The address or interface name we want to find
*
* @return int The interface index
*/
int find_interface(struct sockaddr_storage *, char *, char *);

/**
 * Get the remote MAC address from a given sock.
 *
 * @param sock The socket to extract the MAC address from.
 *
 * @return The remote MAC address as a string.
 */
char *get_mac_address_from_socket(const SOCKET sock,
    struct sockaddr_storage *sa_ip, char *ip);

/**
 * Check whether a given IP is a multicast IP
 * (if IPV6 checks for UPnP multicast IP, should be
 * changed to check if multicast at all).
 *
 * @param address The address to check.
 *
 * @return TRUE on success, FALSE otherwise.
 */
BOOL is_address_multicast(const char *address);

#endif /* __NET_UTILS_H__ */
