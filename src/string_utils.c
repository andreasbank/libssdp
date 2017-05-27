/** \file string_utils.c
 * Functions for manipulating character strings.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#include <string.h>

#include "string_utils.h"

int strpos(const char *haystack, const char *needle) {
  char *p = strstr(haystack, needle);
  if(p) {
    return p-haystack;
  }
  return -1;
}

unsigned char strcount(const char *haystack, const char *needle) {
  unsigned char count = 0;
  const char *pos = haystack;

  do {
    pos = strstr(pos, needle);
    if(pos != NULL) {
      count++;
      pos++;
    }
  } while(pos != NULL);

  return count;

}

