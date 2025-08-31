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
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <netdb.h>

#define BUFFER_SIZE 1024
#define PATH_MAX_SIZE 2048  // Increased buffer size for paths
#define MAX_EMAIL_SIZE 4096
#define MAX_CLIENTS 10

// Response codes
#define OK "200 OK\r\n"
#define ERR "400 ERR\r\n"
#define NOT_FOUND "401 NOT FOUND\r\n"
#define FORBIDDEN "403 FORBIDDEN\r\n"
#define SERVER_ERROR "500 SERVER ERROR\r\n"

typedef struct {
    int socket;
    struct sockaddr_in addr;
} client_t;

// Function prototypes
void *handle_client(void *arg);
void send_response(int socket, const char *response);
int process_helo(int socket, char *buffer);
int process_mail_from(int socket, char *buffer, char *sender);
int process_rcpt_to(int socket, char *buffer, char *recipient);
int process_data(int socket, char *sender, char *recipient);
int process_list(int socket, char *buffer);
int process_get_mail(int socket, char *buffer);
void create_directory_if_not_exists(const char *path);
void get_current_date(char *date_str);
int get_next_email_id(const char *recipient);

void print_local_ip_addresses() {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }

    printf("Available IP addresses for clients to connect to:\n");
    
    // Walk through linked list, maintaining head pointer
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        // Display only IPv4 addresses (skip loopback 127.0.0.1)
        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr,
                    sizeof(struct sockaddr_in),
                    host, NI_MAXHOST,
                    NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                continue;
            }
            
            // Skip localhost/loopback address
            if (strcmp(host, "127.0.0.1") != 0) {
                printf("  %s: %s\n", ifa->ifa_name, host);
            }
        }
    }

    freeifaddrs(ifaddr);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }
    // if(argc!=3){
    //     printf("Usage: %s <port>\n", argv[0]);
    //     return 1;
    // }

    int port = atoi(argv[1]);
    //int addr = atoi(argv[2]);
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    // Create mailbox directory
    create_directory_if_not_exists("mailbox");

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket to address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d...\n", port);
    print_local_ip_addresses();

    // Accept connections and create threads to handle each client
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));

        client_t *client = malloc(sizeof(client_t));
        client->socket = client_socket;
        client->addr = client_addr;

        if (pthread_create(&thread_id, NULL, handle_client, (void *)client) < 0) {
            perror("Thread creation failed");
            close(client_socket);
            free(client);
        } else {
            pthread_detach(thread_id); // Detach thread to automatically reclaim resources when it terminates
        }
    }

    close(server_socket);
    return 0;
}

void *handle_client(void *arg) {
    client_t *client = (client_t *)arg;
    int socket = client->socket;
    char buffer[BUFFER_SIZE];
    char sender[BUFFER_SIZE] = "";
    char recipient[BUFFER_SIZE] = "";
    int is_quit = 0;

    // Send welcome message
    send_response(socket, OK);

    while (!is_quit) {
        memset(buffer, 0, BUFFER_SIZE);
        
        // Receive command from client
        ssize_t bytes_received = recv(socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("Client disconnected unexpectedly\n");
            break;
        }
        
        buffer[bytes_received] = '\0';
        
        // Remove trailing newline characters
        char *end = buffer + bytes_received - 1;
        while (end >= buffer && (*end == '\n' || *end == '\r')) {
            *end = '\0';
            end--;
        }
        
        printf("Received: %s\n", buffer);
        
        // Process command
        if (strncmp(buffer, "HELO", 4) == 0) {
            process_helo(socket, buffer);
        } else if (strncmp(buffer, "MAIL FROM:", 10) == 0) {
            process_mail_from(socket, buffer, sender);
        } else if (strncmp(buffer, "RCPT TO:", 8) == 0) {
            process_rcpt_to(socket, buffer, recipient);
        } else if (strcmp(buffer, "DATA") == 0) {
            process_data(socket, sender, recipient);
            // Reset sender and recipient after sending email
            memset(sender, 0, BUFFER_SIZE);
            memset(recipient, 0, BUFFER_SIZE);
        } else if (strncmp(buffer, "LIST", 4) == 0) {
            process_list(socket, buffer);
        } else if (strncmp(buffer, "GET_MAIL", 8) == 0) {
            process_get_mail(socket, buffer);
        } else if (strcmp(buffer, "QUIT") == 0) {
            send_response(socket, "200 Goodbye\r\n");
            is_quit = 1;
        } else {
            send_response(socket, ERR);
        }
    }

    printf("Client disconnected\n");
    close(socket);
    free(client);
    return NULL;
}

void send_response(int socket, const char *response) {
    send(socket, response, strlen(response), 0);
}

