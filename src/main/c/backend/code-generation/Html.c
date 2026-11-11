#include "Html.h"

/* MODULE INTERNAL STATE */

/** The colors of the goals, which repeat after the last one. */
#define GOAL_COLORS 8

/** The measures of the graph of dependencies, in pixels. */
#define GRAPH_MARGIN 16
#define NODE_HEIGHT 34
#define COLUMN_GAP 64
#define ROW_GAP 14

/** What a goal has in the schedule. */
typedef struct {
	const Symbol * symbol;
	int sessions;
	int study;
} GoalSummary;

/** What a topic has in the schedule, and where it goes in the graph. */
typedef struct {
	const Symbol * symbol;
	int goal;
	const char * method;
	int sessions;
	int study;
	int firstDay;
	int lastDay;
	int column;
	int row;
} TopicSummary;

/** A topic ("to") that requires another one ("from"). */
typedef struct {
	int from;
	int to;
} Edge;

typedef struct {
	GoalSummary * goals;
	int goalCount;
	TopicSummary * topics;
	int topicCount;
	Edge * edges;
	int edgeCount;
} Report;

/** The style of the page, with a light and a dark theme. */
static const char * const _STYLE =
	":root{--bg:#f6f6f3;--card:#fff;--ink:#1c1c1e;--muted:#6b6b70;--line:#e2e2de;--pause:#e6e6e2;"
	"--g0:#3866d4;--g1:#cf5a2a;--g2:#25936a;--g3:#8f4fcf;--g4:#b9871d;--g5:#cc4477;--g6:#23919f;--g7:#6a7a25}\n"
	"@media (prefers-color-scheme:dark){:root{--bg:#151517;--card:#1e1e21;--ink:#ececee;--muted:#9d9da5;--line:#303035;--pause:#2e2e33;"
	"--g0:#7ea4f6;--g1:#f0935f;--g2:#5cc794;--g3:#c08af0;--g4:#e3bc5a;--g5:#ee80a9;--g6:#5ec6d6;--g7:#b4c45c}}\n"
	"*{box-sizing:border-box}\n"
	"body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,\"Segoe UI\",Roboto,sans-serif}\n"
	"header,main,footer{max-width:960px;margin:0 auto;padding:0 16px}\n"
	"header{padding-top:40px}\n"
	"h1{font-size:28px;line-height:1.2;margin:0 0 4px}\n"
	"h2{font-size:20px;margin:40px 0 12px}\n"
	"h3{font-size:16px;margin:0}\n"
	"p{margin:4px 0}\n"
	".muted{color:var(--muted)}\n"
	".small{font-size:13px}\n"
	".stats{display:flex;flex-wrap:wrap;gap:12px;margin-top:20px}\n"
	".stats div{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:10px 16px;min-width:120px}\n"
	".stats strong{display:block;font-size:22px;font-variant-numeric:tabular-nums}\n"
	".card{background:var(--card);border:1px solid var(--line);border-left:4px solid var(--goal);border-radius:12px;padding:16px;margin-bottom:12px}\n"
	".c0{--goal:var(--g0)}.c1{--goal:var(--g1)}.c2{--goal:var(--g2)}.c3{--goal:var(--g3)}"
	".c4{--goal:var(--g4)}.c5{--goal:var(--g5)}.c6{--goal:var(--g6)}.c7{--goal:var(--g7)}\n"
	".table{overflow-x:auto}\n"
	"table{width:100%;border-collapse:collapse;margin-top:12px;font-size:14px}\n"
	"th{text-align:left;font-weight:600;color:var(--muted);border-bottom:1px solid var(--line);padding:6px 8px;white-space:nowrap}\n"
	"td{border-bottom:1px solid var(--line);padding:6px 8px;vertical-align:top}\n"
	"tr:last-child td{border-bottom:0}\n"
	"td.number{font-variant-numeric:tabular-nums;white-space:nowrap}\n"
	".bar{height:6px;border-radius:3px;background:var(--goal);margin-top:4px;min-width:4px}\n"
	".graph{overflow-x:auto;background:var(--card);border:1px solid var(--line);border-radius:12px;padding:8px}\n"
	".graph svg{display:block}\n"
	".graph rect{fill:var(--card);stroke:var(--goal);stroke-width:2}\n"
	".graph text{fill:var(--ink);font-size:13px}\n"
	".graph .edge{fill:none;stroke:var(--muted);stroke-width:1.5}\n"
	".graph .head{fill:var(--muted)}\n"
	".week h3{color:var(--muted);font-size:13px;font-weight:600;text-transform:uppercase;letter-spacing:.04em;margin:20px 0 8px}\n"
	".day{display:grid;grid-template-columns:120px 1fr;gap:12px;background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px 16px;margin-bottom:8px}\n"
	".date strong{display:block}\n"
	".date strong::first-letter{text-transform:uppercase}\n"
	".blocks{list-style:none;margin:0;padding:0;display:grid;gap:10px}\n"
	".block{border-left:4px solid var(--goal);padding-left:10px}\n"
	".block time{font-variant-numeric:tabular-nums;color:var(--muted);font-size:13px;margin-right:6px}\n"
	".steps{display:flex;gap:2px;height:6px;margin-top:6px;max-width:100%}\n"
	".steps span{background:var(--goal);border-radius:2px}\n"
	".steps .pause{background:var(--pause)}\n"
	"footer{padding-top:32px;padding-bottom:40px;color:var(--muted);font-size:13px}\n"
	"@media (max-width:600px){.day{grid-template-columns:1fr;gap:6px}}\n";

