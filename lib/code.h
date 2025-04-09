/**
 * @file code.h
 *  Defines FbleCode fble bytecode.
 */

#ifndef FBLE_INTERNAL_CODE_H_
#define FBLE_INTERNAL_CODE_H_

#include <fble/fble-compile.h>   // for FbleCode forward declaration.
#include <fble/fble-function.h>  // for FbleExecutable.
#include <fble/fble-profile.h>   // for FbleBlockId.

#include "kind.h"           // for FbleDataTypeTag.
#include "tag.h"            // for FbleTagV
#include "var.h"            // for FbleVar, FbleLocalIndex

/**
 * Different kinds of FbleProfileOps.
 */
typedef enum {
  FBLE_PROFILE_ENTER_OP,
  FBLE_PROFILE_REPLACE_OP,
  FBLE_PROFILE_EXIT_OP,
  FBLE_PROFILE_SAMPLE_OP,
} FbleProfileOpTag;

/**
 * @struct[FbleProfileOp] Singly-linked list of profiling operations.
 *  @field[FbleProfileOpTag][tag] The profiling operation.
 *  @field[size_t][arg]
 *   Block to enter or replace, relative to current profile_block_id.
 *   Time to sample.
 *   Unused for EXIT ops.
 *  @field[FbleProfileOp*][next]
 *   Next profile op in the list, or NULL for end of list.
 */
typedef struct FbleProfileOp {
  FbleProfileOpTag tag;
  size_t arg;
  struct FbleProfileOp* next;
} FbleProfileOp;

/**
 * Different kinds of FbleDebugInfos.
 */
typedef enum {
  FBLE_STATEMENT_DEBUG_INFO,
  FBLE_VAR_DEBUG_INFO,
} FbleDebugInfoTag;

/**
 * @struct[FbleDebugInfo] Singly-linked list of debug info.
 *  @field[FbleDbugInfoTag][tag] The kind of debug info.
 *  @field[FbleDebugInfo*][next] The next element in the list, or NULL.
 */
typedef struct FbleDebugInfo {
  FbleDebugInfoTag tag;
  struct FbleDebugInfo* next;
} FbleDebugInfo;

/**
 * @struct[FbleStatementDebugInfo] FBLE_STATEMENT_DEBUG_INFO
 *  @field[FbleDebugInfo][_base] FbleDebugInfo base class.
 *  @field[FbleLoc][loc] The source code location of the statement.
 */
typedef struct {
  FbleDebugInfo _base;
  FbleLoc loc;
} FbleStatementDebugInfo;

/**
 * @struct[FbleVarDebugInfo] FBLE_VAR_DEBUG_INFO
 *  @field[FbleDebugInfo][_base] FbleDebugInfo base class.
 *  @field[FbleName][name] Name of the variable.
 *  @field[FbleVar][var] Location of the variable in the stack frame.
 */
typedef struct {
  FbleDebugInfo _base;
  FbleName name;
  FbleVar var;
} FbleVarDebugInfo;

/**
 * @func[FbleFreeDebugInfo] Frees the given chain of debug infos.
 *  @arg[FbleDebugInfo*][info] The chain of debug infos to free. May be NULL.
 * 
 *  @sideeffects
 *   Frees memory allocated for the given debug infos.
 */
void FbleFreeDebugInfo(FbleDebugInfo* info);

/**
 * Different kinds of instructions.
 */
typedef enum {
  FBLE_STRUCT_VALUE_INSTR,
  FBLE_UNION_VALUE_INSTR,
  FBLE_STRUCT_ACCESS_INSTR,
  FBLE_UNION_ACCESS_INSTR,
  FBLE_UNION_SELECT_INSTR,
  FBLE_GOTO_INSTR,
  FBLE_FUNC_VALUE_INSTR,
  FBLE_CALL_INSTR,
  FBLE_TAIL_CALL_INSTR,
  FBLE_COPY_INSTR,
  FBLE_REC_DECL_INSTR,
  FBLE_REC_DEFN_INSTR,
  FBLE_RETURN_INSTR,
  FBLE_TYPE_INSTR,
  FBLE_LIST_INSTR,
  FBLE_LITERAL_INSTR,
  FBLE_NOP_INSTR,
  FBLE_UNDEF_INSTR,
} FbleInstrTag;

