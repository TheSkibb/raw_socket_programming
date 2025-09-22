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
#include "utils.h"

struct hello_header {
	uint8_t dest;
	uint8_t src;
	uint8_t sdu_len : 4;
} __attribute__((packed));

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

    //mip arp handling
    if(miphdr.sdu_type == MIP_TYPE_ARP &&  miphdr.dst_addr == 0xFF){
        debugprint("type is MIP arp");
        struct mip_arp_hdr *miparphdr = (struct mip_arp_hdr *)&packet;

        if(miparphdr->Type == MIP_ARP_TYPE_REQUEST){
            debugprint("received MIP ARP request: ");
            send_mip_arp_response(ifs, ethhdr.dst_mac, ethhdr.src_mac, miphdr.dst_addr, miphdr.dst_addr);
        } else if(miparphdr->Type == MIP_ARP_TYPE_RESPONSE){
            debugprint("received MIP ARP response: ");
        } else {
            debugprint("mip arp type invalid");
            return -1;
        }
        print_mip_arp_header(miparphdr);
    }

    //send up to unix socket
    if(miphdr.sdu_type == MIP_TYPE_PING){
        debugprint("received a PING message");
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

    debugprint("sending SDU with number of bytes: %lu, and sdu_len: %d", sizeof(sdu), miphdr.sdu_len);

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
    
    debugprint("MIP packet sent to:");
    print_mac_addr(ethhdr.dst_mac, 6);

    free(msg);

    return rc;
}

int send_mip_arp_request(
    struct ifs_data *ifs,
    uint8_t *src_mac_addr,
    uint8_t src_mip_addr,
    uint8_t dst_mip_addr
){
    debugprint("preparing to send mip arp request\n");
    //check if you are looking for your own address
    if(src_mip_addr == dst_mip_addr){
        return -1;
    }

    uint8_t eth_broadcast[] = ETH_BROADCAST;
    uint8_t mip_broadcast = 0xFF;

    struct mip_arp_hdr arphdr;

    arphdr.Type = 0;
    arphdr.Address = dst_mip_addr;

    printf("sending mip arp packet:\n");
    print_mip_arp_header(&arphdr);
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

    arphdr.Type = 1;
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
