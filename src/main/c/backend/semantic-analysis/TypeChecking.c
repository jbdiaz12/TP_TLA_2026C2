#include "TypeChecking.h"

/* PRIVATE FUNCTIONS */

static Type _arithmeticType(const ExpressionType operator, const Type left, const Type right);
static bool _isComparison(const ExpressionType operator);
static bool _isOrdered(const TypeKind kind);
static bool _isPeriod(const Expression * argument);
static Type _typeOfComparison(const Expression * expression, const Type left, const Type right);
static Type _typeOfDomainFunction(const Factor * factor, const Scope * scope);
static Type _typeOfFactor(const Factor * factor, const Scope * scope);
static Type _typeOfVariable(const Factor * factor, const Scope * scope);
static const char * _verbOf(const ExpressionType operator);

/**
 * The result of an arithmetic operation, or ERROR_TYPE if the operands don't
 * admit it. A duration is scaled by an integer, and a date moves forward or
 * backward by a duration (e.g., "deadline(goal) - 7d").
 */
static Type _arithmeticType(const ExpressionType operator, const Type left, const Type right) {
	const TypeKind l = left.kind;
	const TypeKind r = right.kind;
	switch (operator) {
		case ARITHMETIC_ADD:
			if (l == DURATION_TYPE && r == DURATION_TYPE) return simpleType(DURATION_TYPE);
			if (l == INTEGER_TYPE && r == INTEGER_TYPE) return simpleType(INTEGER_TYPE);
			if (l == DATE_TYPE && r == DURATION_TYPE) return simpleType(DATE_TYPE);
			if (l == DURATION_TYPE && r == DATE_TYPE) return simpleType(DATE_TYPE);
			break;
		case ARITHMETIC_SUB:
			if (l == DURATION_TYPE && r == DURATION_TYPE) return simpleType(DURATION_TYPE);
			if (l == INTEGER_TYPE && r == INTEGER_TYPE) return simpleType(INTEGER_TYPE);
			if (l == DATE_TYPE && r == DURATION_TYPE) return simpleType(DATE_TYPE);
			break;
		case ARITHMETIC_MUL:
			if (l == INTEGER_TYPE && r == INTEGER_TYPE) return simpleType(INTEGER_TYPE);
			if (l == DURATION_TYPE && r == INTEGER_TYPE) return simpleType(DURATION_TYPE);
			if (l == INTEGER_TYPE && r == DURATION_TYPE) return simpleType(DURATION_TYPE);
			break;
		case ARITHMETIC_DIV:
			if (l == INTEGER_TYPE && r == INTEGER_TYPE) return simpleType(INTEGER_TYPE);
			if (l == DURATION_TYPE && r == INTEGER_TYPE) return simpleType(DURATION_TYPE);
			break;
		default:
			break;
	}
	return simpleType(ERROR_TYPE);
}

static bool _isComparison(const ExpressionType operator) {
	switch (operator) {
		case COMPARE_EQUAL:
		case COMPARE_GREATER:
		case COMPARE_GREATER_EQUAL:
		case COMPARE_LESS:
		case COMPARE_LESS_EQUAL:
		case COMPARE_NOT_EQUAL:
			return true;
		default:
			return false;
	}
}

/**
 * Whether the values of a type have an order, so "<" and ">" apply to them.
 */
static bool _isOrdered(const TypeKind kind) {
	return kind == DATE_TYPE || kind == DURATION_TYPE || kind == INTEGER_TYPE || kind == LEVEL_TYPE || kind == TIME_TYPE;
}

/**
 * Whether an argument is one of the periods of the plan: "day" or "week".
 */
static bool _isPeriod(const Expression * argument) {
	if (argument->type != FACTOR_EXPRESSION || argument->factor->type != VARIABLE_FACTOR) {
		return false;
	}
	const char * name = argument->factor->variable;
	return strcmp(name, "day") == 0 || strcmp(name, "week") == 0;
}

/**
 * Two values are comparable when they have the same type, and two levels
 * also need the same scale ("alto" and "urgente" aren't comparable if one is
 * a difficulty and the other a priority). Only the ordered types admit "<"
 * and ">"; any type admits "==" and "!=".
 */
static Type _typeOfComparison(const Expression * expression, const Type left, const Type right) {
	const bool isEquality = (expression->type == COMPARE_EQUAL || expression->type == COMPARE_NOT_EQUAL);
	if (left.kind != right.kind || (!isEquality && !_isOrdered(left.kind))) {
		const char * leftDescription = describeType(left);
		reportSemanticError(expression->position, "no se puede comparar %s con %s.", leftDescription, describeType(right));
		return simpleType(ERROR_TYPE);
	}
	if (left.kind == LEVEL_TYPE && left.scale != right.scale) {
		reportSemanticError(expression->position,
			"no se pueden comparar niveles de escalas distintas ('%s' y '%s').", left.scale->name, right.scale->name);
		return simpleType(ERROR_TYPE);
	}
	return simpleType(BOOLEAN_TYPE);
}

