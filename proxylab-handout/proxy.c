#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_PORT_LEN 6
#define MAX_NUM_HEADERS 1000
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
// static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; 
// Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void echo(int connfd);
void parse_http_request(int connfd, char *host, char *port, char *path, char *hdrs);
int parse_uri(char *uri, char *host, char *port, char *path);

int main(int argc, char **argv) {
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; // IPv independent sockaddr struct
    char client_hostname[MAXLINE], client_port[MAX_PORT_LEN];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        /* Server blocks (waits) until connection request arrives */
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen);

        /* Get the hostname and port. In context of driver, will be localhost
        and ephemeral port that is different from argv[1] */
        Getnameinfo((struct sockaddr *) &clientaddr, clientlen, client_hostname,
                MAXLINE, client_port, MAX_PORT_LEN, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);

        /* parse request and headers, and put result in host, port, path
        note: represents origin server info */
        char host[MAXLINE], port[MAX_PORT_LEN], path[MAXLINE], headers[MAXLINE];
        parse_http_request(connfd, host, port, path, headers);
        
        // if valid connection, proxy establishes its own connection to the
        // appropriate web server then requests the object the client specified
        printf("%s %s %s\n", host, port, path);
        
        // finally, proxy should read server's response and forward it to client
        Close(connfd);
    }
    exit(0);
}

// read the entirety of the request and parse it
// should determine if request was a valid HTTP request
// return 0 if valid http request, else return -1
void parse_http_request(int connfd, char *host, char *port, char *path, char *hdrs) {
    rio_t rio; 
    Rio_readinitb(&rio, connfd);
    char buf[MAXLINE], method[5], uri[MAXLINE], version[9];

    // TODO: redo error handing functions so they don't exit() thereby terminating program
    if (!Rio_readlineb(&rio, buf, MAXLINE)) {
        return;
    }
    if (strcasecmp(method, "GET") | parse_uri(uri, host, port, path) | 
        strcasecmp(version, "HTTP/1.0") | strcasecmp(version, "HTTP/1.1")) {
        // There was an error in one of the functions
        return;
    }
    // parse headers, store in hdrs
    if (parse_request_hdrs(&rio, hdrs)) {
        return;
    }
    
    return;
}

/*
 * Read HTTP headers, if any, and stores them in hdrs. Stored in continuous
 * char array with \r\n to indicate end of header.
 */
int parse_request_hdrs(rio_t *rp, char *hdrs) {
    char buf[MAXLINE];
    int totalLen = 0;
    int len;

    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE); // terminates '\n' with '\0'
        len = strlen(buf);               // len of header txt + "\r\n"
        totalLen += len;
        if (totalLen >= MAXLINE) {
            // TODO: raise error
            hdrs[MAXLINE-1] = '\0';
            return -1;
        }
        strncat(hdrs, buf, len);
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
    // TODO: HTTPS functionality
    char protocol[8];
    strncpy(protocol, uri, 7);
    protocol[7] = '\0';
    if (strcasecmp(protocol, "http://")) {
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


void echo(int connfd) {
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("server recieved %d bytes\n", (int) n);
        buf[n-1] = ' ';
        strncat(buf, "echo", 5);
        n += 5;
        buf[n-1] = '\n';
        Rio_writen(connfd, buf, n);
    }
}
