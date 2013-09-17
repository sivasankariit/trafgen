//****************************************************************************/
// File:            server.cc
// Authors:         Sivasankar Radhakrishnan <sivasankar@cs.ucsd.edu>
//****************************************************************************/

/*
 * Project Headers
 */
#include "common.h"


//****************************************************************************/
// Flags for Server Mode
//****************************************************************************/

DEFINE_int32(recv_size, 65536, "Max size in bytes for each recv() call");
DEFINE_int32(listen_backlog, 1000, "Max listen backlog");


//****************************************************************************/
// Function Definitions
//****************************************************************************/

void *server_thread_main(void *arg) {

    vector <int> srvsockfd (FLAGS_num_ports, 0);
    struct sockaddr_in *servaddr;
    vector <int> clisockfd;
    fd_set server_readfds;
    int server_fdmax = 0;
    char *buff = (char *) malloc(FLAGS_recv_size);

    /* Create separate listen sockets for each port */
    FD_ZERO(&server_readfds);
    for (int i=0; i < FLAGS_num_ports; i++) {
        if (FLAGS_udp) {
            srvsockfd[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        } else {
            srvsockfd[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        }
        if (srvsockfd[i] < 0) {
            perror("socket");
            return NULL;
        }

        /* Set read FD flag */
        FD_SET(srvsockfd[i], &server_readfds);

        /* Set non-blocking flag */
        if (set_non_blocking(srvsockfd[i]) < 0)
            return NULL;

        /* Set reuseaddr flag */
        if (set_reuseaddr(srvsockfd[i]) < 0)
            return NULL;

        /* Set server_fdmax */
        if (server_fdmax < srvsockfd[i])
            server_fdmax = srvsockfd[i];
    }

    /* Bind to server ports
     * For TCP mode, start listening to incoming connections
     * For UDP mode, add all the server sockets to the clisockfd set to later
     * read data from them.
     */
    servaddr = (struct sockaddr_in *) malloc(FLAGS_num_ports *
                                             sizeof(struct sockaddr_in));
    for (int i=0; i < FLAGS_num_ports; i++) {
        bzero(&servaddr[i], sizeof(servaddr[i]));
        servaddr[i].sin_family = AF_INET;
        servaddr[i].sin_addr.s_addr = INADDR_ANY;
        servaddr[i].sin_port = htons(FLAGS_start_port + i);

        if (bind(srvsockfd[i], (struct sockaddr*) &servaddr[i],
                 sizeof(struct sockaddr_in)) < 0) {
            perror("bind");
            return NULL;
        }

        if (FLAGS_tcp) {
            if (listen(srvsockfd[i], FLAGS_listen_backlog) < 0) {
                perror("listen");
                return NULL;
            }
        } else {
            clisockfd.push_back(srvsockfd[i]);
        }
    }

    /* Accept incoming connections and receive traffic */
    while (!interrupted) {
        int ret;
        struct timeval timeout;
        fd_set server_readfds_tmp = server_readfds;

        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // check for interrupt at least every 100ms

        /* Wait for socket event */
        if ((ret = select(server_fdmax+1, &server_readfds_tmp,
                          NULL, NULL, &timeout)) < 0) {
            if (errno != EINTR) {
                perror("select");
                return NULL;
            } else {
                continue;
            }
        } else if (ret == 0) {
            /* Just timed out */
            continue;
        }

        /* For TCP, accept incoming connections on all listen ports */
        for (int i=0; (FLAGS_tcp) && (i < FLAGS_num_ports); i++) {
            /* Check if listen port has any backlogged connections */
            if (!FD_ISSET(srvsockfd[i], &server_readfds_tmp))
                continue;

            /* Accept all backlogged connections on the socket */
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(struct sockaddr_in);
	        while (1) {
                int sd = accept(srvsockfd[i],
                                (struct sockaddr *)&client_addr, &addrlen);
                if (sd < 0) {
                    if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                        perror("accept");
                        return NULL;
                    } else {
                        break;
                    }
                } else {
                    clisockfd.push_back(sd);
                    /* Set FDs for the client connection */
                    FD_SET(sd, &server_readfds);
                    if (sd > server_fdmax)
                        server_fdmax = sd;
                }
            }
        }

        /* Read data from all client connections */
        for (unsigned int i=0; i < clisockfd.size(); i++) {

            /* Check if the connection has any data to be read */
            if (!FD_ISSET(clisockfd[i], &server_readfds_tmp))
                continue;

            /* Read data from socket */
            int ret;

            if (FLAGS_udp) {
                struct sockaddr_in client_addr;
                socklen_t addrlen;
		        ret = recvfrom(clisockfd[i], buff, FLAGS_recv_size,
                               MSG_DONTWAIT, (struct sockaddr*) &client_addr, &addrlen);
            } else {
                ret = recv(clisockfd[i], buff, FLAGS_recv_size, MSG_DONTWAIT);
            }

            /* Check if the receive succeeded */
            if (ret == 0) {
                /* Close the connection */
                close(clisockfd[i]);
                FD_CLR(clisockfd[i], &server_readfds);
                clisockfd.erase(clisockfd.begin() + i);
                i--;
            } else if (ret < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("recv");
                    return NULL;
                } else {
                    continue;
                }
            }
        }
    }

    /* Close all client connections */
    for (unsigned int i=0; i < clisockfd.size(); i++)
        close(clisockfd[i]);
    /* Close listen sockets */
    for (unsigned int i=0; i < srvsockfd.size(); i++)
        close(srvsockfd[i]);

    return NULL;
}
