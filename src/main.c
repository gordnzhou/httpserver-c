#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <sys/wait.h> // for waitpid()
#include <signal.h> // for sigaction functions
#include <unistd.h> // for fork(), exit() sys calls
#include <errno.h> // errno global var for perror()

#define PORT "8008" 
#define BACKLOG 10

#define BUFSIZE 4096 // size of buffer for recv()

// runs at the end of every child process
void sigchld_handler(int s)
{
    (void)s;
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void printalladdr(struct addrinfo **servinfo) 
{
    printf("IP addresses\n");

    void *addr;
    char *ip_ver;
    char ip_str[INET6_ADDRSTRLEN];
    struct sockaddr_in *ipv4;
    struct sockaddr_in6 *ipv6;

    struct addrinfo *p;
    for (struct addrinfo *p = *servinfo; p; p = p->ai_next) {
        // flexibility for both ipv4 and ipv6
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

void *get_in_addr(struct sockaddr *sa) 
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// ignore other request headers for now
struct httprequest {
    char *method;
    char *target;
    char *protocol_ver;
    char *body;
};

// return -1 if request is not properly formatted
int parse_request(struct httprequest *req, char *buf, int len) {
    char *method, *target, *protocol_ver, *buf_p = buf;

    if ((method = strsep(&buf_p, " ")) == NULL) 
        return -1;
    if ((target = strsep(&buf_p, " ")) == NULL) 
        return -1;
    if ((protocol_ver = strsep(&buf_p, "\n")) == NULL) 
        return -1;

    req->method = strdup(method);
    req->target = strdup(target);
    req->protocol_ver = strdup(protocol_ver);

    printf("METHOD: %s\nTARGET: %s\nPROTOCOL VER: %s\n", req->method, req->target, req->protocol_ver);

    // strsep delimiters are only single byte, so we can't split by "\r\n"
    char *token;
    while (strcmp(token = strsep(&buf_p, "\n"), "\r") != 0)
        printf(" Header: %s\n", token);

    req->body = strdup(strsep(&buf_p, "\n"));

    printf(" Body: %s\n", req->body);

    return 0;
}

void freehttprequest(struct httprequest *req) {
    free(req->method);
    free(req->target);
    free(req->protocol_ver);
    free(req->body);
}

int handle_client(int sockfd) {
    char buf[BUFSIZE], *resp;
    int buf_size;
    struct httprequest req;

    memset(&req, 0, sizeof req);

    if ((buf_size = recv(sockfd, buf, sizeof buf, 0)) == -1) {
        perror("recv");
        return -1;
    }

    if (buf_size == 0) 
        // client closed the connection
        return -1;
        
    buf[buf_size] = '\0';

    // TODO: 200 and 400 response should contain content from a files in /root (400 from special error file)
    char *get_resp = "HTTP 1.0 200 OK\r\nContext-type: text/html\r\nContent-Length: 48\r\n\r\n<html><body><h1>Hello, world!</h1></body></html>";
    char *err_resp = "HTTP 1.0 400 Bad Request\r\nContent-Type: application/json\r\nContent-Length: 77\r\n\r\n{'error':'Bad request','message':'Request body could not be read properly.',}";
    char *unimpl_resp = "HTTP 1.0 501 Not Implemented";

    if (parse_request(&req, buf, buf_size) == -1)
        resp = err_resp;
    else
        resp = strcmp(req.method, "GET") == 0 ? get_resp : unimpl_resp;

    if (send(sockfd, resp, strlen(resp), 0) == -1) {
        perror("send");
        return -1;
    }
    
    freehttprequest(&req);

    return 0;
}

int main(int argc, char *argv[]) 
{
    struct addrinfo hints, *servinfo;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    }

    printalladdr(&servinfo);

    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1) {
        perror("server: socket");
        exit(1);
    }

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
        perror("setsockopt");
        exit(1);
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("server: bind");
        exit(1);
    }

    freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) == 1) {
        perror("listen");
        exit(1);
    }

    // socket for send() and recv() to remote port
    struct sockaddr_storage their_addr;
    socklen_t their_addr_size = sizeof their_addr;

    struct sigaction sa;
    sa.sa_handler = sigchld_handler; 
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while (1) {
        their_addr_size = sizeof their_addr;
        int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &their_addr_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        char s[INET6_ADDRSTRLEN];
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // in child process
            close(sockfd);
            handle_client(new_fd); 
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }

    return 0;
}