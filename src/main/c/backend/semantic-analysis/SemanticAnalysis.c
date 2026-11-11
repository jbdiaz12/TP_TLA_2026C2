#include "SemanticAnalysis.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

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

/** The attributes that every block admits, in alphabetical order. */
static const char * const _GOAL_ATTRIBUTES[] = { "description", "due", "priority", NULL };
static const char * const _PLAN_ATTRIBUTES[] = { "from", "strategy", "until", NULL };
static const char * const _TOPIC_ATTRIBUTES[] = { "description", "difficulty", "estimated", "fixed", "method", "requires", NULL };

/**
 * The days of the availability: a single day, or a set of them. When a day
 * belongs to more than one declared set, the most specific one wins (e.g.,
 * "sunday" wins over "every"), which is resolved by the planner.
 */
static const char * const _DAYS[] = {
	"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday",
	"weekdays", "weekend", "every", NULL
};

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
static const Method * _findPreviousMethod(const DeclarationList * declarations, const Method * method);
static Symbol * _findTopicSymbol(const SymbolTable * table, const Topic * topic);
static void _resolveGoals(SymbolTable * table, const DeclarationList * declarations);
static void _setGoal(const SymbolTable * table, const Topic * topic, Symbol * goal);
static bool _contains(const char * const * names, const char * name);
static int _indexOfVisit(const Visit * visits, const int count, const Symbol * topic);
static char * _join(const char * const * names);
static bool _isLevelAttribute(const char * name);
static void _reportCycle(const Symbol ** path, const int depth, const Symbol * required, const Position position);
static void _validateAttributes(const Attribute ** attributes, const int count, const char * const * allowed, const char * owner);
static void _validateAvailability(const Availability * availability, const Scope * scope);
static bool _timeOf(const Expression * expression, const Scope * scope, int * minutes);
static void _validatePeriods(const Slot * slot, const Scope * scope);
static void _validateBlocks(const SymbolTable * table, const DeclarationList * declarations);
static void _validateConstraints(const SymbolTable * table, const DeclarationList * declarations);
static void _validateCycles(const SymbolTable * table);
static void _validateInvocations(const SymbolTable * table, const DeclarationList * declarations);
static void _validateMemberAttributes(const MemberList * members, const char * const * allowed, const char * owner);
static void _validatePlanAttributes(const AttributeList * list);
static bool _hasType(const Attribute * attribute, const Expression * value, const TypeKind expected, const Scope * scope);
static const Expression * _singleValue(const Attribute * attribute);
static void _validateFixedSession(const Attribute * attribute, const Scope * scope);
static void _validatePlanRange(const AttributeList * list);
static void _validatePositiveDuration(const Attribute * attribute, const Expression * value);
static void _validateValues(const Attribute * attribute, const Scope * scope);
static void _visit(const SymbolTable * table, Visit * visits, const int count, const Symbol ** path, const int depth, const int index);
static void _validateLevel(const SymbolTable * table, const Attribute * attribute);
static void _validateLevels(const SymbolTable * table);
static void _validateNestedTopics(const MemberList * members);
static void _validateRequirements(const SymbolTable * table);
static void _validateUniqueBlock(const Declaration ** first, const Declaration * declaration, const char * keyword);
static void _validateMethod(const SymbolTable * table, const DeclarationList * declarations, const Method * method);
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
				reportSemanticError(topic->position,
					"el tema '%s' no pertenece a ningún objetivo: se declara dentro de un 'goal', o como 'topic %s in Objetivo'.",
					topic->name, topic->name);
			}
			else {
				Symbol * goal = findSymbol(table, GOAL_SYMBOL, topic->goal);
				if (goal == NULL) {
					reportSemanticError(topic->goalPosition, "el objetivo '%s' no está declarado.", topic->goal);
				}
				else if (goal->kind != GOAL_SYMBOL) {
					reportSemanticError(topic->goalPosition,
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
 * Whether a list of names, ended by NULL, contains a name.
 */
static bool _contains(const char * const * names, const char * name) {
	for (int k = 0; names[k] != NULL; ++k) {
		if (strcmp(names[k], name) == 0) {
			return true;
		}
	}
	return false;
}

/**
 * Joins a list of names, ended by NULL, separated by commas. The result must
 * be freed.
 */
static char * _join(const char * const * names) {
	size_t length = 1;
	for (int k = 0; names[k] != NULL; ++k) {
		length += strlen(names[k]) + strlen(", ");
	}
	char * joined = calloc(length, sizeof(char));
	for (int k = 0; names[k] != NULL; ++k) {
		if (0 < k) {
			strcat(joined, ", ");
		}
		strcat(joined, names[k]);
	}
	return joined;
}

/**
 * The attributes of a block are the ones that block admits, and each of them
 * is declared once: a second "estimated" would contradict the first, and a
 * second "requires" is written as more values of the first one.
 */
static void _validateAttributes(const Attribute ** attributes, const int count, const char * const * allowed, const char * owner) {
	for (int k = 0; k < count; ++k) {
		const Attribute * attribute = attributes[k];
		if (!_contains(allowed, attribute->name)) {
			char * names = _join(allowed);
			reportSemanticError(attribute->position,
				"'%s' no es un atributo %s; los atributos válidos son: %s.", attribute->name, owner, names);
			free(names);
			continue;
		}
		for (int previous = 0; previous < k; ++previous) {
			if (strcmp(attributes[previous]->name, attribute->name) == 0) {
				reportSemanticError(attribute->position,
					"el atributo '%s' ya está declarado %s.", attribute->name, describePosition(attributes[previous]->position));
				break;
			}
		}
	}
}

/**
 * Every slot of the availability names a day, or a set of days, only once,
 * and its periods make sense in a single day.
 */
static void _validateAvailability(const Availability * availability, const Scope * scope) {
	for (const SlotList * node = availability->slots; node != NULL; node = node->next) {
		const Slot * slot = node->slot;
		if (!_contains(_DAYS, slot->day)) {
			char * days = _join(_DAYS);
			reportSemanticError(slot->position, "'%s' no es un día; los días válidos son: %s.", slot->day, days);
			free(days);
			continue;
		}
		for (const SlotList * previous = availability->slots; previous != node; previous = previous->next) {
			if (strcmp(previous->slot->day, slot->day) == 0) {
				reportSemanticError(slot->position,
					"'%s' ya está declarado %s.", slot->day, describePosition(previous->slot->position));
				break;
			}
		}
		_validatePeriods(slot, scope);
	}
}

/**
 * The time of one end of a window, which is a time written as a constant
 * (e.g., "18:00"). It reports any other value.
 */
static bool _timeOf(const Expression * expression, const Scope * scope, int * minutes) {
	const Type type = typeOf(expression, scope);
	if (type.kind == ERROR_TYPE) {
		return false;
	}
	if (type.kind != TIME_TYPE) {
		reportSemanticError(expression->position,
			"una ventana horaria va de una hora a otra (por ejemplo, '18:00 .. 21:00'), pero este extremo es %s.", describeType(type));
		return false;
	}
	if (expression->type != FACTOR_EXPRESSION || expression->factor->type != CONSTANT_FACTOR) {
		return false;
	}
	*minutes = expression->factor->constant->time;
	return true;
}

/**
 * The periods of a day are either time windows (e.g., "18:00 .. 21:00"),
 * which don't overlap, or a single amount of time (e.g., "2h"), which the
 * planner places anywhere in that day. A day doesn't mix both, because the
 * amount would be ambiguous: inside the windows, or besides them?
 */
static void _validatePeriods(const Slot * slot, const Scope * scope) {
	int windowCount = 0;
	int durationCount = 0;
	for (const PeriodList * node = slot->periods; node != NULL; node = node->next) {
		if (node->period->type == RANGE_PERIOD) {
			++windowCount;
		}
		else {
			++durationCount;
		}
	}
	if (0 < windowCount && 0 < durationCount) {
		reportSemanticError(slot->position, "'%s' lleva ventanas horarias o una duración, pero no las dos.", slot->day);
		return;
	}
	if (1 < durationCount) {
		reportSemanticError(slot->position, "'%s' lleva una sola duración.", slot->day);
		return;
	}
	if (durationCount == 1) {
		const Expression * amount = slot->periods->period->amount;
		const Type type = typeOf(amount, scope);
		if (type.kind != ERROR_TYPE && type.kind != DURATION_TYPE) {
			reportSemanticError(amount->position,
				"'%s' lleva una duración o ventanas horarias, pero este valor es %s.", slot->day, describeType(type));
			return;
		}
		int minutes = 0;
		if (type.kind == DURATION_TYPE && evaluateConstant(amount, &minutes)) {
			if (minutes <= 0) {
				reportSemanticError(amount->position, "'%s' tiene que tener una duración mayor que cero.", slot->day);
			}
			else if (24 * 60 < minutes) {
				reportSemanticError(amount->position, "un día tiene 24 horas, pero '%s' declara más.", slot->day);
			}
		}
		return;
	}
	int * starts = calloc(windowCount, sizeof(int));
	int * ends = calloc(windowCount, sizeof(int));
	int valid = 0;
	for (const PeriodList * node = slot->periods; node != NULL; node = node->next) {
		const Period * period = node->period;
		int start = 0;
		int end = 0;
		if (!_timeOf(period->from, scope, &start) || !_timeOf(period->to, scope, &end)) {
			continue;
		}
		if (end <= start) {
			reportSemanticError(period->from->position,
				"la ventana %02d:%02d .. %02d:%02d termina antes de empezar.", start / 60, start % 60, end / 60, end % 60);
			continue;
		}
		for (int k = 0; k < valid; ++k) {
			if (start < ends[k] && starts[k] < end) {
				reportSemanticError(period->from->position,
					"la ventana %02d:%02d .. %02d:%02d se superpone con %02d:%02d .. %02d:%02d.",
					start / 60, start % 60, end / 60, end % 60,
					starts[k] / 60, starts[k] % 60, ends[k] / 60, ends[k] % 60);
				break;
			}
		}
		starts[valid] = start;
		ends[valid] = end;
		++valid;
	}
	free(starts);
	free(ends);
}

/**
 * Whether the value of an attribute has the expected type. The value has no
 * variables to read, and it can't call the functions of the domain.
 */
static bool _hasType(const Attribute * attribute, const Expression * value, const TypeKind expected, const Scope * scope) {
	const Type type = typeOf(value, scope);
	if (type.kind == ERROR_TYPE) {
		return false;
	}
	if (type.kind != expected) {
		const char * expectedDescription = describeType(simpleType(expected));
		reportSemanticError(value->position, "'%s' lleva %s, pero este valor es %s.",
			attribute->name, expectedDescription, describeType(type));
		return false;
	}
	return true;
}

/**
 * The value of an attribute that admits a single one, or NULL (after
 * reporting it) if the attribute has more than one.
 */
static const Expression * _singleValue(const Attribute * attribute) {
	if (attribute->values->next != NULL) {
		reportSemanticError(attribute->position, "'%s' lleva un solo valor.", attribute->name);
		return NULL;
	}
	return attribute->values->expression;
}

/**
 * A fixed session is a date, the time when it starts and its duration, and
 * the planner never moves it.
 */
static void _validateFixedSession(const Attribute * attribute, const Scope * scope) {
	const ExpressionList * values = attribute->values;
	if (values->next == NULL || values->next->next == NULL || values->next->next->next != NULL) {
		reportSemanticError(attribute->position,
			"'fixed' lleva una fecha, una hora y una duración (por ejemplo, 'fixed: 2026-11-20, 18:00, 90m;').");
		return;
	}
	_hasType(attribute, values->expression, DATE_TYPE, scope);
	_hasType(attribute, values->next->expression, TIME_TYPE, scope);
	if (_hasType(attribute, values->next->next->expression, DURATION_TYPE, scope)) {
		_validatePositiveDuration(attribute, values->next->next->expression);
	}
}

/**
 * When the plan sets both of its limits as dates, the end comes after the
 * beginning.
 */
static void _validatePlanRange(const AttributeList * list) {
	const Expression * from = NULL;
	const Expression * until = NULL;
	for (const AttributeList * node = list; node != NULL; node = node->next) {
		const Attribute * attribute = node->attribute;
		if (attribute->values->next != NULL) {
			continue;
		}
		if (strcmp(attribute->name, "from") == 0) {
			from = attribute->values->expression;
		}
		else if (strcmp(attribute->name, "until") == 0) {
			until = attribute->values->expression;
		}
	}
	const bool areDates = from != NULL && until != NULL
		&& from->type == FACTOR_EXPRESSION && from->factor->type == CONSTANT_FACTOR && from->factor->constant->type == DATE_CONSTANT
		&& until->type == FACTOR_EXPRESSION && until->factor->type == CONSTANT_FACTOR && until->factor->constant->type == DATE_CONSTANT;
	if (areDates && until->factor->constant->date <= from->factor->constant->date) {
		reportSemanticError(until->position, "'until' tiene que ser posterior a 'from'.");
	}
}

/**
 * A duration that measures time to study (or to rest) is greater than zero.
 * Its value is known while compiling unless it reads a variable, such as the
 * parameter of a method, whose value comes from each invocation.
 */
static void _validatePositiveDuration(const Attribute * attribute, const Expression * value) {
	int minutes = 0;
	if (evaluateConstant(value, &minutes) && minutes <= 0) {
		reportSemanticError(value->position,
			"'%s' tiene que ser una duración mayor que cero, pero da %d minutos.", attribute->name, minutes);
	}
}

/**
 * Validates the values of an attribute, according to its name. The levels
 * and the requirements are validated on their own, and the methods together
 * with their invocations.
 */
static void _validateValues(const Attribute * attribute, const Scope * scope) {
	const char * name = attribute->name;
	if (strcmp(name, "fixed") == 0) {
		_validateFixedSession(attribute, scope);
		return;
	}
	const bool isDate = (strcmp(name, "due") == 0 || strcmp(name, "from") == 0);
	const bool isText = (strcmp(name, "description") == 0);
	const bool isEstimation = (strcmp(name, "estimated") == 0);
	const bool isEnd = (strcmp(name, "until") == 0);
	const bool isStrategy = (strcmp(name, "strategy") == 0);
	if (!isDate && !isText && !isEstimation && !isEnd && !isStrategy) {
		return;
	}
	const Expression * value = _singleValue(attribute);
	if (value == NULL) {
		return;
	}
	const char * word = (value->type == FACTOR_EXPRESSION && value->factor->type == VARIABLE_FACTOR) ? value->factor->variable : NULL;
	if (isStrategy) {
		if (word == NULL || (strcmp(word, "earliestDeadlineFirst") != 0 && strcmp(word, "priorityFirst") != 0)) {
			reportSemanticError(value->position, "'strategy' lleva earliestDeadlineFirst o priorityFirst.");
		}
	}
	else if (isEnd && word != NULL && strcmp(word, "deadlines") == 0) {
		return;
	}
	else if (isDate || isEnd) {
		_hasType(attribute, value, DATE_TYPE, scope);
	}
	else if (isText) {
		_hasType(attribute, value, STRING_TYPE, scope);
	}
	else if (_hasType(attribute, value, DURATION_TYPE, scope)) {
		_validatePositiveDuration(attribute, value);
	}
}

/**
 * Validates the attributes of every goal, topic and plan, and the days of
 * the availability.
 */
static void _validateBlocks(const SymbolTable * table, const DeclarationList * declarations) {
	const Scope scope = {
		.table = table,
		.variables = NULL,
		.variableCount = 0,
		.allowsDomainFunctions = false
	};
	for (const Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (symbol->kind == GOAL_SYMBOL) {
			_validateMemberAttributes(symbol->goal->members, _GOAL_ATTRIBUTES, "de un objetivo");
			for (const MemberList * member = symbol->goal->members; member != NULL; member = member->next) {
				if (member->member->type == ATTRIBUTE_MEMBER) {
					_validateValues(member->member->attribute, &scope);
				}
			}
		}
		else if (symbol->kind == TOPIC_SYMBOL) {
			_validateMemberAttributes(symbol->topic->members, _TOPIC_ATTRIBUTES, "de un tema");
			for (const MemberList * member = symbol->topic->members; member != NULL; member = member->next) {
				if (member->member->type == ATTRIBUTE_MEMBER) {
					_validateValues(member->member->attribute, &scope);
				}
			}
		}
	}
	for (const DeclarationList * node = declarations; node != NULL; node = node->next) {
		if (node->declaration->type == PLAN_DECLARATION) {
			_validatePlanAttributes(node->declaration->plan);
			for (const AttributeList * attribute = node->declaration->plan; attribute != NULL; attribute = attribute->next) {
				_validateValues(attribute->attribute, &scope);
			}
			_validatePlanRange(node->declaration->plan);
		}
		else if (node->declaration->type == AVAILABILITY_DECLARATION) {
			_validateAvailability(node->declaration->availability, &scope);
		}
	}
}

/**
 * Validates the attributes of a goal or a topic, which are mixed with the
 * topics nested inside it.
 */
static void _validateMemberAttributes(const MemberList * members, const char * const * allowed, const char * owner) {
	int count = 0;
	for (const MemberList * node = members; node != NULL; node = node->next) {
		if (node->member->type == ATTRIBUTE_MEMBER) {
			++count;
		}
	}
	if (count == 0) {
		return;
	}
	const Attribute ** attributes = calloc(count, sizeof(Attribute *));
	int k = 0;
	for (const MemberList * node = members; node != NULL; node = node->next) {
		if (node->member->type == ATTRIBUTE_MEMBER) {
			attributes[k++] = node->member->attribute;
		}
	}
	_validateAttributes(attributes, count, allowed, owner);
	free(attributes);
}

/**
 * Validates the attributes of the plan.
 */
static void _validatePlanAttributes(const AttributeList * list) {
	int count = 0;
	for (const AttributeList * node = list; node != NULL; node = node->next) {
		++count;
	}
	if (count == 0) {
		return;
	}
	const Attribute ** attributes = calloc(count, sizeof(Attribute *));
	int k = 0;
	for (const AttributeList * node = list; node != NULL; node = node->next) {
		attributes[k++] = node->attribute;
	}
	_validateAttributes(attributes, count, _PLAN_ATTRIBUTES, "del plan");
	free(attributes);
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
		reportSemanticError(position, "el tema '%s' se requiere a sí mismo.", required->name);
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
	reportSemanticError(position, "las dependencias forman un ciclo: %s.", cycle);
	free(cycle);
}

/**
 * Every constraint is a condition over the resulting schedule, written with
 * the functions of the domain (e.g., "load(week) <= 20h").
 */
static void _validateConstraints(const SymbolTable * table, const DeclarationList * declarations) {
	const Scope scope = {
		.table = table,
		.variables = NULL,
		.variableCount = 0,
		.allowsDomainFunctions = true
	};
	for (const DeclarationList * node = declarations; node != NULL; node = node->next) {
		if (node->declaration->type != CONSTRAINTS_DECLARATION) {
			continue;
		}
		for (const ConstraintList * constraint = node->declaration->constraints; constraint != NULL; constraint = constraint->next) {
			const Type type = typeOf(constraint->constraint, &scope);
			if (type.kind != ERROR_TYPE && type.kind != BOOLEAN_TYPE) {
				reportSemanticError(constraint->constraint->position,
					"una restricción es una condición (por ejemplo, 'load(week) <= 20h'), pero esta es %s.", describeType(type));
			}
		}
	}
}

/**
 * Validates every invocation of a method: the ones that assign a method to a
 * topic directly, and the actions of the rules.
 */
static void _validateInvocations(const SymbolTable * table, const DeclarationList * declarations) {
	for (const Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (symbol->kind != TOPIC_SYMBOL) {
			continue;
		}
		for (const MemberList * node = symbol->topic->members; node != NULL; node = node->next) {
			if (node->member->type == ATTRIBUTE_MEMBER && strcmp(node->member->attribute->name, "method") == 0) {
				const Expression * value = _singleValue(node->member->attribute);
				if (value != NULL) {
					validateInvocation(table, declarations, value, "'method'");
				}
			}
		}
	}
	for (const DeclarationList * node = declarations; node != NULL; node = node->next) {
		if (node->declaration->type != RULES_DECLARATION) {
			continue;
		}
		for (const RuleList * rule = node->declaration->rules; rule != NULL; rule = rule->next) {
			validateInvocation(table, declarations, rule->rule->action, "la acción de una regla");
		}
	}
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
		reportSemanticError(attribute->position, "'%s' lleva un solo nivel.", attribute->name);
		return;
	}
	const Expression * value = attribute->values->expression;
	if (value->type != FACTOR_EXPRESSION || value->factor->type != VARIABLE_FACTOR) {
		reportSemanticError(attribute->position,
			"'%s' lleva el nombre de un nivel de una escala (por ejemplo, 'alto').", attribute->name);
		return;
	}
	const Factor * factor = value->factor;
	if (findSymbol(table, LEVEL_SYMBOL, factor->variable) != NULL) {
		return;
	}
	if (findSymbol(table, SCALE_SYMBOL, factor->variable) != NULL) {
		reportSemanticError(factor->position,
			"'%s' es una escala, pero '%s' lleva uno de sus niveles.", factor->variable, attribute->name);
	}
	else {
		reportSemanticError(factor->position,
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
				reportSemanticError(topic->goalPosition,
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
					reportSemanticError(attribute->position, "'requires' lleva nombres de temas, separados por comas.");
					break;
				}
				const Factor * factor = expression->factor;
				const Symbol * required = findSymbol(table, TOPIC_SYMBOL, factor->variable);
				if (required == NULL) {
					reportSemanticError(factor->position, "el tema '%s' no está declarado.", factor->variable);
				}
				else if (required->kind == GOAL_SYMBOL) {
					reportSemanticError(factor->position, "'%s' es un objetivo, pero 'requires' lleva temas.", factor->variable);
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
		reportSemanticError(position,
			"ya hay %s llamado '%s' %s; los objetivos y los temas no pueden compartir nombre.",
			(previous->kind == GOAL_SYMBOL) ? "un objetivo" : "un tema", name, describePosition(previous->position));
	}
	else if (kind == GOAL_SYMBOL) {
		reportSemanticError(position, "el objetivo '%s' ya está declarado %s.", name, describePosition(previous->position));
	}
	else if (kind == TOPIC_SYMBOL) {
		reportSemanticError(position, "el tema '%s' ya está declarado %s.", name, describePosition(previous->position));
	}
	else {
		reportSemanticError(position, "la escala '%s' ya está declarada %s.", name, describePosition(previous->position));
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
			reportSemanticError(node->position,
				"el nivel '%s' está repetido en la escala '%s'.", node->level, scale->name);
		}
		else if (previous != NULL) {
			reportSemanticError(node->position,
				"el nivel '%s' ya pertenece a la escala '%s' (%s); cada nivel pertenece a una sola escala.",
				node->level, previous->owner->name, describePosition(previous->position));
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
 * A block that describes the whole plan (e.g., "plan" or "availability") is
 * declared only once, because two of them would contradict each other.
 */
static void _validateUniqueBlock(const Declaration ** first, const Declaration * declaration, const char * keyword) {
	if (*first == NULL) {
		*first = declaration;
	}
	else {
		reportSemanticError(declaration->position,
			"ya hay un bloque '%s' %s; se declara una sola vez.", keyword, describePosition((*first)->position));
	}
}

static void _validateMethod(const SymbolTable * table, const DeclarationList * declarations, const Method * method) {
	_validateMethodName(declarations, method);
	_validateParameters(method);
	validateMethodBody(table, method);
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
		reportSemanticError(method->position,
			"el método '%s' tiene que empezar con mayúscula (por ejemplo, '%c%s').",
			method->name, toupper((unsigned char) method->name[0]), method->name + 1);
	}
	const Method * previous = _findPreviousMethod(declarations, method);
	if (previous == NULL) {
		return;
	}
	if (previous->position.file != NULL && method->position.file == NULL) {
		reportSemanticError(method->position,
			"'%s' ya está definido en %s y no se puede redefinir; se personaliza con sus argumentos.",
			method->name, previous->position.file);
	}
	else if (previous->position.file == NULL && method->position.file != NULL) {
		reportSemanticError(previous->position,
			"'%s' ya está definido en %s y no se puede redefinir; se personaliza con sus argumentos.",
			method->name, method->position.file);
	}
	else if (previous->position.file != NULL) {
		reportSemanticError(method->position,
			"el método '%s' ya está declarado en %s (línea %d).",
			method->name, previous->position.file, previous->position.line);
	}
	else {
		reportSemanticError(method->position,
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
				reportSemanticError(parameter->position,
					"el parámetro '%s' está repetido en el método '%s'.",
					parameter->name, method->name);
				break;
			}
		}
		if (parameter->defaultValue != NULL) {
			hasDefaultValue = true;
		}
		else if (hasDefaultValue) {
			reportSemanticError(parameter->position,
				"el parámetro '%s' necesita un valor por defecto, porque está después de uno que lo tiene.",
				parameter->name);
		}
	}
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeSemanticAnalysis(Program * program) {
	logDebugging(_logger, "Analyzing...");
	resetSemanticErrors();
	SymbolTable * table = createSymbolTable();
	_declareSymbols(table, program->declarations);
	_resolveGoals(table, program->declarations);
	_validateRequirements(table);
	_validateLevels(table);
	_validateCycles(table);
	_validateBlocks(table, program->declarations);
	_validateConstraints(table, program->declarations);
	_validateInvocations(table, program->declarations);
	for (const DeclarationList * node = program->declarations; node != NULL; node = node->next) {
		if (node->declaration->type == METHOD_DECLARATION) {
			_validateMethod(table, program->declarations, node->declaration->method);
		}
	}
	destroySymbolTable(table);
	const unsigned int errors = semanticErrorCount();
	logDebugging(_logger, "Analysis is done (errors=%u).", errors);
	return (errors == 0) ? SUCCEEDED : FAILED;
}
