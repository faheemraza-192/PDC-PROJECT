#include <stdio.h>
#include <string.h>
#include "booking.h"

void viewPackages() {
    FILE *fp = fopen("data/package_dataset_pakistan.txt", "r");
    char line[300];

    if (!fp) {
        printf("Package file not found\n");
        return;
    }

    printf("\n--- AVAILABLE PACKAGES ---\n");
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    fclose(fp);
}

void priceBasedSuggestion() {
    int budget;
    printf("Enter max budget: ");
    scanf("%d", &budget);

    FILE *fp = fopen("data/package_dataset_pakistan.txt", "r");
    char id[20], loc[50], prov[50], cat[30];
    int days, price;
    float rating;

    if (!fp) return;

    printf("\n--- PRICE BASED SUGGESTIONS ---\n");
    while (fscanf(fp, "%s %s %s %d %d %f %s",
                  id, loc, prov, &days, &price, &rating, cat) != EOF) {
        if (price <= budget) {
            printf("%s  %s  %d\n", id, loc, price);
        }
    }
    fclose(fp);
}

void destinationSuggestion() {
    char search[50];
    printf("Enter province or location: ");
    scanf("%s", search);

    FILE *fp = fopen("data/package_dataset_pakistan.txt", "r");
    char id[20], loc[50], prov[50], cat[30];
    int days, price;
    float rating;

    if (!fp) return;

    printf("\n--- DESTINATION BASED SUGGESTIONS ---\n");
    while (fscanf(fp, "%s %s %s %d %d %f %s",
                  id, loc, prov, &days, &price, &rating, cat) != EOF) {
        if (strcmp(loc, search) == 0 || strcmp(prov, search) == 0) {
            printf("%s  %s  %s\n", id, loc, prov);
        }
    }
    fclose(fp);
}

void bookPackage(const char *username) {
    char pkg[20], guide[20];

    printf("Enter Package ID: ");
    scanf("%19s", pkg);

    printf("Choose Guide (e.g. guide1): ");
    scanf("%19s", guide);

    FILE *fp = fopen("data/bookings.txt", "a");
    if (!fp) {
        printf("Booking file error\n");
        return;
    }

    fprintf(fp, "%s %s %s PENDING\n", username, pkg, guide);
    fclose(fp);

    printf("\n✅ Booking placed successfully!\n");
    printf("📌 Status: PENDING (Waiting for Guide Approval)\n");
}


void viewUserBookings(char *username) {
    FILE *fp = fopen("data/bookings.txt", "r");
    char user[50], pkg[20], guide[20], status[20];

    if (!fp) {
        printf("No bookings found\n");
        return;
    }

    printf("\n--- MY BOOKINGS ---\n");
    while (fscanf(fp, "%s %s %s %s", user, pkg, guide, status) != EOF) {
        if (strcmp(user, username) == 0) {
            printf("%s  %s  %s\n", pkg, guide, status);
        }
    }
    fclose(fp);
}
