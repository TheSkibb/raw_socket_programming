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
#include "lib/routing.h"

void print_usage(){
    printf("./routing_daemon <socket_lower>\n");
}

struct neighbor{
    uint8_t mip_addr;
    uint8_t mac_addr[6];
    uint8_t interface_index;
}__attribute__((packed));

struct route{
    uint8_t dst; //
    uint8_t next_hop;
    uint8_t cost;
}__attribute__((packed));


struct route_table{
    struct route routes[5];
}__attribute__((packed));

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

    int socket_unix = create_unix_socket(socket_unix_name, UNIX_SOCKET_MODE_CLIENT, SOCKET_TYPE_ROUTER);

    debugprint("=======================================done=\n");

    debugprint("timer socket setup==========================");

    // Create the timerfd
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd == -1) {
        perror("timerfd_create");
        exit(EXIT_FAILURE);
    }

    int timer_interval = 5; //seconds
    struct itimerspec new_value;
    new_value.it_value.tv_sec = timer_interval; // Initial expiration
    new_value.it_value.tv_nsec = 0;
    new_value.it_interval.tv_sec = timer_interval; // Interval
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
            debugprint("=Sending hello message========================");
            //read the timer file descriptor to remove the event from epoll
            ssize_t s = read(timerfd, &missed, sizeof(uint64_t));
            if(s != sizeof(uint64_t)){
                perror("read");
                exit(EXIT_FAILURE);
            }

            //every 30 seconds check send update message


            struct unix_sock_sdu message;
            memset(&message, 0, sizeof(message));

            uint8_t msg[] = ROUTING_HELLO_MSG; //HEL
            memcpy(message.payload, msg, sizeof(msg));

            message.mip_addr = 0x00;

            send(socket_unix, &message, sizeof(message), 0);
            debugprint("=======================================done=\n");

        }else if(events->data.fd == socket_unix){
            debugprint("=Received a unix message======================");
            //do some stuff
            struct unix_sock_sdu recv_sdu;
            memset(&recv_sdu, 0, sizeof(struct unix_sock_sdu));

            int r = read(socket_unix, (void *)&recv_sdu, sizeof(struct unix_sock_sdu));
            if (r == -1) {
               perror("read");
               exit(EXIT_FAILURE);
            }
            if (r == 0){
                debugprint("connection closed");
                exit(EXIT_SUCCESS);
            }

            debugprint("Message is %s, from %d", recv_sdu.payload, recv_sdu.mip_addr);
            debugprint("=======================================done=\n");
        }
    }
    return 0;

}
