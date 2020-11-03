// tc.h --
//   Header file defining the FbleTc type, which is used as the implementation
//   for FbleValue.
//   This is an internal library interface.

#ifndef FBLE_INTERNAL_TC_H_
#define FBLE_INTERNAL_TC_H_

#include "fble-value.h"     // for FbleValueV
#include "instr.h"

// FbleTc --
//   An already type-checked representation of an fble value or syntactic
//   expression.
//
// FbleTc is like FbleExpr, except that:
// * Field and variable names are replaced with integer indicies.
// * Types are eliminated.
// * Processes are treated as zero argument functions.
// * There is no difference between a function context and a process context.
//   In particular, LINK_TC and EXEC_TC represent the computation that returns
//   the result of running the link and exec processes, rather than a
//   computation that creates link and exec process values.
//
// FbleTc is like FbleValue, except that:
// * It can represent expressions like union select and function application,
//   for the purpose of describing values that have not yet been computed and
//   for describing symbolic values.
//
// In reality FbleTc is used as the underlying implementation of the FbleValue
// type, though in theory external users shouldn't know or care about that.
typedef struct FbleValue FbleTc;

// FbleTcV --
//   A vector of FbleTc.
typedef FbleValueV FbleTcV;

// FbleTcTag --
//   A tag used to distinguish among different kinds of FbleTc.
typedef enum {
  FBLE_TYPE_VALUE_TC,
  FBLE_VAR_TC,
  FBLE_LET_TC,

  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_UNION_SELECT_VALUE,
  FBLE_DATA_ACCESS_VALUE,

  FBLE_FUNC_VALUE_TC,
  FBLE_FUNC_VALUE,
  FBLE_FUNC_APPLY_TC,

  FBLE_LINK_VALUE,
  FBLE_PORT_VALUE,
  FBLE_LINK_TC,
  FBLE_EXEC_TC,

  FBLE_SYMBOLIC_VALUE_TC,
  FBLE_SYMBOLIC_COMPILE_TC,

  FBLE_PROFILE_TC,

  FBLE_REF_VALUE,
} FbleTcTag;

// FbleValue --
//   A tagged union of value types. All values have the same initial
//   layout as FbleValue. The tag can be used to determine what kind of
//   value this is to get access to additional fields of the value
//   by first casting to that specific type of value.
struct FbleValue {
  FbleTcTag tag;
};

// FbleTypeValueTc --
//   FBLE_TYPE_VALUE_TC
//
// Represents the type value. Because types are compile-time concepts, not
// runtime concepts, the type value contains no information.
typedef struct {
  FbleTc _base;
} FbleTypeValueTc;

