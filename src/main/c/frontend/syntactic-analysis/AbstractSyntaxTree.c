#include "AbstractSyntaxTree.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownAbstractSyntaxTreeModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: AbstractSyntaxTree...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeAbstractSyntaxTreeModule() {
	_logger = createLogger("AbstractSyntaxTree");
	return _shutdownAbstractSyntaxTreeModule;
}

/* PUBLIC FUNCTIONS */

void destroyConstant(Constant * constant) {
	if (constant != NULL) {
		if (constant->type == STRING_CONSTANT) {
			free(constant->string);
		}
		free(constant);
	}
}

void destroyFactor(Factor * factor) {
	if (factor != NULL) {
		switch (factor->type) {
			case CONSTANT_FACTOR:
				destroyConstant(factor->constant);
				break;
			case INVOCATION_FACTOR:
				free(factor->function);
				destroyExpressionList(factor->arguments);
				break;
			case LIST_FACTOR:
				destroyExpressionList(factor->elements);
				break;
			case PARENTHESIZED_FACTOR:
				destroyExpression(factor->expression);
				break;
			case VARIABLE_FACTOR:
				free(factor->variable);
				break;
			default:
				logError(_logger, "The specified factor type is unknown: %d", factor->type);
				break;
		}
		free(factor);
	}
}

void destroyExpression(Expression * expression) {
	if (expression != NULL) {
		switch (expression->type) {
			case ARITHMETIC_ADD:
			case ARITHMETIC_DIV:
			case ARITHMETIC_MUL:
			case ARITHMETIC_SUB:
			case COMPARE_EQUAL:
			case COMPARE_GREATER:
			case COMPARE_GREATER_EQUAL:
			case COMPARE_LESS:
			case COMPARE_LESS_EQUAL:
			case COMPARE_NOT_EQUAL:
			case LOGICAL_AND:
			case LOGICAL_OR:
				destroyExpression(expression->leftExpression);
				destroyExpression(expression->rightExpression);
				break;
			case LOGICAL_NOT:
				destroyExpression(expression->operand);
				break;
			case FACTOR_EXPRESSION:
				destroyFactor(expression->factor);
				break;
			default:
				logError(_logger, "The specified expression type is unknown: %d", expression->type);
				break;
		}
		free(expression);
	}
}

void destroyExpressionList(ExpressionList * expressionList) {
	while (expressionList != NULL) {
		ExpressionList * next = expressionList->next;
		destroyExpression(expressionList->expression);
		free(expressionList);
		expressionList = next;
	}
}

void destroyLevelList(LevelList * levelList) {
	while (levelList != NULL) {
		LevelList * next = levelList->next;
		free(levelList->level);
		free(levelList);
		levelList = next;
	}
}

void destroyScale(Scale * scale) {
	if (scale != NULL) {
		free(scale->name);
		destroyLevelList(scale->levels);
		free(scale);
	}
}

void destroyParameter(Parameter * parameter) {
	if (parameter != NULL) {
		free(parameter->name);
		free(parameter->type);
		free(parameter);
	}
}

void destroyParameterList(ParameterList * parameterList) {
	while (parameterList != NULL) {
		ParameterList * next = parameterList->next;
		destroyParameter(parameterList->parameter);
		free(parameterList);
		parameterList = next;
	}
}

void destroyStatement(Statement * statement) {
	if (statement != NULL) {
		switch (statement->type) {
			case PAUSE_STATEMENT:
			case SESSION_STATEMENT:
				destroyExpression(statement->duration);
				break;
			case AFTER_STATEMENT:
				destroyExpression(statement->delay);
				destroyStatement(statement->delayedStatement);
				break;
			case REPEAT_STATEMENT:
				destroyExpression(statement->count);
				destroyStatementList(statement->repeatBody);
				break;
			case FOR_STATEMENT:
				free(statement->variable);
				destroyExpression(statement->iterable);
				destroyStatementList(statement->forBody);
				break;
			default:
				logError(_logger, "The specified statement type is unknown: %d", statement->type);
				break;
		}
		free(statement);
	}
}

void destroyStatementList(StatementList * statementList) {
	while (statementList != NULL) {
		StatementList * next = statementList->next;
		destroyStatement(statementList->statement);
		free(statementList);
		statementList = next;
	}
}

void destroyMethod(Method * method) {
	if (method != NULL) {
		free(method->name);
		destroyParameterList(method->parameters);
		destroyStatementList(method->body);
		free(method);
	}
}

