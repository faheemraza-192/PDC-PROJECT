#include <stdio.h>
#include "auth.h"
#include "user.h"
#include "booking.h"

void userLogin() {
    char u[50], p[50];

    printf("Username: ");
    scanf("%49s", u);
    printf("Password: ");
    scanf("%49s", p);

    if (login("data/users.txt", u, p)) {
        printf("\n✅ Login successful. Welcome %s!\n", u);
        userMenu(u);
    } else {
        printf("\n❌ Invalid username or password\n");
    }
}

void userSignup() {
    char u[50], p[50];

    printf("Choose username (no spaces): ");
    scanf("%49s", u);
    printf("Choose password (no spaces): ");
    scanf("%49s", p);

    if (signup("data/users.txt", u, p)) {
        printf("\n✅ Signup successful. Please login.\n");
    } else {
        printf("\n❌ Username already exists.\n");
    }
}

void userMenu(const char *username) {
    int choice;

    do {
        printf("\n====================================\n");
        printf(" 🌍 Wander-Hub-AI | User Dashboard\n");
        printf("====================================\n");
        printf("1. View Packages (Filtered)\n");
        printf("2. Price Based Recommendation\n");
        printf("3. Destination Based Recommendation\n");
        printf("4. Book a Package\n");
        printf("0. Logout\n");
        printf("Choose option: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1: viewPackages(); break;
            case 2: priceBasedSuggestion(); break;
            case 3: destinationSuggestion(); break;
            case 4: bookPackage(username); break;
        }
    } while (choice != 0);
}
