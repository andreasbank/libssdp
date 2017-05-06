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
 *  Copyright(C) 2014-1017 by Andreas Bank, andreas.mikael.bank@gmail.com
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>

#include <net/if.h>
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
#endif

#include "configuration.h"
#include "common_definitions.h"
#include "log.h"
#include "net_definitions.h"
#include "net_utils.h"
#include "output_format.h"
#include "ssdp.h"
#include "string_utils.h"

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

/* Globals */
static SOCKET               notif_server_sock = 0;       /* The main socket for listening for UPnP (SSDP) devices (or device answers) */
static SOCKET               notif_client_sock = 0;       /* The sock that we want to ask/look for devices on (unicast) */
static SOCKET               notif_recipient_sock = 0;    /* Notification recipient socket, where the results will be forwarded */
struct sockaddr_storage    *notif_recipient_addr = NULL; /* The sockaddr where we store the recipient address */
static filters_factory_s   *filters_factory = NULL;      /* The structure contining all the filters information */
static configuration_s      conf;                         /* The program configuration */

/* Functions */
static void free_stuff();
static void exit_sig(int);
static BOOL build_ssdp_message(ssdp_message_s *, char *, char *, int, const char *);
static int send_stuff(const char *, const char *, const struct sockaddr_storage *, int, int);
static void free_ssdp_message(ssdp_message_s **);
static int fetch_custom_fields(ssdp_message_s *);
static void parse_filters(char *, filters_factory_s **, BOOL);
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
static BOOL create_plain_text_message(char *, int, ssdp_message_s *);
static BOOL add_ssdp_message_to_cache(ssdp_cache_s **, ssdp_message_s **);
static void free_ssdp_cache(ssdp_cache_s **);
static void daemonize();
static BOOL filter(ssdp_message_s *, filters_factory_s *);
static BOOL flush_ssdp_cache(ssdp_cache_s **, const char *, struct sockaddr_storage *, int, int);
static void display_ssdp_cache(ssdp_cache_s *, BOOL);
static void move_cursor(int, int);
static void get_window_size(int *, int *);
static ssdp_custom_field_s *get_custom_field(ssdp_message_s *, const char *);

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
 * Send and free the passed SSDP cache
 */
