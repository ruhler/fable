/**
 * @file generate-c.c
 * Converts FbleCode fble bytecode to C code.
 */

#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdio.h>    // for sprintf
#include <stdarg.h>   // for va_list, va_start, va_end.
#include <stddef.h>   // for offsetof
#include <string.h>   // for strlen, strcat, memset
#include <unistd.h>   // for getcwd

#include <fble/fble-compile.h>
#include <fble/fble-vector.h>    // for FbleVectorInit, etc.

#include "code.h"
#include "tc.h"
#include "unreachable.h"
#include "value.h"

/** Type representing a name as an integer. */
typedef unsigned int LabelId;

/** Printf format string for a LabelId. */
#define LABEL "l%x"

static void CollectBlocks(FbleCodeV* blocks, FbleCode* code);

static void StringLit(FILE* fout, const char* string);
static LabelId StaticString(FILE* fout, LabelId* label_id, const char* string);
static LabelId StaticNames(FILE* fout, LabelId* label_id, FbleNameV names);
static LabelId StaticModulePath(FILE* fout, LabelId* label_id, FbleModulePath* path);
static LabelId StaticExecutableModule(FILE* fout, LabelId* label_id, FbleCompiledModule* module);

static void ReturnAbort(FILE* fout, void* code, const char* function_label, size_t pc, const char* lmsg, FbleLoc loc);

static void EmitCode(FILE* fout, FbleNameV profile_blocks, FbleCode* code);
static void EmitInstrForAbort(FILE* fout, size_t pc, FbleInstr* instr);
static void EmitCodeForAbort(FILE* fout, FbleNameV profile_blocks, FbleCode* code);
static size_t SizeofSanitizedString(const char* str);
static void SanitizeString(const char* str, char* dst);
static FbleString* LabelForPath(FbleModulePath* path);

/**
 * Gets the list of code blocks referenced by a code block.
 *
 * Includes the the code block itself.
 *
 * @param blocks  The collection of blocks to add to.
 * @param code  The code to collect the blocks from.
 *
 * @sideeffects
 *   Appends collected blocks to 'blocks'.
 */
