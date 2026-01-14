#include <stdio.h>
#include "user.h"
#include "admin.h"
#include "guide.h"

int main() {
    int choice;

    while (1) {
        printf("\n--- WANDER HUB AI ---\n");
        printf("1. User Login\n");
        printf("2. User Signup\n");
        printf("3. Admin Login\n");
        printf("4. Guide Login\n");
        printf("0. Exit\n");
        scanf("%d", &choice);

        switch (choice) {
            case 1: userLogin(); break;
            case 2: userSignup(); break;
            case 3: adminLogin(); break;
            case 4: guideLogin(); break;
            case 0: return 0;
            default: printf("Invalid choice\n");
        }
    }
}
