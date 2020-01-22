
#ifndef FBLE_TYPE_H_
#define FBLE_TYPE_H_

#include "fble-syntax.h"
#include "ref.h"

typedef FbleRefArena TypeArena;

// KindTag --
//   A tag used to distinguish between the two kinds of kinds.
typedef enum {
  BASIC_KIND,
  POLY_KIND
} KindTag;

// Kind --
//   A tagged union of kind types. All kinds have the same initial
//   layout as Kind. The tag can be used to determine what kind of
//   kind this is to get access to additional fields of the kind
//   by first casting to that specific type of kind.
typedef struct {
  KindTag tag;
  FbleLoc loc;
  int refcount;
} Kind;

// KindV --
//   A vector of Kind.
typedef struct {
  size_t size;
  Kind** xs;
} KindV;

// BasicKind --
//   BASIC_KIND (level :: size_t)
//
// levels
//   0: A normal, non-type value.
//   1: A normal type. A type of a level 0.
//   2: A type of a type of a value.
//   3: A type of a type of a type of a value.
//   etc.
typedef struct {
  Kind _base;
  size_t level;
} BasicKind;

// PolyKind --
//   POLY_KIND (arg :: Kind) (return :: Kind)
typedef struct {
  Kind _base;
  Kind* arg;
  Kind* rkind;
} PolyKind;

// TypeTag --
//   A tag used to dinstinguish among different kinds of compiled types.
typedef enum {
  STRUCT_TYPE,
  UNION_TYPE,
  FUNC_TYPE,
  PROC_TYPE,
  POLY_TYPE,
  POLY_APPLY_TYPE,
  VAR_TYPE,
  TYPE_TYPE,
} TypeTag;

// Type --
//   A tagged union of type types. All types have the same initial
//   layout as Type. The tag can be used to determine what kind of
//   type this is to get access to additional fields of the type
//   by first casting to that specific type of type.
//
//   The evaluating field is set to true if the type is currently being
//   evaluated. See the Eval function for more info.
//
// Design notes on types:
// * Instances of Type represent both unevaluated and evaluated versions of
//   the type. We use the unevaluated versions of the type when printing error
//   messages and as a stable reference to a type before and after evaluation.
// * Cycles are allowed in the Type data structure, to represent recursive
//   types. Every cycle is guaranteed to go through a Var type.
// * Types are evaluated as they are constructed.
// * TYPE_TYPE is handled specially: we propagate TYPE_TYPE up to the top of
//   the type during construction rather than save the unevaluated version of
//   a typeof.
typedef struct Type {
  FbleRef ref;
  TypeTag tag;
  FbleLoc loc;
  bool evaluating;
} Type;

// TypeV --
//   A vector of Type.
typedef struct {
  size_t size;
  Type** xs;
} TypeV;

// Field --
//   A pair of (Type, Name) used to describe type and function arguments.
typedef struct {
  Type* type;
  FbleName name;
} Field;

// FieldV --
//   A vector of Field.
typedef struct {
  size_t size;
  Field* xs;
} FieldV;

// StructType -- STRUCT_TYPE
typedef struct {
  Type _base;
  FieldV fields;
} StructType;

// UnionType -- UNION_TYPE
typedef struct {
  Type _base;
  FieldV fields;
} UnionType;

// FuncType -- FUNC_TYPE
typedef struct {
  Type _base;
  Type* arg;
  struct Type* rtype;
} FuncType;

// ProcType -- PROC_TYPE
typedef struct {
  Type _base;
  Type* type;
} ProcType;

// PolyType -- POLY_TYPE
//
// We maintain an invariant when constructing PolyTypes that the body is not a
// TYPE_TYPE. For example: \a -> typeof(a) is constructed as typeof(\a -> a)
typedef struct {
  Type _base;
  Type* arg;
  Type* body;
} PolyType;

// PolyApplyType -- POLY_APPLY_TYPE
//   The 'result' field is the result of evaluating the poly apply type, or
//   NULL if it has not yet been evaluated.
//
// We maintain an invariant when constructing PolyApplyTypes that the poly is
// not a TYPE_TYPE. For example: (typeof(f) x) is constructed as typeof(f x).
typedef struct {
  Type _base;
  Type* poly;
  Type* arg;
  Type* result;
} PolyApplyType;

// VarType --
//   VAR_TYPE
//
// A variable type whose value may or may not be known. Used for the value of
// type paramaters and recursive type values.
typedef struct VarType {
  Type _base;
  Kind* kind;
  FbleName name;
  Type* value;
} VarType;

// VarTypeV - a vector of var types.
typedef struct {
  size_t size;
  VarType** xs;
} VarTypeV;

// TypeType --
//   TYPE_TYPE
//
// The type of a type.
typedef struct TypeType {
  Type _base;
  Type* type;
} TypeType;

// FreeKind --
//   Frees a (refcount) copy of a compiled kind.
//
// Inputs:
//   arena - for deallocations.
//   kind - the kind to free. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Decrements the refcount for the kind and frees it if there are no
//   more references to it.
void FreeKind(FbleArena* arena, Kind* kind);

// TypeFree --
//   The free function for types. See documentation in ref.h
void TypeFree(TypeArena* arena, FbleRef* ref);

#endif // FBLE_TYPE_H_