/* PRIVATE FUNCTIONS */

static void _addEdge(Report * report, const int from, const int to);
static const Attribute * _attributeOf(const MemberList * members, const char * name);
static void _destroyReport(Report * report);
static int _findGoal(const Report * report, const Symbol * symbol);
static int _findTopic(const Report * report, const Symbol * symbol);
static bool _isWithin(const Symbol * topic, const Symbol * ancestor);
static void _layOut(Report * report, int * columns, int * rows);
static const MemberList * _membersOf(const Symbol * symbol);
static void _readEdges(Report * report, const SymbolTable * table);
static Report _summarize(const Schedule * schedule, const SymbolTable * table);
static void _writeAgenda(FILE * file, const Schedule * schedule, const Report * report);
static void _writeBlock(FILE * file, const Schedule * schedule, const Report * report, const int first, const int last);
static void _writeDate(FILE * file, const int day, const bool hasYear);
static void _writeDuration(FILE * file, const int minutes);
static void _writeGoals(FILE * file, const Report * report);
static void _writeGraph(FILE * file, Report * report);
static void _writeHeader(FILE * file, const Schedule * schedule, const Report * report);
static void _writeText(FILE * file, const char * text);
static void _writeTime(FILE * file, const int minutes);

static void _addEdge(Report * report, const int from, const int to) {
	for (int k = 0; k < report->edgeCount; ++k) {
		if (report->edges[k].from == from && report->edges[k].to == to) {
			return;
		}
	}
	report->edges = realloc(report->edges, (report->edgeCount + 1) * sizeof(Edge));
	report->edges[report->edgeCount++] = (Edge) { .from = from, .to = to };
}

static const Attribute * _attributeOf(const MemberList * members, const char * name) {
	for (const MemberList * node = members; node != NULL; node = node->next) {
		if (node->member->type == ATTRIBUTE_MEMBER && strcmp(node->member->attribute->name, name) == 0) {
			return node->member->attribute;
		}
	}
	return NULL;
}

static void _destroyReport(Report * report) {
	free(report->goals);
	free(report->topics);
	free(report->edges);
}

static int _findGoal(const Report * report, const Symbol * symbol) {
	for (int k = 0; k < report->goalCount; ++k) {
		if (report->goals[k].symbol == symbol) {
			return k;
		}
	}
	return -1;
}

