#include "SemanticError.h"

/* MODULE INTERNAL STATE */

/** The amount of errors reported since the last restart. */
static unsigned int _errors = 0;

/* PUBLIC FUNCTIONS */

const char * describePosition(const Position position) {
	static char description[256];
	if (position.file != NULL) {
		snprintf(description, sizeof(description), "en %s (línea %d)", position.file, position.line);
	}
	else {
		snprintf(description, sizeof(description), "en la línea %d", position.line);
	}
	return description;
}

void reportSemanticError(const Position position, const char * const format, ...) {
	if (position.file != NULL) {
		fprintf(stderr, "Error semántico en %s (%d:%d): ", position.file, position.line, position.column);
	}
	else {
		fprintf(stderr, "Error semántico (%d:%d): ", position.line, position.column);
	}
	va_list arguments;
	va_start(arguments, format);
	vfprintf(stderr, format, arguments);
	va_end(arguments);
	fprintf(stderr, "\n");
	++_errors;
}

void resetSemanticErrors() {
	_errors = 0;
}

unsigned int semanticErrorCount() {
	return _errors;
}
