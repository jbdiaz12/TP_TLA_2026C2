#include "Assignment.h"

/* MODULE INTERNAL STATE */

#define UNPLACED -1

/** How much of a work is already placed. Times are absolute: day * 1440 + minutes. */
typedef struct {
	int * starts;
	int * ends;
	int placedCount;
	bool isComplete;
	/** When the last placed block (or the fixed session) of the work ends. */
	int finish;
	/** The works that must be finished before this one starts. */
	int * prerequisites;
	int prerequisiteCount;
	/** Whether the work has a block that doesn't fit inside any window. */
	bool isUnfit;
	/** For a work with "daily": the days studied, and their minutes of study. */
	int dailyDays;
	int dailyStudy;
} Progress;

/** A block of a work that can be placed, and the earliest time when it can start. */
typedef struct {
	int work;
	int block;
	int earliest;
} Candidate;

/** The occupied intervals of a day, in minutes since midnight. */
typedef struct {
	Window * intervals;
	int count;
	int capacity;
} Occupation;

/** What the current day and week already contain, to enforce the limits of the plan. */
typedef struct {
	int dayLoad;
	int weekLoad;
	int dayTopicCount;
	int weekTopicCount;
	bool * dayTopics;
	bool * weekTopics;
} Usage;

/* PRIVATE FUNCTIONS */

static void _addPrerequisite(Progress * progress, const int work);
static void _appendBlock(Schedule * schedule, const Work * work, const Block * block, const int day, const int start);
static void _assignDaily(Schedule * schedule, const Work * works, const int count, Progress * progress, Usage * usage, Occupation * occupation,
	const Urgency * urgencies, const DayAvailability * windows, const Settings * settings, const int day);
static int _compareCandidates(const Candidate * first, const Candidate * second, const Urgency * urgencies, const bool isPriorityFirst);
static bool _earliestOf(const Work * works, const Progress * progress, const int work, const int block, const int planStart, int * earliest);
static bool _findStart(const DayAvailability * day, const Occupation * occupation, const int minimum, const int length, int * start);
static int _firstOfRepetition(const Work * work, const int block);
static bool _isDescendant(const Symbol * symbol, const Symbol * ancestor);
static bool _isExcluded(const Work * work, const int day);
static void _reportDailyShortage(const Work * work, const Progress * state, const Urgency * urgency, const Settings * settings);
static bool _isWithinLimits(const Usage * usage, const int work, const int study, const Settings * settings);
static void _occupy(Occupation * occupation, const int start, const int end);
static void _place(Schedule * schedule, const Work * works, Progress * progress, Usage * usage, Occupation * occupation, const Candidate * candidate, const int day, const int start);
/**
 * Reports a work with "daily" that couldn't study all of its days, or all of
 * its estimated time, before its deadline (or the end of the plan).
 */
static void _reportDailyShortage(const Work * work, const Progress * state, const Urgency * urgency, const Settings * settings) {
	const int limit = (urgency->deadline < settings->lastDay) ? urgency->deadline : settings->lastDay;
	char limitText[11];
	formatDate(toDate(limit), limitText);
	if (0 < work->days) {
		reportPlanningError(work->topic->position,
			"no alcanza el tiempo para el tema '%s': de sus %d días de 'daily', solo %d entran a más tardar el %s.",
			work->topic->name, work->days, state->dailyDays, limitText);
	}
	else if (0 < work->estimated) {
		char missingText[16];
		formatDuration(work->estimated - state->dailyStudy, missingText);
		reportPlanningError(work->topic->position,
			"no alcanza el tiempo para el tema '%s': le %s %s de estudio para terminarlo a más tardar el %s.",
			work->topic->name, (work->estimated - state->dailyStudy == 60) ? "falta" : "faltan", missingText, limitText);
	}
	else {
		reportPlanningError(work->topic->position,
			"el tema '%s' no tiene ningún día para estudiar con 'daily' antes del %s.", work->topic->name, limitText);
	}
}

