/** \file ssdp_filter.h
 * Header file for ssdp_filter.c.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __SSDP_FILTER_H__
#define __SSDP_FILTER_H__

#include "ssdp_message.h"

/** A filter. */
typedef struct filter_struct {
  /** The filter header name (key). */
  char *header;
  /** The filter value. */
  char *value;
} filter_s;

/** Filters factory. */
typedef struct filters_factory_struct {
  /** Filter list. */
  filter_s *filters;
  /** The first filter in the list. */
  filter_s *first_filter;
  /** The filters string, as passed in to the program/lib. */
  char *raw_filters;
  /** The number of filters. */
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
 *
 * @return TRUE when message does not match the filter and needs to be dropped,
 *         FALSE if the message matches the filter and needs to be kept.
 */
BOOL filter(ssdp_message_s *ssdp_message, filters_factory_s *filters_factory);

#endif /* __SSDP_FILTER_H__ */
