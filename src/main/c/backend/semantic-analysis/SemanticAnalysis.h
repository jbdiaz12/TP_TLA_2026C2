#ifndef SEMANTIC_ANALYSIS_HEADER
#define SEMANTIC_ANALYSIS_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../planning/Calendar.h"
#include "../planning/WeeklyAvailability.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/ModuleDestructor.h"
#include "MethodValidation.h"
#include "SemanticError.h"
#include "SymbolTable.h"
#include "TypeChecking.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** Initialize module's internal state. */
ModuleDestructor initializeSemanticAnalysisModule();

/**
 * Validates that a syntactically valid program also makes sense in the domain
 * of the study plans. It reports every error found, not only the first one,
 * and fails if there is at least one.
 */
CompilationStatus executeSemanticAnalysis(Program * program);

#endif
