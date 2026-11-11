#ifndef WEEKLY_AVAILABILITY_HEADER
#define WEEKLY_AVAILABILITY_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../semantic-analysis/TypeChecking.h"
#include "Calendar.h"
#include <stdlib.h>
#include <string.h>

/**
 * Where a day declared with an amount of time (e.g., "sunday: 2h") begins,
 * unless the plan specifies another time.
 */
#define DEFAULT_DAY_START (9 * 60)

/** A time window of a day, in minutes since midnight (the end is excluded). */
typedef struct {
	int start;
	int end;
} Window;

/** The windows of a day of the week, sorted by their start. */
typedef struct {
	Window * windows;
	int count;
} DayAvailability;

/** The windows of every day of the week, from monday to sunday. */
typedef struct {
	DayAvailability days[DAYS_PER_WEEK];
} WeeklyAvailability;

/**
 * Resolves the availability of every day of the week. A day can belong to
 * several declared slots (e.g., "every" and "sunday"), and the most specific
 * one wins: a single day over a set of days ("weekdays" or "weekend"), and
 * a set of days over "every". A slot with an amount of time (e.g., "2h")
 * becomes a window that begins at the specified start of the day.
 */
WeeklyAvailability * createWeeklyAvailability(const Availability * availability, const int dayStart);

/** Destroys a weekly availability. */
void destroyWeeklyAvailability(WeeklyAvailability * weeklyAvailability);

#endif
