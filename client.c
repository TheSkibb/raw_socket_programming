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

#include "lib/sockets.h"

void printUsage(){
    printf("ping_client [-h] <socket_lower> <message> <destination_host>\n");
}

//modified from man 7 unix
int main(int argc, char *argv[]){
    ssize_t             w;
    //char                buffer[BUFFER_SIZE];
    char socket_name[MAX_UNIX_PATH_LENGTH];

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
    strncpy(sdu.payload, argv[2], strlen(argv[2])+1);
    uint8_t mip_addr = atoi(argv[3]);

    printf("unix socket on %s\n", socket_name);
    printf("sending message \"%s\"\n", sdu.payload);

    sdu.mip_addr = mip_addr;

    int data_socket = create_unix_socket(socket_name, UNIX_SOCKET_MODE_CLIENT);

    w = write(data_socket, &sdu, sizeof(struct unix_sock_sdu));
    if (w == -1) {
       perror("write");
       exit(EXIT_FAILURE);
    }

    /* Create epoll table */
    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        return -1;
    }
    /* Close socket. */
    close(data_socket);

    exit(EXIT_SUCCESS);
}

