/** \file log.h
 * The logging system header file.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

#include "common_definitions.h"

/** Uncomment this line to enable detailed debug information */
#define DEBUG___
/** Uncomment this line to write to a to a file instead of stdout */
#define DEBUG_TO_FILE___

/** Terminal code for the purple color */
#define ANSI_COLOR_PURPLE  "\x1b[1;35m"
/** Terminal code for the color gray */
#define ANSI_COLOR_GRAY    "\x1b[0;90m"
/** Terminal code for the color green */
#define ANSI_COLOR_GREEN   "\x1b[0;32m"
/** Terminal code for the default color */
#define ANSI_COLOR_DEFAULT "\x1b[0;39m"
/** Terminal code for the color yellow */
#define ANSI_COLOR_YELLOW  "\x1b[0;33m"
/** Terminal code for the color red */
#define ANSI_COLOR_RED     "\x1b[0;31m"
/** Terminal code for resetting the color */
#define ANSI_COLOR_RESET   "\x1b[0m"

#ifdef DEBUG___
  #define PRINT_DEV(...)  print_debug(NULL, ANSI_COLOR_PURPLE, __FILE__, \
      __LINE__, __VA_ARGS__)
  #define PRINT_DEBUG(...)  print_debug(NULL, ANSI_COLOR_GRAY, __FILE__, \
      __LINE__, __VA_ARGS__)
  #define PRINT_INFO(...)  print_debug(NULL, ANSI_COLOR_DEFAULT, __FILE__, \
      __LINE__, __VA_ARGS__)
  #define PRINT_WARN(...)  print_debug(NULL, ANSI_COLOR_YELLOW, __FILE__, \
      __LINE__, __VA_ARGS__)
  #define PRINT_ERROR(...)  print_debug(stderr, ANSI_COLOR_RED, __FILE__, \
      __LINE__, __VA_ARGS__)
#else
  #define PRINT_DEV(...)    do { } while (FALSE)
  #define PRINT_DEBUG(...)  do { } while (FALSE)
  #define PRINT_INFO(...)   fprintf(stderr, "[INFO] " __VA_ARGS__)
  #define PRINT_WARN(...)   fprintf(stderr, "[WARN] " __VA_ARGS__)
  #define PRINT_ERROR(...)  fprintf(stderr, "[ERR] " __VA_ARGS__)
#endif

#ifdef DEBUG___
/**
 * Prints a debug message, mimicking printf-like function, supports
 * only %d, %s and %c
 *
 * @param std Standard output to use.
 * @paran color Color to print the message in.
 * @param file The file the debug message generated in.
 * @param line The line number in the file the debug message generated at.
 * @param message The debug message to be displayed.
 * @param ... Variable number of optional arguments that match with the
 *        va_format.
 */
void print_debug(FILE *std, const char *color, const char* file, int line,
    char *va_format, ...);
#endif

/**
 * Logs the command line arguments the process was started with.
 *
 * @param argc The argc passed to the program/lib.
 * @param argv The argv passed to the program/lib.
 */
void log_start_args(int argc, char **argv);

#endif /* __LOG_H__ */
