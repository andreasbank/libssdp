#ifndef __COMMON_DEFINITIONS__
#define __COMMON_DEFINITIONS__

/* The current version of the program */
#define ABUSED_VERSION "0.0.3"

/* PORTABILITY */
#ifndef TRUE
#define TRUE             (1==1)
#define FALSE            (!TRUE)
#endif

#define INVALID_SOCKET   -1
#define SOCKET_ERROR     -1

#ifndef SIOCGARP
    #define SIOCGARP SIOCARPIPLL
#endif

typedef int SOCKET;
typedef int BOOL;

#endif /* __COMMON_DEFINITIONS__ */
