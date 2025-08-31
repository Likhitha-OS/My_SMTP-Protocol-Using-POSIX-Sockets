/*
===================================== 
Assignment 6 Submission 
Name: NAGALLA DEVISRI PRASAD
Roll number: 22CS10045 
=====================================
*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 1024    //For receiving and sending data
#define MAX_EMAIL_SIZE 4096 //Maximum size for email storage

// Function prototypes
int connect_to_server(const char *address, int port);
void send_command(int socket, const char *command);
int receive_response(int socket, char *response, int size);
void send_email(int socket);
void list_emails(int socket);
void get_email(int socket);
void handle_command(int socket, const char *command);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server_address> <port>\n", argv[0]);
        return 1;
    }

    const char *server_address = argv[1];
    int port = atoi(argv[2]);
    
    int socket_fd = connect_to_server(server_address, port);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to connect to server\n");
        return 1;
    }
    
    printf("Connected to My_SMTP server.\n");
    
    // Receive welcome message
    char response[BUFFER_SIZE];
    receive_response(socket_fd, response, BUFFER_SIZE);
    
    // Main command loop
    char command[BUFFER_SIZE];
    while (1) {
        printf("> ");
        if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        
        // Remove trailing newline
        command[strcspn(command, "\n")] = '\0';
        
        // Exit on QUIT
        if (strcmp(command, "QUIT") == 0) {
            send_command(socket_fd, "QUIT");
            receive_response(socket_fd, response, BUFFER_SIZE);
            break;
        }
        
        handle_command(socket_fd, command);
    }
    
    close(socket_fd);
    return 0;
}

int connect_to_server(const char *address, int port) {
    int socket_fd;
    struct sockaddr_in server_addr;
    
    // Create socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    // Initialize server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Convert address
    if (inet_pton(AF_INET, address, &server_addr.sin_addr) <= 0) {
        // Try to resolve as hostname
        struct hostent *he = gethostbyname(address);
        if (he == NULL) {
            perror("Invalid address / Address not supported");
            close(socket_fd);
            return -1;
        }
        memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    // Connect to server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(socket_fd);
        return -1;
    }
    
    return socket_fd;
}

//Sends a command to the server
void send_command(int socket, const char *command) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%s\r\n", command);
    send(socket, buffer, strlen(buffer), 0);
}

//Receives a response from the server
int receive_response(int socket, char *response, int size) {
    memset(response, 0, size);
    ssize_t bytes_received = recv(socket, response, size - 1, 0);
    
    if (bytes_received <= 0) {
        return -1;  //Connection closed or error
    }
    
    response[bytes_received] = '\0';
    printf("%s", response);
    
    // Return response code
    int code;
    sscanf(response, "%d", &code);  //Extract response code
    return code;
}

void handle_command(int socket, const char *command) {
    char response[BUFFER_SIZE];
    
    if (strncmp(command, "DATA", 4) == 0) {
        send_command(socket, command);
        if (receive_response(socket, response, BUFFER_SIZE) == 200) {
            // Start entering email content
            printf("Enter your message (end with a single dot '.'):\n");
            
            char line[BUFFER_SIZE];
            
            while (1) {
                if (fgets(line, BUFFER_SIZE, stdin) == NULL) {
                    break;
                }
                
                // Remove trailing newline for comparison
                line[strcspn(line, "\n")] = '\0';
                
                // Check for end of message (single dot)
                if (strcmp(line, ".") == 0) {
                    send(socket, ".\r\n", 3, 0);
                    break;
                }
                
                // Restore newline for sending
                strcat(line, "\r\n");
                send(socket, line, strlen(line), 0);
            }
            
            // Get final response after email body
            receive_response(socket, response, BUFFER_SIZE);
        }
    } 
    else if (strncmp(command, "LIST", 4) == 0 || strncmp(command, "GET_MAIL", 8) == 0) {
        // Send command to server
        send_command(socket, command);
        
        // Receive potentially large response
        char large_response[MAX_EMAIL_SIZE];
        memset(large_response, 0, MAX_EMAIL_SIZE);
        
        ssize_t total_bytes = 0;
        ssize_t bytes_received;
        
        // Receive response in chunks to handle large responses
        while ((bytes_received = recv(socket, large_response + total_bytes, MAX_EMAIL_SIZE - total_bytes - 1, 0)) > 0) {
            total_bytes += bytes_received;
            large_response[total_bytes] = '\0';
            
            // Check if we've received the entire response
            if (strstr(large_response, "\r\n") != NULL) {
                break;
            }
        }
        
        if (bytes_received < 0) {
            perror("Error receiving response");
        } else {
            printf("%s", large_response);
        }
    }
    else {
        // Regular command
        send_command(socket, command);
        receive_response(socket, response, BUFFER_SIZE);
    }
}