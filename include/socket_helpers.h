/** \file socket_helpers.h
 * Header file for socket_helpers.c.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __SOCKET_HELPERS_H__
#define __SOCKET_HELPERS_H__

#include <sys/socket.h>

#include "common_definitions.h"

/**
 * A socket configuration. Used by setup_socket().
 */
typedef struct socket_conf_s {
  /** Is the socket IPv6 only. */
  BOOL is_ipv6;
  /** Is the socket UDP (SOCK_DGRAM, or TCP/SOCK_STREAM). */
  BOOL is_udp;
  /** Is the socket going to join a multicast group. */
  BOOL is_multicast;
  /** The interface to bind on / send from. */
  char *interface;
  /** The IP address to bind on / send from. */
  char *if_ip;
  /** TODO: remove! */
  struct sockaddr_storage *sa;
  /** The IP address we want send to. */
  const char *ip;
  /** The port to bind to. */
  int port;
  /** Is it a server (will it listen()/accept()). */
  BOOL is_server;
  /** The length of the server queue. */
  int queue_length;
  /** Is it a keep-alive connection. */
  BOOL keepalive;
  /** TTL of sent packets. */
  int ttl;
  /** Listen on loopback (if multicast). */
  BOOL loopback;
  /** Timeout for sending. */
  int send_timeout;
  /** Timeout for the listening. */
  int recv_timeout;
} socket_conf_s;

/**
 * Set the send-timeout for a socket.
 *
 * @param sock The socket to set the timeout for.
 * @param timeout The timeout value to set.
 *
 * @return 0 on success, errno otherwise.
 */
int set_send_timeout(SOCKET sock, int timeout);

/**
 * Set the recv-timeout for a socket.
 *
 * @param sock The socket to set the timeout for.
 * @param timeout The timeout value to set.
 *
 * @return 0 on success, errno otherwise.
 */
int set_receive_timeout(SOCKET sock, int timeout);

/**
 * Set the reuseaddr for a socket. This enables the socket to receive from
 * an address already in use (address and port in linux >= 3.9).
 *
 * @param sock The socket to set the reuseaddr for.
 *
 * @return 0 on success, errno otherwise.
 */
int set_reuseaddr(SOCKET sock);

/**
 * Set the reuseport for a socket. This enable socket to receive from
 * a port already in use (no support in linux < 3.9).
 *
 * @param sock The socket to set the reuseport for.
 *
 * @return 0 on success, errno otherwise.
 */
int set_reuseport(SOCKET sock);

/**
 * Set the keepalive for a socket.
 *
 * @param sock The socket to enable keepalive for.
 * @param keepalive TRUE to enable keepalive for this socket,
 *        FALSE to disable it.
 *
 * @return 0 on success, errno otherwise.
 */
int set_keepalive(SOCKET sock, BOOL keepalive);

/**
 * Set the Time-To-Live (ttl) for a socket.
 *
 * @param sock The socket to set the ttl for.
 * @param ttl The ttl value to set.
 *
 * @return 0 on success, errno otherwise.
 */
int set_ttl(SOCKET sock, int family, int ttl);

/**
 * Disable multicast loopback traffic for a socket.
 *
 * @param sock The socket to disable it for.
 * @param family Socket IP type (AF_INET or AF_INET6).
 *
 * @return 0 on success, errno otherwise.
 */
int disable_multicast_loopback(SOCKET sock, int family);

/**
 * Join a multicast group.
 *
 * @param multicast_group The group to join.
 * @param interface_ip The interface to join on.
 *
 * @return 0 on success, errno otherwise.
 */
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
