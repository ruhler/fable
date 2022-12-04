
#include <fble/fble-interpret.h>
#include "interpret.h"

#include <assert.h>   // for assert
#include <stdlib.h>   // for rand

#include <fble/fble-alloc.h>   // for FbleAlloc, FbleFree
#include <fble/fble-execute.h>
#include <fble/fble-vector.h>  // for FbleVectorInit, etc.

#include "code.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// PROFILE_SAMPLE_PERIOD --
//   The approximate number of instructions to execute before taking another
//   profiling sample.
#define PROFILE_SAMPLE_PERIOD 1024

static FbleValue* FrameGet(FbleThread* thread, FbleFrameIndex index);
static FbleValue* FrameGetStrict(FbleThread* thread, FbleFrameIndex index);
static void FrameSetBorrowed(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value);
static void FrameSetConsumed(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value);

// Control --
//   Used to control the interpreter loop from executed instructions.
typedef enum {
  RETURN,        // Break out of the interpreter loop.
  CONTINUE       // Continue executing instructions.
} Control;

// RunInstr --
//   A function that executes an instruction.
//
// Inputs:
//   heap - heap to use for allocations.
//   thread - the current thread state.
//   instr - the instruction to execute.
//   status - set to the exec status to return if result is RETURN.
//   pc - the program counter.
//
// Results:
// * CONTINUE to indicate the interpreter instruction loop should continue
//   executing instructions from this function.
// * RETURN to indicate the interpreter instruction loop should return
//   'status'.
//
// Side effects:
// * Executes the instruction.
// * Updates pc to point to appropriate next value.
typedef Control (*RunInstr)(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc);

// AbortInstr --
//   A function that executes an instruction for the purposes of aborting.
//
// To abort a stack frame we execute the remaining instructions in the stack
// frame only as much as necessary to release any live local variables. We
// don't do new allocations or function calls as part of this execution.
//
// While aborting, any value normally expected to be allocated may be set to
// NULL.
//
// Inputs:
//   heap - heap to use for allocations.
//   stack - the stack frame to abort.
//   instr - the instruction to execute.
//   pc - the program counter.
//
// Results:
// * RETURN - if the function is done aborting.
// * CONTINUE - if there are more instructions in the function to execute.
//
// Side effects:
// * Executes the instruction for the purposes of aborting the stack frame.
// * Updates pc to point to appropriate next value.
typedef Control (*AbortInstr)(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc);

// FrameGet --
//   Get a value from the frame on the top of the execution stack.
//
// Inputs:
//   thread - the current thread.
//   index - the index of the value to access.
//
// Results:
//   The value in the top stack frame at the given index.
//
// Side effects:
//   None.
static FbleValue* FrameGet(FbleThread* thread, FbleFrameIndex index)
{
  switch (index.section) {
    case FBLE_STATICS_FRAME_SECTION: return FbleFuncValueStatics(thread->stack->func)[index.index];
    case FBLE_LOCALS_FRAME_SECTION: return thread->stack->locals[index.index];
  }
  UNREACHABLE("should never get here");
}

// FrameGetStrict --
//   Get and dereference a value from the frame at the top of the given stack.
//   Dereferences the data value, removing all layers of ref values
//   until a non-ref value is encountered and returns the non-reference
//   value.
//
// Inputs:
//   thread - the current thread.
//   index - the location of the value in the frame.
//
// Results:
//   The dereferenced value. Returns null in case of abstract value or
//   unevaluated ref dereference.
//
// Side effects:
//   The returned value will only stay alive as long as the original value on
//   the stack frame.
static FbleValue* FrameGetStrict(FbleThread* thread, FbleFrameIndex index)
{
  return FbleStrictValue(FrameGet(thread, index));
}

// FrameSetBorrowed -- 
//   Store a value onto the frame on the top of the stack without consuming
//   ownership of the value.
//
// Inputs:
//   heap - the value heap.
//   thread - the current thread.
//   index - the index of the value to set.
//   value - the value to set. Borrowed.
//
// Side effects:
//   Sets the value at the given index in the frame.
static void FrameSetBorrowed(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value)
{
  FbleRetainValue(heap, value);
  thread->stack->locals[index] = value;
}

