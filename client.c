/*
* File client.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <time.h>

#include "lib/sockets.h"
#include "lib/utils.h"

void printUsage(){
    printf("ping_client [-h] <socket_lower> <message> <destination_host>\n");
}

//modified from man 7 unix
int main(int argc, char *argv[]){
    ssize_t             w;
    char socket_name[MAX_UNIX_PATH_LENGTH];
    char ping_str[] = "PING:";
    struct timespec time_start, time_end;


    //argument handling
    if(argc > 1 && strcmp(argv[1], "-h") == 0){
        printf("usage:");
        printUsage();
        return 0;
    }
    if(argc <= 3){
        printf("too few arguments\n");
        printUsage();
        return 0;
    }
    struct unix_sock_sdu sdu;
    memset(&sdu, 0, sizeof(struct unix_sock_sdu));

    strncpy(socket_name, argv[1], strlen(argv[1])+1);

    //strncpy(sdu.payload, argv[2], strlen(argv[2])+1);
    strncpy(sdu.payload, ping_str, 5);
    strncat(sdu.payload, argv[2], strlen(argv[2]));

    uint8_t mip_addr = atoi(argv[3]);

    printf("<\"%s\">\n", sdu.payload);

    sdu.mip_addr = mip_addr;

    int data_socket = create_unix_socket(socket_name, UNIX_SOCKET_MODE_CLIENT);

    clock_gettime(CLOCK_MONOTONIC, &time_start);
    
    w = write(data_socket, &sdu, sizeof(struct unix_sock_sdu));
    if (w == -1) {
       perror("write");
       exit(EXIT_FAILURE);
    }

    int r, rc;
    //int rc;
    char                buffer[BUFFER_SIZE];
    int epoll_max_events = 10;
    struct epoll_event events[epoll_max_events];

    int epollfd = create_epoll_table();
    add_socket_to_epoll(epollfd, data_socket, EPOLLIN);
    while(1){
        //timeout after 1 s = 1000ms
        rc = epoll_wait(epollfd, events, epoll_max_events, 1000);
        if(rc == -1){
            perror("epoll_wait");
            break;
        }else if(rc == 0){
            printf("timout\n");
            clock_gettime(CLOCK_MONOTONIC, &time_end);
            break;
        }else if(events->data.fd == data_socket){

            r = read(data_socket, buffer, sizeof(buffer));
            if (r == -1) {
               perror("read");
               exit(EXIT_FAILURE);
            }
            clock_gettime(CLOCK_MONOTONIC, &time_end);

            buffer[sizeof(buffer) - 1] = 0;

            printf("<\"%s\">\n", buffer);
            break;
        }
    }

    long seconds = time_end.tv_sec - time_start.tv_sec;
    long nanoseconds = time_end.tv_nsec - time_start.tv_nsec;

    // Convert nanoseconds to milliseconds
    long milliseconds = (seconds * 1000) + (nanoseconds / 1000000);
    
    printf("%ld ms\n", milliseconds);

    close(data_socket);

    exit(EXIT_SUCCESS);
}

