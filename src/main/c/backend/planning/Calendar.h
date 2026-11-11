#ifndef CALENDAR_HEADER
#define CALENDAR_HEADER

#include <stdio.h>

/**
 * The dates of the language are integers with the shape yyyymmdd (see the
 * lexical-analyzer), which keep their order but can't be subtracted. The
 * calendar converts them into day numbers: consecutive days have consecutive
 * numbers, so the distance between two dates is a subtraction.
 */

/** The days of the week, from monday (as in the ISO 8601 standard). */
#define DAYS_PER_WEEK 7

/** The minutes of a day, to turn a day number and a time into an absolute time. */
#define MINUTES_PER_DAY (24 * 60)

/** Converts a date (yyyymmdd) into its day number. */
int toDayNumber(const int date);

/** Converts a day number into its date (yyyymmdd). */
int toDate(const int dayNumber);

/** The day of the week of a day number, from 0 (monday) to 6 (sunday). */
int weekdayOf(const int dayNumber);

/** The name of a day of the week, for the messages (e.g., "viernes"). */
const char * weekdayName(const int weekday);

/** The name of a month, from 1 (january) to 12 (december), e.g., "septiembre". */
const char * monthName(const int month);

/** Writes a date as "yyyy-mm-dd", in a buffer of at least 11 characters. */
void formatDate(const int date, char * buffer);

/** Writes a duration as "18h", "1h30m" or "45m", in a buffer of at least 16 characters. */
void formatDuration(const int minutes, char * buffer);

/** Writes an amount of minutes since midnight as "hh:mm", in a buffer of at least 6 characters. */
void formatTime(const int minutes, char * buffer);

#endif
