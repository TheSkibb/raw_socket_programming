#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/un.h>

#include "lib/sockets.h"
//#include "lib/utils.h"

void printHelp(){
    printf("ping_server [-h] <socket_lower>\n");
}

int create_epoll_table(){
	/* Create epoll table */
	int epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		return -1;
	}

    return epollfd;
}


//lots of functionality here from man 7 unix
int main(int argc, char *argv[]){

    int rc =1;

    if(argc <= 1){
        printf("too few arguments\n");
        printHelp();
        return 1;
    }
    if(argc >= 3){
        printf("too many arguments\n");
        printHelp();
        return 1;
    }
    if(strcmp(argv[1], "-h") == 0 ){
        printHelp();
        return 0;
    }

    //TODO: change out for cmd arg
    int un_sock_fd = create_unix_socket(argv[1], UNIX_SOCKET_MODE_CLIENT);

    struct unix_sock_sdu sdu;
    memset(&sdu, 0, sizeof(struct unix_sock_sdu));


    //set up epoll
    int efd;
    int epoll_max_events = 10;
    struct epoll_event events[epoll_max_events];
    
    //create epoll table
    efd = create_epoll_table();
    if(rc == -1){
        printf("failed to create epoll table");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event_un;
    event_un.events = EPOLLIN; // Listen for input events

    // Add the Unix socket to epoll
    event_un.data.fd = un_sock_fd;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, un_sock_fd, &event_un) == -1) {
        perror("epoll_ctl unix");
        close(efd);
        close(un_sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("sending empty to socket\n");
    //send an empty sdu to test connection
    //rc = send_unix_socket(un_sock_fd, &sdu);

    int firstRound = 1;

    printf("starting main loop\n");
    //main listening loop
    while (1) {
        if(firstRound ==1){
            int ret = write(un_sock_fd, &sdu, sizeof(struct unix_sock_sdu));
            if (ret == -1) {
               perror("write");
               exit(EXIT_FAILURE);
            }
            firstRound = 0;
        }
        rc = epoll_wait(efd, events, epoll_max_events, -1);
        if (rc == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }
        if(events->data.fd == un_sock_fd){
            printf("=received on unix socket================================\n");

            printf("==========================================end unix sock=\n");
        }
    }

	return 0;
};