static int _findTopic(const Report * report, const Symbol * symbol) {
	for (int k = 0; k < report->topicCount; ++k) {
		if (report->topics[k].symbol == symbol) {
			return k;
		}
	}
	return -1;
}

/** Whether a topic is another one, or is nested inside it. */
static bool _isWithin(const Symbol * topic, const Symbol * ancestor) {
	for (const Symbol * current = topic; current != NULL; current = current->parent) {
		if (current == ancestor) {
			return true;
		}
	}
	return false;
}

/**
 * Places the topics of the graph in columns: a topic goes to the right of
 * every topic that it requires (there are no cycles, the analysis rejects
 * them). Inside a column, the topics keep the order of their declarations.
 */
static void _layOut(Report * report, int * columns, int * rows) {
	bool hasMoved = true;
	while (hasMoved) {
		hasMoved = false;
		for (int k = 0; k < report->edgeCount; ++k) {
			const Edge * edge = &report->edges[k];
			if (report->topics[edge->to].column <= report->topics[edge->from].column) {
				report->topics[edge->to].column = report->topics[edge->from].column + 1;
				hasMoved = true;
			}
		}
	}
	*columns = 0;
	*rows = 0;
	for (int k = 0; k < report->topicCount; ++k) {
		if (*columns <= report->topics[k].column) {
			*columns = report->topics[k].column + 1;
		}
	}
	int * counts = calloc(*columns, sizeof(int));
	for (int k = 0; k < report->topicCount; ++k) {
		report->topics[k].row = counts[report->topics[k].column]++;
		if (*rows < counts[report->topics[k].column]) {
			*rows = counts[report->topics[k].column];
		}
	}
	free(counts);
}

static const MemberList * _membersOf(const Symbol * symbol) {
	return (symbol->kind == GOAL_SYMBOL) ? symbol->goal->members : symbol->topic->members;
}

/**
 * Reads the topics that each topic requires: the ones of its own "requires",
 * and the ones of the topics that contain it. Requiring a topic with
 * subtopics means requiring every one of them.
 */
static void _readEdges(Report * report, const SymbolTable * table) {
	for (int k = 0; k < report->topicCount; ++k) {
		for (const Symbol * symbol = report->topics[k].symbol; symbol != NULL; symbol = symbol->parent) {
			const Attribute * requires = _attributeOf(symbol->topic->members, "requires");
			if (requires == NULL) {
				continue;
			}
			for (const ExpressionList * value = requires->values; value != NULL; value = value->next) {
				const Symbol * required = findSymbol(table, TOPIC_SYMBOL, value->expression->factor->variable);
				for (int j = 0; j < report->topicCount; ++j) {
					if (j != k && _isWithin(report->topics[j].symbol, required)) {
						_addEdge(report, j, k);
					}
				}
			}
		}
	}
}

/**
 * Collects the goals and the topics that the schedule has, in the order of
 * their declarations, with their sessions, their study time and their days.
 */
static Report _summarize(const Schedule * schedule, const SymbolTable * table) {
	Report report = { 0 };
	int capacity = 0;
	for (const Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (symbol->kind == GOAL_SYMBOL || symbol->kind == TOPIC_SYMBOL) {
			++capacity;
		}
	}
	report.goals = calloc((capacity == 0) ? 1 : capacity, sizeof(GoalSummary));
	report.topics = calloc((capacity == 0) ? 1 : capacity, sizeof(TopicSummary));
	for (const Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (symbol->kind == GOAL_SYMBOL) {
			report.goals[report.goalCount++] = (GoalSummary) { .symbol = symbol };
		}
	}
	for (const Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (symbol->kind != TOPIC_SYMBOL) {
			continue;
		}
		TopicSummary summary = { .symbol = symbol, .goal = _findGoal(&report, symbol->owner) };
		for (int e = 0; e < schedule->count; ++e) {
			const Entry * entry = &schedule->entries[e];
			if (entry->topic != symbol || !entry->isSession) {
				continue;
			}
			if (summary.sessions++ == 0) {
				summary.firstDay = entry->day;
			}
			summary.lastDay = entry->day;
			summary.study += entry->duration;
			if (summary.method == NULL && entry->method != NULL) {
				summary.method = entry->method;
			}
		}
		if (0 < summary.sessions) {
			report.goals[summary.goal].sessions += summary.sessions;
			report.goals[summary.goal].study += summary.study;
			report.topics[report.topicCount++] = summary;
		}
	}
	_readEdges(&report, table);
	return report;
}

