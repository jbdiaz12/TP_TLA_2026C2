#include "SemanticAnalysis.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** The amount of errors reported by the current analysis. */
static unsigned int _errors = 0;

/** The state of a topic while the dependencies are searched for cycles. */
typedef enum {
	UNVISITED,
	IN_PATH,
	FINISHED
} VisitState;

typedef struct {
	const Symbol * topic;
	VisitState state;
} Visit;

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

static Symbol * _declare(SymbolTable * table, const SymbolKind kind, const char * name, const Position position);
static void _declareMembers(SymbolTable * table, const MemberList * members, Symbol * goal);
static void _declareScale(SymbolTable * table, const Scale * scale);
static void _declareSymbols(SymbolTable * table, const DeclarationList * declarations);
static void _declareTopic(SymbolTable * table, const Topic * topic, Symbol * goal);
static const char * _describe(const Position position);
static const Method * _findPreviousMethod(const DeclarationList * declarations, const Method * method);
static Symbol * _findTopicSymbol(const SymbolTable * table, const Topic * topic);
static void _reportSemanticError(const Position position, const char * const format, ...);
static void _resolveGoals(SymbolTable * table, const DeclarationList * declarations);
static void _setGoal(const SymbolTable * table, const Topic * topic, Symbol * goal);
static int _indexOfVisit(const Visit * visits, const int count, const Symbol * topic);
static bool _isLevelAttribute(const char * name);
static void _reportCycle(const Symbol ** path, const int depth, const Symbol * required, const Position position);
static void _validateCycles(const SymbolTable * table);
static void _visit(const SymbolTable * table, Visit * visits, const int count, const Symbol ** path, const int depth, const int index);
static void _validateLevel(const SymbolTable * table, const Attribute * attribute);
static void _validateLevels(const SymbolTable * table);
static void _validateNestedTopics(const MemberList * members);
static void _validateRequirements(const SymbolTable * table);
static void _validateUniqueBlock(const Declaration ** first, const Declaration * declaration, const char * keyword);
static void _validateMethod(const DeclarationList * declarations, const Method * method);
static void _validateMethodName(const DeclarationList * declarations, const Method * method);
static void _validateParameters(const Method * method);

/**
 * Every topic belongs to a goal, because the deadline of its goal is what
 * schedules it. A topic declared outside of the goals names its goal with
 * "in", and once it's resolved, the topic and the topics nested inside it
 * belong to that goal.
 */
static void _resolveGoals(SymbolTable * table, const DeclarationList * declarations) {
	for (const DeclarationList * node = declarations; node != NULL; node = node->next) {
		const Declaration * declaration = node->declaration;
		if (declaration->type == GOAL_DECLARATION) {
			_validateNestedTopics(declaration->goal->members);
		}
		else if (declaration->type == TOPIC_DECLARATION) {
			const Topic * topic = declaration->topic;
			if (topic->goal == NULL) {
				_reportSemanticError(topic->position,
					"el tema '%s' no pertenece a ningún objetivo: se declara dentro de un 'goal', o como 'topic %s in Objetivo'.",
					topic->name, topic->name);
			}
			else {
				Symbol * goal = findSymbol(table, GOAL_SYMBOL, topic->goal);
				if (goal == NULL) {
					_reportSemanticError(topic->goalPosition, "el objetivo '%s' no está declarado.", topic->goal);
				}
				else if (goal->kind != GOAL_SYMBOL) {
					_reportSemanticError(topic->goalPosition,
						"'%s' es un tema, pero 'in' lleva el objetivo al que pertenece el tema.", topic->goal);
				}
				else {
					_setGoal(table, topic, goal);
				}
			}
			_validateNestedTopics(topic->members);
		}
	}
}

/**
 * Sets the goal of a topic and of every topic nested inside it.
 */
static void _setGoal(const SymbolTable * table, const Topic * topic, Symbol * goal) {
	Symbol * symbol = _findTopicSymbol(table, topic);
	if (symbol != NULL) {
		symbol->owner = goal;
	}
	for (const MemberList * node = topic->members; node != NULL; node = node->next) {
		if (node->member->type == TOPIC_MEMBER) {
			_setGoal(table, node->member->topic, goal);
		}
	}
}

/**
 * Finds the visit of a topic.
 */
static int _indexOfVisit(const Visit * visits, const int count, const Symbol * topic) {
	for (int k = 0; k < count; ++k) {
		if (visits[k].topic == topic) {
			return k;
		}
	}
	return -1;
}

/**
 * Reports a cycle of dependencies, written from the topic where it starts
 * back to that same topic (e.g., "A → B → C → A").
 */
