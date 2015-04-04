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
 *  Copyright(C) 2014 by Andreas Bank, andreas.mikael.bank@gmail.com
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
 *  - Fix fetch_info so it is done at upnp message creation
 *    and not in to_xml() function
 *  - Fix IPv6!
 */

#define ABUSED_VERSION "0.0.3"

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

/* Uncomment the line below to enable detailed debug information */
#define DEBUG___
/* Uncomment the line below to write to a to a file instead of stdout */
#define DEBUG_TO_FILE___

#define DEBUG_COLOR_BEGIN "\x1b[0;32m"
#define ERROR_COLOR_BEGIN "\x1b[0;31m"
#define DEBUG_COLOR_END "\x1b[0;0m"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netdb.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>

#include <net/if.h>
#include <ifaddrs.h>
#include <ctype.h>

#include <errno.h>
#include <signal.h>
#include <time.h>

#ifdef BSD
#include <sys/sysctl.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h> //test if needed?
#include <sys/file.h>
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

/* PORTABILITY */
#define TRUE              1
#define FALSE             0
#define INVALID_SOCKET   -1
#define SOCKET_ERROR     -1

#ifndef SIOCGARP
    #define SIOCGARP SIOCARPIPLL
#endif

#ifdef DEBUG___
  #define DEBUG_PRINT(...)  printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)  do { } while (false)
#endif

#ifdef DEBUG___
  #define PRINT_DEBUG(...)  print_debug(NULL, DEBUG_COLOR_BEGIN, __FILE__, __LINE__, __VA_ARGS__)
  #define PRINT_ERROR(...)  print_debug(stderr, ERROR_COLOR_BEGIN, __FILE__, __LINE__, __VA_ARGS__)
  #define ADD_ALLOC(...)    add_alloc(__VA_ARGS__)
  #define REMOVE_ALLOC(...) remove_alloc(__VA_ARGS__)
  #define PRINT_ALLOC()     print_alloc()
#else
  #define PRINT_DEBUG(...)  do { } while (FALSE)
  #define PRINT_ERROR(...)  fprintf(stderr, __VA_ARGS__)
  #define ADD_ALLOC(...)    do { } while (FALSE)
  #define REMOVE_ALLOC(...) do { } while (FALSE)
  #define PRINT_ALLOC(...)  do { } while (FALSE)
#endif

#define DAEMON_PORT           43210   // port the daemon will listen on
#define RESULT_SIZE           1048576 // 1MB for the scan results
#define SSDP_ADDR             "239.255.255.250" // SSDP address
#define SSDP_ADDR6_LL         "ff02::c" // SSDP IPv6 link-local address
#define SSDP_ADDR6_SL         "ff05::c" // SSDP IPv6 site-local address
#define SSDP_PORT             1900 // SSDP port
#define NOTIF_RECV_BUFFER     2048 // notification receive-buffer
#define XML_BUFFER_SIZE       2048 // XML buffer/container string
#define IPv4_STR_MAX_SIZE     16 // aaa.bbb.ccc.ddd
#define IPv6_STR_MAX_SIZE     46 // xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:aaa.bbb.ccc.ddd
#define MAC_STR_MAX_SIZE      18 // 00:40:8C:1A:2B:3C + '\0'
#define PORT_MAX_NUMBER       65535
#define LISTEN_QUEUE_LENGTH   2
#define DEVICE_INFO_SIZE      16384
#define MULTICAST_TIMEOUT     2

#define ANSI_COLOR_GREEN   "\x1b[1;32m"
#define ANSI_COLOR_RED     "\x1b[1;31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* SSDP header types string representations */
#define SSDP_HEADER_HOST_STR        "host"
#define SSDP_HEADER_ST_STR          "st"
#define SSDP_HEADER_MAN_STR         "man"
#define SSDP_HEADER_MX_STR          "mx"
#define SSDP_HEADER_CACHE_STR       "cache"
#define SSDP_HEADER_LOCATION_STR    "location"
#define SSDP_HEADER_OPT_STR         "opt"
#define SSDP_HEADER_01NLS_STR       "01-nls"
#define SSDP_HEADER_NT_STR          "nt"
#define SSDP_HEADER_NTS_STR         "nts"
#define SSDP_HEADER_SERVER_STR      "server"
#define SSDP_HEADER_XUSERAGENT_STR  "x-user-agent"
#define SSDP_HEADER_USN_STR         "usn"
#define SSDP_HEADER_UNKNOWN_STR     "unknown"

/* SSDP header types uchars */
#define SSDP_HEADER_HOST            1
#define SSDP_HEADER_ST              2
#define SSDP_HEADER_MAN             3
#define SSDP_HEADER_MX              4
#define SSDP_HEADER_CACHE           5
#define SSDP_HEADER_LOCATION        6
#define SSDP_HEADER_OPT             7
#define SSDP_HEADER_01NLS           8
#define SSDP_HEADER_NT              9
#define SSDP_HEADER_NTS             10
#define SSDP_HEADER_SERVER          11
#define SSDP_HEADER_XUSERAGENT      12
#define SSDP_HEADER_USN             13
#define SSDP_HEADER_UNKNOWN         0


typedef int SOCKET;
typedef int BOOL;

/* Structs */
typedef struct ssdp_header_struct {
  unsigned char type;
  char *unknown_type;
  char *contents;
  struct ssdp_header_struct *first;
  struct ssdp_header_struct *next;
} ssdp_header_s;

typedef struct ssdp_custom_field_struct {
  char *name;
  char *contents;
  struct ssdp_custom_field_struct *first;
  struct ssdp_custom_field_struct *next;
} ssdp_custom_field_s;

typedef struct ssdp_message_struct {
  char *mac;
  char *ip;
  int  message_length;
  char *datetime;
  char *request;
  char *protocol;
  char *answer;
  char *info;
  unsigned char header_count;
  struct ssdp_header_struct *headers;
  unsigned char custom_field_count;
  struct ssdp_custom_field_struct *custom_fields;
} ssdp_message_s;

/* the ssdp_message_s cache that
   acts as a buffer for sending
   bulks of messages instead of spamming;
   *ssdp_message should always point to
   the last ssdp_message in the buffer */
typedef struct ssdp_cache_struct {
  struct ssdp_cache_struct *first;
  ssdp_message_s *ssdp_message;
  struct ssdp_cache_struct *next;
  unsigned int *ssdp_messages_count;
} ssdp_cache_s;

typedef struct filter_struct {
  char *header;
  char *value;
} filter_s;

typedef struct filters_factory_struct {
  filter_s *filters;
  filter_s *first_filter;
  char *raw_filters;
  unsigned char filters_count;
} filters_factory_s;

typedef struct configuration_struct {
  char                interface[IPv6_STR_MAX_SIZE];   /* Interface to use */
  char                ip[IPv6_STR_MAX_SIZE];          /* Interface IP to use */
  BOOL                run_as_daemon;                  /* Run the program as a daemon ignoring control signals and terminal */
  BOOL                run_as_server;                  /* Run the program as a server, open a port and wait for incoming connection (requests) */
  BOOL                listen_for_upnp_notif;          /* Switch for enabling listening for UPnP (SSDP) notifications */
  BOOL                scan_for_upnp_devices;          /* Switch to perform an active UPnP (SSDP) scan */
  BOOL                forward_enabled;                /* Switch to enable forwarding of the scan results */
  BOOL                fetch_info;                     /* Don't try to fetch device info from the url in the "Location" header */
  BOOL                ssdp_cache_size;                /* The size of the ssdp_cache list */
  BOOL                json_output;                    /* Convert to JSON before forwarding */
  BOOL                xml_output;                     /* Convert to XML before forwarding */
  unsigned char       ttl;                            /* Time-To-Live value, how many routers the multicast shall propagate through */
  BOOL                ignore_search_msgs;             /* Automatically add a filter to ignore M-SEARCH messages*/
  char               *filter;                         /* Grep-like filter string */
  BOOL                use_ipv4;                       /* Force the usage of the IPv4 protocol */
  BOOL                use_ipv6;                       /* Force the usage of the IPv6 protocol */
  BOOL                quiet_mode;                     /* Minimize informative output */
  int                 upnp_timeout;                   /* The time to wait for a device to answer a query */
  BOOL                enable_loopback;                /* Enable multicast loopback traffic */
} configuration_s;


/* Globals */
static SOCKET               notif_server_sock = 0;       /* The main socket for listening for UPnP (SSDP) devices (or device answers) */
static SOCKET               notif_client_sock = 0;       /* The sock that we want to ask/look for devices on (unicast) */
static SOCKET               notif_recipient_sock = 0;    /* Notification recipient socket, where the results will be forwarded */
struct sockaddr_storage    *notif_recipient_addr = NULL; /* The sockaddr where we store the recipient address */
static filters_factory_s   *filters_factory = NULL;      /* The structure contining all the filters information */
static configuration_s      conf;                         /* The program configuration */

/* Functions */
static void free_stuff();
static void set_default_configuration(configuration_s *);
static void parse_args(int, char * const *, configuration_s *);
static void exitSig(int);
static BOOL build_ssdp_message(ssdp_message_s *, char *, char *, int, const char *);
static unsigned char get_header_type(const char *);
static const char *get_header_string(const unsigned int, const ssdp_header_s *);
static int strpos(const char*, const char *);
static int send_stuff(const char *, const char *, const struct sockaddr_storage *, int, int);
static void free_ssdp_message(ssdp_message_s **);
static int fetch_custom_fields(ssdp_message_s *);
static unsigned int to_json(const ssdp_message_s *, BOOL, BOOL, BOOL, char *, int);
static unsigned int to_xml(const ssdp_message_s *, BOOL, BOOL, BOOL, char *, int);
static BOOL parse_url(const char *, char *, int, int *, char *, int);
static void parse_filters(char *, filters_factory_s **, BOOL);
//static char *get_ip_address_from_socket(const SOCKET);
static char *get_mac_address_from_socket(const SOCKET, struct sockaddr_storage *, char *);
static BOOL parse_address(const char *, struct sockaddr_storage **);
static int find_interface(struct sockaddr_storage *, char *, char *);
static SOCKET create_upnp_listener(char *, int, int);
static BOOL set_send_timeout(SOCKET, int);
static BOOL set_receive_timeout(SOCKET, int);
static BOOL set_reuseaddr(SOCKET);
static BOOL set_keepalive(SOCKET, BOOL);
static BOOL set_ttl(SOCKET, int, int);
static BOOL disable_multicast_loopback(SOCKET, int);
static BOOL join_multicast_group(SOCKET, char *, char *);
static SOCKET setup_socket(BOOL, BOOL, BOOL, char *, char *, struct sockaddr_storage *, const char *, int, BOOL, BOOL);
static BOOL is_address_multicast(const char *);
static BOOL init_ssdp_message(ssdp_message_s **);
static BOOL create_plain_text_message(char *, int, ssdp_message_s *, BOOL);
static BOOL add_ssdp_message_to_cache(ssdp_cache_s **, ssdp_message_s *);
static void free_ssdp_cache(ssdp_cache_s **);
static void daemonize();
static BOOL filter(ssdp_message_s *, filters_factory_s *);
static unsigned int cache_to_json(ssdp_cache_s *, char *, unsigned int, BOOL);
static unsigned int cache_to_xml(ssdp_cache_s *, char *, unsigned int, BOOL);
static BOOL flush_ssdp_cache(ssdp_cache_s **, const char *, struct sockaddr_storage *, int, int, BOOL);
static void display_ssdp_cache(ssdp_cache_s *);
static char *horizontal_line(const char, const char, int);
static void move_cursor(int, int);
static void get_window_size(int *, int *);
static ssdp_custom_field_s *get_custom_field(ssdp_message_s *, const char *);
#ifdef DEBUG_TO_FILE___
static int fsize(const char *);
#endif
#ifdef DEBUG___
static int chr_count(char *, char);
static void print_debug(FILE *, const char *, const char*, int, char *, ...);
#endif

/**
 * Prepares the process to run as a daemon
 */
static void daemonize() {

  PRINT_DEBUG("Running as daemon");

  int fd, max_open_fds;

  /**
  * Daemon preparation steps:
  *
  * 1. Set up rules for ignoring all (tty related)
  *    signals that can stop the process
  *
  * 2. Fork to child and exit parent to free
  *    the terminal and the starting process
  *
  * 3. Change PGID to ignore parent stop signals
  *    and disassociate from controlling terminal
  *    (two solutions, normal and BSD)
  *
  */

  /* If started with init then no need to detach and do all the stuff */
  if(getppid() == 1) {
    
    PRINT_DEBUG("Started by init, skipping some daemon setup");
    goto started_with_init;

  }

  /* Ignore signals */
  #ifdef SIGTTOU
  signal(SIGTTOU, SIG_IGN);
  #endif

  #ifdef SIGTTIN
  signal(SIGTTIN, SIG_IGN);
  #endif

  #ifdef SIGTSTP
  signal(SIGTSTP, SIG_IGN);
  #endif

  /*  Get new PGID (not PG-leader and not zero) and free parent process
  so terminal can be used/closed */
  if(fork() != 0) {

    /* Exit parent */
    exit(EXIT_SUCCESS);

  }

  #ifdef BSD
  PRINT_DEBUG("Using BSD daemon proccess");
  setpgrp();

  if((fd = open("/dev/tty", O_RDWR)) >= 0) {

    /* Get rid of the controlling terminal */
    ioctl(fd, TIOCNOTTY, 0);
    close(fd);

  }
  else {

    PRINT_ERROR("Could not dissassociate from controlling terminal: openning /dev/tty failed.");
    free_stuff();
    exit(EXIT_FAILURE);

  }
  #else
  PRINT_DEBUG("Using UNIX daemon proccess");

  /* Non-BSD UNIX systems do the above with a single call to setpgrp() */
  setpgrp();

  /* Ignore death if parent dies */
  signal(SIGHUP,SIG_IGN);

  /*  Get new PGID (not PG-leader and not zero)
      because non-BSD systems don't allow assigning
      controlling terminals to non-PG-leader processes */
  pid_t pid = fork();
  if(pid < 0) {

    /* Exit the both processess */
    exit(EXIT_FAILURE);

  }

  if(pid > 0) {

    /* Exit the parent */
    exit(EXIT_SUCCESS);

  }
  #endif

  started_with_init:

  /* Close all possibly open descriptors */
  PRINT_DEBUG("Closing all open descriptors");
  max_open_fds = sysconf(_SC_OPEN_MAX);
  for(fd = 0; fd < max_open_fds; fd++) {

    close(fd);

  }

  /* Change cwd in case it was on a mounted system */
  if(-1 == chdir("/")) {
    PRINT_ERROR("Failed to change to a safe directory");
    free_stuff();
    exit(EXIT_FAILURE);
  }

  /* Clear filemode creation mask */
  umask(0);

  PRINT_DEBUG("Now running in daemon mode");

}

/**
 * Convert a ssdp cache list (multiple ssdp_messages)
 * to a single JSON blob
 * @param ssdp_cache The SSDP messages to convert
 * @param json_message The buffer to write to
 * @param json_buffer_size The buffer size
 *
 * @return The number of byter written
 */
static unsigned int cache_to_json(ssdp_cache_s *ssdp_cache,
                                  char *json_buffer,
                                  unsigned int json_buffer_size,
                                  BOOL fetch_info) {
  unsigned int used_buffer = 0;

  /* Point at the beginning */
  ssdp_cache = ssdp_cache->first;

  /* For every element in the ssdp cache */
  used_buffer = snprintf(json_buffer,
                         json_buffer_size,
                         "root {\n");
  while(ssdp_cache) {
    used_buffer += to_json(ssdp_cache->ssdp_message,
                          FALSE,
                          fetch_info,
                          FALSE,
                          (json_buffer + used_buffer),
                          (json_buffer_size - used_buffer));
    ssdp_cache = ssdp_cache->next;
  }
  used_buffer += snprintf((json_buffer + used_buffer),
                          (json_buffer_size - used_buffer),
                          "}\n");
  return used_buffer;
}

/**
 * Convert a ssdp cache list (multiple ssdp_messages)
 * to a single XML blob
 * @param ssdp_cache The SSDP messages to convert
 * @param xml_message The buffer to write to
 * @param xml_buffer_size The buffer size
 *
 * @return The number of byter written
 */
static unsigned int cache_to_xml(ssdp_cache_s *ssdp_cache,
                                 char *xml_buffer,
                                 unsigned int xml_buffer_size,
                                 BOOL fetch_info) {
  unsigned int used_buffer = 0;

  if(NULL == ssdp_cache) {
    PRINT_ERROR("No valid SSDP cache given (NULL)");
    return 0;
  }

  /* Point at the beginning */
  ssdp_cache = ssdp_cache->first;

  /* For every element in the ssdp cache */
  used_buffer = snprintf(xml_buffer,
                         xml_buffer_size,
                         "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<root>\n");
  while(ssdp_cache) {
    PRINT_DEBUG("cache_to_xml buffer used: %d; left: %d", used_buffer, xml_buffer_size - used_buffer);
    used_buffer += to_xml(ssdp_cache->ssdp_message,
                          FALSE,
                          fetch_info,
                          FALSE,
                          (xml_buffer + used_buffer),
                          (xml_buffer_size - used_buffer));
    ssdp_cache = ssdp_cache->next;
  }
  used_buffer += snprintf((xml_buffer + used_buffer),
                          (xml_buffer_size - used_buffer),
                          "</root>\n");
  return used_buffer;
}

/**
 * Send and free the passed SSDP cache
 */
