// code.h --
//   Defines FbleInstr and FbleCode types, describing an internal instruction
//   set that can be used to run fble programs.

#ifndef FBLE_INTERNAL_CODE_H_
#define FBLE_INTERNAL_CODE_H_

#include <fble/fble-compile.h>   // for FbleCode forward declaration.
#include <fble/fble-execute.h>   // for FbleExecutable.
#include <fble/fble-profile.h>   // for FbleBlockId.

#include "kind.h"           // for FbleDataTypeTag.
#include "var.h"            // for FbleVar, FbleLocalIndex

// FbleProfileOpTag --
//   Enum used to distinguish among different kinds of FbleProfileOps.
typedef enum {
  FBLE_PROFILE_ENTER_OP,
  FBLE_PROFILE_REPLACE_OP,
  FBLE_PROFILE_EXIT_OP,
} FbleProfileOpTag;

// FbleProfileOp
//
// A singly-linked list of profiling operations.
//
// ENTER: Enters a new profiling block, as given by the 'block' field.
// EXIT: Exits the current profiling block. 'block' is ignored.
typedef struct FbleProfileOp {
  FbleProfileOpTag tag;
  FbleBlockId block;
  struct FbleProfileOp* next;
} FbleProfileOp;

// FbleDebugInfoTag --
//   Enum used to distinguish among different kinds of FbleDebugInfos.
typedef enum {
  FBLE_STATEMENT_DEBUG_INFO,
  FBLE_VAR_DEBUG_INFO,
} FbleDebugInfoTag;

// FbleDebugInfo
//
// A singly-linked list of debug info.
typedef struct FbleDebugInfo {
  FbleDebugInfoTag tag;
  struct FbleDebugInfo* next;
} FbleDebugInfo;

// FbleStatementDebugInfo --
//   FBLE_STATEMENT_DEBUG_INFO
//
// Indicates the instruction is the start of a new statement.
//
// Fields:
//   loc - the location of the statement.
typedef struct {
  FbleDebugInfo _base;
  FbleLoc loc;
} FbleStatementDebugInfo;

// FbleVarDebugInfo --
//   FBLE_VAR_DEBUG_INFO
//
// Fields:
//   name - the name a variable that enters scope at this instruction.
//   var - the location of the variable in the stack frame.
typedef struct {
  FbleDebugInfo _base;
  FbleName name;
  FbleVar var;
} FbleVarDebugInfo;

// FbleFreeDebugInfo --
//   Free the given chain of debug infos.
//
// Inputs:
//   info - the chain of debug infos to free. May be NULL.
//
// Side effect:
//   Frees memory allocated for the given debug infos.
void FbleFreeDebugInfo(FbleDebugInfo* info);

// FbleInstrTag --
//   Enum used to distinguish among different kinds of instructions.
typedef enum {
  FBLE_DATA_TYPE_INSTR,
  FBLE_STRUCT_VALUE_INSTR,
  FBLE_UNION_VALUE_INSTR,
  FBLE_STRUCT_ACCESS_INSTR,
  FBLE_UNION_ACCESS_INSTR,
  FBLE_UNION_SELECT_INSTR,
  FBLE_JUMP_INSTR,
  FBLE_FUNC_VALUE_INSTR,
  FBLE_CALL_INSTR,
  FBLE_COPY_INSTR,
  FBLE_REF_VALUE_INSTR,
  FBLE_REF_DEF_INSTR,
  FBLE_RETURN_INSTR,
  FBLE_TYPE_INSTR,
  FBLE_RELEASE_INSTR,
  FBLE_LIST_INSTR,
  FBLE_LITERAL_INSTR,
} FbleInstrTag;