static void _resolvePrerequisites(const SymbolTable * table, const Work * works, const int count, Progress * progress);
static void _use(Usage * usage, const int work, const int study);

static void _addPrerequisite(Progress * progress, const int work) {
	for (int k = 0; k < progress->prerequisiteCount; ++k) {
		if (progress->prerequisites[k] == work) {
			return;
		}
	}
	progress->prerequisites = realloc(progress->prerequisites, (progress->prerequisiteCount + 1) * sizeof(int));
	progress->prerequisites[progress->prerequisiteCount++] = work;
}

/** Adds the sessions and the pauses of a block to the schedule, as a new block. */
static void _appendBlock(Schedule * schedule, const Work * work, const Block * block, const int day, const int start) {
	const int number = schedule->blockCount++;
	int cursor = start;
	for (int s = 0; s < block->stepCount; ++s) {
		appendEntry(schedule, (Entry) {
			.topic = work->topic,
			.method = work->method,
			.day = day,
			.start = cursor,
			.duration = block->steps[s].duration,
			.isSession = block->steps[s].isSession,
			.isFixed = false,
			.block = number
		});
		cursor += block->steps[s].duration;
	}
}

/**
 * Places the block of every work with "daily" on a day, before the rest of
 * the works: like a fixed session, it's a commitment of that day. A day
 * without availability, or excluded by "except", doesn't count. A work
 * starts when the topics that it requires are finished, and on its last day
 * it studies only what remains of its estimated time. A day that can't hold
 * its block is reported, and the work isn't placed anymore.
 */
static void _assignDaily(Schedule * schedule, const Work * works, const int count, Progress * progress, Usage * usage, Occupation * occupation,
		const Urgency * urgencies, const DayAvailability * windows, const Settings * settings, const int day) {
	if (windows->count == 0) {
		return;
	}
	for (int k = 0; k < count; ++k) {
		const Work * work = &works[k];
		Progress * state = &progress[k];
		if (work->daily == 0 || work->blockCount == 0 || state->isComplete || state->isUnfit
				|| urgencies[k].deadline < day || _isExcluded(work, day)) {
			continue;
		}
		int minimum = 0;
		bool isReady = true;
		for (int p = 0; p < state->prerequisiteCount && isReady; ++p) {
			const Progress * prerequisite = &progress[state->prerequisites[p]];
			isReady = prerequisite->isComplete;
			if (minimum < prerequisite->finish - (day * MINUTES_PER_DAY)) {
				minimum = prerequisite->finish - (day * MINUTES_PER_DAY);
			}
		}
		if (!isReady || MINUTES_PER_DAY <= minimum) {
			continue;
		}
		const Block * unit = &work->blocks[0];
		Block block = { .steps = calloc(unit->stepCount, sizeof(Step)) };
		const bool isLimited = (work->days == 0 && 0 < work->estimated);
		int remaining = isLimited ? (work->estimated - state->dailyStudy) : unit->study;
		for (int s = 0; s < unit->stepCount && 0 < remaining; ++s) {
			Step step = unit->steps[s];
			if (step.isSession) {
				step.duration = (remaining < step.duration) ? remaining : step.duration;
				remaining -= step.duration;
				block.study += step.duration;
			}
			block.steps[block.stepCount++] = step;
			block.length += step.duration;
		}
		char dateText[11];
		char studyText[16];
		formatDate(toDate(day), dateText);
		formatDuration(block.study, studyText);
		int start = 0;
		if (!_isWithinLimits(usage, k, block.study, settings)) {
			reportPlanningError(work->topic->position,
				"el tema '%s' no puede estudiar sus %s de 'daily' el %s %s sin pasarse de los límites de las restricciones.",
				work->topic->name, studyText, weekdayName(weekdayOf(day)), dateText);
			state->isUnfit = true;
		}
		else if (!_findStart(windows, occupation, (minimum < 0) ? 0 : minimum, block.length, &start)) {
			reportPlanningError(work->topic->position,
				"el tema '%s' no tiene lugar para sus %s de 'daily' el %s %s: no entran en la disponibilidad que queda ese día.",
				work->topic->name, studyText, weekdayName(weekdayOf(day)), dateText);
			state->isUnfit = true;
		}
		else {
			_appendBlock(schedule, work, &block, day, start);
			_occupy(occupation, start, start + block.length);
			_use(usage, k, block.study);
			state->finish = (day * MINUTES_PER_DAY) + start + block.length;
			++state->dailyDays;
			state->dailyStudy += block.study;
			state->isComplete = (0 < work->days) ? (work->days <= state->dailyDays)
				: (0 < work->estimated && work->estimated <= state->dailyStudy);
		}
		free(block.steps);
	}
}

