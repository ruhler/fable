/**
 * @file c.c
 *  Converts FbleCode fble bytecode to C code.
 */

#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdio.h>    // for sprintf
#include <stdarg.h>   // for va_list, va_start, va_end.
#include <stddef.h>   // for offsetof
#include <string.h>   // for strlen, strcat, memset

#include <fble/fble-compile.h>
#include <fble/fble-vector.h>    // for FbleInitVector, etc.

#include "code.h"
#include "tc.h"
#include "unreachable.h"

/** Type representing a name as an integer. */
typedef unsigned int LabelId;

/** Printf format string for a LabelId. */
#define LABEL "l%x"

static void CollectBlocks(FbleCodeV* blocks, FbleCode* code);

static void StringLit(FILE* fout, const char* string);
static LabelId StaticString(FILE* fout, LabelId* label_id, const char* string);
static LabelId StaticNames(FILE* fout, LabelId* label_id, FbleNameV names);
static LabelId StaticModulePath(FILE* fout, LabelId* label_id, FbleModulePath* path);
static void StaticPreloadedModule(FILE* fout, LabelId* label_id, FbleModule* module);

static void ReturnAbort(FILE* fout, const char* lmsg, FbleLoc loc);

static void EmitCode(FILE* fout, FbleNameV profile_blocks, FbleCode* code);
static size_t SizeofSanitizedString(const char* str);
static void SanitizeString(const char* str, char* dst);
static FbleString* LabelForPath(FbleModulePath* path);

/**
 * @func[CollectBlocks] Gets the list of code blocks referenced by a code block.
 *  Includes the the code block itself.
 *
 *  @arg[FbleCodeV*][blocks] The collection of blocks to add to.
 *  @arg[FbleCode*][code] The code to collect the blocks from.
 *
 *  @sideeffects
 *   Appends collected blocks to @a[blocks].
 */
static void CollectBlocks(FbleCodeV* blocks, FbleCode* code)
{
  FbleAppendToVector(*blocks, code);
  for (size_t i = 0; i < code->instrs.size; ++i) {
    switch (code->instrs.xs[i]->tag) {
      case FBLE_STRUCT_VALUE_INSTR: break;
      case FBLE_UNION_VALUE_INSTR: break;
      case FBLE_STRUCT_ACCESS_INSTR: break;
      case FBLE_UNION_ACCESS_INSTR: break;
      case FBLE_UNION_SELECT_INSTR: break;
      case FBLE_GOTO_INSTR: break;

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* instr = (FbleFuncValueInstr*)code->instrs.xs[i];
        CollectBlocks(blocks, instr->code);
        break;
      }

      case FBLE_CALL_INSTR: break;
      case FBLE_TAIL_CALL_INSTR: break;
      case FBLE_COPY_INSTR: break;
      case FBLE_REC_DECL_INSTR: break;
      case FBLE_REC_DEFN_INSTR: break;
      case FBLE_RETURN_INSTR: break;
      case FBLE_TYPE_INSTR: break;
      case FBLE_LIST_INSTR: break;
      case FBLE_LITERAL_INSTR: break;
      case FBLE_NOP_INSTR: break;
      case FBLE_UNDEF_INSTR: break;
    }
  }
}

/**
 * @func[StringLit] Outputs a string literal to fout.
 *  @arg[FILE*][fout] The file to write to.
 *  @arg[const char*][string] The contents of the string to write.
 *  
 *  @sideeffects
 *   Outputs the given string as a C string literal to the given file.
 */
static void StringLit(FILE* fout, const char* string)
{
  fprintf(fout, "\"");
  for (const char* p = string; *p; p++) {
    switch (*p) {
      case '\a': fprintf(fout, "\\a"); break;
      case '\b': fprintf(fout, "\\b"); break;
      case '\f': fprintf(fout, "\\f"); break;
      case '\n': fprintf(fout, "\\n"); break;
      case '\r': fprintf(fout, "\\r"); break;
      case '\t': fprintf(fout, "\\t"); break;
      case '\v': fprintf(fout, "\\v"); break;
      case '"': fprintf(fout, "\\\""); break;
      case '\\': fprintf(fout, "\\\\"); break;
      default: fprintf(fout, "%c", *p); break;
    }
  }
  fprintf(fout, "\"");
}

