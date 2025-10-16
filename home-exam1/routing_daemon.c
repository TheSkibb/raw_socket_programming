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


struct route{
    uint8_t dst; //
    uint8_t next_hop;
    uint8_t cost;
}__attribute__((packed));

//in this scenario we have 5 nodes, so number of nodes will not exceed 5
#define MAX_ROUTES 5

struct route_table{
    struct route routes[MAX_ROUTES];
}__attribute__((packed));

//in this scenario there is 5 nodes, 
//max number of connections for home-exam topology is 3
#define MAX_NEIGHBOURS 5

struct neighbour_table{
    //list (array) of neightbors
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

void send_update_message(int socket_unix){
            debugprint("=Sending hello message========================");
            struct unix_sock_sdu message;
            memset(&message, 0, sizeof(message));

            uint8_t msg[] = ROUTING_UPDATE_MSG; //UPD
            memcpy(message.payload, msg, sizeof(msg));

            message.mip_addr = 0x00;

            if(send(socket_unix, &message, sizeof(message), 0) == -1){
                perror("send");
                exit(EXIT_FAILURE);
            }

            debugprint("=======================================done=\n");
}

void print_tables(struct neighbour_table *n_t, struct route_table *r_t){
    if(get_debug() == 0){
        return;
    }
    printf("neighbours: \n");
    printf("|mip\t|seconds\t|missed \t|state\t|\n");
    for(int i = 0; (i < MAX_NEIGHBOURS && i < MAX_ROUTES); i++){
        if(i < MAX_NEIGHBOURS && i < n_t->count){
            printf("|%d\t|%lo\t\t|%d\t\t|%s\t|\n", 
                n_t->neighbours[i].mip_addr,
                time(NULL) - n_t->neighbours[i].last_hello_time,
                n_t->neighbours[i].missed_hellos,
                (n_t->neighbours[i].state == CONNECTED) ? "CONNECTED" : "DISCONNECTED"
            );
        }
    }
    printf("\n\n");
}

void check_neighbors(struct neighbour_table *n_table){
}

#define EPOLL_MAX_EVENTS 10

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
            print_tables(n_table, r_table);

            check_neighbors(n_table);

            if(update_table_changed == 0){
                send_hello_message(socket_unix);
            }else{
                send_update_message(socket_unix);
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

    int timer_interval = 5; //seconds
    struct itimerspec new_value;
    new_value.it_value.tv_sec = timer_interval; // Initial expiration
    new_value.it_value.tv_nsec = 0;
    new_value.it_interval.tv_sec = timer_interval; // Interval
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
