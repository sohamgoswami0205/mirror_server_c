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
#include <dirent.h>

#define PORT 8080
// Allowing server to listen to 10 clients at a time
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define TAR_FILE_NAME "server_temp.tar.gz"
#define MAX_FILES 6
#define MAX_FILENAME_LEN 50

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

int find_files(const char *dir_name, const char *filename, char *tar_file) {
    int found = 0;
    DIR *dir;
    struct dirent *entry;
    struct stat file_info;
    char path[PATH_MAX];

    if ((dir = opendir(dir_name)) == NULL) {
        perror("opendir");
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(path, PATH_MAX, "%s/%s", dir_name, entry->d_name);

        if (lstat(path, &file_info) < 0) {
            perror("lstat");
            continue;
        }

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

int find_file_paths(char *root_dir, char *filename, char *tar_file) {
    const char *homedir = getenv("HOME");
    if (homedir == NULL) {
        printf("Could not get HOME directory\n");
        return 0;
    }

    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s/%s", homedir, filename);

    return find_files(homedir, filename, tar_file);
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

    // To send only files and not the entire directory path in tar.gz file
    // strcat(tar_cmd, " --transform \'s#.*/##\' ");

    int file_found = 0;
    for (int i = 0; i < num_files; i++) {
        printf("Filename: %s\n", files[i]);
        char file_path[BUFFER_SIZE];
        snprintf(file_path, BUFFER_SIZE, "%s/%s", dir_path, files[i]);
        
        file_found = find_file_paths(dir_path, files[i], tar_cmd);
    }

    if (file_found) {
        printf("Atleast 1 file found\n");
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

void gettargz_recursive(const char *dir_path, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions, FILE *temp_list) {
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
                    // This file has the matching extension, add it to the list
                    fprintf(temp_list, "%s/%s\n", dir_path, name);
                    break;
                }
            }
        } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // This is a directory, recursively search it
            char subdir_path[BUFFER_SIZE];
            snprintf(subdir_path, sizeof(subdir_path), "%s/%s", dir_path, entry->d_name);
            gettargz_recursive(subdir_path, extensions, num_extensions, temp_list);
        }
    }

    closedir(dir);
}

char* gettargz(int socket_fd, char extensions[MAX_FILES][MAX_FILENAME_LEN], int num_extensions) {
    int file_found = 0;
    // Create a temporary file to store the list of matching files
    FILE *temp_list = tmpfile();
    if (!temp_list) {
        printf("Error: could not create temporary file\n");
        return NULL;
    }

    // Recursively search for files with matching extensions
    gettargz_recursive(getenv("HOME"), extensions, num_extensions, temp_list);
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

void read_filenames(char* buffer, char filenames[MAX_FILES][MAX_FILENAME_LEN], int* num_files, int* unzip_flag) {
    char* token;
    char delim[] = " ";
    int i = 0;

    // Set the unzip flag to 0 by default
    *unzip_flag = 0;

    // Get the first token
    token = strtok(buffer, delim);

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
        } else if (strncmp(buffer, "getfiles", strlen("getfiles")) == 0) {
            char filenames[MAX_FILES][MAX_FILENAME_LEN];
            int num_files, unzip_flag;
            read_filenames(buffer, filenames, &num_files, &unzip_flag);
            result = getfiles(socket_fd, filenames, num_files);
            if (result == NULL) {
                continue;
            }
        } else if (strncmp(buffer, "gettargz", strlen("gettargz")) == 0) {
            char extensions[MAX_FILES][MAX_FILENAME_LEN];
            int num_extensions, unzip_flag;
            read_filenames(buffer, extensions, &num_extensions, &unzip_flag);
            result = gettargz(socket_fd, extensions, num_extensions);
            if (result == NULL) {
                continue;
            }
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