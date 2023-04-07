#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>

// PORT is the port number where the server will be deployed
#define PORT 8080
// MIRROR_PORT is the port number where the mirror server
// is deployed
#define MIRROR_PORT 8081
// The IP address of the mirror server where the main
// server will redirect the incoming excess clients to
#define MIRROR_SERVER_IP_ADDR "127.0.0.1"
// Maximum length of the IP addresses used in the server
#define IP_LENGTH 16
// Maximum length of the port numbers used in the server
#define PORT_LENGTH 6
// On successful connection between client and server, the 
// server sends CONN_SUCCESS message to client
#define CONN_SUCCESS "success"

// Allowing server to listen to 4 clients before involving
// the mirror server
#define MAX_CLIENTS 4
// The max buffer size that needs to be stored
#define BUFFER_SIZE 1024
// Temporary tar.gz file created by the server to
// be sent to the client
#define TAR_FILE_NAME "server_temp.tar.gz"
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

// Function to Find the file in the server from /home/soham
char* findfile(char* filename) {
    char str_appended[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    sprintf(command, "find ~/ -name %s -printf '%%f|%%s|%%T@\\n' | head -n 1", filename);
    FILE* fp = popen(command, "r");
    char path[BUFFER_SIZE];
    if (fgets(path, BUFFER_SIZE, fp) != NULL) {
        printf("Requested File Found.\n");
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
    char *ptr_client_str = str_appended;
    return ptr_client_str;
}

// Function to handle sending stream of file
void send_tar_file(int socket_fd) {
    int break_flag = 0;
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

    // Receive acknowledgement from the client for tar file size share
    if (recv(socket_fd, size_buffer, BUFFER_SIZE, 0) == -1) {
        perror("Error sending tar file size to client");
        fclose(fp);
        return;
    }
    printf("Client Acknowledged.\n");
    printf("File of size %ld being sent.\n", tar_file_size);

    // Sending the tar file contents to the client
    char buffer[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0) {
        if (send(socket_fd, buffer, n, 0) != n) {
            perror("Error sending tar file contents to client");
            break_flag = 1;
            break;
        }
    }
    if (break_flag) {
        printf("Unable to send tar.gz file to client\n");
    } else {
        printf("File sent successfully\n");
    }
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

// Function to find all the files present in directory(s)
// rooted at dir_name passed as argument
int find_files(const char *dir_name, const char *filename, char *tar_file) {
    int found = 0;
    DIR *dir;
    struct dirent *entry;
    struct stat file_info;
    char path[PATH_MAX];

    // Open the directory for traversal
    if ((dir = opendir(dir_name)) == NULL) {
        perror("opendir");
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Creating path for the entry being checked for
        snprintf(path, PATH_MAX, "%s/%s", dir_name, entry->d_name);

        // Fetching the stats
        if (lstat(path, &file_info) < 0) {
            perror("lstat");
            continue;
        }

        // Recursive call to find_files if the current entry is a directory
        // to search for the filename inside that directory tree
        if (S_ISDIR(file_info.st_mode)) {
            find_files(path, filename, tar_file);
        } else if (S_ISREG(file_info.st_mode)) {
            if (strcmp(entry->d_name, filename) == 0) {
                strncat(tar_file, " ", BUFFER_SIZE - strlen(tar_file) - 1);
                strncat(tar_file, path, BUFFER_SIZE - strlen(tar_file) - 1);
                printf("File found at: %s\n", path);
                found = 1;
            }
        }
    }

    closedir(dir);
    return found;
}

// Function to get the files
char* getfiles(int socket_fd, char files[MAX_FILES][MAX_FILENAME_LEN], int num_files) {
    // Get the current directory path
    char *dir_path = getenv("HOME");
    if (dir_path == NULL) {
        fprintf(stderr, "Error getting HOME directory path\n");
        return NULL;
    }

    // Create the tar command
    char tar_cmd[BUFFER_SIZE] = "tar -czvf ";
    strcat(tar_cmd, TAR_FILE_NAME);

    int file_found = 0;
    for (int i = 0; i < num_files; i++) {
        printf("Finding Filename: %s\n", files[i]);
        char file_path[BUFFER_SIZE];
        snprintf(file_path, BUFFER_SIZE, "%s/%s", dir_path, files[i]);

        const char *homedir = getenv("HOME");
        if (homedir == NULL) {
            printf("Could not get HOME directory\n");
            return 0;
        }

        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/%s", homedir, files[i]);
        file_found += find_files(homedir, files[i], tar_cmd);
    }

    if (file_found) {
        printf("File(s) found\n");
        // Run the tar command
        system(tar_cmd);

        // Send the tar file to the client
        FILE *tar_file = fopen(TAR_FILE_NAME, "r");
        if (tar_file == NULL) {
            fprintf(stderr, "Error opening tar file\n");
            return NULL;
        }
        fclose(tar_file);
        // create socket and send the tar file to client
        send_tar_file(socket_fd);
    } else {
        printf("No file found.\n");
        if (send(socket_fd, "0", strlen("0"), 0) != strlen("0")) {
            perror("Error sending tar file size to client");
            return NULL;
        }
        return "No file found.";
    }

    return NULL;
}

// Recursive Function to create a list of matching files with extensions
void find_gettargz_files(const char *dir_path, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions, FILE *temp_list) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        printf("Error: could not open directory %s\n", dir_path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            // This is a regular file
            char *name = entry->d_name;
            for (int i = 0; i < num_extensions; i++) {
                char *extension = extensions[i];
                int len_ext = strlen(extension);
                int len_name = strlen(name);
                if (len_name >= len_ext && strcmp(name + len_name - len_ext, extension) == 0) {
                    // This file has the matching extension, adding it to the list
                    fprintf(temp_list, "%s/%s\n", dir_path, name);
                    break;
                }
            }
        } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // This is a directory, recursively search it
            char subdir_path[BUFFER_SIZE];
            snprintf(subdir_path, sizeof(subdir_path), "%s/%s", dir_path, entry->d_name);
            find_gettargz_files(subdir_path, extensions, num_extensions, temp_list);
        }
    }

    closedir(dir);
}

