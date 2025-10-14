/* package for dealing with interfaces (and ethernet) */
#ifndef interfaces_h
#define interfaces_h

#include <stddef.h>
#include <stdint.h>
#include <linux/if_packet.h>

#define ETH_BROADCAST	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

//ethernet header
struct eth_hdr {
	uint8_t  dst_mac[6];
	uint8_t  src_mac[6];
	uint16_t ethertype;
} __attribute__((packed));

//we have max 3 nodes in out topology, therefore max_interfaces=3
#define MAX_IF 3

/*interface data*/
struct ifs_data {
	//socket address link layer
	struct sockaddr_ll addr[MAX_IF];
	//raw socket
	int rsock;
    //unix socket
    int usock;
    //unix socket router
    int rusock;
	//interface number (unique for each interface)
	int ifn;
    //mip address
    uint8_t mip_addr;
};

void get_mac_from_interfaces(struct ifs_data *);
void init_ifs(struct ifs_data *, int);
void print_mac_addr(uint8_t *, size_t);

#endif 
