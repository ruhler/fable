// generate_c.c --
//   This file describes code to generate c code for fble values.

#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdio.h>    // for sprintf
#include <string.h>   // for strlen, strcat

#include "fble-vector.h"    // for FbleVectorInit, etc.

#include "code.h"
#include "fble-compile.h"
#include "tc.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// VarId --
//   Type representing a C variable name as an integer.
//
// The number is turned into a C variable name using printf format "v%x".
typedef unsigned int VarId;

typedef struct {
  size_t size;
  const char** xs;
} LocV;

static void AddLoc(const char* source, LocV* locs);
static void CollectBlocksAndLocs(FbleCodeV* blocks, LocV* locs, FbleCode* code);

static void ReturnAbort(FILE* fout, const char* indent, size_t pc, const char* msg, FbleLoc loc);

static void FrameGet(FILE* fout, FbleFrameIndex index);
static void FrameGetStrict(FILE* fout, FbleFrameIndex index);
static void FrameSetBorrowed(FILE* fout, const char* indent, FbleLocalIndex index, const char* value);
static void FrameSetConsumed(FILE* fout, const char* indent, FbleLocalIndex index, const char* value);

static void StringLit(FILE* fout, const char* string);
static VarId StaticString(FILE* fout, VarId* var_id, const char* string);
static VarId StaticNames(FILE* fout, VarId* var_id, FbleNameV names);
static VarId StaticModulePath(FILE* fout, VarId* var_id, FbleModulePath* path);
static VarId StaticExecutableModule(FILE* fout, VarId* var_id, FbleCompiledModule* module);

static void EmitInstr(FILE* fout, VarId* var_id, size_t pc, FbleInstr* instr, bool abort);
static void EmitCode(FILE* fout, FbleCode* code, bool abort);
static int CIdentifierForLocSize(const char* str);
static void CIdentifierForLocStr(const char* str, char* dest);
static FbleString* CIdentifierForPath(FbleModulePath* path);

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

      case FBLE_LINK_INSTR: break;
      case FBLE_FORK_INSTR: break;
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
    }
  }
}

// ReturnAbort --
//   Emit code to return an error.
//
// Inputs:
//   fout - the output stream.
//   indent - indent to use at start of line.
//   pc - the program counter of the abort location.
//   msg - the name of a variable in scope containing the error message.
//   loc - the location to report with the error message.
//
// Side effects:
//   Emit code to return the error.
static void ReturnAbort(FILE* fout, const char* indent, size_t pc, const char* msg, FbleLoc loc)
{
  size_t size = CIdentifierForLocSize(loc.source->str);
  char source[size];
  CIdentifierForLocStr(loc.source->str, source);
  fprintf(fout, "%sreturn Abort(thread->stack, %zi, %s, %s, %i, %i);\n", indent, pc, msg, source, loc.line, loc.col);
}

// FrameGet --
//   Emit code to get a value from the stack frame.
//
// Inputs:
//   fout - the output stream.
//   index - the index of the value to get.
//
// Side effects:
//   Outputs to fout an atomic C expression that gets the value from the
//   frame.
static void FrameGet(FILE* fout, FbleFrameIndex index)
{
  fprintf(fout, "thread->stack->");
  switch (index.section) {
    case FBLE_STATICS_FRAME_SECTION: {
      fprintf(fout, "func->statics[%zi]", index.index);
      break;
    }

    case FBLE_LOCALS_FRAME_SECTION: {
      fprintf(fout, "locals[%zi]", index.index);
      break;
    }
  }
}

// FrameGetStrict --
//   Emit code to get a strict value from the stack frame.
//
// Inputs:
//   fout - the output stream.
//   index - the index of the value to get.
//
// Side effects:
//   Outputs to fout an atomic C expression that gets the dereferenced value
//   from the frame.
static void FrameGetStrict(FILE* fout, FbleFrameIndex index)
{
  fprintf(fout, "FbleStrictValue(");
  FrameGet(fout, index);
  fprintf(fout, ")");
}

// FrameSetBorrowed --
//   Emit code to set a borrowed value in the stack frame.
//
// Inputs:
//   fout - the output stream.
//   indent - indent to use at beginning of lines.
//   index - the index of the value to set.
//   value - an atomic C expression describing the value to set.
//
// Side effects:
//   Outputs to fout an atomic C expression that sets the value in the frame,
//   taking care of releasing any existing values as necessary.
static void FrameSetBorrowed(FILE* fout, const char* indent, FbleLocalIndex index, const char* value)
{
  fprintf(fout, "%sFbleRetainValue(heap, %s);\n", indent, value);
  fprintf(fout, "%sthread->stack->locals[%zi] = %s;\n", indent, index, value);
}