/**
 * Writes the agenda: the days with sessions, grouped by week.
 */
static void _writeAgenda(FILE * file, const Schedule * schedule, const Report * report) {
	fprintf(file, "<section>\n<h2>Agenda</h2>\n");
	int week = -1;
	for (int k = 0; k < schedule->count;) {
		const int day = schedule->entries[k].day;
		const int monday = day - weekdayOf(day);
		if (monday != week) {
			fprintf(file, "%s<div class=\"week\">\n<h3>Semana del ", (week < 0) ? "" : "</div>\n");
			_writeDate(file, monday, false);
			fprintf(file, "</h3>\n");
			week = monday;
		}
		int study = 0;
		int end = k;
		while (end < schedule->count && schedule->entries[end].day == day) {
			if (schedule->entries[end].isSession) {
				study += schedule->entries[end].duration;
			}
			++end;
		}
		fprintf(file, "<div class=\"day\">\n<div class=\"date\"><strong>");
		_writeDate(file, day, false);
		fprintf(file, "</strong><span class=\"muted small\">");
		_writeDuration(file, study);
		fprintf(file, " de estudio</span></div>\n<ul class=\"blocks\">\n");
		while (k < end) {
			const int last = lastOfBlock(schedule, k);
			_writeBlock(file, schedule, report, k, last);
			k = last + 1;
		}
		fprintf(file, "</ul>\n</div>\n");
	}
	if (0 <= week) {
		fprintf(file, "</div>\n");
	}
	fprintf(file, "</section>\n");
}

/**
 * Writes a block of the agenda: its time, its topic, its method, and a bar
 * with its sessions and pauses.
 */
static void _writeBlock(FILE * file, const Schedule * schedule, const Report * report, const int first, const int last) {
	const Entry * entry = &schedule->entries[first];
	const int topic = _findTopic(report, entry->topic);
	int sessions = 0;
	int study = 0;
	int end = entry->start;
	for (int k = first; k <= last; ++k) {
		if (schedule->entries[k].isSession) {
			++sessions;
			study += schedule->entries[k].duration;
			end = schedule->entries[k].start + schedule->entries[k].duration;
		}
	}
	fprintf(file, "<li class=\"block c%d\"><time>", report->topics[topic].goal % GOAL_COLORS);
	_writeTime(file, entry->start);
	fprintf(file, "–");
	_writeTime(file, end);
	fprintf(file, "</time><strong>");
	_writeText(file, entry->topic->name);
	fprintf(file, "</strong> <span class=\"muted\">· ");
	_writeText(file, entry->topic->owner->name);
	fprintf(file, " · ");
	if (entry->isFixed) {
		fprintf(file, "sesión fija");
	}
	else if (entry->method == NULL) {
		fprintf(file, "bloque de estudio");
	}
	else {
		_writeText(file, entry->method);
		if (1 < sessions) {
			fprintf(file, ", %d sesiones, ", sessions);
			_writeDuration(file, study);
			fprintf(file, " de estudio");
		}
	}
	fprintf(file, "</span>\n");
	if (first < last) {
		fprintf(file, "<div class=\"steps\" style=\"width:%dpx\">", 2 * (end - entry->start));
		for (int k = first; k <= last && schedule->entries[k].start < end; ++k) {
			fprintf(file, "<span%s style=\"flex:%d\"></span>", schedule->entries[k].isSession ? "" : " class=\"pause\"",
				schedule->entries[k].duration);
		}
		fprintf(file, "</div>\n");
	}
	fprintf(file, "</li>\n");
}

