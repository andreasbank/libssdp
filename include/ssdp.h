#ifndef __SSDP_H__
#define __SSDP_H__

// TODO: move daemon port to daemon.h ?
#define DAEMON_PORT           43210   // port the daemon will listen on
#define NOTIF_RECV_BUFFER     2048 // notification receive-buffer
#define XML_BUFFER_SIZE       2048 // XML buffer/container string
#define LISTEN_QUEUE_LENGTH   2
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

/**
 * Returns the appropriate unsigned char (number) representation of the header
 * string.
 *
 * @param header_string The header string to be looked up.
 *
 * @return A unsigned char representing the header type.
 */
unsigned char get_header_type(const char *header_string);

/**
 * Returns the appropriate string representation of the header type.
 *
 * @param header_type The header type (int) to be looked up.
 *
 * @return A string representing the header type.
 */
const char *get_header_string(const unsigned int header_type,
    const ssdp_header_s *header);

#endif /* __SSDP_H__ */
