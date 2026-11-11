#include "MethodValidation.h"

/* PRIVATE FUNCTIONS */

static bool _containsSession(const StatementList * statements);
static int _countLoops(const StatementList * statements);
static Type _parameterType(const Parameter * parameter);
static void _validateDefaultValue(const SymbolTable * table, const Parameter * parameter, const Type type);
static void _validateDuration(const Expression * expression, const Scope * scope, const char * keyword, const bool admitsZero);
static void _validateStatement(const SymbolTable * table, const Statement * statement, Variable * variables, const int count);
static void _validateStatements(const SymbolTable * table, const StatementList * statements, Variable * variables, const int count);

/**
 * Whether a body contains at least one session. Without one, a method never
 * advances the study of a topic, so the planner would repeat it forever.
 */
static bool _containsSession(const StatementList * statements) {
	for (const StatementList * node = statements; node != NULL; node = node->next) {
		const Statement * statement = node->statement;
		switch (statement->type) {
			case SESSION_STATEMENT:
				return true;
			case AFTER_STATEMENT: {
				const StatementList delayed = { .statement = statement->delayedStatement, .next = NULL };
				if (_containsSession(&delayed)) {
					return true;
				}
				break;
			}
			case REPEAT_STATEMENT:
				if (_containsSession(statement->repeatBody)) {
					return true;
				}
				break;
			case FOR_STATEMENT:
				if (_containsSession(statement->forBody)) {
					return true;
				}
				break;
			default:
				break;
		}
	}
	return false;
}

/**
 * The amount of loops in a body, which bounds how many variables a body
 * declares besides its parameters.
 */
static int _countLoops(const StatementList * statements) {
	int count = 0;
	for (const StatementList * node = statements; node != NULL; node = node->next) {
		const Statement * statement = node->statement;
		if (statement->type == FOR_STATEMENT) {
			count += 1 + _countLoops(statement->forBody);
		}
		else if (statement->type == REPEAT_STATEMENT) {
			count += _countLoops(statement->repeatBody);
		}
		else if (statement->type == AFTER_STATEMENT) {
			const StatementList delayed = { .statement = statement->delayedStatement, .next = NULL };
			count += _countLoops(&delayed);
		}
	}
	return count;
}

/**
 * The type that a parameter declares. A method describes how the time of a
 * topic is spent, so its parameters are durations, integers (to repeat) or
 * lists of durations (to space the sessions).
 */
static Type _parameterType(const Parameter * parameter) {
	if (strcmp(parameter->type, "duration") == 0) {
		return simpleType(DURATION_TYPE);
	}
	if (strcmp(parameter->type, "integer") == 0) {
		return simpleType(INTEGER_TYPE);
	}
	if (strcmp(parameter->type, "list") == 0) {
		return simpleType(LIST_TYPE);
	}
	reportSemanticError(parameter->position,
		"'%s' no es un tipo de parámetro; los tipos válidos son: duration, integer, list.", parameter->type);
	return simpleType(ERROR_TYPE);
}

/**
 * The default value of a parameter has its type, and it's known while
 * compiling, so it can't read other parameters.
 */
static void _validateDefaultValue(const SymbolTable * table, const Parameter * parameter, const Type type) {
	const Scope scope = {
		.table = table,
		.variables = NULL,
		.variableCount = 0,
		.allowsDomainFunctions = false
	};
	const Type valueType = typeOf(parameter->defaultValue, &scope);
	if (valueType.kind == ERROR_TYPE || type.kind == ERROR_TYPE) {
		return;
	}
	if (valueType.kind != type.kind) {
		const char * expected = describeType(type);
		reportSemanticError(parameter->defaultValue->position,
			"el valor por defecto de '%s' tiene que ser %s, pero es %s.", parameter->name, expected, describeType(valueType));
		return;
	}
	int value = 0;
	if (type.kind == DURATION_TYPE && evaluateConstant(parameter->defaultValue, &value) && value <= 0) {
		reportSemanticError(parameter->defaultValue->position,
			"el valor por defecto de '%s' tiene que ser una duración mayor que cero.", parameter->name);
	}
}

/**
 * The duration of a session, a pause or a delay. A session or a pause lasts
 * more than zero, while a delay can be zero (the delayed session is placed
 * right away). The check of the value is only possible when it doesn't read
 * a parameter.
 */
