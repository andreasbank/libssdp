#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "common_definitions.h"
#include "configuration.h"
#include "log.h"
#include "net_utils.h"
#include "socket_helpers.h"
#include "ssdp_listener.h"
#include "ssdp_static_defs.h"

#define LISTEN_QUEUE_LENGTH 5 /* The queue length for the listener */

typedef struct ssdp_listener_s {
  SOCKET sock;
} ssdp_listener_s;

static ssdp_listener_s *create_ssdp_listener(configuration_s *conf,
    BOOL is_active, int port, int timeout) {
  SOCKET sock = 0;
  ssdp_listener_s *l = NULL;

  socket_conf_s sock_conf = {
    conf->use_ipv6,       // BOOL is_ipv6
    TRUE,                 // BOOL is_udp
    is_active,            // BOOL is_multicast
    conf->interface,      // char interface
    conf->ip,             // the IP we want to bind to
    NULL,                 // struct sockaddr_storage *sa
    SSDP_ADDR,            // const char *ip
    port,                 // int port
    TRUE,                 // BOOL is_server
    LISTEN_QUEUE_LENGTH,  // the length of the listen queue
    FALSE,                // BOOL keepalive
    conf->ttl,            // time to live (router hops)
    conf->enable_loopback,// see own messages on multicast
    timeout               // set the receive timeout for the socket
  };

  sock = setup_socket(&sock_conf);
  if (sock == SOCKET_ERROR) {
    PRINT_DEBUG("[%d] %s", errno, strerror(errno));
    return NULL;
  }

  /* Set a timeout limit when waiting for ssdp messages */
  if (set_receive_timeout(sock, timeout)) {
    PRINT_ERROR("Failed to set the receive timeout");
    goto err;
  }

  l = malloc(sizeof (ssdp_listener_s));
  l->sock = sock;

  return l;

err:
  close(sock);

  return NULL;
}

ssdp_listener_s *create_ssdp_passive_listener(configuration_s *conf) {
  return create_ssdp_listener(conf, TRUE, SSDP_PORT, 10);
}

ssdp_listener_s *create_ssdp_active_listener(configuration_s *conf, int port) {
  return create_ssdp_listener(conf, FALSE, port, 2);
}

void destroy_ssdp_listener(ssdp_listener_s *listener) {

  if (!listener)
    return;

  if (listener->sock)
    close(listener->sock);

  free(listener);
}

void read_ssdp_listener(ssdp_listener_s *listener,
    ssdp_recv_node_s *recv_node) {
  struct sockaddr_storage recv_addr;
  size_t addr_size = sizeof recv_addr;

  recv_node->recv_bytes = recvfrom(listener->sock, recv_node->recv_data,
      SSDP_RECV_DATA_LEN, 0, (struct sockaddr *)&recv_addr,
      (socklen_t *)&addr_size);

  if (recv_node->recv_bytes > 0) {
    get_ip_from_sock_address(&recv_addr, recv_node->from_ip);
    get_mac_address_from_socket(listener->sock, &recv_addr, NULL,
        recv_node->from_mac);
  }
}

SOCKET get_sock_from_listener(ssdp_listener_s *listener) {
  return listener->sock;
}
