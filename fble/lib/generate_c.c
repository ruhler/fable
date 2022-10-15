// generate_c.c --
//   This file describes code to generate C code for fble modules.

#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdio.h>    // for sprintf
#include <stdarg.h>   // for va_list, va_start, va_end.
#include <stddef.h>   // for offsetof
#include <string.h>   // for strlen, strcat
#include <unistd.h>   // for getcwd

#include "fble-vector.h"    // for FbleVectorInit, etc.

#include "code.h"
#include "fble-compile.h"
#include "tc.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// LabelId --
//   Type representing a name as an integer.
//
// The number is turned into a label using printf format LABEL.
typedef unsigned int LabelId;
#define LABEL "l%x"

typedef struct {
  size_t size;
  const char** xs;
} LocV;

static void AddLoc(const char* source, LocV* locs);
static void CollectBlocksAndLocs(FbleCodeV* blocks, LocV* locs, FbleCode* code);

static void StringLit(FILE* fout, const char* string);
static LabelId StaticString(FILE* fout, LabelId* label_id, const char* string);
static LabelId StaticNames(FILE* fout, LabelId* label_id, FbleNameV names);
static LabelId StaticModulePath(FILE* fout, LabelId* label_id, FbleModulePath* path);
static LabelId StaticExecutableModule(FILE* fout, LabelId* label_id, FbleCompiledModule* module);

static void ReturnAbort(FILE* fout, void* code, size_t pc, const char* lmsg, FbleLoc loc);

static void EmitInstr(FILE* fout, FbleNameV profile_blocks, void* code, size_t pc, FbleInstr* instr);
static void EmitCode(FILE* fout, FbleNameV profile_blocks, FbleCode* code);
static void EmitInstrForAbort(FILE* fout, void* code, size_t pc, FbleInstr* instr);
static void EmitCodeForAbort(FILE* fout, FbleNameV profile_blocks, FbleCode* code);
static size_t SizeofSanitizedString(const char* str);
static void SanitizeString(const char* str, char* dst);
static FbleString* LabelForPath(FbleModulePath* path);

// AddLoc --
//   Add a source location to the list of locations.
//
// Inputs:
//   source - the source file name to add
//   locs - the list of locs to add to.
//
// Side effects:
//   Adds the source filename to the list of locations if it is not already
//   present in the list.
static void AddLoc(const char* source, LocV* locs)
{
  for (size_t i = 0; i < locs->size; ++i) {
    if (strcmp(source, locs->xs[i]) == 0) {
      return;
    }
  }
  FbleVectorAppend(*locs, source);
}

// CollectBlocksAndLocs --
//   Get the list of all instruction blocks and location source file names
//   referenced from the given block of code, including the code itself.
//
// Inputs:
//   blocks - the collection of blocks to add to.
//   locs - the collection of location source names to add to.
//   code - the code to collect the blocks from.
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

      case FBLE_JUMP_INSTR: break;

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

      case FBLE_COPY_INSTR: break;
      case FBLE_REF_VALUE_INSTR: break;

      case FBLE_REF_DEF_INSTR: {
        FbleRefDefInstr* instr = (FbleRefDefInstr*)code->instrs.xs[i];
        AddLoc(instr->loc.source->str, locs);
        break;
      }

      case FBLE_RETURN_INSTR: break;
      case FBLE_TYPE_INSTR: break;
      case FBLE_RELEASE_INSTR: break;
      case FBLE_LIST_INSTR: break;
      case FBLE_LITERAL_INSTR: break;
    }
  }
}

// StringLit --
//   Output a string literal to fout.
//
// Inputs:
//   fout - the file to write to.
//   string - the contents of the string to write.
//
// Side effects:
//   Outputs the given string as a C string literal to the given file.
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

// StaticString --
//   Output code to declare a static FbleString value.
//
// Inputs:
//   fout - the file to write to
//   label_id - pointer to next available label id for use.
//   string - the value of the string.
//
// Returns:
//   A label id of a local, static FbleString.
//
// Side effects:
//   Writes code to fout and allocates label ids out of label_id.
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

// StaticNames --
//   Output code to declare a static FbleNameV.xs value.
//
// Inputs:
//   fout - the file to write to
//   label_id - pointer to next available label id for use.
//   names - the value of the names.
//
// Returns:
//   A label id of a local, static FbleNameV.xs.
//
// Side effects:
//   Writes code to fout and allocates label ids out of label_id.
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

