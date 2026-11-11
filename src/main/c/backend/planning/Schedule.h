#ifndef SCHEDULE_HEADER
#define SCHEDULE_HEADER

#include "../semantic-analysis/SymbolTable.h"
#include <stdbool.h>
#include <stdlib.h>

/**
 * The resulting schedule: every session and pause of the plan, placed in a
 * day and a time. It's what the code generation turns into the artifacts.
 */

typedef struct {
	const Symbol * topic;
	/** The method that produced it, or NULL for the implicit blocks and the fixed sessions. */
	const char * method;
	/** The day number of its day (see the calendar). */
	int day;
	/** The minutes since midnight when it starts. */
	int start;
	int duration;
	bool isSession;
	bool isFixed;
	/** The block that the entry belongs to: the entries of a block share it. */
	int block;
} Entry;

typedef struct {
	Entry * entries;
	int count;
	int capacity;
	/** The amount of blocks placed, to number the next one. */
	int blockCount;
} Schedule;

/** Adds an entry at the end of a schedule. */
void appendEntry(Schedule * schedule, const Entry entry);

/** Creates an empty schedule. */
Schedule * createSchedule();

/** Destroys a schedule (but not the symbols its entries point to). */
void destroySchedule(Schedule * schedule);

/**
 * The index of the last entry of the block that starts at an entry. In a
 * sorted schedule, the entries of a block are consecutive.
 */
int lastOfBlock(const Schedule * schedule, const int first);

/** Sorts the entries of a schedule chronologically. */
void sortSchedule(Schedule * schedule);

#endif
