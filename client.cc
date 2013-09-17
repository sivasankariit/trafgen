//****************************************************************************/
// File:            client.cc
// Authors:         Sivasankar Radhakrishnan <sivasankar@cs.ucsd.edu>
//****************************************************************************/

/*
 * Project Headers
 */
#include "common.h"


//****************************************************************************/
// Flags for Client Mode
//****************************************************************************/

DEFINE_int32(sk_prio, 0, "Socket priority");
DEFINE_int32(rate_mbps, 0,
             "User level rate limit for UDP client mode [0 = unlimited]");
DEFINE_int32(send_buff, (1 << 20), "Send buffer size in bytes");
DEFINE_int32(send_size, 1472, "Size in bytes for each send() call");
DEFINE_int32(mtu, 1500, "Interface MTU");


//****************************************************************************/
// Macro Definitions
//****************************************************************************/

#define UDP_HEADER_SIZE     8
#define IP_HEADER_SIZE      20
#define ETH_HEADER_SIZE     14
#define NSEC_PER_SEC        1000000000LLU


//****************************************************************************/
// Local Function Declarations
//****************************************************************************/

static inline int udp_bytes_on_wire(int write_size);
static unsigned long long timespec_diff_nsec(struct timespec *start,
                                             struct timespec *end);
static unsigned long long spin_sleep_nsec(unsigned long long nsec,
                                          struct timespec *prev);


//****************************************************************************/
// Function Definitions
//****************************************************************************/

static inline int udp_bytes_on_wire(int write_size) {
    int size_per_packet = FLAGS_mtu - IP_HEADER_SIZE - UDP_HEADER_SIZE;
    int num_packets = (write_size + size_per_packet - 1) / size_per_packet;
    int total_header_size = UDP_HEADER_SIZE + IP_HEADER_SIZE + ETH_HEADER_SIZE;
    return write_size + num_packets * total_header_size;
}

static unsigned long long timespec_diff_nsec(struct timespec *start,
                                             struct timespec *end) {
    return ((end->tv_sec - start->tv_sec) * 1LLU * NSEC_PER_SEC
            + (end->tv_nsec - start->tv_nsec));
}

/* Returns the time it actually slept for.
 * Sets prev to the last measured timespec after the sleeptime.
 * If prev is not 0, then the function uses prev as the start time to sleep from
 * instead of taking the current time.
 */
static unsigned long long spin_sleep_nsec(unsigned long long nsec,
                                          struct timespec *prev) {
    struct timespec start, curr;
    if (prev->tv_sec == 0 && prev->tv_nsec == 0)
        clock_gettime(CLOCK_REALTIME, &start);
    else
        start = *prev;

    do {
        clock_gettime(CLOCK_REALTIME, &curr);
    } while (timespec_diff_nsec(&start, &curr) < nsec);

    *prev = curr;
    return timespec_diff_nsec(&start, &curr);
}


/* This function's interface allows it to be called directly or to start a
 * client thread through pthreads.
 */