// StaticModulePath --
//   Generate code to declare a static FbleModulePath value.
//
// Inputs:
//   fout - the output stream to write the code to.
//   label_id - pointer to next available label id for use.
//   path - the FbleModulePath to generate code for.
//
// Results:
//   The label id of a local, static FbleModulePath.
//
// Side effects:
// * Outputs code to fout.
// * Increments label_id based on the number of internal labels used.
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

// StaticExecutableModule --
//   Generate code to declare a static FbleExecutableModule value.
//
// Inputs:
//   fout - the output stream to write the code to.
//   label_id - pointer to next available label id for use.
//   module - the FbleCompiledModule to generate code for.
//
// Results:
//   The label id of a local, static FbleExecutableModule.
//
// Side effects:
// * Outputs code to fout.
// * Increments label_id based on the number of internal labels used.
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

  LabelId profile_blocks_xs_id = StaticNames(fout, label_id, module->code->_base.profile_blocks);

  LabelId executable_id = (*label_id)++;
  fprintf(fout, "static FbleExecutable " LABEL " = {\n", executable_id);
  fprintf(fout, "  .refcount = 1,\n");
  fprintf(fout, "  .magic = FBLE_EXECUTABLE_MAGIC,\n");
  fprintf(fout, "  .args = %zi, \n", module->code->_base.args);
  fprintf(fout, "  .statics = %zi,\n", module->code->_base.statics);
  fprintf(fout, "  .locals = %zi,\n", module->code->_base.locals);
  fprintf(fout, "  .profile = %zi,\n", module->code->_base.profile);
  fprintf(fout, "  .profile_blocks = { .size = %zi, .xs = " LABEL "},\n",
      module->code->_base.profile_blocks.size, profile_blocks_xs_id);

  FbleName function_block = module->code->_base.profile_blocks.xs[module->code->_base.profile];
  char function_label[SizeofSanitizedString(function_block.name->str)];
  SanitizeString(function_block.name->str, function_label);
  fprintf(fout, "  .run = &_Run_%p_%s,\n", (void*)module->code, function_label);
  fprintf(fout, "  .abort = &_Abort_%p_%s,\n", (void*)module->code, function_label);
  fprintf(fout, "  .on_free = &FbleExecutableNothingOnFree\n");
  fprintf(fout, "};\n");

  LabelId module_id = (*label_id)++;
  fprintf(fout, "static FbleExecutableModule " LABEL " = {\n", module_id);
  fprintf(fout, "  .refcount = 1,\n");
  fprintf(fout, "  .magic = FBLE_EXECUTABLE_MODULE_MAGIC,\n");
  fprintf(fout, "  .path = &" LABEL ",\n", path_id);
  fprintf(fout, "  .deps = { .size = %zi, .xs = " LABEL "},\n",
      module->deps.size, deps_xs_id);
  fprintf(fout, "  .executable = &" LABEL "\n", executable_id);
  fprintf(fout, "};\n");
  return module_id;
}

// ReturnAbort --
//   Emit code to return an error from a Run function.
//
// Inputs:
//   fout - the output stream.
//   code - pointer to current code block to use for labels.
//   pc - the program counter of the abort location.
//   msg - the error message to use.
//   loc - the location to report with the error message.
//
// Side effects:
//   Emit code to return the error.
static void ReturnAbort(FILE* fout, void* code, size_t pc, const char* msg, FbleLoc loc)
{
  fprintf(fout, "    thread->stack->pc = %zi;\n", pc);
  fprintf(fout, "    fprintf(stderr, \"%s:%d:%d: error: %s\\n\");\n",
      loc.source->str, loc.line, loc.col, msg);
  fprintf(fout, "    return FBLE_EXEC_ABORTED;\n");
}

