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

//amount of neightbors the neighbor list will hold
//in this scenario there is 5 nodes, 
//max number of connections for home-exam topology is 3
#define MAX_NEIGHBOURS 5

//time interval between sending hello/update messages
#define TIMER_INTERVAL 5 

//how many seconds can go since last hello before node is disconnected
#define DISCONNECTION_TIME_LIMIT 5 * TIMER_INTERVAL 
#define EPOLL_MAX_EVENTS 10

//Data structures local to only routing_deamon

// FSM States
typedef enum {
	INIT,
	CONNECTED,
	DISCONNECTED
} node_state_t;

// Neighbor structure
typedef struct {
	uint8_t mip_addr;        //mip address of neighbor
	time_t last_hello_time;   // Last time hello was received
	int missed_hellos;        // Count of missed hellos
	node_state_t state;       // Current state of this neighbor
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

void send_update_message(int socket_unix, struct route_table *r_t, uint8_t dst_mip){
            debugprint("=Sending UPDATE message=======================");
            struct unix_sock_sdu message;
            memset(&message, 0, sizeof(message));

            //first three are UPD
            uint8_t msg[] = ROUTING_UPDATE_MSG; 
            memcpy(message.payload, msg, sizeof(msg));

            message.mip_addr = dst_mip;

            int buffer_size = MAX_ROUTES * 3;
            uint8_t buffer[buffer_size];
            memset(&buffer, 0, buffer_size);

            int count = 0;

            for(int i = 0; i < r_t->count; i++){
                struct route r = r_t->routes[i];

                //split horizon: we should not send routes which go through the node we are sending to
                if(r.dst == dst_mip){
                    continue;
                }

                int offset = 1 + 3*i;
                buffer[offset+0] = r.dst;
                buffer[offset+1] = r.next_hop;
                buffer[offset+2] = r.cost;
                count++;
            }

            //there werent any routes to update
            if(count == 0){
                debugprint("=======================================done=\n");
                return;
            }

            buffer[0] = count;

            memcpy(message.payload+3, buffer, buffer_size);

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
    printf("|mip\t|seconds\t|missed \t|state\t|\n");
    for(int i = 0; i < n_t->count; i++){
        neighbour_t curr_n = n_t->neighbours[i]; //current neighbour
        printf("|%d\t|%lo\t\t|%d\t\t|%s\t|\n", 
            curr_n.mip_addr,
            time(NULL) - curr_n.last_hello_time,
            curr_n.missed_hellos,
            (curr_n.state == CONNECTED) ? "CONNECTED" :
                   (curr_n.state == INIT) ? "INIT" : 
                   "DISCONNECTED"
        );
    }
    printf("\n\n");
}

void check_neighbors(struct neighbour_table *n_table){
    for(int i = 0; i < n_table->count ; i++){
        neighbour_t curr_neighbour = n_table->neighbours[i];

        if(curr_neighbour.state == DISCONNECTED){
            continue;
        }

        long time_diff = time(NULL) - n_table->neighbours[i].last_hello_time;

        if(time_diff > DISCONNECTION_TIME_LIMIT){
            debugprint("neighbor: %d has not said hello in %lo seconds", curr_neighbour.mip_addr, time_diff);
            n_table->neighbours[i].state = DISCONNECTED;
        }
        //TODO: update routing table with infinity for all the 
    }   


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
            //  check the time diff for the neighbor table if some node is DISCONNECTED

            //read the timer file descriptor to remove the event from epoll
            ssize_t s = read(timerfd, &missed, sizeof(uint64_t));
            if(s != sizeof(uint64_t)){
                perror("read");
                exit(EXIT_FAILURE);
            }

            //print current state
            print_neighbour_table(n_table);
            print_routing_table(r_table);

            check_neighbors(n_table);

            send_hello_message(socket_unix);

            //we only send the update messages if there has been any changes to the table
            if(update_table_changed != 0){
                send_update_messages(socket_unix, r_table, n_table);
                update_table_changed = 0;
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

                //check neighbour list
                int index = find_neighbour_by_addr(n_table, recv_sdu.mip_addr);

                //if not in neighbour list -> add, in state INIT
                if(index == -1){
                    debugprint("%d was not found in n_table, adding it now", recv_sdu.mip_addr);
                    index = neighbour_table_add_entry(n_table, recv_sdu.mip_addr);
                    routing_table_add_entry(r_table, recv_sdu.mip_addr, recv_sdu.mip_addr, 1);
                    update_table_changed = 1;
                }else if(n_table->neighbours[index].state == INIT){
                //if in list && state == INIT -> set state to CONNECTED
                    n_table->neighbours[index].state = CONNECTED;
                }else if(n_table->neighbours[index].state == DISCONNECTED){
                //if in list && state == DISCONNECTED -> set state to CONNECTED
                    n_table->neighbours[index].state = CONNECTED;
                }

                n_table->neighbours[index].last_hello_time = time(NULL);
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
