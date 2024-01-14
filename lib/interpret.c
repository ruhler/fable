/**
 * @file interpret.c
 *  Interpreter for FbleCode fble bytecode.
 */

#include "interpret.h"

#include <assert.h>   // for assert
#include <stdlib.h>   // for rand

#include <fble/fble-alloc.h>    // for FbleAlloc, FbleFree
#include <fble/fble-function.h> // For FbleFunction, etc.
#include <fble/fble-vector.h>   // for FbleInitVector, etc.

#include "code.h"
#include "unreachable.h"

static FbleValue* RunAbort(FbleValueHeap* heap, FbleCode* code, FbleValue*** vars, size_t pc);

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
static FbleValue* RunAbort(FbleValueHeap* heap, FbleCode* code, FbleValue*** vars, size_t pc)
{
  FbleValue** locals = vars[FBLE_LOCAL_VAR];
  while (true) {
    assert(pc < code->instrs.size && "Missing return instruction?");
    FbleInstr* instr = code->instrs.xs[pc];
    switch (instr->tag) {
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
        pc = select_instr->default_;
        break;
      }

      case FBLE_GOTO_INSTR: {
        FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
        pc = goto_instr->target;
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
        locals[call_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_TAIL_CALL_INSTR: {
        FbleTailCallInstr* call_instr = (FbleTailCallInstr*)instr;

        FbleReleaseValue(heap, GET(call_instr->func));
        for (size_t i = 0; i < call_instr->args.size; ++i) {
          FbleReleaseValue(heap, GET(call_instr->args.xs[i]));
        }

        return NULL;
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
        FbleReleaseValue(heap, locals[return_instr->result.index]);
        return NULL;
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
        locals[type_instr->dest] = NULL;
        pc++;
        break;
      }

      case FBLE_RETAIN_INSTR: {
        FbleRetainInstr* retain_instr = (FbleRetainInstr*)instr;
        FbleValue* target = GET(retain_instr->target);
        FbleRetainValue(heap, target);
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

      case FBLE_NOP_INSTR: {
        pc++;
        break;
      }
    }
  }

  FbleUnreachable("Should never get here");
  return NULL;
}

// See documentation in interpret.h.
FbleValue* FbleInterpreterRunFunction(
    FbleValueHeap* heap,
    FbleProfileThread* profile,
    FbleValue** tail_call_buffer,
    FbleFunction* function,
    FbleValue** args)
{
  FbleCode* code = (FbleCode*)function->executable;
  FbleInstr** instrs = code->instrs.xs;
  FbleValue* locals[code->num_locals];

  FbleValue** vars[3];
  vars[FBLE_STATIC_VAR] = function->statics;
  vars[FBLE_ARG_VAR] = args;
  vars[FBLE_LOCAL_VAR] = locals;

  FbleBlockId profile_block_offset = function->profile_block_offset;

  size_t pc = 0;
  while (true) {
    FbleInstr* instr = instrs[pc];

    if (profile) {
      for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
        switch (op->tag) {
          case FBLE_PROFILE_ENTER_OP:
            FbleProfileEnterBlock(profile, profile_block_offset + op->arg);
            break;

          case FBLE_PROFILE_REPLACE_OP:
            FbleProfileReplaceBlock(profile, profile_block_offset + op->arg);
            break;

          case FBLE_PROFILE_EXIT_OP:
            FbleProfileExitBlock(profile);
            break;

          case FBLE_PROFILE_SAMPLE_OP:
            FbleProfileRandomSample(profile, op->arg);
            break;
        }
      }
    }

    switch (instr->tag) {
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

        FbleValue* obj = GET(access_instr->obj);
        locals[access_instr->dest] = FbleStructValueField(obj, access_instr->tag);

        if (locals[access_instr->dest] == NULL) {
          FbleReportError("undefined struct value access\n", access_instr->loc);
          return RunAbort(heap, code, vars, pc);
        }

        pc++;
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleValue* obj = GET(access_instr->obj);
        locals[access_instr->dest] = FbleUnionValueField(obj, access_instr->tag);

        if (locals[access_instr->dest] == NULL) {
          FbleReportError("undefined union value access\n", access_instr->loc);
          return RunAbort(heap, code, vars, pc);
        }

        if (locals[access_instr->dest] == FbleWrongUnionTag) {
          locals[access_instr->dest] = NULL;
          FbleReportError("union field access undefined: wrong tag\n", access_instr->loc);
          return RunAbort(heap, code, vars, pc);
        }

        pc++;
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
        FbleValue* obj = GET(select_instr->condition);
        size_t tag = FbleUnionValueTag(obj);

        if (tag == (size_t)(-1)) {
          FbleReportError("undefined union value select\n", select_instr->loc);
          return RunAbort(heap, code, vars, pc);
        }

        // Binary search for the matching tag.
        assert(select_instr->targets.size > 0);
        size_t target = select_instr->default_;
        size_t lo = 0;
        size_t hi = select_instr->targets.size;
        while (lo < hi) {
          size_t mid = (lo + hi)/2;
          size_t mid_tag = select_instr->targets.xs[mid].tag;
          if (tag == mid_tag) {
            target = select_instr->targets.xs[mid].target;
            break;
          }
          if (tag < mid_tag) {
            hi = mid;
          } else {
            lo = mid + 1;
          }
        }

        pc = target;
        break;
      }

      case FBLE_GOTO_INSTR: {
        FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
        pc = goto_instr->target;
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
        FbleValue* func = GET(call_instr->func);
        FbleValue* call_args[call_instr->args.size];
        for (size_t i = 0; i < call_instr->args.size; ++i) {
          call_args[i] = GET(call_instr->args.xs[i]);
        }

        locals[call_instr->dest] = FbleCall(heap, profile, func, call_instr->args.size, call_args);
        if (locals[call_instr->dest] == NULL) {
          FbleReportError("callee aborted\n", call_instr->loc);
          return RunAbort(heap, code, vars, pc);
        }

        pc++;
        break;
      }

      case FBLE_TAIL_CALL_INSTR: {
        FbleTailCallInstr* call_instr = (FbleTailCallInstr*)instr;
        FbleFunction* func = FbleFuncValueFunction(GET(call_instr->func));
        if (func == NULL) {
          FbleReportError("called undefined function\n", call_instr->loc);
          return RunAbort(heap, code, vars, pc);
        };

        tail_call_buffer[0] = GET(call_instr->func);
        for (size_t i = 0; i < call_instr->args.size; ++i) {
          tail_call_buffer[1+i] = GET(call_instr->args.xs[i]);
        }
        tail_call_buffer[call_instr->args.size+1] = NULL;
        return FbleTailCallSentinelValue;
      }

      case FBLE_COPY_INSTR: {
        FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
        locals[copy_instr->dest] = GET(copy_instr->source);
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
          return RunAbort(heap, code, vars, pc);
        }

        pc++;
        break;
      }

      case FBLE_RETURN_INSTR: {
        FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
        return GET(return_instr->result);
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
        locals[type_instr->dest] = FbleGenericTypeValue;
        pc++;
        break;
      }

      case FBLE_RETAIN_INSTR: {
        FbleRetainInstr* retain_instr = (FbleRetainInstr*)instr;
        FbleValue* target = GET(retain_instr->target);
        FbleRetainValue(heap, target);
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

      case FBLE_NOP_INSTR: {
        pc++;
        break;
      }
    }
  }
}
