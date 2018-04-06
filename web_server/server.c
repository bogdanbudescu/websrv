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
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#define MYPORT   80
#define BACKLOG  10
#define GET      "GET"
#define POST     "POST"

typedef struct web_req {
    char *method;
    char *url;
    char *host;
    char *content;
}web_req;

typedef struct web_res {
    int status;
    char *body;
    long content_length;
    char *content_type;
    char *content_encoding;
}web_res;

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },  
	{"jpg", "image/jpeg"}, 
	{"jpeg","image/jpeg"},
	{"png", "image/png" },  
	{"htm", "text/html" },  
	{"html","text/html" },  
	{"cgi", "text/cgi"  },  
	{"asp", "text/asp"   },  
	{"jsp", "image/jsp" },  
	{"xml", "text/xml"  },  
	{"js"," text/js"     },
    {"css", "test/css"   }, 
    {"pdf", "application/pdf"},
	{0,0} };

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

int get_content(struct web_req *req,struct web_res *res) {

    char *file_path;
    char *root = ".";
    if(strcmp(req->url, "/") == 0) {
        strcpy(req->url, "/index.html");
    } 

    file_path = malloc(strlen(root) + strlen(req->url) + 1);
    strcpy(file_path, root);
    strcat(file_path, req->url);
    
    struct stat s;
    if(stat(file_path, &s) == 0) {
        if(s.st_mode & S_IFDIR) {
            return 404;
        }
    } else {
        return 404;
    }
    
    FILE *file;
    file = fopen(file_path, "rb");
    if (!file) {
        return -1;
    }
    
    char *file_content;
    file_content = malloc(s.st_size + 1);
    fread(file_content, 1, s.st_size, file);
    res->body = file_content;
    res->content_length = s.st_size;
    fclose(file);

    return 0;

}

int http_message(int http_status, char **http_message) {

    *http_message = malloc(30);

    switch (http_status) {
        case 200:
            strcpy(*http_message, "OK");
            break;
        case 201:
            strcpy(*http_message, "Created");
            break;
        case 400:
            strcpy(*http_message, "Bad Request");
            break;
        case 401:
            strcpy(*http_message, "Unauthorized");
            break;
        case 403:
            strcpy(*http_message, "Forbidden");
            break;
        case 404:
            strcpy(*http_message, "Not Found");
            break;
        case 405:
            strcpy(*http_message, "Method Not Allowed");
            break;
        case 500:
            strcpy(*http_message, "Internal Server Error");
            break;
        default:
            return -1;
            break;
    }

    return 0;
}

int respond(int new_fd, struct web_res *res) {
    char *status_message;
    char response_headers[200];

    http_message(res->status, &status_message);
    size_t content_type_length = strlen(res->content_type + 11);
    if (res->content_encoding != NULL) {
        content_type_length += strlen(res->content_encoding);
    }

    char *content_type = malloc(content_type_length);
    strcpy(content_type, res->content_type);

    if (res->content_encoding != NULL) {
        strcat(content_type, "; charset=");
        strcat(content_type, res->content_encoding);
    }

    sprintf(
        response_headers,
        "HTTP/1.0 %d %s\nContent-Type: %s\nContent-Length: %lu\nServer: Websrv C\r\n\r\n",
        res->status,
        status_message,
        content_type,
        res->content_length
    );

    write(new_fd, response_headers, strlen(response_headers));
    write(new_fd, res->body, res->content_length);
    write(new_fd, "\r\n", strlen("\r\n"));

    free(status_message);
    free(content_type);

    return 0;
}

void respond_with_error(int new_fd, int http_status) {

    char *message;
    char *html = malloc(100);

    http_message(http_status, &message);
    sprintf(html, "<html><body>%s</body></html>", message);

    struct web_res res;
    res.status = http_status;
    res.body = html;
    res.content_length = strlen(html);
    res.content_type = strdup("text/html");
    res.content_encoding = strdup("utf-8");

    respond(new_fd, &res);
}

char* get_content_type_from_filepath(char *file_path) {

    char *file_path_copy = strdup(file_path);
    char *token;
    char *extension;
    char *content_type = malloc(20);

    extension = malloc(0);
    while ((token = strsep(&file_path_copy, ".")) != NULL) {

        free(extension);
        extension = strdup(token);
    }

    free(file_path_copy);
    free(token);
    int i = 0;
    for(i=0;extensions[i].ext != 0;i++) {
		if( strcmp(extension, extensions[i].ext) == 0) {
			strcpy(content_type, extensions[i].filetype);
		}
	}

    return content_type;
}

void log_request(struct web_req *req, int http_status) {

    printf("%s %s %d\n", req->method, req->url, http_status);
}

int handle_request(int new_fd, web_req *req) {
    
    char buf[256];
    int byte_count;
    int status;
    if ((byte_count = recv(new_fd, buf, 255, 0)) == -1) {
        perror("recv");
    }
    
    status = parse_line(req, buf);
    if(status == 0) {
        char *file_content;
        int *file_length;
        struct web_res res;

        status = get_content(req, &res);
        
        if(status == 0) {
            status = 200;
            char *content_type = get_content_type_from_filepath(req->url);
            res.status = status;
            res.content_type = content_type;
            res.content_encoding = "utf-8";

            // if(strcmp(content_type, "text/html") == 0) {
            //     res.content_encoding = "utf-8";
            // }
            respond(new_fd, &res);
        } else if(status > 0){
            respond_with_error(new_fd, status);
        } else {
            respond_with_error(new_fd, 500);
        }
    } 

    log_request(req, status);
    close(new_fd);
    return 0;

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