/**
 * @func[StaticString] Outputs code to declare a static FbleString value.
 *  @arg[FILE*][fout] The file to write to
 *  @arg[LabelId*][label_id] Pointer to next available label id for use.
 *  @arg[const char*][string] The value of the string.
 *  
 *  @returns[LabelId]
 *   A label id of a local, static FbleString.
 *
 *  @sideeffects
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
 * @func[StaticNames] Output code to declare a static FbleNameV.xs value.
 *  @arg[FILE*][fout] The file to write to
 *  @arg[LabelId*][label_id] Pointer to next available label id for use.
 *  @arg[FbleNameV][names] The value of the names.
 *  
 *  @returns[LabelId]
 *   A label id of a local, static FbleNameV.xs.
 *
 *  @sideeffects
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
    fprintf(fout, "    .loc = { .source = &" LABEL ", .line = %zi, .col = %zi }},\n",
        src_ids[i], names.xs[i].loc.line, names.xs[i].loc.col);
  }
  fprintf(fout, "};\n");
  return id;
}

/**
 * @func[StaticModulePath]
 * @ Generates code to declare a static FbleModulePath value.
 *  @arg[FILE*][fout] The output stream to write the code to.
 *  @arg[LabelId*][label_id] Pointer to next available label id for use.
 *  @arg[FbleModulePath*][path] The FbleModulePath to generate code for.
 *
 *  @returns[LabelId]
 *   The label id of a local, static FbleModulePath.
 *
 *  @sideeffects
 *   @i Outputs code to fout.
 *   @i Increments label_id based on the number of internal labels used.
 */
static LabelId StaticModulePath(FILE* fout, LabelId* label_id, FbleModulePath* path)
{
  LabelId src_id = StaticString(fout, label_id, path->loc.source->str);
  LabelId names_id = StaticNames(fout, label_id, path->path);
  LabelId path_id = (*label_id)++;

  fprintf(fout, "static FbleModulePath " LABEL " = {\n", path_id);
  fprintf(fout, "  .refcount = 1,\n");
  fprintf(fout, "  .magic = FBLE_MODULE_PATH_MAGIC,\n");
  fprintf(fout, "  .loc = { .source = &" LABEL ", .line = %zi, .col = %zi },\n",
      src_id, path->loc.line, path->loc.col);
  fprintf(fout, "  .path = { .size = %zi, .xs = " LABEL "},\n",
      path->path.size, names_id);
  fprintf(fout, "};\n");
  return path_id;
}

/**
 * @func[StaticPreloadedModule]
 * @ Generates code to declare a static FblePreloadedModule value.
 *  @arg[FILE*][fout] The output stream to write the code to.
 *  @arg[LabelId*][label_id] Pointer to next available label id for use.
 *  @arg[FbleModule*][module] The FbleModule to generate code for.
 *
 *  @sideeffects
 *   @i Outputs code to fout.
 *   @i Increments label_id based on the number of internal labels used.
 */
