%{

#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonActions.h"

/**
 * The error reporting function for Bison parser.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Error-Reporting-Function.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Tracking-Locations.html
 */
void yyerror(const YYLTYPE * location, const char * message) {}

/**
 * @see https://www.gnu.org/software/bison/manual/html_node/Location-Default-Action.html
 */
# define YYLLOC_DEFAULT(location, rhs, k) \
	do { \
		if ((k)) { \
			(location).first_column = YYRHSLOC((rhs), 1).first_column; \
			(location).first_line = YYRHSLOC((rhs), 1).first_line; \
			(location).last_column = YYRHSLOC((rhs), (k)).last_column; \
			(location).last_line = YYRHSLOC((rhs), (k)).last_line; \
		} else { \
			(location).first_column = (location).last_column = YYRHSLOC((rhs), 0).last_column; \
			(location).first_line = (location).last_line = YYRHSLOC((rhs), 0).last_line; \
		} \
	} while (0)

%}

// You touch this, and you die.
%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations

%union {
	/** Terminals. */

	signed int integer;
	char * string;
	TokenLabel token;

	/** Non-terminals. */

	Attribute * attribute;
	AttributeList * attributeList;
	Availability * availability;
	Constant * constant;
	ConstraintList * constraintList;
	Declaration * declaration;
	DeclarationList * declarationList;
	Expression * expression;
	ExpressionList * expressionList;
	Factor * factor;
	Goal * goal;
	LevelList * levelList;
	Member * member;
	MemberList * memberList;
	Method * method;
	Parameter * parameter;
	ParameterList * parameterList;
	Period * period;
	PeriodList * periodList;
	Program * program;
	Rule * rule;
	RuleList * ruleList;
	Scale * scale;
	Slot * slot;
	SlotList * slotList;
	Statement * statement;
	StatementList * statementList;
	Topic * topic;
}

/**
 * Destructors. This functions are executed after the parsing ends, so if the
 * AST must be used in the following phases of the compiler you shouldn't used
 * this approach for the AST root node ("program" non-terminal, in this
 * grammar), or it will drop the entire tree even if the parsing succeeds.
 *
 * The lexemes of the terminals that transport a string live in heap-memory
 * too, so they need a destructor of their own: without it, a parsing error
 * leaks every identifier and literal read up to that point.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Destructor-Decl.html
 */
%destructor { free($$); } <string>
%destructor { destroyAttribute($$); } <attribute>
%destructor { destroyConstant($$); } <constant>
%destructor { destroyDeclaration($$); } <declaration>
%destructor { destroyDeclarationList($$); } <declarationList>
%destructor { destroyExpression($$); } <expression>
%destructor { destroyExpressionList($$); } <expressionList>
%destructor { destroyFactor($$); } <factor>
%destructor { destroyGoal($$); } <goal>
%destructor { destroyMember($$); } <member>
%destructor { destroyMemberList($$); } <memberList>
%destructor { destroyTopic($$); } <topic>

/** Terminals: literals. */
%token <integer> DATE
%token <integer> DURATION
%token <integer> INTEGER
%token <integer> TIME
%token <string> IDENTIFIER
%token <string> STRING

/** Terminals: reserved words. */
%token <token> AFTER
%token <token> AND
%token <token> AVAILABILITY
%token <token> CONSTRAINTS
%token <token> FOR
%token <token> GOAL
%token <token> IN
%token <token> METHOD
%token <token> NOT
%token <token> OR
%token <token> OTHERWISE
%token <token> PAUSE
%token <token> PLAN
%token <token> REPEAT
%token <token> RULES
%token <token> SCALE
%token <token> SESSION
%token <token> TOPIC
%token <token> WHEN

/** Terminals: operators and punctuation. */
%token <token> ADD "+"
%token <token> ARROW "->"
%token <token> CLOSE_BRACE "}"
%token <token> CLOSE_BRACKET "]"
%token <token> CLOSE_COMMENT "*/"
%token <token> CLOSE_PARENTHESIS ")"
%token <token> COLON ":"
%token <token> COMMA ","
%token <token> DIV "/"
%token <token> EQUAL "=="
%token <token> GREATER ">"
%token <token> GREATER_EQUAL ">="
%token <token> LESS "<"
%token <token> LESS_EQUAL "<="
%token <token> MUL "*"
%token <token> NOT_EQUAL "!="
%token <token> OPEN_BRACE "{"
%token <token> OPEN_BRACKET "["
%token <token> OPEN_COMMENT "/*"
%token <token> OPEN_PARENTHESIS "("
%token <token> RANGE ".."
%token <token> SEMICOLON ";"
%token <token> SUB "-"

/** Terminals: control. */
%token <token> EXCEPTION
%token <token> IGNORED
%token <token> UNKNOWN

/** Non-terminals. */
%type <attribute> attribute
%type <constant> constant
%type <declaration> declaration
%type <declarationList> declarationList
%type <expression> expression
%type <expressionList> expressionList
%type <factor> factor
%type <goal> goalDeclaration
%type <member> member
%type <memberList> memberList
%type <program> program
%type <topic> topicDeclaration

/**
 * Precedence and associativity, from the lowest to the highest.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Precedence.html
 */
%left OR
%left AND
%precedence NOT
%left EQUAL NOT_EQUAL
%left LESS GREATER LESS_EQUAL GREATER_EQUAL
%left ADD SUB
%left MUL DIV

%%

// IMPORTANT: To use λ in the following grammar, use the %empty symbol.
// This grammar has no nullable productions on purpose: every empty block is
// written as an explicit production instead of an empty list.

program: declarationList																{ $$ = ProgramSemanticAction($1); }
	;

