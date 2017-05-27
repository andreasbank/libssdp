/** \file ssdp_listener.c
 * Create, manage and delete a SSDP listener.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> /* struct sockaddr_storage */
#include <unistd.h> /* close() */

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
#include "ssdp_common.h"
#include "ssdp_listener.h"
#include "ssdp_message.h"
#include "ssdp_static_defs.h"

/** The queue length for the listener (how many queued connections) */
#define LISTEN_QUEUE_LENGTH 5
/**
 * The timeout after which the passive SSDP listener will stop a
 * connection to a discovered node when fetching ewxtra information.
 */
#define SSDP_PASSIVE_LISTENER_TIMEOUT 10
/**
 * The timeout after which the active SSDP listener will stop waiting for
 * nodes to answer a SEARCH probe/message.
 */
#define SSDP_ACTIVE_LISTENER_TIMEOUT 2

/**
 * Initialize a SSDP listener. This parses and sets the forwarder address,
 * creates a socket and sets all applicable configuration values to it.
 *
 * @param listener The listener to initialize.
 * @param conf The configuration to use.
 * @param active TRUE if an active SSDP listener is to be initialized.
 * @param port The port to listen on. 0 will set the port to the SSDP port.
 * @param recv_timeout The timeout to set for receiveing/waiting for SSDP
 *        nodes to respond to a SEARCH query.
 *
 * @return 0 on success, errno otherwise.
 */
static int ssdp_listener_init(ssdp_listener_s *listener,
    configuration_s *conf, BOOL is_active, int port, int recv_timeout) {
  PRINT_DEBUG("ssdp_listener_init()");
  SOCKET sock = SOCKET_ERROR;

  if (!listener) {
    PRINT_ERROR("No listener specified");
    return 1;
  }

  memset(listener, 0, sizeof *listener);

  /* If set, parse the forwarder address */
  if (conf->forward_address) {
    if (parse_address(conf->forward_address, &listener->forwarder)) {
      PRINT_WARN("Errnoeous forward address");
      return 1;
    }
  }

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
    0,                    // set the rend timeout for the socker (0 = default)
    recv_timeout          // set the receive timeout for the socket
  };

  sock = setup_socket(&sock_conf);
  if (sock == SOCKET_ERROR) {
    PRINT_DEBUG("[%d] %s", errno, strerror(errno));
    return errno;
  }

  listener->sock = sock;
  PRINT_DEBUG("ssdp_listener has been initialized");

  return 0;
}

int ssdp_passive_listener_init(ssdp_listener_s *listener,
    configuration_s *conf) {
  PRINT_DEBUG("ssdp_passive_listener_init()");
  return ssdp_listener_init(listener, conf, TRUE, SSDP_PORT,
      SSDP_PASSIVE_LISTENER_TIMEOUT);
}

int ssdp_active_listener_init(ssdp_listener_s *listener,
    configuration_s *conf, int port) {
  PRINT_DEBUG("ssdp_active_listener_init()");
  return ssdp_listener_init(listener, conf, FALSE, port,
      SSDP_ACTIVE_LISTENER_TIMEOUT);
}

void ssdp_listener_close(ssdp_listener_s *listener) {

  if (!listener)
    return;

  if (listener->sock > 0)
    close(listener->sock);
}

void ssdp_listener_read(ssdp_listener_s *listener,
    ssdp_recv_node_s *recv_node) {
  PRINT_DEBUG("ssdp_listener_read()");
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

int ssdp_listener_start(ssdp_listener_s *listener, configuration_s *conf) {
  PRINT_DEBUG("ssdp_listener_start()");

  /* The structure contining all the filters information */
  filters_factory_s *filters_factory = NULL;

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
        if(conf->forward_address) {
          PRINT_DEBUG("Forwarding cached SSDP messages");
          if(!flush_ssdp_cache(conf, &ssdp_cache, "/abused/post.php",
              &listener->forwarder, 80, 1)) {
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
        if (conf->forward_address) {

          /* If max ssdp cache size reached then it is time to flush */
          if (*ssdp_cache->ssdp_messages_count >= conf->ssdp_cache_size) {
            PRINT_DEBUG("Cache max size reached, sending and emptying");
            if(!flush_ssdp_cache(conf, &ssdp_cache, "/abused/post.php",
                &listener->forwarder, 80, 1)) {
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
  free_ssdp_filters_factory(filters_factory);


  return 0;
}
