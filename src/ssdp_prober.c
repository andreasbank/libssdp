/** \file ssdp_prober.c
 * Create, manage and delete a SSDP prober.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#include <arpa/inet.h> /* inet_pton() */
#include <errno.h>
#include <netdb.h> /* struct addrinfo */
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h> /* memset() */
#include <sys/socket.h> /* struct sockaddr_storage */
#include <unistd.h> /* close() */

#include "common_definitions.h"
#include "configuration.h"
#include "log.h"
#include "net_definitions.h"
#include "net_utils.h"
#include "socket_helpers.h"
#include "ssdp_cache_output_format.h"
#include "ssdp_common.h"
#include "ssdp_filter.h"
#include "ssdp_listener.h"
#include "ssdp_message.h"
#include "ssdp_prober.h"
#include "ssdp_static_defs.h"

/** A default SSDP probe (SEARCH) message. */
#define PROBE_MSG \
  "M-SEARCH * HTTP/1.1\r\n" \
  "Host:239.255.255.250:1900\r\n" \
  "ST:urn:axis-com:service:BasicService:1\r\n" \
  "Man:\"ssdp:discover\"\r\n" \
  "MX:0\r\n\r\n"

const char *ssdp_probe_message_create(void) {
  return PROBE_MSG;
}

int ssdp_prober_init(ssdp_prober_s *prober, configuration_s *conf) {
  PRINT_DEBUG("ssdp_prober_init()");
  if (!prober) {
    PRINT_ERROR("Prober is empty");
  }

  memset(prober, 0, sizeof *prober);

  if (conf->forward_address) {
    if (parse_address(conf->forward_address, &prober->forwarder)) {
      PRINT_WARN("Errnoeous forward address");
      return 1;
    }
  }

  /* init sending socket */
  socket_conf_s sock_conf = {
    conf->use_ipv6,  // BOOL is_ipv6
    TRUE,           // BOOL is_udp
    TRUE,           // BOOL is_multicast
    conf->interface, // char *interface
    conf->ip,        // char *IP
    NULL,           // struct sockaddr_storage *sa
    (conf->use_ipv6 ? SSDP_ADDR6_LL : SSDP_ADDR),  // const char *ip
    SSDP_PORT,      // int port
    FALSE,          // BOOL is_server
    1,              // int queue_len
    FALSE,          // BOOL keepalive
    conf->ttl,       // int ttl
    conf->enable_loopback, // BOOL loopback
    0,              // Receive timeout
    0               // Send timeout
  };

  if ((prober->sock = setup_socket(&sock_conf)) == SOCKET_ERROR) {
    PRINT_DEBUG("Could not create socket");
    return errno;
  }
  PRINT_DEBUG("ssdp_prober has been initialized");

  return 0;
}

void ssdp_prober_close(ssdp_prober_s *prober) {
  PRINT_DEBUG("ssdp_prober_close()");
  if (!prober)
    return;

  if (prober->sock > 0)
    close(prober->sock);
}

