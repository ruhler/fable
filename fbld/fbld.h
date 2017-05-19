// fbld.h --
//   This header file describes a representation and utilies for working with
//   fbld programs.

#ifndef FBLD_H_
#define FBLD_H_

#include "fblc.h"   // for FblcArena

// FbldName --
//   A type used for unqualified names in fbld programs.
typedef const char* FbldName;

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

// FbldNameL -- 
//   A name along with its associated location in a source file. The location
//   is typically used for error reporting purposes.
typedef struct {
  FbldName name;
  FbldLoc* loc;
} FbldNameL;

// FbldQualifiedName --
//   A qualified fbld name of the form 'Foo@bar'. The module component of the
//   name may be NULL to indicate the name is not explicitly qualified.
typedef struct {
  FbldNameL* module;
  FbldNameL* name;
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

// FbldAppExpr --
//   An application expression of the form 'func(arg0, arg1, ...)'. func may
//   refer to a function or a struct type.
typedef struct {
  FbldExpr _base;
  FbldQualifiedName* func;
  FbldExprV* argv;
} FbldAppExpr;

// FbldDeclTag --
//   Tag used to distinguish amongs different kinds of fbld declarations.
typedef enum {
  FBLD_IMPORT_DECL,
  FBLD_TYPE_DECL,
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

// FbldStructDecl --
//   A declaration of a struct type.
typedef struct {
  FbldDecl _base;
  FbldTypedNameV* fieldv;
} FbldStructDecl;

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

// FbldMDecl --
//   An fbld module declaration.
typedef struct {
  FbldNameL* name;
  FbldNameV* deps;
  FbldDeclV* decls;
} FbldMDecl;

// FbldMDeclV --
//   A vector of fbld module declarations.
typedef struct {
  size_t size;
  FbldMDecl** xs;
} FbldMDeclV;

// FbldDefn --
//   A tagged union of fbdl definitions. All defns have the same initial
//   layout as FbldDefn. The tag from the decl field can be used to determine
//   what kind of defn this is to get access to additional fields of the defn
//   by first casting to that specific type.
typedef struct {
  FbldDecl* decl;
} FbldDefn;

// FbldStructDefn --
//   A definition of a struct type.
typedef struct {
  FbldStructDecl* decl;
} FbldStructDefn;

// FbldFuncDefn --
//   A definition of a function.
typedef struct {
  FbldFuncDecl* decl;
  FbldExpr* body;
} FbldFuncDefn;

// FbldDefnV --
//   A vector of FbldDefns.
typedef struct {
  size_t size;
  FbldDefn** xs;
} FbldDefnV;

// FbldMDefn --
//   An fbld module definition.
typedef struct {
  FbldNameL* name;
  FbldNameV* deps;
  FbldDefnV* defns;
} FbldMDefn;

// FbldMDefnV --
//   A vector of FbldMDefns.
typedef struct {
  size_t size;
  FbldMDefn** xs;
} FbldMDefnV;

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

// FbldCompile --
//   Compile an fbld program to fblc.
//
// Inputs:
//   arena - Arena to use for allocating the fblc program.
//   mdefnv - Modules describing a valid fbld program.
//   entity - The name of the main entry to compile the program for.
//
// Result:
//   A complete fblc program for running the named entity.
//
// Side effects:
//   The behavior is undefined if the fbld program is not a valid fbld
//   program.
FblcDecl* FbldCompile(FblcArena* arena, FbldMDefnV* mdefnv, FbldQualifiedName* entity);
#endif // FBLD_H_

