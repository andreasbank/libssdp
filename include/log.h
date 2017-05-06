#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

/* Uncomment the line below to enable detailed debug information */
#define DEBUG___
/* Uncomment the line below to write to a to a file instead of stdout */
#define DEBUG_TO_FILE___

#define DEBUG_COLOR_BEGIN "\x1b[0;32m"
#define ERROR_COLOR_BEGIN "\x1b[0;31m"
#define DEBUG_COLOR_END "\x1b[0;0m"

#ifdef DEBUG___
  #define DEBUG_PRINT(...)  printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)  do { } while (false)
#endif

#ifdef DEBUG___
  #define PRINT_DEBUG(...)  print_debug(NULL, DEBUG_COLOR_BEGIN, __FILE__, __LINE__, __VA_ARGS__)
  #define PRINT_ERROR(...)  print_debug(stderr, ERROR_COLOR_BEGIN, __FILE__, __LINE__, __VA_ARGS__)
  #define ADD_ALLOC(...)    add_alloc(__VA_ARGS__)
  #define REMOVE_ALLOC(...) remove_alloc(__VA_ARGS__)
  #define PRINT_ALLOC()     print_alloc()
#else
  #define PRINT_DEBUG(...)  do { } while (FALSE)
  #define PRINT_ERROR(...)  fprintf(stderr, __VA_ARGS__)
  #define ADD_ALLOC(...)    do { } while (FALSE)
  #define REMOVE_ALLOC(...) do { } while (FALSE)
  #define PRINT_ALLOC(...)  do { } while (FALSE)
#endif

#define ANSI_COLOR_GREEN   "\x1b[1;32m"
#define ANSI_COLOR_RED     "\x1b[1;31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/**
 * Prints a debug message, mimicking printf-like function, supports only %d, %s and %c
 *
 * @param const char* file The file the debug message generated in
 * @param int line The line number in the file the debug message generated at
 * @param const char* message The debug message to be displayed
 * @param ... variable number of optional arguments that match with the va_format
 */
void print_debug(FILE *std, const char *color, const char* file, int line, char *va_format, ...);

#endif /* __LOG_H__ */
