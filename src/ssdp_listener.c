#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "common_definitions.h"
#include "configuration.h"
#include "log.h"
#include "net_definitions.h"
#include "net_utils.h"
#include "socket_helpers.h"
#include "ssdp_filter.h"
#include "ssdp_cache.h"
#include "ssdp_cache_display.h"
#include "ssdp_cache_output_format.h"
#include "ssdp_listener.h"
#include "ssdp_message.h"
#include "ssdp_static_defs.h"

#define LISTEN_QUEUE_LENGTH 5 /* The queue length for the listener */

typedef struct ssdp_listener_s {
  SOCKET sock;
  struct sockaddr_storage forwarder;
} ssdp_listener_s;

/**
 * //TOTO: write desc
 */
static int ssdp_listener_init(ssdp_listener_s *listener,
    configuration_s *conf, BOOL is_active, int port, int timeout) {
  SOCKET sock = 0;

  if (!listener) {
    PRINT_ERROR("No listener specified");
    return 1;
  }

  memset (listener, 0 sizeof *listener);

  // TODO: make parse_address take in stack alloc
  struct sockaddr_storage *tmp_saddr = parse_address(conf->forward_address);
  if (!tmp_saddr) {
    PRINT_WARN("Errnoeous forward address");
    return 1;
  }
  listener->forwarder = *tmp_saddr;

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
    return errno;
  }

  /* Set a timeout limit when waiting for ssdp messages */
  if (set_receive_timeout(sock, timeout)) {
    PRINT_DEBUG("Failed to set the receive timeout");
    goto err;
  }

  l = malloc(sizeof (ssdp_listener_s));
  l->sock = sock;

  return 0;

err:
  close(sock);

  return errno;
}

int ssdp_passive_listener_init(ssdp_listener *listener, configuration_s *conf) {
  return ssdp_listener_init(listener, conf, TRUE, SSDP_PORT, 10);
}

int ssdp_active_listener_init(ssdp_listener *listener,
    configuration_s *conf, int port) {
  return ssdp_listener_init(listener, conf, FALSE, port, 2);
}

void ssdp_listener_close(ssdp_listener_s *listener) {

  if (!listener)
    return;

  if (listener->sock)
    close(listener->sock);
}

void ssdp_listener_read(ssdp_listener_s *listener,
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

int ssdp_listener_start(ssdp_listener_s *listener, configuration_s *conf) {
  PRINT_DEBUG("listen_for_upnp_notif start");

  /* Parse the filters */
  PRINT_DEBUG("parse_filters()");
  parse_filters(conf->filter, &filters_factory, TRUE & (~conf->quiet_mode));

  /* Child process server loop */
  PRINT_DEBUG("Strating infinite loop");
  ssdp_message_s *ssdp_message;
  BOOL drop_message;

  /* Create a list for keeping/caching SSDP messages */
  ssdp_cache_s *ssdp_cache = NULL;

  while (TRUE) {
    ssdp_recv_node_s recv_node;
    drop_message = FALSE;
    ssdp_message = NULL;

    PRINT_DEBUG("loop: ready to receive");
    ssdp_listener_read(listener, &recv_node);

    #ifdef __DEBUG
    if (recv_node.recv_bytes > 0) {
      PRINT_DEBUG("**** RECEIVED %d bytes ****\n%s", recv_node.recv_bytes,
          notif_string);
      PRINT_DEBUG("************************");
    }
    #endif

    /* If timeout reached then go through the
      ssdp_cache list and see if anything needs
      to be sent */
    if (recv_node.recv_bytes < 1) {
      PRINT_DEBUG("Timed-out waiting for a SSDP message");
      if(!ssdp_cache || *ssdp_cache->ssdp_messages_count == 0) {
        PRINT_DEBUG("No messages in the SSDP cache, continuing to listen");
      }
      else {
        /* If forwarding has been enabled, send the cached
           SSDP messages and empty the list*/
        if(conf->forward_enabled) {
          PRINT_DEBUG("Forwarding cached SSDP messages");
          if(!flush_ssdp_cache(conf, &ssdp_cache, "/abused/post.php",
              listener->notif_recipient_addr, 80, 1)) {
            PRINT_DEBUG("Failed flushing SSDP cache");
            continue;
          }
        }
        /* Else just display the cached messages in a table */
        else {
          PRINT_DEBUG("Displaying cached SSDP messages");
          display_ssdp_cache(ssdp_cache, FALSE);
        }
      }
      continue;
    }
    /* Else a new ssdp message has been received */
    else {

      /* init ssdp_message */
      if (!init_ssdp_message(&ssdp_message)) {
        PRINT_ERROR("Failed to initialize the SSDP message buffer");
        continue;
      }

      /* Build the ssdp message struct */
      if (!build_ssdp_message(ssdp_message, recv_node.from_ip,
          recv_node.from_mac, recv_node.recv_bytes, recv_node.recv_data)) {
        PRINT_ERROR("Failed to build the SSDP message");
        free_ssdp_message(&ssdp_message);
        continue;
      }

      // TODO: Make it recognize both AND and OR (search for ; inside a ,)!!!

      /* If -M is not set check if it is a M-SEARCH message
         and drop it */
      if (conf->ignore_search_msgs && (strstr(ssdp_message->request,
          "M-SEARCH") != NULL)) {
          PRINT_DEBUG("Message contains a M-SEARCH request, dropping "
              "message");
          free_ssdp_message(&ssdp_message);
          continue;
      }

      /* Check if notification should be used (if any filters have been set) */
      if (filters_factory != NULL) {
        drop_message = filter(ssdp_message, filters_factory);
      }

      /* If message is not filtered then use it */
      if (filters_factory == NULL || !drop_message) {

        /* Add ssdp_message to ssdp_cache
           (this internally checks for duplicates) */
        if (!add_ssdp_message_to_cache(&ssdp_cache, &ssdp_message)) {
          PRINT_ERROR("Failed adding SSDP message to SSDP cache, skipping");
          continue;
        }

        /* Fetch custom fields */
        if (conf->fetch_info && !fetch_custom_fields(conf, ssdp_message)) {
          PRINT_DEBUG("Could not fetch custom fields");
        }
        ssdp_message = NULL;

        /* Check if forwarding ('-a') is enabled */
        if (conf->forward_enabled) {

          /* If max ssdp cache size reached then it is time to flush */
          if (*ssdp_cache->ssdp_messages_count >= conf->ssdp_cache_size) {
            PRINT_DEBUG("Cache max size reached, sending and emptying");
            if(!flush_ssdp_cache(conf, &ssdp_cache, "/abused/post.php",
                listener->notif_recipient_addr, 80, 1)) {
              PRINT_DEBUG("Failed flushing SSDP cache");
              continue;
            }
          }
          else {
            PRINT_DEBUG("Cache max size not reached, not sending yet");
          }
        }
        else {
          /* Display results on console */
          PRINT_DEBUG("Displaying cached SSDP messages");
          display_ssdp_cache(ssdp_cache, FALSE);
        }
      }
    }

    PRINT_DEBUG("scan loop: done");
  }

  return 0;
}