static void _reportCycle(const Symbol ** path, const int depth, const Symbol * required, const Position position) {
	int start = depth;
	while (0 < start && path[start] != required) {
		--start;
	}
	if (start == depth) {
		_reportSemanticError(position, "el tema '%s' se requiere a sí mismo.", required->name);
		return;
	}
	size_t length = strlen(required->name) + 1;
	for (int k = start; k <= depth; ++k) {
		length += strlen(path[k]->name) + strlen(" → ");
	}
	char * cycle = calloc(length, sizeof(char));
	for (int k = start; k <= depth; ++k) {
		strcat(cycle, path[k]->name);
		strcat(cycle, " → ");
	}
	strcat(cycle, required->name);
	_reportSemanticError(position, "las dependencias forman un ciclo: %s.", cycle);
	free(cycle);
}

/**
 * The dependencies between topics form a directed graph, and a cycle in it
 * means that no topic of the cycle can ever be studied first. A depth-first
 * search finds every cycle once: when it reaches a topic that is still in
 * the current path.
 */
static void _validateCycles(const SymbolTable * table) {
	int count = 0;
	for (const Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (symbol->kind == TOPIC_SYMBOL) {
			++count;
		}
	}
	if (count == 0) {
		return;
	}
	Visit * visits = calloc(count, sizeof(Visit));
	const Symbol ** path = calloc(count, sizeof(Symbol *));
	int k = 0;
	for (const Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (symbol->kind == TOPIC_SYMBOL) {
			visits[k++] = (Visit) { .topic = symbol, .state = UNVISITED };
		}
	}
	for (k = 0; k < count; ++k) {
		if (visits[k].state == UNVISITED) {
			_visit(table, visits, count, path, 0, k);
		}
	}
	free(path);
	free(visits);
}

/**
 * Visits a topic and, recursively, the topics it requires.
 */
static void _visit(const SymbolTable * table, Visit * visits, const int count, const Symbol ** path, const int depth, const int index) {
	visits[index].state = IN_PATH;
	path[depth] = visits[index].topic;
	for (const MemberList * node = visits[index].topic->topic->members; node != NULL; node = node->next) {
		if (node->member->type != ATTRIBUTE_MEMBER || strcmp(node->member->attribute->name, "requires") != 0) {
			continue;
		}
		for (const ExpressionList * value = node->member->attribute->values; value != NULL; value = value->next) {
			const Expression * expression = value->expression;
			if (expression->type != FACTOR_EXPRESSION || expression->factor->type != VARIABLE_FACTOR) {
				continue;
			}
			const Symbol * required = findSymbol(table, TOPIC_SYMBOL, expression->factor->variable);
			if (required == NULL || required->kind != TOPIC_SYMBOL) {
				continue;
			}
			const int next = _indexOfVisit(visits, count, required);
			if (visits[next].state == IN_PATH) {
				_reportCycle(path, depth, required, expression->factor->position);
			}
			else if (visits[next].state == UNVISITED) {
				_visit(table, visits, count, path, depth + 1, next);
			}
		}
	}
	visits[index].state = FINISHED;
}

/**
 * Whether an attribute takes a level of a scale as its value.
 */
static bool _isLevelAttribute(const char * name) {
	return strcmp(name, "difficulty") == 0 || strcmp(name, "priority") == 0;
}

/**
 * The value of an attribute like "difficulty" is a single level, declared in
 * any scale. Every level belongs to a single scale, so the level alone tells
 * which scale the attribute uses.
 */
static void _validateLevel(const SymbolTable * table, const Attribute * attribute) {
	if (attribute->values->next != NULL) {
		_reportSemanticError(attribute->position, "'%s' lleva un solo nivel.", attribute->name);
		return;
	}
	const Expression * value = attribute->values->expression;
	if (value->type != FACTOR_EXPRESSION || value->factor->type != VARIABLE_FACTOR) {
		_reportSemanticError(attribute->position,
			"'%s' lleva el nombre de un nivel de una escala (por ejemplo, 'alto').", attribute->name);
		return;
	}
	const Factor * factor = value->factor;
	if (findSymbol(table, LEVEL_SYMBOL, factor->variable) != NULL) {
		return;
	}
	if (findSymbol(table, SCALE_SYMBOL, factor->variable) != NULL) {
		_reportSemanticError(factor->position,
			"'%s' es una escala, pero '%s' lleva uno de sus niveles.", factor->variable, attribute->name);
	}
	else {
		_reportSemanticError(factor->position,
			"el nivel '%s' no está declarado en ninguna escala.", factor->variable);
	}
}

