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
static void AppendInstr(FILE* fout, VarId* var_id, VarId block_id, FbleInstr* instr);
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

// AppendInstr --
//   Generate code to append an FbleInstr to a vector of instructions.
//
// Assumes there is a C variable 'FbleArena* arena' in scope for allocating
// FbleInstr.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   block_id - id of the variable for the FbleInstrBlock to add the
//              instruction to.
//   instr - the FbleInstr to add to the block.
//
// Side effects:
// * Outputs code to fout with two space indent.
// * Increments var_id based on the number of internal variables used.
static void AppendInstr(FILE* fout, VarId* var_id, VarId block_id, FbleInstr* instr)
{
  static const char* sections[] = {
    "FBLE_STATICS_FRAME_SECTION",
    "FBLE_LOCALS_FRAME_SECTION"
  };

  VarId instr_id = (*var_id)++;
  switch (instr->tag) {
    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;

      if (struct_instr->args.size == 0) {
        fprintf(fout, "  AppendStruct0Instr(arena, %i, v%x);\n",
          struct_instr->dest, block_id);
        return;
      }

      if (struct_instr->args.size == 2) {
        fprintf(fout, "  AppendStruct2Instr(arena, %s, %i, %s, %i, %i, v%x);\n",
          sections[struct_instr->args.xs[0].section],
          struct_instr->args.xs[0].index,
          sections[struct_instr->args.xs[1].section],
          struct_instr->args.xs[1].index,
          struct_instr->dest, block_id);
        return;
      }

      fprintf(fout, "  FbleStructValueInstr* v%x = FbleAlloc(arena, FbleStructValueInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_STRUCT_VALUE_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  FbleVectorInit(arena, v%x->args);\n", instr_id);
      for (size_t i = 0; i < struct_instr->args.size; ++i) {
        VarId frame_index = GenFrameIndex(fout, var_id, struct_instr->args.xs[i]);
        fprintf(fout, "  FbleVectorAppend(arena, v%x->args, v%x);\n", instr_id, frame_index);
      }
      fprintf(fout, "  v%x->dest = %i;\n\n", instr_id, struct_instr->dest);
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      fprintf(fout, "  AppendUnionInstr(arena, %i, %s, %i, %i, v%x);\n",
        union_instr->tag,
        sections[union_instr->arg.section],
        union_instr->arg.index,
        union_instr->dest,
        block_id);
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR:
    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      fprintf(fout, "  AppendAccessInstr(arena, %s, \"%s\", %i, %i, %s, %i, %i, %i, v%x);\n",
        instr->tag == FBLE_STRUCT_ACCESS_INSTR ? "FBLE_STRUCT_ACCESS_INSTR" : "FBLE_UNION_ACCESS_INSTR",
        access_instr->loc.source->str,
        access_instr->loc.line,
        access_instr->loc.col,
        sections[access_instr->obj.section],
        access_instr->obj.index,
        access_instr->tag,
        access_instr->dest,
        block_id);
      return;
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
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
    }

    case FBLE_JUMP_INSTR: {
      FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
      fprintf(fout, "  FbleJumpInstr* v%x = FbleAlloc(arena, FbleJumpInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_JUMP_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  v%x->count = %i;\n\n", instr_id, jump_instr->count);
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
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
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
    };

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      if (call_instr->args.size == 1) {
        fprintf(fout, "  AppendCall1Instr(arena, \"%s\", %i, %i, %s, %s, %i, %s, %i, %i, v%x);\n",
          call_instr->loc.source->str,
          call_instr->loc.line,
          call_instr->loc.col,
          call_instr->exit ? "true" : "false",
          sections[call_instr->func.section],
          call_instr->func.index,
          sections[call_instr->args.xs[0].section],
          call_instr->args.xs[0].index,
          call_instr->dest,
          block_id);
        return;
      }

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
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
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
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
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
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
    }

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      fprintf(fout, "  FbleCopyInstr* v%x = FbleAlloc(arena, FbleCopyInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_COPY_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      VarId source = GenFrameIndex(fout, var_id, copy_instr->source);
      fprintf(fout, "  v%x->source = v%x;\n", instr_id, source);
      fprintf(fout, "  v%x->dest = %i;\n\n", instr_id, copy_instr->dest);
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      fprintf(fout, "  FbleRefValueInstr* v%x = FbleAlloc(arena, FbleRefValueInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_REF_VALUE_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  v%x->dest = %i;\n\n", instr_id, ref_instr->dest);
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
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
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
    }

    case FBLE_RETURN_INSTR: {
      FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
      fprintf(fout, "  FbleReturnInstr* v%x = FbleAlloc(arena, FbleReturnInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_RETURN_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      VarId result = GenFrameIndex(fout, var_id, return_instr->result);
      fprintf(fout, "  v%x->result = v%x;\n\n", instr_id, result);
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      fprintf(fout, "  FbleTypeInstr* v%x = FbleAlloc(arena, FbleTypeInstr);\n", instr_id);
      fprintf(fout, "  v%x->_base.tag = FBLE_TYPE_INSTR;\n", instr_id);
      fprintf(fout, "  v%x->_base.profile_ops = NULL;\n", instr_id);
      fprintf(fout, "  v%x->dest = %i;\n\n", instr_id, type_instr->dest);
      fprintf(fout, "  FbleVectorAppend(arena, v%x->instrs, &v%x->_base);\n", block_id, instr_id);
      return;
    }
  }
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
    AppendInstr(fout, var_id, id, code->instrs.xs[i]);
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

  // Helper function for appending zero-argument struct value instructions,
  // which can take up a lot of space from literals.
  fprintf(fout, "static void AppendStruct0Instr(FbleArena* arena, FbleLocalIndex dest, FbleInstrBlock* block)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleStructValueInstr* si = FbleAlloc(arena, FbleStructValueInstr);\n");
  fprintf(fout, "  si->_base.tag = FBLE_STRUCT_VALUE_INSTR;\n");
  fprintf(fout, "  si->_base.profile_ops = NULL;\n");
  fprintf(fout, "  FbleVectorInit(arena, si->args);\n");
  fprintf(fout, "  si->dest = dest;\n");
  fprintf(fout, "  FbleVectorAppend(arena, block->instrs, &si->_base);\n");
  fprintf(fout, "}\n\n");

  // Helper function for appending two-argument struct value instructions,
  // which can take up a lot of space from literals.
  fprintf(fout, "static void AppendStruct2Instr(FbleArena* arena, FbleFrameSection arg0_section, size_t arg0_index, FbleFrameSection arg1_section, size_t arg1_index, FbleLocalIndex dest, FbleInstrBlock* block)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleStructValueInstr* si = FbleAlloc(arena, FbleStructValueInstr);\n");
  fprintf(fout, "  si->_base.tag = FBLE_STRUCT_VALUE_INSTR;\n");
  fprintf(fout, "  si->_base.profile_ops = NULL;\n");
  fprintf(fout, "  FbleVectorInit(arena, si->args);\n");
  fprintf(fout, "  FbleFrameIndex arg0 = { .section = arg0_section, .index = arg0_index };\n");
  fprintf(fout, "  FbleVectorAppend(arena, si->args, arg0);\n");
  fprintf(fout, "  FbleFrameIndex arg1 = { .section = arg1_section, .index = arg1_index };\n");
  fprintf(fout, "  FbleVectorAppend(arena, si->args, arg1);\n");
  fprintf(fout, "  si->dest = dest;\n");
  fprintf(fout, "  FbleVectorAppend(arena, block->instrs, &si->_base);\n");
  fprintf(fout, "}\n\n");

  // Helper function for appending union value instructions, which can take up
  // a lot of space from literals.
  fprintf(fout, "static void AppendUnionInstr(FbleArena* arena, size_t tag, FbleFrameSection arg_section, size_t arg_index, FbleLocalIndex dest, FbleInstrBlock* block)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleUnionValueInstr* ui = FbleAlloc(arena, FbleUnionValueInstr);\n");
  fprintf(fout, "  ui->_base.tag = FBLE_UNION_VALUE_INSTR;\n");
  fprintf(fout, "  ui->_base.profile_ops = NULL;\n");
  fprintf(fout, "  ui->tag = tag;\n");
  fprintf(fout, "  ui->arg.section = arg_section;\n");
  fprintf(fout, "  ui->arg.index = arg_index;\n");
  fprintf(fout, "  ui->dest = dest;\n");
  fprintf(fout, "  FbleVectorAppend(arena, block->instrs, &ui->_base);\n");
  fprintf(fout, "}\n\n");

  // Helper function for appending one-argument call instructions,
  // which can take up a lot of space from literals.
  fprintf(fout, "static void AppendCall1Instr(FbleArena* arena, const char* src, size_t line, size_t col, bool exit, FbleFrameSection func_section, size_t func_index, FbleFrameSection arg_section, size_t arg_index, FbleLocalIndex dest, FbleInstrBlock* block)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleCallInstr* i = FbleAlloc(arena, FbleCallInstr);\n");
  fprintf(fout, "  i->_base.tag = FBLE_CALL_INSTR;\n");
  fprintf(fout, "  i->_base.profile_ops = NULL;\n");
  fprintf(fout, "  i->loc.source = FbleNewString(arena, src);\n");
  fprintf(fout, "  i->loc.line = line;\n");
  fprintf(fout, "  i->loc.line = col;\n");
  fprintf(fout, "  i->exit = exit;\n");
  fprintf(fout, "  i->func.section = func_section;\n");
  fprintf(fout, "  i->func.index = func_index;\n");
  fprintf(fout, "  i->dest = dest;\n");
  fprintf(fout, "  FbleVectorInit(arena, i->args);\n");
  fprintf(fout, "  FbleFrameIndex arg = { .section = arg_section, .index = arg_index };\n");
  fprintf(fout, "  FbleVectorAppend(arena, i->args, arg);\n");
  fprintf(fout, "  FbleVectorAppend(arena, block->instrs, &i->_base);\n");
  fprintf(fout, "}\n\n");

  // Helper function for appending an access instruction, which has
  // caused problems taking up a lot of space in the past.
  fprintf(fout, "static void AppendAccessInstr(FbleArena* arena, FbleInstrTag itag, const char* src, size_t line, size_t col, FbleFrameSection obj_section, size_t obj_index, size_t tag, FbleLocalIndex dest, FbleInstrBlock* block)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleAccessInstr* i = FbleAlloc(arena, FbleAccessInstr);\n");
  fprintf(fout, "  i->_base.tag = itag;\n");
  fprintf(fout, "  i->_base.profile_ops = NULL;\n");
  fprintf(fout, "  i->loc.source = FbleNewString(arena, src);\n");
  fprintf(fout, "  i->loc.line = line;\n");
  fprintf(fout, "  i->loc.col = col;\n");
  fprintf(fout, "  i->obj.section = obj_section;\n");
  fprintf(fout, "  i->obj.index = obj_index;\n");
  fprintf(fout, "  i->tag = tag;\n");
  fprintf(fout, "  i->dest = dest;\n");
  fprintf(fout, "  FbleVectorAppend(arena, block->instrs, &i->_base);\n");
  fprintf(fout, "}\n\n");

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

  // Prevent warnings about unused helper functions.
  fprintf(fout, "  (void)AppendStruct0Instr;\n");
  fprintf(fout, "  (void)AppendStruct2Instr;\n");
  fprintf(fout, "  (void)AppendUnionInstr;\n");
  fprintf(fout, "  (void)AppendCall1Instr;\n");
  fprintf(fout, "  (void)AppendAccessInstr;\n");

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