/**
 * The functions of the domain, which describe the resulting schedule and
 * are only meaningful inside the constraints:
 *
 *	topics(day | week)	the amount of different topics in a day or a week
 *	load(day | week)	the amount of study time in a day or a week
 *	finish(subject)		the date when a goal or a topic is finished
 *	deadline(goal)		the due date of a goal
 *
 * The subject is "goal" or "topic" (the one being constrained), or the name
 * of a declared goal or topic.
 */
static Type _typeOfDomainFunction(const Factor * factor, const Scope * scope) {
	const char * name = factor->function;
	const bool isPeriodFunction = (strcmp(name, "topics") == 0 || strcmp(name, "load") == 0);
	const bool isFinish = (strcmp(name, "finish") == 0);
	const bool isDeadline = (strcmp(name, "deadline") == 0);
	if (!isPeriodFunction && !isFinish && !isDeadline) {
		reportSemanticError(factor->position,
			"'%s' no es una función del dominio; las funciones válidas son: deadline, finish, load, topics.", name);
		return simpleType(ERROR_TYPE);
	}
	if (factor->arguments == NULL || factor->arguments->next != NULL) {
		reportSemanticError(factor->position, "'%s' recibe un solo argumento.", name);
		return simpleType(ERROR_TYPE);
	}
	const Expression * argument = factor->arguments->expression;
	if (isPeriodFunction) {
		if (!_isPeriod(argument)) {
			reportSemanticError(argument->position, "'%s' recibe un período: day o week.", name);
			return simpleType(ERROR_TYPE);
		}
		return simpleType(strcmp(name, "topics") == 0 ? INTEGER_TYPE : DURATION_TYPE);
	}
	const char * subject = NULL;
	if (argument->type == FACTOR_EXPRESSION && argument->factor->type == VARIABLE_FACTOR) {
		subject = argument->factor->variable;
	}
	const Symbol * symbol = (subject == NULL) ? NULL : findSymbol(scope->table, GOAL_SYMBOL, subject);
	if (isFinish) {
		const bool isValid = subject != NULL && (strcmp(subject, "goal") == 0 || strcmp(subject, "topic") == 0 || symbol != NULL);
		if (!isValid) {
			reportSemanticError(argument->position, "'finish' recibe goal, topic, o el nombre de un objetivo o de un tema.");
			return simpleType(ERROR_TYPE);
		}
	}
	else {
		const bool isValid = subject != NULL && (strcmp(subject, "goal") == 0 || (symbol != NULL && symbol->kind == GOAL_SYMBOL));
		if (!isValid) {
			reportSemanticError(argument->position, "'deadline' recibe goal, o el nombre de un objetivo.");
			return simpleType(ERROR_TYPE);
		}
	}
	return simpleType(DATE_TYPE);
}

static Type _typeOfFactor(const Factor * factor, const Scope * scope) {
	switch (factor->type) {
		case CONSTANT_FACTOR:
			switch (factor->constant->type) {
				case DATE_CONSTANT: return simpleType(DATE_TYPE);
				case DURATION_CONSTANT: return simpleType(DURATION_TYPE);
				case INTEGER_CONSTANT: return simpleType(INTEGER_TYPE);
				case STRING_CONSTANT: return simpleType(STRING_TYPE);
				case TIME_CONSTANT: return simpleType(TIME_TYPE);
			}
			return simpleType(ERROR_TYPE);
		case PARENTHESIZED_FACTOR:
			return typeOf(factor->expression, scope);
		case LIST_FACTOR: {
			Type type = simpleType(LIST_TYPE);
			for (const ExpressionList * node = factor->elements; node != NULL; node = node->next) {
				const Type element = typeOf(node->expression, scope);
				if (element.kind != ERROR_TYPE && element.kind != DURATION_TYPE) {
					reportSemanticError(node->expression->position,
						"una lista lleva duraciones, pero este elemento es %s.", describeType(element));
					type = simpleType(ERROR_TYPE);
				}
				else if (element.kind == ERROR_TYPE) {
					type = simpleType(ERROR_TYPE);
				}
			}
			return type;
		}
		case INVOCATION_FACTOR:
			if (scope->allowsDomainFunctions) {
				return _typeOfDomainFunction(factor, scope);
			}
			reportSemanticError(factor->position,
				"'%s(...)' no se puede usar acá: las funciones del dominio solo van en 'constraints'.", factor->function);
			return simpleType(ERROR_TYPE);
		case VARIABLE_FACTOR:
			return _typeOfVariable(factor, scope);
	}
	return simpleType(ERROR_TYPE);
}

/**
 * A name inside an expression is a variable of the scope (e.g., a parameter
 * of a method), or a level of a scale. The variables are searched from the
 * innermost one, so the variable of a loop hides any other with its name.
 */
