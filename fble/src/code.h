// code.h --
//   Defines FbleInstr and FbleCode types, describing an internal instruction
//   set that can be used to run fble programs.

#ifndef FBLE_INTERNAL_CODE_H_
#define FBLE_INTERNAL_CODE_H_

#include "fble-compile.h"   // for FbleCode forward declaration.
#include "fble-profile.h"   // for FbleBlockId.
#include "execute.h"        // for FbleExecutable.

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

// FbleFrameIndexV --
//   A vector of FbleFrameIndex.
typedef struct {
  size_t size;
  FbleFrameIndex* xs;
} FbleFrameIndexV;

// FbleLocalIndex --
//   The position of a value in the locals section of a stack frame.
typedef size_t FbleLocalIndex;

// FbleLocalIndexV -- A vector of FbleLocalIndex.
typedef struct {
  size_t size;
  FbleLocalIndex* xs;
} FbleLocalIndexV;

// FbleProfileOpTag --
//   Enum used to distinguish among different kinds of FbleProfileOps.
typedef enum {
  FBLE_PROFILE_ENTER_OP,
  FBLE_PROFILE_REPLACE_OP,
  FBLE_PROFILE_EXIT_OP,
  FBLE_PROFILE_AUTO_EXIT_OP,
} FbleProfileOpTag;

// FbleProfileOp
//
// A singly-linked list of profiling operations.
//
// ENTER: Enters a new profiling block, as given by the 'block' field.
// EXIT: Exits the current profiling block. 'block' is ignored.
// AUTO_EXIT: Auto-exits the current profiling block. 'block' is ignored.
typedef struct FbleProfileOp {
  FbleProfileOpTag tag;
  FbleBlockId block;
  struct FbleProfileOp* next;
} FbleProfileOp;

// FbleInstrTag --
//   Enum used to distinguish among different kinds of instructions.
typedef enum {
  FBLE_STRUCT_VALUE_INSTR,
  FBLE_UNION_VALUE_INSTR,
  FBLE_STRUCT_ACCESS_INSTR,
  FBLE_UNION_ACCESS_INSTR,
  FBLE_UNION_SELECT_INSTR,
  FBLE_JUMP_INSTR,
  FBLE_FUNC_VALUE_INSTR,
  FBLE_CALL_INSTR,
  FBLE_LINK_INSTR,
  FBLE_FORK_INSTR,
  FBLE_COPY_INSTR,
  FBLE_REF_VALUE_INSTR,
  FBLE_REF_DEF_INSTR,
  FBLE_RETURN_INSTR,
  FBLE_TYPE_INSTR,
} FbleInstrTag;

// FbleInstr --
//   Common base type for all instructions.
//
// profile_ops are profiling operations to perform before executing the
// instruction.
typedef struct {
  FbleInstrTag tag;
  FbleProfileOp* profile_ops;
} FbleInstr;

// FbleInstrV --
//   A vector of FbleInstr.
typedef struct {
  size_t size;
  FbleInstr** xs;
} FbleInstrV;

// FbleCode --
//   A subclass of FbleExecutable that executes code by interpreting
//   instructions.
struct FbleCode {
  FbleExecutable _base;
  FbleInstrV instrs;
};

// FbleCodeV --
//   A vector of FbleCode.
typedef struct {
  size_t size;
  FbleCode** xs;
} FbleCodeV;

// FbleStructValueInstr -- FBLE_STRUCT_VALUE_INSTR
//   Allocate a struct value.
//
// *dest = struct(a1, a2, ..., aN)
typedef struct {
  FbleInstr _base;
  FbleFrameIndexV args;
  FbleLocalIndex dest;
} FbleStructValueInstr;

// FbleUnionValueInstr -- FBLE_UNION_VALUE_INSTR
//   Allocate a union value.
//
// *dest = union(arg)
typedef struct {
  FbleInstr _base;
  size_t tag;
  FbleFrameIndex arg;
  FbleLocalIndex dest;
} FbleUnionValueInstr;

// FbleAccessInstr --
//   FBLE_STRUCT_ACCESS_INSTR
//   FBLE_UNION_ACCESS_INSTR
//   Access a tagged field from an object.
//
// *dest = obj.<tag>
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  FbleFrameIndex obj;
  size_t tag;
  FbleLocalIndex dest;
} FbleAccessInstr;

// FbleOffsetV --
//   A vector of offsets.
typedef struct {
  size_t size;
  size_t* xs;
} FbleOffsetV;

// FbleUnionSelectInstr -- FBLE_UNION_SELECT_INSTR
//   Select the next thing to execute based on the tag of the value on top of
//   the value stack.
//
// next_pc += ?(condition.tag; jumps[0], jumps[1], ...);
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  FbleFrameIndex condition;
  FbleOffsetV jumps;
} FbleUnionSelectInstr;

