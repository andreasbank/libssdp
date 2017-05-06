#ifndef __NET_DEFINITIONS__
#define __NET_DEFINITIONS__

#ifdef BSD
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

#define IPv4_STR_MAX_SIZE     16 // aaa.bbb.ccc.ddd
#define IPv6_STR_MAX_SIZE     46 // xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:aaa.bbb.ccc.ddd
#define MAC_STR_MAX_SIZE      18 // 00:40:8C:1A:2B:3C + '\0'
#define PORT_MAX_NUMBER       65535

#endif /* __NET_DEFINITIONS__ */