/** Writes a day as "martes 1 de septiembre", and its year if it's asked. */
static void _writeDate(FILE * file, const int day, const bool hasYear) {
	const int date = toDate(day);
	fprintf(file, "%s %d de %s", weekdayName(weekdayOf(day)), date % 100, monthName((date / 100) % 100));
	if (hasYear) {
		fprintf(file, " de %d", date / 10000);
	}
}

static void _writeDuration(FILE * file, const int minutes) {
	char text[16];
	formatDuration(minutes, text);
	fprintf(file, "%s", text);
}

/**
 * Writes a card for every goal: its due date, its priority, its description,
 * and a table with the study time of each of its topics.
 */
static void _writeGoals(FILE * file, const Report * report) {
	fprintf(file, "<section>\n<h2>Objetivos</h2>\n");
	int longest = 1;
	for (int k = 0; k < report->topicCount; ++k) {
		if (longest < report->topics[k].study) {
			longest = report->topics[k].study;
		}
	}
	for (int g = 0; g < report->goalCount; ++g) {
		const GoalSummary * goal = &report->goals[g];
		if (goal->sessions == 0) {
			continue;
		}
		const MemberList * members = _membersOf(goal->symbol);
		const Attribute * due = _attributeOf(members, "due");
		const Attribute * priority = _attributeOf(members, "priority");
		const Attribute * description = _attributeOf(members, "description");
		fprintf(file, "<article class=\"card c%d\">\n<h3>", g % GOAL_COLORS);
		_writeText(file, goal->symbol->name);
		fprintf(file, "</h3>\n<p class=\"muted small\">");
		_writeDuration(file, goal->study);
		fprintf(file, " de estudio en %d sesiones", goal->sessions);
		if (due != NULL) {
			fprintf(file, " · vence el ");
			_writeDate(file, toDayNumber(due->values->expression->factor->constant->date), true);
		}
		if (priority != NULL) {
			fprintf(file, " · prioridad ");
			_writeText(file, priority->values->expression->factor->variable);
		}
		fprintf(file, "</p>\n");
		if (description != NULL) {
			fprintf(file, "<p>");
			_writeText(file, description->values->expression->factor->constant->string);
			fprintf(file, "</p>\n");
		}
		fprintf(file, "<div class=\"table\"><table>\n<thead><tr><th>Tema</th><th>Método</th><th>Sesiones</th><th>Estudio</th><th>Días</th></tr></thead>\n<tbody>\n");
		for (int k = 0; k < report->topicCount; ++k) {
			const TopicSummary * topic = &report->topics[k];
			if (topic->goal != g) {
				continue;
			}
			fprintf(file, "<tr><td>");
			if (topic->symbol->parent != NULL) {
				fprintf(file, "<span class=\"muted\">");
				_writeText(file, topic->symbol->parent->name);
				fprintf(file, " › </span>");
			}
			fprintf(file, "<strong>");
			_writeText(file, topic->symbol->name);
			fprintf(file, "</strong>");
			const Attribute * text = _attributeOf(topic->symbol->topic->members, "description");
			if (text != NULL) {
				fprintf(file, "<p class=\"muted small\">");
				_writeText(file, text->values->expression->factor->constant->string);
				fprintf(file, "</p>");
			}
			fprintf(file, "</td><td>");
			_writeText(file, (topic->method == NULL) ? "—" : topic->method);
			fprintf(file, "</td><td class=\"number\">%d</td><td class=\"number\">", topic->sessions);
			_writeDuration(file, topic->study);
			fprintf(file, "<div class=\"bar\" style=\"width:%d%%\"></div></td><td class=\"number\">", (100 * topic->study) / longest);
			const int firstDate = toDate(topic->firstDay);
			const int lastDate = toDate(topic->lastDay);
			fprintf(file, "%d/%d", firstDate % 100, (firstDate / 100) % 100);
			if (topic->firstDay != topic->lastDay) {
				fprintf(file, " al %d/%d", lastDate % 100, (lastDate / 100) % 100);
			}
			fprintf(file, "</td></tr>\n");
		}
		fprintf(file, "</tbody>\n</table></div>\n</article>\n");
	}
	fprintf(file, "</section>\n");
}

