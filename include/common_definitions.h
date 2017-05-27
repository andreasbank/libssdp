/** \file common_definitions.h
 * Defines common to more than one file.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __COMMON_DEFINITIONS__
#define __COMMON_DEFINITIONS__

/** The current version of the program. */
#define ABUSED_VERSION "0.0.3"

/* PORTABILITY */
#ifndef TRUE
/** The definition of TRUE. */
#define TRUE             (1==1)
/** The definition of FALSE. */
#define FALSE            (!TRUE)
#endif

/** An invalid socket. */
#define INVALID_SOCKET   -1
/** An socket error. */
#define SOCKET_ERROR     -1

#ifndef SIOCGARP
    /** This is a workaround for systems missing the definition of SIOCGARP. */
    #define SIOCGARP SIOCARPIPLL
#endif

/** A socket. */
typedef int SOCKET;
/** The type boolean. */
typedef int BOOL;

#endif /* __COMMON_DEFINITIONS__ */
