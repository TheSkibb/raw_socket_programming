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

/*
struct pdu *alloc_pdu(void){
    struct pdu *pdu = (struct pdu *)malloc(sizeof(struct pdu));

    pdu->ethhdr = (struct eth_hdr *)malloc(sizeof(struct eth_hdr));
    //TODO: maybe change for MIP type
    pdu->ethhdr->ethertype = htons(0xFFFF); 

    pdu->mip_hdr = (struct mip_hdr *)malloc(sizeof(struct mip_hdr));

    return pdu;

}
*/

int handle_mip_packet(
        struct ifs_data *ifs
    ){

    struct pdu *pdu = (struct pdu *)malloc(sizeof(struct pdu));
    if(NULL==pdu){
        perror("malloc");
        return -ENOMEM;
    }

    uint8_t rcv_buf[MAX_BUF_SIZE];

    /*receive from the serialized buffer */
    if(recvfrom(
        ifs->rsock, 
        rcv_buf, 
        MAX_BUF_SIZE, 
        0, 
        NULL, 
        NULL) <= 0
    ){
        perror("recvfrom()");
        close(ifs->rsock);
    }

    //size_t rcv_len = 1024; //mip_deserialize_pdu(pdu, rcv_buf);

    printf("received a PDU");
    //TODO: handle what kind of mip packet it is

    return 0;
}

/*
void print_pdu_content(struct pdu *pdu)
{
	printf("====================================================\n");
	printf("\t Source MAC address: ");
	print_mac_addr(pdu->ethhdr->src_mac, 6);
	printf("\t Destination MAC address: ");
	print_mac_addr(pdu->ethhdr->dst_mac, 6);
	printf("\t Ethertype: 0x%04x\n", pdu->ethhdr->ethertype);

	printf("\t Source HIP address: %u\n", pdu->mip_hdr->src_addr);
	printf("\t Destination HIP address: %u\n", pdu->mip_hdr->dst_addr);
	printf("\t SDU length: %d\n", pdu->mip_hdr->sdu_len * 4);
	printf("\t HIP protocol version: %u\n", pdu->mip_hdr->ttl);
	printf("\t PDU type: 0x%02x\n", pdu->mip_hdr->sdu_type);

	printf("\t SDU: %s\n", pdu->sdu);
	printf("====================================================\n");
}
*/

/*
void fill_pdu(struct pdu *pdu,
	      uint8_t *src_mac_addr,
	      uint8_t *dst_mac_addr,
	      uint8_t src_hip_addr,
	      uint8_t dst_hip_addr,
	      const char *sdu)
{
	size_t slen = 0;
	
	memcpy(pdu->ethhdr->dst_mac, dst_mac_addr, 6);
	memcpy(pdu->ethhdr->src_mac, src_mac_addr, 6);
    //TODO:change for actual mip type
	pdu->ethhdr->ethertype = htons(0xFFFF);
	
        pdu->mip_hdr->dst_addr = dst_hip_addr;
        pdu->mip_hdr->src_addr = src_hip_addr;
        pdu->mip_hdr->ttl= 0x01;
        //TODO: handle type properly
        pdu->mip_hdr->sdu_type = 0x01;

	slen = strlen(sdu) + 1;

    //TODO: look at sdu length calculation
	// SDU length must be divisible by 4 
    //can be used in mip implementation
	if (slen % 4 != 0){
		slen = slen + (4 - (slen % 4));
    }

	// to get the real SDU length in bytes, the len value is multiplied by 4 
    pdu->mip_hdr->sdu_len = slen * 4;

	pdu->sdu = (uint8_t *)calloc(1, slen);
	memcpy(pdu->sdu, sdu, slen);
}
*/

/*
// copy struct to a byte array
size_t mip_serialize_pdu(
    struct pdu *pdu, //pdu we want to serialize
    uint8_t *snd_buf //buffer we want to copy the serialized pdu into, and later send
){
	size_t snd_len = 0;

	// Copy ethernet header 
    //copy from the beginning of the buffer, snd_len = 0
	memcpy(snd_buf + snd_len, pdu->ethhdr, sizeof(struct eth_hdr)); //why sizeof instead of ETH_HDR_LEN?
    //advance with length of ethernet header
	snd_len += sizeof(struct eth_hdr);

	// Copy HIP header 
    //allocate a 32 bit int for hiphdr, then bitwise or with bitshifted fields of pdu.hiphdr
	uint32_t hiphdr = 0; 
	hiphdr |= (uint32_t) pdu->mip_hdr->dst_addr << 24; //8 bits (32 - 8 = 24)
	hiphdr |= (uint32_t) pdu->mip_hdr->src_addr << 16; //8 bits (32 - 8 - 8 = 16)
	hiphdr |= (uint32_t) (pdu->mip_hdr->ttl & 0xf) << 12; //4 bits (32 - 8 - 8 - 4 = 12)
	hiphdr |= (uint32_t) (pdu->mip_hdr->sdu_len & 0xff) << 3; //9 bits (32 - 8 - 4 - 9 = 3)
    //TODO: noe her med type som er tre bits
	hiphdr |= (uint32_t) (pdu->mip_hdr->sdu_type & 0xf) >> 1; //3 bits (32 - 8 - 8 - 4 - 9 - 3 = 0)

	// prepare it to be sent from host to network 
	hiphdr = htonl(hiphdr);

    //copy hipheader into the send buffer
	memcpy(snd_buf + snd_len, &hiphdr, sizeof(struct mip_hdr));
	snd_len += sizeof(struct mip_hdr);

	// Attach SDU 
    //copy message into the buffer
    memcpy(snd_buf + snd_len, pdu->sdu, pdu->mip_hdr->sdu_len * 4);

	snd_len += pdu->mip_hdr->sdu_len * 4;

	return snd_len;
}
*/

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
    struct iovec msgvec[2];
    int rc;

    // fill in eth_hdr 
    memcpy(ethhdr.dst_mac, dst_mac_addr, 6);
    memcpy(ethhdr.src_mac, src_mac_addr, 6);

    //TODO: set to MIP type
    ethhdr.ethertype = htons(0xFF);

    //point to ethernet header
    msgvec[0].iov_base = &ethhdr;
    msgvec[0].iov_len = sizeof(struct eth_hdr);

    //fill in mip hdr
    miphdr.dst_addr = dst_mip_addr;
    miphdr.src_addr = src_mip_addr;

    //point to mip header
    msgvec[1].iov_base = &miphdr;
    msgvec[1].iov_len = sizeof(struct mip_hdr);

    //allocate a zeroed out message info struct
    msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));

    //fill out message metadata struct
    msg->msg_name = &(ifs->addr[0]);
    msg->msg_namelen = sizeof(struct sockaddr_ll);
    msg->msg_iovlen = 2;
    msg->msg_iov = msgvec;


    rc = sendmsg(ifs->rsock, msg, 0);
    if(rc == -1){
        perror("sendmsg");
        free(msg);
        return -1;
    }
    
    printf("sending a %lu bytes MIP pkt to:", sizeof(msg));
    print_mac_addr(ethhdr.dst_mac, 6);
    printf("\n");

    free(msg);

    return rc;
}