static Type _typeOfVariable(const Factor * factor, const Scope * scope) {
	const char * name = factor->variable;
	for (int k = scope->variableCount - 1; 0 <= k; --k) {
		if (strcmp(scope->variables[k].name, name) == 0) {
			return scope->variables[k].type;
		}
	}
	const Symbol * level = findSymbol(scope->table, LEVEL_SYMBOL, name);
	if (level != NULL) {
		return (Type) { .kind = LEVEL_TYPE, .scale = level->owner };
	}
	if (strcmp(name, "goal") == 0 || strcmp(name, "topic") == 0) {
		reportSemanticError(factor->position,
			"'%s' solo se usa como argumento de una función (por ejemplo, 'finish(%s)').", name, name);
	}
	else if (strcmp(name, "day") == 0 || strcmp(name, "week") == 0) {
		reportSemanticError(factor->position,
			"'%s' solo se usa como argumento de topics o load (por ejemplo, 'load(%s)').", name, name);
	}
	else {
		reportSemanticError(factor->position, "'%s' no está declarado.", name);
	}
	return simpleType(ERROR_TYPE);
}

static const char * _verbOf(const ExpressionType operator) {
	switch (operator) {
		case ARITHMETIC_ADD: return "sumar";
		case ARITHMETIC_SUB: return "restar";
		case ARITHMETIC_MUL: return "multiplicar";
		case ARITHMETIC_DIV: return "dividir";
		default: return "operar";
	}
}

/* PUBLIC FUNCTIONS */

const char * describeType(const Type type) {
	switch (type.kind) {
		case BOOLEAN_TYPE: return "una condición";
		case DATE_TYPE: return "una fecha";
		case DURATION_TYPE: return "una duración";
		case INTEGER_TYPE: return "un entero";
		case LEVEL_TYPE: return "un nivel";
		case LIST_TYPE: return "una lista";
		case STRING_TYPE: return "un texto";
		case TIME_TYPE: return "una hora";
		default: return "un valor inválido";
	}
}

bool evaluateConstant(const Expression * expression, int * value) {
	if (expression->type == FACTOR_EXPRESSION) {
		const Factor * factor = expression->factor;
		if (factor->type == PARENTHESIZED_FACTOR) {
			return evaluateConstant(factor->expression, value);
		}
		if (factor->type != CONSTANT_FACTOR) {
			return false;
		}
		switch (factor->constant->type) {
			case DURATION_CONSTANT: *value = factor->constant->duration; return true;
			case INTEGER_CONSTANT: *value = factor->constant->integer; return true;
			default: return false;
		}
	}
	int left = 0;
	int right = 0;
	switch (expression->type) {
		case ARITHMETIC_ADD:
		case ARITHMETIC_SUB:
		case ARITHMETIC_MUL:
		case ARITHMETIC_DIV:
			if (!evaluateConstant(expression->leftExpression, &left) || !evaluateConstant(expression->rightExpression, &right)) {
				return false;
			}
			break;
		default:
			return false;
	}
	switch (expression->type) {
		case ARITHMETIC_ADD: *value = left + right; return true;
		case ARITHMETIC_SUB: *value = left - right; return true;
		case ARITHMETIC_MUL: *value = left * right; return true;
		default:
			if (right == 0) {
				reportSemanticError(expression->position, "no se puede dividir por cero.");
				return false;
			}
			*value = left / right;
			return true;
	}
}

Type simpleType(const TypeKind kind) {
	return (Type) { .kind = kind, .scale = NULL };
}

Type typeOf(const Expression * expression, const Scope * scope) {
	if (expression->type == FACTOR_EXPRESSION) {
		return _typeOfFactor(expression->factor, scope);
	}
	if (expression->type == LOGICAL_NOT) {
		const Type operand = typeOf(expression->operand, scope);
		if (operand.kind == ERROR_TYPE) {
			return operand;
		}
		if (operand.kind != BOOLEAN_TYPE) {
			reportSemanticError(expression->position, "'not' se aplica a una condición, pero recibe %s.", describeType(operand));
			return simpleType(ERROR_TYPE);
		}
		return operand;
	}
	const Type left = typeOf(expression->leftExpression, scope);
	const Type right = typeOf(expression->rightExpression, scope);
	if (left.kind == ERROR_TYPE || right.kind == ERROR_TYPE) {
		return simpleType(ERROR_TYPE);
	}
	if (expression->type == LOGICAL_AND || expression->type == LOGICAL_OR) {
		if (left.kind != BOOLEAN_TYPE || right.kind != BOOLEAN_TYPE) {
			const char * leftDescription = describeType(left);
			reportSemanticError(expression->position, "'%s' une condiciones, pero recibe %s y %s.",
				(expression->type == LOGICAL_AND) ? "and" : "or", leftDescription, describeType(right));
			return simpleType(ERROR_TYPE);
		}
		return left;
	}
	if (_isComparison(expression->type)) {
		return _typeOfComparison(expression, left, right);
	}
	const Type result = _arithmeticType(expression->type, left, right);
	if (result.kind == ERROR_TYPE) {
		const char * leftDescription = describeType(left);
		reportSemanticError(expression->position, "no se puede %s %s y %s.",
			_verbOf(expression->type), leftDescription, describeType(right));
	}
	return result;
}