static void CollectBlocks(FbleCodeV* blocks, FbleCode* code)
{
  FbleVectorAppend(*blocks, code);
  for (size_t i = 0; i < code->instrs.size; ++i) {
    switch (code->instrs.xs[i]->tag) {
      case FBLE_DATA_TYPE_INSTR: break;
      case FBLE_STRUCT_VALUE_INSTR: break;
      case FBLE_UNION_VALUE_INSTR: break;
      case FBLE_STRUCT_ACCESS_INSTR: break;
      case FBLE_UNION_ACCESS_INSTR: break;
      case FBLE_UNION_SELECT_INSTR: break;
      case FBLE_JUMP_INSTR: break;

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* instr = (FbleFuncValueInstr*)code->instrs.xs[i];
        CollectBlocks(blocks, instr->code);
        break;
      }

      case FBLE_CALL_INSTR: break;
      case FBLE_COPY_INSTR: break;
      case FBLE_REF_VALUE_INSTR: break;
      case FBLE_REF_DEF_INSTR: break;
      case FBLE_RETURN_INSTR: break;
      case FBLE_TYPE_INSTR: break;
      case FBLE_RELEASE_INSTR: break;
      case FBLE_LIST_INSTR: break;
      case FBLE_LITERAL_INSTR: break;
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

  fprintf(fout, "static FbleString " LABEL " = {\n", id);
  fprintf(fout, "  .refcount = 1,\n");
  fprintf(fout, "  .magic = FBLE_STRING_MAGIC,\n");
  fprintf(fout, "  .str = ");
  StringLit(fout, string);
  fprintf(fout, "\n};\n");
  return id;
}

/**
 * Output code to declare a static FbleNameV.xs value.
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
  fprintf(fout, "static FbleName " LABEL "[] = {\n", id);
  for (size_t i = 0; i < names.size; ++i) {
    fprintf(fout, "  { .name = &" LABEL ",\n", str_ids[i]);
    fprintf(fout, "    .space = %i,\n", names.xs[i].space);
    fprintf(fout, "    .loc = { .source = &" LABEL ", .line = %i, .col = %i }},\n",
        src_ids[i], names.xs[i].loc.line, names.xs[i].loc.col);
  }
  fprintf(fout, "};\n");
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

  fprintf(fout, "static FbleModulePath " LABEL " = {\n", path_id);
  fprintf(fout, "  .refcount = 1,\n");
  fprintf(fout, "  .magic = FBLE_MODULE_PATH_MAGIC,\n");
  fprintf(fout, "  .loc = { .source = &" LABEL ", .line = %i, .col = %i },\n",
      src_id, path->loc.line, path->loc.col);
  fprintf(fout, "  .path = { .size = %zi, .xs = " LABEL "},\n",
      path->path.size, names_id);
  fprintf(fout, "};\n");
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
  fprintf(fout, "static FbleModulePath* " LABEL "[] = {\n", deps_xs_id);
  for (size_t i = 0; i < module->deps.size; ++i) {
    fprintf(fout, "  &" LABEL ",\n", dep_ids[i]);
  }
  fprintf(fout, "};\n");

  LabelId executable_id = (*label_id)++;
  fprintf(fout, "static FbleExecutable " LABEL " = {\n", executable_id);
  fprintf(fout, "  .refcount = 1,\n");
  fprintf(fout, "  .magic = FBLE_EXECUTABLE_MAGIC,\n");
  fprintf(fout, "  .num_args = %zi, \n", module->code->_base.num_args);
  fprintf(fout, "  .num_statics = %zi,\n", module->code->_base.num_statics);
  fprintf(fout, "  .profile_block_id = %zi,\n", module->code->_base.profile_block_id);

  FbleName function_block = module->profile_blocks.xs[module->code->_base.profile_block_id];
  char function_label[SizeofSanitizedString(function_block.name->str)];
  SanitizeString(function_block.name->str, function_label);
  fprintf(fout, "  .run = &_Run_%p_%s,\n", (void*)module->code, function_label);
  fprintf(fout, "  .on_free = &FbleExecutableNothingOnFree\n");
  fprintf(fout, "};\n");

  LabelId profile_blocks_xs_id = StaticNames(fout, label_id, module->profile_blocks);

  LabelId module_id = (*label_id)++;
  fprintf(fout, "static FbleExecutableModule " LABEL " = {\n", module_id);
  fprintf(fout, "  .refcount = 1,\n");
  fprintf(fout, "  .magic = FBLE_EXECUTABLE_MODULE_MAGIC,\n");
  fprintf(fout, "  .path = &" LABEL ",\n", path_id);
  fprintf(fout, "  .deps = { .size = %zi, .xs = " LABEL "},\n",
      module->deps.size, deps_xs_id);
  fprintf(fout, "  .executable = &" LABEL ",\n", executable_id);
  fprintf(fout, "  .profile_blocks = { .size = %zi, .xs = " LABEL "},\n",
      module->profile_blocks.size, profile_blocks_xs_id);
  fprintf(fout, "};\n");
  return module_id;
}

/**
 * Emits code to return an error from a Run function.
 *
 * @param fout  The output stream.
 * @param code  Pointer to current code block to use for labels.
 * @param function_label  The label for the currently executing function.
 * @param pc  The program counter of the abort location.
 * @param lmsg  The name of the error message to use.
 * @param loc  The location to report with the error message.
 *
 * @sideeffects
 *   Emits code to return the error.
 */
static void ReturnAbort(FILE* fout, void* code, const char* function_label, size_t pc, const char* lmsg, FbleLoc loc)
{
  fprintf(fout, "{\n");
  fprintf(fout, "    ReportAbort(%s, %d, %d);\n", lmsg, loc.line, loc.col);
  fprintf(fout, "    return _Abort_%p_%s(heap, thread, l, %zi);\n",
      code, function_label, pc);
  fprintf(fout, "  }\n");
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
  FbleName block = profile_blocks.xs[code->_base.profile_block_id];
  char label[SizeofSanitizedString(block.name->str)];
  SanitizeString(block.name->str, label);
  fprintf(fout, "static FbleValue* _Run_%p_%s("
      "FbleValueHeap* heap, FbleThread* thread, "
      "FbleExecutable* executable, FbleValue** args, "
      "FbleValue** statics, FbleBlockId profile_block_offset)\n",
      (void*)code, label);
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleValue** a = args;\n");
  fprintf(fout, "  FbleValue* l[%zi];\n", code->num_locals);
  fprintf(fout, "  FbleValue** s = statics;\n");

  // x0 is a temporary variable individual instructions can use however they
  // wish.
  fprintf(fout, "  FbleValue* x0 = NULL;\n");

  // Emit code for each fble instruction
  bool jump_target[code->instrs.size];
  memset(jump_target, 0, sizeof(bool) * code->instrs.size);
  size_t lit_id = 0;
  size_t exe_id = 0;
  for (size_t pc = 0; pc < code->instrs.size; ++pc) {
    FbleInstr* instr = code->instrs.xs[pc];

    // Output a label for the instruction for other instructions to jump to if
    // needed. Avoid outputting a label otherwise to reduce the memory
    // overheads of compiling the generated code by a tiny bit.
    if (jump_target[pc]) {
      fprintf(fout, "pc_%zi:\n", pc);
    }

    // Profiling logic.
    fprintf(fout, "  FbleThreadSample(thread);\n");
    for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
      switch (op->tag) {
        case FBLE_PROFILE_ENTER_OP: {
          fprintf(fout, "  FbleThreadEnterBlock(thread, profile_block_offset + %zi);\n", op->block);
          break;
        }

        case FBLE_PROFILE_REPLACE_OP: {
          fprintf(fout, "  FbleThreadReplaceBlock(thread, profile_block_offset + %zi);\n", op->block);
          break;
        }

        case FBLE_PROFILE_EXIT_OP: {
          fprintf(fout, "  FbleThreadExitBlock(thread);\n");
          break;
        }
      }
    }

    // Instruction logic.
    static const char* var_tag[] = { "s", "a", "l" };
    switch (instr->tag) {
      case FBLE_DATA_TYPE_INSTR: {
        FbleDataTypeInstr* dt_instr = (FbleDataTypeInstr*)instr;
        size_t fieldc = dt_instr->fields.size;

        fprintf(fout, "  {\n");
        fprintf(fout, "    FbleValue* fields[] = {");
        for (size_t i = 0; i < fieldc; ++i) {
          fprintf(fout, " %s[%zi],",
              var_tag[dt_instr->fields.xs[i].tag],
              dt_instr->fields.xs[i].index);
        };
        fprintf(fout, " };\n");

        static const char* dtkind[] = { "FBLE_STRUCT_DATATYPE", "FBLE_UNION_DATATYPE" };
        fprintf(fout, "    l[%zi] = FbleNewDataTypeValue(heap, %s, %zi, fields);\n",
            dt_instr->dest, dtkind[dt_instr->kind], fieldc);
        fprintf(fout, "  }\n");
        break;
      }

      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_instr->args.size;

        fprintf(fout, "  l[%zi] = FbleNewStructValue_(heap, %zi", struct_instr->dest, argc);
        for (size_t i = 0; i < argc; ++i) {
          fprintf(fout, ", %s[%zi]",
              var_tag[struct_instr->args.xs[i].tag],
              struct_instr->args.xs[i].index);
        };

        fprintf(fout, ");\n");
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
        fprintf(fout, "  l[%zi] = FbleNewUnionValue(heap, %zi, %s[%zi]);\n",
            union_instr->dest, union_instr->tag,
            var_tag[union_instr->arg.tag],
            union_instr->arg.index);
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        fprintf(fout, "  x0 = FbleStrictValue(%s[%zi]);\n",
            var_tag[access_instr->obj.tag], access_instr->obj.index);
        fprintf(fout, "  if (!x0) ");
        ReturnAbort(fout, code, label, pc, "UndefinedStructValue", access_instr->loc);

        fprintf(fout, "  l[%zi] = FbleStructValueAccess(x0, %zi);\n",
            access_instr->dest, access_instr->tag);
        fprintf(fout, "  FbleRetainValue(heap, l[%zi]);\n", access_instr->dest);
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        fprintf(fout, "  x0 = FbleStrictValue(%s[%zi]);\n",
            var_tag[access_instr->obj.tag], access_instr->obj.index);
        fprintf(fout, "  if (!x0) ");
        ReturnAbort(fout, code, label, pc, "UndefinedUnionValue", access_instr->loc);

        fprintf(fout, "  if (%zi != FbleUnionValueTag(x0)) ", access_instr->tag);
        ReturnAbort(fout, code, label, pc, "WrongUnionTag", access_instr->loc);

        fprintf(fout, "  l[%zi] = FbleUnionValueAccess(x0);\n", access_instr->dest);
        fprintf(fout, "  FbleRetainValue(heap, l[%zi]);\n", access_instr->dest);
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;

        fprintf(fout, "  x0 = FbleStrictValue(%s[%zi]);\n",
            var_tag[select_instr->condition.tag],
            select_instr->condition.index);
        fprintf(fout, "  if (!x0) ");
        ReturnAbort(fout, code, label, pc, "UndefinedUnionSelect", select_instr->loc);

        fprintf(fout, "  switch (FbleUnionValueTag(x0)) {\n");
        for (size_t i = 0; i < select_instr->jumps.size; ++i) {
          size_t target = pc + 1 + select_instr->jumps.xs[i];
          assert(target > pc);
          jump_target[target] = true;
          fprintf(fout, "    case %zi: goto pc_%zi;\n", i, target);
        }
        fprintf(fout, "  }\n");
        break;
      }

      case FBLE_JUMP_INSTR: {
        FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
        size_t target = pc + 1 + jump_instr->count;
        assert(target > pc);
        jump_target[target] = true;
        fprintf(fout, "  goto pc_%zi;\n", target);
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;

        FbleName function_block = profile_blocks.xs[func_instr->code->_base.profile_block_id];
        char function_label[SizeofSanitizedString(function_block.name->str)];
        SanitizeString(function_block.name->str, function_label);

        fprintf(fout, "  static FbleExecutable exe_%zi = {\n", exe_id);
        fprintf(fout, "    .refcount = 1,\n");
        fprintf(fout, "    .magic = FBLE_EXECUTABLE_MAGIC,\n");
        fprintf(fout, "    .num_args = %zi,\n", func_instr->code->_base.num_args);
        fprintf(fout, "    .num_statics = %zi,\n", func_instr->code->_base.num_statics);
        fprintf(fout, "    .profile_block_id = %zi,\n", func_instr->code->_base.profile_block_id);
        fprintf(fout, "    .run = &_Run_%p_%s,\n", (void*)func_instr->code, function_label);
        fprintf(fout, "    .on_free = NULL\n");
        fprintf(fout, "  };\n");
        fprintf(fout, "  l[%zi] = FbleNewFuncValue_(heap, &exe_%zi, profile_block_offset", func_instr->dest, exe_id);
        exe_id++;
        for (size_t i = 0; i < func_instr->code->_base.num_statics; ++i) {
          fprintf(fout, ", %s[%zi]",
              var_tag[func_instr->scope.xs[i].tag],
              func_instr->scope.xs[i].index);
        }
        fprintf(fout, ");\n");
        break;
      }

      case FBLE_CALL_INSTR: {
        FbleCallInstr* call_instr = (FbleCallInstr*)instr;

        fprintf(fout, "  x0 = FbleStrictValue(%s[%zi]);\n",
            var_tag[call_instr->func.tag],
            call_instr->func.index);
        fprintf(fout, "  if (!x0) ");
        ReturnAbort(fout, code, label, pc, "UndefinedFunctionValue", call_instr->loc);

        if (call_instr->exit) {
          fprintf(fout, "  FbleRetainValue(heap, x0);\n");

          for (size_t i = 0; i < call_instr->args.size; ++i) {
            // We need to do a Retain on every arg from statics. For args from
            // locals, we don't need to do a Retain on the arg the first time we
            // see the local, because we can transfer the caller's ownership of
            // the local to the callee for that arg.
            bool retain = call_instr->args.xs[i].tag != FBLE_LOCAL_VAR;
            for (size_t j = 0; j < i; ++j) {
              if (call_instr->args.xs[i].tag == call_instr->args.xs[j].tag
                  && call_instr->args.xs[i].index == call_instr->args.xs[j].index) {
                retain = true;
                break;
              }
            }

            if (retain) {
              fprintf(fout, "  FbleRetainValue(heap, %s[%zi]);\n",
                  var_tag[call_instr->args.xs[i].tag],
                  call_instr->args.xs[i].index);
            }
          }

          if (call_instr->func.tag == FBLE_LOCAL_VAR) {
            fprintf(fout, "  FbleReleaseValue(heap, l[%zi]);\n", call_instr->func.index);
          }

          fprintf(fout, "  return FbleThreadTailCall_(heap, thread, x0");
          for (size_t i = 0; i < call_instr->args.size; ++i) {
            fprintf(fout, ", %s[%zi]",
                var_tag[call_instr->args.xs[i].tag],
                call_instr->args.xs[i].index);
          }
          fprintf(fout, ");\n");
          break;
        }

        fprintf(fout, "  l[%zi] = FbleThreadCall_(heap, thread, x0", call_instr->dest);
        for (size_t i = 0; i < call_instr->args.size; ++i) {
          fprintf(fout, ", %s[%zi]",
              var_tag[call_instr->args.xs[i].tag],
              call_instr->args.xs[i].index);
        }
        fprintf(fout, ");\n");
        fprintf(fout, "  if (l[%zi] == NULL) ", call_instr->dest);
        ReturnAbort(fout, code, label, pc, "CalleeAborted", call_instr->loc);
        break;
      }

      case FBLE_COPY_INSTR: {
        FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
        fprintf(fout, "  l[%zi] = %s[%zi];\n",
            copy_instr->dest,
            var_tag[copy_instr->source.tag],
            copy_instr->source.index);
        fprintf(fout, "  FbleRetainValue(heap, l[%zi]);\n", copy_instr->dest);
        break;
      }

      case FBLE_REF_VALUE_INSTR: {
        FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
        fprintf(fout, "  l[%zi] = FbleNewRefValue(heap);\n", ref_instr->dest);
        break;
      }

      case FBLE_REF_DEF_INSTR: {
        FbleRefDefInstr* ref_instr = (FbleRefDefInstr*)instr;

        fprintf(fout, "  if (!FbleAssignRefValue(heap, l[%zi], %s[%zi])) ",
            ref_instr->ref,
            var_tag[ref_instr->value.tag],
            ref_instr->value.index);
        ReturnAbort(fout, code, label, pc, "VacuousValue", ref_instr->loc);
        break;
      }

      case FBLE_RETURN_INSTR: {
        FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;

        switch (return_instr->result.tag) {
          case FBLE_STATIC_VAR:
          case FBLE_ARG_VAR: {
            fprintf(fout, "  FbleRetainValue(heap, %s[%zi]);\n",
                var_tag[return_instr->result.tag],
                return_instr->result.index);
            break;
          }

          case FBLE_LOCAL_VAR: break;
        }

        fprintf(fout, "  return %s[%zi];\n",
            var_tag[return_instr->result.tag],
            return_instr->result.index);
        break;
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
        fprintf(fout, "  l[%zi] = FbleGenericTypeValue;\n", type_instr->dest);
        break;
      }

      case FBLE_RELEASE_INSTR: {
        FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
        fprintf(fout, "  FbleReleaseValues_(heap, %zi", release_instr->targets.size);
        for (size_t i = 0; i < release_instr->targets.size; ++i) {
          fprintf(fout, ", l[%zi]", release_instr->targets.xs[i]);
        }
        fprintf(fout, ");\n");
        break;
      }

      case FBLE_LIST_INSTR: {
        FbleListInstr* list_instr = (FbleListInstr*)instr;
        size_t argc = list_instr->args.size;

        fprintf(fout, "  l[%zi] = FbleNewListValue_(heap, %zi", list_instr->dest, argc);
        for (size_t i = 0; i < argc; ++i) {
          fprintf(fout, ", %s[%zi]",
              var_tag[list_instr->args.xs[i].tag],
              list_instr->args.xs[i].index);
        }
        fprintf(fout, ");\n");
        break;
      }

      case FBLE_LITERAL_INSTR: {
        FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
        size_t argc = literal_instr->letters.size;
        fprintf(fout, "  static size_t lit_%zi[] = {", lit_id);
        for (size_t i = 0; i < argc; ++i) {
          fprintf(fout, " %zi,", literal_instr->letters.xs[i]);
        }
        fprintf(fout, " };\n");
        fprintf(fout, "  l[%zi] = FbleNewLiteralValue(heap, %zi, lit_%zi);\n",
            literal_instr->dest, argc, lit_id);
        lit_id++;
        break;
      }
    }
  }
  fprintf(fout, "}\n");
}

