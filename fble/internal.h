// internal.h --
//   This header file describes the internal interface used in the
//   implementation of the fble library.

#ifndef FBLE_INTERNAL_H_
#define FBLE_INTERNAL_H_

#include "fble.h"
#include "ref.h"

// FbleInstrTag --
//   Enum used to distinguish among different kinds of instructions.
typedef enum {
  FBLE_STRUCT_VALUE_INSTR,
  FBLE_UNION_VALUE_INSTR,
  FBLE_STRUCT_ACCESS_INSTR,
  FBLE_UNION_ACCESS_INSTR,
  FBLE_UNION_SELECT_INSTR,
  FBLE_GOTO_INSTR,
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
  FBLE_STRUCT_IMPORT_INSTR,
  FBLE_ENTER_SCOPE_INSTR,
  FBLE_EXIT_SCOPE_INSTR,
  FBLE_TYPE_INSTR,
  FBLE_VPUSH_INSTR,
  FBLE_PROFILE_ENTER_BLOCK_INSTR,
  FBLE_PROFILE_EXIT_BLOCK_INSTR,
  FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR,
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
// data_stack:  ..., aN, ..., a2, a1, type
//         ==>  ..., struct(a1, a2, ..., aN)
typedef struct {
  FbleInstr _base;
  size_t argc;
} FbleStructValueInstr;

// FbleUnionValueInstr -- FBLE_UNION_VALUE_INSTR
//   Allocate a union value.
//
// data_stack: ..., arg
//         ==> ..., union(arg)
typedef struct {
  FbleInstr _base;
  size_t tag;
} FbleUnionValueInstr;

// FbleAccessInstr -- FBLE_STRUCT_ACCESS_INSTR or FBLE_UNION_ACCESS_INSTR
//   Access the tagged field from the object on top of the vstack.
//
// data_stack: ..., obj
//         ==> ..., obj.tag
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  size_t tag;
} FbleAccessInstr;

typedef struct {
  size_t size;
  FbleInstrBlock** xs;
} FbleInstrBlockV;

// FbleUnionSelectInstr -- FBLE_UNION_SELECT_INSTR
//   Select the next thing to execute based on the tag of the value on top of
//   the value stack.
//
// pc += obj.tag
typedef struct {
  FbleInstr _base;
} FbleUnionSelectInstr;

// FbleGotoInstr -- FBLE_GOTO_INSTR
//   Jumped to the specified position in the current instruction block.
//
// pc = pc
typedef struct {
  FbleInstr _base;
  size_t pc;
} FbleGotoInstr;

