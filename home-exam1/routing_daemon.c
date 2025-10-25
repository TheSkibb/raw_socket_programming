#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#include "lib/utils.h"
#include "lib/sockets.h"
#include "lib/utils.h"
#include "lib/routing.h"

//Constants

//amount of neightbors the neighbour list will hold
//in this scenario there is 5 nodes, 
//max number of connections for home-exam topology is 3
#define MAX_NEIGHBOURS 5

//time interval between sending hello/update messages
#define TIMER_INTERVAL 3

//number of hellos to send before sending an update message
#define NO_UPDATE_COUNT_LIMIT 2

//how many seconds can go since last hello before node is disconnected
#define DISCONNECTION_TIME_LIMIT 3 * TIMER_INTERVAL 
#define EPOLL_MAX_EVENTS 10

//Data structures local to only routing_deamon

// FSM States
typedef enum {
	INIT,
	CONNECTED,
	DISCONNECTED
} node_state_t;

// Neighbour structure
typedef struct {
	uint8_t mip_addr;        //mip address of neighbour
	time_t last_hello_time;   // Last time hello was received
	int missed_hellos;        // Count of missed hellos
	node_state_t state;       // Current state of this neighbour
} neighbour_t;

//data structure for storing neighbours
struct neighbour_table{
    //list (array) of neighbours
    neighbour_t neighbours[MAX_NEIGHBOURS];
    //how many neighbours are stored in the table
    //increases automatically when neighbour_table_add_entry is called
    int count;
}__attribute__((packed));

//finds a matching entry to the mip address in the neighbour_table
//returns index of matching entry, or -1 if no match was found
int find_neighbour_by_addr(
    struct neighbour_table *n_t, 
    uint8_t mip_addr
){
    for(int i = 0; i < MAX_NEIGHBOURS; i++){
        if(n_t->neighbours[i].mip_addr == mip_addr){
            return i;
        }
    }

    return -1;
}

//add an entry to the entry table
//returns the index of the added entry, or -1 if there was no available slots
int neighbour_table_add_entry(
        struct neighbour_table *n_t, 
        uint8_t mip_addr
){
    int i = n_t->count;

    //table is full
    if(i == MAX_NEIGHBOURS - 1){
        return -1;
    }

    n_t->neighbours[i].mip_addr = mip_addr;
    n_t->neighbours[i].last_hello_time = time(NULL);
    n_t->neighbours[i].state = INIT;
    n_t->neighbours[i].missed_hellos = 0;

    n_t->count++;

    return i;
}

int routing_table_add_entry(
    struct route_table *r_t,
    uint8_t dst_mip,
    uint8_t next_hop,
    uint8_t cost
){

    debugprint("adding route for node %d", dst_mip);
    int i = r_t->count;
    
    //routing table is full
    if(i == MAX_ROUTES -1){
        return -1;
    }

    r_t->routes[i].dst = dst_mip;
    r_t->routes[i].next_hop = next_hop;
    r_t->routes[i].cost = cost;

    r_t->count++;

    return i;
}

int routing_table_get_route_by_dst(struct route_table *r_t, uint8_t dst){

    for(int i = 0; i < r_t->count; i++){
        if(r_t->routes[i].dst == dst){
            return i;
        }
    }

    return -1;
}

void print_usage(){
    printf("./routing_daemon <socket_lower>\n");
}

void send_hello_message(int socket_unix){
            debugprint("=Sending hello message========================");
            struct unix_sock_sdu message;
            memset(&message, 0, sizeof(message));

            uint8_t msg[] = ROUTING_HELLO_MSG; //HEL
            memcpy(message.payload, msg, sizeof(msg));

            message.mip_addr = 0x00;

            if(send(socket_unix, &message, sizeof(message), 0) == -1){
                perror("send");
                exit(EXIT_FAILURE);
            }

            debugprint("=======================================done=\n");
}

