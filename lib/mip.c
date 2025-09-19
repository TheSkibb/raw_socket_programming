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

void print_mip_arp_header(
    struct mip_arp_hdr *miparphdr
){
    printf("--------MIP ARP------\n");
    printf("type: %d\n", miparphdr->Type);
    printf("Address: %d\n", miparphdr->Address);
    printf("---------------------\n");
}

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

    //check if packet is mip arp
    if(miphdr.dst_addr == 0xFF){
        printf("type is MIP arp\n");
        struct mip_arp_hdr *miparphdr = (struct mip_arp_hdr *)&packet;
        print_mip_arp_header(miparphdr);

        printf("TYPEINFO:");
        if(miparphdr->Type == 1){
            printf("type is 1\n");
        } else if(miparphdr->Type == 0){
            printf("type is 0\n");
        } else {
            printf("mip arp type invalid\n");
            return -1;
        }
    }


    return rc;
}

int send_mip_packet(
    struct ifs_data *ifs,
    uint8_t *src_mac_addr,
    uint8_t *dst_mac_addr,
    uint8_t src_mip_addr,
    uint8_t dst_mip_addr,
    uint8_t *sdu
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
    miphdr.ttl = 1;
    miphdr.sdu_len = (sizeof(sdu)+3)/4;
    miphdr.sdu_type = 0;

    printf("sdu size: %lu, sdu_len: %d\n", sizeof(sdu), miphdr.sdu_len);

    //point to mip header
    msgvec[1].iov_base = &miphdr;
    msgvec[1].iov_len = sizeof(struct mip_hdr);

    //point to sdu
    msgvec[2].iov_base = sdu;
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

int send_mip_arp_request(
    struct ifs_data *ifs,
    uint8_t *src_mac_addr,
    uint8_t src_mip_addr,
    uint8_t dst_mip_addr
){
    //check if you are looking for your own address
    if(src_mip_addr == dst_mip_addr){
        return -1;
    }

    uint8_t eth_broadcast[] = ETH_BROADCAST;
    uint8_t mip_broadcast = 0xFF;

    struct mip_arp_hdr arphdr;

    arphdr.Type = 0;
    arphdr.Address = dst_mip_addr;

    int rc = send_mip_packet(
        ifs,
        src_mac_addr,
        eth_broadcast,
        src_mip_addr,
        mip_broadcast,
        (uint8_t *)&arphdr
    );

    return rc;
}

int send_mip_arp_response(
    struct ifs_data *ifs,
    uint8_t *src_mac_addr,
    uint8_t *dst_mac_addr,
    uint8_t src_mip_addr,
    uint8_t dst_mip_addr
    ){

    uint8_t eth_broadcast[] = ETH_BROADCAST;
    uint8_t mip_broadcast = 0xFF;

    struct mip_arp_hdr arphdr;

    arphdr.Type = 0;
    arphdr.Address = dst_mip_addr;

    int rc = send_mip_packet(
        ifs,
        src_mac_addr,
        eth_broadcast,
        src_mip_addr,
        mip_broadcast,
        (uint8_t *)&arphdr
    );

    return rc;
}