int process_helo(int socket, char *buffer) {
    char domain[BUFFER_SIZE];
    if (sscanf(buffer, "HELO %s", domain) == 1) {
        printf("HELO received from %s\n", domain);
        send_response(socket, OK);
        return 0;
    } else {
        send_response(socket, ERR);
        return -1;
    }
}

int process_mail_from(int socket, char *buffer, char *sender) {
    // Extract email from "MAIL FROM: <email>"
    // Allow flexible whitespace after the colon
    char *email_start = strstr(buffer, "FROM:");
    if (email_start) {
        email_start += 5;  // Skip "FROM:"
        
        // Skip leading whitespace
        while (*email_start == ' ' || *email_start == '\t') {
            email_start++;
        }
        
        // Copy email address
        strcpy(sender, email_start);
        printf("MAIL FROM: %s\n", sender);
        send_response(socket, OK);
        return 0;
    } else {
        send_response(socket, ERR);
        return -1;
    }
}

int process_rcpt_to(int socket, char *buffer, char *recipient) {
    // Extract email from "RCPT TO: <email>"
    // Allow flexible whitespace after the colon
    char *email_start = strstr(buffer, "TO:");
    if (email_start) {
        email_start += 3;  // Skip "TO:"
        
        // Skip leading whitespace
        while (*email_start == ' ' || *email_start == '\t') {
            email_start++;
        }
        
        // Copy email address
        strcpy(recipient, email_start);
        printf("RCPT TO: %s\n", recipient);
        send_response(socket, OK);
        return 0;
    } else {
        send_response(socket, ERR);
        return -1;
    }
}

int process_data(int socket, char *sender, char *recipient) {
    char buffer[BUFFER_SIZE];
    char email_content[MAX_EMAIL_SIZE] = "";
    char date_str[32];
    
    // Check if sender and recipient are set
    if (strlen(sender) == 0 || strlen(recipient) == 0) {
        send_response(socket, FORBIDDEN);
        return -1;
    }
    
    // Send OK to start receiving data
    send_response(socket, OK);
    printf("DATA received\n");
    
    // Get current date
    get_current_date(date_str);
    
    // Prepare email header
    int content_len = snprintf(email_content, MAX_EMAIL_SIZE, "From: %s\nDate: %s\n", sender, date_str);
    if (content_len < 0 || content_len >= MAX_EMAIL_SIZE) {
        send_response(socket, SERVER_ERROR);
        return -1;
    }
    
    // Receive email body line by line until a single dot is encountered
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recv(socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            return -1;
        }
        
        buffer[bytes_received] = '\0';
        
        // Check for the end of data marker (a line with just a dot)
        if (strcmp(buffer, ".\r\n") == 0) {
            break;
        }
        
        // Append line to email content safely
        size_t remaining = MAX_EMAIL_SIZE - strlen(email_content) - 1;
        if (remaining > 0) {
            strncat(email_content, buffer, remaining);
        } else {
            // Email content is full, truncate the rest
            printf("Warning: Email content truncated\n");
            break;
        }
    }
    
    // Create recipient directory if it doesn't exist
    char recipient_dir[PATH_MAX_SIZE];
    if (snprintf(recipient_dir, PATH_MAX_SIZE, "mailbox/%s", recipient) >= PATH_MAX_SIZE) {
        printf("Error: Recipient path too long\n");
        send_response(socket, SERVER_ERROR);
        return -1;
    }
    
    create_directory_if_not_exists(recipient_dir);
    
    // Get the next email ID for this recipient
    int email_id = get_next_email_id(recipient);
    
    // Create email file
    char email_path[PATH_MAX_SIZE];
    if (snprintf(email_path, PATH_MAX_SIZE, "mailbox/%s/%d.txt", recipient, email_id) >= PATH_MAX_SIZE) {
        printf("Error: Email path too long\n");
        send_response(socket, SERVER_ERROR);
        return -1;
    }
    
    FILE *email_file = fopen(email_path, "w");
    if (email_file == NULL) {
        perror("Failed to create email file");
        send_response(socket, SERVER_ERROR);
        return -1;
    }
    
    fprintf(email_file, "%s", email_content);
    fclose(email_file);
    
    printf("Message stored successfully\n");
    send_response(socket, "200 Message stored successfully\r\n");
    return 0;
}

