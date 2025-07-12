#ifndef NETWORK_H
#define NETWORK_H

void *get_in_addr(struct sockaddr *sa); 

int setup_sockfd(struct addrinfo *servinfo);

void printalladdr(struct addrinfo *servinfo);

#endif