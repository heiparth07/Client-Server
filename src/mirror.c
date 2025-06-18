#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#define MIRROR_PORT 8081
#define MAX_BUFFER 4096
#define MAX_PATH 1024
#define MAX_FILES 1000

// Function prototypes - same as server
void handle_client(int client_socket);
void find_files(int client_socket, char *filename);
void get_files_by_size(int client_socket, long size1, long size2);
void get_files_by_date(int client_socket, char *date1, char *date2);
void get_file_tar(int client_socket, char *filename);
void get_files_by_extension(int client_socket, char **extensions, int ext_count);
void send_tar_file(int client_socket, char *tar_filename);
void search_directory(char *dir_path, char *filename, char *result_path);
void search_files_by_size(char *dir_path, long size1, long size2, char files[][MAX_PATH], int *count);
void search_files_by_date(char *dir_path, char *date1, char *date2, char files[][MAX_PATH], int *count);
void search_files_by_extension(char *dir_path, char **extensions, int ext_count, char files[][MAX_PATH], int *count);
int is_valid_date(char *date);
long convert_date_to_timestamp(char *date);

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    printf("Starting mirror server on port %d...\n", MIRROR_PORT);
    
    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(MIRROR_PORT);
    
    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Mirror server listening on port %d\n", MIRROR_PORT);
    
    while (1) {
        printf("Mirror waiting for connections...\n");
        
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        printf("Mirror server: Connection established\n");
        
        // Fork a child process to handle the client
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(server_fd);
            handle_client(client_socket);
            close(client_socket);
            exit(0);
        } else if (pid > 0) {
            // Parent process
            close(client_socket);
        } else {
            perror("fork failed");
        }
        
        // Clean up zombie processes
        while (waitpid(-1, NULL, WNOHANG) > 0);
    }
    
    return 0;
}

void handle_client(int client_socket) {
    char buffer[MAX_BUFFER];
    char command[256];
    
    while (1) {
        memset(buffer, 0, MAX_BUFFER);
        int bytes_received = recv(client_socket, buffer, MAX_BUFFER - 1, 0);
        
        if (bytes_received <= 0) {
            printf("Mirror: Client disconnected\n");
            break;
        }
        
        buffer[bytes_received] = '\0';
        printf("Mirror received command: %s\n", buffer);
        
        // Parse command - same logic as main server
        if (strncmp(buffer, "findfile", 8) == 0) {
            char filename[256];
            if (sscanf(buffer, "findfile %s", filename) == 1) {
                find_files(client_socket, filename);
            } else {
                send(client_socket, "Invalid findfile syntax", 23, 0);
            }
        }
        else if (strncmp(buffer, "sgetfiles", 9) == 0) {
            long size1, size2;
            if (sscanf(buffer, "sgetfiles %ld %ld", &size1, &size2) == 2) {
                if (size1 <= size2) {
                    get_files_by_size(client_socket, size1, size2);
                } else {
                    send(client_socket, "Invalid size range", 18, 0);
                }
            } else {
                send(client_socket, "Invalid sgetfiles syntax", 24, 0);
            }
        }
        else if (strncmp(buffer, "dgetfiles", 9) == 0) {
            char date1[32], date2[32];
            if (sscanf(buffer, "dgetfiles %s %s", date1, date2) == 2) {
                if (is_valid_date(date1) && is_valid_date(date2)) {
                    get_files_by_date(client_socket, date1, date2);
                } else {
                    send(client_socket, "Invalid date format", 19, 0);
                }
            } else {
                send(client_socket, "Invalid dgetfiles syntax", 24, 0);
            }
        }
        else if (strncmp(buffer, "getfiles", 8) == 0) {
            char extensions[6][16];
            int ext_count = 0;
            char *token = strtok(buffer + 9, " ");
            
            while (token != NULL && ext_count < 6) {
                strcpy(extensions[ext_count], token);
                ext_count++;
                token = strtok(NULL, " ");
            }
            
            if (ext_count >= 1 && ext_count <= 6) {
                char *ext_ptrs[6];
                for (int i = 0; i < ext_count; i++) {
                    ext_ptrs[i] = extensions[i];
                }
                get_files_by_extension(client_socket, ext_ptrs, ext_count);
            } else {
                send(client_socket, "Invalid getfiles syntax", 23, 0);
            }
        }
        else if (strncmp(buffer, "getftar", 7) == 0) {
            char filename[256];
            if (sscanf(buffer, "getftar %s", filename) == 1) {
                get_file_tar(client_socket, filename);
            } else {
                send(client_socket, "Invalid getftar syntax", 22, 0);
            }
        }
        else if (strncmp(buffer, "quit", 4) == 0) {
            printf("Mirror: Client requested to quit\n");
            break;
        }
        else {
            send(client_socket, "Unknown command", 15, 0);
        }
    }
}

