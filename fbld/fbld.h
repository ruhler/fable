// fbld.h --
//   This header file describes a representation and utilies for working with
//   fbld programs.

#ifndef FBLD_H_
#define FBLD_H_

#include <stdio.h>  // for FILE

#include "fblc.h"   // for FblcArena

// FbldLoc --
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
} FbldLoc;

// FbldReportError --
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
void FbldReportError(const char* format, FbldLoc* loc, ...);

// Forward declarations. See below for descriptions of these types.
typedef struct FbldQRef FbldQRef;
typedef struct FbldQRefV FbldQRefV;
typedef struct FbldInterfV FbldInterfV;
typedef struct FbldModuleV FbldModuleV;

// FbldName -- 
//   A name along with its associated location in a source file. The location
//   is typically used for error reporting purposes.
typedef struct {
  const char* name;
  FbldLoc* loc;
} FbldName;

// FbldNameV --
//   A vector of fbld names.
typedef struct {
  size_t size;
  FbldName** xs;
} FbldNameV;

// FbldId --
//   A reference to a variable, field, or port.
//
// Fields:
//   name - The name of the field.
//   id - The fblc id of the field. This is set to FBLC_NULL_ID by the parser,
//        then later filled in during type check and used by the compiler.
typedef struct {
  FbldName* name;
  size_t id;
} FbldId;

// FbldIdV -- A vector of fbld ids.
typedef struct {
  size_t size;
  FbldId* xs;
} FbldIdV;

// FbldRState --
//   The resolution states for a qref.
typedef enum {
  FBLD_RSTATE_UNRESOLVED, // The qref has not yet been resolved.
  FBLD_RSTATE_FAILED,     // The qref failed resolution.
  FBLD_RSTATE_RESOLVED,   // The qref resolved to a normal entity.
  FBLD_RSTATE_PARAM       // The qref resolved to a parameter.
} FbldRState;

// FbldQRef --
//   A reference to a qualified interface, module, type, function or process,
//   such as:
//    find<Nat; Eq<Nat> eqNat>@ListM
//
// FbldQRef stores both unresolved and resolved versions of the qualified
// reference. The unresolved version is as the reference appears in the source
// code, the resolved version is the canonical global path to the relevant
// entity or type parameter. The parser leaves the qref unresolved. The type
// checker resolves all entities, updating r.state, r.name, and r.mref as
// appropriate. TODO: Is r.name enough to distinguish which parameter is being
// referred to in the case of parameters?
//
// Fields:
//   name - The unresolved name of the entity.
//   targv - The type arguments to the entity.
//   margv - The module arguments to the entity.
//   mref - The unresolved module the entity is from. NULL for references that
//          are not explicitly qualified.
//
//   r.state - The resolution state of the qref.
//   r.name - The resolved name of the entity.
//   r.mref - The resolved module reference. NULL for references to parameters
//            or top level declarations.
struct FbldQRef {
  FbldName* name;
  FbldQRefV* targv;
  FbldQRefV* margv;
  FbldQRef* mref;

  struct {
    FbldRState state;
    FbldName* name;
    FbldQRef* mref;
  } r;
};

// FbldQRefV --
//   A vector of qrefs.
struct FbldQRefV {
  size_t size;
  FbldQRef** xs;
}; 

// FbldArg --
//   An fbld name with associated (possibly qualified) type. Used for
//   declaring fields of structures and arguments to functions.
typedef struct {
  FbldQRef* type;
  FbldName* name;
} FbldArg;

// FbldArgV --
//   A vector of fbld args.
typedef struct {
  size_t size;
  FbldArg** xs;
} FbldArgV;

// FbldNamesEqual --
//   Test whether two names are equal.
//
// Inputs:
//   a - The first name.
//   b - The second name.
//
// Results:
//   true if the first name equals the second, false otherwise.
//
// Side effects:
//   None.
bool FbldNamesEqual(const char* a, const char* b);

// FbldQRefsEqual --
//   Test whether two qrefs are structurally equal.
//
// Inputs:
//   a - The first qref.
//   b - The second qref.
//
// Results:
//   true if the first qref structurally equals the second, false otherwise.
//
// Side effects:
//   None.
bool FbldQRefsEqual(FbldQRef* a, FbldQRef* b);

