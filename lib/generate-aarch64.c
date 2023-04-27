/**
 * @file generate-aarch64.c
 * Converts FbleCode fble bytecode to 64-bit ARM machine code.
 */

#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdio.h>    // for sprintf
#include <stdarg.h>   // for va_list, va_start, va_end.
#include <stddef.h>   // for offsetof
#include <string.h>   // for strlen, strcat
#include <unistd.h>   // for getcwd

#include <fble/fble-compile.h>
#include <fble/fble-vector.h>    // for FbleVectorInit, etc.

#include "code.h"
#include "tc.h"
#include "unreachable.h"
#include "value.h"

/** Type representing a name as an integer. */
typedef unsigned int LabelId;

/** Printf format string for printing label. */
#define LABEL ".L.%x"

/** A vector of strings. */
typedef struct {
  size_t size;        /**< Number of elements. */
  const char** xs;    /**< The elements. */
} LocV;

/**
 * Stack frame layout for _Run_ functions.
 *
 * Note: The size of this struct must be a multiple of 16 bytes to avoid bus
 * errors.
 */
typedef struct {
  void* FP;                 /**< Frame pointer. */
  void* LR;                 /**< Link register. */
  void* r_heap_save;        /**< Saved contents of R_HEAP reg. */
  void* r_thread_save;      /**< Saved contents of R_THREAD reg. */
  void* r_locals_save;      /**< Saved contents of R_LOCALS reg. */
  void* r_args_save;        /**< Saved contents of R_ARGS reg. */
  void* r_statics_save;     /**< Saved contents of R_STATICS reg. */
  void* r_profile_block_offset_save;  /**< Saved contents of R_PROFILE_BLOCK_OFFSET reg. */
  void* r_profile_save;     /**< Saved contents of R_PROFILE reg. */
  void* r_scratch_0_save;   /**< Saved contents of R_SCRATCH_0 reg. */
  void* locals[];           /**< Local variables. */
} RunStackFrame;

static void AddLoc(const char* source, LocV* locs);
static void CollectBlocksAndLocs(FbleCodeV* blocks, LocV* locs, FbleCode* code);

static void StringLit(FILE* fout, const char* string);
static LabelId StaticString(FILE* fout, LabelId* label_id, const char* string);
static LabelId StaticNames(FILE* fout, LabelId* label_id, FbleNameV names);
static LabelId StaticModulePath(FILE* fout, LabelId* label_id, FbleModulePath* path);
static LabelId StaticExecutableModule(FILE* fout, LabelId* label_id, FbleCompiledModule* module);

static void GetFrameVar(FILE* fout, const char* rdst, FbleVar index);
static void SetFrameVar(FILE* fout, const char* rsrc, FbleLocalIndex index);
static void DoAbort(FILE* fout, void* code, size_t pc, const char* lmsg, FbleLoc loc);

static size_t StackBytesForCount(size_t count);

static void Adr(FILE* fout, const char* r_dst, const char* fmt, ...);


// Context for emitting binary search instructions for union select.
typedef struct {
  FILE* fout;
  void* code;
  size_t pc;
  size_t label;
} Context;

static const size_t NONE = -1;

// Interval info for emitting binary search instructions for union select.
typedef struct {
  size_t lo;                    // lowest possible tag, inclusive.
  size_t hi;                    // highest possible tag, inclusive.
  FbleBranchTarget* targets;    // branch targets in the interval.
  size_t num_targets;           // number of branch targets in the interval
  size_t default_;              // default target.
} Interval;

static size_t GetSingleTarget(Interval* interval);
static void EmitSearch(Context* context, Interval* interval);


static void EmitInstr(FILE* fout, FbleNameV profile_blocks, void* code, size_t pc, FbleInstr* instr);
static void EmitOutlineCode(FILE* fout, void* code, size_t pc, FbleInstr* instr);
static void EmitCode(FILE* fout, FbleNameV profile_blocks, FbleCode* code);
static void EmitInstrForAbort(FILE* fout, void* code, FbleInstr* instr);
static size_t SizeofSanitizedString(const char* str);
static void SanitizeString(const char* str, char* dst);
static FbleString* LabelForPath(FbleModulePath* path);

/**
 * Adds a source location to the list of locations.
 *
 * @param source  The source file name to add
 * @param locs  The list of locs to add to.
 *
 * @sideeffects
 *   Adds the source filename to the list of locations if it is not already
 *   present in the list.
 */
static void AddLoc(const char* source, LocV* locs)
{
  for (size_t i = 0; i < locs->size; ++i) {
    if (strcmp(source, locs->xs[i]) == 0) {
      return;
    }
  }
  FbleVectorAppend(*locs, source);
}

/**
 * Gets the list of blocks and locs referenced by a code block.
 *
 * Includes the the code block itself.
 *
 * @param blocks  The collection of blocks to add to.
 * @param locs  The collection of location source names to add to.
 * @param code  The code to collect the blocks from.
 *
 * @sideeffects
 * * Appends collected blocks to 'blocks'.
 * * Appends source file names to 'locs'.
 */
static void CollectBlocksAndLocs(FbleCodeV* blocks, LocV* locs, FbleCode* code)
{
  FbleVectorAppend(*blocks, code);
  for (size_t i = 0; i < code->instrs.size; ++i) {
    switch (code->instrs.xs[i]->tag) {
      case FBLE_DATA_TYPE_INSTR: break;
      case FBLE_STRUCT_VALUE_INSTR: break;
      case FBLE_UNION_VALUE_INSTR: break;

      case FBLE_STRUCT_ACCESS_INSTR:
      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* instr = (FbleAccessInstr*)code->instrs.xs[i];
        AddLoc(instr->loc.source->str, locs);
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* instr = (FbleUnionSelectInstr*)code->instrs.xs[i];
        AddLoc(instr->loc.source->str, locs);
        break;
      }

      case FBLE_GOTO_INSTR: break;

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* instr = (FbleFuncValueInstr*)code->instrs.xs[i];
        CollectBlocksAndLocs(blocks, locs, instr->code);
        break;
      }

      case FBLE_CALL_INSTR: {
        FbleCallInstr* instr = (FbleCallInstr*)code->instrs.xs[i];
        AddLoc(instr->loc.source->str, locs);
        break;
      }

      case FBLE_TAIL_CALL_INSTR: {
        FbleTailCallInstr* instr = (FbleTailCallInstr*)code->instrs.xs[i];
        AddLoc(instr->loc.source->str, locs);
        break;
      }

      case FBLE_COPY_INSTR: break;
      case FBLE_REF_VALUE_INSTR: break;

      case FBLE_REF_DEF_INSTR: {
        FbleRefDefInstr* instr = (FbleRefDefInstr*)code->instrs.xs[i];
        AddLoc(instr->loc.source->str, locs);
        break;
      }

      case FBLE_RETURN_INSTR: break;
      case FBLE_TYPE_INSTR: break;
      case FBLE_RETAIN_INSTR: break;
      case FBLE_RELEASE_INSTR: break;
      case FBLE_LIST_INSTR: break;
      case FBLE_LITERAL_INSTR: break;
      case FBLE_NOP_INSTR: break;
    }
  }
}

/**
 * Outputs a string literal to fout.
 *
 * @param fout  The file to write to.
 * @param string  The contents of the string to write.
 *
 * @sideeffects
 *   Outputs the given string as a C string literal to the given file.
 */
static void StringLit(FILE* fout, const char* string)
{
  fprintf(fout, "\"");
  for (const char* p = string; *p; p++) {
    // TODO: Handle other special characters too.
    switch (*p) {
      case '\n': fprintf(fout, "\\n"); break;
      case '"': fprintf(fout, "\\\""); break;
      case '\\': fprintf(fout, "\\\\"); break;
      default: fprintf(fout, "%c", *p); break;
    }
  }
  fprintf(fout, "\"");
}

/**
 * Outputs code to declare a static FbleString value.
 *
 * @param fout  The file to write to
 * @param label_id  Pointer to next available label id for use.
 * @param string  The value of the string.
 *
 * @returns
 *   A label id of a local, static FbleString.
 *
 * @sideeffects
 *   Writes code to fout and allocates label ids out of label_id.
 */