// FbleVarSource
//   Where to find a variable.
//
// FBLE_LOCAL_VAR is a local variable.
// FBLE_STATIC_VAR is a variable captured from the parent scope.
// FBLE_FREE_VAR is a unbound var introduced for symbolic elaboration.
typedef enum {
  FBLE_LOCAL_VAR,
  FBLE_STATIC_VAR,
  FBLE_FREE_VAR,
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
//
// A variable expression.
// * Used to represent variables refering to function arguments or local
//   variables. 
// * Used to represent pure symbolic values for symbolic elaboration.
typedef struct {
  FbleTc _base;
  FbleVarIndex index;
} FbleVarTc;

// FbleLetTc --
//   FBLE_LET_TC
//
// Represents a let expression.
//
// The bindings are bound to variables implicitly based on the position of the
// binding in the let expression and the position of the let expression in its
// parent expression as specified for FbleVarIndex.
//
// Fields:
//   recursive - false if the let is a non-recursive let expression.
typedef struct {
  FbleTc _base;
  bool recursive;
  FbleTcV bindings;
  FbleTc* body;
} FbleLetTc;

// FbleStructValue --
//   FBLE_STRUCT_VALUE
typedef struct {
  FbleTc _base;
  size_t fieldc;
  FbleTc* fields[];
} FbleStructValue;

// FbleUnionValue --
//   FBLE_UNION_VALUE
typedef struct {
  FbleTc _base;
  size_t tag;
  FbleTc* arg;
} FbleUnionValue;

// FbleFuncValue -- FBLE_FUNC_VALUE
//
// Fields:
//   argc - The number of arguments expected by the function.
//   code - The code for the function.
//   scope - The scope at the time the function was created, representing the
//           lexical context available to the function. The length of this
//           array is code->statics.
//
// Note: Function values are used for both pure functions and processes. We
// don't distinguish between the two at runtime, except that argc == 0
// suggests this is for a process instead of a function.
typedef struct {
  FbleTc _base;
  size_t argc;
  FbleInstrBlock* code;
  FbleValue* scope[];
} FbleFuncValue;

// FbleProcValue -- FBLE_PROC_VALUE
//   A proc value is represented as a function that takes no arguments.
#define FBLE_PROC_VALUE FBLE_FUNC_VALUE
typedef FbleFuncValue FbleProcValue;

// FbleValues --
//   A non-circular singly linked list of values.
typedef struct FbleValues {
  FbleValue* value;
  struct FbleValues* next;
} FbleValues;

// FbleLinkValue -- FBLE_LINK_VALUE
//   Holds the list of values on a link. Values are added to the tail and taken
//   from the head. If there are no values on the list, both head and tail are
//   set to NULL.
typedef struct {
  FbleTc _base;
  FbleValues* head;
  FbleValues* tail;
} FbleLinkValue;

// FblePortValue --
//   FBLE_PORT_VALUE
//
// Use for input and output values linked to external IO.
//
// data is a pointer to a value owned externally where data should be put to
// and got from.
typedef struct {
  FbleTc _base;
  FbleValue** data;
} FblePortValue;

// FbleRefValue --
//   FBLE_REF_VALUE
//
// A implementation-specific value introduced to support recursive values. A
// ref value is simply a reference to another value. All values must be
// dereferenced before being otherwise accessed in case they are reference
// values.
//
// Fields:
//   value - the value being referenced, or NULL if no value is referenced.
typedef struct FbleRefValue {
  FbleTc _base;
  FbleValue* value;
} FbleRefValue;

// FbleDataAccessValue --
//   FBLE_DATA_ACCESS_VALUE
typedef struct {
  FbleTc _base;
  FbleDataTypeTag datatype;
  FbleTc* obj;
  size_t tag;
  FbleLoc loc;
} FbleDataAccessValue;

// FbleUnionSelectValue --
//   FBLE_UNION_SELECT_VALUE
//
// Because of default branches in union select, it is possible that multiple
// choices point to the same value. Code generation is expected to check for
// that and avoid generating duplicate code.
typedef struct {
  FbleTc _base;
  FbleLoc loc;
  FbleTc* condition;
  size_t choicec;
  FbleTc* choices[];
} FbleUnionSelectValue;

// FbleProfileTc --
//   FBLE_PROFILE_TC
//
// FbleProfileTc is used to denote a profiling block.
//
// Fields:
//   name - the name of the block for profiling purposes.
//   loc - the location of the profile block.
//   body - the body of the profile block.
//
// The location of the profiling block is passed through loc, not name.loc.
typedef struct {
  FbleTc _base;
  FbleName name;
  FbleLoc loc;
  FbleTc* body;
} FbleProfileTc;

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
  FbleValueV bindings;
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
  FbleLoc loc;
  FbleVarIndexV args;
  FbleTc* body;
} FbleSymbolicCompileTc;

// FbleFuncValueTc --
//   FBLE_FUNC_VALUE_TC
//
// Note: FuncValueTc is used for process values as well as function values.
typedef struct {
  FbleTc _base;
  FbleLoc body_loc;
  FbleVarIndexV scope;
  size_t argc;
  FbleTc* body;
} FbleFuncValueTc;

// FbleFuncApplyTc --
//   FBLE_FUNC_APPLY_TC
typedef struct {
  FbleTc _base;
  FbleLoc loc;
  FbleTc* func;
  FbleTcV args;
} FbleFuncApplyTc;

#endif // FBLE_INTERNAL_TC_H_