// EmitInstr --
//   Generate code to execute an instruction.
//
// Inputs:
//   fout - the output stream to write the code to.
//   profile_blocks - the list of profile block names for the module.
//   code - pointer to the current code block, for referencing labels.
//   pc - the program counter of the instruction.
//   instr - the instruction to execute.
//
// Side effects:
// * Outputs code to fout.
static void EmitInstr(FILE* fout, FbleNameV profile_blocks, void* code, size_t pc, FbleInstr* instr)
{
  fprintf(fout, "  if (profile) {\n");
  fprintf(fout, "    if (rand() %% 1024 == 0) FbleProfileSample(profile, 1);\n");
  for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
    switch (op->tag) {
      case FBLE_PROFILE_ENTER_OP: {
        fprintf(fout, "    FbleProfileEnterBlock(profile, %zi);\n", op->block);
        break;
      }

      case FBLE_PROFILE_REPLACE_OP: {
        fprintf(fout, "    FbleProfileReplaceBlock(profile, %zi);\n", op->block);
        break;
      }

      case FBLE_PROFILE_EXIT_OP: {
        fprintf(fout, "    FbleProfileExitBlock(profile);\n");
        break;
      }
    }
  }
  fprintf(fout, "  }\n");

  static const char* section[] = { "s", "l" };
  switch (instr->tag) {
    case FBLE_DATA_TYPE_INSTR: {
      FbleDataTypeInstr* dt_instr = (FbleDataTypeInstr*)instr;
      size_t fieldc = dt_instr->fields.size;

      fprintf(fout, "  {\n");
      fprintf(fout, "    FbleValue* fields[%zi];\n", fieldc);
      for (size_t i = 0; i < fieldc; ++i) {
        fprintf(fout, "    fields[%zi] = %s[%zi];\n", i,
            section[dt_instr->fields.xs[i].section],
            dt_instr->fields.xs[i].index);
      };

      static const char* dtkind[] = { "FBLE_STRUCT_DATATYPE", "FBLE_UNION_DATATYPE" };
      fprintf(fout, "    l[%zi] = FbleNewDataTypeValue(heap, %s, %zi, fields);\n",
          dt_instr->dest, dtkind[dt_instr->kind], fieldc);
      fprintf(fout, "  }\n");
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      size_t argc = struct_instr->args.size;

      fprintf(fout, "  l[%zi] = FbleNewStructValue(heap, %zi", struct_instr->dest, argc);
      for (size_t i = 0; i < argc; ++i) {
        fprintf(fout, ", %s[%zi]",
            section[struct_instr->args.xs[i].section],
            struct_instr->args.xs[i].index);
      };

      fprintf(fout, ");\n");
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      fprintf(fout, "  l[%zi] = FbleNewUnionValue(heap, %zi, %s[%zi]);\n",
          union_instr->dest, union_instr->tag,
          section[union_instr->arg.section],
          union_instr->arg.index);
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "  x0 = FbleStrictValue(%s[%zi]);\n",
          section[access_instr->obj.section], access_instr->obj.index);
      fprintf(fout, "  if (!x0) {\n");
      ReturnAbort(fout, code, pc, "undefined struct value access", access_instr->loc);
      fprintf(fout, "  }\n");

      fprintf(fout, "  l[%zi] = FbleStructValueAccess(x0, %zi);\n",
          access_instr->dest, access_instr->tag);
      fprintf(fout, "  FbleRetainValue(heap, l[%zi]);\n", access_instr->dest);
      return;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "  x0 = FbleStrictValue(%s[%zi]);\n",
          section[access_instr->obj.section], access_instr->obj.index);
      fprintf(fout, "  if (!x0) {\n");
      ReturnAbort(fout, code, pc, "undefined union value access", access_instr->loc);
      fprintf(fout, "  }\n");

      fprintf(fout, "  if (%zi != FbleUnionValueTag(x0)) {\n", access_instr->tag);
      ReturnAbort(fout, code, pc, "union field access undefined: wrong tag", access_instr->loc);
      fprintf(fout, "  }\n");

      fprintf(fout, "  l[%zi] = FbleUnionValueAccess(x0);\n", access_instr->dest);
      fprintf(fout, "  FbleRetainValue(heap, l[%zi]);\n", access_instr->dest);
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;

      fprintf(fout, "  x0 = FbleStrictValue(%s[%zi]);\n",
          section[select_instr->condition.section],
          select_instr->condition.index);
      fprintf(fout, "  if (!x0) {\n");
      ReturnAbort(fout, code, pc, "undefined union value select", select_instr->loc);
      fprintf(fout, "  }\n");

      fprintf(fout, "  switch (FbleUnionValueTag(x0)) {\n");
      for (size_t i = 0; i < select_instr->jumps.size; ++i) {
        fprintf(fout, "    case %zi: goto pc_%zi;\n", i, pc + 1 + select_instr->jumps.xs[i]);
      }
      fprintf(fout, "  }\n");
      return;
    }

    case FBLE_JUMP_INSTR: {
      FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
      fprintf(fout, "  goto pc_%zi;\n", pc + 1 + jump_instr->count);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;

      FbleName function_block = profile_blocks.xs[func_instr->code->_base.profile];
      char function_label[SizeofSanitizedString(function_block.name->str)];
      SanitizeString(function_block.name->str, function_label);

      fprintf(fout, "  {\n");
      fprintf(fout, "    static FbleExecutable e = {\n");
      fprintf(fout, "      .refcount = 1,\n");
      fprintf(fout, "      .magic = FBLE_EXECUTABLE_MAGIC,\n");
      fprintf(fout, "      .args = %zi,\n", func_instr->code->_base.args);
      fprintf(fout, "      .statics = %zi,\n", func_instr->code->_base.statics);
      fprintf(fout, "      .locals = %zi,\n", func_instr->code->_base.locals);
      fprintf(fout, "      .profile = %zi,\n", func_instr->code->_base.profile);
      fprintf(fout, "      .profile_blocks = { .size = 0, .xs = NULL },\n");
      fprintf(fout, "      .run = &_Run_%p_%s,\n", (void*)func_instr->code, function_label);
      fprintf(fout, "      .abort = &_Abort_%p_%s,\n", (void*)func_instr->code, function_label);
      fprintf(fout, "      .on_free = NULL\n");
      fprintf(fout, "    };\n");

      fprintf(fout, "    FbleValue* statics[%zi];\n", func_instr->code->_base.statics);
      for (size_t i = 0; i < func_instr->code->_base.statics; ++i) {
        fprintf(fout, "    statics[%zi] = %s[%zi];\n", i,
            section[func_instr->scope.xs[i].section],
            func_instr->scope.xs[i].index);
      }

      fprintf(fout, "    l[%zi] = FbleNewFuncValue(heap, &e, profile_base_id, statics);\n",
          func_instr->dest);
      fprintf(fout, "  }\n");
      return;
    }

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;

      fprintf(fout, "  x0 = FbleStrictValue(%s[%zi]);\n",
          section[call_instr->func.section],
          call_instr->func.index);
      fprintf(fout, "  if (!x0) {\n");
      ReturnAbort(fout, code, pc, "called undefined function", call_instr->loc);
      fprintf(fout, "  }\n");

      fprintf(fout, "  {\n");
      fprintf(fout, "    FbleValue* args[%zi];\n", call_instr->args.size);
      for (size_t i = 0; i < call_instr->args.size; ++i) {
        fprintf(fout, "    args[%zi] = %s[%zi];\n", i,
            section[call_instr->args.xs[i].section],
            call_instr->args.xs[i].index);
      }

      if (call_instr->exit) {
        fprintf(fout, "    FbleRetainValue(heap, x0);\n");

        for (size_t i = 0; i < call_instr->args.size; ++i) {
          // We need to do a Retain on every arg from statics. For args from
          // locals, we don't need to do a Retain on the arg the first time we
          // see the local, because we can transfer the caller's ownership of
          // the local to the callee for that arg.
          bool retain = call_instr->args.xs[i].section != FBLE_LOCALS_FRAME_SECTION;
          for (size_t j = 0; j < i; ++j) {
            if (call_instr->args.xs[i].section == call_instr->args.xs[j].section
                && call_instr->args.xs[i].index == call_instr->args.xs[j].index) {
              retain = true;
              break;
            }
          }

          if (retain) {
            fprintf(fout, "    FbleRetainValue(heap, args[%zi]);\n", i);
          }
        }

        if (call_instr->func.section == FBLE_LOCALS_FRAME_SECTION) {
          fprintf(fout, "    FbleReleaseValue(heap, l[%zi]);\n", call_instr->func.index);
        }

        fprintf(fout, "    FbleThreadTailCall(heap, x0, args, thread);\n");
        fprintf(fout, "    return FBLE_EXEC_CONTINUED;\n");
        fprintf(fout, "  }\n");
        return;
      }

      // stack->pc = pc + 1
      fprintf(fout, "    thread->stack->pc = %zi;\n", pc + 1);
      fprintf(fout, "    FbleThreadCall(heap, l+%zi, x0, args, thread);\n", call_instr->dest);
      fprintf(fout, "    FbleExecStatus status;\n");
      fprintf(fout, "    do {\n");
      fprintf(fout, "      status = FbleFuncValueExecutable(thread->stack->func)->run(heap, thread);\n");
      fprintf(fout, "    } while (status == FBLE_EXEC_CONTINUED);\n");
      fprintf(fout, "    if (status != FBLE_EXEC_FINISHED) return status;\n");
      fprintf(fout, "  }\n");
      return;
    }

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      fprintf(fout, "  l[%zi] = %s[%zi];\n",
          copy_instr->dest,
          section[copy_instr->source.section],
          copy_instr->source.index);
      fprintf(fout, "  FbleRetainValue(heap, l[%zi]);\n", copy_instr->dest);
      return;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      fprintf(fout, "  l[%zi] = FbleNewRefValue(heap);\n", ref_instr->dest);
      return;
    }

    case FBLE_REF_DEF_INSTR: {
      FbleRefDefInstr* ref_instr = (FbleRefDefInstr*)instr;

      fprintf(fout, "  if (!FbleAssignRefValue(heap, l[%zi], %s[%zi])) {\n",
          ref_instr->ref,
          section[ref_instr->value.section],
          ref_instr->value.index);
      ReturnAbort(fout, code, pc, "vacuous value", ref_instr->loc);
      fprintf(fout, "  }\n");
      return;
    }

    case FBLE_RETURN_INSTR: {
      FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;

      switch (return_instr->result.section) {
        case FBLE_STATICS_FRAME_SECTION: {
          fprintf(fout, "  FbleRetainValue(heap, %s[%zi]);\n",
              section[return_instr->result.section],
              return_instr->result.index);
          break;
        }

        case FBLE_LOCALS_FRAME_SECTION: break;
      }

      fprintf(fout, "  FbleThreadReturn(heap, thread, %s[%zi]);\n",
          section[return_instr->result.section],
          return_instr->result.index);

      fprintf(fout, "  return FBLE_EXEC_FINISHED;\n");
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      fprintf(fout, "  l[%zi] = FbleGenericTypeValue;\n", type_instr->dest);
      return;
    }

    case FBLE_RELEASE_INSTR: {
      FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
      fprintf(fout, "  FbleReleaseValue(heap, l[%zi]);\n", release_instr->target);
      return;
    }

    case FBLE_LIST_INSTR: {
      FbleListInstr* list_instr = (FbleListInstr*)instr;
      size_t argc = list_instr->args.size;

      fprintf(fout, "  {\n");
      fprintf(fout, "    FbleValue* args[%zi];\n", argc);
      for (size_t i = 0; i < argc; ++i) {
        fprintf(fout, "    args[%zi] = %s[%zi];\n", i,
            section[list_instr->args.xs[i].section],
            list_instr->args.xs[i].index);
      }

      fprintf(fout, "    l[%zi] = FbleNewListValue(heap, %zi, args);\n", list_instr->dest, argc);
      fprintf(fout, "  }\n");
      return;
    }

    case FBLE_LITERAL_INSTR: {
      FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
      size_t argc = literal_instr->letters.size;
      fprintf(fout, "  {\n");
      fprintf(fout, "    size_t args[%zi];\n", argc);
      for (size_t i = 0; i < argc; ++i) {
        fprintf(fout, "    args[%zi] = %zi;\n", i, literal_instr->letters.xs[i]);
      }
      fprintf(fout, "    l[%zi] = FbleNewLiteralValue(heap, %zi, args);\n",
          literal_instr->dest, argc);
      fprintf(fout, "  }\n");
      return;
    }
  }
}