static LabelId StaticString(FILE* fout, LabelId* label_id, const char* string)
{
  LabelId id = (*label_id)++;

  fprintf(fout, "  .section .data\n");
  fprintf(fout, "  .align 3\n");                       // 64 bit alignment
  fprintf(fout, LABEL ":\n", id);
  fprintf(fout, "  .xword 1\n");                       // .refcount = 1
  fprintf(fout, "  .xword %i\n", FBLE_STRING_MAGIC);   // .magic
  fprintf(fout, "  .string ");                         // .str
  StringLit(fout, string);
  fprintf(fout, "\n");
  return id;
}

/**
 * Outputs code to declare a static FbleNameV.xs value.
 *
 * @param fout  The file to write to
 * @param label_id  Pointer to next available label id for use.
 * @param names  The value of the names.
 *
 * @returns
 *   A label id of a local, static FbleNameV.xs.
 *
 * @sideeffects
 *   Writes code to fout and allocates label ids out of label_id.
 */
static LabelId StaticNames(FILE* fout, LabelId* label_id, FbleNameV names)
{
  LabelId str_ids[names.size];
  LabelId src_ids[names.size];
  for (size_t i = 0; i < names.size; ++i) {
    str_ids[i] = StaticString(fout, label_id, names.xs[i].name->str);
    src_ids[i] = StaticString(fout, label_id, names.xs[i].loc.source->str);
  }

  LabelId id = (*label_id)++;
  fprintf(fout, "  .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", id);
  for (size_t i = 0; i < names.size; ++i) {
    fprintf(fout, "  .xword " LABEL "\n", str_ids[i]);    // name
    fprintf(fout, "  .word %i\n", names.xs[i].space);     // space
    fprintf(fout, "  .zero 4\n");                         // padding
    fprintf(fout, "  .xword " LABEL "\n", src_ids[i]);    // loc.src
    fprintf(fout, "  .word %i\n", names.xs[i].loc.line);  // loc.line
    fprintf(fout, "  .word %i\n", names.xs[i].loc.col);   // loc.col
  }
  return id;
}

/**
 * Generates code to declare a static FbleModulePath value.
 *
 * @param fout  The output stream to write the code to.
 * @param label_id  Pointer to next available label id for use.
 * @param path  The FbleModulePath to generate code for.
 *
 * @returns
 *   The label id of a local, static FbleModulePath.
 *
 * @sideeffects
 * * Outputs code to fout.
 * * Increments label_id based on the number of internal labels used.
 */
static LabelId StaticModulePath(FILE* fout, LabelId* label_id, FbleModulePath* path)
{
  LabelId src_id = StaticString(fout, label_id, path->loc.source->str);
  LabelId names_id = StaticNames(fout, label_id, path->path);
  LabelId path_id = (*label_id)++;

  fprintf(fout, "  .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", path_id);
  fprintf(fout, "  .xword 1\n");                  // .refcount
  fprintf(fout, "  .xword 2004903300\n");         // .magic
  fprintf(fout, "  .xword " LABEL "\n", src_id);        // path->loc.src
  fprintf(fout, "  .word %i\n", path->loc.line);
  fprintf(fout, "  .word %i\n", path->loc.col);
  fprintf(fout, "  .xword %zi\n", path->path.size);
  fprintf(fout, "  .xword " LABEL "\n", names_id);
  return path_id;
}

/**
 * Generates code to declare a static FbleExecutableModule value.
 *
 * @param fout  The output stream to write the code to.
 * @param label_id  Pointer to next available label id for use.
 * @param module  The FbleCompiledModule to generate code for.
 *
 * @returns
 *   The label id of a local, static FbleExecutableModule.
 *
 * @sideeffects
 * * Outputs code to fout.
 * * Increments label_id based on the number of internal labels used.
 */
static LabelId StaticExecutableModule(FILE* fout, LabelId* label_id, FbleCompiledModule* module)
{
  LabelId path_id = StaticModulePath(fout, label_id, module->path);

  LabelId dep_ids[module->deps.size];
  for (size_t i = 0; i < module->deps.size; ++i) {
    dep_ids[i] = StaticModulePath(fout, label_id, module->deps.xs[i]);
  }

  LabelId deps_xs_id = (*label_id)++;
  fprintf(fout, "  .section .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", deps_xs_id);
  for (size_t i = 0; i < module->deps.size; ++i) {
    fprintf(fout, "  .xword " LABEL "\n", dep_ids[i]);
  }

  LabelId executable_id = (*label_id)++;
  fprintf(fout, "  .section .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", executable_id);
  fprintf(fout, "  .xword 1\n");                          // .refcount
  fprintf(fout, "  .xword %i\n", FBLE_EXECUTABLE_MAGIC);  // .magic
  fprintf(fout, "  .xword %zi\n", module->code->_base.num_args);
  fprintf(fout, "  .xword %zi\n", module->code->_base.num_statics);
  fprintf(fout, "  .xword %zi\n", module->code->_base.tail_call_buffer_size);
  fprintf(fout, "  .xword %zi\n", module->code->_base.profile_block_id);

  FbleName function_block = module->profile_blocks.xs[module->code->_base.profile_block_id];
  char function_label[SizeofSanitizedString(function_block.name->str)];
  SanitizeString(function_block.name->str, function_label);
  fprintf(fout, "  .xword _Run.%p.%s\n", (void*)module->code, function_label);
  fprintf(fout, "  .xword FbleExecutableNothingOnFree\n");

  LabelId profile_blocks_xs_id = StaticNames(fout, label_id, module->profile_blocks);

  LabelId module_id = (*label_id)++;
  fprintf(fout, "  .section .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", module_id);
  fprintf(fout, "  .xword 1\n");                                  // .refcount
  fprintf(fout, "  .xword %i\n", FBLE_EXECUTABLE_MODULE_MAGIC);   // .magic
  fprintf(fout, "  .xword " LABEL "\n", path_id);                 // .path
  fprintf(fout, "  .xword %zi\n", module->deps.size);
  fprintf(fout, "  .xword " LABEL "\n", deps_xs_id);
  fprintf(fout, "  .xword " LABEL "\n", executable_id);
  fprintf(fout, "  .xword %zi\n", module->profile_blocks.size);
  fprintf(fout, "  .xword " LABEL "\n", profile_blocks_xs_id);
  return module_id;
}

/**
 * Generates code to read a variable from the current frame into register rdst.
 *
 * @param fout  The output stream
 * @param rdst  The name of the register to read the variable into
 * @param var  The variable to read.
 *
 * @sideeffects
 * * Writes to the output stream.
 */
static void GetFrameVar(FILE* fout, const char* rdst, FbleVar var)
{
  static const char* regs[] = { "R_STATICS", "R_ARGS", "R_LOCALS" };
  fprintf(fout, "  ldr %s, [%s, #%zi]\n", rdst, regs[var.tag], sizeof(FbleValue*) * var.index);
}

/**
 * Generates code to write a variable to the current frame from register rsrc.
 *
 * @param fout  The output stream
 * @param rsrc  The name of the register with the value to write.
 * @param index  The index of the value to write.
 *
 * @sideeffects
 * * Writes to the output stream.
 */
static void SetFrameVar(FILE* fout, const char* rsrc, FbleLocalIndex index)
{
  fprintf(fout, "  str %s, [R_LOCALS, #%zi]\n", rsrc, sizeof(FbleValue*) * index);
}

/**
 * Emits code to return an error from a Run function.
 *
 * @param fout  The output stream.
 * @param code  Pointer to current code block to use for labels.
 * @param pc  The program counter of the abort location.
 * @param lmsg  The label of the error message to use.
 * @param loc  The location to report with the error message.
 *
 * @sideeffects
 *   Emit code to return the error.
 */
static void DoAbort(FILE* fout, void* code, size_t pc, const char* lmsg, FbleLoc loc)
{
  // Print error message.
  Adr(fout, "x0", "stderr");
  fprintf(fout, "  ldr x0, [x0]\n");
  Adr(fout, "x1", ".L.ErrorFormatString");

  char label[SizeofSanitizedString(loc.source->str)];
  SanitizeString(loc.source->str, label);
  Adr(fout, "x2", ".L.loc.%s", label);

  fprintf(fout, "  mov x3, %i\n", loc.line);
  fprintf(fout, "  mov x4, %i\n", loc.col);
  Adr(fout, "x5", "%s", lmsg);
  fprintf(fout, "  bl fprintf\n");

  // Go to abort cleanup code.
  fprintf(fout, "  b .L._Abort_%p.pc.%zi\n", code, pc);
}

