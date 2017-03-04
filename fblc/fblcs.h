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
typedef struct FblcsNameL {
  FblcsName name;
  FblcsLoc* loc;
} FblcsNameL;

// FblcsSymbolTag -- 
//   Enum used to distinguish among different kinds of symbols.
typedef enum {
  FBLCS_LOC_SYMBOL,
  FBLCS_ID_SYMBOL,
  FBLCS_TYPED_ID_SYMBOL,
  FBLCS_LINK_SYMBOL,
  FBLCS_DECL_SYMBOL
} FblcsSymbolTag;

// FblcsSymbol --
//   A tagged union of symbol types. All symbols have the same initial layout
//   as FblcsSymbol. The tag can be used to determine what kind of symbol this
//   is to get access to additional fields of the symbol by first casting to
//   that specific type of symbol.
//
//   Symbols encode information about names, locations, and other useful
//   information relevant to an FblcLocId location in an FblcProgram.
typedef struct {
  FblcsSymbolTag tag;
} FblcsSymbol;

// FblcsLocSymbol --
//   A symbol that stores information about a location only. This is used to
//   store the source location of expressions and actions.
typedef struct {
  FblcsSymbolTag tag;
  FblcsLoc loc;
} FblcsLocSymbol;

// FblcsIdSymbol --
//   A symbol that stores information about a name and a location. This is
//   used to store the name and location of variable names and return types.
typedef struct {
  FblcsSymbolTag tag;
  FblcsNameL name;
} FblcsIdSymbol;

// FblcsTypedIdSymbol --
//   A symbol that stores location and name information for an id and it's
//   type. This is used to store the name, type, and location for field and
//   variable declarations.
typedef struct {
  FblcsSymbolTag tag;
  FblcsNameL name;
  FblcsNameL type;
} FblcsTypedIdSymbol;

// FblcsLinkSymbol --
//   A symbol that stores location and name information for a link action's
//   type, get port name, and put port name.
typedef struct {
  FblcsSymbolTag tag;
  FblcsNameL type;
  FblcsNameL get;
  FblcsNameL put;
} FblcsLinkSymbol;

// FblcsDeclSymbol --
//   A symbol that stores information for a declaration.
typedef struct {
  FblcsSymbolTag tag;
  FblcsNameL name;
  FblcDeclId decl;
} FblcsDeclSymbol;

// FblcsSymbols --
//   A structure used for mapping source level names and locations to and from
//   their corresponding abstract syntax FblcLocId locations.
//
// Fields:
//   symbolc/symbolv - A vector of symbol information indexed by FblcLocId.
//   declc/declv - A vector mapping FblcDeclId to corresponding FblcLocId.
typedef struct {
  size_t symbolc;
  FblcsSymbol** symbolv;
  size_t declc;
  FblcLocId* declv;
} FblcsSymbols;

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
void FblcsPrintValue(FILE* stream, FblcsProgram* sprog, FblcTypeId type_id, FblcValue* value);

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
//   sprog - The program to look up the declaration in.
//   name - The name of the declaration to look up.
//
// Results:
//   The id of the declaration in the program with the given name, or
//   FBLC_NULL_ID if no such declaration was found.
//
// Side effects:
//   None.
FblcDeclId FblcsLookupDecl(FblcsProgram* sprog, FblcsName name);

// FblcsLookupDecl --
//   Look up the id of a port argument with the given name.
//
// Inputs:
//   sprog - The program to look up the port in.
//   proc_id - The id of the process declaration to look the port up in.
//   port - The name of the port to look up.
//
// Results:
//   The (field) id of the declared port in the process with the given name, or
//   FBLC_NULL_ID if no such port was found. For example, if name refers to
//   the third port argument to the process, the id '2' is returned.
//
// Side effects:
//   None.
FblcFieldId FblcsLookupPort(FblcsProgram* sprog, FblcDeclId proc_id, FblcsName port);

// TODO: Remove these functions?
FblcsName FieldName(FblcsProgram* sprog, FblcDeclId decl_id, FblcFieldId field_id);
FblcsName DeclName(FblcsProgram* sprog, FblcDeclId decl_id);
FblcFieldId SLookupField(FblcsProgram* sprog, FblcDeclId decl_id, FblcsName field);

#endif  // FBLCS_H_