static BOOL flush_ssdp_cache(ssdp_cache_s **ssdp_cache_pointer,
                             const char *url,
                             struct sockaddr_storage *sockaddr_recipient,
                             int port,
                             int timeout,
                             BOOL fetch_info) {
  ssdp_cache_s *ssdp_cache = *ssdp_cache_pointer;
  int results_size = *ssdp_cache->ssdp_messages_count * XML_BUFFER_SIZE;
  char results[results_size];

  /* If -j then convert all messages to one JSON blob */
  if(conf.json_output) {
    if(1 > cache_to_json(ssdp_cache, results, results_size, fetch_info)) {
      PRINT_ERROR("Failed creating JSON blob from ssdp cache");
      return FALSE;
    }
  }

  /* If -x then convert all messages to one XML blob */
  if(conf.xml_output) {
    if(1 > cache_to_xml(ssdp_cache, results, results_size, fetch_info)) {
      PRINT_ERROR("Failed creating XML blob from ssdp cache");
      return FALSE;
    }
  }

  // TODO: make it create a list instead of single plain message
  else if(!create_plain_text_message(results, XML_BUFFER_SIZE, ssdp_cache->ssdp_message, fetch_info)) {
    PRINT_ERROR("Failed creating plain-text message");
    return FALSE;
  }

  /* Send the converted cache list to the recipient (-a) */
  send_stuff(url, results, sockaddr_recipient, port, timeout);

  /* When the ssdp_cache has been sent
     then free/empty the cache list */
  free_ssdp_cache(ssdp_cache_pointer);

  return TRUE;
}

/**
 * Check if the SSDP message needs to be filtered-out (dropped)
 *
 * @param ssdp_message The SSDP message to be checked
 * @param filters_factory The filters to check against
 */
static BOOL filter(ssdp_message_s *ssdp_message, filters_factory_s *filters_factory) {

  PRINT_DEBUG("traversing filters");
  BOOL drop_message = FALSE;
  int fc;
  for(fc = 0; fc < filters_factory->filters_count; fc++) {

    BOOL filter_found = FALSE;
    char *filter_value = filters_factory->filters[fc].value;
    char *filter_header = filters_factory->filters[fc].header;

    /* If IP filtering has been set, check values */
    if(strcmp(filter_header, "ip") == 0) {
      filter_found = TRUE;
      if(strstr(ssdp_message->ip, filter_value) == NULL) {
        PRINT_DEBUG("IP filter mismatch, dropping message");
        drop_message = TRUE;
        break;
      }
    }

    /* If mac filtering has been set, check values */
    if(strcmp(filter_header, "mac") == 0) {
      filter_found = TRUE;
      if(strstr(ssdp_message->mac, filter_value) == NULL) {
        PRINT_DEBUG("MAC filter mismatch, dropping message");
        drop_message = TRUE;
        break;
      }
    }

    /* If protocol filtering has been set, check values */
    if(strcmp(filter_header, "protocol") == 0) {
      filter_found = TRUE;
      if(strstr(ssdp_message->protocol, filter_value) == NULL) {
        PRINT_DEBUG("Protocol filter mismatch, dropping message");
        drop_message = TRUE;
        break;
      }
    }

    /* If request filtering has been set, check values */
    if(strcmp(filter_header, "request") == 0) {
      filter_found = TRUE;
      if(strstr(ssdp_message->request, filter_value) == NULL) {
        PRINT_DEBUG("Request filter mismatch, dropping message");
        drop_message = TRUE;
        break;
      }
    }

    ssdp_header_s *ssdp_headers = ssdp_message->headers;

    /* If any other filter has been set try to match it
       with a header type and then check values */
    while(ssdp_headers) {

      /* See if the headet type matches the filter type
         and if so check the values */
      char const *ssdp_header_string = get_header_string(ssdp_headers->type, ssdp_headers);
      if(strcmp(ssdp_header_string, filter_header) == 0) {
        filter_found = TRUE;

        /* Then see if the values match */
        if(strstr(ssdp_headers->contents, filter_value) == NULL) {
          PRINT_DEBUG("Header (%s) filter mismatch, marked for dropping", filter_header);
          drop_message = TRUE;
          break;
        }
      }
      ssdp_headers = ssdp_headers->next;
    }

    /* If no comparison (match or missmatch) was made then drop the message */
    if(!filter_found) {
      PRINT_DEBUG("Filter type not found, marked for dropping");
      drop_message = TRUE;
    }

    if(drop_message) {
      break;
    }
  }

  return drop_message;
}

/**
 * Searches the SSDP message custom-fields for the given custom-field name
 *
 * @param ssdp_message The SSDP message to search in
 * @param custom_field The Custom Field to search for
 *
 * @return Returns the found custom-field or NULL
 */
static ssdp_custom_field_s *get_custom_field(ssdp_message_s *ssdp_message, const char *custom_field) {
  ssdp_custom_field_s *cf = NULL;

  if(ssdp_message && custom_field) {
    cf = ssdp_message->custom_fields;

    /* Loop through the custom-fields */
    while(cf) {

      /* If anyone matches return its value */
      if(0 == strcmp(custom_field, cf->name)) {
        return cf;
      }

      cf = cf->next;
    }

  }

  return NULL;
}

static void free_stuff() {

  if(notif_server_sock != 0) {
    close(notif_server_sock);
    notif_server_sock = 0;
  }

  if(notif_client_sock != 0) {
    close(notif_client_sock);
    notif_client_sock = 0;
  }

  if(notif_recipient_sock != 0) {
    close(notif_recipient_sock);
    notif_recipient_sock = 0;
  }

  if(filters_factory != NULL) {
    if(filters_factory->filters != NULL) {
      int fc;
      for(fc = 0; fc < (filters_factory->filters_count); fc++) {
        if(filters_factory->filters[fc].header != NULL) {
          free(filters_factory->filters[fc].header);
          filters_factory->filters[fc].header = NULL;
        }
        if((filters_factory->filters + fc)->value) {
          free(filters_factory->filters[fc].value);
          filters_factory->filters[fc].value = NULL;
        }
      }
      free(filters_factory->filters);
      filters_factory->filters = NULL;
    }
    free(filters_factory);
    filters_factory = NULL;
  }

  if(!conf.quiet_mode) {
    printf("\nCleaning up and exitting...\n");
  }

}

static void usage() {
  printf("[A]bused is [B]anks [U]ber-[S]exy-[E]dition [D]aemon\"\n\n");
  printf("USAGE: abused [OPTIONS]\n");
  printf("OPTIONS:\n");
  //printf("\t-C <file.conf>    Configuration file to use\n");
  printf("\t-i                Interface to use, default is all\n");
  printf("\t-I                Interface IP address to use, default is a bind-all address\n");
  printf("\t-t                TTL value (routers to hop), default is 1\n");
  printf("\t-f <string>       Filter for capturing, 'grep'-like effect. Also works\n");
  printf("\t                  for -u and -U where you can specify a list of\n");
  printf("\t                  comma separated filters\n");
  printf("\t-M                Don't ignore UPnP M-SEARCH messages\n");
  printf("\t-S                Run as a server,\n");
  printf("\t                  listens on port 43210 and returns a\n");
  printf("\t                  UPnP scan result (formatted list) for AXIS devices\n");
  printf("\t                  upon receiving the string 'abused'\n");
  printf("\t-d                Run as a UNIX daemon,\n");
  printf("\t                  only works in combination with -S or -a\n");
  printf("\t-u                Listen for local UPnP (SSDP) notifications\n");
  printf("\t-U                Perform an active search for UPnP devices\n");
  printf("\t-a <ip>:<port>    Forward the events to the specified ip and port,\n");
  printf("\t                  also works in combination with -u.\n");
  printf("\t-F                Do not try to parse the \"Location\" header and fetch device info\n");
  //printf("\t-j                Convert results to JSON before sending\n");
  printf("\t-x                Convert results to XML before sending\n");
  //printf("\t-4                Force the use of the IPv4 protocol\n");
  //printf("\t-6                Force the use of the IPv6 protocol\n");
  printf("\t-q                Be quiet!\n");
  printf("\t-T                The time to wait for a device to answer a query\n");
  printf("\t-L                Enable multicast loopback traffic\n");
}

static void set_default_configuration(configuration_s *c) {

  /* Default configuration */
  memset(c->interface, '\0', IPv6_STR_MAX_SIZE);
  memset(c->ip, '\0', IPv6_STR_MAX_SIZE);
  c->run_as_daemon         = FALSE;
  c->run_as_server         = FALSE;
  c->listen_for_upnp_notif = FALSE;
  c->scan_for_upnp_devices = FALSE;
  c->forward_enabled       = FALSE;
  c->ssdp_cache_size       = 10;
  c->fetch_info            = TRUE;
  c->json_output           = FALSE;
  c->xml_output            = FALSE;
  c->ttl                   = 1;
  c->filter                = NULL;
  c->ignore_search_msgs    = TRUE;
  c->use_ipv4              = FALSE;
  c->use_ipv6              = FALSE;
  c->quiet_mode            = FALSE;
  c->upnp_timeout          = MULTICAST_TIMEOUT;
  c->enable_loopback       = FALSE;
}

/* Parse arguments */
static void parse_args(const int argc, char * const *argv, configuration_s *conf) {
  int opt;

  while ((opt = getopt(argc, argv, "C:i:I:t:f:MSduUma:RFc:jx64qT:L")) > 0) {
    char *pend = NULL;

    switch (opt) {

    case 'C':
      /*if(!parse_configuration_file(conf, optarg)) {
        printf("Parsing of '%s' failed!, optarg);
      };*/
      break;

    case 'i':
      strncpy(conf->interface, optarg, IPv6_STR_MAX_SIZE);
      break;

    case 'I':
      strncpy(conf->ip, optarg, IPv6_STR_MAX_SIZE);
      break;

    case 't':
      // TODO: errorhandling
      pend = NULL;
      conf->ttl = (unsigned char)strtol(optarg, &pend, 10);
      break;

    case 'f':
      conf->filter = optarg;
      break;

    case 'M':
      conf->ignore_search_msgs = FALSE;
      break;

    case 'S':
      conf->run_as_server = TRUE;
      break;

    case 'd':
      conf->run_as_daemon = TRUE;
      break;

    case 'u':
      conf->listen_for_upnp_notif = TRUE;
      break;

    case 'U':
      conf->scan_for_upnp_devices = TRUE;
      break;

    case 'a':
      if(optarg != NULL && strlen(optarg) > 0) {
        PRINT_DEBUG("parse_address()");
        if(!parse_address(optarg, &notif_recipient_addr)) {
          usage();
          free_stuff();
          exit(EXIT_FAILURE);
        }
        conf->forward_enabled = TRUE;
      }
      break;

    case 'm':
      printf("         {\n      {   }\n       }_{ __{\n    .-{   }   }-.\n   (   }     {   )\n   |`-.._____..-'|\n   |             ;--.\n   |            (__  \\\n   |             | )  )\n   |   ABUSED    |/  /\n   |             /  /\n   |            (  /\n   \\             y'\n    `-.._____..-'\n");
      free_stuff();
      exit(EXIT_SUCCESS);

    case 'F':
      conf->fetch_info = FALSE;
      break;

    case 'c':
      // TODO: errorhandling
      pend = NULL;
      conf->ssdp_cache_size = (int)strtol(optarg, &pend, 10);
      break;

    case 'j':
      conf->xml_output = FALSE;
      conf->json_output = TRUE;
      break;

    case 'x':
      conf->json_output = FALSE;
      conf->xml_output = TRUE;
      break;

    case '4':
      conf->use_ipv6 = FALSE;
      conf->use_ipv4 = TRUE;
      break;

    case '6':
      conf->use_ipv4 = FALSE;
      conf->use_ipv6 = TRUE;
      break;

    case 'q':
      conf->quiet_mode = TRUE;
      break;

    case 'T':
      // TODO: errorhandling
      pend = NULL;
      conf->upnp_timeout = (int)strtol(optarg, &pend, 10);
      break;

    case 'L':
      conf->enable_loopback = TRUE;
      break;

    default:
      usage();
      free_stuff();
      exit(EXIT_FAILURE);
    }
  }
}

static char *horizontal_line(const char start_char,
                             const char end_char,
                             int line_length) {
  char *h_line = (char *)malloc(sizeof(char) * (line_length + 1));
  const char h_char = '-';

  memset(h_line, h_char, sizeof(char) * line_length + 1);
  h_line[line_length] = '\0';

  h_line[0] = (start_char == 0 ? h_char : end_char);
  h_line[line_length - 1] = (end_char == 0 ? h_char : end_char);

  return h_line;
}

static void get_window_size(int *width, int *height) {
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    *width = w.ws_col;
    *height = w.ws_row;
}

/* Moves the terminal cursor to the given coords */
static void move_cursor(int row, int col) {
  printf("\033[%d;%dH", row, col);
}

/**
 * Displays the SSDP cache list as a table
 */
static void display_ssdp_cache(ssdp_cache_s *ssdp_cache) {
  int horizontal_lines_printed = 4;
  int h_line_length;
  char *h_line;
//  printf("%c", 14);
//  printf("\033[2J");
  move_cursor(0, 0);
  h_line_length = 97;
  h_line = horizontal_line('+', '+', h_line_length);
  printf("%s\n", h_line);
  printf("| ID                  | IPv4            | MAC               |  Model          | Version         |\n");
  h_line[0] = '+';
  h_line[h_line_length - 1] = '+';
  printf("%s\n", h_line);
  if(ssdp_cache) {
    ssdp_custom_field_s *cf = NULL;
    ssdp_cache = ssdp_cache->first;
    const char no_info[] = "<no info>";
    while(ssdp_cache) {
        cf = get_custom_field(ssdp_cache->ssdp_message, "serialNumber");
        printf("%c %-20s", '|', (cf && cf->contents ? cf->contents : no_info));
        printf("%c %-16s", '|', ssdp_cache->ssdp_message->ip);
        printf("%c %-18s", '|', (ssdp_cache->ssdp_message->mac &&
                                 0 != strcmp(ssdp_cache->ssdp_message->mac, "") ?
                                 ssdp_cache->ssdp_message->mac :
                                 no_info));
        cf = get_custom_field(ssdp_cache->ssdp_message, "modelName");
        printf("%c %-16s", '|', (cf && cf->contents ? cf->contents : no_info));
        cf = get_custom_field(ssdp_cache->ssdp_message, "modelNumber");
        printf("%c %-16s", '|', (cf && cf->contents ? cf->contents : no_info));
        printf("|\n");

        horizontal_lines_printed++;

        ssdp_cache = ssdp_cache->next;
    }
  }
  else {
    printf("%c%60s%c", '\x6D', "No UPnP messages received", '\x6A');
    horizontal_lines_printed++;
  }
  h_line[0] = '+';
  h_line[h_line_length - 1] = '+';
  printf("%s\n", h_line);
  free(h_line);
  int width, height, i;
  get_window_size(&width, &height);
  for(i = 1; i < height - horizontal_lines_printed; i++) {
    printf("%*s\n", width, " ");
  }
//  printf("%c", 15);
}