/**
 * Calculates a 16 byte aligned number of bytes sufficient to store count xwords.
 *
 * @param count  The number of xwords.
 *
 * @returns
 *   The number of bytes to allocate on the stack.
 *
 * @sideeffects
 *   None.
 */
static size_t StackBytesForCount(size_t count)
{
  return 16 * ((count + 1) / 2);
}

/**
 * Emits an adr instruction to load a label into a register.
 *
 * @param fout  The output stream
 * @param r_dst  The name of the register to load the label into
 * @param fmt  A printf format string for the label to load.
 * @param ...  Printf format arguments. 
 *
 * @sideeffects
 *   Emits a sequence of instructions to load the label into the register.
 */
static void Adr(FILE* fout, const char* r_dst, const char* fmt, ...)
{
  va_list ap;

  fprintf(fout, "  adrp %s, ", r_dst);
  va_start(ap, fmt);
  vfprintf(fout, fmt, ap);
  va_end(ap);
  fprintf(fout, "\n");

  fprintf(fout, "  add %s, %s, :lo12:", r_dst, r_dst);
  va_start(ap, fmt);
  vfprintf(fout, fmt, ap);
  va_end(ap);
  fprintf(fout, "\n");
}

/**
 * Tests whether the given search interval has a single possible target.
 *
 * @param interval  The specific search interval.
 *
 * @returns
 *  The target if there is a single possible target, NONE otherwise.
 */
static size_t GetSingleTarget(Interval* interval)
{
  if (interval->num_targets == 0) {
    return interval->default_;
  }

  if (interval->num_targets == 1 && interval->lo == interval->hi) {
    assert(interval->lo == interval->targets[0].tag);
    return interval->targets[0].target;
  }

  return NONE;
}

/**
 * Generates code to jump to a union select target.
 *
 * @param context  Context on what is being generated for.
 * @param interval  The specific interval in the search to generate.
 *
 * @sideeffects
 * * Outputs code to fout.
 */
static void EmitSearch(Context* context, Interval* interval)
{
  FILE* fout = context->fout;
  void* code = context->code;

  size_t target = GetSingleTarget(interval);
  if (target != NONE) {
    fprintf(fout, "  b .L._Run_%p.pc.%zi\n", code, target);
    return;
  }

  size_t mid = interval->num_targets / 2;
  size_t mid_tag = interval->targets[mid].tag;
  size_t mid_target = interval->targets[mid].target;
  fprintf(fout, "  cmp x0, %zi\n", mid_tag);
  fprintf(fout, "  b.eq .L._Run_%p.pc.%zi\n", code, mid_target);

  Interval low = {
    .lo = interval->lo,
    .hi = mid_tag - 1,
    .targets = interval->targets,
    .num_targets = mid,
    .default_ = interval->default_
  };

  Interval high = {
    .lo = mid_tag + 1,
    .hi = interval->hi,
    .targets = interval->targets + mid + 1,
    .num_targets = interval->num_targets - mid - 1,
    .default_ = interval->default_
  };

  if (interval->lo == mid_tag) {
    // The low interval is not possible. Go straight to the high interval.
    EmitSearch(context, &high);
    return;
  }

  if (mid_tag == interval->hi) {
    // The high interval is not possible. Go straigth to the low interval.
    EmitSearch(context, &low);
    return;
  }

  // Note: b.cc is 'carry clear', means unsigned less than.
  // See Table C1-1 on page C-195 of the aarch64 spec for details.
  size_t low_target = GetSingleTarget(&low);
  size_t low_label = NONE;
  if (low_target == NONE) {
    low_label = context->label++;
    fprintf(fout, "  b.cc .L._Run_%p.pc.%zi.%zi\n", code, context->pc, low_label);
  } else {
    fprintf(fout, "  b.cc .L._Run_%p.pc.%zi\n", code, low_target);
  }

  EmitSearch(context, &high);

  if (low_target == NONE) {
    fprintf(fout, ".L._Run_%p.pc.%zi.%zi:\n", code, context->pc, low_label);
    EmitSearch(context, &low);
  }
}

/**
 * Generates code to execute an instruction.
 *
 * @param fout  The output stream to write the code to.
 * @param profile_blocks  The list of profile block names for the module.
 * @param code  Pointer to the current code block, for referencing labels.
 * @param pc  The program counter of the instruction.
 * @param instr  The instruction to execute.
 *
 * @sideeffects
 * * Outputs code to fout.
 */
