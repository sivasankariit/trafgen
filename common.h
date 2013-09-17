//****************************************************************************/
// File:            common.h
// Authors:         Sivasankar Radhakrishnan <sivasankar@cs.ucsd.edu>
//****************************************************************************/

#ifndef COMMON_H
#define COMMON_H

using namespace std;

/*
 * Standard C Libraries
 */
extern "C" {
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
}

/*
 * C++ Libraries
 */
#include <gflags/gflags.h>
#include <iomanip>
#include <iostream>
#include <vector>


//****************************************************************************/
// Declare Common Flags for Traffic Generator
//****************************************************************************/

DECLARE_bool(s);
DECLARE_bool(c);
DECLARE_bool(tcp);
DECLARE_bool(udp);
DECLARE_int32(start_port);
DECLARE_int32(num_ports);


//****************************************************************************/
// Global (External) Variable Declarations
//****************************************************************************/

extern volatile bool interrupted;


//****************************************************************************/
// Global (External) Function Declarations
//****************************************************************************/

int set_non_blocking(int fd);
int set_sendbuff_size(int sockfd, int size);
int set_sock_priority(int sockfd, int prio);
int set_reuseaddr(int sockfd);
void *client_thread_main(void *arg);
void *server_thread_main(void *arg);
void add_to_total_bytes_in(int len);
unsigned long long get_total_bytes_in();
void add_to_total_bytes_out(int len);
unsigned long long get_total_bytes_out();
double get_current_time();

#endif
