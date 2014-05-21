/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  dummy_device.c - A simple program for sending upnp multicast notifs
 *  Copyright(C) 2014 by Andreas Bank, andreas.mikael.bank@gmail.com
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define VERSION "1.0"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#ifdef __WIN__
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")
#endif

#define TRUE 1
#define FALSE 0
#define SPAM_PORT 1900
#define SPAM_GROUP_ADDRESS "239.255.255.250"
#define SEND_BUFFER 1024

void cleanup();
void exitSig();
char *findArgumentValue(const char *arg, int argc, char **argv);
int findInterface(struct in_addr *interface, const char *address, int af_family);

int sock;
char *message;
char *time_string;

int main(int argc, char **argv) {
  struct sockaddr_in addr;
  int success;
  unsigned int count = 0;
  struct in_addr mcast_address;
  unsigned short mcast_port = htons(SPAM_PORT);
  char *arg_value;
  struct in_addr mcast_interface;
  u_char ttl = 1;
  inet_pton(AF_INET, SPAM_GROUP_ADDRESS, &mcast_address);
  mcast_interface.s_addr = htonl(INADDR_ANY);
  char *message = (char *)malloc(sizeof(char) * SEND_BUFFER);
  memset(message, '\0', SEND_BUFFER);
  strcat(message, "NOTIFY * HTTP/1.1\r\n");
  strcat(message, "Host:239.255.255.250:1900\r\n");
  strcat(message, "NT:urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1\r\n");
  strcat(message, "NTS:ssdp:alive\r\n");
  strcat(message, "Location:http://10.83.128.16:2869/upnphost/udhisapi.dll?content=uuid:d910f885-192e-4344-baa7-610083434017\r\n");
  strcat(message, "USN:uuid:d910f885-192e-4344-baa7-610083434017::urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1\r\n");
  strcat(message, "Cache-Control:max-age=900\r\n");
  strcat(message, "Server:Microsoft-Windows-NT/5.1 UPnP/1.0 UPnP-Device-Host/1.0\r\n");
  strcat(message, "OPT:\"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n");
  strcat(message, "01-NLS:c6d434f85431d29de259ea9a6b9fc7d7\r\n\r\n");

  /* sort out arguments */
  arg_value = findArgumentValue("-i", argc, argv);
  if(arg_value) {
    if(findInterface(&mcast_interface, arg_value, AF_INET)) {
      printf("mcast_interface=%s\n", arg_value);
    }
    else {
      errno = EINVAL;
      cleanup("findInterface");
    }
  }
  else {
    printf("mcast_interface=INADDR_ANY (default)\n");
  }
  ttl = 1;

  /* init socket */
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock < 0) {
    cleanup("socket");
  }
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr = mcast_address;
  addr.sin_port = mcast_port;

  /* signal management */
  signal(SIGTERM, &exitSig);
  signal(SIGABRT, &exitSig);
  signal(SIGINT, &exitSig);

  printf("\ndummy_device.c - ABUSED UPnP Dummy device version %s\nCopyright(C) 2014 by Andreas Bank, <andreas.mikael.bank@gmail.com>\n\n", VERSION);
  printf("Usage:\n\t%s [-i]\n\n", argv[0]);
  printf("\t-i: interface to use\n");

  /* client mode */
  printf("Starting a UPnP dummy device...\n\n");
  if(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))){
    perror("setcoskopt() TTL:");
  }
  if(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &mcast_interface, sizeof(mcast_interface))) {
    perror("setcoskopt() IF:");
  }

  printf("String that will be spammed with:\n\n%s\n", message);

  time_string = (char *)malloc(sizeof(char) * 20);
  while(TRUE) {
    time_t t = time(NULL);
    strftime(time_string, 20, "%Y-%m-%d %H:%M:%S", localtime(&t));
    printf("\rsending(\e[31;1m%d\e[m) \e[1;37;40m[%s]\e[m     ", ++count, time_string);
    fflush(stdout);
    success = sendto(sock, message, strlen(message), 0, (struct sockaddr *) &addr, sizeof(addr));
    if(success < 0) {
       cleanup("sendto");
    }
    sleep(1);
  }

  cleanup(NULL);
  return 0;
}

void cleanup(const char *error) {
  if(sock) {
    close(sock);
    sock = 0;
  }
  if(message) {
    free(message);
    message = NULL;
  }
  if(time_string) {
    free(time_string);
    time_string = NULL;
  }
  if(error) {
    perror(error);
    exit(1);
  }
}

void exitSig() {
  cleanup(NULL);
  printf("\nCaught exit signal, exitting the dummy device...\n\n");
  exit(0);
}

char *findArgumentValue(const char *arg, int argc, char **argv) {
  int i;
  char *tmpStr = malloc(sizeof(char)*50);
  for(i=1; i<argc; i++) {
    if(strcmp(arg, argv[i]) == 0) {
      switch(argv[i][1]) {

      case 'i':
        /* interface to use */
        if(i==argc-1 || (i<argc-1 && argv[i+1][0] == '-')) {
          sprintf(tmpStr, "No interface given after '-i'");
          errno = EINVAL;
          cleanup(tmpStr);
        }
        return argv[i+1];

      }
    }
  }
  return NULL;
}

int findInterface(struct in_addr *interface, const char *address, int af_family) {
  // TODO: for porting to Windows see http://msdn.microsoft.com/en-us/library/aa365915.aspx
  struct ifaddrs *interfaces, *ifa;
  char *paddr;
  int found = FALSE;
  if(getifaddrs(&interfaces)<0) {
    cleanup("getifaddr");
  }
  for(ifa=interfaces; ifa&&ifa->ifa_next; ifa=ifa->ifa_next) {
    paddr = inet_ntoa(((struct sockaddr_in *)ifa->ifa_addr)->sin_addr);
    if((strcmp(address, ifa->ifa_name) == 0) || (strcmp(address, paddr) == 0)) {
      if(ifa->ifa_addr->sa_family == AF_INET) {
        if(strcmp(address, ifa->ifa_name) == 0) {
          printf("Matched ifa_name: %s (paddr: %s)\n", ifa->ifa_name, paddr);
        }
        else if(strcmp(address, paddr) == 0) {
          printf("Matched paddr: %s (ifa_name: %s)\n", paddr, ifa->ifa_name);
        }
        found = TRUE;
        interface->s_addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
        break;
      }
    }
  }
  if(interfaces) {
    freeifaddrs(interfaces);
  }
  return found;
}