// FbldExprTag --
//   A tag used to distinguish among different kinds of expressions.
typedef enum {
  FBLD_VAR_EXPR,
  FBLD_APP_EXPR,
  FBLD_UNION_EXPR,
  FBLD_ACCESS_EXPR,
  FBLD_COND_EXPR,
  FBLD_LET_EXPR
} FbldExprTag;

// FbldExpr --
//   Common base type for the following fbld expr types. The tag can be used
//   to determine what kind of expr this is to get access to additional fields
//   of the expr by first casting to that specific type.
typedef struct {
  FbldExprTag tag;
  FbldLoc* loc;
} FbldExpr;

// FbldExprV --
//   A vector of fbld expressions.
typedef struct {
  size_t size;
  FbldExpr** xs;
} FbldExprV;

// FbldVarExpr --
//   A variable expression of the form 'var' whose value is the value of the
//   corresponding variable in scope.
//
// Fields:
//   var - The name of the variable.
typedef struct {
  FbldExpr _base;
  FbldId var;
} FbldVarExpr;

// FbldAppExpr --
//   An application expression of the form 'func(arg0, arg1, ...)'. func may
//   refer to a function or a struct type.
typedef struct {
  FbldExpr _base;
  FbldQRef* func;
  FbldExprV* argv;
} FbldAppExpr;

// FbldUnionExpr --
//   A union expression of the form 'type:field(arg)', used to construct a
//   union value.
typedef struct {
  FbldExpr _base;
  FbldQRef* type;
  FbldId field;
  FbldExpr* arg;
} FbldUnionExpr;

// FbldAccessExpr --
//   An access expression of the form 'obj.field' used to access a field of
//   a struct or union value.
typedef struct {
  FbldExpr _base;
  FbldExpr* obj;
  FbldId field;
} FbldAccessExpr;

// FbldCondExpr --
//   A conditional expression of the form '?(select; arg0, arg1, ...)', which
//   conditionally selects an argument based on the tag of the select value.
typedef struct {
  FbldExpr _base;
  FbldExpr* select;
  FbldExprV* argv;
} FbldCondExpr;

// FbldLetExpr --
//   A let expression of the form '{ type var = def; body }'.
typedef struct {
  FbldExpr _base;
  FbldQRef* type;
  FbldName* var;
  FbldExpr* def;
  FbldExpr* body;
} FbldLetExpr;

// FbldActnTag --
//   A tag used to distinguish among different kinds of actions.
typedef enum {
  FBLD_EVAL_ACTN,
  FBLD_GET_ACTN,
  FBLD_PUT_ACTN,
  FBLD_COND_ACTN,
  FBLD_CALL_ACTN,
  FBLD_LINK_ACTN,
  FBLD_EXEC_ACTN
} FbldActnTag;

// FbldActn --
//   A tagged union of action types. All actions have the same initial
//   layout as FbldActn. The tag can be used to determine what kind of
//   action this is to get access to additional fields of the action
//   by first casting to that specific type of action.
typedef struct {
  FbldActnTag tag;
  FbldLoc* loc;
} FbldActn;

// FbldActnV --
//   A vector of fblcs actions.
typedef struct {
  size_t size;
  FbldActn** xs;
} FbldActnV;

// FbldEvalActn --
//   An evaluation action of the form '$(arg)' which evaluates the given
//   expression without side effects.
typedef struct {
  FbldActn _base;
  FbldExpr* arg;
} FbldEvalActn;

// FbldGetActn --
//   A get action of the form '~port()' used to get a value from a port.
typedef struct {
  FbldActn _base;
  FbldId port;
} FbldGetActn;

// FbldPutActn --
//   A put action of the form '~port(arg)' used to put a value onto a port.
typedef struct {
  FbldActn _base;
  FbldId port;
  FbldExpr* arg;
} FbldPutActn;

// FbldCondActn --
//   A conditional action of the form '?(select; arg0, arg1, ...)', which
//   conditionally selects an argument based on the tag of the select value.
typedef struct {
  FbldActn _base;
  FbldExpr* select;
  FbldActnV* argv;
} FbldCondActn;

