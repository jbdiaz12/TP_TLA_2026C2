#ifndef ICALENDAR_HEADER
#define ICALENDAR_HEADER

#include "../planning/Calendar.h"
#include "../planning/Schedule.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Writes a schedule as an iCalendar file (RFC 5545), to import it in Google
 * Calendar, Apple Calendar or Outlook. Every block becomes an event, whose
 * description tells its method and its sessions. The times are "floating":
 * they're read in the time zone of the calendar that imports them. It
 * returns false when the file can't be written.
 */
bool generateICalendar(const Schedule * schedule, const char * path);

#endif
