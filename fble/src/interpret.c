
#include "fble-interpret.h"

#include <assert.h>   // for assert
#include <stdlib.h>   // for rand

#include "code.h"
#include "execute.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// PROFILE_SAMPLE_PERIOD --
//   The approximate number of instructions to execute before taking another
//   profiling sample.
#define PROFILE_SAMPLE_PERIOD 1024

static FbleValue* FrameGet(FbleThread* thread, FbleFrameIndex index);
static FbleValue* FrameGetStrict(FbleThread* thread, FbleFrameIndex index);
static void FrameSet(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value);
static void FrameSetAndRelease(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value);
static FbleExecStatus InterpretedRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);

// InstrImpl --
//   A function that executes an instruction.
//
// Inputs:
//   heap - heap to use for allocations.
//   instr - the instruction to execute.
//   threads - the thread list, for forking new threads.
//   thread - the current thread state.
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
typedef FbleExecStatus (*InstrImpl)(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);

static FbleExecStatus StructValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus UnionValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus StructAccessInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus UnionAccessInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus UnionSelectInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus JumpInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus FuncValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus CallInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus GetInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus PutInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus CopyInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus LinkInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus ForkInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RefValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RefDefInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus ReturnInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus TypeInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity);

