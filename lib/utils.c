#include <stdarg.h>
#include <stdio.h>
#include <sys/ioctl.h>

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

// Check if there is data available to read on the socket
//we check if data is available, because sending a packet triggers the epoll, 
int is_data_available(int sockfd) {
    int bytes_available = 0;
    
    if (ioctl(sockfd, FIONREAD, &bytes_available) < 0) {
        perror("ioctl");
        return -1;
    }

    return bytes_available > 0;
}

