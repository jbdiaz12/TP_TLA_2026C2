#include "MethodExpansion.h"

/* MODULE INTERNAL STATE */

/** The duration of each session of the implicit method, used when a topic has no other. */
#define IMPLICIT_SESSION 60

/**
 * The limits of an expansion. A method like "repeat 100000 { ... }" is valid,
 * but scheduling it makes no sense, so it's reported instead of exhausting
 * the memory.
 */
#define MAXIMUM_STEPS 20000
#define MAXIMUM_REPETITIONS 20000

/** The value of a parameter or of the variable of a loop, while a method runs. */
typedef struct {
	const char * name;
	bool isList;
	int value;
	int * items;
	int itemCount;
} Binding;

/** The variables that a running method can read, as a stack. */
typedef struct {
	Binding * bindings;
	int count;
	int capacity;
} Environment;

/** The blocks that one repetition of a method produces. */
typedef struct {
	Block * blocks;
	int count;
	int capacity;
	/** The block that receives the next steps. */
	int current;
	int steps;
	bool hasFailed;
} Pattern;

/** What a running method needs to report its errors. */
typedef struct {
	const char * method;
	const char * topic;
	Position position;
} Invocation;

/** The value of an expression inside the condition of a rule. */
typedef struct {
	bool isKnown;
	bool isLevel;
	int number;
	const Symbol * scale;
} RuleValue;

/** The topic that a rule is evaluated for. */
typedef struct {
	const SymbolTable * table;
	const Symbol * topic;
	int estimated;
} RuleContext;

/* PRIVATE FUNCTIONS */

static void _appendStep(Pattern * pattern, const int duration, const bool isSession, const Invocation * invocation);
static void _bind(Environment * environment, const Binding binding);
static bool _bindArguments(const Method * method, const Factor * invocation, Environment * environment);
static const Expression * _chooseMethod(const SymbolTable * table, const DeclarationList * declarations, const Symbol * topic, const int estimated);
static void _cover(Work * work, Pattern * pattern, const int minutes);
static void _destroyEnvironment(Environment * environment);
static void _destroyPattern(Pattern * pattern);
static bool _evaluateList(const Expression * expression, const Environment * environment, int ** items, int * count);
static bool _evaluateNumber(const Expression * expression, const Environment * environment, int * value);
static void _execute(const StatementList * statements, Environment * environment, Pattern * pattern, const Invocation * invocation);
static void _executeStatement(const Statement * statement, Environment * environment, Pattern * pattern, const Invocation * invocation);
static void _expand(Work * work, const SymbolTable * table, const DeclarationList * declarations, const int minutes);
static const Attribute * _findAttribute(const MemberList * members, const char * name);
static const Binding * _findBinding(const Environment * environment, const char * name);
static const Method * _findMethod(const DeclarationList * declarations, const char * name);
static bool _hasSubtopics(const Topic * topic);
static const Attribute * _inheritedAttribute(const Symbol * topic, const char * name);
static void _openBlock(Pattern * pattern, const int offset);
static bool _ruleHolds(const Expression * condition, const RuleContext * context);
static RuleValue _ruleValue(const Expression * expression, const RuleContext * context);
static RuleValue _levelValue(const SymbolTable * table, const Attribute * attribute);

/**
 * Adds a session or a pause to the current block of a pattern.
 */
static void _appendStep(Pattern * pattern, const int duration, const bool isSession, const Invocation * invocation) {
	if (pattern->hasFailed) {
		return;
	}
	if (MAXIMUM_STEPS <= ++pattern->steps) {
		reportPlanningError(invocation->position,
			"'%s' genera demasiadas sesiones para el tema '%s'.", invocation->method, invocation->topic);
		pattern->hasFailed = true;
		return;
	}
	Block * block = &pattern->blocks[pattern->current];
	block->steps = realloc(block->steps, (block->stepCount + 1) * sizeof(Step));
	block->steps[block->stepCount++] = (Step) { .duration = duration, .isSession = isSession };
}

static void _bind(Environment * environment, const Binding binding) {
	if (environment->count == environment->capacity) {
		environment->capacity = (environment->capacity == 0) ? 8 : 2 * environment->capacity;
		environment->bindings = realloc(environment->bindings, environment->capacity * sizeof(Binding));
	}
	environment->bindings[environment->count++] = binding;
}

/**
 * Binds every parameter of a method to the value of its argument, or to its
 * default value when the invocation omits it. The analysis guarantees that
 * the values are constants of the right type.
 */
