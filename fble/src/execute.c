// execute.c --
//   Code for executing fble instructions, functions, and processes.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for NULL, abort, rand
#include <string.h>   // for memset

#include "fble.h"
#include "heap.h"
#include "execute.h"
#include "isa.h"
#include "tc.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// TIME_SLICE --
//   The number of instructions a thread is allowed to run before switching to
//   another thread. Also used as the profiling sample period in number of
//   instructions executed.
#define TIME_SLICE 1024

typedef FbleThunkValue Stack;

// FbleThread --
//   Represents a thread of execution.
//
// Fields:
//   stack - the execution stack.
//   profile - the profile thread associated with this thread. May be NULL to
//             disable profiling.
struct FbleThread {
  Stack* stack;
  FbleProfileThread* profile;
};

static FbleValue* FrameGet(FbleThread* thread, FbleFrameIndex index);
static FbleValue* FrameGetStrict(FbleThread* thread, FbleFrameIndex index);
static void FrameSet(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value);
static void FrameSetAndRelease(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value);

static FbleValue* PushFrame(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleThread* thread);
static void ReplaceFrame(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleThread* thread);

// InstrImpl --
//   A function that executes an instruction.
//
// Inputs:
//   heap - heap to use for allocations.
//   instr - the instruction to execute.
//   thread - the thread state.
//   io_activity - set to true if the thread does any i/o activity that could
//                 unblock another thread.
//
// Results:
//   FBLE_EXEC_FINISHED - if we have just returned from the current stack frame.
//   FBLE_EXEC_BLOCKED - if the thread is blocked on I/O.
//   FBLE_EXEC_YIELDED - if our time slice for executing instructions is over.
//   FBLE_EXEC_RUNNING - if there are more instructions in the frame to execute.
//   FBLE_EXEC_ABORTED - if the thread should be aborted.
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

static FbleExecStatus RunThread(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void AbortThreads(FbleValueHeap* heap, FbleThreadV* threads);
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleFuncValue* func, FbleValue** args, FbleProfile* profile);

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
    case FBLE_STATICS_FRAME_SECTION: return thread->stack->func->scope[index.index];
    case FBLE_LOCALS_FRAME_SECTION: return thread->stack->locals.xs[index.index];
  }
  UNREACHABLE("should never get here");
}

// FrameGetStrict --
//   Get and dereference a value from the frame at the top of the given stack.
//   Dereferences the data value, removing all layers of thunk values
//   until a non-thunk value is encountered and returns the non-reference
//   value.
//
// Inputs:
//   thread - the current thread.
//   index - the location of the value in the frame.
//
// Results:
//   The dereferenced value. Returns null in case of abstract value or
//   unevaluated thunk dereference.
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

// PushFrame --
//   Push a frame onto the execution stack.
//
// Inputs:
//   arena - the arena to use for allocations
//   func - the function to execute.
//   args - arguments to pass to the function. length == func->argc.
//   thread - the thread whose stack to push the frame on to.
//
// Result:
//   A thunk value that will refer to the computed result when the frame 
//   completes its execution. The thread will hold a strong reference to the
//   returned thunk value.
//
// Side effects:
//   Takes a references to a newly allocated Stack instance that should be
//   freed when no longer needed.
//   Does not take ownership of the function or the args.
static FbleValue* PushFrame(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleThread* thread)
{
  FbleArena* arena = heap->arena;

  size_t locals = func->executable->code->locals;

  Stack* stack = FbleNewValue(heap, FbleThunkValue);
  stack->_base.tag = FBLE_THUNK_VALUE;
  stack->value = NULL;
  stack->tail = thread->stack;
  stack->joins = 0;
  stack->func = func;
  stack->pc = 0;
  stack->locals.size = func->executable->code->locals;
  stack->locals.xs = FbleArrayAlloc(arena, FbleValue*, locals);
  memset(stack->locals.xs, 0, locals * sizeof(FbleValue*));

  if (stack->tail != NULL) {
    FbleValueAddRef(heap, &stack->_base, &stack->tail->_base);
    FbleReleaseValue(heap, &stack->tail->_base);
  }

  FbleValueAddRef(heap, &stack->_base, &stack->func->_base);
  for (size_t i = 0; i < func->argc; ++i) {
    stack->locals.xs[i] = args[i];
    FbleValueAddRef(heap, &stack->_base, args[i]);
  }
  thread->stack = stack;
  return &stack->_base;
}