// FrameSetConsumed --
//   Store a value onto the frame on the top of the stack.
//
//   The caller should hold a strong reference to the value that will be
//   transfered to the stack.
//
// Inputs:
//   heap - the value heap.
//   thread - the current thread.
//   index - the index of the value to set.
//   value - the value to set. Consumed.
//
// Side effects:
//   Sets the value at the given index in the frame, taking over strong
//   reference ownership of the value.
static void FrameSetConsumed(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value)
{
  thread->stack->locals[index] = value;
}

// RunDataTypeInstr -- see documentation of RunInstr.
//   Execute an FBLE_DATA_TYPE_INSTR.
static Control RunDataTypeInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleDataTypeInstr* data_type_instr = (FbleDataTypeInstr*)instr;
  size_t fieldc = data_type_instr->fields.size;
  FbleValue* fields[fieldc];
  for (size_t i = 0; i < fieldc; ++i) {
    fields[i] = FrameGet(thread, data_type_instr->fields.xs[i]);
  }

  FbleValue* value = FbleNewDataTypeValue(heap, data_type_instr->kind, fieldc, fields);
  FrameSetConsumed(heap, thread, data_type_instr->dest, value);
  (*pc)++;
  return CONTINUE;
}

// AbortDataTypeInstr -- see documentation of AbortInstr.
//   Execute a FBLE_DATA_TYPE_INSTR for abort.
static Control AbortDataTypeInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleDataTypeInstr* data_type_instr = (FbleDataTypeInstr*)instr;
  stack->locals[data_type_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// RunStructValueInstr -- see documentation of RunInstr.
//   Execute a STRUCT_VALUE_INSTR.
static Control RunStructValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
  size_t argc = struct_value_instr->args.size;
  FbleValue* args[argc];
  for (size_t i = 0; i < argc; ++i) {
    args[i] = FrameGet(thread, struct_value_instr->args.xs[i]);
  }

  FbleValue* value = FbleNewStructValue(heap, argc, args);
  FrameSetConsumed(heap, thread, struct_value_instr->dest, value);
  (*pc)++;
  return CONTINUE;
}

// AbortStructValueInstr -- see documentation of AbortInstr.
//   Execute a STRUCT_VALUE_INSTR for abort.
static Control AbortStructValueInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
  stack->locals[struct_value_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// RunUnionValueInstr -- see documentation of RunInstr
//   Execute a UNION_VALUE_INSTR.
static Control RunUnionValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
  size_t tag = union_value_instr->tag;
  FbleValue* arg = FrameGet(thread, union_value_instr->arg);
  FbleValue* value = FbleNewUnionValue(heap, tag, arg);
  FrameSetConsumed(heap, thread, union_value_instr->dest, value);
  (*pc)++;
  return CONTINUE;
}

// AbortUnionValueInstr -- see documentation of AbortInstr
//   Execute a UNION_VALUE_INSTR for abort.
static Control AbortUnionValueInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
  stack->locals[union_value_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// RunStructAccessInstr -- see documentation of RunInstr
//   Execute a STRUCT_ACCESS_INSTR.
static Control RunStructAccessInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

  FbleValue* sv = FrameGetStrict(thread, access_instr->obj);
  if (sv == NULL) {
    FbleReportError("undefined struct value access\n", access_instr->loc);
    *status = FBLE_EXEC_ABORTED;
    return RETURN;
  }

  FbleValue* v = FbleStructValueAccess(sv, access_instr->tag);
  FrameSetBorrowed(heap, thread, access_instr->dest, v);
  (*pc)++;
  return CONTINUE;
}

