#include <sys/socket.h> /* struct sockaddr_storage */
#include <string.h> /* memset() */

#include "configuration.h"
#include "net_utils.h"

void print_forwarder(configuration_s *conf,
    struct sockaddr_storage *forwarder) {
  if(!conf->quiet_mode && forwarder) {
    char ip[IPv6_STR_MAX_SIZE];

    memset(ip, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);

    get_ip_from_sock_address(forwarder, ip);

    printf("Forwarding is enabled, ");
    printf("forwarding to IP %s on port %d\n", ip,
        get_port_from_sock_address(forwarder));
  }
}

