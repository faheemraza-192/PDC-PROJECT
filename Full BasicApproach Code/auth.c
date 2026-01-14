#include <stdio.h>
#include <string.h>
#include "auth.h"

int login(const char *file, const char *username, const char *password) {
    FILE *fp = fopen(file, "r");
    char u[50], p[50];

    if (!fp) return 0;

    while (fscanf(fp, "%49s %49s", u, p) == 2) {
        if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

int signup(const char *file, const char *username, const char *password) {
    FILE *fp = fopen(file, "r");
    char u[50], p[50];

    if (fp) {
        while (fscanf(fp, "%49s %49s", u, p) == 2) {
            if (strcmp(u, username) == 0) {
                fclose(fp);
                return 0; // already exists
            }
        }
        fclose(fp);
    }

    fp = fopen(file, "a");
    fprintf(fp, "%s %s\n", username, password);
    fclose(fp);
    return 1;
}