int main(int argc, char **argv) {
  struct sockaddr_storage notif_client_addr;
  int recvLen = 1;

  signal(SIGTERM, &exitSig);
  signal(SIGABRT, &exitSig);
  signal(SIGINT, &exitSig);

  #ifdef DEBUG___
  PRINT_DEBUG("%sDebug color%s", DEBUG_COLOR_BEGIN, DEBUG_COLOR_END);
  PRINT_DEBUG("%sError color%s", ERROR_COLOR_BEGIN, DEBUG_COLOR_END);
  #endif

  set_default_configuration(&conf);
  parse_args(argc, argv, &conf);

  /* Output forward status */
  if(!conf.quiet_mode && notif_recipient_addr) {
    char ip[IPv6_STR_MAX_SIZE];
    memset(ip, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);
    inet_ntop(notif_recipient_addr->ss_family,
              (notif_recipient_addr->ss_family == AF_INET ?
                (void *)&(((struct sockaddr_in *)notif_recipient_addr)->sin_addr) :
                (void *)&(((struct sockaddr_in6 *)notif_recipient_addr)->sin6_addr)),
              ip,
              sizeof(char) * IPv6_STR_MAX_SIZE);
    printf("Forwarding is enabled, ");
    printf("forwarding to IP %s on port %d\n",
           ip,
           ntohs((notif_recipient_addr->ss_family == AF_INET ?
                   ((struct sockaddr_in *)notif_recipient_addr)->sin_port :
                   ((struct sockaddr_in6 *)notif_recipient_addr)->sin6_port)));
  }

  /* If missconfigured, stop and notify the user */
  if(conf.run_as_daemon &&
     !(conf.run_as_server ||
       (conf.listen_for_upnp_notif && conf.forward_enabled) ||
       conf.scan_for_upnp_devices)) {

    PRINT_ERROR("Cannot start as daemon.\nUse -d in combination with -S or -a.\n");
    exit(EXIT_FAILURE);

  }
  /* If run as a daemon */
  else if(conf.run_as_daemon) {
    daemonize();
  }

  /* If set to listen for UPnP notifications then
     fork() and live a separate life */
  if(conf.listen_for_upnp_notif &&
     ((conf.run_as_daemon && conf.forward_enabled) ||
     conf.run_as_server)) {
    if(fork() != 0) {
      /* listen_for_upnp_notif went to the forked process,
         so it is set to false in parent so it doesn't run twice'*/
      conf.listen_for_upnp_notif = FALSE;
    }
    else {
      PRINT_DEBUG("Created a process for listening of UPnP notifications");
      /* scan_for_upnp_devices is set to false
         so it doesn't run in this child (if it will be run at all) */
      conf.scan_for_upnp_devices = FALSE;
    }
  }

  /* If set to scan for UPnP-enabled devices then
     fork() and live a separate life */
  if(conf.scan_for_upnp_devices && (conf.run_as_daemon || conf.run_as_server)) {
    if(fork() != 0) {
      /* scan_for_upnp_devices went to the forked process,
         so it set to false in parent so it doesn't run twice */
      conf.scan_for_upnp_devices = FALSE;
    }
    else {
      PRINT_DEBUG("Created a process for scanning UPnP devices");
      /* listen_for_upnp_notif is set to false
         so it doesn't run in this child (if it will be run at all) */
      conf.listen_for_upnp_notif = FALSE;
    }
  }

  /* If set to listen for AXIS devices notifications then
     start listening for notifications but never continue
     to do any other work the parent should be doing */
  if(conf.listen_for_upnp_notif) {
    PRINT_DEBUG("listen_for_upnp_notif start");

    /* Init buffer for receiving messages */
    char notif_string[NOTIF_RECV_BUFFER];

    /* init socket */
    PRINT_DEBUG("setup_socket for listening to upnp");
    if((notif_server_sock = setup_socket(
              FALSE,      // BOOL is_ipv6
              TRUE,       // BOOL is_udp
              TRUE,       // BOOL is_multicast
              conf.interface,  // char interface
              conf.ip,
              NULL,       // struct sockaddr_storage *sa
              SSDP_ADDR,  // const char *ip
              SSDP_PORT,  // int port
              TRUE,       // BOOL is_server
              FALSE       // BOOL keepalive
      )) < 0) {
      PRINT_ERROR("Could not create socket");
      free_stuff();
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

      memset(notif_string, '\0', NOTIF_RECV_BUFFER);
      int size = sizeof(notif_client_addr);
      PRINT_DEBUG("loop: ready to receive");

      /* Set a timeout limit when waiting for ssdp messages */
      if(!set_receive_timeout(notif_server_sock, 10)) {
        PRINT_ERROR("Failed to set the receive timeout");
        free_stuff();
        exit(EXIT_FAILURE);
      }

      recvLen = recvfrom(notif_server_sock, notif_string, NOTIF_RECV_BUFFER, 0,
                        (struct sockaddr *) &notif_client_addr, (socklen_t *)&size);

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
            if(!flush_ssdp_cache(&ssdp_cache,
                                 "/abused/post.php",
                                 notif_recipient_addr,
                                 80,
                                 1,
                                 conf.fetch_info)) {
              PRINT_DEBUG("Failed flushing SSDP cache");
              continue;
            }
          }
          /* Else just display the cached messages in a table */
          else {
            PRINT_DEBUG("Displaying cached SSDP messages");
            display_ssdp_cache(ssdp_cache);
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
        if(!inet_ntop(notif_client_addr.ss_family,
                      (notif_client_addr.ss_family == AF_INET ?
                        (void *)&((struct sockaddr_in *)&notif_client_addr)->sin_addr :
                        (void *)&((struct sockaddr_in6 *)&notif_client_addr)->sin6_addr),
                      tmp_ip,
                      IPv6_STR_MAX_SIZE)) {
          PRINT_ERROR("Erroneous IP from sender");
          free_ssdp_message(&ssdp_message);
          continue;
        }
  
        /* Retrieve MAC address from socket (if possible, else NULL) */
        char *tmp_mac = NULL;
        tmp_mac = get_mac_address_from_socket(notif_server_sock, (struct sockaddr_storage *)&notif_client_addr, NULL);
  
        /* Build the ssdp message struct */
        BOOL build_success = build_ssdp_message(ssdp_message, tmp_ip, tmp_mac, recvLen, notif_string);
        free(tmp_mac);
  
        if(!build_success) {
          PRINT_ERROR("Failed to build the SSDP message");
          free_ssdp_message(&ssdp_message);
          continue;
        }
  
        // TODO: Make it recognize both AND and OR (search for ; inside a ,)!!!
  
        /* If -M is not set check if it is a M-SEARCH message
           and drop it */
        if(conf.ignore_search_msgs && (strstr(ssdp_message->request, "M-SEARCH") != NULL)) {
            PRINT_DEBUG("Message contains a M-SEARCH request, dropping message");
            free_ssdp_message(&ssdp_message);
            continue;
        }
  
        /* Check if notification should be used (if any filters have been set) */
        if(filters_factory != NULL) {
          drop_message = filter(ssdp_message, filters_factory);
        }

        /* If message is not filtered then use it */
        if(filters_factory == NULL || !drop_message) {

          /* Fetch custom fields */
          if(!fetch_custom_fields(ssdp_message)) {
            PRINT_DEBUG("Could not fetch custom fields");
          }

          /* Add ssdp_message to ssdp_cache */
          if(!add_ssdp_message_to_cache(&ssdp_cache, ssdp_message)) {
            PRINT_ERROR("Failed adding SSDP message to SSDP cache, skipping");
            continue;
          }
          ssdp_message = NULL;

          /* Check if forwarding ('-a') is enabled */
          if(conf.forward_enabled) {

            /* If max ssdp cache size reached then it is time to flush */
            if(*ssdp_cache->ssdp_messages_count >= conf.ssdp_cache_size) {
              PRINT_DEBUG("Cache max size reached, sending and emptying");
              if(!flush_ssdp_cache(&ssdp_cache,
                               "/abused/post.php",
                               notif_recipient_addr,
                               80,
                               1,
                               conf.fetch_info)) {
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
            display_ssdp_cache(ssdp_cache);
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
    notif_client_sock = setup_socket(conf.use_ipv6,  // BOOL is_ipv6
              TRUE,       // BOOL is_udp
              TRUE,       // BOOL is_multicast
              conf.interface,  // char interface
              conf.ip,
              NULL,       // struct sockaddr_storage *sa
              (conf.use_ipv6 ? SSDP_ADDR6_LL : SSDP_ADDR),  // const char *ip
              SSDP_PORT,  // int port
              FALSE,      // BOOL is_server
              FALSE);     // BOOL keepalive

    /* Parse the filters */
    PRINT_DEBUG("parse_filters()");
    parse_filters(conf.filter, &filters_factory, TRUE & (~conf.quiet_mode));

    char *response = (char *)malloc(sizeof(char) * NOTIF_RECV_BUFFER);
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
      free_stuff();
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
      free_stuff();
      PRINT_ERROR("sendto(): Failed sending any data");
      exit(EXIT_FAILURE);
    }

    drop_message = FALSE;

    size_t size = sizeof(notif_client_addr);

    /* init listening socket */
    PRINT_DEBUG("setup_socket() listening");
    notif_server_sock = setup_socket(conf.use_ipv6,  // BOOL is_ipv6
              TRUE,       // BOOL is_udp
              FALSE,       // BOOL is_multicast
              conf.interface,  // char interface
              conf.ip,
              NULL,       // struct sockaddr_storage *sa
              NULL,       // const char *ip
              response_port,  // int port
              TRUE,       // BOOL is_server
              FALSE);     // BOOL keepalive

    /* Set socket timeout, to timeout 2 seconds after devices stop answering */
    struct timeval rtimeout;
    rtimeout.tv_sec = conf.upnp_timeout;
    rtimeout.tv_usec = 0;

    PRINT_DEBUG("setting socket receive-timeout to %d", (int)rtimeout.tv_sec);
    if(setsockopt(notif_server_sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&rtimeout, sizeof(rtimeout)) < 0) {
      PRINT_ERROR("setsockopt() SO_RCVTIMEO: (%d) %s", errno, strerror(errno));
      free(response);
      free_stuff();
      exit(EXIT_FAILURE);
    }
    PRINT_DEBUG("reciving responses");
    ssdp_message_s *ssdp_message;
    do {
      /* Wait for an answer */
      memset(response, '\0', NOTIF_RECV_BUFFER);
      recvLen = recvfrom(notif_server_sock, response, NOTIF_RECV_BUFFER, 0,
               (struct sockaddr *) &notif_client_addr, (socklen_t *)&size);
      PRINT_DEBUG("Recived %d bytes%s", (recvLen < 0 ? 0 : recvLen), (recvLen < 0 ? " (wait time limit reached)" : ""));

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
      if(!inet_ntop(notif_client_addr.ss_family, (notif_client_addr.ss_family == AF_INET ? (void *)&((struct sockaddr_in *)&notif_client_addr)->sin_addr : (void *)&((struct sockaddr_in6 *)&notif_client_addr)->sin6_addr), tmp_ip, IPv6_STR_MAX_SIZE)) {
        PRINT_ERROR("inet_ntop(): (%d) %s", errno, strerror(errno));
        free_ssdp_message(&ssdp_message);
        free_stuff();
        exit(EXIT_FAILURE);
      }

      char *tmp_mac = (char *)malloc(sizeof(char) * MAC_STR_MAX_SIZE);
      memset(tmp_mac, '\0', MAC_STR_MAX_SIZE);
      tmp_mac = get_mac_address_from_socket(notif_server_sock, &notif_client_addr, NULL);

      BOOL build_success = build_ssdp_message(ssdp_message, tmp_ip, tmp_mac, recvLen, response);
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
          to_xml(ssdp_message, TRUE, conf.fetch_info, FALSE, xml_string, XML_BUFFER_SIZE);
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
    free_stuff();
    PRINT_DEBUG("scan_for_upnp_devices end");
    exit(EXIT_SUCCESS);
  } // scan_for_upnp_devices end

  if(!conf.listen_for_upnp_notif) {
    usage();
  }

  free_stuff();
  exit(EXIT_SUCCESS);
} // main end

static void exitSig(int param) {
  free_stuff();
  exit(EXIT_SUCCESS);
}

/**
* Returns the appropriate unsigned char (number) representation of the header string
*
* @param const char *header_string The header string to be looked up
*
* @return unsigned char A unsigned char representing the header type
*/
static unsigned char get_header_type(const char *header_string) {
  int headers_size;
  int header_string_length = 0;
  char *header_lower = NULL;
  const char *header_strings[] = {
    SSDP_HEADER_UNKNOWN_STR,
    SSDP_HEADER_HOST_STR,
    SSDP_HEADER_ST_STR,
    SSDP_HEADER_MAN_STR,
    SSDP_HEADER_MX_STR,
    SSDP_HEADER_CACHE_STR,
    SSDP_HEADER_LOCATION_STR,
    SSDP_HEADER_HOST_STR,
    SSDP_HEADER_OPT_STR,
    SSDP_HEADER_01NLS_STR,
    SSDP_HEADER_NT_STR,
    SSDP_HEADER_NTS_STR,
    SSDP_HEADER_SERVER_STR,
    SSDP_HEADER_XUSERAGENT_STR,
    SSDP_HEADER_USN_STR
  };
  headers_size = sizeof(header_strings)/sizeof(char *);

  header_string_length = strlen(header_string);
  if(header_string_length < 1) {
    PRINT_ERROR("Erroneous header string detected");
    return (unsigned char)SSDP_HEADER_UNKNOWN;
  }
  header_lower = (char *)malloc(sizeof(char) * (header_string_length + 1));

  memset(header_lower, '\0', sizeof(char) * (header_string_length + 1));

  int i;
  for(i = 0; header_string[i] != '\0'; i++){
    header_lower[i] = tolower(header_string[i]);
  }

  for(i=0; i<headers_size; i++) {
    if(strcmp(header_lower, header_strings[i]) == 0) {
      free(header_lower);
      return (unsigned char)i;
    }
  }
  free(header_lower);
  return (unsigned char)SSDP_HEADER_UNKNOWN;
}

/**
* Returns the appropriate string representation of the header type
*
* @param const unsigned int header_type The header type (int) to be looked up
*
* @return char * A string representing the header type
*/
static const char *get_header_string(const unsigned int header_type, const ssdp_header_s *header) {
  const char *header_strings[] = {
    SSDP_HEADER_UNKNOWN_STR,
    SSDP_HEADER_HOST_STR,
    SSDP_HEADER_ST_STR,
    SSDP_HEADER_MAN_STR,
    SSDP_HEADER_MX_STR,
    SSDP_HEADER_CACHE_STR,
    SSDP_HEADER_LOCATION_STR,
    SSDP_HEADER_HOST_STR,
    SSDP_HEADER_OPT_STR,
    SSDP_HEADER_01NLS_STR,
    SSDP_HEADER_NT_STR,
    SSDP_HEADER_NTS_STR,
    SSDP_HEADER_SERVER_STR,
    SSDP_HEADER_XUSERAGENT_STR,
    SSDP_HEADER_USN_STR
  };

  if((header_type == 0 || header_type > (sizeof(header_strings)/sizeof(char *) - 1)) && header != NULL && header->unknown_type != NULL) {
    return header->unknown_type;
  }

  return header_strings[header_type];
}

/**
* Parse a single SSDP header
*
* @param ssdp_header_s *header The location where the parsed result should be stored
* @param const char *raw_header The header string to be parsed
*/
static void build_ssdp_header(ssdp_header_s *header, const char *raw_header) {
/*
[172.26.150.15][458B] "NOTIFY * HTTP/1.1
HOST: 239.255.255.250:1900
CACHE-CONTROL: max-age=1800
LOCATION: http://172.26.150.15:49154/rootdesc1.xml
OPT: "http://schemas.upnp.org/upnp/1/0/"; ns=01
01-NLS: 1966d9e6-1dd2-11b2-aa65-a2d9092ea049
NT: urn:axis-com:service:BasicService:1
NTS: ssdp:alive
SERVER: Linux/3.4.0, UPnP/1.0, Portable SDK for UPnP devices/1.6.18
X-User-Agent: redsonic
USN: uuid:Upnp-BasicDevice-1_0-00408C184D0E::urn:axis-com:service:BasicService:1

"
*/
  int header_name_length = -1, header_contents_length = -1;
  int raw_header_length = 0;
  char *header_name;

  header_name_length = strpos(raw_header, ":");
  if(header_name_length < 1) {
    return;
  }
  header_name = (char *)malloc(sizeof(char) * (header_name_length + 1));
  memset(header_name, '\0', sizeof(char) * (header_name_length + 1));
  strncpy(header_name, raw_header, header_name_length);

  header->type = (unsigned char)get_header_type(header_name);
  if(header->type == SSDP_HEADER_UNKNOWN) {
    header->unknown_type = (char *)malloc(sizeof(char) * header_name_length + 1);
    memset(header->unknown_type, '\0', header_name_length + 1);
    strcpy(header->unknown_type, header_name);
  }
  else {
    header->unknown_type = NULL;
  }

  raw_header_length = strlen(raw_header);
  header_contents_length = raw_header_length - header_name_length + 2;
  header->contents = (char *)malloc(sizeof(char) * header_contents_length);
  memset(header->contents, '\0', sizeof(char) * header_contents_length);
  strcpy(header->contents, &raw_header[header_name_length + 1 + (raw_header[header_name_length + 2] == ' ' ? 1 : 0)]); // avoid ": " in header contents
  free(header_name);
}

/**
* Parse a SSDP message
*
* @param ssdp_message_s *message The location where the parsed result should be stored
* @param const char *ip The IP address of the sender
* @param const char *mac The MAC address of the sender
* @param const const int *message_length The message length
* @param const char *raw_message The message string to be parsed
*/
static BOOL build_ssdp_message(ssdp_message_s *message, char *ip, char *mac, int message_length, const char *raw_message) {
  char *raw_header;
  int newline = 0, last_newline = 0;
  int raw_message_left = strlen(raw_message);
  time_t t;

  t = time(NULL);
  strftime(message->datetime, 20, "%Y-%m-%d %H:%M:%S", localtime(&t));

  if(mac) {
    strncpy(message->mac, mac, MAC_STR_MAX_SIZE);
  }

  if(ip) {
    strncpy(message->ip, ip, IPv6_STR_MAX_SIZE);
  }
  message->message_length = message_length;

  /* find end of request string */
  last_newline = strpos(raw_message, "\r\n");
  if(last_newline < 0) {
    PRINT_DEBUG("build_ssdp_message() failed: last_newline < 0");
    return FALSE;
  }
  PRINT_DEBUG("last_newline: %d", last_newline);

  /* get past request string, point at first header row */
  last_newline += 2;

  /* save request string and protocol */
  /* TODO: make it search and save
  * http-answer if present too
  * eg. in "HTTP/1.1 200 OK",
  * see if next is not newline and space
  * and then save all the way to newline.
  */
  newline = strpos(raw_message, "HTTP");
  if(newline < 0) {
    free(message->datetime);
    PRINT_DEBUG("build_ssdp_message() failed: newline < 0");
    return FALSE;
  }
  strncpy(message->request, raw_message, (!newline? newline : newline - 1));
  strncpy(message->protocol, &raw_message[newline], last_newline - 2 - newline);

  /* allocate starting header heap */
  message->headers = (ssdp_header_s *)malloc(sizeof(ssdp_header_s));
  message->headers->first = message->headers;

  BOOL has_next_header = TRUE;

  do {
    int pos;

    /* find where new header ends in raw message */
    pos = strpos(&raw_message[last_newline], "\r\n");
    if(pos < 0) {
      free(message->datetime);
      PRINT_DEBUG("build_ssdp_message() failed: pos < 0");
      return FALSE;
    }
    newline = last_newline + pos;
    /* allocate new raw header (header name, contents and '\0') heap */
    raw_header = (char *)malloc(sizeof(char) * (newline - last_newline + 1));
    /* fill it with row from raw_message */
    memset(raw_header, '\0', (newline - last_newline + 1));
    strncpy(raw_header, &raw_message[last_newline], newline - last_newline);

    /* now lets go and build the header struct */
    build_ssdp_header(message->headers, raw_header);
    if(message->headers->contents) {
      message->header_count++;
    }

    free(raw_header);

    raw_message_left = strlen(&raw_message[newline]);
    if(raw_message_left < 4 || (raw_message[newline] == '\r' && raw_message[newline + 1] == '\n' &&
      raw_message[newline + 2] == '\r' && raw_message[newline + 3] == '\n')) {
      has_next_header = FALSE;
    }

    /* if there is another header */
    if(has_next_header) {
      /* make newline last_newline and skip "\r\n"*/
      last_newline = newline + 2;
      /* if this header was successfully created */
      if(message->headers->contents) {
        /* allocate next header heap */
        message->headers->next = (ssdp_header_s *)malloc(sizeof(ssdp_header_s));
        memset(message->headers->next, '\0', sizeof(ssdp_header_s));
        /* pass over first header pointer */
        message->headers->next->first = message->headers->first;
        /* and move to it */
        message->headers = message->headers->next;
      }
    }
    else {
      message->headers->next = NULL;
    }

  } while(has_next_header);

  /* reset chain (linked list) */
  message->headers = message->headers->first;

  return TRUE;

}

/**
* Fins the position of a string in a string (or char in a string)
*
* @return int The position of the string/char in the searched string
*/
static int strpos(const char *haystack, const char *needle) {
  char *p = strstr(haystack, needle);
  if(p) {
    return p-haystack;
  }
  return -1;
}

/**
* Counts how many times a string is present in another string
*
* @param const char * haystack The string to be searched in
* @param const char * needle The string to be searched for
*
* @return unsigned char The number of times needle is preent in haystack
*/
static unsigned char strcount(const char *haystack, const char *needle) {
  unsigned char count = 0;
  const char *pos = haystack;

  do {
    pos = strstr(pos, needle);
    if(pos != NULL) {
      count++;
      pos++;
    }
  } while(pos != NULL);

  return count;

}

// TODO: remove port, it is contained in **pp_address
// TODO: make it not use global variables (conf.interface, conf.ip)
static int send_stuff(const char *url, const char *data, const struct sockaddr_storage *da, int port, int timeout) {

  if(url == NULL || strlen(url) < 1) {
    PRINT_ERROR("send_stuff(): url not set");
    return FALSE;
  }

  /* Create socket */
  PRINT_DEBUG("send_stuff(): creating socket");
  SOCKET send_sock = setup_socket(conf.use_ipv6,
            FALSE,
            FALSE,
            conf.interface,
            conf.ip,
            NULL,
            NULL,
            0,
            FALSE,
            FALSE);

  if(send_sock == SOCKET_ERROR) {
    PRINT_ERROR("send_stuff(): %d, %s", errno, strerror(errno));
    return 0;
  }

  if(!set_receive_timeout(send_sock, timeout)) {
    close(send_sock);
    return 0;
  }

  if(!set_send_timeout(send_sock, 1)) {
    close(send_sock);
    return 0;
  }

  /* Setup socket destination address */
  PRINT_DEBUG("send_stuff(): setting up socket addresses");

  if(da->ss_family == AF_INET) {
    struct sockaddr_in *da_ipv4 = (struct sockaddr_in *)da;
    da_ipv4->sin_port = htons(port);
  }
  else {
    struct sockaddr_in6 *da_ipv6 = (struct sockaddr_in6 *)da;
    da_ipv6->sin6_port = htons(port);
  }

  /* Connect socket to destination */
  #ifdef DEBUG___
  char *tmp_ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
  memset(tmp_ip, '\0', IPv6_STR_MAX_SIZE);
  inet_ntop(da->ss_family, (da->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)da)->sin_addr : (void *)&((struct sockaddr_in6 *)da)->sin6_addr), tmp_ip, IPv6_STR_MAX_SIZE);
  PRINT_DEBUG("send_stuff(): connecting to destination (%s; ss_family = %s [%d])", tmp_ip, (da->ss_family == AF_INET ? "AF_INET" : "AF_INET6"), da->ss_family);
  free(tmp_ip);
  tmp_ip = NULL;
  #endif
  if(connect(send_sock, (struct sockaddr*)da, sizeof(struct sockaddr)) == SOCKET_ERROR) {
    PRINT_ERROR("send_stuff(): connect(): %d, %s", errno, strerror(errno));
    close(send_sock);
    return 0;
  }

  /*
  POST </path/file.html> HTTP/1.0\r\n
  Host: <own_ip>\r\n
  User-Agent: abused-<X>\r\n
  \r\n
  */
  char *ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
  memset(ip, '\0', IPv6_STR_MAX_SIZE);
  inet_ntop(da->ss_family, (da->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)da)->sin_addr : (void *)&((struct sockaddr_in6 *)da)->sin6_addr), ip, IPv6_STR_MAX_SIZE);

    int request_size = strlen(data) + 150;
    char *request = (char *)malloc(sizeof(char) * request_size);
  memset(request, '\0', request_size);

  int used_length = 0;
  used_length = snprintf(request + used_length,
           request_size - used_length,
           "POST %s HTTP/1.1\r\n", url);
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "Host: %s\r\n", ip);
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "Connection: close\r\n");
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "User-Agent: abused-%s\r\n", ABUSED_VERSION);
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "Content-type: text/xml\r\n");
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "Content-length: %d\r\n\r\n", (int)strlen(data));
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "%s\r\n\r\n", data);
  free(ip);

  PRINT_DEBUG("send_stuff(): sending string:\n%s", request);
  int bytes = send(send_sock, request, strlen(request), 0);
  free(request);
  if(bytes < 1) {
    PRINT_ERROR("Failed forwarding message");
    close(send_sock);
    return 0;
  }

  PRINT_DEBUG("send_stuff(): sent %d bytes", bytes);
  int response_size = 10240; // 10KB
  char *response = (char *)malloc(sizeof(char) * response_size);
  memset(response, '\0', response_size);

  int all_bytes = 0;
  do {
    bytes = recv(send_sock, response + all_bytes, response_size - all_bytes, 0);
    all_bytes += bytes;
  } while(bytes > 0);

  PRINT_DEBUG("send_stuff(): received %d bytes", all_bytes);
  PRINT_DEBUG("send_stuff(): received:\n%s", response);

  free(response);
  PRINT_DEBUG("send_stuff(): closing socket");
  close(send_sock);
  return all_bytes;
}