static bool _bindArguments(const Method * method, const Factor * invocation, Environment * environment) {
	const ExpressionList * argument = invocation->arguments;
	for (const ParameterList * node = method->parameters; node != NULL; node = node->next) {
		const Parameter * parameter = node->parameter;
		const Expression * value = (argument != NULL) ? argument->expression : parameter->defaultValue;
		if (argument != NULL) {
			argument = argument->next;
		}
		Binding binding = { .name = parameter->name };
		if (strcmp(parameter->type, "list") == 0) {
			binding.isList = true;
			if (!_evaluateList(value, environment, &binding.items, &binding.itemCount)) {
				return false;
			}
		}
		else if (!_evaluateNumber(value, environment, &binding.value)) {
			return false;
		}
		_bind(environment, binding);
	}
	return true;
}

/**
 * Chooses the invocation of the method of a topic: its own "method", or the
 * one of the nearest topic that contains it, or the action of the first
 * rule that the topic meets. It returns NULL when none applies.
 */
static const Expression * _chooseMethod(const SymbolTable * table, const DeclarationList * declarations, const Symbol * topic, const int estimated) {
	const Attribute * attribute = _inheritedAttribute(topic, "method");
	if (attribute != NULL) {
		return attribute->values->expression;
	}
	const RuleContext context = { .table = table, .topic = topic, .estimated = estimated };
	for (const DeclarationList * node = declarations; node != NULL; node = node->next) {
		if (node->declaration->type != RULES_DECLARATION) {
			continue;
		}
		for (const RuleList * item = node->declaration->rules; item != NULL; item = item->next) {
			const Rule * rule = item->rule;
			if (rule->type == OTHERWISE_RULE || _ruleHolds(rule->condition, &context)) {
				return rule->action;
			}
		}
	}
	return NULL;
}

/**
 * Repeats a pattern until its sessions cover the specified minutes. The last
 * session is cut where the time is covered, and whatever follows it in that
 * repetition is discarded.
 */
static void _cover(Work * work, Pattern * pattern, const int minutes) {
	int remaining = minutes;
	for (int repetition = 0; 0 < remaining && repetition < MAXIMUM_REPETITIONS; ++repetition) {
		for (int k = 0; k < pattern->count && 0 < remaining; ++k) {
			const Block * source = &pattern->blocks[k];
			Block block = { .repetition = repetition, .offset = source->offset };
			block.steps = calloc(source->stepCount, sizeof(Step));
			for (int s = 0; s < source->stepCount && 0 < remaining; ++s) {
				Step step = source->steps[s];
				if (step.isSession) {
					if (remaining < step.duration) {
						step.duration = remaining;
					}
					remaining -= step.duration;
					block.study += step.duration;
				}
				block.steps[block.stepCount++] = step;
				block.length += step.duration;
			}
			while (0 < block.stepCount && !block.steps[block.stepCount - 1].isSession) {
				block.length -= block.steps[--block.stepCount].duration;
			}
			if (block.study == 0) {
				free(block.steps);
				continue;
			}
			work->blocks = realloc(work->blocks, (work->blockCount + 1) * sizeof(Block));
			work->blocks[work->blockCount++] = block;
		}
	}
}

static void _destroyEnvironment(Environment * environment) {
	for (int k = 0; k < environment->count; ++k) {
		free(environment->bindings[k].items);
	}
	free(environment->bindings);
}

static void _destroyPattern(Pattern * pattern) {
	for (int k = 0; k < pattern->count; ++k) {
		free(pattern->blocks[k].steps);
	}
	free(pattern->blocks);
}

/**
 * Evaluates a list: a list literal, or a variable that holds one. The items
 * are copied, so the caller frees them.
 */
static bool _evaluateList(const Expression * expression, const Environment * environment, int ** items, int * count) {
	if (expression->type != FACTOR_EXPRESSION) {
		return false;
	}
	const Factor * factor = expression->factor;
	if (factor->type == PARENTHESIZED_FACTOR) {
		return _evaluateList(factor->expression, environment, items, count);
	}
	if (factor->type == VARIABLE_FACTOR) {
		const Binding * binding = _findBinding(environment, factor->variable);
		if (binding == NULL || !binding->isList) {
			return false;
		}
		*count = binding->itemCount;
		*items = calloc((binding->itemCount == 0) ? 1 : binding->itemCount, sizeof(int));
		memcpy(*items, binding->items, binding->itemCount * sizeof(int));
		return true;
	}
	if (factor->type != LIST_FACTOR) {
		return false;
	}
	*count = 0;
	for (const ExpressionList * node = factor->elements; node != NULL; node = node->next) {
		++*count;
	}
	*items = calloc((*count == 0) ? 1 : *count, sizeof(int));
	int k = 0;
	for (const ExpressionList * node = factor->elements; node != NULL; node = node->next) {
		if (!_evaluateNumber(node->expression, environment, &(*items)[k++])) {
			free(*items);
			*items = NULL;
			return false;
		}
	}
	return true;
}

