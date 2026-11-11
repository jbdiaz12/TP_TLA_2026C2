#ifndef GENERATOR_HEADER
#define GENERATOR_HEADER

#include "../../support/configuration/Environment.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/ModuleDestructor.h"
#include "../planning/Schedule.h"
#include "ICalendar.h"
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

/** Initialize module's internal state. */
ModuleDestructor initializeGeneratorModule();

/**
 * Writes the artifacts of a schedule in the output folder ("out", or the one
 * in the variable STUJFY_OUTPUT_PATH): the calendar "plan.ics".
 */
CompilationStatus executeGenerator(const Schedule * schedule);

#endif
