%{

#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonActions.h"
#include <stdio.h>

/**
 * Prints the beginning of a syntax error message, with the file (only when
 * it isn't the main program) and the location of the error.
 */
static void _printSyntaxErrorPrefix(const YYLTYPE * location) {
	if (location->file != NULL) {
		fprintf(stderr, "Error sintáctico en %s (%d:%d): ", location->file, location->first_line, location->first_column);
	}
	else {
		fprintf(stderr, "Error sintáctico (%d:%d): ", location->first_line, location->first_column);
	}
}

/**
 * The error reporting function for Bison parser. Syntax errors are reported
 * by "yyreport_syntax_error" (see the epilogue), so this function only
 * receives the remaining failures, such as running out of memory.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Error-Reporting-Function.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Tracking-Locations.html
 */
void yyerror(const YYLTYPE * location, const char * message) {
	_printSyntaxErrorPrefix(location);
	fprintf(stderr, "%s.\n", message);
}

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
			(location).file = YYRHSLOC((rhs), 1).file; \
		} else { \
			(location).first_column = (location).last_column = YYRHSLOC((rhs), 0).last_column; \
			(location).first_line = (location).last_line = YYRHSLOC((rhs), 0).last_line; \
			(location).file = YYRHSLOC((rhs), 0).file; \
		} \
	} while (0)

%}

/**
 * The location of a symbol. Besides its lines and columns, it remembers the
 * file that contains it, because a program can include other files. The file
 * is NULL for the main program, which is read from the standard input.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Location-Type.html
 */
%code requires {
	typedef struct YYLTYPE {
		int first_line;
		int first_column;
		int last_line;
		int last_column;
		const char * file;
	} YYLTYPE;
	#define YYLTYPE_IS_DECLARED 1
	#define YYLTYPE_IS_TRIVIAL 1
}

/** Converts the location of a symbol into the position that the AST keeps. */
%code {
	#define POSITION(location) ((Position) { .file = (location).file, .line = (location).first_line, .column = (location).first_column })
}

// You touch this, and you die.
%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error custom
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
%destructor { destroyMethod($$); } <method>
%destructor { destroyLevelList($$); } <levelList>
%destructor { destroyParameter($$); } <parameter>
%destructor { destroyParameterList($$); } <parameterList>
%destructor { destroyScale($$); } <scale>
%destructor { destroyStatement($$); } <statement>
%destructor { destroyStatementList($$); } <statementList>
%destructor { destroyAttributeList($$); } <attributeList>
%destructor { destroyAvailability($$); } <availability>
%destructor { destroyConstraintList($$); } <constraintList>
%destructor { destroyPeriod($$); } <period>
%destructor { destroyPeriodList($$); } <periodList>
%destructor { destroyRule($$); } <rule>
%destructor { destroyRuleList($$); } <ruleList>
%destructor { destroySlot($$); } <slot>
%destructor { destroySlotList($$); } <slotList>
%destructor { destroyTopic($$); } <topic>

/**
 * The aliases are the names that a syntax error shows to the user (e.g.,
 * "se esperaba una fecha"), so they're written in the language of the user.
 */

/** Terminals: literals. */
%token <integer> DATE "una fecha"
%token <integer> DURATION "una duración"
%token <integer> INTEGER "un entero"
%token <integer> TIME "una hora"
%token <string> IDENTIFIER "un identificador"
%token <string> STRING "una cadena"

/** Terminals: reserved words. */
%token <token> AFTER "after"
%token <token> AND "and"
%token <token> AVAILABILITY "availability"
%token <token> CONSTRAINTS "constraints"
%token <token> DEFAULT "default"
%token <token> FOR "for"
%token <token> GOAL "goal"
%token <token> IN "in"
%token <token> METHOD "method"
%token <token> NOT "not"
%token <token> OR "or"
%token <token> OTHERWISE "otherwise"
%token <token> PAUSE "pause"
%token <token> PLAN "plan"
%token <token> REPEAT "repeat"
%token <token> RULES "rules"
%token <token> SCALE "scale"
%token <token> SESSION "session"
%token <token> TOPIC "topic"
%token <token> WHEN "when"

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
%type <levelList> levelList
%type <method> methodDeclaration
%type <parameter> parameter
%type <parameterList> parameterList
%type <scale> scaleDeclaration
%type <statement> statement
%type <statementList> statementList
%type <attributeList> attributeList planDeclaration
%type <availability> availabilityDeclaration
%type <constraintList> constraintList constraintsDeclaration
%type <period> period
%type <periodList> periodList
%type <rule> rule
%type <ruleList> ruleList rulesDeclaration
%type <slot> slot
%type <slotList> slotList
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

