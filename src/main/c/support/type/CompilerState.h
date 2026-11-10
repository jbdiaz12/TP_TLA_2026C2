#ifndef COMPILER_STATE_HEADER
#define COMPILER_STATE_HEADER

/**
 * The global state of the compiler. Should transport every data structure
 * needed across the different phases of a compilation.
 */
typedef struct {
	/**
	 * The root node of the AST.
	 */
	void * abstractSyntaxtTree;
} CompilerState;

#endif
