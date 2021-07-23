
#include "interpret.h"

#include <assert.h>   // for assert
#include <stdlib.h>   // for rand

#include "fble-alloc.h"   // for FbleAlloc, FbleFree
#include "fble-interpret.h"
#include "fble-vector.h"  // for FbleVectorInit, etc.

#include "code.h"
#include "execute.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// PROFILE_SAMPLE_PERIOD --
//   The approximate number of instructions to execute before taking another
//   profiling sample.
#define PROFILE_SAMPLE_PERIOD 1024

static FbleValue FrameGet(FbleThread* thread, FbleFrameIndex index);
static FbleValue FrameGetStrict(FbleThread* thread, FbleFrameIndex index);
static void FrameSetBorrowed(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue value);
static void FrameSetConsumed(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue value);

// RunInstr --
//   A function that executes an instruction.
//
// Inputs:
//   heap - heap to use for allocations.
//   threads - the thread list, for forking new threads.
//   thread - the current thread state.
//   instr - the instruction to execute.
//   io_activity - set to true if the thread does any i/o activity that could
//                 unblock another thread.
//
// Results:
//   FBLE_EXEC_FINISHED - if the function is done executing.
//   FBLE_EXEC_BLOCKED - if the thread is blocked on I/O.
//   FBLE_EXEC_YIELDED - if our time slice for executing instructions is over.
//   FBLE_EXEC_RUNNING - if there are more instructions in the function to execute.
//   FBLE_EXEC_ABORTED - if execution should be aborted.
//
// Side effects:
//   Executes the instruction.
typedef FbleExecStatus (*RunInstr)(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);

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
//
// Results:
//   FBLE_EXEC_FINISHED - if the function is done aborting.
//   FBLE_EXEC_RUNNING - if there are more instructions in the function to execute.
//
// Side effects:
//   Executes the instruction for the purposes of aborting the stack frame.
typedef FbleExecStatus (*AbortInstr)(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr);

static FbleExecStatus RunStructValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunUnionValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunStructAccessInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunUnionAccessInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunUnionSelectInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunJumpInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunFuncValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunCallInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunCopyInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunLinkInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunForkInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunRefValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunRefDefInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunReturnInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunTypeInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunReleaseInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunListInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RunLiteralInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);

static FbleExecStatus AbortStructValueInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortUnionValueInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortStructAccessInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortUnionAccessInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortUnionSelectInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortJumpInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortFuncValueInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortCallInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortCopyInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortLinkInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortForkInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortRefValueInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortRefDefInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortReturnInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortTypeInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortReleaseInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortListInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);
static FbleExecStatus AbortLiteralInstr(FbleValueHeap* heap, FbleStack*, FbleInstr* instr);

// sRunInstr --
//   Implementations of instructions, indexed by instruction tag.
static RunInstr sRunInstr[] = {
  &RunStructValueInstr,      // FBLE_STRUCT_VALUE_INSTR
  &RunUnionValueInstr,       // FBLE_UNION_VALUE_INSTR
  &RunStructAccessInstr,     // FBLE_STRUCT_ACCESS_INSTR
  &RunUnionAccessInstr,      // FBLE_UNION_ACCESS_INSTR
  &RunUnionSelectInstr,      // FBLE_UNION_SELECT_INSTR
  &RunJumpInstr,             // FBLE_JUMP_INSTR
  &RunFuncValueInstr,        // FBLE_FUNC_VALUE_INSTR
  &RunCallInstr,             // FBLE_CALL_INSTR
  &RunLinkInstr,             // FBLE_LINK_INSTR
  &RunForkInstr,             // FBLE_FORK_INSTR
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
  &AbortStructValueInstr,      // FBLE_STRUCT_VALUE_INSTR
  &AbortUnionValueInstr,       // FBLE_UNION_VALUE_INSTR
  &AbortStructAccessInstr,     // FBLE_STRUCT_ACCESS_INSTR
  &AbortUnionAccessInstr,      // FBLE_UNION_ACCESS_INSTR
  &AbortUnionSelectInstr,      // FBLE_UNION_SELECT_INSTR
  &AbortJumpInstr,             // FBLE_JUMP_INSTR
  &AbortFuncValueInstr,        // FBLE_FUNC_VALUE_INSTR
  &AbortCallInstr,             // FBLE_CALL_INSTR
  &AbortLinkInstr,             // FBLE_LINK_INSTR
  &AbortForkInstr,             // FBLE_FORK_INSTR
  &AbortCopyInstr,             // FBLE_COPY_INSTR
  &AbortRefValueInstr,         // FBLE_REF_VALUE_INSTR
  &AbortRefDefInstr,           // FBLE_REF_DEF_INSTR
  &AbortReturnInstr,           // FBLE_RETURN_INSTR
  &AbortTypeInstr,             // FBLE_TYPE_INSTR
  &AbortReleaseInstr,          // FBLE_RELEASE_INSTR
  &AbortListInstr,             // FBLE_LIST_INSTR
  &AbortLiteralInstr,          // FBLE_LITERAL_INSTR
};

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
static FbleValue FrameGet(FbleThread* thread, FbleFrameIndex index)
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
static FbleValue FrameGetStrict(FbleThread* thread, FbleFrameIndex index)
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
static void FrameSetBorrowed(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue value)
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
static void FrameSetConsumed(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue value)
{
  thread->stack->locals[index] = value;
}

