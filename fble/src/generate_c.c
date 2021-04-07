// generate_c.c --
//   This file describes code to generate c code for fble values.

#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdio.h>    // for sprintf
#include <string.h>   // for strlen, strcat

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

static void CollectBlocks(FbleArena* arena, FbleCodeV* blocks, FbleCode* code);

static void FrameGet(FILE* fout, FbleFrameIndex index);
static void FrameGetStrict(FILE* fout, FbleFrameIndex index);
static void FrameSetBorrowed(FILE* fout, const char* indent, FbleLocalIndex index, const char* value);
static void FrameSetConsumed(FILE* fout, const char* indent, FbleLocalIndex index, const char* value);

static VarId GenLoc(FILE* fout, const char* indent, VarId* var_id, FbleLoc loc);
static VarId GenName(FILE* fout, VarId* var_id, FbleName name);
static VarId GenModulePath(FILE* fout, VarId* var_id, FbleModulePath* path);
static void EmitInstr(FILE* fout, VarId* var_id, size_t pc, FbleInstr* instr);
static FbleString* FuncNameForPath(FbleArena* arena, FbleModulePath* path);

// CollectBlocks --
//   Get the list of all instruction blocks referenced from the given block of
//   code, including the code itself.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the collection of blocks to add to.
//   code - the code to collect the blocks from.
static void CollectBlocks(FbleArena* arena, FbleCodeV* blocks, FbleCode* code)
{
  FbleVectorAppend(arena, *blocks, code);
  for (size_t i = 0; i < code->instrs.size; ++i) {
    if (code->instrs.xs[i]->tag == FBLE_FUNC_VALUE_INSTR) {
      FbleFuncValueInstr* instr = (FbleFuncValueInstr*)code->instrs.xs[i];
      CollectBlocks(arena, blocks, instr->code);
    }
  }
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
      fprintf(fout, "func->statics[%i]", index.index);
      break;
    }

    case FBLE_LOCALS_FRAME_SECTION: {
      fprintf(fout, "locals.xs[%i]", index.index);
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
  fprintf(fout, "%sFbleReleaseValue(heap, thread->stack->locals.xs[%i]);\n", indent, index);
  fprintf(fout, "%sthread->stack->locals.xs[%i] = %s;\n", indent, index, value);
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
  fprintf(fout, "%sFbleReleaseValue(heap, thread->stack->locals.xs[%i]);\n", indent, index);
  fprintf(fout, "%sthread->stack->locals.xs[%i] = %s;\n", indent, index, value);
}

// GenLoc --
//   Generate code for a FbleLoc.
//
// Assumes there is a C variable 'FbleArena* arena' in scope for allocating
// FbleLoc.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   loc - the FbleLoc to generate code for.
//
// Results:
//   The variable id of the generated FbleLoc.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static VarId GenLoc(FILE* fout, const char* indent, VarId* var_id, FbleLoc loc)
{
  VarId source = (*var_id)++;
  fprintf(fout, "%sFbleString* v%x = FbleNewString(arena, \"%s\");\n",
      indent, source, loc.source->str);

  VarId id = (*var_id)++;
  fprintf(fout, "%sFbleLoc v%x = { .source = v%x, .line = %i, .col = %i };\n",
      indent, id, source, loc.line, loc.col);
  return id;
}

// GenName --
//   Generate code for a FbleName.
//
// Assumes there is a C variable 'FbleArena* arena' in scope for allocating
// FbleName.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   name - the FbleName to generate code for.
//
// Results:
//   The variable id of the generated FbleName.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static VarId GenName(FILE* fout, VarId* var_id, FbleName name)
{
  static const char* spaces[] = {
    "FBLE_NORMAL_NAME_SPACE",
    "FBLE_TYPE_NAME_SPACE"
  };

  VarId loc_id = GenLoc(fout, "  ", var_id, name.loc);

  VarId id = (*var_id)++;
  fprintf(fout, "  FbleName v%x;\n", id);
  // TODO: Handle funny chars in the string literal properly.
  fprintf(fout, "  v%x.name = FbleNewString(arena, \"%s\");\n", id, name.name->str);
  fprintf(fout, "  v%x.space = %s;\n", id, spaces[name.space]);
  fprintf(fout, "  v%x.loc = v%x;\n", id, loc_id);
  return id;
}