// sInstrImpls --
//   Implementations of instructions, indexed by instruction tag.
static InstrImpl sInstrImpls[] = {
  &StructValueInstr,      // FBLE_STRUCT_VALUE_INSTR
  &UnionValueInstr,       // FBLE_UNION_VALUE_INSTR
  &StructAccessInstr,     // FBLE_STRUCT_ACCESS_INSTR
  &UnionAccessInstr,      // FBLE_UNION_ACCESS_INSTR
  &UnionSelectInstr,      // FBLE_UNION_SELECT_INSTR
  &JumpInstr,             // FBLE_JUMP_INSTR
  &FuncValueInstr,        // FBLE_FUNC_VALUE_INSTR
  &CallInstr,             // FBLE_CALL_INSTR
  &GetInstr,              // FBLE_GET_INSTR
  &PutInstr,              // FBLE_PUT_INSTR
  &LinkInstr,             // FBLE_LINK_INSTR
  &ForkInstr,             // FBLE_FORK_INSTR
  &CopyInstr,             // FBLE_COPY_INSTR
  &RefValueInstr,         // FBLE_REF_VALUE_INSTR
  &RefDefInstr,           // FBLE_REF_DEF_INSTR
  &ReturnInstr,           // FBLE_RETURN_INSTR
  &TypeInstr,             // FBLE_TYPE_INSTR
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
static FbleValue* FrameGet(FbleThread* thread, FbleFrameIndex index)
{
  switch (index.section) {
    case FBLE_STATICS_FRAME_SECTION: return thread->stack->func->statics[index.index];
    case FBLE_LOCALS_FRAME_SECTION: return thread->stack->locals.xs[index.index];
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

// FrameSet -- 
//   Store a value onto the frame on the top of the stack.
//
// Inputs:
//   heap - the value heap.
//   thread - the current thread.
//   index - the index of the value to set.
//   value - the value to set.
//
// Side effects:
//   Sets the value at the given index in the frame.
static void FrameSet(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value)
{
  thread->stack->locals.xs[index] = value;
  FbleValueAddRef(heap, &thread->stack->_base, value);
}

// FrameSetAndRelease --
//   Store a value onto the frame on the top of the stack and call
//   FbleValueRelease on the value.
//
// Inputs:
//   heap - the value heap.
//   thread - the current thread.
//   index - the index of the value to set.
//   value - the value to set.
//
// Side effects:
//   Sets the value at the given index in the frame and calls FbleValueRelease
//   on the value.
static void FrameSetAndRelease(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value)
{
  FrameSet(heap, thread, index, value);
  FbleReleaseValue(heap, value);
}

// StructValueInstr -- see documentation of InstrImpl.
//   Execute a STRUCT_VALUE_INSTR.
static FbleExecStatus StructValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
  size_t argc = struct_value_instr->args.size;

  FbleStructValue* value = FbleNewValueExtra(heap, FbleStructValue, sizeof(FbleValue*) * argc);
  value->_base.tag = FBLE_STRUCT_VALUE;
  value->fieldc = argc;
  for (size_t i = 0; i < argc; ++i) {
    value->fields[i] = FrameGet(thread, struct_value_instr->args.xs[i]);
    FbleValueAddRef(heap, &value->_base, value->fields[i]);
  }

  FrameSetAndRelease(heap, thread, struct_value_instr->dest, &value->_base);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// UnionValueInstr -- see documentation of InstrImpl
//   Execute a UNION_VALUE_INSTR.
static FbleExecStatus UnionValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;

  FbleUnionValue* value = FbleNewValue(heap, FbleUnionValue);
  value->_base.tag = FBLE_UNION_VALUE;
  value->tag = union_value_instr->tag;
  value->arg = FrameGet(thread, union_value_instr->arg);
  FbleValueAddRef(heap, &value->_base, value->arg);

  FrameSetAndRelease(heap, thread, union_value_instr->dest, &value->_base);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// StructAccessInstr -- see documentation of InstrImpl
//   Execute a STRUCT_ACCESS_INSTR.
static FbleExecStatus StructAccessInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

  FbleStructValue* sv = (FbleStructValue*)FrameGetStrict(thread, access_instr->obj);
  if (sv == NULL) {
    FbleReportError("undefined struct value access\n", access_instr->loc);
    return FBLE_EXEC_ABORTED;
  }

  assert(sv->_base.tag == FBLE_STRUCT_VALUE);
  assert(access_instr->tag < sv->fieldc);
  FrameSet(heap, thread, access_instr->dest, sv->fields[access_instr->tag]);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// UnionAccessInstr -- see documentation of InstrImpl
//   Execute a UNION_ACCESS_INSTR.
static FbleExecStatus UnionAccessInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

  FbleUnionValue* uv = (FbleUnionValue*)FrameGetStrict(thread, access_instr->obj);
  if (uv == NULL) {
    FbleReportError("undefined union value access\n", access_instr->loc);
    return FBLE_EXEC_ABORTED;
  }

  assert(uv->_base.tag == FBLE_UNION_VALUE);
  if (uv->tag != access_instr->tag) {
    FbleReportError("union field access undefined: wrong tag\n", access_instr->loc);
    return FBLE_EXEC_ABORTED;
  }

  FrameSet(heap, thread, access_instr->dest, uv->arg);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// UnionSelectInstr -- see documentation of InstrImpl
//   Execute a UNION_SELECT_INSTR.
static FbleExecStatus UnionSelectInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
  FbleUnionValue* uv = (FbleUnionValue*)FrameGetStrict(thread, select_instr->condition);
  if (uv == NULL) {
    FbleReportError("undefined union value select\n", select_instr->loc);
    return FBLE_EXEC_ABORTED;
  }
  assert(uv->_base.tag == FBLE_UNION_VALUE);
  thread->stack->pc += 1 + select_instr->jumps.xs[uv->tag];
  return FBLE_EXEC_RUNNING;
}

// JumpInstr -- see documentation of InstrImpl
//   Execute a JUMP_INSTR.
static FbleExecStatus JumpInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
  thread->stack->pc += 1 + jump_instr->count;
  return FBLE_EXEC_RUNNING;
}

// FuncValueInstr -- see documentation of InstrImpl
//   Execute a FUNC_VALUE_INSTR.
static FbleExecStatus FuncValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
  size_t staticc = func_value_instr->code->statics;

  FbleFuncValue* value = FbleNewValueExtra(heap, FbleFuncValue, sizeof(FbleValue*) * staticc);
  value->_base.tag = FBLE_FUNC_VALUE;
  value->executable = FbleAlloc(heap->arena, FbleExecutable);
  value->executable->code = func_value_instr->code;
  value->executable->code->refcount++;
  value->executable->run = &InterpretedRunFunction;
  value->argc = func_value_instr->argc;
  value->localc = func_value_instr->code->locals;
  value->staticc = staticc;
  for (size_t i = 0; i < staticc; ++i) {
    FbleValue* arg = FrameGet(thread, func_value_instr->scope.xs[i]);
    value->statics[i] = arg;
    FbleValueAddRef(heap, &value->_base, arg);
  }
  FrameSetAndRelease(heap, thread, func_value_instr->dest, &value->_base);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// CallInstr -- see documentation of InstrImpl
//   Execute a CALL_INSTR.
static FbleExecStatus CallInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleCallInstr* call_instr = (FbleCallInstr*)instr;
  FbleFuncValue* func = (FbleFuncValue*)FrameGetStrict(thread, call_instr->func);
  if (func == NULL) {
    FbleReportError("called undefined function\n", call_instr->loc);
    return FBLE_EXEC_ABORTED;
  };
  assert(func->_base.tag == FBLE_FUNC_VALUE);

  FbleValue* args[func->argc];
  for (size_t i = 0; i < func->argc; ++i) {
    args[i] = FrameGet(thread, call_instr->args.xs[i]);
  }

  if (call_instr->exit) {
    FbleThreadTailCall(heap, func, args, thread);
    return FBLE_EXEC_FINISHED;
  }

  thread->stack->pc++;
  FbleRefValue* result = FbleThreadCall(heap, func, args, thread);
  thread->stack->tail->locals.xs[call_instr->dest] = &result->_base;
  FbleValueAddRef(heap, &thread->stack->tail->_base, &result->_base);
  FbleReleaseValue(heap, &result->_base);
  return FBLE_EXEC_FINISHED;
}

