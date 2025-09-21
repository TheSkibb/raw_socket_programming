#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

//borrowed from man 7 unix
int create_unix_socket(char *socket_name){

    unlink(socket_name);

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

    /* Bind socket to socket name. */

    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, socket_name, sizeof(name.sun_path) - 1);

    ret = bind(connection_socket, (const struct sockaddr *) &name,
              sizeof(name));
    if (ret == -1) {
       perror("bind");
       exit(EXIT_FAILURE);
    }

    return connection_socket;
}