declarationList: declaration															{ $$ = SingletonDeclarationListSemanticAction($1); }
	| declarationList[list] declaration[item]											{ $$ = DeclarationListSemanticAction($list, $item); }
	;

declaration: goalDeclaration															{ $$ = GoalDeclarationSemanticAction($1); }
	| topicDeclaration																	{ $$ = TopicDeclarationSemanticAction($1); }
	;

/** Goals and topics. */

goalDeclaration: GOAL IDENTIFIER[name] OPEN_BRACE memberList[members] CLOSE_BRACE		{ $$ = GoalSemanticAction($name, $members); }
	| GOAL IDENTIFIER[name] OPEN_BRACE CLOSE_BRACE										{ $$ = GoalSemanticAction($name, NULL); }
	;

topicDeclaration: TOPIC IDENTIFIER[name] OPEN_BRACE memberList[members] CLOSE_BRACE		{ $$ = TopicSemanticAction($name, NULL, $members); }
	| TOPIC IDENTIFIER[name] OPEN_BRACE CLOSE_BRACE										{ $$ = TopicSemanticAction($name, NULL, NULL); }
	| TOPIC IDENTIFIER[name] IN IDENTIFIER[goal] OPEN_BRACE memberList[members] CLOSE_BRACE	{ $$ = TopicSemanticAction($name, $goal, $members); }
	| TOPIC IDENTIFIER[name] IN IDENTIFIER[goal] OPEN_BRACE CLOSE_BRACE					{ $$ = TopicSemanticAction($name, $goal, NULL); }
	;

memberList: member																		{ $$ = SingletonMemberListSemanticAction($1); }
	| memberList[list] member[item]														{ $$ = MemberListSemanticAction($list, $item); }
	;

member: attribute																		{ $$ = AttributeMemberSemanticAction($1); }
	| topicDeclaration																	{ $$ = TopicMemberSemanticAction($1); }
	;

attribute: IDENTIFIER[name] COLON expressionList[values] SEMICOLON						{ $$ = AttributeSemanticAction($name, $values); }
	| METHOD COLON expressionList[values] SEMICOLON										{ $$ = MethodAttributeSemanticAction($values); }
	;

/** Expressions. */

expression: expression[left] OR expression[right]										{ $$ = BinaryExpressionSemanticAction($left, $right, LOGICAL_OR); }
	| expression[left] AND expression[right]											{ $$ = BinaryExpressionSemanticAction($left, $right, LOGICAL_AND); }
	| NOT expression[operand]															{ $$ = UnaryExpressionSemanticAction($operand, LOGICAL_NOT); }
	| expression[left] EQUAL expression[right]											{ $$ = BinaryExpressionSemanticAction($left, $right, COMPARE_EQUAL); }
	| expression[left] NOT_EQUAL expression[right]										{ $$ = BinaryExpressionSemanticAction($left, $right, COMPARE_NOT_EQUAL); }
	| expression[left] LESS expression[right]											{ $$ = BinaryExpressionSemanticAction($left, $right, COMPARE_LESS); }
	| expression[left] GREATER expression[right]										{ $$ = BinaryExpressionSemanticAction($left, $right, COMPARE_GREATER); }
	| expression[left] LESS_EQUAL expression[right]										{ $$ = BinaryExpressionSemanticAction($left, $right, COMPARE_LESS_EQUAL); }
	| expression[left] GREATER_EQUAL expression[right]									{ $$ = BinaryExpressionSemanticAction($left, $right, COMPARE_GREATER_EQUAL); }
	| expression[left] ADD expression[right]											{ $$ = BinaryExpressionSemanticAction($left, $right, ARITHMETIC_ADD); }
	| expression[left] SUB expression[right]											{ $$ = BinaryExpressionSemanticAction($left, $right, ARITHMETIC_SUB); }
	| expression[left] MUL expression[right]											{ $$ = BinaryExpressionSemanticAction($left, $right, ARITHMETIC_MUL); }
	| expression[left] DIV expression[right]											{ $$ = BinaryExpressionSemanticAction($left, $right, ARITHMETIC_DIV); }
	| factor																			{ $$ = FactorExpressionSemanticAction($1); }
	;

factor: OPEN_PARENTHESIS expression CLOSE_PARENTHESIS									{ $$ = ParenthesizedFactorSemanticAction($2); }
	| OPEN_BRACKET expressionList[elements] CLOSE_BRACKET								{ $$ = ListFactorSemanticAction($elements); }
	| OPEN_BRACKET CLOSE_BRACKET														{ $$ = ListFactorSemanticAction(NULL); }
	| IDENTIFIER[function] OPEN_PARENTHESIS expressionList[arguments] CLOSE_PARENTHESIS	{ $$ = InvocationFactorSemanticAction($function, $arguments); }
	| IDENTIFIER[function] OPEN_PARENTHESIS CLOSE_PARENTHESIS							{ $$ = InvocationFactorSemanticAction($function, NULL); }
	| IDENTIFIER[variable]																{ $$ = VariableFactorSemanticAction($variable); }
	| constant																			{ $$ = ConstantFactorSemanticAction($1); }
	;

constant: INTEGER																		{ $$ = IntegerConstantSemanticAction($1); }
	| DURATION																			{ $$ = DurationConstantSemanticAction($1); }
	| TIME																				{ $$ = TimeConstantSemanticAction($1); }
	| DATE																				{ $$ = DateConstantSemanticAction($1); }
	| STRING																			{ $$ = StringConstantSemanticAction($1); }
	;

expressionList: expression																{ $$ = SingletonExpressionListSemanticAction($1); }
	| expressionList[list] COMMA expression[item]										{ $$ = ExpressionListSemanticAction($list, $item); }
	;

%%