static void StaticPreloadedModule(FILE* fout, LabelId* label_id, FbleModule* module)
{
  LabelId path_id = StaticModulePath(fout, label_id, module->path);

  for (size_t i = 0; i < module->link_deps.size; ++i) {
    FbleString* dep_name = LabelForPath(module->link_deps.xs[i]);
    fprintf(fout, "extern FblePreloadedModule %s;\n", dep_name->str);
    FbleFreeString(dep_name);
  }

  LabelId deps_xs_id = (*label_id)++;
  fprintf(fout, "static FblePreloadedModule* " LABEL "[] = {\n", deps_xs_id);
  for (size_t i = 0; i < module->link_deps.size; ++i) {
    FbleString* dep_name = LabelForPath(module->link_deps.xs[i]);
    fprintf(fout, "  &%s,\n", dep_name->str);
    FbleFreeString(dep_name);
  }
  fprintf(fout, "};\n");

  LabelId executable_id = (*label_id)++;
  fprintf(fout, "static FbleExecutable " LABEL " = {\n", executable_id);
  fprintf(fout, "  .num_args = %zi, \n", module->code->executable.num_args);
  fprintf(fout, "  .num_statics = %zi,\n", module->code->executable.num_statics);
  fprintf(fout, "  .max_call_args = %zi,\n", module->code->executable.max_call_args);

  FbleName function_block = module->profile_blocks.xs[module->code->profile_block_id];
  char function_label[SizeofSanitizedString(function_block.name->str)];
  SanitizeString(function_block.name->str, function_label);
  fprintf(fout, "  .run = &%s_%04zx,\n", function_label, module->code->profile_block_id);
  fprintf(fout, "};\n");

  LabelId profile_blocks_xs_id = StaticNames(fout, label_id, module->profile_blocks);

  FbleString* module_name = LabelForPath(module->path);
  fprintf(fout, "FblePreloadedModule %s = {\n", module_name->str);
  fprintf(fout, "  .path = &" LABEL ",\n", path_id);
  fprintf(fout, "  .deps = { .size = %zi, .xs = " LABEL "},\n",
      module->link_deps.size, deps_xs_id);
  fprintf(fout, "  .executable = &" LABEL ",\n", executable_id);
  fprintf(fout, "  .profile_blocks = { .size = %zi, .xs = " LABEL "},\n",
      module->profile_blocks.size, profile_blocks_xs_id);
  fprintf(fout, "};\n");
  FbleFreeString(module_name);
}

/**
 * @func[ReturnAbort] Emits code to return an error from a Run function.
 *  @arg[FILE*][fout] The output stream.
 *  @arg[const char*][lmsg] The name of the error message to use.
 *  @arg[FbleLoc][loc] The location to report with the error message.
 *
 *  @sideeffects
 *   Emits code to return the error.
 */
static void ReturnAbort(FILE* fout, const char* lmsg, FbleLoc loc)
{
  fprintf(fout, "{\n");
  fprintf(fout, "    ReportAbort(%s, %zi, %zi);\n", lmsg, loc.line, loc.col);
  fprintf(fout, "    return NULL;\n");
  fprintf(fout, "  }\n");
}

/**
 * @func[EmitCode] Generates code to execute an FbleCode block.
 *  @arg[FILE*][fout] The output stream to write the code to.
 *  @arg[FbleNameV][profile_blocks]
 *   The list of profile block names for the module.
 *  @arg[FbleCode*][code] The block of code to generate a C function for.
 *
 *  @sideeffects
 *   Outputs code to fout with two space indent.
 */