/**
* Frees all neccessary allocations in a ssdp_message_s
*
* @param ssdp_message_s *message The message to free allocations for
*/
static void free_ssdp_message(ssdp_message_s **message_pointer) {
  ssdp_header_s *next_header = NULL;
  ssdp_custom_field_s *next_custom_field = NULL;

  if(!message_pointer || !*message_pointer) {
    PRINT_ERROR("Message was empty, nothing to free");
    return;
  }

  ssdp_message_s *message = *message_pointer;

  if(message->mac != NULL) {
    free(message->mac);
    message->mac = NULL;
  }

  if(message->ip != NULL) {
    free(message->ip);
    message->ip = NULL;
  }

  if(message->datetime != NULL) {
    free(message->datetime);
    message->datetime = NULL;
  }

  if(message->request != NULL) {
    free(message->request);
    message->request = NULL;
  }

  if(message->protocol != NULL) {
    free(message->protocol);
    message->protocol = NULL;
  }

  if(message->answer != NULL) {
    free(message->answer);
    message->answer = NULL;
  }

  if(message->info != NULL) {
    free(message->info);
    message->info = NULL;
  }

  do {

    if(message->headers->contents != NULL) {
      free(message->headers->contents);
      message->headers->contents = NULL;
    }

    if(message->headers->unknown_type != NULL) {
      free(message->headers->unknown_type);
      message->headers->unknown_type = NULL;
    }

    next_header = message->headers->next;
    free(message->headers);
    message->headers = next_header;
    next_header = NULL;

  } while(message->headers);

  while(message->custom_fields) {

    if(message->custom_fields->name != NULL) {
      free(message->custom_fields->name);
      message->custom_fields->name = NULL;
    }

    if(message->custom_fields->contents != NULL) {
      free(message->custom_fields->contents);
      message->custom_fields->contents = NULL;
    }

    next_custom_field = message->custom_fields->next;
    free(message->custom_fields);
    message->custom_fields = next_custom_field;
    next_custom_field = NULL;

  };

  free(message);
  *message_pointer = NULL;
  
}

/**
* Parses the filter argument
*
* @param const char *raw_filter The raw filter string
* @param const unsigned char filters_count The number of filters found
* @param const char **filters The parsed filters array
*/
static void parse_filters(char *raw_filter, filters_factory_s **filters_factory, BOOL print_filters) {
  char *pos = NULL, *last_pos = raw_filter, *splitter = NULL;
  unsigned char filters_count = 0;
  filters_factory_s *ff = NULL;

  if(raw_filter == NULL || strlen(raw_filter) < 1) {
    if(print_filters) {
      printf("No filters applied.\n");
    }
    return;
  }

  /* Get rid of leading and trailing ',' and '=' */
  while(*last_pos == ',' || *last_pos == '=') {
    last_pos++;
  }

  while(*(last_pos + strlen(last_pos) - 1) == ',' || *(last_pos + strlen(last_pos) - 1) == '=') {
    memcpy((last_pos + strlen(last_pos) - 1), "\0", 1);
  }

  /* Number of filters (filter delimiters) found */
  filters_count = strcount(last_pos, ",") + 1;

  /* Create filter factory (a container for global use) */
  *filters_factory = (filters_factory_s *)malloc(sizeof(filters_factory_s));

  /* Simplify access name and initialize */
  ff = *filters_factory;
  memset(ff, '\0', sizeof(filters_factory_s));
  ff->filters_count = filters_count;

  /* Create place for all the filters */
  ff->filters = (filter_s *)malloc(sizeof(filter_s) * filters_count);
  memset(ff->filters, '\0', sizeof(filter_s) * filters_count);

  int fc;
  for(fc = 0; fc < filters_count; fc++) {

    /* Header name which value to apply filter on */
    ff->filters[fc].header = (char *)malloc(sizeof(char) * 64);
    memset((ff->filters[fc]).header, '\0', sizeof(char) * 64);

    /* Header value to filter on */
    ff->filters[fc].value = (char *)malloc(sizeof(char) * 2048);
    memset(ff->filters[fc].value, '\0', sizeof(char) * 2048);

    /* Find filter splitter (',') */
    pos = strstr(last_pos, ",");
    if(pos == NULL) {
      pos = last_pos + strlen(last_pos);
    }

    /* Find name and value splitter ('=') */
    splitter = strstr(last_pos, "=");
    if(splitter > 0) {
      strncpy(ff->filters[fc].header, last_pos, splitter - last_pos);
      splitter++;
      strncpy(ff->filters[fc].value, splitter, pos - splitter);
    }
    else {
      strncpy(ff->filters[fc].header, last_pos, pos - last_pos);
    }
    last_pos = pos + 1;

  }

  if(print_filters) {
    printf("\nFilters applied:\n");
    int c;
    for(c = 0; c < filters_count; c++) {
      printf("%d: %s = %s\n", c, ff->filters[c].header, ff->filters[c].value);
    }
  }
}

/**
* Get the remote MAC address from a given sock
*
* @param sock The socket to extract the MAC address from
*
* @return int The remote MAC address as a string
*/
// TODO: fix for IPv6
static char *get_mac_address_from_socket(const SOCKET sock, struct sockaddr_storage *sa_ip, char *ip) {
  char *mac_string = (char *)malloc(sizeof(char) * MAC_STR_MAX_SIZE);
  memset(mac_string, '\0', MAC_STR_MAX_SIZE);

  #if defined BSD || defined APPLE
  /* xxxBSD or MacOS solution */
  PRINT_DEBUG("Using BSD style MAC discovery (sysctl)");
  int sysctl_flags[] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO };
  char *arp_buffer = NULL;
  char *arp_buffer_end = NULL;
  char *arp_buffer_next = NULL;
  struct rt_msghdr *rtm = NULL;
  struct sockaddr_inarp *sin_arp = NULL;
  struct sockaddr_dl *sdl = NULL;
  size_t arp_buffer_size;
  BOOL found_arp = FALSE;
  struct sockaddr_storage *tmp_sin = NULL;

  /* Sanity check, choose IP source */
  if(NULL == sa_ip && NULL != ip) {
    tmp_sin = (struct sockaddr_storage *)malloc(sizeof(struct sockaddr_storage));
    memset(tmp_sin, '\0', sizeof(struct sockaddr_storage));
    if(!inet_pton(sa_ip->ss_family, ip, (sa_ip->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)sa_ip)->sin_addr : (void *)&((struct sockaddr_in6 *)sa_ip)->sin6_addr))) {
      PRINT_ERROR("get_mac_address_from_socket(): Failed to get MAC from given IP (%s)", ip);
      free(mac_string);
      free(tmp_sin);
      return NULL;
    }
    sa_ip = tmp_sin;
  }
  else if(NULL == sa_ip) {
    PRINT_ERROR("Neither IP nor SOCKADDR given, MAC not fetched");
    free(mac_string);
    return NULL;
  }

  /* See how much we need to allocate */
  if(sysctl(sysctl_flags, 6, NULL, &arp_buffer_size, NULL, 0) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): sysctl(): Could not determine neede size");
    free(mac_string);
    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
    return NULL;
  }

  /* Allocate needed memmory */
  if((arp_buffer = malloc(arp_buffer_size)) == NULL) {
    PRINT_ERROR("get_mac_address_from_socket(): Failed to allocate memory for the arp table buffer");
    free(mac_string);
    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
    return NULL;
  }

  /* Fetch the arp table */
  if(sysctl(sysctl_flags, 6, arp_buffer, &arp_buffer_size, NULL, 0) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): sysctl(): Could not retrieve arp table");
    free(arp_buffer);
    free(mac_string);
    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
    return NULL;
  }

  /* Loop through the arp table/list */
  arp_buffer_end = arp_buffer + arp_buffer_size;
  for(arp_buffer_next = arp_buffer; arp_buffer_next < arp_buffer_end; arp_buffer_next += rtm->rtm_msglen) {

    /* See it through another perspective */
    rtm = (struct rt_msghdr *)arp_buffer_next;

    /* Skip to the address portion */
    sin_arp = (struct sockaddr_inarp *)(rtm + 1);
    sdl = (struct sockaddr_dl *)(sin_arp + 1);

    /* Check if address is the one we are looking for */
    if(((struct sockaddr_in *)sa_ip)->sin_addr.s_addr != sin_arp->sin_addr.s_addr) {
      continue;
    }
    found_arp = TRUE;

    /* The proudly save the MAC to a string */
    if (sdl->sdl_alen) {
      unsigned char *cp = (unsigned char *)LLADDR(sdl);
      sprintf(mac_string, "%x:%x:%x:%x:%x:%x", cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
    }

    free(arp_buffer);

    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
  }
  #else
  /* Linux (and rest) solution */
  PRINT_DEBUG("Using Linux style MAC discovery (ioctl SIOCGARP)");
  struct arpreq arp;
  unsigned char *mac = NULL;

  memset(&arp, '\0', sizeof(struct arpreq));
  sa_family_t ss_fam = AF_INET;
  struct in_addr *sin_a = NULL;
  struct in6_addr *sin6_a = NULL;
  BOOL is_ipv6 = FALSE;

  /* Sanity check */
  if(sa_ip == NULL && ip == NULL){
    PRINT_ERROR("Neither IP string nor sockaddr given, MAC not fetched");
    free(mac_string);
    return NULL;
  }

  /* Check if IPv6 */
  struct sockaddr_in6 *tmp = (struct sockaddr_in6 *)malloc(sizeof(struct sockaddr_in6));
  if((sa_ip != NULL && sa_ip->ss_family == AF_INET6)
  || (ip != NULL && inet_pton(AF_INET6, ip, tmp))) {
    is_ipv6 = TRUE;
    ss_fam = AF_INET6;
    PRINT_DEBUG("Looking for IPv6-MAC association");
  }
  else if((sa_ip != NULL && sa_ip->ss_family == AF_INET)
  || (ip != NULL && inet_pton(AF_INET, ip, tmp))) {
    PRINT_DEBUG("Looking for IPv4-MAC association");
  }
  else {
    PRINT_ERROR("sa_ip or ip variable error");
  }
  free(tmp);
  tmp = NULL;

  /* Assign address family for arpreq */
  ((struct sockaddr_storage *)&arp.arp_pa)->ss_family = ss_fam;

  /* Point the needed pointer */
  if(!is_ipv6){
    sin_a = &((struct sockaddr_in *)&arp.arp_pa)->sin_addr;
  }
  else if(is_ipv6){
    sin6_a = &((struct sockaddr_in6 *)&arp.arp_pa)->sin6_addr;
  }

  /* Fill the fields */
  if(sa_ip != NULL) {
    if(ss_fam == AF_INET) {
      *sin_a = ((struct sockaddr_in *)sa_ip)->sin_addr;
    }
    else {
      *sin6_a = ((struct sockaddr_in6 *)sa_ip)->sin6_addr;
    }
  }
  else if(ip != NULL) {

    if(!inet_pton(ss_fam,
        ip,
        (ss_fam == AF_INET ? (void *)sin_a: (void *)sin6_a))) {
    PRINT_ERROR("Failed to get MAC from given IP (%s)", ip);
    PRINT_ERROR("Error %d: %s", errno, strerror(errno));
    free(mac_string);
    return NULL;
    }
  }

  /* Go through all interfaces' arp-tables and search for the MAC */
  /* Possible explanation: Linux arp-tables are if-name specific */
  struct ifaddrs *interfaces = NULL, *ifa = NULL;
  if(getifaddrs(&interfaces) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): getifaddrs():Could not retrieve interfaces");
    free(mac_string);
    return NULL;
  }

  /* Start looping through the interfaces*/
  for(ifa = interfaces; ifa && ifa->ifa_next; ifa = ifa->ifa_next) {

    /* Skip the loopback interface */
    if(strcmp(ifa->ifa_name, "lo") == 0) {
      PRINT_DEBUG("Skipping interface 'lo'");
      continue;
    }

    /* Copy current interface name into the arpreq structure */
    PRINT_DEBUG("Trying interface '%s'", ifa->ifa_name);
    strncpy(arp.arp_dev, ifa->ifa_name, 15);

    ((struct sockaddr_storage *)&arp.arp_ha)->ss_family = ARPHRD_ETHER;

    /* Ask for thE arp-table */
    if(ioctl(sock, SIOCGARP, &arp) < 0) {

      /* Handle failure */
      if(errno == 6) {
        /* if error is "Unknown device or address" then continue/try next interface */
        PRINT_DEBUG("get_mac_address_from_socket(): ioctl(): (%d) %s", errno, strerror(errno));
        continue;
      }
      else {
        PRINT_ERROR("get_mac_address_from_socket(): ioctl(): (%d) %s", errno, strerror(errno));
        continue;
      }
    }

    mac = (unsigned char *)&arp.arp_ha.sa_data[0];

  }

  freeifaddrs(interfaces);

  if(!mac) {
    PRINT_DEBUG("mac is NULL");
    free(mac_string);
    return NULL;
  }

  sprintf(mac_string, "%x:%x:%x:%x:%x:%x", *mac, *(mac + 1), *(mac + 2), *(mac + 3), *(mac + 4), *(mac + 5));
  mac = NULL;
  #endif

  PRINT_DEBUG("Determined MAC string: %s", mac_string);
  return mac_string;
}