// AbortStructAccessInstr -- see documentation of AbortInstr
//   Execute a STRUCT_ACCESS_INSTR for abort.
static Control AbortStructAccessInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
  stack->locals[access_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// RunUnionAccessInstr -- see documentation of RunInstr
//   Execute a UNION_ACCESS_INSTR.
static Control RunUnionAccessInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

  FbleValue* uv = FrameGetStrict(thread, access_instr->obj);
  if (uv == NULL) {
    FbleReportError("undefined union value access\n", access_instr->loc);
    *status = FBLE_EXEC_ABORTED;
    return RETURN;
  }

  if (FbleUnionValueTag(uv) != access_instr->tag) {
    FbleReportError("union field access undefined: wrong tag\n", access_instr->loc);
    *status = FBLE_EXEC_ABORTED;
    return RETURN;
  }

  FrameSetBorrowed(heap, thread, access_instr->dest, FbleUnionValueAccess(uv));
  (*pc)++;
  return CONTINUE;
}

// AbortUnionAccessInstr -- see documentation of AbortInstr
//   Execute a UNION_ACCESS_INSTR.
static Control AbortUnionAccessInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
  stack->locals[access_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// RunUnionSelectInstr -- see documentation of RunInstr
//   Execute a UNION_SELECT_INSTR.
static Control RunUnionSelectInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
  FbleValue* uv = FrameGetStrict(thread, select_instr->condition);
  if (uv == NULL) {
    FbleReportError("undefined union value select\n", select_instr->loc);
    *status = FBLE_EXEC_ABORTED;
    return RETURN;
  }
  *pc += 1 + select_instr->jumps.xs[FbleUnionValueTag(uv)];
  return CONTINUE;
}

// AbortUnionSelectInstr -- see documentation of AbortInstr
//   Execute a UNION_SELECT_INSTR for abort.
static Control AbortUnionSelectInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  // For the purposes of abort, it doesn't matter which branch we take,
  // because all branches have to clean up memory the same way.
  FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
  assert(select_instr->jumps.size > 0);
  *pc += 1 + select_instr->jumps.xs[0];
  return CONTINUE;
}

// RunJumpInstr -- see documentation of RunInstr
//   Execute a JUMP_INSTR.
static Control RunJumpInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
  *pc += 1 + jump_instr->count;
  return CONTINUE;
}

// AbortJumpInstr -- see documentation of AbortInstr
//   Execute a JUMP_INSTR for abort.
static Control AbortJumpInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
  *pc += 1 + jump_instr->count;
  return CONTINUE;
}

// RunFuncValueInstr -- see documentation of RunInstr
//   Execute a FUNC_VALUE_INSTR.
static Control RunFuncValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
  FbleValue* statics[func_value_instr->scope.size];
  for (size_t i = 0; i < func_value_instr->scope.size; ++i) {
    statics[i] = FrameGet(thread, func_value_instr->scope.xs[i]);
  }

  FbleValue* value = FbleNewFuncValue(heap, &func_value_instr->code->_base, FbleFuncValueProfileBaseId(thread->stack->func), statics);
  FrameSetConsumed(heap, thread, func_value_instr->dest, value);
  (*pc)++;
  return CONTINUE;
}

// AbortFuncValueInstr -- see documentation of AbortInstr
//   Execute a FUNC_VALUE_INSTR for abort.
static Control AbortFuncValueInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
  stack->locals[func_value_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// RunCallInstr -- see documentation of RunInstr
//   Execute a CALL_INSTR.
static Control RunCallInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleCallInstr* call_instr = (FbleCallInstr*)instr;
  FbleValue* func = FrameGetStrict(thread, call_instr->func);
  if (func == NULL) {
    FbleReportError("called undefined function\n", call_instr->loc);
    *status = FBLE_EXEC_ABORTED;
    return RETURN;
  };

  FbleExecutable* executable = FbleFuncValueExecutable(func);
  FbleValue* args[executable->num_args];
  for (size_t i = 0; i < executable->num_args; ++i) {
    args[i] = FrameGet(thread, call_instr->args.xs[i]);
  }

  if (call_instr->exit) {
    FbleRetainValue(heap, func);
    for (size_t i = 0; i < executable->num_args; ++i) {
      FbleRetainValue(heap, args[i]);
    }

    if (call_instr->func.section == FBLE_LOCALS_FRAME_SECTION) {
      FbleReleaseValue(heap, thread->stack->locals[call_instr->func.index]);
      thread->stack->locals[call_instr->func.index] = NULL;
    }

    for (size_t i = 0; i < call_instr->args.size; ++i) {
      if (call_instr->args.xs[i].section == FBLE_LOCALS_FRAME_SECTION) {
        FbleReleaseValue(heap, thread->stack->locals[call_instr->args.xs[i].index]);
        thread->stack->locals[call_instr->args.xs[i].index] = NULL;
      }
    }

    *status = FbleThreadTailCall(heap, thread, func, args);
    return RETURN;
  }

  (*pc)++;
  *status = FbleThreadCall(heap, thread, thread->stack->locals + call_instr->dest, func, args);
  return (*status == FBLE_EXEC_FINISHED) ? CONTINUE : RETURN;
}