// FrameSetConsumed --
//   Emit code to set a consumed value in the stack frame.
//
// Inputs:
//   fout - the output stream.
//   indent - indent to use at beginning of lines.
//   index - the index of the value to set.
//   value - an atomic C expression describing the value to set.
//
// Side effects:
//   Outputs to fout an atomic C expression that sets the value in the frame,
//   taking care of releasing any existing values as necessary.
static void FrameSetConsumed(FILE* fout, const char* indent, FbleLocalIndex index, const char* value)
{
  fprintf(fout, "%sthread->stack->locals[%zi] = %s;\n", indent, index, value);
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
//   var_id - pointer to next available variable id for use.
//   string - the value of the string.
//
// Returns:
//   A variable name of a local, static FbleString.
//
// Side effects:
//   Writes code to fout and allocates variable ids out of var_id.
static VarId StaticString(FILE* fout, VarId* var_id, const char* string)
{
  VarId id = (*var_id)++;

  fprintf(fout, "  static FbleString v%x = {", id);
  fprintf(fout, " .refcount = 1,");
  fprintf(fout, " .magic = FBLE_STRING_MAGIC,");
  fprintf(fout, " .str = ");
  StringLit(fout, string);
  fprintf(fout, " };\n");
  return id;
}

// StaticNames --
//   Output code to declare a static FbleNameV.xs value.
//
// Inputs:
//   fout - the file to write to
//   var_id - pointer to next available variable id for use.
//   names - the value of the names.
//
// Returns:
//   A variable id of a local, static FbleNameV.xs.
//
// Side effects:
//   Writes code to fout and allocates variable ids out of var_id.
static VarId StaticNames(FILE* fout, VarId* var_id, FbleNameV names)
{
  static const char* spaces[] = {
    "FBLE_NORMAL_NAME_SPACE",
    "FBLE_TYPE_NAME_SPACE"
  };

  VarId id = (*var_id)++;
  for (size_t i = 0; i < names.size; ++i) {
    fprintf(fout, "  static FbleString v%x_str_%zi = {", id, i);
    fprintf(fout, " .refcount = 1,");
    fprintf(fout, " .magic = FBLE_STRING_MAGIC,");
    fprintf(fout, " .str = ");
    StringLit(fout, names.xs[i].name->str);
    fprintf(fout, " };\n");
  }

  for (size_t i = 0; i < names.size; ++i) {
    fprintf(fout, "  static FbleString v%x_src_%zi = {", id, i);
    fprintf(fout, " .refcount = 1,");
    fprintf(fout, " .magic = FBLE_STRING_MAGIC,");
    fprintf(fout, " .str = ");
    StringLit(fout, names.xs[i].loc.source->str);
    fprintf(fout, " };\n");
  }

  fprintf(fout, "  static FbleName v%x[%zi] = {\n", id, names.size);
  for (size_t i = 0; i < names.size; ++i) {
    fprintf(fout, "    { .name = &v%x_str_%zi,", id, i);
    fprintf(fout, " .space = %s,", spaces[names.xs[i].space]);
    fprintf(fout, " .loc = { .source = &v%x_src_%zi, .line = %i, .col = %i}},\n", id, i, names.xs[i].loc.line, names.xs[i].loc.col);
  }
  fprintf(fout, "  };\n");
  return id;
}

// StaticModulePath --
//   Generate code to declare a static FbleModulePath value.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   path - the FbleModulePath to generate code for.
//
// Results:
//   The variable id of a local, static FbleModulePath.
//
// Side effects:
// * Outputs code to fout.
// * Increments var_id based on the number of internal variables used.
static VarId StaticModulePath(FILE* fout, VarId* var_id, FbleModulePath* path)
{
  VarId src_id = StaticString(fout, var_id, path->loc.source->str);
  VarId names_id = StaticNames(fout, var_id, path->path);
  VarId path_id = (*var_id)++;

  fprintf(fout, "  static FbleModulePath v%x = {\n", path_id);
  fprintf(fout, "    .refcount = 1,\n");
  fprintf(fout, "    .magic = FBLE_MODULE_PATH_MAGIC,\n");
  fprintf(fout, "    .loc = { .source = &v%x, .line = %i, .col = %i },\n", src_id, path->loc.line, path->loc.col);
  fprintf(fout, "    .path = { .size = %zi, .xs = v%x },\n", path->path.size, names_id);
  fprintf(fout, "  };\n");
  return path_id;
}

// StaticExecutableModule --
//   Generate code to declare a static FbleExecutableModule value.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   module - the FbleCompiledModule to generate code for.
//
// Results:
//   The variable id of a local, static FbleExecutableModule.
//
// Side effects:
// * Outputs code to fout.
// * Increments var_id based on the number of internal variables used.
static VarId StaticExecutableModule(FILE* fout, VarId* var_id, FbleCompiledModule* module)
{
  VarId path_id = StaticModulePath(fout, var_id, module->path);

  VarId dep_ids[module->deps.size];
  for (size_t i = 0; i < module->deps.size; ++i) {
    dep_ids[i] = StaticModulePath(fout, var_id, module->deps.xs[i]);
  }

  VarId deps_xs_id = (*var_id)++;
  fprintf(fout, "  static FbleModulePath* v%x[] = {", deps_xs_id);
  for (size_t i = 0; i < module->deps.size; ++i) {
    fprintf(fout, " &v%x,", dep_ids[i]);
  }
  fprintf(fout, " };\n");

  VarId profile_blocks_xs_id = StaticNames(fout, var_id, module->code->_base.profile_blocks);

  VarId executable_id = (*var_id)++;
  fprintf(fout, "  static FbleExecutable v%x = {\n", executable_id);
  fprintf(fout, "    .refcount = 1,\n");
  fprintf(fout, "    .magic = FBLE_EXECUTABLE_MAGIC,\n");
  fprintf(fout, "    .args = %zi,\n", module->code->_base.args);
  fprintf(fout, "    .statics = %zi,\n", module->code->_base.statics);
  fprintf(fout, "    .locals = %zi,\n", module->code->_base.locals);
  fprintf(fout, "    .profile = %zi,\n", module->code->_base.profile);
  fprintf(fout, "    .profile_blocks = { .size = %zi, .xs = v%x },\n", module->code->_base.profile_blocks.size, profile_blocks_xs_id);
  fprintf(fout, "    .run = &_Run_%p,\n", (void*)module->code);
  fprintf(fout, "    .abort = &_Abort_%p,\n", (void*)module->code);
  fprintf(fout, "    .on_free = &FbleExecutableNothingOnFree,\n");
  fprintf(fout, "  };\n");

  VarId module_id = (*var_id)++;
  fprintf(fout, "  static FbleExecutableModule v%x = {\n", module_id);
  fprintf(fout, "    .refcount = 1,\n");
  fprintf(fout, "    .magic = FBLE_EXECUTABLE_MODULE_MAGIC,\n");
  fprintf(fout, "    .path = &v%x,\n", path_id);
  fprintf(fout, "    .deps = { .size = %zi, .xs = v%x },\n", module->deps.size, deps_xs_id);
  fprintf(fout, "    .executable = &v%x,\n", executable_id);
  fprintf(fout, "  };\n");

  return module_id;
}

// EmitInstr --
//   Generate code to execute an instruction.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   pc - the program counter of the instruction.
//   instr - the instruction to execute.
//   abort - if true, emit code to abort the instruction instead of execute it.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static void EmitInstr(FILE* fout, VarId* var_id, size_t pc, FbleInstr* instr, bool abort)
{
  if (!abort) {
    fprintf(fout, "      ProfileSample(profile);\n");
    for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
      switch (op->tag) {
        case FBLE_PROFILE_ENTER_OP:
          fprintf(fout, "      ProfileEnterBlock(profile, stack->func->profile_base_id + %zi);\n", op->block);
          break;

        case FBLE_PROFILE_REPLACE_OP:
          fprintf(fout, "      ProfileReplaceBlock(profile, stack->func->profile_base_id + %zi);\n", op->block);
          break;

        case FBLE_PROFILE_EXIT_OP:
          fprintf(fout, "      ProfileExitBlock(profile);\n");
          break;
      }
    }
  }

  switch (instr->tag) {
    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      if (abort) {
        FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
        fprintf(fout, "      stack->locals[%zi] = NULL;\n", struct_instr->dest);
        return;
      } else {
        size_t argc = struct_instr->args.size;

        if (argc == 0) {
          fprintf(fout, "      Struct0ValueInstr(heap, thread, %zi);\n", struct_instr->dest);
          return;
        }

        if (argc == 2
            && struct_instr->args.xs[0].section == FBLE_LOCALS_FRAME_SECTION
            && struct_instr->args.xs[1].section == FBLE_LOCALS_FRAME_SECTION) {
          fprintf(fout, "      Struct2LLValueInstr(heap, thread, %zi, %zi, %zi);\n",
              struct_instr->args.xs[0].index, struct_instr->args.xs[1].index,
              struct_instr->dest);
          return;
        }

        fprintf(fout, "      FbleValue* v = FbleNewStructValue(heap, %zi", argc);
        for (size_t i = 0; i < argc; ++i) {
          fprintf(fout, ", "); FrameGet(fout, struct_instr->args.xs[i]);
        }
        fprintf(fout, ");\n");
        FrameSetConsumed(fout, "      ", struct_instr->dest, "v");
        return;
      }
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      if (abort) {
        fprintf(fout, "      stack->locals[%zi] = NULL;\n", union_instr->dest);
      } else {
        const char* section = union_instr->arg.section == FBLE_LOCALS_FRAME_SECTION ?  "Locals" : "Statics";
        fprintf(fout, "      UnionValueInstr%s(heap, thread, %zi, %zi, %zi);\n",
            section, union_instr->tag, union_instr->arg.index, union_instr->dest);
      }
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      if (abort) {
        fprintf(fout, "      stack->locals[%zi] = NULL;\n", access_instr->dest);
      } else {
        fprintf(fout, "      FbleValue* sv = "); FrameGetStrict(fout, access_instr->obj); fprintf(fout, ";\n");
        fprintf(fout, "      if (sv == NULL) {\n");
        ReturnAbort(fout, "        ", pc, "UndefinedStructValue", access_instr->loc);
        fprintf(fout, "      };\n");
        fprintf(fout, "      StructAccess(heap, thread, sv, %zi, %zi);\n",
            access_instr->tag, access_instr->dest);
      }
      return;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      if (abort) {
        fprintf(fout, "      stack->locals[%zi] = NULL;\n", access_instr->dest);
      } else {
        fprintf(fout, "      FbleUnionValue* uv = (FbleUnionValue*)"); FrameGetStrict(fout, access_instr->obj); fprintf(fout, ";\n");
        fprintf(fout, "      if (uv == NULL) {\n");
        ReturnAbort(fout, "        ", pc, "UndefinedUnionValue", access_instr->loc);
        fprintf(fout, "      };\n");

        fprintf(fout, "      assert(uv->_base.tag == FBLE_UNION_VALUE);\n");
        fprintf(fout, "      if (uv->tag != %zi) {;\n", access_instr->tag);
        ReturnAbort(fout, "        ", pc, "WrongUnionTag", access_instr->loc);
        fprintf(fout, "      };\n");

        FrameSetBorrowed(fout, "      ", access_instr->dest, "uv->arg");
      }
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
      if (abort) {
        fprintf(fout, "      goto _pc_%zi;\n", pc + 1 + select_instr->jumps.xs[0]);
      } else {
        fprintf(fout, "      FbleUnionValue* v = (FbleUnionValue*)");
        FrameGetStrict(fout, select_instr->condition);
        fprintf(fout, ";\n");
        fprintf(fout, "      if (v == NULL) {\n");
        ReturnAbort(fout, "        ", pc, "UndefinedUnionSelect", select_instr->loc);
        fprintf(fout, "      };\n");

        fprintf(fout, "      assert(v->_base.tag == FBLE_UNION_VALUE);\n");
        fprintf(fout, "      switch (v->tag) {\n");
        for (size_t i = 0; i < select_instr->jumps.size; ++i) {
          fprintf(fout, "        case %zi: goto _pc_%zi;\n", i, pc + 1 + select_instr->jumps.xs[i]);
        }
        fprintf(fout, "      }\n");
      }
      return;
    }

    case FBLE_JUMP_INSTR: {
      FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
      fprintf(fout, "      goto _pc_%zi;\n", pc + 1 + jump_instr->count);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;
      if (abort) {
        fprintf(fout, "      stack->locals[%zi] = NULL;\n", func_instr->dest);
      } else {
        size_t staticc = func_instr->code->_base.statics;
        fprintf(fout, "      static FbleExecutable executable = {\n");
        fprintf(fout, "        .refcount = 1,\n");
        fprintf(fout, "        .magic = FBLE_EXECUTABLE_MAGIC,\n");
        fprintf(fout, "        .args = %zi,\n", func_instr->code->_base.args);
        fprintf(fout, "        .statics = %zi,\n", func_instr->code->_base.statics);
        fprintf(fout, "        .locals = %zi,\n", func_instr->code->_base.locals);
        fprintf(fout, "        .profile = %zi,\n", func_instr->code->_base.profile);
        fprintf(fout, "        .profile_blocks = { .size = 0, .xs = NULL },\n");
        fprintf(fout, "        .run = &_Run_%p,\n", (void*)func_instr->code);
        fprintf(fout, "        .abort = &_Abort_%p,\n", (void*)func_instr->code);
        fprintf(fout, "        .on_free = NULL,\n");
        fprintf(fout, "      };\n");
        fprintf(fout, "      FbleFuncValue* v = FbleNewFuncValue(heap, &executable, stack->func->profile_base_id);\n");
        for (size_t i = 0; i < staticc; ++i) {
          fprintf(fout, "      v->statics[%zi] = ", i); FrameGet(fout, func_instr->scope.xs[i]); fprintf(fout, ";\n");
          fprintf(fout, "      FbleValueAddRef(heap, &v->_base, v->statics[%zi]);\n", i);
        }
        FrameSetConsumed(fout, "      ", func_instr->dest, "&v->_base");
      }
      return;
    };

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      if (abort) {
        if (call_instr->exit) {
          if (call_instr->func.section == FBLE_LOCALS_FRAME_SECTION) {
            fprintf(fout, "      FbleReleaseValue(heap, stack->locals[%zi]);\n", call_instr->func.index);
            fprintf(fout, "      stack->locals[%zi] = NULL;\n", call_instr->func.index);
          }

          for (size_t i = 0; i < call_instr->args.size; ++i) {
            if (call_instr->args.xs[i].section == FBLE_LOCALS_FRAME_SECTION) {
              fprintf(fout, "      FbleReleaseValue(heap, stack->locals[%zi]);\n", call_instr->args.xs[i].index);
              fprintf(fout, "      stack->locals[%zi] = NULL;\n", call_instr->args.xs[i].index);
            }
          }

          fprintf(fout, "      *(stack->result) = NULL;\n");
          fprintf(fout, "      return;\n");
          return;
        }

        fprintf(fout, "      stack->locals[%zi] = NULL;\n", call_instr->dest);
      } else {
        fprintf(fout, "      FbleFuncValue* func = (FbleFuncValue*)"); FrameGetStrict(fout, call_instr->func); fprintf(fout, ";\n");
        fprintf(fout, "      if (func == NULL) {\n");
        ReturnAbort(fout, "        ", pc, "UndefinedFunctionValue", call_instr->loc);
        fprintf(fout, "      }\n");

        fprintf(fout, "      assert(func->_base.tag == FBLE_FUNC_VALUE);\n");
        fprintf(fout, "      FbleValue* args[%zi];\n", call_instr->args.size == 0 ? 1 : call_instr->args.size);
        for (size_t i = 0; i < call_instr->args.size; ++i) {
          fprintf(fout, "      args[%zi] = ", i); FrameGet(fout, call_instr->args.xs[i]); fprintf(fout, ";\n");
        }

        if (call_instr->exit) {
          fprintf(fout, "      FbleRetainValue(heap, &func->_base);\n");

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
              fprintf(fout, "      FbleRetainValue(heap, args[%zi]);\n", i);
            }
          }

          if (call_instr->func.section == FBLE_LOCALS_FRAME_SECTION) {
            fprintf(fout, "      FbleReleaseValue(heap, stack->locals[%zi]);\n", call_instr->func.index);
          }

          fprintf(fout, "      FbleThreadTailCall(heap, func, args, thread);\n");
          fprintf(fout, "      return FBLE_EXEC_FINISHED;\n");
          return;
        }

        fprintf(fout, "      stack->pc = %zi;\n", pc+1);

        fprintf(fout, "      FbleValue** result = stack->locals + %zi;\n", call_instr->dest);
        fprintf(fout, "      FbleThreadCall(heap, result, func, args, thread);\n");
        fprintf(fout, "      return FBLE_EXEC_FINISHED;\n");
      }
      return;
    }

    case FBLE_LINK_INSTR: {
      FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
      if (abort) {
        fprintf(fout, "      stack->locals[%zi] = NULL;\n", link_instr->get);
        fprintf(fout, "      stack->locals[%zi] = NULL;\n", link_instr->put);
      } else {
        fprintf(fout, "      FbleLinkValue* link = FbleNewValue(heap, FbleLinkValue);\n");
        fprintf(fout, "      link->_base.tag = FBLE_LINK_VALUE;\n");
        fprintf(fout, "      link->head = NULL;\n");
        fprintf(fout, "      link->tail = NULL;\n");

        fprintf(fout, "      FbleValue* get = FbleNewGetValue(heap, &link->_base, stack->func->profile_base_id + %zi);\n", link_instr->profile);
        fprintf(fout, "      FbleValue* put = FbleNewPutValue(heap, &link->_base, stack->func->profile_base_id + %zi);\n", link_instr->profile + 1);
        fprintf(fout, "      FbleReleaseValue(heap, &link->_base);\n");

        FrameSetConsumed(fout, "      ", link_instr->get, "get");
        FrameSetConsumed(fout, "      ", link_instr->put, "put");
      }
      return;
    }

    case FBLE_FORK_INSTR: {
      FbleForkInstr* fork_instr = (FbleForkInstr*)instr;
      if (abort) {
        for (size_t i = 0; i < fork_instr->args.size; ++i) {
          fprintf(fout, "      stack->locals[%zi] = NULL;\n", fork_instr->dests.xs[i]);
        }
      } else {
        fprintf(fout, "      FbleProcValue* arg;\n");
        fprintf(fout, "      FbleValue** result;\n");

        for (size_t i = 0; i < fork_instr->args.size; ++i) {
          fprintf(fout, "      arg = (FbleProcValue*)"); FrameGetStrict(fout, fork_instr->args.xs[i]); fprintf(fout, ";\n");
          fprintf(fout, "      assert(arg != NULL && \"undefined proc value\");");
          fprintf(fout, "      assert(arg->_base.tag == FBLE_PROC_VALUE);\n");

          fprintf(fout, "      result = stack->locals + %zi;\n", fork_instr->dests.xs[i]);

          fprintf(fout, "      FbleThreadFork(heap, threads, thread, result, arg, NULL);\n");
        }

        fprintf(fout, "      stack->pc = %zi;\n", pc+1);
        fprintf(fout, "      return FBLE_EXEC_YIELDED;\n");
      }
      return;
    }

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      if (abort) {
        fprintf(fout, "      stack->locals[%zi] = NULL;\n", copy_instr->dest);
      } else {
        fprintf(fout, "      FbleValue* v = "); FrameGet(fout, copy_instr->source); fprintf(fout, ";\n");
        FrameSetBorrowed(fout, "      ", copy_instr->dest, "v");
      }
      return;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      if (abort) {
        fprintf(fout, "      stack->locals[%zi] = NULL;\n", ref_instr->dest);
      } else {
        fprintf(fout, "      FbleRefValue* v = FbleNewValue(heap, FbleRefValue);\n");
        fprintf(fout, "      v->_base.tag = FBLE_REF_VALUE;\n");
        fprintf(fout, "      v->value = NULL;\n");

        FrameSetConsumed(fout, "      ", ref_instr->dest, "&v->_base");
      }
      return;
    }

    case FBLE_REF_DEF_INSTR: {
      if (!abort) {
        FbleRefDefInstr* ref_instr = (FbleRefDefInstr*)instr;
        fprintf(fout, "      FbleRefValue* rv = (FbleRefValue*)stack->locals[%zi];\n", ref_instr->ref);
        fprintf(fout, "      assert(rv->_base.tag == FBLE_REF_VALUE);\n");
        fprintf(fout, "      assert(rv->value == NULL);\n");

        fprintf(fout, "      FbleValue* v = "); FrameGet(fout, ref_instr->value); fprintf(fout, ";\n");
        fprintf(fout, "      assert(v != NULL);\n");

        fprintf(fout, "      FbleRefValue* ref = (FbleRefValue*)FbleStrictRefValue(v);\n");
        fprintf(fout, "      if (ref == rv) {\n");
        ReturnAbort(fout, "        ", pc, "VacuousValue", ref_instr->loc);
        fprintf(fout, "      }\n");

        fprintf(fout, "      rv->value = v;\n");
        fprintf(fout, "      FbleValueAddRef(heap, &rv->_base, rv->value);\n");
      }
      return;
    }

    case FBLE_RETURN_INSTR: {
      FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
      if (abort) {
        switch (return_instr->result.section) {
          case FBLE_STATICS_FRAME_SECTION: break;
          case FBLE_LOCALS_FRAME_SECTION: {
            fprintf(fout, "      FbleReleaseValue(heap, stack->locals[%zi]);\n", return_instr->result.index);
            break;
          }
        }
        fprintf(fout, "      *(stack->result) = NULL;\n");
        fprintf(fout, "      return;\n");
      } else {
        switch (return_instr->result.section) {
          case FBLE_STATICS_FRAME_SECTION: {
            fprintf(fout, "      FbleValue* result = stack->func->statics[%zi];\n", return_instr->result.index);
            fprintf(fout, "      FbleRetainValue(heap, result);\n");
            fprintf(fout, "      FbleThreadReturn(heap, thread, result);\n");
            fprintf(fout, "      return FBLE_EXEC_FINISHED;\n");
            break;
          }

          case FBLE_LOCALS_FRAME_SECTION: {
            fprintf(fout, "      FbleValue* result = stack->locals[%zi];\n", return_instr->result.index);
            fprintf(fout, "      FbleThreadReturn(heap, thread, result);\n");
            fprintf(fout, "      return FBLE_EXEC_FINISHED;\n");
            break;
          }
        }
      }
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      if (abort) {
        fprintf(fout, "      stack->locals[%zi] = NULL;\n", type_instr->dest);
      } else {
        fprintf(fout, "      FbleTypeValue* v = FbleNewValue(heap, FbleTypeValue);\n");
        fprintf(fout, "      v->_base.tag = FBLE_TYPE_VALUE;\n");
        FrameSetConsumed(fout, "      ", type_instr->dest, "&v->_base");
      }
      return;
    }

    case FBLE_RELEASE_INSTR: {
      FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
      fprintf(fout, "      FbleReleaseValue(heap, stack->locals[%zi]);\n", release_instr->target);
      return;
    }
  }
}

