#ifndef METHOD_VALIDATION_HEADER
#define METHOD_VALIDATION_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "SemanticError.h"
#include "SymbolTable.h"
#include "TypeChecking.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * Validates the parameters of a method (their types and default values) and
 * every statement of its body.
 */
void validateMethodBody(const SymbolTable * table, const Method * method);

/**
 * Validates the invocation of a method, which assigns that method to the
 * topics: the value of a "method" attribute, or the action of a rule. The
 * method is declared, and the arguments fill its parameters in order, where
 * the trailing ones can be omitted if they have a default value. The context
 * names where the invocation is written, for the messages.
 */
void validateInvocation(const SymbolTable * table, const DeclarationList * declarations, const Expression * expression, const char * context);

#endif
