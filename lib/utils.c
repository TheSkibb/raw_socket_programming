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

