// fble-internal.h --
//   This header file describes the internal interface used in the
//   implementation of the fble library.

#ifndef FBLE_INTERNAL_H_
#define FBLE_INTERNAL_H_

#include "fble.h"

// FbleInstrTag --
//   Enum used to distinguish among different kinds of instructions.
typedef enum {
  FBLE_STRUCT_VALUE_INSTR,
  FBLE_UNION_VALUE_INSTR,
  FBLE_STRUCT_ACCESS_INSTR,
  FBLE_UNION_ACCESS_INSTR,
  FBLE_COND_INSTR,
  FBLE_FUNC_VALUE_INSTR,
  FBLE_DESCOPE_INSTR,
  FBLE_FUNC_APPLY_INSTR,
  FBLE_GET_INSTR,
  FBLE_PUT_INSTR,
  FBLE_EVAL_INSTR,
  FBLE_LINK_INSTR,
  FBLE_EXEC_INSTR,
  FBLE_JOIN_INSTR,
  FBLE_PROC_INSTR,
  FBLE_VAR_INSTR,
  FBLE_LET_PREP_INSTR,
  FBLE_LET_DEF_INSTR,
  FBLE_NAMESPACE_INSTR,
  FBLE_IPOP_INSTR,
} FbleInstrTag;

// FbleInstr --
//   Common base type for all instructions.
typedef struct {
  FbleInstrTag tag;
} FbleInstr;

// FbleInstrV --
//   A vector of FbleInstr.
typedef struct {
  size_t size;
  FbleInstr** xs;
} FbleInstrV;

// FbleInstrBlock --
//   A reference counted block of instructions.
typedef struct {
  size_t refcount;
  FbleInstrV instrs;
} FbleInstrBlock;

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

typedef struct {
  size_t size;
  FbleInstrBlock** xs;
} FbleInstrBlockV;

// FbleCondInstr -- FBLE_COND_INSTR
//   Select the next thing to execute based on the tag of the value on top of
//   the value stack.
//
// dstack: ..., obj
//     ==> ...
//
// istack: ...
//     ==> ..., choices[obj.tag]
typedef struct {
  FbleInstr _base;
  FbleInstrBlockV choices;
} FbleCondInstr;

// FbleFuncValueInstr -- FBLE_FUNC_VALUE_INSTR
//   Allocate a function, capturing the current variable context in the
//   process.
//
// dstack: ...
//     ==> ..., func
//
// Fields:
//   contextc - The number of variables from the scope to capture from the top
//              of the variable stack.
//   body - A block of instructions that will execute the body of the function
//          in the context of its scope and arguments. The instruction should
//          remove the context of its scope and arguments.
typedef struct {
  FbleInstr _base;
  size_t contextc;
  FbleInstrBlock* body;
} FbleFuncValueInstr;

// FbleDescopeInstr -- FBLE_DESCOPE_INSTR
//   Pop count values from the top of the variable stack.
//
// vstack: ..., vN, ..., v2, v1
//     ==> ...,
typedef struct {
  FbleInstr _base;
  size_t count;
} FbleDescopeInstr;

// FbleFuncApplyInstr -- FBLE_FUNC_APPLY_INSTR
//   dstack: ..., f, x1
//       ==> ..., f(x1)
typedef struct {
  FbleInstr _base;
} FbleFuncApplyInstr;

// FbleGetInstr -- FBLE_GET_INSTR
//   Allocate an FbleGetProcValue.
//
// dstack: ..., port
//     ==> ..., get(port)
typedef struct {
  FbleInstr _base;
} FbleGetInstr;

// FblePutInstr -- FBLE_PUT_INSTR
//   Allocate an FblePutProcValue.
//
// dstack: ..., port, arg
//     ==> ..., put(port, arg)
typedef struct {
  FbleInstr _base;
} FblePutInstr;

// FbleEvalInstr -- FBLE_EVAL_INSTR
//   Allocate an FbleEvalProcValue.
//
// dstack: ..., arg
//     ==> ..., eval(arg)
typedef struct {
  FbleInstr _base;
} FbleEvalInstr;

