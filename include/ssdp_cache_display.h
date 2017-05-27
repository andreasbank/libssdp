/** \file ssdp_cache_display.h
 * Header file for ssdp_cache_display.c.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __SSDP_CACHE_DISPLAY_H__
#define __SSDP_CACHE_DISPLAY_H__

#include "ssdp_cache.h"

/**
 * Displays the SSDP cache list as a table in the terminal.
 *
 * @param ssdp_cache The SSDP cache to display.
 * @param draw_asci Draw the table with ASCII characters only as oposed to
 *        UTF8.
 */
void display_ssdp_cache(ssdp_cache_s *ssdp_cache, BOOL draw_asci);

#endif /* __SSDP_CACHE_DISPLAY_H__ */