// EmitCode --
//   Generate code to execute an FbleCode block.
//
// Inputs:
//   fout - the output stream to write the code to.
//   code - the block of code to generate a C function for.
//   abort - if true, emit code to abort the block instead of execute it.
//
// Side effects:
// * Outputs code to fout with two space indent.
static void EmitCode(FILE* fout, FbleCode* code, bool abort)
{
  if (abort) {
    fprintf(fout, "static void _Abort_%p(FbleValueHeap* heap, FbleStack* stack)\n", (void*)code);
    fprintf(fout, "{\n");
  } else {
    fprintf(fout, "static FbleExecStatus _Run_%p(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)\n", (void*)code);
    fprintf(fout, "{\n");
    fprintf(fout, "  FbleProfileThread* profile = thread->profile;\n");
    fprintf(fout, "  FbleStack* stack = thread->stack;\n");
  }

  VarId var_id = 0;

  fprintf(fout, "  switch (stack->pc) {\n");
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, "    case %zi:  _pc_%zi: {\n", i, i);
    EmitInstr(fout, &var_id, i, code->instrs.xs[i], abort);
    fprintf(fout, "    }\n");
  }
  fprintf(fout, "  }\n");
  fprintf(fout, "  Unreachable();\n");
  fprintf(fout, "}\n\n");
}

