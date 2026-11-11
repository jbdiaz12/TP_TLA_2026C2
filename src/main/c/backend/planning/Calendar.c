#include "Calendar.h"

/**
 * The day numbers count the days since 1970-01-01, which was a thursday. The
 * conversions follow the proleptic Gregorian calendar, and work by eras of
 * 400 years (146097 days), after which the calendar repeats itself.
 *
 * @see https://howardhinnant.github.io/date_algorithms.html
 */

int toDayNumber(const int date) {
	const int month = (date / 100) % 100;
	const int day = date % 100;
	const int year = (date / 10000) - (month <= 2 ? 1 : 0);
	const int era = (0 <= year ? year : year - 399) / 400;
	const int yearOfEra = year - (era * 400);
	const int dayOfYear = ((153 * (month + (2 < month ? -3 : 9))) + 2) / 5 + day - 1;
	const int dayOfEra = (yearOfEra * 365) + (yearOfEra / 4) - (yearOfEra / 100) + dayOfYear;
	return (era * 146097) + dayOfEra - 719468;
}

int toDate(const int dayNumber) {
	const int shifted = dayNumber + 719468;
	const int era = (0 <= shifted ? shifted : shifted - 146096) / 146097;
	const int dayOfEra = shifted - (era * 146097);
	const int yearOfEra = (dayOfEra - (dayOfEra / 1460) + (dayOfEra / 36524) - (dayOfEra / 146096)) / 365;
	const int dayOfYear = dayOfEra - ((365 * yearOfEra) + (yearOfEra / 4) - (yearOfEra / 100));
	const int shiftedMonth = ((5 * dayOfYear) + 2) / 153;
	const int day = dayOfYear - (((153 * shiftedMonth) + 2) / 5) + 1;
	const int month = shiftedMonth + (shiftedMonth < 10 ? 3 : -9);
	const int year = yearOfEra + (era * 400) + (month <= 2 ? 1 : 0);
	return (year * 10000) + (month * 100) + day;
}

int weekdayOf(const int dayNumber) {
	return (((dayNumber % DAYS_PER_WEEK) + DAYS_PER_WEEK) + 3) % DAYS_PER_WEEK;
}

const char * weekdayName(const int weekday) {
	static const char * const names[] = { "lunes", "martes", "miércoles", "jueves", "viernes", "sábado", "domingo" };
	return names[weekday];
}

void formatDate(const int date, char * buffer) {
	sprintf(buffer, "%04d-%02d-%02d", date / 10000, (date / 100) % 100, date % 100);
}

void formatTime(const int minutes, char * buffer) {
	sprintf(buffer, "%02d:%02d", minutes / 60, minutes % 60);
}
