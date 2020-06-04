// typecheck.h --
//   Header file for fble type checking.

#ifndef FBLE_INTERNAL_TYPECHECK_H_
#define FBLE_INTERNAL_TYPECHECK_H_

#include "fble-alloc.h"
#include "syntax.h"
#include "type.h"

// FbleTcTag --
//   A tag used to distinguish among different kinds of type checked
//   expressions.
typedef enum {
  FBLE_TYPE_TC,
  FBLE_VAR_TC,
  FBLE_LET_TC,

  FBLE_STRUCT_VALUE_TC,
  FBLE_STRUCT_ACCESS_TC,

  FBLE_UNION_VALUE_TC,
  FBLE_UNION_ACCESS_TC,
  FBLE_UNION_SELECT_TC,

  FBLE_FUNC_VALUE_TC,
  FBLE_FUNC_APPLY_TC,

  FBLE_EVAL_TC,
  FBLE_LINK_TC,
  FBLE_EXEC_TC,

  FBLE_POLY_VALUE_TC,
  FBLE_POLY_APPLY_TC,

  FBLE_PROFILE_TC,
} FbleTcTag;

// FbleTc --
//   A tagged union of type checked expression types. All tcs have the same
//   initial layout as FbleTc. The tag can be used to determine what kind of
//   tc this is to get access to additional fields of the tc by first
//   casting to that specific type of tc.
typedef struct {
  FbleTcTag tag;
  FbleLoc loc;
} FbleTc;

// FbleTcV --
//   A vector of FbleTc.
typedef struct {
  size_t size;
  FbleTc** xs;
} FbleTcV;

// FbleTypeTc --
//   FBLE_TYPE_TC
//
// Represents the type value. Used for struct, union, func, proc and all other
// kinds of types.
typedef struct {
  FbleTc _base;
} FbleTypeTc;

// FbleVarSource
//   Where to find a variable.
//
// FBLE_LOCAL_VAR is a local variable.
// FBLE_STATIC_VAR is a variable captured from the parent scope.
typedef enum {
  FBLE_LOCAL_VAR,
  FBLE_STATIC_VAR
} FbleVarSource;

// FbleVarIndex --
//   Identifies a variable in scope.
typedef struct {
  FbleVarSource source;
  size_t index;
} FbleVarIndex;

// FbleVarIndexV --
//   A vector of FbleVarIndex.
typedef struct {
  size_t size;
  FbleVarIndex* xs
} FbleVarIndexV;

// FbleVarTc --
//   FBLE_VAR_TC
//
typedef struct {
  FbleTc _base;
  FbleVarIndex index;
} FbleVarTc;

// FbleLetTc --
//   FBLE_LET_TC
//
// recursive is false if the let is a non-recursive let expression.
typedef struct {
  FbleTc _base;
  bool recursive;
  FbleTcV bindings;
  FbleTc* body;
} FbleLetTc;

// FbleStructValueTc --
//   FBLE_STRUCT_VALUE_TC
//
// An expression to allocate a new struct value.
typedef struct {
  FbleTc _base;
  FbleTcV args;
} FbleStructValueTc;

// FbleAccessTc --
//   FBLE_STRUCT_ACCESS_TC
//   FBLE_UNION_ACCESS_TC
typedef struct {
  FbleTc _base;
  FbleTc* obj;
  size_t tag;
} FbleStructAccessTc;

// FbleUnionValueTc --
//   FBLE_UNION_VALUE_TC
typedef struct {
  FbleTc _base;
  FbleTc* arg;
  size_t tag;
} FbleUnionValueTc;

// FbleUnionSelectTc --
//   FBLE_UNION_SELECT_TC
typedef struct {
  FbleTc _base;
  FbleTc* condition;
  FbleTcV choices;
} FbleUnionSelectTc;

// FbleFuncValueTc --
//   FBLE_FUNC_VALUE_TC
typedef struct {
  FbleTc _base;
  FbleVarIndexV scope;
  size_t argc;
  FbleTc* body;
} FbleFuncValueTc;

// FbleFuncApplyTc --
//   FBLE_FUNC_APPLY_TC
typedef struct {
  FbleTc _base;
  FbleTc* func;
  FbleTcV args;
} FbleFuncApplyTc;

// FbleEvalTc --
//   FBLE_EVAL_TC
typedef struct {
  FbleTc _base;
  FbleTc* body;
} FbleEvalTc;

// FbleLinkTc --
//   FBLE_LINK_TC
typedef struct {
  FbleTc _base;
  FbleTc* body;
} FbleLinkTc;

// FbleExecTc --
//   FBLE_EXEC_TC
typedef struct {
  FbleTc _base;
  FbleTcV bindings;
  FbleTc* body;
} FbleExecTc;

// FblePolyValueTc --
//   FBLE_POLY_VALUE_TC
typedef struct {
  FbleTc _base;
  FbleTc* body;
} FblePolyValueTc;

// FblePolyApplyTc --
//   FBLE_POLY_APPLY_TC
typedef struct {
  FbleTc _base;
  FbleTc* poly;
} FblePolyApplyTc;

// FbleProfileTc --
//   FBLE_PROFILE_TC
//
// FbleProfileTc is used to denote a profiling block.
//
// Fields:
//   name - the name of the block for profiling purposes.
//   body - the body of the profile block.
typedef struct {
  FbleTc _base;
  FbleName name;
  FbleTc* body;
} FbleProfileTc;

// FbleTypeCheck -- 
//   Run typecheck on the given program.
//
// Inputs:
//   arena - arena to use for allocations
//   program - the program to typecheck
//
// Result:
//   The type checked program, or NULL if the program failed to type check.
//
// Side effects:
// * Prints messages to stderr in case of failure to type check.
FbleTc* FbleTypeCheck(FbleArena* arena, FbleProgram* program);

#endif // FBLE_INTERNAL_TYPECHECK_H_
