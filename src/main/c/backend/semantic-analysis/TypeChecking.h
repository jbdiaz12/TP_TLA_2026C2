#ifndef TYPE_CHECKING_HEADER
#define TYPE_CHECKING_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "SemanticError.h"
#include "SymbolTable.h"
#include <stdbool.h>
#include <string.h>

/**
 * The types of the values of the language. An expression with an error has
 * the type ERROR_TYPE: its error is already reported, so whatever contains it
 * doesn't report another one.
 */
typedef enum {
	ERROR_TYPE,
	BOOLEAN_TYPE,
	DATE_TYPE,
	DURATION_TYPE,
	INTEGER_TYPE,
	LEVEL_TYPE,
	LIST_TYPE,
	STRING_TYPE,
	TIME_TYPE
} TypeKind;

typedef struct {
	TypeKind kind;
	/** The scale of a level, because only the levels of the same scale are comparable. */
	const Symbol * scale;
} Type;

/** A name that an expression can read, such as the parameter of a method. */
typedef struct {
	const char * name;
	Type type;
} Variable;

/**
 * What an expression can use, which depends on where it's written. Only the
 * constraints can call the functions of the domain (e.g., "load(week)").
 */
typedef struct {
	const SymbolTable * table;
	const Variable * variables;
	int variableCount;
	bool allowsDomainFunctions;
} Scope;

/**
 * Describes a type for the messages, with its article (e.g., "una duración").
 */
const char * describeType(const Type type);

/**
 * Builds a type that isn't a level.
 */
Type simpleType(const TypeKind kind);

/**
 * Computes the type of an expression, and reports every operation between
 * incompatible types that it contains.
 */
Type typeOf(const Expression * expression, const Scope * scope);

#endif
