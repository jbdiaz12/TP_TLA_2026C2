#include "FlexActions.h"

/* MODULE INTERNAL STATE */

static bool _logIgnoredLexemes = true;
static LexicalAnalyzer * _lexicalAnalyzer = NULL;
static Logger * _logger = NULL;


/** Shutdown module's internal state. */
void _shutdownFlexActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: FlexActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_lexicalAnalyzer = NULL;
}

ModuleDestructor initializeFlexActionsModule(LexicalAnalyzer * lexicalAnalyzer) {
	_lexicalAnalyzer = lexicalAnalyzer;
	_logger = createLogger("FlexActions");
	_logIgnoredLexemes = getBooleanOrDefault("LOG_IGNORED_LEXEMES", _logIgnoredLexemes);
	return _shutdownFlexActionsModule;
}

/* PRIVATE FUNCTIONS */

static char * _copyLexeme(const char * lexeme, const unsigned int length);
static void _logTokenAction(const char * actionName, Token * token);
static CompilationStatus _push(Token * token, const char * actionName);
static CompilationStatus _throw();
static const char * _toContextString(const FlexContext context);

/**
 * Copies the specified amount of characters of a lexeme into heap-memory. The
 * returned string must be freed by the owner of the AST node that holds it.
 */
static char * _copyLexeme(const char * lexeme, const unsigned int length) {
	char * copy = (char *) calloc(length + 1, sizeof(char));
	strncpy(copy, lexeme, length);
	return copy;
}

/**
 * Get the context string of the specified Flex context.
 */
static const char * _toContextString(const FlexContext context) {
	switch (context) {
		case 0: return "INITIAL";
		case 1: return "MULTILINE_COMMENT";
		default:
			logError(_logger, "The specified Flex context is unknown: %d", context);
			return "<UNKNOWN CONTEXT>";
	}
}

/**
 * Logs a lexical-analyzer action over a token in DEBUGGING level.
 */
static void _logTokenAction(const char * actionName, Token * token) {
	char * _lexeme = escape(token->lexeme);
	YYLTYPE * location = (YYLTYPE *) _lexicalAnalyzer->location;
	logDebugging(_logger,
		WARNING_COLOR "%s" DEFAULT_COLOR
		": Token(context=%s, label=%d, length=%d, lexeme=%s\"%s\"%s, line=%d, location=%d:%d-%d:%d, semanticValue=%p)",
		actionName,
		_toContextString(token->context),
		token->label,
		token->length,
		INFORMATION_COLOR, _lexeme, DEFAULT_COLOR,
		token->line,
		location->first_line,
		location->first_column,
		location->last_line,
		location->last_column,
		token->semanticValue);
	free(_lexeme);
	_lexeme = NULL;
}

/**
 * Logs, pushes and destroys a token, returning the resulting compilation
 * status. Every action that emits a symbol of the alphabet ends here.
 */
static CompilationStatus _push(Token * token, const char * actionName) {
	_logTokenAction(actionName, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

/**
 * Instructs the parser to halt execution unrecoverably.
 */
CompilationStatus _throw() {
	logError(_logger, "An exception is thrown.");
	Token * token = createToken(_lexicalAnalyzer, EXCEPTION);
	pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return FAILED;
}

/* PUBLIC FUNCTIONS */

/**
 * Maps a date with the shape "yyyy-mm-dd" into a single integer with the shape
 * yyyymmdd, which preserves the chronological order of the dates.
 */
CompilationStatus DateLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, DATE);
	int year = 0;
	int month = 0;
	int day = 0;
	sscanf(token->lexeme, "%d-%d-%d", &year, &month, &day);
	token->semanticValue->integer = (10000 * year) + (100 * month) + day;
	return _push(token, __FUNCTION__);
}

/**
 * Maps a duration with the shape "2w3d4h5m" into its total amount of minutes.
 * Every component is optional, but at least one must be present.
 */
CompilationStatus DurationLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, DURATION);
	int minutes = 0;
	int amount = 0;
	for (unsigned int k = 0; k < token->length; ++k) {
		const char character = token->lexeme[k];
		switch (character) {
			case 'w': minutes += 10080 * amount; amount = 0; break;
			case 'd': minutes += 1440 * amount; amount = 0; break;
			case 'h': minutes += 60 * amount; amount = 0; break;
			case 'm': minutes += amount; amount = 0; break;
			default: amount = (10 * amount) + (character - '0'); break;
		}
	}
	token->semanticValue->integer = minutes;
	return _push(token, __FUNCTION__);
}

CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context) {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, OPEN_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	enterLexicalAnalyzerContext(_lexicalAnalyzer, context);
	return IN_PROGRESS;
}

CompilationStatus EOFLexemeAction() {
	CompilationStatus status = IN_PROGRESS;
	Token * token = createToken(_lexicalAnalyzer, 0);
	_logTokenAction(__FUNCTION__, token);
	if (!popInputBuffer(_lexicalAnalyzer)) {
		status = pushToken(_lexicalAnalyzer, token);
		FlexContext context = currentLexicalAnalyzerContext(_lexicalAnalyzer);
		if (0 != context) {
			logError(_logger, "The final context is not closed (context=%s).", _toContextString(context));
			status = _throw();
		}
	}
	destroyToken(token);
	return status;
}

CompilationStatus IdentifierLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, IDENTIFIER);
	token->semanticValue->string = _copyLexeme(token->lexeme, token->length);
	return _push(token, __FUNCTION__);
}

CompilationStatus IgnoredLexemeAction() {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, IGNORED);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

CompilationStatus IntegerLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, INTEGER);
	token->semanticValue->integer = atoi(token->lexeme);
	return _push(token, __FUNCTION__);
}

/**
 * Every reserved word is mapped by this single action, because the symbol of
 * the alphabet can be determined from the lexeme itself.
 */
CompilationStatus KeywordLexemeAction(TokenLabel label) {
	Token * token = createToken(_lexicalAnalyzer, label);
	return _push(token, __FUNCTION__);
}

CompilationStatus LeaveMultilineCommentLexemeAction() {
	leaveLexicalAnalyzerContext(_lexicalAnalyzer);
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, CLOSE_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

/**
 * Every operator and punctuation mark is mapped by this single action.
 */
CompilationStatus OperatorLexemeAction(TokenLabel label) {
	Token * token = createToken(_lexicalAnalyzer, label);
	return _push(token, __FUNCTION__);
}

/**
 * Strips the surrounding quotes from the lexeme, because they delimit the
 * literal but are not part of its value.
 */
CompilationStatus StringLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, STRING);
	token->semanticValue->string = _copyLexeme(1 + token->lexeme, token->length - 2);
	return _push(token, __FUNCTION__);
}

/**
 * Maps a time with the shape "hh:mm" into its amount of minutes since midnight.
 */
CompilationStatus TimeLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, TIME);
	int hours = 0;
	int minutes = 0;
	sscanf(token->lexeme, "%d:%d", &hours, &minutes);
	token->semanticValue->integer = (60 * hours) + minutes;
	return _push(token, __FUNCTION__);
}

CompilationStatus UnknownLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, UNKNOWN);
	_logTokenAction(__FUNCTION__, token);
	pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return FAILED;
}
