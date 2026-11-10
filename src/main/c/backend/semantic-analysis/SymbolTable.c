#include "SymbolTable.h"

/* PRIVATE FUNCTIONS */

static bool _shareNames(const SymbolKind kind, const SymbolKind other);

/**
 * Whether two kinds of symbols share the same names.
 */
static bool _shareNames(const SymbolKind kind, const SymbolKind other) {
	const bool isStudyUnit = (kind == GOAL_SYMBOL || kind == TOPIC_SYMBOL);
	const bool isOtherStudyUnit = (other == GOAL_SYMBOL || other == TOPIC_SYMBOL);
	return (isStudyUnit && isOtherStudyUnit) || kind == other;
}

/* PUBLIC FUNCTIONS */

Symbol * addSymbol(SymbolTable * table, const SymbolKind kind, const char * name, const Position position) {
	Symbol * symbol = calloc(1, sizeof(Symbol));
	symbol->kind = kind;
	symbol->name = name;
	symbol->position = position;
	if (table->last == NULL) {
		table->first = symbol;
	}
	else {
		table->last->next = symbol;
	}
	table->last = symbol;
	return symbol;
}

SymbolTable * createSymbolTable() {
	return calloc(1, sizeof(SymbolTable));
}

void destroySymbolTable(SymbolTable * table) {
	if (table != NULL) {
		Symbol * symbol = table->first;
		while (symbol != NULL) {
			Symbol * next = symbol->next;
			free(symbol);
			symbol = next;
		}
		free(table);
	}
}

Symbol * findSymbol(const SymbolTable * table, const SymbolKind kind, const char * name) {
	for (Symbol * symbol = table->first; symbol != NULL; symbol = symbol->next) {
		if (_shareNames(kind, symbol->kind) && strcmp(symbol->name, name) == 0) {
			return symbol;
		}
	}
	return NULL;
}
