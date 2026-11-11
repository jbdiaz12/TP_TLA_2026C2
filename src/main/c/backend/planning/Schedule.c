#include "Schedule.h"

/* PRIVATE FUNCTIONS */

static int _compareEntries(const void * first, const void * second);

static int _compareEntries(const void * first, const void * second) {
	const Entry * a = (const Entry *) first;
	const Entry * b = (const Entry *) second;
	if (a->day != b->day) {
		return a->day - b->day;
	}
	return a->start - b->start;
}

/* PUBLIC FUNCTIONS */

void appendEntry(Schedule * schedule, const Entry entry) {
	if (schedule->count == schedule->capacity) {
		schedule->capacity = (schedule->capacity == 0) ? 16 : 2 * schedule->capacity;
		schedule->entries = realloc(schedule->entries, schedule->capacity * sizeof(Entry));
	}
	schedule->entries[schedule->count++] = entry;
}

Schedule * createSchedule() {
	return calloc(1, sizeof(Schedule));
}

void destroySchedule(Schedule * schedule) {
	if (schedule != NULL) {
		free(schedule->entries);
		free(schedule);
	}
}

int lastOfBlock(const Schedule * schedule, const int first) {
	int last = first;
	while (last + 1 < schedule->count && schedule->entries[last + 1].block == schedule->entries[first].block) {
		++last;
	}
	return last;
}

void sortSchedule(Schedule * schedule) {
	qsort(schedule->entries, schedule->count, sizeof(Entry), _compareEntries);
}
