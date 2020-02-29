// internal.h --
//   This header file describes the internal interface used in the
//   implementation of the fble library.

#ifndef FBLE_INTERNAL_H_
#define FBLE_INTERNAL_H_

#include "fble.h"
#include "ref.h"

// FbleFrameSection --
//   Which section of a frame a value can be found in.
typedef enum {
  FBLE_STATICS_FRAME_SECTION,
  FBLE_LOCALS_FRAME_SECTION,
} FbleFrameSection;

// FbleFrameIndex --
//   The position of a value in a stack frame.
typedef struct {
  FbleFrameSection section;
  size_t index;
} FbleFrameIndex;

// FbleLocalIndex --
//   The position of a value in the locals section of a stack frame.
typedef size_t FbleLocalIndex;

// FbleLocalIndexV -- A vector of FbleLocalIndex.
typedef struct {
  size_t size;
  FbleLocalIndex* xs;
} FbleLocalIndexV;

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
  FBLE_PROC_VALUE_INSTR,
  FBLE_GET_INSTR,
  FBLE_PUT_INSTR,
  FBLE_LINK_INSTR,
  FBLE_FORK_INSTR,
  FBLE_JOIN_INSTR,
  FBLE_PROC_INSTR,
  FBLE_VAR_INSTR,
  FBLE_REF_VALUE_INSTR,
  FBLE_REF_DEF_INSTR,
  FBLE_STRUCT_IMPORT_INSTR,
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
  size_t statics;     // The number of statics used by this frame.
  size_t locals;      // The number of locals required by this stack frame.
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
  FbleLoc loc;
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
//   Allocate a function, capturing the values from the data stack to use for
//   as variable values when the function is executed.
//
// data_stack: ...
//         ==> ..., func
//
// Fields:
//   scopec - The number of values to capture from the top of the data stack.
//   argc - The number of arguments to the function.
//   code - A block of instructions that will execute the body of the function
//          in the context of its scope and arguments. The instruction should
//          remove the context of its scope and arguments.
typedef struct {
  FbleInstr _base;
  size_t argc;
  FbleInstrBlock* code;
} FbleFuncValueInstr;

// FbleDescopeInstr -- FBLE_DESCOPE_INSTR
//   Release and remove a value from the locals section of the stack frame.
typedef struct {
  FbleInstr _base;
  FbleLocalIndex index;
} FbleDescopeInstr;

// FbleFuncApplyInstr -- FBLE_FUNC_APPLY_INSTR
//   data_stack: ..., x, f
//           ==> ..., f(x)
//
// If exit is true, this is treated as a tail call.
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  bool exit;
} FbleFuncApplyInstr;

// FbleProcValueInstr -- FBLE_PROC_VALUE_INSTR
//   Allocate an FbleProcValue.
//
// Fields:
//   scopec - The number of variables from the scope to capture from the top
//   of the data stack.
//   code - A block of instructions that will execute in the context of the
//          captured scope. The instruction should remove the context of its
//          scope.
//
// data_stack: ..., vN, , ..., v2, v1
//         ==> ..., proc(v1, v2, ..., vN, code)
typedef struct {
  FbleInstr _base;
  FbleInstrBlock* code;
} FbleProcValueInstr;

// FbleGetInstr -- FBLE_GET_INSTR
//   Get a value from a port and return from the stack frame.
//   The port is specified as static variable 0.
//
// return get(port)
typedef struct {
  FbleInstr _base;
} FbleGetInstr;

// FblePutInstr -- FBLE_PUT_INSTR
//   Put a value to a port. The port and arg are assumed to be supplied as
//   static values 0 and 1 respectively.
//
// put(port, arg);
// return unit;
typedef struct {
  FbleInstr _base;
} FblePutInstr;

// FbleLinkInstr -- FBLE_LINK_INSTR
//   Allocate a new link with get and put ports.
//
// *get_index = get;
// *put_index = put;
typedef struct {
  FbleInstr _base;
  FbleLocalIndex get_index;
  FbleLocalIndex put_index;
} FbleLinkInstr;

// FbleForkInstr -- FBLE_FORK_INSTR
//   Fork child threads.
//
// data_stack: ..., b1, b2, ..., bN
//         ==> ...
//
// children: _ ==> b1, b2, ... bN
typedef struct {
  FbleInstr _base;
  FbleLocalIndexV args;
} FbleForkInstr;

// FbleJoinInstr -- FBLE_JOIN_INSTR
//   If all child threads are done executing, move their results to the top of
//   the variable stack and free the child thread resources.
typedef struct {
  FbleInstr _base;
} FbleJoinInstr;

