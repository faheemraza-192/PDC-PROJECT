#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

#define MAX_LINE_LENGTH 2048
#define MAX_PACKAGES 1000
#define MAX_FIELDS 20
#define BUFFER_SIZE 4096

// Global arrays to store package data (no structs)
char package_ids[MAX_PACKAGES][32];
char place_names[MAX_PACKAGES][256];
char provinces[MAX_PACKAGES][128];
char categories[MAX_PACKAGES][128];
int duration_days[MAX_PACKAGES];
double avg_prices[MAX_PACKAGES];
double ratings[MAX_PACKAGES];
int reviews_counts[MAX_PACKAGES];
double popularity_scores[MAX_PACKAGES];

int total_packages = 0;

// Query parameters
char query_province[128] = "";
char query_category[128] = "";
double query_budget_min = 0.0;
double query_budget_max = 1000000.0;
int query_days = -1;
double query_min_rating = 0.0;
int query_topk = 5;

// Function to parse a line and extract fields (TAB-delimited)
int parse_line(char* line, int package_index) {
    char* token;
    int field_index = 0;
    
    if (strstr(line, "package_id") != NULL) {
        return 0;
    }
    
    token = strtok(line, "\t");
    while (token != NULL && field_index < MAX_FIELDS) {
        char* newline = strchr(token, '\n');
        if (newline) *newline = '\0';
        
        if (field_index == 0) strncpy(package_ids[package_index], token, 31);
        else if (field_index == 1) strncpy(place_names[package_index], token, 255);
        else if (field_index == 2) strncpy(provinces[package_index], token, 127);
        else if (field_index == 5) strncpy(categories[package_index], token, 127);
        else if (field_index == 6) duration_days[package_index] = atoi(token);
        else if (field_index == 8) avg_prices[package_index] = atof(token);
        else if (field_index == 12) ratings[package_index] = atof(token);
        else if (field_index == 13) reviews_counts[package_index] = atoi(token);
        else if (field_index == 14) popularity_scores[package_index] = atof(token);
        
        token = strtok(NULL, "\t");
        field_index++;
    }
    
    return 1;
}

// Function to parse query string
void parse_query(char* query_str) {
    strcpy(query_province, "");
    strcpy(query_category, "");
    query_budget_min = 0.0;
    query_budget_max = 1000000.0;
    query_days = -1;
    query_min_rating = 0.0;
    query_topk = 5;
    
    char* token = strtok(query_str, ";");
    while (token != NULL) {
        if (strncmp(token, "PROVINCE=", 9) == 0) {
            strcpy(query_province, token + 9);
        } else if (strncmp(token, "CATEGORY=", 9) == 0) {
            strcpy(query_category, token + 9);
        } else if (strncmp(token, "BUDGET_MIN=", 11) == 0) {
            query_budget_min = atof(token + 11);
        } else if (strncmp(token, "BUDGET_MAX=", 11) == 0) {
            query_budget_max = atof(token + 11);
        } else if (strncmp(token, "DAYS=", 5) == 0) {
            query_days = atoi(token + 5);
        } else if (strncmp(token, "MIN_RATING=", 11) == 0) {
            query_min_rating = atof(token + 11);
        } else if (strncmp(token, "TOPK=", 5) == 0) {
            query_topk = atoi(token + 5);
        }
        token = strtok(NULL, ";");
    }
}

// Function to check if a package matches the query filters
int matches_filter(int index) {
    if (strlen(query_province) > 0 && strcmp(provinces[index], query_province) != 0) {
        return 0;
    }
    if (strlen(query_category) > 0 && strcmp(categories[index], query_category) != 0) {
        return 0;
    }
    if (avg_prices[index] < query_budget_min || avg_prices[index] > query_budget_max) {
        return 0;
    }
    if (query_days > 0 && duration_days[index] != query_days) {
        return 0;
    }
    if (ratings[index] < query_min_rating) {
        return 0;
    }
    return 1;
}

