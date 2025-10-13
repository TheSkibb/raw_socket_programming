#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include "lib/utils.h"
#include "lib/sockets.h"
#include "lib/utils.h"

void print_usage(){
    printf("./routing_daemon <socket_lower>\n");
}

int main(int argc, char *argv[]){
    set_debug(1);
    debugprint("=checking cmd arguments=====================");
    if(argc <= 1){
        printf("too few arguments");
        print_usage();
        return 1;
    }

    char socket_unix_name[MAX_UNIX_PATH_LENGTH] ="";
    strncpy(socket_unix_name, argv[1], strlen(argv[1]) +1);

    debugprint("unix socket name: %s", socket_unix_name);

    debugprint("=======================================done=\n");

    debugprint("=unix socket setup==========================");

    int socket_unix = create_unix_socket(socket_unix_name, UNIX_SOCKET_MODE_CLIENT);

    debugprint("=======================================done=\n");

    debugprint("timer socket setup==========================");
    
    //timer file descriptor
    //

    // Create the timerfd
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd == -1) {
        perror("timerfd_create");
        exit(EXIT_FAILURE);
    }

    // Set the timer to expire every 2 seconds
    struct itimerspec new_value;
    new_value.it_value.tv_sec = 2; // Initial expiration
    new_value.it_value.tv_nsec = 0;
    new_value.it_interval.tv_sec = 2; // Interval
    new_value.it_interval.tv_nsec = 0;

    if (timerfd_settime(timerfd, 0, &new_value, NULL) == -1) {
        perror("timerfd_settime");
        exit(EXIT_FAILURE);
    }

    debugprint("=======================================done=\n");

    debugprint("epoll socket setup==========================");
    
    int epollfd = create_epoll_table();
    add_socket_to_epoll(epollfd, socket_unix, EPOLLIN);
    add_socket_to_epoll(epollfd, timerfd, EPOLLIN);
    int epoll_max_events = 10;
    struct epoll_event events[epoll_max_events];

    debugprint("=======================================done=\n");
    debugprint("=setup done, now entering main loop=========\n\n\n\n\n");

    int rc;
    uint64_t missed;
    while(1){
        rc = epoll_wait(epollfd, events, epoll_max_events, -1);
        if(rc == -1){
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }else if(events->data.fd == timerfd){
            ssize_t s = read(timerfd, &missed, sizeof(uint64_t));
            if(s != sizeof(uint64_t)){
                perror("read");
                exit(EXIT_FAILURE);
            }

            const char *message = "Hello unix socket";
            send(socket_unix, message, strlen(message), 0);
        }else if(events->data.fd == socket_unix){
            //do some stuff
        }
    }
    return 0;

}
