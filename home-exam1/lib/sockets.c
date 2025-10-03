#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "sockets.h"
#include "utils.h"

int create_raw_socket(void){
	//socket descriptor
	int sd;
	//change number to the one in the oblig
	short unsigned int protocol = 0xFFFF;

	/* Set up a raw AF_PACKET socket without ethertype filtering */
	sd = socket(AF_PACKET, SOCK_RAW, htons(protocol));
	if (sd == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	return sd;
}

int create_unix_socket(
        char *socket_name,
        int mode
){

    if(mode == UNIX_SOCKET_MODE_SERVER){
        //NOTICE: this will disconnect other existing connections
        unlink(socket_name);
    }

    int connection_socket;
    int ret;
    struct sockaddr_un name;
    
    connection_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (connection_socket == -1) {
       perror("socket");
       exit(EXIT_FAILURE);
    }

    /*
    * For portability clear the whole structure, since some
    * implementations have additional (nonstandard) fields in
    * the structure.
    */

    memset(&name, 0, sizeof(name));


    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, socket_name, sizeof(name.sun_path) - 1);

    if(mode == UNIX_SOCKET_MODE_SERVER){
        /* Bind socket to socket name. */
        ret = bind(connection_socket, (const struct sockaddr *) &name,
                  sizeof(name));
        if (ret == -1) {
           perror("bind");
           exit(EXIT_FAILURE);
        }
    }else if(mode == UNIX_SOCKET_MODE_CLIENT){
       /* connect to socket with socket name. */
       ret = connect(connection_socket, (const struct sockaddr *) &name,
          sizeof(name));
        if (ret == -1) {
           perror("connect");
           exit(EXIT_FAILURE);
        }
    }
    debugprint("returning unix socket with name %s", name);

    return connection_socket;
}

void handle_unix_socket_message(int unix_sockfd, struct unix_sock_sdu *sdu) {

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE); // Clear the buffer
    int data_socket;

    // Accept a new connection
    data_socket = accept(unix_sockfd, NULL, NULL);
    if (data_socket == -1) {
        perror("accept");
        close(data_socket); 
        exit(EXIT_FAILURE);
    }
    // Wait for next data packet from the accepted socket
    int rc = read(data_socket, sdu, sizeof(struct unix_sock_sdu)); // Leave space for null-termination
    if (rc == -1) {
        perror("read");
        close(data_socket); 
        exit(EXIT_FAILURE);
    }

    // Properly handle the bytes read; ensure there's at least one byte read
    // rc = number of bytes received from read operation
    if (rc > 0) {
        // Null-terminate the string
        buffer[rc] = '\0';
    }

    close(data_socket);
}

int new_unix_connection(int unix_sockfd){
    int data_socket = -1;
    // Accept a new connection
    data_socket = accept(unix_sockfd, NULL, NULL);
    if (data_socket == -1) {
        perror("accept");
        close(data_socket); 
        exit(EXIT_FAILURE);
    }

    return data_socket;
}

int handle_unix_connection(int data_socket, struct unix_sock_sdu *sdu){
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE); // Clear the buffer

    // Wait for next data packet from the accepted socket
    int rc = read(data_socket, sdu, sizeof(struct unix_sock_sdu)); // Leave space for null-termination
    if (rc == -1) {
        perror("read");
        close(data_socket); 
        exit(EXIT_FAILURE);
    }

    //ensure that the string is null terminated
    if (rc > 0) {
        buffer[rc] = '\0';
    }
    //DONT close the connection!!
    //close(data_socket);
    return rc;
}