// Function to find and send the tar.gz file with the files with extensions from extensions list
char* gettargz(int socket_fd, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions) {
    int file_found = 0;
    // Create a temporary file to store the list of matching files
    FILE *temp_list = tmpfile();
    if (!temp_list) {
        printf("Error: could not create temporary file\n");
        return NULL;
    }

    // Recursively search for files with matching extensions in the directory tree
    // starting from the user directory - /home/soham
    find_gettargz_files(getenv("HOME"), extensions, num_extensions, temp_list);
    rewind(temp_list);
    char filename[BUFFER_SIZE];

    while (fgets(filename, sizeof(filename), temp_list) != NULL) {
        // Remove the newline character at the end of the filename
        filename[strcspn(filename, "\n")] = 0;
        file_found++;
    }

    if (file_found) {
        printf("Atleast 1 file found\n");
        // Create a tar file containing the matching files
        rewind(temp_list);
        char command[BUFFER_SIZE] = "tar -czvf ";
        strcat(command, TAR_FILE_NAME);
        char filename[BUFFER_SIZE];
        while (fgets(filename, sizeof(filename), temp_list) != NULL) {
            // Remove the newline character at the end of the filename
            filename[strcspn(filename, "\n")] = 0;
            // Add the filename to the tar command
            strcat(command, " ");
            strcat(command, filename);
        }
        int result = system(command);
        // create socket and send the tar file to client
        send_tar_file(socket_fd);
        fclose(temp_list);
    } else {
        printf("No file found.\n");
        if (send(socket_fd, "0", strlen("0"), 0) != strlen("0")) {
            perror("Error sending tar file size to client");
            return NULL;
        }
        fclose(temp_list);
        return "No file found.";
    }
    return NULL;
}

