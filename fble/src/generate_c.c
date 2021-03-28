// generate_c.c --
//   This file describes code to generate c code for fble values.

#include <assert.h>   // for assert

#include "fble-compile.h"
#include "isa.h"
#include "tc.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// VarId --
//   Type representing a C variable name as an integer.
//
// The number is turned into a C variable name using printf format "v%x".
typedef unsigned int VarId;

static void CollectBlocks(FbleArena* arena, FbleInstrBlockV* blocks, FbleInstrBlock* code);

static VarId GenLoc(FILE* fout, VarId* var_id, FbleLoc loc);
static VarId GenFrameIndex(FILE* fout, VarId* var_id, FbleFrameIndex index);
static VarId GenInstr(FILE* fout, VarId* var_id, FbleInstr* instr);
static VarId GetInstrBlock(FILE* fout, VarId* var_id, FbleInstrBlock* code);
static VarId GenInstrBlock(FILE* fout, VarId* var_id, FbleInstrBlock* code);

// CollectBlocks --
//   Get the list of all instruction blocks referenced from the given block of
//   code, including the code itself.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the collection of blocks to add to.
//   code - the code to collect the blocks from.
static void CollectBlocks(FbleArena* arena, FbleInstrBlockV* blocks, FbleInstrBlock* code)
{
  FbleVectorAppend(arena, *blocks, code);
  for (size_t i = 0; i < code->instrs.size; ++i) {
    if (code->instrs.xs[i]->tag == FBLE_FUNC_VALUE_INSTR) {
      FbleFuncValueInstr* instr = (FbleFuncValueInstr*)code->instrs.xs[i];
      CollectBlocks(arena, blocks, instr->code);
    }
  }
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
static VarId GenLoc(FILE* fout, VarId* var_id, FbleLoc loc)
{
  VarId source = (*var_id)++;
  fprintf(fout, "  FbleString* v%x = FbleNewString(arena, \"%s\");\n",
      source, loc.source->str);

  VarId id = (*var_id)++;
  fprintf(fout, "  FbleLoc v%x = { .source = v%x, .line = %i, .col = %i };\n",
      id, source, loc.line, loc.col);
  return id;
}

// GenFrameIndex --
//   Generate code for a FbleFrameIndex.
//
// Assumes there is a C variable 'FbleArena* arena' in scope for allocating
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
// Assumes there is a C variable 'FbleArena* arena' in scope for allocating
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
      fprintf(fout, "  FbleVectorInit(arena, v%x->args);\n", instr_id);
      for (size_t i = 0; i < struct_instr->args.size; ++i) {
        VarId frame_index = GenFrameIndex(fout, var_id, struct_instr->args.xs[i]);
        fprintf(fout, "  FbleVectorAppend(arena, v%x->args, v%x);\n", instr_id, frame_index);
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
      fprintf(fout, "  FbleVectorInit(arena, v%x->jumps);\n", instr_id);
      for (size_t i = 0; i < select_instr->jumps.size; ++i) {
        fprintf(fout, "  FbleVectorAppend(arena, v%x->jumps, %i);\n", instr_id, select_instr->jumps.xs[i]);
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
      VarId code = GetInstrBlock(fout, var_id, func_instr->code);
      fprintf(fout, "  v%x->code = v%x;\n", instr_id, code);
      fprintf(fout, "  FbleVectorInit(arena, v%x->scope);\n", instr_id);
      for (size_t i = 0; i < func_instr->scope.size; ++i) {
        VarId frame_index = GenFrameIndex(fout, var_id, func_instr->scope.xs[i]);
        fprintf(fout, "  FbleVectorAppend(arena, v%x->scope, v%x);\n", instr_id, frame_index);
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
      fprintf(fout, "  FbleVectorInit(arena, v%x->args);\n", instr_id);
      for (size_t i = 0; i < call_instr->args.size; ++i) {
        VarId frame_index = GenFrameIndex(fout, var_id, call_instr->args.xs[i]);
        fprintf(fout, "  FbleVectorAppend(arena, v%x->args, v%x);\n", instr_id, frame_index);
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
      fprintf(fout, "  FbleVectorInit(arena, v%x->args);\n", instr_id);
      for (size_t i = 0; i < fork_instr->args.size; ++i) {
        VarId frame_index = GenFrameIndex(fout, var_id, fork_instr->args.xs[i]);
        fprintf(fout, "  FbleVectorAppend(arena, v%x->args, v%x);\n", instr_id, frame_index);
      }
      fprintf(fout, "  FbleVectorInit(arena, v%x->dests);\n", instr_id);
      for (size_t i = 0; i < fork_instr->dests.size; ++i) {
        fprintf(fout, "  FbleVectorAppend(arena, v%x->dests, %i);\n",
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

    fprintf(fout, "  v%x = FbleAlloc(arena, FbleProfileOp);\n", op_id);
    fprintf(fout, "  v%x->tag = %s;\n", op_id, op_tags[op->tag]);
    fprintf(fout, "  v%x->block = %i;\n", op_id, op->block);
    fprintf(fout, "  v%x->next = NULL;\n", op_id);
    fprintf(fout, "  *v%x = v%x;\n", next, op_id);
    fprintf(fout, "  v%x = &v%x->next;\n\n", next, op_id);
  }

  return base;
}

// GetInstrBlock --
//   Generate code for getting an FbleInstrBlock.
//
// Assumes there is a C variable 'FbleArena* arena' in scope for allocating
// FbleInstrBlock.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   code - the FbleInstrBlock to get.
//
// Results:
//   The variable id of the generated FbleInstrBlock.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static VarId GetInstrBlock(FILE* fout, VarId* var_id, FbleInstrBlock* code)
{
  VarId id = (*var_id)++;
  fprintf(fout, "  FbleInstrBlock* v%x = _block_%p(arena);\n", id, (void*)code);
  return id;
}

// GenInstrBlock --
//   Generate code for an FbleInstrBlock.
//
// Assumes there is a C variable 'FbleArena* arena' in scope for allocating
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
  fprintf(fout, "  FbleInstrBlock* v%x = FbleAlloc(arena, FbleInstrBlock);\n", id);
  fprintf(fout, "  v%x->refcount = 1;\n", id);
  fprintf(fout, "  v%x->magic = FBLE_INSTR_BLOCK_MAGIC;\n", id);
  fprintf(fout, "  v%x->statics = %i;\n", id, code->statics);
  fprintf(fout, "  v%x->locals = %i;\n", id, code->locals);
  fprintf(fout, "  FbleVectorInit(arena, v%x->instrs);\n\n", id);

  for (size_t i = 0; i < code->instrs.size; ++i) {
    VarId instr_id = GenInstr(fout, var_id, code->instrs.xs[i]);
    fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, v%x);\n", id, instr_id);
  }
  return id;
}

// FbleGenerateC -- see documentation in fble-compile.h
bool FbleGenerateC(FILE* fout, const char* entry, FbleInstrBlock* code)
{
  FbleArena* arena = FbleNewArena();

  FbleInstrBlockV blocks;
  FbleVectorInit(arena, blocks);
  CollectBlocks(arena, &blocks, code);

  fprintf(fout, "#include \"fble.h\"\n");
  fprintf(fout, "#include \"isa.h\"\n");
  fprintf(fout, "#include \"tc.h\"\n");
  fprintf(fout, "#include \"value.h\"\n");
  fprintf(fout, "\n");

  for (int i = 0; i < blocks.size; ++i) {
    fprintf(fout, "static FbleInstrBlock* _block_%p(FbleArena* arena);\n", (void*)blocks.xs[i]);
  }
  fprintf(fout, "\n");

  for (int i = 0; i < blocks.size; ++i) {
    fprintf(fout, "static FbleInstrBlock* _block_%p(FbleArena* arena)\n", (void*)blocks.xs[i]);
    fprintf(fout, "{\n");
    VarId var_id = 0;
    VarId result = GenInstrBlock(fout, &var_id, blocks.xs[i]);
    fprintf(fout, "  return v%x;\n", result);
    fprintf(fout, "}\n\n");
  }
  FbleFree(arena, blocks.xs);
  FbleFreeArena(arena);

  fprintf(fout, "FbleValue* %s(FbleValueHeap* heap)\n", entry);
  fprintf(fout, "{\n");
  VarId var_id = 0;

  VarId func_id = var_id++;
  fprintf(fout, "  FbleArena* arena = heap->arena;\n");
  fprintf(fout, "  FbleFuncValue* v%x = FbleNewValue(heap, FbleFuncValue);\n", func_id);
  fprintf(fout, "  v%x->_base.tag = FBLE_FUNC_VALUE;\n", func_id);
  fprintf(fout, "  v%x->argc = %i;\n", func_id, 0);   // TODO: don't assume this?
  VarId code_id = GetInstrBlock(fout, &var_id, code);
  fprintf(fout, "  v%x->code = FbleAlloc(arena, FbleCode);\n", func_id);
  fprintf(fout, "  v%x->code->code = v%x;\n", func_id, code_id);
  fprintf(fout, "  v%x->code->run = &FbleStandardRunFunction;\n\n", func_id);

  fprintf(fout, "  return &v%x->_base;\n", func_id);
  fprintf(fout, "}\n");

  return true;
}
