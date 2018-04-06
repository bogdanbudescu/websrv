#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>

#define MYPORT   80
#define BACKLOG  10
#define GET      "GET"
#define POST     "POST"

typedef struct web_req {
    char *method;
    char *url;
    char *host;
};

typedef struct web_res {
    int status;
    char *body;
    long content_length;
    char *content_type;
    char *content_encoding;
};

int parse_line( web_req *req, char *buffer) {

    char *token;
    int token_counter = 0;
    token = strtok(buffer, " ");

    while (token != NULL) {

        if (token_counter == 0) {
            if (strcmp(token, GET) == 0) {
                req->method = (char *) malloc(strlen(token));
                strcpy(req->method, token);
            } else {
                free(token);
                return 400;
            }
        } else if (token_counter == 1) {
            req->url = (char *) malloc(strlen(token));
            strcpy(req->url, token);
        }

        token = strtok(NULL, " ");
        token_counter++;
    }

    free(token);
    return 0;

}

int handle_request(int new_fd, web_req *req) 
{
    char buf[256];
    int byte_count;
    int status;
    if ((byte_count = recv(new_fd, buf, 255, 0)) == -1) {
        perror("recv");
    }
    
    status = parse_line(req, buf);
    if(status == 0) {

    } else if(status > 0){
        // error 
    } else {
        // 500
    }
    
    write(new_fd, "HTTP/1.1 200 OK\n", 16);
    write(new_fd, "Content-length: 46\n", 19);
    write(new_fd, "Content-Type: text/html\n\n", 25);
    write(new_fd, "<html><body><H1>Hello world</H1></body></html>",46);
   
}

int main(void)
{
    int sockfd, new_fd;
    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;
    struct web_req req;
    int sin_size;
    int yes=1;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MYPORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(my_addr.sin_zero), '\0', 8);
    
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    
    printf("Starts listening for new connections...\n");

    while(1) {  
        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            handle_request(new_fd, &req);
            exit(0);
        } else {
            close(new_fd);
        }
    }

    return 0;
}
