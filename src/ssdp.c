#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "ssdp.h"
#include "ssdp_static_defs.h"

unsigned char get_header_type(const char *header_string) {
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
 * Returns the appropriate string representation of the header type
 *
 * @param const unsigned int header_type The header type (int) to be looked up
 *
 * @return char * A string representing the header type
 */
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

  if((header_type == 0 || header_type > (sizeof(header_strings)/sizeof(char *) - 1)) && header != NULL && header->unknown_type != NULL) {
    return header->unknown_type;
  }

  return header_strings[header_type];
}

