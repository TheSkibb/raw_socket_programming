#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#include "lib/sockets.h"

void printHelp(){
    printf("ping_server [-h] <socket_lower>\n");
}

//lots of functionality here from man 7 unix
int main(int argc, char *argv[]){

    if(argc < 1){
        printf("too few arguments\n");
        printHelp();
    }
    if(argc > 1){
        printf("too many arguments\n");
        printHelp();
    }

    //TODO: change out for cmd arg
    int un_sock_fd = create_unix_socket("/tmp/test.Socket", UNIX_SOCKET_MODE_SERVER);

    int max_backlog = 20;

    int rc = listen(un_sock_fd, max_backlog);
    if (rc == -1) {
        perror("listen");
        return 0;
    }

    int r;
    int buffer_size = 12;
    char buffer[buffer_size];
    int data_socket;

    printf("trying to send message without knowing if connected");
    char *msg = "hello";
    rc = write(un_sock_fd, msg, strlen(msg)+1);

    printf("ready to receive messages\n");

    ssize_t w;

    for (;;) {

       /* Wait for incoming connection. */

       data_socket = accept(un_sock_fd, NULL, NULL);
       if (data_socket == -1) {
           perror("accept");
           return 0;
       }

       /* Wait for next data packet. */
       r = read(data_socket, buffer, sizeof(buffer));
       if (r == -1) {
           perror("read");
           return 0;
       }

       /* Ensure buffer is 0-terminated. */
       buffer[sizeof(buffer) - 1] = 0;

       printf("received \"%s\"\n", buffer);

       /* Send result. */

       sprintf(buffer, "pong");
       printf("now sending response to client\n");
       w = write(data_socket, buffer, sizeof(buffer));
       if (w == -1) {
           perror("write");
           return 0;
       }

       /* Close socket. */

       close(data_socket);
    }

	return 0;
};
