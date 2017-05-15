#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "ssdp_filter.h"
#include "ssdp_message.h"
#include "string_utils.h"

void free_ssdp_filters_factory(filters_factory_s *factory) {
  if (factory) {
    if (factory->filters) {
      int fc;
      for(fc = 0; fc < (factory->filters_count); fc++) {
        if (factory->filters[fc].header) {
          free(factory->filters[fc].header);
          factory->filters[fc].header = NULL;
        }
        if ((factory->filters + fc)->value) {
          free(factory->filters[fc].value);
          factory->filters[fc].value = NULL;
        }
      }
      free(factory->filters);
      factory->filters = NULL;
    }
    free(factory);
    factory = NULL;
  }
}

void parse_filters(char *raw_filter, filters_factory_s **filters_factory,
    BOOL print_filters) {
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
 * Check if the SSDP message needs to be filtered-out (dropped)
 *
 * @param ssdp_message The SSDP message to be checked
 * @param filters_factory The filters to check against
 */
BOOL filter(ssdp_message_s *ssdp_message, filters_factory_s *filters_factory) {

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

