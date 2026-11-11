#ifndef PLANNING_ERROR_HEADER
#define PLANNING_ERROR_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stdarg.h>
#include <stdio.h>

/**
 * Reports an error found while planning: a program that is valid, but whose
 * plan can't be scheduled (e.g., its study time doesn't fit before its
 * deadline). When the position has no line, the message has no location.
 */
void reportPlanningError(const Position position, const char * const format, ...);

/** Restarts the count of the planning errors reported. */
void resetPlanningErrors();

/** The amount of planning errors reported since the last restart. */
unsigned int planningErrorCount();

#endif