// Function to calculate score for a package
double calculate_score(int index) {
    double score = 0.0;
    score += 50.0 * ratings[index];
    score += 10.0 * popularity_scores[index];
    score += 5.0 * log(reviews_counts[index] + 1.0);
    
    if (query_budget_max < 1000000.0) {
        double budget_diff = fabs(avg_prices[index] - query_budget_max);
        double budget_closeness = 100.0 / (1.0 + budget_diff / 1000.0);
        score += budget_closeness;
    }
    
    if (query_days > 0) {
        int duration_diff = abs(duration_days[index] - query_days);
        double duration_closeness = 50.0 / (1.0 + duration_diff);
        score += duration_closeness;
    }
    
    return score;
}

// Simple bubble sort for TOPK
void sort_topk(int* indices, double* scores, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (scores[j] < scores[j + 1]) {
                double temp_score = scores[j];
                scores[j] = scores[j + 1];
                scores[j + 1] = temp_score;
                int temp_idx = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = temp_idx;
            }
        }
    }
}

// Function to process query and return results as string
void process_query_and_format(char* query_str, char* response, int response_size) {
    parse_query(query_str);
    
    // Filter and score packages
    int filtered_indices[MAX_PACKAGES];
    double filtered_scores[MAX_PACKAGES];
    int filtered_count = 0;
    
    for (int i = 0; i < total_packages; i++) {
        if (matches_filter(i)) {
            filtered_indices[filtered_count] = i;
            filtered_scores[filtered_count] = calculate_score(i);
            filtered_count++;
        }
    }
    
    // Sort to get TOPK
    if (filtered_count > 0) {
        sort_topk(filtered_indices, filtered_scores, filtered_count);
        int topk_count = (filtered_count < query_topk) ? filtered_count : query_topk;
        
        // Format response
        response[0] = '\0';
        snprintf(response, response_size, "FOUND %d matching packages. TOP %d:\n", filtered_count, topk_count);
        
        for (int i = 0; i < topk_count; i++) {
            int idx = filtered_indices[i];
            char line[512];
            snprintf(line, sizeof(line),
                    "%d. %s | %s, %s | Category: %s | Days: %d | Price: %.0f | Rating: %.1f | Score: %.2f\n",
                    i + 1,
                    package_ids[idx],
                    place_names[idx],
                    provinces[idx],
                    categories[idx],
                    duration_days[idx],
                    avg_prices[idx],
                    ratings[idx],
                    filtered_scores[i]);
            strncat(response, line, response_size - strlen(response) - 1);
        }
    } else {
        snprintf(response, response_size, "No packages match the query filters.\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <port> <dataset_file>\n", argv[0]);
        printf("Example: %s 8080 package_dataset_pakistan.txt\n", argv[0]);
        return 1;
    }
    
    int port = atoi(argv[1]);
    char* dataset_file = argv[2];
    
    // Load dataset
    FILE* file = fopen(dataset_file, "r");
    if (file == NULL) {
        printf("Error: Cannot open file %s\n", dataset_file);
        return 1;
    }
    
    char line[MAX_LINE_LENGTH];
    printf("Loading dataset from %s...\n", dataset_file);
    
    while (fgets(line, MAX_LINE_LENGTH, file) != NULL && total_packages < MAX_PACKAGES) {
        char line_copy[MAX_LINE_LENGTH];
        strcpy(line_copy, line);
        if (parse_line(line_copy, total_packages)) {
            total_packages++;
        }
    }
    
    fclose(file);
    printf("Loaded %d packages.\n", total_packages);
    
    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    // Server address setup
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);
    
    // Bind the socket
    if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(1);
    }
    
    printf("Server running on port %d. Waiting for queries...\n", port);
    
    // Receive queries (UDP)
    while (1) {
        char buffer[BUFFER_SIZE];
        len = sizeof(cliaddr);
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                        (struct sockaddr*)&cliaddr, &len);
        if (n > 0) {
            buffer[n] = '\0';
            printf("Received from client %s:%d: %s\n", 
                   inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), buffer);
            
            // Process query
            char response[BUFFER_SIZE];
            process_query_and_format(buffer, response, BUFFER_SIZE);
            
            // Send response back
            sendto(sockfd, response, strlen(response), 0,
                  (struct sockaddr*)&cliaddr, len);
            printf("Sent response to client.\n\n");
        }
    }
    
    close(sockfd);
    return 0;
}
