#ifndef PDU_H
#define PDU_H

#include <stdint.h>

struct pdu {
	struct eth_hdr *ethhdr;
	struct mip_hdr *miphdr;
	uint8_t        *sdu;
} __attribute__((packed));

#endif
