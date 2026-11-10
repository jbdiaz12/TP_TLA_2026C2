%top{

#include "../../support/type/CompilationStatus.h"
#include "../../support/type/FlexContext.h"
#include "../../support/type/TokenLabel.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include "../syntactic-analysis/BisonParser.h"

}

%{

/**
 * Updates the location of every lexeme before its action runs, so both
 * analyzers can report where an error takes place. The column counts
 * characters instead of bytes: the continuation bytes of a multi-byte UTF-8
 * character don't advance it.
 *
 * @see https://westes.github.io/flex/manual/Misc-Macros.html
 */
#define YY_USER_ACTION											\
	yylloc->first_line = yylloc->last_line;						\
	yylloc->first_column = yylloc->last_column;					\
	for (int k = 0; k < yyleng; ++k) {							\
		if (yytext[k] == '\n') {								\
			++yylloc->last_line;								\
			yylloc->last_column = 1;							\
		}														\
		else if ((yytext[k] & 0xC0) != 0x80) {					\
			++yylloc->last_column;								\
		}														\
	}


/**
 * @see https://westes.github.io/flex/manual/Definitions-Section.html#Definitions-Section
 */

extern CompilationStatus DateLexemeAction();
extern CompilationStatus DurationLexemeAction();
extern CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
extern CompilationStatus EOFLexemeAction();
extern CompilationStatus IdentifierLexemeAction();
extern CompilationStatus IgnoredLexemeAction();
extern CompilationStatus IntegerLexemeAction();
extern CompilationStatus KeywordLexemeAction(TokenLabel label);
extern CompilationStatus LeaveMultilineCommentLexemeAction();
extern CompilationStatus OperatorLexemeAction(TokenLabel label);
extern CompilationStatus StringLexemeAction();
extern CompilationStatus TimeLexemeAction();
extern CompilationStatus UnknownLexemeAction();

%}

/**
 * Flex contexts (a.k.a. start conditions).
 *
 * @see https://westes.github.io/flex/manual/Start-Conditions.html
 */
%x MULTILINE_COMMENT

/**
 * Reusable patterns.
 *
 * @see https://westes.github.io/flex/manual/Matching.html
 * @see https://westes.github.io/flex/manual/Patterns.html
 */
date					[[:digit:]]{4}"-"[[:digit:]]{2}"-"[[:digit:]]{2}
duration				([[:digit:]]+[wdhm])+
identifier				[[:alpha:]_][[:alnum:]_]*
integer					[[:digit:]]+
string					\"[^\"\n]*\"
time					[[:digit:]]{1,2}":"[[:digit:]]{2}
whitespace				[[:space:]]+

%%

%{

/**
 * @see https://westes.github.io/flex/manual/Rules-Section.html#Rules-Section
 */

%}

"/*"								{ return EnterMultilineCommentLexemeAction(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>"*/"				{ return LeaveMultilineCommentLexemeAction(); }
<MULTILINE_COMMENT>[^*]+			{ return IgnoredLexemeAction(); }
<MULTILINE_COMMENT>.				{ return IgnoredLexemeAction(); }

"//".*								{ return IgnoredLexemeAction(); }

"after"								{ return KeywordLexemeAction(AFTER); }
"and"								{ return KeywordLexemeAction(AND); }
"availability"						{ return KeywordLexemeAction(AVAILABILITY); }
"constraints"						{ return KeywordLexemeAction(CONSTRAINTS); }
"default"							{ return KeywordLexemeAction(DEFAULT); }
"for"								{ return KeywordLexemeAction(FOR); }
"goal"								{ return KeywordLexemeAction(GOAL); }
"in"								{ return KeywordLexemeAction(IN); }
"method"							{ return KeywordLexemeAction(METHOD); }
"not"								{ return KeywordLexemeAction(NOT); }
"or"								{ return KeywordLexemeAction(OR); }
"otherwise"							{ return KeywordLexemeAction(OTHERWISE); }
"pause"								{ return KeywordLexemeAction(PAUSE); }
"plan"								{ return KeywordLexemeAction(PLAN); }
"repeat"							{ return KeywordLexemeAction(REPEAT); }
"rules"								{ return KeywordLexemeAction(RULES); }
"scale"								{ return KeywordLexemeAction(SCALE); }
"session"							{ return KeywordLexemeAction(SESSION); }
"topic"								{ return KeywordLexemeAction(TOPIC); }
"when"								{ return KeywordLexemeAction(WHEN); }

"->"								{ return OperatorLexemeAction(ARROW); }
".."								{ return OperatorLexemeAction(RANGE); }
"<="								{ return OperatorLexemeAction(LESS_EQUAL); }
">="								{ return OperatorLexemeAction(GREATER_EQUAL); }
"=="								{ return OperatorLexemeAction(EQUAL); }
"!="								{ return OperatorLexemeAction(NOT_EQUAL); }
"<"									{ return OperatorLexemeAction(LESS); }
">"									{ return OperatorLexemeAction(GREATER); }

"+"									{ return OperatorLexemeAction(ADD); }
"-"									{ return OperatorLexemeAction(SUB); }
"*"									{ return OperatorLexemeAction(MUL); }
"/"									{ return OperatorLexemeAction(DIV); }

"{"									{ return OperatorLexemeAction(OPEN_BRACE); }
"}"									{ return OperatorLexemeAction(CLOSE_BRACE); }
"("									{ return OperatorLexemeAction(OPEN_PARENTHESIS); }
")"									{ return OperatorLexemeAction(CLOSE_PARENTHESIS); }
"["									{ return OperatorLexemeAction(OPEN_BRACKET); }
"]"									{ return OperatorLexemeAction(CLOSE_BRACKET); }
";"									{ return OperatorLexemeAction(SEMICOLON); }
":"									{ return OperatorLexemeAction(COLON); }
","									{ return OperatorLexemeAction(COMMA); }

{date}								{ return DateLexemeAction(); }
{time}								{ return TimeLexemeAction(); }
{duration}							{ return DurationLexemeAction(); }
{integer}							{ return IntegerLexemeAction(); }
{string}							{ return StringLexemeAction(); }
{identifier}						{ return IdentifierLexemeAction(); }

{whitespace}						{ return IgnoredLexemeAction(); }
.									{ return UnknownLexemeAction(); }

<<EOF>>								{ return EOFLexemeAction(); }

%%

/**
 * @see https://westes.github.io/flex/manual/User-Code-Section.html#User-Code-Section
 */

#include "FlexActions.h"
#include "FlexExport.h"
