#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>		/* getifaddrs */
#include <arpa/inet.h>		/* htons */
#include <stdint.h>
#include <linux/if_packet.h>
#include <sys/socket.h>
#include "arp_table.h"

#include "common.h"

/*
 * Print MAC address in hex format
 */
void print_mac_addr(uint8_t *addr, size_t len)
{
	size_t i;

	for (i = 0; i < len - 1; i++) {
		printf("%02x:", addr[i]);
	}
	printf("%02x\n", addr[i]);
}

/*
 * This function stores struct sockaddr_ll addresses for all interfaces of the
 * node (except loopback interface)
 */
void get_mac_from_interfaces(struct ifs_data *ifs)
{
	struct ifaddrs *ifaces, *ifp;
	int i = 0;

	/* Enumerate interfaces: */
	/* Note in man getifaddrs that this function dynamically allocates
	   memory. It becomes our responsability to free it! */
	if (getifaddrs(&ifaces)) {
		perror("getifaddrs");
		exit(-1);
	}

	/* Walk the list looking for ifaces interesting to us */
	for (ifp = ifaces; ifp != NULL; ifp = ifp->ifa_next) {
		/* We make sure that the ifa_addr member is actually set: */
		if (ifp->ifa_addr != NULL &&
		    ifp->ifa_addr->sa_family == AF_PACKET &&
		    strcmp("lo", ifp->ifa_name)) //ignore loopback address
		/* Copy the address info into the array of our struct */
		memcpy(&(ifs->addr[i++]),
		       (struct sockaddr_ll*)ifp->ifa_addr,
		       sizeof(struct sockaddr_ll));
	}
	/* After the for loop, the address info of all interfaces are stored */
	/* Update the counter of the interfaces */
	ifs->ifn = i;

	/* Free the interface list */
	freeifaddrs(ifaces);
}

void init_ifs(
    struct ifs_data *ifs, 
    int rsock
){
	/* Walk through the interface list */
	get_mac_from_interfaces(ifs);

	/* We use one RAW socket per node */
	ifs->rsock = rsock;
}

int create_raw_socket(void){
	//socket descriptor
	int sd;
	//change number to the one in the oblig
	short unsigned int protocol = 0xFFFF;

	/* Set up a raw AF_PACKET socket without ethertype filtering */
	sd = socket(AF_PACKET, SOCK_RAW, htons(protocol));
	if (sd == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	return sd;
}

int send_arp_request(
    struct ifs_data *ifs
){
	struct ether_frame frame_hdr;
	struct msghdr	*msg;
	struct iovec	msgvec[1];
	int    rc;

	/* Fill in Ethernet header. ARP request is a BROADCAST packet. */
	uint8_t dst_addr[] = ETH_BROADCAST;

	memcpy(frame_hdr.dst_addr, dst_addr, 6);
	memcpy(frame_hdr.src_addr, ifs->addr[0].sll_addr, 6);

	/* Match the ethertype in packet_socket.c: */
	frame_hdr.eth_proto[0] = frame_hdr.eth_proto[1] = 0xFF;

	/* Point to frame header */
	msgvec[0].iov_base = &frame_hdr;
	msgvec[0].iov_len  = sizeof(struct ether_frame);

	/* Allocate a zeroed-out message info struct */
	msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));

	/* Fill out message metadata struct */
	/* host A and C (senders) have only one interface, which is stored in
	 * the first element of the array when we walked through the interface
	 * list.
	 */
	//NB: set to statically use the first address in the ifs struct
	msg->msg_name	 = &(ifs->addr[0]);
	msg->msg_namelen = sizeof(struct sockaddr_ll);
	msg->msg_iovlen	 = 1;
	msg->msg_iov	 = msgvec;

	/* Send message via RAW socket */
	rc = sendmsg(ifs->rsock, msg, 0);
	if (rc == -1) {
		perror("sendmsg");
		free(msg);
		return -1;
	}

	/* Remember that we allocated this on the heap; free it */
	free(msg);

	return rc;
}

