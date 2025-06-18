#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define SERVER_PORT 8080
#define MAX_BUFFER 4096
#define MAX_COMMAND 512

// Function prototypes
int connect_to_server(char *server_ip, int port);
int validate_command(char *command);
void send_command(int socket, char *command);
void receive_response(int socket);
void receive_file(int socket, char *filename);
int is_valid_date(char *date);
int is_valid_size(char *size_str);
int is_valid_extension(char *ext);
void print_usage();
void handle_redirect(int socket);

int main() {
    int client_socket;
    char command[MAX_COMMAND];
    char server_ip[] = "127.0.0.1";
    
    printf("=== File Server Client ===\n");
    printf("Connecting to server at %s:%d\n", server_ip, SERVER_PORT);
    
    // Connect to server
    client_socket = connect_to_server(server_ip, SERVER_PORT);
    if (client_socket < 0) {
        printf("Failed to connect to server\n");
        return 1;
    }
    
    printf("Connected to server successfully!\n");
    print_usage();
    
    while (1) {
        printf("\nEnter command (or 'quit' to exit): ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        // Remove newline character
        command[strcspn(command, "\n")] = 0;
        
        // Skip empty commands
        if (strlen(command) == 0) {
            continue;
        }
        
        // Check for quit command
        if (strcmp(command, "quit") == 0) {
            send_command(client_socket, command);
            break;
        }
        
        // Validate command syntax
        if (!validate_command(command)) {
            printf("Invalid command syntax. Type 'help' for usage information.\n");
            continue;
        }
        
        // Send command to server
        send_command(client_socket, command);
        
        // Handle server response
        char response[MAX_BUFFER];
        int bytes_received = recv(client_socket, response, sizeof(response) - 1, 0);
        
        if (bytes_received <= 0) {
            printf("Connection lost to server\n");
            break;
        }
        
        response[bytes_received] = '\0';
        
        // Check for redirect message
        if (strncmp(response, "REDIRECT", 8) == 0) {
            printf("Server is redirecting to mirror server...\n");
            close(client_socket);
            
            // Parse redirect message
            char mirror_ip[64];
            int mirror_port;
            sscanf(response, "REDIRECT %s %d", mirror_ip, &mirror_port);
            
            // Connect to mirror server
            client_socket = connect_to_server(mirror_ip, mirror_port);
            if (client_socket < 0) {
                printf("Failed to connect to mirror server\n");
                break;
            }
            
            printf("Connected to mirror server at %s:%d\n", mirror_ip, mirror_port);
            
            // Resend the command to mirror server
            send_command(client_socket, command);
            bytes_received = recv(client_socket, response, sizeof(response) - 1, 0);
            if (bytes_received <= 0) {
                printf("Connection lost to mirror server\n");
                break;
            }
            response[bytes_received] = '\0';
        }
        
        // Handle different types of responses
        if (strncmp(command, "findfile", 8) == 0) {
            if (strcmp(response, "File not found") == 0) {
                printf("File not found\n");
            } else {
                printf("File found at: %s\n", response);
            }
        }
        else if (strncmp(command, "getftar", 7) == 0 || 
                 strncmp(command, "sgetfiles", 9) == 0 ||
                 strncmp(command, "dgetfiles", 9) == 0 ||
                 strncmp(command, "getfiles", 8) == 0) {
            
            if (strcmp(response, "No file found") == 0) {
                printf("No files found matching the criteria\n");
            } else if (strncmp(response, "Error", 5) == 0) {
                printf("Server error: %s\n", response);
            } else {
                // Response should be file size
                long file_size = atol(response);
                if (file_size > 0) {
                    printf("Receiving file (%ld bytes)...\n", file_size);
                    
                    // Send acknowledgment
                    send(client_socket, "ACK", 3, 0);
                    
                    // Generate filename based on command
                    char filename[64];
                    if (strncmp(command, "getftar", 7) == 0) {
                        strcpy(filename, "file.tar.gz");
                    } else if (strncmp(command, "sgetfiles", 9) == 0) {
                        strcpy(filename, "sizefiles.tar.gz");
                    } else if (strncmp(command, "dgetfiles", 9) == 0) {
                        strcpy(filename, "datefiles.tar.gz");
                    } else {
                        strcpy(filename, "files.tar.gz");
                    }
                    
                    // Receive the file
                    receive_file(client_socket, filename);
                    printf("File saved as: %s\n", filename);
                } else {
                    printf("Invalid file size received\n");
                }
            }
        }
        else {
            printf("Server response: %s\n", response);
        }
    }
    
    close(client_socket);
    printf("Disconnected from server\n");
    return 0;
}

int connect_to_server(char *server_ip, int port) {
    int client_socket;
    struct sockaddr_in server_addr;
    
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    
    // Connect to server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        return -1;
    }
    
    return client_socket;
}

