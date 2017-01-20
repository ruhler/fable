// fblcs.h --
//   This header file describes the externally visible interface to the
//   symbol and source level fblc facilities.

#ifndef FBLCS_H_
#define FBLCS_H_

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
typedef struct FblcsNameL {
  FblcsName name;
  FblcsLoc* loc;
} FblcsNameL;

// FblcsSymbols --
//   A structure used for mapping source level names and locations to and from
//   their corresponding abstract syntax FblcLocId locations.
typedef struct FblcsSymbols FblcsSymbols;

// FblcsProgram --
//   An FblcProgram augmented with symbols information.
typedef struct {
  FblcProgram* program;
  FblcsSymbols* symbols;
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
FblcValue* FblcsParseValue(FblcArena* arena, FblcsProgram* sprog, FblcTypeId type, int fd);

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
FblcValue* FblcsParseValueFromString(FblcArena* arena, FblcsProgram* sprog, FblcTypeId type, const char* string);

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

// FblcsCheckProgram -- see documentation in fblcs.h
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

// FblcsLookupDecl --
//   Look up the id of a declaration with the given name.
//
// Inputs:
//   sprog - The program to look up th declaration in.
//   name - The name of the declaration to look up.
//
// Results:
//   The id of the declaration in the program with the given name, or
//   FBLC_NULL_ID if no such declaration was found.
//
// Side effects:
//   None.
FblcDeclId FblcsLookupDecl(FblcsProgram* sprog, FblcsName name);

// TODO: Remove these functions?
FblcsSymbols* NewSymbols(FblcArena* arena);
void SetLocExpr(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsLoc* loc);
void SetLocActn(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsLoc* loc);
void SetLocId(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsNameL* name);
void SetLocTypedId(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsNameL* type, FblcsNameL* name);
void SetLocLink(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsNameL* type, FblcsNameL* get, FblcsNameL* put);
void SetLocDecl(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsNameL* name, FblcDeclId decl_id);

FblcsLoc* LocIdLoc(FblcsSymbols* symbols, FblcLocId loc_id);
FblcsNameL* LocIdName(FblcsSymbols* symbols, FblcLocId loc_id);
FblcsNameL* LocIdType(FblcsSymbols* symbols, FblcLocId loc_id);
FblcsNameL* LocIdLinkGet(FblcsSymbols* symbols, FblcLocId loc_id);
FblcsNameL* LocIdLinkPut(FblcsSymbols* symbols, FblcLocId loc_id);

FblcLocId DeclLocId(FblcsProgram* sprog, FblcDeclId decl_id);
FblcsName FieldName(FblcsProgram* sprog, FblcDeclId decl_id, FblcFieldId field_id);
FblcsName DeclName(FblcsProgram* sprog, FblcDeclId decl_id);
FblcFieldId SLookupField(FblcsProgram* sprog, FblcDeclId decl_id, FblcsName field);

#endif  // FBLCS_H_