/**
 * Fetches additional info from a UPnP message "Location" header
 * and stores it in the custom_fields in the ssdp_message
 *
 * @param ssdp_message_s * The message whos "Location" header to use
 *
 * @return The number of bytes received
 */
static int fetch_custom_fields(ssdp_message_s *ssdp_message) {
  int bytes_received = 0;
  char *location_header = NULL;
  ssdp_header_s *ssdp_headers = ssdp_message->headers;
  while(ssdp_headers) {
    if(ssdp_headers->type == SSDP_HEADER_LOCATION) {
      location_header = ssdp_headers->contents;
      // Ex. location_header:
      //http://10.83.128.46:2869/upnphost/udhisapi.dll?content=uuid:59e293c8-9179-4efb-ac32-3c9514238505
      PRINT_DEBUG("found header_location: %s", location_header);
    }
    ssdp_headers = ssdp_headers->next;
  }
  ssdp_headers = NULL;

  if(location_header != NULL) {

    /* URL parsing allocations*/
    PRINT_DEBUG("allocating for URL parsing");
    char *ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
    int port = 0;
    char *rest = (char *)malloc(sizeof(char) * 256);
    char *request = (char *)malloc(sizeof(char) * 1024); // 1KB
    char *response = (char *)malloc(sizeof(char) * DEVICE_INFO_SIZE); // 8KB
    memset(ip, '\0', IPv6_STR_MAX_SIZE);
    memset(rest, '\0', 256);
    memset(request, '\0', 1024);
    memset(response, '\0', DEVICE_INFO_SIZE);

    /* Try to parse the location_header URL */
    PRINT_DEBUG("trying to parse URL");
    if(parse_url(location_header, ip, IPv6_STR_MAX_SIZE, &port, rest, 256)) {

      /* Create socket */
      PRINT_DEBUG("creating socket");
      SOCKET resolve_sock = setup_socket(conf.use_ipv6,
                 FALSE,
                 FALSE,
                 conf.interface,
                 conf.ip,
                 NULL,
                 NULL,
                 0,
                 FALSE,
                 FALSE);

      if(resolve_sock == SOCKET_ERROR) {
        PRINT_ERROR("fetch_custom_fields(); setup_socket(): (%d) %s", errno, strerror(errno));
        free(ip);
        free(rest);
        free(request);
        free(response);
        return 0;
      }

      if(!set_receive_timeout(resolve_sock, 5)) {
        free(ip);
        close(resolve_sock);
        free(rest);
        free(request);
        free(response);
        return 0;
      }

      if(!set_send_timeout(resolve_sock, 1)) {
        free(ip);
        close(resolve_sock);
        free(rest);
        free(request);
        free(response);
        return 0;
      }

      /* Setup socket destination address */
      PRINT_DEBUG("setting up socket addresses");
      struct sockaddr_storage *da = (struct sockaddr_storage *)malloc(sizeof(struct sockaddr_storage));
      memset(da, 0, sizeof(struct sockaddr_storage));
      da->ss_family = conf.use_ipv6 ? AF_INET6 : AF_INET;
      if(!inet_pton(da->ss_family, ip, (da->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)da)->sin_addr : (void *)&((struct sockaddr_in6 *)da)->sin6_addr))) {
        int ip_length = strlen(ip);
        if(ip_length < 1) {
          PRINT_ERROR("The destination IP address could be determined (%s)\n", (ip_length < 1 ? "empty" : ip));
        }
        free(ip);
        close(resolve_sock);
        free(rest);
        free(request);
        free(response);
        free(da);
        return 0;
      }

      if(da->ss_family == AF_INET) {
        struct sockaddr_in *da_ipv4 = (struct sockaddr_in *)da;
        da_ipv4->sin_port = htons(port);
      }
      else {
        struct sockaddr_in6 *da_ipv6 = (struct sockaddr_in6 *)da;
        da_ipv6->sin6_port = htons(port);
      }

      /* Connect socket to destination */
      #ifdef DEBUG___
      char *tmp_ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
      memset(tmp_ip, '\0', IPv6_STR_MAX_SIZE);
      inet_ntop(da->ss_family, (da->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)da)->sin_addr : (void *)&((struct sockaddr_in6 *)da)->sin6_addr), tmp_ip, IPv6_STR_MAX_SIZE);
      PRINT_DEBUG("connecting to destination (%s; ss_family = %s [%d])", tmp_ip, (da->ss_family == AF_INET ? "AF_INET" : "AF_INET6"), da->ss_family);
      free(tmp_ip);
      tmp_ip = NULL;
      #endif
      if(connect(resolve_sock, (struct sockaddr*)da, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        PRINT_ERROR("fetch_custom_fields(); connect(): (%d) %s", errno, strerror(errno));
        free(ip);
        close(resolve_sock);
        free(rest);
        free(request);
        free(response);
        free(da);
        return 0;
      }

      free(da);
      int used_length = 0;
      /*
      GET </path/file.html> HTTP/1.0\r\n
      Host: <own_ip>\r\n
      User-Agent: abused-<X>\r\n
      \r\n
      */
      used_length += snprintf(request + used_length, 1024 - used_length, "GET %s HTTP/1.0\r\n", rest);
      used_length += snprintf(request + used_length, 1024 - used_length, "Host: %s\r\n", ip);
      snprintf(request + used_length, 1024 - used_length, "User-Agent: abused-%s\r\n\r\n", ABUSED_VERSION);
      PRINT_DEBUG("sending string:\n%s", request);
      int bytes = send(resolve_sock, request, strlen(request), 0);
      PRINT_DEBUG("sent %d bytes", bytes);
      do {
        bytes = 0;
        bytes = recv(resolve_sock, response + bytes_received, DEVICE_INFO_SIZE - bytes_received, 0);
        bytes_received += bytes;
      } while(bytes > 0);
      PRINT_DEBUG("received %d bytes", bytes_received);
      PRINT_DEBUG("%s", response);
      PRINT_DEBUG("closing socket");
      close(resolve_sock);
      free(ip);
      free(rest);
      free(request);

      /* Init the ssdp_custom_field struct */
      int i;
      char *tmp_pointer = NULL;
      int buffer_size = 0;

      const char *field[] = {
        "serialNumber",
        "friendlyName",
        "manufacturer",
        "manufacturerURL",
        "modelName",
        "modelNumber",
        "modelURL"
      };
      int fields_size = sizeof(field) / sizeof(char *);
      PRINT_DEBUG("fields_size: %d", fields_size);

      for(i = 0; i < fields_size; i++) {

        /* Create a new ssdp_custom_field_s */
        ssdp_custom_field_s *cf = (ssdp_custom_field_s *)malloc(sizeof(ssdp_custom_field_s));
        memset(cf, 0, sizeof(ssdp_custom_field_s));

        /* Parse 'name' and 'contents' */
        int field_length = strlen(field[i]);

        cf->name = (char *)malloc(sizeof(char) * field_length + 1);
        strcpy(cf->name, field[i]);

        char needle[field_length + 4];
        sprintf(needle, "<%s>", field[i]);
        tmp_pointer = strstr(response, needle);

        if(tmp_pointer) {
          sprintf(needle, "</%s>", field[i]);
          buffer_size = (int)(strstr(response, needle) - (tmp_pointer + field_length + 2) + 1);
          cf->contents = (char *)malloc(sizeof(char) * buffer_size);
          memset(cf->contents, '\0', buffer_size);
          strncpy(cf->contents, (tmp_pointer + field_length + 2), buffer_size - 1);
          PRINT_DEBUG("Found expected custom field (%d) '%s' with value '%s'",
                      ssdp_message->custom_field_count,
                      cf->name,
                      cf->contents);
        }
        else {
          PRINT_DEBUG("Expected custom field '%s' is missing", field[i]);
        }

        /* If it is the first one then set this as the
           start and set 'first' to it */
        if(!ssdp_message->custom_fields) {
          cf->first = cf;
        }
        /* Else set 'first' to previous 'first' 
           and this one as 'next' */
        else {
          cf->first = ssdp_message->custom_fields->first;
          ssdp_message->custom_fields->next = cf;
        }

        /* Add the custom field array to the ssdp_message */
        ssdp_message->custom_fields = cf;

        /* Tell ssdp_message that we added one ssdp_custom_field_s */
        ssdp_message->custom_field_count++;

      }

      /* End the linked list and reset the pointer to the beginning */
      ssdp_message->custom_fields->next = NULL;
      ssdp_message->custom_fields = ssdp_message->custom_fields->first;

    }

    free(response);
  }

  return bytes_received;
}

/**
* Converts a UPnP message to a JSON string
*
* @param ssdp_message The message to be converted
* @param full_json Whether to contain the JSON opening hash
* @param fetch_info Whether to follow the Location header to fetch additional data
* @param hide_headers Whether to include the ssdp headers in the resulting JSON document
* @param json_buffer The JSON buffer to write the JSON document to
* @param json_size The size of the passed buffer (json_buffer)
*/
static unsigned int to_json(const ssdp_message_s *ssdp_message,
                            BOOL full_json,
                            BOOL fetch_info,
                            BOOL hide_headers,
                            char *json_buffer,
                            int json_buffer_size) {
  int used_length = 0;

  // TODO: write it!

  return used_length;
}

/**
* Converts a UPnP message to a XML string
*
* @param ssdp_message The message to be converted
* @param full_xml Whether to contain the XML declaration and the root tag
* @param fetch_info Whether to follow the Location header to fetch additional data
* @param hide_headers Whether to include the ssdp headers in the resulting XML document
* @param xml_buffer The XML buffer to write the XML document to
* @param xml_buffer_size The size of the passed buffer (xml_buffer)
*/
static unsigned int to_xml(const ssdp_message_s *ssdp_message,
                           BOOL full_xml,
                           BOOL fetch_info,
                           BOOL hide_headers,
                           char *xml_buffer,
                           int xml_buffer_size) {
  int used_length = 0;

  if(xml_buffer == NULL) {
    PRINT_ERROR("to_xml(): No XML message buffer specified");
  }
  else if(ssdp_message == NULL) {
    PRINT_ERROR("to_xml(): No SSDP message specified");
  }

  memset(xml_buffer, '\0', sizeof(char) * xml_buffer_size);
  if(full_xml) {
    used_length = snprintf(xml_buffer, xml_buffer_size,
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<root>\n");
  }
  used_length += snprintf(xml_buffer + used_length, xml_buffer_size - used_length,
  "\t<message length=\"%d\">\n", ssdp_message->message_length);
  used_length += snprintf(xml_buffer + used_length, xml_buffer_size - used_length,
  "\t\t<mac>\n\t\t\t%s\n\t\t</mac>\n", ssdp_message->mac);
  used_length += snprintf(xml_buffer + used_length, xml_buffer_size - used_length,
  "\t\t<ip>\n\t\t\t%s\n\t\t</ip>\n", ssdp_message->ip);
  used_length += snprintf(xml_buffer + used_length, xml_buffer_size - used_length,
  "\t\t<request protocol=\"%s\">\n\t\t\t%s\n\t\t</request>\n", ssdp_message->protocol, ssdp_message->request);
  used_length += snprintf(xml_buffer + used_length, xml_buffer_size - used_length,
  "\t\t<datetime>\n\t\t\t%s\n\t\t</datetime>\n", ssdp_message->datetime);

  ssdp_custom_field_s *cf = ssdp_message->custom_fields->first;
  int needed_size = 0;

  /* Calculate needed buffer size */
  while(cf) {
    // 13 + <custom_field name=""></custom_field> = 50 bytes
    needed_size += strlen(cf->name) + strlen(cf->contents) + 50;
    cf = cf->next;
  }
  cf = ssdp_message->custom_fields->first;

  /* Check if buffer has enought room */
  PRINT_DEBUG("buffer left: %d, buffer_needed: %d", (xml_buffer_size - used_length), needed_size);
  if((xml_buffer_size - used_length) < needed_size) {
    PRINT_ERROR("to_xml(): Not enought memory in the buffer to convert the SSDP message to XML, skipping\n");
  }
  else {

    /* Convert to XML format */
    used_length += snprintf(xml_buffer + used_length, xml_buffer_size - used_length,
    "\t\t<custom_fields count=\"%d\">\n", ssdp_message->custom_field_count);

    while(cf) {
      used_length += snprintf(xml_buffer + used_length,
                              xml_buffer_size - used_length,
                              "\t\t\t<custom_field name=\"%s\">\n\t\t\t\t%s\n\t\t\t</custom_field>\n",
                              cf->name,
                              cf->contents);
      cf = cf->next;
    }

    cf = NULL;
    used_length += snprintf(xml_buffer + used_length,
                            xml_buffer_size - used_length,
                            "\t\t</custom_fields>\n");
    PRINT_DEBUG("XML custom-fields bytes: %d", (int)strlen(xml_buffer));
  }

  if(!hide_headers) {
    ssdp_header_s *ssdp_headers = ssdp_message->headers;

    used_length += snprintf(xml_buffer + used_length, xml_buffer_size - used_length,
    "\t\t<headers count=\"%d\">\n", (unsigned int)ssdp_message->header_count);
    while(ssdp_headers) {
      used_length += snprintf(xml_buffer + used_length,
                              xml_buffer_size - used_length,
                              "\t\t\t<header typeInt=\"%d\" typeStr=\"%s\">\n",
                              ssdp_headers->type,
                              get_header_string(ssdp_headers->type, ssdp_headers));
      used_length += snprintf(xml_buffer + used_length,
                              xml_buffer_size - used_length,
                              "\t\t\t\t%s\n\t\t\t</header>\n",
                              ssdp_headers->contents);
      ssdp_headers = ssdp_headers->next;
    }
    ssdp_headers = NULL;
  }

  used_length += snprintf(xml_buffer + used_length, xml_buffer_size - used_length,
                          "\t\t</headers>\n\t</message>\n");
  if(full_xml) {
    used_length += snprintf(xml_buffer + used_length, xml_buffer_size - used_length,
                            "</root>\n");
  }

  return used_length;
}

// TODO: remove ip_size ?
static BOOL parse_url(const char *url, char *ip, int ip_size, int *port, char *rest, int rest_size) {

  /* Check if HTTPS */
  if(!url || strlen(url) < 8) {
    PRINT_ERROR("The argument 'url' is not set or is empty");
    return FALSE;
  }

  PRINT_DEBUG("passed url: %s", url);

  if(*(url + 4) == 's') {
    /* HTTPS is not supported at the moment, skip */
    PRINT_DEBUG("HTTPS is not supported, skipping");
    return FALSE;
  }

  const char *ip_begin = strchr(url, ':') + 3;              // http[s?]://^<ip>:<port>/<rest>
  PRINT_DEBUG("ip_begin: %s", ip_begin);
  BOOL is_ipv6 = *ip_begin == '[' ? TRUE : FALSE;
  PRINT_DEBUG("is_ipv6: %s", (is_ipv6 ? "TRUE" : "FALSE"));

  char *rest_begin = NULL;
  if(is_ipv6) {
    if(!(rest_begin = strchr(ip_begin, ']')) || !(rest_begin = strchr(rest_begin, '/'))) {
      // [<ipv6>]^:<port>/<rest>             && :<port>^/<rest>
      PRINT_ERROR("Error: (IPv6) rest_begin is NULL\n");
      return FALSE;
    }
    PRINT_DEBUG("rest_begin: %s", rest_begin);
  }
  else {
    if(!(rest_begin = strchr(ip_begin, '/'))) {                // <ip>:<port>^/<rest>
      PRINT_ERROR("Error: rest_begin is NULL\n");
      return FALSE;
    }
  }

  if(rest_begin != NULL) {
    strcpy(rest, rest_begin);
  }

  char *working_str = (char *)malloc(sizeof(char) * 256); // temporary string buffer
  char *ip_with_brackets = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE + 2); // temporary IP buffer
  memset(working_str, '\0', 256);
  memset(ip_with_brackets, '\0', IPv6_STR_MAX_SIZE + 2);
  PRINT_DEBUG("size to copy: %d", (int)(rest_begin - ip_begin));
  strncpy(working_str, ip_begin, (size_t)(rest_begin - ip_begin)); // "<ip>:<port>"
  PRINT_DEBUG("working_str: %s", working_str);
  char *port_begin = strrchr(working_str, ':');                 // "<ip>^:<port>^"
  PRINT_DEBUG("port_begin: %s", port_begin);
  if(port_begin != NULL) {
    *port_begin = ' ';                                           // "<ip>^ <port>^"
    // maybe memset instead of value assignment ?
    sscanf(working_str, "%s %d", ip_with_brackets, port);      // "<%s> <%d>"
    PRINT_DEBUG("port: %d", *port);
    if(strlen(ip_with_brackets) < 1) {
      PRINT_ERROR("Malformed header_location detected in: '%s'", url);
      free(working_str);
      free(ip_with_brackets);
      return FALSE;
    }
    if(*ip_with_brackets == '[') {
      PRINT_DEBUG("ip_with_brackets: %s", ip_with_brackets);
      //strncpy(ip, ip_with_brackets + 1, strlen(ip_with_brackets) - 1); // uncomment in case we dont need brackets
    }
    else {
      //strcpy(ip, ip_with_brackets); // uncomment in case we dont need brackets
    }
      strcpy(ip, ip_with_brackets);
      PRINT_DEBUG("ip: %s", ip);
  }

  if(rest_begin == NULL) {
    PRINT_ERROR("Malformed header_location detected in: '%s'", url);
    free(working_str);
    free(ip_with_brackets);
    return FALSE;
  }

  free(working_str);
  free(ip_with_brackets);
  return TRUE;
}