/**
 * Writes the graph of dependencies as an SVG: every topic is a box in the
 * color of its goal, and an arrow goes from a topic to each topic that
 * requires it.
 */
static void _writeGraph(FILE * file, Report * report) {
	fprintf(file, "<section>\n<h2>Dependencias</h2>\n");
	if (report->edgeCount == 0) {
		fprintf(file, "<p class=\"muted\">Ningún tema requiere a otro, así que se estudian en el orden de sus fechas límite.</p>\n</section>\n");
		return;
	}
	int columns = 0;
	int rows = 0;
	_layOut(report, &columns, &rows);
	int longestName = 0;
	for (int k = 0; k < report->topicCount; ++k) {
		const int length = (int) strlen(report->topics[k].symbol->name);
		if (longestName < length) {
			longestName = length;
		}
	}
	const int nodeWidth = (longestName * 8 + 28 < 100) ? 100 : (longestName * 8 + 28);
	const int width = 2 * GRAPH_MARGIN + columns * nodeWidth + (columns - 1) * COLUMN_GAP;
	const int height = 2 * GRAPH_MARGIN + rows * NODE_HEIGHT + (rows - 1) * ROW_GAP;
	fprintf(file, "<p class=\"muted small\">Cada flecha va de un tema al que lo requiere: el segundo empieza cuando termina el primero.</p>\n");
	fprintf(file, "<div class=\"graph\"><svg width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\" role=\"img\" aria-label=\"Grafo de dependencias entre los temas\">\n",
		width, height, width, height);
	fprintf(file, "<defs><marker id=\"arrow\" viewBox=\"0 0 10 10\" refX=\"9\" refY=\"5\" markerWidth=\"7\" markerHeight=\"7\" orient=\"auto\">"
		"<path class=\"head\" d=\"M0,0 L10,5 L0,10 z\"/></marker></defs>\n");
	for (int k = 0; k < report->edgeCount; ++k) {
		const TopicSummary * from = &report->topics[report->edges[k].from];
		const TopicSummary * to = &report->topics[report->edges[k].to];
		const int x1 = GRAPH_MARGIN + from->column * (nodeWidth + COLUMN_GAP) + nodeWidth;
		const int y1 = GRAPH_MARGIN + from->row * (NODE_HEIGHT + ROW_GAP) + NODE_HEIGHT / 2;
		const int x2 = GRAPH_MARGIN + to->column * (nodeWidth + COLUMN_GAP) - 2;
		const int y2 = GRAPH_MARGIN + to->row * (NODE_HEIGHT + ROW_GAP) + NODE_HEIGHT / 2;
		fprintf(file, "<path class=\"edge\" d=\"M%d,%d C%d,%d %d,%d %d,%d\" marker-end=\"url(#arrow)\"/>\n",
			x1, y1, x1 + COLUMN_GAP / 2, y1, x2 - COLUMN_GAP / 2, y2, x2, y2);
	}
	for (int k = 0; k < report->topicCount; ++k) {
		const TopicSummary * topic = &report->topics[k];
		const int x = GRAPH_MARGIN + topic->column * (nodeWidth + COLUMN_GAP);
		const int y = GRAPH_MARGIN + topic->row * (NODE_HEIGHT + ROW_GAP);
		fprintf(file, "<g class=\"c%d\"><title>", topic->goal % GOAL_COLORS);
		_writeText(file, topic->symbol->owner->name);
		fprintf(file, ": ");
		_writeDuration(file, topic->study);
		fprintf(file, " de estudio</title><rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"8\"/>", x, y, nodeWidth, NODE_HEIGHT);
		fprintf(file, "<text x=\"%d\" y=\"%d\" text-anchor=\"middle\" dominant-baseline=\"central\">", x + nodeWidth / 2, y + NODE_HEIGHT / 2);
		_writeText(file, topic->symbol->name);
		fprintf(file, "</text></g>\n");
	}
	fprintf(file, "</svg></div>\n</section>\n");
}

