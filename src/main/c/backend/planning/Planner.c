#include "Planner.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownPlannerModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Planner...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializePlannerModule() {
	_logger = createLogger("Planner");
	return _shutdownPlannerModule;
}

/* PRIVATE FUNCTIONS */

static int _dueDay(const Symbol * goal);
static bool _evaluateDate(const Expression * expression, const Symbol * goal, const SymbolTable * table, int * minutes);
static const Attribute * _findAttribute(const MemberList * members, const char * name);
static const Declaration * _findDeclaration(const Program * program, const DeclarationType type);
static int _floorDivision(const int dividend, const int divisor);
static bool _isWithin(const Symbol * topic, const Symbol * subject);
static void _logSchedule(const Schedule * schedule);
static void _logWork(const Work * work);
static void _printSummary(const Schedule * schedule, const Work * works, const int count, const Urgency * urgencies);
static void _readDeadlines(const Program * program, const SymbolTable * table, const Work * works, const int count, Urgency * urgencies);
static void _readLimits(const Program * program, Settings * settings);
static Settings _readSettings(const Program * program, const Urgency * urgencies, const int count, int * dayStart);
static Urgency * _readUrgencies(const SymbolTable * table, const Work * works, const int count);

/** The due date of a goal, as a day number. */
static int _dueDay(const Symbol * goal) {
	const Attribute * due = _findAttribute(goal->goal->members, "due");
	return toDayNumber(due->values->expression->factor->constant->date);
}

/**
 * Computes a date that the analysis accepts in a constraint (a date, or the
 * deadline of a goal, moved by constant durations), as an absolute time in
 * minutes. Inside the constraint, "goal" is the goal being constrained.
 */
static bool _evaluateDate(const Expression * expression, const Symbol * goal, const SymbolTable * table, int * minutes) {
	if (expression->type == FACTOR_EXPRESSION) {
		const Factor * factor = expression->factor;
		if (factor->type == PARENTHESIZED_FACTOR) {
			return _evaluateDate(factor->expression, goal, table, minutes);
		}
		if (factor->type == CONSTANT_FACTOR) {
			*minutes = toDayNumber(factor->constant->date) * MINUTES_PER_DAY;
			return true;
		}
		const char * subject = factor->arguments->expression->factor->variable;
		const Symbol * target = (strcmp(subject, "goal") == 0) ? goal : findSymbol(table, GOAL_SYMBOL, subject);
		*minutes = _dueDay(target) * MINUTES_PER_DAY;
		return true;
	}
	int base = 0;
	int shift = 0;
	if (!_evaluateDate(expression->leftExpression, goal, table, &base) || !evaluateConstant(expression->rightExpression, &shift)) {
		return false;
	}
	*minutes = (expression->type == ARITHMETIC_ADD) ? (base + shift) : (base - shift);
	return true;
}

static const Attribute * _findAttribute(const MemberList * members, const char * name) {
	for (const MemberList * node = members; node != NULL; node = node->next) {
		if (node->member->type == ATTRIBUTE_MEMBER && strcmp(node->member->attribute->name, name) == 0) {
			return node->member->attribute;
		}
	}
	return NULL;
}

static const Declaration * _findDeclaration(const Program * program, const DeclarationType type) {
	for (const DeclarationList * node = program->declarations; node != NULL; node = node->next) {
		if (node->declaration->type == type) {
			return node->declaration;
		}
	}
	return NULL;
}

/** A division that rounds towards minus infinity, to turn negative times into days. */
static int _floorDivision(const int dividend, const int divisor) {
	const int quotient = dividend / divisor;
	return (dividend % divisor != 0 && dividend < 0) ? (quotient - 1) : quotient;
}

/** Whether a topic is the subject of a constraint, or is nested inside it. */
static bool _isWithin(const Symbol * topic, const Symbol * subject) {
	for (const Symbol * current = topic; current != NULL; current = current->parent) {
		if (current == subject) {
			return true;
		}
	}
	return false;
}

/**
 * Logs the resulting schedule in DEBUGGING level.
 */
static void _logSchedule(const Schedule * schedule) {
	for (int k = 0; k < schedule->count; ++k) {
		const Entry * entry = &schedule->entries[k];
		char date[11];
		char start[6];
		formatDate(toDate(entry->day), date);
		formatTime(entry->start, start);
		logDebugging(_logger, "%s %s %s %dm %s%s", date, start, entry->isSession ? "session" : "pause  ",
			entry->duration, entry->topic->name, entry->isFixed ? " (fixed)" : "");
	}
}

/**
 * Logs the blocks of a topic in DEBUGGING level, to follow the expansion of
 * its method.
 */
