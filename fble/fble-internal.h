// fble-internal.h --
//   This header file describes the internal interface used in the
//   implementation of the fble library.

#ifndef FBLE_INTERNAL_H_
#define FBLE_INTERNAL_H_

#include "fble.h"

// FbleInstrTag --
//   Enum used to distinguish among different kinds of instructions.
typedef enum {
  FBLE_COMPOUND_INSTR,
  FBLE_STRUCT_VALUE_INSTR,
  FBLE_UNION_VALUE_INSTR,
  FBLE_STRUCT_ACCESS_INSTR,
  FBLE_UNION_ACCESS_INSTR,
  FBLE_COND_INSTR,

  FBLE_FUNC_VALUE_INSTR,
  FBLE_FUNC_APPLY_INSTR,
  FBLE_GET_INSTR,
  FBLE_PUT_INSTR,
  FBLE_EVAL_INSTR,
  FBLE_LINK_INSTR,
  FBLE_EXEC_INSTR,
  FBLE_PROC_INSTR,
  FBLE_VAR_INSTR,
  FBLE_LET_INSTR,
  FBLE_PUSH_INSTR,
  FBLE_POP_INSTR,
  FBLE_DATA_POP_INSTR,
  FBLE_JOIN_INSTR,
  FBLE_BREAK_CYCLE_INSTR,
} FbleInstrTag;

// FbleInstr --
//   Common base type for all instructions.
typedef struct {
  FbleInstrTag tag;
  int refcount;
} FbleInstr;

// FbleInstrV --
//   A vector of FbleInstr.
typedef struct {
  size_t size;
  FbleInstr** xs;
} FbleInstrV;

// FbleCompoundInstr -- FBLE_COMPOUND_INSTR
//   Execute a sequence of instructions in order.
//
// istack: ...
//     ==> ..., iN, ..., i2, i1
typedef struct {
  FbleInstr _base;
  FbleInstrV instrs;
} FbleCompoundInstr;

// FbleStructValueInstr -- FBLE_STRUCT_VALUE_INSTR
//   Allocate a struct value.
//
// dstack:  ..., a1, a2, ..., aN
//     ==>  ..., struct(a1, a2, ..., aN)
typedef struct {
  FbleInstr _base;
  size_t argc;
} FbleStructValueInstr;

// FbleUnionValueInstr -- FBLE_UNION_VALUE_INSTR
//   Allocate a union value.
//
// dstack: ..., arg
//     ==> ..., union(arg)
typedef struct {
  FbleInstr _base;
  size_t tag;
} FbleUnionValueInstr;

// FbleAccessInstr -- FBLE_STRUCT_ACCESS_INSTR or FBLE_UNION_ACCESS_INSTR
//   Access the tagged field from the object on top of the vstack.
//
// dstack: ..., obj
//     ==> ..., obj.tag
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  size_t tag;
} FbleAccessInstr;

// FbleCondInstr -- FBLE_COND_INSTR
//   Select the next thing to execute based on the tag of the value on top of
//   the value stack.
//
// dstack: ..., obj
//     ==> ...
//
// istack: ...
//     ==> ...,choices[obj.tag]
typedef struct {
  FbleInstr _base;
  FbleInstrV choices;
} FbleCondInstr;

// FbleFuncValueInstr -- FBLE_FUNC_VALUE_INSTR
//   Allocate a function, capturing the current variable context in the
//   process.
typedef struct {
  FbleInstr _base;
  size_t argc;
  FbleInstr* body;
} FbleFuncValueInstr;

// FbleFuncApplyInstr -- FBLE_FUNC_APPLY_INSTR
//   Given ..., x3, x2, x1, f on the top of the data stack,
//   apply f(x1, x2, ...).
typedef struct {
  FbleInstr _base;
  size_t argc;
} FbleFuncApplyInstr;

// FbleGetInstr -- FBLE_GET_INSTR
//   Allocate an FbleGetProcValue, taking the port from the stack.
typedef struct {
  FbleInstr _base;
} FbleGetInstr;

// FblePutInstr -- FBLE_PUT_INSTR
//   Given ..., arg, port on the top of the data stack.
//   Allocate an FblePutProcValue, taking the port and argument from the stack.
typedef struct {
  FbleInstr _base;
} FblePutInstr;

// FbleEvalInstr -- FBLE_EVAL_INSTR
//   Allocate an FbleEvalProcValue process, taking the result value from the
//   stack.
typedef struct {
  FbleInstr _base;
} FbleEvalInstr;

// FbleLinkInstr -- FBLE_LINK_INSTR
//   Allocate an FbleLinkProcValue, capturing the context in the process.
typedef struct {
  FbleInstr _base;
  FbleInstr* body;
} FbleLinkInstr;

// FbleExecInstr -- FBLE_EXEC_INSTR
//   Allocate an FbleExecProcValue, taking the binding argument values from
//   the stack.
//
// From the top of the stack down, we should find the binding arguments in
// reverse order.
typedef struct {
  FbleInstr _base;
  size_t argc;
  FbleInstr* body;
} FbleExecInstr;

// FbleJoinInstr -- FBLE_JOIN_INSTR
//   Move the top value of the data stack to the variable stack.
typedef struct {
  FbleInstr _base;
} FbleJoinInstr;

// FblePopInstr -- FBLE_POP_INSTR
//   Pop count values from the variable value stack.
typedef struct {
  FbleInstr _base;
  size_t count;
} FblePopInstr;

// FbleDataPopInstr -- FBLE_DATA_POP_INSTR
//   Given data stack ..., P1, V1
//   Release and pop P1, so that the data stack ends up: ..., V1
//
// TODO: Come up with a better name for this instruction.
typedef struct {
  FbleInstr _base;
} FbleDataPopInstr;

