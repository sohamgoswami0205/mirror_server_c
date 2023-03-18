#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

int main(int argc, char const *argv[]) {
    int server_fd, client_fd, optval;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pid_t childpid;

    /**
     * Storing server socket file descriptor
     * AF_INET: IPv4 connection
     * SOCK_STREAM: TCP/IP protocol
    */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    optval = 1;
    /**
     * level: SOL_SOCKET
     * option: SO_REUSEADDR - For allowing reuse of local addresses while bind()
     * option_value: 1 - Needs value for processing the incoming request
    */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt error");
        exit(EXIT_FAILURE);
    }

    /**
     * Setting up the socket address structure values
     * sin_family: AF_INET - IPv4 connections
     * sin_addr.s_addr: INADDR_ANY - Accessing default interface for gateway
     * sin_port: 8080
    */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Binding the address structure to the socket openend
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    // Connect to the network and await until client connects
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen error");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        // Assigning the connecting client info to the file descriptor
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_fd < 0) {
            perror("accept error");
            continue;
        }

        printf("Client connected.\n");

        char buffer[BUFFER_SIZE];
        int valread = read(client_fd, buffer, BUFFER_SIZE);
        if (valread < 0) {
            perror("read error");
            close(client_fd);
            continue;
        }

        printf("Message received from Client:\n%s\n", buffer);

        // if (strncmp(buffer, "Hello", 5) != 0) {
        //     printf("Invalid message from client.\n");
        //     close(client_fd);
        //     continue;
        // }

        char* response = "I am server";
        if (send(client_fd, response, strlen(response), 0) != strlen(response)) {
            perror("send error");
            close(client_fd);
            continue;
        }

        printf("Sent response to client: %s\n", response);

        printf("Disconnecting client.\n");
        close(client_fd);
    }

    close(server_fd);
    return 0;
}