// This grammar has no nullable productions on purpose: every empty block is
// written as an explicit production instead of an empty list.

program: declarationList																{ $$ = ProgramSemanticAction($1); }
	;

declarationList: declaration															{ $$ = SingletonDeclarationListSemanticAction($1); }
	| declarationList[list] declaration[item]											{ $$ = DeclarationListSemanticAction($list, $item); }
	;

declaration: goalDeclaration															{ $$ = GoalDeclarationSemanticAction($1, POSITION(@1)); }
	| topicDeclaration																	{ $$ = TopicDeclarationSemanticAction($1, POSITION(@1)); }
	| scaleDeclaration																	{ $$ = ScaleDeclarationSemanticAction($1, POSITION(@1)); }
	| methodDeclaration																	{ $$ = MethodDeclarationSemanticAction($1, POSITION(@1)); }
	| availabilityDeclaration															{ $$ = AvailabilityDeclarationSemanticAction($1, POSITION(@1)); }
	| constraintsDeclaration															{ $$ = ConstraintsDeclarationSemanticAction($1, POSITION(@1)); }
	| rulesDeclaration																	{ $$ = RulesDeclarationSemanticAction($1, POSITION(@1)); }
	| planDeclaration																	{ $$ = PlanDeclarationSemanticAction($1, POSITION(@1)); }
	;

/** Availability. */

availabilityDeclaration: AVAILABILITY OPEN_BRACE slotList[slots] CLOSE_BRACE				{ $$ = AvailabilitySemanticAction($slots); }
	;

slotList: slot																			{ $$ = SingletonSlotListSemanticAction($1); }
	| slotList[list] slot[item]															{ $$ = SlotListSemanticAction($list, $item); }
	;

slot: IDENTIFIER[day] COLON periodList[periods] SEMICOLON								{ $$ = SlotSemanticAction($day, $periods, POSITION(@day)); }
	;

periodList: period																		{ $$ = SingletonPeriodListSemanticAction($1); }
	| periodList[list] COMMA period[item]												{ $$ = PeriodListSemanticAction($list, $item); }
	;

period: expression[amount]																{ $$ = SinglePeriodSemanticAction($amount); }
	| expression[from] RANGE expression[to]												{ $$ = RangePeriodSemanticAction($from, $to); }
	;

/** Constraints. */

constraintsDeclaration: CONSTRAINTS OPEN_BRACE constraintList[constraints] CLOSE_BRACE	{ $$ = $constraints; }
	;

constraintList: expression[constraint] SEMICOLON										{ $$ = SingletonConstraintListSemanticAction($constraint); }
	| constraintList[list] expression[item] SEMICOLON									{ $$ = ConstraintListSemanticAction($list, $item); }
	;

/** Rules. */

rulesDeclaration: RULES OPEN_BRACE ruleList[rules] CLOSE_BRACE							{ $$ = $rules; }
	;

ruleList: rule																			{ $$ = SingletonRuleListSemanticAction($1); }
	| ruleList[list] rule[item]															{ $$ = RuleListSemanticAction($list, $item); }
	;

rule: WHEN expression[condition] ARROW expression[action] SEMICOLON						{ $$ = WhenRuleSemanticAction($condition, $action); }
	| OTHERWISE ARROW expression[action] SEMICOLON										{ $$ = OtherwiseRuleSemanticAction($action); }
	;

/** Plan. */

planDeclaration: PLAN OPEN_BRACE attributeList[attributes] CLOSE_BRACE					{ $$ = $attributes; }
	| PLAN OPEN_BRACE CLOSE_BRACE														{ $$ = NULL; }
	;

attributeList: attribute																{ $$ = SingletonAttributeListSemanticAction($1); }
	| attributeList[list] attribute[item]												{ $$ = AttributeListSemanticAction($list, $item); }
	;

/** Scales. */

