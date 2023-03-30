#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP_ADDR "127.0.0.1"
#define TAR_FILE_NAME "temp.tar.gz"

// Function to handle receiving stream of file
int receive_files(int socket_fd) {
    // Open the tar file
    FILE* fp = fopen(TAR_FILE_NAME, "wb");
    if (!fp) {
        perror("Error creating tar file");
        return 1;
    }

    // Receive the tar file size from the server
    char size_buffer[BUFFER_SIZE];
    if (recv(socket_fd, size_buffer, BUFFER_SIZE, 0) == -1) {
        perror("Error receiving tar file size from server");
        fclose(fp);
        return 1;
    }
    long tar_file_size = atol(size_buffer);
    printf("File size received: %ld \n", tar_file_size);

    sprintf(size_buffer, "%ld", tar_file_size);
    if (send(socket_fd, size_buffer, strlen(size_buffer), 0) != strlen(size_buffer)) {
        perror("Error acknowledging to server");
        fclose(fp);
        return 1;
    }

    // Receive the tar file contents from the server
    char buffer[BUFFER_SIZE];
    long bytes_received = 0;
    size_t n;
    while (bytes_received < tar_file_size && (n = recv(socket_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        if (fwrite(buffer, sizeof(char), n, fp) != n) {
            perror("Error writing to tar file");
            break;
        }
        bytes_received += n;
    }
    printf("File received successfully\n");
    fclose(fp);
    return 0;
}

// Function to parse and validate the dates passed
int validate_dates(char* date1, char* date2) {
    struct tm tm_date1 = {0};
    struct tm tm_date2 = {0};
    time_t time_date1, time_date2;

    if (strptime(date1, "%Y-%m-%d", &tm_date1) == NULL) {
        printf("Failed to parse date string: %s\n", date1);
        return 1;
    }
    if (strptime(date2, "%Y-%m-%d", &tm_date2) == NULL) {
        printf("Failed to parse date string: %s\n", date2);
        return 1;
    }

    time_date1 = mktime(&tm_date1);
    time_date2 = mktime(&tm_date2);

    if (time_date1 > time_date2) {
        return 1;
    } else {
        return 0;
    }
}

void send_command(int client_fd, char* buffer) {
    // Send command to server
    if (send(client_fd, buffer, strlen(buffer), 0) != strlen(buffer)) {
        perror("TCP Client - Send Error");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char const *argv[]) {
    int client_fd, n;
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE] = {0};

    /**
     * Creating client socket file descriptor
     * AF_INET: IPv4 connection
     * SOCK_STREAM: TCP/IP protocol
    */
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("TCP Client - Socket Creation Error\n");
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
    address.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP_ADDR, &address.sin_addr) <= 0) {
        perror("TCP Client -Invalid Address");
        exit(EXIT_FAILURE);
    }

    // Connecting to the server
    if (connect(client_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("TCP Client - Connection Error");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Read command from user
        printf("Enter command: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strlen(buffer)-1] = '\0';

        // Exit if user types "quit"
        if (strcmp(buffer, "quit") == 0) {
            send_command(client_fd, buffer);
            break;
        } else if (strncmp(buffer, "sgetfiles", strlen("sgetfiles")) == 0) {
            char unzip_status[BUFFER_SIZE] = "";
            int min_value, max_value;
            sscanf(buffer, "%*s %d %d %s", &min_value, &max_value, unzip_status);
            if (max_value < min_value) {
                printf("Min value is not lesser than the Max value argument.\n");
                continue;
            }
            send_command(client_fd, buffer);
            int receive_status = receive_files(client_fd);
            int unzip = strncmp(unzip_status, "-u", strlen("-u")) == 0 ? 1 : 0;
            if (unzip && !receive_status) {
                printf("Unzipping %s\n", TAR_FILE_NAME);
                char system_call[BUFFER_SIZE] = "tar -xzvf";
                strcat(system_call, TAR_FILE_NAME);
                system(system_call);
            }
        } else if (strncmp(buffer, "dgetfiles", strlen("dgetfiles")) == 0) {
            char unzip_status[BUFFER_SIZE] = "";
            char min_date[BUFFER_SIZE];
            char max_date[BUFFER_SIZE];
            sscanf(buffer, "%*s %s %s", min_date, max_date);
            if (validate_dates(min_date, max_date)) {
                printf("Dates passed as arguments either do not follow the date format or min_date is after max_date\n");
                continue;
            }
            send_command(client_fd, buffer);
            int receive_status = receive_files(client_fd);
            int unzip = strncmp(unzip_status, "-u", strlen("-u")) == 0 ? 1 : 0;
            if (unzip && !receive_status) {
                printf("Unzipping %s\n", TAR_FILE_NAME);
                char system_call[BUFFER_SIZE] = "tar -xzvf";
                strcat(system_call, TAR_FILE_NAME);
                system(system_call);
            }
        } else {
            send_command(client_fd, buffer);
            // Receive response from server
            // bzero(buffer, BUFFER_SIZE);
            memset(buffer, 0, BUFFER_SIZE);
            n = read(client_fd, buffer, BUFFER_SIZE - 1);
            if (n < 0) {
                perror("TCP Client - Read Error");
                exit(EXIT_FAILURE);
            }
            if (n == 0) {
                printf("Server disconnected.\n");
                break;
            }
            buffer[n] = '\0';
            printf("Server response: \n%s\n", buffer);
        }
    }

    close(client_fd);
    return 0;
}
