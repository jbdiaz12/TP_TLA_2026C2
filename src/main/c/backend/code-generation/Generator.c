#include "Generator.h"

/* MODULE INTERNAL STATE */

#define MAXIMUM_PATH_LENGTH 4096

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownGeneratorModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Generator...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeGeneratorModule() {
	_logger = createLogger("Generator");
	return _shutdownGeneratorModule;
}

/* PRIVATE FUNCTIONS */

static void _reportGenerationError(const char * path);

static void _reportGenerationError(const char * path) {
	fprintf(stderr, "Error de generación: no se pudo escribir '%s'.\n", path);
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeGenerator(const Schedule * schedule, const SymbolTable * table) {
	logDebugging(_logger, "Generating...");
	const char * folder = getStringOrDefault("STUJFY_OUTPUT_PATH", "out");
	if (mkdir(folder, 0755) != 0 && errno != EEXIST) {
		_reportGenerationError(folder);
		return FAILED;
	}
	char page[MAXIMUM_PATH_LENGTH];
	snprintf(page, sizeof(page), "%s/plan.html", folder);
	if (!generateHtml(schedule, table, page)) {
		_reportGenerationError(page);
		return FAILED;
	}
	char calendar[MAXIMUM_PATH_LENGTH];
	snprintf(calendar, sizeof(calendar), "%s/plan.ics", folder);
	if (!generateICalendar(schedule, calendar)) {
		_reportGenerationError(calendar);
		return FAILED;
	}
	printf("Generé la página '%s' y el calendario '%s'.\n", page, calendar);
	logDebugging(_logger, "Generation is done.");
	return SUCCEEDED;
}
