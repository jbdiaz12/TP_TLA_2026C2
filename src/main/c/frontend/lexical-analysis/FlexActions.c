#include "FlexActions.h"

/* MODULE INTERNAL STATE */

static bool _logIgnoredLexemes = true;
static LexicalAnalyzer * _lexicalAnalyzer = NULL;
static Logger * _logger = NULL;

/** Where the last multiline comment was opened, to report it if it's never closed. */
static YYLTYPE _commentLocation;

/** The folder of the standard library, where "include <name>" looks for "name.stujfy". */
static const char * _libraryPath = NULL;

/**
 * The deepest chain of nested includes. Every file is included only once, so
 * this limit is only reached by a really deep (and odd) chain of libraries.
 */
#define MAXIMUM_INCLUDE_DEPTH 16

/** An included file that is being scanned, and where to resume the file that includes it. */
typedef struct {
	InputBuffer * inputBuffer;
	YYLTYPE resumeLocation;
} IncludedFile;

static IncludedFile _includeStack[MAXIMUM_INCLUDE_DEPTH];
static int _includeDepth = 0;

/**
 * Every file included so far: its canonical path, used to include it only
 * once, and the name shown in the messages (e.g., "<metodos>"). The names
 * live until the end of the compilation, because the locations of the AST
 * point to them.
 */
typedef struct SourceFile {
	char * path;
	char * name;
	struct SourceFile * next;
} SourceFile;

static SourceFile * _sourceFiles = NULL;

/** Shutdown module's internal state. */
void _shutdownFlexActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: FlexActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	/**
	 * When the compilation fails inside an included file, its buffers are
	 * still stacked. They're popped here, because "yylex_destroy" only
	 * deletes the buffer at the top of the stack and leaks the rest.
	 */
	while (0 < _includeDepth) {
		--_includeDepth;
		popInputBuffer(_lexicalAnalyzer);
		destroyInputBuffer(_includeStack[_includeDepth].inputBuffer);
	}
	while (_sourceFiles != NULL) {
		SourceFile * next = _sourceFiles->next;
		free(_sourceFiles->path);
		free(_sourceFiles->name);
		free(_sourceFiles);
		_sourceFiles = next;
	}
	_lexicalAnalyzer = NULL;
}

ModuleDestructor initializeFlexActionsModule(LexicalAnalyzer * lexicalAnalyzer) {
	_lexicalAnalyzer = lexicalAnalyzer;
	_logger = createLogger("FlexActions");
	_logIgnoredLexemes = getBooleanOrDefault("LOG_IGNORED_LEXEMES", _logIgnoredLexemes);
	_libraryPath = getStringOrDefault("STUJFY_LIBRARY_PATH", "src/main/stujfy/lib");
	return _shutdownFlexActionsModule;
}

/* PRIVATE FUNCTIONS */

static char * _copyLexeme(const char * lexeme, const unsigned int length);
static CompilationStatus _include(const char * path, char * name, const bool isLibrary, const YYLTYPE * includeLocation);
static bool _isIncluded(const char * canonicalPath);
static void _logTokenAction(const char * actionName, Token * token);
static CompilationStatus _push(Token * token, const char * actionName);
static void _reportLexicalError(const YYLTYPE * location, const char * const format, ...);
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
 * Starts scanning an included file, unless it was already included before:
 * like "#pragma once" in C, every file is included only once, which also
 * prevents two libraries that include each other from looping forever. The
 * name is owned by this function, which keeps it or frees it.
 */
static CompilationStatus _include(const char * path, char * name, const bool isLibrary, const YYLTYPE * includeLocation) {
	char * canonicalPath = realpath(path, NULL);
	if (canonicalPath == NULL) {
		if (isLibrary) {
			_reportLexicalError(includeLocation, "no existe la librería %s.", name);
		}
		else {
			_reportLexicalError(includeLocation, "no se encuentra el archivo \"%s\".", name);
		}
		free(name);
		return _throw();
	}
	if (_isIncluded(canonicalPath)) {
		logDebugging(_logger, "The file \"%s\" is already included.", canonicalPath);
		free(canonicalPath);
		free(name);
		return IN_PROGRESS;
	}
	InputBuffer * inputBuffer = NULL;
	if (_includeDepth == MAXIMUM_INCLUDE_DEPTH || (inputBuffer = createInputBuffer(_lexicalAnalyzer, canonicalPath)) == NULL) {
		_reportLexicalError(includeLocation, "no se puede incluir %s.", name);
		free(canonicalPath);
		free(name);
		return _throw();
	}
	SourceFile * source = calloc(1, sizeof(SourceFile));
	source->path = canonicalPath;
	source->name = name;
	source->next = _sourceFiles;
	_sourceFiles = source;
	YYLTYPE * location = (YYLTYPE *) _lexicalAnalyzer->location;
	_includeStack[_includeDepth].inputBuffer = inputBuffer;
	_includeStack[_includeDepth].resumeLocation = *location;
	++_includeDepth;
	pushInputBuffer(inputBuffer);
	*location = (YYLTYPE) {
		.first_line = 1,
		.first_column = 1,
		.last_line = 1,
		.last_column = 1,
		.file = source->name
	};
	return IN_PROGRESS;
}

