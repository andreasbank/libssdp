/** \file daemon.h
 * Header file for daemon.c.
 *
 * @copyright 2017 Andreas Bank, andreas.mikael.bank@gmail.com
 */

#ifndef __DAEMON_H__
#define __DAEMON_H__

/**
 * Prepares the process to run as a daemon.
 */
void daemonize(void);

#endif /* __DAEMON_H__ */
