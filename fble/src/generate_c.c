// generate_c.c --
//   This file describes code to generate c code for fble values.

#include <assert.h>   // for assert

#include "tc.h"

#define UNREACHABLE(x) assert(false && x)

// VarId --
//   Type representing a C variable name as an integer.
//
// The number is turned into a C variable name using printf format "v%x".
typedef unsigned int VarId;

static VarId GenLoc(FILE* fout, VarId* var_id, FbleLoc loc);
static VarId GenFrameIndex(FILE* fout, VarId* var_id, FbleFrameIndex index);
static VarId GenInstr(FILE* fout, VarId* var_id, FbleInstr* instr);
static VarId GenInstrBlock(FILE* fout, VarId* var_id, FbleInstrBlock* code);
static VarId GenCompiledFuncValue(FILE* fout, VarId* var_id, FbleCompiledFuncValueTc* tc);

// GenLoc --
//   Generate code for a FbleLoc.
//
// Assumes there is a C variable 'FbleValueHeap* heap' in scope for allocating
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
static VarId GenLoc(FILE* fout, VarId* var_id, FbleLoc loc)
{
  VarId source = (*var_id)++;
  fprintf(fout, "  FbleString v%x = FbleNewString(heap->arena, \"%s\");\n",
      source, loc.source->str);

  VarId id = (*var_id)++;
  fprintf(fout, "  FbleLoc v%x = { .source = v%x, .line = %i, .col = %i };\n",
      id, source, loc.line, loc.col);
  return id;
}

// GenFrameIndex --
//   Generate code for a FbleFrameIndex.
//
// Assumes there is a C variable 'FbleValueHeap* heap' in scope for allocating
// FbleFrameIndex.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   index - the FbleFrameIndex to generate code for.
//
// Results:
//   The variable id of the generated FbleFrameIndex.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static VarId GenFrameIndex(FILE* fout, VarId* var_id, FbleFrameIndex index)
{
  static const char* sections[] = {
    "FBLE_STATICS_FRAME_SECTION",
    "FBLE_LOCALS_FRAME_SECTION"
  };
  VarId frame_index = (*var_id)++;
  fprintf(fout, "  FbleFrameIndex v%x = { .section = %s, .index = %i };\n",
      frame_index, sections[index.section], index.index);
  return frame_index;
}

