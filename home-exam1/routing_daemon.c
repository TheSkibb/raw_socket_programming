#include <stdio.h>
#include <string.h>

#include "lib/utils.h"
#include "lib/sockets.h"

void print_usage(){
    printf("./routing_daemon <socket_lower>\n");
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

    int socket_unix = create_unix_socket(socket_unix_name, UNIX_SOCKET_MODE_CLIENT);

    debugprint("=======================================done=\n");

}