/**
 * @struct[FbleInstr] Base type for all instructions.
 *  @field[FbleInstrTag][tag] The kind of instruction.
 *  @field[FbleDebugInfo*][debug_info]
 *   Debug info that applies to just before executing the instruction.
 *  @field[FbleProfileOp*][profile_ops]
 *   Profiling operations to perform before executing the instruction.
 */
typedef struct {
  FbleInstrTag tag;
  FbleDebugInfo* debug_info;  
  FbleProfileOp* profile_ops;
} FbleInstr;

/**
 * @struct[FbleInstrV] Vector of FbleInstr.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleInstr**][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleInstr** xs;
} FbleInstrV;

/**
 * Magic number used by FbleCode.
 */
typedef enum {
  FBLE_CODE_MAGIC = 0xB01CE,
} FbleCodeMagic;

/**
 * @struct[FbleCode] Fble bytecode.
 *  @field[size_t][refcount] Reference count.
 *  @field[FbleCodeMagic][magic] FBLE_CODE_MAGIC
 *  @field[FbleExecutable][executable] FbleExecutable. Run function is unused.
 *  @field[FbleBlockId][profile_block_id]
 *   Id of the profile block for this code.
 *  @field[size_t][num_locals]
 *   Number of local variable slots used/required.
 *  @field[FbleInstrV][instrs] The instructions to execute.
 */
struct FbleCode {
  size_t refcount;
  FbleCodeMagic magic;
  FbleExecutable executable;
  FbleBlockId profile_block_id;
  size_t num_locals;
  FbleInstrV instrs;
};

/**
 * @struct[FbleCodeV] Vector of FbleCode.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleCode**][xs] The elements.
 */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleCode** xs;    /**< The elements. */
} FbleCodeV;

/**
 * @struct[FbleStructValueINstr]
 * @ FBLE_STRUCT_VALUE_INSTR: Creates a struct value.
 *  @code[txt] @
 *   *dest = struct(a1, a2, ..., aN)
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[FbleVarV][args] Arguments to the struct value.
 *  @field[FbleLocalIndex][dest] Where to put the created value.
 */
typedef struct {
  FbleInstr _base;
  FbleVarV args;
  FbleLocalIndex dest;
} FbleStructValueInstr;

/**
 * @struct[FbleUnionValueInstr]
 * @ FBLE_UNION_VALUE_INSTR: Creates a union value.
 *  @code[txt] @
 *   *dest = union(arg)
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[size_t][tagwidth] The number of bits needed to hold the tag.
 *  @field[size_t][tag] The tag of the value to create.
 *  @field[FbleVar][arg] The argument to the value to create.
 *  @field[FbleLocalIndex][dest] Where to put the created value.
 */
typedef struct {
  FbleInstr _base;
  size_t tagwidth;
  size_t tag;
  FbleVar arg;
  FbleLocalIndex dest;
} FbleUnionValueInstr;

/**
 * @struct[FbleAccessInstr] Accesses a tagged field from an object.
 *  Used for both FBLE_STRUCT_ACCESS_INSTR and FBLE_UNION_ACCESS_INSTR.
 *
 *  @code[txt] @
 *   *dest = obj.tag
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[FbleLoc][loc] Location of the access, for error reporting.
 *  @field[FbleVar][obj] The object whose field to access.
 *  @field[size_t][fieldc] The number of fields in the type.
 *  @field[size_t][tagwidth] The number of bits needed for the tag.
 *  @field[size_t][tag] The field to access.
 *  @field[FbleLocalIndex][dest] Where to store the result.
 */
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  FbleVar obj;
  size_t fieldc;
  size_t tagwidth;
  size_t tag;
  FbleLocalIndex dest;
} FbleAccessInstr;