static BOOL flush_ssdp_cache(ssdp_cache_s **ssdp_cache_pointer,
                             const char *url,
                             struct sockaddr_storage *sockaddr_recipient,
                             int port,
                             int timeout) {
  ssdp_cache_s *ssdp_cache = *ssdp_cache_pointer;
  int results_size = *ssdp_cache->ssdp_messages_count * XML_BUFFER_SIZE;
  char results[results_size];

  /* If -j then convert all messages to one JSON blob */
  if(conf.json_output) {
    if(1 > cache_to_json(ssdp_cache, results, results_size)) {
      PRINT_ERROR("Failed creating JSON blob from ssdp cache");
      return FALSE;
    }
  }

  /* If -x then convert all messages to one XML blob */
  if(conf.xml_output) {
    if(1 > cache_to_xml(ssdp_cache, results, results_size)) {
      PRINT_ERROR("Failed creating XML blob from ssdp cache");
      return FALSE;
    }
  }

  // TODO: make it create a list instead of single plain message
  else if(!create_plain_text_message(results, XML_BUFFER_SIZE, ssdp_cache->ssdp_message)) {
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
static void display_ssdp_cache(ssdp_cache_s *ssdp_cache, BOOL draw_asci) {
  int horizontal_lines_printed = 4;
  const char **tbl_ele = NULL;

  const char *single_line_table_elements[] = {
    "┤", // 185 "\e(0\x75\e(B"
    "│", // 186 "\e(0\x78\e(B"
    "┐", // 187 "\e(0\x6b\e(B"
    "┘", // 188 "\e(0\x6a\e(B"
    "└", // 200 "\e(0\x6d\e(B"
    "┌", // 201 "\e(0\x6c\e(B"
    "┴", // 202 "\e(0\x76\e(B"
    "┬", // 203 "\e(0\x77\e(B"
    "├", // 204 "\e(0\x74\e(B"
    "─", // 205 "\e(0\x71\e(B"
    "┼"  // 206 "\e(0\x6e\e(B"
  };

  const char *asci_table_elements[] = {
    "+",
    "|",
    "+",
    "+",
    "+",
    "+",
    "+",
    "+",
    "+",
    "-",
    "+",
  };

  const char *columns[] = {
    " ID                  ",
    " IPv4            ",
    " MAC               ",
    " Model           ",
    " Version         "
  };

  if(draw_asci) {
    tbl_ele = asci_table_elements;
  }
  else {
    tbl_ele = single_line_table_elements;
  }

  move_cursor(0, 0);

  /* Draw the topmost line */
  int i;
  for(i = 0; i < sizeof(columns) / sizeof(char *); i++) {
    printf("%s",
           (i == 0 ? tbl_ele[5] : tbl_ele[7]));
    int j;
    for(j = 0; j < strlen(columns[i]); j++) {
      printf("%s", tbl_ele[9]);
    }
  }
  printf("%s\n", tbl_ele[2]);

  /* Draw first row with column titles */
  for(i = 0; i < sizeof(columns) / sizeof(char *); i++) {
    printf("%s\x1b[1m%s\x1b[0m", tbl_ele[1], columns[i]);
  }
  printf("%s\n", tbl_ele[1]);

  if(ssdp_cache) {

    /* Draw a row-dividing line */
    for(i = 0; i < sizeof(columns) / sizeof(char *); i++) {
      printf("%s", (i == 0 ? tbl_ele[8] : tbl_ele[10]));
      int j;
      for(j = 0; j < strlen(columns[i]); j++) {
        printf("%s", tbl_ele[9]);
      }
    }
    printf("%s\n", tbl_ele[0]);

    ssdp_custom_field_s *cf = NULL;
    ssdp_cache = ssdp_cache->first;
    const char no_info[] = "-";
    while(ssdp_cache) {
        cf = get_custom_field(ssdp_cache->ssdp_message, "serialNumber");
        printf("%s %-20s", tbl_ele[1], (cf && cf->contents ? cf->contents : no_info));
        printf("%s %-16s", tbl_ele[1], ssdp_cache->ssdp_message->ip);
        printf("%s %-18s", tbl_ele[1], (ssdp_cache->ssdp_message->mac &&
                                 0 != strcmp(ssdp_cache->ssdp_message->mac, "") ?
                                 ssdp_cache->ssdp_message->mac :
                                 no_info));
        cf = get_custom_field(ssdp_cache->ssdp_message, "modelName");
        printf("%s %-*s", tbl_ele[1], 16, (cf && cf->contents ? cf->contents : no_info));
        cf = get_custom_field(ssdp_cache->ssdp_message, "modelNumber");
        printf("%s %-16s", tbl_ele[1], (cf && cf->contents ? cf->contents : no_info));
        printf("%s\n", tbl_ele[1]);

        horizontal_lines_printed++;

        ssdp_cache = ssdp_cache->next;
    }
  }

  /* Draw the bottom line */
  for(i = 0; i < sizeof(columns) / sizeof(char *); i++) {
    printf("%s",
           (i == 0 ? tbl_ele[4] : tbl_ele[6]));
    int j;
    for(j = 0; j < strlen(columns[i]); j++) {
      printf("%s", tbl_ele[9]);
    }
  }
  printf("%s\n", tbl_ele[3]);
  int width, height;

  get_window_size(&width, &height);
  for(i = 1; i < height - horizontal_lines_printed; i++) {
    printf("%*s\n", width, " ");
  }
}

int main(int argc, char **argv) {
  struct sockaddr_storage notif_client_addr;
  int recvLen = 1;

  signal(SIGTERM, &exit_sig);
  signal(SIGABRT, &exit_sig);
  signal(SIGINT, &exit_sig);

  #ifdef DEBUG___
  PRINT_DEBUG("%sDebug color%s", DEBUG_COLOR_BEGIN, DEBUG_COLOR_END);
  PRINT_DEBUG("%sError color%s", ERROR_COLOR_BEGIN, DEBUG_COLOR_END);
  #endif

  set_default_configuration(&conf);
  if (parse_args(argc, argv, &conf)) {
    free_stuff();
    exit(EXIT_FAILURE);
  }

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
                                 1)) {
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

          /* Add ssdp_message to ssdp_cache
             (this internally checks for duplicates) */
          if(!add_ssdp_message_to_cache(&ssdp_cache, &ssdp_message)) {
            PRINT_ERROR("Failed adding SSDP message to SSDP cache, skipping");
            continue;
          }

          /* Fetch custom fields */
          if(conf.fetch_info && !fetch_custom_fields(ssdp_message)) {
            PRINT_DEBUG("Could not fetch custom fields");
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

static void exit_sig(int param) {
  free_stuff();
  exit(EXIT_SUCCESS);
}

/**
 * Parse a single SSDP header.
 *
 * @param header The location where the parsed result should be stored.
 * @param raw_header The header string to be parsed.
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

// TODO: remove port, it is contained in **pp_address
// TODO: make it not use global variables (conf.interface, conf.ip)
static int send_stuff(const char *url, const char *data,
    const struct sockaddr_storage *da, int port, int timeout) {

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
 * Parses the filter argument.
 *
 * @param raw_filter The raw filter string.
 * @param filters_count The number of filters found.
 * @param filters The parsed filters array.
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
 * Fetches additional info from a UPnP message "Location" header
 * and stores it in the custom_fields in the ssdp_message.
 *
 * @param ssdp_message The message whos "Location" header to use.
 *
 * @return The number of bytes received.
 */
static int fetch_custom_fields(ssdp_message_s *ssdp_message) {
  int bytes_received = 0;
  char *location_header = NULL;
  ssdp_header_s *ssdp_headers = ssdp_message->headers;

  if(ssdp_message->custom_fields) {
    PRINT_DEBUG("Custom info has already been fetched for this device");
    return 1;
  }

  if(!ssdp_headers) {
    PRINT_ERROR("Missing headers, cannot fetch custom info");
    return 0;
  }

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
      SOCKET fetch_sock = setup_socket(conf.use_ipv6,
                 FALSE,
                 FALSE,
                 conf.interface,
                 conf.ip,
                 NULL,
                 NULL,
                 0,
                 FALSE,
                 FALSE);

      if(fetch_sock == SOCKET_ERROR) {
        PRINT_ERROR("fetch_custom_fields(); setup_socket(): (%d) %s", errno, strerror(errno));
        free(ip);
        free(rest);
        free(request);
        free(response);
        return 0;
      }

      if(!set_receive_timeout(fetch_sock, 5) ||
         !set_send_timeout(fetch_sock, 1)) {
        free(ip);
        close(fetch_sock);
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
        close(fetch_sock);
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
      if(connect(fetch_sock, (struct sockaddr*)da, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        PRINT_ERROR("fetch_custom_fields(); connect(): (%d) %s", errno, strerror(errno));
        free(ip);
        close(fetch_sock);
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
      int bytes = send(fetch_sock, request, strlen(request), 0);
      PRINT_DEBUG("sent %d bytes", bytes);
      do {
        bytes = 0;
        bytes = recv(fetch_sock, response + bytes_received, DEVICE_INFO_SIZE - bytes_received, 0);
        bytes_received += bytes;
      } while(bytes > 0);
      PRINT_DEBUG("received %d bytes", bytes_received);
      PRINT_DEBUG("%s", response);
      PRINT_DEBUG("closing socket");
      close(fetch_sock);
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
        ssdp_custom_field_s *cf = NULL;

        int field_length = strlen(field[i]);

        char needle[field_length + 4];
        sprintf(needle, "<%s>", field[i]);
        tmp_pointer = strstr(response, needle);

        if(tmp_pointer) {

          /* Create a new ssdp_custom_field_s */
          cf = (ssdp_custom_field_s *)malloc(sizeof(ssdp_custom_field_s));
          memset(cf, 0, sizeof(ssdp_custom_field_s));

          /* Set 'name' */
          cf->name = (char *)malloc(sizeof(char) * field_length + 1);
          strcpy(cf->name, field[i]);

          /* Parse 'contents' */
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
          continue;
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

      if(ssdp_message->custom_fields) {
        /* End the linked list and reset the pointer to the beginning */
        ssdp_message->custom_fields->next = NULL;
        ssdp_message->custom_fields = ssdp_message->custom_fields->first;
      }

    }

    free(response);
  }

  return bytes_received;
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

  free(saddr);

  return sock;
}

/**
 * Check whether a given IP is a multicast IP
 * (if IPV6 checks for UPnP multicast IP, should be
 * changed to check if multicast at all).
 *
 * @param address The address to check.
 *
 * @return TRUE on success, FALSE otherwise.
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
 * Initializes (allocates neccessary memory) for a SSDP message.
 *
 * @param message The message to initialize.
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
static BOOL create_plain_text_message(char *results, int buffer_size, ssdp_message_s *ssdp_message) {
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
 * @param ssdp_cache_pointer The address of a pointer to a ssdp cache list
 * @param ssdp_message_pointer The ssdp message to be appended to the cache
 *        list.
 *
 * @return TRUE on success, exits on failure.
 */
static BOOL add_ssdp_message_to_cache(ssdp_cache_s **ssdp_cache_pointer, ssdp_message_s **ssdp_message_pointer) {
  ssdp_message_s *ssdp_message = *ssdp_message_pointer;
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

    /* Check for duplicate and update it if found */
    while(ssdp_cache) {
      if(0 == strcmp(ssdp_message->ip, ssdp_cache->ssdp_message->ip)) {
        /* Found a duplicate, update existing instead */
        PRINT_DEBUG("Found duplicate SSDP message (IP '%s'), updating", ssdp_cache->ssdp_message->ip);
        strcpy(ssdp_cache->ssdp_message->datetime, ssdp_message->datetime);
        if(strlen(ssdp_cache->ssdp_message->mac) < 1) {
          PRINT_DEBUG("Field MAC was empty, updating to '%s'", ssdp_message->mac);
          strcpy(ssdp_cache->ssdp_message->mac, ssdp_message->mac);
        }
        // TODO: make it update all existing fields before freeing it...
        PRINT_DEBUG("Trowing away the duplicate ssdp message and using existing instead");
        free_ssdp_message(ssdp_message_pointer);
        /* Point to the existing ssdp message */
        *ssdp_message_pointer = ssdp_cache->ssdp_message;
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
      ssdp_cache = ssdp_cache->next;
    }
    PRINT_DEBUG("Moved to the last element in the cache list");
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
 * Frees all the elements in the ssdp messages list.
 *
 * @param ssdp_cache_pointer The ssdp cache list to be cleared.
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

