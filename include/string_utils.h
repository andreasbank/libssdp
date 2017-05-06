#ifndef __STRING_UTILS_H__
#define __STRING_UTILS_H__

/**
 * Fins the position of a string in a string (or char in a string)
 *
 * @return int The position of the string/char in the searched string
 */
int strpos(const char *haystack, const char *needle);

/**
 * Counts how many times a string is present in another string
 *
 * @param const char * haystack The string to be searched in
 * @param const char * needle The string to be searched for
 *
 * @return unsigned char The number of times needle is preent in haystack
 */
unsigned char strcount(const char *haystack, const char *needle);

#endif /* __STRING_UTILS_H__ */
