//****************************************************************************/
// File:            main.cc
// Authors:         Sivasankar Radhakrishnan <sivasankar@cs.ucsd.edu>
//****************************************************************************/

/*
 * Project Headers
 */
#include "common.h"


//****************************************************************************/
// Flags for Traffic Generator
//****************************************************************************/

DEFINE_bool(s, false, "Server mode");
DEFINE_bool(c, false, "Client mode");
DEFINE_bool(tcp, false, "TCP mode");
DEFINE_bool(udp, false, "UDP mode");
DEFINE_int32(start_port, 5000,
             "Start port that client connects to, server listens on");
DEFINE_int32(num_ports, 1,
             "Num ports that client connects to, server listens on");

//****************************************************************************/
// Global Variable Declarations
//****************************************************************************/

volatile bool interrupted;


//****************************************************************************/
// Global Function Declarations
//****************************************************************************/

void handleint(int signum);
void set_num_file_limit(int n);


//****************************************************************************/
// Function Definitions
//****************************************************************************/

void handleint(int signum)
{
    interrupted = 1;
}

void set_num_file_limit(int n) {
    struct rlimit rl;
    int ret;

    ret = getrlimit(RLIMIT_NOFILE, &rl);
    if (ret == 0) {
        rl.rlim_cur = n + 1000;
        rl.rlim_max = n + 1000;
        ret = setrlimit(RLIMIT_NOFILE, &rl);
        if (ret != 0) {
            perror("setrlimit");
            exit(-1);
        }
    } else {
        perror("getrlimit");
        exit(-1);
    }
}

int main(int argc, char *argv[]) {

    string usage("This is a traffic generator for TCP and UDP traffic.\n"
                 "Usage: %s [options] [server]");
    google::SetUsageMessage(usage);
    google::ParseCommandLineFlags(&argc, &argv, true);

    /* Validate flags */
    if ((FLAGS_s && FLAGS_c) || (!FLAGS_s && !FLAGS_c)) {
        cerr << "Must enable exactly one of server or client mode" << endl;
        exit(-1);
    }

    if ((FLAGS_tcp && FLAGS_udp) || (!FLAGS_tcp && !FLAGS_udp)) {
        cerr << "Must enable exactly one of TCP or UDP mode" << endl;
        exit(-1);
    }

    if ((FLAGS_c) && (argc != 2)) {
        cerr << "Specify server IP to connect to" << endl;
        exit(-1);
    } else if ((FLAGS_s) && (argc > 1)) {
        cerr << "Extra command line arguments provided" << endl;
        exit(-1);
    }

    /* Set file resource limits */
    set_num_file_limit(FLAGS_num_ports);

    /* Register signal handler */
	interrupted = 0;
    signal(SIGINT, handleint);
    signal(SIGTERM, handleint);
    signal(SIGHUP, handleint);
    signal(SIGPIPE, handleint);
    signal(SIGKILL, handleint);

    /* Call server or client main function */
    if (FLAGS_c)
        client_thread_main(argv[1]);
    else
        server_thread_main(NULL);

    return 0;
}