// AbortCallInstr -- see documentation of AbortInstr
//   Execute a CALL_INSTR for abort.
static Control AbortCallInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleCallInstr* call_instr = (FbleCallInstr*)instr;

  if (call_instr->exit) {
    if (call_instr->func.section == FBLE_LOCALS_FRAME_SECTION) {
      FbleReleaseValue(heap, stack->locals[call_instr->func.index]);

      // Set function to NULL so it's safe to release it again if the function
      // is also one of the arguments.
      stack->locals[call_instr->func.index] = NULL;
    }

    for (size_t i = 0; i < call_instr->args.size; ++i) {
      if (call_instr->args.xs[i].section == FBLE_LOCALS_FRAME_SECTION) {
        FbleReleaseValue(heap, stack->locals[call_instr->args.xs[i].index]);

        // Set the arg to NULL so it's safe to release it again if the
        // arg is used more than once.
        stack->locals[call_instr->args.xs[i].index] = NULL;
      }
    }

    *(stack->result) = NULL;
    return RETURN;
  }

  stack->locals[call_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// RunCopyInstr -- see documentation of RunInstr
//   Execute a COPY_INSTR.
static Control RunCopyInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
  FbleValue* value = FrameGet(thread, copy_instr->source);
  FrameSetBorrowed(heap, thread, copy_instr->dest, value);
  (*pc)++;
  return CONTINUE;
}

// AbortCopyInstr -- see documentation of AbortInstr
//   Execute a COPY_INSTR for abort.
static Control AbortCopyInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
  stack->locals[copy_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// RunRefValueInstr -- see documentation of RunInstr
//   Execute a REF_VALUE_INSTR.
static Control RunRefValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
  FbleValue* rv = FbleNewRefValue(heap);
  FrameSetConsumed(heap, thread, ref_instr->dest, rv);
  (*pc)++;
  return CONTINUE;
}

// AbortRefValueInstr -- see documentation of AbortInstr
//   Execute a REF_VALUE_INSTR for abort.
static Control AbortRefValueInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
  stack->locals[ref_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// RunRefDefInstr -- see documentation of RunInstr
//   Execute a REF_DEF_INSTR.
static Control RunRefDefInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
  FbleValue* ref = thread->stack->locals[ref_def_instr->ref];
  FbleValue* value = FrameGet(thread, ref_def_instr->value);
  if (!FbleAssignRefValue(heap, ref, value)) {
    FbleReportError("vacuous value\n", ref_def_instr->loc);
    *status = FBLE_EXEC_ABORTED;
    return RETURN;
  }

  (*pc)++;
  return CONTINUE;
}

// AbortRefDefInstr -- see documentation of AbortInstr
//   Execute a REF_DEF_INSTR for abort
static Control AbortRefDefInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  (*pc)++;
  return CONTINUE;
}

// RunReturnInstr -- see documentation of RunInstr
//   Execute a RETURN_INSTR.
static Control RunReturnInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
  FbleValue* result = NULL;
  switch (return_instr->result.section) {
    case FBLE_STATICS_FRAME_SECTION: {
      result = FbleFuncValueStatics(thread->stack->func)[return_instr->result.index];
      FbleRetainValue(heap, result);
      break;
    }

    case FBLE_LOCALS_FRAME_SECTION: {
      result = thread->stack->locals[return_instr->result.index];
      break;
    }
  }

  *status = FbleThreadReturn(heap, thread, result);
  return RETURN;
}

