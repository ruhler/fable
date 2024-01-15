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
 * Singly-linked list of profiling operations.
 */
typedef struct FbleProfileOp {
  /** The profiling operation. */
  FbleProfileOpTag tag;

  /** Block to enter or replace. Time to sample. Unused for EXIT ops. */
  size_t arg;

  /** Next profile op in the list, or NULL for end of list. */
  struct FbleProfileOp* next;
} FbleProfileOp;

/**
 * Different kinds of FbleDebugInfos.
 */
typedef enum {
  FBLE_STATEMENT_DEBUG_INFO,
  FBLE_VAR_DEBUG_INFO,
} FbleDebugInfoTag;

/** Singly-linked list of debug info. */
typedef struct FbleDebugInfo {
  FbleDebugInfoTag tag;         /**< The kind of debug info. */
  struct FbleDebugInfo* next;   /**< The next element in the list, or NULL. */
} FbleDebugInfo;

/**
 * Debug info indicating start of a new statement.
 *
 * FBLE_STATEMENT_DEBUG_INFO
 */
typedef struct {
  FbleDebugInfo _base;    /**< FbleDebugInfo base class. */
  FbleLoc loc;            /**< The source code location of the statement. */
} FbleStatementDebugInfo;

/**
 * Debug info about a variable.
 *
 * FBLE_VAR_DEBUG_INFO
 */
typedef struct {
  FbleDebugInfo _base;  /**< FbleDebugInfo base class. */
  FbleName name;        /**< Name of the variable. */
  FbleVar var;          /**< Location of the variable in the stack frame. */
} FbleVarDebugInfo;

/**
 * Frees the given chain of debug infos.
 *
 * @param info  The chain of debug infos to free. May be NULL.
 * @sideeffects  Frees memory allocated for the given debug infos.
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
  FBLE_REF_VALUE_INSTR,
  FBLE_REF_DEF_INSTR,
  FBLE_RETURN_INSTR,
  FBLE_TYPE_INSTR,
  FBLE_RETAIN_INSTR,
  FBLE_RELEASE_INSTR,
  FBLE_LIST_INSTR,
  FBLE_LITERAL_INSTR,
  FBLE_NOP_INSTR,
} FbleInstrTag;

/**
 * Base type for all instructions.
 */
typedef struct {
  FbleInstrTag tag;           /**< The kind of instruction. */

  /** Debug info that applies to just before executing the instruction. */
  FbleDebugInfo* debug_info;  

  /** Profiling operations to perform before executing the instruction. */
  FbleProfileOp* profile_ops;
} FbleInstr;

/** Vector of FbleInstr. */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleInstr** xs;   /**< The elements. */
} FbleInstrV;

/**
 * Magic number used by FbleCode.
 */
typedef enum {
  FBLE_CODE_MAGIC = 0xB01CE,
} FbleCodeMagic;

/**
 * Fble bytecode.
 */
struct FbleCode {
  size_t refcount;        /**< Reference count. */
  FbleCodeMagic magic;    /**< FBLE_CODE_MAGIC */
  FbleExecutable executable;   /**< FbleExecutable. Run function is unused. */
  size_t num_locals;      /**< Number of local variable slots used/required. */
  FbleInstrV instrs;      /**< The instructions to execute. */
};

/** Vector of FbleCode. */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleCode** xs;    /**< The elements. */
} FbleCodeV;

/**
 * FBLE_STRUCT_VALUE_INSTR: Creates a struct value.
 *
 * *dest = struct(a1, a2, ..., aN)
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  FbleVarV args;        /**< Arguments to the struct value. */
  FbleLocalIndex dest;  /**< Where to put the created value. */
} FbleStructValueInstr;

/**
 * FBLE_UNION_VALUE_INSTR: Creates a union value.
 *
 * *dest = union(arg)
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  size_t tag;           /**< The tag of of the value to create. */
  FbleVar arg;          /**< The argument to the value to create. */
  FbleLocalIndex dest;  /**< Where to put the created value. */
} FbleUnionValueInstr;

/**
 * Accesses a tagged field from an object.
 *
 * Used for both FBLE_STRUCT_ACCESS_INSTR and FBLE_UNION_ACCESS_INSTR.
 *
 * *dest = obj.tag
 *
 * The resulting value is not implicitly retained. Use an explicit retain
 * instruction to keep the returned value alive if desired.
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  FbleLoc loc;          /**< Location of the access, for error reporting. */
  FbleVar obj;          /**< The object whose field to access. */
  size_t tag;           /**< The field to access. */
  FbleLocalIndex dest;  /**< Where to store the result. */
} FbleAccessInstr;

