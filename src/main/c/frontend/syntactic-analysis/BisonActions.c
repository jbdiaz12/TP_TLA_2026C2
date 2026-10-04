#include "BisonActions.h"

/* MODULE INTERNAL STATE */

static CompilerState * _compilerState = NULL;
static Logger * _logger = NULL;


/** Shutdown module's internal state. */
void _shutdownBisonActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: BisonActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_compilerState = NULL;
}

ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState) {
	_compilerState = compilerState;
	_logger = createLogger("BisonActions");
	return _shutdownBisonActionsModule;
}

/* PRIVATE FUNCTIONS */

static void _logSyntacticAnalyzerAction(const char * functionName);

/**
 * Logs a syntactic-analyzer action in DEBUGGING level.
 */
static void _logSyntacticAnalyzerAction(const char * functionName) {
	logDebugging(_logger, "%s", functionName);
}

/**
 * Every list of this grammar is left-recursive, so the new item always belongs
 * at the end of the list that is being built. The macro walks the list to find
 * its last node and links the new one there, which keeps the order of the
 * program and avoids writing the same loop once per type of list.
 */
#define APPEND(ListType, list, field, item)					\
	do {													\
		ListType * _node = calloc(1, sizeof(ListType));		\
		_node->field = (item);								\
		ListType * _last = (list);							\
		while (_last->next != NULL) {						\
			_last = _last->next;							\
		}													\
		_last->next = _node;								\
	} while (0)

/* PUBLIC FUNCTIONS */

/** Constants. */

Constant * DateConstantSemanticAction(const int value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Constant * constant = calloc(1, sizeof(Constant));
	constant->date = value;
	constant->type = DATE_CONSTANT;
	return constant;
}

Constant * DurationConstantSemanticAction(const int minutes) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Constant * constant = calloc(1, sizeof(Constant));
	constant->duration = minutes;
	constant->type = DURATION_CONSTANT;
	return constant;
}

Constant * IntegerConstantSemanticAction(const int value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Constant * constant = calloc(1, sizeof(Constant));
	constant->integer = value;
	constant->type = INTEGER_CONSTANT;
	return constant;
}

Constant * StringConstantSemanticAction(char * value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Constant * constant = calloc(1, sizeof(Constant));
	constant->string = value;
	constant->type = STRING_CONSTANT;
	return constant;
}

Constant * TimeConstantSemanticAction(const int minutes) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Constant * constant = calloc(1, sizeof(Constant));
	constant->time = minutes;
	constant->type = TIME_CONSTANT;
	return constant;
}

/** Factors. */

Factor * ConstantFactorSemanticAction(Constant * constant) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->constant = constant;
	factor->type = CONSTANT_FACTOR;
	return factor;
}

Factor * InvocationFactorSemanticAction(char * function, ExpressionList * arguments) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->function = function;
	factor->arguments = arguments;
	factor->type = INVOCATION_FACTOR;
	return factor;
}

Factor * ListFactorSemanticAction(ExpressionList * elements) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->elements = elements;
	factor->type = LIST_FACTOR;
	return factor;
}

Factor * ParenthesizedFactorSemanticAction(Expression * expression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->expression = expression;
	factor->type = PARENTHESIZED_FACTOR;
	return factor;
}

Factor * VariableFactorSemanticAction(char * variable) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->variable = variable;
	factor->type = VARIABLE_FACTOR;
	return factor;
}

/**
 * The implicit quantifiers of a constraint ("goal" and "topic") are reserved
 * words, so the lexical-analyzer never emits them as an IDENTIFIER. They
 * behave as a variable anywhere inside an expression, and this action copies
 * their name so that the node owns its own memory, like every other variable.
 */
Factor * QuantifierFactorSemanticAction(const char * quantifier) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	char * name = calloc(1 + strlen(quantifier), sizeof(char));
	strcpy(name, quantifier);
	return VariableFactorSemanticAction(name);
}

/** Expressions. */

Expression * BinaryExpressionSemanticAction(Expression * leftExpression, Expression * rightExpression, ExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->leftExpression = leftExpression;
	expression->rightExpression = rightExpression;
	expression->type = type;
	return expression;
}

Expression * FactorExpressionSemanticAction(Factor * factor) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->factor = factor;
	expression->type = FACTOR_EXPRESSION;
	return expression;
}

Expression * UnaryExpressionSemanticAction(Expression * operand, ExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->operand = operand;
	expression->type = type;
	return expression;
}

ExpressionList * SingletonExpressionListSemanticAction(Expression * expression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ExpressionList * list = calloc(1, sizeof(ExpressionList));
	list->expression = expression;
	return list;
}

ExpressionList * ExpressionListSemanticAction(ExpressionList * list, Expression * expression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	APPEND(ExpressionList, list, expression, expression);
	return list;
}

/** Goals and topics. */

