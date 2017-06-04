/** \file main.c
 * The main file for the 'ssdpscan' tool.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

/*  ______  ____     __  __  ____    ____    ____
 * /\  _  \/\  _`\  /\ \/\ \/\  _`\ /\  _`\ /\  _`\
 * \ \ \L\ \ \ \L\ \\ \ \ \ \ \,\L\_\ \ \L\_\ \ \/\ \
 *  \ \  __ \ \  _ <'\ \ \ \ \/_\__ \\ \  _\L\ \ \ \ \
 *   \ \ \/\ \ \ \L\ \\ \ \_\ \/\ \L\ \ \ \L\ \ \ \_\ \
 *    \ \_\ \_\ \____/ \ \_____\ `\____\ \____/\ \____/
 *     \/_/\/_/\/___/   \/_____/\/_____/\/___/  \/___/
 *
 *
 *  [A]bused is [B]anks [U]ber-[S]exy-[E]dition [D]aemon
 *
 *  Copyright(C) 2014-2017 by Andreas Bank, andreas.mikael.bank@gmail.com
 */

/*
 *  ████████╗ ██████╗ ██████╗  ██████╗
 *  ╚══██╔══╝██╔═══██╗██╔══██╗██╔═══██╗██╗
 *     ██║   ██║   ██║██║  ██║██║   ██║╚═╝
 *     ██║   ██║   ██║██║  ██║██║   ██║██╗
 *     ██║   ╚██████╔╝██████╔╝╚██████╔╝╚═╝
 *     ╚═╝    ╚═════╝ ╚═════╝  ╚═════╝
 *  01010100 01001111 01000100 01001111 00111010
 *
 *  - Write JSON converter
 *  - Fix IPv6!
 */

/* Uncomment the line below to enable simulating SSDP notifications */
//#define DEBUG_MSG___

/** Set the frequency at which the messages will be received */
#define DEBUG_MSG_FREQ___ 0.5

/**
 * When using simulated messages it is mandatory to
 * specify a UPnP device url in DEBUG_MSG_DEVICE___.
 * This "UPnP device" can be emulated with a http server (e.g. Apache)
 * serving the UPnP-descriptive XML file included in the
 * abused repository (<repo>/udhisapi.xml)
 */
#define DEBUG_MSG_LOCATION_HEADER "http://127.0.0.1:80/udhisapi.xml"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ctype.h>

#include <errno.h>
#include <signal.h>

#include "configuration.h"
#include "common_definitions.h"
#include "daemon.h"
#include "log.h"
#include "ssdp_common.h"
#include "ssdp_listener.h"
#include "ssdp_prober.h"
#include "string_utils.h"

/** The SSDP listener */
static ssdp_listener_s ssdp_listener;

/** The SSDP prober */
static ssdp_prober_s ssdp_prober;

/** The program configuration */
static configuration_s conf;

/**
 * Frees all global allocations.
 */
static void cleanup(void) {
  ssdp_listener_close(&ssdp_listener);
  ssdp_prober_close(&ssdp_prober);
  PRINT_DEBUG("Cleaning up and exiting...\n");
}

/**
 * The exit callback function called on any exit signal.
 *
 * @param param The signal handler parameter (ignored).
 */
static void exit_sig(int param) {
  cleanup();
  exit(EXIT_SUCCESS);
}

/**
 * Decides what the program will run as and does the neccessary
 * deamonizing and forking.
 *
 * @param conf The global configuration to use.
 */
static void verify_running_states(configuration_s *conf) {
  if(conf->run_as_daemon &&
     !(conf->run_as_server ||
       (conf->listen_for_upnp_notif && conf->forward_address) ||
       conf->scan_for_upnp_devices)) {
    /* If missconfigured, stop and notify the user */

    PRINT_ERROR("Cannot start as daemon.\nUse -d in combination with "
        "-S or -a.\n");
    exit(EXIT_FAILURE);

  }
  else if(conf->run_as_daemon) {
    /* If run as a daemon */
    daemonize();
  }

  /* If set to listen for UPnP notifications then
     fork() and live a separate life */
  if (conf->listen_for_upnp_notif &&
     ((conf->run_as_daemon && conf->forward_address) ||
     conf->run_as_server)) {
    if (fork() != 0) {
      /* listen_for_upnp_notif went to the forked process,
         so it is set to false in parent so it doesn't run twice'*/
      conf->listen_for_upnp_notif = FALSE;
    }
    else {
      PRINT_DEBUG("Created a process for listening of UPnP notifications");
      /* scan_for_upnp_devices is set to false
         so it doesn't run in this child (if it will be run at all) */
      conf->scan_for_upnp_devices = FALSE;
    }
  }

  /* If set to scan for UPnP-enabled devices then
     fork() and live a separate life */
  if(conf->scan_for_upnp_devices &&
      (conf->run_as_daemon || conf->run_as_server)) {
    if (fork() != 0) {
      /* scan_for_upnp_devices went to the forked process,
         so it set to false in parent so it doesn't run twice */
      conf->scan_for_upnp_devices = FALSE;
    }
    else {
      PRINT_DEBUG("Created a process for scanning UPnP devices");
      /* listen_for_upnp_notif is set to false
         so it doesn't run in this child (if it will be run at all) */
      conf->listen_for_upnp_notif = FALSE;
    }
  }
}

int main(int argc, char **argv) {
  int ret = 0;

  signal(SIGTERM, &exit_sig);
  signal(SIGABRT, &exit_sig);
  signal(SIGINT, &exit_sig);

  #ifdef DEBUG___
  log_start_args(argc, argv);
  PRINT_DEBUG("%sDevelopment color%s", ANSI_COLOR_PURPLE, ANSI_COLOR_RESET);
  PRINT_DEBUG("%sDebug color%s", ANSI_COLOR_GRAY, ANSI_COLOR_RESET);
  PRINT_DEBUG("%sWarning color%s", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET);
  PRINT_DEBUG("%sError color%s", ANSI_COLOR_RED, ANSI_COLOR_RESET);
  #endif

  set_default_configuration(&conf);
  if (parse_args(argc, argv, &conf)) {
    cleanup();
    exit(EXIT_FAILURE);
  }

  verify_running_states(&conf);

  if (conf.listen_for_upnp_notif) {
    /* If set to listen for devices notifications then
       start listening for notifications but never continue
       to do any other work the parent should be doing */

    /* init socket */
    if ((ret = ssdp_passive_listener_init(&ssdp_listener, &conf))) {
      PRINT_ERROR("Could not create SSDP listener");
      return ret;
    }

    /* Display forwarding info */
    print_forwarder(&conf, &ssdp_listener.forwarder);

    if (ssdp_listener_start(&ssdp_listener, &conf)) {
      PRINT_ERROR("%s", strerror(errno));
      return errno;
    }

  } else if(conf.scan_for_upnp_devices) {
    /* If set to scan for devices then
       start scanning but never continue
       to do any other work the parent should be doing */

    if ((ret = ssdp_prober_init(&ssdp_prober, &conf))) {
      PRINT_ERROR("Could not create SSDP prober");
      return ret;
    }

    /* Display forwarding info */
    print_forwarder(&conf, &ssdp_prober.forwarder);

    if (ssdp_prober_start(&ssdp_prober, &conf)) {
      PRINT_ERROR("%s", strerror(errno));
    }

  } else {
    usage();
  }

  cleanup();
  exit(EXIT_SUCCESS);
}