/**
 * The order in which the candidates are tried. The strategy decides first:
 * the earliest deadline, or the highest priority (and then the earliest
 * deadline). The ties are broken by the earliest start, and finally by the
 * order of the declarations.
 */
static int _compareCandidates(const Candidate * first, const Candidate * second, const Urgency * urgencies, const bool isPriorityFirst) {
	const Urgency * a = &urgencies[first->work];
	const Urgency * b = &urgencies[second->work];
	if (isPriorityFirst && a->priority != b->priority) {
		return b->priority - a->priority;
	}
	if (a->deadline != b->deadline) {
		return a->deadline - b->deadline;
	}
	if (first->earliest != second->earliest) {
		return first->earliest - second->earliest;
	}
	return first->work - second->work;
}

/**
 * The earliest time when a block can start, or false if it can't be placed
 * yet. The first block of a repetition starts after the first block of the
 * previous repetition ends (so the repetitions overlap, like the chains of
 * reviews of a spaced repetition). Any other block starts after the block
 * before it, and its offset after the beginning of its repetition. Every
 * block starts after the topics that its topic requires are finished.
 */
static bool _earliestOf(const Work * works, const Progress * progress, const int work, const int block, const int planStart, int * earliest) {
	const Work * current = &works[work];
	const Progress * state = &progress[work];
	const int first = _firstOfRepetition(current, block);
	const int offset = current->blocks[block].offset;
	if (block == first) {
		if (current->blocks[block].repetition == 0) {
			*earliest = planStart + offset;
		}
		else {
			const int previous = _firstOfRepetition(current, first - 1);
			if (state->starts[previous] == UNPLACED) {
				return false;
			}
			*earliest = state->ends[previous] + offset;
		}
	}
	else {
		if (state->starts[first] == UNPLACED || state->starts[block - 1] == UNPLACED) {
			return false;
		}
		*earliest = state->starts[first] - current->blocks[first].offset + offset;
		if (*earliest < state->ends[block - 1]) {
			*earliest = state->ends[block - 1];
		}
	}
	for (int k = 0; k < state->prerequisiteCount; ++k) {
		const Progress * prerequisite = &progress[state->prerequisites[k]];
		if (!prerequisite->isComplete) {
			return false;
		}
		if (*earliest < prerequisite->finish) {
			*earliest = prerequisite->finish;
		}
	}
	return true;
}

/**
 * Finds the earliest start, not before the minimum, where a block of the
 * specified length fits inside a window without overlapping anything else.
 */
static bool _findStart(const DayAvailability * day, const Occupation * occupation, const int minimum, const int length, int * start) {
	for (int w = 0; w < day->count; ++w) {
		int candidate = (day->windows[w].start < minimum) ? minimum : day->windows[w].start;
		bool hasMoved = true;
		while (hasMoved) {
			hasMoved = false;
			for (int k = 0; k < occupation->count; ++k) {
				const Window * interval = &occupation->intervals[k];
				if (candidate < interval->end && interval->start < candidate + length) {
					candidate = interval->end;
					hasMoved = true;
				}
			}
		}
		if (candidate + length <= day->windows[w].end) {
			*start = candidate;
			return true;
		}
	}
	return false;
}

