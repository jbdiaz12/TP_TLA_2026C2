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

/** Initialize module's internal state. */
ModuleDestructor initializePlannerModule();

/**
 * Schedules the plan of a program that passed the semantic-analysis. A
 * program without a plan is only validated, so it has nothing to schedule.
 */
CompilationStatus executePlanning(const Program * program, const SymbolTable * table);

#endif