// All other functions are identical to server.c
void find_files(int client_socket, char *filename) {
    char result_path[MAX_PATH] = {0};
    char home_path[MAX_PATH];
    
    snprintf(home_path, sizeof(home_path), "%s", getenv("HOME"));
    
    search_directory(home_path, filename, result_path);
    
    if (strlen(result_path) > 0) {
        send(client_socket, result_path, strlen(result_path), 0);
    } else {
        send(client_socket, "File not found", 14, 0);
    }
}

void search_directory(char *dir_path, char *filename, char *result_path) {
    DIR *dir;
    struct dirent *entry;
    char full_path[MAX_PATH];
    struct stat file_stat;
    
    dir = opendir(dir_path);
    if (dir == NULL) {
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        if (stat(full_path, &file_stat) == 0) {
            if (S_ISREG(file_stat.st_mode) && strcmp(entry->d_name, filename) == 0) {
                strcpy(result_path, full_path);
                closedir(dir);
                return;
            } else if (S_ISDIR(file_stat.st_mode)) {
                search_directory(full_path, filename, result_path);
                if (strlen(result_path) > 0) {
                    closedir(dir);
                    return;
                }
            }
        }
    }
    
    closedir(dir);
}

void get_files_by_size(int client_socket, long size1, long size2) {
    char files[MAX_FILES][MAX_PATH];
    int count = 0;
    char home_path[MAX_PATH];
    
    snprintf(home_path, sizeof(home_path), "%s", getenv("HOME"));
    
    search_files_by_size(home_path, size1, size2, files, &count);
    
    if (count > 0) {
        char tar_command[MAX_BUFFER];
        char tar_filename[] = "/tmp/mirror_temp.tar.gz";
        
        snprintf(tar_command, sizeof(tar_command), "tar -czf %s", tar_filename);
        
        for (int i = 0; i < count; i++) {
            strcat(tar_command, " \"");
            strcat(tar_command, files[i]);
            strcat(tar_command, "\"");
        }
        
        system(tar_command);
        send_tar_file(client_socket, tar_filename);
        unlink(tar_filename);
    } else {
        send(client_socket, "No file found", 13, 0);
    }
}

void search_files_by_size(char *dir_path, long size1, long size2, char files[][MAX_PATH], int *count) {
    DIR *dir;
    struct dirent *entry;
    char full_path[MAX_PATH];
    struct stat file_stat;
    
    if (*count >= MAX_FILES) return;
    
    dir = opendir(dir_path);
    if (dir == NULL) {
        return;
    }
    
    while ((entry = readdir(dir)) != NULL && *count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        if (stat(full_path, &file_stat) == 0) {
            if (S_ISREG(file_stat.st_mode)) {
                if (file_stat.st_size >= size1 && file_stat.st_size <= size2) {
                    strcpy(files[*count], full_path);
                    (*count)++;
                }
            } else if (S_ISDIR(file_stat.st_mode)) {
                search_files_by_size(full_path, size1, size2, files, count);
            }
        }
    }
    
    closedir(dir);
}

void get_files_by_date(int client_socket, char *date1, char *date2) {
    char files[MAX_FILES][MAX_PATH];
    int count = 0;
    char home_path[MAX_PATH];
    
    snprintf(home_path, sizeof(home_path), "%s", getenv("HOME"));
    
    search_files_by_date(home_path, date1, date2, files, &count);
    
    if (count > 0) {
        char tar_command[MAX_BUFFER];
        char tar_filename[] = "/tmp/mirror_temp.tar.gz";
        
        snprintf(tar_command, sizeof(tar_command), "tar -czf %s", tar_filename);
        
        for (int i = 0; i < count; i++) {
            strcat(tar_command, " \"");
            strcat(tar_command, files[i]);
            strcat(tar_command, "\"");
        }
        
        system(tar_command);
        send_tar_file(client_socket, tar_filename);
        unlink(tar_filename);
    } else {
        send(client_socket, "No file found", 13, 0);
    }
}