/**
 * Validates the levels of every goal and topic.
 */
static void _validateLevels(const SymbolTable * table) {
	for (const Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		const MemberList * members = NULL;
		if (symbol->kind == GOAL_SYMBOL) {
			members = symbol->goal->members;
		}
		else if (symbol->kind == TOPIC_SYMBOL) {
			members = symbol->topic->members;
		}
		for (const MemberList * node = members; node != NULL; node = node->next) {
			if (node->member->type == ATTRIBUTE_MEMBER && _isLevelAttribute(node->member->attribute->name)) {
				_validateLevel(table, node->member->attribute);
			}
		}
	}
}

/**
 * A nested topic already belongs to the goal of the topic (or goal) that
 * contains it, so it can't name another one with "in".
 */
static void _validateNestedTopics(const MemberList * members) {
	for (const MemberList * node = members; node != NULL; node = node->next) {
		if (node->member->type == TOPIC_MEMBER) {
			const Topic * topic = node->member->topic;
			if (topic->goal != NULL) {
				_reportSemanticError(topic->goalPosition,
					"el tema '%s' está dentro de otro bloque, así que ya pertenece a su objetivo y no lleva 'in'.",
					topic->name);
			}
			_validateNestedTopics(topic->members);
		}
	}
}

/**
 * The values of "requires" are the names of declared topics. A topic can
 * require a topic of another goal, but not a goal itself.
 */
static void _validateRequirements(const SymbolTable * table) {
	for (const Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (symbol->kind != TOPIC_SYMBOL) {
			continue;
		}
		for (const MemberList * node = symbol->topic->members; node != NULL; node = node->next) {
			if (node->member->type != ATTRIBUTE_MEMBER || strcmp(node->member->attribute->name, "requires") != 0) {
				continue;
			}
			const Attribute * attribute = node->member->attribute;
			for (const ExpressionList * value = attribute->values; value != NULL; value = value->next) {
				const Expression * expression = value->expression;
				if (expression->type != FACTOR_EXPRESSION || expression->factor->type != VARIABLE_FACTOR) {
					_reportSemanticError(attribute->position, "'requires' lleva nombres de temas, separados por comas.");
					break;
				}
				const Factor * factor = expression->factor;
				const Symbol * required = findSymbol(table, TOPIC_SYMBOL, factor->variable);
				if (required == NULL) {
					_reportSemanticError(factor->position, "el tema '%s' no está declarado.", factor->variable);
				}
				else if (required->kind == GOAL_SYMBOL) {
					_reportSemanticError(factor->position, "'%s' es un objetivo, pero 'requires' lleva temas.", factor->variable);
				}
			}
		}
	}
}

/**
 * Declares a goal, a topic or a scale, and reports it if its name is already
 * taken. The symbol is added anyway, so the rest of the analysis can go on.
 */
static Symbol * _declare(SymbolTable * table, const SymbolKind kind, const char * name, const Position position) {
	const Symbol * previous = findSymbol(table, kind, name);
	if (previous == NULL) {
		return addSymbol(table, kind, name, position);
	}
	if (previous->kind != kind) {
		_reportSemanticError(position,
			"ya hay %s llamado '%s' %s; los objetivos y los temas no pueden compartir nombre.",
			(previous->kind == GOAL_SYMBOL) ? "un objetivo" : "un tema", name, _describe(previous->position));
	}
	else if (kind == GOAL_SYMBOL) {
		_reportSemanticError(position, "el objetivo '%s' ya está declarado %s.", name, _describe(previous->position));
	}
	else if (kind == TOPIC_SYMBOL) {
		_reportSemanticError(position, "el tema '%s' ya está declarado %s.", name, _describe(previous->position));
	}
	else {
		_reportSemanticError(position, "la escala '%s' ya está declarada %s.", name, _describe(previous->position));
	}
	return addSymbol(table, kind, name, position);
}

/**
 * Declares the topics nested inside a goal or a topic, which belong to the
 * same goal.
 */
static void _declareMembers(SymbolTable * table, const MemberList * members, Symbol * goal) {
	for (const MemberList * node = members; node != NULL; node = node->next) {
		if (node->member->type == TOPIC_MEMBER) {
			_declareTopic(table, node->member->topic, goal);
		}
	}
}

/**
 * Declares a scale and its levels, from the lowest to the highest. A level
 * belongs to a single scale, so an attribute like "difficulty: alto" always
 * identifies the scale it uses.
 */
