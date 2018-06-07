// fblcs.h --
//   This header file describes the externally visible interface to the
//   symbol and source level fblc facilities.

#ifndef FBLCS_H_
#define FBLCS_H_

#include <stdio.h>    // for FILE

#include "fblc.h"

// FblcsNamesEqual --
//   Test if two names are equal.
//
// Inputs:
//   a - The first name.
//   b - The second name.
//
// Results:
//   true if the names are the same, false otherwise.
//
// Side effects:
//   None.
bool FblcsNamesEqual(const char* a, const char* b);

// FblcsLoc --
//   Represents a location in a source file.
//
// Fields:
//   source - The name of the source file or other description of the source
//            of the program text.
//   line - The line within the file for the location.
//   col - The column within the line for the location.
typedef struct {
  const char* source;
  int line;
  int col;
} FblcsLoc;

// FblcsReportError --
//   Report an error message associated with a location in a source file.
//
// Inputs:
//   format - A printf format string for the error message.
//   loc - The location of the error message to report.
//   ... - printf arguments as specified by the format string.
//
// Results:
//   None.
//
// Side effects:
//   Prints an error message to stderr with error location.
void FblcsReportError(const char* format, FblcsLoc* loc, ...);

// FblcsName -- 
//   A name along with its associated location in a source file. The location
//   is typically used for error reporting purposes.
typedef struct {
  const char* name;
  FblcsLoc* loc;
} FblcsName;

// FblcsName --
//   A vector of FblcsNames.
typedef struct {
  size_t size;
  FblcsName* xs;
} FblcsNameV;

// FblcsArg --
//   An fblcs name with associated type. Used for declaring fields of
//   types and arguments to functions and processes.
typedef struct {
  FblcsName type;
  FblcsName name;
} FblcsArg;

// FblcsArgV --
//   A vector of fblcs args
typedef struct {
  size_t size;
  FblcsArg* xs;
} FblcsArgV;

// FblcsId --
//   A reference to a variable, field, or port.
//
// Fields:
//   name - The name of the field.
//   id - The fblc id of the field. This is set to FBLC_NULL_ID by the parser,
//        then later filled in during type check and used by the compiler.
typedef struct {
  FblcsName name;
  size_t id;
} FblcsId;

// FblcsIdV --
//   A vector of FblcsIds.
typedef struct {
  size_t size;
  FblcsId* xs;
} FblcsIdV;


// FblcsExprTag --
//   A tag used to distinguish among different kinds of expressions.
typedef enum {
  FBLCS_VAR_EXPR,
  FBLCS_APP_EXPR,
  FBLCS_UNION_EXPR,
  FBLCS_ACCESS_EXPR,
  FBLCS_COND_EXPR,
  FBLCS_LET_EXPR
} FblcsExprTag;

// FblcsExpr --
//   A tagged union of expression types. All expressions have the same initial
//   layout as FblcExpr. The tag can be used to determine what kind of
//   expression this is to get access to additional fields of the expression
//   by first casting to that specific type of expression.
typedef struct {
  FblcsExprTag tag;
  FblcsLoc* loc;
} FblcsExpr;

// FblcExprV --
//   A vector of fblc expressions.
typedef struct {
  size_t size;
  FblcsExpr** xs;
} FblcsExprV;

// FblcsVarExpr --
//   A variable expression of the form 'var' whose value is the value of the
//   corresponding variable in scope.
typedef struct {
  FblcsExpr _base;
  FblcsId var;
} FblcsVarExpr;

// FblcsAppExpr --
//   An application or struct expression of the form 'func(arg0, arg1, ...)'.
typedef struct {
  FblcsExpr _base;
  FblcsName func;
  FblcsExprV argv;
} FblcsAppExpr;

// FblcsUnionExpr --
//   A union expression of the form 'type:field(arg)', used to construct a
//   union value.
typedef struct {
  FblcsExpr _base;
  FblcsName type;
  FblcsId field;
  FblcsExpr* arg;
} FblcsUnionExpr;

