#include <stdio.h>
#include <string.h>
#include "auth.h"
#include "admin.h"

void adminLogin() {
    char u[50], p[50];

    printf("Admin Username: ");
    scanf("%49s", u);
    printf("Password: ");
    scanf("%49s", p);

    if (login("data/admins.txt", u, p)) {
        adminMenu();
    } else {
        printf("❌ Invalid admin login\n");
    }
}

void adminMenu() {
    int ch;
    do {
        printf("\n--- ADMIN DASHBOARD ---\n");
        printf("1. View All Users\n");
        printf("2. View All Guides\n");
        printf("3. View All Bookings\n");
        printf("4. Confirm Booking\n");
        printf("0. Logout\n");
        scanf("%d", &ch);

        if (ch == 1) viewUsers();
        else if (ch == 2) viewGuides();
        else if (ch == 3) viewBookings();
        else if (ch == 4) confirmBooking();

    } while (ch != 0);
}

void viewUsers() {
    FILE *fp = fopen("data/users.txt", "r");
    char u[50], p[50];

    printf("\n--- USERS ---\n");
    while (fscanf(fp, "%s %s", u, p) == 2) {
        printf("%s\n", u);
    }
    fclose(fp);
}

void viewGuides() {
    FILE *fp = fopen("data/guides.txt", "r");
    char u[50], p[50];

    printf("\n--- GUIDES ---\n");
    while (fscanf(fp, "%s %s", u, p) == 2) {
        printf("%s\n", u);
    }
    fclose(fp);
}

void viewBookings() {
    FILE *fp = fopen("data/bookings.txt", "r");
    char user[50], pkg[20], guide[20], status[30];

    printf("\nUser   Package   Guide   Status\n");
    printf("--------------------------------\n");

    while (fscanf(fp, "%s %s %s %s", user, pkg, guide, status) == 4) {
        printf("%s   %s   %s   %s\n", user, pkg, guide, status);
    }
    fclose(fp);
}

void confirmBooking() {
    FILE *fp = fopen("data/bookings.txt", "r");
    FILE *temp = fopen("data/temp.txt", "w");

    char user[50], pkg[20], guide[20], status[30];
    char targetPkg[20];

    printf("Enter Package ID to confirm: ");
    scanf("%19s", targetPkg);

    while (fscanf(fp, "%s %s %s %s", user, pkg, guide, status) == 4) {
        if (strcmp(pkg, targetPkg) == 0 && strcmp(status, "GUIDE_ACCEPTED") == 0) {
            fprintf(temp, "%s %s %s ADMIN_CONFIRMED\n", user, pkg, guide);
        } else {
            fprintf(temp, "%s %s %s %s\n", user, pkg, guide, status);
        }
    }

    fclose(fp);
    fclose(temp);

    remove("data/bookings.txt");
    rename("data/temp.txt", "data/bookings.txt");

    printf("✅ Booking confirmed by Admin.\n");
}