static void EmitInstr(FILE* fout, FbleNameV profile_blocks, void* code, size_t pc, FbleInstr* instr)
{
  // Emit dwarf location information for the instruction.
  for (FbleDebugInfo* info = instr->debug_info; info != NULL; info = info->next) {
    if (info->tag == FBLE_STATEMENT_DEBUG_INFO) {
      FbleStatementDebugInfo* stmt = (FbleStatementDebugInfo*)info;
      fprintf(fout, "  .loc 1 %i %i\n", stmt->loc.line, stmt->loc.col);
    }
  }

  if (instr->profile_ops != NULL) {
    fprintf(fout, "  cbnz R_PROFILE, .L.%p.%zi.prof\n", code, pc);
    fprintf(fout, ".L.%p.%zi.postprof:\n", code, pc);
  }

  switch (instr->tag) {
    case FBLE_DATA_TYPE_INSTR: {
      FbleDataTypeInstr* dt_instr = (FbleDataTypeInstr*)instr;
      size_t fieldc = dt_instr->fields.size;

      // Allocate space for the fields array on the stack.
      size_t sp_offset = StackBytesForCount(fieldc);
      fprintf(fout, "  sub SP, SP, %zi\n", sp_offset);
      for (size_t i = 0; i < fieldc; ++i) {
        GetFrameVar(fout, "x0", dt_instr->fields.xs[i]);
        fprintf(fout, "  str x0, [SP, #%zi]\n", sizeof(FbleValue*) * i);
      };

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, %i\n", dt_instr->kind);
      fprintf(fout, "  mov x2, %zi\n", fieldc);
      fprintf(fout, "  mov x3, SP\n");
      fprintf(fout, "  bl FbleNewDataTypeValue\n");
      SetFrameVar(fout, "x0", dt_instr->dest);

      fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      size_t argc = struct_instr->args.size;

      // Allocate space for the arguments array on the stack.
      size_t sp_offset = StackBytesForCount(argc);
      fprintf(fout, "  sub SP, SP, %zi\n", sp_offset);
      for (size_t i = 0; i < argc; ++i) {
        GetFrameVar(fout, "x0", struct_instr->args.xs[i]);
        fprintf(fout, "  str x0, [SP, #%zi]\n", sizeof(FbleValue*) * i);
      };

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, %zi\n", argc);
      fprintf(fout, "  mov x2, SP\n");
      fprintf(fout, "  bl FbleNewStructValue\n");
      SetFrameVar(fout, "x0", struct_instr->dest);

      fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, %zi\n", union_instr->tag);
      GetFrameVar(fout, "x2", union_instr->arg);
      fprintf(fout, "  bl FbleNewUnionValue\n");
      SetFrameVar(fout, "x0", union_instr->dest);
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      GetFrameVar(fout, "x0", access_instr->obj);
      fprintf(fout, "  bl FbleStrictValue\n");
      fprintf(fout, "  cbz x0, .L.%p.%zi.undef\n", code, pc);
      fprintf(fout, "  mov x1, #%zi\n", access_instr->tag);
      fprintf(fout, "  bl FbleStructValueAccess\n");
      SetFrameVar(fout, "x0", access_instr->dest);
      return;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      // Get the union value.
      GetFrameVar(fout, "x0", access_instr->obj);
      fprintf(fout, "  bl FbleStrictValue\n");

      // Abort if the union object is NULL.
      fprintf(fout, "  cbz x0, .L.%p.%zi.undef\n", code, pc);

      // Abort if the tag is wrong.
      fprintf(fout, "  mov R_SCRATCH_0, x0\n");
      fprintf(fout, "  bl FbleUnionValueTag\n");
      fprintf(fout, "  cmp x0, %zi\n", access_instr->tag);
      fprintf(fout, "  b.ne .L.%p.%zi.badtag\n", code, pc);

      // Access the field.
      fprintf(fout, "  mov x0, R_SCRATCH_0\n");
      fprintf(fout, "  bl FbleUnionValueAccess\n");
      SetFrameVar(fout, "x0", access_instr->dest);
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;

      // Get the union value.
      GetFrameVar(fout, "x0", select_instr->condition);
      fprintf(fout, "  bl FbleStrictValue\n");

      // Abort if the union object is NULL.
      fprintf(fout, "  cbz x0, .L.%p.%zi.undef\n", code, pc);


      // Binary search for the jump target based on the tag in x0.
      fprintf(fout, "  bl FbleUnionValueTag\n");
      Context context = { .fout = fout, .code = code, .pc = pc, .label = 0 };
      Interval interval = {
        .lo = 0, .hi = select_instr->num_tags - 1,
        .targets = select_instr->targets.xs,
        .num_targets = select_instr->targets.size,
        .default_ = select_instr->default_
      };

      EmitSearch(&context, &interval);
      return;
    }

    case FBLE_GOTO_INSTR: {
      FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
      fprintf(fout, "  b .L._Run_%p.pc.%zi\n", code, goto_instr->target);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;
      fprintf(fout, "  .section .data\n");
      fprintf(fout, "  .align 3\n");
      fprintf(fout, ".L._Run_%p.%zi.exe:\n", code, pc);
      fprintf(fout, "  .xword 1\n");                          // .refcount
      fprintf(fout, "  .xword %i\n", FBLE_EXECUTABLE_MAGIC);  // .magic
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.num_args);
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.num_statics);
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.tail_call_buffer_size);
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.profile_block_id);

      FbleName function_block = profile_blocks.xs[func_instr->code->_base.profile_block_id];
      char function_label[SizeofSanitizedString(function_block.name->str)];
      SanitizeString(function_block.name->str, function_label);
      fprintf(fout, "  .xword _Run.%p.%s\n", (void*)func_instr->code, function_label);
      fprintf(fout, "  .xword 0\n"); // .on_free

      fprintf(fout, "  .text\n");
      fprintf(fout, "  .align 2\n");

      // Allocate space for the statics array on the stack.
      size_t sp_offset = StackBytesForCount(func_instr->code->_base.num_statics);
      fprintf(fout, "  sub SP, SP, %zi\n", sp_offset);
      for (size_t i = 0; i < func_instr->code->_base.num_statics; ++i) {
        GetFrameVar(fout, "x0", func_instr->scope.xs[i]);
        fprintf(fout, "  str x0, [SP, #%zi]\n", sizeof(FbleValue*) * i);
      }

      fprintf(fout, "  mov x0, R_HEAP\n");
      Adr(fout, "x1", ".L._Run_%p.%zi.exe", code, pc);
      fprintf(fout, "  mov x2, R_PROFILE_BLOCK_OFFSET\n");
      fprintf(fout, "  mov x3, SP\n");
      fprintf(fout, "  bl FbleNewFuncValue\n");
      SetFrameVar(fout, "x0", func_instr->dest);

      fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
      return;
    }

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      GetFrameVar(fout, "x0", call_instr->func);
      fprintf(fout, "  mov R_SCRATCH_0, x0\n");
      fprintf(fout, "  bl FbleStrictValue\n");
      fprintf(fout, "  cbz x0, .L.%p.%zi.undef\n", code, pc);
      fprintf(fout, "  mov x2, x0\n");         // func

      // Allocate space for the arguments array on the stack.
      size_t sp_offset = StackBytesForCount(call_instr->args.size);
      fprintf(fout, "  sub SP, SP, %zi\n", sp_offset);
      for (size_t i = 0; i < call_instr->args.size; ++i) {
        GetFrameVar(fout, "x0", call_instr->args.xs[i]);
        fprintf(fout, "  str x0, [SP, #%zi]\n", sizeof(FbleValue*) * i);
      }

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, R_THREAD\n");
      fprintf(fout, "  mov x3, SP\n");          // args

      fprintf(fout, "  bl FbleThreadCall\n");
      SetFrameVar(fout, "x0", call_instr->dest);
      fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
      fprintf(fout, "  cbz x0, .L.%p.%zi.abort\n", code, pc);
      return;
    }

    case FBLE_TAIL_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      GetFrameVar(fout, "x0", call_instr->func);
      fprintf(fout, "  mov R_SCRATCH_0, x0\n");
      fprintf(fout, "  bl FbleStrictValue\n");
      fprintf(fout, "  cbz x0, .L.%p.%zi.undef\n", code, pc);
      fprintf(fout, "  mov x2, x0\n");         // func

      // Allocate space for the arguments array on the stack.
      size_t sp_offset = StackBytesForCount(call_instr->args.size);
      fprintf(fout, "  sub SP, SP, %zi\n", sp_offset);
      for (size_t i = 0; i < call_instr->args.size; ++i) {
        GetFrameVar(fout, "x0", call_instr->args.xs[i]);
        fprintf(fout, "  str x0, [SP, #%zi]\n", sizeof(FbleValue*) * i);
      }

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, R_THREAD\n");
      fprintf(fout, "  mov x3, SP\n");          // args

      // Note: Pass the original func value to FbleThreadTailCall, not the
      // strict value, so that FbleThreadTailCall can take ownership of the
      // original value.
      fprintf(fout, "  mov x2, R_SCRATCH_0\n"); // func
      fprintf(fout, "  bl FbleThreadTailCall\n");
      fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
      fprintf(fout, "  b .L._Run_.%p.exit\n", code);
      return;
    }

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      GetFrameVar(fout, "x1", copy_instr->source);
      SetFrameVar(fout, "x1", copy_instr->dest);
      return;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  bl FbleNewRefValue\n");
      SetFrameVar(fout, "x0", ref_instr->dest);
      return;
    }

    case FBLE_REF_DEF_INSTR: {
      FbleRefDefInstr* ref_instr = (FbleRefDefInstr*)instr;

      FbleVar ref = {
        .tag = FBLE_LOCAL_VAR,
        .index = ref_instr->ref
      };

      fprintf(fout, "  mov x0, R_HEAP\n");
      GetFrameVar(fout, "x1", ref);
      GetFrameVar(fout, "x2", ref_instr->value);
      fprintf(fout, "  bl FbleAssignRefValue\n");
      fprintf(fout, "  cbz x0, .L.%p.%zi.vacuous\n", code, pc);
      return;
    }

    case FBLE_RETURN_INSTR: {
      FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
      GetFrameVar(fout, "x0", return_instr->result);
      fprintf(fout, "  b .L._Run_.%p.exit\n", code);
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      Adr(fout, "x0", "FbleGenericTypeValue");
      fprintf(fout, "  ldr x0, [x0]\n");
      SetFrameVar(fout, "x0", type_instr->dest);
      return;
    }

    case FBLE_RETAIN_INSTR: {
      FbleRetainInstr* retain_instr = (FbleRetainInstr*)instr;
      fprintf(fout, "  mov x0, R_HEAP\n");
      GetFrameVar(fout, "x1", retain_instr->target);
      fprintf(fout, "  bl FbleRetainValue\n");
      return;
    }

    case FBLE_RELEASE_INSTR: {
      FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;

      size_t sp_offset = StackBytesForCount(release_instr->targets.size);
      fprintf(fout, "  sub SP, SP, #%zi\n", sp_offset);
      for (size_t i = 0; i < release_instr->targets.size; ++i) {
        FbleVar target = {
          .tag = FBLE_LOCAL_VAR,
          .index = release_instr->targets.xs[i]
        };
        GetFrameVar(fout, "x9", target);
        fprintf(fout, "  str x9, [SP, #%zi]\n", 8 * i);
      }

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, %zi\n", release_instr->targets.size);
      fprintf(fout, "  mov x2, SP\n");
      fprintf(fout, "  bl FbleReleaseValues\n");
      fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
      return;
    }

    case FBLE_LIST_INSTR: {
      FbleListInstr* list_instr = (FbleListInstr*)instr;
      size_t argc = list_instr->args.size;

      // Allocate space on the stack for the array of arguments.
      size_t sp_offset = StackBytesForCount(argc);
      fprintf(fout, "  sub SP, SP, #%zi\n", sp_offset);
      for (size_t i = 0; i < argc; ++i) {
        GetFrameVar(fout, "x9", list_instr->args.xs[i]);
        fprintf(fout, "  str x9, [SP, #%zi]\n", 8 * i);
      }

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, %zi\n", argc);
      fprintf(fout, "  mov x2, SP\n");
      fprintf(fout, "  bl FbleNewListValue\n");

      SetFrameVar(fout, "x0", list_instr->dest);
      fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
      return;
    }

    case FBLE_LITERAL_INSTR: {
      FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
      size_t argc = literal_instr->letters.size;

      fprintf(fout, "  .section .data\n");
      fprintf(fout, "  .align 3\n");
      fprintf(fout, ".L._Run_%p.%zi.letters:\n", code, pc);
      for (size_t i = 0; i < argc; ++i) {
        fprintf(fout, "  .xword %zi\n", literal_instr->letters.xs[i]);
      }

      fprintf(fout, "  .text\n");
      fprintf(fout, "  .align 2\n");
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, %zi\n", argc);
      Adr(fout, "x2", ".L._Run_%p.%zi.letters", code, pc);
      fprintf(fout, "  bl FbleNewLiteralValue\n");
      SetFrameVar(fout, "x0", literal_instr->dest);
      return;
    }

    case FBLE_NOP_INSTR: {
      // Nothing to do.
      return;
    }
  }
}