Attribute * AttributeSemanticAction(char * name, ExpressionList * values) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Attribute * attribute = calloc(1, sizeof(Attribute));
	attribute->name = name;
	attribute->values = values;
	return attribute;
}

/**
 * The attribute "method" needs a production of its own because METHOD is a
 * reserved word, so the lexical-analyzer never emits it as an IDENTIFIER. The
 * name is written here instead of being carried by the token.
 */
Attribute * MethodAttributeSemanticAction(ExpressionList * values) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	char * name = calloc(7, sizeof(char));
	strcpy(name, "method");
	return AttributeSemanticAction(name, values);
}

Member * AttributeMemberSemanticAction(Attribute * attribute) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Member * member = calloc(1, sizeof(Member));
	member->attribute = attribute;
	member->type = ATTRIBUTE_MEMBER;
	return member;
}

Member * TopicMemberSemanticAction(Topic * topic) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Member * member = calloc(1, sizeof(Member));
	member->topic = topic;
	member->type = TOPIC_MEMBER;
	return member;
}

MemberList * SingletonMemberListSemanticAction(Member * member) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	MemberList * list = calloc(1, sizeof(MemberList));
	list->member = member;
	return list;
}

MemberList * MemberListSemanticAction(MemberList * list, Member * member) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	APPEND(MemberList, list, member, member);
	return list;
}

Goal * GoalSemanticAction(char * name, MemberList * members) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Goal * goal = calloc(1, sizeof(Goal));
	goal->name = name;
	goal->members = members;
	return goal;
}

Topic * TopicSemanticAction(char * name, char * goal, MemberList * members) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Topic * topic = calloc(1, sizeof(Topic));
	topic->name = name;
	topic->goal = goal;
	topic->members = members;
	return topic;
}

/** Scales. */

LevelList * SingletonLevelListSemanticAction(char * level) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	LevelList * list = calloc(1, sizeof(LevelList));
	list->level = level;
	return list;
}

LevelList * LevelListSemanticAction(LevelList * list, char * level) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	APPEND(LevelList, list, level, level);
	return list;
}

Scale * ScaleSemanticAction(char * name, LevelList * levels) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Scale * scale = calloc(1, sizeof(Scale));
	scale->name = name;
	scale->levels = levels;
	return scale;
}

/** Methods. */

Parameter * ParameterSemanticAction(char * name, char * type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Parameter * parameter = calloc(1, sizeof(Parameter));
	parameter->name = name;
	parameter->type = type;
	return parameter;
}

ParameterList * SingletonParameterListSemanticAction(Parameter * parameter) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ParameterList * list = calloc(1, sizeof(ParameterList));
	list->parameter = parameter;
	return list;
}

ParameterList * ParameterListSemanticAction(ParameterList * list, Parameter * parameter) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	APPEND(ParameterList, list, parameter, parameter);
	return list;
}

/**
 * A session and a pause only differ in their type, so a single action builds
 * both of them.
 */
Statement * DurationStatementSemanticAction(Expression * duration, StatementType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = calloc(1, sizeof(Statement));
	statement->duration = duration;
	statement->type = type;
	return statement;
}

Statement * AfterStatementSemanticAction(Expression * delay, Statement * delayedStatement) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = calloc(1, sizeof(Statement));
	statement->delay = delay;
	statement->delayedStatement = delayedStatement;
	statement->type = AFTER_STATEMENT;
	return statement;
}

Statement * RepeatStatementSemanticAction(Expression * count, StatementList * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = calloc(1, sizeof(Statement));
	statement->count = count;
	statement->repeatBody = body;
	statement->type = REPEAT_STATEMENT;
	return statement;
}

Statement * ForStatementSemanticAction(char * variable, Expression * iterable, StatementList * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = calloc(1, sizeof(Statement));
	statement->variable = variable;
	statement->iterable = iterable;
	statement->forBody = body;
	statement->type = FOR_STATEMENT;
	return statement;
}

StatementList * SingletonStatementListSemanticAction(Statement * statement) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	StatementList * list = calloc(1, sizeof(StatementList));
	list->statement = statement;
	return list;
}

StatementList * StatementListSemanticAction(StatementList * list, Statement * statement) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	APPEND(StatementList, list, statement, statement);
	return list;
}

Method * MethodSemanticAction(char * name, ParameterList * parameters, StatementList * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Method * method = calloc(1, sizeof(Method));
	method->name = name;
	method->parameters = parameters;
	method->body = body;
	return method;
}

/** Availability. */

Period * SinglePeriodSemanticAction(Expression * amount) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Period * period = calloc(1, sizeof(Period));
	period->amount = amount;
	period->type = SINGLE_PERIOD;
	return period;
}

Period * RangePeriodSemanticAction(Expression * from, Expression * to) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Period * period = calloc(1, sizeof(Period));
	period->from = from;
	period->to = to;
	period->type = RANGE_PERIOD;
	return period;
}