void search_files_by_date(char *dir_path, char *date1, char *date2, char files[][MAX_PATH], int *count) {
    DIR *dir;
    struct dirent *entry;
    char full_path[MAX_PATH];
    struct stat file_stat;
    long timestamp1 = convert_date_to_timestamp(date1);
    long timestamp2 = convert_date_to_timestamp(date2);
    
    if (*count >= MAX_FILES) return;
    
    dir = opendir(dir_path);
    if (dir == NULL) {
        return;
    }
    
    while ((entry = readdir(dir)) != NULL && *count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        if (stat(full_path, &file_stat) == 0) {
            if (S_ISREG(file_stat.st_mode)) {
                if (file_stat.st_mtime >= timestamp1 && file_stat.st_mtime <= timestamp2) {
                    strcpy(files[*count], full_path);
                    (*count)++;
                }
            } else if (S_ISDIR(file_stat.st_mode)) {
                search_files_by_date(full_path, date1, date2, files, count);
            }
        }
    }
    
    closedir(dir);
}

void get_file_tar(int client_socket, char *filename) {
    char result_path[MAX_PATH] = {0};
    char home_path[MAX_PATH];
    
    snprintf(home_path, sizeof(home_path), "%s", getenv("HOME"));
    
    search_directory(home_path, filename, result_path);
    
    if (strlen(result_path) > 0) {
        char tar_command[MAX_BUFFER];
        char tar_filename[] = "/tmp/mirror_temp.tar.gz";
        
        snprintf(tar_command, sizeof(tar_command), "tar -czf %s \"%s\"", tar_filename, result_path);
        system(tar_command);
        
        send_tar_file(client_socket, tar_filename);
        unlink(tar_filename);
    } else {
        send(client_socket, "No file found", 13, 0);
    }
}

void get_files_by_extension(int client_socket, char **extensions, int ext_count) {
    char files[MAX_FILES][MAX_PATH];
    int count = 0;
    char home_path[MAX_PATH];
    
    snprintf(home_path, sizeof(home_path), "%s", getenv("HOME"));
    
    search_files_by_extension(home_path, extensions, ext_count, files, &count);
    
    if (count > 0) {
        char tar_command[MAX_BUFFER];
        char tar_filename[] = "/tmp/mirror_temp.tar.gz";
        
        snprintf(tar_command, sizeof(tar_command), "tar -czf %s", tar_filename);
        
        for (int i = 0; i < count; i++) {
            strcat(tar_command, " \"");
            strcat(tar_command, files[i]);
            strcat(tar_command, "\"");
        }
        
        system(tar_command);
        send_tar_file(client_socket, tar_filename);
        unlink(tar_filename);
    } else {
        send(client_socket, "No file found", 13, 0);
    }
}

void search_files_by_extension(char *dir_path, char **extensions, int ext_count, char files[][MAX_PATH], int *count) {
    DIR *dir;
    struct dirent *entry;
    char full_path[MAX_PATH];
    struct stat file_stat;
    
    if (*count >= MAX_FILES) return;
    
    dir = opendir(dir_path);
    if (dir == NULL) {
        return;
    }
    
    while ((entry = readdir(dir)) != NULL && *count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        if (stat(full_path, &file_stat) == 0) {
            if (S_ISREG(file_stat.st_mode)) {
                char *dot = strrchr(entry->d_name, '.');
                if (dot != NULL) {
                    for (int i = 0; i < ext_count; i++) {
                        if (strcmp(dot + 1, extensions[i]) == 0) {
                            strcpy(files[*count], full_path);
                            (*count)++;
                            break;
                        }
                    }
                }
            } else if (S_ISDIR(file_stat.st_mode)) {
                search_files_by_extension(full_path, extensions, ext_count, files, count);
            }
        }
    }
    
    closedir(dir);
}

void send_tar_file(int client_socket, char *tar_filename) {
    FILE *file = fopen(tar_filename, "rb");
    if (file == NULL) {
        send(client_socket, "Error creating tar file", 23, 0);
        return;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char size_str[32];
    snprintf(size_str, sizeof(size_str), "%ld", file_size);
    send(client_socket, size_str, strlen(size_str), 0);
    
    char ack[10];
    recv(client_socket, ack, sizeof(ack), 0);
    
    char buffer[MAX_BUFFER];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, MAX_BUFFER, file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
    
    fclose(file);
}

int is_valid_date(char *date) {
    if (strlen(date) != 10) return 0;
    if (date[4] != '-' || date[7] != '-') return 0;
    
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (date[i] < '0' || date[i] > '9') return 0;
    }
    
    return 1;
}

long convert_date_to_timestamp(char *date) {
    struct tm tm = {0};
    sscanf(date, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return mktime(&tm);
}