// GenModulePath --
//   Generate code for a FbleModulePath..
//
// Assumes there is a C variable 'FbleArena* arena' in scope for allocating
// the FbleModulePath.
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
  VarId loc_id = GenLoc(fout, "  ", var_id, path->loc);
  VarId path_id = (*var_id)++;
  fprintf(fout, "  FbleModulePath* v%x = FbleNewModulePath(arena, v%x);\n", path_id, loc_id);
  fprintf(fout, "  FbleFreeLoc(arena, v%x);\n", loc_id);

  for (size_t i = 0; i < path->path.size; ++i) {
    VarId name_id = GenName(fout, var_id, path->path.xs[i]);
    fprintf(fout, "  FbleVectorAppend(arena, v%x->path, v%x);\n", path_id, name_id);
  }
  return path_id;
}

// EmitInstr --
//   Generate code to append an FbleInstr to a vector of instructions.
//
// Assumes there is a C variable 'FbleArena* arena' in scope for allocating
// FbleInstr.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   block_id - id of the variable for the FbleCode to add the
//              instruction to.
//   instr - the FbleInstr to add to the block.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static void EmitInstr(FILE* fout, VarId* var_id, size_t pc, FbleInstr* instr)
{
  switch (instr->tag) {
    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      size_t argc = struct_instr->args.size;

      fprintf(fout, "      FbleStructValue* v = FbleNewValueExtra(heap, FbleStructValue, sizeof(FbleValue*) * %i);\n", argc);
      fprintf(fout, "      v->_base.tag = FBLE_STRUCT_VALUE;\n");
      fprintf(fout, "      v->fieldc = %i;\n", argc);
      for (size_t i = 0; i < argc; ++i) {
        fprintf(fout, "      v->fields[%i] = ", i); FrameGet(fout, struct_instr->args.xs[i]); fprintf(fout, ";\n");
        fprintf(fout, "      FbleValueAddRef(heap, &v->_base, v->fields[%i]);\n", i);
      }

      FrameSetConsumed(fout, "      ", struct_instr->dest, "&v->_base");
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      fprintf(fout, "      FbleUnionValue* v = FbleNewValue(heap, FbleUnionValue);\n");
      fprintf(fout, "      v->_base.tag = FBLE_UNION_VALUE;\n");
      fprintf(fout, "      v->tag = %i;\n", union_instr->tag);
      fprintf(fout, "      v->arg = "); FrameGet(fout, union_instr->arg); fprintf(fout, ";\n");
      fprintf(fout, "      FbleValueAddRef(heap, &v->_base, v->arg);\n");
      FrameSetConsumed(fout, "      ", union_instr->dest, "&v->_base");
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "      FbleStructValue* sv = (FbleStructValue*)"); FrameGetStrict(fout, access_instr->obj); fprintf(fout, ";\n");
      fprintf(fout, "      if (sv == NULL) {\n");
      VarId loc_id = GenLoc(fout, "        ", var_id, access_instr->loc);
      fprintf(fout, "        FbleReportError(\"undefined struct value access\\n\", v%x);\n", loc_id);
      fprintf(fout, "        FbleFreeLoc(heap->arena, v%x);\n", loc_id);
      fprintf(fout, "        return FBLE_EXEC_ABORTED;\n");
      fprintf(fout, "      };\n");

      fprintf(fout, "      assert(sv->_base.tag == FBLE_STRUCT_VALUE);\n");
      fprintf(fout, "      assert(%i < sv->fieldc);\n", access_instr->tag);
      fprintf(fout, "      FbleValue* value = sv->fields[%i];\n", access_instr->tag);
      FrameSetBorrowed(fout, "      ", access_instr->dest, "value");
      return;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "      FbleUnionValue* uv = (FbleUnionValue*)"); FrameGetStrict(fout, access_instr->obj); fprintf(fout, ";\n");
      fprintf(fout, "      if (uv == NULL) {\n");
      VarId loc_id = GenLoc(fout, "      ", var_id, access_instr->loc);
      fprintf(fout, "        FbleReportError(\"undefined union value access\\n\", v%x);\n", loc_id);
      fprintf(fout, "        FbleFreeLoc(heap->arena, v%x);\n", loc_id);
      fprintf(fout, "        return FBLE_EXEC_ABORTED;\n");
      fprintf(fout, "      };\n");

      fprintf(fout, "      assert(uv->_base.tag == FBLE_UNION_VALUE);\n");
      fprintf(fout, "      if (uv->tag != %i) {;\n", access_instr->tag);
      loc_id = GenLoc(fout, "      ", var_id, access_instr->loc);
      fprintf(fout, "        FbleReportError(\"union field access undefined: wrong tag\\n\", v%x);\n", loc_id);
      fprintf(fout, "        FbleFreeLoc(heap->arena, v%x);\n", loc_id);
      fprintf(fout, "        return FBLE_EXEC_ABORTED;\n");
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
      VarId loc_id = GenLoc(fout, "      ", var_id, select_instr->loc);
      fprintf(fout, "        FbleReportError(\"undefined union value select\\n\", v%x);\n", loc_id);
      fprintf(fout, "        FbleFreeLoc(heap->arena, v%x);\n", loc_id);
      fprintf(fout, "        return FBLE_EXEC_ABORTED;\n");
      fprintf(fout, "      };\n");

      fprintf(fout, "      assert(v->_base.tag == FBLE_UNION_VALUE);\n");
      fprintf(fout, "      switch (v->tag) {\n");
      for (size_t i = 0; i < select_instr->jumps.size; ++i) {
        fprintf(fout, "        case %i: goto _pc_%i;\n", i, pc + 1 + select_instr->jumps.xs[i]);
      }
      fprintf(fout, "      }\n");
      return;
    }

    case FBLE_JUMP_INSTR: {
      FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
      fprintf(fout, "      goto _pc_%i;\n", pc + 1 + jump_instr->count);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;
      size_t staticc = func_instr->code->statics;
      fprintf(fout, "      FbleFuncValue* v = FbleNewValueExtra(heap, FbleFuncValue, sizeof(FbleValue*) * %i);\n", staticc);
      fprintf(fout, "      v->_base.tag = FBLE_FUNC_VALUE;\n");
      fprintf(fout, "      v->executable = FbleAlloc(heap->arena, FbleExecutable);\n");
      fprintf(fout, "      v->executable->code = FbleAlloc(heap->arena, FbleCode);\n");
      fprintf(fout, "      v->executable->code->refcount = 1;\n");
      fprintf(fout, "      v->executable->code->magic = FBLE_CODE_MAGIC;\n");
      fprintf(fout, "      v->executable->code->statics = %i;\n", staticc);
      fprintf(fout, "      v->executable->code->locals = %i;\n", func_instr->code->locals);
      fprintf(fout, "      FbleVectorInit(heap->arena, v->executable->code->instrs);\n");
      fprintf(fout, "      v->executable->run = &_block_%p;\n", (void*)func_instr->code);
      fprintf(fout, "      v->argc = %i;\n", func_instr->argc);
      fprintf(fout, "      v->localc = %i;\n", func_instr->code->locals);
      fprintf(fout, "      v->staticc = %i;\n", staticc);
      for (size_t i = 0; i < staticc; ++i) {
        fprintf(fout, "      v->statics[%i] = ", i); FrameGet(fout, func_instr->scope.xs[i]); fprintf(fout, ";\n");
        fprintf(fout, "      FbleValueAddRef(heap, &v->_base, v->statics[%i]);\n", i);
      }
      FrameSetConsumed(fout, "      ", func_instr->dest, "&v->_base");
      return;
    };

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      fprintf(fout, "      FbleFuncValue* func = (FbleFuncValue*)"); FrameGetStrict(fout, call_instr->func); fprintf(fout, ";\n");
      fprintf(fout, "      if (func == NULL) {\n");
      VarId loc_id = GenLoc(fout, "      ", var_id, call_instr->loc);
      fprintf(fout, "        FbleReportError(\"called undefined function\\n\", v%x);\n", loc_id);
      fprintf(fout, "        FbleFreeLoc(heap->arena, v%x);\n", loc_id);
      fprintf(fout, "        return FBLE_EXEC_ABORTED;\n");
      fprintf(fout, "      }\n");

      fprintf(fout, "      assert(func->_base.tag == FBLE_FUNC_VALUE);\n");
      fprintf(fout, "      FbleValue* args[%i];\n", call_instr->args.size == 0 ? 1 : call_instr->args.size);
      for (size_t i = 0; i < call_instr->args.size; ++i) {
        fprintf(fout, "      args[%i] = ", i); FrameGet(fout, call_instr->args.xs[i]); fprintf(fout, ";\n");
      }

      if (call_instr->exit) {
        fprintf(fout, "      FbleThreadTailCall(heap, func, args, thread);\n");
        fprintf(fout, "      return FBLE_EXEC_FINISHED;\n");
        return;
      }

      fprintf(fout, "      thread->stack->pc = %i;\n", pc+1);

      fprintf(fout, "      FbleValue** result = thread->stack->locals.xs + %i;", call_instr->dest);
      fprintf(fout, "      FbleReleaseValue(heap, *result);\n");
      fprintf(fout, "      *result = NULL;\n");
      fprintf(fout, "      FbleThreadCall(heap, result, func, args, thread);\n");
      fprintf(fout, "      return FBLE_EXEC_FINISHED;\n");
      return;
    }

    case FBLE_GET_INSTR: assert(false && "TODO"); break;
    case FBLE_PUT_INSTR: assert(false && "TODO"); break;

    case FBLE_LINK_INSTR: {
      FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
      fprintf(fout, "      FbleLinkValue* link = FbleNewValue(heap, FbleLinkValue);\n");
      fprintf(fout, "      link->_base.tag = FBLE_LINK_VALUE;\n");
      fprintf(fout, "      link->head = NULL;\n");
      fprintf(fout, "      link->tail = NULL;\n");

      fprintf(fout, "      FbleValue* get = FbleNewGetValue(heap, &link->_base);\n");
      fprintf(fout, "      FbleValue* put = FbleNewPutValue(heap, &link->_base);\n");
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

        fprintf(fout, "      child = FbleAlloc(heap->arena, FbleThread);\n");
        fprintf(fout, "      child->stack = thread->stack;\n");
        fprintf(fout, "      child->profile = thread->profile == NULL ? NULL : FbleForkProfileThread(heap->arena, thread->profile);\n");
        fprintf(fout, "      child->stack->joins++;\n");
        fprintf(fout, "      FbleVectorAppend(heap->arena, *threads, child);\n");

        fprintf(fout, "      result = thread->stack->locals.xs + %i;", fork_instr->dests.xs[i]);
        fprintf(fout, "      FbleReleaseValue(heap, *result);\n");
        fprintf(fout, "      *result = NULL;\n");
        fprintf(fout, "      FbleThreadCall(heap, result, arg, NULL, child);\n");
      }

      fprintf(fout, "      thread->stack->pc = %i;\n", pc+1);
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
      fprintf(fout, "      FbleRefValue* rv = (FbleRefValue*)thread->stack->locals.xs[%i];\n", ref_instr->ref);
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
      VarId loc_id = GenLoc(fout, "      ", var_id, ref_instr->loc);
      fprintf(fout, "        FbleReportError(\"vacuous value\\n\", v%x);\n", loc_id);
      fprintf(fout, "        FbleFreeLoc(heap->arena, v%x);\n", loc_id);
      fprintf(fout, "        return FBLE_EXEC_ABORTED;\n");
      fprintf(fout, "      }\n");

      fprintf(fout, "      rv->value = v;\n");
      fprintf(fout, "      FbleValueAddRef(heap, &rv->_base, rv->value);\n");
      return;
    }

    case FBLE_RETURN_INSTR: {
      FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
      fprintf(fout, "      FbleValue* result = "); FrameGet(fout, return_instr->result); fprintf(fout, ";\n");
      fprintf(fout, "      FbleRefValue* ref_result = (FbleRefValue*)result;\n");
      fprintf(fout, "      while (result->tag == FBLE_REF_VALUE && ref_result->value != NULL) {;\n");
      fprintf(fout, "        result = ref_result->value;\n");
      fprintf(fout, "        ref_result = (FbleRefValue*)result;\n");
      fprintf(fout, "      }\n");

      fprintf(fout, "      FbleThreadReturn(heap, thread, result);\n");
      fprintf(fout, "      return FBLE_EXEC_FINISHED;\n");
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      fprintf(fout, "      FbleTypeValue* v = FbleNewValue(heap, FbleTypeValue);\n");
      fprintf(fout, "      v->_base.tag = FBLE_TYPE_VALUE;\n");
      FrameSetConsumed(fout, "      ", type_instr->dest, "&v->_base");
      return;
    }
  }
}

