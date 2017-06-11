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

// FbldName --
//   A type used for unqualified names in fbld programs.
typedef const char* FbldName;

// FbldNameL -- 
//   A name along with its associated location in a source file. The location
//   is typically used for error reporting purposes.
typedef struct {
  FbldName name;
  FbldLoc* loc;
} FbldNameL;

// FbldQualifiedName --
//   A qualified fbld name of the form 'bar@Foo'. The module->name component
//   of the name may be NULL to indicate the name is not explicitly qualified.
typedef struct {
  FbldNameL* name;
  FbldNameL* module;
} FbldQualifiedName;

// FbldTypedName --
//   An fbld name with associated (possibly qualified) type.
typedef struct {
  FbldQualifiedName* type;
  FbldNameL* name;
} FbldTypedName;

typedef struct {
  size_t size;
  FbldTypedName** xs;
} FbldTypedNameV;

// FbldNameV --
//   A vector of fbld names.
typedef struct {
  size_t size;
  FbldNameL** xs;
} FbldNameV;

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
bool FbldNamesEqual(FbldName a, FbldName b);

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

// FblcVarExpr --
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
  FbldNameL* var;
  FblcVarId id;
} FbldVarExpr;

// FbldAppExpr --
//   An application expression of the form 'func(arg0, arg1, ...)'. func may
//   refer to a function or a struct type.
typedef struct {
  FbldExpr _base;
  FbldQualifiedName* func;
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
  FbldNameL* name;
  FblcFieldId id;
} FbldField;

// FbldUnionExpr --
//   A union expression of the form 'type:field(arg)', used to construct a
//   union value.
typedef struct {
  FbldExpr _base;
  FbldQualifiedName* type;
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

// FblcCondExpr --
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
  FbldQualifiedName* type;
  FbldNameL* var;
  FbldExpr* def;
  FbldExpr* body;
} FbldLetExpr;

// FbldDeclTag --
//   Tag used to distinguish amongs different kinds of fbld declarations.
typedef enum {
  FBLD_IMPORT_DECL,
  FBLD_ABSTRACT_TYPE_DECL,
  FBLD_UNION_DECL,
  FBLD_STRUCT_DECL,
  FBLD_FUNC_DECL,
  FBLD_PROC_DECL
} FbldDeclTag;

// FbldDecl --
//   A tagged union of fbdl declarations. All decls have the same initial
//   layout as FbldDecl. The tag can be used to determine what kind of decl
//   this is to get access to additional fields of the decl by first casting
//   to that specific type.
typedef struct {
  FbldDeclTag tag;
  FbldNameL* name;
} FbldDecl;

// FbldImportDecl --
//   An import declaration of the form 'import <module>(<name1>, <name2>, ...)'
typedef struct {
  FbldDecl _base;
  FbldNameV* namev;
} FbldImportDecl;

// FbldAbstractTypeDecl --
//   A declaration of an abstract type.
typedef FbldDecl FbldAbstractTypeDecl;

// FbldConcreteTypeDecl --
//   A declaration of a struct or union type.
typedef struct {
  FbldDecl _base;
  FbldTypedNameV* fieldv;
} FbldConcreteTypeDecl;

// FbldStructDecl --
//   A declaration of a struct type.
typedef FbldConcreteTypeDecl FbldStructDecl;

// FbldUnionDecl --
//   A declaration of a union type.
typedef FbldConcreteTypeDecl FbldUnionDecl;

// FbldFuncDecl --
//   A declaration of a function.
//   The body is NULL for function prototypes specified in module
//   declarations.
typedef struct {
  FbldDecl _base;
  FbldTypedNameV* argv;
  FbldQualifiedName* return_type;
  FbldExpr* body;
} FbldFuncDecl;

// FbldDeclV --
//   A vector of FbldDecls.
typedef struct {
  size_t size;
  FbldDecl** xs;
} FbldDeclV;

// FbldModule --
//   An fbld module declaration or definition.
typedef struct {
  FbldNameL* name;
  FbldNameV* deps;
  FbldDeclV* declv;
} FbldModule;

// FbldModuleV --
//   A vector of fbld module declarations or definitions.
typedef struct {
  size_t size;
  FbldModule** xs;
} FbldModuleV;

// FbldMDecl --
//   An fbld module declaration.
//   FbldMDecls may contain abstract type declarations and have the bodies of
//   function and process declarations set to NULL.
typedef FbldModule FbldMDecl;

// FbldModuleV --
//   A vector of fbld module declarations.
typedef FbldModuleV FbldMDeclV;

// FbldMDefn --
//   An fbld module definition.
//   FbldmDefns do not contain abstract type declarations and have non-NULL
//   bodies of functions and processes.
typedef FbldModule FbldMDefn;

// FbldMDefnV --
//   A vector of FbldMDefns.
typedef FbldModuleV FbldMDefnV;

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
FbldDecl* FbldLookupDecl(FbldModuleV* env, FbldMDefn* mdefn, FbldQualifiedName* entity);

// FbldKind --
//   An enum used to distinguish between struct and union values.
typedef enum {
  FBLD_STRUCT_KIND,
  FBLD_UNION_KIND
} FbldKind;

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
  FbldQualifiedName* type;
  FbldNameL* tag;
  FbldValueV* fieldv;
};

// FbldParseMDecl --
//   Parse the module declaration from the file with the given filename.
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
FbldMDecl* FbldParseMDecl(FblcArena* arena, const char* filename);

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
FbldMDecl* FbldLoadMDecl(FblcArena* arena, FbldStringV* path, FbldName name, FbldMDeclV* mdeclv);

// FbldLoadMDefn --
//   Load the module definition for the module with the given name.
//   The module definition and all of the module declarations it depends on
//   are located according to the given search path, parsed, checked, and
//   for the module declarations added to the collection of loaded module
//   declarations before the loaded module definition is returned.
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
FbldMDefn* FbldLoadMDefn(FblcArena* arena, FbldStringV* path, FbldName name, FbldMDeclV* mdeclv);

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
bool FbldLoadModules(FblcArena* arena, FbldStringV* path, FbldName name, FbldMDeclV* mdeclv, FbldMDefnV* mdefnv);

// FbldCheckMDefn --
//   Check that the given module definition is well formed and well typed.
//
// Inputs:
//   mdeclv - Already loaded and checked module declarations required by the
//            module definition.
//   mdefn - The module definition to check.
//
// Results:
//   true if the module definition is well formed and well typed in the
//   environment of the given module declarations, false otherwise.
//
// Side effects:
//   If the module definition is not well formed, an error message is
//   printed to stderr describing the problem with the module definition.
bool FbldCheckMDefn(FbldMDeclV* mdeclv, FbldMDefn* mdefn);

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
FblcDecl* FbldCompile(FblcArena* arena, FbldAccessLocV* accessv, FbldMDefnV* mdefnv, FbldQualifiedName* entity);

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
FblcValue* FbldCompileValue(FblcArena* arena, FbldMDefnV* mdefnv, FbldValue* value);
#endif // FBLD_H_

