#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 8080
// Allowing server to listen to 10 clients at a time
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define TAR_FILE_NAME "server_temp.tar.gz"

// Function to Find the file in the server from /home/soham
char* findfile(char* filename) {
    char *ptr_client_str;
    char str_appended[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    sprintf(command, "find ~/ -name %s -printf '%%f|%%s|%%T@\\n' | head -n 1", filename);
    FILE* fp = popen(command, "r");
    char path[BUFFER_SIZE];
    if (fgets(path, BUFFER_SIZE, fp) != NULL) {
        printf("File Found.\n");
        path[strcspn(path, "\n")] = 0; // Remove trailing newline
        // Extract the filename, size, and date from the path string
        char* filename_ptr = strtok(path, "|");
        char* size_ptr = strtok(NULL, "|");
        char* date_ptr = strtok(NULL, "|");

        // Convert the size and date strings to integers
        int size = atoi(size_ptr);
        time_t date = atoi(date_ptr);

        // Creating Filename string to be sent to client for displaying data
        char print_filename[BUFFER_SIZE];
        strcpy(print_filename, "File Name: ");
        strcat(print_filename, filename_ptr);
        strcat(print_filename, "\n");

        // Creating File Size string to be sent to client for displaying data
        char print_size[BUFFER_SIZE];
        strcpy(print_size, "File Size: ");
        strcat(print_size, size_ptr);
        strcat(print_size, "\n");

        // Creating File Created At string to be sent to client for displaying data
        char print_created[BUFFER_SIZE];
        strcpy(print_created, "File Created At: ");
        strcat(print_created, ctime(&date));
        strcat(print_created, "\n");

        // Appending all necessary data of the file to be sent to client as a single string
        strcpy(str_appended, print_filename);
        strcat(str_appended, print_size);
        strcat(str_appended, print_created);
    } else {
        printf("File not Found.\n");
        strcpy(str_appended, "File not Found.\n");
    }
    pclose(fp);
    ptr_client_str = str_appended;
    return ptr_client_str;
}

// Function to handle sending stream of file
void send_tar_file(int socket_fd) {
    // Open the tar file in binary read mode
    FILE* fp = fopen(TAR_FILE_NAME, "rb");
    if (!fp) {
        perror("Error opening tar file");
        return;
    }

    // Send the tar file size to the client
    fseek(fp, 0, SEEK_END);
    long tar_file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char size_buffer[BUFFER_SIZE];
    sprintf(size_buffer, "%ld", tar_file_size);
    if (send(socket_fd, size_buffer, strlen(size_buffer), 0) != strlen(size_buffer)) {
        perror("Error sending tar file size to client");
        fclose(fp);
        return;
    }
    printf("Client Acknowledged.\n");
    if (recv(socket_fd, size_buffer, BUFFER_SIZE, 0) == -1) {
        perror("Error sending tar file size to client");
        fclose(fp);
        return;
    }
    printf("File of size %ld being sent.\n", tar_file_size);

    // Sending the tar file contents to the client
    char buffer[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0) {
        if (send(socket_fd, buffer, n, 0) != n) {
            perror("Error sending tar file contents to client");
            break;
        }
    }
    printf("File sent successfully\n");
    fclose(fp);
}

// TODO - If file is removed, there is error of "Error opening tar file"
// Function to get the files in a range of size
void sgetfiles(int socket_fd, int size1, int size2) {
    char command[BUFFER_SIZE];
    sprintf(command, "find ~/ -type f -size +%d -size -%d -print0 | tar -czvf %s --null -T -", size1, size2, TAR_FILE_NAME);
    FILE* fp = popen(command, "r");
    send_tar_file(socket_fd);
}

// Function to get the files in a range of dates
void dgetfiles(int socket_fd, char* date1, char* date2) {
    char command[BUFFER_SIZE];
    sprintf(command, "find ~/ -type f -newermt \"%s\" ! -newermt \"%s\" -print0 | tar -czvf %s --null -T -", date1, date2, TAR_FILE_NAME);
    FILE* fp = popen(command, "r");
    send_tar_file(socket_fd);
}

int processClient(int socket_fd) {
    char *result;
    char buffer[BUFFER_SIZE];
    int n, fd;
    while(1) {
        result = NULL;
        bzero(buffer, BUFFER_SIZE);
        n = read(socket_fd, buffer, BUFFER_SIZE - 1);
        if (n < 0) {
            perror("TCP Server - Read Error");
            return 1;
        }
        if (n == 0) {
            break;
        }
        
        buffer[n] = '\0';
        printf("Command received: %s\n", buffer);
        printf("Processing command...\n");

        if (strncmp(buffer, "findfile", strlen("findfile")) == 0) {
            // Client asking for finding a file details
            char filename[BUFFER_SIZE];
            sscanf(buffer, "%*s %s", filename);
            printf("Filename: %s\n", filename);
            result = findfile(filename);
        } else if (strncmp(buffer, "sgetfiles", strlen("sgetfiles")) == 0) {
            int min_value, max_value;
            sscanf(buffer, "%*s %d %d", &min_value, &max_value);
            sgetfiles(socket_fd, min_value, max_value);
            result = NULL;
            continue;
        } else if (strncmp(buffer, "dgetfiles", strlen("dgetfiles")) == 0) {
            char min_date[BUFFER_SIZE];
            char max_date[BUFFER_SIZE];
            sscanf(buffer, "%*s %s %s", min_date, max_date);
            dgetfiles(socket_fd, min_date, max_date);
            result = NULL;
            continue;
        } else if (strcmp(buffer, "quit") == 0) {
            // Client disconnecting from the server
            printf("Client Quitting.\n");
            break;
        } else {
            // If there is no handler for handling client request, return the 
            // command sent by the client back to it
            result = buffer;
        }
        if (send(socket_fd, result, strlen(result), 0) != strlen(result)) {
            perror("TCP Server - Send Error");
            close(socket_fd);
            return 1;
        }
        printf("Sent response to client: %s\n", result);
    }
    close(socket_fd);
    return 0;
}

int main(int argc, char const *argv[]) {
    int num_of_clients = 0;
    int server_fd, client_fd, optval;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pid_t childpid;

    /**
     * Creating server socket file descriptor
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
            int exit_status = processClient(client_fd);
            if (exit_status == 0) {
                printf("Client Disconnected with Success Code.\n");
                exit(EXIT_SUCCESS);
            } else {
                printf("Client Disconnected with Error Code.\n");
                exit(EXIT_FAILURE);
            }
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