/**
 * Generates code that doesn't need to be in the main execution path.
 *
 * This code is referenced from the EmitInstr code in rare or unexpected
 * cases.
 *
 * @param fout  The output stream to write the code to.
 * @param profile_blocks  The list of profile block names for the module.
 * @param code  Pointer to the current code block, for referencing labels.
 * @param pc  The program counter of the instruction.
 * @param instr  The instruction to generate code for.
 *
 * @sideeffects
 * * Outputs code to fout.
 */
static void EmitOutlineCode(FILE* fout, void* code, size_t pc, FbleInstr* instr)
{
  if (instr->profile_ops != NULL) {
    fprintf(fout, ".L.%p.%zi.prof:\n", code, pc);
    for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
      switch (op->tag) {
        case FBLE_PROFILE_ENTER_OP: {
          fprintf(fout, "  mov x0, R_PROFILE\n");
          fprintf(fout, "  mov x1, R_PROFILE_BLOCK_OFFSET\n");
          fprintf(fout, "  add x1, x1, #%zi\n", op->arg);
          fprintf(fout, "  bl FbleProfileEnterBlock\n");
          break;
        }

        case FBLE_PROFILE_REPLACE_OP: {
          fprintf(fout, "  mov x0, R_PROFILE\n");
          fprintf(fout, "  mov x1, R_PROFILE_BLOCK_OFFSET\n");
          fprintf(fout, "  add x1, x1, #%zi\n", op->arg);
          fprintf(fout, "  bl FbleProfileReplaceBlock\n");
          break;
        }

        case FBLE_PROFILE_EXIT_OP: {
          fprintf(fout, "  mov x0, R_PROFILE\n");
          fprintf(fout, "  bl FbleProfileExitBlock\n");
          break;
        }

        case FBLE_PROFILE_SAMPLE_OP: {
          fprintf(fout, "  mov x0, R_PROFILE\n");
          fprintf(fout, "  mov x1, #%zi\n", op->arg);
          fprintf(fout, "  bl FbleProfileRandomSample\n");
          break;
        }
      }
    }
    fprintf(fout, "  b .L.%p.%zi.postprof\n", code, pc);
  }

  switch (instr->tag) {
    case FBLE_DATA_TYPE_INSTR: return;
    case FBLE_STRUCT_VALUE_INSTR: return;
    case FBLE_UNION_VALUE_INSTR: return;
    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, ".L.%p.%zi.undef:\n", code, pc);
      DoAbort(fout, code, pc, ".L.UndefinedStructValue", access_instr->loc);
      return;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, ".L.%p.%zi.undef:\n", code, pc);
      DoAbort(fout, code, pc, ".L.UndefinedUnionValue", access_instr->loc);

      fprintf(fout, ".L.%p.%zi.badtag:\n", code, pc);
      DoAbort(fout, code, pc, ".L.WrongUnionTag", access_instr->loc);
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;

      fprintf(fout, ".L.%p.%zi.undef:\n", code, pc);
      DoAbort(fout, code, pc, ".L.UndefinedUnionSelect", select_instr->loc);
      return;
    }

    case FBLE_GOTO_INSTR: return;
    case FBLE_FUNC_VALUE_INSTR: return;

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      fprintf(fout, ".L.%p.%zi.undef:\n", code, pc);
      DoAbort(fout, code, pc, ".L.UndefinedFunctionValue", call_instr->loc);

      fprintf(fout, ".L.%p.%zi.abort:\n", code, pc);
      DoAbort(fout, code, pc, ".L.CalleeAborted", call_instr->loc);
      return;
    }

    case FBLE_TAIL_CALL_INSTR: {
      FbleTailCallInstr* call_instr = (FbleTailCallInstr*)instr;
      fprintf(fout, ".L.%p.%zi.undef:\n", code, pc);
      DoAbort(fout, code, pc, ".L.UndefinedFunctionValue", call_instr->loc);

      fprintf(fout, ".L.%p.%zi.abort:\n", code, pc);
      DoAbort(fout, code, pc, ".L.CalleeAborted", call_instr->loc);
      return;
    }

    case FBLE_COPY_INSTR: return;
    case FBLE_REF_VALUE_INSTR: return;

    case FBLE_REF_DEF_INSTR: {
      FbleRefDefInstr* ref_instr = (FbleRefDefInstr*)instr;

      fprintf(fout, ".L.%p.%zi.vacuous:\n", code, pc);
      DoAbort(fout, code, pc, ".L.VacuousValue", ref_instr->loc);
      return;
    }

    case FBLE_RETURN_INSTR: return;
    case FBLE_TYPE_INSTR: return;
    case FBLE_RETAIN_INSTR: return;
    case FBLE_RELEASE_INSTR: return;
    case FBLE_LIST_INSTR: return;
    case FBLE_LITERAL_INSTR: return;
    case FBLE_NOP_INSTR: return;
  }
}

/**
 * Generates code to execute an FbleCode block.
 *
 * @param fout  The output stream to write the code to.
 * @param profile_blocks  The list of profile block names for the module.
 * @param code  The block of code to generate a C function for.
 *
 * @sideeffects
 * * Outputs code to fout with two space indent.
 */
static void EmitCode(FILE* fout, FbleNameV profile_blocks, FbleCode* code)
{
  fprintf(fout, "  .text\n");
  fprintf(fout, "  .align 2\n");
  FbleName function_block = profile_blocks.xs[code->_base.profile_block_id];
  char function_label[SizeofSanitizedString(function_block.name->str)];
  SanitizeString(function_block.name->str, function_label);
  fprintf(fout, "_Run.%p.%s:\n", (void*)code, function_label);

  // Output the location of the function.
  // This is intended to match the .loc info gcc outputs on the open brace of
  // a function body.
  fprintf(fout, "  .loc 1 %i %i\n", function_block.loc.line, function_block.loc.col);

  // Set up stack and frame pointer.
  size_t sp_offset = StackBytesForCount(code->num_locals);
  fprintf(fout, "  sub SP, SP, %zi\n", sp_offset);
  fprintf(fout, "  stp FP, LR, [SP, #-%zi]!\n", sizeof(RunStackFrame));
  fprintf(fout, "  mov FP, SP\n");

  // Save callee saved registers for later restoration.
  fprintf(fout, "  stp R_HEAP, R_THREAD, [SP, #%zi]\n", offsetof(RunStackFrame, r_heap_save));
  fprintf(fout, "  stp R_LOCALS, R_ARGS, [SP, #%zi]\n", offsetof(RunStackFrame, r_locals_save));
  fprintf(fout, "  stp R_STATICS, R_PROFILE_BLOCK_OFFSET, [SP, #%zi]\n", offsetof(RunStackFrame, r_statics_save));
  fprintf(fout, "  stp R_PROFILE, R_SCRATCH_0, [SP, #%zi]\n", offsetof(RunStackFrame, r_profile_save));

  // Set up common registers.
  fprintf(fout, "  mov R_HEAP, x0\n");
  fprintf(fout, "  mov R_THREAD, x1\n");
  fprintf(fout, "  mov R_ARGS, x3\n");
  fprintf(fout, "  mov R_STATICS, x4\n");
  fprintf(fout, "  mov R_PROFILE_BLOCK_OFFSET, x5\n");
  fprintf(fout, "  mov R_PROFILE, x6\n");
  fprintf(fout, "  add R_LOCALS, SP, #%zi\n", offsetof(RunStackFrame, locals));

  // Emit code for each fble instruction
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, ".L._Run_%p.pc.%zi:\n", (void*)code, i);
    EmitInstr(fout, profile_blocks, code, i, code->instrs.xs[i]);
  }

  // Restores stack and frame pointer and return whatever is in x0.
  fprintf(fout, ".L._Run_.%p.exit:\n", (void*)code);
  fprintf(fout, "  ldp R_HEAP, R_THREAD, [SP, #%zi]\n", offsetof(RunStackFrame, r_heap_save));
  fprintf(fout, "  ldp R_LOCALS, R_ARGS, [SP, #%zi]\n", offsetof(RunStackFrame, r_locals_save));
  fprintf(fout, "  ldp R_STATICS, R_PROFILE_BLOCK_OFFSET, [SP, #%zi]\n", offsetof(RunStackFrame, r_statics_save));
  fprintf(fout, "  ldp R_PROFILE, R_SCRATCH_0, [SP, #%zi]\n", offsetof(RunStackFrame, r_profile_save));
  fprintf(fout, "  ldp FP, LR, [SP], #%zi\n", sizeof(RunStackFrame));
  fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
  fprintf(fout, "  ret\n");

  // Emit code that's outside of the main execution path.
  for (size_t i = 0; i < code->instrs.size; ++i) {
    EmitOutlineCode(fout, code, i, code->instrs.xs[i]);
  }

  // Emit code for aborts:
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, ".L._Abort_%p.pc.%zi:\n", (void*)code, i);
    EmitInstrForAbort(fout, code, code->instrs.xs[i]);
  }


  fprintf(fout, ".L.%p.%s.high_pc:\n", (void*)code, function_label);
}

