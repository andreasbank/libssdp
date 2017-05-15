#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common_definitions.h"
#include "configuration.h"
#include "log.h"
#include "socket_helpers.h"
#include "ssdp_message.h"
#include "ssdp_cache.h"
#include "ssdp_message.h"
#include "ssdp_cache_output_format.h"

/**
 * Create a plain-text message.
 *
 * @param results The buffer to store the results in.
 * @param buffer_size The size of the buffer 'results'.
 * @param message The message to convert.
 */
static BOOL create_plain_text_message(char *results, int buffer_size,
    ssdp_message_s *ssdp_message) {
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
        (ssdp_message->mac != NULL ? ssdp_message->mac :
        "(Could not be determined)"));
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

// TODO: remove port, it is contained in **pp_address
// TODO: make it not use global variables (conf->interface, conf->ip)
// TODO: add doxygen
static int send_stuff(const char *url, const char *data,
    const struct sockaddr_storage *da, int port, int timeout,
    configuration_s *conf) {

  if(url == NULL || strlen(url) < 1) {
    PRINT_ERROR("send_stuff(): url not set");
    return FALSE;
  }

  /* Create socket */
  PRINT_DEBUG("send_stuff(): creating socket");

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

  SOCKET send_sock = setup_socket(&sock_conf);
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
  inet_ntop(da->ss_family, (da->ss_family == AF_INET ?
      (void *)&((struct sockaddr_in *)da)->sin_addr :
      (void *)&((struct sockaddr_in6 *)da)->sin6_addr), tmp_ip,
      IPv6_STR_MAX_SIZE);
  PRINT_DEBUG("send_stuff(): connecting to destination (%s; ss_family = "
      "%s [%d])", tmp_ip, (da->ss_family == AF_INET ? "AF_INET" : "AF_INET6"),
      da->ss_family);
  free(tmp_ip);
  tmp_ip = NULL;
  #endif
  if(connect(send_sock, (struct sockaddr*)da, sizeof(struct sockaddr)) ==
      SOCKET_ERROR) {
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
  inet_ntop(da->ss_family, (da->ss_family == AF_INET ?
      (void *)&((struct sockaddr_in *)da)->sin_addr :
      (void *)&((struct sockaddr_in6 *)da)->sin6_addr), ip, IPv6_STR_MAX_SIZE);

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
 * Frees all the elements in the ssdp messages list.
 *
 * @param ssdp_cache_pointer The ssdp cache list to be cleared.
 */
static void free_ssdp_cache(ssdp_cache_s **ssdp_cache_pointer) {
  ssdp_cache_s *ssdp_cache = NULL;
  ssdp_cache_s *next_cache = NULL;

  /* Sanity check */
  if (!ssdp_cache_pointer) {
    PRINT_ERROR("No ssdp cache list given");
    return;
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

BOOL add_ssdp_message_to_cache(ssdp_cache_s **ssdp_cache_pointer,
    ssdp_message_s **ssdp_message_pointer) {
  ssdp_message_s *ssdp_message = *ssdp_message_pointer;
  ssdp_cache_s *ssdp_cache = NULL;

  /* Sanity check */
  if (!ssdp_cache_pointer) {
    PRINT_ERROR("No ssdp cache list given");
    return FALSE;
  }

  /* Initialize the list if needed */
  if (!(*ssdp_cache_pointer)) {
    PRINT_DEBUG("Initializing the SSDP cache");
    *ssdp_cache_pointer = (ssdp_cache_s *) malloc(sizeof(ssdp_cache_s));
    if (!(*ssdp_cache_pointer)) {
      PRINT_ERROR("Failed to allocate memory for the ssdp cache list");
      return FALSE;
    }
    memset(*ssdp_cache_pointer, 0, sizeof(ssdp_cache_s));

    /* Set the ->first to point to this element */
    (*ssdp_cache_pointer)->first = *ssdp_cache_pointer;

    /* Set ->new to point to NULL */
    (*ssdp_cache_pointer)->next = NULL;

    /* Set the counter to 0 */
    (*ssdp_cache_pointer)->ssdp_messages_count =
        (unsigned int *)malloc(sizeof(unsigned int));
    *(*ssdp_cache_pointer)->ssdp_messages_count = 0;
  }
  else {

    /* Point to the begining of the cache list */
    ssdp_cache = (*ssdp_cache_pointer)->first;

    /* Check for duplicate and update it if found */
    while (ssdp_cache) {
      if (0 == strcmp(ssdp_message->ip, ssdp_cache->ssdp_message->ip)) {
        /* Found a duplicate, update existing instead */
        PRINT_DEBUG("Found duplicate SSDP message (IP '%s'), updating",
            ssdp_cache->ssdp_message->ip);
        strcpy(ssdp_cache->ssdp_message->datetime, ssdp_message->datetime);
        if(strlen(ssdp_cache->ssdp_message->mac) < 1) {
          PRINT_DEBUG("Field MAC was empty, updating to '%s'",
              ssdp_message->mac);
          strcpy(ssdp_cache->ssdp_message->mac, ssdp_message->mac);
        }
        // TODO: make it update all existing fields before freeing it...
        PRINT_DEBUG("Trowing away the duplicate ssdp message and using "
            "existing instead");
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
  PRINT_DEBUG("SSDP cache counter increased to %d",
      *ssdp_cache->ssdp_messages_count);
  ssdp_cache->ssdp_message = ssdp_message;

  /* Set the passed ssdp_cache to point to the last element */
  *ssdp_cache_pointer = ssdp_cache;

  return TRUE;
}

/**
 * Send and free the passed SSDP cache
 */
BOOL flush_ssdp_cache(configuration_s *conf, ssdp_cache_s **ssdp_cache_pointer,
    const char *url, struct sockaddr_storage *sockaddr_recipient, int port,
    int timeout) {
  ssdp_cache_s *ssdp_cache = *ssdp_cache_pointer;
  int results_size = *ssdp_cache->ssdp_messages_count * XML_BUFFER_SIZE;
  char results[results_size];

  /* If -j then convert all messages to one JSON blob */
  if(conf->json_output) {
    if(1 > cache_to_json(ssdp_cache, results, results_size)) {
      PRINT_ERROR("Failed creating JSON blob from ssdp cache");
      return FALSE;
    }
  }

  /* If -x then convert all messages to one XML blob */
  if(conf->xml_output) {
    if(1 > cache_to_xml(ssdp_cache, results, results_size)) {
      PRINT_ERROR("Failed creating XML blob from ssdp cache");
      return FALSE;
    }
  }

  // TODO: make it create a list instead of single plain message
  else if(!create_plain_text_message(results, XML_BUFFER_SIZE,
      ssdp_cache->ssdp_message)) {
    PRINT_ERROR("Failed creating plain-text message");
    return FALSE;
  }

  /* Send the converted cache list to the recipient (-a) */
  send_stuff(url, results, sockaddr_recipient, port, timeout, conf);

  /* When the ssdp_cache has been sent
     then free/empty the cache list */
  free_ssdp_cache(ssdp_cache_pointer);

  return TRUE;
}

