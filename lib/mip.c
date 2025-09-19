#include <linux/if_packet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <asm-generic/errno-base.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#include "interfaces.h"
#include "mip.h"

struct hello_header {
	uint8_t dest;
	uint8_t src;
	uint8_t sdu_len : 4;
} __attribute__((packed));

int handle_mip_packet(
        struct ifs_data *ifs
    ){

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
	/* We can read up to 256 characters. Who cares? PONG is only 5 bytes */
	msgvec[2].iov_len  = 256;

    //fill out message metadata
    msg.msg_name = &so_name;
    msg.msg_namelen = sizeof(struct sockaddr_ll);
    msg.msg_iovlen = iov_len;
    msg.msg_iov = msgvec;

    rc = recvmsg(ifs->rsock, &msg, 0);
    if(rc <= 0){
        perror("recvmsg");
        return -1;
    }

    printf("we got a packet for address: %d, with content %s\n", 
            miphdr.dst_addr, 
            (char *)packet);

    if(strcmp("ping", (char *)packet) == 0){
        uint8_t broadcast[] = ETH_BROADCAST;
        rc = send_mip_packet(ifs, ifs->addr[0].sll_addr, broadcast, 0x01, 0x02, "pong");
    }

    return rc;
}

int send_mip_packet(
    struct ifs_data *ifs,
    uint8_t *src_mac_addr,
    uint8_t *dst_mac_addr,
    uint8_t src_mip_addr,
    uint8_t dst_mip_addr,
    const char *sdu
){
    struct eth_hdr ethhdr;
    struct mip_hdr miphdr;
    //TODO: send buf
    struct msghdr *msg;

    int iov_len = 3;
    struct iovec msgvec[iov_len];
    int rc;

    // fill in eth_hdr 
    memcpy(ethhdr.dst_mac, dst_mac_addr, 6);
    memcpy(ethhdr.src_mac, src_mac_addr, 6);

    //TODO: set to MIP type
    ethhdr.ethertype = htons(0xFFFF);

    //point to ethernet header
    msgvec[0].iov_base = &ethhdr;
    msgvec[0].iov_len = sizeof(struct eth_hdr);

    //fill in mip hdr
    miphdr.dst_addr = dst_mip_addr;
    miphdr.src_addr = src_mip_addr;

    //point to mip header
    msgvec[1].iov_base = &miphdr;
    msgvec[1].iov_len = sizeof(struct mip_hdr);

    //point to sdu
    msgvec[2].iov_base = (uint8_t *)sdu;
    msgvec[2].iov_len = sizeof(sdu);

    //allocate a zeroed out message info struct
    msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));

    //fill out message metadata struct
    msg->msg_name = &(ifs->addr[0]);
    msg->msg_namelen = sizeof(struct sockaddr_ll);
    msg->msg_iovlen = iov_len;
    msg->msg_iov = msgvec;


    rc = sendmsg(ifs->rsock, msg, 0);
    if(rc == -1){
        perror("sendmsg");
        free(msg);
        return -1;
    }
    
    print_mac_addr(ethhdr.dst_mac, 6);
    printf("\n");

    free(msg);

    return rc;
}

