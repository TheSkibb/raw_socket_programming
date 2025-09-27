#include <stdarg.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <stdlib.h>

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

void set_debug(int mode){
    debug = mode;
}

int get_debug(){
    //printf("returning debug %d", debug);
    return debug;
}


int create_epoll_table(){
	/* Create epoll table */
	int epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		return -1;
	}

    return epollfd;
}

int add_socket_to_epoll(int epollfd, int socket, int flags){
    struct epoll_event events;
    events.events = flags;
    events.data.fd = socket;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, socket, &events) == -1){
        exit(EXIT_FAILURE);
    }

    return 0;
}