int process_list(int socket, char *buffer) {
    char email_addr[BUFFER_SIZE];
    char response[MAX_EMAIL_SIZE] = "200 OK\r\n";
    
    // Extract email from "LIST <email>"
    if (sscanf(buffer, "LIST %s", email_addr) != 1) {
        send_response(socket, ERR);
        return -1;
    }
    
    printf("LIST %s\n", email_addr);
    
    char mailbox_dir[PATH_MAX_SIZE];
    if (snprintf(mailbox_dir, PATH_MAX_SIZE, "mailbox/%s", email_addr) >= PATH_MAX_SIZE) {
        printf("Error: Mailbox path too long\n");
        send_response(socket, SERVER_ERROR);
        return -1;
    }
    
    DIR *dir = opendir(mailbox_dir);
    if (dir == NULL) {
        if (errno == ENOENT) {
            // Directory doesn't exist - no emails
            send_response(socket, OK);
            return 0;
        } else {
            perror("Failed to open mailbox directory");
            send_response(socket, SERVER_ERROR);
            return -1;
        }
    }
    
    struct dirent *entry;
    int count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".txt") != NULL) {
            char email_path[PATH_MAX_SIZE];
            if (snprintf(email_path, PATH_MAX_SIZE, "%s/%s", mailbox_dir, entry->d_name) >= PATH_MAX_SIZE) {
                printf("Warning: Email path too long, skipping\n");
                continue;
            }
            
            FILE *email_file = fopen(email_path, "r");
            if (email_file == NULL) {
                continue;
            }
            
            char line[BUFFER_SIZE];
            char from[BUFFER_SIZE] = "";
            char date[BUFFER_SIZE] = "";
            
            // Read first two lines to get From and Date
            if (fgets(line, BUFFER_SIZE, email_file) != NULL) {
                sscanf(line, "From: %s", from);
            }
            
            if (fgets(line, BUFFER_SIZE, email_file) != NULL) {
                sscanf(line, "Date: %s", date);
            }
            
            fclose(email_file);
            
            int id;
            sscanf(entry->d_name, "%d.txt", &id);
            
            // Safely append to response
            size_t remaining = MAX_EMAIL_SIZE - strlen(response) - 1;
            char email_info[BUFFER_SIZE];
            
            int info_len = snprintf(email_info, BUFFER_SIZE, "%d: Email from %s (%s)\r\n", id, from, date);
            if (info_len >= 0 && info_len < BUFFER_SIZE && remaining > (size_t)info_len) {
                strcat(response, email_info);
            } else {
                // Response is getting too large
                printf("Warning: Response buffer full, truncating email list\n");
                break;
            }
            
            count++;
        }
    }
    
    closedir(dir);
    
    if (count == 0) {
        size_t remaining = MAX_EMAIL_SIZE - strlen(response) - 1;
        if (remaining > 16) { // Length of "No emails found\r\n"
            strcat(response, "No emails found\r\n");
        }
    }
    
    send_response(socket, response);
    printf("Emails retrieved; list sent.\n");
    return 0;
}

int process_get_mail(int socket, char *buffer) {
    char email_addr[BUFFER_SIZE];
    int email_id;
    
    // Extract email and id from "GET_MAIL <email> <id>"
    if (sscanf(buffer, "GET_MAIL %s %d", email_addr, &email_id) != 2) {
        send_response(socket, ERR);
        return -1;
    }
    
    printf("GET_MAIL %s %d\n", email_addr, email_id);
    
    char email_path[PATH_MAX_SIZE];
    if (snprintf(email_path, PATH_MAX_SIZE, "mailbox/%s/%d.txt", email_addr, email_id) >= PATH_MAX_SIZE) {
        printf("Error: Email path too long\n");
        send_response(socket, SERVER_ERROR);
        return -1;
    }
    
    FILE *email_file = fopen(email_path, "r");
    if (email_file == NULL) {
        send_response(socket, NOT_FOUND);
        return -1;
    }
    
    char response[MAX_EMAIL_SIZE];
    strcpy(response, "200 OK\r\n");
    
    char line[BUFFER_SIZE];
    size_t response_len = strlen(response);
    
    while (fgets(line, BUFFER_SIZE, email_file) != NULL) {
        size_t line_len = strlen(line);
        if (response_len + line_len < MAX_EMAIL_SIZE - 1) {
            strcat(response, line);
            response_len += line_len;
        } else {
            printf("Warning: Email content truncated in response\n");
            break;
        }
    }
    
    fclose(email_file);
    
    send_response(socket, response);
    printf("Email with id %d sent.\n", email_id);
    return 0;
}

void create_directory_if_not_exists(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

void get_current_date(char *date_str) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(date_str, 32, "%d-%m-%Y", t);
}

int get_next_email_id(const char *recipient) {
    char mailbox_dir[PATH_MAX_SIZE];
    if (snprintf(mailbox_dir, PATH_MAX_SIZE, "mailbox/%s", recipient) >= PATH_MAX_SIZE) {
        printf("Error: Recipient path too long\n");
        return 1; // Fallback to ID 1
    }
    
    DIR *dir = opendir(mailbox_dir);
    if (dir == NULL) {
        return 1; // If directory doesn't exist, start with ID 1
    }
    
    struct dirent *entry;
    int max_id = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".txt") != NULL) {
            int id;
            if (sscanf(entry->d_name, "%d.txt", &id) == 1) {
                if (id > max_id) {
                    max_id = id;
                }
            }
        }
    }
    
    closedir(dir);
    return max_id + 1;
}
