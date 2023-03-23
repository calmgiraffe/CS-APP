#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_PORT_LEN 6
#define MAX_NUM_HEADERS 1000
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define debug(lineno) printf("called from %d.\n", lineno);

/* You won't lose style points for including this long line in your code */
// static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; 
// Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void handle_request(int fd);
int build_new_request(rio_t *rp, char *newRequest, char *path, char* host);
int parse_uri(char *uri, char *host, char *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
    int listenfd, connfd;
    socklen_t clientLen;
    struct sockaddr_storage clientAddr; // IPv independent sockaddr struct
    char clientHost[MAXLINE], clientPort[MAXLINE];

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(0);
    }
    listenfd = Open_listenfd(argv[1]); // listening fd for incoming requests
    while (1) {
        /* Proxy blocks (waits) until connection request arrives.
        Get the hostname and port. In context of driver, will be localhost
        and ephemeral port that is different from argv[1] */
        clientLen = sizeof(clientAddr);
        connfd = Accept(listenfd, (SA *) &clientAddr, &clientLen);
        Getnameinfo((SA *) &clientAddr, clientLen, clientHost, MAXLINE, 
                clientPort, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", clientHost, clientPort);
        handle_request(connfd);
        Close(connfd);
    }
    return 0;
}

/*
 * doit - handle one HTTP request/response transaction
 */
void handle_request(int fd) {
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], port[6], path[MAXLINE]; // extracted from uri
    char newRequest[MAXLINE]; // new http request to send to server
    char response[MAX_OBJECT_SIZE]; // response from server
    char buf[MAXLINE]; // pointer to char array for rio package
    rio_t rioClient, rioServer;
    int serverfd;

    /* Read request line and headers */
    Rio_readinitb(&rioClient, fd);
    Rio_readlineb(&rioClient, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version); 

    /* a) method: error msg if other than GET */
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented", 
                "Proxy does not implement this method");
        return;
    }
    /* b) uri: break down uri into host, port, & path 
    note: default port is 80, but client can specify a specific port */
    if (parse_uri(uri, host, port, path)) {
        clienterror(fd, method, "400", "Bad Request", "Bad URI");
        return;
    }
    /* c) version: error msg if other than HTTP/1.0 or HTTP/1.1 */
    if (strcasecmp(version, "HTTP/1.0") & strcasecmp(version, "HTTP/1.1")) {
        clienterror(fd, method, "400", "Bad Request", "Bad HTTP version");
        return;
    }
    /* http headers: error if max len exceeded */
    if (build_new_request(&rioClient, newRequest, path, host)) {
        clienterror(fd, method, "400", "Bad Request", "Request size exceeded");
        return;
    }
    /* connect proxy to web server and send new HTTP request */
    serverfd = Open_clientfd(host, port);
    Rio_readinitb(&rioServer, serverfd);
    Rio_writen(serverfd, newRequest, strlen(newRequest));

    /* get response and send to client */
    int len = Rio_readnb(&rioServer, response, sizeof(response));
    printf("Read %d bytes from server\n", len);
    Rio_writen(fd, response, sizeof(response));

    close(serverfd);
}

/*
 * Read HTTP headers, if any, and stores them in hdrs. Stored in continuous
 * char array with \r\n to indicate end of header. Returns -1 if header len
 * exceed 8K, 0 on success.
 */
int build_new_request(rio_t *rp, char *newRequest, char *path, char* host) {

    sprintf(newRequest, "GET %s HTTP/1.0\r\n", path);
    strcat(newRequest, "Host: ");
    strcat(newRequest, host);
    strcat(newRequest, "\r\n");
    strcat(newRequest, "Connection: close\r\n");
    strcat(newRequest, "Proxy-Connection: close\r\n");

    char buf[MAXLINE] = "";
    int totalLen = strlen(newRequest);
    int len;

    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE); // terminates '\n' with '\0'
        len = strlen(buf);               // len of header txt + "\r\n"
        totalLen += len;
        debug(__LINE__);

        if (totalLen >= MAXLINE) {
            // raise error bc buffer space exceeded
            newRequest[MAXLINE-1] = '\0';
            return -1;
        }
        strncat(newRequest, buf, len);   // appends \0
    }
    return 0;
}

/*
 * Break the URI down into host, port, and path fields. The pointers of these
 * fields are passed in. url ends with \0 since it's an output of sscanf
 * 
 * Example URIS:
 * http://example.com:8080/path/to/resource?query=string#fragment
 * http://example.com/path/to/resource?query=string#fragment
 */
int parse_uri(char *uri, char *host, char *port, char *path) {
    /* Check that the first 7 chars of uri are "http://".
    Return with error code if not. */
    char protocol[8];
    strncpy(protocol, uri, 7);
    protocol[7] = '\0';
    if (strcmp(protocol, "http://")) { // TODO: HTTPS functionality
        return -1;
    }
    /* Get a pointer to the first instance of '/' within uri, starting from the
    substring after "http://". Every request must have a path. */
    char* pathPtr;
    if ((pathPtr = strstr(uri + 7, "/")) == NULL) {
        return -1;
    }
    strcpy(path, pathPtr);

    /* Get the substr from uri+7 to path, which is the host. If curr becomes ":"
    before hostPtr + i == path, use the requested port that lies between ':'
    and '/' instead of the default 80. */
    char *hostPtr = uri + 7, *portPtr = NULL, curr;
    int i;
    for (i = 0; hostPtr + i != pathPtr; i++) {
        curr = hostPtr[i];
        if (curr == ':') {
            portPtr = hostPtr + i + 1;
            break;
        }
        host[i] = curr;
    }
    host[i] = '\0';

    if (portPtr == NULL) { 
        strcpy(port, "80");
    } else {
        /* Get the substring from portPtr to path */
        for (i = 0; portPtr + i != pathPtr; i++) {
            port[i] = portPtr[i];
        }
        port[i] = '\0';
    }
    return 0;
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Proxy Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Proxy Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}