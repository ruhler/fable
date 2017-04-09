// fbld.h --
//   This header file describes a representation and utilies for working with
//   fbld programs.

#ifndef FBLD_H_
#define FBLD_H_

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
  FbldTypedName* xs;
} FbldTypedNameV;

// FbldNameV --
//   A vector of fbld names.
typedef struct {
  size_t size;
  FbldNameL* xs;
} FbldNameLV;

// FbldDeclItemTag --
//   Tag used to distinguish amongs different kinds of fbld module declaration
//   items.
typedef enum {
  FBLD_IMPORT_DECL_ITEM,
  FBLD_TYPE_DECL_ITEM,
  FBLD_UNION_DECL_ITEM,
  FBLD_STRUCT_DECL_ITEM,
  FBLD_FUNC_DECL_ITEM,
  FBLD_PROC_DECL_ITEM
} FbldDeclItemTag;

// FbldDeclItem --
//   A tagged union of fbdl module declaration items. All decl items have the
//   same initial layout as FbldDeclItem. The tag can be used to determine
//   what kind of decl item this is to get access to additional fields of the
//   decl item by first casting to that specific type.
typedef struct {
  FbldDeclItemTag tag;
  FbldNameL name;
} FblcDeclItem;

// FbldStructDeclItem --
//   A declaration of a struct in a module declaration.
typedef struct {
  FbldDeclItem _base;
  FbldTypedNameV fieldv;
} FbldStructDeclItem;

// FbldFuncDeclItem --
//   A declaration of a function in a module declaration.
typedef struct {
  FbldDeclItem _base;
  FbldTypedNameV argv;
  FbldQualifiedName return_type;
} FbldFuncDeclItem;

// FbldMDecl --
//   An fbld module declaration.
typedef struct {
  FbldNameL name;
  FbldNameLV deps;
  FbldDeclItemV items;
} FbldMDecl;

FbldMDecl* FbldParseMDecl(

#endif // FBLD_H_

