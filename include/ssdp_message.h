#ifndef __SSDP_MESSAGE_H__
#define __SSDP_MESSAGE_H__

/**
 * Frees all neccessary allocations in a ssdp_message_s.
 *
 * @param ssdp_message_s *message The message to free allocations for.
 */
void free_ssdp_message(ssdp_message_s **message_pointer);

#endif /* __SSDP_MESSAGE_H__ */

