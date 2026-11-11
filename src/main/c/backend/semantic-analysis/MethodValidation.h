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

#endif