/**
 * @struct[FbleOffsetV] Vector of offsets.
 *  @field[size_t][size] Number of elements.
 *  @field[size_t*][xs] The elements.
 */
typedef struct {
  size_t size;
  size_t* xs;
} FbleOffsetV;

/**
 * @struct[FbleBranchTarget] Specifies a target for branch.
 *  If object has the given tag, go to the absolute pc target.
 *
 *  @field[size_t][tag] The condition of the branch.
 *  @field[size_t][target] The target of the branch.
 */
typedef struct {
  size_t tag;
  size_t target;
} FbleBranchTarget;

/**
 * @struct[FbleBranchTargetV] Vector of FbleBranchTarget.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleBranchTarget*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleBranchTarget* xs;
} FbleBranchTargetV;

/**
 * @struct[FbleUnionSelectInstr] FBLE_UNION_SELECT_INSTR
 *  Branches based on object tag.
 *
 *  @code[txt] @
 *   next_pc = ?(condition.tag;
 *                targets[0].tag: targets[0].target,
 *                targets[1].tag: targets[1].target,
 *                ...
 *                : default_);
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[FbleLoc][loc] Location to use for error reporting.
 *  @field[FbleVar][condition] The object to branch based on.
 *  @field[size_t][tagwidth] Number of bits needed for the tag.
 *  @field[size_t][num_tags] Number of possible tag values.
 *  @field[FbleBranchTargetV][targets]
 *   Non-default branch targets. Sorted in increasing order of tag.
 *  @field[size_t][default_] Default branch target.
 */
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  FbleVar condition;
  size_t tagwidth;
  size_t num_tags;
  FbleBranchTargetV targets;
  size_t default_;
} FbleUnionSelectInstr;

/**
 * @struct[FbleGotoInstr] FBLE_GOTO_INSTR: Jump to a given address.
 *  @code[txt] @
 *   next_pc = target
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[size_t][target] Absolute pc to jump to.
 */
typedef struct {
  FbleInstr _base;
  size_t target;
} FbleGotoInstr;

/**
 * @struct[FbleFuncValueInstr] FBLE_FUNC_VALUE_INSTR
 *  Creates a function value.
 *
 *  @code[txt] @
 *   *dest = code[v1, v2, ...](argc)
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[FbleLocalIndex][dest] Where to store the allocated function.
 *  @field[FbleBlockId][profile_block_offset]
 *   The profile_block_id of the function, relative to the profile_block_id of
 *   the currently executing function.
 *  @field[FbleCode*][code]
 *   A block of instructions that executes the body of the function in the
 *   context of its scope and arguments. The instruction should remove the
 *   context of its scope and arguments.
 *  @field[FbleVarV][scope]
 *   Variables from the scope to capture for the function.
 */
typedef struct {
  FbleInstr _base;
  FbleLocalIndex dest;
  FbleBlockId profile_block_offset;
  FbleCode* code;
  FbleVarV scope;
} FbleFuncValueInstr;

/**
 * @struct[FbleCallInstr] FBLE_CALL_INSTR: Calls a function.
 *  @code[txt] @
 *   *dest = func(args[0], args[1], ...)
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[FbleLoc][loc] Location of the call for error reporting.
 *  @field[FbleVar][func] The function to call.
 *  @field[FbleVarV][args] The arguments to pass to the called function.
 *  @field[FbleLocalIndex][dest] Where to store the result of the call.
 */
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  FbleVar func;
  FbleVarV args;
  FbleLocalIndex dest;
} FbleCallInstr;

