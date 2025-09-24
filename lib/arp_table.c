#include <stdio.h>
#include <string.h>

#include "arp_table.h"
#include "utils.h"

int arp_t_add_entry(
    struct arp_table *arp_t,
    uint8_t mip_addr,
    int sll_ifindex,
    uint8_t sll_addr[6]
){
    int i = arp_t->arp_table_n;
    arp_t->mip_addr[i] = mip_addr;
    arp_t->sll_ifindex[i] = sll_ifindex;
    memcpy(arp_t->sll_addr[i], sll_addr, 6);
    arp_t->arp_table_n++;
    return arp_t->arp_table_n-1;
}

int arp_t_get_index_from_mip_addr(
    struct arp_table *arp_t,
    uint8_t mip_addr
){
    for(int i = 0; i < arp_t->arp_table_n; i++){
        if(arp_t->mip_addr[i] == mip_addr){
            printf("we found the address!!!! @%d", i);
            return i;
        }
    }
    return -1;
}