// EmitCode --
//   Generate code to execute an FbleCode block.
//
// Inputs:
//   fout - the output stream to write the code to.
//   profile_blocks - the list of profile block names for the module.
//   code - the block of code to generate a C function for.
//
// Side effects:
// * Outputs code to fout with two space indent.
static void EmitCode(FILE* fout, FbleNameV profile_blocks, FbleCode* code)
{
  FbleName function_block = profile_blocks.xs[code->_base.profile];
  char function_label[SizeofSanitizedString(function_block.name->str)];
  SanitizeString(function_block.name->str, function_label);
  fprintf(fout, "static FbleExecStatus _Run_%p_%s(FbleValueHeap* heap, FbleThread* thread)\n", (void*)code, function_label);
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleProfileThread* profile = thread->profile;\n");
  fprintf(fout, "  FbleStack* stack = thread->stack;\n");
  fprintf(fout, "  FbleValue** l = stack->locals;\n");
  fprintf(fout, "  FbleValue* func = stack->func;\n");
  fprintf(fout, "  FbleValue** s = FbleFuncValueStatics(func);\n");
  fprintf(fout, "  size_t profile_base_id = FbleFuncValueProfileBaseId(func);\n");

  // x0 is a temporary variable individual instructions can use however they
  // wish.
  fprintf(fout, "  FbleValue* x0 = NULL;\n");

  // Emit code for each fble instruction
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, "pc_%zi:\n", i);
    EmitInstr(fout, profile_blocks, code, i, code->instrs.xs[i]);
  }
  fprintf(fout, "}\n");
}

