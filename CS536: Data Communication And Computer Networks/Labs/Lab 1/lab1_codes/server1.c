//
// Created by ksmubasshir on 1/27/23.
//
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <dirent.h>


#define MAX_THREADS 10
#define BUFFER_SIZE 1024

int server_fd, new_socket;
struct sockaddr_in address;
pthread_t thread_pool[MAX_THREADS];
int thread_count = 0;

void *client_handler(void *socket_desc) {

    struct sockaddr_in address;
    socklen_t address_len = sizeof(address);
    getpeername(*(int*)socket_desc, (struct sockaddr *)&address, &address_len);

    int hasFile = 0;
    char *html_file;
    char response_header[1024];
    char buffer[BUFFER_SIZE] = {0};
    struct stat file_stat;
    char method[8], url[BUFFER_SIZE], http_version[16];
    int valread = 0;
    char path[50];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(*(int *)socket_desc, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            break;
        }
        printf("message-from-client: %s, %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        printf("%s\n", buffer);
        sscanf(buffer, "%s %s %s", method, url, http_version);

        char *requested_file_name = strtok(url, "/");
        strcpy(path, "www");
        strcat(path, url);

        if (strcmp(method, "GET") != 0) {
            sprintf(response_header, "HTTP/1.1 400 Bad Request\r\n\r\n<!DOCTYPE html>\n"
                                     "<html>\n"
                                     "\t<body>\n"
                                     "\t\t<h1>400 Bad Request</h1>\n"
                                     "\t\t<p>400 Bad Request</p>\n"
                                     "\t</body>\n"
                                     "</html>\n"
                                     "");
        }
        else if (strcmp(http_version, "HTTP/1.1") != 0) {
            sprintf(response_header, "HTTP/1.1 505 HTTP Version Not Supported\r\n\r\n<!DOCTYPE html>\n"
                                     "<html>\n"
                                     "\t<body>\n"
                                     "\t\t<h1>505 HTTP Version Not Supported</h1>\n"
                                     "\t\t<p>505 HTTP Version Not Supported</p>\n"
                                     "\t</body>\n"
                                     "</html>\n"
                                     "");
        }
        else if (stat(path, &file_stat) < 0) {
            sprintf(response_header, "HTTP/1.1 404 Not Found\r\n\r\n<!DOCTYPE html>\n"
                                     "<html>\n"
                                     "\t<body>\n"
                                     "\t\t<h1>404 Not Found</h1>\n"
                                     "\t\t<p>404 Not Found</p>\n"
                                     "\t</body>\n"
                                     "</html>\n"
                                     "");
        }
        else if (!S_ISREG(file_stat.st_mode)) {
            sprintf(response_header, "HTTP/1.1 404 Not Found\r\n\r\n<!DOCTYPE html>\n"
                                     "<html>\n"
                                     "\t<body>\n"
                                     "\t\t<h1>404 Not Found</h1>\n"
                                     "\t\t<p>404 Not Found</p>\n"
                                     "\t</body>\n"
                                     "</html>");
        }
        else{
            hasFile = 1;
            html_file = malloc(file_stat.st_size + 1);
            FILE *file = fopen(path, "r");
            fread(html_file, file_stat.st_size, 1, file);
            fclose(file);

            char *extension = strrchr(requested_file_name, '.');
            if (extension == NULL) {
                printf("Invalid file type\n");
            }
            if (strcmp(extension, ".html") == 0) {
                sprintf(response_header, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n", file_stat.st_size);
            } else if (strcmp(extension, ".jpeg") == 0) {
                sprintf(response_header, "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %ld\r\n\r\n", file_stat.st_size);
            } else if (strcmp(extension, ".mp4") == 0) {
                sprintf(response_header, "HTTP/1.1 200 OK\r\nContent-Type: video/mp4\r\nContent-Length: %ld\r\n\r\n", file_stat.st_size);
            } else {
                printf("Invalid file type\n");
            }

        }
        printf("message-to-client: %s, %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        printf("%s\n", response_header);
        if(hasFile==1){
            char *response = malloc(strlen(response_header) + file_stat.st_size);
            strcpy(response, response_header);
            memcpy(response + strlen(response_header), html_file, file_stat.st_size);
            send(*(int *)socket_desc, response, strlen(response_header) + file_stat.st_size, 0);
            free(html_file);
            free(response);
        }
        else{
            send(*(int *)socket_desc, response_header, strlen(response_header) , 0);
        }
    }
    close(*(int *)socket_desc);
    return NULL;
}

int main(int argc, char const* argv[])
{
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(atoi(argv[1]));

    if (bind(server_fd, (struct sockaddr*)&address,sizeof(address))< 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("The server is ready to receive\n");
    while (1){
        if ((new_socket
                     = accept(server_fd, (struct sockaddr*)&address,
                              (socklen_t*)&addrlen)) < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }
        // Create a new thread for each client
        pthread_t client_thread;
        int* new_sock = malloc(sizeof(int));
        *new_sock = new_socket;
        if (pthread_create(&client_thread, NULL, client_handler, (void*)new_sock) < 0) {
            perror("could not create thread");
            return 1;
        }
    }
    // closing the listening socket
    shutdown(server_fd, SHUT_RDWR);
    return 0;
}