// instr.h --
//   Header file describing the interface for working with fble instructions.
//   This is an internal library interface.

#ifndef FBLE_INSTR_H_
#define FBLE_INSTR_H_

#include "fble.h"

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
  FBLE_RELEASE_INSTR,
  FBLE_FUNC_APPLY_INSTR,
  FBLE_PROC_VALUE_INSTR,
  FBLE_GET_INSTR,
  FBLE_PUT_INSTR,
  FBLE_LINK_INSTR,
  FBLE_FORK_INSTR,
  FBLE_PROC_INSTR,
  FBLE_COPY_INSTR,
  FBLE_REF_VALUE_INSTR,
  FBLE_REF_DEF_INSTR,
  FBLE_RETURN_INSTR,
  FBLE_TYPE_INSTR,
  FBLE_PROFILE_ENTER_BLOCK_INSTR,
  FBLE_PROFILE_EXIT_BLOCK_INSTR,
  FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR,
  FBLE_PROFILE_EXIT_FUNC_INSTR,
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

// FbleAccessInstr -- FBLE_STRUCT_ACCESS_INSTR or FBLE_UNION_ACCESS_INSTR
//   Access a tagged field from an object.
//
// *dest = obj.tag
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  FbleFrameIndex obj;
  size_t tag;
  FbleLocalIndex dest;
} FbleAccessInstr;

typedef struct {
  size_t size;
  FbleInstrBlock** xs;
} FbleInstrBlockV;

// FbleUnionSelectInstr -- FBLE_UNION_SELECT_INSTR
//   Select the next thing to execute based on the tag of the value on top of
//   the value stack.
//
// pc += condition.tag
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  FbleFrameIndex condition;
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
//   Allocate a function, capturing the values to use for as variable values
//   when the function is executed.
//
// *dest = code[v1, v2, ...](argc)
//
// Fields:
//   argc - The number of arguments to the function.
//   dest - Where to store the allocated function.
//   code - A block of instructions that will execute the body of the function
//          in the context of its scope and arguments. The instruction should
//          remove the context of its scope and arguments.
//   scope - Variables from the scope to capture for the function.
typedef struct {
  FbleInstr _base;
  size_t argc;
  FbleLocalIndex dest;
  FbleInstrBlock* code;
  FbleFrameIndexV scope;
} FbleFuncValueInstr;

// FbleReleaseInstr -- FBLE_RELEASE_INSTR
//   Release and remove a value from the locals section of the stack frame.
typedef struct {
  FbleInstr _base;
  FbleLocalIndex value;
} FbleReleaseInstr;

// FbleFuncApplyInstr -- FBLE_FUNC_APPLY_INSTR
//   Apply a function to a single argument.
//
// *dest = func(arg)
//
// If exit is true, this is treated as a tail call. In that case, dest is
// ignored and the result is returned to the caller.
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  bool exit;
  FbleLocalIndex dest;
  FbleFrameIndex func;
  FbleFrameIndex arg;
} FbleFuncApplyInstr;

// FbleProcValueInstr -- FBLE_PROC_VALUE_INSTR
//   Allocate an FbleProcValue.
//
// dest = proc code [v1, v2, ...]
//
// Fields:
//   code - A block of instructions that will execute in the context of the
//          captured scope. The instruction should remove the context of its
//          scope.
//   scope - Variables from the scope to capture for the function.
//   dest - Where to put allocated proc value.
//
typedef struct {
  FbleInstr _base;
  FbleInstrBlock* code;
  FbleFrameIndexV scope;
  FbleLocalIndex dest;
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

// FbleProcInstr -- FBLE_PROC_INSTR
//   Execute a process value.
//
// *dest = exec(proc)
//
// if exit is true, this is treated as a tail call. In that case, dest is
// ignored and the result is returned to the caller.
typedef struct {
  FbleInstr _base;
  bool exit;
  FbleLocalIndex dest;
  FbleFrameIndex proc;
} FbleProcInstr;

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
//
// Note: it is important performance optimization not to set the value of a
// reference if the reference is unused, because the assignment triggers a
// pathological case in the cyclic reference counting approach we use.
typedef struct {
  FbleInstr _base;
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
//
typedef struct {
  FbleInstr _base;
  FbleLocalIndex dest;
} FbleTypeInstr;

// FbleProfileEnterBlockInstr -- FBLE_PROFILE_ENTER_BLOCK_INSTR
//
// Enters a new profiling block.
// 
// Fields:
//   block - the block to enter.
typedef struct {
  FbleInstr _base;
  FbleBlockId block;
} FbleProfileEnterBlockInstr;

// FbleProfileExitBlockInstr -- FBLE_PROFILE_EXIT_BLOCK_INSTR
//
// Exits the current profiling block.
typedef struct {
  FbleInstr _base;
} FbleProfileExitBlockInstr;

// FbleProfileAutoExitBlockInstr -- FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR
//
// Auto-exits the current profiling block.
typedef struct {
  FbleInstr _base;
} FbleProfileAutoExitBlockInstr;

// FbleProfileExitFuncInstr -- FBLE_PROFILE_EXIT_FUNC_INSTR
//
// Exits or auto-exits the current profiling block as appropriate based on the
// value of a function about to be applied tail-recursively.
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  FbleFrameIndex func;
} FbleProfileExitFuncInstr;

// FbleFreeInstr --
//   Free the given instruction.
//
// Inputs:
//   arena - the arena used to allocation the instructions.
//   instr - the instruction to free.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the given instruction.
void FbleFreeInstr(FbleArena* arena, FbleInstr* instr);

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
//   profile - profile to populate with blocks.
//   prgm - the program to compile.
//
// Results:
//   The compiled program, or NULL if the program is not well typed.
//
// Side effects:
//   Prints warning messages to stderr.
//   Prints a message to stderr if the program fails to compile. Allocates
//   memory for the instructions which must be freed with FbleFreeInstrBlock when
//   it is no longer needed.
//   Adds blocks to the given profile.
FbleInstrBlock* FbleCompile(FbleArena* arena, FbleProfile* profile, FbleProgram* program);

#endif // FBLE_INSTR_H_
