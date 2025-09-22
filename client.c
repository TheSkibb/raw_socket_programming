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

#include "lib/sockets.h"

//modified from man 7 unix
int main(int argc, char *argv[]){
   ssize_t             r, w;
   int BUFFER_SIZE = 12;
   char                buffer[BUFFER_SIZE];
   char *SOCKET_NAME = "/tmp/test.Socket";

   int data_socket = create_unix_socket(SOCKET_NAME, UNIX_SOCKET_MODE_CLIENT);

   /*
   // Send arguments. 
   for (int i = 1; i < argc; ++i) {
       printf("sending argument: \"%s\"\n", argv[i]);
       w = write(data_socket, argv[i], strlen(argv[i]) + 1);
       if (w == -1) {
           perror("write");
           break;
       }
   }
   */


   w = write(data_socket, "connected", strlen("connected") + 1);
   if (w == -1) {
       perror("write");
       exit(EXIT_FAILURE);
   }

   // Receive result. 
   r = read(data_socket, buffer, sizeof(buffer));
   if (r == -1) {
       perror("read");
       exit(EXIT_FAILURE);
   }

   // Ensure buffer is 0-terminated. 

   buffer[sizeof(buffer) - 1] = 0;

   printf("Result = %s\n", buffer);

   printf("ready to receive data\n");
   // Receive result. 
   r = read(data_socket, buffer, sizeof(buffer));
   if (r == -1) {
       perror("read");
       exit(EXIT_FAILURE);
   }

   /* Close socket. */

   close(data_socket);

   exit(EXIT_SUCCESS);
}