// GetInstr -- see documentation of InstrImpl
//   Execute a GET_INSTR.
static FbleExecStatus GetInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleGetInstr* get_instr = (FbleGetInstr*)instr;
  FbleValue* get_port = FrameGet(thread, get_instr->port);
  if (get_port->tag == FBLE_LINK_VALUE) {
    FbleLinkValue* link = (FbleLinkValue*)get_port;

    if (link->head == NULL) {
      // Blocked on get.
      assert(instr->profile_ops == NULL && "profile op might run twice");
      return FBLE_EXEC_BLOCKED;
    }

    FbleValues* head = link->head;
    link->head = link->head->next;
    if (link->head == NULL) {
      link->tail = NULL;
    }

    FrameSet(heap, thread, get_instr->dest, head->value);
    FbleFree(heap->arena, head);
    thread->stack->pc++;
    return FBLE_EXEC_RUNNING;
  }

  if (get_port->tag == FBLE_PORT_VALUE) {
    FblePortValue* port = (FblePortValue*)get_port;
    if (*port->data == NULL) {
      // Blocked on get.
      assert(instr->profile_ops == NULL && "profile op might run twice");
      return FBLE_EXEC_BLOCKED;
    }

    FrameSetAndRelease(heap, thread, get_instr->dest, *port->data);
    *port->data = NULL;
    *io_activity = true;
    thread->stack->pc++;
    return FBLE_EXEC_RUNNING;
  }

  UNREACHABLE("get port must be an input or port value");
  return FBLE_EXEC_ABORTED;
}