static void EmitCode(FILE* fout, FbleNameV profile_blocks, FbleCode* code)
{
  FbleName block = profile_blocks.xs[code->profile_block_id];
  char label[SizeofSanitizedString(block.name->str)];
  SanitizeString(block.name->str, label);
  fprintf(fout, "static FbleValue* %s_%04zx("
      "FbleValueHeap* heap, FbleProfileThread* profile, "
      "FbleFunction* function, FbleValue** args)\n",
      label, code->profile_block_id);
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleValue** a = args;\n");
  fprintf(fout, "  FbleValue* l[%zi];\n", code->num_locals);
  fprintf(fout, "  FbleValue** s = function->statics;\n");

  fprintf(fout, "  FbleBlockId profile_block_id = function->profile_block_id;\n");

  // x0, f0 are temporary variables individual instructions can use however
  // they wish.
  fprintf(fout, "  FbleValue* x0 = NULL;\n");
  fprintf(fout, "  FbleFunction* f0 = NULL;\n");

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
    if (instr->profile_ops != NULL) {
      fprintf(fout, "  if (profile) {\n");
      for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
        switch (op->tag) {
          case FBLE_PROFILE_ENTER_OP: {
            fprintf(fout, "    FbleProfileEnterBlock(profile, profile_block_id + %zi);\n", op->arg);
            break;
          }

          case FBLE_PROFILE_REPLACE_OP: {
            fprintf(fout, "    FbleProfileReplaceBlock(profile, profile_block_id + %zi);\n", op->arg);
            break;
          }

          case FBLE_PROFILE_EXIT_OP: {
            fprintf(fout, "    FbleProfileExitBlock(profile);\n");
            break;
          }

          case FBLE_PROFILE_SAMPLE_OP: {
            fprintf(fout, "    FbleProfileRandomSample(profile, %zi);\n", op->arg);
            break;
          }
        }
      }
      fprintf(fout, "  }\n");
    }

    // Instruction logic.
    static const char* var_tag[] = { "s", "a", "l" };
    switch (instr->tag) {
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
        fprintf(fout, "  l[%zi] = FbleNewUnionValue(heap, %zi, %zi, %s[%zi]);\n",
            union_instr->dest, union_instr->tagwidth, union_instr->tag,
            var_tag[union_instr->arg.tag],
            union_instr->arg.index);
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        fprintf(fout, "  l[%zi] = FbleStructValueField(%s[%zi], %zi, %zi);\n",
            access_instr->dest, var_tag[access_instr->obj.tag],
            access_instr->obj.index, access_instr->fieldc, access_instr->tag);
        fprintf(fout, "  if (l[%zi] == NULL) ", access_instr->dest);
        ReturnAbort(fout, "UndefinedStructValue", access_instr->loc);
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        fprintf(fout, "  l[%zi] = FbleUnionValueField(%s[%zi], %zi, %zi);\n",
            access_instr->dest, var_tag[access_instr->obj.tag],
            access_instr->obj.index, access_instr->tagwidth, access_instr->tag);

        fprintf(fout, "  if (l[%zi] == NULL) ", access_instr->dest);
        ReturnAbort(fout, "UndefinedUnionValue", access_instr->loc);

        fprintf(fout, "  if (l[%zi] == FbleWrongUnionTag) {", access_instr->dest);
        fprintf(fout, "    l[%zi] = NULL;", access_instr->dest);
        ReturnAbort(fout, "WrongUnionTag", access_instr->loc);
        fprintf(fout, "  }");
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;

        fprintf(fout, "  switch (FbleUnionValueTag(%s[%zi], %zi)) {\n",
            var_tag[select_instr->condition.tag],
            select_instr->condition.index,
            select_instr->tagwidth);
        fprintf(fout, "    case -1:");
        ReturnAbort(fout, "UndefinedUnionSelect", select_instr->loc);

        for (size_t i = 0; i < select_instr->targets.size; ++i) {
          size_t tag = select_instr->targets.xs[i].tag;
          size_t target = select_instr->targets.xs[i].target;
          assert(target > pc);
          jump_target[target] = true;
          fprintf(fout, "    case %zi: goto pc_%zi;\n", tag, target);
        }
        size_t target = select_instr->default_;
        assert(target > pc);
        jump_target[target] = true;
        fprintf(fout, "    default: goto pc_%zi;\n", target);
        fprintf(fout, "  }\n");
        break;
      }

      case FBLE_GOTO_INSTR: {
        FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
        size_t target = goto_instr->target;
        assert(target > pc);
        jump_target[target] = true;
        fprintf(fout, "  goto pc_%zi;\n", target);
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;

        FbleName function_block = profile_blocks.xs[func_instr->code->profile_block_id];
        char function_label[SizeofSanitizedString(function_block.name->str)];
        SanitizeString(function_block.name->str, function_label);

        fprintf(fout, "  static FbleExecutable exe_%zi = {\n", exe_id);
        fprintf(fout, "    .num_args = %zi,\n", func_instr->code->executable.num_args);
        fprintf(fout, "    .num_statics = %zi,\n", func_instr->code->executable.num_statics);
        fprintf(fout, "    .max_call_args = %zi,\n", func_instr->code->executable.max_call_args);
        fprintf(fout, "    .run = &%s_%04zx,\n", function_label, func_instr->code->profile_block_id);
        fprintf(fout, "  };\n");

        fprintf(fout, "  FbleValue* fv%zi[%zi] = {", pc, func_instr->code->executable.num_statics);
        for (size_t i = 0; i < func_instr->code->executable.num_statics; ++i) {
          fprintf(fout, "%s[%zi], ",
              var_tag[func_instr->scope.xs[i].tag],
              func_instr->scope.xs[i].index);
        }
        fprintf(fout, "};\n");

        fprintf(fout, "  l[%zi] = FbleNewFuncValue(heap, &exe_%zi, profile_block_id + %zi, fv%zi",
            func_instr->dest, exe_id, func_instr->profile_block_offset, pc);
        exe_id++;
        fprintf(fout, ");\n");
        break;
      }

      case FBLE_CALL_INSTR: {
        FbleCallInstr* call_instr = (FbleCallInstr*)instr;

        fprintf(fout, "  FbleValue* ca%zi[%zi] = {", pc, call_instr->args.size);
        for (size_t i = 0; i < call_instr->args.size; ++i) {
          fprintf(fout, "%s[%zi],",
              var_tag[call_instr->args.xs[i].tag],
              call_instr->args.xs[i].index);
        }
        fprintf(fout, "};\n");
        fprintf(fout, "  l[%zi] = FbleCall(heap, profile, %s[%zi], %zi, ca%zi);\n",
            call_instr->dest,
            var_tag[call_instr->func.tag], call_instr->func.index,
            call_instr->args.size, pc);
        fprintf(fout, "  if (l[%zi] == NULL) ", call_instr->dest);
        ReturnAbort(fout, "CalleeAborted", call_instr->loc);
        break;
      }

      case FBLE_TAIL_CALL_INSTR: {
        FbleTailCallInstr* call_instr = (FbleTailCallInstr*)instr;

        fprintf(fout, "  f0 = FbleFuncValueFunction(%s[%zi]);\n",
            var_tag[call_instr->func.tag],
            call_instr->func.index);
        fprintf(fout, "  if (f0 == NULL) ");
        ReturnAbort(fout, "UndefinedFunctionValue", call_instr->loc);

        fprintf(fout, "  heap->tail_call_buffer[0] = %s[%zi];\n",
            var_tag[call_instr->func.tag],
            call_instr->func.index);
        fprintf(fout, "  heap->tail_call_argc = %zi;\n", call_instr->args.size);
        for (size_t i = 0; i < call_instr->args.size; ++i) {
          fprintf(fout, "  heap->tail_call_buffer[%zi] = %s[%zi];\n", i + 1,
              var_tag[call_instr->args.xs[i].tag],
              call_instr->args.xs[i].index);
        }

        fprintf(fout, "  return heap->tail_call_sentinel;\n");
        break;
      }

      case FBLE_COPY_INSTR: {
        FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
        fprintf(fout, "  l[%zi] = %s[%zi];\n",
            copy_instr->dest,
            var_tag[copy_instr->source.tag],
            copy_instr->source.index);
        break;
      }

      case FBLE_REC_DECL_INSTR: {
        FbleRecDeclInstr* decl_instr = (FbleRecDeclInstr*)instr;
        fprintf(fout, "  l[%zi] = FbleDeclareRecursiveValues(heap, %zi);\n",
            decl_instr->dest, decl_instr->n);
        break;
      }

      case FBLE_REC_DEFN_INSTR: {
        FbleRecDefnInstr* defn_instr = (FbleRecDefnInstr*)instr;
        fprintf(fout, "  switch (FbleDefineRecursiveValues(heap, l[%zi], l[%zi])) {\n",
            defn_instr->decl, defn_instr->defn);
        fprintf(fout, "    case 0: break;\n");
        for (size_t i = 0; i < defn_instr->locs.size; ++i) {
          fprintf(fout, "    case %zi: ", i+1);
          ReturnAbort(fout, "VacuousValue", defn_instr->locs.xs[i]);
        }
        fprintf(fout, "  }\n");
        break;
      }

      case FBLE_RETURN_INSTR: {
        FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
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
        fprintf(fout, "  l[%zi] = FbleNewLiteralValue(heap, %zi, %zi, lit_%zi);\n",
            literal_instr->dest, literal_instr->tagwidth, argc, lit_id);
        lit_id++;
        break;
      }

      case FBLE_NOP_INSTR: {
        break;
      }

      case FBLE_UNDEF_INSTR: {
        FbleUndefInstr* undef_instr = (FbleUndefInstr*)instr;
        fprintf(fout, "  l[%zi] = NULL;\n", undef_instr->dest);
        break;
      }
    }
  }
  fprintf(fout, "}\n");
}