// FbldCallActn --
//   A call action of the form 'proc(port0, port1, ... ; arg0, arg1, ...)',
//   which calls a process with the given port and value arguments.
typedef struct {
  FbldActn _base;
  FbldQRef* proc;
  FbldIdV* portv;
  FbldExprV* argv;
} FbldCallActn;

// FbldLinkActn --
//   A link action of the form 'type <~> get, put; body'. The names of the get
//   and put ports are De Bruijn indices based on the context where the ports
//   are accessed.
typedef struct {
  FbldActn _base;
  FbldQRef* type;
  FbldName* get;
  FbldName* put;
  FbldActn* body;
} FbldLinkActn;

// FbldExec --
//   Trip of type, name, and action used in the FbldExecActn.
typedef struct {
  FbldQRef* type;
  FbldName* name;
  FbldActn* actn;
} FbldExec;

// FbldExecV --
//   Vector of FbldExec.
typedef struct {
  size_t size;
  FbldExec* xs;
} FbldExecV;

// FbldExecActn --
//   An exec action of the form 'type0 var0 = exec0, type1 var1 = exec1, ...; body',
//   which executes processes in parallel.
typedef struct {
  FbldActn _base;
  FbldExecV* execv;
  FbldActn* body;
} FbldExecActn;

// FbldImportItem --
//   Specification for a single item in an import declaration of the form
//   <dest>[=<source>].
//
// Fields:
//   source - The name of the entity in the original module.
//   dest - The name under which the entity is referred to in the importing
//          module.
//
// It is not uncommon to have the source and destination names be the same, in
// which case the source and dest fields should be identical, rather than
// setting one or the other to NULL.
typedef struct {
  FbldName* source;
  FbldName* dest;
} FbldImportItem;

// FbldImportItemV --
//   A vector of fbld import items.
typedef struct {
  size_t size;
  FbldImportItem** xs;
} FbldImportItemV;

// FbldImport --
//   An import declaration of the form:
//    using <mref> {<name>[=<name>]; <name>[=<name>]; ...; }
//
//   mref is set to NULL to indicate we are importing from the parent
//   namespace '@'.
typedef struct {
  FbldQRef* mref;
  FbldImportItemV* itemv;
} FbldImport;

// FbldImportV --
//   A vector of FbldImports.
typedef struct {
  size_t size;
  FbldImport** xs;
} FbldImportV;

// FbldMArg --
//   A module argument, such as: Map<Int, String> M
//
// Fields:
//   iref - The interface of the module argument.
//   name - The name of the module argument.
typedef struct {
  FbldQRef* iref;
  FbldName* name;
} FbldMArg;

// FbldMArgV --
//   A vector of module arguments.
typedef struct {
  size_t size;
  FbldMArg** xs;
} FbldMArgV;

// FbldParams --
//   The polymorphic type and module parameters of a declaration.
typedef struct {
  FbldNameV* targv;
  FbldMArgV* margv;
} FbldParams;

// FbldKind --
//   An enum used to distinguish between struct, union, and abstract types.
typedef enum {
  FBLD_STRUCT_KIND,
  FBLD_UNION_KIND,
  FBLD_ABSTRACT_KIND
} FbldKind;

// FbldType --
//   An fbld type declaration.
//   fieldv is NULL for abstract type declarations.
typedef struct {
  FbldName* name;
  FbldKind kind;
  FbldParams* params;
  FbldArgV* fieldv;
} FbldType;

// FbldTypeV --
//   A vector of FbldTypes.
typedef struct {
  size_t size;
  FbldType** xs;
} FbldTypeV;

// FbldFunc--
//   A declaration of a function.
//   The body is NULL for function prototypes specified in module
//   declarations.
typedef struct {
  FbldName* name;
  FbldParams* params;
  FbldArgV* argv;
  FbldQRef* return_type;
  FbldExpr* body;
} FbldFunc;

// FbldFuncV --
//   A vector of FbldFuncs.
typedef struct {
  size_t size;
  FbldFunc** xs;
} FbldFuncV;

// FbldPolarity --
//   The polarity of a port.
typedef enum {
  FBLD_GET_POLARITY,
  FBLD_PUT_POLARITY
} FbldPolarity;