static int _firstOfRepetition(const Work * work, const int block) {
	int first = block;
	while (0 < first && work->blocks[first - 1].repetition == work->blocks[block].repetition) {
		--first;
	}
	return first;
}

/**
 * Whether a topic is another one, or is nested inside it.
 */
static bool _isDescendant(const Symbol * symbol, const Symbol * ancestor) {
	for (const Symbol * current = symbol; current != NULL; current = current->parent) {
		if (current == ancestor) {
			return true;
		}
	}
	return false;
}

/** Whether a work isn't studied on a day, because of its "except". */
static bool _isExcluded(const Work * work, const int day) {
	if (work->excludedWeekdays[weekdayOf(day)]) {
		return true;
	}
	for (int k = 0; k < work->excludedDayCount; ++k) {
		if (work->excludedDays[k] == day) {
			return true;
		}
	}
	return false;
}

static bool _isWithinLimits(const Usage * usage, const int work, const int study, const Settings * settings) {
	if (settings->loadPerDay - study < usage->dayLoad || settings->loadPerWeek - study < usage->weekLoad) {
		return false;
	}
	if (!usage->dayTopics[work] && settings->topicsPerDay <= usage->dayTopicCount) {
		return false;
	}
	return usage->weekTopics[work] || usage->weekTopicCount < settings->topicsPerWeek;
}

static void _occupy(Occupation * occupation, const int start, const int end) {
	if (occupation->count == occupation->capacity) {
		occupation->capacity = (occupation->capacity == 0) ? 8 : 2 * occupation->capacity;
		occupation->intervals = realloc(occupation->intervals, occupation->capacity * sizeof(Window));
	}
	occupation->intervals[occupation->count++] = (Window) { .start = start, .end = end };
}

/**
 * Places a block at a time of a day, and adds its sessions and pauses to the
 * schedule.
 */
static void _place(Schedule * schedule, const Work * works, Progress * progress, Usage * usage, Occupation * occupation, const Candidate * candidate, const int day, const int start) {
	const Work * work = &works[candidate->work];
	const Block * block = &work->blocks[candidate->block];
	Progress * state = &progress[candidate->work];
	_appendBlock(schedule, work, block, day, start);
	_occupy(occupation, start, start + block->length);
	_use(usage, candidate->work, block->study);
	state->starts[candidate->block] = (day * MINUTES_PER_DAY) + start;
	state->ends[candidate->block] = (day * MINUTES_PER_DAY) + start + block->length;
	if (state->finish < state->ends[candidate->block]) {
		state->finish = state->ends[candidate->block];
	}
	state->isComplete = (++state->placedCount == work->blockCount);
}

/**
 * A topic waits for the topics that it requires, and for the ones that the
 * topics containing it require. Requiring a topic with subtopics means
 * requiring every one of them.
 */
static void _resolvePrerequisites(const SymbolTable * table, const Work * works, const int count, Progress * progress) {
	for (int k = 0; k < count; ++k) {
		for (const Symbol * symbol = works[k].topic; symbol != NULL; symbol = symbol->parent) {
			for (const MemberList * node = symbol->topic->members; node != NULL; node = node->next) {
				if (node->member->type != ATTRIBUTE_MEMBER || strcmp(node->member->attribute->name, "requires") != 0) {
					continue;
				}
				for (const ExpressionList * value = node->member->attribute->values; value != NULL; value = value->next) {
					const Symbol * required = findSymbol(table, TOPIC_SYMBOL, value->expression->factor->variable);
					for (int j = 0; j < count; ++j) {
						if (j != k && _isDescendant(works[j].topic, required)) {
							_addPrerequisite(&progress[k], j);
						}
					}
				}
			}
		}
	}
}

