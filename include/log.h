#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

#include "common_definitions.h"

/* Uncomment the line below to enable detailed debug information */
#define DEBUG___
/* Uncomment the line below to write to a to a file instead of stdout */
#define DEBUG_TO_FILE___

#define DEBUG_COLOR_BEGIN "\x1b[0;32m"
#define ERROR_COLOR_BEGIN "\x1b[0;31m"
#define WARNING_COLOR_BEGIN "\x1b[1;33m"
#define DEBUG_COLOR_END "\x1b[0;0m"

#ifdef DEBUG___
  #define PRINT_DEBUG(...)  print_debug(NULL, DEBUG_COLOR_BEGIN, __FILE__, \
      __LINE__, __VA_ARGS__)
  #define PRINT_WARN(...)  print_debug(NULL, WARNING_COLOR_BEGIN, __FILE__, \
      __LINE__, __VA_ARGS__)
  #define PRINT_ERROR(...)  print_debug(stderr, ERROR_COLOR_BEGIN, __FILE__, \
      __LINE__, __VA_ARGS__)
#else
  #define PRINT_DEBUG(...)  do { } while (FALSE)
  #define PRINT_WARN(...)  fprintf(stderr, "[WARN] " __VA_ARGS__)
  #define PRINT_ERROR(...)  fprintf(stderr, "[ERR] " __VA_ARGS__)
#endif

#define ANSI_COLOR_GREEN   "\x1b[1;32m"
#define ANSI_COLOR_RED     "\x1b[1;31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/**
 * Prints a debug message, mimicking printf-like function, supports only %d, %s and %c
 *
 * @param std Standard output to use.
 * @paran color Color to print the message in.
 * @param file The file the debug message generated in.
 * @param line The line number in the file the debug message generated at.
 * @param message The debug message to be displayed.
 * @param ... Variable number of optional arguments that match with the
 *        va_format.
 */
void print_debug(FILE *std, const char *color, const char* file, int line, char *va_format, ...);

#endif /* __LOG_H__ */
