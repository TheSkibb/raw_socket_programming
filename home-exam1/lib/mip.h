#ifndef MIP_H
#define MIP_H

#include <stdint.h>
#include <sys/socket.h>

#include "interfaces.h"
#include "arp_table.h"
#include "sockets.h"

#define MIP_TYPE_ARP 0x01
#define MIP_TYPE_PING 0x02

#define MIP_ARP_TYPE_REQUEST 0x0
#define MIP_ARP_TYPE_RESPONSE 0x1

/* mip arp packet specification
    +--------+-----------+--------------------+
    | Type   | Address   | Padding/ Reserved  |
    +--------+-----------+--------------------+
    | 1 bit  | 8 bits    | 23 bits of zeroes  |
    +--------+-----------+--------------------+
*/
struct mip_arp_hdr{
    uint32_t Type : 1;
    uint32_t Address : 8;
    uint32_t Reserved : 23;
} __attribute__((packed));

//TODO: change out for actual max buffer size, calculated from sdu length
#define MAX_BUF_SIZE 1024


/* mip packet specification
     +--------------+-------------+---------+-----------+-----------+
     | Dest. Addr.  | Src. Addr.  | TTL     | SDU Len.  | SDU type  |
     +--------------+-------------+---------+-----------+-----------+
     | 8 bits       | 8 bits      | 4 bits  | 9 bits    | 3 bits    |
     +--------------+-------------+---------+-----------+-----------+
total: 32 bits
*/
struct mip_hdr {
    uint32_t dst_addr   : 8;
    uint32_t src_addr   : 8;
    uint32_t ttl        : 8;
    uint32_t sdu_len    : 9;
    uint32_t sdu_type   : 3;
} __attribute__((packed));

//we use this struct to return all the information about a received packet from recv_mip_packet
struct pdu {
    struct eth_hdr  ethhdr;
    struct mip_hdr  mip_hdr;
    uint8_t         sdu[256];
} __attribute__((packed));

/* functions */

//send a mip packet
//writes the contents of sdu to raw socket
//returns 
int send_mip_packet(
    //interface data
    struct ifs_data *ifs, 
    //the index of the interface to use
    int addr_index,
    //mac address of recipient
    uint8_t *dst_mac_addr,
    //mip address of recipient
    uint8_t dst_mip_addr,
    //type of mip package
    uint8_t type,
    //service data unit (the actual data we are sending)
    uint8_t *sdu
);

int recv_mip_packet(
    struct ifs_data *ifs,
    struct arp_table *arp_t,
    struct unix_sock_sdu *sdu,
    int socket_unix,
    struct pdu *out_pdu,
    int *out_received_index
);

//reads from ifs.rsock and puts the contents into the
/*
int recv_mip_packet(
    struct ifs_data *ifs,
    struct arp_table *arp_t,
    struct unix_sock_sdu *sdu
);
*/

//the handle_mip_packet does different things based on the received packet
//if MIP_TYPE_ARP && MIP_ARP_TYPE_REQUEST -> save in arp cache and send_mip_arp_response
//if MIP_TYPE_ARP && MIP_ARP_TYPE_RESPONSE -> save in arp cache,
//                  check if there is a message we want to send to this address (by checking contents of sdu)
//                  if sdu.mip_addr == MIP_ARP_TYPE_RESPONSE.mip_addr -> send_mip_packet
//if MIP_TYPE_PING -> create unix_sock_sdu and send to unix socket
int handle_mip_packet(
    struct ifs_data *ifs,
    struct arp_table *arp_t,
    struct unix_sock_sdu *sdu,
    int socket_unix,
    struct pdu *mip_pdu,
    int received_index
);

//send a mip arp package
int send_mip_arp_request(
    //interfaces
    struct ifs_data *ifs,
    //the mip address you are looking for
    uint8_t dst_mip_addr
);

//sends a mip arp response packet
int send_mip_arp_response(
    struct ifs_data *ifs,
    int interface_index,
    uint8_t *dst_mac_addr,
    uint8_t dst_mip_addr
);

#endif

