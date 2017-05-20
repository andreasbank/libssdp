/**
 *  ______  ____     __  __  ____    ____    ____
 * /\  _  \/\  _`\  /\ \/\ \/\  _`\ /\  _`\ /\  _`\
 * \ \ \L\ \ \ \L\ \\ \ \ \ \ \,\L\_\ \ \L\_\ \ \/\ \
 *  \ \  __ \ \  _ <'\ \ \ \ \/_\__ \\ \  _\L\ \ \ \ \
 *   \ \ \/\ \ \ \L\ \\ \ \_\ \/\ \L\ \ \ \L\ \ \ \_\ \
 *    \ \_\ \_\ \____/ \ \_____\ `\____\ \____/\ \____/
 *     \/_/\/_/\/___/   \/_____/\/_____/\/___/  \/___/
 *
 *
 *  [A]bused is [B]anks [U]ber-[S]exy-[E]dition [D]aemon
 *
 *  Copyright(C) 2014-2017 by Andreas Bank, andreas.mikael.bank@gmail.com
 */

/*
 *  ████████╗ ██████╗ ██████╗  ██████╗
 *  ╚══██╔══╝██╔═══██╗██╔══██╗██╔═══██╗██╗
 *     ██║   ██║   ██║██║  ██║██║   ██║╚═╝
 *     ██║   ██║   ██║██║  ██║██║   ██║██╗
 *     ██║   ╚██████╔╝██████╔╝╚██████╔╝╚═╝
 *     ╚═╝    ╚═════╝ ╚═════╝  ╚═════╝
 *  01010100 01001111 01000100 01001111 00111010
 *
 *  - Write JSON converter
 *  - Fix IPv6!
 */

/* Uncomment the line below to enable simulating notifs */
//#define DEBUG_MSG___

/* Set the frequency at which the messages will be received */
#define DEBUG_MSG_FREQ___ 0.5

/* When using simulated messages it is mandatory to
   specify a UPnP device url in DEBUG_MSG_DEVICE___.
   This "UPnP device" can be emulated with a http server (e.g. Apache)
   serving the UPnP-descriptive XML file included in the
   abused repository (<repo>/udhisapi.xml) */
#define DEBUG_MSG_LOCATION_HEADER "http://127.0.0.1:80/udhisapi.xml"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <arpa/inet.h>

#include <ctype.h>

#include <errno.h>
#include <signal.h>

#include "configuration.h"
#include "common_definitions.h"
#include "daemon.h"
#include "ssdp_filter.h"
#include "log.h"
#include "net_definitions.h"
#include "net_utils.h"
#include "ssdp_cache_output_format.h"
#include "socket_helpers.h"
#include "ssdp_cache.h"
#include "ssdp_cache_display.h"
#include "ssdp_message.h"
#include "ssdp_listener.h"
#include "ssdp_static_defs.h"
#include "string_utils.h"

#define SSDP_RECV_BUFFER_LEN 2048 /* 2KiB */

/* The main socket for listening for UPnP (SSDP) devices (or device answers) */
static SOCKET notif_server_sock = 0;
static ssdp_listener_s *ssdp_listener = NULL;

/* The sock that we want to ask/look for devices on (unicast) */
static SOCKET notif_client_sock = 0;

/* Notification recipient socket, where the results will be forwarded */
static SOCKET notif_recipient_sock = 0;

/* The sockaddr where we store the recipient address */
struct sockaddr_storage *notif_recipient_addr = NULL;

/* The structure contining all the filters information */
static filters_factory_s *filters_factory = NULL;

/* The program configuration */
static configuration_s conf;




/**
 * Frees all global allocations.
 */
static void cleanup() {

  if (notif_server_sock != 0) {
    close(notif_server_sock);
    notif_server_sock = 0;
  }

  if (ssdp_listener) {
    destroy_ssdp_listener(ssdp_listener);
  }

  if (notif_client_sock != 0) {
    close(notif_client_sock);
    notif_client_sock = 0;
  }

  if (notif_recipient_sock != 0) {
    close(notif_recipient_sock);
    notif_recipient_sock = 0;
  }

  free_ssdp_filters_factory(filters_factory);

  PRINT_DEBUG("\nCleaning up and exitting...\n");
}

