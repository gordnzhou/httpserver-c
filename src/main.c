#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <errno.h> 

#include "network.h"
#include "sigchld.h"
#include "handle_connection.h"

#define PORT "8008" 

int main() 
{
    struct addrinfo hints, *servinfo;
    int status, sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    printalladdr(servinfo);

    if ((sockfd = setup_sockfd(servinfo)) == -1) {
        exit(1);
    }

    freeaddrinfo(servinfo);

    if (setup_sigchld() == -1) {
        exit(1);
    }

    // socket for send() and recv() to remote port
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size = sizeof client_addr;
    char s[INET6_ADDRSTRLEN];

    printf("server: waiting for connections...\n");

    while (1) {
        client_addr_size = sizeof client_addr;
        int new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_size);

        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // in child process
            close(sockfd);

            handle_connection(new_fd);

            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }

    return 0;
}