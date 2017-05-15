#ifndef __SOCKET_HELPERS_H__
#define __SOCKET_HELPERS_H__

#include <sys/socket.h>

#include "common_definitions.h"

/**
 * A socket configuration. Used by setup_socket().
 */
typedef struct socket_conf_s {
  BOOL is_ipv6; /* Is the socket IPv6 only */
  BOOL is_udp; /* Is the socket UDP (SOCK_DGRAM, or TCP/SOCK_STREAM) */
  BOOL is_multicast; /* Is the socket going to join a multicast group */
  char *interface; /* The interface to bind on / send from */
  char *if_ip; /* The IP address to bind on / send from */
  struct sockaddr_storage *sa; // TODO: remove!
  const char *ip; /* The actual IP it got bound to (after bind()) */
  int port; /* The port to bind to */
  BOOL is_server; /* Is it a server (will it listen()/accept()) */
  int queue_length; /* The length of the server queue */
  BOOL keepalive; /* Is it a keep-alive connection */
  int ttl; /* TTL of sent packets */
  BOOL loopback; /* Listen on loopback (if multicast) */
} socket_conf_s;

int set_send_timeout(SOCKET sock, int timeout);

int set_receive_timeout(SOCKET sock, int timeout);

int set_reuseaddr(SOCKET sock);

int set_keepalive(SOCKET sock, BOOL keepalive);

int set_ttl(SOCKET sock, int family, int ttl);

int disable_multicast_loopback(SOCKET sock, int family);

int join_multicast_group(SOCKET sock, char *multicast_group,
    char *interface_ip);

/**
 * Create and configure a new socket.
 *
 * @param conf The socket configuration to use when creating and configuring
 *        the socket.
 *
 * @return A new socket.
 */
SOCKET setup_socket(socket_conf_s *conf);

#endif /* __SOCKET_HELPERS_H__ */
