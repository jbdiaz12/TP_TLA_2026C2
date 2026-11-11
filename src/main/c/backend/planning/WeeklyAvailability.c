#include "WeeklyAvailability.h"

/* PRIVATE FUNCTIONS */

static int _compareWindows(const void * first, const void * second);
static int _daysOf(const char * name, int * specificity);
static void _fillDay(DayAvailability * day, const Slot * slot, const int dayStart);

static int _compareWindows(const void * first, const void * second) {
	return ((const Window *) first)->start - ((const Window *) second)->start;
}

/**
 * The days that a slot names, as a set of bits (bit 0 is monday), and how
 * specific the name is: 2 for a single day, 1 for "weekdays" or "weekend",
 * and 0 for "every".
 */
static int _daysOf(const char * name, int * specificity) {
	static const char * const days[] = { "monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday" };
	for (int k = 0; k < DAYS_PER_WEEK; ++k) {
		if (strcmp(name, days[k]) == 0) {
			*specificity = 2;
			return 1 << k;
		}
	}
	*specificity = 1;
	if (strcmp(name, "weekdays") == 0) {
		return 0x1F;
	}
	if (strcmp(name, "weekend") == 0) {
		return 0x60;
	}
	*specificity = 0;
	return (strcmp(name, "every") == 0) ? 0x7F : 0;
}

/**
 * Builds the windows of a day from the periods of its slot. The analysis has
 * already validated the periods, so they're constants that don't overlap.
 */
static void _fillDay(DayAvailability * day, const Slot * slot, const int dayStart) {
	int count = 0;
	for (const PeriodList * node = slot->periods; node != NULL; node = node->next) {
		++count;
	}
	day->windows = calloc(count, sizeof(Window));
	day->count = 0;
	for (const PeriodList * node = slot->periods; node != NULL; node = node->next) {
		const Period * period = node->period;
		if (period->type == SINGLE_PERIOD) {
			int amount = 0;
			if (evaluateConstant(period->amount, &amount) && 0 < amount) {
				const int end = dayStart + amount;
				day->windows[day->count++] = (Window) { .start = dayStart, .end = (end < 24 * 60) ? end : 24 * 60 };
			}
		}
		else if (period->from->type == FACTOR_EXPRESSION && period->from->factor->type == CONSTANT_FACTOR
				&& period->to->type == FACTOR_EXPRESSION && period->to->factor->type == CONSTANT_FACTOR) {
			day->windows[day->count++] = (Window) {
				.start = period->from->factor->constant->time,
				.end = period->to->factor->constant->time
			};
		}
	}
	qsort(day->windows, day->count, sizeof(Window), _compareWindows);
}

/* PUBLIC FUNCTIONS */

WeeklyAvailability * createWeeklyAvailability(const Availability * availability, const int dayStart) {
	WeeklyAvailability * weeklyAvailability = calloc(1, sizeof(WeeklyAvailability));
	for (int weekday = 0; weekday < DAYS_PER_WEEK; ++weekday) {
		const Slot * chosen = NULL;
		int chosenSpecificity = -1;
		for (const SlotList * node = availability->slots; node != NULL; node = node->next) {
			int specificity = 0;
			const int days = _daysOf(node->slot->day, &specificity);
			if ((days & (1 << weekday)) != 0 && chosenSpecificity < specificity) {
				chosen = node->slot;
				chosenSpecificity = specificity;
			}
		}
		if (chosen != NULL) {
			_fillDay(&weeklyAvailability->days[weekday], chosen, dayStart);
		}
	}
	return weeklyAvailability;
}

void destroyWeeklyAvailability(WeeklyAvailability * weeklyAvailability) {
	if (weeklyAvailability != NULL) {
		for (int weekday = 0; weekday < DAYS_PER_WEEK; ++weekday) {
			free(weeklyAvailability->days[weekday].windows);
		}
		free(weeklyAvailability);
	}
}