// RunStructValueInstr -- see documentation of RunInstr.
//   Execute a STRUCT_VALUE_INSTR.
static FbleExecStatus RunStructValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
  size_t argc = struct_value_instr->args.size;
  FbleValue args[argc];
  for (size_t i = 0; i < argc; ++i) {
    args[i] = FrameGet(thread, struct_value_instr->args.xs[i]);
  }

  FbleValue value = FbleNewStructValue_(heap, argc, args);
  FrameSetConsumed(heap, thread, struct_value_instr->dest, value);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortStructValueInstr -- see documentation of AbortInstr.
//   Execute a STRUCT_VALUE_INSTR for abort.
static FbleExecStatus AbortStructValueInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
  stack->locals[struct_value_instr->dest] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunUnionValueInstr -- see documentation of RunInstr
//   Execute a UNION_VALUE_INSTR.
static FbleExecStatus RunUnionValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
  size_t tag = union_value_instr->tag;
  FbleValue arg = FrameGet(thread, union_value_instr->arg);
  FbleValue value = FbleNewUnionValue(heap, tag, arg);
  FrameSetConsumed(heap, thread, union_value_instr->dest, value);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortUnionValueInstr -- see documentation of AbortInstr
//   Execute a UNION_VALUE_INSTR for abort.
static FbleExecStatus AbortUnionValueInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
  stack->locals[union_value_instr->dest] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunStructAccessInstr -- see documentation of RunInstr
//   Execute a STRUCT_ACCESS_INSTR.
static FbleExecStatus RunStructAccessInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

  FbleValue sv = FrameGetStrict(thread, access_instr->obj);
  if (FbleValueIsNull(sv)) {
    FbleReportError("undefined struct value access\n", access_instr->loc);
    return FBLE_EXEC_ABORTED;
  }

  FbleValue v = FbleStructValueAccess(sv, access_instr->tag);
  FrameSetBorrowed(heap, thread, access_instr->dest, v);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortStructAccessInstr -- see documentation of AbortInstr
//   Execute a STRUCT_ACCESS_INSTR for abort.
static FbleExecStatus AbortStructAccessInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
  stack->locals[access_instr->dest] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunUnionAccessInstr -- see documentation of RunInstr
//   Execute a UNION_ACCESS_INSTR.
static FbleExecStatus RunUnionAccessInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

  FbleValue uv = FrameGetStrict(thread, access_instr->obj);
  if (FbleValueIsNull(uv)) {
    FbleReportError("undefined union value access\n", access_instr->loc);
    return FBLE_EXEC_ABORTED;
  }

  if (FbleUnionValueTag(uv) != access_instr->tag) {
    FbleReportError("union field access undefined: wrong tag\n", access_instr->loc);
    return FBLE_EXEC_ABORTED;
  }

  FrameSetBorrowed(heap, thread, access_instr->dest, FbleUnionValueAccess(uv));
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortUnionAccessInstr -- see documentation of AbortInstr
//   Execute a UNION_ACCESS_INSTR.
static FbleExecStatus AbortUnionAccessInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
  stack->locals[access_instr->dest] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunUnionSelectInstr -- see documentation of RunInstr
//   Execute a UNION_SELECT_INSTR.
static FbleExecStatus RunUnionSelectInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
  FbleValue uv = FrameGetStrict(thread, select_instr->condition);
  if (FbleValueIsNull(uv)) {
    FbleReportError("undefined union value select\n", select_instr->loc);
    return FBLE_EXEC_ABORTED;
  }
  thread->stack->pc += 1 + select_instr->jumps.xs[FbleUnionValueTag(uv)];
  return FBLE_EXEC_RUNNING;
}

