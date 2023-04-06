// Using _XOPEN_SOURCE for using strptime
// date function
#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// PORT is the port number where the client will
// send request to for connection with server
#define PORT 8080
// The IP address of the server where the client
// send request to for connection with server
#define SERVER_IP_ADDR "127.0.0.1"
// Maximum length of the IP addresses used in the server
#define IP_LENGTH 16
// Maximum length of the port numbers used in the server
#define PORT_LENGTH 6
// On successful connection between client and server, the 
// server sends CONN_SUCCESS message to client
#define CONN_SUCCESS "success"

// The max buffer size that needs to be stored
#define BUFFER_SIZE 1024
// tar.gz file received from the server with
// all the requested files
#define TAR_FILE_NAME "temp.tar.gz"
// Maximum number of filenames that the server can handle
// in an incoming client command
#define MAX_FILES 6
// Maximum filename length that the server can handle while
// executing the file based client commands
#define MAX_FILENAME_LEN 50

// Commands used by the client to request information
// from the server
#define FIND_FILE "findfile"
#define S_GET_FILES "sgetfiles"
#define D_GET_FILES "dgetfiles"
#define GET_FILES "getfiles"
#define GET_TAR_GZ "gettargz"
#define QUIT "quit"

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
    printf("File size to be received: %ld \n", tar_file_size);

    char buffer[BUFFER_SIZE];
    sprintf(size_buffer, "%ld", tar_file_size);

    // Send acknowledgement to the server for file size received
    if (send(socket_fd, size_buffer, strlen(size_buffer), 0) != strlen(size_buffer)) {
        perror("Error acknowledging to server");
        fclose(fp);
        return 1;
    }

    // If the server has sent tar.gz file size to be 0,
    // No files found scenario
    if (strcmp(size_buffer, "0") == 0) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = read(socket_fd, buffer, BUFFER_SIZE - 1);
        if (n < 0) {
            perror("TCP Client - Read Error");
            exit(EXIT_FAILURE);
        }
        if (n == 0) {
            printf("Server disconnected.\n");
        }
        buffer[n] = '\0';
        printf("Server response: \n%s\n", buffer);
        // Returning 1 to avoid unzipping command to execute
        return 1;
    }

    // Receive the tar file contents from the server
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

// Function to read the arguments sent in the command to determine the files/extensions
// and to determine the value of the unzip flag
void read_filenames(char* buffer, char filenames[MAX_FILES][MAX_FILENAME_LEN], int* num_files, int* unzip_flag) {
    char buffer_copy[BUFFER_SIZE];
    strcpy(buffer_copy, buffer);
    char* token;
    char delim[] = " ";
    int i = 0;

    // Set the unzip flag to 0 by default
    *unzip_flag = 0;

    // Get the first token
    token = strtok(buffer_copy, delim);

    // Skip the first token (which is "getfiles")
    token = strtok(NULL, delim);

    // Read the filenames
    while (token != NULL && i < MAX_FILES) {
        if (strcmp(token, "-u") == 0) {
            // If "-u" is present, set the unzip flag to 1
            *unzip_flag = 1;
        } else {
            // Otherwise, store the filename in the array
            strncpy(filenames[i], token, MAX_FILENAME_LEN);
            i++;
        }
        token = strtok(NULL, delim);
    }

    *num_files = i;
}

// Function to handle the sending of commands from client to server
void send_command(int client_fd, char* buffer) {
    // Send command to server
    if (send(client_fd, buffer, strlen(buffer), 0) != strlen(buffer)) {
        perror("TCP Client - Send Error");
        exit(EXIT_FAILURE);
    }
}

