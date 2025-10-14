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

int mipd(
        struct ifs_data *interfaces,
        int epollfd,
        struct arp_table *arp_t,
        int socket_raw,
        int socket_unix
){
    int rc = 0;
    //global message to send
    //this way we can just check if this one is set if we receive an arp response
    struct unix_sock_sdu sdu;
    memset(&sdu, 0, sizeof(struct unix_sock_sdu));

    int epoll_max_events = 10;
    struct epoll_event events[epoll_max_events];

    //NB: edge-cases where this is not set to a valid file descriptor
    int socket_data = -1;
    int socket_routing = -1;
    int socket_application = -1;

    //set unix socket to listen
    int max_backlog = 10;
    rc = listen(socket_unix, max_backlog);
    if (rc == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //main loop
    while (1) {
        rc = epoll_wait(epollfd, events, epoll_max_events, -1);
        if (rc == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        if (events->data.fd == socket_raw) {
            debugprint("=received on raw socket=================================");
            struct pdu mip_pdu;
            memset(&mip_pdu, 0, sizeof(struct pdu));

            int received_index = -1;

            recv_mip_packet(
                interfaces,
                arp_t,
                &sdu,
                socket_unix,
                &mip_pdu,
                &received_index
            );

            debugprint("attempting to handle on unix socket");
            handle_mip_packet(interfaces, arp_t, &sdu, socket_data, &mip_pdu, received_index);
            debugprint("===========================================end raw sock=");
        }
        
        // Check for events on the Unix socket
        else if (events->data.fd == socket_unix) {
            debugprint("=New unix socket connection==========================");
            //someone has connected to the unix socket
            //NB: reassigns socket_data
            socket_data = new_unix_connection(socket_unix);

            //get identifier
            uint8_t identifier = -1;
            int rc = read(socket_data, &identifier, sizeof(uint8_t)); // Leave space for null-termination
            if (rc == -1) {
                perror("read");
                close(socket_data); 
                exit(EXIT_FAILURE);
            }

            if(identifier == SOCKET_TYPE_CLIENT || identifier == SOCKET_TYPE_SERVER){
                socket_application = socket_data;
            }else if(identifier == SOCKET_TYPE_ROUTER){
                socket_routing = socket_data;
                interfaces->rusock = socket_data;
            }
            debugprint("the socket is of type: %d", identifier);


            add_socket_to_epoll(epollfd, socket_data, EPOLLIN );
            debugprint("==========================================end unix sock=");
        }else if(events->data.fd == socket_application){
            debugprint("=handle application socket==============================");
            debugprint("socket_application: %d", socket_application);

            //put data from unix socket into sdu
            rc = recv_unix_connection(socket_application, &sdu);
            //if no data was received on connection, the socket was closed on the client side
            if(rc == 0){
                debugprint("========================================= socket closed=");
                close(socket_application);
                continue;
            }
            
            //check mip arp table if this is in the arp table
            int index = arp_t_get_index_from_mip_addr(arp_t, sdu.mip_addr);
            if(index == -1){
                debugprint("%d, was not in the arp table", sdu.mip_addr);
                //we dont know what interface and mac address to send the packet to, so we need to run a mip-arp request
                //the sdu will be sent when the right arp response is received in handle_mip_packet
                send_mip_arp_request(interfaces, sdu.mip_addr);
            }else{
                //we know what interface and mac address is associated with the mip address, so we can send it right away
                debugprint("%d, was in the arp table", sdu.mip_addr);
                send_mip_packet(
                    interfaces,
                    arp_t->sll_ifindex[index],
                    arp_t->sll_addr[index],
                    interfaces->mip_addr,
                    sdu.mip_addr,
                    MIP_TYPE_PING,
                    (uint8_t *)sdu.payload
                );
            }

            debugprint("==========================================end unix sock=");
        }else if(events->data.fd == socket_routing){
            debugprint("=Handle router socket===================================");
            struct unix_sock_sdu router_sdu;
            //put data from unix socket into sdu
            rc = recv_unix_connection(socket_routing, &router_sdu);
            //if no data was received on connection, the socket was closed on the client side
            if(rc == 0){
                debugprint("========================================= socket closed=");
                close(socket_routing);
                continue;
            }

            //based on what type of router message was received, we want to do differnet things
            if(strncmp(router_sdu.payload, "HEL", 3) == 0){
                debugprint("received HELLO message from router socket");
                //send hello mip packet on all interfaces
                send_mip_route_hello(interfaces);
            }else if(strncmp(router_sdu.payload, "UPD", 3) == 0){
                debugprint("received UDATE message from router socket");
                //send update packet with routing table neighbors
            }else if(strncmp(router_sdu.payload, "RSP", 3) == 0){
                debugprint("received RESPONSE message from router socket");
                //search queue for a packet that is to be sent to the dst mip addr in the request
            }

            debugprint("======================================router sock handled");
        }
    }
	close(interfaces->rsock);
    close(interfaces->usock);
    return 0; //success
}

int handle_cmd_args(int argc, char *argv[]){
    int offset = 0;

    if(argc > 1 && strcmp(argv[1], "-d") == 0){
        set_debug(1);
        debugprint("you have enabled debugging");
        offset++;
    }
    if(argc > 1 && strcmp(argv[1], "-h") == 0){
        printhelp();
        //we are done
        exit(EXIT_SUCCESS);
    }
    //otherwise we assume that correct inputs have been made
    debugprint("you have initialized the program with the following options:");
    debugprint("socket_upper: %s", argv[2]);
    debugprint("mip address: %s", argv[3]);
    return offset;
}

int main(int argc, char *argv[]){
    debugprint("============================================");
    debugprint("=cmd args handling==========================");
    
    int offset = handle_cmd_args(argc, argv);
    char unixSocketName[MAX_UNIX_PATH_LENGTH];
    strncpy(unixSocketName, argv[1+offset], strlen(argv[1+offset])+1);
    uint8_t mip_address = atoi(argv[2+offset]);

    debugprint("=======================================done=");

    debugprint("=initialize raw socket======================");

    int socket_raw = create_raw_socket();

    debugprint("=======================================done=");

    debugprint("=initialize unix socket=====================");

    int socket_unix = create_unix_socket(unixSocketName, UNIX_SOCKET_MODE_SERVER, 0);

    debugprint("=======================================done=");

    debugprint("=initialize interfaces======================");
    struct ifs_data interfaces;
    init_ifs(&interfaces, socket_raw);

    interfaces.mip_addr = mip_address;

    //print the MAC addresses
    //A and C should have 1, B should have 2
    for(int i = 0; i < interfaces.ifn; i++){
        print_mac_addr(interfaces.addr[i].sll_addr, 6);
    }

    debugprint("=======================================done=");

    debugprint("=initialize epoll===========================");

    int epollfd = create_epoll_table();
    add_socket_to_epoll(epollfd, socket_raw, EPOLLIN);
    add_socket_to_epoll(epollfd, socket_unix, EPOLLIN);

    debugprint("=======================================done=");

    debugprint("=initialize arp table=======================");
    struct arp_table arp_t;
    debugprint("=======================================done=");
    debugprint("=setup done, starting mip_daemon process====\n\n\n\n\n\n");

    mipd(
        &interfaces,
        epollfd,
        &arp_t,
        socket_raw,
        socket_unix
    );
}