// FbleProcInstr -- FBLE_PROC_INSTR
//   Execute the process value on top of the value stack.
typedef struct {
  FbleInstr _base;
  FbleDataPopInstr pop;
} FbleProcInstr;

// FbleVarInstr -- FBLE_VAR_INSTR
//   Reads the variable at the given position in the stack. Position 0 is the
//   top of the stack.
typedef struct {
  FbleInstr _base;
  size_t position;
} FbleVarInstr;

// FbleBreakCycleInstr -- FBLE_BREAK_CYCLE_INSTR
//   The top count values on the variable stack should be ref values that whose
//   values have not yet been assigned.
//   The top count values on the data stack should be the values to assign to
//   those ref values on the variable stack.
typedef struct {
  FbleInstr _base;
  size_t count;
} FbleBreakCycleInstr;

// FbleLetInstr -- FBLE_LET_INSTR
//   Evaluate each of the bindings, add the results to the scope, then execute
//   the body.
typedef struct {
  FbleInstr _base;
  FbleInstrV bindings;
  FbleInstr* body;
  FblePopInstr pop;
  FbleBreakCycleInstr break_cycle;
} FbleLetInstr;

// FblePushInstr -- FBLE_PUSH_INSTR
//   Evaluate and push the given values on top of the (data) value stack and
//   execute the following instruction.
//   TODO: Rename this to FBLE_COMPOUND_INSTR once instructions implicitly
//   push their results to the value stack.
typedef struct {
  FbleInstr _base;
  FbleInstrV values;
  FbleInstr* next;
} FblePushInstr;

// FbleFreeInstrs --
//   Free the given sequence of instructions.
//
// Inputs:
//   arena - the arena used to allocation the instructions.
//   instrs - the instructions to free. May be NULL.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the instrs.
void FbleFreeInstrs(FbleArena* arena, FbleInstr* instrs);

// FbleCompile --
//   Type check and compile the given expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   expr - the expression to compile.
//
// Results:
//   The compiled expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression fails to compile. Allocates
//   memory for the instructions which must be freed with FbleFreeInstrs when
//   it is no longer needed.
FbleInstr* FbleCompile(FbleArena* arena, FbleExpr* expr);

// FbleVStack --
//   A stack of values.
typedef struct FbleVStack {
  FbleValue* value;
  struct FbleVStack* tail;
} FbleVStack;

// FbleFuncValue -- FBLE_FUNC_VALUE
//
// Fields:
//   context - The value stack at the time the function was created,
//             representing the lexical context available to the function.
//             Stored in reverse order of the standard value stack.
//   body - The instr representing the body of the function.
//   pop - An instruction that can be used to pop the arguments and the
//         context after a function is done executing.
//   dpop - An instruction that can be used to pop the function value after
//          the function is done executing.
struct FbleFuncValue {
  FbleValue _base;
  FbleVStack* context;
  FbleInstr* body;
  FblePopInstr pop;
  FbleDataPopInstr dpop;
};

// FbleProcValueTag --
//   A tag used to distinguish among different kinds of proc values.
typedef enum {
  FBLE_GET_PROC_VALUE,
  FBLE_PUT_PROC_VALUE,
  FBLE_EVAL_PROC_VALUE,
  FBLE_LINK_PROC_VALUE,
  FBLE_EXEC_PROC_VALUE,
} FbleProcValueTag;

// FbleProcValue -- FBLE_PROC_VALUE
//   A tagged union of proc value types. All values have the same initial
//   layout as FbleProcValue. The tag can be used to determine what kind of
//   proc value this is to get access to additional fields of the proc value
//   by first casting to that specific type of proc value.
struct FbleProcValue {
  FbleValue _base;
  FbleProcValueTag tag;
};

// FbleGetProcValue -- FBLE_GET_PROC_VALUE
typedef struct {
  FbleProcValue _base;
  FbleValue* port;
} FbleGetProcValue;

// FblePutProcValue -- FBLE_PUT_PROC_VALUE
typedef struct {
  FbleProcValue _base;
  FbleValue* port;
  FbleValue* arg;
} FblePutProcValue;

// FbleEvalProcValue -- FBLE_EVAL_PROC_VALUE
typedef struct {
  FbleProcValue _base;
  FbleValue* result;
} FbleEvalProcValue;

// FbleLinkProcValue -- FBLE_LINK_PROC_VALUE
typedef struct {
  FbleProcValue _base;
  FbleVStack* context;
  FbleInstr* body;
  FblePopInstr pop;
  FbleProcInstr proc;
} FbleLinkProcValue;

// FbleExecProcValue -- FBLE_EXEC_PROC_VALUE
typedef struct {
  FbleProcValue _base;
  FbleValueV bindings;
  FbleVStack* context;
  FbleInstr* body;
  FblePopInstr pop;
  FbleProcInstr proc;
  FbleJoinInstr join;
} FbleExecProcValue;

// FbleValues --
//   A non-circular singly linked list of values.
typedef struct FbleValues {
  FbleValue* value;
  struct FbleValues* next;
} FbleValues;

// FbleInputValue -- FBLE_INPUT_VALUE
//   Holds the list of values to get. Values are added to the tail and taken
//   from the head. If there are no values on the list, both head and tail are
//   set to NULL.
struct FbleInputValue {
  FbleValue _base;
  FbleValues* head;
  FbleValues* tail;
};

// FbleOutputValue -- FBLE_OUTPUT_VALUE
struct FbleOutputValue {
  FbleValue _base;
  FbleInputValue* dest;
};

#endif // FBLE_INTERNAL_H_
