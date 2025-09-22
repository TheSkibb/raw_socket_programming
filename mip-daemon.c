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

int epoll_add_sock(
    int sd,
    int epollfd
){
	struct epoll_event ev;

	/* Add RAW socket to epoll table */
	ev.events = EPOLLIN;
	ev.data.fd = sd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sd, &ev) == -1) {
		perror("epoll_ctl: raw_sock");
		return -1;
	}
    return 0;
    exit(EXIT_FAILURE);
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
            //return 1;
        }
    }

    /* raw socket setup */
    debugprint("setting up raw socket");
    int raw_sock = create_raw_socket();

    /* interface setup */
    debugprint("setting up interfaces");
    struct ifs_data interfaces;
    init_ifs(&interfaces, raw_sock);

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
        debugprint("message sent");
    }

    debugprint("ready to receive message");

    int unfd = create_unix_socket("/tmp/test.Socket", UNIX_SOCKET_MODE_SERVER);

    //set up epoll
    int efd;
    int epoll_max_events = 10;
    struct epoll_event events[epoll_max_events];
    
    efd = create_epoll_table();
    if(rc == -1){
        printf("failed to create epoll table");
        exit(EXIT_FAILURE);
    }
    rc = epoll_add_sock(raw_sock, efd);
    if(rc != 0){
        printf("failed to add raw socket to epoll table");
        exit(EXIT_FAILURE);
    }
    rc = epoll_add_sock(unfd, efd);
    if(rc != 0){
        printf("failed to add unix socket to epoll table");
        exit(EXIT_FAILURE);
    }

    int BUFFER_SIZE = 12;
    char                buffer[BUFFER_SIZE];
    int data_socket;

    //wait for messages
    while(1) {
		rc = epoll_wait(efd, events, epoll_max_events, -1);
		if (rc == -1) {
			perror("epoll_wait");
            exit(EXIT_FAILURE);
		} else if (events->data.fd == raw_sock) {
            debugprint("you received a packet");
            rc = handle_mip_packet(&interfaces);
            if(rc <= 0){
                debugprint("rc == %d", rc);
                perror("handle_mip_packet");
                return 1;
            }
		} /*else if(events->data.fd == unfd){
            debugprint("received data via unix socket\n");

        data_socket = accept(unfd, NULL, NULL);
        if (data_socket == -1) {
           perror("accept");
           exit(EXIT_FAILURE);
        }

        // Wait for next data packet. 
        rc = read(data_socket, buffer, sizeof(buffer));
        if (rc == -1) {
           perror("read");
           exit(EXIT_FAILURE);
        }

        debugprint("data: \"%s\"\n", buffer);
        }*/
	}
	close(raw_sock);


    close(raw_sock);
    return 0; //success
}