/** Vector of offsets. */
typedef struct {
  size_t size;    /**< Number of elements. */
  size_t* xs;     /**< The elements. */
} FbleOffsetV;

/**
 * Specifies a target for branch.
 *
 * If object has the given tag, go to the absolute pc target.
 */
typedef struct {
  size_t tag;
  size_t target;
} FbleBranchTarget;

typedef struct {
  size_t size;
  FbleBranchTarget* xs;
} FbleBranchTargetV;

/**
 * FBLE_UNION_SELECT_INSTR: Branches based on object tag.
 *
 * next_pc = ?(condition.tag;
 *              targets[0].tag: targets[0].target,
 *              targets[1].tag: targets[1].target,
 *              ...
 *              : default_);
 *
 * targets is sorted in increasing order of tag.
 */
typedef struct {
  FbleInstr _base;        /**< FbleInstr base class. */
  FbleLoc loc;            /**< Location to use for error reporting. */
  FbleVar condition;      /**< The object to branch based on. */
  size_t num_tags;        /**< Number of possible tag values. */
  FbleBranchTargetV targets;  /**< Non-default branch targets. */
  size_t default_;        /**< Default branch target. */
} FbleUnionSelectInstr;

/**
 * FBLE_GOTO_INSTR: Jump to a given address.
 *
 * next_pc = target
 */
typedef struct {
  FbleInstr _base;    /**< FbleInstr base class. */
  size_t target;      /**< Absolute pc to jump to. */
} FbleGotoInstr;

/**
 * FBLE_FUNC_VALUE_INSTR: Creates a function value.
 *
 * *dest = code[v1, v2, ...](argc)
 */
typedef struct {
  /** FbleInstr base class. */
  FbleInstr _base;
  
  /** Where to store the allocated function. */
  FbleLocalIndex dest;

  /**
   * A block of instructions that executes the body of the function in the
   * context of its scope and arguments. The instruction should remove the
   * context of its scope and arguments.
   */
  FbleCode* code;

  /** Variables from the scope to capture for the function. */
  FbleVarV scope;
} FbleFuncValueInstr;

/**
 * FBLE_CALL_INSTR: Calls a function.
 *
 * *dest = func(args[0], args[1], ...)
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  FbleLoc loc;          /**< Location of the call for error reporting. */
  FbleVar func;         /**< The function to call. */
  FbleVarV args;        /**< The arguments to pass to the called function. */
  FbleLocalIndex dest;  /**< Where to store the result of the call. */
} FbleCallInstr;

/**
 * FBLE_TAIL_CALL_INSTR: Tail calls a function.
 *
 * return func(args[0], args[1], ...)
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  FbleLoc loc;          /**< Location of the call for error reporting. */
  FbleVar func;         /**< The function to call. */
  FbleVarV args;        /**< The arguments to pass to the called function. */
} FbleTailCallInstr;

/**
 * FBLE_COPY_INSTR: Copies a value from one location to another.
 *
 * Does not increment the ref count. If refcount increment is required, use an
 * explicit retain instruction.
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  FbleVar source;       /**< The value to copy. */
  FbleLocalIndex dest;  /**< Where to copy the value to. */
} FbleCopyInstr;

/**
 * FBLE_REF_VALUE_INSTR: Creates a ref value.
 *
 * *dest = new ref
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  FbleLocalIndex dest;  /**< Where to put the created value. */
} FbleRefValueInstr;

/**
 * FBLE_REF_DEF_INSTR: Sets the value of a reference.
 *
 * ref->value = value
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  FbleLoc loc;          /**< Location to user for error reporting. */
  FbleLocalIndex ref;   /**< The ref value to update. */
  FbleVar value;        /**< The updated target for the ref value. */
} FbleRefDefInstr;

/**
 * FBLE_RETURN_INSTR: Returns 'result' and exits the current stack frame.
 *
 * Does not implicitly retain the value to be returned. If the value needs to
 * be retained before returning, use an explicit return instruction.
 *
 */
typedef struct {
  FbleInstr _base;    /**< FbleInstr base class. */
  FbleVar result;     /**< The value to return. */
} FbleReturnInstr;

/**
 * FBLE_TYPE_INSTR: Creates a type value.
 *
 * *dest = @<>
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  FbleLocalIndex dest;  /**< Where to put the created value. */
} FbleTypeInstr;

