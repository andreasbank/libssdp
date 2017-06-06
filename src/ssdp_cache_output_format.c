/** \file ssdp_cache_output_format.c
 * Functions for converting the SSDP cache to different output formats.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#include <stdlib.h>
#include <string.h>

#include "common_definitions.h"
#include "log.h"
#include "ssdp_cache_output_format.h"
#include "ssdp_message.h"

#define SSDP_CUSTOM_FIELD_SERIALNUMBER "serialNumber"
#define SSDP_CUSTOM_FIELD_FRIENDLYNAME "friendlyName"
#define ONELINE_ANSI_COLOR_GREEN   "\x1b[1;32m"
#define ONELINE_ANSI_COLOR_GREEN_SIZE 10
#define ONELINE_ANSI_COLOR_RESET   "\x1b[0m"
#define ONELINE_ANSI_COLOR_RESET_SIZE 7

unsigned int cache_to_json(ssdp_cache_s *ssdp_cache, char *json_buffer,
    unsigned int json_buffer_size) {
  unsigned int used_buffer = 0;

  /* Point at the beginning */
  ssdp_cache = ssdp_cache->first;

  /* For every element in the ssdp cache */
  used_buffer = snprintf(json_buffer, json_buffer_size, "root {\n");

  while(ssdp_cache) {
    used_buffer += to_json(ssdp_cache->ssdp_message, FALSE,
        (json_buffer + used_buffer), (json_buffer_size - used_buffer));
    ssdp_cache = ssdp_cache->next;
  }
  used_buffer += snprintf((json_buffer + used_buffer),
      (json_buffer_size - used_buffer), "}\n");

  return used_buffer;
}

