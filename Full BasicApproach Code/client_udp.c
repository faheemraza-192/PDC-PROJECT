#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096

// If user types only digits (like "3"), treat it as TOPK=3
int is_only_number(const char *s) {
    if (s == NULL || s[0] == '\0') return 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
    }
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server_port> [server_ip]\n", argv[0]);
        printf("Example (same PC): %s 8080\n", argv[0]);
        printf("Example (other PC): %s 8080 192.168.1.10\n", argv[0]);
        return 1;
    }

    int server_port = atoi(argv[1]);

    // Default IP so you don't need to type it on same machine
    char server_ip[64];
    if (argc >= 3) strcpy(server_ip, argv[2]);
    else strcpy(server_ip, "127.0.0.1");

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Server address setup
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        printf("Invalid IP address: %s\n", server_ip);
        close(sockfd);
        return 1;
    }

    // Read query from user
    char query[1024];
    printf("Enter query (or press Enter for default TOPK=5):\n> ");
    fflush(stdout);

    if (fgets(query, sizeof(query), stdin) == NULL) {
        strcpy(query, "TOPK=5");
    }

    // Remove newline
    char *nl = strchr(query, '\n');
    if (nl) *nl = '\0';

    // If empty => default
    if (strlen(query) == 0) {
        strcpy(query, "TOPK=5");
    }

    // ✅ If user typed only a number => convert to TOPK=number
    if (is_only_number(query)) {
        char fixed[1024];
        snprintf(fixed, sizeof(fixed), "TOPK=%s", query);
        strcpy(query, fixed);
    }

    printf("\nSending query to %s:%d\nQuery: %s\n\n", server_ip, server_port, query);

    // Send query
    sendto(sockfd, query, strlen(query), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));

    // Receive response
    char buffer[BUFFER_SIZE];
    socklen_t len = sizeof(servaddr);
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&servaddr, &len);

    if (n > 0) {
        buffer[n] = '\0';
        printf("==== Server Response ====\n%s", buffer);
    } else {
        printf("No response received.\n");
    }

    close(sockfd);
    return 0;
}
