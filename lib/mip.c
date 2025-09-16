#include <linux/if_packet.h>
#include <sys/socket.h>
#include <stdio.h>

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

    struct sockaddr_ll  so_name;
    struct ether_frame  frame_hdr;
    //TODO: change out for mip header
    struct hello_header hello_hdr;
    struct msghdr       msg = {0};
    
    int iovlen      =   3;
    struct iovec        msgvec[iovlen];

    int packet_len  =   256;
    uint8_t             packet[packet_len];

    /* point to frame header */
    msgvec[0].iov_base  = &frame_hdr;
    msgvec[0].iov_len   = sizeof(struct ether_frame);

    /* point to hello header */
    msgvec[1].iov_base  = &hello_hdr;
    msgvec[1].iov_len   = sizeof(struct hello_header);

    /*point to packet (actual content)*/
    msgvec[2].iov_base  = (void *)packet;
    msgvec[2].iov_len   = packet_len;

    /* fill out message metadata struct */
    msg.msg_name    = &so_name;
    msg.msg_namelen = sizeof(struct sockaddr_ll);
    //number of vectors
    msg.msg_iovlen  = iovlen;
    msg.msg_iov     = msgvec;

    int rc = recvmsg(ifs->rsock, &msg, 0);

    printf("<info>: We got a MIP pkt. with content '%s' from node %d with MAC addr.: ",
       (char *)packet, hello_hdr.src);
	print_mac_addr(frame_hdr.src_addr, 6);




    return 0;
}
