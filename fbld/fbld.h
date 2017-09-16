// fbld.h --
//   This header file describes a representation and utilies for working with
//   fbld programs.

#ifndef FBLD_H_
#define FBLD_H_

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
typedef struct FbldQNameV FbldQNameV;
typedef struct FbldMRefV FbldMRefV;

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

// FbldIRef --
//   A reference to a concrete interface, such as:
//    Map<Int, String@String<;>>
//
// Fields:
//   name - The name of the interface.
//   targs - The type arguments to the interface.
typedef struct {
  FbldName* name;
  FbldQNameV* targs;
} FbldIRef;

// FbldMRef --
//   A reference to a concrete module, such as:
//    HashMap<Int, String@String<;>; HashInt<;>>
//
// Fields:
//   name - The name of the module.
//   targs - The type arguments to the module.
//   margs - The module arguments to the module.
//
// In the case of modules passed as parameters, FbldMRef is still used, but
// with NULL targs and margs. This is in contrast to a global module with no
// type or module arguments, which will have non-NULL, empty targs and margs.
typedef struct {
  FbldName* name;
  FbldQNameV* targs;
  FbldMRefV* margs;
} FbldMRef;

// FbldMRefV --
//   A vector of module references.
struct FbldMRefV {
  size_t size;
  FbldMRef** xs;
}; 

// FbldQName --
//   A reference to an entity in some module, such as:
//    contains@HashMap<Int, String@String<;>; HashInt<;>>
//
// Fields:
//   name - The name of the entity.
//   mref - The module the entity is from. May be NULL to indicate the module
//          should be determined based on context.
typedef struct {
  FbldName* name;
  FbldMRef* mref;
} FbldQName;

// FbldQNameV --
//   A vector of qualified names.
struct FbldQNameV {
  size_t size;
  FbldQName** xs;
}; 

// FbldArg --
//   An fbld name with associated (possibly qualified) type. Used for
//   declaring fields of structures and arguments to functions.
typedef struct {
  FbldQName* type;
  FbldName* name;
} FbldArg;

// FbldArgV --
//   A vector of fbld args.
typedef struct {
  size_t size;
  FbldArg** xs;
} FbldArgV;

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

// FbldQNamesEqual --
//   Test whether two qnames are structurally equal.
//
// Inputs:
//   a - The first name.
//   b - The second name.
//
// Results:
//   true if the first name structurally equals the second, false otherwise.
//
// Side effects:
//   None.
bool FbldQNamesEqual(FbldQName* a, FbldQName* b);

// FbldExpr --
//   Common base type for the following fbld expr types. The tag can be used
//   to determine what kind of expr this is to get access to additional fields
//   of the expr by first casting to that specific type.
typedef struct {
  FblcExprTag tag;
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
  FbldQName* func;
  FbldExprV* argv;
} FbldAppExpr;

// FbldUnionExpr --
//   A union expression of the form 'type:field(arg)', used to construct a
//   union value.
typedef struct {
  FbldExpr _base;
  FbldQName* type;
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
  FbldQName* type;
  FbldName* var;
  FbldExpr* def;
  FbldExpr* body;
} FbldLetExpr;

// FbldUsingItem --
//   Specification for a single item in a using declaration of the form
//   <name>[=<name>].
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
} FbldUsingItem;

// FbldUsingItemV --
//   A vector of fbld using items.
typedef struct {
  size_t size;
  FbldUsingItem** xs;
} FbldUsingItemV;

// FbldUsing --
//   A using declaration of the form:
//    using <mref>{<name>[=<name>]; <name>[=<name>]; ...; }
typedef struct {
  FbldMRef* mref;
  FbldUsingItemV* itemv;
} FbldUsing;

// FbldUsingV --
//   A vector of FbldUsings.
typedef struct {
  size_t size;
  FbldUsing** xs;
} FbldUsingV;


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
  FbldArgV* argv;
  FbldQName* return_type;
  FbldExpr* body;
} FbldFunc;

// FbldFuncV --
//   A vector of FbldFuncs.
typedef struct {
  size_t size;
  FbldFunc** xs;
} FbldFuncV;

// FbldMType --
//   An fbld mtype declaration.
typedef struct {
  FbldName* name;
  FbldNameV* targs;
  FbldUsingV* usingv;
  FbldTypeV* typev;
  FbldFuncV* funcv;
} FbldMType;

// FbldMTypeV --
//   A vector of fbld mtypes.
typedef struct {
  size_t size;
  FbldMType** xs;
} FbldMTypeV;

// FbldMArg --
//   A module argument, such as: Map<Int, String> M
//
// Fields:
//   iref - The interface of the module argument.
//   name - The name of the module argument.
typedef struct {
  FbldIRef* iref;
  FbldName* name;
} FbldMArg;

// FbldMArgV --
//   A vector of module arguments.
typedef struct {
  size_t size;
  FbldMArg** xs;
} FbldMArgV;

// FbldMDefn --
//   An fbld mdefn declaration.
//
// Fields:
//   name - The name of the module being defined.
//   targs - The type parameters of the module.
//   margs - The module parameters of the module.
//   iref - The interface the module implements.
//   usingv - The using declarations within the module definition.
//   typev - The type declarations within the module definition.
//   funcv - The func declarations within the module definition.
typedef struct {
  FbldName* name;
  FbldNameV* targs;
  FbldMArgV* margs;
  FbldIRef* iref;
  FbldUsingV* usingv;
  FbldTypeV* typev;
  FbldFuncV* funcv;
} FbldMDefn;

// FbldMDefnV --
//   A vector of fbld mdefns.
typedef struct {
  size_t size;
  FbldMDefn** xs;
} FbldMDefnV;