/** Writes the title of the page, the days of the plan, and its totals. */
static void _writeHeader(FILE * file, const Schedule * schedule, const Report * report) {
	int sessions = 0;
	int study = 0;
	int goals = 0;
	for (int g = 0; g < report->goalCount; ++g) {
		sessions += report->goals[g].sessions;
		study += report->goals[g].study;
		goals += (0 < report->goals[g].sessions) ? 1 : 0;
	}
	fprintf(file, "<header>\n<h1>Plan de estudio</h1>\n<p class=\"muted\">");
	if (schedule->count == 0) {
		fprintf(file, "La agenda no tiene sesiones de estudio.</p>\n</header>\n");
		return;
	}
	fprintf(file, "Del ");
	_writeDate(file, schedule->entries[0].day, false);
	fprintf(file, " al ");
	_writeDate(file, schedule->entries[schedule->count - 1].day, true);
	fprintf(file, "</p>\n<div class=\"stats\">\n<div><strong>");
	_writeDuration(file, study);
	fprintf(file, "</strong><span class=\"muted\">de estudio</span></div>\n");
	fprintf(file, "<div><strong>%d</strong><span class=\"muted\">%s</span></div>\n", sessions, (sessions == 1) ? "sesión" : "sesiones");
	fprintf(file, "<div><strong>%d</strong><span class=\"muted\">%s</span></div>\n", report->topicCount, (report->topicCount == 1) ? "tema" : "temas");
	fprintf(file, "<div><strong>%d</strong><span class=\"muted\">%s</span></div>\n", goals, (goals == 1) ? "objetivo" : "objetivos");
	fprintf(file, "</div>\n</header>\n");
}

/** Writes a text, escaping the characters that HTML reserves. */
static void _writeText(FILE * file, const char * text) {
	for (const char * character = text; *character != '\0'; ++character) {
		switch (*character) {
			case '&': fputs("&amp;", file); break;
			case '<': fputs("&lt;", file); break;
			case '>': fputs("&gt;", file); break;
			case '"': fputs("&quot;", file); break;
			default: fputc(*character, file);
		}
	}
}

static void _writeTime(FILE * file, const int minutes) {
	char text[6];
	formatTime(minutes, text);
	fprintf(file, "%s", text);
}

/* PUBLIC FUNCTIONS */

bool generateHtml(const Schedule * schedule, const SymbolTable * table, const char * path) {
	FILE * file = fopen(path, "wb");
	if (file == NULL) {
		return false;
	}
	Report report = _summarize(schedule, table);
	fprintf(file, "<!doctype html>\n<html lang=\"es\">\n<head>\n<meta charset=\"utf-8\">\n"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
		"<meta name=\"color-scheme\" content=\"light dark\">\n<title>Plan de estudio</title>\n<style>\n");
	fputs(_STYLE, file);
	fprintf(file, "</style>\n</head>\n<body>\n");
	_writeHeader(file, schedule, &report);
	if (0 < schedule->count) {
		fprintf(file, "<main>\n");
		_writeGoals(file, &report);
		_writeGraph(file, &report);
		_writeAgenda(file, schedule, &report);
		fprintf(file, "</main>\n");
	}
	fprintf(file, "<footer>Generado por el compilador de Stujfy. El calendario está en plan.ics, para importarlo en Google Calendar, Apple o Outlook.</footer>\n");
	fprintf(file, "</body>\n</html>\n");
	_destroyReport(&report);
	return fclose(file) == 0;
}
