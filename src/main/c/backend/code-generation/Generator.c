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

CompilationStatus executeGenerator(const Schedule * schedule) {
	logDebugging(_logger, "Generating...");
	const char * folder = getStringOrDefault("STUJFY_OUTPUT_PATH", "out");
	if (mkdir(folder, 0755) != 0 && errno != EEXIST) {
		_reportGenerationError(folder);
		return FAILED;
	}
	char path[MAXIMUM_PATH_LENGTH];
	snprintf(path, sizeof(path), "%s/plan.ics", folder);
	if (!generateICalendar(schedule, path)) {
		_reportGenerationError(path);
		return FAILED;
	}
	printf("Generé el calendario '%s'.\n", path);
	logDebugging(_logger, "Generation is done.");
	return SUCCEEDED;
}
