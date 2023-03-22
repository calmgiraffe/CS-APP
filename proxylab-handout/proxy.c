#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void echo(int connfd);

int main(int argc, char** argv) {   
    int listenfd, connfd;
    socklen_t clientlen; // represents the length of a socket address
    struct sockaddr_storage clientaddr; // IPv independent sockaddr struct
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(0);
    }
    // getaddrinfo returns a linked list, and each element here points to a socket address struct
    // since we're running on same computer, localhost connects to localhost
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (struct sockaddr*) &clientaddr, &clientlen);
        Getnameinfo((struct sockaddr*) &clientaddr, clientlen, client_hostname,
                MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        echo(connfd);
        Close(connfd);

        // accept a connection on specified port
        // read the entirety of the request and parse it
        // should determine if request was a valid HTTP request
        // if so, proxy establishes its own connection to the appropriate web server
        // then request the object the client specified
        // finally, proxy should read server's response and forward it to client
    }
    exit(0);
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
