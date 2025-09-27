#ifndef UTILS_H
#define UTILS_H

/*debug stuff*/

//prints if global debug flag is set
//takes in format string and arguments
//NOTICE: adds \n to end of string
int debugprint(const char *format, ...);
//sets the global debug flag
void set_debug(int mode);
//returns the global debug flag
int get_debug();
//returns a file descriptor for a newly created epoll table
int create_epoll_table();

int add_socket_to_epoll(int epollfd, int socket, int flags);

#endif