// Function to read the arguments sent in the command to determine the files/extensions
void read_filenames(char* buffer, char filenames[MAX_FILES][MAX_FILENAME_LEN], int* num_files) {
    char* token;
    char delim[] = " ";
    int i = 0;

    // Get the first token
    token = strtok(buffer, delim);

    // Skip the first token (which is "getfiles")
    token = strtok(NULL, delim);

    // Read the filenames
    while (token != NULL && i < MAX_FILES) {
        if (strcmp(token, "-u") == 0) {
            // If "-u" is present, ignore it in server
        } else {
            // Otherwise, store the filename in the array
            strncpy(filenames[i], token, MAX_FILENAME_LEN);
            i++;
        }
        token = strtok(NULL, delim);
    }

    *num_files = i;
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
        printf("\n************************************************************\n");
        printf("Command received: %s\n", buffer);
        printf("************************************************************\n");
        printf("Processing command...\n");

        if (strncmp(buffer, FIND_FILE, strlen(FIND_FILE)) == 0) {
            // Client asking for finding a file details
            char filename[BUFFER_SIZE];
            sscanf(buffer, "%*s %s", filename);
            printf("Filename: %s\n", filename);
            result = findfile(filename);
        } else if (strncmp(buffer, S_GET_FILES, strlen(S_GET_FILES)) == 0) {
            int min_value, max_value;
            sscanf(buffer, "%*s %d %d", &min_value, &max_value);
            sgetfiles(socket_fd, min_value, max_value);
            result = NULL;
            continue;
        } else if (strncmp(buffer, D_GET_FILES, strlen(D_GET_FILES)) == 0) {
            char min_date[BUFFER_SIZE];
            char max_date[BUFFER_SIZE];
            sscanf(buffer, "%*s %s %s", min_date, max_date);
            dgetfiles(socket_fd, min_date, max_date);
            result = NULL;
            continue;
        } else if (strncmp(buffer, GET_FILES, strlen(GET_FILES)) == 0) {
            char filenames[MAX_FILES][MAX_FILENAME_LEN];
            int num_files;
            read_filenames(buffer, filenames, &num_files);
            result = getfiles(socket_fd, filenames, num_files);
            if (result == NULL) {
                continue;
            }
        } else if (strncmp(buffer, GET_TAR_GZ, strlen(GET_TAR_GZ)) == 0) {
            char extensions[MAX_FILES][MAX_FILENAME_LEN];
            int num_extensions;
            read_filenames(buffer, extensions, &num_extensions);
            result = gettargz(socket_fd, extensions, num_extensions);
            if (result == NULL) {
                continue;
            }
        } else if (strcmp(buffer, QUIT) == 0) {
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
    // Counter to keep track of number of client
    // connections facilitated
    int num_of_clients = 0, num_of_server_clients = 0;
    
    // Integers to store the socket connection
    // file descriptors
    int server_fd, client_fd;

    // address will store the details regarding
    // the socket connections being handled
    // like port to be connected to, etc.
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Integer to store the option value = 1 for forceful
    // creation of socket as per mentioned attributes
    int optval = 1;

    // Storing the forked child process for handling
    // each client
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

    // Binding the address structure to the socket opened
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("TCP Server - Bind Error");
        exit(EXIT_FAILURE);
    }

    // Connect to the network and await until client connects
    if (listen(server_fd, MAX_CLIENTS - 1) < 0) {
        perror("TCP Server - Listen Error");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        // Assigning the connecting client info to the file descriptor
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);

        /**
         * Deciding on whether the server will handle the request or needs to be redirected to the mirror
         * For connections 1 to 4, server will handle itself
         * For connections 5 to 8, requests will be redirected to mirror server
         * For connections from 9, alternate requests will be handled by server
         * and the other alternate ones will be redirected to mirror server
         * 9th -> server, 10th -> mirror, 11th -> server ...
         */
        if ((num_of_clients >= MAX_CLIENTS && num_of_clients < 2*MAX_CLIENTS) || (num_of_clients >= 2*MAX_CLIENTS && num_of_clients % 2 != 0)) {
            // Redirection to the mirror handled here

            // Creating string bearing the port number of the mirror server to be
            // sent to the client for assisting the connection to the mirror
            char mirror_port[PORT_LENGTH];
            sprintf(mirror_port, "%d", MIRROR_PORT);

            // Creating string bearing the ip address and the port number of the mirror server
            // to be sent to the client for assisting the connection to the mirror
            char mirror_address[IP_LENGTH + PORT_LENGTH + 1] = MIRROR_SERVER_IP_ADDR;
            strcat(mirror_address, ":");
            strcat(mirror_address, mirror_port);

            printf("Redirecting client to mirror server.\n");
            // Sending the mirror ip address and port for the client to create
            // new socket connection with the mirror server
            if (send(client_fd, mirror_address, strlen(mirror_address), 0) < 0) {
                perror("TCP Server - Mirror Address Send failed");
                exit(EXIT_FAILURE);
            }
            // Incrementing number of clients to keep track of total clients connected with server and mirror
            // to make the client connection decisions
            num_of_clients++;
        } else {
            // Server itself handles the client connection here

            if (client_fd < 0) {
                perror("TCP Server - Accept Error");
                continue;
            }

            // Acknowledging client socket connection successful
            if (send(client_fd, CONN_SUCCESS, strlen(CONN_SUCCESS), 0) < 0) {
                perror("TCP Server - Connection Acknowledgement Send failed");
                exit(EXIT_FAILURE);
            }

            printf("------ Client connected. ------\n");
            
            // Creating separate specific process for handling
            // single client connection individually
            childpid = fork();
            if (childpid < 0) {
                perror("TCP Server - Fork Error");
                exit(EXIT_FAILURE);
            }
            if (childpid == 0) {
                // Child process handling client connection
                close(server_fd);
                // Handling client requests in the processClient function
                int exit_status = processClient(client_fd);
                if (exit_status == 0) {
                    printf("------ Client Disconnected with Success Code. ------\n");
                    exit(EXIT_SUCCESS);
                } else {
                    printf("------ Client Disconnected with Error Code. ------\n");
                    exit(EXIT_FAILURE);
                }
            } else {
                // Parent process
                num_of_clients++;
                num_of_server_clients++;
                printf("Total Clients connected till now with server and mirror: %d\n", num_of_clients);
                printf("Total Clients connected to the server: %d\n", num_of_server_clients);
                close(client_fd);
                /**
                 * It returns a value greater than 0 if at least one child process has 
                 * changed state, or 0 if no child processes have changed state yet.
                 * -1: Wait for any child process
                 * NULL: No options being set for waitpid
                 * WNOHANG: waitpid must return immediately if no child process has exited yet
                */
                while (waitpid(-1, NULL, WNOHANG) > 0);
            }
        }
    }

    close(server_fd);
    return 0;
}