// AbortUnionSelectInstr -- see documentation of AbortInstr
//   Execute a UNION_SELECT_INSTR for abort.
static FbleExecStatus AbortUnionSelectInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  // For the purposes of abort, it doesn't matter which branch we take,
  // because all branches have to clean up memory the same way.
  FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
  assert(select_instr->jumps.size > 0);
  stack->pc += 1 + select_instr->jumps.xs[0];
  return FBLE_EXEC_RUNNING;
}

// RunJumpInstr -- see documentation of RunInstr
//   Execute a JUMP_INSTR.
static FbleExecStatus RunJumpInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
  thread->stack->pc += 1 + jump_instr->count;
  return FBLE_EXEC_RUNNING;
}

// AbortJumpInstr -- see documentation of AbortInstr
//   Execute a JUMP_INSTR for abort.
static FbleExecStatus AbortJumpInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
  stack->pc += 1 + jump_instr->count;
  return FBLE_EXEC_RUNNING;
}

// RunFuncValueInstr -- see documentation of RunInstr
//   Execute a FUNC_VALUE_INSTR.
static FbleExecStatus RunFuncValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
  FbleValue value = FbleNewFuncValue(heap, &func_value_instr->code->_base, FbleFuncValueProfileBaseId(thread->stack->func));
  FbleValue* statics = FbleFuncValueStatics(value);
  for (size_t i = 0; i < func_value_instr->scope.size; ++i) {
    FbleValue arg = FrameGet(thread, func_value_instr->scope.xs[i]);
    statics[i] = arg;
    FbleValueAddRef(heap, value, arg);
  }
  FrameSetConsumed(heap, thread, func_value_instr->dest,value);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortFuncValueInstr -- see documentation of AbortInstr
//   Execute a FUNC_VALUE_INSTR for abort.
static FbleExecStatus AbortFuncValueInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
  stack->locals[func_value_instr->dest] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunCallInstr -- see documentation of RunInstr
//   Execute a CALL_INSTR.
static FbleExecStatus RunCallInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleCallInstr* call_instr = (FbleCallInstr*)instr;
  FbleValue func = FrameGetStrict(thread, call_instr->func);
  if (FbleValueIsNull(func)) {
    FbleReportError("called undefined function\n", call_instr->loc);
    return FBLE_EXEC_ABORTED;
  };

  FbleExecutable* executable = FbleFuncValueExecutable(func);
  FbleValue args[executable->args];
  for (size_t i = 0; i < executable->args; ++i) {
    args[i] = FrameGet(thread, call_instr->args.xs[i]);
  }

  if (call_instr->exit) {
    FbleRetainValue(heap, func);
    for (size_t i = 0; i < executable->args; ++i) {
      FbleRetainValue(heap, args[i]);
    }

    if (call_instr->func.section == FBLE_LOCALS_FRAME_SECTION) {
      FbleReleaseValue(heap, thread->stack->locals[call_instr->func.index]);
      thread->stack->locals[call_instr->func.index] = FbleNullValue;
    }

    for (size_t i = 0; i < call_instr->args.size; ++i) {
      if (call_instr->args.xs[i].section == FBLE_LOCALS_FRAME_SECTION) {
        FbleReleaseValue(heap, thread->stack->locals[call_instr->args.xs[i].index]);
        thread->stack->locals[call_instr->args.xs[i].index] = FbleNullValue;
      }
    }

    FbleThreadTailCall(heap, func, args, thread);
    return FBLE_EXEC_FINISHED;
  }

  thread->stack->pc++;
  FbleThreadCall(heap, thread->stack->locals + call_instr->dest, func, args, thread);
  return FBLE_EXEC_FINISHED;
}

// AbortCallInstr -- see documentation of AbortInstr
//   Execute a CALL_INSTR for abort.
static FbleExecStatus AbortCallInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleCallInstr* call_instr = (FbleCallInstr*)instr;

  if (call_instr->exit) {
    if (call_instr->func.section == FBLE_LOCALS_FRAME_SECTION) {
      FbleReleaseValue(heap, stack->locals[call_instr->func.index]);

      // Set function to NULL so it's safe to release it again if the function
      // is also one of the arguments.
      stack->locals[call_instr->func.index] = FbleNullValue;
    }

    for (size_t i = 0; i < call_instr->args.size; ++i) {
      if (call_instr->args.xs[i].section == FBLE_LOCALS_FRAME_SECTION) {
        FbleReleaseValue(heap, stack->locals[call_instr->args.xs[i].index]);

        // Set the arg to NULL so it's safe to release it again if the
        // arg is used more than once.
        stack->locals[call_instr->args.xs[i].index] = FbleNullValue;
      }
    }

    *(stack->result) = FbleNullValue;
    return FBLE_EXEC_FINISHED;
  }

  stack->locals[call_instr->dest] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunForkInstr -- see documentation of RunInstr