// FbldPort --
//   The type, name, and polarity of a port.
typedef struct {
  FbldQRef* type;
  FbldName* name;
  FbldPolarity polarity;
} FbldPort;

// FbldPortV --
//   A vector of fbld ports.
typedef struct {
  size_t size;
  FbldPort* xs;
} FbldPortV;

// FbldProc --
//   A declaration of a process.
//   The body is NULL for process prototypes specified in module
//   declarations.
typedef struct {
  FbldName* name;
  FbldParams* params;
  FbldPortV* portv;
  FbldArgV* argv;
  FbldQRef* return_type;
  FbldActn* body;
} FbldProc;

// FbldProcV --
//   A vector of FbldProcs.
typedef struct {
  size_t size;
  FbldProc** xs;
} FbldProcV;

// FbldInterf --
//   An fbld interface declaration.
typedef struct {
  FbldName* name;
  FbldParams* params;
  FbldImportV* importv;
  FbldTypeV* typev;
  FbldFuncV* funcv;
  FbldProcV* procv;
  FbldInterfV* interfv;
  FbldModuleV* modulev;
} FbldInterf;

// FbldInterfV --
//   A vector of fbld interface declarations.
struct FbldInterfV {
  size_t size;
  FbldInterf** xs;
};

// FbldModule --
//   An fbld module declaration.
//
// Fields:
//   name - The name of the module being defined.
//   params - The polymorphic type and module parameters of the module.
//   iref - The interface the module implements.
//   importv - The using declarations within the module definition.
//   typev - The type declarations within the module definition.
//   funcv - The func declarations within the module definition.
//   procv - The proc declarations within the module definition.
//   interfv - The interface declarations within the module definition.
//   modulev - The module declarations within the module definition.
typedef struct {
  FbldName* name;
  FbldParams* params;
  FbldQRef* iref;
  FbldImportV* importv;
  FbldTypeV* typev;
  FbldFuncV* funcv;
  FbldProcV* procv;
  FbldInterfV* interfv;
  FbldModuleV* modulev;
} FbldModule;

// FbldModuleV --
//   A vector of fbld modules.
struct FbldModuleV {
  size_t size;
  FbldModule** xs;
};

// FbldProgram --
//   A collection of top level interfaces, module headers, and modules
//   representing a program.
//
//   A module header is an module whose body has not necessarily been checked
//   for validity. This makes it possible to check the validity of a module
//   without the need for the implementations of the other modules it depends
//   on.
typedef struct {
  FbldInterfV interfv;
  FbldModuleV mheaderv;
  FbldModuleV modulev;
} FbldProgram;

// Forward declaration of FbldValue type.
typedef struct FbldValue FbldValue;

// FbldValueV -- 
//   A vector of fbld values.
typedef struct {
  size_t size;
  FbldValue** xs;
} FbldValueV;

// FbldValue --
//   fbld representation of an value object.
struct FbldValue {
  FbldKind kind;
  FbldQRef* type;
  FbldName* tag;
  FbldValueV* fieldv;
};

// FbldParseInterf --
//   Parse the interface declaration from the file with the given filename.
//
// Inputs:
//   arena - The arena to use for allocating the parsed declaration.
//   filename - The name of the file to parse the declaration from.
//
// Results:
//   The parsed declaration, or NULL if the declaration could not be parsed.
//
// Side effects:
//   Prints an error message to stderr if the declaration cannot be parsed.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the returned declaration if there is no error.
FbldInterf* FbldParseInterf(FblcArena* arena, const char* filename);

// FbldParseModule --
//   Parse the module definition from the file with the given filename.
//
// Inputs:
//   arena - The arena to use for allocating the parsed definition.
//   filename - The name of the file to parse the definition from.
//
// Results:
//   The parsed definition, or NULL if the definition could not be parsed.
//
// Side effects:
//   Prints an error message to stderr if the definition cannot be parsed.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the returned definition if there is no error.
FbldModule* FbldParseModule(FblcArena* arena, const char* filename);

// FbldParseValueFromString --
//   Parse an fbld value from the given string.
//
// Inputs:
//   arena - The arena to use for allocating the parsed definition.
//   string - The string to parse the value from.
//
// Results:
//   The parsed value, or NULL if the value could not be parsed.
//
// Side effects:
//   Prints an error message to stderr if the value cannot be parsed.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the returned value if there is no error.
FbldValue* FbldParseValueFromString(FblcArena* arena, const char* string);