/**
* Parse a given ip:port combination into an internet address structure
*
* @param char *raw_address The <ip>:<port> string combination to be parsed
* @param sockaddr_storage ** The IP internet address structute to be allocated and filled
*
* @return BOOL TRUE on success
*/
static BOOL parse_address(const char *raw_address, struct sockaddr_storage **pp_address) {
  char *ip;
  struct sockaddr_storage *address = NULL;
  int colon_pos = 0;
  int port = 0;
  BOOL is_ipv6;

  PRINT_DEBUG("parse:address()");

  if(strlen(raw_address) < 1) {
    PRINT_ERROR("No valid IP address specified");
    return FALSE;
  }

  /* Allocate the input address */
  *pp_address = (struct sockaddr_storage *)malloc(sizeof(struct sockaddr_storage));

  /* Get rid of the "pointer to a pointer" */
  address = *pp_address;
  memset(address, 0, sizeof(struct sockaddr_storage));
  colon_pos = strpos(raw_address, ":");

  if(colon_pos < 1) {
    PRINT_ERROR("No valid port specified (%s)\n", raw_address);
    return FALSE;
  }

  ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
  memset(ip, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);

  /* Get rid of [] if IPv6 */
  strncpy(ip, strchr(raw_address, '[') + 1, strchr(raw_address, '[') - strchr(raw_address, ']'));

  is_ipv6 = inet_pton(AF_INET6, ip, address);
  PRINT_DEBUG("is_ipv6 == %s", is_ipv6 ? "TRUE" : "FALSE");
  if(is_ipv6) {
    address->ss_family = AF_INET6;
    port = atoi(strrchr(raw_address, ':') + 1);
  }
  else {
    memset(ip, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);
    strncpy(ip, raw_address, colon_pos);
    if(!inet_pton(AF_INET, ip, &(((struct sockaddr_in *)address)->sin_addr))) {
      PRINT_ERROR("No valid IP address specified (%s)\n", ip);
      return FALSE;
    }
    address->ss_family = AF_INET;
    port = atoi(raw_address + colon_pos + 1);
  }

  if(port < 80 || port > PORT_MAX_NUMBER) {
    PRINT_ERROR("No valid port specified (%d)\n", port);
    return FALSE;
  }

  if(is_ipv6) {
    ((struct sockaddr_in6 *)address)->sin6_port = htons(port);
  }
  else {
    ((struct sockaddr_in *)address)->sin_port = htons(port);
  }

  free(ip);

  return TRUE;
}

/**
* Tries to find a interface with the specified name or IP address
*
* @param struct sockaddr_storage *saddr A pointer to the buffer of the address that should be updated
* @param char * address The address or interface name we want to find
*
* @return int The interface index
*/
static int find_interface(struct sockaddr_storage *saddr, char *interface, char *address) {
  // TODO: for porting to Windows see http://msdn.microsoft.com/en-us/library/aa365915.aspx
  struct ifaddrs *interfaces, *ifa;
  struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *)saddr;
  struct sockaddr_in *saddr4 = (struct sockaddr_in *)saddr;
  char *compare_address = NULL;
  int ifindex = -1;
  BOOL is_ipv6 = FALSE;

  PRINT_DEBUG("find_interface(%s, \"%s\", \"%s\")",
              saddr == NULL ? NULL : "saddr",
              interface,
              address);

  /* Make sure we have the buffers allocated */
  if(interface == NULL) {
    PRINT_ERROR("No interface string-buffer available");
    return -1;
  }

  if(address == NULL) {
    PRINT_ERROR("No IP address string-buffer available");
    return -1;
  }

  if(saddr == NULL) {
    PRINT_ERROR("No address structure-buffer available");
    return -1;
  }

  /* Check if address is IPv6*/
  is_ipv6 = inet_pton(AF_INET6,
                      address,
                      (void *)&saddr6->sin6_addr) > 0;

  /* If address not set or is bind-on-all
     then set a bindall address in the struct */
  if(((strlen(interface) == 0) && (strlen(address) == 0)) ||
    (strcmp("0.0.0.0", address) == 0) || (strcmp("::", address) == 0)) {
    if(is_ipv6) {
      saddr->ss_family = AF_INET6;
      saddr6->sin6_addr = in6addr_any;
      PRINT_DEBUG("find_interface(): Matched all addresses (::)\n");
    }
    else {
      saddr->ss_family = AF_INET;
      saddr4->sin_addr.s_addr = htonl(INADDR_ANY);
      PRINT_DEBUG("find_interface(): Matched all addresses (0.0.0.0)");
    }
    return 0;
  }

  /* Try to query for all devices on the system */
  if(getifaddrs(&interfaces) < 0) {
    PRINT_ERROR("Could not find any interfaces\n");
    return -1;
  }

  compare_address = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);

  /* Loop through the interfaces*/
  for(ifa=interfaces; ifa&&ifa->ifa_next; ifa=ifa->ifa_next) {

    /* Helpers */
    struct sockaddr_in6 *ifaddr6 = (struct sockaddr_in6 *)ifa->ifa_addr;
    struct sockaddr_in *ifaddr4 = (struct sockaddr_in *)ifa->ifa_addr;
    int ss_family = ifa->ifa_addr->sa_family;

    memset(compare_address, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);

    /* Extract the IP address in printable format */
    if(is_ipv6 && ss_family == AF_INET6) {
      PRINT_DEBUG("Extracting a IPv6 address");
      if(inet_ntop(AF_INET6,
                (void *)&ifaddr6->sin6_addr,
                compare_address,
                IPv6_STR_MAX_SIZE) == NULL) {
        PRINT_ERROR("Could not extract printable IPv6 for the interface %s: (%d) %s",
                    ifa->ifa_name,
                    errno,
                    strerror(errno));
        return -1;
      }
    }
    else if(!is_ipv6 && ss_family == AF_INET) {
      PRINT_DEBUG("Extracting a IPv4 address");
      if(inet_ntop(AF_INET,
                (void *)&ifaddr4->sin_addr,
                compare_address,
                IPv6_STR_MAX_SIZE) == NULL) {
        PRINT_ERROR("Could not extract printable IPv4 for the interface %s: (%d) %s",
                    ifa->ifa_name,
                    errno,
                    strerror(errno));
        return -1;
      }
    }
    else {
      continue;
    }

    /* Check if this is the desired interface and/or address
       5 possible scenarios:
       interface and address given
       only interface given (address is automatically bindall)
       only address given
       none given (address is automatically bindall) */
    BOOL if_present = strlen(interface) > 0 ? TRUE : FALSE;
    BOOL addr_present = strlen(address) > 0 ? TRUE : FALSE;
    BOOL addr_is_bindall = ((strcmp("0.0.0.0", address) == 0) ||
                            (strcmp("::", address) == 0) ||
                            (strlen(address) == 0)) ?
                            TRUE :
                            FALSE;

    if((if_present && (strcmp(interface, ifa->ifa_name) == 0) &&
       addr_present && (strcmp(address, compare_address))) ||
      (if_present && (strcmp(interface, ifa->ifa_name) == 0) &&
       addr_is_bindall) ||
      (!if_present &&
       addr_present && (strcmp(address, compare_address) == 0)) ||
      (!if_present &&
       addr_is_bindall)) {

      /* Set the interface index to be returned*/
      ifindex = if_nametoindex(ifa->ifa_name);

      PRINT_DEBUG("Matched interface (with index %d) name '%s' with %s address %s", ifindex, ifa->ifa_name, (ss_family == AF_INET ? "IPv4" : "IPv6"), compare_address);

      /* Set the appropriate address in the sockaddr struct */
      if(!is_ipv6 && ss_family == AF_INET) {
        saddr4->sin_addr = ifaddr4->sin_addr;
        PRINT_DEBUG("Setting IPv4 saddr...");
      }
      else if(is_ipv6 && ss_family == AF_INET6){
        saddr6->sin6_addr = ifaddr6->sin6_addr;
        PRINT_DEBUG("Setting IPv6 saddr");
      }

      /* Set family to IP version*/
      saddr->ss_family = ss_family;

      break;
    }

  }

  /* Free getifaddrs allocations */
  if(interfaces) {
    freeifaddrs(interfaces);
  }

  /* free our compare buffer */
  if(compare_address != NULL) {
    free(compare_address);
  }

  return ifindex;
}

/* Create a UPNP listener */
// TODO: Write it...
static SOCKET create_upnp_listener(char *interface, int send_timeout, int receive_timeout) {
  return FALSE;
}

/* Set send timeout */
static BOOL set_send_timeout(SOCKET sock, int timeout) {
  struct timeval stimeout;
  stimeout.tv_sec = timeout;
  stimeout.tv_usec = 0;

  PRINT_DEBUG("Setting send-timeout to %d", (int)stimeout.tv_sec);

  if(setsockopt(sock,
                SOL_SOCKET,
                SO_SNDTIMEO,
                (char *)&stimeout,
                sizeof(stimeout)) < 0) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return FALSE;
  }
  return TRUE;
}

/* Set receive timeout */
static BOOL set_receive_timeout(SOCKET sock, int timeout) {
  struct timeval rtimeout;
  rtimeout.tv_sec = timeout;
  rtimeout.tv_usec = 0;

  PRINT_DEBUG("Setting receive-timeout to %d", (int)rtimeout.tv_sec);

  if(setsockopt(sock,
                SOL_SOCKET,
                SO_RCVTIMEO,
                (char *)&rtimeout,
                sizeof(rtimeout)) < 0) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return FALSE;
  }
  return TRUE;
}

/* Enable socket to receive from an already used port */
static BOOL set_reuseaddr(SOCKET sock) {
  int reuse = 1;
  PRINT_DEBUG("Setting reuseaddr");
  if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return FALSE;
  }
  /* linux >= 3.9
  if(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return FALSE;
  }
  */
  return TRUE;
}

/* Set socket keepalive */
static BOOL set_keepalive(SOCKET sock, BOOL keepalive) {
  PRINT_DEBUG("Setting keepalive to %d", keepalive);
  if(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&keepalive, sizeof(BOOL)) < 0) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return FALSE;
  }
  return TRUE;
}

/* Set TTL */
static BOOL set_ttl(SOCKET sock, int family, int ttl) {
    PRINT_DEBUG("Setting TTL to %d", ttl);
    if(setsockopt(sock,
                  (family == AF_INET ? IPPROTO_IP : IPPROTO_IPV6),
                  (family == AF_INET ? IP_MULTICAST_TTL : IPV6_MULTICAST_HOPS),
                  (char *)&ttl,
                  sizeof(ttl)) < 0) {
      PRINT_ERROR("(%d) %s", errno, strerror(errno));
      return FALSE;
    }
    return TRUE;
}

/* Disable multicast loopback traffic */
static BOOL disable_multicast_loopback(SOCKET sock, int family) {
  unsigned char loop = FALSE;
  PRINT_DEBUG("Disabling loopback multicast traffic");
  if(setsockopt(sock,
                family == AF_INET ? IPPROTO_IP :
                                    IPPROTO_IPV6,
                family == AF_INET ? IP_MULTICAST_LOOP :
                                     IPV6_MULTICAST_LOOP,
                &loop,
                sizeof(loop)) < 0) {
    PRINT_ERROR("(%d) %s", errno, strerror(errno));
    return FALSE;
  }
  return TRUE;
}



