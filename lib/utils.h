#ifndef UTILS_H
#define UTILS_H

/*debug stuff*/

//prints if global debug flag is set
//takes in format string and arguments
//NOTICE: adds \n to end of string
int debugprint(const char *format, ...);
//sets the global debug flag 
//1 = debug on
//0 = debug off
void set_debug(int mode);
//returns the global debug flag
int get_debug();
//returns a file descriptor for a newly created epoll table
int create_epoll_table();

//adds a socket to the epoll table
void add_socket_to_epoll(
    //epoll table file descriptor
    int epollfd, 
    //the socket file descriptor which will be added to the table
    int socket, 
    //see man epoll_ctl for what flags can be passed
    int flags
);

#endif
