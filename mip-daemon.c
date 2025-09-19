#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/epoll.h>

#include "lib/raw_sockets.h"
#include "lib/interfaces.h"
#include "lib/mip.h"

//TODO: remove send/recv mode, change for handle

int printhelp(){
    printf("usage mipd [-h] [-d] <socket_upper> <MIP address>\n");
    return 0;
}

int epoll_add_sock(int sd)
{
	struct epoll_event ev;

	/* Create epoll table */
	int epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		return 1;
	}

	/* Add RAW socket to epoll table */
	ev.events = EPOLLIN;
	ev.data.fd = sd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sd, &ev) == -1) {
		perror("epoll_ctl: raw_sock");
		return 1;
	}

	return epollfd;
}

int debug = 0;

int debugprint(const char *format, ...) {
    if (debug) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
    return 0;
}

int main(int argc, char *argv[]){
    //TODO: do proper flag checking

    /* determine mode */
    if(argc <= 1){
        printf("you need to supply at least one argument, see usage with -h\n");
        return 1;
    }

    int send = 0;

    char *arg;

    for(int i = 1; i < argc; i++){


        arg = argv[i];
        printf("arg%d: %s\n", i, arg);
        // s = SEND_MODE
        // r = RECEIVE_MODE
        if(strcmp(arg, "-h") == 0){
            printhelp();
            return 0;
        }else if(strcmp(arg, "-d") == 0){
            debug = 1;
            debugprint("you have started the daemon with debug prints");
        }else if(strcmp(arg, "s") == 0){
            debugprint("send mode");
            send = 1;
        }else if(strcmp(arg, "r") == 0){
            debugprint("recvmode");
            send = 0;
        }else{
            debugprint("starting a unix socket with %s", arg);
            //return 1;
        }
    }

    /* raw socket setup */
    debugprint("setting up raw socket");
    int raw_sock = create_raw_socket();

    /* interface setup */
    debugprint("setting up interfaces");
    struct ifs_data interfaces;
    init_ifs(&interfaces, raw_sock);

    //print the MAC addresses
    //A and C should have 1, B should have 2
    for(int i = 0; i < interfaces.ifn; i++){
        print_mac_addr(interfaces.addr[i].sll_addr, 6);
    }

    int rc = 0;

    if(send){
        uint8_t broadcast[] = ETH_BROADCAST;
        debugprint("ready to send broadcast message %s", argv[argc-1]);
        rc = send_mip_packet(&interfaces, interfaces.addr[1].sll_addr, broadcast, 0x01, 0x02, argv[argc-1]);
        if(rc < 0){
            perror("send_mip_packet");
            return 0;
        }
        debugprint("message sent");
    }

    debugprint("ready to receive message");

    int efd;
    int epoll_max_events = 10;
    struct epoll_event events[epoll_max_events];

    efd = epoll_add_sock(raw_sock);


    while(1) {
		rc = epoll_wait(efd, events, epoll_max_events, -1);
		if (rc == -1) {
			perror("epoll_wait");
			return 0;
		} else if (events->data.fd == raw_sock) {
            printf("you received a packet\n");
            rc = handle_mip_packet(&interfaces);
            if(rc <= 0){
                debugprint("rc == %d", rc);
                perror("handle_mip_packet");
                return 1;
            }
		}
	}
	close(raw_sock);


    close(raw_sock);
    return 0; //success
}
