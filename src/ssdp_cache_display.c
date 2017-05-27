/** \file ssdp_cache_display.c
 * Functions for displaying the SSDP cache.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "common_definitions.h"
#include "log.h"
#include "ssdp_cache.h"
#include "ssdp_cache_output_format.h"

/**
 * Read the terminal width and height.
 *
 * @param width The buffer to set the width in.
 * @param height The buffer to set the height in.
 */
static void get_window_size(int *width, int *height) {
  struct winsize w;
  ioctl(0, TIOCGWINSZ, &w);

  *width = w.ws_col;
  *height = w.ws_row;
}

/**
 * Moves the terminal cursor to the given coords.
 *
 * @param row The row to move the cursor to.
 * @param col The column to move the cursor to.
 */
static void move_cursor(int row, int col) {
  printf("\033[%d;%dH", row, col);
}

void display_ssdp_cache(ssdp_cache_s *ssdp_cache, BOOL draw_asci) {
  int horizontal_lines_printed = 4;
  const char **tbl_ele = NULL;

  const char *single_line_table_elements[] = {
    "┤", // 185 "\e(0\x75\e(B"
    "│", // 186 "\e(0\x78\e(B"
    "┐", // 187 "\e(0\x6b\e(B"
    "┘", // 188 "\e(0\x6a\e(B"
    "└", // 200 "\e(0\x6d\e(B"
    "┌", // 201 "\e(0\x6c\e(B"
    "┴", // 202 "\e(0\x76\e(B"
    "┬", // 203 "\e(0\x77\e(B"
    "├", // 204 "\e(0\x74\e(B"
    "─", // 205 "\e(0\x71\e(B"
    "┼"  // 206 "\e(0\x6e\e(B"
  };

  const char *asci_table_elements[] = {
    "+",
    "|",
    "+",
    "+",
    "+",
    "+",
    "+",
    "+",
    "+",
    "-",
    "+",
  };

  const char *columns[] = {
    " ID                  ",
    " IPv4            ",
    " MAC               ",
    " Model           ",
    " Version         "
  };

  if (draw_asci) {
    tbl_ele = asci_table_elements;
  } else {
    tbl_ele = single_line_table_elements;
  }

  move_cursor(0, 0);

  /* Draw the topmost line */
  int i;
  for (i = 0; i < sizeof(columns) / sizeof(char *); i++) {
    printf("%s",
           (i == 0 ? tbl_ele[5] : tbl_ele[7]));
    int j;
    for (j = 0; j < strlen(columns[i]); j++) {
      printf("%s", tbl_ele[9]);
    }
  }
  printf("%s\n", tbl_ele[2]);

  /* Draw first row with column titles */
  for (i = 0; i < sizeof(columns) / sizeof(char *); i++) {
    printf("%s\x1b[1m%s\x1b[0m", tbl_ele[1], columns[i]);
  }
  printf("%s\n", tbl_ele[1]);

  if (ssdp_cache) {

    /* Draw a row-dividing line */
    for (i = 0; i < sizeof(columns) / sizeof(char *); i++) {
      printf("%s", (i == 0 ? tbl_ele[8] : tbl_ele[10]));
      int j;
      for (j = 0; j < strlen(columns[i]); j++) {
        printf("%s", tbl_ele[9]);
      }
    }
    printf("%s\n", tbl_ele[0]);

    ssdp_custom_field_s *cf = NULL;
    ssdp_cache = ssdp_cache->first;
    const char no_info[] = "-";

    while (ssdp_cache) {
        cf = get_custom_field(ssdp_cache->ssdp_message, "serialNumber");
        printf("%s %-20s", tbl_ele[1],
            (cf && cf->contents ? cf->contents : no_info));
        printf("%s %-16s", tbl_ele[1], ssdp_cache->ssdp_message->ip);
        printf("%s %-18s", tbl_ele[1], (ssdp_cache->ssdp_message->mac &&
            0 != strcmp(ssdp_cache->ssdp_message->mac, "") ?
            ssdp_cache->ssdp_message->mac : no_info));
        cf = get_custom_field(ssdp_cache->ssdp_message, "modelName");
        printf("%s %-*s", tbl_ele[1], 16,
            (cf && cf->contents ? cf->contents : no_info));
        cf = get_custom_field(ssdp_cache->ssdp_message, "modelNumber");
        printf("%s %-16s", tbl_ele[1],
            (cf && cf->contents ? cf->contents : no_info));
        printf("%s\n", tbl_ele[1]);

        horizontal_lines_printed++;

        ssdp_cache = ssdp_cache->next;
    }
  }

  /* Draw the bottom line */
  for (i = 0; i < sizeof(columns) / sizeof(char *); i++) {
    printf("%s",
           (i == 0 ? tbl_ele[4] : tbl_ele[6]));
    int j;
    for (j = 0; j < strlen(columns[i]); j++) {
      printf("%s", tbl_ele[9]);
    }
  }
  printf("%s\n", tbl_ele[3]);
  int width, height;

  get_window_size(&width, &height);
  for (i = 1; i < height - horizontal_lines_printed; i++) {
    printf("%*s\n", width, " ");
  }
}