void *client_thread_main(void *arg) {

    vector <int> sockfd (FLAGS_num_ports, 0);
    struct sockaddr_in *servaddr;
    char *buff;
    unsigned long long nsec = 0;
    char *server = (char*) arg;
    double prev_stats_time = 0;
    unsigned long long prev_total_bytes_out = 0;

    /* Validate flags */
    if (FLAGS_tcp && FLAGS_rate_mbps) {
        cerr << "Application rate limiting is only applicable for UDP" << endl;
        exit(-1);
    }

    /* Create a separate socket for each flow.
     * In case of UDP, we will just use sockfd[0] to send traffic to all
     * destinations.
     */
    for (int i=0; i < FLAGS_num_ports; i++) {
        if (FLAGS_udp) {
            sockfd[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        } else {
            sockfd[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        }
        if (sockfd[i] < 0) {
            perror("socket");
            return NULL;
        }

        /* Set send buffer size */
        if (set_sendbuff_size(sockfd[i], FLAGS_send_buff) < 0)
            return NULL;

        /* Set socket priority */
        if (set_sock_priority(sockfd[i], FLAGS_sk_prio) < 0)
            return NULL;

        /* Set socket to be non-blocking in case of TCP */
        if (FLAGS_tcp) {
            if (set_non_blocking(sockfd[i]) < 0)
                return NULL;
        }
    }

    /* Allocate server address objects */
    servaddr = (struct sockaddr_in *) malloc(FLAGS_num_ports *
                                             sizeof(struct sockaddr_in));
    for (int i=0; i < FLAGS_num_ports; i++) {
        bzero(&servaddr[i], sizeof(servaddr[i]));
        servaddr[i].sin_family = AF_INET;
        servaddr[i].sin_addr.s_addr = inet_addr(server);
        servaddr[i].sin_port = htons(FLAGS_start_port + i);
    }

    /* For TCP mode, connect sockets to respective dst ports */
    if (FLAGS_tcp) {
        for (int i=0; i < FLAGS_num_ports; i++) {
            if (connect(sockfd[i], (const struct sockaddr *)&servaddr[i],
                        sizeof servaddr[i])) {
                if (errno != EINPROGRESS) {
                    perror("connect");
                    return NULL;
                }
            }
        }
    }

    /* Mmap the buffer to send data from */
    buff = (char*) mmap(NULL, FLAGS_send_size, PROT_READ,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (buff == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    /* Initialize variables for application level rate limiting if required */
    if (FLAGS_rate_mbps > 0) {
        nsec = udp_bytes_on_wire(FLAGS_send_size) * 8000LLU / FLAGS_rate_mbps;

        printf("Sleeping for %lluns, sendbuff %d, send_size %d, prio %d\n",
               nsec, FLAGS_send_buff, FLAGS_send_size, FLAGS_sk_prio);
    } else {
        printf("App rate limiting disabled, sendbuff %d, "
               "send_size %d, prio %d\n",
               FLAGS_send_buff, FLAGS_send_size, FLAGS_sk_prio);
    }
    struct timespec prev_nsec;
    prev_nsec.tv_sec = 0;
    prev_nsec.tv_nsec = 0;

    printf("Starting %d ports of %s traffic to %s\n",
           FLAGS_num_ports, FLAGS_tcp ? "TCP" : "UDP", server);

    /* Store the start time for logging statistics */
    prev_stats_time = get_current_time();
    cout << "delta_t\trate_mbps_out" << endl;

    /* Send traffic to all dst ports */
    while (!interrupted) {
        double current_time, diff_time;
        for (int i=0; i < FLAGS_num_ports; i++) {
            int ret;
            /* For UDP, always send on same sockfd to all dst ports */
            if (FLAGS_udp) {
                ret = sendto(sockfd[0], buff, FLAGS_send_size, 0,
                         (struct sockaddr *)&servaddr[i],
                         sizeof(servaddr[i]));
                /* For UDP continue sending even if there is no receiver and
                 * sendto() fails.
                 */
                if (ret > 0)
                    add_to_total_bytes_out(ret);

                if (nsec > 0)
                    spin_sleep_nsec(nsec, &prev_nsec);
            } else {
                ret = send(sockfd[i], buff, FLAGS_send_size, 0);
                if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("send");
                    return NULL;
                }
            }
        }
        /* Check if stats must be shown */
        current_time = get_current_time();
        diff_time = current_time - prev_stats_time;
        if (diff_time >= 1.0) {
            unsigned long long curr_bytes_out = get_total_bytes_out();
            double rate = ((curr_bytes_out - prev_total_bytes_out) * 8 / (1000000 * diff_time));

            cout << "Tx";
            cout << "\t" << std::setiosflags(ios::fixed) << std::setprecision(3) << diff_time;
            cout << "\t" << std::setiosflags(ios::fixed) << std::setprecision(2) << rate << endl;

            prev_stats_time = current_time;
            prev_total_bytes_out = curr_bytes_out;
        }
    }

    return NULL;
}
