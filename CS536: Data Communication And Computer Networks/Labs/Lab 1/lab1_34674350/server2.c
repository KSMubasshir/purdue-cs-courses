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
#include <fcntl.h> /* Added for the nonblocking socket */
#include <time.h>



#define MAX_THREADS 10
#define BUFFER_SIZE 1024
#define FRAME_SIZE 40960
#define MAX_OBJECTS 50

struct Object {
    int object_id;
    int object_size;
    int last_sent_frame;
    int total_frames;
    int object_send_complete;
    char *content_type;
    FILE *file;
};

int server_fd, new_socket;
struct sockaddr_in address;
pthread_t thread_pool[MAX_THREADS];
int thread_count = 0;

void *client_handler(void *socket_desc) {
    int hasFile = 0;
    char *html_file;
    char response_header[1024];
    char buffer[BUFFER_SIZE] = {0};
    struct stat file_stat;
    char method[8], url[BUFFER_SIZE], http_version[16];
    int valread = 0;
    char path[50];

    struct sockaddr_in address;
    socklen_t address_len = sizeof(address);
    getpeername(*(int*)socket_desc, (struct sockaddr *)&address, &address_len);

    struct Object objects[MAX_OBJECTS];
    int num_of_objects = 0;
    int objects_sent;


    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(*(int *)socket_desc, buffer, BUFFER_SIZE);
        if (valread > 0) {
            // has new GET request
            printf("message-from-client: %s, %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            printf("%s\n", buffer);
            sscanf(buffer, "%s %s %s", method, url, http_version);

            char *requested_file_name = strtok(url, "/");
            strcpy(path, "www");
            strcat(path, url);

            if (strcmp(method, "GET") != 0) {
                sprintf(response_header, "HTTP/2.0 400 Bad Request\r\n\r\n");
                send(*(int *)socket_desc, response_header, strlen(response_header) , 0);
            }
            else if (strcmp(http_version, "HTTP/2.0") != 0) {
                sprintf(response_header, "HTTP/2.0 505 HTTP Version Not Supported\r\n\r\n");
                send(*(int *)socket_desc, response_header, strlen(response_header) , 0);
            }
            else if (stat(path, &file_stat) < 0) {
                sprintf(response_header, "HTTP/2.0 404 Not Found\r\n\r\n");
                send(*(int *)socket_desc, response_header, strlen(response_header) , 0);
            }
            else if (!S_ISREG(file_stat.st_mode)) {
                sprintf(response_header, "HTTP/2.0 404 Not Found\r\n\r\n");
                send(*(int *)socket_desc, response_header, strlen(response_header) , 0);
            }
            else{
                char *extension = strrchr(requested_file_name, '.');
                if (extension == NULL) {
                    printf("Invalid file type\n");
                }
                char *content_type;
                if (strcmp(extension, ".html") == 0) {
                    content_type = "text/html";
                } else if (strcmp(extension, ".jpeg") == 0) {
                    content_type = "image/jpeg";
                } else if (strcmp(extension, ".mp4") == 0) {
                    content_type = "video/mp4";
                } else {
                    printf("Invalid file type\n");
                }

                objects[num_of_objects].object_id = num_of_objects;
                objects[num_of_objects].object_size = file_stat.st_size;
                objects[num_of_objects].total_frames = (file_stat.st_size + FRAME_SIZE - 1) / FRAME_SIZE;
                objects[num_of_objects].content_type = content_type;
                objects[num_of_objects].last_sent_frame = 0;
                objects[num_of_objects].object_send_complete = 0;
                objects[num_of_objects].file = fopen(path, "r");

                num_of_objects += 1;
            }
        }

        for (int i = 0; i < num_of_objects; ++i) {
            if(objects[i].object_send_complete==1){
                continue;
            }
            if(objects[i].last_sent_frame==objects[i].total_frames){
                objects[i].object_send_complete = 1;
                continue;
            }
            int num_frames = objects[i].total_frames;
            int cur_frame = objects[i].last_sent_frame;
            FILE *file = objects[i].file;
            char frame_header[256];
            int frame_size = (cur_frame == num_frames - 1) ? (file_stat.st_size - i * FRAME_SIZE) : FRAME_SIZE;
            sprintf(frame_header, "HTTP/2.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nObject-Number: %d\r\nFrame-Number: %d\r\nTotal-Frames: %d\r\n\r\n", objects[i].content_type, frame_size, objects[i].object_id, objects[i].last_sent_frame + 1, objects[i].total_frames);
            objects[i].last_sent_frame = objects[i].last_sent_frame + 1;
            char *frame = malloc(frame_size + strlen(frame_header));
            strcpy(frame, frame_header);
            fread(frame + strlen(frame_header), 1, frame_size, file);
            send(*(int *)socket_desc, frame, frame_size + strlen(frame_header), 0);
            free(frame);
            usleep( 30 * 1000);
        }
        objects_sent = 0;
        for (int i = 0; i <= num_of_objects; ++i) {
            if(objects[i].object_send_complete==1){
                objects_sent +=1;
            }
        }
        if(num_of_objects > 1 && objects_sent==num_of_objects){
            break;
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
        fcntl(new_socket, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state	*/

        printf("message-from-client: %s, %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
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