// EmitInstrForAbort --
//   Generate code to execute an instruction for the purposes of abort.
//
// Inputs:
//   fout - the output stream to write the code to.
//   code - pointer to the current code block, for referencing labels.
//   pc - the program counter of the instruction.
//   instr - the instruction to execute.
//
// Side effects:
// * Outputs code to fout.
static void EmitInstrForAbort(FILE* fout, void* code, size_t pc, FbleInstr* instr)
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
        if (call_instr->func.section == FBLE_LOCALS_FRAME_SECTION) {
          fprintf(fout, "  FbleReleaseValue(heap, l[%zi]);\n", call_instr->func.index);
          fprintf(fout, "  l[%zi] = NULL;\n", call_instr->func.index);
        }

        for (size_t i = 0; i < call_instr->args.size; ++i) {
          if (call_instr->args.xs[i].section == FBLE_LOCALS_FRAME_SECTION) {
            fprintf(fout, "  FbleReleaseValue(heap, l[%zi]);\n", call_instr->args.xs[i].index);
            fprintf(fout, "  l[%zi] = NULL;\n", call_instr->args.xs[i].index);
          }
        }

        fprintf(fout, "  stack->result = NULL;\n");
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
      switch (return_instr->result.section) {
        case FBLE_STATICS_FRAME_SECTION: break;
        case FBLE_LOCALS_FRAME_SECTION: {
          fprintf(fout, "  FbleReleaseValue(heap, l[%zi]);\n", return_instr->result.index);
          break;
        }
      }

      fprintf(fout, "  stack->result = NULL;\n");
      fprintf(fout, "  return;\n");
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      fprintf(fout, "  l[%zi] = NULL;\n", type_instr->dest);
      return;
    }

    case FBLE_RELEASE_INSTR: {
      FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
      fprintf(fout, "  FbleReleaseValue(heap, l[%zi]);\n", release_instr->target);
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

// EmitCodeForAbort --
//   Generate code to abort an FbleCode block.
//
// Inputs:
//   fout - the output stream to write the code to.
//   profile_blocks - the list of profile block names for the module.
//   code - the block of code to generate assembly.
//
// Side effects:
// * Outputs generated code to the given output stream.
static void EmitCodeForAbort(FILE* fout, FbleNameV profile_blocks, FbleCode* code)
{
  // Jump table data for jumping to the right fble pc.
  FbleName function_block = profile_blocks.xs[code->_base.profile];
  char function_label[SizeofSanitizedString(function_block.name->str)];
  SanitizeString(function_block.name->str, function_label);
  fprintf(fout, "static void _Abort_%p_%s(FbleValueHeap* heap, FbleStack* stack)\n", (void*)code, function_label);
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleValue** l = stack->locals;\n");
  fprintf(fout, "  switch (stack->pc)\n");
  fprintf(fout, "  {\n");
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, "    case %zi: goto pc_%zi;\n", i, i);
  }
  fprintf(fout, "  }\n");

  // Emit code for each fble instruction
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, "pc_%zi:\n", i);
    EmitInstrForAbort(fout, code, i, code->instrs.xs[i]);
  }
  fprintf(fout, "}\n");
}

