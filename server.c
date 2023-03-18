#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <errno.h>

#define PORT 8080
// Allowing server to listen to 10 clients at a time
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

void processClient(int socket_fd) {
    char buffer[BUFFER_SIZE];
    int n, fd;
    while(1) {
        bzero(buffer, BUFFER_SIZE);
        n = read(socket_fd, buffer, BUFFER_SIZE - 1);
        if (n < 0) {
            perror("TCP Server - Read Error");
            exit(EXIT_FAILURE);
        }
        if (n == 0) {
            printf("Client disconnected.\n");
            break;
        }
        
        buffer[n] = '\0';
        printf("Command received: %s\n", buffer);

        if (strcmp(buffer, "quit") == 0) {
            break;
        }
        else {
            if (send(socket_fd, buffer, strlen(buffer), 0) != strlen(buffer)) {
                perror("TCP Server - Send Error");
                close(socket_fd);
                continue;
            }

            printf("Sent response to client: %s\n", buffer);
        }
    }
    printf("Client disconnected.\n");
    close(socket_fd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char const *argv[]) {
    int num_of_clients = 0;
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
        perror("TCP Server - Socket Error");
        exit(EXIT_FAILURE);
    }

    optval = 1;
    /**
     * level: SOL_SOCKET
     * option: SO_REUSEADDR - For allowing reuse of local addresses while bind()
     * option_value: 1 - Needs value for processing the incoming request
    */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("TCP Server - setsockopt Error");
        exit(EXIT_FAILURE);
    }

    /**
     * Setting up the socket address structure values
     * sin_family: AF_INET - IPv4 connections
     * sin_addr.s_addr: INADDR_ANY - Accessing default interface for gateway
     * sin_port: 8080
    */
    memset(&address, '0', sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Binding the address structure to the socket openend
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("TCP Server - Bind Error");
        exit(EXIT_FAILURE);
    }

    // Connect to the network and await until client connects
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("TCP Server - Listen Error");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        // Assigning the connecting client info to the file descriptor
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_fd < 0) {
            perror("TCP Server - Accept Error");
            continue;
        }

        printf("Client connected.\n");

        childpid = fork();
        if (childpid < 0) {
            perror("TCP Server - Fork Error");
            exit(EXIT_FAILURE);
        }

        if (childpid == 0) {
            // Child process
            close(server_fd);
            processClient(client_fd);
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            num_of_clients++;
            printf("Total Clients connected till now: %d\n", num_of_clients);
            close(client_fd);
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    close(server_fd);
    return 0;
}