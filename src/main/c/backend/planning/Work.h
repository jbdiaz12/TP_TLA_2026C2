#ifndef WORK_HEADER
#define WORK_HEADER

#include "../semantic-analysis/SymbolTable.h"
#include <stdbool.h>
#include <stdlib.h>

/**
 * The work of a topic, as the planner receives it: the blocks of study that
 * its method produces, before they're placed in the calendar.
 */

/** A session (study time) or a pause, in minutes. */
typedef struct {
	int duration;
	bool isSession;
} Step;

/**
 * A sitting: steps that happen one after the other, without a gap, so the
 * planner places the whole block inside a single window of a day.
 */
typedef struct {
	/**
	 * The repetition of the method that the block belongs to: the method is
	 * repeated until it covers the estimated time of its topic.
	 */
	int repetition;
	/**
	 * The minimum time between the start of its repetition and the start of
	 * the block. Only a block delayed with "after" has an offset.
	 */
	int offset;
	Step * steps;
	int stepCount;
	/** The minutes that the block occupies, pauses included. */
	int length;
	/** The minutes of study of the block (its sessions). */
	int study;
} Block;

/** Everything that the planner has to place for a topic without subtopics. */
typedef struct {
	const Symbol * topic;
	/** The name of the method of the topic, or NULL for the implicit blocks of 1h. */
	const char * method;
	/** The minutes of study of the topic, including its fixed session. */
	int estimated;
	/** The fixed session of the topic, which the planner never moves. */
	bool hasFixedSession;
	int fixedDay;
	int fixedStart;
	int fixedDuration;
	/** The blocks that cover the rest of the estimated time, in order. */
	Block * blocks;
	int blockCount;
} Work;

/** Destroys a list of works and their blocks. */
void destroyWorks(Work * works, const int count);

#endif
