#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void echo(int connfd);
int parse_http_request(int connfd, char* host, char* port, char* path);
int parse_uri(char* uri, char* host, char* port, char* path);

int main(int argc, char** argv) {   
    int listenfd, connfd;
    socklen_t clientlen; // represents the length of a socket address
    struct sockaddr_storage clientaddr; // IPv independent sockaddr struct
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(0);
    }
    /* Given a specified port, return a listening file descriptor. */
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        /* Server blocks until connection request, which means it waits until 
        a connection request arrives. Fills in client socket address in clientaddr */
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (struct sockaddr*) &clientaddr, &clientlen);

        /* Get the hostname and port. Here, will be localhost and an ephemeral 
        port that is different from argv[1] */
        Getnameinfo((struct sockaddr*) &clientaddr, clientlen, client_hostname,
                MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);

        /* Listen for http request on connfd. Parse the request and any headers,
        and put result in hostname and query */
        char host[MAXLINE], port[6], path[MAXLINE];
        int isValid = parse_http_request(connfd, host, port, path);
        
        // if so, proxy establishes its own connection to the appropriate web server
        // then request the object the client specified
        // finally, proxy should read server's response and forward it to client
        if (isValid) {
            printf("%s %s %s\n", host, port, path);
        }
        Close(connfd);
    }
    exit(0);
}

// read the entirety of the request and parse it
// should determine if request was a valid HTTP request
// return 0 if valid http request, else return -1
int parse_http_request(int connfd, char* host, char* port, char* path) {

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t rio; 
    Rio_readinitb(&rio, connfd);

    if (!Rio_readlineb(&rio, buf, MAXLINE)) {
        return -1;
    }
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) { // TODO: also send HTTP response to client?
        return -1;
    }
    // parse URL into hostname and query
    if (parse_uri(uri, host, port, path)) {
        return -1;
    }
    // Valid HTTP response:
    // version should be HTTP/1.0 or HTTP/1.1
    if (strcasecmp(version, "HTTP/1.0") || strcasecmp(version, "HTTP/1.1")) {
        return -1;
    }
    return 0;
}

/*
 *
 * Example URIS:
 * http://example.com:8080/path/to/resource?query=string#fragment
 * http://example.com/path/to/resource?query=string#fragment
 */
int parse_uri(char* uri, char* host, char* port, char* path) {
    /* Check that the first 7 chars of uri are "http://".
    Return with error code if not */
    char protocol[8];
    strncpy(protocol, uri, 7);
    protocol[7] = '\0';
    if (!strcmp(protocol, "http://")) {
        return -1;
    }
    /* Get a pointer to the first instance of '/' within uri, starting from the
    substring after "http://". Every request must have a path. */
    if ((path = strstr(uri + 7, "/")) == NULL) {
        return -1;
    }
    // Get the substring from uri+7 to path
    char* hostPtr = uri + 7, *portPtr, curr;
    int i = 0;
    while (hostPtr + i != path) {
        curr = *(hostPtr + i);
        /* If curr becomes ":" before hostPtr + i == path, 
        use the requested port that lies between ':' and '/' instead of 80. */
        if (curr == ':') {
            portPtr = hostPtr + i + 1;
            break;
        }
        host[i] = curr;
        i += 1;
    }
    host[i] = '\0';
    if (portPtr == NULL) { // indicating port not specified
        strcpy(port, "80");
        return 0;
    }
    // Else, there is a specified port. Get the substring from portPtr to query
    i = 0;
    while (portPtr + i != path) {
        port[i] = *(portPtr + i);
        i += 1;
    }
    port[i] = '\0';
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