scaleDeclaration: SCALE IDENTIFIER[name] OPEN_BRACE levelList[levels] CLOSE_BRACE		{ $$ = ScaleSemanticAction($name, $levels, POSITION(@name)); }
	;

levelList: IDENTIFIER[level]															{ $$ = SingletonLevelListSemanticAction($level, POSITION(@level)); }
	| levelList[list] LESS IDENTIFIER[item]												{ $$ = LevelListSemanticAction($list, $item, POSITION(@item)); }
	;

/** Methods. */

methodDeclaration: METHOD IDENTIFIER[name] OPEN_PARENTHESIS CLOSE_PARENTHESIS OPEN_BRACE statementList[body] CLOSE_BRACE							{ $$ = MethodSemanticAction($name, NULL, $body, POSITION(@name)); }
	| METHOD IDENTIFIER[name] OPEN_PARENTHESIS parameterList[parameters] CLOSE_PARENTHESIS OPEN_BRACE statementList[body] CLOSE_BRACE				{ $$ = MethodSemanticAction($name, $parameters, $body, POSITION(@name)); }
	;

parameterList: parameter																{ $$ = SingletonParameterListSemanticAction($1); }
	| parameterList[list] COMMA parameter[item]											{ $$ = ParameterListSemanticAction($list, $item); }
	;

parameter: IDENTIFIER[name] COLON IDENTIFIER[type]										{ $$ = ParameterSemanticAction($name, $type, NULL, POSITION(@name)); }
	| IDENTIFIER[name] COLON IDENTIFIER[type] DEFAULT expression[value]					{ $$ = ParameterSemanticAction($name, $type, $value, POSITION(@name)); }
	;

statementList: statement																{ $$ = SingletonStatementListSemanticAction($1); }
	| statementList[list] statement[item]												{ $$ = StatementListSemanticAction($list, $item); }
	;

statement: SESSION expression[duration] SEMICOLON										{ $$ = DurationStatementSemanticAction($duration, SESSION_STATEMENT); }
	| PAUSE expression[duration] SEMICOLON												{ $$ = DurationStatementSemanticAction($duration, PAUSE_STATEMENT); }
	| AFTER expression[delay] statement[delayed]										{ $$ = AfterStatementSemanticAction($delay, $delayed); }
	| REPEAT expression[count] OPEN_BRACE statementList[body] CLOSE_BRACE				{ $$ = RepeatStatementSemanticAction($count, $body); }
	| FOR IDENTIFIER[variable] IN expression[iterable] OPEN_BRACE statementList[body] CLOSE_BRACE	{ $$ = ForStatementSemanticAction($variable, $iterable, $body); }
	;

/** Goals and topics. */

goalDeclaration: GOAL IDENTIFIER[name] OPEN_BRACE memberList[members] CLOSE_BRACE		{ $$ = GoalSemanticAction($name, $members, POSITION(@name)); }
	| GOAL IDENTIFIER[name] OPEN_BRACE CLOSE_BRACE										{ $$ = GoalSemanticAction($name, NULL, POSITION(@name)); }
	;

topicDeclaration: TOPIC IDENTIFIER[name] OPEN_BRACE memberList[members] CLOSE_BRACE		{ $$ = TopicSemanticAction($name, NULL, $members, POSITION(@name), POSITION(@name)); }
	| TOPIC IDENTIFIER[name] OPEN_BRACE CLOSE_BRACE										{ $$ = TopicSemanticAction($name, NULL, NULL, POSITION(@name), POSITION(@name)); }
	| TOPIC IDENTIFIER[name] IN IDENTIFIER[goal] OPEN_BRACE memberList[members] CLOSE_BRACE	{ $$ = TopicSemanticAction($name, $goal, $members, POSITION(@name), POSITION(@goal)); }
	| TOPIC IDENTIFIER[name] IN IDENTIFIER[goal] OPEN_BRACE CLOSE_BRACE					{ $$ = TopicSemanticAction($name, $goal, NULL, POSITION(@name), POSITION(@goal)); }
	;

memberList: member																		{ $$ = SingletonMemberListSemanticAction($1); }
	| memberList[list] member[item]														{ $$ = MemberListSemanticAction($list, $item); }
	;

member: attribute																		{ $$ = AttributeMemberSemanticAction($1); }
	| topicDeclaration																	{ $$ = TopicMemberSemanticAction($1); }
	;

