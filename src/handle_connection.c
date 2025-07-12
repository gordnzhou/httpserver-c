#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#define RECV_BUFSIZE 4096

#define ROOT_DIR "root"
#define HTTP_VER "1.0"

#define BAD_REQ_RESP "HTTP 1.0 400 Bad Request\r\nContent-Type: application/json\r\nContent-Length: 77\r\n\r\n{'error':'Bad request','message':'Request body could not be read properly.',}"
#define UNIMPL_RESP "HTTP 1.0 501 Not Implemented"
#define NOT_FOUND_RESP "HTTP 1.0 404 Not Found\r\nContent-Type: application/json\r\nContent-Length: 77\r\n\r\n{'error':'Not Found','message':'Unable to find the file at the given path.',}"
#define FORBIDDEN_RESP "HTTP 1.0 403 Forbidden"

struct httprequest {
    char *method;
    char *target;
    char *protocol_ver;
    char *body;
    int  is_valid;  // 0 if request is improperly formatted
};

void httprequest(struct httprequest *req, char *buf) 
{   
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

    char *token;
    while (strcmp(token = strsep(&buf_p, "\n"), "\r") != 0) {
        // ignore other headers for now
    }

    req->body = strdup(strsep(&buf_p, "\n"));

    if (strcmp(req->target, "/") == 0)
        strcpy(req->target, "/index.html");
}

void freehttprequest(struct httprequest *req) 
{
    free(req->method);
    free(req->target);
    free(req->protocol_ver);
    free(req->body);
}

ssize_t get_file_size(int fd) 
{
    ssize_t size = lseek(fd, 0, SEEK_END);

    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("lseek reset");
        return -1;
    }

    return size;
}

void parse_content_type(char **content_type, char* target) 
{   
    *content_type = "text/plain"; 
    char *file_ext = strrchr(target, '.');
    
    // TODO: recognize more file types
    if (strcmp(file_ext, ".html") == 0)
        *content_type = "text/html";
    if (strcmp(file_ext, ".png") == 0)
        *content_type = "image/png";
    if (strcmp(file_ext, ".jpg") == 0)
        *content_type = "image/jpg";
    if (strcmp(file_ext, ".ico") == 0)
        *content_type = "image/ico";
}

int is_file_forbidden(int fd) 
{
    struct stat fileStat;
    if (fstat(fd, &fileStat) < 0)
        return 1;

    if ((S_IRUSR & fileStat.st_mode) == 0){
        return 1;
    }
    return 0;
}

int httpresponse(void **resp, size_t *resp_size, struct httprequest *req) 
{
    if (!req->is_valid) {
        *resp_size = (size_t) strlen(BAD_REQ_RESP);
        *resp = strdup(BAD_REQ_RESP);
        return 0;
    }

    if (strcmp(req->method, "GET") != 0) {
        *resp_size = (size_t) strlen(UNIMPL_RESP);
        *resp = strdup(UNIMPL_RESP);
        return 0;
    }

    char resp_head[4096] = "HTTP: ", length_str[20], *content_type;
    int fd;
    ssize_t file_bytes, read_bytes;
    unsigned char *file_buf;

    parse_content_type(&content_type, req->target);

    // TODO: prevent file access outside of ROOT_DIR (no "..")
    char *file_path = malloc(strlen(ROOT_DIR) + strlen(req->target) + 1);
    sprintf(file_path, "%s%s", ROOT_DIR, req->target);

    if ((fd = open(file_path, O_RDONLY)) == -1) {
        free(file_path);
        perror("file open");
        *resp_size = (size_t) strlen(NOT_FOUND_RESP);
        *resp = strdup(NOT_FOUND_RESP);
        return 0;
    }

    free(file_path);
    
    if (is_file_forbidden(fd) || (strstr(req->target, "..") != NULL)) {
        *resp_size = (size_t) strlen(FORBIDDEN_RESP);
        *resp = strdup(FORBIDDEN_RESP);
        close(fd);
        return 0;
    }

    if ((file_bytes = get_file_size(fd)) == -1) {
        perror("file size");
        close(fd);
        return -1;
    }

    sprintf(length_str, "%d", (int) file_bytes);
    file_buf = malloc(file_bytes);

    if (!file_buf) { // file could be way too large
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

    strcat(resp_head, HTTP_VER);
    strcat(resp_head, " 200 OK\r\n");

    strcat(resp_head, "Content-type: ");
    strcat(resp_head, content_type);
    strcat(resp_head, "\r\n");

    strcat(resp_head, "Content-length: ");
    strcat(resp_head, length_str);
    strcat(resp_head, "\r\n\r\n");

    size_t headers_length = strlen(resp_head);

    *resp_size = (size_t) (headers_length + file_bytes);
    *resp = malloc(headers_length + file_bytes);

    memcpy(*resp, resp_head, headers_length);
    memcpy(*resp + headers_length, file_buf, file_bytes);

    free(file_buf); 

    return 0;
}

int handle_connection(int sockfd) 
{
    char buf[RECV_BUFSIZE];
    int buf_size;
    struct httprequest req;
    void *resp;
    size_t resp_size;

    if ((buf_size = recv(sockfd, buf, sizeof buf, 0)) == -1) {
        perror("recv");
        return -1;
    }

    if (buf_size == 0)
        return -1; // client closed the connection
        
    buf[buf_size] = '\0';

    memset(&req, 0, sizeof req);
    httprequest(&req, buf);
    printf("METHOD: %s, TARGET: %s, PROTOCOL VER: %s\n", req.method, req.target, req.protocol_ver);

    if (httpresponse(&resp, &resp_size, &req) == -1) {
        free(resp);
        freehttprequest(&req);
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