int validate_command(char *command) {
    if (strncmp(command, "help", 4) == 0) {
        print_usage();
        return 0; // Don't send to server
    }
    
    // findfile <filename>
    if (strncmp(command, "findfile", 8) == 0) {
        char filename[256];
        if (sscanf(command, "findfile %s", filename) == 1) {
            if (strlen(filename) > 0 && strchr(filename, '/') == NULL) {
                return 1;
            }
        }
        printf("Error: findfile syntax is 'findfile <filename>'\n");
        printf("Note: filename should not contain path separators\n");
        return 0;
    }
    
    // sgetfiles <size1> <size2>
    if (strncmp(command, "sgetfiles", 9) == 0) {
        char size1_str[32], size2_str[32];
        if (sscanf(command, "sgetfiles %s %s", size1_str, size2_str) == 2) {
            if (is_valid_size(size1_str) && is_valid_size(size2_str)) {
                long size1 = atol(size1_str);
                long size2 = atol(size2_str);
                if (size1 >= 0 && size2 >= 0 && size1 <= size2) {
                    return 1;
                }
                printf("Error: size1 should be <= size2, and both should be non-negative\n");
                return 0;
            }
        }
        printf("Error: sgetfiles syntax is 'sgetfiles <size1> <size2>'\n");
        printf("Note: sizes should be non-negative integers in bytes\n");
        return 0;
    }
    
    // dgetfiles <date1> <date2>
    if (strncmp(command, "dgetfiles", 9) == 0) {
        char date1[32], date2[32];
        if (sscanf(command, "dgetfiles %s %s", date1, date2) == 2) {
            if (is_valid_date(date1) && is_valid_date(date2)) {
                return 1;
            }
        }
        printf("Error: dgetfiles syntax is 'dgetfiles <date1> <date2>'\n");
        printf("Note: dates should be in YYYY-MM-DD format\n");
        return 0;
    }
    
    // getfiles <extension1> [extension2] ... [extension6]
    if (strncmp(command, "getfiles", 8) == 0) {
        char extensions[6][16];
        int ext_count = 0;
        char *token = strtok(command + 9, " ");
        
        while (token != NULL && ext_count < 6) {
            if (is_valid_extension(token)) {
                strcpy(extensions[ext_count], token);
                ext_count++;
            } else {
                printf("Error: invalid extension '%s'\n", token);
                printf("Note: extensions should be alphanumeric (e.g., txt, pdf, jpg)\n");
                return 0;
            }
            token = strtok(NULL, " ");
        }
        
        if (ext_count >= 1 && ext_count <= 6) {
            return 1;
        }
        printf("Error: getfiles syntax is 'getfiles <ext1> [ext2] ... [ext6]'\n");
        printf("Note: provide 1-6 file extensions\n");
        return 0;
    }
    
    // getftar <filename>
    if (strncmp(command, "getftar", 7) == 0) {
        char filename[256];
        if (sscanf(command, "getftar %s", filename) == 1) {
            if (strlen(filename) > 0 && strchr(filename, '/') == NULL) {
                return 1;
            }
        }
        printf("Error: getftar syntax is 'getftar <filename>'\n");
        printf("Note: filename should not contain path separators\n");
        return 0;
    }
    
    printf("Error: Unknown command '%s'\n", command);
    return 0;
}

void send_command(int socket, char *command) {
    send(socket, command, strlen(command), 0);
}

void receive_file(int socket, char *filename) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        printf("Error: Cannot create file %s\n", filename);
        return;
    }
    
    char buffer[MAX_BUFFER];
    int bytes_received;
    int total_received = 0;
    
    printf("Downloading");
    fflush(stdout);
    
    while ((bytes_received = recv(socket, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
        total_received += bytes_received;
        
        // Print progress dots
        if (total_received % (MAX_BUFFER * 10) == 0) {
            printf(".");
            fflush(stdout);
        }
        
        // Check if we've received all data (this is a simple approach)
        if (bytes_received < MAX_BUFFER) {
            break;
        }
    }
    
    printf(" Complete!\n");
    fclose(file);
}

int is_valid_date(char *date) {
    if (strlen(date) != 10) return 0;
    if (date[4] != '-' || date[7] != '-') return 0;
    
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (date[i] < '0' || date[i] > '9') return 0;
    }
    
    // Basic range checking
    int year, month, day;
    sscanf(date, "%d-%d-%d", &year, &month, &day);
    
    if (year < 1900 || year > 2100) return 0;
    if (month < 1 || month > 12) return 0;
    if (day < 1 || day > 31) return 0;
    
    return 1;
}

int is_valid_size(char *size_str) {
    if (strlen(size_str) == 0) return 0;
    
    for (int i = 0; size_str[i] != '\0'; i++) {
        if (size_str[i] < '0' || size_str[i] > '9') {
            return 0;
        }
    }
    
    return 1;
}

int is_valid_extension(char *ext) {
    if (strlen(ext) == 0 || strlen(ext) > 10) return 0;
    
    for (int i = 0; ext[i] != '\0'; i++) {
        char c = ext[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
            return 0;
        }
    }
    
    return 1;
}

void print_usage() {
    printf("\n=== Available Commands ===\n");
    printf("findfile <filename>              - Find a file by name\n");
    printf("sgetfiles <size1> <size2>        - Get files within size range (bytes)\n");
    printf("dgetfiles <date1> <date2>        - Get files within date range (YYYY-MM-DD)\n");
    printf("getfiles <ext1> [ext2] ... [ext6] - Get files by extensions (1-6 extensions)\n");
    printf("getftar <filename>               - Get a specific file as tar\n");
    printf("quit                             - Exit the client\n");
    printf("help                             - Show this help message\n");
    printf("\nExamples:\n");
    printf("  findfile document.txt\n");
    printf("  sgetfiles 1024 10485760\n");
    printf("  dgetfiles 2023-01-01 2023-12-31\n");
    printf("  getfiles txt pdf\n");
    printf("  getftar config.conf\n");
    printf("==========================\n");
}