// FblcsAccessExpr --
//   An access expression of the form 'obj.field' used to access a field of
//   a struct or union value.
typedef struct {
  FblcsExpr _base;
  FblcsExpr* obj;
  FblcsId field;
} FblcsAccessExpr;

// FblcsCondExpr --
//   A conditional expression of the form
//      '?(select; tag0: arg0, tag1: arg1, ...)'
//   which conditionally selects an argument based on the tag of the select
//   value.
typedef struct {
  FblcsExpr _base;
  FblcsExpr* select;
  FblcsExprV argv;
  FblcsNameV tagv;
} FblcsCondExpr;

// FblcsLetExpr --
//   A let expression of the form '{ type var = def; body }', where the name
//   of the variable is a De Bruijn index based on the context where the
//   variable is accessed.
typedef struct {
  FblcsExpr _base;
  FblcsName type;
  FblcsName name;
  FblcsExpr* def;
  FblcsExpr* body;
} FblcsLetExpr;

// FblcsActnTag --
//   A tag used to distinguish among different kinds of actions.
typedef enum {
  FBLCS_EVAL_ACTN,
  FBLCS_GET_ACTN,
  FBLCS_PUT_ACTN,
  FBLCS_COND_ACTN,
  FBLCS_CALL_ACTN,
  FBLCS_LINK_ACTN,
  FBLCS_EXEC_ACTN
} FblcsActnTag;

// FblcsActn --
//   A tagged union of action types. All actions have the same initial
//   layout as FblcsActn. The tag can be used to determine what kind of
//   action this is to get access to additional fields of the action
//   by first casting to that specific type of action.
typedef struct {
  FblcsActnTag tag;
  FblcsLoc* loc;
} FblcsActn;

// FblcsActnV --
//   A vector of fblcs actions.
typedef struct {
  size_t size;
  FblcsActn** xs;
} FblcsActnV;

// FblcsEvalActn --
//   An evaluation action of the form '$(arg)' which evaluates the given
//   expression without side effects.
typedef struct {
  FblcsActn _base;
  FblcsExpr* arg;
} FblcsEvalActn;

// FblcsGetActn --
//   A get action of the form '~port()' used to get a value from a port.
typedef struct {
  FblcsActn _base;
  FblcsId port;
} FblcsGetActn;

// FblcsPutActn --
//   A put action of the form '~port(arg)' used to put a value onto a port.
typedef struct {
  FblcsActn _base;
  FblcsId port;
  FblcsExpr* arg;
} FblcsPutActn;

// FblcsCondActn --
//   A conditional action of the form
//    '?(select; tag0: arg0, tag1: arg1, ...)'
//   which conditionally selects an argument based on the tag of the select
//   value.
typedef struct {
  FblcsActn _base;
  FblcsExpr* select;
  FblcsActnV argv;
  FblcsNameV tagv;
} FblcsCondActn;

// FblcsCallActn --
//   A call action of the form 'proc(port0, port1, ... ; arg0, arg1, ...)',
//   which calls a process with the given port and value arguments.
typedef struct {
  FblcsActn _base;
  FblcsName proc;
  FblcsIdV portv;
  FblcsExprV argv;
} FblcsCallActn;

// FblcsLinkActn --
//   A link action of the form 'type <~> get, put; body'. The names of the get
//   and put ports are De Bruijn indices based on the context where the ports
//   are accessed.
typedef struct {
  FblcsActn _base;
  FblcsName type;
  FblcsName get;
  FblcsName put;
  FblcsActn* body;
} FblcsLinkActn;

// FblcsExec --
//   Trip of type, name, and action used in the FblcsExecActn.
typedef struct {
  FblcsName type;
  FblcsName name;
  FblcsActn* actn;
} FblcsExec;

// FblcsExecV --
//   Vector of FblcsExec.
typedef struct {
  size_t size;
  FblcsExec* xs;
} FblcsExecV;

