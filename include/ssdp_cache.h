/** \file ssdp_cache.h
 * Header file for ssdp_cache.c.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __SSDP_CACHE_H__
#define __SSDP_CACHE_H__

#include "configuration.h"
#include "ssdp_message.h"
#include "sys/socket.h"

/**
 * The ssdp_message_s cache that
 * acts as a buffer for sending
 * bulks of messages instead of spamming;
 * *ssdp_message should always point to
 * the last ssdp_message in the buffer.
 */
typedef struct ssdp_cache_struct {
  /** A pointer to the fist cache element. */
  struct ssdp_cache_struct *first;
  /** The message in this cache element. */
  ssdp_message_s *ssdp_message;
  /** A pointer to the next cache element. */
  struct ssdp_cache_struct *next;
  /** A pointer to the total number of cache elements in the list. */
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
 * @param conf The configuration to use.
 * @param ssdp_cache_pointer The SSDP cache to send and delete (flush).
 * @param url The URL (without the protocol and IP) to send the data to.
 * @param sockaddr_recipient The socket address to send to.
 * @param port The port to send to.
 * @param timeout The send-timeout to set.
 *
 * @return TRUE on success, FALSE otherwise.
 */
int flush_ssdp_cache(configuration_s *conf, ssdp_cache_s **ssdp_cache_pointer,
    const char *url, struct sockaddr_storage *sockaddr_recipient, int port,
    int timeout);

#endif /* __SSDP_CACHE_H__ */