//   Execute a FORK_INSTR.
static FbleExecStatus RunForkInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleForkInstr* fork_instr = (FbleForkInstr*)instr;

  for (size_t i = 0; i < fork_instr->args.size; ++i) {
    FbleValue arg = FrameGetStrict(thread, fork_instr->args.xs[i]);
    FbleValue* result = thread->stack->locals + fork_instr->dests.xs[i];
    FbleThreadFork(heap, threads, thread, result, arg, NULL);
  }
  thread->stack->pc++;
  return FBLE_EXEC_YIELDED;
}

// AbortForkInstr -- see documentation of AbortInstr
//   Execute a FORK_INSTR for abort.
static FbleExecStatus AbortForkInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleForkInstr* fork_instr = (FbleForkInstr*)instr;

  for (size_t i = 0; i < fork_instr->args.size; ++i) {
    stack->locals[fork_instr->dests.xs[i]] = FbleNullValue;
  }
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunCopyInstr -- see documentation of RunInstr
//   Execute a COPY_INSTR.
static FbleExecStatus RunCopyInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
  FbleValue value = FrameGet(thread, copy_instr->source);
  FrameSetBorrowed(heap, thread, copy_instr->dest, value);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortCopyInstr -- see documentation of AbortInstr
//   Execute a COPY_INSTR for abort.
static FbleExecStatus AbortCopyInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
  stack->locals[copy_instr->dest] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunLinkInstr -- see documentation of RunInstr
//   Execute a LINK_INSTR.
static FbleExecStatus RunLinkInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;

  FbleNewLinkValue(heap,
      FbleFuncValueProfileBaseId(thread->stack->func) + link_instr->profile,
      thread->stack->locals + link_instr->get,
      thread->stack->locals + link_instr->put);

  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortLinkInstr -- see documentation of AbortInstr
//   Execute a LINK_INSTR for abort.
static FbleExecStatus AbortLinkInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
  stack->locals[link_instr->get] = FbleNullValue;
  stack->locals[link_instr->put] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunRefValueInstr -- see documentation of RunInstr
//   Execute a REF_VALUE_INSTR.
static FbleExecStatus RunRefValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
  FbleValue rv = FbleNewRefValue(heap);
  FrameSetConsumed(heap, thread, ref_instr->dest, rv);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortRefValueInstr -- see documentation of AbortInstr
//   Execute a REF_VALUE_INSTR for abort.
static FbleExecStatus AbortRefValueInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
  stack->locals[ref_instr->dest] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunRefDefInstr -- see documentation of RunInstr
//   Execute a REF_DEF_INSTR.
static FbleExecStatus RunRefDefInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
  FbleValue ref = thread->stack->locals[ref_def_instr->ref];
  FbleValue value = FrameGet(thread, ref_def_instr->value);
  if (!FbleAssignRefValue(heap, ref, value)) {
    FbleReportError("vacuous value\n", ref_def_instr->loc);
    return FBLE_EXEC_ABORTED;
  }

  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortRefDefInstr -- see documentation of AbortInstr
//   Execute a REF_DEF_INSTR for abort
static FbleExecStatus AbortRefDefInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunReturnInstr -- see documentation of RunInstr
//   Execute a RETURN_INSTR.
static FbleExecStatus RunReturnInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
  FbleValue result = FbleNullValue;
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

  FbleThreadReturn(heap, thread, result);
  return FBLE_EXEC_FINISHED;
}

// AbortReturnInstr -- see documentation of AbortInstr
//   Execute a RETURN_INSTR for abort.
static FbleExecStatus AbortReturnInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
  switch (return_instr->result.section) {
    case FBLE_STATICS_FRAME_SECTION: break;
    case FBLE_LOCALS_FRAME_SECTION: {
      FbleReleaseValue(heap, stack->locals[return_instr->result.index]);
      break;
    }
  }

  *(stack->result) = FbleNullValue;
  return FBLE_EXEC_FINISHED;
}

