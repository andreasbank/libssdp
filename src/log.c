#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"

#ifdef DEBUG___
/**
 * Counts the number of times needle occurs in haystack
 *
 * @param haystack The string to search in
 * @param needle The character to search for
 *
 * @return int The number of occurrences
 */
static int chr_count(char *haystack, char needle) {
  int count = 0;
  int len = strlen(haystack);
  int i;
  for (i = 0; i < len; i++) {
    if (haystack[i] == needle) {
      count++;
    }
  }
  return count;
}

#ifdef DEBUG_TO_FILE___
/**
 * Retrieves the size of a file
 *
 * @param file The file to stat
 *
 * @return The size of the file in Bytes
 */
static int fsize(const char *file) {
  struct stat st;

  if(stat(file, &st) == 0) {
    return st.st_size;
  }

  return -1;
}
#endif

void print_debug(FILE *std, const char *color, const char* file, int line,
    char *va_format, ...) {
  va_list va;
  char message[20480];
  int message_used = 0;
  char *from_pointer = va_format;
  char header[100];
  char *to_pointer = va_format;
  int args_count = chr_count(va_format, '%');
  const char *no_memory = "Not enought memory in buffer to build debug message";
  char *s = NULL;
  char c;
  int d;

  /* Create a nice header for the output */
  sprintf(header, "%s[%d][%s:%04d] ", color, (int)getpid(), file, line);

  if(!va_format) {
    fprintf(stderr, "%s[%d][%s:%d] Error, va_arg input format (va_format) "
        "not set%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__,
        DEBUG_COLOR_END);
    printf("some rror\n");
    return;
  }

  int message_length = 20480;
  memset(message, '\0', sizeof(char) * message_length);
  message_length -= 50; // compensate for additional chars in the message
  va_start(va, va_format);

  int copy_length = 0;
  int args_pos;

  for (args_pos = 1; (from_pointer && *from_pointer != '\0'); from_pointer = to_pointer) {

    to_pointer = strchr(from_pointer, '%');

    if(to_pointer && strlen(to_pointer) > 1) {
      copy_length = to_pointer - from_pointer;

      if(copy_length > message_length - message_used) {
      fprintf(stderr, "%s[%d][%s:%d] %s%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
        return;
      }

      strncpy(message + message_used, from_pointer, copy_length);
      message_used += copy_length;

    switch(to_pointer[1]) {
    case 's':
      /* string */
      s = va_arg(va, char *);
      if(s) {
        copy_length = strlen(s);
        if(copy_length > message_length - message_used) {
          fprintf(stderr, "%s[%d][%s:%d] %s%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
          return;
        }
        strncpy(message + message_used, s, copy_length);
        message_used += copy_length;
      }
      else {
        copy_length = 6;
        strncpy(message + message_used, "NULL", copy_length);
        message_used += copy_length;
      }
      break;
    case 'c':
      /* character */
      c = (char)va_arg(va, int);
      copy_length = 1;
      if(copy_length > message_length - message_used) {
        fprintf(stderr, "%s[%d][%s:%d] %s%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
        return;
      }
      strncpy(message + message_used, &c, copy_length);
      message_used += copy_length;
      break;
    case 'd':
      /* integer */
      d = va_arg(va, int);
      char *d_char = (char *)malloc(sizeof(char) * 10);
      memset(d_char, '\0', 10);
      sprintf(d_char, "%d", d);
      copy_length = strlen(d_char);
      if(copy_length > message_length - message_used) {
        fprintf(stderr, "%s[%d][%s:%d] %s%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
        free(d_char);
        return;
      }
      strncpy(message + message_used, d_char, copy_length);
      message_used += copy_length;
      free(d_char);
      break;
    default:
      break;
    }

    to_pointer += 2;
  }
  else {
    strcat(message, from_pointer);
    to_pointer = NULL;
  }

  if(args_pos > args_count) {
    break;
  }
    args_pos++;
  }

  if(!std) {
    std = stdout;
  }

  #ifdef DEBUG_TO_FILE___
  /* Debug file to write to */
  const char debug_file[] = "debug.log";
  FILE *fh = NULL;
  int file_size = fsize(debug_file);
  /* Max file size is 500 KB */
  int max_size = 512000;
  /* If file is larger than 500KB then trunkate it */
  if(file_size > max_size) {
    fh = fopen(debug_file, "w");
  }
  /* Else continue writing to it */
  else {
    fh = fopen(debug_file, "a");
  }

  if(NULL == fh) {
    fprintf(stderr, "%s[%d][%s:%d] Failed to create debug file%s\n", ERROR_COLOR_BEGIN, (int)getpid(), __FILE__, __LINE__, DEBUG_COLOR_END);
  }
  else {
    fprintf(fh, "%s%s%s\n", header, message, DEBUG_COLOR_END);
    fclose(fh);
  }
  #else
  fprintf(std, "%s%s%s\n", header, message, DEBUG_COLOR_END);
  #endif

}
#endif