// AbortReturnInstr -- see documentation of AbortInstr
//   Execute a RETURN_INSTR for abort.
static Control AbortReturnInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
  switch (return_instr->result.section) {
    case FBLE_STATICS_FRAME_SECTION: break;
    case FBLE_LOCALS_FRAME_SECTION: {
      FbleReleaseValue(heap, stack->locals[return_instr->result.index]);
      break;
    }
  }

  *(stack->result) = NULL;
  return RETURN;
}

// RunTypeInstr -- see documentation of RunInstr
//   Execute a FBLE_TYPE_INSTR.
static Control RunTypeInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
  FrameSetConsumed(heap, thread, type_instr->dest, FbleGenericTypeValue);
  (*pc)++;
  return CONTINUE;
}

// AbortTypeInstr -- see documentation of AbortInstr
//   Execute a FBLE_TYPE_INSTR for abort.
static Control AbortTypeInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
  stack->locals[type_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// RunReleaseInstr -- see documentation of RunInstr
//   Execute a FBLE_RELEASE_INSTR.
static Control RunReleaseInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
  FbleReleaseValue(heap, thread->stack->locals[release_instr->target]);
  (*pc)++;
  return CONTINUE;
}

// AbortReleaseInstr -- see documentation of AbortInstr
//   Execute a FBLE_RELEASE_INSTR for abort.
static Control AbortReleaseInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
  FbleReleaseValue(heap, stack->locals[release_instr->target]);
  (*pc)++;
  return CONTINUE;
}

// RunListInstr -- see documentation of RunInstr.
//   Execute an FBLE_LIST_INSTR.
static Control RunListInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleListInstr* list_instr = (FbleListInstr*)instr;
  size_t argc = list_instr->args.size;
  FbleValue* args[argc];
  for (size_t i = 0; i < argc; ++i) {
    args[i] = FrameGet(thread, list_instr->args.xs[i]);
  }

  FbleValue* list = FbleNewListValue(heap, argc, args);
  FrameSetConsumed(heap, thread, list_instr->dest, list);
  (*pc)++;
  return CONTINUE;
}

// AbortListInstr -- see documentation of AbortInstr.
//   Execute a LIST_INSTR for abort.
static Control AbortListInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleListInstr* list_instr = (FbleListInstr*)instr;
  stack->locals[list_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// RunLiteralInstr -- see documentation of RunInstr.
//   Execute an FBLE_LITERAL_INSTR.
static Control RunLiteralInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, FbleExecStatus* status, size_t* pc)
{
  FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
  FbleValue* list = FbleNewLiteralValue(heap, literal_instr->letters.size, literal_instr->letters.xs);
  FrameSetConsumed(heap, thread, literal_instr->dest, list);
  (*pc)++;
  return CONTINUE;
}

// AbortLiteralInstr -- see documentation of AbortInstr.
//   Execute a FBLE_LITERAL_INSTR for abort.
static Control AbortLiteralInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr, size_t* pc)
{
  FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
  stack->locals[literal_instr->dest] = NULL;
  (*pc)++;
  return CONTINUE;
}