/**
 * Generates code to execute an instruction for the purposes of abort.
 *
 * @param fout  The output stream to write the code to.
 * @param pc  The program counter of the instruction.
 * @param instr  The instruction to execute.
 *
 * @sideeffects
 * * Outputs code to fout.
 */
static void EmitInstrForAbort(FILE* fout, size_t pc, FbleInstr* instr)
{
  switch (instr->tag) {
    case FBLE_DATA_TYPE_INSTR: {
      FbleDataTypeInstr* dt_instr = (FbleDataTypeInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", dt_instr->dest);
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", struct_instr->dest);
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", union_instr->dest);
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", access_instr->dest);
      return;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", access_instr->dest);
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
      fprintf(fout, "  goto pc_%zi;\n", pc + 1 + select_instr->jumps.xs[0]);
      return;
    }

    case FBLE_JUMP_INSTR: {
      FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
      fprintf(fout, "  goto pc_%zi;\n", pc + 1 + jump_instr->count);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", func_instr->dest);
      return;
    }

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      if (call_instr->exit) {
        if (call_instr->func.tag == FBLE_LOCAL_VAR) {
          fprintf(fout, "  FbleReleaseValue(heap, l[%zi]);\n", call_instr->func.index);
          fprintf(fout, "  l[%zi] = NULL;\n", call_instr->func.index);
        }

        for (size_t i = 0; i < call_instr->args.size; ++i) {
          if (call_instr->args.xs[i].tag == FBLE_LOCAL_VAR) {
            fprintf(fout, "  FbleReleaseValue(heap, l[%zi]);\n", call_instr->args.xs[i].index);
            fprintf(fout, "  l[%zi] = NULL;\n", call_instr->args.xs[i].index);
          }
        }

        fprintf(fout, "  return NULL;\n");
      }

      fprintf(fout, "  l[%zi] = NULL;\n", call_instr->dest);
      return;
    }

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", copy_instr->dest);
      return;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", ref_instr->dest);
      return;
    }

    case FBLE_REF_DEF_INSTR: {
      return;
    }

    case FBLE_RETURN_INSTR: {
      FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
      switch (return_instr->result.tag) {
        case FBLE_STATIC_VAR: break;
        case FBLE_ARG_VAR: break;
        case FBLE_LOCAL_VAR: {
          fprintf(fout, "  FbleReleaseValue(heap, l[%zi]);\n", return_instr->result.index);
          break;
        }
      }

      fprintf(fout, "  return NULL;\n");
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", type_instr->dest);
      return;
    }

    case FBLE_RELEASE_INSTR: {
      FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
      fprintf(fout, "  FbleReleaseValues_(heap, %zi", release_instr->targets.size);
      for (size_t i = 0; i < release_instr->targets.size; ++i) {
        fprintf(fout, ", l[%zi]", release_instr->targets.xs[i]);
      }
      fprintf(fout, ");\n");
      return;
    }

    case FBLE_LIST_INSTR: {
      FbleListInstr* list_instr = (FbleListInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", list_instr->dest);
      return;
    }

    case FBLE_LITERAL_INSTR: {
      FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", literal_instr->dest);
      return;
    }
  }
}

