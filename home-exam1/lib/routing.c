#include <stdio.h>
#include "utils.h"

#include "routing.h"

//utility for pretty printing the routing table
void print_routing_table(struct route_table *r_t){
    if(get_debug() == 0){
        return;
    }

    printf("routes:\n");
    printf("|dst\t|next_hop\t|cost\t|\n");
    for(int i = 0; i < r_t->count; i++){
        struct route curr_r = r_t->routes[i]; //current route
        printf("|%d\t|%d\t\t|%d\t|\n",
                curr_r.dst,
                curr_r.next_hop,
                curr_r.cost
        );

    }
    printf("\n\n");
}