/* Join the multicast group on required interfaces */
static BOOL join_multicast_group(SOCKET sock, char *multicast_group, char *interface_ip) {
  struct ifaddrs *ifa, *interfaces = NULL;
  BOOL is_bindall = FALSE;
  BOOL is_mc_ipv6 = FALSE;
  BOOL is_ipv6 = FALSE;
  struct sockaddr_storage sa_interface;

  PRINT_DEBUG("join_multicast_group(%d, \"%s\", \"%s\")", sock, multicast_group, interface_ip);

  /* Check if multicast_group is a IPv6 address*/
  if(inet_pton(AF_INET6, interface_ip, (void *)&((struct sockaddr_in6 *)&sa_interface)->sin6_addr) > 0) {
    is_mc_ipv6 = TRUE;
  }
  else {
  /* Check if multicast_group is a IPv4 address */
    if(inet_pton(AF_INET, interface_ip, (void *)&((struct sockaddr_in *)&sa_interface)->sin_addr) < 1) {
      PRINT_ERROR("Given multicast group address '%s' is neither an IPv4 nor IPv6, cannot continue", interface_ip);
      return FALSE;
    }
  }

  PRINT_DEBUG("Multicast group address is IPv%d", (is_mc_ipv6 ? 6 : 4));

  /* If interface_ip is empty string set it to "0.0.0.0" */
  if(interface_ip != NULL && strlen(interface_ip) == 0) {
    if(is_mc_ipv6) {
      strncpy(interface_ip, "::", IPv6_STR_MAX_SIZE);
    }
    else {
      strncpy(interface_ip, "0.0.0.0", IPv6_STR_MAX_SIZE);
    }
    PRINT_DEBUG("interface_ip was empty, set to '%s'", interface_ip);
  }

  /* Check if interface_ip is a IPv6 address*/
  if(inet_pton(AF_INET6, interface_ip, (void *)&((struct sockaddr_in6 *)&sa_interface)->sin6_addr) > 0) {
    is_ipv6 = TRUE;
  }
  else {
  /* Check if interface_ip is a IPv4 address */
    if(inet_pton(AF_INET, interface_ip, (void *)&((struct sockaddr_in *)&sa_interface)->sin_addr) < 1) {
      PRINT_ERROR("Given interface address '%s' is neither an IPv4 nor IPv6, cannot continue", interface_ip);
      return FALSE;
    }
  }

  PRINT_DEBUG("Interface address is IPv%d", (is_mc_ipv6 ? 6 : 4));

  /* Check if the address is a bind-on-all address*/
  if(strcmp("0.0.0.0", interface_ip) == 0 ||
     strcmp("::", interface_ip) == 0) {
    is_bindall = TRUE;
  }

  PRINT_DEBUG("Interface address is %s", (is_bindall ? "a bind-all address" : interface_ip));

  /* Get all interfaces and IPs */
  if(getifaddrs(&interfaces) < 0) {
    PRINT_ERROR("Could not find any interfaces: (%d) %s\n", errno, strerror(errno));
    return FALSE;
  }

  PRINT_DEBUG("List of available interfaces and IPs:");
  PRINT_DEBUG("********************");
  for(ifa=interfaces; ifa != NULL; ifa=ifa->ifa_next) {
    struct in_addr *ifaddr4 = (struct in_addr *)&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
    struct in6_addr *ifaddr6 = (struct in6_addr *)&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
    char ip[IPv6_STR_MAX_SIZE];
    if(ifa->ifa_addr->sa_family != AF_INET &&
       ifa->ifa_addr->sa_family != AF_INET6) {
      continue;
    }
    if(inet_ntop(ifa->ifa_addr->sa_family,
                 ifa->ifa_addr->sa_family == AF_INET ? (void *)ifaddr4 :
                                        (void *)ifaddr6,
                 ip,
                 IPv6_STR_MAX_SIZE) == NULL) {
      PRINT_ERROR("Failed to extract address, will skip current interface: (%d) %s",
                  errno,
                  strerror(errno));
      exit(1);
    }
    PRINT_DEBUG("IF: %s; IP: %s", ifa->ifa_name, ip);
  }
  PRINT_DEBUG("********************");

  PRINT_DEBUG("Start looping through available interfaces and IPs");

  /* Loop throgh all the interfaces */
  for(ifa=interfaces; ifa != NULL; ifa=ifa->ifa_next) {

    /* Skip loopback addresses */
    if(ifa->ifa_flags & IFF_LOOPBACK) {
      PRINT_DEBUG("Loopback address detected, skipping");
      continue;
    }

    /* Helpers */
    struct in_addr *ifaddr4 = (struct in_addr *)&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
    struct in6_addr *ifaddr6 = (struct in6_addr *)&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
    int ss_family = ifa->ifa_addr->sa_family;

    /* Skip if not a required type of IP address */
    if((!is_ipv6 && ss_family != AF_INET) ||
       (is_ipv6 && ss_family != AF_INET6)) {
      PRINT_DEBUG("Skipping interface (%s) address, wrong type (%d)",
                  ifa->ifa_name,
                  ss_family);
      continue;
    }

    /* Extract IP in string format */
    char ip[IPv6_STR_MAX_SIZE];
    if(inet_ntop(ss_family,
                 ss_family == AF_INET ? (void *)ifaddr4 :
                                        (void *)ifaddr6,
                 ip,
                 IPv6_STR_MAX_SIZE) == NULL) {
      PRINT_ERROR("Failed to extract address, will skip current interface: (%d) %s",
                  errno,
                  strerror(errno));
      continue;
    }

    /* If not using a bindall IP then skip all
       IP that do not match the desired IP */
    if(!is_bindall && strcmp(ip, interface_ip) != 0) {
      PRINT_DEBUG("Skipping interface (%s) address, wrong IP (%s != %s)",
                  ifa->ifa_name,
                  ip,
                  interface_ip);
      continue;
    }

    PRINT_DEBUG("Found candidate interface %s, type %d, IP %s", ifa->ifa_name, ss_family, ip);

    struct ip_mreq mreq;
    struct ipv6_mreq mreq6;

    if(!is_ipv6) {
      memset(&mreq, 0, sizeof(struct ip_mreq));
      mreq.imr_interface.s_addr = ifaddr4->s_addr;
    }
    else {
      memset(&mreq6, 0, sizeof(struct ipv6_mreq));
      mreq6.ipv6mr_interface = if_nametoindex(ifa->ifa_name);
    }

    if(inet_pton(ss_family,
                 multicast_group,
                 (is_ipv6 ? (void *)&mreq6.ipv6mr_multiaddr :
                            (void *)&mreq.imr_multiaddr)) < 1) {
      PRINT_ERROR("Incompatible multicast group");
      return FALSE;;
    }

    #ifdef DEBUG___
    {
      PRINT_DEBUG("%s", (is_ipv6 ? "IPV6_ADD_MEMBERSHIP" : "IP_ADD_MEMBERSHIP"));
      char a[IPv6_STR_MAX_SIZE];
      if(is_ipv6) {
        PRINT_DEBUG("mreq6.ipv6mr_interface: %d", mreq6.ipv6mr_interface);
      }
      else {
        inet_ntop(ss_family, (void *)&mreq.imr_interface, a, IPv6_STR_MAX_SIZE);
        PRINT_DEBUG("mreq.imr_interface: %s", a);
      }
      inet_ntop(ss_family,
                is_ipv6 ? (void *)&mreq6.ipv6mr_multiaddr :
                          (void *)&mreq.imr_multiaddr,
                a,
                IPv6_STR_MAX_SIZE);
      PRINT_DEBUG("mreq%smr_multiaddr: %s", (is_ipv6 ? "6.ipv6" : ".i"), a);
    }
    #endif

    PRINT_DEBUG("Joining multicast group '%s' on interface IP '%s'", multicast_group, ip);

    if(setsockopt(sock,
                  is_ipv6 ? IPPROTO_IPV6 :
                            IPPROTO_IP,
                  is_ipv6 ? IPV6_ADD_MEMBERSHIP :
                            IP_ADD_MEMBERSHIP,
                  is_ipv6 ? (void *)&mreq6 :
                            (void *)&mreq,
                  is_ipv6 ? sizeof(struct ipv6_mreq) :
                            sizeof(struct ip_mreq)) < 0) {
      PRINT_ERROR("(%d) %s", errno, strerror(errno));
      close(sock);
      freeifaddrs(interfaces);
      free_stuff();
      exit(EXIT_FAILURE);
    }
  }

  PRINT_DEBUG("Finished looping through interfaces and IPs");

  /* Free ifaddrs interfaces */
  if(interfaces != NULL) {
    freeifaddrs(interfaces);
    interfaces = NULL;
  }

  return TRUE;

}

/**
 * Creates a connection to a given host
 * TODO: >>>TO BE DEPRECATED SOON<<<
 */
static SOCKET setup_socket(BOOL is_ipv6,
                           BOOL is_udp,
                           BOOL is_multicast,
                           char *interface,
                           char *if_ip,
                           struct sockaddr_storage *sa,
                           const char *ip,
                           int port,
                           BOOL is_server,
                           BOOL keepalive) {
  SOCKET sock;
  int protocol;
  int ifindex = 0;
  struct ip_mreq mreq;
  struct ipv6_mreq mreq6;
  struct sockaddr_storage *saddr;

  saddr = (struct sockaddr_storage *)malloc(sizeof(struct sockaddr_storage));
  memset(saddr, 0, sizeof(struct sockaddr_storage));
  memset(&mreq, 0, sizeof(mreq));
  memset(&mreq6, 0, sizeof(mreq6));

  /* If 'sa' (sockaddr) given instead of 'if_ip' (char *)
     then fill 'if_ip' with the IP from the sockaddr */
  if(sa != NULL) {
    inet_ntop(sa->ss_family,
              sa->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)sa)->sin_addr :
                                         (void *)&((struct sockaddr_in6 *)sa)->sin6_addr,
              if_ip,
              IPv6_STR_MAX_SIZE);
  }

  /* Find the index of the interface */
  ifindex = find_interface(saddr, interface, if_ip);
  if(ifindex < 0) {
    PRINT_ERROR("The requested interface '%s' could not be found\n", interface);
    free(saddr);
    free_stuff();
    exit(EXIT_FAILURE);
  }

  #ifdef DEBUG___
  {
    char a[100];
    memset(a, '\0', 100);
    if(!inet_ntop(saddr->ss_family, (saddr->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)saddr)->sin_addr : (void *)&((struct sockaddr_in6 *)saddr)->sin6_addr), a, 100)) {
      PRINT_ERROR("setup_socket(); inet_ntop(): (%d) %s", errno, strerror(errno));
    }
    PRINT_DEBUG("find_interface() returned: saddr->sin_addr: %s", a);
  }
  #endif

  /* If muticast requested, check if it really is a multicast address */
  if(is_multicast && (ip != NULL && strlen(ip) > 0)) {
    if(!is_address_multicast(ip)) {
      PRINT_ERROR("The specified IP (%s) is not a multicast address\n", ip);
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      free_stuff();
      exit(EXIT_FAILURE);
    }
  }

  /* Set protocol version */
  if(is_ipv6) {
    saddr->ss_family = AF_INET6;
    protocol = IPPROTO_IPV6;
  }
  else {
    saddr->ss_family = AF_INET;
    protocol = IPPROTO_IP;
  }

  /* init socket */
  sock = socket(saddr->ss_family,
                is_udp ? SOCK_DGRAM : SOCK_STREAM,
                protocol);

  if(sock < 0) {
    PRINT_ERROR("Failed to create socket. (%d) %s", errno, strerror(errno));
    free(saddr);
    if(sa != NULL) {
      free(interface);
    }
    free_stuff();
    exit(EXIT_FAILURE);
  }


  /* Set reuseaddr */
  if(!set_reuseaddr(sock)) {
    free(saddr);
    if(sa != NULL) {
      free(interface);
    }
    free_stuff();
    exit(errno);
  }

  /* Set keepalive */
  if(!set_keepalive(sock, keepalive)) {
    free(saddr);
    if(sa != NULL) {
      free(interface);
    }
    free_stuff();
    exit(EXIT_FAILURE);
  }

  /* Set TTL */
  if(!is_server && is_multicast) {
    if(set_ttl(sock,
               (is_ipv6 ? AF_INET6 : AF_INET),
               conf.ttl) < 0) {
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      free_stuff();
      exit(EXIT_FAILURE);
    }
  }

  /* Setup address structure containing address information to use to connect */
  if(is_ipv6) {
    PRINT_DEBUG("is_ipv6 == TRUE");
    struct sockaddr_in6 * saddr6 = (struct sockaddr_in6 *)saddr;
    saddr6->sin6_port = htons(port);
    if(is_udp && is_multicast) {
      PRINT_DEBUG("  is_udp == TRUE && is_multicast == TRUE");
      if(ip == NULL || strlen(ip) < 1) {
        inet_pton(saddr->ss_family, SSDP_ADDR6_LL, &mreq6.ipv6mr_multiaddr);
      }
      else {
        inet_pton(saddr->ss_family, ip, &mreq6.ipv6mr_multiaddr);
      }
      if(interface != NULL && strlen(interface) > 0) {
        mreq6.ipv6mr_interface = (unsigned int)ifindex;
      }
      else {
        mreq6.ipv6mr_interface = (unsigned int)0;
      }
      //saddr6->sin6_addr =  mreq6.ipv6mr_multiaddr;

      #ifdef DEBUG___
      {
        if(!is_server) {
          PRINT_DEBUG("    IPV6_MULTICAST_IF");
        }
        char a[100];
        PRINT_DEBUG("    mreq6->ipv6mr_interface: %d", ifindex);
        inet_ntop(saddr->ss_family, (void *)&mreq6.ipv6mr_multiaddr, a, 100);
        PRINT_DEBUG("    mreq6->ipv6mr_multiaddr: %s", a);
      }
      #endif

      if(!is_server && setsockopt(sock,
                                  protocol,
                                  IPV6_MULTICAST_IF,
                                  &mreq6.ipv6mr_interface,
                                  sizeof(mreq6.ipv6mr_interface)) < 0) {
        PRINT_ERROR("setsockopt() IPV6_MULTICAST_IF: (%d) %s", errno, strerror(errno));
        free(saddr);
        if(sa != NULL) {
          free(interface);
        }
        free_stuff();
        exit(EXIT_FAILURE);
      }
    }
  }
  else {
    PRINT_DEBUG("is_ipv6 == FALSE");
    struct sockaddr_in *saddr4 = (struct sockaddr_in *)saddr;
    saddr4->sin_port = htons(port);
    if(is_udp && is_multicast) {
      PRINT_DEBUG("  is_udp == TRUE && is_multicast == TRUE");
      if(ip == NULL || strlen(ip) < 1) {
        inet_pton(saddr->ss_family, SSDP_ADDR, &mreq.imr_multiaddr);
      }
      else {
        inet_pton(saddr->ss_family, ip, &mreq.imr_multiaddr);
      }
      if((if_ip != NULL && strlen(if_ip) > 0) || 
          (interface != NULL && strlen(interface) > 0)) {
         mreq.imr_interface.s_addr = saddr4->sin_addr.s_addr;
      }
      else {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
      }
      //saddr4->sin_addr =  mreq.imr_multiaddr;
      #ifdef DEBUG___
      {
        if(!is_server) {
          PRINT_DEBUG("    IP_MULTICAST_IF");
        }
        char a[100];
        inet_ntop(saddr->ss_family, (void *)&mreq.imr_interface, a, 100);
        PRINT_DEBUG("    mreq->imr_interface: %s", a);
        inet_ntop(saddr->ss_family, (void *)&mreq.imr_multiaddr, a, 100);
        PRINT_DEBUG("    mreq->imr_multiaddr: %s", a);
      }
      #endif
      if(!is_server && setsockopt(sock, protocol, IP_MULTICAST_IF, &mreq.imr_interface, sizeof(mreq.imr_interface)) < 0) {
        PRINT_ERROR("setsockopt() IP_MULTICAST_IF: (%d) %s", errno, strerror(errno));
        free(saddr);
        if(sa != NULL) {
          free(interface);
        }
        free_stuff();
        exit(EXIT_FAILURE);
      }
    }
  }

  /* Enable/disable loopback multicast */
  if(is_server && is_multicast && !conf.enable_loopback &&
     !disable_multicast_loopback(sock, saddr->ss_family)) {
    free(saddr);
    if(sa != NULL) {
      free(interface);
    }
    free_stuff();
    exit(EXIT_FAILURE);
  }

  /* If server requested, bind the socket to the given address and port*/
  // TODO: Fix for IPv6
  if(is_server) {

    struct sockaddr_in bindaddr;
    bindaddr.sin_family = saddr->ss_family;
    bindaddr.sin_addr.s_addr = mreq.imr_multiaddr.s_addr;
    bindaddr.sin_port = ((struct sockaddr_in *)saddr)->sin_port;

    #ifdef DEBUG___
    PRINT_DEBUG("is_server == TRUE");
    char a[100];
    inet_ntop(bindaddr.sin_family, (void *)&((struct sockaddr_in *)&bindaddr)->sin_addr, a, 100);
    PRINT_DEBUG("  bind() to: saddr->sin_family: %d(%d)", ((struct sockaddr_in *)&bindaddr)->sin_family, AF_INET);
    PRINT_DEBUG("  bind() to: saddr->sin_addr: %s (port %d)", a, ntohs(((struct sockaddr_in *)&bindaddr)->sin_port));
    #endif

    if(bind(sock,
           (struct sockaddr *)&bindaddr,
           (bindaddr.sin_family == AF_INET ? sizeof(struct sockaddr_in) :
                                          sizeof(struct sockaddr_in6))) < 0) {
      PRINT_ERROR("setup_socket(); bind(): (%d) %s", errno, strerror(errno));
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      free_stuff();
      exit(EXIT_FAILURE);
    }
    if(!is_udp) {
      PRINT_DEBUG("  is_udp == FALSE");
      if(listen(sock, LISTEN_QUEUE_LENGTH) == SOCKET_ERROR) {
        PRINT_ERROR("setup_socket(); listen(): (%d) %s", errno, strerror(errno));
        close(sock);
        free(saddr);
        if(sa != NULL) {
          free(interface);
        }
        free_stuff();
        exit(errno);
      }
    }
  }

  /* Make sure we have a string IP before joining multicast groups */
  /* NOTE: Workaround until refactoring is complete */
  char iface_ip[IPv6_STR_MAX_SIZE];
  if(if_ip == NULL || strlen(if_ip) < 1) {
    if(inet_ntop(is_ipv6 ? AF_INET6 :
                           AF_INET,
                 is_ipv6 ? (void *)&((struct sockaddr_in6 *)saddr)->sin6_addr :
                           (void *)&((struct sockaddr_in *)saddr)->sin_addr,
                 iface_ip,
                 IPv6_STR_MAX_SIZE) == NULL) {
      PRINT_ERROR("Failed to get string representation of the interface IP address: (%d) %s", errno, strerror(errno));
      free_stuff();
      free(saddr);
      exit(errno);
    }
  }
  else {
    strcpy(iface_ip, if_ip);
  }

  /* Join the multicast group on required interfaces */
  if(is_server && is_multicast && !join_multicast_group(sock,
                          is_ipv6 ? SSDP_ADDR6_SL :
                                    SSDP_ADDR,
                          iface_ip)) {
    PRINT_ERROR("Failed to join required multicast group");
    return -1;
  }

  /* DEPRECATED
  if(FALSE && is_server && is_multicast) {
    PRINT_DEBUG("***************************");
    PRINT_DEBUG("is_server == TRUE && is_multicast == TRUE");

      // Check if it is the correct IP version and skip if needed
      if((conf->use_ipv6 && ifa->ifa_addr->sa_family == AF_INET) ||
        (conf->use_ipv4 && ifa->ifa_addr->sa_family == AF_INET6) ||
        (ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6)) {
        PRINT_DEBUG("skipping ifa %s", if_indextoname(ifindex));
        continue;
      }

      if(!conf->use_ipv6 && ifa->ifa_addr->sa_family == AF_INET) {

        struct sockaddr_in *ifaddr4 = (struct sockaddr_in *)ifa->ifa_addr;
        mreq.imr_interface.s_addr = ifaddr4->sin_addr.s_addr;

        #ifdef DEBUG___
        {
          PRINT_DEBUG("    is_ipv6 == FALSE");
          PRINT_DEBUG("      IP_ADD_MEMBERSHIP");
          char a[100];
          inet_ntop(AF_INET, (void *)&mreq.imr_interface, a, 100);
          PRINT_DEBUG("      mreq->imr_interface: %s", a);
          inet_ntop(AF_INET, (void *)&mreq.imr_multiaddr, a, 100);
          PRINT_DEBUG("      mreq->imr_multiaddr: %s", a);
        }
        #endif

        if(setsockopt(sock, protocol, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
         PRINT_ERROR("setup_socket(); setsockopt() IP_ADD_MEMBERSHIP: (%d) %s", errno, strerror(errno));
         close(sock);
         free(saddr);
          if(sa != NULL) {
            free(interface);
          }
          free_stuff();
          exit(EXIT_FAILURE);
        }
      }
      else if(!conf->use_ipv4 && ifa->ifa_addr->sa_family == AF_INET6) {

        mreq6.ipv6mr_interface = if_nametoindex(ifa->ifa_name);

        #ifdef DEBUG___
        {
          PRINT_DEBUG("    is_ipv6 == TRUE");
          PRINT_DEBUG("      IPV6_ADD_MEMBERSHIP");
          char a[100];
          inet_ntop(AF_INET6, (void *)&mreq6.ipv6mr_interface, a, 100);
          PRINT_DEBUG("      mreq6->ipv6mr_interface: %s", a);
        }
        #endif
        if(setsockopt(sock, protocol, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(struct ipv6_mreq)) < 0) {
          PRINT_ERROR("setup_socket(); setsockopt() IPV6_ADD_MEMBERSHIP: (%d) %s", errno, strerror(errno));
          close(sock);
          free(saddr);
          if(sa != NULL) {
            free(interface);
          }
          free_stuff();
          exit(EXIT_FAILURE);
        }
      }



      if(!conf->quiet_mode) {
        char mcast_str[IPV6_STR_MAX_SIZE];
        char if_str[IPV6_STR_MAX_SIZE];
        inet_ntop(ifa->ifa_addr->sa_family,
                 (ifa->ifa_addr->sa_family == AF_INET6 ? (void *)&mreq6.ipv6mr_multiaddr : (void *)&mreq.imr_multiaddr),
                 mcast_str,
                 sizeof(char) * IPV6_STR_MAC_SIZE);
        inet_ntop(ifa->ifa_addr->sa_family, (void *)ifa-ifa_addr.in_addr, interface, IPV6_STR_MAX_SIZE);
        printf("Interface %s joined multicast group %s\n", interface, mcast_str);
      }

      free(mcast_str);

      // If it is is not a bindall address then end the loop
      if(!is_bindall) {
        ifa = NULL;
      }

    }

    // Free ifaddrs interfaces
    if(interfaces != NULL) {
      freeifaddrs(interfaces);
      interfaces = NULL;
    }
  }
  */

  free(saddr);

  return sock;
}

