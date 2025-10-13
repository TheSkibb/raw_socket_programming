#ifndef routing_h
#define routing_h

#include <stdint.h>
#include <sys/time.h>

/*
 * below are some messages which can be used as payload in the
 * unix_sock_sdu from the sockets library.
 * NOTICE: some of the messages will need you to 
 */

// A HELLO message for discovering neighbouring nodes.
#define ROUTING_HELLO_MSG {0x48, 0x45, 0x4c} //HEL

// An UPDATE message to advertise the routes in your routing table.
// NOTICE: you need to add the routing table after this message
#define ROUTING_UPDATE_MSG { 0x55, 0x50, 0x44} //UPD



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
#define ROUTING_RESPONSE_MSG {0x52, 0x53, 0x50, 0xFF} //RSP<mip address>

struct routing_update_msg{
    uint8_t                 host_mip_addr;
    uint8_t                 TTL;
    uint8_t                 U;
    uint8_t                 P;
    uint8_t                 D;
    struct  forward_table   ft;
} __attribute__((packed));

// FSM States
typedef enum {
	INIT,
	CONNECTED,
	DISCONNECTED
} node_state_t;

// Neighbor structure
typedef struct {
	uint8_t mac_addr[6];      // MAC address of neighbor
	time_t last_hello_time;   // Last time hello was received
	int missed_hellos;        // Count of missed hellos
	node_state_t state;       // Current state of this neighbor
} neighbor_t;

#endif //routing_h