//send an update message:
//this function sends a subset of the host routing table
//(see split horizon)
void send_update_message(
    int socket_unix, 
    //route_table of host (a subset of this will be send)
    struct route_table *r_t, 
    //destination node to send the update message to
    uint8_t dst_mip
){
            debugprint("=Sending UPDATE message=======================");
            struct unix_sock_sdu message;
            memset(&message, 0, sizeof(message));

            //first three bytes of sdu are "UPD"
            uint8_t msg[] = ROUTING_UPDATE_MSG; 
            memcpy(message.payload, msg, sizeof(msg));

            message.mip_addr = dst_mip;

            struct route_table send_r_t;
            memset(&send_r_t, 0, sizeof(struct route_table));

            //check what routes we should send
            for(int i = 0; i < r_t->count; i++){
                struct route r = r_t->routes[i];

                //split horizon: we should not send routes which go through or to the node we are sending to
                if(r.next_hop == dst_mip || r.dst == dst_mip){
                    continue;
                }

                send_r_t.routes[send_r_t.count].dst = r_t->routes[i].dst;
                send_r_t.routes[send_r_t.count].next_hop = r_t->routes[i].next_hop;
                send_r_t.routes[send_r_t.count].cost = r_t->routes[i].cost;

                send_r_t.count++;
            }

            //there werent any routes to update
            if(send_r_t.count == 0){
                debugprint("there were no routes to send");
                debugprint("=======================================done=\n");
                return;
            }

            //routing table to send
            print_routing_table(&send_r_t);

            //put the route_table in the payload
            //(offset by 3 to not overwrite the sdu type
            memcpy(message.payload+3, (void *)&send_r_t, sizeof(struct route_table));

            if(send(socket_unix, &message, sizeof(message), 0) == -1){
                perror("send");
                exit(EXIT_FAILURE);
            }

            debugprint("=======================================done=\n");
}

void send_update_messages(
    int socket_unix, 
    struct route_table *r_t, 
    struct neighbour_table *n_t
){
    //send an update message to each neighbour
    for(int i = 0; i < n_t->count; i++){
        send_update_message(socket_unix, r_t, n_t->neighbours[i].mip_addr);
    }
}

void print_neighbour_table(struct neighbour_table *n_t){
    if(get_debug() == 0){
        return;
    }
    printf("neighbours: \n");
    printf("|mip\t|seconds\t|state\t|\n");
    for(int i = 0; i < n_t->count; i++){
        neighbour_t curr_n = n_t->neighbours[i]; //current neighbour
        printf("|%d\t|%lo\t\t|%s\t|\n", 
            curr_n.mip_addr,
            time(NULL) - curr_n.last_hello_time,
            (curr_n.state == CONNECTED) ? "CONNECTED" :
                   (curr_n.state == INIT) ? "INIT" : 
                   "DISCONNECTED"
        );
    }
    printf("\n\n");
}

void poison_reverse(struct route_table *r_table, uint8_t dst){
    for(int i = 0; i < r_table->count; i++){
        if(r_table->routes[i].next_hop == dst){
            r_table->routes[i].cost = ROUTING_COST_INFINITY;
        }
    }
}

void check_neighbours(struct neighbour_table *n_table, struct route_table *r_table, int *update_table_changed){
    for(int i = 0; i < n_table->count ; i++){
        neighbour_t curr_neighbour = n_table->neighbours[i];

        if(curr_neighbour.state == DISCONNECTED){
            continue;
        }

        long time_diff = time(NULL) - n_table->neighbours[i].last_hello_time;

        if(time_diff > DISCONNECTION_TIME_LIMIT){
            debugprint("neighbour: %d has not said hello in %lo seconds", curr_neighbour.mip_addr, time_diff);
            n_table->neighbours[i].state = DISCONNECTED;
            poison_reverse(r_table, n_table->neighbours[i].mip_addr);
            *update_table_changed = 0;
        }
    }   
}