PeriodList * SingletonPeriodListSemanticAction(Period * period) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	PeriodList * list = calloc(1, sizeof(PeriodList));
	list->period = period;
	return list;
}

PeriodList * PeriodListSemanticAction(PeriodList * list, Period * period) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	APPEND(PeriodList, list, period, period);
	return list;
}

Slot * SlotSemanticAction(char * day, PeriodList * periods) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Slot * slot = calloc(1, sizeof(Slot));
	slot->day = day;
	slot->periods = periods;
	return slot;
}

SlotList * SingletonSlotListSemanticAction(Slot * slot) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	SlotList * list = calloc(1, sizeof(SlotList));
	list->slot = slot;
	return list;
}

SlotList * SlotListSemanticAction(SlotList * list, Slot * slot) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	APPEND(SlotList, list, slot, slot);
	return list;
}

Availability * AvailabilitySemanticAction(SlotList * slots) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Availability * availability = calloc(1, sizeof(Availability));
	availability->slots = slots;
	return availability;
}

/** Constraints. */

ConstraintList * SingletonConstraintListSemanticAction(Expression * constraint) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ConstraintList * list = calloc(1, sizeof(ConstraintList));
	list->constraint = constraint;
	return list;
}

ConstraintList * ConstraintListSemanticAction(ConstraintList * list, Expression * constraint) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	APPEND(ConstraintList, list, constraint, constraint);
	return list;
}

/** Rules. */

Rule * WhenRuleSemanticAction(Expression * condition, Expression * action) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Rule * rule = calloc(1, sizeof(Rule));
	rule->condition = condition;
	rule->action = action;
	rule->type = WHEN_RULE;
	return rule;
}

/**
 * The default rule carries no condition, so its field stays NULL: the
 * destructor of an expression already tolerates it.
 */
Rule * OtherwiseRuleSemanticAction(Expression * action) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Rule * rule = calloc(1, sizeof(Rule));
	rule->condition = NULL;
	rule->action = action;
	rule->type = OTHERWISE_RULE;
	return rule;
}

RuleList * SingletonRuleListSemanticAction(Rule * rule) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	RuleList * list = calloc(1, sizeof(RuleList));
	list->rule = rule;
	return list;
}

RuleList * RuleListSemanticAction(RuleList * list, Rule * rule) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	APPEND(RuleList, list, rule, rule);
	return list;
}

/** Plan. */

AttributeList * SingletonAttributeListSemanticAction(Attribute * attribute) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	AttributeList * list = calloc(1, sizeof(AttributeList));
	list->attribute = attribute;
	return list;
}

AttributeList * AttributeListSemanticAction(AttributeList * list, Attribute * attribute) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	APPEND(AttributeList, list, attribute, attribute);
	return list;
}

/** Program. */

Declaration * AvailabilityDeclarationSemanticAction(Availability * availability) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->availability = availability;
	declaration->type = AVAILABILITY_DECLARATION;
	return declaration;
}

Declaration * ConstraintsDeclarationSemanticAction(ConstraintList * constraints) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->constraints = constraints;
	declaration->type = CONSTRAINTS_DECLARATION;
	return declaration;
}

/**
 * The attributes are NULL when the block of the plan is empty.
 */
Declaration * PlanDeclarationSemanticAction(AttributeList * attributes) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->plan = attributes;
	declaration->type = PLAN_DECLARATION;
	return declaration;
}

Declaration * RulesDeclarationSemanticAction(RuleList * rules) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->rules = rules;
	declaration->type = RULES_DECLARATION;
	return declaration;
}

Declaration * MethodDeclarationSemanticAction(Method * method) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->method = method;
	declaration->type = METHOD_DECLARATION;
	return declaration;
}

Declaration * ScaleDeclarationSemanticAction(Scale * scale) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->scale = scale;
	declaration->type = SCALE_DECLARATION;
	return declaration;
}

Declaration * GoalDeclarationSemanticAction(Goal * goal) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->goal = goal;
	declaration->type = GOAL_DECLARATION;
	return declaration;
}

Declaration * TopicDeclarationSemanticAction(Topic * topic) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->topic = topic;
	declaration->type = TOPIC_DECLARATION;
	return declaration;
}

DeclarationList * SingletonDeclarationListSemanticAction(Declaration * declaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	DeclarationList * list = calloc(1, sizeof(DeclarationList));
	list->declaration = declaration;
	return list;
}

DeclarationList * DeclarationListSemanticAction(DeclarationList * list, Declaration * declaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	APPEND(DeclarationList, list, declaration, declaration);
	return list;
}

Program * ProgramSemanticAction(DeclarationList * declarations) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Program * program = calloc(1, sizeof(Program));
	program->declarations = declarations;
	_compilerState->abstractSyntaxtTree = program;
	return program;
}