/**
 * Evaluates an integer or a duration (in minutes), which can read the
 * variables of the running method.
 */
static bool _evaluateNumber(const Expression * expression, const Environment * environment, int * value) {
	if (expression->type == FACTOR_EXPRESSION) {
		const Factor * factor = expression->factor;
		if (factor->type == PARENTHESIZED_FACTOR) {
			return _evaluateNumber(factor->expression, environment, value);
		}
		if (factor->type == VARIABLE_FACTOR) {
			const Binding * binding = _findBinding(environment, factor->variable);
			if (binding == NULL || binding->isList) {
				return false;
			}
			*value = binding->value;
			return true;
		}
		return evaluateConstant(expression, value);
	}
	int left = 0;
	int right = 0;
	if (!_evaluateNumber(expression->leftExpression, environment, &left) || !_evaluateNumber(expression->rightExpression, environment, &right)) {
		return false;
	}
	switch (expression->type) {
		case ARITHMETIC_ADD: *value = left + right; return true;
		case ARITHMETIC_SUB: *value = left - right; return true;
		case ARITHMETIC_MUL: *value = left * right; return true;
		case ARITHMETIC_DIV:
			if (right == 0) {
				return false;
			}
			*value = left / right;
			return true;
		default:
			return false;
	}
}

static void _execute(const StatementList * statements, Environment * environment, Pattern * pattern, const Invocation * invocation) {
	for (const StatementList * node = statements; node != NULL && !pattern->hasFailed; node = node->next) {
		_executeStatement(node->statement, environment, pattern, invocation);
	}
}

/**
 * Runs a statement of a method. Consecutive sessions and pauses build the
 * current block, while "after" opens a new block, which starts that long
 * after the beginning of the repetition (e.g., the reviews of a spaced
 * repetition, one, three and seven days after the first session).
 */
static void _executeStatement(const Statement * statement, Environment * environment, Pattern * pattern, const Invocation * invocation) {
	int value = 0;
	switch (statement->type) {
		case SESSION_STATEMENT:
		case PAUSE_STATEMENT: {
			const char * keyword = (statement->type == SESSION_STATEMENT) ? "session" : "pause";
			if (!_evaluateNumber(statement->duration, environment, &value) || value <= 0) {
				reportPlanningError(statement->position,
					"con los argumentos que recibe para el tema '%s', '%s' de '%s' no dura más que cero.",
					invocation->topic, keyword, invocation->method);
				pattern->hasFailed = true;
				return;
			}
			_appendStep(pattern, value, statement->type == SESSION_STATEMENT, invocation);
			break;
		}
		case AFTER_STATEMENT:
			if (!_evaluateNumber(statement->delay, environment, &value) || value < 0) {
				reportPlanningError(statement->position,
					"con los argumentos que recibe para el tema '%s', 'after' de '%s' es negativo.", invocation->topic, invocation->method);
				pattern->hasFailed = true;
				return;
			}
			_openBlock(pattern, value);
			_executeStatement(statement->delayedStatement, environment, pattern, invocation);
			break;
		case REPEAT_STATEMENT:
			if (_evaluateNumber(statement->count, environment, &value)) {
				for (int k = 0; k < value && !pattern->hasFailed; ++k) {
					_execute(statement->repeatBody, environment, pattern, invocation);
				}
			}
			break;
		case FOR_STATEMENT: {
			int * items = NULL;
			int count = 0;
			if (!_evaluateList(statement->iterable, environment, &items, &count)) {
				return;
			}
			for (int k = 0; k < count && !pattern->hasFailed; ++k) {
				_bind(environment, (Binding) { .name = statement->variable, .value = items[k] });
				_execute(statement->forBody, environment, pattern, invocation);
				--environment->count;
			}
			free(items);
			break;
		}
	}
}

/**
 * Builds the blocks of a topic: it runs its method once, as a pattern, and
 * repeats that pattern until it covers the minutes of the topic.
 */
