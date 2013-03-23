//****************************************************************************/
// File:            sockutils.cc
// Authors:         Sivasankar Radhakrishnan <sivasankar@cs.ucsd.edu>
//****************************************************************************/

/*
 * Project Headers
 */
#include "common.h"


//****************************************************************************/
// Function Definitions
//****************************************************************************/

int set_non_blocking(int fd) {
    int flags;
    /* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
    /* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    /* Otherwise, use the old way of doing it */
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}

int set_sendbuff_size(int sockfd, int size) {
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0) {
        perror("setsockopt sendbuff");
        return -1;
    } else {
        return 0;
    }
}

int set_sock_priority(int sockfd, int prio) {
    if (setsockopt(sockfd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)) < 0) {
        perror("setsockopt sk_prio");
        return -1;
    } else {
        return 0;
    }
}

int set_reuseaddr(int sockfd) {
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0) {
        perror("setsockopt reuseaddr");
        return -1;
    } else {
        return 0;
    }
}