static void _logWork(const Work * work) {
	logDebugging(_logger, "Topic %s: method=%s, estimated=%d, fixed=%d, blocks=%d",
		work->topic->name, (work->method == NULL) ? "(implicit)" : work->method,
		work->estimated, work->hasFixedSession ? work->fixedDuration : 0, work->blockCount);
	for (int k = 0; k < work->blockCount; ++k) {
		const Block * block = &work->blocks[k];
		logDebugging(_logger, "  block: repetition=%d, offset=%d, length=%d, study=%d, steps=%d",
			block->repetition, block->offset, block->length, block->study, block->stepCount);
	}
}

/**
 * Prints a summary of the schedule in the standard output: the whole plan,
 * and when each topic is studied.
 */
static void _printSummary(const Schedule * schedule, const Work * works, const int count, const Urgency * urgencies) {
	int sessions = 0;
	int study = 0;
	for (int e = 0; e < schedule->count; ++e) {
		if (schedule->entries[e].isSession) {
			++sessions;
			study += schedule->entries[e].duration;
		}
	}
	if (sessions == 0) {
		printf("La agenda no tiene sesiones de estudio.\n");
		return;
	}
	char first[11];
	char last[11];
	char studyText[16];
	formatDate(toDate(schedule->entries[0].day), first);
	formatDate(toDate(schedule->entries[schedule->count - 1].day), last);
	formatDuration(study, studyText);
	printf("Agenda del %s al %s: %d sesiones, %s de estudio.\n", first, last, sessions, studyText);
	int width = 0;
	for (int k = 0; k < count; ++k) {
		const int length = (int) strlen(works[k].topic->name);
		if (width < length) {
			width = length;
		}
	}
	for (int k = 0; k < count; ++k) {
		int topicSessions = 0;
		int topicStudy = 0;
		int firstDay = 0;
		int lastDay = 0;
		for (int e = 0; e < schedule->count; ++e) {
			const Entry * entry = &schedule->entries[e];
			if (entry->topic != works[k].topic || !entry->isSession) {
				continue;
			}
			if (topicSessions++ == 0) {
				firstDay = entry->day;
			}
			lastDay = entry->day;
			topicStudy += entry->duration;
		}
		if (topicSessions == 0) {
			continue;
		}
		char deadline[11];
		formatDate(toDate(firstDay), first);
		formatDate(toDate(lastDay), last);
		formatDate(toDate(urgencies[k].deadline), deadline);
		formatDuration(topicStudy, studyText);
		printf("  %-*s  %3d sesiones (%s), del %s al %s; límite: %s.\n",
			width, works[k].topic->name, topicSessions, studyText, first, last, deadline);
	}
}

/**
 * Moves ahead the deadlines with the constraints "finish(subject) <= date".
 * The subject is every goal ("goal"), every topic ("topic"), or a named goal
 * or topic; inside the date, "goal" is the goal of each constrained topic. A
 * topic can be studied until the day of the date ("<=") or the day before it
 * ("<"), and keeps the tightest deadline.
 */
static void _readDeadlines(const Program * program, const SymbolTable * table, const Work * works, const int count, Urgency * urgencies) {
	for (const DeclarationList * node = program->declarations; node != NULL; node = node->next) {
		if (node->declaration->type != CONSTRAINTS_DECLARATION) {
			continue;
		}
		for (const ConstraintList * item = node->declaration->constraints; item != NULL; item = item->next) {
			const Expression * constraint = item->constraint;
			const Factor * function = constraint->leftExpression->factor;
			if (strcmp(function->function, "finish") != 0) {
				continue;
			}
			const char * name = function->arguments->expression->factor->variable;
			const bool isEvery = (strcmp(name, "goal") == 0 || strcmp(name, "topic") == 0);
			const Symbol * subject = isEvery ? NULL : findSymbol(table, GOAL_SYMBOL, name);
			for (int k = 0; k < count; ++k) {
				const Symbol * goal = works[k].topic->owner;
				if (!isEvery && subject != goal && !_isWithin(works[k].topic, subject)) {
					continue;
				}
				int minutes = 0;
				if (!_evaluateDate(constraint->rightExpression, goal, table, &minutes)) {
					continue;
				}
				if (constraint->type == COMPARE_LESS) {
					--minutes;
				}
				const int deadline = _floorDivision(minutes, MINUTES_PER_DAY);
				if (deadline < urgencies[k].deadline) {
					urgencies[k].deadline = deadline;
					urgencies[k].isConstrained = true;
					urgencies[k].constraint = constraint->position;
				}
			}
		}
	}
}

/**
 * Reads the limits of the constraints: the topics and the load of a day or a
 * week. The analysis guarantees their shape ("function(period) <= value"),
 * and a strict limit ("<") is one less than its value.
 */
