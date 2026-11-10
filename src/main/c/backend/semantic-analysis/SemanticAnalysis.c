#include "SemanticAnalysis.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** The amount of errors reported by the current analysis. */
static unsigned int _errors = 0;

/** Shutdown module's internal state. */
void _shutdownSemanticAnalysisModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: SemanticAnalysis...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeSemanticAnalysisModule() {
	_logger = createLogger("SemanticAnalysis");
	return _shutdownSemanticAnalysisModule;
}

/* PRIVATE FUNCTIONS */

static const Method * _findPreviousMethod(const DeclarationList * declarations, const Method * method);
static void _reportSemanticError(const Position position, const char * const format, ...);
static void _validateMethod(const DeclarationList * declarations, const Method * method);
static void _validateMethodName(const DeclarationList * declarations, const Method * method);
static void _validateParameters(const Method * method);

/**
 * Finds a method with the same name, declared before the specified one. The
 * libraries are included in place, so "before" follows the order in which
 * the declarations are read.
 */
static const Method * _findPreviousMethod(const DeclarationList * declarations, const Method * method) {
	for (const DeclarationList * node = declarations; node != NULL; node = node->next) {
		if (node->declaration->type == METHOD_DECLARATION) {
			const Method * other = node->declaration->method;
			if (other == method) {
				return NULL;
			}
			if (strcmp(other->name, method->name) == 0) {
				return other;
			}
		}
	}
	return NULL;
}

/**
 * Reports a semantic error to the user, in the standard error output and with
 * the file (only when it isn't the main program) and the position of the
 * symbol that causes it.
 */
static void _reportSemanticError(const Position position, const char * const format, ...) {
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

static void _validateMethod(const DeclarationList * declarations, const Method * method) {
	_validateMethodName(declarations, method);
	_validateParameters(method);
}

/**
 * The name of a method starts with an uppercase letter, and it's unique. A
 * method of a library can't be redefined, because its body is what makes it
 * that method (e.g., a "Pomodoro" without short breaks is not a Pomodoro):
 * it's customized through its arguments instead. When a library is included
 * after the user declared the same name, the error is reported at the user's
 * declaration anyway, because that's the one the user can change.
 */
static void _validateMethodName(const DeclarationList * declarations, const Method * method) {
	if (!isupper((unsigned char) method->name[0])) {
		_reportSemanticError(method->position,
			"el método '%s' tiene que empezar con mayúscula (por ejemplo, '%c%s').",
			method->name, toupper((unsigned char) method->name[0]), method->name + 1);
	}
	const Method * previous = _findPreviousMethod(declarations, method);
	if (previous == NULL) {
		return;
	}
	if (previous->position.file != NULL && method->position.file == NULL) {
		_reportSemanticError(method->position,
			"'%s' ya está definido en %s y no se puede redefinir; se personaliza con sus argumentos.",
			method->name, previous->position.file);
	}
	else if (previous->position.file == NULL && method->position.file != NULL) {
		_reportSemanticError(previous->position,
			"'%s' ya está definido en %s y no se puede redefinir; se personaliza con sus argumentos.",
			method->name, method->position.file);
	}
	else if (previous->position.file != NULL) {
		_reportSemanticError(method->position,
			"el método '%s' ya está declarado en %s (línea %d).",
			method->name, previous->position.file, previous->position.line);
	}
	else {
		_reportSemanticError(method->position,
			"el método '%s' ya está declarado en la línea %d.",
			method->name, previous->position.line);
	}
}

/**
 * The names of the parameters are unique, and the parameters with a default
 * value are the last ones: an invocation fills the parameters in order and
 * omits the trailing ones, so a required parameter after an optional one
 * could never be omitted.
 */
static void _validateParameters(const Method * method) {
	bool hasDefaultValue = false;
	for (const ParameterList * node = method->parameters; node != NULL; node = node->next) {
		const Parameter * parameter = node->parameter;
		for (const ParameterList * other = method->parameters; other != node; other = other->next) {
			if (strcmp(other->parameter->name, parameter->name) == 0) {
				_reportSemanticError(parameter->position,
					"el parámetro '%s' está repetido en el método '%s'.",
					parameter->name, method->name);
				break;
			}
		}
		if (parameter->defaultValue != NULL) {
			hasDefaultValue = true;
		}
		else if (hasDefaultValue) {
			_reportSemanticError(parameter->position,
				"el parámetro '%s' necesita un valor por defecto, porque está después de uno que lo tiene.",
				parameter->name);
		}
	}
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeSemanticAnalysis(Program * program) {
	logDebugging(_logger, "Analyzing...");
	_errors = 0;
	for (const DeclarationList * node = program->declarations; node != NULL; node = node->next) {
		if (node->declaration->type == METHOD_DECLARATION) {
			_validateMethod(program->declarations, node->declaration->method);
		}
	}
	logDebugging(_logger, "Analysis is done (errors=%u).", _errors);
	return (_errors == 0) ? SUCCEEDED : FAILED;
}