attribute: IDENTIFIER[name] COLON expressionList[values] SEMICOLON						{ $$ = AttributeSemanticAction($name, $values, POSITION(@name)); }
	| METHOD COLON expressionList[values] SEMICOLON										{ $$ = MethodAttributeSemanticAction($values, POSITION(@1)); }
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

factor: OPEN_PARENTHESIS expression CLOSE_PARENTHESIS									{ $$ = ParenthesizedFactorSemanticAction($2, POSITION(@$)); }
	| OPEN_BRACKET expressionList[elements] CLOSE_BRACKET								{ $$ = ListFactorSemanticAction($elements, POSITION(@$)); }
	| OPEN_BRACKET CLOSE_BRACKET														{ $$ = ListFactorSemanticAction(NULL, POSITION(@$)); }
	| IDENTIFIER[function] OPEN_PARENTHESIS expressionList[arguments] CLOSE_PARENTHESIS	{ $$ = InvocationFactorSemanticAction($function, $arguments, POSITION(@$)); }
	| IDENTIFIER[function] OPEN_PARENTHESIS CLOSE_PARENTHESIS							{ $$ = InvocationFactorSemanticAction($function, NULL, POSITION(@$)); }
	| IDENTIFIER[variable]																{ $$ = VariableFactorSemanticAction($variable, POSITION(@$)); }
	| GOAL																				{ $$ = QuantifierFactorSemanticAction("goal", POSITION(@$)); }
	| TOPIC																				{ $$ = QuantifierFactorSemanticAction("topic", POSITION(@$)); }
	| constant																			{ $$ = ConstantFactorSemanticAction($1, POSITION(@$)); }
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

/** The longest list of expected symbols that is still useful to the user. */
#define MAXIMUM_EXPECTED_SYMBOLS 5

/**
 * Prints the name of a symbol. Literals and identifiers are described (e.g.,
 * "una fecha"), while reserved words and punctuation are quoted, so that a
 * lone ";" can't be confused with the punctuation of the message itself.
 */
static void _printSymbol(const yysymbol_kind_t symbol) {
	switch (symbol) {
		case YYSYMBOL_DATE:
		case YYSYMBOL_DURATION:
		case YYSYMBOL_IDENTIFIER:
		case YYSYMBOL_INTEGER:
		case YYSYMBOL_STRING:
		case YYSYMBOL_TIME:
			fprintf(stderr, "%s", yysymbol_name(symbol));
			break;
		case YYSYMBOL_YYEOF:
			fprintf(stderr, "el final del archivo");
			break;
		default:
			fprintf(stderr, "'%s'", yysymbol_name(symbol));
			break;
	}
}

/**
 * Reports a syntax error with its location, the symbol found and, when there
 * are just a few, the symbols that were expected instead. An unknown symbol
 * or an exception was already reported by the lexical-analyzer, so they're
 * not reported twice.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Syntax-Error-Reporting-Function.html
 */
static int yyreport_syntax_error(const yypcontext_t * context) {
	const yysymbol_kind_t found = yypcontext_token(context);
	if (found == YYSYMBOL_UNKNOWN || found == YYSYMBOL_EXCEPTION) {
		return 0;
	}
	_printSyntaxErrorPrefix(yypcontext_location(context));
	if (found == YYSYMBOL_YYEOF) {
		fprintf(stderr, "el programa termina antes de tiempo");
	}
	else if (found != YYSYMBOL_YYEMPTY) {
		fprintf(stderr, "no se esperaba ");
		_printSymbol(found);
	}
	else {
		fprintf(stderr, "el programa no respeta la gramática");
	}
	yysymbol_kind_t expected[MAXIMUM_EXPECTED_SYMBOLS];
	const int total = yypcontext_expected_tokens(context, NULL, 0);
	if (0 < total && total <= MAXIMUM_EXPECTED_SYMBOLS) {
		yypcontext_expected_tokens(context, expected, MAXIMUM_EXPECTED_SYMBOLS);
		fprintf(stderr, "; se esperaba ");
		for (int k = 0; k < total; ++k) {
			fprintf(stderr, "%s", (k == 0) ? "" : (k == total - 1) ? " o " : ", ");
			_printSymbol(expected[k]);
		}
	}
	fprintf(stderr, ".\n");
	return 0;
}
