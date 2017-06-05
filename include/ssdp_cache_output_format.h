/** \file ssdp_cache_output_format.h
 * Header file for ssdp_output_format.c.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __OUTPUT_FORMAT_H__
#define __OUTPUT_FORMAT_H__

#include "ssdp_cache.h"
#include "ssdp_message.h"

/**
 * Convert a ssdp cache list (multiple ssdp_messages) to a single JSON blob
 *
 * @param ssdp_cache The SSDP messages to convert
 * @param json_message The buffer to write to
 * @param json_buffer_size The buffer size
 *
 * @return The number of byter written
 */
unsigned int cache_to_json(ssdp_cache_s *ssdp_cache, char *json_buffer,
    unsigned int json_buffer_size);

/**
 * Convert a ssdp cache list (multiple ssdp_messages) to a single XML blob
 *
 * @param ssdp_cache The SSDP messages to convert
 * @param xml_message The buffer to write to
 * @param xml_buffer_size The buffer size
 *
 * @return The number of byter written
 */
unsigned int cache_to_xml(ssdp_cache_s *ssdp_cache, char *xml_buffer,
    unsigned int xml_buffer_size);

/**
* Converts a UPnP message to a JSON string
*
* @param ssdp_message The message to be converted
* @param full_json Whether to contain the JSON opening hash
* @param json_buffer The JSON buffer to write the JSON document to
* @param json_size The size of the passed buffer (json_buffer)
*/
unsigned int to_json(const ssdp_message_s *, BOOL full_json,
    char *json_buffer, int json_buffer_size);

/**
* Converts a UPnP message to a XML string
*
* @param ssdp_message The message to be converted
* @param full_xml Whether to contain the XML declaration and the root tag
* @param xml_buffer The XML buffer to write the XML document to
* @param xml_buffer_size The size of the passed buffer (xml_buffer)
*/
unsigned int to_xml(const ssdp_message_s *ssdp_message, BOOL full_xml,
    char *xml_buffer, int xml_buffer_size);

/**
 * Return an oneline string with the message ID, IP and (if present) the model.
 *
 * @param message The ssdp message to convert.
 * @param monochrome Use monochrome (no color) when generating onelines.
 *
 * @return An oneline string with the message ID, IP and
 *         (if present) the model.
 */
char *to_oneline(const ssdp_message_s *message, BOOL monochrome);

#endif /* __OUTPUT_FORMAT_H__ */