// FbleProcInstr -- FBLE_PROC_INSTR
//   Execute the process value on top of the data stack.
//
// If exit is true, this is treated as a tail call.
//
// data_stack: ..., foo!
//         ==> ..., foo
typedef struct {
  FbleInstr _base;
  bool exit;
} FbleProcInstr;

// FbleVarInstr -- FBLE_VAR_INSTR
//   Copy a value in the stack frame to the top of the data stack.
typedef struct {
  FbleInstr _base;
  FbleFrameIndex index;
} FbleVarInstr;

// FbleVarInstrV --
//   A vector of var instructions.
typedef struct {
  size_t size;
  FbleVarInstr** xs;
} FbleVarInstrV;

// FbleRefValueInstr -- FBLE_REF_VALUE_INSTR
//   Allocate a ref value and store the result in index.
//
// *index = new ref
typedef struct {
  FbleInstr _base;
  FbleLocalIndex index;
} FbleRefValueInstr;

// FbleRefDefInstr -- FBLE_REF_DEF_INSTR
//
// vstack:     ..., ri, ..., r1, r0
// data_stack: ..., v
//         ==>
// vstack:     ..., ri=v, ..., r1, r0
// data_stack: ...
//
// If recursive is true, then ri is set to point to the computed v.
// If recursive is false, then it is assumed there are no references remaining
// to ri, and the assignment is avoided. This is an important performance
// optimization because the assignment triggers a pathological case in the
// cyclic reference counting approach we use.
typedef struct {
  FbleInstr _base;
  FbleLocalIndex index;
  bool recursive;
} FbleRefDefInstr;

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
  FbleLoc loc;
  FbleLocalIndexV fields;
} FbleStructImportInstr;

// FbleExitScopeInstr -- FBLE_EXIT_SCOPE_INSTR
//   Return <result> and exit the current stack frame.
typedef struct {
  FbleInstr _base;
  FbleFrameIndex result;
} FbleExitScopeInstr;

// FbleTypeInstr -- FBLE_TYPE_INSTR
// data_stack: ...
//         ==> ..., ()
//
typedef struct {
  FbleInstr _base;
}  FbleTypeInstr;

// FbleVPushInstr -- FBLE_VPUSH_INSTR
//   Move a value from the data stack to the locals section of the stack
//   frame.
typedef struct {
  FbleInstr _base;
  FbleLocalIndex index;
} FbleVPushInstr;

// FbleProfileEnterBlockInstr -- FBLE_PROFILE_ENTER_BLOCK_INSTR
// 
// Fields:
//   time - the shallow time required to execute this block
typedef struct {
  FbleInstr _base;
  FbleBlockId block;
  size_t time;
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
//   Type check and compile the given program.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - vector of locations to record for each block.
//   prgm - the program to compile.
//
// Results:
//   The compiled program, or NULL if the program is not well typed.
//
// Side effects:
//   Prints a message to stderr if the program fails to compile. Allocates
//   memory for the instructions which must be freed with FbleFreeInstrBlock when
//   it is no longer needed.
//   Initializes and populates blocks with the compiled blocks.
FbleInstrBlock* FbleCompile(FbleArena* arena, FbleNameV* blocks, FbleProgram* program);

// FbleValueTag --
//   A tag used to distinguish among different kinds of values.
typedef enum {
  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_FUNC_VALUE,
  FBLE_PROC_VALUE,
  FBLE_LINK_VALUE,
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
  FBLE_PUT_FUNC_VALUE,
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
  FbleInstrBlock* code;
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

// FblePutFuncValue -- FBLE_PUT_FUNC_VALUE
//   A process put function. Given an argument, it returns a process to put
//   that argument onto the associated put port.
typedef struct {
  FbleFuncValue _base;
  FbleValue* port;
} FblePutFuncValue;

// FbleProcValue -- FBLE_PROC_VALUE
typedef struct {
  FbleValue _base;
  FbleValueV scope;
  FbleInstrBlock* code;
} FbleProcValue;

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
  FbleValue _base;
  FbleValues* head;
  FbleValues* tail;
} FbleLinkValue;

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

// FbleNewGetProcValue --
//   Create a new get proc value for the given link.
//
// Inputs:
//   arena - the arena to use for allocations.
//   port - the port value to get from.
//
// Results:
//   A newly allocated get value.
//
// Side effects:
//   The returned get value must be freed using FbleValueRelease when no
//   longer in use. This function does not take ownership of the port value.
//   argument.
FbleValue* FbleNewGetProcValue(FbleValueArena* arena, FbleValue* port);

#endif // FBLE_INTERNAL_H_