// SizeofSanitizedString --
//   Return the size of the label-sanitized version of a given string.
//
// Inputs:
//   str - the string to get the sanitized size of.
//
// Results:
//   The number of bytes needed for the sanitized version of the given string,
//   including nul terminator.
static size_t SizeofSanitizedString(const char* str)
{
  size_t size = 1;
  for (const char* p = str; *p != '\0'; p++) {
    size += isalnum((unsigned char)*p) ? 1 : 4;
  }
  return size;
}

// SanitizeString --
//   Return a version of the string suitable for use in labels.
//
// Inputs:
//   str - the string to sanitize.
//   dst - a character buffer of size SizeofSanitizedString(str) to write
//         the sanitized string to.
//
// Side effects:
//   Fills in dst with the sanitized version of the string.
static void SanitizeString(const char* str, char* dst)
{
  dst[0] = '\0';
  for (const char* p = str; *p != '\0'; p++) {
    char x[5];
    sprintf(x, isalnum((unsigned char)*p) ? "%c" : "_%02x_", *p);
    strcat(dst, x);
  }
}

// LabelForPath --
//   Returns a name suitable for use as a C function identifier to use for the
//   give module path.
//
// Inputs:
//   path - the path to get the name for.
//
// Results:
//   A C function name for the module path.
//
// Side effects:
//   Allocates an FbleString* that should be freed with FbleFreeString when no
//   longer needed.
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

