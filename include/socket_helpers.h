#ifndef __SOCKET_HELPERS_H__
#define __SOCKET_HELPERS_H__

#include <sys/socket.h>

#include "common_definitions.h"

int set_send_timeout(SOCKET sock, int timeout);

int set_receive_timeout(SOCKET sock, int timeout);

int set_reuseaddr(SOCKET sock);

int set_keepalive(SOCKET sock, BOOL keepalive);

int set_ttl(SOCKET sock, int family, int ttl);

int disable_multicast_loopback(SOCKET sock, int family);

int join_multicast_group(SOCKET sock, char *multicast_group,
    char *interface_ip);

SOCKET setup_socket(BOOL is_ipv6, BOOL is_udp, BOOL is_multicast,
    char *interface, char *if_ip, struct sockaddr_storage *sa,
    const char *ip, int port, BOOL is_server, int queue_length, BOOL keepalive,
    int ttl, BOOL loopback);

#endif /* __SOCKET_HELPERS_H__ */
