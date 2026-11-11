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

static const Attribute * _findAttribute(const MemberList * members, const char * name);
static const Declaration * _findDeclaration(const Program * program, const DeclarationType type);
static void _logSchedule(const Schedule * schedule);
static void _logWork(const Work * work);
static void _readLimits(const Program * program, Settings * settings);
static Settings _readSettings(const Program * program, const Urgency * urgencies, const int count, int * dayStart);
static Urgency * _readUrgencies(const SymbolTable * table, const Work * works, const int count);

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
		const Attribute * due = _findAttribute(goal->goal->members, "due");
		const Attribute * priority = _findAttribute(goal->goal->members, "priority");
		urgencies[k].deadline = toDayNumber(due->values->expression->factor->constant->date);
		urgencies[k].priority = -1;
		if (priority != NULL) {
			const Symbol * level = findSymbol(table, LEVEL_SYMBOL, priority->values->expression->factor->variable);
			urgencies[k].priority = level->rank;
		}
	}
	return urgencies;
}

/* PUBLIC FUNCTIONS */

CompilationStatus executePlanning(const Program * program, const SymbolTable * table) {
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
		int dayStart = DEFAULT_DAY_START;
		const Settings settings = _readSettings(program, urgencies, count, &dayStart);
		WeeklyAvailability * availability = createWeeklyAvailability(_findDeclaration(program, AVAILABILITY_DECLARATION)->availability, dayStart);
		Schedule * schedule = assignWorks(table, works, count, urgencies, availability, &settings);
		_logSchedule(schedule);
		destroySchedule(schedule);
		destroyWeeklyAvailability(availability);
		free(urgencies);
	}
	destroyWorks(works, count);
	const unsigned int errors = planningErrorCount();
	logDebugging(_logger, "Planning is done (errors=%u).", errors);
	return (errors == 0) ? SUCCEEDED : FAILED;
}
