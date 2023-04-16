/**
 * @file code.c
 * FbleCode related routines.
 */

#include "code.h"

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>   // for FbleAlloc, FbleFree, etc.
#include <fble/fble-execute.h>
#include <fble/fble-vector.h>  // for FbleVectorInit, etc.

#include "tc.h"
#include "interpret.h"
#include "unreachable.h"

static void OnFree(FbleExecutable* executable);
static void PrintLoc(FILE* fout, FbleLoc loc);

/**
 * FbleExecutable.on_free function for FbleCode.
 *
 * @param executable  The FbleCode to free state from.
 *
 * @sideeffects
 *   Frees FbleCode specific fields.
 */
static void OnFree(FbleExecutable* executable)
{
  FbleCode* code = (FbleCode*)executable;
  for (size_t i = 0; i < code->instrs.size; ++i) {
    FbleFreeInstr(code->instrs.xs[i]);
  }
  FbleFreeVector(code->instrs);
}

// See documentation in code.h.
void* FbleRawAllocInstr(size_t size, FbleInstrTag tag)
{
  assert(sizeof(FbleInstr) <= size);
  FbleInstr* instr = FbleRawAlloc(size);
  instr->tag = tag;
  instr->debug_info = NULL;
  instr->profile_ops = NULL;
  return instr;
}

// See documentation in code.h.
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
        FbleFreeName(var->name);
        break;
      }
    }
    FbleFree(info);
    info = next;
  }
}

// See documentation in code.h.
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
    case FBLE_GOTO_INSTR:
    case FBLE_COPY_INSTR:
    case FBLE_REF_VALUE_INSTR:
    case FBLE_RETURN_INSTR:
    case FBLE_TYPE_INSTR:
      FbleFree(instr);
      return;

    case FBLE_RELEASE_INSTR: {
      FbleReleaseInstr* i = (FbleReleaseInstr*)instr;
      FbleFreeVector(i->targets);
      FbleFree(instr);
      return;
    }

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
      FbleFreeVector(dt_instr->fields);
      FbleFree(instr);
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      FbleFreeVector(struct_instr->args);
      FbleFree(instr);
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
      FbleFreeLoc(select_instr->loc);
      FbleFreeVector(select_instr->targets);
      FbleFree(instr);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
      FbleFreeCode(func_value_instr->code);
      FbleFreeVector(func_value_instr->scope);
      FbleFree(func_value_instr);
      return;
    }

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      FbleFreeLoc(call_instr->loc);
      FbleFreeVector(call_instr->args);
      FbleFree(instr);
      return;
    }

    case FBLE_LIST_INSTR: {
      FbleListInstr* list_instr = (FbleListInstr*)instr;
      FbleFreeVector(list_instr->args);
      FbleFree(instr);
      return;
    }

    case FBLE_LITERAL_INSTR: {
      FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
      FbleFreeVector(literal_instr->letters);
      FbleFree(instr);
      return;
    }

    case FBLE_NOP_INSTR: {
      FbleFree(instr);
      return;
    }
  }

  FbleUnreachable("invalid instruction");
}

// See documentation in code.h.
FbleCode* FbleNewCode(size_t num_args, size_t num_statics, size_t num_locals, FbleBlockId profile_block_id)
{
  FbleCode* code = FbleAlloc(FbleCode);
  code->_base.refcount = 1;
  code->_base.magic = FBLE_EXECUTABLE_MAGIC;
  code->_base.num_args = num_args;
  code->_base.num_statics = num_statics;
  code->_base.profile_block_id = profile_block_id;
  code->_base.run = &FbleInterpreterRunFunction;
  code->_base.on_free = &OnFree;
  code->num_locals = num_locals;
  FbleVectorInit(code->instrs);
  return code;
}

// See documentation in code.h.
void FbleFreeCode(FbleCode* code)
{
  FbleFreeExecutable(&code->_base);
}

/**
 * Prints a location.
 *
 * For use in the diassembly output.
 *
 * @param fout  The stream to print to.
 * @param loc  The location to print.
 */
static void PrintLoc(FILE* fout, FbleLoc loc)
{
  fprintf(fout, "  @ %s:%i:%i\n", loc.source->str, loc.line, loc.col);
}

