// fblcs.h --
//   This header file describes the externally visible interface to the
//   symbol and source level fblc facilities.

#ifndef FBLCS_H_
#define FBLCS_H_

#include <stdio.h>    // for FILE

#include "fblc.h"

// FblcsName --
//   Represents a name used in an fblcs source file, such as of a type, field,
//   variable, port, function, or process.
typedef const char* FblcsName;

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
bool FblcsNamesEqual(FblcsName a, FblcsName b);

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

// FblcsNameL -- 
//   A name along with its associated location in a source file. The location
//   is typically used for error reporting purposes.
typedef struct {
  FblcsName name;
  FblcsLoc* loc;
} FblcsNameL;

// FblcsTypedName --
//   A pair of type and name, each with location, commonly used for
//   representing symbol information in fblcs programs.
typedef struct {
  FblcsNameL type;
  FblcsNameL name;
} FblcsTypedName;

// FblcsExpr --
//   Common base type for the following fblcs expr types. It should be clear
//   from the context based on the tag of the corresponding FblcExpr which
//   specific type an instance of FblcsExpr refers to.
typedef struct {
  FblcsLoc* loc;
} FblcsExpr;

// FblcsVarExpr --
//   Symbol information associated with a variable expression.
typedef struct {
  FblcsExpr _base;
  FblcsNameL var;
} FblcsVarExpr;

// FblcsAppExpr --
//   Symbol information associated with an application expression.
typedef struct {
  FblcsExpr _base;
  FblcsNameL func;
} FblcsAppExpr;

// FblcsUnionExpr --
//   Symbol information associated with a union expression.
typedef struct {
  FblcsExpr _base;
  FblcsNameL type;
  FblcsNameL field;
} FblcsUnionExpr;

// FblcsAccessExpr --
//   Symbol information associated with an access expression.
typedef struct {
  FblcsExpr _base;
  FblcsNameL field;
} FblcsAccessExpr;

// FblcsCondExpr --
//   Symbol information associated with a conditional expression.
typedef struct {
  FblcsExpr _base;
} FblcsCondExpr;

// FblcsLetExpr --
//   Symbol information associated with a let expression.
typedef struct {
  FblcsExpr _base;
  FblcsNameL type;
  FblcsNameL name;
} FblcsLetExpr;

// FblcsActn --
//   Common base type for the following fblcs actn types. It should be clear
//   from the context based on the tag of the corresponding FblcActn which
//   specific type an instance of FblcsActn refers to.
typedef struct {
  FblcsLoc* loc;
} FblcsActn;

// FblcsEvalActn --
//   Symbol information associated with an eval action.
typedef struct {
  FblcsActn _base;
} FblcsEvalActn;

// FblcsGetActn --
//   Symbol information associated with a get action.
typedef struct {
  FblcsActn _base;
  FblcsNameL port;
} FblcsGetActn;

// FblcsPutActn --
//   Symbol information associated with a put action.
typedef struct {
  FblcsActn _base;
  FblcsNameL port;
} FblcsPutActn;

// FblcsCondActn --
//   Symbol information associated with a conditional action.
typedef struct {
  FblcsActn _base;
} FblcsCondActn;

// FblcsNameLV --
//   A vector of FblcsNameLs.
typedef struct {
  size_t size;
  FblcsNameL* xs;
} FblcsNameLV;

// FblcsCallActn --
//   Symbol information associated with a call action.
typedef struct {
  FblcsActn _base;
  FblcsNameL proc;
  FblcsNameLV portv;
} FblcsCallActn;

// FblcsLinkActn --
//   Symbol information associated with a link action.
typedef struct {
  FblcsActn _base;
  FblcsNameL type;
  FblcsNameL get;
  FblcsNameL put;
} FblcsLinkActn;

// FblcsTypedNameV --
//   A vector of typed names.
typedef struct {
  size_t size;
  FblcsTypedName* xs;
} FblcsTypedNameV;

// FblcsExecActn --
//   Symbol information associated with an exec action.
typedef struct {
  FblcsActn _base;
  FblcsTypedNameV execv;
} FblcsExecActn;

// FblcsType --
//   Symbol information associated with a type declaration.
typedef struct {
  FblcsNameL name;
  FblcsTypedNameV fieldv;
} FblcsType;

// FblcsTypeV --
//   A vector of FblcsType.
typedef struct {
  size_t size;
  FblcsType** xs;
} FblcsTypeV;