/**
 * Generates code to abort an FbleCode block.
 *
 * @param fout  The output stream to write the code to.
 * @param profile_blocks  The list of profile block names for the module.
 * @param code  The block of code to generate assembly.
 *
 * @sideeffects
 * * Outputs generated code to the given output stream.
 */
static void EmitCodeForAbort(FILE* fout, FbleNameV profile_blocks, FbleCode* code)
{
  // Jump table data for jumping to the right fble pc.
  FbleName block = profile_blocks.xs[code->_base.profile_block_id];
  char label[SizeofSanitizedString(block.name->str)];
  SanitizeString(block.name->str, label);
  fprintf(fout, "static FbleValue* _Abort_%p_%s(FbleValueHeap* heap, FbleThread* thread, FbleValue** l, size_t pc)\n", (void*)code, label);
  fprintf(fout, "{\n");
  fprintf(fout, "  switch (pc)\n");
  fprintf(fout, "  {\n");
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, "    case %zi: goto pc_%zi;\n", i, i);
  }
  fprintf(fout, "  }\n");

  // Emit code for each fble instruction
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, "pc_%zi:\n", i);
    EmitInstrForAbort(fout, i, code->instrs.xs[i]);
  }
  fprintf(fout, "}\n");
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
 *   None.
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
 * Returns a C identifier for a module path. 
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
void FbleGenerateC(FILE* fout, FbleCompiledModule* module)
{
  FbleCodeV blocks;
  FbleVectorInit(blocks);

  CollectBlocks(&blocks, module->code);

  fprintf(fout, "#include <fble/fble-execute.h>\n");  // for FbleThreadCall_
  fprintf(fout, "#include <fble/fble-link.h>\n");     // for FbleCompiledModuleFunction
  fprintf(fout, "#include <fble/fble-value.h>\n");    // for FbleValue

  // Error messages.
  fprintf(fout, "static const char* CalleeAborted = \"callee aborted\";\n");
  fprintf(fout, "static const char* UndefinedStructValue = \"undefined struct value access\";\n");
  fprintf(fout, "static const char* UndefinedUnionValue = \"undefined union value access\";\n");
  fprintf(fout, "static const char* UndefinedUnionSelect = \"undefined union value select\";\n");
  fprintf(fout, "static const char* WrongUnionTag = \"union field access undefined: wrong tag\";\n");
  fprintf(fout, "static const char* UndefinedFunctionValue = \"called undefined function\";\n");
  fprintf(fout, "static const char* VacuousValue = \"vacuous value\";\n");

  fprintf(fout, "static void ReportAbort(const char* msg, int line, int col)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  fprintf(stderr, \"%s:%%d:%%d: error: %%s\\n\", line, col, msg);\n",
      module->path->loc.source->str);
  fprintf(fout, "}\n");

  // Generate prototypes for all the run and abort functions.
  FbleNameV profile_blocks = module->profile_blocks;
  for (size_t i = 0; i < blocks.size; ++i) {
    FbleCode* code = blocks.xs[i];
    FbleName function_block = profile_blocks.xs[code->_base.profile_block_id];
    char function_label[SizeofSanitizedString(function_block.name->str)];
    SanitizeString(function_block.name->str, function_label);
    fprintf(fout, "static FbleValue* _Run_%p_%s("
      "FbleValueHeap* heap, FbleThread* thread, "
      "FbleExecutable* executable, FbleValue** locals, "
      "FbleValue** statics, FbleBlockId profile_block_offset);\n",
        (void*)code, function_label);
    fprintf(fout, "static FbleValue* _Abort_%p_%s(FbleValueHeap* heap, FbleThread* thread, FbleValue** locals, size_t pc);\n",
        (void*)code, function_label);
  }

  // Generate the implementations of all the run and abort functions.
  for (size_t i = 0; i < blocks.size; ++i) {
    EmitCode(fout, profile_blocks, blocks.xs[i]);
    EmitCodeForAbort(fout, profile_blocks, blocks.xs[i]);
  }

  LabelId label_id = 0;
  LabelId module_id = StaticExecutableModule(fout, &label_id, module);

  // Generate prototypes for dependencies.
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleString* dep_name = LabelForPath(module->deps.xs[i]);
    fprintf(fout, "void %s(FbleExecutableProgram* program);\n", dep_name->str);
    FbleFreeString(dep_name);
  }

  LabelId deps_id = label_id++;
  fprintf(fout, "static FbleCompiledModuleFunction* " LABEL "[] = {\n", deps_id);
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleString* dep_name = LabelForPath(module->deps.xs[i]);
    fprintf(fout, "  &%s,\n", dep_name->str);
    FbleFreeString(dep_name);
  }
  fprintf(fout, "};\n");

  FbleString* func_name = LabelForPath(module->path);
  fprintf(fout, "void %s(FbleExecutableProgram* program)\n", func_name->str);
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleLoadFromCompiled(program, &" LABEL ", %zi, " LABEL ");\n",
      module_id, module->deps.size, deps_id);
  fprintf(fout, "}\n");
  FbleFreeString(func_name);

  FbleVectorFree(blocks);
}

// See documentation in fble-compile.h.
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path)
{
  FbleString* module_name = LabelForPath(path);

  fprintf(fout, "#include <fble/fble-execute.h>\n");
  fprintf(fout, "#include <fble/fble-value.h>\n");
  fprintf(fout, "void %s(FbleExecutableProgram* program);\n", module_name->str);
  fprintf(fout, "void %s(FbleExecutableProgram* program)\n", name);
  fprintf(fout, "{\n");
  fprintf(fout, "  return %s(program);\n", module_name->str);
  fprintf(fout, "}\n");
  FbleFreeString(module_name);
}

// See documentation in fble-compile.h.
void FbleGenerateCMain(FILE* fout, const char* main, FbleModulePath* path)
{
  FbleString* module_name = LabelForPath(path);

  fprintf(fout, "#include <fble/fble-link.h>\n");
  fprintf(fout, "void %s(FbleExecutableProgram* program);\n", module_name->str);
  fprintf(fout, "int %s(int argc, const char** argv, FbleCompiledModuleFunction* module);\n", main);
  fprintf(fout, "int main(int argc, const char** argv)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  return %s(argc, argv, %s);\n", main, module_name->str);
  fprintf(fout, "}\n");

  FbleFreeString(module_name);
}