// FbleLinkInstr -- FBLE_LINK_INSTR
//   Allocate an FbleLinkProcValue.
//
// dstack: ...,
//     ==> ..., link()
//
// Fields:
//   contextc - The number of variables from the scope to capture from the top
//              of the variable stack.
//   body - A block of instructions that will execute the body of the link in
//          the context of its scope and put and get ports. The instruction
//          should remove the context of its scope and put and get ports.
typedef struct {
  FbleInstr _base;
  size_t contextc;
  FbleInstrBlock* body;
} FbleLinkInstr;

// FbleExecInstr -- FBLE_EXEC_INSTR
//   Allocate an FbleExecProcValue.
//
// dstack: ..., p1, p2, ..., pN
//     ==> exec(p1, p2, ..., pN)
//
// The body is an instruction that does the following:
// dstack: ..., exec, b1
//     ==> ..., body(b1)
typedef struct {
  FbleInstr _base;
  size_t contextc;
  size_t argc;
  FbleInstrBlock* body;
} FbleExecInstr;

// FbleJoinInstr -- FBLE_JOIN_INSTR
//   If all child threads are done executing, move their results to the top of
//   the variable stack and free the child thread resources.
typedef struct {
  FbleInstr _base;
} FbleJoinInstr;

// FbleProcInstr -- FBLE_PROC_INSTR
//   Execute the process value on top of the value stack.
typedef struct {
  FbleInstr _base;
} FbleProcInstr;

// FbleVarInstr -- FBLE_VAR_INSTR
// vstack: ..., v[2], v[1], v[0]
// dstack: ...,
//     ==> ..., v[position]
typedef struct {
  FbleInstr _base;
  size_t position;
} FbleVarInstr;

// FbleLetPrepInstr -- FBLE_LET_PREP_INSTR
//   Prepare to evaluate a let.
//
// vstack: ...
//     ==> ..., r1, r2, ..., rN
typedef struct {
  FbleInstr _base;
  size_t count;
} FbleLetPrepInstr;

// FbleLetDefInstr -- FBLE_LET_DEF_INSTR
//
// vstack: ..., r1, r2, ..., rN
// dstack: ..., v1, v2, ..., vN
//     ==>
// vstack: ..., r1=v1, r2=v2, ..., rN=vN
// dstack: ...
typedef struct {
  FbleInstr _base;
  size_t count;
} FbleLetDefInstr;

// FbleNamespaceInstr -- FBLE_NAMESPACE_INSTR
//
// vstack: ...
// dstack: ..., v
//     ==>
// vstack: ..., v[1], v[2], ..., v[n]
// dstack: ...
//
// Where 'v' is a struct value and v[i] is the ith field of the struct value.
typedef struct {
  FbleInstr _base;
  size_t fieldc;
} FbleNamespaceInstr;

// FbleIPopInstr -- FBLE_IPOP_INSTR
//
// istack: ..., i
//     ==> ...
typedef struct {
  FbleInstr _base;
} FbleIPopInstr;

// FbleFreeInstrBlock --
//   Free the given block of instructions.
//
// Inputs:
//   arena - the arena used to allocation the instructions.
//   block - the block of instructions to free. May be NULL.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the given block of instruction.
void FbleFreeInstrBlock(FbleArena* arena, FbleInstrBlock* block);

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
//   memory for the instructions which must be freed with FbleFreeInstrBlock when
//   it is no longer needed.
FbleInstrBlock* FbleCompile(FbleArena* arena, FbleExpr* expr);

// FbleVStack --
//   A stack of values.
typedef struct FbleVStack {
  FbleValue* value;
  struct FbleVStack* tail;
} FbleVStack;

// FbleFuncValue -- FBLE_FUNC_VALUE
//
// Fields:
//   context - The var_stack at the time the function was created,
//             representing the lexical context available to the function.
//   body - The block of instructions representing the body of the function,
//          which should pop the arguments and context.
struct FbleFuncValue {
  FbleValue _base;
  FbleVStack* context;
  FbleInstrBlock* body;
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
  FbleInstrBlock* body;
} FbleLinkProcValue;

// FbleExecProcValue -- FBLE_EXEC_PROC_VALUE
typedef struct {
  FbleProcValue _base;
  FbleValueV bindings;
  FbleVStack* context;
  FbleInstrBlock* body;
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

// FblePortValue -- FBLE_PORT_VALUE
struct FblePortValue {
  FbleValue _base;
  size_t id;
};

#endif // FBLE_INTERNAL_H_
