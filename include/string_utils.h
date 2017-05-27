/** \file string_utils.h
 * Header file for string_utils.c.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __STRING_UTILS_H__
#define __STRING_UTILS_H__

/**
 * Finds the position of a string in a string (or char in a string).
 *
 * @param haystack The string to search in.
 * @param needle The string to search for.
 *
 * @return The position of the needle-string in the haystack-string.
 */
int strpos(const char *haystack, const char *needle);

/**
 * Counts how many times a string is present in another string.
 *
 * @param haystack The string to search in.
 * @param needle The string to search for.
 *
 * @return The number of times needle is present in haystack.
 */
unsigned char strcount(const char *haystack, const char *needle);

#endif /* __STRING_UTILS_H__ */
