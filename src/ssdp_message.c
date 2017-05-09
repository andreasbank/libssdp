#include <stdlib.h>

#include "common_definitions.h"
#include "ssdp.h"
#include "log.h"

/**
 * Frees all neccessary allocations in a ssdp_message_s.
 *
 * @param ssdp_message_s *message The message to free allocations for.
 */
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

