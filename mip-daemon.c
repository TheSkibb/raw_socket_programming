#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <linux/if_packet.h>
#include "common.h"

int main(int argc, char *argv[]){

    //process args to determine mode
    if(argc <= 1){
        printf("you need to specify mode sender or receiver\n");
        return 1;
    }

    char *arg1 = argv[1];

    printf("%s\n", arg1);


    if(strcmp(arg1, "s") == 0){
        printf("you are sender\n");
    }else if(strcmp(arg1, "r") == 0){
        printf("you are receiver\n");
    }else{
        printf("invalid mode\n");
        return 1;
    }

    /* raw socket setup */
    printf("setting up raw socket\n");
    int raw_sock = create_raw_socket();
    printf("socket: %d", raw_sock);

    /* interface setup */
    printf("setting up interfaces\n");
    struct ifs_data interfaces;
    init_ifs(&interfaces, raw_sock);

    //print the MAC addresses
    //A and C should have 1, B should have 2
    for(int i = 0; i < interfaces.ifn; i++){
        print_mac_addr(interfaces.addr[i].sll_addr, 6);
    }

    /* epoll setup */
    printf("setting up epoll\n");
    //event variable
    struct epoll_event ev;
    //epoll file descriptor
    int epollfd = epoll_create1(0);
    //check if epollfd was created correctly
    if(epollfd == -1){
        //print the error
        //fra man page: The perror() function produces a message on standard error describing the last error encountered during a call to a system or library function.
        perror("epoll_create1");
        //close the socket
        close(raw_sock);
    }

    //what events to monitor for
    ev.events = EPOLLIN|EPOLLHUP;
    //add raw socket to epoll to monitor for changes in the file
    ev.data.fd = raw_sock;

    //et eller annet med å kontrollere at epoll er satt opp riktig
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, raw_sock, &ev) == -1){
        perror("epoll_ctl: raw_sock");
        close(raw_sock);
        exit(EXIT_FAILURE);
    }

    //events array to 
    struct epoll_event events[MAX_EVENTS];

    printf("setup complete, entering main loop\n");

    /* main loop */
    while(1){
        //wait for chenges in the epollfd descriptor
        //put events into the events buffer
        //return maximum MAX_EVENTS events
        //-1 = infinite timeout
        int returned_events = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if(returned_events == -1){
            perror("epoll_wait");
            break;
        }
        if(events->data.fd == raw_sock){
            printf("you received data");
        }
    }

    close(raw_sock);
    return 0; //success
}