unsigned int cache_to_xml(ssdp_cache_s *ssdp_cache, char *xml_buffer,
    unsigned int xml_buffer_size) {
  unsigned int used_buffer = 0;

  if(NULL == ssdp_cache) {
    PRINT_ERROR("No valid SSDP cache given (NULL)");
    return 0;
  }

  /* Point at the beginning */
  ssdp_cache = ssdp_cache->first;

  /* For every element in the ssdp cache */
  used_buffer = snprintf(xml_buffer, xml_buffer_size,
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<root>\n");

  while(ssdp_cache) {
    PRINT_DEBUG("cache_to_xml(): buffer used: %d; left: %d", used_buffer,
        xml_buffer_size - used_buffer);
    PRINT_DEBUG("start with with '%s'", ssdp_cache->ssdp_message->ip);
    used_buffer += to_xml(ssdp_cache->ssdp_message, FALSE,
        (xml_buffer + used_buffer), (xml_buffer_size - used_buffer));
    PRINT_DEBUG("done with '%s'", ssdp_cache->ssdp_message->ip);
    ssdp_cache = ssdp_cache->next;
  }
  used_buffer += snprintf((xml_buffer + used_buffer),
      (xml_buffer_size - used_buffer), "</root>\n");

  return used_buffer;
}

unsigned int to_json(const ssdp_message_s *ssdp_message, BOOL full_json,
    char *json_buffer, int json_buffer_size) {
  int used_length = 0;

  // TODO: write it!

  return used_length;
}

unsigned int to_xml(const ssdp_message_s *ssdp_message, BOOL full_xml,
    char *xml_buffer, int xml_buffer_size) {
  int used_length = 0;

  if (!xml_buffer) {
    PRINT_ERROR("to_xml(): No XML message buffer specified");
    return -1;
  } else if (ssdp_message == NULL) {
    PRINT_ERROR("to_xml(): No SSDP message specified");
    return -1;
  }

  memset(xml_buffer, '\0', sizeof(char) * xml_buffer_size);
  if (full_xml) {
    used_length = snprintf(xml_buffer, xml_buffer_size,
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<root>\n");
  }

  PRINT_DEBUG("Setting upnp xml-fields");
  used_length += snprintf(xml_buffer + used_length,
      xml_buffer_size - used_length, "\t<message length=\"%d\">\n",
      ssdp_message->message_length);
  used_length += snprintf(xml_buffer + used_length,
      xml_buffer_size - used_length, "\t\t<mac>\n\t\t\t%s\n\t\t</mac>\n",
      ssdp_message->mac);
  used_length += snprintf(xml_buffer + used_length,
      xml_buffer_size - used_length, "\t\t<ip>\n\t\t\t%s\n\t\t</ip>\n",
      ssdp_message->ip);
  used_length += snprintf(xml_buffer + used_length, xml_buffer_size - used_length,
      "\t\t<request protocol=\"%s\">\n\t\t\t%s\n\t\t</request>\n",
      ssdp_message->protocol, ssdp_message->request);
  used_length += snprintf(xml_buffer + used_length,
      xml_buffer_size - used_length,
      "\t\t<datetime>\n\t\t\t%s\n\t\t</datetime>\n", ssdp_message->datetime);

  if (ssdp_message->custom_fields) {

    ssdp_custom_field_s *cf = ssdp_message->custom_fields->first;

    /*
     * The whole needed size is calculated as:
     * leading headers string: +29
     * each header constant string +50
     * each header name string +X
     * each header contents string +Y
     * tailing header tailing string +19
     */

    /* Leading (29) and tailing (13) string sizes combined */
    int needed_size = 48;

    PRINT_DEBUG("Setting custom xml-fields");
    /* Calculate needed buffer size */
    while (cf) {

      /* Each header combined (46 + X + Y + 18) string size */
      needed_size += strlen(cf->name) + strlen(cf->contents) + 50;
      cf = cf->next;
      PRINT_DEBUG("Done calculating buffer");

    }
    cf = ssdp_message->custom_fields->first;

    /* Check if buffer has enought room */
    PRINT_DEBUG("to_xml(): XML custom fields buffer left: %d, buffer_needed: "
        "%d", (xml_buffer_size - used_length), needed_size);
    if ((xml_buffer_size - used_length) < needed_size) {
      PRINT_ERROR("to_xml(): Not enought buffer space left to convert the "
          "SSDP message custom fields to XML, skipping\n");
    } else {

      /* Convert to XML format */
      used_length += snprintf(xml_buffer + used_length, xml_buffer_size - used_length,
      "\t\t<custom_fields count=\"%d\">\n", ssdp_message->custom_field_count);

      while (cf) {
        PRINT_DEBUG("Setting:");
        PRINT_DEBUG("'%s'", cf->name);
        used_length += snprintf(xml_buffer + used_length,
                                xml_buffer_size - used_length,
                                "\t\t\t<custom_field name=\"%s\">\n\t\t\t\t%s\n\t\t\t</custom_field>\n",
                                cf->name,
                                cf->contents);
        cf = cf->next;
        PRINT_DEBUG("Done with this one");
      }

      cf = NULL;
      used_length += snprintf(xml_buffer + used_length,
                              xml_buffer_size - used_length,
                              "\t\t</custom_fields>\n");
      PRINT_DEBUG("to_xml(): XML custom-fields bytes used: %d",
          (int)strlen(xml_buffer));
    }

  }

  if (ssdp_message->headers) {

    ssdp_header_s *h = ssdp_message->headers->first;

    /*
     * The whole needed size is calculated as:
     * leading headers string: +23
     * each header leading string +46
     * each header type as two digits
     * each header type as string +X
     * each header contents string +Y
     * each header tailing string +18
     * tailing header tailing string +13
     */

    /* Leading (23) and tailing (13) string sizes combined */
    int needed_size = 36;

    /* Calculate needed buffer size */
    while (h) {

      /* Each header combined (46 + X + Y + 18) string size */
      needed_size += (h->unknown_type ?
                        strlen(h->unknown_type) :
                        strlen(get_header_string(h->type, h))) + strlen(h->contents) + 64;
      h = h->next;

    }
    h = ssdp_message->headers->first;

    /* Check if buffer has enought space */
    PRINT_DEBUG("buffer left: %d, buffer needed: %d",
        (xml_buffer_size - used_length), needed_size);
    if ((xml_buffer_size - used_length) < needed_size) {
      PRINT_ERROR("to_xml(): Not enought buffer space left to convert the "
          "SSDP message headers to XML, skipping\n");
    } else {

      used_length += snprintf(xml_buffer + used_length,
          xml_buffer_size - used_length, "\t\t<headers count=\"%d\">\n",
          (unsigned int)ssdp_message->header_count);

      while (h) {
        used_length += snprintf(xml_buffer + used_length,
            xml_buffer_size - used_length,
            "\t\t\t<header typeInt=\"%d\" typeStr=\"%s\">\n", h->type,
            get_header_string(h->type, h));
        used_length += snprintf(xml_buffer + used_length,
            xml_buffer_size - used_length, "\t\t\t\t%s\n\t\t\t</header>\n",
            h->contents);
        h = h->next;
      }
      h = NULL;

      used_length += snprintf(xml_buffer + used_length,
          xml_buffer_size - used_length, "\t\t</headers>\n");
    }

  }

  used_length += snprintf(xml_buffer + used_length,
      xml_buffer_size - used_length, "\t</message>\n");

  if (full_xml) {
    used_length += snprintf(xml_buffer + used_length,
        xml_buffer_size - used_length, "</root>\n");
  }

  return used_length;
}

char *to_oneline(const ssdp_message_s *message, BOOL monochrome) {
  const ssdp_custom_field_s *custom_field_id = NULL;
  const ssdp_custom_field_s *custom_field_model = NULL;
  char *oneline = NULL;

  if (!message)
    return NULL;

  custom_field_id = get_custom_field(message, SSDP_CUSTOM_FIELD_SERIALNUMBER);
  /* 7 is the needed characters for "(no id)" in the sprintf() below */
  int id_size =
      custom_field_id ? strlen(custom_field_id->contents) : 7;

  /* 8 is the needed characters for "(no MAC)" in the sprintf() below */
  int mac_size = message->mac ? strlen(message->mac) : 8;

  custom_field_model = get_custom_field(message,
      SSDP_CUSTOM_FIELD_FRIENDLYNAME);
  /* 10 is the needed characters for "(no model)" in the sprintf() below */
  int model_size =
      custom_field_model ? strlen(custom_field_model->contents) : 10;

  /* Set color control-codes  */
  const char* start_color = monochrome ? "" : ONELINE_ANSI_COLOR_GREEN;
  const char* end_color = monochrome ? "" : ONELINE_ANSI_COLOR_RESET;
  int start_color_size = monochrome ? 0 : ONELINE_ANSI_COLOR_GREEN_SIZE;
  int end_color_size = monochrome ? 0 : ONELINE_ANSI_COLOR_RESET_SIZE;

  /* 10 is the extra characters in the sprintf() below plus a null-term */
  int oneline_size = start_color_size + id_size + end_color_size +
      strlen(message->ip) + mac_size + model_size + 10;
  oneline = malloc(oneline_size + 1);

  sprintf(oneline, "%s%s%s - %s - %s - %s", start_color,
      custom_field_id ? custom_field_id->contents : "(no ID)",
      end_color, message->ip, message->mac ? message->mac : "(no MAC)",
      custom_field_model ? custom_field_model->contents :
      "(no model)");

  return oneline;
}
