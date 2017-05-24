#ifndef __SSDP_CACHE_H__
#define __SSDP_CACHE_H__

#include "configuration.h"
#include "ssdp_message.h"
#include "sys/socket.h"

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
 * Adds a ssdp message to a ssdp messages list. If the list hasn't been
 * initialized then it is initialized first.
 *
 * @param ssdp_cache_pointer The address of a pointer to a ssdp cache list.
 * @param ssdp_message_pointer The ssdp message to be appended to the cache
 *        list.
 *
 * @return TRUE on success, exits on failure.
 */
BOOL add_ssdp_message_to_cache(ssdp_cache_s **ssdp_cache_pointer,
    ssdp_message_s **ssdp_message_pointer);

/**
 * Send and free the passed SSDP cache.
 *
 * @param conf The global configuration.
 * @param ssdp_cache_pointer The cache.
 * @param url The url to send the cache items to.
 * @param sockaddr_recipient The address of the recipient of the cache items.
 * @param port The port of the recipient address.
 * @param timeout The timeout for the request.
 *
 * @return 0 on success, errno on failure.
 */
int flush_ssdp_cache(configuration_s *conf, ssdp_cache_s **ssdp_cache_pointer,
    const char *url, struct sockaddr_storage *sockaddr_recipient, int port,
    int timeout);

#endif /* __SSDP_CACHE_H__ */
