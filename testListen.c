#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <errno.h>

#define MAX_EVENTS 10
#define BUFFER_SIZE 2048

int main() {
    // Step 1: Create a raw socket
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Step 2: Prepare epoll
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event event;
    event.events = EPOLLIN; // Listen for input events
    event.data.fd = sockfd; // Use the raw socket file descriptor

    // Step 3: Add the raw socket to epoll
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event) == -1) {
        perror("epoll_ctl");
        close(epfd);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Step 4: Event loop
    struct epoll_event events[MAX_EVENTS];
    char buffer[BUFFER_SIZE];

    while (1) {
        int num_events = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == sockfd) {
                ssize_t packet_size = recv(sockfd, buffer, BUFFER_SIZE, 0);
                if (packet_size < 0) {
                    perror("recv");
                    continue;
                }
                
                // Print received packet notice
                printf("received packet\n");
            }
        }
    }

    // Cleanup
    close(epfd);
    close(sockfd);
    return 0;
}
