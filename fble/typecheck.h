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
//
// FbleTc is like FbleExpr, except that:
// * Field and variable names are replaced with integer indicies.
// * Types are eliminated.
// * Processes are treated as zero argument functions.
// * There is no difference between a function context and a process context.
//   In particular, LINK_TC and EXEC_TC represent the computation that returns
//   the result of running the link and exec processes, rather than a
//   computation that creates link and exec process values.
typedef enum {
  FBLE_TYPE_TC,
  FBLE_VAR_TC,
  FBLE_LET_TC,
  FBLE_DATA_ACCESS_TC,
  FBLE_STRUCT_VALUE_TC,
  FBLE_UNION_VALUE_TC,
  FBLE_UNION_SELECT_TC,
  FBLE_FUNC_VALUE_TC,
  FBLE_FUNC_APPLY_TC,
  FBLE_LINK_TC,
  FBLE_EXEC_TC,
  FBLE_SYMBOLIC_VALUE_TC,
  FBLE_SYMBOLIC_COMPILE_TC,
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
//
// For local variables, index starts at 0 for the first argument to a
// function. The index increases by one for each new variable introduced,
// going from left to right, outer most to inner most binding.
typedef struct {
  FbleVarSource source;
  size_t index;
} FbleVarIndex;

// FbleVarIndexV --
//   A vector of FbleVarIndex.
typedef struct {
  size_t size;
  FbleVarIndex* xs;
} FbleVarIndexV;

// FbleVarTc --
//   FBLE_VAR_TC
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

// FbleDataAccessTc --
//   FBLE_DATA_ACCESS_TC
//
// Used for struct and union field access.
typedef struct {
  FbleTc _base;
  FbleDataTypeTag datatype;
  FbleTc* obj;
  size_t tag;
  FbleLoc loc;    // Location to use for reporting undefined access.
} FbleDataAccessTc;

// FbleUnionValueTc --
//   FBLE_UNION_VALUE_TC
typedef struct {
  FbleTc _base;
  FbleTc* arg;
  size_t tag;
} FbleUnionValueTc;

// FbleUnionSelectTc --
//   FBLE_UNION_SELECT_TC
//
// Because of default branches in union select, the number of possible
// branches to execute may be fewer than the number of fields in the
// union type.
//
// branches.xs[choices.xs[i]] is the branch to execute when the value of the
// union has tag i.
typedef struct {
  FbleTc _base;
  FbleTc* condition;
  struct { size_t size; size_t* xs; } choices;
  FbleTcV branches;
} FbleUnionSelectTc;

// FbleFuncValueTc --
//   FBLE_FUNC_VALUE_TC
//
// Note: FuncValueTc is used for process values as well as function values.
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

// FbleLinkTc --
//   FBLE_LINK_TC
//
// Unlike FBLE_LINK_EXPR, which evaluates to a proc value, FBLE_LINK_TC
// evaluates to the result of the computing the proc value.
typedef struct {
  FbleTc _base;
  FbleTc* body;
} FbleLinkTc;

// FbleExecTc --
//   FBLE_EXEC_TC
//
// Unlike FBLE_EXEC_EXPR, which evaluates to a proc value, FBLE_EXEC_TC
// evaluates to the result of the computing the proc value.
typedef struct {
  FbleTc _base;
  FbleTcV bindings;
  FbleTc* body;
} FbleExecTc;

// FbleSymbolicValueTc --
//   FBLE_SYMBOLIC_VALUE_TC
//
// An expression to allocate a new symbolic value.
typedef struct {
  FbleTc _base;
} FbleSymbolicValueTc;

// FbleSymbolicCompileTc --
//   FBLE_SYMBOLIC_COMPILE_TC 
//
// An expression to compile a symbolic value into a function.
typedef struct {
  FbleTc _base;
  FbleVarIndexV args;
  FbleTc* body;
} FbleSymbolicCompileTc;

// FbleProfileTc --
//   FBLE_PROFILE_TC
//
// FbleProfileTc is used to denote a profiling block.
//
// Fields:
//   name - the name of the block for profiling purposes.
//   body - the body of the profile block.
//
// The location of the profiling block is based through _base.loc, not
// name.loc.
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
FbleTc* FbleTypeCheck(FbleArena* arena, struct FbleProgram* program);

// FbleFreeTc --
//   Free resources assocatied with an FbleTc.
// 
// Inputs:
//   arena - arena to use for allocations
//   tc - the type checked expression to free
//
// Side effects:
//   Frees resources associated with the given expression.
void FbleFreeTc(FbleArena* arena, FbleTc* tc);

#endif // FBLE_INTERNAL_TYPECHECK_H_
