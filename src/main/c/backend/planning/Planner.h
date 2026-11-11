#ifndef PLANNER_HEADER
#define PLANNER_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/ModuleDestructor.h"
#include "../semantic-analysis/SymbolTable.h"
#include "Assignment.h"
#include "Calendar.h"
#include "MethodExpansion.h"
#include "WeeklyAvailability.h"
#include "PlanningError.h"
#include "Work.h"
#include <stdio.h>
#include <string.h>

/** Initialize module's internal state. */
ModuleDestructor initializePlannerModule();

/**
 * Schedules the plan of a program that passed the semantic-analysis, and
 * returns the resulting schedule (that the caller destroys). A program
 * without a plan is only validated, so it has nothing to schedule, and the
 * result is NULL, as well as when the plan can't be scheduled.
 */
CompilationStatus executePlanning(const Program * program, const SymbolTable * table, Schedule ** result);

#endif