static void _validateDuration(const Expression * expression, const Scope * scope, const char * keyword, const bool admitsZero) {
	const Type type = typeOf(expression, scope);
	if (type.kind == ERROR_TYPE) {
		return;
	}
	if (type.kind != DURATION_TYPE) {
		reportSemanticError(expression->position, "'%s' lleva una duración, pero este valor es %s.", keyword, describeType(type));
		return;
	}
	int minutes = 0;
	if (evaluateConstant(expression, &minutes) && (minutes < 0 || (minutes == 0 && !admitsZero))) {
		reportSemanticError(expression->position,
			admitsZero ? "'%s' no puede llevar una duración negativa." : "'%s' tiene que llevar una duración mayor que cero.", keyword);
	}
}

/**
 * Validates a statement, which can read the parameters of its method and the
 * variables of the loops that contain it.
 */
static void _validateStatement(const SymbolTable * table, const Statement * statement, Variable * variables, const int count) {
	const Scope scope = {
		.table = table,
		.variables = variables,
		.variableCount = count,
		.allowsDomainFunctions = false
	};
	switch (statement->type) {
		case SESSION_STATEMENT:
			_validateDuration(statement->duration, &scope, "session", false);
			break;
		case PAUSE_STATEMENT:
			_validateDuration(statement->duration, &scope, "pause", false);
			break;
		case AFTER_STATEMENT:
			_validateDuration(statement->delay, &scope, "after", true);
			_validateStatement(table, statement->delayedStatement, variables, count);
			break;
		case REPEAT_STATEMENT: {
			const Type type = typeOf(statement->count, &scope);
			int times = 0;
			if (type.kind != ERROR_TYPE && type.kind != INTEGER_TYPE) {
				reportSemanticError(statement->count->position,
					"'repeat' lleva un entero (la cantidad de repeticiones), pero este valor es %s.", describeType(type));
			}
			else if (type.kind == INTEGER_TYPE && evaluateConstant(statement->count, &times) && times < 1) {
				reportSemanticError(statement->count->position, "'repeat' tiene que repetir al menos una vez.");
			}
			_validateStatements(table, statement->repeatBody, variables, count);
			break;
		}
		case FOR_STATEMENT: {
			const Type type = typeOf(statement->iterable, &scope);
			if (type.kind != ERROR_TYPE && type.kind != LIST_TYPE) {
				reportSemanticError(statement->iterable->position,
					"'for' recorre una lista, pero este valor es %s.", describeType(type));
			}
			for (int k = 0; k < count; ++k) {
				if (strcmp(variables[k].name, statement->variable) == 0) {
					reportSemanticError(statement->position,
						"'%s' ya es una variable del método; la variable del 'for' necesita otro nombre.", statement->variable);
					break;
				}
			}
			variables[count] = (Variable) { .name = statement->variable, .type = simpleType(DURATION_TYPE) };
			_validateStatements(table, statement->forBody, variables, count + 1);
			break;
		}
	}
}

static void _validateStatements(const SymbolTable * table, const StatementList * statements, Variable * variables, const int count) {
	for (const StatementList * node = statements; node != NULL; node = node->next) {
		_validateStatement(table, node->statement, variables, count);
	}
}

/* PUBLIC FUNCTIONS */

void validateMethodBody(const SymbolTable * table, const Method * method) {
	int parameterCount = 0;
	for (const ParameterList * node = method->parameters; node != NULL; node = node->next) {
		++parameterCount;
	}
	const int capacity = parameterCount + _countLoops(method->body);
	Variable * variables = calloc((capacity == 0) ? 1 : capacity, sizeof(Variable));
	int count = 0;
	for (const ParameterList * node = method->parameters; node != NULL; node = node->next) {
		const Type type = _parameterType(node->parameter);
		if (node->parameter->defaultValue != NULL) {
			_validateDefaultValue(table, node->parameter, type);
		}
		variables[count++] = (Variable) { .name = node->parameter->name, .type = type };
	}
	_validateStatements(table, method->body, variables, count);
	if (!_containsSession(method->body)) {
		reportSemanticError(method->position,
			"el método '%s' no tiene ninguna 'session', así que nunca avanzaría el estudio de un tema.", method->name);
	}
	free(variables);
}
