#include "backend/code-generation/Generator.h"
#include "backend/planning/Planner.h"
#include "backend/semantic-analysis/SemanticAnalysis.h"
#include "frontend/Frontend.h"
#include "frontend/lexical-analysis/FlexActions.h"
#include "frontend/syntactic-analysis/BisonActions.h"
#include "support/logging/Logger.h"
#include "support/type/CompilationStatus.h"
#include "support/type/CompilerState.h"
#include "support/type/ModuleDestructor.h"

/**
 * The main entry-point of the entire application. If you use "strtok" to
 * parse anything inside this project instead of using Flex and Bison, I will
 * find you, and I will kill you (Bryan Mills; "Taken", 2008).
**/
const int main(const int length, const char ** arguments) {
	LexicalAnalyzer * lexicalAnalyzer = createLexicalAnalyzer();
	Logger * logger = createLogger("EntryPoint");
	for (int k = 0; k < length; ++k) {
		logDebugging(logger, "Argument %d: \"%s\"", k, arguments[k]);
	}
	CompilerState compilerState = {
		.abstractSyntaxtTree = NULL
	};
	ModuleDestructor moduleDestructors[] = {
		initializeAbstractSyntaxTreeModule(),
		initializeFlexActionsModule(lexicalAnalyzer),
		initializeBisonActionsModule(&compilerState),
		initializeFrontendModule(lexicalAnalyzer),
		initializeSemanticAnalysisModule(),
		initializePlannerModule(),
		initializeGeneratorModule()
	};
	CompilationStatus compilationStatus = executeSyntacticAnalysis();
	Program * program = compilerState.abstractSyntaxtTree;
	if (compilationStatus == SUCCEEDED) {
		logInformation(logger, "The syntactic-analysis phase accepts the input program.");
		SymbolTable * symbolTable = NULL;
		compilationStatus = executeSemanticAnalysis(program, &symbolTable);
		if (compilationStatus == SUCCEEDED) {
			logInformation(logger, "The semantic-analysis phase accepts the input program.");
			Schedule * schedule = NULL;
			compilationStatus = executePlanning(program, symbolTable, &schedule);
			if (compilationStatus == SUCCEEDED) {
				logInformation(logger, "The planning phase schedules the input program.");
				if (schedule != NULL) {
					compilationStatus = executeGenerator(schedule);
					if (compilationStatus == SUCCEEDED) {
						logInformation(logger, "The code-generation phase writes the output files.");
					}
					else {
						logError(logger, "The code-generation phase can't write the output files.");
					}
				}
			}
			else {
				logError(logger, "The planning phase can't schedule the input program.");
			}
			destroySchedule(schedule);
		}
		else {
			logError(logger, "The semantic-analysis phase rejects the input program.");
		}
		destroySymbolTable(symbolTable);
	}
	else {
		logError(logger, "The syntactic-analysis phase rejects the input program.");
		compilationStatus = FAILED;
	}
	logDebugging(logger, "Releasing AST resources...");
	destroyProgram(program);
	for (int k = (sizeof(moduleDestructors)/sizeof(ModuleDestructor)) - 1; 0 <= k; --k) {
		moduleDestructors[k]();
	}
	logDebugging(logger, "Compilation is done.");
	destroyLogger(logger);
	destroyLexicalAnalyzer(lexicalAnalyzer);
	return compilationStatus;
}