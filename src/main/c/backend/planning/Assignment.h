#ifndef ASSIGNMENT_HEADER
#define ASSIGNMENT_HEADER

#include "../semantic-analysis/SymbolTable.h"
#include "Calendar.h"
#include "PlanningError.h"
#include "Schedule.h"
#include "WeeklyAvailability.h"
#include "Work.h"
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/** A limit that a plan doesn't set. */
#define UNLIMITED INT_MAX

/**
 * What the plan establishes for the assignment: its range of days, its
 * strategy, and the limits of its constraints.
 */
typedef struct {
	/** The first and the last day (day numbers) where a session can be placed. */
	int firstDay;
	int lastDay;
	/** Whether the goals with higher priority go first (otherwise, the earliest deadline goes first). */
	bool isPriorityFirst;
	/** The limits per day and per week: different topics, and minutes of study. */
	int topicsPerDay;
	int topicsPerWeek;
	int loadPerDay;
	int loadPerWeek;
} Settings;

/**
 * The deadline and the priority of every work: the due date of its goal (as
 * a day number), and the rank of the priority of its goal, or -1 without one.
 */
typedef struct {
	int deadline;
	int priority;
} Urgency;

/**
 * Places every block of every work in the calendar, from the first day of
 * the plan. A block is placed whole inside a window, after the topics that
 * its topic requires are finished, and no later than the deadline of its
 * goal. It reports the topics that can't be finished, and returns whatever
 * it could schedule.
 */
Schedule * assignWorks(const SymbolTable * table, const Work * works, const int count,
	const Urgency * urgencies, const WeeklyAvailability * availability, const Settings * settings);

#endif