// FbldProgram --
//   A collection of mtypes, mdecls, and mdefns representing a program.
//
//   An mdecl is an mdefn whose body has not necessarily been checked for
//   validity. This makes it possible to check the validity of a module
//   without the need for the implementations of the other modules it depends
//   on.
typedef struct {
  FbldMTypeV mtypev;
  FbldMDefnV mdeclv;
  FbldMDefnV mdefnv;
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
  FbldQName* type;
  FbldName* tag;
  FbldValueV* fieldv;
};

// FbldParseMType --
//   Parse the mtype declaration from the file with the given filename.
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
FbldMType* FbldParseMType(FblcArena* arena, const char* filename);

// FbldParseMDefn --
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
FbldMDefn* FbldParseMDefn(FblcArena* arena, const char* filename);

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

// FbldParseQNameFromString --
//   Parse an FbldQName from the given string.
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
FbldQName* FbldParseQNameFromString(FblcArena* arena, const char* string);

// FbldStringV --
//   A vector of char* used for describing fbld search paths.
typedef struct {
  size_t size;
  const char** xs;
} FbldStringV;

// FbldLoadMType --
//   Load the mtype declaration for the module with the given name.
//   If the mtype declaration has already been loaded, it is returned as is.
//   Otherwise the mtype declaration and all of its dependencies are located
//   according to the given search path, parsed, checked, and added to the
//   collection of loaded modules before the requested module declaration is
//   returned.
//
// Inputs:
//   arena - The arena to use for allocating the parsed declaration.
//   path - A search path used to find the mtype declaration on disk.
//   name - The name of the module whose declaration to load.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   The loaded mtype declaration, or NULL if the mtype declaration could
//   not be loaded.
//
// Side effects:
//   Read the mtype delacaration and any other required mtype delacarations
//   from disk and add them to the prgm.
//   Prints an error message to stderr if the mtype declaration or any other
//   required mtype declarations cannot be loaded, either because they cannot
//   be found on the path, fail to parse, or fail to check.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of all loaded declarations if there is no error.
FbldMType* FbldLoadMType(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm);

// FbldLoadMDecl --
//   Load the declaration part of an mdefn declaration for the module with the
//   given name. The module definition, its corresponding module type, and all
//   of the module types and declarations that the mdecl part depends on are
//   located according to the given search path, parsed, checked, and to the
//   program before the loaded module definition is returned. The contents of
//   the returned mdefn are not yet checked.
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
FbldMDefn* FbldLoadMDecl(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm);

// FbldLoadMDefn --
//   Load the mdefn declaration for the module with the given name.
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
FbldMDefn* FbldLoadMDefn(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm);

// FbldLoadMDefns --
//   Load all module definitions and declarations required to compile the
//   named module.
//   All of the module declarations and definitions the named module depends
//   on are located according to the given search path, parsed, checked, and
//   fadded to the collections of loaded module declarations and definitions.
//
// Inputs:
//   arena - The arena to use for allocating the loaded definition and
//           declarations.
//   path - A search path used to find the module definitions and declaration
//          on disk.
//   name - The name of a module whose self and dependencies to load.
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
bool FbldLoadMDefns(FblcArena* arena, FbldStringV* path, const char* name, FbldProgram* prgm);

// FbldCheckMType --
//   Check that the given mtype declaration is well formed and well typed.
//
// Inputs:
//   arena - Arena used for allocations.
//   path - A search path used to find the module definitions and declaration
//          on disk.
//   mtype - The mtype declaration to check.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   true if the mtype declaration is well formed and well typed, false
//   otherwise.
//
// Side effects:
//   Loads and checks required mtype and mdecls (not mdefns), adding them to
//   prgm.
//   If this declaration or any of the required declarations are not well
//   formed, error messages are printed to stderr describing the problems.
bool FbldCheckMType(FblcArena* arena, FbldStringV* path, FbldMType* mtype, FbldProgram* prgm);

// FbldCheckMDecl --
//   Check that the declaration part of the given module definition is well
//   formed, well typed, and consistent with its own module declaration.
//
// Inputs:
//   arena - Arena used for allocations.
//   path - A search path used to find the module definitions and declaration
//          on disk.
//   mdefn - The mdefn declaration part to check.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   true if the mdecl part of the mdefn declaration is well formed and well
//   typed, false otherwise.
//
// Side effects:
//   Loads and checks required mtype and mdecls (not mdefns), adding them to
//   prgm.
//   If this declaration or any of the required declarations are not well
//   formed, error messages are printed to stderr describing the problems.
bool FbldCheckMDecl(FblcArena* arena, FbldStringV* path, FbldMDefn* mdefn, FbldProgram* prgm);

// FbldCheckMDefn --
//   Check that the given module definition is well formed, well typed, and
//   consistent with its own module declaration.
//
// Inputs:
//   arena - Arena used for allocations.
//   path - A search path used to find the module definitions and declaration
//          on disk.
//   mdefn - The mdefn declaration to check.
//   prgm - The collection of declarations loaded for the program so far.
//
// Results:
//   true if the mdefn declaration is well formed and well typed, false
//   otherwise.
//
// Side effects:
//   Loads and checks required mtype and mdecls (not mdefns), adding them to
//   prgm.
//   If this declaration or any of the required declarations are not well
//   formed, error messages are printed to stderr describing the problems.
bool FbldCheckMDefn(FblcArena* arena, FbldStringV* path, FbldMDefn* mdefn, FbldProgram* prgm);

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
  FbldFunc* proc_d;   // TODO: Change this to FbldProc once we support those.
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
FbldLoaded* FbldCompileProgram(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity);

// FbldCompileValue --
//   Compile an fbld value to an fblc value.
//
// Inputs:
//   arena - Arena to use for allocating the fblc program.
//   mdefnv - Modules describing a valid fbld program.
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
