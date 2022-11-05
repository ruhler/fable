// code.c --
//   This file implements routines related to fble instructions.

#include "code.h"

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>   // for FbleAlloc, FbleFree, etc.
#include <fble/fble-vector.h>  // for FbleVectorInit, etc.

#include "tc.h"
#include "execute.h"
#include "interpret.h"

#define UNREACHABLE(x) assert(false && x)

static void OnFree(FbleExecutable* executable);
static void DumpCode(FILE* fout, FbleCode* code);

// OnFree --
//   The FbleExecutable.on_free function for FbleCode.
static void OnFree(FbleExecutable* executable)
{
  FbleCode* code = (FbleCode*)executable;
  for (size_t i = 0; i < code->instrs.size; ++i) {
    FbleFreeInstr(code->instrs.xs[i]);
  }
  FbleFree(code->instrs.xs);
}

// DumpCode -- 
//   For debugging purposes, dump the given code block in human readable
//   format to the given file.
//
// Inputs:
//   fout - where to dump the blocks to.
//   code - the code block to dump.
//
// Results:
//   none
//
// Side effects:
//   Prints the code block in human readable format to stderr.
static void DumpCode(FILE* fout, FbleCode* code)
{
  // Map from FbleFrameSection to short descriptor of the section.
  static const char* sections[] = {"s", "l"};

  FbleNameV profile_blocks = code->_base.profile_blocks;

  struct { size_t size; FbleCode** xs; } blocks;
  FbleVectorInit(blocks);
  FbleVectorAppend(blocks, code);

  while (blocks.size > 0) {
    FbleCode* block = blocks.xs[--blocks.size];
    FbleName block_name = profile_blocks.xs[block->_base.profile];
    fprintf(fout, "%s[%04zx] args[%zi] statics[%zi] locals[%zi]: // %s:%d:%d\n",
        block_name.name->str, block->_base.profile,
        block->_base.args, block->_base.statics, block->_base.locals,
        block_name.loc.source->str, block_name.loc.line, block_name.loc.col);
    for (size_t i = 0; i < block->instrs.size; ++i) {
      FbleInstr* instr = block->instrs.xs[i];

      for (FbleDebugInfo* info = instr->debug_info; info != NULL; info = info->next) {
        switch (info->tag) {
          case FBLE_STATEMENT_DEBUG_INFO: {
            FbleStatementDebugInfo* stmt = (FbleStatementDebugInfo*)info;
            fprintf(fout, "    .  stmt; // %s:%d:%d\n", 
                stmt->loc.source->str, stmt->loc.line, stmt->loc.col);
            break;
          }

          case FBLE_VAR_DEBUG_INFO: {
            FbleVarDebugInfo* var = (FbleVarDebugInfo*)info;
            fprintf(fout, "    .  var ");
            FblePrintName(fout, var->var);
            fprintf(fout, " %s%zi\n", sections[var->index.section], var->index.index);
            break;
          }
        }
      }

      for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
        switch (op->tag) {
          case FBLE_PROFILE_ENTER_OP: {
            FbleBlockId block = op->block;
            FbleName* name = &profile_blocks.xs[block];
            fprintf(fout, "    .  profile enter [%04zx]; ", block);
            fprintf(fout, "// %s[%04zx]: %s:%d:%d\n", name->name->str, block,
                name->loc.source->str, name->loc.line, name->loc.col);
            break;
          }

          case FBLE_PROFILE_REPLACE_OP: {
            FbleBlockId block = op->block;
            FbleName* name = &profile_blocks.xs[block];
            fprintf(fout, "    .  profile replace [%04zx]; ", block);
            fprintf(fout, "// %s[%04zx]: %s:%d:%d\n", name->name->str, block,
                name->loc.source->str, name->loc.line, name->loc.col);
            break;
          }

          case FBLE_PROFILE_EXIT_OP: {
            fprintf(fout, "    .  profile exit;\n");
            break;
          }
        }
      }

      fprintf(fout, "%4zi.  ", i);
      switch (instr->tag) {
        case FBLE_DATA_TYPE_INSTR: {
          FbleDataTypeInstr* data_type_instr = (FbleDataTypeInstr*)instr;
          fprintf(fout, "l%zi = %s(", data_type_instr->dest,
              data_type_instr->kind == FBLE_STRUCT_DATATYPE ? "*" : "+");
          const char* comma = "";
          for (size_t j = 0; j < data_type_instr->fields.size; ++j) {
            FbleFrameIndex field = data_type_instr->fields.xs[j];
            fprintf(fout, "%s%s%zi", comma, sections[field.section], field.index);
            comma = ", ";
          }
          fprintf(fout, ");\n");
          break;
        }

        case FBLE_STRUCT_VALUE_INSTR: {
          FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
          fprintf(fout, "l%zi = struct(", struct_value_instr->dest);
          const char* comma = "";
          for (size_t j = 0; j < struct_value_instr->args.size; ++j) {
            FbleFrameIndex arg = struct_value_instr->args.xs[j];
            fprintf(fout, "%s%s%zi", comma, sections[arg.section], arg.index);
            comma = ", ";
          }
          fprintf(fout, ");\n");
          break;
        }

        case FBLE_UNION_VALUE_INSTR: {
          FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
          fprintf(fout, "l%zi = union(%zi: %s%zi);\n",
              union_value_instr->dest,
              union_value_instr->tag,
              sections[union_value_instr->arg.section],
              union_value_instr->arg.index);
          break;
        }

        case FBLE_STRUCT_ACCESS_INSTR:
        case FBLE_UNION_ACCESS_INSTR: {
          // TODO: Include in the output whether this is struct/union
          FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
          fprintf(fout, "l%zi = %s%zi.%zi; // %s:%i:%i\n",
              access_instr->dest, sections[access_instr->obj.section],
              access_instr->obj.index,
              access_instr->tag, access_instr->loc.source->str,
              access_instr->loc.line, access_instr->loc.col);
          break;
        }

        case FBLE_UNION_SELECT_INSTR: {
          FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
          fprintf(fout, "pc += %s%zi.?(",
              sections[select_instr->condition.section],
              select_instr->condition.index);
          const char* comma = "";
          for (size_t i = 0; i < select_instr->jumps.size; ++i) {
            fprintf(fout, "%s%zi", comma, select_instr->jumps.xs[i]);
            comma = ", ";
          }
          fprintf(fout, ");  // %s:%i:%i\n",
              select_instr->loc.source->str, select_instr->loc.line,
              select_instr->loc.col);
          break;
        }

        case FBLE_JUMP_INSTR: {
          FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
          fprintf(fout, "jump +%zi;\n", jump_instr->count);
          break;
        }

        case FBLE_FUNC_VALUE_INSTR: {
          FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
          FbleCode* func = func_value_instr->code;
          FbleName func_name = profile_blocks.xs[func->_base.profile];
          fprintf(fout, "l%zi = func %s[%04zx] [",
              func_value_instr->dest,
              func_name.name->str, func->_base.profile);
          const char* comma = "";
          for (size_t j = 0; j < func_value_instr->scope.size; ++j) {
            fprintf(fout, "%s%s%zi",
                comma, sections[func_value_instr->scope.xs[j].section],
                func_value_instr->scope.xs[j].index);
            comma = ", ";
          }
          fprintf(fout, "];\n");
          FbleVectorAppend(blocks, func_value_instr->code);
          break;
        }

        case FBLE_CALL_INSTR: {
          FbleCallInstr* call_instr = (FbleCallInstr*)instr;
          if (call_instr->exit) {
            fprintf(fout, "return ");
          } else {
            fprintf(fout, "l%zi = ", call_instr->dest);
          }

          fprintf(fout, "%s%zi(",
              sections[call_instr->func.section],
              call_instr->func.index);

          const char* comma = "";
          for (size_t i = 0; i < call_instr->args.size; ++i) {
            fprintf(fout, "%s%s%zi", comma, 
              sections[call_instr->args.xs[i].section],
              call_instr->args.xs[i].index);
            comma = ", ";
          }
              
          fprintf(fout, "); // %s:%i:%i\n",
              call_instr->loc.source->str, call_instr->loc.line,
              call_instr->loc.col);
          break;
        }

        case FBLE_COPY_INSTR: {
          FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
          fprintf(fout, "l%zi = %s%zi;\n",
              copy_instr->dest,
              sections[copy_instr->source.section],
              copy_instr->source.index);
          break;
        }


        case FBLE_REF_VALUE_INSTR: {
          FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
          fprintf(fout, "l%zi = ref;\n", ref_instr->dest);
          break;
        }

        case FBLE_REF_DEF_INSTR: {
          FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
          fprintf(fout, "l%zi ~= %s%zi; // %s:%i:%i\n",
              ref_def_instr->ref,
              sections[ref_def_instr->value.section],
              ref_def_instr->value.index,
              ref_def_instr->loc.source->str, ref_def_instr->loc.line,
              ref_def_instr->loc.col);
          break;
        }

        case FBLE_RETURN_INSTR: {
          FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
          fprintf(fout, "return %s%zi;\n",
              sections[return_instr->result.section],
              return_instr->result.index);
          break;
        }

        case FBLE_TYPE_INSTR: {
          FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
          fprintf(fout, "l%zi = type;\n", type_instr->dest);
          break;
        }

        case FBLE_RELEASE_INSTR: {
          FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
          fprintf(fout, "release l%zi;\n", release_instr->target);
          break;
        }

        case FBLE_LIST_INSTR: {
          FbleListInstr* list_instr = (FbleListInstr*)instr;
          fprintf(fout, "l%zi = list(", list_instr->dest);
          const char* comma = "";
          for (size_t j = 0; j < list_instr->args.size; ++j) {
            FbleFrameIndex arg = list_instr->args.xs[j];
            fprintf(fout, "%s%s%zi", comma, sections[arg.section], arg.index);
            comma = ", ";
          }
          fprintf(fout, ");\n");
          break;
        }

        case FBLE_LITERAL_INSTR: {
          FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
          fprintf(fout, "l%zi = literal(", literal_instr->dest);
          const char* comma = "";
          for (size_t j = 0; j < literal_instr->letters.size; ++j) {
            fprintf(fout, "%s%zi", comma, literal_instr->letters.xs[j]);
            comma = ", ";
          }
          fprintf(fout, ");\n");
          break;
        }
      }
    }
    fprintf(fout, "\n\n");
  }

  FbleFree(blocks.xs);
}

