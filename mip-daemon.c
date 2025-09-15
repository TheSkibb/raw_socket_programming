#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <linux/if_packet.h>
#include "common.h"
#include "arp_table.h"

#define SEND_MODE 1
#define RECEIVE_MODE 2

int main(int argc, char *argv[]){

    /* determine mode */
    int mode = 0;
    if(argc <= 1){
        printf("you need to specify mode sender or receiver\n");
        return 1;
    }

    char *arg1 = argv[1];

    // s = SEND_MODE
    // r = RECEIVE_MODE
    if(strcmp(arg1, "s") == 0){
        printf("you are sender\n");
        mode = SEND_MODE;
    }else if(strcmp(arg1, "r") == 0){
        printf("you are receiver\n");
        mode = RECEIVE_MODE;
    }else{
        printf("invalid mode\n");
        return 1;
    }

    /* raw socket setup */
    printf("setting up raw socket\n");
    int raw_sock = create_raw_socket();
    printf("socket: %d", raw_sock);

    /* interface setup */
    printf("setting up interfaces\n");
    struct ifs_data interfaces;
    init_ifs(&interfaces, raw_sock);

    //print the MAC addresses
    //A and C should have 1, B should have 2
    for(int i = 0; i < interfaces.ifn; i++){
        print_mac_addr(interfaces.addr[i].sll_addr, 6);
    }

    /* epoll setup */
    printf("setting up epoll\n");
    //event variable
    struct epoll_event ev;
    //epoll file descriptor
    int epollfd = epoll_create1(0);
    //check if epollfd was created correctly
    if(epollfd == -1){
        //print the error
        //fra man page: The perror() function produces a message on standard error describing the last error encountered during a call to a system or library function.
        perror("epoll_create1");
        //close the socket
        close(raw_sock);
    }

    //what events to monitor for
    ev.events = EPOLLIN|EPOLLHUP;
    //add raw socket to epoll to monitor for changes in the file
    ev.data.fd = raw_sock;


    //et eller annet med å kontrollere at epoll er satt opp riktig
    if(e}poll_ctl(epollfd, EPOLL_CTL_ADD, raw_sock, &ev) == -1){
        perror("epoll_ctl: raw_sock");
        close(raw_sock);
        exit(EXIT_FAILURE);
    }

    //events array to 
    struct epoll_event events[MAX_EVENTS];

    /* arp table setup */
    ht *arp_table = ht_create();
    if(arp_table == NULL){
        //out of memory
        return 1;
    }

    //allocate space to the address
    int entry_size = 6 * sizeof(uint8_t);
    int *dst = malloc(entry_size);
    uint8_t values[]= {0xc6, 0xb6, 0x88, 0xd7, 0xdc, 0xdb};
    memcpy(dst, values, entry_size);

    //put the address into the table
    if(ht_set(arp_table, "10", dst) == NULL){
        return 1;
    }

    //get the address from the table
    printf("mip arp table: ");
    print_mac_addr(ht_get(arp_table, "10"), 6);


    if(mode == RECEIVE_MODE){

        /*check for arp address*/

        /* main loop */
        printf("setup complete, entering main loop\n");
        while(1){
            //wait for chenges in the epollfd descriptor
            //put events into the events buffer
            //return maximum MAX_EVENTS events
            //-1 = infinite timeout
            int returned_events = epoll_wait(epollfd, events, MAX_EVENTS, -1);
            if(returned_events == -1){
                perror("epoll_wait");
                break;
            }
            if(events->data.fd == raw_sock){

                int max_buf_size = 1450;
                uint8_t buf[max_buf_size];

                int rc = recv_raw_packet(raw_sock, buf, max_buf_size);

                printf("you received data: <%s>\n", buf);
                if(rc < 1){
                    printf("some error ocurred receiving packet");
                    perror("recv");
                    return -1;
                }
                //break;
            }
        }
    }else if(mode == SEND_MODE){

        int max_msg_size = 10;
        char message[max_msg_size];

        if(argc < 3){
            printf("no message specified, sending \"test\" to:\n");
            strncpy(message, "test", max_msg_size);
        }else{
            char *arg2 = argv[2];
            printf("sending \"%s\" to:", arg2);
            strncpy(message, arg2, max_msg_size);
        }


        uint8_t dst[] = {0xc6, 0xb6, 0x88, 0xd7, 0xdc, 0xdb};
        printf("sending to: ");
        print_mac_addr(dst, 6);
        send_raw_packet(raw_sock, interfaces.addr, (uint8_t *)message, max_msg_size, dst);
    }

    close(raw_sock);
    return 0; //success
}