/**
 * @struct[FbleTailCallInstr] FBLE_TAIL_CALL_INSTR: Tail calls a function.
 *  @code[txt] @
 *   return func(args[0], args[1], ...)
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[FbleLoc][loc] Location of the call for error reporting.
 *  @field[FbleVar][func] The function to call.
 *  @field[FbleVarV][args] The arguments to pass to the called function.
 */
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  FbleVar func;
  FbleVarV args;
} FbleTailCallInstr;

/**
 * @struct[FbleCopyInstr] FBLE_COPY_INSTR
 *  Copies a value from one location to another.
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[FbleVar][source] The value to copy.
 *  @field[FbleLocalIndex][dest] Where to copy the value to.
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  FbleVar source;       /**< The value to copy. */
  FbleLocalIndex dest;  /**< Where to copy the value to. */
} FbleCopyInstr;

/**
 * @struct[FbleRecDeclInstr] FBLE_REC_DECL_INSTR: Declares recursive values.
 *  @code[txt] @
 *   *dest = FbleDeclareRecursiveValues(n);
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[size_t][n] The number of values to declare.
 *  @field[FbleLocalIndex][dest] Where to put the created declaration.
 */
typedef struct {
  FbleInstr _base;
  size_t n;
  FbleLocalIndex dest;
} FbleRecDeclInstr;

/**
 * @struct[FbleRecDefnInstr] FBLE_REC_DEFN_INSTR
 *  Defines recursive values.
 *
 *  @code[txt] @
 *   FbleDefineRecursiveValues(decl, defn)
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[FbleLocalIndex][decl] The declaration.
 *  @field[FbleLocalIndex][defn] The definition.
 *  @field[FbleLocV][locs]
 *   Location associated with each defined variable, for error reporting.
 */
typedef struct {
  FbleInstr _base;
  FbleLocalIndex decl;
  FbleLocalIndex defn;
  FbleLocV locs;
} FbleRecDefnInstr;

/**
 * @struct[FbleReturnInstr] FBLE_RETURN_INSTR
 *  Returns 'result' and exits the current stack frame.
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[FbleVar][result] The value to return.
 */
typedef struct {
  FbleInstr _base;
  FbleVar result;
} FbleReturnInstr;

/**
 * @struct[FbleTypeInstr] FBLE_TYPE_INSTR: Creates a type value.
 *  @code[txt] @
 *   *dest = @<>
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[FbleLocalIndex][dest] Where to put the created value.
 */
typedef struct {
  FbleInstr _base;
  FbleLocalIndex dest;
} FbleTypeInstr;

/**
 * @struct[FbleListInstr] FBLE_LIST_INSTR: Creates a list value.
 *  @code[txt] @
 *   *dest = [a1, a2, ..., aN]
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[FbleVarV][args] The elements of the list to create.
 *  @field[FbleLocalIndex][dest] Where to put the created list.
 */
typedef struct {
  FbleInstr _base;
  FbleVarV args;
  FbleLocalIndex dest;
} FbleListInstr;

/**
 * @struct[FbleLiteralInstr] FBLE_LITERAL_INSTR: Creates a literal value.
 *  @code[txt] @
 *   *dest = "xxx"
 *
 *  @field[FbleInstr][_base] FbleInstr base class.
 *  @field[size_t][tagwidth] The number of bits needed for the tag of a letter.
 *  @field[FbleTagV][letters] The letters to create the literal from.
 *  @field[FbleLocalIndex][dest] Where to put the created value.
 */
typedef struct {
  FbleInstr _base;
  size_t tagwidth;
  FbleTagV letters;
  FbleLocalIndex dest;
} FbleLiteralInstr;

/**
 * @struct[FbleNopInstr] FBLE_NOP_INSTR: Does nothing.
 *  This is used for a particular case where we need to force profiling
 *  operations to run at a certain point in the code.
 *
 *  @field[FbleInstr][_base] FbleInstr base clsas.
 */
typedef struct {
  FbleInstr _base;
} FbleNopInstr;

