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

static void Loc(FILE* fout, FbleLoc loc);
static void Name(FILE* fout, FbleName name);
static VarId GenModulePath(FILE* fout, VarId* var_id, FbleModulePath* path);
static void EmitInstr(FILE* fout, VarId* var_id, size_t pc, FbleInstr* instr);
static void EmitInstrForAbort(FILE* fout, VarId* var_id, size_t pc, FbleInstr* instr);
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

// FrameSetBorrowed --
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

// Loc --
//   Output a C expression to produce an FbleLoc.
//
// Inputs:
//   fout - the output stream to write the C expression to.
//   loc - the FbleLoc to generate code for.
//
// Side effects:
// * Outputs code to fout for the loc.
static void Loc(FILE* fout, FbleLoc loc)
{
  // TODO: Properly escape funny chars in the source string.
  fprintf(fout, "FbleNewLoc(\"%s\", %i, %i)", loc.source->str, loc.line, loc.col);
}

// Name --
//   Generate an expression for constructing an FbleName.
//
// Inputs:
//   fout - the output stream to write the code to.
//   name - the FbleName to generate code for.
//
// Side effects:
// * Outputs code to fout for the expression.
static void Name(FILE* fout, FbleName name)
{
  static const char* spaces[] = {
    "FBLE_NORMAL_NAME_SPACE",
    "FBLE_TYPE_NAME_SPACE"
  };

  fprintf(fout, "Name(\"");
  for (char* p = name.name->str; *p; p++) {
    // TODO: Handle other special characters too.
    switch (*p) {
      case '\n': fprintf(fout, "\\n"); break;
      case '"': fprintf(fout, "\\\""); break;
      case '\\': fprintf(fout, "\\\\"); break;
      default: fprintf(fout, "%c", *p); break;
    }
  }

  fprintf(fout, "\", %s, \"%s\", %i, %i)", spaces[name.space], name.loc.source->str, name.loc.line, name.loc.col);
}

// GenModulePath --
//   Generate code for a FbleModulePath..
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   path - the FbleModulePath to generate code for.
//
// Results:
//   The variable id of the generated FbleModulePath.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static VarId GenModulePath(FILE* fout, VarId* var_id, FbleModulePath* path)
{
  VarId loc_id = (*var_id)++;
  VarId path_id = (*var_id)++;
  fprintf(fout, "  FbleLoc v%x = ", loc_id); Loc(fout, path->loc); fprintf(fout, ";\n");
  fprintf(fout, "  FbleModulePath* v%x = FbleNewModulePath(v%x);\n", path_id, loc_id);
  fprintf(fout, "  FbleFreeLoc(v%x);\n", loc_id);

  for (size_t i = 0; i < path->path.size; ++i) {
    fprintf(fout, "  FbleVectorAppend(v%x->path, ", path_id);
    Name(fout, path->path.xs[i]);
    fprintf(fout, ");\n");
  }
  return path_id;
}