void destroyAttribute(Attribute * attribute) {
	if (attribute != NULL) {
		free(attribute->name);
		destroyExpressionList(attribute->values);
		free(attribute);
	}
}

void destroyAttributeList(AttributeList * attributeList) {
	while (attributeList != NULL) {
		AttributeList * next = attributeList->next;
		destroyAttribute(attributeList->attribute);
		free(attributeList);
		attributeList = next;
	}
}

void destroyMember(Member * member) {
	if (member != NULL) {
		switch (member->type) {
			case ATTRIBUTE_MEMBER:
				destroyAttribute(member->attribute);
				break;
			case TOPIC_MEMBER:
				destroyTopic(member->topic);
				break;
			default:
				logError(_logger, "The specified member type is unknown: %d", member->type);
				break;
		}
		free(member);
	}
}

void destroyMemberList(MemberList * memberList) {
	while (memberList != NULL) {
		MemberList * next = memberList->next;
		destroyMember(memberList->member);
		free(memberList);
		memberList = next;
	}
}

void destroyGoal(Goal * goal) {
	if (goal != NULL) {
		free(goal->name);
		destroyMemberList(goal->members);
		free(goal);
	}
}

void destroyTopic(Topic * topic) {
	if (topic != NULL) {
		free(topic->name);
		free(topic->goal);
		destroyMemberList(topic->members);
		free(topic);
	}
}

void destroyPeriod(Period * period) {
	if (period != NULL) {
		switch (period->type) {
			case SINGLE_PERIOD:
				destroyExpression(period->amount);
				break;
			case RANGE_PERIOD:
				destroyExpression(period->from);
				destroyExpression(period->to);
				break;
			default:
				logError(_logger, "The specified period type is unknown: %d", period->type);
				break;
		}
		free(period);
	}
}

void destroyPeriodList(PeriodList * periodList) {
	while (periodList != NULL) {
		PeriodList * next = periodList->next;
		destroyPeriod(periodList->period);
		free(periodList);
		periodList = next;
	}
}

void destroySlot(Slot * slot) {
	if (slot != NULL) {
		free(slot->day);
		destroyPeriodList(slot->periods);
		free(slot);
	}
}

void destroySlotList(SlotList * slotList) {
	while (slotList != NULL) {
		SlotList * next = slotList->next;
		destroySlot(slotList->slot);
		free(slotList);
		slotList = next;
	}
}

void destroyAvailability(Availability * availability) {
	if (availability != NULL) {
		destroySlotList(availability->slots);
		free(availability);
	}
}

void destroyConstraintList(ConstraintList * constraintList) {
	while (constraintList != NULL) {
		ConstraintList * next = constraintList->next;
		destroyExpression(constraintList->constraint);
		free(constraintList);
		constraintList = next;
	}
}

void destroyRule(Rule * rule) {
	if (rule != NULL) {
		destroyExpression(rule->condition);
		destroyExpression(rule->action);
		free(rule);
	}
}

void destroyRuleList(RuleList * ruleList) {
	while (ruleList != NULL) {
		RuleList * next = ruleList->next;
		destroyRule(ruleList->rule);
		free(ruleList);
		ruleList = next;
	}
}

void destroyDeclaration(Declaration * declaration) {
	if (declaration != NULL) {
		switch (declaration->type) {
			case AVAILABILITY_DECLARATION:
				destroyAvailability(declaration->availability);
				break;
			case CONSTRAINTS_DECLARATION:
				destroyConstraintList(declaration->constraints);
				break;
			case GOAL_DECLARATION:
				destroyGoal(declaration->goal);
				break;
			case METHOD_DECLARATION:
				destroyMethod(declaration->method);
				break;
			case PLAN_DECLARATION:
				destroyAttributeList(declaration->plan);
				break;
			case RULES_DECLARATION:
				destroyRuleList(declaration->rules);
				break;
			case SCALE_DECLARATION:
				destroyScale(declaration->scale);
				break;
			case TOPIC_DECLARATION:
				destroyTopic(declaration->topic);
				break;
			default:
				logError(_logger, "The specified declaration type is unknown: %d", declaration->type);
				break;
		}
		free(declaration);
	}
}

void destroyDeclarationList(DeclarationList * declarationList) {
	while (declarationList != NULL) {
		DeclarationList * next = declarationList->next;
		destroyDeclaration(declarationList->declaration);
		free(declarationList);
		declarationList = next;
	}
}

void destroyProgram(Program * program) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (program != NULL) {
		destroyDeclarationList(program->declarations);
		free(program);
	}
}