// FbleJumpInstr -- FBLE_JUMP_INSTR
//   Jump forward by the given number of instructions beyond what would
//   otherwise have been the next instruction.
// 
// Jumping backwards is not supported.
//
// next_pc += count
typedef struct {
  FbleInstr _base;
  size_t count;
} FbleJumpInstr;

// FbleFuncValueInstr -- FBLE_FUNC_VALUE_INSTR
//   Allocate a function, capturing the values to use for as variable values
//   when the function is executed.
//
// *dest = code[v1, v2, ...](argc)
//
// Fields:
//   dest - Where to store the allocated function.
//   code - A block of instructions that will execute the body of the function
//          in the context of its scope and arguments. The instruction should
//          remove the context of its scope and arguments.
//   scope - Variables from the scope to capture for the function.
//
// Note: FuncValues are used for both pure functions and processes at runtime,
// so FBLE_FUNC_VALUE_INSTR is used for allocating process values as well as
// function values.
typedef struct {
  FbleInstr _base;
  FbleLocalIndex dest;
  FbleCode* code;
  FbleFrameIndexV scope;
} FbleFuncValueInstr;

// FbleProcValueInstr -- FBLE_PROC_VALUE_INSTR
//   A proc value is represented as a function that takes no arguments.
#define FBLE_PROC_VALUE_INSTR FBLE_FUNC_VALUE_INSTR
typedef FbleFuncValueInstr FbleProcValueInstr;

// FbleCallInstr -- FBLE_CALL_INSTR
//   Call a function.
//
// Also used for executing a process value, which is treated as a
// zero-argument function.
//
// *dest = func(args[0], args[1], ...)
//
// If exit is true, this is treated as a tail call. In that case, dest is
// ignored and the result is returned to the caller.
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  bool exit;
  FbleLocalIndex dest;
  FbleFrameIndex func;
  FbleFrameIndexV args;
} FbleCallInstr;

// FbleLinkInstr -- FBLE_LINK_INSTR
//   Allocate a new link with get and put ports.
//
// *get = <get port>;
// *put = <put port>;
typedef struct {
  FbleInstr _base;
  FbleLocalIndex get;
  FbleLocalIndex put;
} FbleLinkInstr;

// FbleForkInstr -- FBLE_FORK_INSTR
//   Fork child threads.
//
// Each argument should be a proc value. Executes the proc value in the child
// thread and stores the result to the given destination in the parent
// thread's stack frame.
//
// The parent thread does not resume until all child threads have finished.
typedef struct {
  FbleInstr _base;
  FbleFrameIndexV args;
  FbleLocalIndexV dests;
} FbleForkInstr;

// FbleCopyInstr -- FBLE_COPY_INSTR
//   Copy a value in the stack frame from one location to another.
typedef struct {
  FbleInstr _base;
  FbleFrameIndex source;
  FbleLocalIndex dest;
} FbleCopyInstr;

// FbleRefValueInstr -- FBLE_REF_VALUE_INSTR
//   Allocate a ref value and store the result in index.
//
// *dest = new ref
typedef struct {
  FbleInstr _base;
  FbleLocalIndex dest;
} FbleRefValueInstr;

// FbleRefDefInstr -- FBLE_REF_DEF_INSTR
//   Set the value of a reference.
//
// ref->value = value
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  FbleLocalIndex ref;
  FbleFrameIndex value;
} FbleRefDefInstr;

// FbleReturnInstr -- FBLE_RETURN_INSTR
//   Return <result> and exit the current stack frame.
typedef struct {
  FbleInstr _base;
  FbleFrameIndex result;
} FbleReturnInstr;

// FbleTypeInstr -- FBLE_TYPE_INSTR
//  *dest = @<>
typedef struct {
  FbleInstr _base;
  FbleLocalIndex dest;
} FbleTypeInstr;

// FbleFreeInstr --
//   Free the given instruction.
//
// Inputs:
//   instr - the instruction to free.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the given instruction.
void FbleFreeInstr(FbleInstr* instr);

// FbleNewCode --
//   Allocate a new, empty FbleCode instance.
//
// Inputs:
//   args - the number of arguments to the function.
//   statics - the number of statics captured by the function.
//   locals - the number of locals used by the function.
//
// Returns:
//   A newly allocated FbleCode object with no initial instructions.
//
// Side effects:
//   Allocates a new FbleCode object that should be freed with FbleFreeCode or
//   FbleFreeExecutable when no longer needed.
FbleCode* FbleNewCode(size_t args, size_t statics, size_t locals);

// FbleFreeCode --
//   Decrement the refcount on the given block of instructions and free it if
//   appropriate.
//
// Inputs:
//   code - the code to free. May be NULL.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the given block of instruction if the refcount
//   has gone to 0.
void FbleFreeCode(FbleCode* code);

#endif // FBLE_INTERNAL_CODE_H_
