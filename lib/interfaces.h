/* package for dealing with interfaces (and ethernet) */
#ifndef interfaces_h
#define interfaces_h

#include <stddef.h>
#include <stdint.h>
#include <linux/if_packet.h>

#define ETH_BROADCAST	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

struct ether_frame {
	uint8_t dst_addr[6];
	uint8_t src_addr[6];
	uint8_t eth_proto[2];
} __attribute__((packed));

//we have max 3 nodes in out topology, therefore max_interfaces=3
#define MAX_IF		3

/*interface data*/
struct ifs_data {
	//socket address link layer
	struct sockaddr_ll addr[MAX_IF];
	//raw socket
	int rsock;
	//interface number (unique for each interface)
	int ifn;
};

void get_mac_from_interfaces(struct ifs_data *);
void init_ifs(struct ifs_data *, int);
void print_mac_addr(uint8_t *, size_t);

#endif 
