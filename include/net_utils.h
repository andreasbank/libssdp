#ifndef __NET_UTILS_H__
#define __NET_UTILS_H__

#include <sys/socket.h>

#include "common_definitions.h"

/**
 * Parse a string containing an IP address.
 *
 * @param raw_address The string containing the IP address.
 *
 * @return A socket address configured for the given IP (raw_address).
 */
struct sockaddr_storage *parse_address(const char *raw_address);

BOOL parse_url(const char *url, char *ip, int ip_size, int *port, char *rest,
    int rest_size);

/**
* Tries to find a interface with the specified name or IP address
*
* @param saddr A pointer to the buffer of the address that should be updated.
* @param interface The interface name we want to find.
* @param address The address name we want to find.
*
* @return The interface index (int).
*/
int find_interface(struct sockaddr_storage *saddr, const char *interface,
    const char *address);

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

/**
 * Retrieve the IP in printable format from a socket address struct.
 *
 * @param saddr The socket addres struct.
 * @param ip_buffer If set, the resulting IP will be stored in it and no new
 *        allocation will be made. The returned pointer will equal this one.
 *
 * @return The IP address in printable format. If ip_buffer was NULL, the
 *         returned value must be freed.
 */
char *get_ip_from_sock_address(struct sockaddr_storage *saddr,
    char *ip_buffer);

#endif /* __NET_UTILS_H__ */
