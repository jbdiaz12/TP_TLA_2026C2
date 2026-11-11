#ifndef HTML_HEADER
#define HTML_HEADER

#include "../planning/Calendar.h"
#include "../planning/Schedule.h"
#include "../semantic-analysis/SymbolTable.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Writes a schedule as a single HTML page, without external resources: the
 * totals of the plan, the hours of each goal and topic, the graph of the
 * topics that require others, and the agenda day by day. It returns false
 * when the file can't be written.
 */
bool generateHtml(const Schedule * schedule, const SymbolTable * table, const char * path);

#endif