// FuncNameForPath --
//   Returns a name suitable for use as a C function identifier to use for the
//   give module path.
//
// Inputs:
//   arena - arena to use for allocations.
//   path - the path to get the name for.
//
// Results:
//   A C function name for the module path.
//
// Side effects:
//   Allocates an FbleString* that should be freed with FbleFreeString when no
//   longer needed.
static FbleString* FuncNameForPath(FbleArena* arena, FbleModulePath* path)
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

  return FbleNewString(arena, name);
}

// FbleGenerateC -- see documentation in fble-compile.h
bool FbleGenerateC(FILE* fout, FbleCompiledModule* module)
{
  FbleArena* arena = FbleNewArena();

  FbleCodeV blocks;
  FbleVectorInit(arena, blocks);
  CollectBlocks(arena, &blocks, module->code);

  fprintf(fout, "#include \"assert.h\"\n\n");

  fprintf(fout, "#include \"fble.h\"\n");
  fprintf(fout, "#include \"code.h\"\n");
  fprintf(fout, "#include \"tc.h\"\n");
  fprintf(fout, "#include \"value.h\"\n");
  fprintf(fout, "\n");

  // Prototypes for module dependencies.
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleString* dep_name = FuncNameForPath(arena, module->deps.xs[i]);
    fprintf(fout, "void %s(FbleArena* arena, FbleExecutableProgram* program);\n", dep_name->str);
    FbleFreeString(arena, dep_name);
  }
  fprintf(fout, "\n");

  // Prototypes for FbleRunFunction functions.
  for (int i = 0; i < blocks.size; ++i) {
    fprintf(fout, "static FbleExecStatus _block_%p(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);\n", (void*)blocks.xs[i]);
  }
  fprintf(fout, "\n");

  for (int i = 0; i < blocks.size; ++i) {
    FbleCode* code = blocks.xs[i];

    fprintf(fout, "static FbleExecStatus _block_%p(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)\n", (void*)blocks.xs[i]);
    fprintf(fout, "{\n");
    fprintf(fout, "  FbleArena* arena = heap->arena;\n");
    VarId var_id = 0;

    fprintf(fout, "  switch (thread->stack->pc) {\n");

    for (size_t i = 0; i < code->instrs.size; ++i) {
      fprintf(fout, "    case %i: _pc_%i: {\n", i, i);
      EmitInstr(fout, &var_id, i, code->instrs.xs[i]);
      fprintf(fout, "    }\n");
    }
    fprintf(fout, "  };\n");
    fprintf(fout, "  assert(false && \"should never get here.\");\n");
    fprintf(fout, "  return FBLE_EXEC_ABORTED;\n");
    fprintf(fout, "}\n\n");
  }
  FbleFree(arena, blocks.xs);

  FbleString* func_name = FuncNameForPath(arena, module->path);
  fprintf(fout, "void %s(FbleArena* arena, FbleExecutableProgram* program)\n", func_name->str);
  FbleFreeString(arena, func_name);

  fprintf(fout, "{\n");

  VarId var_id = 0;

  // Check if the module has already been loaded. If so, there's nothing to
  // do.
  VarId path_id = GenModulePath(fout, &var_id, module->path);
  fprintf(fout, "  for (size_t i = 0; i < program->modules.size; ++i) {\n");
  fprintf(fout, "    if (FbleModulePathsEqual(v%x, program->modules.xs[i].path)) {\n", path_id);
  fprintf(fout, "      FbleFreeModulePath(arena, v%x);\n", path_id);
  fprintf(fout, "      return;\n");
  fprintf(fout, "    }\n");
  fprintf(fout, "  }\n\n");

  // Make sure all dependencies have been loaded.
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleString* dep_name = FuncNameForPath(arena, module->deps.xs[i]);
    fprintf(fout, "  %s(arena, program);\n", dep_name->str);
    FbleFreeString(arena, dep_name);
  }
  fprintf(fout, "\n");

  // Add this module to the program.
  VarId module_id = var_id++;
  fprintf(fout, "  FbleExecutableModule* v%x = FbleVectorExtend(arena, program->modules);\n", module_id);
  fprintf(fout, "  v%x->path = v%x;\n", module_id, path_id);
  fprintf(fout, "  FbleVectorInit(arena, v%x->deps);\n", module_id);

  for (size_t i = 0; i < module->deps.size; ++i) {
    VarId dep_id = GenModulePath(fout, &var_id, module->deps.xs[i]);
    fprintf(fout, "  FbleVectorAppend(arena, v%x->deps, v%x);\n", module_id, dep_id);
  }

  fprintf(fout, "  v%x->executable = FbleAlloc(arena, FbleExecutable);\n", module_id);
  fprintf(fout, "  v%x->executable->code = FbleAlloc(arena, FbleCode);\n", module_id);
  fprintf(fout, "  v%x->executable->code->refcount = 1;\n", module_id);
  fprintf(fout, "  v%x->executable->code->magic = FBLE_CODE_MAGIC;\n", module_id);
  fprintf(fout, "  v%x->executable->code->statics = %i;\n", module_id, module->code->statics);
  fprintf(fout, "  v%x->executable->code->locals = %i;\n", module_id, module->code->locals);
  fprintf(fout, "  FbleVectorInit(arena, v%x->executable->code->instrs);\n", module_id);
  fprintf(fout, "  v%x->executable->run = &_block_%p;\n", module_id, (void*)module->code);

  fprintf(fout, "}\n");

  FbleFreeArena(arena);
  return true;
}

// FbleGenerateCExport -- see documentation in fble-compile.h
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path)
{
  FbleArena* arena = FbleNewArena();

  fprintf(fout, "#include \"fble.h\"\n");
  fprintf(fout, "#include \"value.h\"\n");
  fprintf(fout, "\n");

  // Prototype for the exported module.
  FbleString* module_name = FuncNameForPath(arena, path);
  fprintf(fout, "void %s(FbleArena* arena, FbleExecutableProgram* program);\n\n", module_name->str);

  fprintf(fout, "FbleValue* %s(FbleValueHeap* heap)\n", name);
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleExecutableProgram* program = FbleAlloc(heap->arena, FbleExecutableProgram);\n");
  fprintf(fout, "  FbleVectorInit(heap->arena, program->modules);\n");
  fprintf(fout, "  %s(heap->arena, program);\n", module_name->str);
  fprintf(fout, "  FbleValue* value = FbleLink(heap, program);\n");
  fprintf(fout, "  FbleFreeExecutableProgram(heap->arena, program);\n");
  fprintf(fout, "  return value;\n");
  fprintf(fout, "}\n\n");

  FbleFreeString(arena, module_name);
  FbleFreeArena(arena);
}