// RunTypeInstr -- see documentation of RunInstr
//   Execute a FBLE_TYPE_INSTR.
static FbleExecStatus RunTypeInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
  FrameSetConsumed(heap, thread, type_instr->dest, FbleGenericTypeValue);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortTypeInstr -- see documentation of AbortInstr
//   Execute a FBLE_TYPE_INSTR for abort.
static FbleExecStatus AbortTypeInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
  stack->locals[type_instr->dest] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunReleaseInstr -- see documentation of RunInstr
//   Execute a FBLE_RELEASE_INSTR.
static FbleExecStatus RunReleaseInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
  FbleReleaseValue(heap, thread->stack->locals[release_instr->target]);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortReleaseInstr -- see documentation of AbortInstr
//   Execute a FBLE_RELEASE_INSTR for abort.
static FbleExecStatus AbortReleaseInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
  FbleReleaseValue(heap, stack->locals[release_instr->target]);
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunListInstr -- see documentation of RunInstr.
//   Execute an FBLE_LIST_INSTR.
static FbleExecStatus RunListInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleListInstr* list_instr = (FbleListInstr*)instr;
  size_t argc = list_instr->args.size;
  FbleValue args[argc];
  for (size_t i = 0; i < argc; ++i) {
    args[i] = FrameGet(thread, list_instr->args.xs[i]);
  }

  FbleValue list = FbleNewListValue(heap, argc, args);
  FrameSetConsumed(heap, thread, list_instr->dest, list);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortListInstr -- see documentation of AbortInstr.
//   Execute a LIST_INSTR for abort.
static FbleExecStatus AbortListInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleListInstr* list_instr = (FbleListInstr*)instr;
  stack->locals[list_instr->dest] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunLiteralInstr -- see documentation of RunInstr.
//   Execute an FBLE_LITERAL_INSTR.
static FbleExecStatus RunLiteralInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
  FbleValue list = FbleNewLiteralValue(heap, literal_instr->letters.size, literal_instr->letters.xs);
  FrameSetConsumed(heap, thread, literal_instr->dest, list);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// AbortLiteralInstr -- see documentation of AbortInstr.
//   Execute a FBLE_LITERAL_INSTR for abort.
static FbleExecStatus AbortLiteralInstr(FbleValueHeap* heap, FbleStack* stack, FbleInstr* instr)
{
  FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
  stack->locals[literal_instr->dest] = FbleNullValue;
  stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// See documentation in interpret.h
FbleExecStatus FbleInterpreterRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  FbleProfileThread* profile = thread->profile;
  FbleBlockId profile_base_id = FbleFuncValueProfileBaseId(thread->stack->func);
  FbleInstr** code = ((FbleCode*)FbleFuncValueExecutable(thread->stack->func))->instrs.xs;

  FbleExecStatus status = FBLE_EXEC_RUNNING;
  while (status == FBLE_EXEC_RUNNING) {
    FbleInstr* instr = code[thread->stack->pc];
    if (profile != NULL) {
      if (rand() % PROFILE_SAMPLE_PERIOD == 0) {
        FbleProfileSample(profile, 1);
      }

      for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
        switch (op->tag) {
          case FBLE_PROFILE_ENTER_OP:
            FbleProfileEnterBlock(profile, profile_base_id + op->block);
            break;

          case FBLE_PROFILE_REPLACE_OP:
            FbleProfileReplaceBlock(profile, profile_base_id + op->block);
            break;

          case FBLE_PROFILE_EXIT_OP:
            FbleProfileExitBlock(profile);
            break;
        }
      }
    }

    status = sRunInstr[instr->tag](heap, threads, thread, instr, io_activity);
  }
  return status;
}

// FbleInterpreterAbortFunction -- see documentation in interpret.h
void FbleInterpreterAbortFunction(FbleValueHeap* heap, FbleStack* stack)
{
  FbleInstr** code = ((FbleCode*)FbleFuncValueExecutable(stack->func))->instrs.xs;
  FbleExecStatus status = FBLE_EXEC_RUNNING;
  while (status == FBLE_EXEC_RUNNING) {
    FbleInstr* instr = code[stack->pc];
    status = sAbortInstr[instr->tag](heap, stack, instr);
  }
}

// FbleInterpret -- see documentation in fble-interpret.h
FbleExecutableProgram* FbleInterpret(FbleCompiledProgram* program)
{
  FbleExecutableProgram* executable = FbleAlloc(FbleExecutableProgram);
  FbleVectorInit(executable->modules);

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleCompiledModule* module = program->modules.xs + i;

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

    FbleVectorAppend(executable->modules, executable_module);
  }

  return executable;
}
