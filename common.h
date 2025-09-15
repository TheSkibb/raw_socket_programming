#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>		/* uint8_t */
#include <unistd.h>		/* size_t */
#include <linux/if_packet.h>	/* struct sockaddr_ll */

#define MAX_EVENTS	10
//we have max 3 nodes in out topology, therefore max_interfaces=3
#define MAX_IF		3

#define ETH_BROADCAST	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

struct ether_frame {
	uint8_t dst_addr[6];
	uint8_t src_addr[6];
	uint8_t eth_proto[2];
} __attribute__((packed));

struct ifs_data {
	//socket address link layer
	struct sockaddr_ll addr[MAX_IF];
	//raw socket
	int rsock;
	//interface number (unique for each interface)
	int ifn;
};

/* mip arp packet specification
    +--------+-----------+--------------------+
    | Type   | Address   | Padding/ Reserved  |
    +--------+-----------+--------------------+
    | 1 bit  | 8 bits    | 23 bits of zeroes  |
    +--------+-----------+--------------------+
*/
typedef struct {
    uint32_t Type : 1;
    uint32_t Address : 8;
    uint32_t Reserved : 23;
} __attribute__((packed)) mip_arp_packet;


/* mip packet specification
     +--------------+-------------+---------+-----------+-----------+
     | Dest. Addr.  | Src. Addr.  | TTL     | SDU Len.  | SDU type  |
     +--------------+-------------+---------+-----------+-----------+
     | 8 bits       | 8 bits      | 4 bits  | 9 bits    | 3 bits    |
     +--------------+-------------+---------+-----------+-----------+
*/
typedef struct {
    uint32_t dst_addr : 8;
} __attribute__((packed)) mip_packet;

void get_mac_from_interfaces(struct ifs_data *);
void print_mac_addr(uint8_t *, size_t);
void init_ifs(struct ifs_data *, int);
int create_raw_socket(void);
int send_arp_request(struct ifs_data *);
int send_arp_response(struct ifs_data *, struct sockaddr_ll *,
		      struct ether_frame);
int handle_arp_packet(struct ifs_data *);
int send_raw_packet(
        int socketfd, 
        struct sockaddr_ll *so_name, 
        uint8_t *buf, 
        size_t len,
        uint8_t dest_addr[]
);
int receive_raw_packet(
    int socketfd, 
    uint8_t *buf, 
    size_t len
);
int recv_raw_packet(int sd, uint8_t *buf, size_t len);


#endif /* _COMMON_H */
