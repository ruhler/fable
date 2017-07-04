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
//   id - The id of the variable. This will be set to FBLC_NULL_ID by the
//        parser, and filled in with the actual id when the expression is type
//        checked.
typedef struct {
  FbldExpr _base;
  FbldName* var;
  FblcVarId id;
} FbldVarExpr;

// FbldAppExpr --
//   An application expression of the form 'func(arg0, arg1, ...)'. func may
//   refer to a function or a struct type.
typedef struct {
  FbldExpr _base;
  FbldQName* func;
  FbldExprV* argv;
} FbldAppExpr;

// FbldField --
//   A reference to a field.
//
// Fields:
//   name - The name of the field.
//   id - The fblc id of the field. This is set to FBLC_NULL_ID by the parser,
//        then later filled in during type check.
typedef struct {
  FbldName* name;
  FblcFieldId id;
} FbldField;

// FbldUnionExpr --
//   A union expression of the form 'type:field(arg)', used to construct a
//   union value.
typedef struct {
  FbldExpr _base;
  FbldQName* type;
  FbldField field;
  FbldExpr* arg;
} FbldUnionExpr;

// FbldAccessExpr --
//   An access expression of the form 'obj.field' used to access a field of
//   a struct or union value.
typedef struct {
  FbldExpr _base;
  FbldExpr* obj;
  FbldField field;
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
  FbldMTypeV* mtypes;
  FbldMDefnV* mdecls;
  FbldMDefnV* mdefns;
} FbldProgram;

// FbldLookupDecl --
//   Look up the qualified declaration with the given name in the given program.
//
// Inputs:
//   env - The collection of modules to look up the declaration in.
//   mdefn - An optional module to look the declaration up in before env.
//   entity - The name of the entity to look up.
//
// Returns:
//   The declaration with the given name, or NULL if no such declaration
//   could be found.
//
// Side effects:
//   Behavior is undefined if the module name of the entity has not been
//   resolved.
// FbldDecl* FbldLookupDecl(FbldModuleV* env, FbldMDefn* mdefn, FbldQName* entity);


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
FbldMType* FbldParseMDecl(FblcArena* arena, const char* filename);

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

// FbldLoadMDecl --
//   Load the module declaration for the module with the given name.
//   If the module declaration has already been loaded, it is returned as is.
//   Otherwise the module declaration and all of its dependencies are located
//   according to the given search path, parsed, checked, and added to the
//   collection of loaded modules before the requested module declaration is
//   returned.
//
// Inputs:
//   arena - The arena to use for allocating the parsed declaration.
//   path - A search path used to find the module declaration on disk.
//   name - The name of the module whose declaration to load.
//   mdeclv - A vector of module declarations that have already been
//            loaded using FbldLoadMDecl.
//
// Results:
//   The loaded module declaration, or NULL if the module declaration could
//   not be loaded.
//
// Side effects:
//   Read the module delacaration and any other required module delacarations
//   from disk and add them to the mdeclv vector.
//   Prints an error message to stderr if the module declaration or any other
//   required module declarations cannot be loaded, either because they cannot
//   be found on the path, fail to parse, or fail to check.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of all loaded declarations if there is no error.
// FbldMDecl* FbldLoadMDecl(FblcArena* arena, FbldStringV* path, const char* name, FbldMDeclV* mdeclv);

// FbldLoadMDefn --
//   Load the module definition for the module with the given name.
//   The module definition, its corresponding module declaration, and all of
//   the module declarations they depend on are located according to the given
//   search path, parsed, checked, and for the module declarations added to
//   the collection of loaded module declarations before the loaded module
//   definition is returned.
//
// Inputs:
//   arena - The arena to use for allocating the loaded definition and
//           declarations.
//   path - A search path used to find the module definitions and declaration
//          on disk.
//   name - The name of the module whose definition to load.
//   mdeclv - A vector of module declarations that have already been
//            loaded using FbldLoadMDecl or FbldLoadMDefn.
//
// Results:
//   The loaded module definition, or NULL if the module definition could
//   not be loaded.
//
// Side effects:
//   Read the module definition and any other required module delacarations
//   from disk and add the module declarations to the mdeclv vector.
//   Prints an error message to stderr if the module definition or any other
//   required module declarations cannot be loaded, either because they cannot
//   be found on the path, fail to parse, or fail to check.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the definition and all loaded declarations if there is no error.
// FbldMDefn* FbldLoadMDefn(FblcArena* arena, FbldStringV* path, const char* name, FbldMDeclV* mdeclv);

// FbldLoadModules --
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
//   mdeclv - A vector of module declarations that have already been
//            loaded using FbldLoadMDecl or FbldLoadMDefn or FbldLoadModule.
//   mdefnv - A vector of module definitions that have already been
//            loaded using FbldLoadMDecl or FbldLoadMDefn or FbldLoadModule.
//
// Results:
//   True on success, false on error.
//
// Side effects:
//   Reads required module delacarations and definitions from disk and adds
//   them to the mdeclv and mdefnv vectors.
//   Prints an error message to stderr if there is an error.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of all loaded declarations and definitions if there is no error.
// bool FbldLoadModules(FblcArena* arena, FbldStringV* path, const char* name, FbldMDeclV* mdeclv, FbldMDefnV* mdefnv);

// FbldCheckMDecl --
//   Check that the given module declaration is well formed and well typed.
//
// Inputs:
//   mdeclv - Already loaded and checked module declarations required by the
//            module declaration.
//   mdefn - The module declaration to check.
//
// Results:
//   true if the module declaration is well formed and well typed in the
//   environment of the given module declarations, false otherwise.
//
// Side effects:
//   If the module declaration is not well formed, an error message is
//   printed to stderr describing the problem with the module declaration.
// bool FbldCheckMDecl(FbldMDeclV* mdeclv, FbldMDecl* mdecl);

// FbldCheckMDefn --
//   Check that the given module definition is well formed, well typed, and
//   consistent with its own module declaration.
//
// Inputs:
//   mdeclv - Already loaded and checked module declarations required by the
//            module definition, including its own module declaration.
//   mdefn - The module definition to check.
//
// Results:
//   true if the module definition is well formed and well typed in the
//   environment of the given module declarations, false otherwise.
//
// Side effects:
//   If the module definition is not well formed, an error message is
//   printed to stderr describing the problem with the module definition.
// bool FbldCheckMDefn(FbldMDeclV* mdeclv, FbldMDefn* mdefn);

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

// FbldCompile --
//   Compile an fbld program to fblc.
//
// Inputs:
//   arena - Arena to use for allocating the fblc program.
//   accessv - collection of access expression locations.
//   mdefnv - Modules describing a valid fbld program.
//   entity - The name of the main entry to compile the program for.
//
// Result:
//   A complete fblc program for running the named entity.
//
// Side effects:
//   Updates accessv with the location of compiled access expressions.
//   The behavior is undefined if the fbld program is not a valid fbld
//   program.
// FblcDecl* FbldCompile(FblcArena* arena, FbldAccessLocV* accessv, FbldMDefnV* mdefnv, FbldQName* entity);

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
// FblcValue* FbldCompileValue(FblcArena* arena, FbldMDefnV* mdefnv, FbldValue* value);
#endif // FBLD_H_
