#ifndef BISON_ACTIONS_HEADER
#define BISON_ACTIONS_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../../support/type/TokenLabel.h"
#include "../Frontend.h"
#include "AbstractSyntaxTree.h"
#include "BisonParser.h"
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeBisonActionsModule();

/**
 * Bison semantic actions.
 *
 * Every action allocates a node of the AST and links it to the nodes of the
 * symbols at the right-hand side of its production. Because Bison reduces in
 * post-order, those children are already complete when the action runs.
 */

/** Constants, factors and expressions. */

Constant * DateConstantSemanticAction(const int value);
Constant * DurationConstantSemanticAction(const int minutes);
Constant * IntegerConstantSemanticAction(const int value);
Constant * StringConstantSemanticAction(char * value);
Constant * TimeConstantSemanticAction(const int minutes);

Factor * ConstantFactorSemanticAction(Constant * constant, Position position);
Factor * InvocationFactorSemanticAction(char * function, ExpressionList * arguments, Position position);
Factor * ListFactorSemanticAction(ExpressionList * elements, Position position);
Factor * ParenthesizedFactorSemanticAction(Expression * expression, Position position);
Factor * VariableFactorSemanticAction(char * variable, Position position);
Factor * QuantifierFactorSemanticAction(const char * quantifier, Position position);

Expression * BinaryExpressionSemanticAction(Expression * leftExpression, Expression * rightExpression, ExpressionType type, Position position);
Expression * FactorExpressionSemanticAction(Factor * factor, Position position);
Expression * UnaryExpressionSemanticAction(Expression * operand, ExpressionType type, Position position);

ExpressionList * ExpressionListSemanticAction(ExpressionList * list, Expression * expression);
ExpressionList * SingletonExpressionListSemanticAction(Expression * expression);

/** Goals and topics. */

Attribute * AttributeSemanticAction(char * name, ExpressionList * values, Position position);
Attribute * MethodAttributeSemanticAction(ExpressionList * values, Position position);

Member * AttributeMemberSemanticAction(Attribute * attribute);
Member * TopicMemberSemanticAction(Topic * topic);

MemberList * MemberListSemanticAction(MemberList * list, Member * member);
MemberList * SingletonMemberListSemanticAction(Member * member);

Goal * GoalSemanticAction(char * name, MemberList * members, Position position);
Topic * TopicSemanticAction(char * name, char * goal, MemberList * members, Position position, Position goalPosition);

/** Scales. */

LevelList * LevelListSemanticAction(LevelList * list, char * level, Position position);
LevelList * SingletonLevelListSemanticAction(char * level, Position position);

Scale * ScaleSemanticAction(char * name, LevelList * levels, Position position);

/** Methods. */

Parameter * ParameterSemanticAction(char * name, char * type, Expression * defaultValue, Position position);

ParameterList * ParameterListSemanticAction(ParameterList * list, Parameter * parameter);
ParameterList * SingletonParameterListSemanticAction(Parameter * parameter);

Statement * AfterStatementSemanticAction(Expression * delay, Statement * delayedStatement, Position position);
Statement * DurationStatementSemanticAction(Expression * duration, StatementType type, Position position);
Statement * ForStatementSemanticAction(char * variable, Expression * iterable, StatementList * body, Position position);
Statement * RepeatStatementSemanticAction(Expression * count, StatementList * body, Position position);

StatementList * SingletonStatementListSemanticAction(Statement * statement);
StatementList * StatementListSemanticAction(StatementList * list, Statement * statement);

Method * MethodSemanticAction(char * name, ParameterList * parameters, StatementList * body, Position position);

/** Availability. */

Period * RangePeriodSemanticAction(Expression * from, Expression * to);
Period * SinglePeriodSemanticAction(Expression * amount);

PeriodList * PeriodListSemanticAction(PeriodList * list, Period * period);
PeriodList * SingletonPeriodListSemanticAction(Period * period);

Slot * SlotSemanticAction(char * day, PeriodList * periods, Position position);

SlotList * SingletonSlotListSemanticAction(Slot * slot);
SlotList * SlotListSemanticAction(SlotList * list, Slot * slot);

Availability * AvailabilitySemanticAction(SlotList * slots);

/** Constraints. */

ConstraintList * ConstraintListSemanticAction(ConstraintList * list, Expression * constraint);
ConstraintList * SingletonConstraintListSemanticAction(Expression * constraint);

/** Rules. */

Rule * OtherwiseRuleSemanticAction(Expression * action, Position position);
Rule * WhenRuleSemanticAction(Expression * condition, Expression * action, Position position);

RuleList * RuleListSemanticAction(RuleList * list, Rule * rule);
RuleList * SingletonRuleListSemanticAction(Rule * rule);

/** Plan. */

AttributeList * AttributeListSemanticAction(AttributeList * list, Attribute * attribute);
AttributeList * SingletonAttributeListSemanticAction(Attribute * attribute);

/** Program. */

Declaration * AvailabilityDeclarationSemanticAction(Availability * availability, Position position);
Declaration * ConstraintsDeclarationSemanticAction(ConstraintList * constraints, Position position);
Declaration * GoalDeclarationSemanticAction(Goal * goal, Position position);
Declaration * PlanDeclarationSemanticAction(AttributeList * attributes, Position position);
Declaration * RulesDeclarationSemanticAction(RuleList * rules, Position position);
Declaration * MethodDeclarationSemanticAction(Method * method, Position position);
Declaration * ScaleDeclarationSemanticAction(Scale * scale, Position position);
Declaration * TopicDeclarationSemanticAction(Topic * topic, Position position);

DeclarationList * DeclarationListSemanticAction(DeclarationList * list, Declaration * declaration);
DeclarationList * SingletonDeclarationListSemanticAction(Declaration * declaration);

Program * ProgramSemanticAction(DeclarationList * declarations);

#endif