int handle_arp_packet(struct ifs_data *ifs)
{
	struct sockaddr_ll so_name;
	struct ether_frame frame_hdr;
	struct msghdr	msg = {0};
	struct iovec	msgvec[1];
	int    rc;

	/* Point to frame header */
	msgvec[0].iov_base = &frame_hdr;
	msgvec[0].iov_len  = sizeof(struct ether_frame);

	/* Fill out message metadata struct */
	msg.msg_name	= &so_name;
	msg.msg_namelen = sizeof(struct sockaddr_ll);
	msg.msg_iovlen	= 1;
	msg.msg_iov	= msgvec;

	rc = recvmsg(ifs->rsock, &msg, 0);
	if (rc <= 0) {
		perror("sendmsg");
		return -1;
	}

	/* Send back the ARP response via the same receiving interface */
	/* Send ARP response only if the request was a broadcast ARP request
	 * This is so dummy!
	 */
	int check = 0;
	uint8_t brdcst[] = ETH_BROADCAST;
	for (int i = 0; i < 6; i++) {
		if (frame_hdr.dst_addr[i] != brdcst[i])
		check = -1;
	}
	if (!check) {
		/* Handling an ARP request */
		printf("\nWe received a handshake offer from the neighbor: ");
		print_mac_addr(frame_hdr.src_addr, 6);

		/* print the if_index of the receiving interface */
		printf("We received an incoming packet from iface with index %d\n",
		       so_name.sll_ifindex);

		rc = send_arp_response(ifs, &so_name, frame_hdr);
		if (rc < 0)
		perror("send_arp_response");
	}

	/* Node received an ARP Reply */
	printf("\nHello from neighbor ");
	print_mac_addr(frame_hdr.src_addr, 6);

	return rc;
}

int send_arp_response(
    struct ifs_data *ifs,
    struct sockaddr_ll *so_name,
    struct ether_frame frame
){
	struct msghdr *msg;
	struct iovec msgvec[1];
	int rc;

	/* Swap MAC addresses of the ether_frame to send back (unicast) the ARP
	 * response */
	memcpy(frame.dst_addr, frame.src_addr, 6);

	/* Find the MAC address of the interface where the broadcast packet came
	 * from. We use sll_ifindex recorded in the so_name. */
	for (int i = 0; i < ifs->ifn; i++) {
		if (ifs->addr[i].sll_ifindex == so_name->sll_ifindex)
		memcpy(frame.src_addr, ifs->addr[i].sll_addr, 6);
	}
	/* Match the ethertype in packet_socket.c: */
	frame.eth_proto[0] = frame.eth_proto[1] = 0xFF;

	/* Point to frame header */
	msgvec[0].iov_base = &frame;
	msgvec[0].iov_len  = sizeof(struct ether_frame);

	/* Allocate a zeroed-out message info struct */
	msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));

	/* Fill out message metadata struct */
	msg->msg_name	 = so_name;
	msg->msg_namelen = sizeof(struct sockaddr_ll);
	msg->msg_iovlen	 = 1;
	msg->msg_iov	 = msgvec;

	/* Construct and send message */
	rc = sendmsg(ifs->rsock, msg, 0);
	if (rc == -1) {
		perror("sendmsg");
		free(msg);
		return -1;
	}

	printf("Nice to meet you ");
	print_mac_addr(frame.dst_addr, 6);

	printf("I am ");
	print_mac_addr(frame.src_addr, 6);

	/* Remember that we allocated this on the heap; free it */
	free(msg);

	return rc;
}

/* 
 * send a raw packet.
*/
int send_raw_packet(
        int socketfd,                   //socket file descriptor
        struct sockaddr_ll *so_name,    //socket addr link layer
        uint8_t *buf,                   //what we are sending, when we construct the mip packet, we can put it into here
        size_t len,                     //length of what we are sending
        uint8_t dest_addr[]             //what we are sending to
){
    //return code
    int rc = 0;
    printf("1");

    //ethernet frame header
    struct ether_frame frame_hdr;
    //message to be sent or received
    struct msghdr *msg;
    //iovector
    struct iovec msgvec[2];

    //put the src and dest addresses into the ethernet frame
    memcpy(frame_hdr.dst_addr, dest_addr, 6);
    memcpy(frame_hdr.src_addr, so_name->sll_addr, 6);
    printf("2");

    //set the ethertype of the ethernet frame
    frame_hdr.eth_proto[0] = 0xFF;
    frame_hdr.eth_proto[1] = 0xFF;
    printf("3");

    //point to frame header
    msgvec[0].iov_base = &frame_hdr;
    msgvec[0].iov_len = sizeof(struct ether_frame);
    printf("4");

    //point to frame payload
    msgvec[1].iov_base = buf;
    msgvec[1].iov_len = len;
    printf("5");


    //allocate a zeroed-out message info struct
    msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));
    printf("6");

    /* fill out message metadata */
    msg->msg_name = so_name;
    msg->msg_namelen = sizeof(struct sockaddr_ll);
    msg->msg_iovlen = 2;
    msg->msg_iov = msgvec;
    printf("7");

    //actually send the message to the raw socket file descriptor
    rc = sendmsg(socketfd, msg, 0);
    if(rc == -1){
        perror("sendmsg");
        free(msg);
        return 1;
    }
    printf("8");

    free(msg);

    return rc;
}

