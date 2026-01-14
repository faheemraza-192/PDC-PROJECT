#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

#define MAX_LINE_LENGTH 2048
#define MAX_PACKAGES 1000
#define MAX_FIELDS 20
#define MAX_STRING_LENGTH 256
#define MAX_THREADS 16

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

// Query parameters (defaults: no filter)
char query_province[128] = "";
char query_category[128] = "";
double query_budget_min = 0.0;
double query_budget_max = 1000000.0;
int query_days = -1;
double query_min_rating = 0.0;
int query_topk = 5;

// Thread data arrays (no structs)
int thread_start[MAX_THREADS];
int thread_end[MAX_THREADS];
int local_topk_indices[MAX_THREADS][MAX_PACKAGES];
double local_topk_scores[MAX_THREADS][MAX_PACKAGES];
int local_topk_counts[MAX_THREADS];

pthread_mutex_t lock;

// Function to parse a line and extract fields (TAB-delimited)
int parse_line(char* line, int package_index) {
    char* token;
    int field_index = 0;
    
    // Skip header line
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

// Thread function: process assigned range and compute local TOPK
void* process_range(void* arg) {
    int thread_id = *(int*)arg;
    int start = thread_start[thread_id];
    int end = thread_end[thread_id];
    
    // Local arrays for this thread's TOPK
    int local_indices[MAX_PACKAGES];
    double local_scores[MAX_PACKAGES];
    int local_count = 0;
    
    // Process assigned range
    for (int i = start; i < end; i++) {
        if (matches_filter(i)) {
            local_indices[local_count] = i;
            local_scores[local_count] = calculate_score(i);
            local_count++;
        }
    }
    
    // Sort local results
    if (local_count > 0) {
        sort_topk(local_indices, local_scores, local_count);
    }
    
    // Store local TOPK (keep top query_topk or all if less)
    int topk_count = (local_count < query_topk) ? local_count : query_topk;
    local_topk_counts[thread_id] = topk_count;
    
    for (int i = 0; i < topk_count; i++) {
        local_topk_indices[thread_id][i] = local_indices[i];
        local_topk_scores[thread_id][i] = local_scores[i];
    }
    
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <dataset_file> <num_threads> [query_string]\n", argv[0]);
        printf("Example: %s dataset.txt 4 PROVINCE=Punjab;CATEGORY=Nature;TOPK=5\n", argv[0]);
        return 1;
    }
    
    int num_threads = atoi(argv[2]);
    if (num_threads < 1 || num_threads > MAX_THREADS) {
        printf("Error: Number of threads must be between 1 and %d\n", MAX_THREADS);
        return 1;
    }
    
    FILE* file = fopen(argv[1], "r");
    if (file == NULL) {
        printf("Error: Cannot open file %s\n", argv[1]);
        return 1;
    }
    
    // Read dataset
    char line[MAX_LINE_LENGTH];
    printf("Loading dataset from %s...\n", argv[1]);
    
    while (fgets(line, MAX_LINE_LENGTH, file) != NULL && total_packages < MAX_PACKAGES) {
        char line_copy[MAX_LINE_LENGTH];
        strcpy(line_copy, line);
        if (parse_line(line_copy, total_packages)) {
            total_packages++;
        }
    }
    
    fclose(file);
    printf("Loaded %d packages.\n", total_packages);
    
    // Parse query
    char query_str[1024] = "";
    if (argc >= 4) {
        strcpy(query_str, argv[3]);
    } else {
        strcpy(query_str, "TOPK=5");
    }
    
    parse_query(query_str);
    printf("\nQuery: %s\n", query_str);
    printf("Using %d threads.\n", num_threads);
    printf("Filters: Province=%s, Category=%s, Budget=[%.0f-%.0f], Days=%d, MinRating=%.1f, TopK=%d\n\n",
           strlen(query_province) > 0 ? query_province : "ANY",
           strlen(query_category) > 0 ? query_category : "ANY",
           query_budget_min, query_budget_max,
           query_days > 0 ? query_days : -1,
           query_min_rating, query_topk);
    
    // Initialize mutex
    pthread_mutex_init(&lock, NULL);
    
    // Start timing
    clock_t start = clock();
    
    // Divide work among threads
    int chunk_size = total_packages / num_threads;
    for (int i = 0; i < num_threads; i++) {
        thread_start[i] = i * chunk_size;
        thread_end[i] = (i == num_threads - 1) ? total_packages : (i + 1) * chunk_size;
        local_topk_counts[i] = 0;
    }
    
    // Create threads
    pthread_t threads[MAX_THREADS];
    int thread_ids[MAX_THREADS];
    
    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, process_range, &thread_ids[i]);
    }
    
    // Wait for all threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Merge local TOPK results into global TOPK
    int global_indices[MAX_PACKAGES];
    double global_scores[MAX_PACKAGES];
    int global_count = 0;
    
    // Collect all local TOPK results
    for (int t = 0; t < num_threads; t++) {
        for (int i = 0; i < local_topk_counts[t]; i++) {
            global_indices[global_count] = local_topk_indices[t][i];
            global_scores[global_count] = local_topk_scores[t][i];
            global_count++;
        }
    }
    
    // Sort global results to get final TOPK
    if (global_count > 0) {
        sort_topk(global_indices, global_scores, global_count);
        
        int topk_count = (global_count < query_topk) ? global_count : query_topk;
        
        // Print TOPK results
        printf("==== TOP %d Recommendations (Pthreads) ====\n", topk_count);
        for (int i = 0; i < topk_count; i++) {
            int idx = global_indices[i];
            printf("%d. %s | %s, %s | Category: %s | Days: %d | Price: %.0f | Rating: %.1f | Score: %.2f\n",
                   i + 1,
                   package_ids[idx],
                   place_names[idx],
                   provinces[idx],
                   categories[idx],
                   duration_days[idx],
                   avg_prices[idx],
                   ratings[idx],
                   global_scores[i]);
        }
    } else {
        printf("No packages match the query filters.\n");
    }
    
    // End timing
    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("\nExecution Time (Pthreads with %d threads): %.4f seconds\n", num_threads, time_taken);
    
    pthread_mutex_destroy(&lock);
    return 0;
}

