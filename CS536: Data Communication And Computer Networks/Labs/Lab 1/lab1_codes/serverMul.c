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
#include <arpa/inet.h>


void* client_handler(void* arg) {
    int new_socket = *((int*)arg);

    struct sockaddr_in address;
    socklen_t address_len = sizeof(address);
    getpeername(new_socket, (struct sockaddr *)&address, &address_len);

    char sentence[1024] = { 0 };
    int valread = read(new_socket, sentence, 1024);


    printf("message-from-client: %s, %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    printf("%s\n", sentence);
    send(new_socket, sentence, strlen(sentence), 0);

    // closing the connected socket
    printf("close-client: %s, %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    close(new_socket);
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

    if (bind(server_fd, (struct sockaddr*)&address,
             sizeof(address))
        < 0) {
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