// CIdentifierForLocSize --
//   Returns the size of a buffer needed to hold the c identifier for a
//   location source file name string.
//
// Inputs:
//   str - the location source file name string.
//
// Results:
//   The buffer size needed to hold the identifier.
//
// Side effects:
//   None.
static int CIdentifierForLocSize(const char* str)
{
  // The conversion from string to identifier works as followed:
  // * We add _loc_ as a prefix.
  // * Characters [0-9], [a-z], [A-Z] are kept as is.
  // * Other characters are translated to _XX_, where XX is the 2 digit hex
  //   representation of the ascii value of the character.

  // Determine the length of the name.
  size_t len = strlen("_loc_") + 1; // prefix and terminating '\0'.
  for (const char* p = str; *p != '\0'; p++) {
    if (isalnum(*p)) {
      len++;        // untranslated character
    } else {
      len += 4;     // translated character
    }
  }
  return len;
}

// CIdentifierForLocStr --
//   Fill in a buffer to hold the c identifier for a location source file name
//   string.
//
// Inputs:
//   str - the location source file name string.
//   dest - the buffer to fill in. Should be CIdentifierForLocSize(str) in
//          length.
//
// Side effects:
//   Sets dest to the c identifier to use for the location source file name.
static void CIdentifierForLocStr(const char* str, char* dest)
{
  char translated[5]; 
  dest[0] = '\0';
  strcat(dest, "_loc_");
  for (const char* p = str; *p != '\0'; p++) {
    if (isalnum(*p)) {
      sprintf(translated, "%c", *p);
      strcat(dest, translated);
    } else {
      sprintf(translated, "_%02x_", *p);
      strcat(dest, translated);
    }
  }
}

