#include <linux/if_packet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/ioctl.h>

#include "interfaces.h"
#include "mip.h"
#include "utils.h"
#include "arp_table.h"
#include "sockets.h"
#include "routing.h"


int forward_mip_packet(
        struct ifs_data *ifs,
        struct pdu *mip_pdu,
        int interface_index,
        uint8_t *dst_mac_addr
){

    debugprint("=Forwarding mip packet==================================");
    send_mip_packet(
            ifs,
            interface_index,
            dst_mac_addr,
            mip_pdu->mip_hdr.src_addr,
            mip_pdu->mip_hdr.dst_addr,
            mip_pdu->mip_hdr.sdu_type,
            mip_pdu->sdu
    );
    debugprint("========================================Forwarding done=");

    return 0;
}

void print_mip_arp_header(
    struct mip_arp_hdr *miparphdr
){
    if(get_debug() != 1){
        return;
    }
    printf("--------MIP ARP------\n");
    if(miparphdr->Type == MIP_ARP_TYPE_REQUEST){
        printf("type: Request = %d\n", miparphdr->Type);
    }else if(miparphdr->Type == MIP_ARP_TYPE_RESPONSE){
        printf("type: Response = %d\n", miparphdr->Type);
    }
    printf("Address: %d\n", miparphdr->Address);
    printf("---------------------\n");
}

void print_mip_header(
    struct mip_hdr *miphdr
){
    if(get_debug() != 1){
        return;
    }
    printf("------ MIP ------\n");
    printf("dst_addr: %d\n", miphdr->dst_addr);
    printf("src_addr: %d\n", miphdr->src_addr);
    printf("ttl: %d\n", miphdr->ttl);
    printf("sdu_len: %d\n", miphdr->sdu_len);
    printf("sdu_type: %d\n", miphdr->sdu_type);
    printf("-----------------\n");
}

int recv_mip_packet(
    struct ifs_data *ifs,
    struct arp_table *arp_t,
    struct unix_sock_sdu *sdu,
    int socket_unix,
    struct pdu *out_pdu,
    int *out_received_index
){
    debugprint("handling mip packet\n");
    struct sockaddr_ll so_name;
    struct eth_hdr ethhdr;
    struct mip_hdr miphdr;

    struct msghdr msg;

    int iov_len = 3;
    struct iovec msgvec[iov_len];
    int rc;

    //we put the contents of the packet into this variable
    uint8_t packet[256];

    //point to eth hdr
    msgvec[0].iov_base = &ethhdr;
    msgvec[0].iov_len = sizeof(struct eth_hdr);

    //point to mip header
    msgvec[1].iov_base = &miphdr;
    msgvec[1].iov_len = sizeof(struct mip_hdr);

    //read sdu
	msgvec[2].iov_base = (void *)packet;
    //TODO: fix proper length
	msgvec[2].iov_len  = 256;

    //fill out message metadata
    msg.msg_name = &so_name;
    msg.msg_namelen = sizeof(struct sockaddr_ll);
    msg.msg_iovlen = iov_len;
    msg.msg_iov = msgvec;

    debugprint("i want to receive message");
    rc = recvmsg(ifs->rsock, &msg, 0);
    if(rc <= 0){
        perror("recvmsg");
        return -1;
    }

    //find out what interface we received data on
    int received_index = -1;

    for(int i = 0; i < MAX_IF; i++){
        if(ifs->addr[i].sll_ifindex == so_name.sll_ifindex){
            received_index = i;
        }
    }

    if(received_index == -1){
        printf("could not find out what interface we received data on");
        exit(EXIT_FAILURE);
    }

    memcpy(out_pdu->sdu, &packet, 256);
    debugprint("copyied: \"%s\"", out_pdu->sdu);
    memcpy(&out_pdu->mip_hdr, &miphdr, sizeof(struct mip_hdr));
    memcpy(&out_pdu->ethhdr, &ethhdr, sizeof(struct eth_hdr));
    *out_received_index = received_index;

    return 0;
}