/**
 * Generates code to execute an instruction for the purposes of abort.
 *
 * @param fout  The output stream to write the code to.
 * @param code  Pointer to the current code block, for referencing labels.
 * @param pc  The program counter of the instruction.
 * @param instr  The instruction to execute.
 *
 * @sideeffects
 * * Outputs code to fout.
 */
static void EmitInstrForAbort(FILE* fout, void* code, FbleInstr* instr)
{
  switch (instr->tag) {
    case FBLE_DATA_TYPE_INSTR: {
      FbleDataTypeInstr* dt_instr = (FbleDataTypeInstr*)instr;
      SetFrameVar(fout, "XZR", dt_instr->dest);
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      SetFrameVar(fout, "XZR", struct_instr->dest);
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      SetFrameVar(fout, "XZR", union_instr->dest);
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      SetFrameVar(fout, "XZR", access_instr->dest);
      return;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      SetFrameVar(fout, "XZR", access_instr->dest);
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
      fprintf(fout, "  b .L._Abort_%p.pc.%zi\n", code, select_instr->default_);
      return;
    }

    case FBLE_GOTO_INSTR: {
      FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
      fprintf(fout, "  b .L._Abort_%p.pc.%zi\n", code, goto_instr->target);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;
      SetFrameVar(fout, "XZR", func_instr->dest);
      return;
    }

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      SetFrameVar(fout, "XZR", call_instr->dest);
      return;
    }

    case FBLE_TAIL_CALL_INSTR: {
      FbleTailCallInstr* call_instr = (FbleTailCallInstr*)instr;
      fprintf(fout, "  mov x0, R_HEAP\n");
      GetFrameVar(fout, "x1", call_instr->func);
      fprintf(fout, "  bl FbleReleaseValue\n");

      for (size_t i = 0; i < call_instr->args.size; ++i) {
        fprintf(fout, "  mov x0, R_HEAP\n");
        GetFrameVar(fout, "x1", call_instr->args.xs[i]);
        fprintf(fout, "  bl FbleReleaseValue\n");
      }

      fprintf(fout, "  mov x0, XZR\n");
      fprintf(fout, "  b .L._Run_.%p.exit\n", code);
      return;
    }

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      SetFrameVar(fout, "XZR", copy_instr->dest);
      return;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      SetFrameVar(fout, "XZR", ref_instr->dest);
      return;
    }

    case FBLE_REF_DEF_INSTR: {
      return;
    }

    case FBLE_RETURN_INSTR: {
      FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
      fprintf(fout, "  mov x0, R_HEAP\n");
      GetFrameVar(fout, "x1", return_instr->result);
      fprintf(fout, "  bl FbleReleaseValue\n");
      fprintf(fout, "  mov x0, XZR\n");
      fprintf(fout, "  b .L._Run_.%p.exit\n", code);
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      SetFrameVar(fout, "XZR", type_instr->dest);
      return;
    }

    case FBLE_RETAIN_INSTR: {
      FbleRetainInstr* retain_instr = (FbleRetainInstr*)instr;
      fprintf(fout, "  mov x0, R_HEAP\n");
      GetFrameVar(fout, "x1", retain_instr->target);
      fprintf(fout, "  bl FbleRetainValue\n");
      return;
    }

    case FBLE_RELEASE_INSTR: {
      FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
      size_t sp_offset = StackBytesForCount(release_instr->targets.size);
      fprintf(fout, "  sub SP, SP, #%zi\n", sp_offset);
      for (size_t i = 0; i < release_instr->targets.size; ++i) {
        FbleVar target = {
          .tag = FBLE_LOCAL_VAR,
          .index = release_instr->targets.xs[i]
        };
        GetFrameVar(fout, "x9", target);
        fprintf(fout, "  str x9, [SP, #%zi]\n", 8 * i);
      }

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, %zi\n", release_instr->targets.size);
      fprintf(fout, "  mov x2, SP\n");
      fprintf(fout, "  bl FbleReleaseValues\n");
      fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
      return;
    }

    case FBLE_LIST_INSTR: {
      FbleListInstr* list_instr = (FbleListInstr*)instr;
      SetFrameVar(fout, "XZR", list_instr->dest);
      return;
    }

    case FBLE_LITERAL_INSTR: {
      FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
      SetFrameVar(fout, "XZR", literal_instr->dest);
      return;
    }

    case FBLE_NOP_INSTR: {
      return;
    }
  }
}

/**
 * Returns the size of the label-sanitized version of a given string.
 *
 * @param str  The string to get the sanitized size of.
 *
 * @returns
 *   The number of bytes needed for the sanitized version of the given string,
 *   including nul terminator.
 *
 * @sideeffects
 *   None
 */
static size_t SizeofSanitizedString(const char* str)
{
  size_t size = 1;
  for (const char* p = str; *p != '\0'; p++) {
    size += isalnum((unsigned char)*p) ? 1 : 4;
  }
  return size;
}

/**
 * Returns a version of the string suitable for use in labels.
 *
 * @param str  The string to sanitize.
 * @param dst  A character buffer of size SizeofSanitizedString(str) to write
 *   the sanitized string to.
 *
 * @sideeffects
 *   Fills in dst with the sanitized version of the string.
 */
static void SanitizeString(const char* str, char* dst)
{
  dst[0] = '\0';
  for (const char* p = str; *p != '\0'; p++) {
    char x[5];
    sprintf(x, isalnum((unsigned char)*p) ? "%c" : "_%02x_", *p);
    strcat(dst, x);
  }
}

/**
 * Returns a C function identifier to for the give module path.
 *
 * @param path  The path to get the name for.
 *
 * @returns
 *   A C function name for the module path.
 *
 * @sideeffects
 *   Allocates an FbleString* that should be freed with FbleFreeString when no
 *   longer needed.
 */