/**
 * The exit callback function called on any exit signal.
 *
 * @param param The signal handler parameter (ignored).
 */
static void exit_sig(int param) {
  cleanup();
  exit(EXIT_SUCCESS);
}

/**
 * Decides what the program will run as and does the neccessary
 * deamonizing and forking.
 *
 * @param conf The global configuration to use.
 */
static void verify_running_states(configuration_s *conf) {

  if(conf->run_as_daemon &&
     !(conf->run_as_server ||
       (conf->listen_for_upnp_notif && conf->forward_enabled) ||
       conf->scan_for_upnp_devices)) {
    /* If missconfigured, stop and notify the user */

    PRINT_ERROR("Cannot start as daemon.\nUse -d in combination with "
        "-S or -a.\n");
    exit(EXIT_FAILURE);

  }
  else if(conf->run_as_daemon) {
    /* If run as a daemon */
    daemonize();
  }

  /* If set to listen for UPnP notifications then
     fork() and live a separate life */
  if (conf->listen_for_upnp_notif &&
     ((conf->run_as_daemon && conf->forward_enabled) ||
     conf->run_as_server)) {
    if (fork() != 0) {
      /* listen_for_upnp_notif went to the forked process,
         so it is set to false in parent so it doesn't run twice'*/
      conf->listen_for_upnp_notif = FALSE;
    }
    else {
      PRINT_DEBUG("Created a process for listening of UPnP notifications");
      /* scan_for_upnp_devices is set to false
         so it doesn't run in this child (if it will be run at all) */
      conf->scan_for_upnp_devices = FALSE;
    }
  }

  /* If set to scan for UPnP-enabled devices then
     fork() and live a separate life */
  if(conf->scan_for_upnp_devices &&
      (conf->run_as_daemon || conf->run_as_server)) {
    if (fork() != 0) {
      /* scan_for_upnp_devices went to the forked process,
         so it set to false in parent so it doesn't run twice */
      conf->scan_for_upnp_devices = FALSE;
    }
    else {
      PRINT_DEBUG("Created a process for scanning UPnP devices");
      /* listen_for_upnp_notif is set to false
         so it doesn't run in this child (if it will be run at all) */
      conf->listen_for_upnp_notif = FALSE;
    }
  }
}

/**
 * Print forwarding status.
 *
 * @param conf The global configuration to use.
 * @param sa The address of the forward recipient.
 */
static void print_forwarding_config(configuration_s *conf,
    struct sockaddr_storage *sa) {
  if(!conf->quiet_mode && notif_recipient_addr) {
    char ip[IPv6_STR_MAX_SIZE];
    memset(ip, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);
    inet_ntop(notif_recipient_addr->ss_family,
        (notif_recipient_addr->ss_family == AF_INET ?
        (void *)&(((struct sockaddr_in *)notif_recipient_addr)->sin_addr) :
        (void *)&(((struct sockaddr_in6 *)notif_recipient_addr)->sin6_addr)),
        ip, sizeof(char) * IPv6_STR_MAX_SIZE);
    printf("Forwarding is enabled, ");
    printf("forwarding to IP %s on port %d\n", ip,
        ntohs((notif_recipient_addr->ss_family == AF_INET ?
        ((struct sockaddr_in *)notif_recipient_addr)->sin_port :
        ((struct sockaddr_in6 *)notif_recipient_addr)->sin6_port)));
  }
}