// ReplaceFrame --
//   Replace the current frame with a new one.
//
// Inputs:
//   heap - the value heap.
//   func - the function to execute.
//   args - args to the function. length == func->argc.
//   thread - the thread with the stack to change.
//
// Result:
//   The stack with new frame.
//
// Side effects:
// * Does not take ownership of func and args.
// * Exits the current frame, which potentially frees any instructions
//   belonging to that frame.
// * The caller may assume it is safe for the function and arguments to be
//   retained solely by the frame that's being replaced when ReplaceFrame is
//   called.
static void ReplaceFrame(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleThread* thread)
{
  FbleArena* arena = heap->arena;
  Stack* stack = thread->stack;
  size_t old_localc = stack->func->executable->code->locals;
  size_t localc = func->executable->code->locals;

  // Take references to the function and arguments early to ensure we don't
  // drop the last reference to them before we get the chance.
  FbleValueAddRef(heap, &stack->_base, &func->_base);
  for (size_t i = 0; i < func->argc; ++i) {
    FbleValueAddRef(heap, &stack->_base, args[i]);
  }

  stack->func = func;
  stack->locals.size = func->executable->code->locals;
  stack->pc = 0;

  if (localc > old_localc) {
    FbleFree(arena, stack->locals.xs);
    stack->locals.xs = FbleArrayAlloc(arena, FbleValue*, localc);
  }
  memset(stack->locals.xs, 0, stack->locals.size * sizeof(FbleValue*));

  for (size_t i = 0; i < func->argc; ++i) {
    stack->locals.xs[i] = args[i];
  }
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
  size_t scopec = func_value_instr->code->statics;

  FbleFuncValue* value = FbleNewValueExtra(heap, FbleFuncValue, sizeof(FbleValue*) * scopec);
  value->_base.tag = FBLE_FUNC_VALUE;
  value->argc = func_value_instr->argc;
  value->executable = FbleAlloc(heap->arena, FbleExecutable);
  value->executable->code = func_value_instr->code;
  value->executable->code->refcount++;
  value->executable->run = &FbleStandardRunFunction;
  for (size_t i = 0; i < scopec; ++i) {
    FbleValue* arg = FrameGet(thread, func_value_instr->scope.xs[i]);
    value->scope[i] = arg;
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
    ReplaceFrame(heap, func, args, thread);
    return FBLE_EXEC_FINISHED;
  }

  thread->stack->pc++;
  FbleValue* result = PushFrame(heap, func, args, thread);
  thread->stack->tail->locals.xs[call_instr->dest] = result;
  FbleValueAddRef(heap, &thread->stack->tail->_base, result);
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
    return FBLE_EXEC_RUNNING;
  }

  if (put_port->tag == FBLE_PORT_VALUE) {
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

  UNREACHABLE("put port must be an output or port value");
  return FBLE_EXEC_ABORTED;
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

    FbleValue* result = PushFrame(heap, arg, NULL, child);
    FrameSet(heap, thread, fork_instr->dests.xs[i], result);
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
  FbleThunkValue* rv = FbleNewValue(heap, FbleThunkValue);
  rv->_base.tag = FBLE_THUNK_VALUE;
  rv->value = NULL;
  rv->tail = NULL;
  rv->joins = 0;
  rv->func = NULL;
  rv->pc = 0;
  rv->locals.size = 0;
  rv->locals.xs = NULL;

  FrameSetAndRelease(heap, thread, ref_instr->dest, &rv->_base);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RefDefInstr -- see documentation of InstrImpl
//   Execute a REF_DEF_INSTR.
static FbleExecStatus RefDefInstr(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
  FbleThunkValue* rv = (FbleThunkValue*)thread->stack->locals.xs[ref_def_instr->ref];
  assert(rv->_base.tag == FBLE_THUNK_VALUE);
  assert(rv->value == NULL);
  assert(rv->tail == NULL
      && rv->func == NULL
      && rv->joins == 0
      && rv->pc == 0
      && rv->locals.size == 0);

  FbleValue* value = FrameGet(thread, ref_def_instr->value);
  assert(value != NULL);

  // Unwrap any accumulated layers of thunks on the returned value, and, more
  // importantly, make sure we aren't forming a vacuous value.
  FbleThunkValue* thunk = (FbleThunkValue*)value;
  while (value->tag == FBLE_THUNK_VALUE && thunk->value != NULL) {
    value = thunk->value;
    thunk = (FbleThunkValue*)value;
  }

  if (thunk == rv) {
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

  // Unwrap any layers of thunks on the result to avoid long chains of thunks.
  // TODO: Is this redundant with the thunk unwrapping we do in RefDefInstr?
  FbleThunkValue* thunk_result = (FbleThunkValue*)result;
  while (result->tag == FBLE_THUNK_VALUE && thunk_result->value != NULL) {
    result = thunk_result->value;
    thunk_result = (FbleThunkValue*)result;
  }

  // Pop the top frame from the stack.
  FbleThunkValue* thunk = thread->stack;
  thread->stack = thread->stack->tail;
  if (thread->stack != NULL) {
    FbleRetainValue(heap, &thread->stack->_base);
  }

  // Save the computed value in its thunk and clean up the computation state.
  thunk->value = result;
  FbleValueAddRef(heap, &thunk->_base, result);
  FbleFree(heap->arena, thunk->locals.xs);
  thunk->tail = NULL;
  thunk->func = NULL;
  thunk->pc = 0;
  thunk->locals.size = 0;
  thunk->locals.xs = NULL;
  FbleReleaseValue(heap, &thunk->_base);

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

// RunThread --
//   Run the given thread to completion or until it can no longer make
//   progress.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to run.
//   io_activity - set to true if the thread does any i/o activity that could
//                 unblock another thread.
//   aborted - if true, abort the thread. set to true if thread aborts.
//
// Results:
//   FBLE_EXEC_FINISHED - if the thread has finished running.
//   FBLE_EXEC_BLOCKED - if the thread is blocked on I/O.
//   FBLE_EXEC_YIELDED - if our time slice for executing instructions is over.
//   FBLE_EXEC_RUNNING - not used.
//   FBLE_EXEC_ABORTED - not used.
//
// Side effects:
// * The thread is executed, updating its stack.
// * io_activity is set to true if the thread does any i/o activity that could
//   unblock another thread.
// * aborted is set to true if the thread aborts, then FBLE_EXEC_FINISHED is returned.
static FbleExecStatus RunThread(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  FbleExecStatus status = FBLE_EXEC_FINISHED;
  while (status == FBLE_EXEC_FINISHED && thread->stack != NULL) {
    if (thread->stack->joins > 0) {
      thread->stack->joins--;
      FbleReleaseValue(heap, &thread->stack->_base);
      thread->stack = NULL;
      return FBLE_EXEC_FINISHED;
    }

    status = thread->stack->func->executable->run(heap, threads, thread, io_activity);
  }
  return status;
}

// AbortThreads --
//   Clean up threads.
//
// Inputs:
//   heap - the value heap.
//   threads - the threads to clean up.
//
// Side effects:
//   Cleans up the thread state.
static void AbortThreads(FbleValueHeap* heap, FbleThreadV* threads)
{
  for (size_t i = 0; i < threads->size; ++i) {
    FbleReleaseValue(heap, &threads->xs[i]->stack->_base);
    threads->xs[i]->stack = NULL;
    FbleFreeProfileThread(heap->arena, threads->xs[i]->profile);
    FbleFree(heap->arena, threads->xs[i]);
  }
  threads->size = 0;
}

// Eval --
//   Evaluate the given function.
//
// Inputs:
//   heap - the value heap.
//   io - io to use for external ports.
//   func - the function to evaluate.
//   args - args to pass to the function. length == func->argc.
//   profile - profile to update with execution stats. May be NULL to disable
//             profiling.
//
// Results:
//   The computed value, or NULL on error.
//
// Side effects:
//   The returned value must be freed with FbleReleaseValue when no longer in
//   use. Prints a message to stderr in case of error.
//   Updates profile based on the execution.
//   Does not take ownership of the function or the args.
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleFuncValue* func, FbleValue** args, FbleProfile* profile)
{
  FbleArena* arena = heap->arena;
  FbleThreadV threads;
  FbleVectorInit(arena, threads);

  FbleThread* main_thread = FbleAlloc(arena, FbleThread);
  main_thread->stack = NULL;
  main_thread->profile = profile == NULL ? NULL : FbleNewProfileThread(arena, profile);
  FbleVectorAppend(arena, threads, main_thread);

  FbleThunkValue* final_result = (FbleThunkValue*)PushFrame(heap, func, args, main_thread);
  assert(final_result->_base.tag == FBLE_THUNK_VALUE);
  FbleRetainValue(heap, &final_result->_base);

  while (threads.size > 0) {
    bool unblocked = false;
    for (size_t i = 0; i < threads.size; ++i) {
      FbleThread* thread = threads.xs[i];
      FbleExecStatus status = RunThread(heap, &threads, thread, &unblocked);
      switch (status) {
        case FBLE_EXEC_FINISHED: {
          unblocked = true;
          assert(thread->stack == NULL);
          FbleFreeProfileThread(arena, thread->profile);
          FbleFree(arena, thread);

          threads.xs[i] = threads.xs[threads.size - 1];
          threads.size--;
          i--;
          break;
        }

        case FBLE_EXEC_BLOCKED: {
          break;
        }

        case FBLE_EXEC_YIELDED: {
          unblocked = true;
          break;
        }

        case FBLE_EXEC_RUNNING: UNREACHABLE("unexpected status");
        case FBLE_EXEC_ABORTED: {
          AbortThreads(heap, &threads);
          FbleReleaseValue(heap, &final_result->_base);
          FbleFree(arena, threads.xs);
          return NULL;
        }
      }
    }

    bool blocked = !unblocked;
    if (!io->io(io, heap, blocked) && blocked) {
      FbleString* source = FbleNewString(arena, __FILE__);
      FbleLoc loc = { .source = source, .line = 0, .col = 0 };
      FbleReportError("deadlock\n", loc);
      FbleFreeString(arena, source);

      AbortThreads(heap, &threads);
      FbleReleaseValue(heap, &final_result->_base);
      FbleFree(arena, threads.xs);
      return NULL;
    }
  }

  // Give a chance to process any remaining io before exiting.
  io->io(io, heap, false);

  assert(final_result->value != NULL);
  FbleValue* result = final_result->value;
  if (result != NULL) {
    FbleRetainValue(heap, result);
  }
  FbleReleaseValue(heap, &final_result->_base);
  FbleFree(arena, threads.xs);
  return result;
}

// FbleStandardRunFunction -- see documentation in eval.h
FbleExecStatus FbleStandardRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  FbleArena* arena = heap->arena;
  FbleProfileThread* profile = thread->profile;

  FbleInstr** code = thread->stack->func->executable->code->instrs.xs;

  FbleExecStatus status = FBLE_EXEC_RUNNING;
  while (status == FBLE_EXEC_RUNNING) {
    FbleInstr* instr = code[thread->stack->pc];
    if (rand() % TIME_SLICE == 0) {
      if (profile != NULL) {
        FbleProfileSample(arena, profile, 1);
      }
      return FBLE_EXEC_YIELDED;
    }

    if (profile != NULL) {
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

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleValueHeap* heap, FbleValue* program, FbleProfile* profile)
{
  return FbleApply(heap, program, NULL, profile);
}

// FbleApply -- see documentation in fble.h
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile)
{
  assert(func->tag == FBLE_FUNC_VALUE);
  FbleIO io = { .io = &FbleNoIO, };
  FbleValue* result = Eval(heap, &io, (FbleFuncValue*)func, args, profile);
  return result;
}

// FbleNoIO -- See documentation in fble.h
bool FbleNoIO(FbleIO* io, FbleValueHeap* heap, bool block)
{
  return false;
}

// FbleExec -- see documentation in fble.h
FbleValue* FbleExec(FbleValueHeap* heap, FbleIO* io, FbleValue* proc, FbleProfile* profile)
{
  assert(proc->tag == FBLE_PROC_VALUE);
  return Eval(heap, io, (FbleFuncValue*)proc, NULL, profile);
}
