#ifndef __SSDP_H__
#define __SSDP_H__

// TODO: move daemon port to daemon.h ?
#define DAEMON_PORT           43210   // port the daemon will listen on
#define SSDP_ADDR             "239.255.255.250" // SSDP address
#define SSDP_ADDR6_LL         "ff02::c" // SSDP IPv6 link-local address
#define SSDP_ADDR6_SL         "ff05::c" // SSDP IPv6 site-local address
#define SSDP_PORT             1900 // SSDP port
#define NOTIF_RECV_BUFFER     2048 // notification receive-buffer
#define XML_BUFFER_SIZE       2048 // XML buffer/container string
#define LISTEN_QUEUE_LENGTH   2
#define DEVICE_INFO_SIZE      16384
#define MULTICAST_TIMEOUT     2

#endif /* __SSDP_H__ */