// FbleFuncValueInstr -- FBLE_FUNC_VALUE_INSTR
//   Allocate a function, capturing the current variable scope in the
//   process.
//
// data_stack: ...
//         ==> ..., func
//
// Fields:
//   scopec - The number of variables from the scope to capture from the top
//            of the variable stack.
//   argc - The number of arguments to the function.
//   body - A block of instructions that will execute the body of the function
//          in the context of its scope and arguments. The instruction should
//          remove the context of its scope and arguments.
typedef struct {
  FbleInstr _base;
  size_t scopec;
  size_t argc;
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
//   data_stack: ..., x, f
//           ==> ..., f(x)
//
// If exit is true, this is treated as a tail call.
typedef struct {
  FbleInstr _base;
  bool exit;
} FbleFuncApplyInstr;

// FbleGetInstr -- FBLE_GET_INSTR
//   Allocate an FbleGetProcValue.
//
// data_stack: ..., port
//         ==> ..., get(port)
typedef struct {
  FbleInstr _base;
} FbleGetInstr;

// FblePutInstr -- FBLE_PUT_INSTR
//   Allocate an FblePutProcValue.
//
// data_stack: ..., arg, port
//         ==> ..., put(port, arg)
typedef struct {
  FbleInstr _base;
} FblePutInstr;

// FbleEvalInstr -- FBLE_EVAL_INSTR
//   Allocate an FbleEvalProcValue.
//
// data_stack: ..., arg
//         ==> ..., eval(arg)
typedef struct {
  FbleInstr _base;
} FbleEvalInstr;

// FbleLinkInstr -- FBLE_LINK_INSTR
//   Allocate an FbleLinkProcValue.
//
// data_stack: ...,
//         ==> ..., link()
//
// Fields:
//   scopec - The number of variables from the scope to capture from the top
//            of the variable stack.
//   body - A block of instructions that will execute the body of the link in
//          the context of its scope and put and get ports. The instruction
//          should remove the context of its scope and put and get ports.
typedef struct {
  FbleInstr _base;
  size_t scopec;
  FbleInstrBlock* body;
} FbleLinkInstr;

// FbleExecInstr -- FBLE_EXEC_INSTR
//   Allocate an FbleExecProcValue.
//
// data_stack: ..., p1, p2, ..., pN
//         ==> exec(p1, p2, ..., pN)
//
// The body is an instruction that does the following:
// data_stack: ..., exec, b1
//         ==> ..., body(b1)
typedef struct {
  FbleInstr _base;
  size_t scopec;
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
//   Exit the current scope and execute the process value on top of the data stack.
typedef struct {
  FbleInstr _base;
} FbleProcInstr;

// FbleVarInstr -- FBLE_VAR_INSTR
// vstack: ..., v[2], v[1], v[0]
// data_stack: ...,
//         ==> ..., v[position]
typedef struct {
  FbleInstr _base;
  size_t position;
} FbleVarInstr;

// FbleVarInstrV --
//   A vector of var instructions.
typedef struct {
  size_t size;
  FbleVarInstr** xs;
} FbleVarInstrV;

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
// data_stack: ..., v1, v2, ..., vN
//         ==>
// vstack: ..., r1=v1, r2=v2, ..., rN=vN
// data_stack: ...
typedef struct {
  FbleInstr _base;
  size_t count;
} FbleLetDefInstr;

// FbleStructImportInstr -- FBLE_STRUCT_IMPORT_INSTR
//
// vstack: ...
// data_stack: ..., v
//   ==>
// vstack: ..., v[1], v[2], ..., v[n]
// data_stack: ...
//
// Where 'v' is a struct value and v[i] is the ith field of the struct value.
typedef struct {
  FbleInstr _base;
} FbleStructImportInstr;

// FbleEnterScopeInstr -- FBLE_ENTER_SCOPE_INSTR
//
// scope_stack: ...
//          ==> ..., [block]
typedef struct {
  FbleInstr _base;
  FbleInstrBlock* block;
} FbleEnterScopeInstr;

// FbleExitScopeInstr -- FBLE_EXIT_SCOPE_INSTR
//
// scope_stack: ..., [...]
//          ==> ...
typedef struct {
  FbleInstr _base;
} FbleExitScopeInstr;

// FbleTypeInstr -- FBLE_TYPE_INSTR
// data_stack: ...
//         ==> ..., ()
//
typedef struct {
  FbleInstr _base;
}  FbleTypeInstr;

// FbleVPushInstr -- FBLE_VPUSH_INSTR
// data_stack: ..., x0, x1, ..., xN
//         ==> ...
//
// vstack: ...,
//     ==> ..., xN, ..., x1, x0
//
typedef struct {
  FbleInstr _base;
  size_t count;
}  FbleVPushInstr;

// FbleProfileEnterBlockInstr -- FBLE_PROFILE_ENTER_BLOCK_INSTR
typedef struct {
  FbleInstr _base;
  FbleBlockId block;
}  FbleProfileEnterBlockInstr;

// FbleProfileExitBlockInstr -- FBLE_PROFILE_EXIT_BLOCK_INSTR
typedef struct {
  FbleInstr _base;
}  FbleProfileExitBlockInstr;

// FbleProfileAutoExitBlockInstr -- FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR
typedef struct {
  FbleInstr _base;
}  FbleProfileAutoExitBlockInstr;

// FbleFreeInstrBlock --
//   Decrement the refcount on the given block of instructions and free it if
//   appropriate.
//
// Inputs:
//   arena - the arena used to allocation the instructions.
//   block - the block of instructions to free. May be NULL.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the given block of instruction if the refcount
//   has gone to 0.
void FbleFreeInstrBlock(FbleArena* arena, FbleInstrBlock* block);

// FbleCompile --
//   Type check and compile the given expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - vector of locations to record for each block.
//   expr - the expression to compile.
//
// Results:
//   The compiled expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression fails to compile. Allocates
//   memory for the instructions which must be freed with FbleFreeInstrBlock when
//   it is no longer needed.
//   Initializes and populates blocks with the compiled blocks.
FbleInstrBlock* FbleCompile(FbleArena* arena, FbleNameV* blocks, FbleExpr* expr);

// FbleValueTag --
//   A tag used to distinguish among different kinds of values.
typedef enum {
  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_FUNC_VALUE,
  FBLE_PROC_VALUE,
  FBLE_INPUT_VALUE,
  FBLE_OUTPUT_VALUE,
  FBLE_PORT_VALUE,
  FBLE_REF_VALUE,
  FBLE_TYPE_VALUE,
} FbleValueTag;

// FbleValue --
//   A tagged union of value types. All values have the same initial
//   layout as FbleValue. The tag can be used to determine what kind of
//   value this is to get access to additional fields of the value
//   by first casting to that specific type of value.
struct FbleValue {
  FbleRef ref;
  FbleValueTag tag;
};

// FbleStructValue --
//   FBLE_STRUCT_VALUE
typedef struct {
  FbleValue _base;
  FbleValueV fields;
} FbleStructValue;

// FbleUnionValue --
//   FBLE_UNION_VALUE
typedef struct {
  FbleValue _base;
  size_t tag;
  FbleValue* arg;
} FbleUnionValue;

typedef enum {
  FBLE_BASIC_FUNC_VALUE,
  FBLE_THUNK_FUNC_VALUE,
} FbleFuncValueTag;

// FbleFuncValue -- FBLE_FUNC_VALUE
//   A tagged union of func value types. All values have the same initial
//   layout as FbleFuncValue. The tag can be used to determine what kind of
//   func value this is to get access to additional fields of the func value
//   by first casting to that specific type of func value.
//
// Fields:
//   argc - The number of arguments to be applied to this function before the
//          function body is executed.
typedef struct {
  FbleValue _base;
  FbleFuncValueTag tag;
  size_t argc;
} FbleFuncValue;

// FbleBasicFuncValue -- FBLE_BASIC_FUNC_VALUE
//
// Fields:
//   scope - The scope at the time the function was created,
//           representing the lexical context available to the function.
//           Stored as a vector of variables in scope order.
//   body - The block of instructions representing the body of the function,
//          which should pop the arguments and context.
typedef struct {
  FbleFuncValue _base;
  FbleValueV scope;
  FbleInstrBlock* body;
} FbleBasicFuncValue;

// FbleThunkFuncValue -- FBLE_THUNK_FUNC_VALUE
//   A function value that is the partial application of another function to
//   an argument.
//
// The value of this function value is: func[arg]
typedef struct {
  FbleFuncValue _base;
  FbleFuncValue* func;
  FbleValue* arg;
} FbleThunkFuncValue;

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
typedef struct {
  FbleValue _base;
  FbleProcValueTag tag;
} FbleProcValue;

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
  FbleValueV scope;
  FbleInstrBlock* body;
} FbleLinkProcValue;

// FbleExecProcValue -- FBLE_EXEC_PROC_VALUE
typedef struct {
  FbleProcValue _base;
  FbleValueV bindings;
  FbleValueV scope;
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
typedef struct {
  FbleValue _base;
  FbleValues* head;
  FbleValues* tail;
} FbleInputValue;

// FbleOutputValue --
//   FBLE_OUTPUT_VALUE
typedef struct {
  FbleValue _base;
  FbleInputValue* dest;
} FbleOutputValue;

// FblePortValue --
//   FBLE_PORT_VALUE
//
// Use for input and output values linked to external IO.
typedef struct {
  FbleValue _base;
  size_t id;
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
  FbleValue _base;
  FbleValue* value;
} FbleRefValue;

// FbleTypeValue --
//   FBLE_TYPE_VALUE
//
// A value representing a type. Because types are compile-time concepts, not
// runtime concepts, the type value contains no information.
typedef struct FbleTypeValue {
  FbleValue _base;
} FbleTypeValue;

#endif // FBLE_INTERNAL_H_