// EmitInstr --
//   Generate code to execute an instruction.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   pc - the program counter of the instruction.
//   instr - the instruction to execute.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static void EmitInstr(FILE* fout, VarId* var_id, size_t pc, FbleInstr* instr)
{
  fprintf(fout, "      ProfileSample(profile);\n");

  for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
    switch (op->tag) {
      case FBLE_PROFILE_ENTER_OP:
        fprintf(fout, "      ProfileEnterBlock(profile, thread->stack->func->profile_base_id + %zi);\n", op->block);
        break;

      case FBLE_PROFILE_REPLACE_OP:
        fprintf(fout, "      ProfileReplaceBlock(profile, thread->stack->func->profile_base_id + %zi);\n", op->block);
        break;

      case FBLE_PROFILE_EXIT_OP:
        fprintf(fout, "      ProfileExitBlock(profile);\n");
        break;
    }
  }

  switch (instr->tag) {
    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
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

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      const char* section = union_instr->arg.section == FBLE_LOCALS_FRAME_SECTION ?  "Locals" : "Statics";
      fprintf(fout, "      UnionValueInstr%s(heap, thread, %zi, %zi, %zi);\n",
          section, union_instr->tag, union_instr->arg.index, union_instr->dest);
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "      FbleValue* sv = "); FrameGetStrict(fout, access_instr->obj); fprintf(fout, ";\n");
      fprintf(fout, "      if (sv == NULL) {\n");
      ReturnAbort(fout, "        ", pc, "UndefinedStructValue", access_instr->loc);
      fprintf(fout, "      };\n");
      fprintf(fout, "      StructAccess(heap, thread, sv, %zi, %zi);\n",
          access_instr->tag, access_instr->dest);
      return;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "      FbleUnionValue* uv = (FbleUnionValue*)"); FrameGetStrict(fout, access_instr->obj); fprintf(fout, ";\n");
      fprintf(fout, "      if (uv == NULL) {\n");
      ReturnAbort(fout, "        ", pc, "UndefinedUnionValue", access_instr->loc);
      fprintf(fout, "      };\n");

      fprintf(fout, "      assert(uv->_base.tag == FBLE_UNION_VALUE);\n");
      fprintf(fout, "      if (uv->tag != %zi) {;\n", access_instr->tag);
      ReturnAbort(fout, "        ", pc, "WrongUnionTag", access_instr->loc);
      fprintf(fout, "      };\n");

      FrameSetBorrowed(fout, "      ", access_instr->dest, "uv->arg");
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
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
      return;
    }

    case FBLE_JUMP_INSTR: {
      FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
      fprintf(fout, "      goto _pc_%zi;\n", pc + 1 + jump_instr->count);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;
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
      fprintf(fout, "      FbleFuncValue* v = FbleNewFuncValue(heap, &executable, thread->stack->func->profile_base_id);\n");
      for (size_t i = 0; i < staticc; ++i) {
        fprintf(fout, "      v->statics[%zi] = ", i); FrameGet(fout, func_instr->scope.xs[i]); fprintf(fout, ";\n");
        fprintf(fout, "      FbleValueAddRef(heap, &v->_base, v->statics[%zi]);\n", i);
      }
      FrameSetConsumed(fout, "      ", func_instr->dest, "&v->_base");
      return;
    };

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
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
          fprintf(fout, "      FbleReleaseValue(heap, thread->stack->locals[%zi]);\n", call_instr->func.index);
        }

        fprintf(fout, "      FbleThreadTailCall(heap, func, args, thread);\n");
        fprintf(fout, "      return FBLE_EXEC_FINISHED;\n");
        return;
      }

      fprintf(fout, "      thread->stack->pc = %zi;\n", pc+1);

      fprintf(fout, "      FbleValue** result = thread->stack->locals + %zi;\n", call_instr->dest);
      fprintf(fout, "      FbleThreadCall(heap, result, func, args, thread);\n");
      fprintf(fout, "      return FBLE_EXEC_FINISHED;\n");
      return;
    }

    case FBLE_LINK_INSTR: {
      FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
      fprintf(fout, "      FbleLinkValue* link = FbleNewValue(heap, FbleLinkValue);\n");
      fprintf(fout, "      link->_base.tag = FBLE_LINK_VALUE;\n");
      fprintf(fout, "      link->head = NULL;\n");
      fprintf(fout, "      link->tail = NULL;\n");

      fprintf(fout, "      FbleValue* get = FbleNewGetValue(heap, &link->_base, thread->stack->func->profile_base_id + %zi);\n", link_instr->profile);
      fprintf(fout, "      FbleValue* put = FbleNewPutValue(heap, &link->_base, thread->stack->func->profile_base_id + %zi);\n", link_instr->profile + 1);
      fprintf(fout, "      FbleReleaseValue(heap, &link->_base);\n");

      FrameSetConsumed(fout, "      ", link_instr->get, "get");
      FrameSetConsumed(fout, "      ", link_instr->put, "put");
      return;
    }

    case FBLE_FORK_INSTR: {
      FbleForkInstr* fork_instr = (FbleForkInstr*)instr;
      fprintf(fout, "      FbleProcValue* arg;\n");
      fprintf(fout, "      FbleThread* child;\n");
      fprintf(fout, "      FbleValue** result;\n");

      for (size_t i = 0; i < fork_instr->args.size; ++i) {
        fprintf(fout, "      arg = (FbleProcValue*)"); FrameGetStrict(fout, fork_instr->args.xs[i]); fprintf(fout, ";\n");
        fprintf(fout, "      assert(arg != NULL && \"undefined proc value\");");
        fprintf(fout, "      assert(arg->_base.tag == FBLE_PROC_VALUE);\n");

        fprintf(fout, "      child = FbleAlloc(FbleThread);\n");
        fprintf(fout, "      child->stack = thread->stack;\n");
        fprintf(fout, "      child->profile = profile == NULL ? NULL : FbleForkProfileThread(profile);\n");
        fprintf(fout, "      child->stack->joins++;\n");
        fprintf(fout, "      FbleVectorAppend(*threads, child);\n");

        fprintf(fout, "      result = thread->stack->locals + %zi;\n", fork_instr->dests.xs[i]);
        fprintf(fout, "      FbleThreadCall(heap, result, arg, NULL, child);\n");
      }

      fprintf(fout, "      thread->stack->pc = %zi;\n", pc+1);
      fprintf(fout, "      return FBLE_EXEC_YIELDED;\n");
      return;
    }

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      fprintf(fout, "      FbleValue* v = "); FrameGet(fout, copy_instr->source); fprintf(fout, ";\n");
      FrameSetBorrowed(fout, "      ", copy_instr->dest, "v");
      return;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      fprintf(fout, "      FbleRefValue* v = FbleNewValue(heap, FbleRefValue);\n");
      fprintf(fout, "      v->_base.tag = FBLE_REF_VALUE;\n");
      fprintf(fout, "      v->value = NULL;\n");

      FrameSetConsumed(fout, "      ", ref_instr->dest, "&v->_base");
      return;
    }

    case FBLE_REF_DEF_INSTR: {
      FbleRefDefInstr* ref_instr = (FbleRefDefInstr*)instr;
      fprintf(fout, "      FbleRefValue* rv = (FbleRefValue*)thread->stack->locals[%zi];\n", ref_instr->ref);
      fprintf(fout, "      assert(rv->_base.tag == FBLE_REF_VALUE);\n");
      fprintf(fout, "      assert(rv->value == NULL);\n");

      fprintf(fout, "      FbleValue* v = "); FrameGet(fout, ref_instr->value); fprintf(fout, ";\n");
      fprintf(fout, "      assert(v != NULL);\n");

      fprintf(fout, "      FbleRefValue* ref = (FbleRefValue*)v;\n");
      fprintf(fout, "      while (v->tag == FBLE_REF_VALUE && ref->value != NULL) {\n");
      fprintf(fout, "        v = ref->value;\n");
      fprintf(fout, "        ref = (FbleRefValue*)v;\n");
      fprintf(fout, "      }\n");

      fprintf(fout, "      if (ref == rv) {\n");
      ReturnAbort(fout, "        ", pc, "VacuousValue", ref_instr->loc);
      fprintf(fout, "      }\n");

      fprintf(fout, "      rv->value = v;\n");
      fprintf(fout, "      FbleValueAddRef(heap, &rv->_base, rv->value);\n");
      return;
    }

    case FBLE_RETURN_INSTR: {
      FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
      switch (return_instr->result.section) {
        case FBLE_STATICS_FRAME_SECTION: {
          fprintf(fout, "      FbleValue* result = thread->stack->func->statics[%zi];\n", return_instr->result.index);
          fprintf(fout, "      FbleRetainValue(heap, result);\n");
          fprintf(fout, "      FbleThreadReturn(heap, thread, result);\n");
          fprintf(fout, "      return FBLE_EXEC_FINISHED;\n");
          break;
        }

        case FBLE_LOCALS_FRAME_SECTION: {
          fprintf(fout, "      FbleValue* result = thread->stack->locals[%zi];\n", return_instr->result.index);
          fprintf(fout, "      FbleThreadReturn(heap, thread, result);\n");
          fprintf(fout, "      return FBLE_EXEC_FINISHED;\n");
          break;
        }
      }
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      fprintf(fout, "      FbleTypeValue* v = FbleNewValue(heap, FbleTypeValue);\n");
      fprintf(fout, "      v->_base.tag = FBLE_TYPE_VALUE;\n");
      FrameSetConsumed(fout, "      ", type_instr->dest, "&v->_base");
      return;
    }

    case FBLE_RELEASE_INSTR: {
      FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
      fprintf(fout, "      FbleReleaseValue(heap, thread->stack->locals[%zi]);\n", release_instr->target);
      return;
    }
  }
}

