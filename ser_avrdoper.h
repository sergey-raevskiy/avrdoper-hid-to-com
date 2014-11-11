/*
 * Name: ser_avrdoper.h
 * Project: avrdude
 * Author: Christian Starkjohann, Objective Development Software GmbH
 * Creation Date: 2006-10-18
 * Tabsize: 4
 * License: GNU General Public License.
 * Revision: $Id$
 */

#ifndef __ser_avrdoper_h_included__
#define __ser_avrdoper_h_included__

int avrdoper_open(char *port, long baud);
void avrdoper_close(int fd);
int avrdoper_send(int fd, unsigned char *buf, size_t buflen);
int avrdoper_recv(int fd, unsigned char *buf, size_t buflen);
int avrdoper_drain(int fd, int display);

#endif /* __ser_avrdoper_h_included__ */
