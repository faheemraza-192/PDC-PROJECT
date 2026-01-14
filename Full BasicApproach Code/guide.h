#ifndef GUIDE_H
#define GUIDE_H

void guideLogin();
void guideMenu(const char *guideName);

/* Guide actions */
void viewGuideBookings(const char *guideName);
void acceptBooking(const char *guideName);

#endif
