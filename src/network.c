#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <errno.h>

#define BACKLOG 10

int setup_sockfd(struct addrinfo *servinfo) 
{
    int sockfd = -1;
    struct addrinfo *p;

    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("server: socket");
            continue;
        }

        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
            perror("setsockopt");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("server: bind");
            continue;
        }

        break; // bind to the first avaliable socket 
    }

    if (p == NULL) {
        fprintf(stderr, "unable to bind");
        return -1;
    }

    if (listen(sockfd, BACKLOG) == 1) {
        perror("listen");
        return -1;
    }

    return sockfd;
}

void *get_in_addr(struct sockaddr *sa) 
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void printalladdr(struct addrinfo *servinfo) 
{
    printf("IP addresses:\n");

    void *addr;
    char *ip_ver;
    char ip_str[INET6_ADDRSTRLEN];
    struct sockaddr_in *ipv4;
    struct sockaddr_in6 *ipv6;

    for (struct addrinfo *p = servinfo; p; p = p->ai_next) {

        if (p->ai_family == AF_INET) {
            ipv4 = (struct sockaddr_in*)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ip_ver = "IPv4";
        } else {
            ipv6 = (struct sockaddr_in6*)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ip_ver = "IPv6";
        }

        inet_ntop(p->ai_family, addr, ip_str, sizeof ip_str);
        printf("    %s: %s\n", ip_ver, ip_str);
    }
}