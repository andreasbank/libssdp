/** \file ssdp_message.c
 * SSDP message
 */

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// TODO: move network knowledge to separate file
#include <unistd.h> // close()
#include <arpa/inet.h> // inet_pton()

#include "common_definitions.h"
#include "configuration.h"
#include "net_definitions.h"
#include "net_utils.h"
#include "socket_helpers.h"
#include "ssdp_message.h"
#include "ssdp_static_defs.h"
#include "string_utils.h"
#include "log.h"

/**
 * Returns the appropriate unsigned char (number) representation of the header
 * string.
 *
 * @param header_string The header string to be looked up.
 *
 * @return A unsigned char representing the header type.
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

  for(i = 0; i < headers_size; i++) {
    if(strcmp(header_lower, header_strings[i]) == 0) {
      free(header_lower);
      return (unsigned char)i;
    }
  }
  free(header_lower);
  return (unsigned char)SSDP_HEADER_UNKNOWN;
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
  header_name = (char *)malloc(header_name_length + 1);
  memset(header_name, '\0', header_name_length + 1);
  strncpy(header_name, raw_header, header_name_length);

  header->type = (unsigned char)get_header_type(header_name);
  if(header->type == SSDP_HEADER_UNKNOWN) {
    header->unknown_type = malloc(header_name_length + 1);
    memset(header->unknown_type, '\0', header_name_length + 1);
    strcpy(header->unknown_type, header_name);
  }
  else {
    header->unknown_type = NULL;
  }

  raw_header_length = strlen(raw_header);
  header_contents_length = raw_header_length - header_name_length + 2;
  header->contents = (char *)malloc(header_contents_length);
  memset(header->contents, '\0', header_contents_length);
  // avoid ": " in header contents
  strcpy(header->contents, &raw_header[header_name_length + 1 +
      (raw_header[header_name_length + 2] == ' ' ? 1 : 0)]);
  free(header_name);
}

ssdp_custom_field_s *get_custom_field(const ssdp_message_s *ssdp_message,
    const char *custom_field) {
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

int fetch_custom_fields(configuration_s *conf, ssdp_message_s *ssdp_message) {
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

      socket_conf_s sock_conf = {
        conf->use_ipv6,
        FALSE,
        FALSE,
        conf->interface,
        conf->ip,
        NULL,
        NULL,
        0,
        FALSE,
        0,
        FALSE,
        conf->ttl,
        conf->enable_loopback
      };
      SOCKET fetch_sock = setup_socket(&sock_conf);

      if(fetch_sock == SOCKET_ERROR) {
        PRINT_ERROR("fetch_custom_fields(); setup_socket(): (%d) %s", errno, strerror(errno));
        free(ip);
        free(rest);
        free(request);
        free(response);
        return 0;
      }

      if(set_receive_timeout(fetch_sock, 5) ||
         set_send_timeout(fetch_sock, 1)) {
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
      da->ss_family = conf->use_ipv6 ? AF_INET6 : AF_INET;
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

const char *get_header_string(const unsigned int header_type,
    const ssdp_header_s *header) {
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

  if((header_type == 0 ||
      (header_type > (sizeof(header_strings)/sizeof(char *) - 1))) &&
      header != NULL && header->unknown_type != NULL) {
    return header->unknown_type;
  }

  return header_strings[header_type];
}

BOOL init_ssdp_message(ssdp_message_s **message_pointer) {
  if(NULL == *message_pointer) {
    *message_pointer = malloc(sizeof(ssdp_message_s));
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

BOOL build_ssdp_message(ssdp_message_s *message, char *ip, char *mac,
    int message_length, const char *raw_message) {
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

void free_ssdp_message(ssdp_message_s **message_pointer) {
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

