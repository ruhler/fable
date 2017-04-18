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

// FbldModule --
//   An fbld module declaration or definition.
typedef struct {
  FbldNameL* name;
  FbldNameV* deps;
  FbldDeclV* decls;
} FbldModule;

// FbldMDecl --
//   A module declaration is an FbldModule with the bodies of functions and
//   processes set to NULL. We introduce a separate typedef to help document
//   when we expect this to be the case.
typedef FbldModule FbldMDecl;

// FbldMDefn --
//   A module declaration is an FbldModule with the bodies of functions and
//   processes defined. We introduce a separate typedef to help document when
//   we expect this to be the case.
typedef FbldModule FbldMDefn;


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

#endif // FBLD_H_