// FbleRawAllocInstr -- see documentation in code.h
void* FbleRawAllocInstr(size_t size, FbleInstrTag tag)
{
  assert(sizeof(FbleInstr) <= size);
  FbleInstr* instr = FbleRawAlloc(size);
  instr->tag = tag;
  instr->debug_info = NULL;
  instr->profile_ops = NULL;
  return instr;
}

// FbleFreeDebugInfo -- see documentation in code.h
void FbleFreeDebugInfo(FbleDebugInfo* info)
{
  while (info != NULL) {
    FbleDebugInfo* next = info->next;
    switch (info->tag) {
      case FBLE_STATEMENT_DEBUG_INFO: {
        FbleStatementDebugInfo* stmt = (FbleStatementDebugInfo*)info;
        FbleFreeLoc(stmt->loc);
        break;
      }

      case FBLE_VAR_DEBUG_INFO: {
        FbleVarDebugInfo* var = (FbleVarDebugInfo*)info;
        FbleFreeName(var->var);
        break;
      }
    }
    FbleFree(info);
    info = next;
  }
}

// FbleFreeInstr -- see documentation in code.h
void FbleFreeInstr(FbleInstr* instr)
{
  assert(instr != NULL);
  FbleFreeDebugInfo(instr->debug_info);

  while (instr->profile_ops != NULL) {
    FbleProfileOp* op = instr->profile_ops;
    instr->profile_ops = op->next;
    FbleFree(op);
  }

  switch (instr->tag) {
    case FBLE_UNION_VALUE_INSTR:
    case FBLE_JUMP_INSTR:
    case FBLE_COPY_INSTR:
    case FBLE_REF_VALUE_INSTR:
    case FBLE_RETURN_INSTR:
    case FBLE_TYPE_INSTR:
    case FBLE_RELEASE_INSTR:
      FbleFree(instr);
      return;

    case FBLE_REF_DEF_INSTR: {
      FbleRefDefInstr* i = (FbleRefDefInstr*)instr;
      FbleFreeLoc(i->loc);
      FbleFree(instr);
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR:
    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* i = (FbleAccessInstr*)instr;
      FbleFreeLoc(i->loc);
      FbleFree(instr);
      return;
    }

    case FBLE_DATA_TYPE_INSTR: {
      FbleDataTypeInstr* dt_instr = (FbleDataTypeInstr*)instr;
      FbleFree(dt_instr->fields.xs);
      FbleFree(instr);
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      FbleFree(struct_instr->args.xs);
      FbleFree(instr);
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
      FbleFreeLoc(select_instr->loc);
      FbleFree(select_instr->jumps.xs);
      FbleFree(instr);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
      FbleFreeCode(func_value_instr->code);
      FbleFree(func_value_instr->scope.xs);
      FbleFree(func_value_instr);
      return;
    }

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      FbleFreeLoc(call_instr->loc);
      FbleFree(call_instr->args.xs);
      FbleFree(instr);
      return;
    }

    case FBLE_LIST_INSTR: {
      FbleListInstr* list_instr = (FbleListInstr*)instr;
      FbleFree(list_instr->args.xs);
      FbleFree(instr);
      return;
    }

    case FBLE_LITERAL_INSTR: {
      FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
      FbleFree(literal_instr->letters.xs);
      FbleFree(instr);
      return;
    }
  }

  UNREACHABLE("invalid instruction");
}

// FbleNewCode -- see documentation in code.h
FbleCode* FbleNewCode(size_t args, size_t statics, size_t locals, FbleBlockId profile)
{
  FbleCode* code = FbleAlloc(FbleCode);
  code->_base.refcount = 1;
  code->_base.magic = FBLE_EXECUTABLE_MAGIC;
  code->_base.args = args;
  code->_base.statics = statics;
  code->_base.locals = locals;
  code->_base.profile = profile;
  FbleVectorInit(code->_base.profile_blocks);
  code->_base.run = &FbleInterpreterRunFunction;
  code->_base.on_free = &OnFree;
  FbleVectorInit(code->instrs);
  return code;
}

// FbleFreeCode -- see documentation in code.h
void FbleFreeCode(FbleCode* code)
{
  FbleFreeExecutable(&code->_base);
}

// FbleDisassmeble -- see documentation in fble-compile.h.
void FbleDisassemble(FILE* fout, FbleCode* code)
{
  DumpCode(fout, code);
}