/**
 * Whether a file was already included, given its canonical path.
 */
static bool _isIncluded(const char * canonicalPath) {
	for (SourceFile * source = _sourceFiles; source != NULL; source = source->next) {
		if (strcmp(source->path, canonicalPath) == 0) {
			return true;
		}
	}
	return false;
}

/**
 * Reports a lexical error to the user, in the standard error output and with
 * the location where it takes place. Unlike the logs, it's always visible.
 */
static void _reportLexicalError(const YYLTYPE * location, const char * const format, ...) {
	if (location->file != NULL) {
		fprintf(stderr, "Error léxico en %s (%d:%d): ", location->file, location->first_line, location->first_column);
	}
	else {
		fprintf(stderr, "Error léxico (%d:%d): ", location->first_line, location->first_column);
	}
	va_list arguments;
	va_start(arguments, format);
	vfprintf(stderr, format, arguments);
	va_end(arguments);
	fprintf(stderr, "\n");
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
	_commentLocation = *((YYLTYPE *) _lexicalAnalyzer->location);
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
	YYLTYPE * location = (YYLTYPE *) _lexicalAnalyzer->location;
	/**
	 * The context is checked before the parser receives the end of the input.
	 * Otherwise, a program that is valid up to an unclosed comment would be
	 * accepted first, and rejected afterwards. Every file must close its own
	 * comments, so this also applies at the end of an included file.
	 */
	FlexContext context = currentLexicalAnalyzerContext(_lexicalAnalyzer);
	if (0 != context) {
		logError(_logger, "The final context is not closed (context=%s).", _toContextString(context));
		_reportLexicalError(&_commentLocation, "el comentario de bloque nunca se cierra.");
		status = _throw();
	}
	else if (popInputBuffer(_lexicalAnalyzer)) {
		/** An included file ends, so the file that includes it resumes. */
		--_includeDepth;
		destroyInputBuffer(_includeStack[_includeDepth].inputBuffer);
		*location = _includeStack[_includeDepth].resumeLocation;
	}
	else {
		/** The end of the input is located right after its last lexeme. */
		location->first_line = location->last_line;
		location->first_column = location->last_column;
		status = pushToken(_lexicalAnalyzer, token);
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

/**
 * Includes a file, like "#include" in C: its content is scanned right here,
 * as if it were written in place of the directive, so the parser never sees
 * the directive itself. A library ("include <name>") is searched for in the
 * library folder, and any other file ("include "path"") is relative to the
 * directory where the compiler runs.
 */
CompilationStatus IncludeLexemeAction(bool isLibrary) {
	Token * token = createToken(_lexicalAnalyzer, IGNORED);
	_logTokenAction(__FUNCTION__, token);
	const YYLTYPE includeLocation = *((YYLTYPE *) _lexicalAnalyzer->location);
	const char * opening = strchr(token->lexeme, isLibrary ? '<' : '"');
	const unsigned int length = token->length - (unsigned int) (opening - token->lexeme) - 2;
	char * target = _copyLexeme(opening + 1, length);
	destroyToken(token);
	CompilationStatus status;
	if (isLibrary) {
		char * path = concatenate(4, _libraryPath, "/", target, ".stujfy");
		status = _include(path, concatenate(3, "<", target, ">"), true, &includeLocation);
		free(path);
		free(target);
	}
	else if (length == 0) {
		_reportLexicalError(&includeLocation, "falta la ruta del archivo a incluir.");
		free(target);
		status = _throw();
	}
	else {
		status = _include(target, concatenate(1, target), false, &includeLocation);
		free(target);
	}
	return status;
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
 * An "include" that isn't followed by a library or a file, in the same line.
 */
CompilationStatus MalformedIncludeLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, UNKNOWN);
	_logTokenAction(__FUNCTION__, token);
	destroyToken(token);
	_reportLexicalError((YYLTYPE *) _lexicalAnalyzer->location,
		"después de include va una librería (<nombre>) o un archivo (\"ruta\").");
	return _throw();
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

/**
 * A lone quote means that a string literal reaches the end of its line
 * without being closed, and a byte outside the ASCII range is the first byte
 * of a character like "á" or "ñ", which identifiers don't admit. Both cases
 * get a message of their own, because the lexeme alone doesn't explain them.
 */
CompilationStatus UnknownLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, UNKNOWN);
	_logTokenAction(__FUNCTION__, token);
	YYLTYPE * location = (YYLTYPE *) _lexicalAnalyzer->location;
	if (token->lexeme[0] == '"') {
		_reportLexicalError(location,
			"la cadena no se cierra antes del final de la línea.");
	}
	else if ((unsigned char) token->lexeme[0] >= 0x80) {
		_reportLexicalError(location,
			"solo se admiten tildes y eñes dentro de una cadena entre comillas.");
	}
	else {
		_reportLexicalError(location,
			"no se reconoce el símbolo '%s'.", token->lexeme);
	}
	pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return FAILED;
}
