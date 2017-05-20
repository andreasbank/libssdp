#ifndef __NET_UTILS_H__
#define __NET_UTILS_H__

#include <sys/socket.h>
#include <netinet/in.h>

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
 * @param sa_ip The socket address to extract for.
 * @param ip The IP address to extract for. If sa_ip is set this will be
 *        ignored.
 *
 * @return The remote MAC address as a string.
 */
char *get_mac_address_from_socket(const SOCKET sock,
    struct sockaddr_storage *sa_ip, char *ip, char *mac_buffer);

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

/**
 * Check whether an IP address is of version 6.
 *
 * @param The IP address to check.
 *
 * @return TRUE if it is an IPv6 address, FALSE otherwise.
 */
inline BOOL is_address_ipv6(const char *ip);

/**
 * Check whether an IP address is of version 6. This function will also fill
 * the given socket address structure with the network form of the IP address.
 *
 * @param The IP address to check.
 *
 * @return TRUE if it is an IPv6 address, FALSE otherwise.
 */
BOOL is_address_ipv6_ex(const char *ip, struct sockaddr_in6 *saddr6);

/**
 * Check whether an IP address is of version 4.
 *
 * @param The IP address to check.
 *
 * @return TRUE if it is an IPv4 address, FALSE otherwise.
 */
inline BOOL is_address_ipv4(const char *ip);

/**
 * Check whether an IP address is of version 4. This function will also fill
 * the given socket address structure with the network form of the IP address.
 *
 * @param The IP address to check.
 *
 * @return TRUE if it is an IPv4 address, FALSE otherwise.
 */
BOOL is_address_ipv4_ex(const char *ip, struct sockaddr_in *saddr);

/**
 * Set the network form of the given IP address in the given socket address
 * buffer.
 *
 * @param ip The IP to set.
 * @param port The port to set.
 * @param saddr The socket address buffer to set the IP in.
 *
 * @return TRUE on success, FALSE otherwise.
 */
BOOL set_ip_and_port_in_sock_address(const char *ip, int port,
    struct sockaddr_storage *saddr);

/**
 * Retrieve the port from a socket address.
 *
 * @param The socket address to get the port from.
 *
 * @return The port number.
 */
int get_port_from_sock_address(const struct sockaddr_storage *saddr);

#endif /* __NET_UTILS_H__ */
