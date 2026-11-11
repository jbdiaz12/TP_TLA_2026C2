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

static bool _hasPlan(const Program * program);
static void _logWork(const Work * work);

static bool _hasPlan(const Program * program) {
	for (const DeclarationList * node = program->declarations; node != NULL; node = node->next) {
		if (node->declaration->type == PLAN_DECLARATION) {
			return true;
		}
	}
	return false;
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

/* PUBLIC FUNCTIONS */

CompilationStatus executePlanning(const Program * program, const SymbolTable * table) {
	if (!_hasPlan(program)) {
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
	destroyWorks(works, count);
	const unsigned int errors = planningErrorCount();
	logDebugging(_logger, "Planning is done (errors=%u).", errors);
	return (errors == 0) ? SUCCEEDED : FAILED;
}