// FbldParseQRefFromString --
//   Parse an FbldQRef from the given string.
//
// Inputs:
//   arena - The arena to use for allocating the parsed definition.
//   string - The string to parse the value from.
//
// Results:
//   The parsed qref, or NULL if the qref could not be parsed.
//
// Side effects:
//   Prints an error message to stderr if the qref cannot be parsed.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function.
FbldQRef* FbldParseQRefFromString(FblcArena* arena, const char* string);

// FbldStringV --
//   A vector of char* used for describing fbld search paths.
typedef struct {
  size_t size;
  const char** xs;
} FbldStringV;

// FbldLoadInterf --
//   Load the interface declaration for the module with the given name.
//   If the interface declaration has already been loaded, it is returned as
//   is. Otherwise the interface declaration and all of its dependencies are
//   located according to the given search path, parsed, checked, and added to
//   the collection of loaded modules before the requested interface is
//   returned.
//
// Inputs:
//   arena - The arena to use for allocating the parsed declaration.
//   path - A search path used to find the interface declaration on disk.
//   name - The name of the interface whose declaration to load.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   The loaded interface declaration, or NULL if the interface declaration
//   could not be loaded.
//
// Side effects:
//   Read the interface delacaration and any other required interface
//   delacarations from disk and add them to the prgm.
//   Prints an error message to stderr if the interface declaration or any
//   other required interface declarations cannot be loaded, either because
//   they cannot be found on the path, fail to parse, or fail to check.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of all loaded declarations if there is no error.
FbldInterf* FbldLoadInterf(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm);

// FbldLoadModuleHeader --
//   Load the module header for the module with the
//   given name. The module definition, its corresponding module type, and all
//   of the module types and declarations that the module header depends on are
//   located according to the given search path, parsed, checked, and to the
//   program before the loaded module definition is returned. The contents of
//   the body of the returned module are not yet checked.
//
// Inputs:
//   arena - The arena to use for allocating the loaded definition and
//           declarations.
//   path - A search path used to find the module definitions and declaration
//          on disk.
//   name - The name of the module whose definition to load.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   The loaded module definition, or NULL if the module definition could
//   not be loaded.
//
// Side effects:
//   Read the module definition and any other required module delacarations
//   from disk and add the module declarations to the prgm.
//   Prints an error message to stderr if the module definition or any other
//   required module declarations cannot be loaded, either because they cannot
//   be found on the path, fail to parse, or fail to check.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the definition and all loaded declarations if there is no error.
FbldModule* FbldLoadModuleHeader(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm);

// FbldLoadModule --
//   Load the module declaration for the module with the given name.
//   The module definition, its corresponding module type, and all of
//   the module types and declarations they depend on are located according to
//   the given search path, parsed, checked, and to the program before the
//   loaded module definition is returned.
//
// Inputs:
//   arena - The arena to use for allocating the loaded definition and
//           declarations.
//   path - A search path used to find the module definitions and declaration
//          on disk.
//   name - The name of the module whose definition to load.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   The loaded module definition, or NULL if the module definition could
//   not be loaded.
//
// Side effects:
//   Read the module definition and any other required module delacarations
//   from disk and add the module declarations to the prgm.
//   Prints an error message to stderr if the module definition or any other
//   required module declarations cannot be loaded, either because they cannot
//   be found on the path, fail to parse, or fail to check.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the definition and all loaded declarations if there is no error.
FbldModule* FbldLoadModule(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm);

// FbldLoadModules --
//   Load all module definitions and declarations required by the
//   named module.
//   All of the module declarations and definitions the module depends
//   on are located according to the given search path, parsed, checked, and
//   added to the collections of loaded module declarations and definitions.
//
// Inputs:
//   arena - The arena to use for allocating the loaded definition and
//           declarations.
//   path - A search path used to find the module definitions and declaration
//          on disk.
//   name - The name of the module for which to load dependencies.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   True on success, false on error.
//
// Side effects:
//   Reads required module delacarations and definitions from disk and adds
//   them to the prgm.
//   Prints an error message to stderr if there is an error.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of all loaded declarations and definitions if there is no error.
bool FbldLoadModules(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm);