// FbleGenerateC -- see documentation in fble-compile.h
void FbleGenerateC(FILE* fout, FbleCompiledModule* module)
{
  FbleCodeV blocks;
  FbleVectorInit(blocks);

  LocV locs;
  FbleVectorInit(locs);

  CollectBlocksAndLocs(&blocks, &locs, module->code);

  fprintf(fout, "#include <stdlib.h>\n");         // for rand
  fprintf(fout, "#include \"fble-value.h\"\n");   // for FbleFuncValueStatics, etc.
  fprintf(fout, "#include \"execute.h\"\n");
  fprintf(fout, "#include \"value.h\"\n");

  // Generate prototypes for all the run and abort functions.
  FbleNameV profile_blocks = module->code->_base.profile_blocks;
  for (int i = 0; i < blocks.size; ++i) {
    FbleCode* code = blocks.xs[i];
    FbleName function_block = profile_blocks.xs[code->_base.profile];
    char function_label[SizeofSanitizedString(function_block.name->str)];
    SanitizeString(function_block.name->str, function_label);
    fprintf(fout, "static FbleExecStatus _Run_%p_%s(FbleValueHeap* heap, FbleThread* thread);\n",
        (void*)code, function_label);
    fprintf(fout, "static void _Abort_%p_%s(FbleValueHeap* heap, FbleStack* stack);\n",
        (void*)code, function_label);
  }

  // Generate the implementations of all the run and abort functions.
  for (int i = 0; i < blocks.size; ++i) {
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

  FbleFree(blocks.xs);
  FbleFree(locs.xs);
}

// FbleGenerateCExport -- see documentation in fble-compile.h
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path)
{
  FbleString* module_name = LabelForPath(path);

  fprintf(fout, "#include \"fble-execute.h\"\n");
  fprintf(fout, "#include \"fble-value.h\"\n");
  fprintf(fout, "void %s(FbleExecutableProgram* program);\n", module_name->str);
  fprintf(fout, "void %s(FbleExecutableProgram* program)\n", name);
  fprintf(fout, "{\n");
  fprintf(fout, "  return %s(program);\n", module_name->str);
  fprintf(fout, "}\n");
  FbleFreeString(module_name);
}

// FbleGenerateCMain -- see documentation in fble-compile.h
void FbleGenerateCMain(FILE* fout, const char* main, FbleModulePath* path)
{
  FbleString* module_name = LabelForPath(path);

  fprintf(fout, "#include \"fble-link.h\"\n");
  fprintf(fout, "void %s(FbleExecutableProgram* program);\n", module_name->str);
  fprintf(fout, "int %s(int argc, const char** argv, FbleCompiledModuleFunction* module);\n", main);
  fprintf(fout, "int main(int argc, const char** argv)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  return %s(argc, argv, %s);\n", main, module_name->str);
  fprintf(fout, "}\n");

  FbleFreeString(module_name);
}
