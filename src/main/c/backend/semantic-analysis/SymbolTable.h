#ifndef SYMBOL_TABLE_HEADER
#define SYMBOL_TABLE_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * The kinds of symbols that a program declares. Goals and topics share their
 * names (a topic can't be named as a goal), while scales and levels have
 * names of their own.
 */
typedef enum {
	GOAL_SYMBOL,
	LEVEL_SYMBOL,
	SCALE_SYMBOL,
	TOPIC_SYMBOL
} SymbolKind;

typedef struct Symbol Symbol;

/**
 * A declared name and the node of the AST that declares it. The names belong
 * to the AST, so the symbol table must be destroyed before the AST.
 */
struct Symbol {
	SymbolKind kind;
	const char * name;
	Position position;
	union {
		const Goal * goal;
		const Scale * scale;
		const Topic * topic;
	};
	/**
	 * The symbol that contains this one: the goal of a topic (NULL while
	 * it's unknown), or the scale of a level.
	 */
	Symbol * owner;
	/** The topic that contains a nested topic, or NULL. */
	Symbol * parent;
	/** The order of a level inside its scale, starting at zero (the lowest). */
	int rank;
	Symbol * next;
};

/** The symbols in the order they're declared. */
typedef struct {
	Symbol * first;
	Symbol * last;
} SymbolTable;

/**
 * Adds a new symbol at the end of the table, and returns it so the caller
 * can complete its node, owner and rank.
 */
Symbol * addSymbol(SymbolTable * table, const SymbolKind kind, const char * name, const Position position);

/** Creates an empty symbol table. */
SymbolTable * createSymbolTable();

/** Destroys a symbol table, but not the AST that its symbols point to. */
void destroySymbolTable(SymbolTable * table);

/**
 * Finds the first symbol with the specified name and kind, or NULL. A goal
 * and a topic share their names, so looking for one of them also finds the
 * other: the caller checks the kind of the result.
 */
Symbol * findSymbol(const SymbolTable * table, const SymbolKind kind, const char * name);

#endif