// FblcsExprV --
//   A vector of fblcs exprs.
typedef struct {
  size_t size;
  FblcsExpr** xs;
} FblcsExprV;

// FblcsFunc --
//   Symbol information associated with a function declaration.
typedef struct {
  FblcsNameL name;
  FblcsTypedNameV argv;
  FblcsNameL return_type;
  FblcsExprV exprv;
} FblcsFunc;

// FblcsFuncV --
//   A vector of FblcsFunc.
typedef struct {
  size_t size;
  FblcsFunc** xs;
} FblcsFuncV;

// FblcsActnV --
//   A vector of fblcs actns.
typedef struct {
  size_t size;
  FblcsActn** xs;
} FblcsActnV;

// FblcsProc --
//   Symbol information associated with a process declaration.
typedef struct {
  FblcsNameL name;
  FblcsTypedNameV portv;
  FblcsTypedNameV argv;
  FblcsNameL return_type;
  FblcsExprV exprv;
  FblcsActnV actnv;
} FblcsProc;

// FblcsProcV --
//   A vector of FblcsProc.
typedef struct {
  size_t size;
  FblcsProc** xs;
} FblcsProcV;

// FblcsProgram --
//   An FblcProgram augmented with symbols information.
typedef struct {
  FblcProgram* program;
  FblcsTypeV stypev;
  FblcsFuncV sfuncv;
  FblcsProcV sprocv;
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
//   Name resolution is not performed; ids throughout the parsed program will
//   be set to FBLC_NULL_ID in the returned result.
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
//   type - The type id of value to parse.
//   fd - A file descriptor of a file open for reading.
//
// Result:
//   The parsed value, or NULL on error.
//
// Side effects:
//   The value is read from the given file descriptor. In the case of an
//   error, an error message is printed to stderr
FblcValue* FblcsParseValue(FblcArena* arena, FblcsProgram* sprog, FblcType* type, int fd);

// FblcsParseValueFromString --
//   Parse an fblc value from a string.
//
// Inputs:
//   sprog - The program environment.
//   type - The type id of value to parse.
//   string - The string to parse the value from.
//
// Result:
//   The parsed value, or NULL on error.
//
// Side effects:
//   In the case of an error, an error message is printed to standard error.
FblcValue* FblcsParseValueFromString(FblcArena* arena, FblcsProgram* sprog, FblcType* type, const char* string);

// FblcsPrintValue --
//   Print a value in standard format to the given FILE stream.
//
// Inputs:
//   stream - The stream to print the value to.
//   value - The value to print.
//
// Result:
//   None.
//
// Side effects:
//   The value is printed to the given file stream.
void FblcsPrintValue(FILE* stream, FblcsProgram* sprog, FblcType* type, FblcValue* value);

// FblcsResolveProgram --
//   Perform id/name resolution for references to variables, ports,
//   declarations, and fields in the given program.
//
// Inputs:
//   sprog - A program whose ids need to be resolved.
//
// Results:
//   true if name resolution succeeded, false otherwise.
//
// Side effects:
//   IDs in the program are resolved.
//   Prints error messages to stderr in case of error.
bool FblcsResolveProgram(FblcsProgram* sprog);

// FblcsCheckProgram --
//   Check that the given program environment describes a well formed and well
//   typed program.
//
// Inputs:
//   sprog - The program environment to check.
//
// Results:
//   true if the program environment is well formed and well typed,
//   false otherwise.
//
// Side effects:
//   If the program environment is not well formed, an error message is
//   printed to stderr describing the problem with the program environment.
bool FblcsCheckProgram(FblcsProgram* sprog);

// FblcsLoadProgram --
//   Load a text fblc program from the given file using the given arena for
//   allocations. Performs parsing, name resolution, and type checking of the
//   program.
//
// Inputs:
//   arena - Arena to use for allocations.
//   filename - Name of the file to load the program from.
//
// Results:
//   The fully parsed, name-resolved and type-checked loaded program, or NULL
//   if the program could not be parsed, resolved, or failed to type check.
//
// Side effects:
//   Arena allocations are made in order to load the program. If the program
//   cannot be loaded, an error message is printed to stderr.
FblcsProgram* FblcsLoadProgram(FblcArena* arena, const char* filename);

// FblcsLookupType --
//   Look up the type declaration with the given name.
//
// Inputs:
//   sprog - The program to look up the declaration in.
//   name - The name of the type declaration to look up.
//
// Results:
//   The type declaration in the program with the given name, or
//   NULL if no such declaration was found.
//
// Side effects:
//   None.
FblcType* FblcsLookupType(FblcsProgram* sprog, FblcsName name);

// FblcsLookupFunc --
//   Look up the function declaration with the given name.
//
// Inputs:
//   sprog - The program to look up the declaration in.
//   name - The name of the function declaration to look up.
//
// Results:
//   The function declaration in the program with the given name, or
//   NULL if no such declaration was found.
//
// Side effects:
//   None.
FblcFunc* FblcsLookupFunc(FblcsProgram* sprog, FblcsName name);

// FblcsLookupProc --
//   Look up the process declaration with the given name.
//
// Inputs:
//   sprog - The program to look up the declaration in.
//   name - The name of the process declaration to look up.
//
// Results:
//   The process declaration in the program with the given name, or
//   NULL if no such declaration was found.
//
// Side effects:
//   None.
FblcProc* FblcsLookupProc(FblcsProgram* sprog, FblcsName name);

// FblcsLookupEntry --
//   Look up the function or process declaration with the given name.
//   If the entry is a function, wraps the function in a newly allocated
//   process declaration.
//
// Inputs:
//   arena - Arena to use for allocating the wraper process if necessary.
//   sprog - The program to look up the declaration in.
//   name - The name of the declaration to look up.
//
// Results:
//   The process declaration in the program with the given name, a newly
//   allocated process declaration wrapping the function declaration in the
//   program with the given name, or NULL if no such declaration was found.
//
// Side effects:
//   Allocations a wrapper process if a function declaration is found to match
//   the entry name.
FblcProc* FblcsLookupEntry(FblcArena* arena, FblcsProgram* sprog, FblcsName name);

// FblcsLookupField --
//   Look up the id of a field with the given name.
//
// Inputs:
//   sprog - The program to look up the field in.
//   type - The type to look the field up in.
//   field - The name of the field to look up.
//
// Results:
//   The field id of the declared field in the type with the given name, or
//   FBLC_NULL_ID if no such field was found.
//
// Side effects:
//   None.
FblcFieldId FblcsLookupField(FblcsProgram* sprog, FblcType* type, FblcsName field);

// FblcsLookupPort --
//   Look up the id of a port argument with the given name.
//
// Inputs:
//   sprog - The program to look up the port in.
//   proc - The process declaration to look the port up in.
//   port - The name of the port to look up.
//
// Results:
//   The (field) id of the declared port in the process with the given name, or
//   FBLC_NULL_ID if no such port was found. For example, if name refers to
//   the third port argument to the process, the id '2' is returned.
//
// Side effects:
//   None.
FblcFieldId FblcsLookupPort(FblcsProgram* sprog, FblcProc* proc, FblcsName port);

// FblcsTypeName --
//   Return the name of a type declaration.
//
// Inputs:
//   sprog - The program to get the declaration name in.
//   decl - The declaration to get the name of.
//
// Results:
//   The name of the declaration in the program.
FblcsName FblcsTypeName(FblcsProgram* sprog, FblcType* decl);

// FblcsFuncName --
//   Return the name of a function declaration.
//
// Inputs:
//   sprog - The program to get the declaration name in.
//   decl - The declaration to get the name of.
//
// Results:
//   The name of the declaration in the program.
FblcsName FblcsFuncName(FblcsProgram* sprog, FblcFunc* decl);

// FblcsProcName --
//   Return the name of a process declaration.
//
// Inputs:
//   sprog - The program to get the declaration name in.
//   decl - The declaration to get the name of.
//
// Results:
//   The name of the declaration in the program.
FblcsName FblcsProcName(FblcsProgram* sprog, FblcProc* decl);

// FblcsFieldName --
//   Return the name of a field with the given id.
//
// Inputs:
//   sprog - The program to get the field name from.
//   type - The type to get the field name from.
//   field_id - The id of the field within the type to get the name of.
//
// Results:
//   The name of the field with given field_id in the type declaration with
//   given type_id.
//
// Side effects:
//   The behavior is undefined if type_id does not refer to a type declaring a
//   field with field_id.
FblcsName FblcsFieldName(FblcsProgram* sprog, FblcType* type, FblcFieldId field_id);

#endif  // FBLCS_H_