static void _expand(Work * work, const SymbolTable * table, const DeclarationList * declarations, const int minutes) {
	const Symbol * topic = work->topic;
	Pattern pattern = { 0 };
	_openBlock(&pattern, 0);
	const Expression * choice = _chooseMethod(table, declarations, topic, work->estimated);
	Position position = topic->position;
	if (choice == NULL) {
		const Invocation implicit = { .method = "Bloque", .topic = topic->name, .position = position };
		_appendStep(&pattern, IMPLICIT_SESSION, true, &implicit);
	}
	else {
		const Factor * factor = choice->factor;
		const Method * method = _findMethod(declarations, factor->function);
		const Invocation invocation = { .method = method->name, .topic = topic->name, .position = factor->position };
		Environment environment = { 0 };
		work->method = method->name;
		position = factor->position;
		if (_bindArguments(method, factor, &environment)) {
			_execute(method->body, &environment, &pattern, &invocation);
		}
		_destroyEnvironment(&environment);
	}
	if (pattern.hasFailed) {
		_destroyPattern(&pattern);
		return;
	}
	int study = 0;
	for (int k = 0; k < pattern.count; ++k) {
		for (int s = 0; s < pattern.blocks[k].stepCount; ++s) {
			if (pattern.blocks[k].steps[s].isSession) {
				study += pattern.blocks[k].steps[s].duration;
			}
		}
	}
	if (study == 0) {
		reportPlanningError(position,
			"con estos argumentos, '%s' no tiene ninguna sesión, así que no puede cubrir el tema '%s'.", work->method, topic->name);
	}
	else {
		for (int k = 1; k < pattern.count; ++k) {
			const Block block = pattern.blocks[k];
			int j = k - 1;
			for (; 0 <= j && block.offset < pattern.blocks[j].offset; --j) {
				pattern.blocks[j + 1] = pattern.blocks[j];
			}
			pattern.blocks[j + 1] = block;
		}
		_cover(work, &pattern, minutes);
	}
	_destroyPattern(&pattern);
}

static const Attribute * _findAttribute(const MemberList * members, const char * name) {
	for (const MemberList * node = members; node != NULL; node = node->next) {
		if (node->member->type == ATTRIBUTE_MEMBER && strcmp(node->member->attribute->name, name) == 0) {
			return node->member->attribute;
		}
	}
	return NULL;
}

/**
 * Finds the innermost variable with a name, so the variable of a loop hides
 * any other with its name.
 */
static const Binding * _findBinding(const Environment * environment, const char * name) {
	for (int k = environment->count - 1; 0 <= k; --k) {
		if (strcmp(environment->bindings[k].name, name) == 0) {
			return &environment->bindings[k];
		}
	}
	return NULL;
}

static const Method * _findMethod(const DeclarationList * declarations, const char * name) {
	for (const DeclarationList * node = declarations; node != NULL; node = node->next) {
		if (node->declaration->type == METHOD_DECLARATION && strcmp(node->declaration->method->name, name) == 0) {
			return node->declaration->method;
		}
	}
	return NULL;
}

static bool _hasSubtopics(const Topic * topic) {
	for (const MemberList * node = topic->members; node != NULL; node = node->next) {
		if (node->member->type == TOPIC_MEMBER) {
			return true;
		}
	}
	return false;
}

/**
 * Finds an attribute in a topic or, if it doesn't declare it, in the nearest
 * topic that contains it: a subtopic inherits the method and the difficulty
 * of its topic.
 */
static const Attribute * _inheritedAttribute(const Symbol * topic, const char * name) {
	for (const Symbol * symbol = topic; symbol != NULL; symbol = symbol->parent) {
		const Attribute * attribute = _findAttribute(symbol->topic->members, name);
		if (attribute != NULL) {
			return attribute;
		}
	}
	return NULL;
}

/**
 * The value of a level attribute (e.g., "difficulty: alto") in a condition.
 */
static RuleValue _levelValue(const SymbolTable * table, const Attribute * attribute) {
	RuleValue value = { .isKnown = false };
	if (attribute == NULL) {
		return value;
	}
	const Expression * expression = attribute->values->expression;
	const Symbol * level = findSymbol(table, LEVEL_SYMBOL, expression->factor->variable);
	if (level != NULL) {
		value = (RuleValue) { .isKnown = true, .isLevel = true, .number = level->rank, .scale = level->owner };
	}
	return value;
}

/**
 * Opens a new block in a pattern, which starts after the specified offset.
 */
static void _openBlock(Pattern * pattern, const int offset) {
	if (pattern->count == pattern->capacity) {
		pattern->capacity = (pattern->capacity == 0) ? 4 : 2 * pattern->capacity;
		pattern->blocks = realloc(pattern->blocks, pattern->capacity * sizeof(Block));
	}
	pattern->blocks[pattern->count] = (Block) { .offset = offset };
	pattern->current = pattern->count++;
}