//function for handling routing packets (type 0x04)
int handle_mip_route_packet(
    struct ifs_data *ifs,
    struct arp_table *arp_t,
    struct unix_sock_sdu *sdu,
    int socket_unix,
    struct pdu *mip_pdu,
    int received_index
){
    if(ifs->rusock == -1){
        debugprint("no routing_deamon is attached");
        return -1;
    }

    //add the src address to the mip arp table if we dont already have it
    if(arp_t_get_index_from_mip_addr(arp_t, mip_pdu->mip_hdr.src_addr) == -1){
        //add to table
        arp_t_add_entry(arp_t, mip_pdu->mip_hdr.src_addr, received_index, mip_pdu->ethhdr.src_mac);
    }

    if(strncmp((char *)mip_pdu->sdu, "HEL", 3) == 0){
        debugprint("routing message is a HELLO message");

        //send to unix socket
        struct unix_sock_sdu send_unix;
        memset(&send_unix, 0, sizeof(struct unix_sock_sdu));
        uint8_t msg[] = ROUTING_HELLO_MSG;
        memcpy(send_unix.payload, msg, sizeof(msg));
        send_unix.mip_addr = mip_pdu->mip_hdr.src_addr;
       
        int w = write(ifs->rusock, &send_unix, sizeof(struct unix_sock_sdu));
        if(w == -1){
            debugprint("rusock %d", ifs->rusock);
            perror("write");
            exit(EXIT_FAILURE);
        }

    }else if(strncmp((char *)mip_pdu->sdu, "UPD", 3) == 0){
        debugprint("routing message is a UPDATE message, sending down to unix socket");

        //create sdu to send over unix socket
        struct unix_sock_sdu send_unix;
        memset(&send_unix, 0, sizeof(struct unix_sock_sdu));

        memcpy(send_unix.payload, mip_pdu->sdu, BUFFER_SIZE);

        debugprint("there is %d route. dst: %d, n_h: %d, c: %d", 
            send_unix.payload[3],
            send_unix.payload[4],
            send_unix.payload[5],
            send_unix.payload[6]
        );


        send_unix.mip_addr = mip_pdu->mip_hdr.src_addr;
       
        int w = write(ifs->rusock, &send_unix, sizeof(struct unix_sock_sdu));
        if(w == -1){
            debugprint("rusock %d", ifs->rusock);
            perror("write");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

//function for handling mip arp packets
//adds the source to ARP table if not present.
//if the packet is a MIP_ARP_TYPE_REQUEST we send a response
//if the packet is a MIP_ARP_TYPE_RESPONSE we check if there is a packet queued for this node (sdu != 0)
//TODO: change for queue?
int handle_mip_arp_packet(
        struct ifs_data *ifs,
        struct arp_table *arp_t,
        struct unix_sock_sdu *sdu,
        int socket_unix,
        struct pdu *mip_pdu,
        int received_index
){
    debugprint("type is MIP arp");
    struct mip_arp_hdr *miparphdr = (struct mip_arp_hdr *)&mip_pdu->sdu;

    //check if this address already is in our mip cache
    int index = arp_t_get_index_from_mip_addr(arp_t, mip_pdu->mip_hdr.src_addr);

    debugprint("index from table: %d", index);

    if(index == -1){
        debugprint("address was not found in table");
        index = arp_t_add_entry(arp_t, mip_pdu->mip_hdr.src_addr, received_index, mip_pdu->ethhdr.src_mac);
        if(index == -1){
            printf("not enough space in arp table\n");
            exit(EXIT_FAILURE);
        }
    }

    debugprint("THIS IS THE INDEX: %d", index);
    debugprint("mip: %d", arp_t->mip_addr[index]);
    debugprint("interface: %d", arp_t->sll_ifindex[index]);
    print_mac_addr(arp_t->sll_addr[index], 6);

    //if it is a request we send a response
    if(miparphdr->Type == MIP_ARP_TYPE_REQUEST){

        debugprint("received MIP ARP request: ");
        send_mip_arp_response(
            ifs, 
            received_index, 
            mip_pdu->ethhdr.src_mac, 
            mip_pdu->mip_hdr.src_addr
        );

        uint8_t eth_broadcast[] = ETH_BROADCAST;

        //forward mip arp request on all interfaces (except the one it was received on)
        for(int i = 0; i < ifs->ifn; i++){
            if(i != received_index){
                forward_mip_packet(
                        ifs,
                        mip_pdu,
                        i,
                        eth_broadcast
                );
            }
        }


    } else if(miparphdr->Type == MIP_ARP_TYPE_RESPONSE){
        //check if we want to send any message to this address
        if(mip_pdu->mip_hdr.src_addr == sdu->mip_addr){
        debugprint("received MIP ARP response from %d", mip_pdu->mip_hdr.src_addr);
            send_mip_packet(
                ifs,
                arp_t->sll_ifindex[index],
                arp_t->sll_addr[index],
                ifs->mip_addr,
                sdu->mip_addr,
                MIP_TYPE_PING,
                (uint8_t *)sdu->payload
            );
        } else {
            debugprint("received MIP ARP from %d, which we are not interested in", mip_pdu->mip_hdr.src_addr);
        }

        //check if response was meant for another node
        if(mip_pdu->mip_hdr.dst_addr != ifs->mip_addr){
            debugprint("received MIP ARP response for %d", mip_pdu->mip_hdr.dst_addr);
            uint8_t node_a_mac_addr[] = {0x00,0x00,0x00,0x00,0x00,0x02};
            forward_mip_packet(
                ifs,
                mip_pdu,
                0,
                node_a_mac_addr
            );
        }
    } else {
        debugprint("mip arp type invalid");
        return -1;
    }
    print_mip_arp_header(miparphdr);
    return 0;
}

int handle_mip_ping_packet(
        struct ifs_data *ifs,
        struct arp_table *arp_t,
        struct unix_sock_sdu *sdu,
        int socket_unix,
        struct pdu *mip_pdu,
        int received_index
){
        // TODO: handle forwarding case
        // needs to get info about what the next hop is from unix socket
        if(mip_pdu->mip_hdr.dst_addr != ifs->mip_addr){

            debugprint("this packets should be forwarded");
            return 0;
        }
        debugprint("received PING message: %s", mip_pdu->sdu);
        memcpy(sdu->payload, mip_pdu->sdu, 256);
        sdu->mip_addr = mip_pdu->mip_hdr.src_addr;
        int w = 0;
        if(socket_unix != -1){
            w = write(socket_unix, sdu, sizeof(struct unix_sock_sdu));
        }else{
            debugprint("socket_data is -1, no application is connected");
        }
        if (w == -1) {
           perror("write");
           exit(EXIT_FAILURE);
        }
        return 0;
}

int handle_mip_packet(
        struct ifs_data *ifs,
        struct arp_table *arp_t,
        struct unix_sock_sdu *sdu,
        int socket_unix,
        struct pdu *mip_pdu,
        int received_index
){

    //mip arp handling
    if(mip_pdu->mip_hdr.sdu_type == MIP_TYPE_ARP){
        handle_mip_arp_packet(ifs, arp_t, sdu, socket_unix, mip_pdu, received_index);
    }

    //ping handling
    if(mip_pdu->mip_hdr.sdu_type == MIP_TYPE_PING){
        handle_mip_ping_packet(ifs, arp_t, sdu, socket_unix, mip_pdu, received_index);
    }

    //route handling
    if(mip_pdu->mip_hdr.sdu_type == MIP_TYPE_ROUTE){
        handle_mip_route_packet(ifs, arp_t, sdu, socket_unix, mip_pdu, received_index);
    }

    return mip_pdu->mip_hdr.sdu_type;
}

int send_mip_packet(
    struct ifs_data *ifs,
    int addr_index,
    uint8_t *dst_mac_addr,
    uint8_t src_mip_addr,
    uint8_t dst_mip_addr,
    uint8_t type,
    uint8_t *sdu
){
    struct eth_hdr ethhdr;
    struct mip_hdr miphdr;
    struct msghdr *msg;

    int iov_len = 3;
    struct iovec msgvec[iov_len];
    int rc;

    // fill in eth_hdr 
    memcpy(ethhdr.dst_mac, dst_mac_addr, 6);
    memcpy(ethhdr.src_mac, ifs->addr[addr_index].sll_addr, 6);

    //TODO: set to MIP type
    ethhdr.ethertype = htons(0xFFFF);

    //point to ethernet header
    msgvec[0].iov_base = &ethhdr;
    msgvec[0].iov_len = sizeof(struct eth_hdr);

    //fill in mip hdr
    miphdr.dst_addr = dst_mip_addr;
    miphdr.src_addr = src_mip_addr;
    //miphdr.src_addr = ifs->mip_addr;
    miphdr.ttl = 1;
    miphdr.sdu_len = (sizeof(sdu)+3)/4;
    miphdr.sdu_type = type;

    //point to mip header
    msgvec[1].iov_base = &miphdr;
    msgvec[1].iov_len = sizeof(struct mip_hdr);

    //point to sdu
    msgvec[2].iov_base = sdu;
    msgvec[2].iov_len = 256;

    //allocate a zeroed out message info struct
    msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));

    //fill out message metadata struct
    //TODO: static addr
    msg->msg_name = &(ifs->addr[addr_index]);
    msg->msg_namelen = sizeof(struct sockaddr_ll);
    msg->msg_iovlen = iov_len;
    msg->msg_iov = msgvec;

    rc = sendmsg(ifs->rsock, msg, 0);
    if(rc == -1){
        perror("sendmsg");
        free(msg);
        return -1;
    }

    debugprint("sending mip packet");
    print_mip_header(&miphdr);

    debugprint("MIP packet sent to:");
    free(msg);

    return rc;
}

int send_mip_arp_request(
    struct ifs_data *ifs,
    uint8_t dst_mip_addr
){
    debugprint("preparing to send mip arp request\n");
    //check if you are looking for your own address
    if(ifs->mip_addr==dst_mip_addr){
        debugprint("you are looking for your own mip address, something wrong has happened");
        debugprint("%d == %d", ifs->mip_addr, dst_mip_addr);
        return -1;
    }

    uint8_t eth_broadcast[] = ETH_BROADCAST;
    uint8_t mip_broadcast = 0xFF;

    struct mip_arp_hdr arphdr;

    arphdr.Type = MIP_ARP_TYPE_REQUEST;
    arphdr.Address = dst_mip_addr;

    int rc = 0;

    //TODO: send on all interfaces
    for(int i = 0; i < ifs->ifn; i++){
        debugprint("sending mip request %d / %d", i, ifs->ifn);
        print_mip_arp_header(&arphdr);
        rc = send_mip_packet(
            ifs,
            i,
            eth_broadcast,
            ifs->mip_addr,
            mip_broadcast,
            MIP_TYPE_ARP,
            (uint8_t *)&arphdr
        );

        if(rc < 1){
            printf("epic fail :(\n");
            exit(EXIT_FAILURE);
        }
    }


    return rc;
}

int send_mip_arp_response(
    struct ifs_data *ifs,
    int interface_index,
    uint8_t *dst_mac_addr,
    uint8_t dst_mip_addr
    ){

    uint8_t eth_broadcast[] = ETH_BROADCAST;

    struct mip_arp_hdr arphdr;

    arphdr.Type = MIP_ARP_TYPE_RESPONSE;
    arphdr.Address = ifs->mip_addr;

    int rc = send_mip_packet(
        ifs,
        interface_index,
        eth_broadcast,
        ifs->mip_addr,
        dst_mip_addr,
        MIP_TYPE_ARP,
        (uint8_t *)&arphdr
    );

    return rc;
}


int send_mip_route_hello(
        struct ifs_data *ifs
){
    debugprint("preparing to send mip hello packet \n");

    uint8_t eth_broadcast[] = ETH_BROADCAST;
    uint8_t mip_broadcast = 0xFF;

    uint8_t hello[3] = ROUTING_HELLO_MSG;

    int rc = 0;

    //TODO: send on all interfaces
    for(int i = 0; i < ifs->ifn; i++){
        debugprint("sending HELLO %d / %d", i+1, ifs->ifn);
        rc = send_mip_packet(
            ifs,
            i,
            eth_broadcast,
            ifs->mip_addr,
            mip_broadcast,
            MIP_TYPE_ROUTE,
            hello
        );

        if(rc < 1){
            printf("epic fail :(\n");
            exit(EXIT_FAILURE);
        }
    }
    return 0;
}

int send_mip_route_update(
    struct ifs_data *ifs,
    struct unix_sock_sdu *router_sdu,
    struct arp_table *arp_t
){
    //create a route_table from payload
    int offset = 3; //(first 3 entries are UPD)
    struct route_table r_t;
    //first entry is the count
    r_t.count = router_sdu->payload[offset]; 
    debugprint("routing table to be sent to: %d", router_sdu->mip_addr);
    debugprint("count is %d", r_t.count);
    //copy the rest of the payload
    memcpy(&r_t.routes, router_sdu->payload+offset+1, MAX_ROUTES*3);
    print_routing_table(&r_t);

    //get the node from the arp table
    int index = arp_t_get_index_from_mip_addr(arp_t, router_sdu->mip_addr);
    
    int rc = send_mip_packet(
            ifs,
            arp_t->sll_ifindex[index],
            arp_t->sll_addr[index],
            ifs->mip_addr,
            router_sdu->mip_addr,
            MIP_TYPE_ROUTE,
            (uint8_t *)router_sdu->payload
    );

    return rc;
}