/**
 * FBLE_RETAIN_INSTR: Retains an fble value.
 *
 * FbleRetainValue(target)
 */
typedef struct {
  FbleInstr _base;          /**< FbleInstr base class. */
  FbleVar target;           /**< The value to reatin. */
} FbleRetainInstr;

/**
 * FBLE_RELEASE_INSTR: Releases fble values.
 *
 * FbleReleaseValue(targets)
 */
typedef struct {
  FbleInstr _base;          /**< FbleInstr base class. */
  FbleLocalIndexV targets;  /**< The values to release. */
} FbleReleaseInstr;

/**
 * FBLE_LIST_INSTR: Creates a list value.
 *
 * *dest = [a1, a2, ..., aN]
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  FbleVarV args;        /**< The elements of the list to create. */
  FbleLocalIndex dest;  /**< Where to put the created list. */
} FbleListInstr;

/**
 * FBLE_LITERAL_INSTR: Creates a literal value.
 *
 * *dest = "xxx"
 */
typedef struct {
  FbleInstr _base;      /**< FbleInstr base class. */
  FbleTagV letters;     /**< The letters to create the literal from. */
  FbleLocalIndex dest;  /**< Where to put the created value. */
} FbleLiteralInstr;

/**
 * FBLE_NOP_INSTRUCTION: Does nothing.
 *
 * This is used for a particular case where we need to force profiling
 * operations to run at a certain point in the code.
 */
typedef struct {
  FbleInstr _base;
} FbleNopInstr;

/**
 * Allocates and partially initialize an FbleInstr.
 *
 * This function is not type safe. It is recommended to use the FbleAllocInstr
 * and FbleAllocInstrExtra macros instead.
 *
 * @param size  The total number of bytes to allocate for the instruction.
 * @param tag  The instruction tag.
 *
 * @returns
 *   A pointer to a newly allocated size bytes of memory with FbleInstr tag,
 *   debug_info, and profile_ops initialized.
 */
void* FbleRawAllocInstr(size_t size, FbleInstrTag tag);

/**
 * Allocates an FbleInstr in a type safe way.
 *
 * @param T  The type of instruction to allocate.
 * @param tag  The tag of the instruction to allocate.
 *
 * @returns
 *   A pointer to a newly allocated object of the given type with the
 *   FbleInstr tag, debug_info, and profile_ops fields initialized.
 *
 * @sideeffects
 * * The allocation should be freed by calling FbleFreeInstr when no longer in
 *   use.
 */
#define FbleAllocInstr(T, tag) ((T*) FbleRawAllocInstr(sizeof(T), tag))

/**
 * Allocate an FbleInstr with additional extra space.
 *
 * @param T  The type of instruction to allocate.
 * @param size  The size in bytes of extra space to include.
 * @param tag  The tag of the instruction.
 *
 * @returns
 *   A pointer to a newly allocated instruction of the given type with extra
 *   size, with FbleInstr fields initialized.
 *
 * @sideeffects
 * * The allocation should be freed by calling FbleFreeInstr when no longer in
 *   use.
 */
#define FbleAllocInstrExtra(T, size, tag) ((T*) FbleRawAllocInstr(sizeof(T) + size), tag)

/**
 * Frees an FbleInstr.
 *
 * @param instr  The instruction to free.
 *
 * @sideeffects
 *   Frees memory allocated for the given instruction.
 */
void FbleFreeInstr(FbleInstr* instr);

/**
 * Allocates a new, empty FbleCode instance.
 *
 * @param num_args  The number of arguments to the function.
 * @param num_statics  The number of statics captured by the function.
 * @param num_locals  The number of locals used by the function.
 * @param profile_block_id  The profile block to use for this function.
 *
 * @returns
 *   A newly allocated FbleCode object with no initial instructions.
 *
 * @sideeffects
 *   Allocates a new FbleCode object that should be freed with FbleFreeCode or
 *   FbleFreeExecutable when no longer needed.
 */
FbleCode* FbleNewCode(size_t num_args, size_t num_statics, size_t num_locals, FbleBlockId profile_block_id);

/**
 * Frees an FbleCode.
 *
 * Decrements the refcount on the given block of instructions and free it if
 * appropriate.
 *
 * @param code  The code to free. May be NULL.
 *
 * @sideeffects
 *   Frees memory allocated for the given block of instruction if the refcount
 *   has gone to 0.
 */
void FbleFreeCode(FbleCode* code);

#endif // FBLE_INTERNAL_CODE_H_
