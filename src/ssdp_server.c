#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "common_definitions.h"
#include "configuration.h"
#include "log.h"
#include "socket_helpers.h"
#include "ssdp_server.h"
#include "ssdp_static_defs.h"

#define LISTEN_QUEUE_LENGTH 5 /* The queue length for the server */

typedef struct ssdp_server {
  SOCKET sock;
} ssdp_server;

ssdp_server *create_ssdp_server(configuration_s *conf) {
  SOCKET sock = 0;
  ssdp_server *s = NULL;

  sock = setup_socket(
            FALSE,      // BOOL is_ipv6
            TRUE,       // BOOL is_udp
            TRUE,       // BOOL is_multicast
            conf->interface,  // char interface
            conf->ip,
            NULL,       // struct sockaddr_storage *sa
            SSDP_ADDR,  // const char *ip
            SSDP_PORT,  // int port
            TRUE,       // BOOL is_server
            LISTEN_QUEUE_LENGTH,
            FALSE,      // BOOL keepalive
            conf->ttl,  // time to live (router hops)
            conf->enable_loopback
  );
  if (sock == SOCKET_ERROR) {
    PRINT_DEBUG("[%d] %s", errno, strerror(errno));
    return NULL;
  }

  /* Set a timeout limit when waiting for ssdp messages */
  if(set_receive_timeout(sock, 10)) {
    PRINT_ERROR("Failed to set the receive timeout");
    goto err;
  }

  s = malloc(sizeof (ssdp_server));
  s->sock = sock;

  return s;

err:
  close(sock);

  return NULL;
}

void destroy_ssdp_server(ssdp_server *server) {

  if (!server)
    return;

  if (server->sock)
    close(server->sock);

  free(server);
}

int read_ssdp_server(ssdp_server *server, char *buffer, size_t buffer_len,
    struct sockaddr_storage *client_addr) {


  size_t addr_size = sizeof *client_addr;

  return recvfrom(server->sock, buffer, buffer_len, 0, (struct sockaddr *)client_addr, (socklen_t *)&addr_size);
}