/**
* Check whether a given IP is a multicast IP
* (if IPV6 checks for UPnP multicast IP, should be
* changed to check if multicast at all)
*
* @param const char *address The address to check
*
* @return BOOL Obvious.
*/
static BOOL is_address_multicast(const char *address) {
  char *str_first = NULL;
  int int_first = 0;
  BOOL is_ipv6;
  if(address != NULL) {
    // TODO: Write a better IPv6 discovery mechanism
    is_ipv6 = strchr(address, ':') != NULL ? TRUE : FALSE;
    if(strcmp(address, "0.0.0.0") == 0) {
      return TRUE;
    }
    if(is_ipv6) {
      struct in6_addr ip6_ll;
      struct in6_addr ip6_addr;
      inet_pton(AF_INET6, "ff02::c", &ip6_ll);
      inet_pton(AF_INET6, address, &ip6_addr);
      if(ip6_ll.s6_addr == ip6_addr.s6_addr) {
        return TRUE;
      }
      return FALSE;
    }
    else {
      str_first = (char *)malloc(sizeof(char) * 4);
      memset(str_first, '\0', sizeof(char) * 4);
      strncpy(str_first, address, 3);
      int_first = atoi(str_first);
      free(str_first);
      if(int_first > 223 && int_first < 240) {
        return TRUE;
      }
      return FALSE;
    }
  }
  printf("Supplied address ('%s') is not valid\n", address);
  free_stuff();
  exit(EXIT_FAILURE);
  return FALSE;
}

/**
* Initializes (allocates neccessary memory) for a SSDP message
*
* @param message The message to initialize
*/
static BOOL init_ssdp_message(ssdp_message_s **message_pointer) {
  if(NULL == *message_pointer) {
    *message_pointer = (ssdp_message_s *)malloc(sizeof(ssdp_message_s));
    if(!*message_pointer) {
      return FALSE;
    }
    memset(*message_pointer, 0, sizeof(ssdp_message_s));
  }
  ssdp_message_s *message = *message_pointer;
  message->mac = (char *)malloc(sizeof(char) * MAC_STR_MAX_SIZE);
  if(NULL == message->mac) {
    free(message);
    return FALSE;
  }
  memset(message->mac, '\0', MAC_STR_MAX_SIZE);
  message->ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
  if(NULL == message->ip) {
    free(message->mac);
    free(message);
    return TRUE;
  }
  memset(message->ip, '\0', IPv6_STR_MAX_SIZE);
  message->datetime = (char *)malloc(sizeof(char) * 20);
  if(NULL == message->datetime) {
    free(message->mac);
    free(message->ip);
    free(message);
    return TRUE;
  }
  memset(message->datetime, '\0', 20);
  message->request = (char *)malloc(sizeof(char) * 1024);
  if(NULL == message->request) {
    free(message->mac);
    free(message->ip);
    free(message->datetime);
    free(message);
    return FALSE;
  }
  memset(message->request, '\0', 1024);
  message->protocol = (char *)malloc(sizeof(char) * 48);
  if(NULL == message->protocol) {
    free(message->mac);
    free(message->ip);
    free(message->datetime);
    free(message->request);
    free(message);
    return FALSE;
  }
  memset(message->protocol, '\0', sizeof(char) * 48);
  message->answer = (char *)malloc(sizeof(char) * 1024);
  if(NULL == message->answer) {
    free(message->mac);
    free(message->ip);
    free(message->datetime);
    free(message->request);
    free(message->protocol);
    free(message);
    return FALSE;
  }
  memset(message->answer, '\0', sizeof(char) * 1024);
  message->info = NULL;
  message->message_length = 0;
  message->header_count = 0;

  return TRUE;
}

/**
* Create a plain-text message
*
* @param results The buffer to store the results in
* @param buffer_size The size of the buffer 'results'
* @param message The message to convert
*/
static BOOL create_plain_text_message(char *results, int buffer_size, ssdp_message_s *ssdp_message, BOOL fetch_info) {
  int buffer_used = 0, count = 0;
  ssdp_custom_field_s *cf = NULL;

  if(ssdp_message->custom_fields) {
    cf = ssdp_message->custom_fields->first;
  }

  buffer_used += snprintf(results + buffer_used,
        buffer_size - buffer_used,
        "Time received: %s\n",
        ssdp_message->datetime);
  buffer_used += snprintf(results + buffer_used,
        buffer_size - buffer_used,
        "Origin-MAC: %s\n",
        (ssdp_message->mac != NULL ? ssdp_message->mac : "(Could not be determined)"));
  buffer_used += snprintf(results + buffer_used,
        buffer_size - buffer_used,
        "Origin-IP: %s\nMessage length: %d Bytes\n",
        ssdp_message->ip,
        ssdp_message->message_length);
  buffer_used += snprintf(results + buffer_used,
        buffer_size - buffer_used,
        "Request: %s\nProtocol: %s\n",
        ssdp_message->request,
        ssdp_message->protocol);

  while(cf) {
    
    buffer_used += snprintf(results + buffer_used,
                            buffer_size - buffer_used,
                            "Custom field[%d][%s]: %s\n",
                            count,
                            cf->name,
                            cf->contents);
    count++;
    cf = cf->next;
  }

  count = 0;
  ssdp_header_s *ssdp_headers = ssdp_message->headers;
  while(ssdp_headers) {
  buffer_used += snprintf(results + buffer_used,
          buffer_size - buffer_used,
          "Header[%d][type:%d;%s]: %s\n",
          count, ssdp_headers->type,
          get_header_string(ssdp_headers->type, ssdp_headers),
          ssdp_headers->contents);
    ssdp_headers = ssdp_headers->next;
    count++;
  }

  return TRUE;
}

/**
 * Adds a ssdp message to a ssdp messages list. If the list hasn't been
 * initialized then it is initialized first.
 *
 * @param **ssdp_cache_pointer The address of a pointer to a ssdp cache list
 * @param *ssdp_message The ssdp message to be appended to the cache list
 *
 * @return TRUE on success, exits on error
 */
static BOOL add_ssdp_message_to_cache(ssdp_cache_s **ssdp_cache_pointer, ssdp_message_s *ssdp_message) {
  ssdp_cache_s *ssdp_cache = NULL;

  /* Sanity check */
  if(NULL == ssdp_cache_pointer) {
    PRINT_ERROR("No ssdp cache list given");
    free_stuff();
    exit(EXIT_FAILURE);
  }

  /* Initialize the list if needed */
  if(NULL == *ssdp_cache_pointer) {
    PRINT_DEBUG("Initializing the SSDP cache");
    *ssdp_cache_pointer = (ssdp_cache_s *) malloc(sizeof(ssdp_cache_s));
    if(NULL == *ssdp_cache_pointer) {
      PRINT_ERROR("Failed to allocate memory for the ssdp cache list");
      free_stuff();
      exit(EXIT_FAILURE);
    }
    memset(*ssdp_cache_pointer, 0, sizeof(ssdp_cache_s));

    /* Set the ->first to point to this element */
    (*ssdp_cache_pointer)->first = *ssdp_cache_pointer;

    /* Set ->new to point to NULL */
    (*ssdp_cache_pointer)->next = NULL;

    /* Set the counter to 0 */
    (*ssdp_cache_pointer)->ssdp_messages_count = (unsigned int *)malloc(sizeof(unsigned int));
    *(*ssdp_cache_pointer)->ssdp_messages_count = 0;
  }
  else {

    /* Point to the begining of the cache list */
    ssdp_cache = (*ssdp_cache_pointer)->first;
  
    /* Check for duplicate and abort adding it if found */
    while(ssdp_cache) {
      if(0 == strcmp(ssdp_message->ip, ssdp_cache->ssdp_message->ip)) {
        /* Found a duplicate, skip */
        PRINT_DEBUG("Found duplicate SSDP message, skipping");
        return TRUE;
      }
      ssdp_cache = ssdp_cache->next;
    }

  }

  /* Point to the passed position */
  ssdp_cache = *ssdp_cache_pointer;

  /* Make sure we are at the end of the linked list
     and move to last if needed*/
  if(NULL != ssdp_cache->next) {
    PRINT_DEBUG("Given SSDP Cache list is not pointing to the last element");
    while(NULL != ssdp_cache->next) {
      PRINT_DEBUG("Moving to the next element in the cache list");
      ssdp_cache = ssdp_cache->next;
    }
  }

  /* If working on an preexisting cache list
     then create a new element in the list,
     set the 'first' field to the first element
     and move to the new element */
  if(NULL != ssdp_cache->ssdp_message) {
    PRINT_DEBUG("Creating a new element in the SSDP cache list");
    ssdp_cache->next = (ssdp_cache_s *) malloc(sizeof(ssdp_cache_s));
    memset(ssdp_cache->next, 0, sizeof(ssdp_cache_s));
    ssdp_cache->next->first = ssdp_cache->first;
    ssdp_cache->next->next = NULL;
    ssdp_cache->next->ssdp_messages_count = ssdp_cache->ssdp_messages_count;
    ssdp_cache = ssdp_cache->next;
  }

  /* Point to the ssdp_message from the element
     and increase the counter */
  (*ssdp_cache->ssdp_messages_count)++;
  PRINT_DEBUG("SSDP cache counter increased to %d", *ssdp_cache->ssdp_messages_count);
  ssdp_cache->ssdp_message = ssdp_message;

  /* Set the passed ssdp_cache to point to the last element */
  *ssdp_cache_pointer = ssdp_cache;

  return TRUE;
}

/**
 * Frees all the elements in the ssdp messages list
 *
 * @param **ssdp_cache_pointer The ssdp cache list to be cleared
 */
static void free_ssdp_cache(ssdp_cache_s **ssdp_cache_pointer) {
  ssdp_cache_s *ssdp_cache = NULL;
  ssdp_cache_s *next_cache = NULL;

  /* Sanity check */
  if(NULL == ssdp_cache_pointer) {
    PRINT_ERROR("No ssdp cache list given");
    free_stuff();
    exit(EXIT_FAILURE);
  }

  /* If there is any elements in the list */
  if(NULL != *ssdp_cache_pointer) {

    /* Make life easier */
    ssdp_cache = *ssdp_cache_pointer;

    /* Start from the first element */
    ssdp_cache = ssdp_cache->first;

    /* Free counter */
    free(ssdp_cache->ssdp_messages_count);

    /* Loop through elements and free them */
    do {

      PRINT_DEBUG("Freeing one cache element");

      /* Free the ssdp_message */
      if(NULL != ssdp_cache->ssdp_message) {
        free_ssdp_message(&ssdp_cache->ssdp_message);
      }

      /* Point to the next element in the list */
      next_cache = ssdp_cache->next;
      free(ssdp_cache);
      ssdp_cache = next_cache;

    } while(NULL != ssdp_cache);

    /* Finally set the list to NULL */
    *ssdp_cache_pointer = NULL;
  }
  #ifdef __DEBUG
  else {
    PRINT_DEBUG("*ssdp_cache_pointer is NULL");
  }
  #endif
}

#ifdef DEBUG___
/**
 * Counts the number of times needle occurs in haystack
 *
 * @param haystack The string to search in
 * @param needle The character to search for
 *
 * @return int The number of occurrences
 */
static int chr_count(char *haystack, char needle) {
  int count = 0;
  int len = strlen(haystack);
  int i;
  for (i = 0; i < len; i++) {
    if (haystack[i] == needle) {
      count++;
    }
  }
  return count;
}

#ifdef DEBUG_TO_FILE___
/**
 * Retrieves the size of a file
 *
 * @param file The file to stat
 *
 * @return The size of the file in Bytes
 */
static int fsize(const char *file) {
  struct stat st;

  if(stat(file, &st) == 0) {
    return st.st_size;
  }

  return -1;
}
#endif

/**
 * Prints a debug message, mimicking printf-like function, supports only %d, %s and %c
 *
 * @param const char* file The file the debug message generated in
 * @param int line The line number in the file the debug message generated at
 * @param const char* message The debug message to be displayed
 * @param ... variable number of optional arguments that match with the va_format
 */

static void print_debug(FILE *std, const char *color, const char* file, int line, char *va_format, ...) {
  va_list va;
  char message[20480];
  int message_used = 0;
  char *from_pointer = va_format;
  char header[50];
  char *to_pointer = va_format;
  int args_count = chr_count(va_format, '%');
  const char *no_memory = "Not enought memory in buffer to build debug message";
  char *s = NULL;
  char c;
  int d;

  /* Create a nice header for the output */
  sprintf(header, "%s[%d][%s:%04d] ", color, (int)getpid(), file, line);

  if(!va_format) {
    fprintf(stderr, "%s[%d][%s:%d] Error, va_arg input format (va_format) not set%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__, DEBUG_COLOR_END);
    return;
  }

  int message_length = 20480;
  memset(message, '\0', sizeof(char) * message_length);
  message_length -= 50; // compensate for additional chars in the message
  va_start(va, va_format);

  int copy_length = 0;
  int args_pos;

  for (args_pos = 1; (from_pointer && *from_pointer != '\0'); from_pointer = to_pointer) {

    to_pointer = strchr(from_pointer, '%');

    if(to_pointer && strlen(to_pointer) > 1) {
      copy_length = to_pointer - from_pointer;

      if(copy_length > message_length - message_used) {
      fprintf(stderr, "%s[%d][%s:%d] %s%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
        return;
      }

      strncpy(message + message_used, from_pointer, copy_length);
      message_used += copy_length;

    switch(to_pointer[1]) {
    case 's':
      /* string */
      s = va_arg(va, char *);
      if(s) {
        copy_length = strlen(s);
        if(copy_length > message_length - message_used) {
          fprintf(stderr, "%s[%d][%s:%d] %s%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
          return;
        }
        strncpy(message + message_used, s, copy_length);
        message_used += copy_length;
      }
      else {
        copy_length = 6;
        strncpy(message + message_used, "NULL", copy_length);
        message_used += copy_length;
      }
      break;
    case 'c':
      /* character */
      c = (char)va_arg(va, int);
      copy_length = 1;
      if(copy_length > message_length - message_used) {
        fprintf(stderr, "%s[%d][%s:%d] %s%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
        return;
      }
      strncpy(message + message_used, &c, copy_length);
      message_used += copy_length;
      break;
    case 'd':
      /* integer */
      d = va_arg(va, int);
      char *d_char = (char *)malloc(sizeof(char) * 10);
      memset(d_char, '\0', 10);
      sprintf(d_char, "%d", d);
      copy_length = strlen(d_char);
      if(copy_length > message_length - message_used) {
        fprintf(stderr, "%s[%d][%s:%d] %s%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
        free(d_char);
        return;
      }
      strncpy(message + message_used, d_char, copy_length);
      message_used += copy_length;
      free(d_char);
      break;
    default:
      break;
    }

    to_pointer += 2;
  }
  else {
    strcat(message, from_pointer);
    to_pointer = NULL;
  }

  if(args_pos > args_count) {
    break;
  }
    args_pos++;
  }

  if(!std) {
    std = stdout;
  }

  #ifdef DEBUG_TO_FILE___
  /* Debug file to write to */
  const char debug_file[] = "debug.log";
  FILE *fh = NULL;
  int file_size = fsize(debug_file);
  /* Max file size is 500 KB */
  int max_size = 512000;
  /* If file is larger than 500KB then trunkate it */
  if(file_size > max_size) {
    fh = fopen(debug_file, "w");
  }
  /* Else continue writing to it */
  else {
    fh = fopen(debug_file, "a");
  }

  if(NULL == fh) {
    fprintf(stderr, "%s[%d][%s:%d] Failed to create debug file%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__, DEBUG_COLOR_END);
  }
  else {
    fprintf(fh, "%s%s%s\n", header, message, DEBUG_COLOR_END);
    fclose(fh);
  }
  #else
  fprintf(std, "%s%s%s\n", header, message, DEBUG_COLOR_END);
  #endif

}
#endif