int main(int argc, char **argv) {
  struct sockaddr_storage ssdp_client_addr;
  int recvLen = 1;

  signal(SIGTERM, &exit_sig);
  signal(SIGABRT, &exit_sig);
  signal(SIGINT, &exit_sig);

  #ifdef DEBUG___
  log_start_args(argc, argv);
  PRINT_DEBUG("%sDeveloper color%s", ANSI_COLOR_PURPLE, ANSI_COLOR_RESET);
  PRINT_DEBUG("%sDebug color%s", ANSI_COLOR_GRAY, ANSI_COLOR_RESET);
  PRINT_DEBUG("%sWarning color%s", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET);
  PRINT_DEBUG("%sError color%s", ANSI_COLOR_RED, ANSI_COLOR_RESET);
  #endif

  set_default_configuration(&conf);
  if (parse_args(argc, argv, &conf)) {
    cleanup();
    exit(EXIT_FAILURE);
  }

  print_forwarding_config(&conf, notif_recipient_addr);

  verify_running_states(&conf);

  if(conf.listen_for_upnp_notif) {
    /* If set to listen for AXIS devices notifications then
       start listening for notifications but never continue
       to do any other work the parent should be doing */
    PRINT_DEBUG("listen_for_upnp_notif start");

    /* Init buffer for receiving messages */
    //char notif_string[NOTIF_RECV_BUFFER];
    char ssdp_recv_buffer[SSDP_RECV_BUFFER_LEN];

    /* init socket */
    PRINT_DEBUG("setup_socket for listening to upnp");
    ssdp_listener = create_ssdp_listener(&conf);
    if (!ssdp_listener) {
      PRINT_ERROR("Could not create socket");
      cleanup();
      exit(errno);
    }

    /* Parse the filters */
    PRINT_DEBUG("parse_filters()");
    parse_filters(conf.filter, &filters_factory, TRUE & (~conf.quiet_mode));

    /* Child process server loop */
    PRINT_DEBUG("Strating infinite loop");
    ssdp_message_s *ssdp_message;
    BOOL drop_message;

    /* Create a list for keeping/caching SSDP messages */
    ssdp_cache_s *ssdp_cache = NULL;

    while(TRUE) {
      drop_message = FALSE;
      ssdp_message = NULL;

      memset(ssdp_recv_buffer, '\0', SSDP_RECV_BUFFER_LEN);
      PRINT_DEBUG("loop: ready to receive");

      recvLen = read_ssdp_listener(ssdp_listener, ssdp_recv_buffer,
          SSDP_RECV_BUFFER_LEN, &ssdp_client_addr);

      #ifdef __DEBUG
      if(recvLen > 0) {
        PRINT_DEBUG("**** RECEIVED %d bytes ****\n%s", recvLen, notif_string);
        PRINT_DEBUG("************************");
      }
      #endif

      /* If timeout reached then go through the 
        ssdp_cache list and see if anything needs
        to be sent */
      if(recvLen < 1) {
        PRINT_DEBUG("Timed-out waiting for a SSDP message");
        if(!ssdp_cache || *ssdp_cache->ssdp_messages_count == 0) {
          PRINT_DEBUG("No messages in the SSDP cache, continuing to listen");
        }
        else {
          /* If forwarding has been enabled, send the cached
             SSDP messages and empty the list*/
          if(conf.forward_enabled) {
            PRINT_DEBUG("Forwarding cached SSDP messages");
            if(!flush_ssdp_cache(&conf, &ssdp_cache, "/abused/post.php",
                notif_recipient_addr, 80, 1)) {
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
        if(!init_ssdp_message(&ssdp_message)) {
          PRINT_ERROR("Failed to initialize the SSDP message buffer");
          continue;
        }

        /* Retrieve IP address from socket */
        char tmp_ip[IPv6_STR_MAX_SIZE];
        if (!get_ip_from_sock_address(&ssdp_client_addr, tmp_ip)) {
          PRINT_ERROR("Erroneous IP from sender");
          free_ssdp_message(&ssdp_message);
          continue;
        }

        /* Retrieve MAC address from socket (if possible, else NULL) */
        char *tmp_mac = NULL;
        tmp_mac = get_mac_address_from_socket(notif_server_sock,
            (struct sockaddr_storage *)&ssdp_client_addr, NULL);

        /* Build the ssdp message struct */
        BOOL build_success = build_ssdp_message(ssdp_message, tmp_ip, tmp_mac,
            recvLen, ssdp_recv_buffer);
        free(tmp_mac);

        if (!build_success) {
          PRINT_ERROR("Failed to build the SSDP message");
          free_ssdp_message(&ssdp_message);
          continue;
        }

        // TODO: Make it recognize both AND and OR (search for ; inside a ,)!!!

        /* If -M is not set check if it is a M-SEARCH message
           and drop it */
        if (conf.ignore_search_msgs && (strstr(ssdp_message->request,
            "M-SEARCH") != NULL)) {
            PRINT_DEBUG("Message contains a M-SEARCH request, dropping "
                "message");
            free_ssdp_message(&ssdp_message);
            continue;
        }

        /* Check if notification should be used (if any filters have been set) */
        if(filters_factory != NULL) {
          drop_message = filter(ssdp_message, filters_factory);
        }

        /* If message is not filtered then use it */
        if(filters_factory == NULL || !drop_message) {

          /* Add ssdp_message to ssdp_cache
             (this internally checks for duplicates) */
          if(!add_ssdp_message_to_cache(&ssdp_cache, &ssdp_message)) {
            PRINT_ERROR("Failed adding SSDP message to SSDP cache, skipping");
            continue;
          }

          /* Fetch custom fields */
          if(conf.fetch_info && !fetch_custom_fields(&conf, ssdp_message)) {
            PRINT_DEBUG("Could not fetch custom fields");
          }
          ssdp_message = NULL;

          /* Check if forwarding ('-a') is enabled */
          if(conf.forward_enabled) {

            /* If max ssdp cache size reached then it is time to flush */
            if(*ssdp_cache->ssdp_messages_count >= conf.ssdp_cache_size) {
              PRINT_DEBUG("Cache max size reached, sending and emptying");
              if(!flush_ssdp_cache(&conf,
                                   &ssdp_cache,
                                   "/abused/post.php",
                                   notif_recipient_addr,
                                   80,
                                  1)) {
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

      PRINT_DEBUG("loop: done");
    }

  /* We can never get this far */
  } // listen_for_upnp_notif end

  /* If set to scan for AXIS devices then
  start scanning but never continue
  to do any other work the parent should be doing */
  if(conf.scan_for_upnp_devices) {
    //TODO: TOO OLD CODE! REWRITE!
    PRINT_DEBUG("scan_for_upnp_devices begin");

    /* init sending socket */
    PRINT_DEBUG("setup_socket() request");
    socket_conf_s sock_conf = {
      conf.use_ipv6,  // BOOL is_ipv6
      TRUE,           // BOOL is_udp
      TRUE,           // BOOL is_multicast
      conf.interface, // char *interface
      conf.ip,        // char *IP
      NULL,           // struct sockaddr_storage *sa
      (conf.use_ipv6 ? SSDP_ADDR6_LL : SSDP_ADDR),  // const char *ip
      SSDP_PORT,      // int port
      FALSE,          // BOOL is_server
      2,              // int queue_len
      FALSE,          // BOOL keepalive
      conf.ttl,       // int ttl
      conf.enable_loopback // BOOL loopback
    };
    if ((notif_client_sock = setup_socket(&sock_conf)) == SOCKET_ERROR) {
      PRINT_ERROR("Could not create socket");
      cleanup();
      exit(errno);
    }

    /* Parse the filters */
    PRINT_DEBUG("parse_filters()");
    parse_filters(conf.filter, &filters_factory, TRUE & (~conf.quiet_mode));

    char *response = (char *)malloc(sizeof(char) * SSDP_RECV_BUFFER_LEN);
    char *request = (char *)malloc(sizeof(char) * 2048);
    memset(request, '\0', sizeof(char) * 2048);
    strcpy(request, "M-SEARCH * HTTP/1.1\r\n");
    strcat(request, "Host:239.255.255.250:1900\r\n");
    //strcat(request, "ST:urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n");
    strcat(request, "ST:urn:axis-com:service:BasicService:1\r\n");
    //strcat(request, "ST:urn:schemas-upnp-org:device:SwiftServer:1\r\n");
    strcat(request, "Man:\"ssdp:discover\"\r\n");
    strcat(request, "MX:0\r\n\r\n");

    BOOL drop_message;

    /* Client (mcast group) address */
    PRINT_DEBUG("creating multicast address");
    struct addrinfo *addri;
    struct addrinfo hint;
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = (conf.use_ipv6 ? AF_INET6 : AF_INET);
    hint.ai_socktype = SOCK_DGRAM;
    hint.ai_protocol = IPPROTO_UDP;
    hint.ai_canonname = NULL;
    hint.ai_addr = NULL;
    hint.ai_next = NULL;
    char *port_str = (char *)malloc(sizeof(char) * 6);
    memset(port_str, '\0', 6);
    sprintf(port_str, "%d", SSDP_PORT);
    int err = 0;
    if((err = getaddrinfo((conf.use_ipv6 ? SSDP_ADDR6_LL : SSDP_ADDR), port_str, &hint, &addri)) != 0) {
      PRINT_ERROR("getaddrinfo(): %s\n", gai_strerror(err));
      free(port_str);
      free(response);
      free(request);
      cleanup();
      exit(EXIT_FAILURE);
    }
    free(port_str);

    /* Send the UPnP request */
    PRINT_DEBUG("sending request");
    /*****************/
    // This is the temp solution untill the
    // commented-out sendto() code below is fixed
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(SSDP_PORT);
    inet_pton(AF_INET, SSDP_ADDR, &sa.sin_addr);
    /*****************/
    //recvLen = sendto(notif_client_sock, request, strlen(request), 0, (struct sockaddr *)&addri->ai_addr, addri->ai_addrlen);
    recvLen = sendto(notif_client_sock, request, strlen(request), 0, (struct sockaddr *)&sa, sizeof(sa));
    size_t sento_addr_len = sizeof(struct sockaddr_storage);
    struct sockaddr_storage *sendto_addr = (struct sockaddr_storage *)malloc(sento_addr_len);
    memset(sendto_addr, 0, sento_addr_len);
    int response_port = 0;
    if(getsockname(notif_client_sock, (struct sockaddr *)sendto_addr, (socklen_t *)&sento_addr_len) < 0) {
      PRINT_ERROR("Could not get sendto() port, going to miss the replies");
    }
    #ifdef DEBUG___
    else {
      response_port = ntohs(sendto_addr->ss_family == AF_INET ? ((struct sockaddr_in *)sendto_addr)->sin_port : ((struct sockaddr_in6 *)sendto_addr)->sin6_port);
      PRINT_DEBUG("sendto() port is %d", response_port);
    }
    #endif
    close(notif_client_sock);
    PRINT_DEBUG("sent %d bytes", recvLen);
    free(request);
    freeaddrinfo(addri);
    if(recvLen < 0) {
      cleanup();
      PRINT_ERROR("sendto(): Failed sending any data");
      exit(EXIT_FAILURE);
    }

    drop_message = FALSE;

    size_t size = sizeof(ssdp_client_addr);

    /* init listening socket */
    PRINT_DEBUG("setup_socket() listening");

    /* Reuse last sock_conf for the listener */
    sock_conf.is_udp = TRUE; /* BOOL is_udp */
    sock_conf.is_multicast = FALSE; /* BOOL is_multicast */
    sock_conf.ip = NULL; /* const char *ip */
    sock_conf.port = response_port; /* int port */
    sock_conf.is_server = TRUE; /* BOOL is_server */
    sock_conf.queue_length = 5; /* int queue_len */

    if ((notif_server_sock = setup_socket(&sock_conf)) == SOCKET_ERROR) {
      PRINT_ERROR("Could not create socket");
      cleanup();
      exit(errno);
    }

    /* Set socket timeout, to timeout 2 seconds after devices stop answering */
    struct timeval rtimeout;
    rtimeout.tv_sec = conf.upnp_timeout;
    rtimeout.tv_usec = 0;

    PRINT_DEBUG("setting socket receive-timeout to %d", (int)rtimeout.tv_sec);
    if(setsockopt(notif_server_sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&rtimeout, sizeof(rtimeout)) < 0) {
      PRINT_ERROR("setsockopt() SO_RCVTIMEO: (%d) %s", errno, strerror(errno));
      free(response);
      cleanup();
      exit(EXIT_FAILURE);
    }
    PRINT_DEBUG("reciving responses");
    ssdp_message_s *ssdp_message;
    do {
      /* Wait for an answer */
      memset(response, '\0', SSDP_RECV_BUFFER_LEN);
      recvLen = recvfrom(notif_server_sock, response, SSDP_RECV_BUFFER_LEN, 0,
               (struct sockaddr *) &ssdp_client_addr, (socklen_t *)&size);
      PRINT_DEBUG("Recived %d bytes%s", (recvLen < 0 ? 0 : recvLen),
          (recvLen < 0 ? " (wait time limit reached)" : ""));

      if(recvLen < 0) {
        continue;
      }

      ssdp_message = NULL;

      /* init ssdp_message */
      if(!init_ssdp_message(&ssdp_message)) {
        PRINT_ERROR("Failed to initialize SSDP message holder structure");
        continue;
      }

      /* Extract IP */
      char *tmp_ip = (char*)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
      memset(tmp_ip, '\0', IPv6_STR_MAX_SIZE);

      void *sockaddr_ptr;
      if (ssdp_client_addr.ss_family == AF_INET) {
        sockaddr_ptr = &((struct sockaddr_in *)&ssdp_client_addr)->sin_addr;
      } else {
        sockaddr_ptr = &((struct sockaddr_in6 *)&ssdp_client_addr)->sin6_addr;
      }

      if(!inet_ntop(ssdp_client_addr.ss_family, sockaddr_ptr, tmp_ip,
            IPv6_STR_MAX_SIZE)) {
        PRINT_ERROR("inet_ntop(): (%d) %s", errno, strerror(errno));
        free_ssdp_message(&ssdp_message);
        cleanup();
        exit(EXIT_FAILURE);
      }

      char *tmp_mac = (char *)malloc(sizeof(char) * MAC_STR_MAX_SIZE);
      memset(tmp_mac, '\0', MAC_STR_MAX_SIZE);
      tmp_mac = get_mac_address_from_socket(notif_server_sock,
          &ssdp_client_addr, NULL);

      BOOL build_success = build_ssdp_message(ssdp_message, tmp_ip, tmp_mac,
          recvLen, response);
      if(!build_success) {
        continue;
      }

      // TODO: Make it recognize both AND and OR (search for ; inside a ,)!!!
      // TODO: add "request" string filtering
      /* Check if notification should be used (to print and possibly send to the given destination) */
      ssdp_header_s *ssdp_headers = ssdp_message->headers;
      if(filters_factory != NULL) {
        int fc;
        for(fc = 0; fc < filters_factory->filters_count; fc++) {
          if(strcmp(filters_factory->filters[fc].header, "ip") == 0 && strstr(ssdp_message->ip, filters_factory->filters[fc].value) == NULL) {
            drop_message = TRUE;
            break;
          }

          while(ssdp_headers) {
            if(strcmp(get_header_string(ssdp_headers->type, ssdp_headers), filters_factory->filters[fc].header) == 0 && strstr(ssdp_headers->contents, filters_factory->filters[fc].value) == NULL) {
              drop_message = TRUE;
              break;
            }
            ssdp_headers = ssdp_headers->next;
          }
          ssdp_headers = ssdp_message->headers;

          if(drop_message) {
            break;;
          }
        }
      }

      if(filters_factory == NULL || !drop_message) {

        /* Print the message */
        if(conf.xml_output) {
          char *xml_string = (char *)malloc(sizeof(char) * XML_BUFFER_SIZE);
          to_xml(ssdp_message, TRUE, xml_string, XML_BUFFER_SIZE);
          printf("%s\n", xml_string);
          free(xml_string);
        }
        else {
          printf("\n\n\n----------BEGIN NOTIFICATION------------\n");
          printf("Time received: %s\n", ssdp_message->datetime);
          printf("Origin-MAC: %s\n", (ssdp_message->mac != NULL ? ssdp_message->mac : "(Could not be determined)"));
          printf("Origin-IP: %s\nMessage length: %d Bytes\n", ssdp_message->ip, ssdp_message->message_length);
          printf("Request: %s\nProtocol: %s\n", ssdp_message->request, ssdp_message->protocol);

          int hc = 0;
          while(ssdp_headers) {
            printf("Header[%d][type:%d;%s]: %s\n", hc, ssdp_headers->type, get_header_string(ssdp_headers->type, ssdp_headers), ssdp_headers->contents);
            ssdp_headers = ssdp_headers->next;
            hc++;
          }
          ssdp_headers = NULL;
          printf("-----------END NOTIFICATION-------------\n");
        }
        /* TODO: Send the message back to -a */

      }

      free_ssdp_message(&ssdp_message);
    } while(recvLen > 0);
    free(response);
    cleanup();
    PRINT_DEBUG("scan_for_upnp_devices end");
    exit(EXIT_SUCCESS);
  } // scan_for_upnp_devices end

  if(!conf.listen_for_upnp_notif) {
    usage();
  }

  cleanup();
  exit(EXIT_SUCCESS);
} // main end