// sRunInstr --
//   Implementations of instructions, indexed by instruction tag.
static RunInstr sRunInstr[] = {
  &RunDataTypeInstr,         // FBLE_DATA_TYPE_INSTR
  &RunStructValueInstr,      // FBLE_STRUCT_VALUE_INSTR
  &RunUnionValueInstr,       // FBLE_UNION_VALUE_INSTR
  &RunStructAccessInstr,     // FBLE_STRUCT_ACCESS_INSTR
  &RunUnionAccessInstr,      // FBLE_UNION_ACCESS_INSTR
  &RunUnionSelectInstr,      // FBLE_UNION_SELECT_INSTR
  &RunJumpInstr,             // FBLE_JUMP_INSTR
  &RunFuncValueInstr,        // FBLE_FUNC_VALUE_INSTR
  &RunCallInstr,             // FBLE_CALL_INSTR
  &RunCopyInstr,             // FBLE_COPY_INSTR
  &RunRefValueInstr,         // FBLE_REF_VALUE_INSTR
  &RunRefDefInstr,           // FBLE_REF_DEF_INSTR
  &RunReturnInstr,           // FBLE_RETURN_INSTR
  &RunTypeInstr,             // FBLE_TYPE_INSTR
  &RunReleaseInstr,          // FBLE_RELEASE_INSTR
  &RunListInstr,             // FBLE_LIST_INSTR
  &RunLiteralInstr,          // FBLE_LITERAL_INSTR
};

// sAbortInstr --
//   Implementations of instructions for the purposes of aborting a stack
//   frame, indexed by instruction tag.
static AbortInstr sAbortInstr[] = {
  &AbortDataTypeInstr,         // FBLE_DATA_TYPE_INSTR
  &AbortStructValueInstr,      // FBLE_STRUCT_VALUE_INSTR
  &AbortUnionValueInstr,       // FBLE_UNION_VALUE_INSTR
  &AbortStructAccessInstr,     // FBLE_STRUCT_ACCESS_INSTR
  &AbortUnionAccessInstr,      // FBLE_UNION_ACCESS_INSTR
  &AbortUnionSelectInstr,      // FBLE_UNION_SELECT_INSTR
  &AbortJumpInstr,             // FBLE_JUMP_INSTR
  &AbortFuncValueInstr,        // FBLE_FUNC_VALUE_INSTR
  &AbortCallInstr,             // FBLE_CALL_INSTR
  &AbortCopyInstr,             // FBLE_COPY_INSTR
  &AbortRefValueInstr,         // FBLE_REF_VALUE_INSTR
  &AbortRefDefInstr,           // FBLE_REF_DEF_INSTR
  &AbortReturnInstr,           // FBLE_RETURN_INSTR
  &AbortTypeInstr,             // FBLE_TYPE_INSTR
  &AbortReleaseInstr,          // FBLE_RELEASE_INSTR
  &AbortListInstr,             // FBLE_LIST_INSTR
  &AbortLiteralInstr,          // FBLE_LITERAL_INSTR
};

// See documentation in interpret.h
FbleExecStatus FbleInterpreterRunFunction(FbleValueHeap* heap, FbleThread* thread)
{
  FbleProfileThread* profile = thread->profile;
  FbleBlockId profile_block_offset = FbleFuncValueProfileBaseId(thread->stack->func);
  FbleInstr** code = ((FbleCode*)FbleFuncValueExecutable(thread->stack->func))->instrs.xs;

  FbleExecStatus status = FBLE_EXEC_ABORTED;
  Control control = CONTINUE; 
  size_t pc = 0;
  while (control == CONTINUE) {
    FbleInstr* instr = code[pc];
    if (profile != NULL) {
      if (rand() % PROFILE_SAMPLE_PERIOD == 0) {
        FbleProfileSample(profile, 1);
      }

      for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
        switch (op->tag) {
          case FBLE_PROFILE_ENTER_OP:
            FbleProfileEnterBlock(profile, profile_block_offset + op->block);
            break;

          case FBLE_PROFILE_REPLACE_OP:
            FbleProfileReplaceBlock(profile, profile_block_offset + op->block);
            break;

          case FBLE_PROFILE_EXIT_OP:
            FbleProfileExitBlock(profile);
            break;
        }
      }
    }

    control = sRunInstr[instr->tag](heap, thread, instr, &status, &pc);
  }

  if (status == FBLE_EXEC_ABORTED) {
    Control control = CONTINUE;
    while (control == CONTINUE) {
      FbleInstr* instr = code[pc];
      control = sAbortInstr[instr->tag](heap, thread->stack, instr, &pc);
    }
    FbleThreadReturn(heap, thread, NULL);
  }
  return status;
}

// FbleInterpret -- see documentation in fble-interpret.h
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
