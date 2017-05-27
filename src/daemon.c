/** \file daemon.c
 * Functions for making the process a daemon.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined BSD || defined __APPLE__
#include <fcntl.h>
#include <sys/ioctl.h>
#endif

#include "common_definitions.h"
#include "log.h"

void daemonize() {

  PRINT_DEBUG("Daemonizing...");

  int fd, max_open_fds;

  /**
   * Daemon preparation steps:
   *
   * 1. Set up rules for ignoring all (tty related)
   *    signals that can stop the process
   *
   * 2. Fork to child and exit parent to free
   *    the terminal and the starting process
   *
   * 3. Change PGID to ignore parent stop signals
   *    and disassociate from controlling terminal
   *    (two solutions, normal and BSD)
   *
   */

  /* If started with init then no need to detach and do all the stuff */
  if(getppid() == 1) {
    PRINT_DEBUG("Started by init, skipping some daemon setup");
    goto started_with_init;
  }

  /* Ignore signals */
  #ifdef SIGTTOU
  signal(SIGTTOU, SIG_IGN);
  #endif

  #ifdef SIGTTIN
  signal(SIGTTIN, SIG_IGN);
  #endif

  #ifdef SIGTSTP
  signal(SIGTSTP, SIG_IGN);
  #endif

  /*  Get new PGID (not PG-leader and not zero) and free parent process
  so terminal can be used/closed */
  printf("forking...\n");
  if(fork() != 0) {

    /* Exit parent */
    exit(EXIT_SUCCESS);

  }

  #if defined BSD || defined __APPLE__
  PRINT_DEBUG("Using BSD daemon proccess");
  setpgrp();

  if((fd = open("/dev/tty", O_RDWR)) >= 0) {

    /* Get rid of the controlling terminal */
    ioctl(fd, TIOCNOTTY, 0);
    close(fd);

  }
  else {

    PRINT_ERROR("Could not dissassociate from controlling terminal: openning /dev/tty failed.");
    exit(EXIT_FAILURE);
  }
  #else
  PRINT_DEBUG("Using UNIX daemon proccess");

  /* Non-BSD UNIX systems do the above with a single call to setpgrp() */
  setpgrp();

  /* Ignore death if parent dies */
  signal(SIGHUP,SIG_IGN);

  /*  Get new PGID (not PG-leader and not zero)
      because non-BSD systems don't allow assigning
      controlling terminals to non-PG-leader processes */
  pid_t pid = fork();
  if(pid < 0) {

    /* Exit the both processess */
    exit(EXIT_FAILURE);

  }

  if(pid > 0) {

    /* Exit the parent */
    exit(EXIT_SUCCESS);

  }
  #endif

  started_with_init:

  /* Close all possibly open descriptors */
  PRINT_DEBUG("Closing all open descriptors");
  max_open_fds = sysconf(_SC_OPEN_MAX);
  for(fd = 0; fd < max_open_fds; fd++) {

    close(fd);

  }

  /* Change cwd in case it was on a mounted system */
  if(-1 == chdir("/")) {
    PRINT_ERROR("Failed to change to a safe directory");
    exit(EXIT_FAILURE);
  }

  /* Clear filemode creation mask */
  umask(0);

  PRINT_DEBUG("Now running in daemon mode");

}