// Function to validate the commands entered by the user
// on the client side before sending to the server
int validate_command(char *buffer) {
    int buffer_length = strlen(buffer);
    char buffer_copy[BUFFER_SIZE];
    strcpy(buffer_copy, buffer);
    char *valid_commands[] = {FIND_FILE, S_GET_FILES, D_GET_FILES, GET_FILES, GET_TAR_GZ, QUIT};
    int num_commands = sizeof(valid_commands) / sizeof(valid_commands[0]);
    char *token;
    int arg_count = 0;
    token = strtok(buffer_copy, " ");
    if (strcmp(token, QUIT) == 0) {
        while (token != NULL) {
            arg_count++;
            token = strtok(NULL, " ");
        }
        if (arg_count == 1) {
            // Valid quit command
            return 1;
        } else {
            // Invalid - Extra arguments
            printf("Extra arguments found after the command: %s\n", QUIT);
            return 0;
        }
    } else if (strncmp(token, FIND_FILE, strlen(FIND_FILE)) == 0) {
        while (token != NULL) {
            arg_count++;
            token = strtok(NULL, " ");
        }
        if (arg_count == 2) {
            // Valid findfile command
            return 1;
        } else if (arg_count < 2){
            // Invalid - Filename not found
            printf("File name required after the command: %s\n", FIND_FILE);
            return 0;
        } else {
            // Invalid - Extra arguments
            printf("Extra arguments found after the command: %s\n", FIND_FILE);
            return 0;
        }
    } else if (strncmp(token, S_GET_FILES, strlen(S_GET_FILES)) == 0) {
        int min_value, max_value;
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) { 
            // Invalid - No min or max sizes provided
            printf("Min and max sizes are not provided for fetching files based on sizes after %s.\n", S_GET_FILES);
            return 0;
        }
        for (int i = 0; i < strlen(token); i++) {
            if (token[i] < '0' || token[i] > '9') {
                // Invalid - Min size argument passed with sgetfiles is not integer
                printf("Value %s passed in argument 1 is not an integer\n", token);
                return 0;
            }
        }
        min_value = atoi(token);
        token = strtok(NULL, " ");
        if (token == NULL) {
            // Invalid - No max size provided
            printf("Max size is not provided for fetching files based on sizes.\n");
            return 0;
        }
        for (int i = 0; i < strlen(token); i++) {
            if (token[i] < '0' || token[i] > '9') {
                // Invalid - Max size argument passed with sgetfiles is not integer
                printf("Value %s passed in argument 2 is not an integer\n", token);
                return 0;
            }
        }
        max_value = atoi(token);
        if (max_value < min_value) {
            // Invalid min value in arg 1 should always be less than the max value in arg 2 of command
            printf("Min value: %d is not lesser than the Max value: %d argument.\n", min_value, max_value);
            return 0;
        }
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) {
            if (token != NULL && strtok(NULL, " ") != NULL) {
                // Invalid - Extra arguments found after -u
                printf("Extra arguments found after the command: %s\n", S_GET_FILES);
                return 0;        
            }
            return 1;
        }        
        printf("Extra arguments found after the command: %s\n", S_GET_FILES);
        return 0;
    } else if (strncmp(token, D_GET_FILES, strlen(D_GET_FILES)) == 0) {
        char min_date[BUFFER_SIZE];
        char max_date[BUFFER_SIZE];
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) { 
            // Invalid - No min or max dates provided
            printf("Min and max dates are not provided for fetching files based on dates after %s.\n", D_GET_FILES);
            return 0;
        }
        strcpy(min_date, token);
        token = strtok(NULL, " ");
        if (token == NULL) {
            // Invalid - No max date provided
            printf("Max date is not provided for fetching files based on sizes.\n");
            return 0;
        }
        strcpy(max_date, token);
        if (validate_dates(min_date, max_date)) {
            // Invalid - Dates are either invalid or max date is prior min date
            printf("Dates passed as arguments either do not follow the date format or min date is after max date\n");
            return 0;
        }
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) {
            if (token != NULL && strtok(NULL, " ") != NULL) {
                // Invalid - Extra arguments after -u
                printf("Extra arguments found after the command: %s\n", D_GET_FILES);
                return 0;        
            }
            return 1;
        }        
        printf("Extra arguments found after the command: %s\n", D_GET_FILES);
        return 0;
    } else if (strncmp(token, GET_FILES, strlen(GET_FILES)) == 0) {
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) {
            // Invalid - No filenames provided
            printf("Filenames to be fetched are not provided after %s.\n", GET_FILES);
            return 0;
        }
        while(token != NULL && strcmp(token, "-u") != 0) {
            arg_count++;
            token = strtok(NULL, " ");
            if (token == NULL) {
                if (arg_count <= MAX_FILES) {
                    return 1;
                } else {
                    // Invalid - Extra arguments
                    printf("Extra arguments found after the command: %s\n", GET_FILES);
                    return 0;
                }
            } else if (strcmp(token, "-u") == 0) {
                if (arg_count <= MAX_FILES && strtok(NULL, " ") == NULL) {
                    return 1;
                } else {
                    // Invalid - Extra arguments after -u or number of files requested more than MAX_FILES
                    printf("Extra arguments found after the command: %s\n", GET_FILES);
                    return 0;
                }
            }
        }
    } else if (strncmp(token, GET_TAR_GZ, strlen(GET_TAR_GZ)) == 0) {
        token = strtok(NULL, " ");
        if (token == NULL || strcmp(token, "-u") == 0) {
            // Invalid - No extensions found
            printf("Extensions to fetch files are not provided after %s.\n", GET_TAR_GZ);
            return 0;
        }
        while(token != NULL && strcmp(token, "-u") != 0) {
            arg_count++;
            token = strtok(NULL, " ");
            if (token == NULL) {
                if (arg_count <= MAX_FILES) {
                    return 1;
                } else {
                    // Invalid - Extra arguments
                    printf("Extra arguments found after the command: %s\n", GET_TAR_GZ);
                    return 0;
                }
            } else if (strcmp(token, "-u") == 0) {
                if (arg_count <= MAX_FILES && strtok(NULL, " ") == NULL) {
                    return 1;
                } else {
                    // Invalid - Extra arguments after -u or number of extensions requested more than MAX_FILES
                    printf("Extra arguments found after the command: %s\n", GET_TAR_GZ);
                    return 0;
                }
            }
        }
    }
    // If none of known commands are being entered by user
    // send Invalid command
    return 0;
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

    if (recv(client_fd, buffer, BUFFER_SIZE, 0) == -1) {
        perror("TCP Client - Error receiving connection status from server");
    }

    if (strcmp(buffer, CONN_SUCCESS) == 0) {
        printf("Connected to server successfully.\n");
    } else {
        // If the server is full, it will ask the client to redirect its
        // socket connection to the mirror server.
        // Mirror server IP address and port shall be provided by the server
        // which will be used by the client to create new socket connection
        // with the mirror server
        close(client_fd);
        client_fd = 0;
        if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("TCP Client - Socket Creation Error\n");
            exit(EXIT_FAILURE);
        }
        char mirror_ip[IP_LENGTH], mirror_port[PORT_LENGTH];
        char *ip, *port;
        // Creating copy of buffer to avoid loss of
        // buffer data while tokenizing
        char buffer_copy[BUFFER_SIZE];
        strcpy(buffer_copy, buffer);
        ip = strtok(buffer_copy, ":");
        port = strtok(NULL, ":");
        strncpy(mirror_ip, ip, sizeof(mirror_ip));
        strncpy(mirror_port, port, sizeof(mirror_port));
        memset(&address, '0', sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(atoi(mirror_port));
        if (inet_pton(AF_INET, mirror_ip, &address.sin_addr) <= 0) {
            perror("TCP Client - Invalid Address");
            exit(EXIT_FAILURE);
        }

        // Connecting to the mirror server
        if (connect(client_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("TCP Client - Connection Error");
            exit(EXIT_FAILURE);
        }

        memset(buffer, 0, sizeof(buffer));

        if (recv(client_fd, buffer, BUFFER_SIZE, 0) == -1) {
            perror("TCP Client - Error receiving connection status from server");
        }

        if (strcmp(buffer, CONN_SUCCESS) == 0) {
            printf("Connected to server successfully.\n");
        } else {
            perror("Could not connect to server or mirror");
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        // Read command from user
        printf("Enter command: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strlen(buffer)-1] = '\0';

        if (!validate_command(buffer)) {
            printf("Invalid Command: %s\n", buffer);
            continue;
        }

        // Exit if user types "quit"
        if (strcmp(buffer, QUIT) == 0) {
            send_command(client_fd, buffer);
            break;
        } else if (strncmp(buffer, FIND_FILE, strlen(FIND_FILE)) == 0) {
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
            printf("File details: \n%s\n", buffer);
        } else if (strncmp(buffer, S_GET_FILES, strlen(S_GET_FILES)) == 0) {
            char unzip_status[BUFFER_SIZE] = "";
            sscanf(buffer, "%*s %*d %*d %s", unzip_status);
            send_command(client_fd, buffer);
            int receive_status = receive_files(client_fd);
            int unzip = strncmp(unzip_status, "-u", strlen("-u")) == 0 ? 1 : 0;
            if (unzip && !receive_status) {
                printf("Unzipping %s\n", TAR_FILE_NAME);
                char system_call[BUFFER_SIZE] = "tar -xzvf";
                strcat(system_call, TAR_FILE_NAME);
                system(system_call);
            }
        } else if (strncmp(buffer, D_GET_FILES, strlen(D_GET_FILES)) == 0) {
            char unzip_status[BUFFER_SIZE] = "";
            char min_date[BUFFER_SIZE];
            char max_date[BUFFER_SIZE];
            sscanf(buffer, "%*s %s %s %s", min_date, max_date, unzip_status);
            send_command(client_fd, buffer);
            int receive_status = receive_files(client_fd);
            int unzip = strncmp(unzip_status, "-u", strlen("-u")) == 0 ? 1 : 0;
            if (unzip && !receive_status) {
                printf("Unzipping %s\n", TAR_FILE_NAME);
                char system_call[BUFFER_SIZE] = "tar -xzvf";
                strcat(system_call, TAR_FILE_NAME);
                system(system_call);
            }
        } else if (strncmp(buffer, GET_FILES, strlen(GET_FILES)) == 0) {
            char filenames[MAX_FILES][MAX_FILENAME_LEN];
            int num_files, unzip_flag;
            send_command(client_fd, buffer);
            read_filenames(buffer, filenames, &num_files, &unzip_flag);
            int receive_status = receive_files(client_fd);
            if (unzip_flag && !receive_status) {
                printf("Unzipping %s\n", TAR_FILE_NAME);
                char system_call[BUFFER_SIZE] = "tar -xzvf";
                strcat(system_call, TAR_FILE_NAME);
                system(system_call);
            }
        } else if (strncmp(buffer, GET_TAR_GZ, strlen(GET_TAR_GZ)) == 0) {
            char extensions[MAX_FILES][MAX_FILENAME_LEN];
            int num_extensions, unzip_flag;
            send_command(client_fd, buffer);
            read_filenames(buffer, extensions, &num_extensions, &unzip_flag);
            int receive_status = receive_files(client_fd);
            if (unzip_flag && !receive_status) {
                printf("Unzipping %s\n", TAR_FILE_NAME);
                char system_call[BUFFER_SIZE] = "tar -xzvf";
                strcat(system_call, TAR_FILE_NAME);
                system(system_call);
            }
        } else {
            // If none of the known command is entered and tried to send,
            // client will send it to the server and the server will
            // get the same command in response
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
