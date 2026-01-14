#include <stdio.h>
#include <string.h>
#include "auth.h"
#include "guide.h"

void guideLogin() {
    char u[50], p[50];

    printf("Guide Username: ");
    scanf("%49s", u);
    printf("Password: ");
    scanf("%49s", p);

    if (login("data/guides.txt", u, p)) {
        guideMenu(u);
    } else {
        printf("❌ Invalid guide login\n");
    }
}

void guideMenu(const char *guideName) {
    int ch;
    do {
        printf("\n--- GUIDE DASHBOARD ---\n");
        printf("1. View My Bookings\n");
        printf("2. Accept Booking\n");
        printf("0. Logout\n");
        scanf("%d", &ch);

        if (ch == 1) viewGuideBookings(guideName);
        else if (ch == 2) acceptBooking(guideName);

    } while (ch != 0);
}

void viewGuideBookings(const char *guideName) {
    FILE *fp = fopen("data/bookings.txt", "r");
    char user[50], pkg[20], guide[20], status[30];

    if (!fp) {
        printf("No bookings found\n");
        return;
    }

    printf("\nUser   Package   Status\n");
    printf("---------------------------\n");

    while (fscanf(fp, "%s %s %s %s", user, pkg, guide, status) == 4) {
        if (strcmp(guide, guideName) == 0) {
            printf("%s   %s   %s\n", user, pkg, status);
        }
    }
    fclose(fp);
}

void acceptBooking(const char *guideName) {
    FILE *fp = fopen("data/bookings.txt", "r");
    FILE *temp = fopen("data/temp.txt", "w");

    char user[50], pkg[20], guide[20], status[30];
    char targetPkg[20];

    if (!fp || !temp) return;

    printf("Enter Package ID to accept: ");
    scanf("%19s", targetPkg);

    while (fscanf(fp, "%s %s %s %s", user, pkg, guide, status) == 4) {
        if (strcmp(pkg, targetPkg) == 0 && strcmp(guide, guideName) == 0) {
            fprintf(temp, "%s %s %s GUIDE_ACCEPTED\n", user, pkg, guide);
        } else {
            fprintf(temp, "%s %s %s %s\n", user, pkg, guide, status);
        }
    }

    fclose(fp);
    fclose(temp);

    remove("data/bookings.txt");
    rename("data/temp.txt", "data/bookings.txt");

    printf("✅ Booking accepted. Waiting for Admin approval.\n");
}