// FbldLoadEntry --
//   Load all module definitions and declarations required to compile the
//   named entity.
//   All of the module declarations and definitions the named entity depends
//   on are located according to the given search path, parsed, checked, and
//   added to the collections of loaded module declarations and definitions.
//
// Inputs:
//   arena - The arena to use for allocating the loaded definition and
//           declarations.
//   path - A search path used to find the module definitions and declaration
//          on disk.
//   entry - The entity for which to load dependencies.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   True on success, false on error.
//
// Side effects:
//   Reads required module delacarations and definitions from disk and adds
//   them to the prgm. Resolves qentry.
//   Prints an error message to stderr if there is an error.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of all loaded declarations and definitions if there is no error.
bool FbldLoadEntry(FblcArena* arena, FbldStringV* path, FbldQRef* entry, FbldProgram* prgm);

// FbldCheckQRef --
//   Check that the given qualified reference is well formed and well typed in
//   the global context.
//
// Inputs:
//   arena - Arena used for allocations.
//   path - A search path used to find interface and module declaration on
//          disk.
//   qref - The qualified reference to check.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   true if the qualified reference is well formed and well typed, false
//   otherwise.
//
// Side effects:
//   Loads and checks required interfaces and module headers (not modules),
//   adding them to prgm.
//   If this qualified reference or any of the required declarations are not
//   well formed, error messages are printed to stderr describing the
//   problems.
bool FbldCheckQRef(FblcArena* arena, FbldStringV* path, FbldQRef* qref, FbldProgram* prgm);

// FbldCheckInterf --
//   Check that the given interface declaration is well formed and well typed.
//
// Inputs:
//   arena - Arena used for allocations.
//   path - A search path used to find interface and module declaration on
//          disk.
//   interf - The interface declaration to check.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   true if the interface declaration is well formed and well typed, false
//   otherwise.
//
// Side effects:
//   Loads and checks required interfaces and module headers (not modules),
//   adding them to prgm.
//   If this declaration or any of the required declarations are not well
//   formed, error messages are printed to stderr describing the problems.
bool FbldCheckInterf(FblcArena* arena, FbldStringV* path, FbldInterf* interf, FbldProgram* prgm);

// FbldCheckModuleHeader --
//   Check that the declaration part of the given module definition is well
//   formed, well typed, and consistent with its own module declaration.
//
// Inputs:
//   arena - Arena used for allocations.
//   path - A search path used to find the module definitions and declaration
//          on disk.
//   module - The module declaration part to check.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   true if the module header is well formed and well typed, false otherwise.
//
// Side effects:
//   Loads and checks required interface declarations and module headers (not
//   module definitions), adding them to prgm.
//   If this declaration or any of the required declarations are not well
//   formed, error messages are printed to stderr describing the problems.
bool FbldCheckModuleHeader(FblcArena* arena, FbldStringV* path, FbldModule* module, FbldProgram* prgm);

// FbldCheckModule --
//   Check that the given module definition is well formed, well typed, and
//   consistent with its own module declaration.
//
// Inputs:
//   arena - Arena used for allocations.
//   path - A search path used to find the module definitions and declaration
//          on disk.
//   module - The module declaration to check.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   true if the module declaration is well formed and well typed, false
//   otherwise.
//
// Side effects:
//   Loads and checks required interface declarations and module headers (not
//   module definitions), adding them to prgm.
//   If this declaration or any of the required declarations are not well
//   formed, error messages are printed to stderr describing the problems.
bool FbldCheckModule(FblcArena* arena, FbldStringV* path, FbldModule* module, FbldProgram* prgm);

// FbldCheckValue --
//   Check that the value is well formed in a global context.
//
// Inputs:
//   arena - Arena to use for any necessary internal allocations.
//   prgm - The program environment.
//   value - The value to check.
//
// Results:
//   true if the value is well formed, false otherwise.
//
// Side effects:
//   Resolves references in the value.
//   Prints a message to stderr if the value is not well formed.
bool FbldCheckValue(FblcArena* arena, FbldProgram* prgm, FbldValue* value);

