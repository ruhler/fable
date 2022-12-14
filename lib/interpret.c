/**
 * @file interpret.c
 * Interpreter for FbleCode fble bytecode.
 */

#include <fble/fble-interpret.h>
#include "interpret.h"

#include <assert.h>   // for assert
#include <stdlib.h>   // for rand

#include <fble/fble-alloc.h>   // for FbleAlloc, FbleFree
#include <fble/fble-execute.h>
#include <fble/fble-vector.h>  // for FbleVectorInit, etc.

#include "code.h"
#include "unreachable.h"
#include "value.h"

static FbleValue* RunAbort(FbleValueHeap* heap, FbleCode* code, FbleValue** locals, size_t pc);


/**
 * Aborts a function.
 *
 * To abort a stack frame we execute the remaining instructions in the stack
 * frame only as much as necessary to release any live local variables. We
 * don't do new allocations or function calls as part of this execution.
 *
 * While aborting, any value normally expected to be allocated may be set to
 * NULL.
 *
 * @param heap  The value heap.
 * @param code  The code for the function.
 * @param locals  The function's local variables.
 * @param pc  The pc to start aborting from.
 *
 * @return NULL for convenience.
 *
 * @sideeffects
 * Cleans up the function's local variables.
 */
static FbleValue* RunAbort(FbleValueHeap* heap, FbleCode* code, FbleValue** locals, size_t pc)
{
  while (true) {
    assert(pc < code->instrs.size && "Missing return instruction?");
    FbleInstr* instr = code->instrs.xs[pc];
    switch (instr->tag) {
      case FBLE_DATA_TYPE_INSTR: {
        FbleDataTypeInstr* data_type_instr = (FbleDataTypeInstr*)instr;
        locals[data_type_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        locals[struct_value_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        locals[union_value_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        locals[access_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        locals[access_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        // For the purposes of abort, it doesn't matter which branch we take,
        // because all branches have to clean up memory the same way.
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
        assert(select_instr->jumps.size > 0);
        pc += 1 + select_instr->jumps.xs[0];
        break;
      }

      case FBLE_JUMP_INSTR: {
        FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
        pc += 1 + jump_instr->count;
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        locals[func_value_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_CALL_INSTR: {
        FbleCallInstr* call_instr = (FbleCallInstr*)instr;

        if (call_instr->exit) {
          if (call_instr->func.tag == FBLE_LOCAL_VAR) {
            FbleReleaseValue(heap, locals[call_instr->func.index]);

            // Set function to NULL so it's safe to release it again if the function
            // is also one of the arguments.
            locals[call_instr->func.index] = NULL;
          }

          for (size_t i = 0; i < call_instr->args.size; ++i) {
            if (call_instr->args.xs[i].tag == FBLE_LOCAL_VAR) {
              FbleReleaseValue(heap, locals[call_instr->args.xs[i].index]);

              // Set the arg to NULL so it's safe to release it again if the
              // arg is used more than once.
              locals[call_instr->args.xs[i].index] = NULL;
            }
          }

          return NULL;
        }

        locals[call_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_COPY_INSTR: {
        FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
        locals[copy_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_REF_VALUE_INSTR: {
        FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
        locals[ref_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_REF_DEF_INSTR: {
        pc++;
        break;
      }

      case FBLE_RETURN_INSTR: {
        FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
        switch (return_instr->result.tag) {
          case FBLE_STATIC_VAR: break;
          case FBLE_ARG_VAR: break;
          case FBLE_LOCAL_VAR: {
            FbleReleaseValue(heap, locals[return_instr->result.index]);
            break;
          }
        }

        return NULL;
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
        locals[type_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_RELEASE_INSTR: {
        FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
        for (size_t i = 0; i < release_instr->targets.size; ++i) {
          FbleReleaseValue(heap, locals[release_instr->targets.xs[i]]);
        }
        pc++;
        break;
      }

      case FBLE_LIST_INSTR: {
        FbleListInstr* list_instr = (FbleListInstr*)instr;
        locals[list_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_LITERAL_INSTR: {
        FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
        locals[literal_instr->dest] = NULL;
        pc++;
        break;
      }
    }
  }

  FbleUnreachable("Should never get here");
  return NULL;
}

/**
 * Gets the value of a variable in scope.
 *
 * Assumes existance of a local vars variable:
 *   FbleValue** vars[3];
 *   vars[FBLE_STATIC_VAR] = statics;
 *   vars[FBLE_ARG_VAR] = args;
 *   vars[FBLE_LOCAL_VAR] = locals;
 *
 * @param idx  The index of the variable.
 *
 * @returns
 *   The value of the variable.
 *
 * @sideeffects
 *   None.
 */
#define GET(idx) (vars[idx.tag][idx.index])

/**
 * Gets the strict value of a variable in scope.
 *
 * Assumes existance of a local vars variable:
 *   FbleValue** vars[3];
 *   vars[FBLE_STATIC_VAR] = statics;
 *   vars[FBLE_ARG_VAR] = args;
 *   vars[FBLE_LOCAL_VAR] = locals;
 *
 * @param idx  The index of the variable.
 *
 * @returns
 *   The FbleStrctValue of the variable.
 *
 * @sideeffects
 *   None.
 */
#define GET_STRICT(idx) FbleStrictValue(GET(idx))

// See documentation in interpret.h.
FbleValue* FbleInterpreterRunFunction(
    FbleValueHeap* heap,
    FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** args,
    FbleValue** statics,
    FbleBlockId profile_block_offset)
{
  FbleCode* code = (FbleCode*)executable;
  FbleInstr** instrs = code->instrs.xs;
  FbleValue* locals[code->num_locals];

  FbleValue** vars[3];
  vars[FBLE_STATIC_VAR] = statics;
  vars[FBLE_ARG_VAR] = args;
  vars[FBLE_LOCAL_VAR] = locals;

  size_t pc = 0;
  while (true) {
    FbleInstr* instr = instrs[pc];
    FbleThreadSample(thread);

    for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
      switch (op->tag) {
        case FBLE_PROFILE_ENTER_OP:
          FbleThreadEnterBlock(thread, profile_block_offset + op->block);
          break;

        case FBLE_PROFILE_REPLACE_OP:
          FbleThreadReplaceBlock(thread, profile_block_offset + op->block);
          break;

        case FBLE_PROFILE_EXIT_OP:
          FbleThreadExitBlock(thread);
          break;
      }
    }

    switch (instr->tag) {
      case FBLE_DATA_TYPE_INSTR: {
        FbleDataTypeInstr* data_type_instr = (FbleDataTypeInstr*)instr;
        size_t fieldc = data_type_instr->fields.size;
        FbleValue* fields[fieldc];
        for (size_t i = 0; i < fieldc; ++i) {
          fields[i] = GET(data_type_instr->fields.xs[i]);
        }

        locals[data_type_instr->dest] = FbleNewDataTypeValue(heap, data_type_instr->kind, fieldc, fields);
        pc++;
        break;
      }

      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_value_instr->args.size;
        FbleValue* struct_args[argc];
        for (size_t i = 0; i < argc; ++i) {
          struct_args[i] = GET(struct_value_instr->args.xs[i]);
        }

        locals[struct_value_instr->dest] = FbleNewStructValue(heap, argc, struct_args);
        pc++;
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        size_t tag = union_value_instr->tag;
        FbleValue* arg = GET(union_value_instr->arg);
        locals[union_value_instr->dest] = FbleNewUnionValue(heap, tag, arg);
        pc++;
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleValue* sv = GET_STRICT(access_instr->obj);
        if (sv == NULL) {
          FbleReportError("undefined struct value access\n", access_instr->loc);
          return RunAbort(heap, code, locals, pc);
        }

        FbleValue* v = FbleStructValueAccess(sv, access_instr->tag);
        FbleRetainValue(heap, v);
        locals[access_instr->dest] = v;
        pc++;
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleValue* uv = GET_STRICT(access_instr->obj);
        if (uv == NULL) {
          FbleReportError("undefined union value access\n", access_instr->loc);
          return RunAbort(heap, code, locals, pc);
        }

        if (FbleUnionValueTag(uv) != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", access_instr->loc);
          return RunAbort(heap, code, locals, pc);
        }

        FbleValue* v = FbleUnionValueAccess(uv);
        FbleRetainValue(heap, v);
        locals[access_instr->dest] = v;
        pc++;
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
        FbleValue* uv = GET_STRICT(select_instr->condition);
        if (uv == NULL) {
          FbleReportError("undefined union value select\n", select_instr->loc);
          return RunAbort(heap, code, locals, pc);
        }
        pc += 1 + select_instr->jumps.xs[FbleUnionValueTag(uv)];
        break;
      }

      case FBLE_JUMP_INSTR: {
        FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
        pc += 1 + jump_instr->count;
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        FbleValue* func_statics[func_value_instr->scope.size];
        for (size_t i = 0; i < func_value_instr->scope.size; ++i) {
          func_statics[i] = GET(func_value_instr->scope.xs[i]);
        }

        locals[func_value_instr->dest] = FbleNewFuncValue(heap, &func_value_instr->code->_base, profile_block_offset, func_statics);
        pc++;
        break;
      }

      case FBLE_CALL_INSTR: {
        FbleCallInstr* call_instr = (FbleCallInstr*)instr;
        FbleValue* func = GET_STRICT(call_instr->func);
        if (func == NULL) {
          FbleReportError("called undefined function\n", call_instr->loc);
          return RunAbort(heap, code, locals, pc);
        };

        FbleExecutable* func_exe = FbleFuncValueInfo(func).executable;
        FbleValue* call_args[func_exe->num_args];
        for (size_t i = 0; i < func_exe->num_args; ++i) {
          call_args[i] = GET(call_instr->args.xs[i]);
        }

        if (call_instr->exit) {
          FbleRetainValue(heap, func);
          for (size_t i = 0; i < func_exe->num_args; ++i) {
            FbleRetainValue(heap, call_args[i]);
          }

          if (call_instr->func.tag == FBLE_LOCAL_VAR) {
            FbleReleaseValue(heap, locals[call_instr->func.index]);
            locals[call_instr->func.index] = NULL;
          }

          for (size_t i = 0; i < call_instr->args.size; ++i) {
            if (call_instr->args.xs[i].tag == FBLE_LOCAL_VAR) {
              FbleReleaseValue(heap, locals[call_instr->args.xs[i].index]);
              locals[call_instr->args.xs[i].index] = NULL;
            }
          }

          return FbleThreadTailCall(heap, thread, func, call_args);
        }

        pc++;
        locals[call_instr->dest] = FbleThreadCall(heap, thread, func, call_args);
        if (locals[call_instr->dest] == NULL) {
          return RunAbort(heap, code, locals, pc);
        }
        break;
      }

      case FBLE_COPY_INSTR: {
        FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
        FbleValue* value = GET(copy_instr->source);
        FbleRetainValue(heap, value);
        locals[copy_instr->dest] = value;
        pc++;
        break;
      }

      case FBLE_REF_VALUE_INSTR: {
        FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
        locals[ref_instr->dest] = FbleNewRefValue(heap);
        pc++;
        break;
      }

      case FBLE_REF_DEF_INSTR: {
        FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
        FbleValue* ref = locals[ref_def_instr->ref];
        FbleValue* value = GET(ref_def_instr->value);
        if (!FbleAssignRefValue(heap, ref, value)) {
          FbleReportError("vacuous value\n", ref_def_instr->loc);
          return RunAbort(heap, code, locals, pc);
        }

        pc++;
        break;
      }

      case FBLE_RETURN_INSTR: {
        FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
        FbleValue* result = NULL;
        switch (return_instr->result.tag) {
          case FBLE_STATIC_VAR: {
            result = statics[return_instr->result.index];
            FbleRetainValue(heap, result);
            break;
          }

          case FBLE_ARG_VAR: {
            result = args[return_instr->result.index];
            FbleRetainValue(heap, result);
            break;
          }

          case FBLE_LOCAL_VAR: {
            result = locals[return_instr->result.index];
            break;
          }
        }

        return result;
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
        locals[type_instr->dest] = FbleGenericTypeValue;
        pc++;
        break;
      }

      case FBLE_RELEASE_INSTR: {
        FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
        for (size_t i = 0; i < release_instr->targets.size; ++i) {
          FbleReleaseValue(heap, locals[release_instr->targets.xs[i]]);
        }
        pc++;
        break;
      }

      case FBLE_LIST_INSTR: {
        FbleListInstr* list_instr = (FbleListInstr*)instr;
        size_t argc = list_instr->args.size;
        FbleValue* list_args[argc];
        for (size_t i = 0; i < argc; ++i) {
          list_args[i] = GET(list_instr->args.xs[i]);
        }

        locals[list_instr->dest] = FbleNewListValue(heap, argc, list_args);
        pc++;
        break;
      }

      case FBLE_LITERAL_INSTR: {
        FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
        locals[literal_instr->dest] = FbleNewLiteralValue(heap, literal_instr->letters.size, literal_instr->letters.xs);
        pc++;
        break;
      }
    }
  }
}

// See documentation in fble-interpret.h.
FbleExecutableProgram* FbleInterpret(FbleCompiledProgram* program)
{
  FbleExecutableProgram* executable = FbleAlloc(FbleExecutableProgram);
  FbleVectorInit(executable->modules);

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleCompiledModule* module = program->modules.xs[i];

    FbleExecutableModule* executable_module = FbleAlloc(FbleExecutableModule);
    executable_module->refcount = 1;
    executable_module->magic = FBLE_EXECUTABLE_MODULE_MAGIC;
    executable_module->path = FbleCopyModulePath(module->path);
    FbleVectorInit(executable_module->deps);
    for (size_t d = 0; d < module->deps.size; ++d) {
      FbleVectorAppend(executable_module->deps, FbleCopyModulePath(module->deps.xs[d]));
    }

    executable_module->executable = &module->code->_base;
    executable_module->executable->refcount++;

    FbleVectorInit(executable_module->profile_blocks);
    for (size_t n = 0; n < module->profile_blocks.size; ++n) {
      FbleVectorAppend(executable_module->profile_blocks,
          FbleCopyName(module->profile_blocks.xs[n]));
    }

    FbleVectorAppend(executable->modules, executable_module);
  }

  return executable;
}
