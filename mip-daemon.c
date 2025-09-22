#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdlib.h>

#include "lib/sockets.h"
#include "lib/interfaces.h"
#include "lib/mip.h"
#include "lib/utils.h"

//TODO: remove send/recv mode, change for handle

int printhelp(){
    printf("usage mipd [-h] [-d] <socket_upper> <MIP address>\n");
    return 0;
    exit(EXIT_FAILURE);
}

int create_epoll_table(){
	/* Create epoll table */
	int epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		return -1;
	}

    return epollfd;
}

#define BUFFER_SIZE 256 // Define a buffer size for incoming messages

void handle_unix_socket_message(int unix_sockfd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE); // Clear the buffer
    int data_socket;

    // Accept a new connection
    data_socket = accept(unix_sockfd, NULL, NULL);
    if (data_socket == -1) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // Wait for next data packet from the accepted socket
    int rc = read(data_socket, buffer, sizeof(buffer) - 1); // Leave space for null-termination
    if (rc == -1) {
        perror("read");
        close(data_socket); // Close the accepted socket on error
        exit(EXIT_FAILURE);
    }

    // Properly handle the bytes read; ensure there's at least one byte read
    if (rc > 0) {
        // Null-terminate the string
        buffer[rc] = '\0';
        // Print the received message
        printf("Received message on Unix socket: %s\n", buffer);
    }

    // Close the connected socket after done using it
    close(data_socket);
}

int main(int argc, char *argv[]){
    //TODO: do proper flag checking

    /* determine mode */
    if(argc <= 2){
        printf("arguments supplied, see usage with -h\n");
        return 1;
    }

    int send = 0;
    char *arg;
    char unixSocketName[106] = "";

    //parse cmdline arguments
    //TODO: add mip address (i think, maybe in client and server instead)
    for(int i = 1; i < argc; i++){
        arg = argv[i];
        if(strcmp(arg, "-h") == 0){ //print help
            printhelp();
            exit(EXIT_FAILURE);
        }else if(strcmp(arg, "-d") == 0){ //enable debug prints
            set_debug(1);
            debugprint("you have started the daemon with debug prints");
        }else if(strcmp(arg, "s") == 0){ //send mode TODO: remove, everything should be one mode
            debugprint("send mode");
            send = 1;
        }else if(strcmp(arg, "r") == 0){ //receiver mode TODO
            debugprint("recvmode");
            send = 0;
        }else{ // name of unix socket
            debugprint("starting a unix socket with %s", arg);
            strncpy(unixSocketName, arg, strlen(arg)+1);
            //return 1;
        }
    }

    /* raw socket setup */
    debugprint("setting up raw socket");
    int raw_sockfd = create_raw_socket();

    /* interface setup */
    debugprint("setting up interfaces");
    struct ifs_data interfaces;
    init_ifs(&interfaces, raw_sockfd);

    //print the MAC addresses
    //A and C should have 1, B should have 2
    for(int i = 0; i < interfaces.ifn; i++){
        print_mac_addr(interfaces.addr[i].sll_addr, 6);
    }

    int rc = 0;

    if(send){
        uint8_t broadcast[] = ETH_BROADCAST;
        rc = send_mip_packet(&interfaces, interfaces.addr[1].sll_addr, broadcast, 0x01, 0x02, (uint8_t *)argv[argc-1]);
        /*rc = send_mip_arp_request(
                &interfaces, 
                interfaces.addr[1].sll_addr, 
                0x01, 
                0x02
        );
        */
        if(rc < 0){
            perror("send_mip_packet");
            exit(EXIT_FAILURE);
        }
        debugprint("message sent\n");
    }

    debugprint("ready to receive message:\n");

    //set up unix socket
    debugprint("setting up unix socket with name %s\n", unixSocketName);
    int unix_sockfd = create_unix_socket("/tmp/test.Socket", UNIX_SOCKET_MODE_SERVER);
    if(unix_sockfd < 0){
        perror("create_unix_socket");
        exit(EXIT_FAILURE);
    }

    //set unix socket to be listening
    int max_backlog = 10;
    rc = listen(unix_sockfd, max_backlog);
    if (rc == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }


    //set up epoll
    int efd;
    int epoll_max_events = 10;
    struct epoll_event events[epoll_max_events];
    
    //create epoll table
    efd = create_epoll_table();
    if(rc == -1){
        printf("failed to create epoll table");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event_un;
    event_un.events = EPOLLIN; // Listen for input events

    // Add the Unix socket to epoll
    event_un.data.fd = unix_sockfd;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, unix_sockfd, &event_un) == -1) {
        perror("epoll_ctl unix");
        close(efd);
        close(unix_sockfd);
        close(raw_sockfd);
        exit(EXIT_FAILURE);
    }


    struct epoll_event event_raw;
    event_raw.events = EPOLLIN; // Listen for input events
    // Add the raw socket to epoll
    event_raw.data.fd = raw_sockfd;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, raw_sockfd, &event_raw) == -1) {
        perror("epoll_ctl raw");
        close(efd);
        close(unix_sockfd);
        close(raw_sockfd);
        exit(EXIT_FAILURE);
    }

    /*
    //wait for messages
    while(1) {
		rc = epoll_wait(efd, events, epoll_max_events, -1);
		if (rc == -1) {
			perror("epoll_wait");
            exit(EXIT_FAILURE);
		} else if (events->data.fd == raw_sockfd) {
            debugprint("you received a packet");
            rc = handle_mip_packet(&interfaces);
            if(rc <= 0){
                debugprint("rc == %d", rc);
                perror("handle_mip_packet");
                return 1;
            }
        }
	}
*/
    while (1) {
    rc = epoll_wait(efd, events, epoll_max_events, -1);
    if (rc == -1) {
        perror("epoll_wait");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < rc; i++) {
        if (events[i].data.fd == raw_sockfd) {
            debugprint("You received a packet on the raw socket");
            rc = handle_mip_packet(&interfaces);
            if (rc <= 0) {
                debugprint("rc == %d", rc);
                perror("handle_mip_packet");
                return 1;
            }
        }
        
        // Check for events on the Unix socket
        else if (events[i].data.fd == unix_sockfd) {
            //debugprint("You received a message on the Unix socket");
            // Implement handling for messages received on the Unix socket
            // e.g., receive the message and process it accordingly
            handle_unix_socket_message(unix_sockfd);
        }
    }
}
	close(raw_sockfd);
    close(unix_sockfd);
    return 0; //success
}