// PutInstr -- see documentation of InstrImpl
//   Execute a PUT_INSTR.
static FbleExecStatus PutInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FblePutInstr* put_instr = (FblePutInstr*)instr;
  FbleValue* put_port = FrameGet(thread, put_instr->port);
  FbleValue* arg = FrameGet(thread, put_instr->arg);

  FbleValueV args = { .size = 0, .xs = NULL, };
  FbleValue* unit = FbleNewStructValue(heap, args);

  if (put_port->tag == FBLE_LINK_VALUE) {
    FbleLinkValue* link = (FbleLinkValue*)put_port;

    FbleValues* tail = FbleAlloc(heap->arena, FbleValues);
    tail->value = arg;
    tail->next = NULL;

    if (link->head == NULL) {
      link->head = tail;
      link->tail = tail;
    } else {
      assert(link->tail != NULL);
      link->tail->next = tail;
      link->tail = tail;
    }

    FbleValueAddRef(heap, &link->_base, tail->value);
    FrameSetAndRelease(heap, thread, put_instr->dest, unit);
    *io_activity = true;
    thread->stack->pc++;

    // We yield in this case instead of continuing to execute instructions
    // from this function. This is an arbitrary thing to do, motivated by the
    // fear (not seen in practice thus far) that a producer/consumer pipeline
    // will blow up memory producing everything before the consumer gets a
    // chance to consume anything. It's probably worth figuring out a better
    // reason to yield here.
    return FBLE_EXEC_YIELDED;
  }

  assert(put_port->tag == FBLE_PORT_VALUE);
  FblePortValue* port = (FblePortValue*)put_port;
  if (*port->data != NULL) {
    // Blocked on put.
    assert(instr->profile_ops == NULL && "profile op might run twice");
    return FBLE_EXEC_BLOCKED;
  }

  FbleRetainValue(heap, arg);
  *port->data = arg;
  FrameSetAndRelease(heap, thread, put_instr->dest, unit);
  *io_activity = true;
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// ForkInstr -- see documentation of InstrImpl
//   Execute a FORK_INSTR.
static FbleExecStatus ForkInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleForkInstr* fork_instr = (FbleForkInstr*)instr;

  for (size_t i = 0; i < fork_instr->args.size; ++i) {
    FbleProcValue* arg = (FbleProcValue*)FrameGetStrict(thread, fork_instr->args.xs[i]);

    // You cannot execute a proc in a let binding, so it should be
    // impossible to ever have an undefined proc value.
    assert(arg != NULL && "undefined proc value");
    assert(arg->_base.tag == FBLE_PROC_VALUE);

    FbleThread* child = FbleAlloc(heap->arena, FbleThread);
    child->stack = thread->stack;
    child->profile = thread->profile == NULL ? NULL : FbleForkProfileThread(heap->arena, thread->profile);
    FbleRetainValue(heap, &thread->stack->_base);
    child->stack->joins++;
    FbleVectorAppend(heap->arena, *threads, child);

    FbleRefValue* result = FbleThreadCall(heap, arg, NULL, child);
    FrameSetAndRelease(heap, thread, fork_instr->dests.xs[i], &result->_base);
  }
  thread->stack->pc++;
  return FBLE_EXEC_YIELDED;
}

// CopyInstr -- see documentation of InstrImpl
//   Execute a COPY_INSTR.
static FbleExecStatus CopyInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
  FbleValue* value = FrameGet(thread, copy_instr->source);
  FrameSet(heap, thread, copy_instr->dest, value);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// LinkInstr -- see documentation of InstrImpl
//   Execute a LINK_INSTR.
static FbleExecStatus LinkInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;

  FbleLinkValue* link = FbleNewValue(heap, FbleLinkValue);
  link->_base.tag = FBLE_LINK_VALUE;
  link->head = NULL;
  link->tail = NULL;

  FbleValue* get = FbleNewGetValue(heap, &link->_base);
  FbleValue* put = FbleNewPutValue(heap, &link->_base);
  FbleReleaseValue(heap, &link->_base);

  FrameSetAndRelease(heap, thread, link_instr->get, get);
  FrameSetAndRelease(heap, thread, link_instr->put, put);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RefValueInstr -- see documentation of InstrImpl
//   Execute a REF_VALUE_INSTR.
static FbleExecStatus RefValueInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
  FbleRefValue* rv = FbleNewValue(heap, FbleRefValue);
  rv->_base.tag = FBLE_REF_VALUE;
  rv->value = NULL;

  FrameSetAndRelease(heap, thread, ref_instr->dest, &rv->_base);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RefDefInstr -- see documentation of InstrImpl
//   Execute a REF_DEF_INSTR.
static FbleExecStatus RefDefInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
  FbleRefValue* rv = (FbleRefValue*)thread->stack->locals.xs[ref_def_instr->ref];
  assert(rv->_base.tag == FBLE_REF_VALUE);
  assert(rv->value == NULL);

  FbleValue* value = FrameGet(thread, ref_def_instr->value);
  assert(value != NULL);

  // Unwrap any accumulated layers of references on the returned value, and,
  // more importantly, make sure we aren't forming a vacuous value.
  FbleRefValue* ref = (FbleRefValue*)value;
  while (value->tag == FBLE_REF_VALUE && ref->value != NULL) {
    value = ref->value;
    ref = (FbleRefValue*)value;
  }

  if (ref == rv) {
    FbleReportError("vacuous value\n", ref_def_instr->loc);
    return FBLE_EXEC_ABORTED;
  }

  rv->value = value;
  FbleValueAddRef(heap, &rv->_base, rv->value);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// ReturnInstr -- see documentation of InstrImpl