/**
 * Whether a topic meets the condition of a rule. A comparison with an
 * attribute that the topic doesn't declare (e.g., the difficulty of a topic
 * without one) doesn't hold, and neither does a comparison between levels
 * of different scales.
 */
static bool _ruleHolds(const Expression * condition, const RuleContext * context) {
	switch (condition->type) {
		case LOGICAL_AND:
			return _ruleHolds(condition->leftExpression, context) && _ruleHolds(condition->rightExpression, context);
		case LOGICAL_OR:
			return _ruleHolds(condition->leftExpression, context) || _ruleHolds(condition->rightExpression, context);
		case LOGICAL_NOT:
			return !_ruleHolds(condition->operand, context);
		case FACTOR_EXPRESSION:
			return condition->factor->type == PARENTHESIZED_FACTOR && _ruleHolds(condition->factor->expression, context);
		default:
			break;
	}
	const RuleValue left = _ruleValue(condition->leftExpression, context);
	const RuleValue right = _ruleValue(condition->rightExpression, context);
	if (!left.isKnown || !right.isKnown || left.isLevel != right.isLevel) {
		return false;
	}
	if (left.isLevel && left.scale != right.scale) {
		return false;
	}
	switch (condition->type) {
		case COMPARE_EQUAL: return left.number == right.number;
		case COMPARE_NOT_EQUAL: return left.number != right.number;
		case COMPARE_LESS: return left.number < right.number;
		case COMPARE_LESS_EQUAL: return left.number <= right.number;
		case COMPARE_GREATER: return left.number > right.number;
		case COMPARE_GREATER_EQUAL: return left.number >= right.number;
		default: return false;
	}
}

/**
 * The value of an expression inside a condition: an attribute of the topic,
 * a level, or a number (an integer, or a duration in minutes).
 */
static RuleValue _ruleValue(const Expression * expression, const RuleContext * context) {
	if (expression->type == FACTOR_EXPRESSION && expression->factor->type == PARENTHESIZED_FACTOR) {
		return _ruleValue(expression->factor->expression, context);
	}
	if (expression->type == FACTOR_EXPRESSION && expression->factor->type == VARIABLE_FACTOR) {
		const char * name = expression->factor->variable;
		if (strcmp(name, "estimated") == 0) {
			return (RuleValue) { .isKnown = true, .number = context->estimated };
		}
		if (strcmp(name, "difficulty") == 0) {
			return _levelValue(context->table, _inheritedAttribute(context->topic, "difficulty"));
		}
		if (strcmp(name, "priority") == 0) {
			const Symbol * goal = context->topic->owner;
			return _levelValue(context->table, (goal == NULL) ? NULL : _findAttribute(goal->goal->members, "priority"));
		}
		const Symbol * level = findSymbol(context->table, LEVEL_SYMBOL, name);
		if (level != NULL) {
			return (RuleValue) { .isKnown = true, .isLevel = true, .number = level->rank, .scale = level->owner };
		}
		return (RuleValue) { .isKnown = false };
	}
	int number = 0;
	if (evaluateConstant(expression, &number)) {
		return (RuleValue) { .isKnown = true, .number = number };
	}
	return (RuleValue) { .isKnown = false };
}

/* PUBLIC FUNCTIONS */

Work * expandWorks(const DeclarationList * declarations, const SymbolTable * table, int * count) {
	*count = 0;
	for (const Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (symbol->kind == TOPIC_SYMBOL && !_hasSubtopics(symbol->topic)) {
			++*count;
		}
	}
	Work * works = calloc((*count == 0) ? 1 : *count, sizeof(Work));
	int k = 0;
	for (const Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (symbol->kind != TOPIC_SYMBOL || _hasSubtopics(symbol->topic)) {
			continue;
		}
		Work * work = &works[k++];
		work->topic = symbol;
		const Attribute * estimated = _findAttribute(symbol->topic->members, "estimated");
		evaluateConstant(estimated->values->expression, &work->estimated);
		int minutes = work->estimated;
		const Attribute * fixed = _findAttribute(symbol->topic->members, "fixed");
		if (fixed != NULL) {
			const ExpressionList * values = fixed->values;
			work->hasFixedSession = true;
			work->fixedDay = toDayNumber(values->expression->factor->constant->date);
			work->fixedStart = values->next->expression->factor->constant->time;
			evaluateConstant(values->next->next->expression, &work->fixedDuration);
			minutes -= work->fixedDuration;
		}
		if (0 < minutes) {
			_expand(work, table, declarations, minutes);
		}
	}
	return works;
}
