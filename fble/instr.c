// instr.c --
//   This file implements routines related to fble instructions.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf
#include <stdlib.h>   // for NULL

#include "instr.h"

#define UNREACHABLE(x) assert(false && x)

static void DumpInstrBlock(FILE* fout, FbleInstrBlock* code, FbleProfile* profile);

// DumpInstrBlock -- 
//   For debugging purposes, dump the given code block in human readable
//   format to the given file.
//
// Inputs:
//   fout - where to dump the blocks to.
//   code - the code block to dump.
//   profile - the profile, for getting names of profiling blocks.
//
// Results:
//   none
//
// Side effects:
//   Prints the code block in human readable format to stderr.
static void DumpInstrBlock(FILE* fout, FbleInstrBlock* code, FbleProfile* profile)
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

      for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
        switch (op->tag) {
          case FBLE_PROFILE_ENTER_OP: {
            FbleBlockId block = op->block;
            FbleName* name = &profile->blocks.xs[block]->name;
            fprintf(fout, "    .  profile enter [%04x]; ", block);
            fprintf(fout, "// %s[%04x]: %s:%d:%d\n", name->name->str, block,
                name->loc.source->str, name->loc.line, name->loc.col);
            break;
          }

          case FBLE_PROFILE_EXIT_OP: {
            fprintf(fout, "    .  profile exit;\n");
            break;
          }

          case FBLE_PROFILE_AUTO_EXIT_OP: {
            fprintf(fout, "    .  profile auto exit;\n");
            break;
          }
        }
      }

      fprintf(fout, "%4zi.  ", i);
      switch (instr->tag) {
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
          fprintf(fout, "l%zi = func %p [",
              func_value_instr->dest,
              (void*)func_value_instr->code);
          const char* comma = "";
          for (size_t j = 0; j < func_value_instr->scope.size; ++j) {
            fprintf(fout, "%s%s%zi",
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
          fprintf(fout, "release l%zi;\n", release->value);
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

        case FBLE_GET_INSTR: {
          FbleGetInstr* get_instr = (FbleGetInstr*)instr;
          fprintf(fout, "l%zi := get(%s%zi);\n",
              get_instr->dest,
              sections[get_instr->port.section],
              get_instr->port.index);
          break;
        }

        case FBLE_PUT_INSTR: {
          FblePutInstr* put_instr = (FblePutInstr*)instr;
          fprintf(fout, "l%zi := put(%s%zi, %s%zi);\n",
              put_instr->dest,
              sections[put_instr->port.section],
              put_instr->port.index,
              sections[put_instr->arg.section],
              put_instr->arg.index);
          break;
        }

        case FBLE_LINK_INSTR: {
          FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
          fprintf(fout, "l%zi, l%zi = link;\n", link_instr->get, link_instr->put);
          break;
        }

        case FBLE_FORK_INSTR: {
          FbleForkInstr* fork_instr = (FbleForkInstr*)instr;
          fprintf(fout, "fork [");
          const char* comma = "";
          for (size_t j = 0; j < fork_instr->dests.size; ++j) {
            fprintf(fout, "%sl%zi := %s%zi",
                comma, fork_instr->dests.xs[j],
                sections[fork_instr->args.xs[j].section],
                fork_instr->args.xs[j].index);
            comma = ", ";
          }
          fprintf(fout, "];\n");
          break;
        }

        case FBLE_REF_VALUE_INSTR: {
          FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
          fprintf(fout, "l%zi = ref;\n", ref_instr->dest);
          break;
        }

        case FBLE_REF_DEF_INSTR: {
          FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
          fprintf(fout, "l%zi ~= %s%zi;\n",
              ref_def_instr->ref,
              sections[ref_def_instr->value.section],
              ref_def_instr->value.index);
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

        case FBLE_SYMBOLIC_VALUE_INSTR: {
          FbleSymbolicValueInstr* symbolic_value_instr = (FbleSymbolicValueInstr*)instr;
          fprintf(fout, "l%zi = symbolic;\n", symbolic_value_instr->dest);
          break;
        }

        case FBLE_SYMBOLIC_COMPILE_INSTR: {
          FbleSymbolicCompileInstr* compile_instr = (FbleSymbolicCompileInstr*)instr;
          fprintf(fout, "l%zi = compile \\", compile_instr->dest);
          const char* space = "";
          for (size_t j = 0; j < compile_instr->args.size; ++j) {
            fprintf(fout, "%s%s%zi",
                space, sections[compile_instr->args.xs[j].section],
                compile_instr->args.xs[j].index);
            space = " ";
          }
          fprintf(fout, " -> %s%zi;\n", 
              sections[compile_instr->body.section],
              compile_instr->body.index);
          break;
        }
      }
    }
    fprintf(fout, "\n\n");
  }

  FbleFree(arena, blocks.xs);
  FbleFreeArena(arena);
}

// FbleFreeInstr -- see documentation in instr.h
void FbleFreeInstr(FbleArena* arena, FbleInstr* instr)
{
  assert(instr != NULL);
  while (instr->profile_ops != NULL) {
    FbleProfileOp* op = instr->profile_ops;
    instr->profile_ops = op->next;
    FbleFree(arena, op);
  }

  switch (instr->tag) {
    case FBLE_UNION_VALUE_INSTR:
    case FBLE_JUMP_INSTR:
    case FBLE_RELEASE_INSTR:
    case FBLE_COPY_INSTR:
    case FBLE_GET_INSTR:
    case FBLE_PUT_INSTR:
    case FBLE_LINK_INSTR:
    case FBLE_REF_VALUE_INSTR:
    case FBLE_REF_DEF_INSTR:
    case FBLE_RETURN_INSTR:
    case FBLE_TYPE_INSTR:
    case FBLE_SYMBOLIC_VALUE_INSTR:
      FbleFree(arena, instr);
      return;

    case FBLE_STRUCT_ACCESS_INSTR:
    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* i = (FbleAccessInstr*)instr;
      FbleFreeLoc(arena, i->loc);
      FbleFree(arena, instr);
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      FbleFree(arena, struct_instr->args.xs);
      FbleFree(arena, instr);
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
      FbleFreeLoc(arena, select_instr->loc);
      FbleFree(arena, select_instr->jumps.xs);
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

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      FbleFreeLoc(arena, call_instr->loc);
      FbleFree(arena, call_instr->args.xs);
      FbleFree(arena, instr);
      return;
    }

    case FBLE_FORK_INSTR: {
      FbleForkInstr* fork_instr = (FbleForkInstr*)instr;
      FbleFree(arena, fork_instr->args.xs);
      FbleFree(arena, fork_instr->dests.xs);
      FbleFree(arena, instr);
      return;
    }

    case FBLE_SYMBOLIC_COMPILE_INSTR: {
      FbleSymbolicCompileInstr* compile_instr = (FbleSymbolicCompileInstr*)instr;
      FbleFree(arena, compile_instr->args.xs);
      FbleFree(arena, instr);
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

  // We've had trouble with double free of instr blocks in the past. Check to
  // make sure the magic in the block hasn't been corrupted. Otherwise we've
  // probably already freed this instruction block and decrementing the
  // refcount could end up corrupting whatever is now making use of the memory
  // that was previously used for the instruction block.
  assert(block->magic == FBLE_INSTR_BLOCK_MAGIC && "corrupt FbleInstrBlock");

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

// FbleDisassmeble -- see documentation in fble.h.
void FbleDisassemble(FILE* fout, FbleCompiledProgram* program, FbleProfile* profile)
{
  DumpInstrBlock(fout, program->code, profile);
}
