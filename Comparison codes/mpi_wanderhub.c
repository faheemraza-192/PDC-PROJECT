
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpi.h>

#define MAX_LINE_LENGTH 2048
#define MAX_PACKAGES 1000
#define MAX_FIELDS 20
#define MAX_QUERY 1024

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

    // Skip header line
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

    // If just a number like "3" => TOPK=3
    if (is_only_number(query_str)) {
        query_topk = atoi(query_str);
        if (query_topk < 1) query_topk = 1;
        if (query_topk > MAX_PACKAGES) query_topk = MAX_PACKAGES;
        return;
    }

    // Example:
    // PROVINCE=Punjab;CATEGORY=Nature;BUDGET_MIN=10000;BUDGET_MAX=30000;DAYS=3;MIN_RATING=4.0;TOPK=5
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

// ---------------- Broadcast dataset from rank 0 to all ranks ----------------
void bcast_dataset(int rank) {
    MPI_Bcast(&total_packages, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Broadcast fixed-size arrays (full MAX_PACKAGES)
    MPI_Bcast(package_ids,        MAX_PACKAGES * 32,  MPI_CHAR,   0, MPI_COMM_WORLD);
    MPI_Bcast(place_names,        MAX_PACKAGES * 256, MPI_CHAR,   0, MPI_COMM_WORLD);
    MPI_Bcast(provinces,          MAX_PACKAGES * 128, MPI_CHAR,   0, MPI_COMM_WORLD);
    MPI_Bcast(categories,         MAX_PACKAGES * 128, MPI_CHAR,   0, MPI_COMM_WORLD);

    MPI_Bcast(duration_days,      MAX_PACKAGES,       MPI_INT,    0, MPI_COMM_WORLD);
    MPI_Bcast(avg_prices,         MAX_PACKAGES,       MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(ratings,            MAX_PACKAGES,       MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(reviews_counts,     MAX_PACKAGES,       MPI_INT,    0, MPI_COMM_WORLD);
    MPI_Bcast(popularity_scores,  MAX_PACKAGES,       MPI_DOUBLE, 0, MPI_COMM_WORLD);
}

// ---------------- Main ----------------
int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, world;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world);

    if (argc < 2) {
        if (rank == 0) {
            printf("Usage:\n");
            printf("  mpirun -np <P> %s <dataset_file> [query_string]\n", argv[0]);
            printf("Examples:\n");
            printf("  mpirun -np 4 %s package_dataset_pakistan.txt \"TOPK=5\"\n", argv[0]);
            printf("  mpirun -np 4 %s package_dataset_pakistan.txt \"PROVINCE=Punjab;CATEGORY=Nature;TOPK=3\"\n", argv[0]);
            printf("  mpirun -np 4 %s package_dataset_pakistan.txt 3   (means TOPK=3)\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    // -------- Rank 0 loads dataset --------
    if (rank == 0) {
        const char* dataset_file = argv[1];
        FILE* file = fopen(dataset_file, "r");
        if (file == NULL) {
            printf("Error: Cannot open file %s\n", dataset_file);
            total_packages = 0;
        } else {
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
        }
    }

    // Broadcast dataset to all ranks
    bcast_dataset(rank);

    // -------- Read query on rank 0, broadcast to all --------
    char query_str[MAX_QUERY];
    char query_original[MAX_QUERY];

    if (rank == 0) {
        if (argc >= 3) {
            strncpy(query_str, argv[2], sizeof(query_str) - 1);
            query_str[sizeof(query_str) - 1] = '\0';
        } else {
            printf("\nEnter query (example: TOPK=3 or PROVINCE=Punjab;CATEGORY=Nature;TOPK=3)\n");
            printf("You can also just type: 3  (means TOPK=3)\n> ");
            fflush(stdout);

            if (fgets(query_str, sizeof(query_str), stdin) == NULL) {
                strcpy(query_str, "TOPK=5");
            }

            char *nl = strchr(query_str, '\n');
            if (nl) *nl = '\0';

            if (strlen(query_str) == 0) strcpy(query_str, "TOPK=5");
        }

        strncpy(query_original, query_str, sizeof(query_original) - 1);
        query_original[sizeof(query_original) - 1] = '\0';
    }

    // Broadcast query to all ranks
    MPI_Bcast(query_str, MAX_QUERY, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Parse query on each rank (strtok modifies string -> use local copy)
    char query_copy[MAX_QUERY];
    strncpy(query_copy, query_str, sizeof(query_copy) - 1);
    query_copy[sizeof(query_copy) - 1] = '\0';
    parse_query(query_copy);

    if (rank == 0) {
        printf("\nQuery: %s\n", (argc >= 3) ? argv[2] : query_original);
        printf("Using %d MPI processes.\n", world);
        printf("Filters: Province=%s, Category=%s, Budget=[%.0f-%.0f], Days=%d, MinRating=%.1f, TopK=%d\n\n",
               strlen(query_province) > 0 ? query_province : "ANY",
               strlen(query_category) > 0 ? query_category : "ANY",
               query_budget_min, query_budget_max,
               query_days > 0 ? query_days : -1,
               query_min_rating, query_topk);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    // -------- Divide work across ranks --------
    int start = (rank * total_packages) / world;
    int end   = ((rank + 1) * total_packages) / world;

    int local_indices[MAX_PACKAGES];
    double local_scores[MAX_PACKAGES];
    int local_count = 0;

    for (int i = start; i < end; i++) {
        if (matches_filter(i)) {
            local_indices[local_count] = i;
            local_scores[local_count] = calculate_score(i);
            local_count++;
        }
    }

    if (local_count > 0) sort_topk(local_indices, local_scores, local_count);

    int local_topk = (local_count < query_topk) ? local_count : query_topk;

    // Prepare arrays to send only topk
    int send_idx[MAX_PACKAGES];
    double send_sc[MAX_PACKAGES];
    for (int i = 0; i < local_topk; i++) {
        send_idx[i] = local_indices[i];
        send_sc[i] = local_scores[i];
    }

    // -------- Gather match counts for summary --------
    int* all_match_counts = NULL;
    if (rank == 0) all_match_counts = (int*)malloc(world * sizeof(int));
    MPI_Gather(&local_count, 1, MPI_INT, all_match_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // -------- Gather topk counts to know receive sizes --------
    int* recv_counts = NULL;
    if (rank == 0) recv_counts = (int*)malloc(world * sizeof(int));
    MPI_Gather(&local_topk, 1, MPI_INT, recv_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // -------- Gatherv topk indices + scores to rank 0 --------
    int* displs = NULL;
    int total_recv = 0;

    int* all_indices = NULL;
    double* all_scores = NULL;

    if (rank == 0) {
        displs = (int*)malloc(world * sizeof(int));
        displs[0] = 0;
        total_recv = recv_counts[0];
        for (int i = 1; i < world; i++) {
            displs[i] = displs[i - 1] + recv_counts[i - 1];
            total_recv += recv_counts[i];
        }

        all_indices = (int*)malloc((total_recv > 0 ? total_recv : 1) * sizeof(int));
        all_scores  = (double*)malloc((total_recv > 0 ? total_recv : 1) * sizeof(double));
    }

    MPI_Gatherv(send_idx, local_topk, MPI_INT,
                all_indices, recv_counts, displs, MPI_INT,
                0, MPI_COMM_WORLD);

    MPI_Gatherv(send_sc, local_topk, MPI_DOUBLE,
                all_scores, recv_counts, displs, MPI_DOUBLE,
                0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double t1 = MPI_Wtime();

    // -------- Rank 0 merges + prints final results --------
    if (rank == 0) {
        int total_matched = 0;
        for (int i = 0; i < world; i++) total_matched += all_match_counts[i];

        printf("------------------------------------------------------------\n");
        printf("TOTAL MATCHED (sum of ranks): %d\n", total_matched);

        if (total_recv > 0) {
            // Sort merged candidates (only total_recv items, at most world*TOPK)
            sort_topk(all_indices, all_scores, total_recv);

            int final_topk = (total_recv < query_topk) ? total_recv : query_topk;
            printf("\n==== FINAL TOP %d Recommendations (MPI/OpenMPI) ====\n", final_topk);

            for (int i = 0; i < final_topk; i++) {
                int idx = all_indices[i];
                printf("%d. %s | %s, %s | Category: %s | Days: %d | Price: %.0f | Rating: %.1f | Score: %.2f\n",
                       i + 1,
                       package_ids[idx],
                       place_names[idx],
                       provinces[idx],
                       categories[idx],
                       duration_days[idx],
                       avg_prices[idx],
                       ratings[idx],
                       all_scores[i]);
            }
        } else {
            printf("No packages match the query filters.\n");
        }

        printf("\nExecution Time (MPI with %d processes): %.4f seconds\n", world, (t1 - t0));

        free(all_match_counts);
        free(recv_counts);
        free(displs);
        free(all_indices);
        free(all_scores);
    }

    MPI_Finalize();
    return 0;
}