// EmitInstrForAbort --
//   Generate code to execute an instruction for abort.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   pc - the program counter of the instruction.
//   instr - the instruction to execute.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static void EmitInstrForAbort(FILE* fout, VarId* var_id, size_t pc, FbleInstr* instr)
{
  switch (instr->tag) {
    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      fprintf(fout, "      stack->locals[%zi] = NULL;\n", struct_instr->dest);
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      fprintf(fout, "      stack->locals[%zi] = NULL;\n", union_instr->dest);
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "      stack->locals[%zi] = NULL;\n", access_instr->dest);
      return;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "      stack->locals[%zi] = NULL;\n", access_instr->dest);
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
      fprintf(fout, "      goto _pc_%zi;\n", pc + 1 + select_instr->jumps.xs[0]);
      return;
    }

    case FBLE_JUMP_INSTR: {
      FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
      fprintf(fout, "      goto _pc_%zi;\n", pc + 1 + jump_instr->count);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;
      fprintf(fout, "      stack->locals[%zi] = NULL;\n", func_instr->dest);
      return;
    };

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
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
      return;
    }

    case FBLE_LINK_INSTR: {
      FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;

      fprintf(fout, "      stack->locals[%zi] = NULL;\n", link_instr->get);
      fprintf(fout, "      stack->locals[%zi] = NULL;\n", link_instr->put);
      return;
    }

    case FBLE_FORK_INSTR: {
      FbleForkInstr* fork_instr = (FbleForkInstr*)instr;
      for (size_t i = 0; i < fork_instr->args.size; ++i) {
        fprintf(fout, "      stack->locals[%zi] = NULL;\n", fork_instr->dests.xs[i]);
      }
      return;
    }

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      fprintf(fout, "      stack->locals[%zi] = NULL;\n", copy_instr->dest);
      return;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      fprintf(fout, "      stack->locals[%zi] = NULL;\n", ref_instr->dest);
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
          fprintf(fout, "      FbleReleaseValue(heap, stack->locals[%zi]);\n", return_instr->result.index);
          break;
        }
      }
      fprintf(fout, "      *(stack->result) = NULL;\n");
      fprintf(fout, "      return;\n");
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      fprintf(fout, "      stack->locals[%zi] = NULL;\n", type_instr->dest);
      return;
    }

    case FBLE_RELEASE_INSTR: {
      FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
      fprintf(fout, "      FbleReleaseValue(heap, stack->locals[%zi]);\n", release_instr->target);
      return;
    }
  }
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
bool FbleGenerateC(FILE* fout, FbleCompiledModule* module)
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
  fprintf(fout, "#define ProfileAutoExitBlock(profile) if (profile) { FbleProfileAutoExitBlock(profile); }\n");
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

  fprintf(fout, "static FbleName Name(const char* name, FbleNameSpace space, const char* source, int line, int col)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleName n;\n");
  fprintf(fout, "  n.name = FbleNewString(name);\n");
  fprintf(fout, "  n.space = space;\n");
  fprintf(fout, "  n.loc = FbleNewLoc(source, line, col);\n");
  fprintf(fout, "  return n;\n");
  fprintf(fout, "}\n\n");

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

    // RunFunction
    fprintf(fout, "static FbleExecStatus _Run_%p(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)\n", (void*)blocks.xs[i]);
    fprintf(fout, "{\n");
    VarId var_id = 0;

    fprintf(fout, "  register FbleProfileThread* profile = thread->profile;\n");

    // Output a jump table to jump into the right place in the function to
    // resume. We'll only ever resume into a place that's after a normal call
    // or fork instruction, so that's all we need to check for.
    fprintf(fout, "  switch (thread->stack->pc) {\n");
    fprintf(fout, "    case 0: goto _pc_0;\n");
    for (size_t i = 0; i < code->instrs.size; ++i) {
      FbleInstr* instr = code->instrs.xs[i];
      if (instr->tag == FBLE_CALL_INSTR) {
        FbleCallInstr* call = (FbleCallInstr*)instr;
        if (!call->exit) {
          fprintf(fout, "    case %zi: goto _pc_%zi;\n", i+1, i+1);
        }
      } else if (instr->tag == FBLE_FORK_INSTR) {
        fprintf(fout, "    case %zi: goto _pc_%zi;\n", i+1, i+1);
      }
    }
    fprintf(fout, "  };\n");

    // Output code to execute the individual instructions.
    for (size_t i = 0; i < code->instrs.size; ++i) {
      fprintf(fout, "    _pc_%zi: {\n", i);
      EmitInstr(fout, &var_id, i, code->instrs.xs[i]);
      fprintf(fout, "    }\n");
    }
    fprintf(fout, "  return Unreachable();\n");
    fprintf(fout, "}\n\n");

    // AbortFunction
    fprintf(fout, "static void _Abort_%p(FbleValueHeap* heap, FbleStack* stack)\n", (void*)blocks.xs[i]);
    fprintf(fout, "{\n");
    var_id = 0;

    fprintf(fout, "  switch (stack->pc) {\n");
    for (size_t i = 0; i < code->instrs.size; ++i) {
      fprintf(fout, "    case %zi:  _pc_%zi: {\n", i, i);
      EmitInstrForAbort(fout, &var_id, i, code->instrs.xs[i]);
      fprintf(fout, "    }\n");
    }
    fprintf(fout, "  }\n");
    fprintf(fout, "  Unreachable();\n");
    fprintf(fout, "}\n\n");
  }
  FbleFree(blocks.xs);

  FbleString* func_name = CIdentifierForPath(module->path);
  fprintf(fout, "void %s(FbleExecutableProgram* program)\n", func_name->str);
  FbleFreeString(func_name);

  fprintf(fout, "{\n");

  VarId var_id = 0;

  // Check if the module has already been loaded. If so, there's nothing to
  // do.
  VarId path_id = GenModulePath(fout, &var_id, module->path);
  fprintf(fout, "  for (size_t i = 0; i < program->modules.size; ++i) {\n");
  fprintf(fout, "    if (FbleModulePathsEqual(v%x, program->modules.xs[i].path)) {\n", path_id);
  fprintf(fout, "      FbleFreeModulePath(v%x);\n", path_id);
  fprintf(fout, "      return;\n");
  fprintf(fout, "    }\n");
  fprintf(fout, "  }\n\n");

  // Make sure all dependencies have been loaded.
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleString* dep_name = CIdentifierForPath(module->deps.xs[i]);
    fprintf(fout, "  %s(program);\n", dep_name->str);
    FbleFreeString(dep_name);
  }
  fprintf(fout, "\n");

  // Add this module to the program.
  VarId module_id = var_id++;
  fprintf(fout, "  FbleExecutableModule* v%x = FbleVectorExtend(program->modules);\n", module_id);
  fprintf(fout, "  v%x->path = v%x;\n", module_id, path_id);
  fprintf(fout, "  FbleVectorInit(v%x->deps);\n", module_id);

  for (size_t i = 0; i < module->deps.size; ++i) {
    VarId dep_id = GenModulePath(fout, &var_id, module->deps.xs[i]);
    fprintf(fout, "  FbleVectorAppend(v%x->deps, v%x);\n", module_id, dep_id);
  }

  fprintf(fout, "  v%x->executable = FbleAlloc(FbleExecutable);\n", module_id);
  fprintf(fout, "  v%x->executable->refcount = 1;\n", module_id);
  fprintf(fout, "  v%x->executable->magic = FBLE_EXECUTABLE_MAGIC;\n", module_id);
  fprintf(fout, "  v%x->executable->args = %zi;\n", module_id, module->code->_base.args);
  fprintf(fout, "  v%x->executable->statics = %zi;\n", module_id, module->code->_base.statics);
  fprintf(fout, "  v%x->executable->locals = %zi;\n", module_id, module->code->_base.locals);
  fprintf(fout, "  v%x->executable->profile = %zi;\n", module_id, module->code->_base.profile);
  fprintf(fout, "  FbleVectorInit(v%x->executable->profile_blocks);\n", module_id);
  fprintf(fout, "  v%x->executable->run = &_Run_%p;\n", module_id, (void*)module->code);
  fprintf(fout, "  v%x->executable->abort = &_Abort_%p;\n", module_id, (void*)module->code);
  fprintf(fout, "  v%x->executable->on_free = &FbleExecutableNothingOnFree;\n", module_id);

  for (size_t i = 0; i < module->code->_base.profile_blocks.size; ++i) {
    fprintf(fout, "  FbleVectorAppend(v%x->executable->profile_blocks, ", module_id);
    Name(fout, module->code->_base.profile_blocks.xs[i]);
    fprintf(fout, ");\n");
  }

  fprintf(fout, "}\n");

  return true;
}

// FbleGenerateCExport -- see documentation in fble-compile.h
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path)
{
  fprintf(fout, "#include \"fble-link.h\"\n");    // for FbleLink
  fprintf(fout, "#include \"fble-value.h\"\n");   // for FbleValue
  fprintf(fout, "#include \"fble-vector.h\"\n");  // for FbleVectorInit, etc.
  fprintf(fout, "\n");

  // Prototype for the exported module.
  FbleString* module_name = CIdentifierForPath(path);
  fprintf(fout, "void %s(FbleExecutableProgram* program);\n\n", module_name->str);

  fprintf(fout, "FbleValue* %s(FbleValueHeap* heap, FbleProfile* profile)\n", name);
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleExecutableProgram* program = FbleAlloc(FbleExecutableProgram);\n");
  fprintf(fout, "  FbleVectorInit(program->modules);\n");
  fprintf(fout, "  %s(program);\n", module_name->str);
  fprintf(fout, "  FbleValue* value = FbleLink(heap, program, profile);\n");
  fprintf(fout, "  FbleFreeExecutableProgram(program);\n");
  fprintf(fout, "  return value;\n");
  fprintf(fout, "}\n\n");

  FbleFreeString(module_name);
}
