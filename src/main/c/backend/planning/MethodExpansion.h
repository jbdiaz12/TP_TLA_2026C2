#ifndef METHOD_EXPANSION_HEADER
#define METHOD_EXPANSION_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../semantic-analysis/SymbolTable.h"
#include "../semantic-analysis/TypeChecking.h"
#include "Calendar.h"
#include "PlanningError.h"
#include "Work.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * Builds the work of every topic without subtopics, which are the ones that
 * the planner schedules (a topic with subtopics is studied through them).
 * It chooses the method of each topic, runs it with the arguments of its
 * invocation, and repeats it until it covers the estimated time of the
 * topic. It reports the methods that can't cover a topic, and returns the
 * works in the order their topics are declared.
 */
Work * expandWorks(const DeclarationList * declarations, const SymbolTable * table, int * count);

#endif
