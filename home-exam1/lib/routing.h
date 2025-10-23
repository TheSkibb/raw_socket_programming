/*
 * ROUTING:
 *  Only contains things which are shared between routing_daemon and mip_daemon
 *  All actual routing logic is contained withing routing_daemon.c
 */
#ifndef routing_h
#define routing_h

#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>
#include "utils.h"

/*
 * Message types: 
 * The message type between the routing daemon and the mip daemon are distinguished from three first characters
 * NB: Some of the messages will contain more data after the first three characters
 */

// A HELLO message for discovering neighbouring nodes.
#define ROUTING_HELLO_MSG {0x48, 0x45, 0x4c} //HEL

// An UPDATE message to advertise the routes in your routing table.
// NOTICE: you need to add the routing table after this message
#define ROUTING_UPDATE_MSG {0x55, 0x50, 0x44, 0xFF} //UPD<mip address>

//   A REQUEST message for intra-host route lookups.
//   <MIP address of the host itself (8 bits)> 
//   <zero (0) TTL (8 bits)> 
//   <R (0x52, 8 bits)> 
//   <E (0x45, 8 bits)> 
//   <Q (0x51, 8 bits)> 
//   <MIP address to look up (8 bits)>
#define ROUTING_REQUEST_MSG {0x052, 0x45, 0x51, 0xFF} //REQ<mip address>

//   A RESPONSE message for intra-host route responses. This message is fully specified in the next section.
//   <MIP address of the host itself (8 bits)> 
//   <zero (0) TTL (8 bits)> 
//   <R (0x52, 8 bits)> 
//   <S (0x53, 8 bits)> 
//   <P (0x50, 8 bits)> 
//   <next hop MIP address (8 bits)>
#define ROUTING_RESPONSE_MSG {0x51, 0x53, 0x50, 0xFF} //RSP<mip address>
                                                      
/*
 * Other:
*/

//We use a uint8_t to represent the cost of routes. 
//We use 0xFF as infinity, because in our example no route is that long
#define ROUTING_COST_INFINITY 0xFF

//amount of routes the routing table will hold
//in this scenario we have 5 nodes, so number of nodes will not exceed 5
#define MAX_ROUTES 5 

//data structure for storing where to forward packets
struct route{
    //mip address
    uint8_t dst; 
    //mip address of the node which will forward
    uint8_t next_hop;
    //how many hops it takes to get to dst with this route
    uint8_t cost;
}__attribute__((packed));

//data structure for storing routes
struct route_table{
    uint8_t count;
    struct route routes[MAX_ROUTES];
}__attribute__((packed));

void print_routing_table(struct route_table *r_t);

#endif //routing_h
