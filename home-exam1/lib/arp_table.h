#ifndef ARP_T_H
#define ARP_T_H

#include <stdint.h>

//#include "mip.h"
//
#define MAX_MIP_NODES 5
#define MAC_ADDRESS_LENGTH 6

//structure for storing mip addresses and the corresponding interface to send
struct arp_table {
    //mip address
    uint8_t mip_addr[MAX_MIP_NODES];
    //interface number (in ifs_data.addr)
    int sll_ifindex[MAX_MIP_NODES];
    //mac address
    uint8_t sll_addr[MAX_MIP_NODES][MAC_ADDRESS_LENGTH];
    //number of entries in the arp table
    int arp_table_n;
};

//adds a new entry to the arp table
//returns -1 if there is not enough space for the new entry
int arp_t_add_entry(
    struct arp_table *arp_t,
    uint8_t mip_addr,
    int sll_ifindex,
    uint8_t sll_addr[6]
);

//returns the index of a mip address
//returns -1 if not found
int arp_t_get_index_from_mip_addr(
    struct arp_table *arp_t,
    uint8_t mip_addr
);

void arp_t_print_contents(struct arp_table *arp_t);

#endif // ARP_T_H
