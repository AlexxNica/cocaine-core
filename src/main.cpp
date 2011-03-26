#include <iostream>
#include <stdexcept>

#include <unistd.h>
#include <signal.h>

#include "common.hpp"
#include "registry.hpp"
#include "core.hpp"

using namespace yappi::core;

core_t* theCore;

void terminate(int signum) {
    theCore->signal(signum);
}

int main(int argc, char* argv[]) {
    char option = 0;

    bool daemonize = false;
    std::vector<std::string> pub_eps;
    std::vector<std::string> ctl_eps;
    std::string path = "/usr/lib/yappi";
    time_t interval = 1000;
    unsigned int watermark = 100;
    unsigned int threads = 1;

    while((option = getopt(argc, argv, "de:l:i:w:t:p:h")) != -1) {
        switch(option) {
            case 'd':
                daemonize = true;
                break;
            case 'e':
                pub_eps.push_back(optarg);
                break;
            case 'l':
                ctl_eps.push_back(optarg);
                break;
            case 'i':
                interval = atol(optarg);
                break;
            case 'w':
                watermark = atol(optarg);
                break;
            case 't':
                threads = atol(optarg);
                break;
            case 'p':
                path = optarg;
                break;
            case 'h':
            default:
                std::cout << "Yappi " << core_t::version << " - The information devourer" << std::endl;
                std::cout << std::endl;
                std::cout << "Usage: yappi -d -l endpoint -e endpoint [-p /usr/lib/yappi] [-i 1000] [-w 100] [-t 1]" << std::endl;
                std::cout << "  -d\tdaemonize" << std::endl;
                std::cout << "  -l\tendpoint for listening for requests, might be used multiple times" << std::endl;
                std::cout << "  -e\tendpoint for exporting events, might be used multiple times" << std::endl;
                std::cout << "  -p\tplugin path" << std::endl;
                std::cout << "  -i\tcore poll timeout in milliseconds" << std::endl;
                std::cout << "  -w\thigh watermark" << std::endl;
                std::cout << "  -t\tio threadpool size" << std::endl;
                std::cout << std::endl;
                std::cout << "Endpoint types:" << std::endl;
                std::cout << "  * ipc://pathname" << std::endl;
                std::cout << "  * tcp://(name|address|*):port" << std::endl;
                std::cout << "  * epgm://(name|address);multicast-address:port [export only]" << std::endl;
                return EXIT_FAILURE;
        }
    }

    // Daemonizing, if needed
    if(daemonize) {
        if(daemon(0, 0) < 0) {
            syslog(LOG_EMERG, "daemonization failed");
            return EXIT_FAILURE;
        }

        // Setting up the syslog
        openlog(core_t::identity, LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);
        setlogmask(LOG_UPTO(LOG_DEBUG));
    }

    syslog(LOG_INFO, "yappi %s is starting", core_t::version);

    signal(SIGINT, &terminate);
    signal(SIGTERM, &terminate);

    try {
        theCore = new core_t(ctl_eps, pub_eps, path, watermark, threads, interval);
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "runtime error: %s", e.what());
        return EXIT_FAILURE;
    } catch(const zmq::error_t& e) {
        syslog(LOG_ERR, "network error: %s", e.what());
        return EXIT_FAILURE;
    }

    // This call blocks
    theCore->run();

    // Cleanup
    delete theCore;

    syslog(LOG_INFO, "yappi has terminated");    
    return EXIT_SUCCESS;
}