// 
int handle_hello_message(
        //this nodes list of neighbours
        struct neighbour_table *n_table,
        //this nodes list of routes
        struct route_table *r_table,
        //hello packet from unix socket
        struct unix_sock_sdu *recv_sdu,
        //pointer to changed_variable to signify if the routing table was changed
        int *update_table_changed
){
    //check neighbour list
    int index = find_neighbour_by_addr(n_table, recv_sdu->mip_addr);

    //if not in neighbour list -> add to neightbor list in state INIT, add to route_table
    if(index == -1){

        debugprint("%d was not found in n_table, adding it now", recv_sdu->mip_addr);
        index = neighbour_table_add_entry(n_table, recv_sdu->mip_addr);
        routing_table_add_entry(r_table, recv_sdu->mip_addr, recv_sdu->mip_addr, 1);

        //this will make the routing daemon send out a update message
        *update_table_changed = 1;

    //if in list and state == INIT -> set to CONNECTED
    }else if(n_table->neighbours[index].state == INIT){
        n_table->neighbours[index].state = CONNECTED;

    //if in list && state == DISCONNECTED -> set state to CONNECTED
    }else if(n_table->neighbours[index].state == DISCONNECTED){
        n_table->neighbours[index].state = CONNECTED;
        
        //in our example we know that if a neighbour reconnects, there is already a route to this node
        int route_index = routing_table_get_route_by_dst(r_table, n_table->neighbours[index].mip_addr);
        if(route_index == -1){
            printf("nonexistent route has been reconnected");
            exit(EXIT_FAILURE);
        }

        //set route back to using the neighbour to stop possible redirection and poison_reverse infinity cost
        r_table->routes[route_index].cost = 1;
        r_table->routes[route_index].next_hop = n_table->neighbours[index].mip_addr;

    }

    n_table->neighbours[index].last_hello_time = time(NULL);

    return 0;
}

//compares and updates the host_routing_table if:
//  the new_routing_table contains a new route
//  the cost of an existing route has changed either up or down
//  new_routing_table contains route to a known dst with a different next_hop (if this route is cheaper)
int compare_routing_tables(
    //route_table of this node
    struct route_table *host_routing_table,
    //route_table we have received
    struct route_table *new_routing_table,
    //mip address of the node we received the table from
    uint8_t remote_node
){

    debugprint("comparing routing tables...");

    debugprint("host_routing_table has %d routes", host_routing_table->count);
    debugprint("new_routing_table has %d routes", new_routing_table->count);

    for(int i = 0; i < new_routing_table->count; i++){
        debugprint("incoming route %d: { %d %d %d}", i,
                new_routing_table->routes[i].dst,
                new_routing_table->routes[i].next_hop,
                new_routing_table->routes[i].cost);
        int is_new_route = 1;

        int j = 0;

        //check if there is a route on this nodes routing table which has the same destination
        for(j = 0; j < host_routing_table->count; j++){
            debugprint("->comparing with host route %d: { %d %d %d}", j,
                    host_routing_table->routes[j].dst,
                    host_routing_table->routes[j].next_hop,
                    host_routing_table->routes[j].cost);
            if(new_routing_table->routes[i].dst == host_routing_table->routes[j].dst){
                is_new_route = 0;
                break;
            }
        }

        if(is_new_route){
            //new routes should be added to the routing table
            debugprint("this is a NEW route!");
            routing_table_add_entry(
                host_routing_table,
                new_routing_table->routes[i].dst,
                remote_node,
                new_routing_table->routes[i].cost+1
            );
        }else{
            debugprint("this is NOT a new destination!");

            //uint8_t host_dst = host_routing_table->routes[j].dst;
            uint8_t host_nxt_hop = host_routing_table->routes[j].next_hop;
            uint8_t host_cost = host_routing_table->routes[j].cost;

            //uint8_t recv_dst = new_routing_table->routes[i].dst;
            //uint8_t recv_nxt_hop = new_routing_table->routes[i].next_hop;
            uint8_t recv_cost = new_routing_table->routes[i].cost;
            
            //check if a path we already have has an updated cost
            if(host_nxt_hop == remote_node && host_cost != recv_cost){
                debugprint("updating routing cost");
                if(recv_cost != ROUTING_COST_INFINITY){
                    host_routing_table->routes[j].cost = recv_cost + 1;
                }else{
                    host_routing_table->routes[j].cost = ROUTING_COST_INFINITY;
                }
            }
            //check if you receive a route which is cheaper (with different next hop)
            else if(host_cost > recv_cost+1){
                debugprint("received route is cheaper");
                host_routing_table->routes[j].dst = new_routing_table->routes[i].dst;
                host_routing_table->routes[j].next_hop = remote_node;
                host_routing_table->routes[j].cost = new_routing_table->routes[i].cost + 1;
            }
        }
    }
    return 0;
}