//   Execute a RETURN_INSTR.
static FbleExecStatus ReturnInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
  FbleValue* result = FrameGet(thread, return_instr->result);

  // Unwrap any layers of refs on the result to avoid long chains of refs.
  // TODO: Is this redundant with the ref unwrapping we do in RefDefInstr?
  FbleRefValue* ref_result = (FbleRefValue*)result;
  while (result->tag == FBLE_REF_VALUE && ref_result->value != NULL) {
    result = ref_result->value;
    ref_result = (FbleRefValue*)result;
  }

  // Pop the top frame from the stack.
  FbleStackValue* stack = thread->stack;
  thread->stack = thread->stack->tail;
  if (thread->stack != NULL) {
    FbleRetainValue(heap, &thread->stack->_base);
  }

  // Save the computed value and pop up the computation state.
  stack->result->value = result;
  FbleValueAddRef(heap, &stack->result->_base, result);
  FbleReleaseValue(heap, &stack->_base);
  return FBLE_EXEC_FINISHED;
}

// TypeInstr -- see documentation of InstrImpl
//   Execute a FBLE_TYPE_INSTR.
static FbleExecStatus TypeInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
  FbleTypeValue* value = FbleNewValue(heap, FbleTypeValue);
  value->_base.tag = FBLE_TYPE_VALUE;
  FrameSetAndRelease(heap, thread, type_instr->dest, &value->_base);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// InterpretedRunFunction --
//   An standard run function that runs an fble function by interpreting the
//   instructions in its instruction block.
//
// See documentation of FbleRunFunction in execute.h.
static FbleExecStatus InterpretedRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  FbleArena* arena = heap->arena;
  FbleProfileThread* profile = thread->profile;

  FbleInstr** code = thread->stack->func->executable->code->instrs.xs;

  FbleExecStatus status = FBLE_EXEC_RUNNING;
  while (status == FBLE_EXEC_RUNNING) {
    FbleInstr* instr = code[thread->stack->pc];
    if (profile != NULL) {
      if (rand() % PROFILE_SAMPLE_PERIOD == 0) {
        FbleProfileSample(arena, profile, 1);
      }

      for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
        switch (op->tag) {
          case FBLE_PROFILE_ENTER_OP:
            FbleProfileEnterBlock(arena, profile, op->block);
            break;

          case FBLE_PROFILE_EXIT_OP:
            FbleProfileExitBlock(arena, profile);
            break;

          case FBLE_PROFILE_AUTO_EXIT_OP: {
            FbleProfileAutoExitBlock(arena, profile);
            break;
          }
        }
      }
    }

    status = sInstrImpls[instr->tag](heap, threads, thread, instr, io_activity);
  }
  return status;
}

// FbleInterpretCode -- see documentation in fble-interpret.h
FbleExecutable* FbleInterpretCode(FbleArena* arena, FbleCode* code)
{
  FbleExecutable* executable = FbleAlloc(arena, FbleExecutable);
  executable->code = code;
  executable->code->refcount++;
  executable->run = &InterpretedRunFunction;
  return executable;
}

// FbleNewInterpretedFuncValue -- see documentation in fble-interpret.h
FbleFuncValue* FbleNewInterpretedFuncValue(FbleValueHeap* heap, size_t argc, FbleCode* code)
{
  FbleFuncValue* v = FbleNewValueExtra(heap, FbleFuncValue, sizeof(FbleValue*) * code->statics);
  v->_base.tag = FBLE_FUNC_VALUE;
  v->executable = FbleInterpretCode(heap->arena, code);
  v->argc = argc;
  v->localc = code->locals;
  v->staticc = code->statics;
  return v;
}

// FbleInterpret -- see documentation in fble-interpret.h
FbleExecutableProgram* FbleInterpret(FbleArena* arena, FbleCompiledProgram* program)
{
  FbleExecutableProgram* executable = FbleAlloc(arena, FbleExecutableProgram);
  FbleVectorInit(arena, executable->modules);

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleCompiledModule* module = program->modules.xs + i;

    FbleExecutableModule* executable_module = FbleVectorExtend(arena, executable->modules);
    executable_module->path = FbleCopyModulePath(module->path);
    FbleVectorInit(arena, executable_module->deps);
    for (size_t d = 0; d < module->deps.size; ++d) {
      FbleVectorAppend(arena, executable_module->deps, FbleCopyModulePath(module->deps.xs[d]));
    }

    executable_module->executable = FbleInterpretCode(arena, module->code);
  }

  return executable;
}