static void _use(Usage * usage, const int work, const int study) {
	usage->dayLoad += study;
	usage->weekLoad += study;
	if (!usage->dayTopics[work]) {
		usage->dayTopics[work] = true;
		++usage->dayTopicCount;
	}
	if (!usage->weekTopics[work]) {
		usage->weekTopics[work] = true;
		++usage->weekTopicCount;
	}
}

/* PUBLIC FUNCTIONS */

Schedule * assignWorks(const SymbolTable * table, const Work * works, const int count,
		const Urgency * urgencies, const WeeklyAvailability * availability, const Settings * settings) {
	Schedule * schedule = createSchedule();
	if (count == 0) {
		return schedule;
	}
	const int planStart = settings->firstDay * MINUTES_PER_DAY;
	int longestWindow = 0;
	for (int weekday = 0; weekday < DAYS_PER_WEEK; ++weekday) {
		for (int w = 0; w < availability->days[weekday].count; ++w) {
			const Window * window = &availability->days[weekday].windows[w];
			if (longestWindow < window->end - window->start) {
				longestWindow = window->end - window->start;
			}
		}
	}
	Progress * progress = calloc(count, sizeof(Progress));
	for (int k = 0; k < count; ++k) {
		const Work * work = &works[k];
		Progress * state = &progress[k];
		state->starts = calloc((work->blockCount == 0) ? 1 : work->blockCount, sizeof(int));
		state->ends = calloc((work->blockCount == 0) ? 1 : work->blockCount, sizeof(int));
		state->isComplete = (work->blockCount == 0);
		state->finish = planStart;
		for (int b = 0; b < work->blockCount; ++b) {
			state->starts[b] = UNPLACED;
			state->ends[b] = UNPLACED;
			if (longestWindow < work->blocks[b].length && !state->isUnfit) {
				char blockText[16];
				char windowText[16];
				formatDuration(work->blocks[b].length, blockText);
				formatDuration(longestWindow, windowText);
				reportPlanningError(work->topic->position,
					"el tema '%s' tiene bloques de %s, pero la ventana más larga de la disponibilidad dura %s.",
					work->topic->name, blockText, windowText);
				state->isUnfit = true;
			}
		}
		if (work->hasFixedSession) {
			appendEntry(schedule, (Entry) {
				.topic = work->topic,
				.method = NULL,
				.day = work->fixedDay,
				.start = work->fixedStart,
				.duration = work->fixedDuration,
				.isSession = true,
				.isFixed = true,
				.block = schedule->blockCount++
			});
			const int fixedEnd = (work->fixedDay * MINUTES_PER_DAY) + work->fixedStart + work->fixedDuration;
			if (state->finish < fixedEnd) {
				state->finish = fixedEnd;
			}
		}
	}
	_resolvePrerequisites(table, works, count, progress);
	Usage usage = {
		.dayTopics = calloc(count, sizeof(bool)),
		.weekTopics = calloc(count, sizeof(bool))
	};
	Occupation occupation = { 0 };
	Candidate * candidates = calloc(count, sizeof(Candidate));
	int week = INT_MIN;
	for (int day = settings->firstDay; day <= settings->lastDay; ++day) {
		const int weekday = weekdayOf(day);
		if (week != day - weekday) {
			week = day - weekday;
			usage.weekLoad = 0;
			usage.weekTopicCount = 0;
			memset(usage.weekTopics, 0, count * sizeof(bool));
		}
		usage.dayLoad = 0;
		usage.dayTopicCount = 0;
		memset(usage.dayTopics, 0, count * sizeof(bool));
		occupation.count = 0;
		for (int k = 0; k < count; ++k) {
			if (works[k].hasFixedSession && works[k].fixedDay == day) {
				_occupy(&occupation, works[k].fixedStart, works[k].fixedStart + works[k].fixedDuration);
				_use(&usage, k, works[k].fixedDuration);
			}
		}
		const DayAvailability * windows = &availability->days[weekday];
		_assignDaily(schedule, works, count, progress, &usage, &occupation, urgencies, windows, settings, day);
		bool hasPlaced = (0 < windows->count);
		while (hasPlaced) {
			hasPlaced = false;
			int candidateCount = 0;
			for (int k = 0; k < count; ++k) {
				if (works[k].daily != 0 || progress[k].isComplete || progress[k].isUnfit
						|| urgencies[k].deadline < day || _isExcluded(&works[k], day)) {
					continue;
				}
				Candidate best = { .work = -1 };
				for (int b = 0; b < works[k].blockCount; ++b) {
					int earliest = 0;
					if (progress[k].starts[b] == UNPLACED && _earliestOf(works, progress, k, b, planStart, &earliest)
							&& earliest < (day + 1) * MINUTES_PER_DAY && (best.work < 0 || earliest < best.earliest)) {
						best = (Candidate) { .work = k, .block = b, .earliest = earliest };
					}
				}
				if (0 <= best.work) {
					int position = candidateCount++;
					while (0 < position && 0 < _compareCandidates(&candidates[position - 1], &best, urgencies, settings->isPriorityFirst)) {
						candidates[position] = candidates[position - 1];
						--position;
					}
					candidates[position] = best;
				}
			}
			for (int c = 0; c < candidateCount && !hasPlaced; ++c) {
				const Block * block = &works[candidates[c].work].blocks[candidates[c].block];
				const int minimum = candidates[c].earliest - (day * MINUTES_PER_DAY);
				int start = 0;
				if (_isWithinLimits(&usage, candidates[c].work, block->study, settings)
						&& _findStart(windows, &occupation, (minimum < 0) ? 0 : minimum, block->length, &start)) {
					_place(schedule, works, progress, &usage, &occupation, &candidates[c], day, start);
					hasPlaced = true;
				}
			}
		}
	}
	for (int k = 0; k < count; ++k) {
		if (progress[k].isComplete || progress[k].isUnfit) {
			continue;
		}
		if (works[k].daily != 0) {
			if (0 < works[k].days || 0 < works[k].estimated || progress[k].dailyDays == 0) {
				_reportDailyShortage(&works[k], &progress[k], &urgencies[k], settings);
			}
			continue;
		}
		int missing = 0;
		for (int b = 0; b < works[k].blockCount; ++b) {
			if (progress[k].starts[b] == UNPLACED) {
				missing += works[k].blocks[b].study;
			}
		}
		const int limit = (urgencies[k].deadline < settings->lastDay) ? urgencies[k].deadline : settings->lastDay;
		char missingText[16];
		char limitText[11];
		formatDuration(missing, missingText);
		formatDate(toDate(limit), limitText);
		const char * verb = (missing == 1 || missing == 60) ? "falta" : "faltan";
		if (urgencies[k].isConstrained && urgencies[k].deadline <= settings->lastDay) {
			reportPlanningError(works[k].topic->position,
				"no alcanza el tiempo para el tema '%s': le %s %s de estudio para terminarlo a más tardar el %s, como pide la restricción %s.",
				works[k].topic->name, verb, missingText, limitText, describePosition(urgencies[k].constraint));
		}
		else {
			reportPlanningError(works[k].topic->position,
				"no alcanza el tiempo para el tema '%s': le %s %s de estudio para terminarlo a más tardar el %s.",
				works[k].topic->name, verb, missingText, limitText);
		}
	}
	for (int k = 0; k < count; ++k) {
		free(progress[k].starts);
		free(progress[k].ends);
		free(progress[k].prerequisites);
	}
	free(progress);
	free(candidates);
	free(occupation.intervals);
	free(usage.dayTopics);
	free(usage.weekTopics);
	sortSchedule(schedule);
	return schedule;
}
