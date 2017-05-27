/** \file net_definitions.h
 * Network definitions used in many files.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __NET_DEFINITIONS__
#define __NET_DEFINITIONS__

#if defined BSD || defined __APPLE__
/** Workaround for Apple/BSD systems that have a different define.  */
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
/** Workaround for Apple/BSD systems that have a different define.  */
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

/** The maximum size an IPv4 address can take (aaa.bbb.ccc.ddd + '\0'). */
#define IPv4_STR_MAX_SIZE     16
/**
 * The maximum size an IPv6 address can take
 * (xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:aaa.bbb.ccc.ddd + '\0').
 */
#define IPv6_STR_MAX_SIZE     46
/** The maximum size a MAC address can take (00:40:8C:1A:2B:3C + '\0').*/
#define MAC_STR_MAX_SIZE      18
/** The last port number. */
#define PORT_MAX_NUMBER       65535

#endif /* __NET_DEFINITIONS__ */