int ssdp_prober_start(ssdp_prober_s *prober, configuration_s *conf) {
  PRINT_DEBUG("ssdp_prober_start()");

  /* The structure contining all the filters information */
  filters_factory_s *filters_factory = NULL;

  /* Parse the filters */
  PRINT_DEBUG("parse_filters()");
  parse_filters(conf->filter, &filters_factory, TRUE & (~conf->quiet_mode));

  /* Create a SSDP probe message */
  const char *request = ssdp_probe_message_create();

  BOOL drop_message;

  /* Client (mcast group) address */
  PRINT_DEBUG("creating multicast address");
  //struct addrinfo *addri;
  //struct addrinfo hint;
  //memset(&hint, 0, sizeof(hint));
  //hint.ai_family = (conf->use_ipv6 ? AF_INET6 : AF_INET);
  //hint.ai_socktype = SOCK_DGRAM;
  //hint.ai_protocol = IPPROTO_UDP;
  //hint.ai_canonname = NULL;
  //hint.ai_addr = NULL;
  //hint.ai_next = NULL;
  //char *port_str = (char *)malloc(sizeof(char) * 6);
  //memset(port_str, '\0', 6);
  //sprintf(port_str, "%d", SSDP_PORT);

  //int err = 0;

  //if ((err = getaddrinfo((conf->use_ipv6 ? SSDP_ADDR6_LL : SSDP_ADDR),
  //    port_str, &hint, &addri)) != 0) {
  //  PRINT_ERROR("getaddrinfo(): %s\n", gai_strerror(err));
  //  free(port_str);
  //  return err;
  //}
  //free(port_str);

  /*****************
   This is the temp solution until the
   commented-out sendto() code below (and above) is fixed */
  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(SSDP_PORT);
  inet_pton(AF_INET, SSDP_ADDR, &sa.sin_addr);
  /*****************/

  /* Send the UPnP request */
  PRINT_DEBUG("sending request");
  int sent_bytes;
  //sent_bytes = sendto(prober->sock, request, strlen(request), 0,
  //    (struct sockaddr *)&addri->ai_addr, addri->ai_addrlen);
  sent_bytes = sendto(prober->sock, request, strlen(request), 0,
      (struct sockaddr *)&sa, sizeof(sa));

  if (sent_bytes < 0) {
    PRINT_DEBUG("sendto(): Failed sending any data");
    return errno;
  }


  size_t sento_addr_len = sizeof(struct sockaddr_storage);
  struct sockaddr_storage *sendto_addr = malloc(sento_addr_len);
  memset(sendto_addr, 0, sento_addr_len);
  int response_port = 0;

  if (getsockname(prober->sock, (struct sockaddr *)sendto_addr,
      (socklen_t *)&sento_addr_len) < 0) {
    PRINT_ERROR("Could not get sendto() port, going to miss the replies");
  }
  else {
    response_port = get_port_from_sock_address(sendto_addr);
    PRINT_DEBUG("sendto() port is %d", response_port);
  }
  free(sendto_addr);
  close(prober->sock);
  PRINT_DEBUG("sent %d bytes", sent_bytes);
  //freeaddrinfo(addri);
  drop_message = FALSE;

  /* init listening socket */
  PRINT_DEBUG("setup_socket() listening");

  // TODO: fix this listener, its acting weird
  ssdp_listener_s response_listener;
  ssdp_active_listener_init(&response_listener, conf, response_port);

  ssdp_message_s *ssdp_message;
  ssdp_recv_node_s recv_node;

  do {
    memset(&recv_node, 0, sizeof recv_node);

    /* Wait for an answer */
    PRINT_DEBUG("Waiting for a response");
    ssdp_listener_read(&response_listener, &recv_node);
    PRINT_DEBUG("Recived %d bytes%s",
        (recv_node.recv_bytes < 0 ? 0 : recv_node.recv_bytes),
        (recv_node.recv_bytes < 0 ? " (wait time limit reached)" : ""));

    if(recv_node.recv_bytes < 0) {
      continue;
    }

    ssdp_message = NULL;

    /* Initialize and build ssdp_message */
    if(!init_ssdp_message(&ssdp_message)) {
      PRINT_ERROR("Failed to initialize SSDP message holder structure");
      continue;
    }

    if (!build_ssdp_message(ssdp_message, recv_node.from_ip,
        recv_node.from_mac, recv_node.recv_bytes, recv_node.recv_data)) {
      continue;
    }

    // TODO: Make it recognize both AND and OR (search for ; inside a ,)!!!
    // TODO: add "request" string filtering
    /* Check if notification should be used (to print and possibly send to the given destination) */
    ssdp_header_s *ssdp_headers = ssdp_message->headers;
    if (filters_factory != NULL) {
      int fc;
      for (fc = 0; fc < filters_factory->filters_count; fc++) {
        if ((strcmp(filters_factory->filters[fc].header, "ip") == 0) &&
            strstr(ssdp_message->ip,
            filters_factory->filters[fc].value) == NULL) {
          drop_message = TRUE;
          break;
        }

        while (ssdp_headers) {
          if ((strcmp(get_header_string(ssdp_headers->type, ssdp_headers),
              filters_factory->filters[fc].header) == 0) &&
              strstr(ssdp_headers->contents,
              filters_factory->filters[fc].value) == NULL) {
            drop_message = TRUE;
            break;
          }
          ssdp_headers = ssdp_headers->next;
        }
        ssdp_headers = ssdp_message->headers;

        if (drop_message) {
          break;;
        }
      }
    }

    if (filters_factory == NULL || !drop_message) {

      /* Fetch custom fields */
      if (conf->fetch_info && !fetch_custom_fields(conf, ssdp_message)) {
        PRINT_DEBUG("Could not fetch custom fields");
      }

      /* Print the message */
      if (conf->xml_output) {
        char *xml_string = (char *)malloc(sizeof(char) * XML_BUFFER_SIZE);
        to_xml(ssdp_message, TRUE, xml_string, XML_BUFFER_SIZE);
        printf("%s\n", xml_string);
        free(xml_string);
      } else if (conf->oneline_output) {
        char *oneline_string = to_oneline(ssdp_message, conf->monochrome);
        if (oneline_string) {
          printf("%s\n", oneline_string);
          free(oneline_string);
        }
      } else {
        printf("\n\n\n----------BEGIN NOTIFICATION------------\n");
        printf("Time received: %s\n", ssdp_message->datetime);
        printf("Origin-MAC: %s\n", (ssdp_message->mac != NULL ?
            ssdp_message->mac : "(Could not be determined)"));
        printf("Origin-IP: %s\nMessage length: %d Bytes\n", ssdp_message->ip,
            ssdp_message->message_length);
        printf("Request: %s\nProtocol: %s\n", ssdp_message->request,
            ssdp_message->protocol);

        int hc = 0;
        while (ssdp_headers) {
          printf("Header[%d][type:%d;%s]: %s\n", hc, ssdp_headers->type,
              get_header_string(ssdp_headers->type, ssdp_headers),
              ssdp_headers->contents);
          ssdp_headers = ssdp_headers->next;
          hc++;
        }
        ssdp_headers = NULL;
        printf("-----------END NOTIFICATION-------------\n");
      }
      /* TODO: Send the message back to -a (or not support that for probing?)*/

    }

    free_ssdp_message(&ssdp_message);
  } while(recv_node.recv_bytes > 0);

  //TODO: ssdp_listener_close(&response_listener) ?
  free_ssdp_filters_factory(filters_factory);

  PRINT_DEBUG("scan_for_upnp_devices end");

  return 0;
}