/**
 * @struct[FbleUndefInstr] FBLE_UNDEF_INSTR: Creates an undefined value.
 *  @code[txt] @
 *   *dest = NULL
 *
 *  @field[FbleInstr][_base] FbleInstr base clsas.
 *  @field[FbleLocalIndex][dest] Where to store the undefined value.
 */
typedef struct {
  FbleInstr _base;
  FbleLocalIndex dest;
} FbleUndefInstr;

/**
 * @func[FbleRawAllocInstr]
 * @ Allocates and partially initialize an FbleInstr.
 *  This function is not type safe. It is recommended to use the FbleAllocInstr
 *  and FbleAllocInstrExtra macros instead.
 *
 *  @arg[size_t][size]
 *   The total number of bytes to allocate for the instruction.
 *  @arg[FbleInstrTag][tag] The instruction tag.
 *
 *  @returns[void*]
 *   A pointer to a newly allocated size bytes of memory with FbleInstr tag,
 *   debug_info, and profile_ops initialized.
 */
void* FbleRawAllocInstr(size_t size, FbleInstrTag tag);

/**
 * @func[FbleAllocInstr] Allocates an FbleInstr in a type safe way.
 *  @arg[<type>][T] The type of instruction to allocate.
 *  @arg[FbleInstrTag][tag] The tag of the instruction to allocate.
 *
 *  @returns[T*]
 *   A pointer to a newly allocated object of the given type with the
 *   FbleInstr tag, debug_info, and profile_ops fields initialized.
 *
 *  @sideeffects
 *   The allocation should be freed by calling FbleFreeInstr when no longer in
 *   use.
 */
#define FbleAllocInstr(T, tag) ((T*) FbleRawAllocInstr(sizeof(T), tag))

/**
 * @func[FbleAllocInstrExtra]
 * @ Allocates an FbleInstr with additional extra space.
 *  @arg[<type>][T] The type of instruction to allocate.
 *  @arg[size_t][size] The size in bytes of extra space to include.
 *  @arg[FbleInstrTag][tag] The tag of the instruction.
 *
 *  @returns[T*]
 *   A pointer to a newly allocated instruction of the given type with extra
 *   size, with FbleInstr fields initialized.
 *
 *  @sideeffects
 *   The allocation should be freed by calling FbleFreeInstr when no longer in
 *   use.
 */
#define FbleAllocInstrExtra(T, size, tag) ((T*) FbleRawAllocInstr(sizeof(T) + size), tag)

/**
 * @func[FbleFreeInstr] Frees an FbleInstr.
 *  @arg[FbleInstr*][instr] The instruction to free.
 *
 *  @sideeffects
 *   Frees memory allocated for the given instruction.
 */
void FbleFreeInstr(FbleInstr* instr);

/**
 * @func[FbleNewCode] Allocates a new, empty FbleCode instance.
 *  @arg[size_t][num_args] The number of arguments to the function.
 *  @arg[size_t][num_statics] The number of statics captured by the function.
 *  @arg[size_t][num_locals] The number of locals used by the function.
 *  @arg[FbleBlockId][profile_block_id]
 *   The profile block to use for this function.
 *
 *  @returns[FbleCode*]
 *   A newly allocated FbleCode object with no initial instructions.
 *
 *  @sideeffects
 *   Allocates a new FbleCode object that should be freed with FbleFreeCode or
 *   FbleFreeExecutable when no longer needed.
 */
FbleCode* FbleNewCode(size_t num_args, size_t num_statics, size_t num_locals, FbleBlockId profile_block_id);

/**
 * @func[FbleFreeCode] Frees an FbleCode.
 *  Decrements the refcount on the given block of instructions and free it if
 *  appropriate.
 *
 *  @arg[FbleCode*][code] The code to free. May be NULL.
 *
 *  @sideeffects
 *   Frees memory allocated for the given block of instruction if the refcount
 *   has gone to 0.
 */
void FbleFreeCode(FbleCode* code);

#endif // FBLE_INTERNAL_CODE_H_
