#ifndef SEMANTIC_ERROR_HEADER
#define SEMANTIC_ERROR_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stdarg.h>
#include <stdio.h>

/**
 * Describes where something was declared, to point at it from a message
 * (e.g., "en la línea 3", or "en <metodos> (línea 3)"). The description is
 * overwritten by the next call, so it's used only once per message.
 */
const char * describePosition(const Position position);

/**
 * Reports a semantic error to the user, in the standard error output and with
 * the file (only when it isn't the main program) and the position of the
 * symbol that causes it.
 */
void reportSemanticError(const Position position, const char * const format, ...);

/** Restarts the count of the semantic errors reported. */
void resetSemanticErrors();

/** The amount of semantic errors reported since the last restart. */
unsigned int semanticErrorCount();

#endif
