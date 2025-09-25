#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdlib.h>

#include "lib/sockets.h"
#include "lib/interfaces.h"
#include "lib/mip.h"
#include "lib/utils.h"
#include "lib/arp_table.h"

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

int main(int argc, char *argv[]){
    // determine mode 
    if(argc <= 2){
        printf("arguments supplied, see usage with -h\n");
        return 1;
    }

    char unixSocketName[MAX_UNIX_PATH_LENGTH] = "";
    uint8_t mip_address = 0xFF;

    //parse cmdline arguments
    for(int i = 1; i < argc; i++){

        if(strcmp(argv[i], "-h") == 0){ //print help
                                        //
            printhelp();
            exit(EXIT_SUCCESS);

        }else if(strcmp(argv[i], "-d") == 0){ //enable debug prints
                                              //
            set_debug(1);
            debugprint("you have started the daemon with debug prints");

        }else{ // name of unix socket & mip address
               
            if(strcmp(unixSocketName, "") == 0){
                debugprint("unix socket name: %s", argv[i]);
                strncpy(unixSocketName, argv[i], strlen(argv[i])+1);
            }else if (strcmp(argv[i], "") != 0){
                mip_address = atoi(argv[i]); 
                debugprint("argument %s, gives MIP address is: %d", argv[i], mip_address);
            }

        }

    }

    //validate cmdline arguments
    if(mip_address == 0xFF){ //255 is reserved for special case
        printf("address 255 is reserved for broadcast\n");
        exit(EXIT_FAILURE);
    }

    if(strcmp(unixSocketName, "") == 0){
        printf("you need to specify a unix socket name");
        exit(EXIT_FAILURE);
    }

    // raw socket setup 
    debugprint("setting up raw socket");
    int raw_sockfd = create_raw_socket();

    // interface setup 
    debugprint("setting up interfaces");
    struct ifs_data interfaces;
    init_ifs(&interfaces, raw_sockfd);

    interfaces.mip_addr = mip_address;

    //print the MAC addresses
    //A and C should have 1, B should have 2
    for(int i = 0; i < interfaces.ifn; i++){
        print_mac_addr(interfaces.addr[i].sll_addr, 6);
    }

    int rc = 0;

    debugprint("ready to receive message:\n");

    //set up unix socket
    debugprint("setting up unix socket with name %s\n", unixSocketName);
    int unix_sockfd = create_unix_socket(unixSocketName, UNIX_SOCKET_MODE_SERVER);
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

    //set up arp table
    struct arp_table arp_t;

    while (1) {
        rc = epoll_wait(efd, events, epoll_max_events, -1);
        if (rc == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < rc; i++) {

            //check for events on raw socket
            if (events[i].data.fd == raw_sockfd) {
                debugprint("You received a packet on the raw socket");
                rc = handle_mip_packet(&interfaces, &arp_t);
                if (rc <= 0) {
                    debugprint("rc == %d", rc);
                    perror("handle_mip_packet");
                    return 1;
                }
            }
            
            // Check for events on the Unix socket
            else if (events[i].data.fd == unix_sockfd) {

                struct unix_sock_sdu sdu;
                memset(&sdu, 0, sizeof(struct unix_sock_sdu));

                handle_unix_socket_message(unix_sockfd, &sdu);
                rc = send_mip_arp_request(
                        &interfaces, 
                        0x02
                );

                debugprint("=== received on unix socket: %d, \"%s\"", sdu.mip_addr, sdu.payload);
            }
        }
    }
	close(raw_sockfd);
    close(unix_sockfd);
    return 0; //success
}