static void _readLimits(const Program * program, Settings * settings) {
	for (const DeclarationList * node = program->declarations; node != NULL; node = node->next) {
		if (node->declaration->type != CONSTRAINTS_DECLARATION) {
			continue;
		}
		for (const ConstraintList * item = node->declaration->constraints; item != NULL; item = item->next) {
			const Expression * constraint = item->constraint;
			const Factor * function = constraint->leftExpression->factor;
			int value = 0;
			if (strcmp(function->function, "finish") == 0 || !evaluateConstant(constraint->rightExpression, &value)) {
				continue;
			}
			if (constraint->type == COMPARE_LESS) {
				--value;
			}
			const bool isDay = strcmp(function->arguments->expression->factor->variable, "day") == 0;
			int * limit = NULL;
			if (strcmp(function->function, "topics") == 0) {
				limit = isDay ? &settings->topicsPerDay : &settings->topicsPerWeek;
			}
			else {
				limit = isDay ? &settings->loadPerDay : &settings->loadPerWeek;
			}
			if (value < *limit) {
				*limit = value;
			}
		}
	}
}

/**
 * Reads the settings of the plan. Without "until", or with "until:
 * deadlines", the plan lasts until the latest deadline of its goals.
 */
static Settings _readSettings(const Program * program, const Urgency * urgencies, const int count, int * dayStart) {
	Settings settings = {
		.isPriorityFirst = false,
		.topicsPerDay = UNLIMITED,
		.topicsPerWeek = UNLIMITED,
		.loadPerDay = UNLIMITED,
		.loadPerWeek = UNLIMITED
	};
	*dayStart = DEFAULT_DAY_START;
	bool hasUntil = false;
	const Declaration * plan = _findDeclaration(program, PLAN_DECLARATION);
	for (const AttributeList * node = plan->plan; node != NULL; node = node->next) {
		const char * name = node->attribute->name;
		const Factor * value = node->attribute->values->expression->factor;
		if (strcmp(name, "from") == 0) {
			settings.firstDay = toDayNumber(value->constant->date);
		}
		else if (strcmp(name, "until") == 0 && value->type == CONSTANT_FACTOR) {
			settings.lastDay = toDayNumber(value->constant->date);
			hasUntil = true;
		}
		else if (strcmp(name, "strategy") == 0) {
			settings.isPriorityFirst = (strcmp(value->variable, "priorityFirst") == 0);
		}
		else if (strcmp(name, "start") == 0) {
			*dayStart = value->constant->time;
		}
	}
	if (!hasUntil) {
		settings.lastDay = settings.firstDay - 1;
		for (int k = 0; k < count; ++k) {
			if (settings.lastDay < urgencies[k].deadline) {
				settings.lastDay = urgencies[k].deadline;
			}
		}
	}
	_readLimits(program, &settings);
	return settings;
}

/**
 * Reads the deadline and the priority of the goal of every work.
 */
static Urgency * _readUrgencies(const SymbolTable * table, const Work * works, const int count) {
	Urgency * urgencies = calloc((count == 0) ? 1 : count, sizeof(Urgency));
	for (int k = 0; k < count; ++k) {
		const Symbol * goal = works[k].topic->owner;
		const Attribute * priority = _findAttribute(goal->goal->members, "priority");
		urgencies[k].deadline = _dueDay(goal);
		urgencies[k].priority = -1;
		if (priority != NULL) {
			const Symbol * level = findSymbol(table, LEVEL_SYMBOL, priority->values->expression->factor->variable);
			urgencies[k].priority = level->rank;
		}
	}
	return urgencies;
}

/* PUBLIC FUNCTIONS */

CompilationStatus executePlanning(const Program * program, const SymbolTable * table, Schedule ** result) {
	*result = NULL;
	if (_findDeclaration(program, PLAN_DECLARATION) == NULL) {
		logDebugging(_logger, "The program has no plan to schedule.");
		return SUCCEEDED;
	}
	logDebugging(_logger, "Planning...");
	resetPlanningErrors();
	int count = 0;
	Work * works = expandWorks(program->declarations, table, &count);
	for (int k = 0; k < count; ++k) {
		_logWork(&works[k]);
	}
	if (planningErrorCount() == 0) {
		Urgency * urgencies = _readUrgencies(table, works, count);
		_readDeadlines(program, table, works, count, urgencies);
		int dayStart = DEFAULT_DAY_START;
		const Settings settings = _readSettings(program, urgencies, count, &dayStart);
		WeeklyAvailability * availability = createWeeklyAvailability(_findDeclaration(program, AVAILABILITY_DECLARATION)->availability, dayStart);
		Schedule * schedule = assignWorks(table, works, count, urgencies, availability, &settings);
		_logSchedule(schedule);
		if (planningErrorCount() == 0) {
			_printSummary(schedule, works, count, urgencies);
			*result = schedule;
		}
		else {
			destroySchedule(schedule);
		}
		destroyWeeklyAvailability(availability);
		free(urgencies);
	}
	destroyWorks(works, count);
	const unsigned int errors = planningErrorCount();
	logDebugging(_logger, "Planning is done (errors=%u).", errors);
	return (errors == 0) ? SUCCEEDED : FAILED;
}