// FbldLookupModule --
//   Look up a module definition in the program.
//
// Inputs:
//   prgm - The program to look in.
//   name - The name of the module to look up.
//
// Results:
//   The module definition, or NULL if no such module definition could be
//   found.
//
// Side effects:
//   none.
FbldModule* FbldLookupModule(FbldProgram* prgm, FbldName* name);

// FbldLookupType --
//   Look up a resolved type declaration in the program.
//
// Inputs:
//   prgm - The program to look in.
//   entity - The entity to look up.
//
// Results:
//   The type declaration or NULL if no such type could be found.
//
// Side effects:
//   None.
FbldType* FbldLookupType(FbldProgram* prgm, FbldQRef* entity);

// FbldLookupFunc --
//   Look up a function declaration in the program.
//
// Inputs:
//   prgm - The program to look in.
//   entity - The entity to look up.
//
// Results:
//   The function declaration or NULL if no such function could be found.
//
// Side effects:
//   None.
FbldFunc* FbldLookupFunc(FbldProgram* prgm, FbldQRef* entity);

// FbldLookupProc --
//   Look up a process declaration in the program.
//
// Inputs:
//   prgm - The program to look in.
//   entity - The entity to look up.
//
// Results:
//   The process declaration or NULL if no such process could be found.
//
// Side effects:
//   None.
FbldProc* FbldLookupProc(FbldProgram* prgm, FbldQRef* entity);

// FbldImportQRef --
//   Import an already resolved qref from another module.
//   Substitutes all references to local type parameters and module parameters
//   with the arguments supplied in the given module reference context.
//
// Inputs:
//   arena - Arena to use for allocations.
//   prgm - The program context.
//   ctx - The context the entity is being referred to from.
//   entity - The entity to import.
//
// Results:
//   The entity imported into the given context.
//
// Side effects:
//   Behavior is undefined if the entity has not already been resolved.
//   TODO: Allocates a new entity that somebody should probably clean up somehow.
FbldQRef* FbldImportQRef(FblcArena* arena, FbldProgram* prgm, FbldQRef* ctx, FbldQRef* qref);

// FbldPrintQRef --
//   Print a resolved qref in human readable format to the given stream.
//
// Inputs:
//   stream - Stream to print the type to.
//   qref - The qref to print.
//
// Results:
//   None.
//
// Side effects:
//   Prints the qref to the stream.
void FbldPrintQRef(FILE* stream, FbldQRef* qref);

// FbldAccessLoc --
//   The location of an access expression, for aid in debugging undefined
//   member access.
//
// Fields:
//   expr - An access expression.
//   loc - The source location of the access expression.
typedef struct {
  FblcExpr* expr;
  FbldLoc* loc;
} FbldAccessLoc;

// FbldAccessLocV --
//   A vector of access locs.
typedef struct {
  size_t size;
  FbldAccessLoc* xs;
} FbldAccessLocV;

// FbldLoaded --
//   The fbld program, fbld proc, and fblc proc corresponding to a compiled
//   entry point of a loaded program.
typedef struct {
  FbldProgram* prog;
  FbldProc* proc_d;
  FblcProc* proc_c;
} FbldLoaded;

// FbldCompileProgram --
//   Compile an fbld program from an already checked fbld program.
//
// Inputs:
//   arena - arena to use for allocating the fblc program.
//   accessv - collection of access expression locations.
//   prgm - The fbld program environment.
//   entity - The name of the main entry to compile the program for.
//
// Result:
//   The loaded program and entry points, or NULL if the entry could not be
//   found.
//
// Side effects:
//   Updates accessv with the location of compiled access expressions.
//   The behavior is undefined if the program environment is not well formed.
FbldLoaded* FbldCompileProgram(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* entity);

// FbldCompileValue --
//   Compile an fbld value to an fblc value.
//
// Inputs:
//   arena - Arena to use for allocating the fblc program.
//   modulev - Modules describing a valid fbld program.
//   value - The fbld value to compile.
//
// Result:
//   An fblc value equivalent to the given fbld value.
//
// Side effects:
//   The behavior is undefined if the fbld program is not a valid fbld
//   program or the fbld value is not well typed.
FblcValue* FbldCompileValue(FblcArena* arena, FbldProgram* prgm, FbldValue* value);
#endif // FBLD_H_
