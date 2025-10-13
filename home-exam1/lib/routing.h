#ifndef routing_h
#define routing_h

#include <stdint.h>
#include <sys/time.h>

// A HELLO message for discovering neighbouring nodes.
// An UPDATE message to advertise the routes in your routing table.

//   A REQUEST message for intra-host route lookups.
//   <MIP address of the host itself (8 bits)> 
//   <zero (0) TTL (8 bits)> 
//   <R (0x52, 8 bits)> 
//   <E (0x45, 8 bits)> 
//   <Q (0x51, 8 bits)> 
//   <MIP address to look up (8 bits)>
struct routing_request_msg {
    uint8_t     host_mip_addr;
    uint8_t     TTL; //Time To Live
    uint8_t     R; //0x52
    uint8_t     E;
    uint8_t     Q;
    uint8_t     lookup_mip_addr;
} __attribute__((packed));

//   A RESPONSE message for intra-host route responses. This message is fully specified in the next section.
//   <MIP address of the host itself (8 bits)> 
//   <zero (0) TTL (8 bits)> 
//   <R (0x52, 8 bits)> 
//   <S (0x53, 8 bits)> 
//   <P (0x50, 8 bits)> 
//   <next hop MIP address (8 bits)>
struct routing_response_msg{
    uint8_t     host_mip_addr;
    uint8_t     TTL; //Time To Live
    uint8_t     R; //0x52
    uint8_t     S; //0x53
    uint8_t     P; //0x50
    uint8_t     next_hop_mip_addr;
} __attribute__((packed));

struct routing_hello_msg{
    uint8_t     host_mip_addr;
    uint8_t     TTL;
    uint8_t     H;
    uint8_t     E;
    uint8_t     L;
} __attribute__((packed));

struct forward_table {
};

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
