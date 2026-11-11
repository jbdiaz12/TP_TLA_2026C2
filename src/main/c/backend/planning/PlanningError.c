#include "PlanningError.h"

/* MODULE INTERNAL STATE */

/** The amount of errors reported since the last restart. */
static unsigned int _errors = 0;

/* PUBLIC FUNCTIONS */

void reportPlanningError(const Position position, const char * const format, ...) {
	if (position.line == 0) {
		fprintf(stderr, "Error de planificación: ");
	}
	else if (position.file != NULL) {
		fprintf(stderr, "Error de planificación en %s (%d:%d): ", position.file, position.line, position.column);
	}
	else {
		fprintf(stderr, "Error de planificación (%d:%d): ", position.line, position.column);
	}
	va_list arguments;
	va_start(arguments, format);
	vfprintf(stderr, format, arguments);
	va_end(arguments);
	fprintf(stderr, "\n");
	++_errors;
}

void resetPlanningErrors() {
	_errors = 0;
}

unsigned int planningErrorCount() {
	return _errors;
}
