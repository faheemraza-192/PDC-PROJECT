#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_LINE_LENGTH 2048
#define MAX_PACKAGES 1000
#define MAX_FIELDS 20

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

// ----------------- DATASET PARSING (TAB DELIMITED) -----------------
int parse_line(char* line, int package_index) {
    char* token;
    int field_index = 0;

    // Skip header line
    if (strstr(line, "package_id") != NULL) {
        return 0;
    }

    token = strtok(line, "\t");
    while (token != NULL && field_index < MAX_FIELDS) {
        // Remove newline if present
        char* newline = strchr(token, '\n');
        if (newline) *newline = '\0';

        // Extract fields based on column index (your dataset mapping)
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

// ----------------- QUERY PARSING -----------------
void reset_query_defaults() {
    strcpy(query_province, "");
    strcpy(query_category, "");
    query_budget_min = 0.0;
    query_budget_max = 1000000.0;
    query_days = -1;
    query_min_rating = 0.0;
    query_topk = 5;
}

// If user types only a number like "3", treat it as TOPK=3
int is_only_number(const char *s) {
    if (s == NULL || s[0] == '\0') return 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
    }
    return 1;
}

void parse_query(char* query_str) {
    reset_query_defaults();

    // If query is just a number => TOPK
    if (is_only_number(query_str)) {
        query_topk = atoi(query_str);
        if (query_topk < 1) query_topk = 1;
        if (query_topk > MAX_PACKAGES) query_topk = MAX_PACKAGES;
        return;
    }

    // Parse query:
    // PROVINCE=Punjab;CATEGORY=Nature;BUDGET_MIN=10000;BUDGET_MAX=30000;DAYS=3;MIN_RATING=4.0;TOPK=5
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

    if (query_topk < 1) query_topk = 1;
    if (query_topk > MAX_PACKAGES) query_topk = MAX_PACKAGES;
}

// ----------------- FILTER + SCORE -----------------
int matches_filter(int index) {
    if (strlen(query_province) > 0 && strcmp(provinces[index], query_province) != 0) return 0;
    if (strlen(query_category) > 0 && strcmp(categories[index], query_category) != 0) return 0;
    if (avg_prices[index] < query_budget_min || avg_prices[index] > query_budget_max) return 0;
    if (query_days > 0 && duration_days[index] != query_days) return 0;
    if (ratings[index] < query_min_rating) return 0;
    return 1;
}

double calculate_score(int index) {
    double score = 0.0;

    // Weighted formula:
    // 50*rating + 10*popularity_score + 5*log(reviews_count+1) + budget_closeness + duration_closeness
    score += 50.0 * ratings[index];
    score += 10.0 * popularity_scores[index];
    score += 5.0 * log(reviews_counts[index] + 1.0);

    // Budget closeness (only if budget max is changed by user)
    if (query_budget_max < 1000000.0) {
        double budget_diff = fabs(avg_prices[index] - query_budget_max);
        double budget_closeness = 100.0 / (1.0 + budget_diff / 1000.0);
        score += budget_closeness;
    }

    // Duration closeness (only if days filter used)
    if (query_days > 0) {
        int duration_diff = abs(duration_days[index] - query_days);
        double duration_closeness = 50.0 / (1.0 + duration_diff);
        score += duration_closeness;
    }

    return score;
}

// Simple bubble sort (descending by score)
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

// ----------------- MAIN -----------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <dataset_file> [query_string]\n", argv[0]);
        printf("Example query: PROVINCE=Punjab;CATEGORY=Nature;TOPK=3\n");
        printf("Or just type: 3   (means TOPK=3)\n");
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

    // Read query
    char query_str[1024];
    char query_original[1024];

    if (argc >= 3) {
        // command-line query
        strncpy(query_str, argv[2], sizeof(query_str) - 1);
        query_str[sizeof(query_str) - 1] = '\0';
    } else {
        // interactive query
        printf("\nEnter query (example: TOPK=3 or PROVINCE=Punjab;CATEGORY=Nature;TOPK=3)\n");
        printf("You can also just type: 3  (means TOPK=3)\n> ");
        fflush(stdout);

        if (fgets(query_str, sizeof(query_str), stdin) == NULL) {
            strcpy(query_str, "TOPK=5");
        }

        // remove newline
        char *nl = strchr(query_str, '\n');
        if (nl) *nl = '\0';

        // empty => default
        if (strlen(query_str) == 0) strcpy(query_str, "TOPK=5");
    }

    // save for printing (because strtok modifies query_str)
    strncpy(query_original, query_str, sizeof(query_original) - 1);
    query_original[sizeof(query_original) - 1] = '\0';

    // parse (modifies query_str)
    parse_query(query_str);

    printf("\nQuery: %s\n", query_original);
    printf("Filters: Province=%s, Category=%s, Budget=[%.0f-%.0f], Days=%d, MinRating=%.1f, TopK=%d\n\n",
           strlen(query_province) > 0 ? query_province : "ANY",
           strlen(query_category) > 0 ? query_category : "ANY",
           query_budget_min, query_budget_max,
           query_days > 0 ? query_days : -1,
           query_min_rating, query_topk);

    // Timing start
    clock_t start = clock();

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

    printf("Found %d matching packages.\n", filtered_count);

    if (filtered_count > 0) {
        int topk_count = (filtered_count < query_topk) ? filtered_count : query_topk;

        sort_topk(filtered_indices, filtered_scores, filtered_count);

        printf("\n==== TOP %d Recommendations ====\n", topk_count);
        for (int i = 0; i < topk_count; i++) {
            int idx = filtered_indices[i];
            printf("%d. %s | %s, %s | Category: %s | Days: %d | Price: %.0f | Rating: %.1f | Score: %.2f\n",
                   i + 1,
                   package_ids[idx],
                   place_names[idx],
                   provinces[idx],
                   categories[idx],
                   duration_days[idx],
                   avg_prices[idx],
                   ratings[idx],
                   filtered_scores[i]);
        }
    } else {
        printf("No packages match the query filters.\n");
    }

    // Timing end
    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("\nExecution Time (Serial): %.4f seconds\n", time_taken);

    return 0;
}
