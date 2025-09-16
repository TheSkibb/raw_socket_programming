#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lib/raw_sockets.h"
#include "lib/interfaces.h"

//TODO: remove send/recv mode, change for handle
#define SEND_MODE 1
#define RECEIVE_MODE 2

/*usage mipd [-h] [-d] <socket_upper> <MIP address>*/
int main(int argc, char *argv[]){
    //TODO: do proper flag checking

    /* determine mode */
    int mode = 0;
    if(argc <= 1){
        printf("you need to specify mode sender or receiver\n");
        return 1;
    }

    char *arg1 = argv[1];

    // s = SEND_MODE
    // r = RECEIVE_MODE
    if(strcmp(arg1, "s") == 0){
        printf("you are sender\n");
        mode = SEND_MODE;
    }else if(strcmp(arg1, "r") == 0){
        printf("you are receiver\n");
        mode = RECEIVE_MODE;
    }else{
        printf("invalid mode\n");
        return 1;
    }

    /* raw socket setup */
    printf("setting up raw socket\n");
    int raw_sock = create_raw_socket();
    printf("socket: %d", raw_sock);

    /* interface setup */
    printf("setting up interfaces\n");
    struct ifs_data interfaces;
    init_ifs(&interfaces, raw_sock);

    //print the MAC addresses
    //A and C should have 1, B should have 2
    for(int i = 0; i < interfaces.ifn; i++){
        print_mac_addr(interfaces.addr[i].sll_addr, 6);
    }

    close(raw_sock);
    return 0; //success
}
