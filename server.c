#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/un.h>

#include "lib/utils.h"
#include "lib/sockets.h"
//#include "lib/utils.h"

void printHelp(){
    printf("ping_server [-h] <socket_lower>\n");
}

//lots of functionality here from man 7 unix
int main(int argc, char *argv[]){

    set_debug(1);

    debugprint("=checking cmd arguments=====================");
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

    debugprint("=======================================done=\n");

    debugprint("=unix socket setup==========================");

    //TODO: change out for cmd arg
    int socket_unix = create_unix_socket(argv[1], UNIX_SOCKET_MODE_CLIENT);

    debugprint("=======================================done=\n");

    debugprint("=epoll setup================================");

    int epollfd = create_epoll_table();

    add_socket_to_epoll(epollfd, socket_unix, EPOLLIN );

    int epoll_max_events = 10;
    struct epoll_event events[epoll_max_events];

    debugprint("=======================================done=\n");


    struct unix_sock_sdu sdu;
    memset(&sdu, 0, sizeof(struct unix_sock_sdu));

    debugprint("=setup done, now entering main loop=========\n\n\n\n\n");

    debugprint("waiting for message on unix socket");
    while(1){
        rc = epoll_wait(epollfd, events, epoll_max_events, -1);
        if(rc == -1){
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }else if(events->data.fd == socket_unix){
            debugprint("=received on unix socket================================");
            //rc = read(socket_unix, &sdu, sizeof(struct unix_sock_sdu));
            rc = read(socket_unix, &sdu, sizeof(struct unix_sock_sdu));
            if(rc < 0){
                perror("recv");
                exit(EXIT_FAILURE);
            }
            debugprint("received %d bytes on unix socket, hurray!", rc);
            debugprint("received \"%s\"", sdu.payload);
            debugprint("received %d", sdu.payload);
            debugprint("==========================================end unix sock=");
        }
    }

    return 0;

};