static void _declareScale(SymbolTable * table, const Scale * scale) {
	Symbol * symbol = _declare(table, SCALE_SYMBOL, scale->name, scale->position);
	symbol->scale = scale;
	int rank = 0;
	for (const LevelList * node = scale->levels; node != NULL; node = node->next) {
		const Symbol * previous = findSymbol(table, LEVEL_SYMBOL, node->level);
		if (previous != NULL && previous->owner == symbol) {
			_reportSemanticError(node->position,
				"el nivel '%s' está repetido en la escala '%s'.", node->level, scale->name);
		}
		else if (previous != NULL) {
			_reportSemanticError(node->position,
				"el nivel '%s' ya pertenece a la escala '%s' (%s); cada nivel pertenece a una sola escala.",
				node->level, previous->owner->name, _describe(previous->position));
		}
		Symbol * level = addSymbol(table, LEVEL_SYMBOL, node->level, node->position);
		level->owner = symbol;
		level->rank = rank++;
	}
}

/**
 * Declares every goal, topic, scale and level of the program, in the order
 * they're written, and checks that the blocks that describe the whole plan
 * are declared only once.
 */
static void _declareSymbols(SymbolTable * table, const DeclarationList * declarations) {
	const Declaration * availability = NULL;
	const Declaration * plan = NULL;
	for (const DeclarationList * node = declarations; node != NULL; node = node->next) {
		const Declaration * declaration = node->declaration;
		switch (declaration->type) {
			case GOAL_DECLARATION: {
				Symbol * goal = _declare(table, GOAL_SYMBOL, declaration->goal->name, declaration->goal->position);
				goal->goal = declaration->goal;
				_declareMembers(table, declaration->goal->members, goal);
				break;
			}
			case TOPIC_DECLARATION:
				_declareTopic(table, declaration->topic, NULL);
				break;
			case SCALE_DECLARATION:
				_declareScale(table, declaration->scale);
				break;
			case AVAILABILITY_DECLARATION:
				_validateUniqueBlock(&availability, declaration, "availability");
				break;
			case PLAN_DECLARATION:
				_validateUniqueBlock(&plan, declaration, "plan");
				break;
			default:
				break;
		}
	}
}

/**
 * Declares a topic and the topics nested inside it. The goal of a topic
 * declared outside of any goal ("topic X in Goal") is resolved later, once
 * every goal is known.
 */
static void _declareTopic(SymbolTable * table, const Topic * topic, Symbol * goal) {
	Symbol * symbol = _declare(table, TOPIC_SYMBOL, topic->name, topic->position);
	symbol->topic = topic;
	symbol->owner = goal;
	_declareMembers(table, topic->members, goal);
}

/**
 * Describes where something was declared, to point at it from a message
 * (e.g., "en la línea 3", or "en <metodos> (línea 3)"). The description is
 * overwritten by the next call, so it's used only once per message.
 */
static const char * _describe(const Position position) {
	static char description[256];
	if (position.file != NULL) {
		snprintf(description, sizeof(description), "en %s (línea %d)", position.file, position.line);
	}
	else {
		snprintf(description, sizeof(description), "en la línea %d", position.line);
	}
	return description;
}

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
 * Finds the symbol of a topic. Two topics can share their name (which is an
 * error, already reported), so the topic is found by its node instead.
 */
static Symbol * _findTopicSymbol(const SymbolTable * table, const Topic * topic) {
	for (Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (symbol->kind == TOPIC_SYMBOL && symbol->topic == topic) {
			return symbol;
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

/**
 * A block that describes the whole plan (e.g., "plan" or "availability") is
 * declared only once, because two of them would contradict each other.
 */
static void _validateUniqueBlock(const Declaration ** first, const Declaration * declaration, const char * keyword) {
	if (*first == NULL) {
		*first = declaration;
	}
	else {
		_reportSemanticError(declaration->position,
			"ya hay un bloque '%s' %s; se declara una sola vez.", keyword, _describe((*first)->position));
	}
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
	SymbolTable * table = createSymbolTable();
	_declareSymbols(table, program->declarations);
	_resolveGoals(table, program->declarations);
	_validateRequirements(table);
	_validateLevels(table);
	_validateCycles(table);
	for (const DeclarationList * node = program->declarations; node != NULL; node = node->next) {
		if (node->declaration->type == METHOD_DECLARATION) {
			_validateMethod(program->declarations, node->declaration->method);
		}
	}
	destroySymbolTable(table);
	logDebugging(_logger, "Analysis is done (errors=%u).", _errors);
	return (_errors == 0) ? SUCCEEDED : FAILED;
}