// FblcsExecActn --
//   An exec action of the form 'type0 var0 = exec0, type1 var1 = exec1, ...; body',
//   which executes processes in parallel.
typedef struct {
  FblcsActn _base;
  FblcsExecV execv;
  FblcsActn* body;
} FblcsExecActn;

// FblcsKind --
//   An enum used to distinguish between struct and union types or values.
typedef enum {
  FBLCS_STRUCT_KIND,
  FBLCS_UNION_KIND
} FblcsKind;

// FblcsType --
//   A type declaration of the form 'name(field0 name0, field1 name1, ...)'.
//   This is a common structure used for both struct and union declarations.
typedef struct {
  FblcsKind kind;
  FblcsName name;
  FblcsArgV fieldv;
} FblcsType;

// FblcsTypeV --
//   A vector of FblcsType.
typedef struct {
  size_t size;
  FblcsType** xs;
} FblcsTypeV;

// FblcsFunc --
//   Declaration of a function of the form:
//     'name(arg0 name0, arg1 name1, ...; return_type) body'
typedef struct {
  FblcsName name;
  FblcsArgV argv;
  FblcsName return_type;
  FblcsExpr* body;
} FblcsFunc;

// FblcsFuncV --
//   A vector of FblcsFunc.
typedef struct {
  size_t size;
  FblcsFunc** xs;
} FblcsFuncV;

// FblcsPolarity --
//   The polarity of a port.
typedef enum {
  FBLCS_GET_POLARITY,
  FBLCS_PUT_POLARITY
} FblcsPolarity;

// FblcsPort --
//   The type, name, and polarity of a port.
typedef struct {
  FblcsName type;
  FblcsName name;
  FblcsPolarity polarity;
} FblcsPort;

// FblcsPortV --
//   A vector of fblcs ports.
typedef struct {
  size_t size;
  FblcsPort* xs;
} FblcsPortV;

// FblcsProc --
//   Declaration of a process of the form:
//     'name(p0type p0polarity p0name, p1type p1polarity p1name, ... ;
//           arg0 name0, arg1, name1, ... ; return_type) body'
typedef struct {
  FblcsName name;
  FblcsPortV portv;
  FblcsArgV argv;
  FblcsName return_type;
  FblcsActn* body;
} FblcsProc;

// FblcsProcV --
//   A vector of FblcsProc.
typedef struct {
  size_t size;
  FblcsProc** xs;
} FblcsProcV;

// FblcsProgram --
//   A collection of declarations that make up a program.
typedef struct {
  FblcsTypeV typev;
  FblcsFuncV funcv;
  FblcsProcV procv;
} FblcsProgram;

// FblcsParseProgram --
//   Parse an fblc program from a file.
//
// Inputs:
//   arena - The arena to use for allocations.
//   filename - The name of the file to parse the program from.
//
// Result:
//   The parsed program environment, or NULL on error.
//   Name resolution is not performed; FblcsIds throughout the parsed program
//   will have their id field set to FBLC_NULL_ID in the returned result.
//
// Side effects:
//   A program environment is allocated. In the case of an error, an error
//   message is printed to standard error; the caller is still responsible for
//   freeing (unused) allocations made with the allocator in this case.
FblcsProgram* FblcsParseProgram(FblcArena* arena, const char* filename);

// FblcsParseValue --
//   Parse an fblc value from a text file.
//
// Inputs:
//   sprog - The program environment.
//   typename - The name of the type of value to parse.
//   fd - A file descriptor of a file open for reading.
//
// Result:
//   The parsed value, or NULL on error.
//
// Side effects:
//   The value is read from the given file descriptor. In the case of an
//   error, an error message is printed to stderr
FblcValue* FblcsParseValue(FblcArena* arena, FblcsProgram* sprog, FblcsName* typename, int fd);

// FblcsParseValueFromString --
//   Parse an fblc value from a string.
//
// Inputs:
//   sprog - The program environment.
//   typename - The name of the type of value to parse.
//   string - The string to parse the value from.
//
// Result:
//   The parsed value, or NULL on error.
//
// Side effects:
//   In the case of an error, an error message is printed to standard error.
FblcValue* FblcsParseValueFromString(FblcArena* arena, FblcsProgram* sprog, FblcsName* typename, const char* string);