// CIdentifierForPath --
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
static FbleString* CIdentifierForPath(FbleModulePath* path)
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
      if (isalnum(*p)) {
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
      if (isalnum(*p)) {
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

  fprintf(fout, "#include \"assert.h\"\n\n");    // for assert

  fprintf(fout, "#include \"fble-alloc.h\"\n");  // for FbleAlloc, etc.
  fprintf(fout, "#include \"fble-name.h\"\n");   // for FbleName, etc.
  fprintf(fout, "#include \"fble-value.h\"\n");  // for FbleValue, etc.
  fprintf(fout, "#include \"fble-vector.h\"\n"); // for FbleVectorInit, etc.
  fprintf(fout, "#include \"execute.h\"\n");     // for FbleThread, etc.
  fprintf(fout, "#include \"value.h\"\n");       // for FbleStructValue, etc.
  fprintf(fout, "\n");

  fprintf(fout, "#include \"stdlib.h\"\n\n");    // for rand
  fprintf(fout, "#define ProfileEnterBlock(profile, block) if (profile) { FbleProfileEnterBlock(profile, block); }\n");
  fprintf(fout, "#define ProfileReplaceBlock(profile, block) if (profile) { FbleProfileReplaceBlock(profile, block); }\n");
  fprintf(fout, "#define ProfileExitBlock(profile) if (profile) { FbleProfileExitBlock(profile); }\n");
  fprintf(fout, "#define ProfileSample(profile) if (profile && rand() %% 1024 == 0) { FbleProfileSample(profile, 1); }\n");

  // Prototypes for module dependencies.
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleString* dep_name = CIdentifierForPath(module->deps.xs[i]);
    fprintf(fout, "void %s(FbleExecutableProgram* program);\n", dep_name->str);
    FbleFreeString(dep_name);
  }
  fprintf(fout, "\n");

  // Prototypes for FbleRunFunction functions.
  for (size_t i = 0; i < blocks.size; ++i) {
    fprintf(fout, "static FbleExecStatus _Run_%p(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);\n", (void*)blocks.xs[i]);
    fprintf(fout, "static void _Abort_%p(FbleValueHeap* heap, FbleStack* stack);\n", (void*)blocks.xs[i]);
  }
  fprintf(fout, "\n");

  // Definitions of source code locations.
  for (size_t i = 0; i < locs.size; ++i) {
    size_t size = CIdentifierForLocSize(locs.xs[i]);
    char source[size];
    CIdentifierForLocStr(locs.xs[i], source);
    fprintf(fout, "static const char* %s = \"%s\";\n", source, locs.xs[i]);
  }
  fprintf(fout, "\n");
  FbleFree(locs.xs);

  // Helper functions to reduce generated code size so it is easier for the
  // compiler to deal with.
  fprintf(fout, "static const char* UndefinedStructValue = \"undefined struct value access\\n\";\n");
  fprintf(fout, "static const char* UndefinedUnionValue = \"undefined union value access\\n\";\n");
  fprintf(fout, "static const char* UndefinedUnionSelect = \"undefined union value select\\n\";\n");
  fprintf(fout, "static const char* WrongUnionTag = \"union field access undefined: wrong tag\\n\";\n");
  fprintf(fout, "static const char* UndefinedFunctionValue = \"called undefined function\\n\";\n");
  fprintf(fout, "static const char* VacuousValue = \"vacuous value\\n\";\n\n");

  fprintf(fout, "static FbleExecStatus Abort(FbleStack* stack, size_t pc, const char* msg, const char* source, size_t line, size_t col)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  stack->pc = pc;\n");
  fprintf(fout, "  fprintf(stderr, \"%%s:%%d:%%d: error: %%s\", source, line, col, msg);\n");
  fprintf(fout, "  return FBLE_EXEC_ABORTED;\n");
  fprintf(fout, "}\n\n");

  fprintf(fout, "static FbleExecStatus Unreachable()\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  assert(false && \"should never get here.\");\n");
  fprintf(fout, "  return FBLE_EXEC_ABORTED;\n");
  fprintf(fout, "}\n\n");

  fprintf(fout, "static void Struct0ValueInstr(FbleValueHeap* heap, FbleThread* thread, size_t dest)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleValue* v = FbleNewStructValue(heap, 0);\n");
  fprintf(fout, "  thread->stack->locals[dest] = v;\n");
  fprintf(fout, "}\n\n");

  fprintf(fout, "static void Struct2LLValueInstr(FbleValueHeap* heap, FbleThread* thread, size_t arg0, size_t arg1, size_t dest)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleValue* v = FbleNewStructValue(heap, 2, thread->stack->locals[arg0], thread->stack->locals[arg1]);\n");
  fprintf(fout, "  thread->stack->locals[dest] = v;\n");
  fprintf(fout, "}\n\n");

  fprintf(fout, "static void UnionValueInstrStatics(FbleValueHeap* heap, FbleThread* thread, size_t tag, size_t arg, size_t dest)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleValue* v = FbleNewUnionValue(heap, tag, thread->stack->func->statics[arg]);\n");
  fprintf(fout, "  thread->stack->locals[dest] = v;\n");
  fprintf(fout, "}\n\n");

  fprintf(fout, "static void UnionValueInstrLocals(FbleValueHeap* heap, FbleThread* thread, size_t tag, size_t arg, size_t dest)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleValue* v = FbleNewUnionValue(heap, tag, thread->stack->locals[arg]);\n");
  fprintf(fout, "  thread->stack->locals[dest] = v;\n");
  fprintf(fout, "}\n\n");

  fprintf(fout, "static void StructAccess(FbleValueHeap* heap, FbleThread* thread, FbleValue* sv, size_t tag, size_t dest)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleValue* value = FbleStructValueAccess(sv, tag);\n");
  fprintf(fout, "  FbleRetainValue(heap, value);\n");
  fprintf(fout, "  thread->stack->locals[dest] = value;\n");
  fprintf(fout, "}\n\n");

  for (int i = 0; i < blocks.size; ++i) {
    FbleCode* code = blocks.xs[i];
    EmitCode(fout, code, false); // RunFunction
    EmitCode(fout, code, true); // AbortFunction
  }
  FbleFree(blocks.xs);

  FbleString* func_name = CIdentifierForPath(module->path);
  fprintf(fout, "void %s(FbleExecutableProgram* program)\n", func_name->str);
  FbleFreeString(func_name);

  fprintf(fout, "{\n");

  VarId var_id = 0;

  fprintf(fout, "  static size_t depc = %zi;\n", module->deps.size);
  fprintf(fout, "  static FbleCompiledModuleFunction* deps[%zi] = {\n", module->deps.size);
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleString* dep_name = CIdentifierForPath(module->deps.xs[i]);
    fprintf(fout, "    &%s,\n", dep_name->str);
    FbleFreeString(dep_name);
  }
  fprintf(fout, "  };\n");

  VarId module_id = StaticExecutableModule(fout, &var_id, module);
  fprintf(fout, "  FbleLoadFromCompiled(program, &v%x, depc, deps);\n", module_id);
  fprintf(fout, "}\n");
}

// FbleGenerateCExport -- see documentation in fble-compile.h
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path)
{
  fprintf(fout, "#include \"fble-execute.h\"\n");    // for FbleExecutableProgram
  fprintf(fout, "\n");

  FbleString* module_name = CIdentifierForPath(path);
  fprintf(fout, "void %s(FbleExecutableProgram* program);\n\n", module_name->str);
  fprintf(fout, "void %s(FbleExecutableProgram* program)\n", name);
  fprintf(fout, "{\n");
  fprintf(fout, "  %s(program);\n", module_name->str);
  fprintf(fout, "}\n\n");
  FbleFreeString(module_name);
}
