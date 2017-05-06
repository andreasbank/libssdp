#ifndef __COMMON_DEFINITIONS__
#define __COMMON_DEFINITIONS__

/* PORTABILITY */
#define TRUE             (1==1)
#define FALSE            (!TRUE)
#define INVALID_SOCKET   -1
#define SOCKET_ERROR     -1

#ifndef SIOCGARP
    #define SIOCGARP SIOCARPIPLL
#endif

typedef int SOCKET;
typedef int BOOL;

#endif /* __COMMON_DEFINITIONS__ */
