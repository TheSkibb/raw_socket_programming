#ifndef MIP_H
#define MIP_H

#include <stdint.h>
#include <sys/socket.h>

#include "interfaces.h"

#define MIP_TYPE_ARP_REQUEST 0x0
#define MIP_TYPE_ARP_RESPONSE 0x1

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

struct pdu {
    struct eth_hdr  ethhdr;
    struct mip_hdr  mip_hdr;
    uint8_t         *sdu;
} __attribute__((packed));


/* functions */

//send a mip packet
int send_mip_packet(
    //interface data
    struct ifs_data *ifs, 
    //mac address of sender
    uint8_t *src_mac_addr,
    //mac address of recipient
    uint8_t *dst_mac_addr,
    //mip address of sender
    uint8_t src_hip_addr,
    //mip address of recipient
    uint8_t dst_hip_addr,
    uint8_t *sdu
);
int handle_mip_packet(
        struct ifs_data *ifs
);
int send_mip_arp_request(
    struct ifs_data *ifs,
    uint8_t *src_mac_addr,
    uint8_t src_mip_addr,
    uint8_t dst_mip_addr
);

#endif