// FbleInstr --
//   Common base type for all instructions.
//
// profile_ops are profiling operations to perform before executing the
// instruction.
typedef struct {
  FbleInstrTag tag;
  FbleDebugInfo* debug_info;
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

// FbleDataTypeInstr -- FBLE_DATA_TYPE_INSTR
//   Allocate a data type value.
//
// *dest = +(a1, a2, ..., aN)
// *dest = *(a1, a2, ..., aN)
typedef struct {
  FbleInstr _base;
  FbleDataTypeTag kind;
  FbleVarV fields;
  FbleLocalIndex dest;
} FbleDataTypeInstr;

// FbleStructValueInstr -- FBLE_STRUCT_VALUE_INSTR
//   Allocate a struct value.
//
// *dest = struct(a1, a2, ..., aN)
typedef struct {
  FbleInstr _base;
  FbleVarV args;
  FbleLocalIndex dest;
} FbleStructValueInstr;

// FbleUnionValueInstr -- FBLE_UNION_VALUE_INSTR
//   Allocate a union value.
//
// *dest = union(arg)
typedef struct {
  FbleInstr _base;
  size_t tag;
  FbleVar arg;
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
  FbleVar obj;
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
  FbleVar condition;
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
  FbleVarV scope;
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
  FbleVar func;
  FbleVarV args;
} FbleCallInstr;

// FbleCopyInstr -- FBLE_COPY_INSTR
//   Copy a value in the stack frame from one location to another.
typedef struct {
  FbleInstr _base;
  FbleVar source;
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
  FbleVar value;
} FbleRefDefInstr;

// FbleReturnInstr -- FBLE_RETURN_INSTR
//   Return <result> and exit the current stack frame.
typedef struct {
  FbleInstr _base;
  FbleVar result;
} FbleReturnInstr;

// FbleTypeInstr -- FBLE_TYPE_INSTR
//  *dest = @<>
typedef struct {
  FbleInstr _base;
  FbleLocalIndex dest;
} FbleTypeInstr;

// FbleReleaseInstr -- FBLE_RELEASE_INSTR
//  FbleReleaseValue(target)
typedef struct {
  FbleInstr _base;
  FbleLocalIndex target;
} FbleReleaseInstr;

// FbleListInstr -- FBLE_LIST_INSTR
// *dest = [a1, a2, ..., aN]
typedef struct {
  FbleInstr _base;
  FbleVarV args;
  FbleLocalIndex dest;
} FbleListInstr;

// FbleTagV --
//   A vector of tags.
typedef struct {
  size_t size;
  size_t* xs;
} FbleTagV;

// FbleLiteralInstr -- FBLE_LITERAL_INSTR
// *dest = "xxx"
typedef struct {
  FbleInstr _base;
  FbleTagV letters;
  FbleLocalIndex dest;
} FbleLiteralInstr;

// FbleRawAllocInstr --
//   Allocate and partially initialize an FbleInstr. This function is not type
//   safe. It is recommended to use the FbleAllocInstr and FbleAllocInstrExtra
//   macros instead.
//
// Inputs:
//   size - The total number of bytes to allocate for the instruction.
//   tag - The instruction tag.
//
// Result:
//   A pointer to a newly allocated size bytes of memory with FbleInstr tag,
//   debug_info, and profile_ops initialized.
void* FbleRawAllocInstr(size_t size, FbleInstrTag tag);

// FbleAllocInstr --
//   A type safe way of allocating instructions.
//
// Inputs:
//   T - The type of instruction to allocate.
//   tag - The tag of the instruction to allocate.
//
// Results:
//   A pointer to a newly allocated object of the given type with the
//   FbleInstr tag, debug_info, and profile_ops fields initialized.
//
// Side effects:
// * The allocation should be freed by calling FbleFreeInstr when no longer in
//   use.
#define FbleAllocInstr(T, tag) ((T*) FbleRawAllocInstr(sizeof(T), tag))

// FbleAllocInstrExtra --
//   Allocate an instruction with additional extra space.
//
// Inputs:
//   T - The type of instruction to allocate.
//   size - The size in bytes of extra space to include.
//   tag - The tag of the instruction.
//
// Results:
//   A pointer to a newly allocated instruction of the given type with extra
//   size, with FbleInstr fields initialized.
//
// Side effects:
// * The allocation should be freed by calling FbleFreeInstr when no longer in
//   use.
#define FbleAllocInstrExtra(T, size, tag) ((T*) FbleRawAllocInstr(sizeof(T) + size), tag)

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
//   num_args - the number of arguments to the function.
//   num_statics - the number of statics captured by the function.
//   num_locals - the number of locals used by the function.
//   profile_block_id - the profile block to use for this function.
//
// Returns:
//   A newly allocated FbleCode object with no initial instructions.
//
// Side effects:
//   Allocates a new FbleCode object that should be freed with FbleFreeCode or
//   FbleFreeExecutable when no longer needed.
FbleCode* FbleNewCode(size_t num_args, size_t num_statics, size_t num_locals, FbleBlockId profile_doc_id);

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