//unpacks a unix_sock_sdu update message.
//compares the contents of the received table, 
//and checks whether to update any routes in the local routing table
//if any routes are changed, 
int handle_update_message(
        //this nodes neighbour table
        struct neighbour_table *n_table,
        //this nodes routing table
        struct route_table *r_table,
        //update sdu
        struct unix_sock_sdu *recv_sdu,
        int *update_table_changed
){
    debugprint("RECEIVED A ROUTING TABLE UPDATE FROM %d:", recv_sdu->mip_addr);

    //unpack the sdu into a new routing table
    int offset = 3;
    struct route_table r_t;
    memset(&r_t, 0, sizeof(struct route_table));
    r_t.count = recv_sdu->payload[offset];
    memcpy(r_t.routes, recv_sdu->payload+offset+1, MAX_ROUTES*3);

    print_routing_table(&r_t);
    compare_routing_tables(r_table, &r_t, recv_sdu->mip_addr);

    return 0;
}

int handle_forwarding_packet(
        //sdu received on unix socket
        struct unix_sock_sdu *sdu,
        //route_table of this node
        struct route_table *r_t,
        int socket_unix
){
    debugprint("=Handling forwarding========================\n");
    for(int i = 0; i < r_t->count; i++){
        if(r_t->routes[i].dst == sdu->payload[4]){
            debugprint("found a matching route");
            sdu->payload[4] = r_t->routes[i].next_hop;
            sdu->TTL = sdu->TTL - 1;
            break;
        }
    }

    //send sdu back on unix socket
    int rc = write(socket_unix, sdu, 0);

    if(rc == -1){
        perror("sendmsg");
        exit(EXIT_FAILURE);
    }

    debugprint("=======================================done=\n");

    return 0;
}

