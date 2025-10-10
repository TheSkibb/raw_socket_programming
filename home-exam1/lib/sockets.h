#ifndef raw_sockets_h
#define raw_sockets_h

#include <stdint.h>

//when passed to create_raw_socket, the socket will **bind**
#define UNIX_SOCKET_MODE_SERVER 1
//when passed to create_raw_socket, the socket will **connect**
#define UNIX_SOCKET_MODE_CLIENT 2

// Define a buffer size for incoming unix messages
#define BUFFER_SIZE 256 
//On Linux, sun_path is 108 bytes in size; - from man 7 unix
#define MAX_UNIX_PATH_LENGTH 108

//shape of data being sent on unix sockets
struct unix_sock_sdu{
    uint8_t mip_addr;
    char payload[BUFFER_SIZE];
} __attribute__((packed));

//creates a new raw socket
//returns file descriptor of the created socket
int create_raw_socket(void);

//returns file descriptor of the created socket
//see UNIX_SOCKET_MODE_SERVER and UNIX_SOCKET_MODE_CLIENT for info on modes
int create_unix_socket(
    char *name, 
    int mode
);

//reads content from unix_sockfd into sdu
void handle_unix_socket_message(
    int unix_sockfd, 
    struct unix_sock_sdu *sdu
);

//accepts a connection on unix_sockfd
//returns a file descriptor of the new socket
int new_unix_connection(int unix_sockfd);

//reads from data socket into sdu
//returns the return code from the read operation
int handle_unix_connection(int data_socket, struct unix_sock_sdu *sdu);

#endif