// FblcsCheckProgram --
//   Check that the given program environment describes a well formed and well
//   typed program. Performs name resolution, updating FblcsId id fields
//   throughout the program.
//
// Inputs:
//   prog - The program environment to check.
//
// Results:
//   true if the program environment is well formed and well typed,
//   false otherwise.
//
// Side effects:
//   Update FblcsId id fields in the program to their resolved values.
//   If the program environment is not well formed, an error message is
//   printed to stderr describing the problem with the program environment.
bool FblcsCheckProgram(FblcsProgram* prog);

// FblcsLoaded --
//   The fblcs program, fblcs proc, and fblc procs corresponding to a compiled
//   entry point of a loaded program.
typedef struct {
  FblcsProgram* prog;
  FblcsProc* sproc;
  FblcProc* proc;
} FblcsLoaded;

// FblcsCompileProgram --
//   Compile an fblc program from an already checked fblcs program.
//
// Inputs:
//   arena - Arena to use for allocating the fblc program.
//   prog - The fblcs program environment.
//   entry - The main entry point of the program to compile. This must refer
//           to either a function or process. If it refers to a function, the
//           function will be wrapped as a process.
//
// Results:
//   The loaded program and entry points, or NULL if the entry could not be found.
//
// Side effects:
//   The behavior is undefined if the program environment is not well formed.
FblcsLoaded* FblcsCompileProgram(FblcArena* arena, FblcsProgram* prog, const char* entry);

// FblcsLoadProgram --
//   Load a text fblc program from the given file using the given arena for
//   allocations. Performs parsing, type check, and compilation of the
//   given entry point of the program.
//
// Inputs:
//   arena - Arena to use for allocations.
//   filename - Name of the file to load the program from.
//   entry - The name of the main entry point, which should be a function or
//           process in the program.
//
// Results:
//   The loaded program and entry points or NULL on error.
//
// Side effects:
//   Arena allocations are made in order to load the program. If the program
//   cannot be loaded, an error message is printed to stderr.
FblcsLoaded* FblcsLoadProgram(FblcArena* arena, const char* filename, const char* entry);

// FblcsLookupType --
//   Look up the type declaration with the given name in the given program.
//
// Inputs:
//   prog - The program environment.
//   name - The name of the type to look up.
//
// Results:
//   The type declaration in prog with the given name, or NULL if no such type
//   is found.
//
// Side effects:
//   None.
FblcsType* FblcsLookupType(FblcsProgram* prog, const char* name);

// FblcsLookupFunc --
//   Look up the func declaration with the given name in the given program.
//
// Inputs:
//   prog - The program environment.
//   name - The name of the func to look up.
//
// Results:
//   The func declaration in prog with the given name, or NULL if no such func
//   is found.
//
// Side effects:
//   None.
FblcsFunc* FblcsLookupFunc(FblcsProgram* prog, const char* name);

// FblcsLookupProc --
//   Look up the proc declaration with the given name in the given program.
//
// Inputs:
//   prog - The program environment.
//   name - The name of the proc to look up.
//
// Results:
//   The proc declaration in prog with the given name, or NULL if no such proc
//   is found.
//
// Side effects:
//   None.
FblcsProc* FblcsLookupProc(FblcsProgram* prog, const char* name);

// FblcsPrintValue --
//   Print a value in standard format to the given FILE stream.
//
// Inputs:
//   stream - The stream to print the value to.
//   sprog - The fblc program environment.
//   typename - The name of the type of value to print.
//   value - The value to print.
//
// Result:
//   None.
//
// Side effects:
//   The value is printed to the given file stream.
void FblcsPrintValue(FILE* stream, FblcsProgram* prog, FblcsName* typename, FblcValue* value);

#endif  // FBLCS_H_