// GenInstr --
//   Generate code for an FbleInstr.
//
// Assumes there is a C variable 'FbleValueHeap* heap' in scope for allocating
// FbleInstr.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   instr - the FbleInstr to generate code for.
//
// Results:
//   The variable id of the generated FbleInstr.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static VarId GenInstr(FILE* fout, VarId* var_id, FbleInstr* instr)
{
  VarId instr_id = (*var_id)++;
  switch (instr->tag) {
    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      fprintf(fout, "  FbleStructValueInstr* v%x = FbleAlloc(arena, FbleStructValueInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_STRUCT_VALUE_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  FbleVectorInit(heap->arena, v%x->args);\n", instr_id);
      for (size_t i = 0; i < struct_instr->args.size; ++i) {
        VarId frame_index = GenFrameIndex(fout, var_id, struct_instr->args.xs[i]);
        fprintf(fout, "  FbleVectorAppend(heap->arena, v%x->args, v%x);\n", instr_id, frame_index);
      }
      fprintf(fout, "  v%x->dest = %i;\n\n", instr_id, struct_instr->dest);
      break;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      fprintf(fout, "  FbleUnionValueInstr* v%x = FbleAlloc(arena, FbleUnionValueInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_UNION_VALUE_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  v%x->tag = %i;\n", instr_id, union_instr->tag);
      VarId arg = GenFrameIndex(fout, var_id, union_instr->arg);
      fprintf(fout, "  v%x->arg = v%x;\n", instr_id, arg);
      fprintf(fout, "  v%x->dest = %i;\n\n", instr_id, union_instr->dest);
      break;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "  FbleAccessInstr* v%x = FbleAlloc(arena, FbleAccessInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_STRUCT_ACCESS_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      VarId loc = GenLoc(fout, var_id, access_instr->loc);
      fprintf(fout, "  v%x->loc = v%x;\n", instr_id, loc);
      VarId obj = GenFrameIndex(fout, var_id, access_instr->obj);
      fprintf(fout, "  v%x->obj = v%x;\n", instr_id, obj);
      fprintf(fout, "  v%x->tag = %i;\n", instr_id, access_instr->tag);
      fprintf(fout, "  v%x->dest = %i;\n\n", instr_id, access_instr->dest);
      break;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "  FbleAccessInstr* v%x = FbleAlloc(arena, FbleAccessInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_UNION_ACCESS_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      VarId loc = GenLoc(fout, var_id, access_instr->loc);
      fprintf(fout, "  v%x->loc = v%x;\n", instr_id, loc);
      VarId obj = GenFrameIndex(fout, var_id, access_instr->obj);
      fprintf(fout, "  v%x->obj = v%x;\n", instr_id, obj);
      fprintf(fout, "  v%x->tag = %i;\n", instr_id, access_instr->tag);
      fprintf(fout, "  v%x->dest = %i;\n\n", instr_id, access_instr->dest);
      break;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
      fprintf(fout, "  FbleUnionSelectInstr* v%x = FbleAlloc(arena, FbleUnionSelectInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_UNION_SELECT_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      VarId loc = GenLoc(fout, var_id, select_instr->loc);
      fprintf(fout, "  v%x->loc = v%x;\n", instr_id, loc);
      VarId condition = GenFrameIndex(fout, var_id, select_instr->condition);
      fprintf(fout, "  v%x->condition = v%x;\n", instr_id, condition);
      fprintf(fout, "  FbleVectorInit(heap->arena, v%x->jumps);\n", instr_id);
      for (size_t i = 0; i < select_instr->jumps.size; ++i) {
        fprintf(fout, "  FbleVectorAppend(heap->arena, v%x->jumps, %i);\n", instr_id, select_instr->jumps.xs[i]);
      }
      break;
    }

    case FBLE_JUMP_INSTR: {
      FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
      fprintf(fout, "  FbleJumpInstr* v%x = FbleAlloc(arena, FbleJumpInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_JUMP_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  v%x->count = %i;\n\n", instr_id, jump_instr->count);
      break;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;
      fprintf(fout, "  FbleFuncValueInstr* v%x = FbleAlloc(arena, FbleFuncValueInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_FUNC_VALUE_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  v%x->argc = %i;\n", instr_id, func_instr->argc);
      fprintf(fout, "  v%x->dest = %i;\n", instr_id, func_instr->dest);
      VarId code = GenInstrBlock(fout, var_id, func_instr->code);
      fprintf(fout, "  v%x->code = v%x;\n", instr_id, code);
      fprintf(fout, "  FbleVectorInit(heap->arena, v%x->scope);\n", instr_id);
      for (size_t i = 0; i < func_instr->scope.size; ++i) {
        VarId frame_index = GenFrameIndex(fout, var_id, func_instr->scope.xs[i]);
        fprintf(fout, "  FbleVectorAppend(heap->arena, v%x->scope, v%x);\n", instr_id, frame_index);
      }
      fprintf(fout, "\n");
      break;
    };

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      fprintf(fout, "  FbleCallInstr* v%x = FbleAlloc(arena, FbleCallInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_CALL_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      VarId loc = GenLoc(fout, var_id, call_instr->loc);
      fprintf(fout, "  v%x->loc = v%x;\n", instr_id, loc);
      fprintf(fout, "  v%x->exit = %s;\n", instr_id, call_instr->exit ? "true" : "false");
      fprintf(fout, "  v%x->dest = %i;\n", instr_id, call_instr->dest);
      VarId func = GenFrameIndex(fout, var_id, call_instr->func);
      fprintf(fout, "  v%x->func = v%x;\n", instr_id, func);
      fprintf(fout, "  FbleVectorInit(heap->arena, v%x->args);\n", instr_id);
      for (size_t i = 0; i < call_instr->args.size; ++i) {
        VarId frame_index = GenFrameIndex(fout, var_id, call_instr->args.xs[i]);
        fprintf(fout, "  FbleVectorAppend(heap->arena, v%x->args, v%x);\n", instr_id, frame_index);
      }
      fprintf(fout, "\n");
      break;
    }

    case FBLE_GET_INSTR: assert(false && "TODO"); break;
    case FBLE_PUT_INSTR: assert(false && "TODO"); break;

    case FBLE_LINK_INSTR: {
      FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
      fprintf(fout, "  FbleLinkInstr* v%x = FbleAlloc(arena, FbleLinkInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_LINK_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  v%x->get = %i;\n", instr_id, link_instr->get);
      fprintf(fout, "  v%x->put = %i;\n\n", instr_id, link_instr->put);
      break;
    }

    case FBLE_FORK_INSTR: {
      FbleForkInstr* fork_instr = (FbleForkInstr*)instr;
      fprintf(fout, "  FbleForkInstr* v%x = FbleAlloc(arena, FbleForkInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_FORK_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  FbleVectorInit(heap->arena, v%x->args);\n", instr_id);
      for (size_t i = 0; i < fork_instr->args.size; ++i) {
        VarId frame_index = GenFrameIndex(fout, var_id, fork_instr->args.xs[i]);
        fprintf(fout, "  FbleVectorAppend(heap->arena, v%x->args, v%x);\n", instr_id, frame_index);
      }
      fprintf(fout, "  FbleVectorInit(heap->arena, v%x->dests);\n", instr_id);
      for (size_t i = 0; i < fork_instr->dests.size; ++i) {
        fprintf(fout, "  FbleVectorAppend(heap->arena, v%x->dests, %i);\n",
            instr_id, fork_instr->dests.xs[i]);
      }
      fprintf(fout, "\n");
      break;
    }

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      fprintf(fout, "  FbleCopyInstr* v%x = FbleAlloc(arena, FbleCopyInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_COPY_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      VarId source = GenFrameIndex(fout, var_id, copy_instr->source);
      fprintf(fout, "  v%x->source = v%x;\n", instr_id, source);
      fprintf(fout, "  v%x->dest = %i;\n\n", instr_id, copy_instr->dest);
      break;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      fprintf(fout, "  FbleRefValueInstr* v%x = FbleAlloc(arena, FbleRefValueInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_REF_VALUE_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  v%x->dest = %i;\n\n", instr_id, ref_instr->dest);
      break;
    }

    case FBLE_REF_DEF_INSTR: {
      FbleRefDefInstr* ref_instr = (FbleRefDefInstr*)instr;
      fprintf(fout, "  FbleRefDefInstr* v%x = FbleAlloc(arena, FbleRefDefInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_REF_DEF_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      VarId loc = GenLoc(fout, var_id, ref_instr->loc);
      fprintf(fout, "  v%x->loc = v%x;\n", instr_id, loc);
      fprintf(fout, "  v%x->ref = %i;\n", instr_id, ref_instr->ref);
      VarId value = GenFrameIndex(fout, var_id, ref_instr->value);
      fprintf(fout, "  v%x->value = v%x;\n\n", instr_id, value);
      break;
    }

    case FBLE_RETURN_INSTR: {
      FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
      fprintf(fout, "  FbleReturnInstr* v%x = FbleAlloc(arena, FbleReturnInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_RETURN_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      VarId result = GenFrameIndex(fout, var_id, return_instr->result);
      fprintf(fout, "  v%x->result = v%x;\n\n", instr_id, result);
      break;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      fprintf(fout, "  FbleTypeInstr* v%x = FbleAlloc(arena, FbleTypeInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_TYPE_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  v%x->dest = %i;\n\n", instr_id, type_instr->dest);
      break;
    }
  }

  VarId base = (*var_id)++;
  fprintf(fout, "  FbleInstr* v%x = &v%x->_base;\n", base, instr_id);

  VarId next = (*var_id)++;
  VarId op_id = (*var_id)++;
  if (instr->profile_ops != NULL) {
    fprintf(fout, "  FbleProfileOp** v%x = &v%x->profile_ops;\n", next, base);
    fprintf(fout, "  FbleProfileOp* v%x = NULL;\n", op_id);
  }

  for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
    static const char* op_tags[] = {
      "FBLE_PROFILE_ENTER_OP",
      "FBLE_PROFILE_EXIT_OP",
      "FBLE_PROFILE_AUTO_EXIT_OP"
    };

    fprintf(fout, "  v%x = FbleAlloc(heap->arena, FbleProfileOp);\n", op_id);
    fprintf(fout, "  v%x->tag = %s\n", op_id, op_tags[op->tag]);
    fprintf(fout, "  v%x->block = %i;\n", op_id, op->block);
    fprintf(fout, "  v%x->next = NULL;\n", op_id);
    fprintf(fout, "  *v%x = v%x;\n", next, op_id);
    fprintf(fout, "  v%x = &v%x->next;\n\n", next, op_id);
  }

  return base;
}

// GenInstrBlock --
//   Generate code for an FbleInstrBlock.
//
// Assumes there is a C variable 'FbleValueHeap* heap' in scope for allocating
// FbleInstrBlock.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   code - the FbleInstrBlock to generate code for.
//
// Results:
//   The variable id of the generated FbleInstrBlock.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static VarId GenInstrBlock(FILE* fout, VarId* var_id, FbleInstrBlock* code)
{
  VarId id = (*var_id)++;
  fprintf(fout, "  FbleInstrBlock* v%x = FbleAlloc(heap->arena, FbleInstrBlock);\n", id);
  fprintf(fout, "  v%x->refcount = 1;\n", id);
  fprintf(fout, "  v%x->magic = FBLE_INSTR_BLOCK_MAGIC;\n", id);
  fprintf(fout, "  v%x->statics = %i;\n", id, code->statics);
  fprintf(fout, "  v%x->locals = %i;\n", id, code->locals);
  fprintf(fout, "  FbleVectorInit(heap->arena, v%x->instrs);\n\n", id);

  for (size_t i = 0; i < code->instrs.size; ++i) {
    VarId instr_id = GenInstr(fout, var_id, code->instrs.xs[i]);
    fprintf(fout, "  FbleVectorAppend(heap->arena, v%x->instrs, v%x);\n", id, instr_id);
  }
  return id;
}

// GenCompiledFuncValue --
//   Generate code for an FbleCompiledFuncValueTc.
//
// Assumes there is a C variable 'FbleValueHeap* heap' in scope for allocating
// the value.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   tc - the value to generate code for.
//
// Results:
//   The variable id of the generated value.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static VarId GenCompiledFuncValue(FILE* fout, VarId* var_id, FbleCompiledFuncValueTc* tc)
{
  assert(tc->argc == 0 && "TODO: support arguments to compiled funcs");
  VarId func = (*var_id)++;
  fprintf(fout, "  FbleCompiledFuncValueTc* v%x = FbleNewValue(heap, FbleCompiledFuncValueTc);\n", func);
  fprintf(fout, "  v%x->_base.tag = FBLE_COMPILED_FUNC_VALUE_TC;\n", func);
  fprintf(fout, "  v%x->argc = %i;\n", func, tc->argc);
  VarId code = GenInstrBlock(fout, var_id, tc->code);
  fprintf(fout, "  v%x->code = v%x;\n", func, code);
  fprintf(fout, "  v%x->run = &FbleStandardRunFunction;\n\n", func);
  return func;
}

// FbleGenerateC -- see documentation in fble.h
bool FbleGenerateC(FILE* fout, const char* entry, FbleValue* value)
{
  VarId var_id = 0;

  fprintf(fout, "#include \"fble.h\"\n");
  fprintf(fout, "#include \"instr.h\"\n");

  fprintf(fout, "FbleValue* %s(FbleValueHeap* heap)\n", entry);
  fprintf(fout, "{\n");

  VarId result_id;
  switch (value->tag) {
    case FBLE_TYPE_VALUE_TC: assert(false && "TODO"); return false;
    case FBLE_VAR_TC: assert(false && "TODO"); return false;
    case FBLE_LET_TC: assert(false && "TODO"); return false;
    case FBLE_STRUCT_VALUE_TC: assert(false && "TODO"); return false;
    case FBLE_UNION_VALUE_TC: assert(false && "TODO"); return false;
    case FBLE_UNION_SELECT_TC: assert(false && "TODO"); return false;
    case FBLE_DATA_ACCESS_TC: assert(false && "TODO"); return false;
    case FBLE_FUNC_VALUE_TC: assert(false && "TODO"); return false;

    case FBLE_COMPILED_FUNC_VALUE_TC: {
      result_id = GenCompiledFuncValue(fout, &var_id, (FbleCompiledFuncValueTc*)value);
      break;
    }

    case FBLE_FUNC_APPLY_TC: assert(false && "TODO"); return false;
    case FBLE_LINK_VALUE_TC: assert(false && "TODO"); return false;
    case FBLE_PORT_VALUE_TC: assert(false && "TODO"); return false;
    case FBLE_LINK_TC: assert(false && "TODO"); return false;
    case FBLE_EXEC_TC: assert(false && "TODO"); return false;
    case FBLE_PROFILE_TC: assert(false && "TODO"); return false;
    case FBLE_THUNK_VALUE_TC: assert(false && "TODO"); return false;
  }

  fprintf(fout, "  return v%x;\n", result_id);
  fprintf(fout, "}\n");
  return true;
}