#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP_ADDR "127.0.0.1"

int main(int argc, char const *argv[]) {
    int client_fd;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = {0};

    // Creating a client socket
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("TCP Client - Socket Creation Error\n");
        exit(EXIT_FAILURE);
    }

    // Setting up the server address structure
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP_ADDR, &server_address.sin_addr) <= 0) {
        perror("TCP Client -Invalid Address");
        exit(EXIT_FAILURE);
    }

    // Connecting to the server
    if (connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("TCP Client - Connection Error");
        exit(EXIT_FAILURE);
    }

    // Sending a message to the server
    char *message = "Hello from the client!";
    send(client_fd, message, strlen(message), 0);

    // Receiving a message from the server
    int valread = read(client_fd, buffer, BUFFER_SIZE);
    printf("Server says: %s\n", buffer);

    close(client_fd);
    return 0;
}