static FbleString* LabelForPath(FbleModulePath* path)
{
  // The conversion from path to name works as followed:
  // * We add _Fble as a prefix.
  // * Characters [0-9], [a-z], [A-Z] are kept as is.
  // * Other characters are translated to _XX_, where XX is the 2 digit hex
  //   representation of the ascii value of the character.
  // * We include translated '/' and '%' characters where expected in the
  //   path.

  // Determine the length of the name.
  size_t len = strlen("_Fble") + 1; // prefix and terminating '\0'.
  for (size_t i = 0; i < path->path.size; ++i) {
    len += 4;   // translated '/' character.
    for (const char* p = path->path.xs[i].name->str; *p != '\0'; p++) {
      if (isalnum((unsigned char)*p)) {
        len++;        // untranslated character
      } else {
        len += 4;     // translated character
      }
    }
  }
  len += 4; // translated '%' character.

  // Construct the name.
  char name[len];
  char translated[5]; 
  name[0] = '\0';
  strcat(name, "_Fble");
  for (size_t i = 0; i < path->path.size; ++i) {
    sprintf(translated, "_%02x_", '/');
    strcat(name, translated);
    for (const char* p = path->path.xs[i].name->str; *p != '\0'; p++) {
      if (isalnum((unsigned char)*p)) {
        sprintf(translated, "%c", *p);
        strcat(name, translated);
      } else {
        sprintf(translated, "_%02x_", *p);
        strcat(name, translated);
      }
    }
  }
  sprintf(translated, "_%02x_", '%');
  strcat(name, translated);

  return FbleNewString(name);
}

// See documentation in fble-compile.h.
void FbleGenerateAArch64(FILE* fout, FbleCompiledModule* module)
{
  FbleCodeV blocks;
  FbleVectorInit(blocks);

  LocV locs;
  FbleVectorInit(locs);

  CollectBlocksAndLocs(&blocks, &locs, module->code);

  fprintf(fout, "  .file 1 \"%s\"\n", module->path->loc.source->str);

  // Common things we hold in callee saved registers for Run and Abort
  // functions.
  fprintf(fout, "  R_HEAP .req x19\n");
  fprintf(fout, "  R_THREAD .req x20\n");
  fprintf(fout, "  R_LOCALS .req x21\n");
  fprintf(fout, "  R_ARGS .req x22\n");
  fprintf(fout, "  R_STATICS .req x23\n");
  fprintf(fout, "  R_PROFILE_BLOCK_OFFSET .req x24\n");
  fprintf(fout, "  R_PROFILE .req x25\n");
  fprintf(fout, "  R_SCRATCH_0 .req x26\n");

  // Error messages.
  fprintf(fout, "  .section .data\n");
  fprintf(fout, ".L.ErrorFormatString:\n");
  fprintf(fout, "  .string \"%%s:%%d:%%d: error: %%s\"\n");
  fprintf(fout, ".L.CalleeAborted:\n");
  fprintf(fout, "  .string \"callee aborted\\n\"\n");
  fprintf(fout, ".L.UndefinedStructValue:\n");
  fprintf(fout, "  .string \"undefined struct value access\\n\"\n");
  fprintf(fout, ".L.UndefinedUnionValue:\n");
  fprintf(fout, "  .string \"undefined union value access\\n\";\n");
  fprintf(fout, ".L.UndefinedUnionSelect:\n");
  fprintf(fout, "  .string \"undefined union value select\\n\";\n");
  fprintf(fout, ".L.WrongUnionTag:\n");
  fprintf(fout, "  .string \"union field access undefined: wrong tag\\n\";\n");
  fprintf(fout, ".L.UndefinedFunctionValue:\n");
  fprintf(fout, "  .string \"called undefined function\\n\";\n");
  fprintf(fout, ".L.VacuousValue:\n");
  fprintf(fout, "  .string \"vacuous value\\n\";\n");

  // Definitions of source code locations.
  for (size_t i = 0; i < locs.size; ++i) {
    char label[SizeofSanitizedString(locs.xs[i])];
    SanitizeString(locs.xs[i], label);
    fprintf(fout, ".L.loc.%s:\n  .string \"%s\"\n", label, locs.xs[i]);
  }

  fprintf(fout, "  .text\n");
  fprintf(fout, "  .align 2\n");
  fprintf(fout, ".Llow_pc:\n");

  FbleNameV profile_blocks = module->profile_blocks;

  for (size_t i = 0; i < blocks.size; ++i) {
    EmitCode(fout, profile_blocks, blocks.xs[i]);
  }

  LabelId label_id = 0;
  LabelId module_id = StaticExecutableModule(fout, &label_id, module);
  LabelId deps_id = label_id++;

  fprintf(fout, "  .section .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", deps_id);
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleString* dep_name = LabelForPath(module->deps.xs[i]);
    fprintf(fout, "  .xword %s\n", dep_name->str);
    FbleFreeString(dep_name);
  }

  FbleString* func_name = LabelForPath(module->path);
  fprintf(fout, "  .text\n");
  fprintf(fout, "  .align 2\n");
  fprintf(fout, "  .global %s\n", func_name->str);
  fprintf(fout, "%s:\n", func_name->str);
  fprintf(fout, "  stp FP, LR, [SP, #-16]!\n");
  fprintf(fout, "  mov FP, SP\n");

  Adr(fout, "x1", LABEL, module_id);
  fprintf(fout, "  mov x2, %zi\n", module->deps.size);
  Adr(fout, "x3", LABEL, deps_id);
  fprintf(fout, "  bl FbleLoadFromCompiled\n");

  fprintf(fout, "  ldp FP, LR, [SP], #16\n");
  fprintf(fout, "  ret\n");
  fprintf(fout, ".Lhigh_pc:\n");
  FbleFreeString(func_name);

  // Emit dwarf debug info.
  fprintf(fout, "  .section .debug_info\n");

  // Compilation Unit Header
  fprintf(fout, ".Ldebug_info:\n");
  fprintf(fout, "  .4byte .Ldebug_info_end-.Ldebug_info-4\n");  // length
  fprintf(fout, "  .2byte 2\n");                // DWARF version 2
  fprintf(fout, "  .4byte .Ldebug_abbrev\n");   // .debug_abbrev offset
  fprintf(fout, "  .byte 8\n");                 // pointer size in bytes

  // compile_unit entry
  char cwd[1024];
  char* gotten = getcwd(cwd, 1024);
  assert(gotten && "TODO: handle longer paths");
  fprintf(fout, "  .uleb128 1\n");       // abbrev code for compile_unit
  fprintf(fout, "  .8byte .Llow_pc\n");  // low_pc value.
  fprintf(fout, "  .8byte .Lhigh_pc\n"); // high_pc value.
  fprintf(fout, "  .string \"%s\"\n",    // source file name.
      module->path->loc.source->str);    
  fprintf(fout, "  .4byte .Ldebug_line\n");          // stmt_list offset.
  fprintf(fout, "  .string \"%s\"\n", cwd); // compilation directory.
  fprintf(fout, "  .string \"FBLE\"\n"); // producer.

  // FbleValue* type entry
  fprintf(fout, ".LFbleValuePointerType:\n");
  fprintf(fout, "  .uleb128 4\n");       // abbrev code for point type
  fprintf(fout, "  .byte 8\n");          // bite_size value
  fprintf(fout, "  .8byte .LFbleValueStructType\n");  // type value

  fprintf(fout, ".LFbleValueStructType:\n");
  fprintf(fout, "  .uleb128 5\n");            // abbrev code for structure type
  fprintf(fout, "  .string \"FbleValue\"\n"); // name
  fprintf(fout, "  .byte 1\n");               // declaration

  // subprogram entries
  for (size_t i = 0; i < blocks.size; ++i) {
    FbleCode* code = blocks.xs[i];

    FbleName function_block = profile_blocks.xs[code->_base.profile_block_id];
    char function_label[SizeofSanitizedString(function_block.name->str)];
    SanitizeString(function_block.name->str, function_label);

    fprintf(fout, "  .uleb128 2\n");       // abbrev code for subprogram.
    fprintf(fout, "  .string \"%s\"\n",    // source function name.
        function_block.name->str);

    // low_pc and high_pc attributes.
    fprintf(fout, "  .8byte _Run.%p.%s\n", (void*)code, function_label);
    fprintf(fout, "  .8byte .L.%p.%s.high_pc\n", (void*)code, function_label);

    for (size_t j = 0; j < code->instrs.size; ++j) {
      FbleInstr* instr = code->instrs.xs[j];
      for (FbleDebugInfo* info = instr->debug_info; info != NULL; info = info->next) {
        if (info->tag == FBLE_VAR_DEBUG_INFO) {
          FbleVarDebugInfo* var = (FbleVarDebugInfo*)info;
          fprintf(fout, "  .uleb128 3\n");  // abbrev code for var.

          // variable name.
          fprintf(fout, "  .string \"");
          FblePrintName(fout, var->name);
          fprintf(fout, "\"\n");

          // location.
          // var_tags are 0x70 + X for bregX. In this case:
          //   statics: x23: 0x70 + 23 = 0x87
          //   args:    x22: 0x70 + 22 = 0x86
          //   locals:  x21: 0x70 + 21 = 0x85
          static const char* var_tags[] = { "0x87", "0x86", "0x85"};
          fprintf(fout, "  .byte 1f - 0f\n");   // length of block.
          fprintf(fout, "0:\n");
          fprintf(fout, "  .byte %s\n", var_tags[var->var.tag]);
          fprintf(fout, "  .sleb128 %zi\n", sizeof(FbleValue*) * var->var.index);
          fprintf(fout, "1:\n");

          // start_scope
          fprintf(fout, "  .8byte .L._Run_%p.pc.%zi - _Run.%p.%s\n",
              (void*)code, j,
              (void*)code, function_label);

          // type
          fprintf(fout, "  .8byte .LFbleValuePointerType\n");
        }
      }
    }
    fprintf(fout, "  .uleb128 0\n");    // abbrev code for NULL (end of list).
  };

  fprintf(fout, "  .uleb128 0\n");    // abbrev code for NULL (end of list).

  fprintf(fout, ".Ldebug_info_end:\n");

  fprintf(fout, "  .section .debug_abbrev\n");
  fprintf(fout, ".Ldebug_abbrev:\n");
  fprintf(fout, "  .uleb128 1\n");     // compile_unit abbrev code declaration
  fprintf(fout, "  .uleb128 0x11\n");  // DW_TAG_compile_unit
  fprintf(fout, "  .byte 1\n");        // DW_CHILDREN_yes
  fprintf(fout, "  .uleb128 0x11\n");  // DW_AT_low_pc
  fprintf(fout, "  .uleb128 0x01\n");  // DW_FORM_addr
  fprintf(fout, "  .uleb128 0x12\n");  // DW_AT_high_pc
  fprintf(fout, "  .uleb128 0x01\n");  // DW_FORM_addr
  fprintf(fout, "  .uleb128 0x03\n");  // DW_AT_name
  fprintf(fout, "  .uleb128 0x08\n");  // DW_FORM_string
  fprintf(fout, "  .uleb128 0x10\n");  // DW_AT_stmt_list
  fprintf(fout, "  .uleb128 0x06\n");  // DW_FORM_data4 (expected by dwarfdump)
  fprintf(fout, "  .uleb128 0x1b\n");  // DW_AT_comp_dir
  fprintf(fout, "  .uleb128 0x08\n");  // DW_FORM_string
  fprintf(fout, "  .uleb128 0x25\n");  // DW_AT_producer
  fprintf(fout, "  .uleb128 0x08\n");  // DW_FORM_string
  fprintf(fout, "  .uleb128 0\n");     // NULL attribute NAME
  fprintf(fout, "  .uleb128 0\n");     // NULL attribute FORM

  fprintf(fout, "  .uleb128 2\n");     // subprogram abbrev code declaration
  fprintf(fout, "  .uleb128 0x2e\n");  // DW_TAG_subprogram
  fprintf(fout, "  .byte 1\n");        // DW_CHILDREN_yes
  fprintf(fout, "  .uleb128 0x03\n");  // DW_AT_name
  fprintf(fout, "  .uleb128 0x08\n");  // DW_FORM_string
  fprintf(fout, "  .uleb128 0x11\n");  // DW_AT_low_pc
  fprintf(fout, "  .uleb128 0x01\n");  // DW_FORM_addr
  fprintf(fout, "  .uleb128 0x12\n");  // DW_AT_high_pc
  fprintf(fout, "  .uleb128 0x01\n");  // DW_FORM_addr
  fprintf(fout, "  .uleb128 0\n");     // NULL attribute NAME
  fprintf(fout, "  .uleb128 0\n");     // NULL attribute FORM

  fprintf(fout, "  .uleb128 3\n");     // var abbrev code declaration
  fprintf(fout, "  .uleb128 0x34\n");  // DW_TAG_variable
  fprintf(fout, "  .byte 0\n");        // DW_CHILDREN_yes
  fprintf(fout, "  .uleb128 0x03\n");  // DW_AT_name
  fprintf(fout, "  .uleb128 0x08\n");  // DW_FORM_string
  fprintf(fout, "  .uleb128 0x02\n");  // DW_AT_location
  fprintf(fout, "  .uleb128 0x0a\n");  // DW_FORM_block1
  fprintf(fout, "  .uleb128 0x2c\n");  // DW_AT_start_scope
  fprintf(fout, "  .uleb128 0x07\n");  // DW_FORM_data8
  fprintf(fout, "  .uleb128 0x49\n");  // DW_AT_type
  fprintf(fout, "  .uleb128 0x10\n");  // DW_FORM_ref_addr
  fprintf(fout, "  .uleb128 0\n");     // NULL attribute NAME
  fprintf(fout, "  .uleb128 0\n");     // NULL attribute FORM

  fprintf(fout, "  .uleb128 4\n");     // pointer type abbrev code declaration
  fprintf(fout, "  .uleb128 0x0f\n");  // DW_TAG_pointer_type
  fprintf(fout, "  .byte 0\n");        // DW_CHILDREN_no
  fprintf(fout, "  .uleb128 0x0b\n");  // DW_AT_byte_size
  fprintf(fout, "  .uleb128 0x0b\n");  // DW_FORM_data1
  fprintf(fout, "  .uleb128 0x49\n");  // DW_AT_type
  fprintf(fout, "  .uleb128 0x10\n");  // DW_FORM_ref_addr
  fprintf(fout, "  .uleb128 0\n");     // NULL attribute NAME
  fprintf(fout, "  .uleb128 0\n");     // NULL attribute FORM

  fprintf(fout, "  .uleb128 5\n");     // struct type abbrev code declaration
  fprintf(fout, "  .uleb128 0x13\n");  // DW_TAG_structure_type
  fprintf(fout, "  .byte 0\n");        // DW_CHILDREN_no
  fprintf(fout, "  .uleb128 0x03\n");  // DW_AT_name
  fprintf(fout, "  .uleb128 0x08\n");  // DW_FORM_string
  fprintf(fout, "  .uleb128 0x3c\n");  // DW_AT_declaration
  fprintf(fout, "  .uleb128 0x0c\n");  // DW_FORM_flag
  fprintf(fout, "  .uleb128 0\n");     // NULL attribute NAME
  fprintf(fout, "  .uleb128 0\n");     // NULL attribute FORM

  fprintf(fout, "  .uleb128 0\n");     // End of abbrev declarations.

  fprintf(fout, "  .section .debug_line\n");
  fprintf(fout, ".Ldebug_line:\n");

  FbleFreeVector(blocks);
  FbleFreeVector(locs);
}

