#ifndef raw_sockets_h
#define raw_sockets_h

#define UNIX_SOCKET_MODE_SERVER 1
#define UNIX_SOCKET_MODE_CLIENT 2

int create_raw_socket(void);
int create_unix_socket(
    char *name, 
    int mode
);

#endif
