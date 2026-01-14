// openmp_wanderhub.c
// OpenMP version of WanderHub recommender
// Same dataset parsing + same query format + same scoring logic as your serial/pthreads

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>

#define MAX_LINE_LENGTH 2048
#define MAX_PACKAGES 1000
#define MAX_FIELDS 20

// ---------------- Global arrays (no structs) ----------------
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

// ---------------- Query parameters (defaults) ----------------
char query_province[128] = "";
char query_category[128] = "";
double query_budget_min = 0.0;
double query_budget_max = 1000000.0;
int query_days = -1;
double query_min_rating = 0.0;
int query_topk = 5;

// ---------------- Helpers ----------------
int is_only_number(const char *s) {
    if (s == NULL || s[0] == '\0') return 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
    }
    return 1;
}

void reset_query_defaults() {
    strcpy(query_province, "");
    strcpy(query_category, "");
    query_budget_min = 0.0;
    query_budget_max = 1000000.0;
    query_days = -1;
    query_min_rating = 0.0;
    query_topk = 5;
}

// ---------------- Dataset parsing (TAB-delimited) ----------------
int parse_line(char* line, int package_index) {
    char* token;
    int field_index = 0;

    if (strstr(line, "package_id") != NULL) return 0;

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

// ---------------- Query parsing ----------------
void parse_query(char* query_str) {
    reset_query_defaults();

    // If query is just a number => TOPK
    if (is_only_number(query_str)) {
        query_topk = atoi(query_str);
        if (query_topk < 1) query_topk = 1;
        if (query_topk > MAX_PACKAGES) query_topk = MAX_PACKAGES;
        return;
    }

    char* token = strtok(query_str, ";");
    while (token != NULL) {
        if (strncmp(token, "PROVINCE=", 9) == 0) strcpy(query_province, token + 9);
        else if (strncmp(token, "CATEGORY=", 9) == 0) strcpy(query_category, token + 9);
        else if (strncmp(token, "BUDGET_MIN=", 11) == 0) query_budget_min = atof(token + 11);
        else if (strncmp(token, "BUDGET_MAX=", 11) == 0) query_budget_max = atof(token + 11);
        else if (strncmp(token, "DAYS=", 5) == 0) query_days = atoi(token + 5);
        else if (strncmp(token, "MIN_RATING=", 11) == 0) query_min_rating = atof(token + 11);
        else if (strncmp(token, "TOPK=", 5) == 0) query_topk = atoi(token + 5);

        token = strtok(NULL, ";");
    }

    if (query_topk < 1) query_topk = 1;
    if (query_topk > MAX_PACKAGES) query_topk = MAX_PACKAGES;
}

// ---------------- Filter + Score ----------------
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

// Bubble sort (descending by score)
void sort_topk(int* indices, double* scores, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (scores[j] < scores[j + 1]) {
                double ts = scores[j];
                scores[j] = scores[j + 1];
                scores[j + 1] = ts;

                int ti = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = ti;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <dataset_file> <num_threads> [query_string]\n", argv[0]);
        printf("Example: %s package_dataset_pakistan.txt 4 \"PROVINCE=Punjab;TOPK=3\"\n", argv[0]);
        printf("Or:      %s package_dataset_pakistan.txt 4 3   (means TOPK=3)\n", argv[0]);
        printf("If no query_string, it will ask you in terminal.\n");
        return 1;
    }

    const char* dataset_file = argv[1];
    int num_threads = atoi(argv[2]);
    if (num_threads < 1) num_threads = 1;

    // Load dataset
    FILE* file = fopen(dataset_file, "r");
    if (file == NULL) {
        printf("Error: Cannot open file %s\n", dataset_file);
        return 1;
    }

    char line[MAX_LINE_LENGTH];
    total_packages = 0;

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

    // Read query (argv OR stdin)
    char query_str[1024];
    char query_original[1024];

    if (argc >= 4) {
        strncpy(query_str, argv[3], sizeof(query_str) - 1);
        query_str[sizeof(query_str) - 1] = '\0';
    } else {
        printf("\nEnter query (example: TOPK=3 or PROVINCE=Punjab;CATEGORY=Nature;TOPK=3)\n");
        printf("You can also just type: 3  (means TOPK=3)\n> ");
        fflush(stdout);

        if (fgets(query_str, sizeof(query_str), stdin) == NULL) strcpy(query_str, "TOPK=5");

        char* nl = strchr(query_str, '\n');
        if (nl) *nl = '\0';
        if (strlen(query_str) == 0) strcpy(query_str, "TOPK=5");
    }

    strncpy(query_original, query_str, sizeof(query_original) - 1);
    query_original[sizeof(query_original) - 1] = '\0';

    // parse_query modifies the string (strtok)
    char query_copy[1024];
    strncpy(query_copy, query_str, sizeof(query_copy) - 1);
    query_copy[sizeof(query_copy) - 1] = '\0';
    parse_query(query_copy);

    printf("\nQuery: %s\n", query_original);
    printf("Using %d OpenMP threads.\n", num_threads);
    printf("Filters: Province=%s, Category=%s, Budget=[%.0f-%.0f], Days=%d, MinRating=%.1f, TopK=%d\n\n",
           strlen(query_province) > 0 ? query_province : "ANY",
           strlen(query_category) > 0 ? query_category : "ANY",
           query_budget_min, query_budget_max,
           query_days > 0 ? query_days : -1,
           query_min_rating, query_topk);

    // Timing start
    omp_set_num_threads(num_threads);
    double t0 = omp_get_wtime();

    // Parallel filter + score
    int global_indices[MAX_PACKAGES];
    double global_scores[MAX_PACKAGES];
    int global_count = 0;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < total_packages; i++) {
        if (matches_filter(i)) {
            double sc = calculate_score(i);

            int pos;
            #pragma omp atomic capture
            pos = global_count++;

            if (pos < MAX_PACKAGES) {
                global_indices[pos] = i;
                global_scores[pos] = sc;
            }
        }
    }

    // Clamp just in case (should not exceed MAX_PACKAGES anyway)
    if (global_count > MAX_PACKAGES) global_count = MAX_PACKAGES;

    printf("Found %d matching packages.\n", global_count);

    if (global_count > 0) {
        sort_topk(global_indices, global_scores, global_count);
        int topk_count = (global_count < query_topk) ? global_count : query_topk;

        printf("\n==== FINAL TOP %d Recommendations (OpenMP) ====\n", topk_count);
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

    double t1 = omp_get_wtime();
    printf("\nExecution Time (OpenMP with %d threads): %.4f seconds\n", num_threads, (t1 - t0));

    return 0;
}
