#include <linux/if_packet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <asm-generic/errno-base.h>
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

int handle_mip_packet(
    struct ifs_data *ifs,
    struct arp_table *arp_t,
    struct unix_sock_sdu *sdu,
    int socket_unix
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

    print_mip_header(&miphdr);

    //mip arp handling
    if(miphdr.sdu_type == MIP_TYPE_ARP){
        debugprint("type is MIP arp");
        struct mip_arp_hdr *miparphdr = (struct mip_arp_hdr *)&packet;

        //check if this address already is in our mip cache
        int index = arp_t_get_index_from_mip_addr(arp_t, miphdr.src_addr);

        debugprint("index from table: %d", index);

        if(index == -1){
            debugprint("address was not found in table");
            index = arp_t_add_entry(arp_t, miphdr.src_addr, received_index, ethhdr.src_mac);
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
                ethhdr.src_mac, 
                miphdr.dst_addr
            );

        } else if(miparphdr->Type == MIP_ARP_TYPE_RESPONSE){
            //check if we want to send any message to this address
            if(miphdr.src_addr == sdu->mip_addr){
            debugprint("received MIP ARP from %d", miphdr.src_addr);
                send_mip_packet(
                    ifs,
                    arp_t->sll_ifindex[index],
                    arp_t->sll_addr[index],
                    sdu->mip_addr,
                    MIP_TYPE_PING,
                    (uint8_t *)sdu->payload
                );
            } else {
                debugprint("received MIP ARP from %d, which we are not interested in", miphdr.src_addr);
            }
        } else {
            debugprint("mip arp type invalid");
            return -1;
        }
        print_mip_arp_header(miparphdr);
    }

    //send up to unix socket
    if(miphdr.sdu_type == MIP_TYPE_PING){
        debugprint("received PING message: %s", packet);
        memcpy(sdu->payload, packet, 256);
        sdu->mip_addr = miphdr.src_addr;
        int w = 0;
        if(socket_unix != -1){
            w = write(socket_unix, sdu, sizeof(struct unix_sock_sdu));
        }
        if (w == -1 || w == 0) {
           perror("write");
           exit(EXIT_FAILURE);
        }
    }

    return miphdr.sdu_type;
}

int send_mip_packet(
    struct ifs_data *ifs,
    int addr_index,
    uint8_t *dst_mac_addr,
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
    miphdr.src_addr = ifs->mip_addr;
    miphdr.ttl = 1;
    miphdr.sdu_len = (sizeof(sdu)+3)/4;
    miphdr.sdu_type = type;

    debugprint("sending SDU with number of bytes: %lu, and sdu_len: %d", sizeof(sdu), miphdr.sdu_len);

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

    debugprint("MIP packet sent to:");
    print_mac_addr(ethhdr.dst_mac, 6);

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
    uint8_t mip_broadcast = 0xFF;

    struct mip_arp_hdr arphdr;

    arphdr.Type = MIP_ARP_TYPE_RESPONSE;
    arphdr.Address = dst_mip_addr;

    int rc = send_mip_packet(
        ifs,
        interface_index,
        eth_broadcast,
        mip_broadcast,
        MIP_TYPE_ARP,
        (uint8_t *)&arphdr
    );

    return rc;
}
