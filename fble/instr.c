// instr.c --
//   This file implements routines related to fble instructions.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf
#include <stdlib.h>   // for NULL

#include "instr.h"

#define UNREACHABLE(x) assert(false && x)

static void DumpInstrBlock(FILE* fout, FbleInstrBlock* code, FbleNameV* profile_blocks);

// DumpInstrBlock -- 
//   For debugging purposes, dump the given code block in human readable
//   format to the given file.
//
// Inputs:
//   fout - where to dump the blocks to.
//   code - the code block to dump.
//   profile_blocks - the names of the profiling blocks.
//
// Results:
//   none
//
// Side effects:
//   Prints the code block in human readable format to stderr.
static void DumpInstrBlock(FILE* fout, FbleInstrBlock* code, FbleNameV* profile_blocks)
{
  // Map from FbleFrameSection to short descriptor of the section.
  static const char* sections[] = {"s", "l"};

  FbleArena* arena = FbleNewArena();
  struct { size_t size; FbleInstrBlock** xs; } blocks;
  FbleVectorInit(arena, blocks);
  FbleVectorAppend(arena, blocks, code);

  while (blocks.size > 0) {
    FbleInstrBlock* block = blocks.xs[--blocks.size];
    fprintf(fout, "%p statics[%zi] locals[%zi]:\n",
        (void*)block, block->statics, block->locals);
    for (size_t i = 0; i < block->instrs.size; ++i) {
      FbleInstr* instr = block->instrs.xs[i];
      fprintf(fout, "%4zi.  ", i);
      switch (instr->tag) {
        case FBLE_STRUCT_VALUE_INSTR: {
          FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
          fprintf(fout, "l[%zi] = struct(", struct_value_instr->dest);
          const char* comma = "";
          for (size_t j = 0; j < struct_value_instr->args.size; ++j) {
            FbleFrameIndex arg = struct_value_instr->args.xs[j];
            fprintf(fout, "%s%s[%zi]", comma, sections[arg.section], arg.index);
            comma = ", ";
          }
          fprintf(fout, ");\n");
          break;
        }

        case FBLE_UNION_VALUE_INSTR: {
          FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
          fprintf(fout, "l[%zi] = union(%s[%zi]);\n",
              union_value_instr->dest,
              sections[union_value_instr->arg.section],
              union_value_instr->arg.index);
          break;
        }

        case FBLE_STRUCT_ACCESS_INSTR:
        case FBLE_UNION_ACCESS_INSTR: {
          FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
          fprintf(fout, "l[%zi] = %s[%zi].%zi; // %s:%i:%i\n",
              access_instr->dest, sections[access_instr->obj.section],
              access_instr->obj.index,
              access_instr->tag, access_instr->loc.source,
              access_instr->loc.line, access_instr->loc.col);
          break;
        }

        case FBLE_UNION_SELECT_INSTR: {
          FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
          fprintf(fout, "goto +%s[%zi].tag; // %s:%i:%i\n",
              sections[select_instr->condition.section],
              select_instr->condition.index,
              select_instr->loc.source, select_instr->loc.line,
              select_instr->loc.col);
          break;
        }

        case FBLE_GOTO_INSTR: {
          FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
          fprintf(fout, "goto %zi;\n", goto_instr->pc);
          break;
        }

        case FBLE_FUNC_VALUE_INSTR: {
          FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
          fprintf(fout, "l[%zi] = func %p [",
              func_value_instr->dest,
              (void*)func_value_instr->code);
          const char* comma = "";
          for (size_t j = 0; j < func_value_instr->scope.size; ++j) {
            fprintf(fout, "%s%s[%zi]",
                comma, sections[func_value_instr->scope.xs[j].section],
                func_value_instr->scope.xs[j].index);
            comma = ", ";
          }
          fprintf(fout, "] %zi;\n", func_value_instr->argc);
          FbleVectorAppend(arena, blocks, func_value_instr->code);
          break;
        }

        case FBLE_RELEASE_INSTR: {
          FbleReleaseInstr* release = (FbleReleaseInstr*)instr;
          fprintf(fout, "release l[%zi];\n", release->value);
          break;
        }

        case FBLE_FUNC_APPLY_INSTR: {
          FbleFuncApplyInstr* func_apply_instr = (FbleFuncApplyInstr*)instr;
          if (func_apply_instr->exit) {
            fprintf(fout, "$");
          } else {
            fprintf(fout, "l[%zi]", func_apply_instr->dest);
          }

          fprintf(fout, " = %s[%zi](%s[%zi]); // %s:%i:%i\n",
              sections[func_apply_instr->func.section],
              func_apply_instr->func.index,
              sections[func_apply_instr->arg.section],
              func_apply_instr->arg.index,
              func_apply_instr->loc.source, func_apply_instr->loc.line,
              func_apply_instr->loc.col);
          break;
        }

        case FBLE_PROC_VALUE_INSTR: {
          FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
          fprintf(fout, "l[%zi] = proc %p [",
              proc_value_instr->dest,
              (void*)proc_value_instr->code);
          const char* comma = "";
          for (size_t j = 0; j < proc_value_instr->scope.size; ++j) {
            fprintf(fout, "%s%s[%zi]",
                comma, sections[proc_value_instr->scope.xs[j].section],
                proc_value_instr->scope.xs[j].index);
            comma = ", ";
          }
          fprintf(fout, "];\n");
          FbleVectorAppend(arena, blocks, proc_value_instr->code);
          break;
        }

        case FBLE_COPY_INSTR: {
          FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
          fprintf(fout, "l[%zi] = %s[%zi];\n",
              copy_instr->dest,
              sections[copy_instr->source.section],
              copy_instr->source.index);
          break;
        }

        case FBLE_GET_INSTR: {
          fprintf(fout, "return get(s[0]);\n");
          break;
        }

        case FBLE_PUT_INSTR: {
          fprintf(fout, "return put(s[0], s[1]);\n");
          break;
        }

        case FBLE_LINK_INSTR: {
          FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
          fprintf(fout, "l[%zi], l[%zi] = link;\n", link_instr->get, link_instr->put);
          break;
        }

        case FBLE_FORK_INSTR: {
          FbleForkInstr* fork_instr = (FbleForkInstr*)instr;
          fprintf(fout, "fork [");
          const char* comma = "";
          for (size_t j = 0; j < fork_instr->dests.size; ++j) {
            fprintf(fout, "%sl[%zi] := %s[%zi]",
                comma, fork_instr->dests.xs[j],
                sections[fork_instr->args.xs[j].section],
                fork_instr->args.xs[j].index);
            comma = ", ";
          }
          fprintf(fout, "];\n");
          break;
        }

        case FBLE_JOIN_INSTR: {
          fprintf(fout, "join;\n");
          break;
        }

        case FBLE_PROC_INSTR: {
          FbleProcInstr* proc_instr = (FbleProcInstr*)instr;
          if (proc_instr->exit) {
            fprintf(fout, "$");
          } else {
            fprintf(fout, "l[%zi]", proc_instr->dest);
          }
          fprintf(fout, " := %s[%zi];\n",
              sections[proc_instr->proc.section],
              proc_instr->proc.index);
          break;
        }

        case FBLE_REF_VALUE_INSTR: {
          FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
          fprintf(fout, "l[%zi] = ref;\n", ref_instr->dest);
          break;
        }

        case FBLE_REF_DEF_INSTR: {
          FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
          fprintf(fout, "l[%zi] ~= %s[%zi];\n",
              ref_def_instr->ref,
              sections[ref_def_instr->value.section],
              ref_def_instr->value.index);
          break;
        }

        case FBLE_RETURN_INSTR: {
          FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
          fprintf(fout, "$ = %s[%zi];\n",
              sections[return_instr->result.section],
              return_instr->result.index);
          break;
        }

        case FBLE_TYPE_INSTR: {
          FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
          fprintf(fout, "l[%zi] = type;\n", type_instr->dest);
          break;
        }

        case FBLE_PROFILE_ENTER_BLOCK_INSTR: {
          FbleProfileEnterBlockInstr* enter = (FbleProfileEnterBlockInstr*)instr;
          FbleName* name = profile_blocks->xs + enter->block;
          fprintf(fout, "enter [%04x]; ", enter->block);
          fprintf(fout, "// %s[%04x]: %s:%d:%d\n",
              name->name, enter->block,
              name->loc.source, name->loc.line, name->loc.col);
          break;
        }

        case FBLE_PROFILE_EXIT_BLOCK_INSTR: {
          fprintf(fout, "exit block;\n");
          break;
        }

        case FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR: {
          fprintf(fout, "auto exit block;\n");
          break;
        }
      }
    }
    fprintf(fout, "\n\n");
  }

  FbleFree(arena, blocks.xs);
  FbleAssertEmptyArena(arena);
  FbleFreeArena(arena);
}

