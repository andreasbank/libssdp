#ifndef __SSDP_FILTER_H__
#define __SSDP_FILTER_H__

#include "ssdp_message.h"

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

/**
 * Free the given ssdp filter factory.
 *
 * @param factory The ssdp filter factory to free.
 */
void free_ssdp_filters_factory(filters_factory_s *factory);

/**
 * Parses the filter argument.
 *
 * @param raw_filter The raw filter string.
 * @param filters_count The number of filters found.
 * @param filters The parsed filters array.
 */
void parse_filters(char *raw_filter, filters_factory_s **filters_factory,
    BOOL print_filters);

/**
 * Check if the SSDP message needs to be filtered-out (dropped).
 *
 * @param ssdp_message The SSDP message to be checked.
 * @param filters_factory The filters to check against.
 */
BOOL filter(ssdp_message_s *ssdp_message, filters_factory_s *filters_factory);

#endif /* __SSDP_FILTER_H__ */