// See documentation in fble-compile.h.
void FbleGenerateAArch64Export(FILE* fout, const char* name, FbleModulePath* path)
{
  fprintf(fout, "  .text\n");
  fprintf(fout, "  .align 2\n");
  fprintf(fout, "  .global %s\n", name);
  fprintf(fout, "%s:\n", name);
  fprintf(fout, "  stp FP, LR, [SP, #-16]!\n");
  fprintf(fout, "  mov FP, SP\n");

  FbleString* module_name = LabelForPath(path);
  fprintf(fout, "  bl %s\n\n", module_name->str);
  FbleFreeString(module_name);

  fprintf(fout, "  ldp FP, LR, [SP], #16\n");
  fprintf(fout, "  ret\n");
}

// See documentation in fble-compile.h.
void FbleGenerateAArch64Main(FILE* fout, const char* main, FbleModulePath* path)
{
  fprintf(fout, "  .text\n");
  fprintf(fout, "  .align 2\n");
  fprintf(fout, "  .global main\n");
  fprintf(fout, "main:\n");
  fprintf(fout, "  stp FP, LR, [SP, #-16]!\n");
  fprintf(fout, "  mov FP, SP\n");

  FbleString* module_name = LabelForPath(path);
  Adr(fout, "x2", "%s", module_name->str);
  FbleFreeString(module_name);

  Adr(fout, "x3", "%s", main);

  fprintf(fout, "  br x3\n");
  fprintf(fout, "  ldp FP, LR, [SP], #16\n");
  fprintf(fout, "  ret\n");
}