void routing_daemon(
        int epollfd,
        struct epoll_event events[EPOLL_MAX_EVENTS],
        int timerfd,
        struct neighbour_table *n_table,
        struct route_table *r_table,
        int socket_unix
){
    int rc;
    int update_table_changed = 0;
    int no_new_update_count;
    uint64_t missed;
    while(1){
        rc = epoll_wait(epollfd, events, EPOLL_MAX_EVENTS, -1);
        if(rc == -1){
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }else if(events->data.fd == timerfd){
            //every 5 seconds the routing deamon:
            //  checks for changes in the update table
            //  sends a HELLO or UPDATE message (depending on whether or not update table was changed)
            //  check the time diff for the neighbour table if some node is DISCONNECTED

            //read the timer file descriptor to remove the event from epoll
            ssize_t s = read(timerfd, &missed, sizeof(uint64_t));
            if(s != sizeof(uint64_t)){
                perror("read");
                exit(EXIT_FAILURE);
            }

            debugprint("CURRENT STATE");
            print_neighbour_table(n_table);
            print_routing_table(r_table);

            //check if any neighbours have disconnected
            check_neighbours(n_table, r_table, &update_table_changed);

            send_hello_message(socket_unix);

            //we send the update messages if there has been any changes to the table
            //or if there hasnt been any update in some amount of hellos
            if(update_table_changed != 0 || no_new_update_count == NO_UPDATE_COUNT_LIMIT){
                send_update_messages(socket_unix, r_table, n_table);
                update_table_changed = 0;
                no_new_update_count = 0;
            }else{
                no_new_update_count++;
            }


        }else if(events->data.fd == socket_unix){
            debugprint("=Received a unix message======================");
            struct unix_sock_sdu recv_sdu;
            memset(&recv_sdu, 0, sizeof(struct unix_sock_sdu));

            int r = read(socket_unix, (void *)&recv_sdu, sizeof(struct unix_sock_sdu));
            if (r == -1) {
               perror("read");
               exit(EXIT_FAILURE);
            }
            if (r == 0){
                debugprint("connection closed");
                exit(EXIT_SUCCESS);
            }

            if(strncmp(recv_sdu.payload, "HEL", 3) == 0){
                debugprint("HELLO message: %s, from %d", recv_sdu.payload, recv_sdu.mip_addr);
                handle_hello_message(
                        n_table,
                        r_table,
                        &recv_sdu,
                        &update_table_changed
                );
            }else if(strncmp(recv_sdu.payload, "UPD", 3) == 0){
                debugprint("UPDATE message from %d", recv_sdu.mip_addr);
                handle_update_message(
                        n_table,
                        r_table,
                        &recv_sdu,
                        &update_table_changed
                );
                
            }else if(strncmp(recv_sdu.payload, "REQ", 3) == 0){
                debugprint("forwarding REQUEST for %d");
                handle_forwarding_packet(
                        &recv_sdu,
                        r_table,
                        socket_unix
                );
            }else{
                debugprint("message: %s, from %d", recv_sdu.payload, recv_sdu.mip_addr);
            }

            debugprint("=======================================done=\n");
        }
    }
}

int main(int argc, char *argv[]){
    set_debug(1);
    debugprint("=checking cmd arguments=====================");
    if(argc <= 1){
        printf("too few arguments");
        print_usage();
        return 1;
    }

    char socket_unix_name[MAX_UNIX_PATH_LENGTH] ="";
    strncpy(socket_unix_name, argv[1], strlen(argv[1]) +1);

    debugprint("unix socket name: %s", socket_unix_name);

    debugprint("=======================================done=\n");

    debugprint("=unix socket setup==========================");

    int socket_unix = create_unix_socket(socket_unix_name, UNIX_SOCKET_MODE_CLIENT, SOCKET_TYPE_ROUTER);

    debugprint("=======================================done=\n");

    debugprint("=setup tables=================================");

    struct neighbour_table n_table;
    memset(&n_table, 0, sizeof(struct neighbour_table));

    struct route_table r_table;
    memset(&r_table, 0, sizeof(struct route_table));

    debugprint("=======================================done=\n");

    debugprint("timer socket setup==========================");

    // Create the timerfd
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd == -1) {
        perror("timerfd_create");
        exit(EXIT_FAILURE);
    }

    struct itimerspec new_value;
    new_value.it_value.tv_sec = TIMER_INTERVAL; // Initial expiration
    new_value.it_value.tv_nsec = 0;
    new_value.it_interval.tv_sec = TIMER_INTERVAL; // Interval
    new_value.it_interval.tv_nsec = 0;

    if (timerfd_settime(timerfd, 0, &new_value, NULL) == -1) {
        perror("timerfd_settime");
        exit(EXIT_FAILURE);
    }

    debugprint("=======================================done=\n");

    debugprint("epoll socket setup==========================");
    
    int epollfd = create_epoll_table();
    add_socket_to_epoll(epollfd, socket_unix, EPOLLIN);
    add_socket_to_epoll(epollfd, timerfd, EPOLLIN);
    struct epoll_event events[EPOLL_MAX_EVENTS];

    debugprint("=======================================done=\n");
    debugprint("=setup done, starting routing daemon========\n\n\n\n\n");

    routing_daemon(
        epollfd,
        events,
        timerfd,
        &n_table,
        &r_table,
        socket_unix
    );

    return 0;

}
