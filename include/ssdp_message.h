#ifndef __SSDP_MESSAGE_H__
#define __SSDP_MESSAGE_H__

#include "configuration.h"

// TODO: move daemon port to daemon.h ?
#define DAEMON_PORT           43210   // port the daemon will listen on
#define XML_BUFFER_SIZE       2048 // XML buffer/container string
#define DEVICE_INFO_SIZE      16384
#define MULTICAST_TIMEOUT     2

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

/**
 * Searches the SSDP message custom-fields for the given custom-field name
 *
 * @param ssdp_message The SSDP message to search in
 * @param custom_field The Custom Field to search for
 *
 * @return Returns the found custom-field or NULL
 */
ssdp_custom_field_s *get_custom_field(ssdp_message_s *ssdp_message,
    const char *custom_field);

/**
 * Fetches additional info from a UPnP message "Location" header
 * and stores it in the custom_fields in the ssdp_message.
 *
 * @param conf The global configuration.
 * @param ssdp_message The message whos "Location" header to use.
 *
 * @return The number of bytes received.
 */
int fetch_custom_fields(configuration_s *conf, ssdp_message_s *ssdp_message);

/**
 * Returns the appropriate string representation of the header type.
 *
 * @param header_type The header type (int) to be looked up.
 *
 * @return A string representing the header type.
 */
const char *get_header_string(const unsigned int header_type,
    const ssdp_header_s *header);

/**
 * Searches the SSDP message custom-fields for the given custom-field name
 *
 * @param ssdp_message The SSDP message to search in
 * @param custom_field The Custom Field to search for
 *
 * @return Returns the found custom-field or NULL
 */
ssdp_custom_field_s *get_custom_field(ssdp_message_s *ssdp_message,
    const char *custom_field);

/**
 * Initializes (allocates neccessary memory) for a SSDP message.
 *
 * @param message The message to initialize.
 */
BOOL init_ssdp_message(ssdp_message_s **message_pointer);

/**
 * Parse a SSDP message.
 *
 * @param message The location where the parsed result should be stored.
 * @param ip The IP address of the sender.
 * @param mac The MAC address of the sender.
 * @param int *message_length The message length.
 * @param raw_message The message string to be parsed.
 */
BOOL build_ssdp_message(ssdp_message_s *message, char *ip, char *mac,
    int message_length, const char *raw_message);

/**
 * Frees all neccessary allocations in a ssdp_message_s.
 *
 * @param message The message to free allocations for.
 */
void free_ssdp_message(ssdp_message_s **message_pointer);

#endif /* __SSDP_MESSAGE_H__ */

