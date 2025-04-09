/**
 * @file code.c
 *  FbleCode related routines.
 */

#include "code.h"

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>      // for FbleAlloc, FbleFree, etc.
#include <fble/fble-function.h>   // for FbleExecutable
#include <fble/fble-vector.h>     // for FbleInitVector, etc.

#include "tc.h"
#include "unreachable.h"

static void PrintLoc(FILE* fout, FbleLoc loc);

// See documentation in code.h.
void* FbleRawAllocInstr(size_t size, FbleInstrTag tag)
{
  assert(sizeof(FbleInstr) <= size);
  FbleInstr* instr = FbleAllocRaw(size);
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
    case FBLE_REC_DECL_INSTR:
    case FBLE_RETURN_INSTR:
    case FBLE_TYPE_INSTR:
      FbleFree(instr);
      return;

    case FBLE_REC_DEFN_INSTR: {
      FbleRecDefnInstr* i = (FbleRecDefnInstr*)instr;
      for (size_t j = 0; j < i->locs.size; ++j) {
        FbleFreeLoc(i->locs.xs[j]);
      }
      FbleFreeVector(i->locs);
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

    case FBLE_TAIL_CALL_INSTR: {
      FbleTailCallInstr* call_instr = (FbleTailCallInstr*)instr;
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

    case FBLE_UNDEF_INSTR: {
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
  code->refcount = 1;
  code->magic = FBLE_CODE_MAGIC;
  code->executable.num_args = num_args;
  code->executable.num_statics = num_statics;
  code->executable.max_call_args = 0;
  code->executable.run = NULL;
  code->profile_block_id = profile_block_id;
  code->num_locals = num_locals;
  FbleInitVector(code->instrs);
  return code;
}

// See documentation in code.h.
void FbleFreeCode(FbleCode* code)
{
  if (code == NULL) {
    return;
  }

  // We've had trouble with double free in the past. Check to make sure the
  // magic in the block hasn't been corrupted. Otherwise we've probably
  // already freed this code and decrementing the refcount could end up
  // corrupting whatever is now making use of the memory that was previously
  // used for the instruction block.
  assert(code->magic == FBLE_CODE_MAGIC && "corrupt FbleCode");
  assert(code->refcount > 0);

  code->refcount--;
  if (code->refcount == 0) {
    for (size_t i = 0; i < code->instrs.size; ++i) {
      FbleFreeInstr(code->instrs.xs[i]);
    }
    FbleFreeVector(code->instrs);
    FbleFree(code);
  }
}

/**
 * @func[PrintLoc] Prints a location.
 *  For use in the diassembly output.
 *
 *  @arg[FILE*][fout] The stream to print to.
 *  @arg[FbleLoc][loc] The location to print.
 *
 *  @sideeffects
 *   Prints the location to the given output stream.
 */
static void PrintLoc(FILE* fout, FbleLoc loc)
{
  fprintf(fout, "  @ %s:%zi:%zi\n", loc.source->str, loc.line, loc.col);
}

// See documentation in fble-compile.h.
void FbleDisassemble(FILE* fout, FbleModule* module)
{
  assert(module->code != NULL && "module hasn't been compiled yet");

  // Map from FbleFrameSection to short descriptor of the source.
  static const char* var_tags[] = {"s", "a", "l"};

  fprintf(fout, "Module: ");
  FblePrintModulePath(fout, module->path);
  fprintf(fout, "\n");
  fprintf(fout, "Source: %s\n\n", module->path->loc.source->str);

  fprintf(fout, "Type Dependencies:\n");
  if (module->type_deps.size == 0) {
    fprintf(fout, "  (none)\n");
  }
  for (size_t i = 0; i < module->type_deps.size; ++i) {
    fprintf(fout, "  ");
    FblePrintModulePath(fout, module->type_deps.xs[i]);
    fprintf(fout, "\n");
  }
  fprintf(fout, "\n");

  fprintf(fout, "Link Dependencies:\n");
  if (module->link_deps.size == 0) {
    fprintf(fout, "  (none)\n");
  }
  for (size_t i = 0; i < module->link_deps.size; ++i) {
    fprintf(fout, "  ");
    FblePrintModulePath(fout, module->link_deps.xs[i]);
    fprintf(fout, "\n");
  }
  fprintf(fout, "\n");

  struct { size_t size; FbleCode** xs; } blocks;
  FbleInitVector(blocks);
  FbleAppendToVector(blocks, module->code);

  FbleNameV profile_blocks = module->profile_blocks;
  while (blocks.size > 0) {
    FbleCode* block = blocks.xs[--blocks.size];
    FbleName block_name = profile_blocks.xs[block->profile_block_id];
    fprintf(fout, "%s[%04zx]\n", block_name.name->str, block->profile_block_id);
    fprintf(fout, "  args: %zi, statics: %zi, max_call_args: %zi, locals: %zi\n",
        block->executable.num_args, block->executable.num_statics,
        block->executable.max_call_args, block->num_locals);
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
            FbleBlockId block_id = op->arg;
            FbleName* name = &profile_blocks.xs[block_id];
            fprintf(fout, "    .  profile enter %s[%04zx];", name->name->str, block_id);
            PrintLoc(fout, name->loc);
            break;
          }

          case FBLE_PROFILE_REPLACE_OP: {
            FbleBlockId block_id = op->arg;
            FbleName* name = &profile_blocks.xs[block_id];
            fprintf(fout, "    .  profile replace %s[%04zx];", name->name->str, block_id);
            PrintLoc(fout, name->loc);
            break;
          }

          case FBLE_PROFILE_EXIT_OP: {
            fprintf(fout, "    .  profile exit;\n");
            break;
          }

          case FBLE_PROFILE_SAMPLE_OP: {
            fprintf(fout, "    .  profile sample %zi;\n", op->arg);
            break;
          }
        }
      }

      switch (instr->tag) {
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
          fprintf(fout, "l%zi = union(%zi/%zi: %s%zi);\n",
              union_value_instr->dest,
              union_value_instr->tag, union_value_instr->tagwidth,
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
          FbleName func_name = profile_blocks.xs[func->profile_block_id];
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = func %s[%04zx] [",
              func_value_instr->dest,
              func_name.name->str, func->profile_block_id);
          const char* comma = "";
          for (size_t j = 0; j < func_value_instr->scope.size; ++j) {
            fprintf(fout, "%s%s%zi",
                comma, var_tags[func_value_instr->scope.xs[j].tag],
                func_value_instr->scope.xs[j].index);
            comma = ", ";
          }
          fprintf(fout, "]; +%zi\n", func_value_instr->profile_block_offset);
          FbleAppendToVector(blocks, func_value_instr->code);
          break;
        }

        case FBLE_CALL_INSTR: {
          FbleCallInstr* call_instr = (FbleCallInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = ", call_instr->dest);
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

        case FBLE_TAIL_CALL_INSTR: {
          FbleTailCallInstr* call_instr = (FbleTailCallInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "return ");
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

        case FBLE_REC_DECL_INSTR: {
          FbleRecDeclInstr* decl_instr = (FbleRecDeclInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = decl %zi;\n", decl_instr->dest, decl_instr->n);
          break;
        }

        case FBLE_REC_DEFN_INSTR: {
          FbleRecDefnInstr* defn_instr = (FbleRecDefnInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "defn l%zi = l%zi\n", defn_instr->decl, defn_instr->defn);
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

        case FBLE_UNDEF_INSTR: {
          FbleUndefInstr* undef_instr = (FbleUndefInstr*)instr;
          fprintf(fout, "%4zi.  ", i);
          fprintf(fout, "l%zi = undef;\n", undef_instr->dest);
          break;
        }
      }
    }
    fprintf(fout, "\n\n");
  }

  fprintf(fout, "Profile Blocks:\n");
  for (size_t i = 0; i < module->profile_blocks.size; ++i) {
    FbleName name = module->profile_blocks.xs[i];
    fprintf(fout, "  [%04zx] %s %s:%zi:%zi\n", i, name.name->str,
        name.loc.source->str, name.loc.line, name.loc.col);
  }

  FbleFreeVector(blocks);
}
