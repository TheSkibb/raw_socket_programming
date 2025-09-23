#ifndef raw_sockets_h
#define raw_sockets_h

#define UNIX_SOCKET_MODE_SERVER 1
#define UNIX_SOCKET_MODE_CLIENT 2
// Define a buffer size for incoming unix messages
#define BUFFER_SIZE 256 
//On Linux, sun_path is 108 bytes in size; - from man 7 unix
#define MAX_UNIX_PATH_LENGTH 108

int create_raw_socket(void);
int create_unix_socket(
    char *name, 
    int mode
);
void handle_unix_socket_message(int unix_sockfd);

#endif