/**
 * @func[SizeofSanitizedString]
 * @ Returns the size of the label-sanitized version of a given string.
 *  @arg[const char*][str] The string to get the sanitized size of.
 *
 *  @returns[size_t]
 *   The number of bytes needed for the sanitized version of the given string,
 *   including nul terminator.
 *
 *  @sideeffects
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
 * @func[SanitizeString]
 * @ Returns a version of the string suitable for use in labels.
 *  @arg[const char*][str] The string to sanitize.
 *  @arg[char*][dst]
 *   A character buffer of size SizeofSanitizedString(str) to write the
 *   sanitized string to.
 *
 *  @sideeffects
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
 * @func[LabelForPath] Returns a C identifier for a module path. 
 *  @arg[FbleMOdulePath*][path] The path to get the name for.
 *
 *  @returns[FbleString*]
 *   A C function name for the module path.
 *
 *  @sideeffects
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

// See documentation in fble-generate.h.
void FbleGenerateC(FILE* fout, FbleModule* module)
{
  FbleCodeV blocks;
  FbleInitVector(blocks);

  CollectBlocks(&blocks, module->code);

  fprintf(fout, "#include <fble/fble-program.h>\n");  // for FbleNativedModule
  fprintf(fout, "#include <fble/fble-function.h>\n"); // for FbleCall
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

  // Generate prototypes for all the run functions.
  FbleNameV profile_blocks = module->profile_blocks;
  for (size_t i = 0; i < blocks.size; ++i) {
    FbleCode* code = blocks.xs[i];
    FbleName function_block = profile_blocks.xs[code->profile_block_id];
    char function_label[SizeofSanitizedString(function_block.name->str)];
    SanitizeString(function_block.name->str, function_label);
    fprintf(fout, "static FbleValue* %s_%04zx("
      "FbleValueHeap* heap, FbleProfileThread* profile, "
      "FbleFunction* function, FbleValue** args);\n",
        function_label, code->profile_block_id);
  }

  // Generate the implementations of all the run functions.
  for (size_t i = 0; i < blocks.size; ++i) {
    EmitCode(fout, profile_blocks, blocks.xs[i]);
  }

  LabelId label_id = 0;
  StaticPreloadedModule(fout, &label_id, module);

  FbleFreeVector(blocks);
}

// See documentation in fble-generate.h.
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path)
{
  FbleString* module_name = LabelForPath(path);
  fprintf(fout, "#include <fble/fble-link.h>\n");   // for FbleExecutableProgram
  fprintf(fout, "extern FblePreloadedModule %s;\n", module_name->str);
  fprintf(fout, "FblePreloadedModule* %s = &%s;\n", name, module_name->str);
  FbleFreeString(module_name);
}

// See documentation in fble-generate.h.
void FbleGenerateCMain(FILE* fout, const char* main, FbleModulePath* path)
{
  FbleString* module_name = LabelForPath(path);

  fprintf(fout, "#include <fble/fble-link.h>\n");
  fprintf(fout, "extern FblePreloadedModule %s;\n", module_name->str);
  fprintf(fout, "int %s(int argc, const char** argv, FblePreloadedModule* module);\n", main);
  fprintf(fout, "int main(int argc, const char** argv)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  return %s(argc, argv, &%s);\n", main, module_name->str);
  fprintf(fout, "}\n");

  FbleFreeString(module_name);
}
