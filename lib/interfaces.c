#include <sys/types.h>
#include <stdio.h>
#include <ifaddrs.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "interfaces.h"
#include "utils.h"

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

/*
 * Print MAC address in hex format
 */
void print_mac_addr(uint8_t *addr, size_t len)
{
    //get debug returns 0 or 1
    if(get_debug() != 1){
        return ;
    }
	size_t i;

	for (i = 0; i < len - 1; i++) {
		printf("%02x:", addr[i]);
	}
	printf("%02x\n", addr[i]);
}