//returns a buffer of the mip arp packet, which can be sent with send_raw_packet
uint8_t *create_mip_arp_packet(
        uint8_t address, // address we want to look up
        ht *arp_table, // pointer to the arp_table (not used in this snippet)
        int type
) {
    // Validate the type
    if (type != 0 && type != 1) {
        printf("type is wrong\n");
        return NULL;
    }

    // Allocate memory for the arp packet, should be 32 bits
    mip_arp_packet *arp_packet = malloc(sizeof(mip_arp_packet));
    if (arp_packet == NULL) {
        printf("cannot allocate memory for arp_packet\n");
        return NULL;
    }

    // Fill the arp_packet fields
    arp_packet->Type = type;
    arp_packet->Address = address;
    arp_packet->Reserved = 0;

    // Create a buffer from the arp packet
    uint8_t *buf = malloc(sizeof(mip_arp_packet));
    if(buf == NULL){
        printf("cannot malloc bits for packet buffer\n");
        free(arp_packet); // Free the allocated arp_packet memory
        return NULL;
    }

    // Copy the packet into the buffer
    memcpy(buf, arp_packet, sizeof(mip_arp_packet));

    // Free arp_packet memory as it's no longer needed
    free(arp_packet);

    return buf;
}

int receive_raw_packet(
    int socketfd, 
    uint8_t *buf, 
    size_t len
){

    struct ether_frame frame_hdr;
    struct iovec msgvec[2];

    //point to frame header
    msgvec[0].iov_base = &frame_hdr;
    msgvec[0].iov_len = sizeof(struct ether_frame);

    //point to frame payload
    msgvec[1].iov_base = buf;
    msgvec[1].iov_len = len;

    struct sockaddr_ll so_name;
    struct msghdr msg;

    //fill out message metadata struct
    msg.msg_name = &so_name;
    msg.msg_namelen = sizeof(struct sockaddr_ll);
    msg.msg_iovlen = 2;
    msg.msg_iov = msgvec;

    int rc = recvmsg(socketfd, &msg, 0);
    if(rc == -1) {
        perror("sendmsg");
        return -1;
    }

    //noe memcpy greier her

    return 0;
}

int recv_raw_packet(int sd, uint8_t *buf, size_t len)
{
	struct sockaddr_ll so_name;
	struct ether_frame frame_hdr;
	struct msghdr	   msg;
	struct iovec	   msgvec[2];
	int    rc;

	/* Point to frame header */
	msgvec[0].iov_base = &frame_hdr;
	msgvec[0].iov_len  = sizeof(struct ether_frame);
	/* Point to frame payload */
	msgvec[1].iov_base = buf;
	msgvec[1].iov_len  = len;
    //printf("recv len %ld", len);

	/* Fill out message metadata struct */
	msg.msg_name	= &so_name;
	msg.msg_namelen = sizeof(struct sockaddr_ll);
	msg.msg_iovlen	= 2;
	msg.msg_iov	= msgvec;

	rc = recvmsg(sd, &msg, 0);
	if (rc == -1) {
		perror("sendmsg");
		return -1;
	}
	/*
	 * Copy the src_addr of the current frame to the global dst_addr We need
	 * that address as a dst_addr for the next frames we're going to send
	 * from the server
	 */
	//memcpy(dst_addr, frame_hdr.src_addr, 6);

	return rc;
}

