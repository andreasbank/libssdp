#ifndef __SOCKET_HELPERS_H__
#define __SOCKET_HELPERS_H__

#include "common_definitions.h"

SOCKET create_upnp_listener(char *interface, int send_timeout, int receive_timeout);
BOOL set_send_timeout(SOCKET sock, int timeout);
BOOL set_receive_timeout(SOCKET sock, int timeout);
BOOL set_reuseaddr(SOCKET sock);
BOOL set_keepalive(SOCKET sock, BOOL keepalive);
BOOL set_ttl(SOCKET sock, int family, int ttl);
BOOL disable_multicast_loopback(SOCKET sock, int family);
BOOL join_multicast_group(SOCKET sock, char *multicast_group, char *interface_ip);
SOCKET setup_socket(BOOL is_ipv6, BOOL is_udp, BOOL is_multicast,
    char *interface, char *if_ip, struct sockaddr_storage *sa, const char *ip,
    int port, BOOL is_server, int queue_length, BOOL keepalive, int ttl,
    BOOL loopback);

#endif /* __SOCKET_HELPERS_H__ */