// See documentation in fble-compile.h.
void FbleDisassemble(FILE* fout, FbleCompiledModule* module)
{
  // Map from FbleFrameSection to short descriptor of the source.
  static const char* var_tags[] = {"s", "a", "l"};

  fprintf(fout, "Module: ");
  FblePrintModulePath(fout, module->path);
  fprintf(fout, "\n");
  fprintf(fout, "Source: %s\n\n", module->path->loc.source->str);

  fprintf(fout, "Dependencies:\n");
  if (module->deps.size == 0) {
    fprintf(fout, "  (none)\n");
  }
  for (size_t i = 0; i < module->deps.size; ++i) {
    fprintf(fout, "  ");
    FblePrintModulePath(fout, module->deps.xs[i]);
    fprintf(fout, "\n");
  }

  fprintf(fout, "\n");

  struct { size_t size; FbleCode** xs; } blocks;
  FbleVectorInit(blocks);
  FbleVectorAppend(blocks, module->code);

  FbleNameV profile_blocks = module->profile_blocks;
  while (blocks.size > 0) {
    FbleCode* block = blocks.xs[--blocks.size];
    FbleName block_name = profile_blocks.xs[block->_base.profile_block_id];
    fprintf(fout, "%s[%04zx] args[%zi] statics[%zi] locals[%zi]",
        block_name.name->str, block->_base.profile_block_id,
        block->_base.num_args, block->_base.num_statics, block->num_locals);
    PrintLoc(fout, block_name.loc);
    for (size_t i = 0; i < block->instrs.size; ++i) {
      FbleInstr* instr = block->instrs.xs[i];

      for (FbleDebugInfo* info = instr->debug_info; info != NULL; info = info->next) {
        switch (info->tag) {
          case FBLE_STATEMENT_DEBUG_INFO: {
            FbleStatementDebugInfo* stmt = (FbleStatementDebugInfo*)info;
            fprintf(fout, "    .  stmt;");
            PrintLoc(fout, stmt->loc);
            break;
          }

          case FBLE_VAR_DEBUG_INFO: {
            FbleVarDebugInfo* var = (FbleVarDebugInfo*)info;
            fprintf(fout, "    .  var ");
            FblePrintName(fout, var->name);
            fprintf(fout, " %s%zi\n", var_tags[var->var.tag], var->var.index);
            break;
          }
        }
      }

      for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
        switch (op->tag) {
          case FBLE_PROFILE_ENTER_OP: {
            FbleBlockId block_id = op->block;
            FbleName* name = &profile_blocks.xs[block_id];
            fprintf(fout, "    .  profile enter %s[%04zx];", name->name->str, block_id);
            PrintLoc(fout, name->loc);
            break;
          }

          case FBLE_PROFILE_REPLACE_OP: {
            FbleBlockId block_id = op->block;
            FbleName* name = &profile_blocks.xs[block_id];
            fprintf(fout, "    .  profile replace %s[%04zx];", name->name->str, block_id);
            PrintLoc(fout, name->loc);
            break;
          }

          case FBLE_PROFILE_EXIT_OP: {
            fprintf(fout, "    .  profile exit;\n");
            break;
          }
        }
      }

      switch (instr->tag) {
        case FBLE_DATA_TYPE_INSTR: {
          FbleDataTypeInstr* data_type_instr = (FbleDataTypeInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = %s(", data_type_instr->dest,
              data_type_instr->kind == FBLE_STRUCT_DATATYPE ? "*" : "+");
          const char* comma = "";
          for (size_t j = 0; j < data_type_instr->fields.size; ++j) {
            FbleVar field = data_type_instr->fields.xs[j];
            fprintf(fout, "%s%s%zi", comma, var_tags[field.tag], field.index);
            comma = ", ";
          }
          fprintf(fout, ");\n");
          break;
        }

        case FBLE_STRUCT_VALUE_INSTR: {
          FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = struct(", struct_value_instr->dest);
          const char* comma = "";
          for (size_t j = 0; j < struct_value_instr->args.size; ++j) {
            FbleVar arg = struct_value_instr->args.xs[j];
            fprintf(fout, "%s%s%zi", comma, var_tags[arg.tag], arg.index);
            comma = ", ";
          }
          fprintf(fout, ");\n");
          break;
        }

        case FBLE_UNION_VALUE_INSTR: {
          FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = union(%zi: %s%zi);\n",
              union_value_instr->dest,
              union_value_instr->tag,
              var_tags[union_value_instr->arg.tag],
              union_value_instr->arg.index);
          break;
        }

        case FBLE_STRUCT_ACCESS_INSTR:
        case FBLE_UNION_ACCESS_INSTR: {
          // TODO: Include in the output whether this is struct/union
          FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = %s%zi.%zi;",
              access_instr->dest, var_tags[access_instr->obj.tag],
              access_instr->obj.index,
              access_instr->tag);
          PrintLoc(fout, access_instr->loc);
          break;
        }

        case FBLE_UNION_SELECT_INSTR: {
          FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "goto %s%zi.?(",
              var_tags[select_instr->condition.tag],
              select_instr->condition.index);
          const char* comma = "";
          for (size_t j = 0; j < select_instr->targets.size; ++j) {
            fprintf(fout, "%s%zi: %zi", comma,
                select_instr->targets.xs[j].tag,
                select_instr->targets.xs[j].target);
            comma = ", ";
          }
          fprintf(fout, "%s: %zi of %zi", comma,
              select_instr->default_, select_instr->num_tags);
          fprintf(fout, ");");
          PrintLoc(fout, select_instr->loc);
          break;
        }

        case FBLE_GOTO_INSTR: {
          FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "goto %zi;\n", goto_instr->target);
          break;
        }

        case FBLE_FUNC_VALUE_INSTR: {
          FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
          FbleCode* func = func_value_instr->code;
          FbleName func_name = profile_blocks.xs[func->_base.profile_block_id];
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = func %s[%04zx] [",
              func_value_instr->dest,
              func_name.name->str, func->_base.profile_block_id);
          const char* comma = "";
          for (size_t j = 0; j < func_value_instr->scope.size; ++j) {
            fprintf(fout, "%s%s%zi",
                comma, var_tags[func_value_instr->scope.xs[j].tag],
                func_value_instr->scope.xs[j].index);
            comma = ", ";
          }
          fprintf(fout, "];\n");
          FbleVectorAppend(blocks, func_value_instr->code);
          break;
        }

        case FBLE_CALL_INSTR: {
          FbleCallInstr* call_instr = (FbleCallInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          if (call_instr->exit) {
            fprintf(fout, "return ");
          } else {
            fprintf(fout, "l%zi = ", call_instr->dest);
          }

          fprintf(fout, "%s%zi(",
              var_tags[call_instr->func.tag],
              call_instr->func.index);

          const char* comma = "";
          for (size_t j = 0; j < call_instr->args.size; ++j) {
            fprintf(fout, "%s%s%zi", comma, 
              var_tags[call_instr->args.xs[j].tag],
              call_instr->args.xs[j].index);
            comma = ", ";
          }
              
          fprintf(fout, ");");
          PrintLoc(fout, call_instr->loc);
          break;
        }

        case FBLE_COPY_INSTR: {
          FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = %s%zi;\n",
              copy_instr->dest,
              var_tags[copy_instr->source.tag],
              copy_instr->source.index);
          break;
        }


        case FBLE_REF_VALUE_INSTR: {
          FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = ref;\n", ref_instr->dest);
          break;
        }

        case FBLE_REF_DEF_INSTR: {
          FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi ~= %s%zi;",
              ref_def_instr->ref,
              var_tags[ref_def_instr->value.tag],
              ref_def_instr->value.index);
          PrintLoc(fout, ref_def_instr->loc);
          break;
        }

        case FBLE_RETURN_INSTR: {
          FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "return %s%zi;\n",
              var_tags[return_instr->result.tag],
              return_instr->result.index);
          break;
        }

        case FBLE_TYPE_INSTR: {
          FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = type;\n", type_instr->dest);
          break;
        }

        case FBLE_RELEASE_INSTR: {
          FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "release ");
          const char* comma = "";
          for (size_t j = 0; j < release_instr->targets.size; ++j) {
            fprintf(fout, "%sl%zi", comma, release_instr->targets.xs[j]);
            comma = ", ";
          }
          fprintf(fout, ";\n");
          break;
        }

        case FBLE_LIST_INSTR: {
          FbleListInstr* list_instr = (FbleListInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = list(", list_instr->dest);
          const char* comma = "";
          for (size_t j = 0; j < list_instr->args.size; ++j) {
            FbleVar arg = list_instr->args.xs[j];
            fprintf(fout, "%s%s%zi", comma, var_tags[arg.tag], arg.index);
            comma = ", ";
          }
          fprintf(fout, ");\n");
          break;
        }

        case FBLE_LITERAL_INSTR: {
          FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = literal(", literal_instr->dest);
          const char* comma = "";
          for (size_t j = 0; j < literal_instr->letters.size; ++j) {
            fprintf(fout, "%s%zi", comma, literal_instr->letters.xs[j]);
            comma = ", ";
          }
          fprintf(fout, ");\n");
          break;
        }

        case FBLE_NOP_INSTR: {
          fprintf(fout, "%4zi.  nop;\n", i);
          break;
        }
      }
    }
    fprintf(fout, "\n\n");
  }

  fprintf(fout, "Profile Blocks:\n");
  for (size_t i = 0; i < module->profile_blocks.size; ++i) {
    FbleName name = module->profile_blocks.xs[i];
    fprintf(fout, "  [%04zx] %s %s:%d:%d\n", i, name.name->str,
        name.loc.source->str, name.loc.line, name.loc.col);
  }

  FbleFreeVector(blocks);
}