// FbleFreeInstr -- see documentation in instr.h
void FbleFreeInstr(FbleArena* arena, FbleInstr* instr)
{
  assert(instr != NULL);
  switch (instr->tag) {
    case FBLE_UNION_VALUE_INSTR:
    case FBLE_STRUCT_ACCESS_INSTR:
    case FBLE_UNION_ACCESS_INSTR:
    case FBLE_UNION_SELECT_INSTR:
    case FBLE_GOTO_INSTR:
    case FBLE_RELEASE_INSTR:
    case FBLE_FUNC_APPLY_INSTR:
    case FBLE_COPY_INSTR:
    case FBLE_GET_INSTR:
    case FBLE_PUT_INSTR:
    case FBLE_LINK_INSTR:
    case FBLE_PROC_INSTR:
    case FBLE_JOIN_INSTR:
    case FBLE_REF_VALUE_INSTR:
    case FBLE_REF_DEF_INSTR:
    case FBLE_RETURN_INSTR:
    case FBLE_TYPE_INSTR:
    case FBLE_PROFILE_ENTER_BLOCK_INSTR:
    case FBLE_PROFILE_EXIT_BLOCK_INSTR:
    case FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR: {
      FbleFree(arena, instr);
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      FbleFree(arena, struct_instr->args.xs);
      FbleFree(arena, instr);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
      FbleFreeInstrBlock(arena, func_value_instr->code);
      FbleFree(arena, func_value_instr->scope.xs);
      FbleFree(arena, func_value_instr);
      return;
    }

    case FBLE_FORK_INSTR: {
      FbleForkInstr* fork_instr = (FbleForkInstr*)instr;
      FbleFree(arena, fork_instr->args.xs);
      FbleFree(arena, fork_instr->dests.xs);
      FbleFree(arena, instr);
      return;
    }

    case FBLE_PROC_VALUE_INSTR: {
      FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
      FbleFreeInstrBlock(arena, proc_value_instr->code);
      FbleFree(arena, proc_value_instr->scope.xs);
      FbleFree(arena, proc_value_instr);
      return;
    }
  }

  UNREACHABLE("invalid instruction");
}

// FbleFreeInstrBlock -- see documentation in instr.h
void FbleFreeInstrBlock(FbleArena* arena, FbleInstrBlock* block)
{
  if (block == NULL) {
    return;
  }

  assert(block->refcount > 0);
  block->refcount--;
  if (block->refcount == 0) {
    for (size_t i = 0; i < block->instrs.size; ++i) {
      FbleFreeInstr(arena, block->instrs.xs[i]);
    }
    FbleFree(arena, block->instrs.xs);
    FbleFree(arena, block);
  }
}

// FbleDecompile -- see documentation in fble.h.
bool FbleDecompile(FILE* fout, FbleProgram* program)
{
  FbleArena* arena = FbleNewArena();
  FbleNameV blocks;
  FbleInstrBlock* code = FbleCompile(arena, &blocks, program);
  if (code != NULL) {
    DumpInstrBlock(fout, code, &blocks);
    FbleFreeInstrBlock(arena, code);
  }
  FbleFreeBlockNames(arena, &blocks);
  FbleAssertEmptyArena(arena);
  FbleFreeArena(arena);
  return code != NULL;
}
