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
#include <fcntl.h> // for file sys calls
#include <assert.h>

#define PORT "8008" 
#define BACKLOG 10
#define RECV_BUFSIZE 4096

#define ROOT_DIR "root"
#define HTTP_VER "1.0"

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
    int  is_valid;  // 0 if request is improperly formatted
};

void httprequest(struct httprequest *req, char *buf, int len) {
    char *method, *target, *protocol_ver, *buf_p = buf;
    req->is_valid = 1;

    if ((method = strsep(&buf_p, " ")) == NULL) 
        req->is_valid = 0;
    if ((target = strsep(&buf_p, " ")) == NULL) 
        req->is_valid = 0;
    if ((protocol_ver = strsep(&buf_p, "\n")) == NULL) 
        req->is_valid = 0;

    if (!req->is_valid)
        return;

    req->method = strdup(method);
    req->target = strdup(target);
    req->protocol_ver = strdup(protocol_ver);

    printf("METHOD: %s, TARGET: %s, PROTOCOL VER: %s\n", req->method, req->target, req->protocol_ver);

    // strsep delimiters are only single byte, so we can't split by "\r\n"
    char *token;
    while (strcmp(token = strsep(&buf_p, "\n"), "\r") != 0) {
        char *key = strsep(&token, ":");
        char *value = token + 1;

        // printf(" HEADER key: %s, value: %s\n", key, value);
    }

    req->body = strdup(strsep(&buf_p, "\n"));

    // printf(" Body: %s\n", req->body);
}

void freehttprequest(struct httprequest *req) {
    free(req->method);
    free(req->target);
    free(req->protocol_ver);
    free(req->body);
}

ssize_t file_size(int fd) {
    ssize_t size = lseek(fd, 0, SEEK_END);

    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("lseek reset");
        return -1;
    }

    return size;
}

int httpresponse(void **resp, size_t *resp_size, struct httprequest *req) {
    char *bad_resp = "HTTP 1.0 400 Bad Request\r\nContent-Type: application/json\r\nContent-Length: 77\r\n\r\n{'error':'Bad request','message':'Request body could not be read properly.',}";
    char *unimpl_resp = "HTTP 1.0 501 Not Implemented";
    char *not_found_resp = "HTTP 1.0 404 Not Found";

    if (!req->is_valid) {
        *resp_size = (size_t) strlen(bad_resp);
        *resp = strdup(bad_resp);
        return 0;
    }

    if (strcmp(req->method, "GET") != 0) {
        *resp_size = (size_t) strlen(unimpl_resp);
        *resp = strdup(unimpl_resp);
        return 0;
    }

    if (strcmp(req->target, "/") == 0)
        strcpy(req->target, "/index.html");

    char file_path[2048];
    int fd;
    ssize_t file_bytes, read_bytes;
    unsigned char *file_buf;

    // TODO: prevent file access outside of ROOT_DIR (no "..")
    sprintf(file_path, "%s%s", ROOT_DIR, req->target);

    if ((fd = open(file_path, O_RDONLY)) == -1) {
        perror("file open");
        *resp_size = (size_t) strlen(not_found_resp);
        *resp = strdup(not_found_resp);
        return 0;
    }

    if ((file_bytes = file_size(fd)) == -1) {
        perror("file size");
        close(fd);
        return -1;
    }

    file_buf = malloc(file_bytes);
    if (!file_buf) {
        perror("malloc");
        close(fd);
        return -1;
    }

    if ((read_bytes = read(fd, file_buf, file_bytes)) == -1 || file_bytes != read_bytes) {
        perror("file read");
        free(file_buf);
        close(fd);
        return -1;
    };

    close(fd);

    char *content_type = "text/plain"; 
    char *file_ext = strrchr(file_path, '.');
    char length_str[20];
    sprintf(length_str, "%d", (int) file_bytes);
    
    if (strcmp(file_ext, ".html") == 0)
        content_type = "text/html";
    if (strcmp(file_ext, ".png") == 0)
        content_type = "image/png";
    if (strcmp(file_ext, ".jpg") == 0)
        content_type = "image/jpg";
    if (strcmp(file_ext, ".ico") == 0)
        content_type = "image/ico";

    char headers[4096];

    strcat(headers, "HTTP ");
    strcat(headers, HTTP_VER);
    strcat(headers, " 200 OK\r\n");

    strcat(headers, "Content-type: ");
    strcat(headers, content_type);
    strcat(headers, "\r\n");

    strcat(headers, "Content-length: ");
    strcat(headers, length_str);
    strcat(headers, "\r\n\r\n");

    size_t headers_length = strlen(headers);

    *resp_size = (size_t) (headers_length + file_bytes);
    *resp = malloc(headers_length + file_bytes);

    memcpy(*resp, headers, headers_length);
    memcpy(*resp + headers_length, file_buf, file_bytes);

    free(file_buf); 

    return 0;
}

int handle_client(int sockfd) {
    char buf[RECV_BUFSIZE];
    int buf_size;
    struct httprequest req;

    if ((buf_size = recv(sockfd, buf, sizeof buf, 0)) == -1) {
        perror("recv");
        return -1;
    }

    if (buf_size == 0) 
        // client closed the connection
        return -1;
        
    buf[buf_size] = '\0';

    memset(&req, 0, sizeof req);

    httprequest(&req, buf, buf_size);

    void *resp;
    size_t resp_size;
    if (httpresponse(&resp, &resp_size, &req) == -1) {
        free(resp);
        freehttprequest(&req);
        perror("httpresponse");
        return -1;
    }

    if (send(sockfd, resp, resp_size, 0) == -1) {
        free(resp);
        freehttprequest(&req);
        perror("send");
        return -1;
    }

    free(resp);
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