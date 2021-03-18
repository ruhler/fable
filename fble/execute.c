// execute.c --
//   Code for executing fble instructions, functions, and processes.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for NULL, abort, rand
#include <string.h>   // for memset

#include "fble.h"
#include "heap.h"
#include "execute.h"
#include "instr.h"
#include "syntax.h"   // for FbleReportError
#include "tc.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// TIME_SLICE --
//   The number of instructions a thread is allowed to run before switching to
//   another thread. Also used as the profiling sample period in number of
//   instructions executed.
#define TIME_SLICE 1024

typedef FbleThunkValueTc Stack;

// FbleThreadV --
//   A vector of threads.
typedef struct {
  size_t size;
  FbleThread** xs;
} FbleThreadV;

// FbleThread --
//   Represents a thread of execution.
//
// Fields:
//   stack - the execution stack.
//   profile - the profile thread associated with this thread. May be NULL to
//             disable profiling.
//   children - child threads that this thread is potentially blocked on.
//      This vector is {0, NULL} when there are no child threads.
//   next_child - the child of this thread to execute next.
//   parent - the parent of this thread.
struct FbleThread {
  Stack* stack;
  FbleProfileThread* profile;
  FbleThreadV children;
  size_t next_child;
  FbleThread* parent;
};

static FbleValue* FrameGet(FbleThread* thread, FbleFrameIndex index);
static FbleValue* FrameGetStrict(FbleThread* thread, FbleFrameIndex index);
static void FrameSet(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value);
static void FrameSetAndRelease(FbleValueHeap* heap, FbleThread* thread, FbleLocalIndex index, FbleValue* value);

static FbleValue* PushFrame(FbleValueHeap* heap, FbleCompiledFuncValueTc* func, FbleValue** args, FbleThread* thread);
static void ReplaceFrame(FbleValueHeap* heap, FbleCompiledFuncValueTc* func, FbleValue** args, FbleThread* thread);

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
//   FBLE_EXEC_CONTINUE - to indicate the frame has just been replaced by its tail
//              call.
//   FBLE_EXEC_ABORTED - if the thread should be aborted.
//
// Side effects:
//   Executes the instruction.
typedef FbleExecStatus (*InstrImpl)(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);

static FbleExecStatus StructValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus UnionValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus StructAccessInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus UnionAccessInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus UnionSelectInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus JumpInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus FuncValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus CallInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus GetInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus PutInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus CopyInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus LinkInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus ForkInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RefValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus RefDefInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus ReturnInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);
static FbleExecStatus TypeInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity);

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

static FbleExecStatus RunFunction(FbleValueHeap* heap, FbleThread* thread, bool* io_activity);
static FbleExecStatus RunFbleThread(FbleValueHeap* heap, FbleThread* thread, bool* io_activity, bool* aborted);
static FbleExecStatus AbortFbleThread(FbleValueHeap* heap, FbleThread* thread, bool* aborted);
static FbleExecStatus RunFbleThreads(FbleValueHeap* heap, FbleThread* thread, bool* aborted);
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleCompiledFuncValueTc* func, FbleValue** args, FbleProfile* profile);

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
static FbleValue* PushFrame(FbleValueHeap* heap, FbleCompiledFuncValueTc* func, FbleValue** args, FbleThread* thread)
{
  FbleArena* arena = heap->arena;

  size_t locals = func->code->locals;

  Stack* stack = FbleNewValue(heap, FbleThunkValueTc);
  stack->_base.tag = FBLE_THUNK_VALUE_TC;
  stack->value = NULL;
  stack->tail = thread->stack;
  stack->func = func;
  stack->pc = 0;
  stack->locals.size = func->code->locals;
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
static void ReplaceFrame(FbleValueHeap* heap, FbleCompiledFuncValueTc* func, FbleValue** args, FbleThread* thread)
{
  FbleArena* arena = heap->arena;
  Stack* stack = thread->stack;
  size_t old_localc = stack->func->code->locals;
  size_t localc = func->code->locals;

  // Take references to the function and arguments early to ensure we don't
  // drop the last reference to them before we get the chance.
  FbleValueAddRef(heap, &stack->_base, &func->_base);
  for (size_t i = 0; i < func->argc; ++i) {
    FbleValueAddRef(heap, &stack->_base, args[i]);
  }

  stack->func = func;
  stack->locals.size = func->code->locals;
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
static FbleExecStatus StructValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
  size_t argc = struct_value_instr->args.size;

  FbleStructValueTc* value = FbleNewValueExtra(heap, FbleStructValueTc, sizeof(FbleValue*) * argc);
  value->_base.tag = FBLE_STRUCT_VALUE_TC;
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
static FbleExecStatus UnionValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;

  FbleUnionValueTc* value = FbleNewValue(heap, FbleUnionValueTc);
  value->_base.tag = FBLE_UNION_VALUE_TC;
  value->tag = union_value_instr->tag;
  value->arg = FrameGet(thread, union_value_instr->arg);
  FbleValueAddRef(heap, &value->_base, value->arg);

  FrameSetAndRelease(heap, thread, union_value_instr->dest, &value->_base);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// StructAccessInstr -- see documentation of InstrImpl
//   Execute a STRUCT_ACCESS_INSTR.
static FbleExecStatus StructAccessInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

  FbleStructValueTc* sv = (FbleStructValueTc*)FrameGetStrict(thread, access_instr->obj);
  if (sv == NULL) {
    FbleReportError("undefined struct value access\n", access_instr->loc);
    return FBLE_EXEC_ABORTED;
  }

  assert(sv->_base.tag == FBLE_STRUCT_VALUE_TC);
  assert(access_instr->tag < sv->fieldc);
  FrameSet(heap, thread, access_instr->dest, sv->fields[access_instr->tag]);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// UnionAccessInstr -- see documentation of InstrImpl
//   Execute a UNION_ACCESS_INSTR.
static FbleExecStatus UnionAccessInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

  FbleUnionValueTc* uv = (FbleUnionValueTc*)FrameGetStrict(thread, access_instr->obj);
  if (uv == NULL) {
    FbleReportError("undefined union value access\n", access_instr->loc);
    return FBLE_EXEC_ABORTED;
  }

  assert(uv->_base.tag == FBLE_UNION_VALUE_TC);
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
static FbleExecStatus UnionSelectInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
  FbleUnionValueTc* uv = (FbleUnionValueTc*)FrameGetStrict(thread, select_instr->condition);
  if (uv == NULL) {
    FbleReportError("undefined union value select\n", select_instr->loc);
    return FBLE_EXEC_ABORTED;
  }
  assert(uv->_base.tag == FBLE_UNION_VALUE_TC);
  thread->stack->pc += 1 + select_instr->jumps.xs[uv->tag];
  return FBLE_EXEC_RUNNING;
}

// JumpInstr -- see documentation of InstrImpl
//   Execute a JUMP_INSTR.
static FbleExecStatus JumpInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
  thread->stack->pc += 1 + jump_instr->count;
  return FBLE_EXEC_RUNNING;
}

// FuncValueInstr -- see documentation of InstrImpl
//   Execute a FUNC_VALUE_INSTR.
static FbleExecStatus FuncValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
  size_t scopec = func_value_instr->code->statics;

  FbleCompiledFuncValueTc* value = FbleNewValueExtra(heap, FbleCompiledFuncValueTc, sizeof(FbleValue*) * scopec);
  value->_base.tag = FBLE_COMPILED_FUNC_VALUE_TC;
  value->argc = func_value_instr->argc;
  value->code = func_value_instr->code;
  value->code->refcount++;
  value->run = &FbleStandardRunFunction;
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
static FbleExecStatus CallInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleCallInstr* call_instr = (FbleCallInstr*)instr;
  FbleCompiledFuncValueTc* func = (FbleCompiledFuncValueTc*)FrameGetStrict(thread, call_instr->func);
  if (func == NULL) {
    FbleReportError("called undefined function\n", call_instr->loc);
    return FBLE_EXEC_ABORTED;
  };
  assert(func->_base.tag == FBLE_COMPILED_FUNC_VALUE_TC);

  FbleValue* args[func->argc];
  for (size_t i = 0; i < func->argc; ++i) {
    args[i] = FrameGet(thread, call_instr->args.xs[i]);
  }

  if (call_instr->exit) {
    ReplaceFrame(heap, func, args, thread);
    return FBLE_EXEC_CONTINUE;
  }

  thread->stack->pc++;
  FbleValue* result = PushFrame(heap, func, args, thread);
  thread->stack->tail->locals.xs[call_instr->dest] = result;
  FbleValueAddRef(heap, &thread->stack->tail->_base, result);
  return RunFunction(heap, thread, io_activity);
}

// GetInstr -- see documentation of InstrImpl
//   Execute a GET_INSTR.
static FbleExecStatus GetInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleGetInstr* get_instr = (FbleGetInstr*)instr;
  FbleValue* get_port = FrameGet(thread, get_instr->port);
  if (get_port->tag == FBLE_LINK_VALUE_TC) {
    FbleLinkValueTc* link = (FbleLinkValueTc*)get_port;

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

  if (get_port->tag == FBLE_PORT_VALUE_TC) {
    FblePortValueTc* port = (FblePortValueTc*)get_port;
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
static FbleExecStatus PutInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FblePutInstr* put_instr = (FblePutInstr*)instr;
  FbleValue* put_port = FrameGet(thread, put_instr->port);
  FbleValue* arg = FrameGet(thread, put_instr->arg);

  FbleValueV args = { .size = 0, .xs = NULL, };
  FbleValue* unit = FbleNewStructValue(heap, args);

  if (put_port->tag == FBLE_LINK_VALUE_TC) {
    FbleLinkValueTc* link = (FbleLinkValueTc*)put_port;

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

  if (put_port->tag == FBLE_PORT_VALUE_TC) {
    FblePortValueTc* port = (FblePortValueTc*)put_port;

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
static FbleExecStatus ForkInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleForkInstr* fork_instr = (FbleForkInstr*)instr;

  assert(thread->children.size == 0);
  assert(thread->children.xs == NULL);
  FbleVectorInit(heap->arena, thread->children);

  for (size_t i = 0; i < fork_instr->args.size; ++i) {
    FbleCompiledProcValueTc* arg = (FbleCompiledProcValueTc*)FrameGetStrict(thread, fork_instr->args.xs[i]);

    // You cannot execute a proc in a let binding, so it should be
    // impossible to ever have an undefined proc value.
    assert(arg != NULL && "undefined proc value");
    assert(arg->_base.tag == FBLE_COMPILED_PROC_VALUE_TC);

    FbleThread* child = FbleAlloc(heap->arena, FbleThread);
    child->stack = NULL;
    child->profile = thread->profile == NULL ? NULL : FbleForkProfileThread(heap->arena, thread->profile);
    child->children.size = 0;
    child->children.xs = NULL;
    child->next_child = 0;
    child->parent = thread;
    FbleVectorAppend(heap->arena, thread->children, child);

    FbleValue* result = PushFrame(heap, arg, NULL, child);
    FrameSet(heap, thread, fork_instr->dests.xs[i], result);
  }
  thread->next_child = 0;
  thread->stack->pc++;
  return FBLE_EXEC_YIELDED;
}

// CopyInstr -- see documentation of InstrImpl
//   Execute a COPY_INSTR.
static FbleExecStatus CopyInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
  FbleValue* value = FrameGet(thread, copy_instr->source);
  FrameSet(heap, thread, copy_instr->dest, value);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// LinkInstr -- see documentation of InstrImpl
//   Execute a LINK_INSTR.
static FbleExecStatus LinkInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;

  FbleLinkValueTc* link = FbleNewValue(heap, FbleLinkValueTc);
  link->_base.tag = FBLE_LINK_VALUE_TC;
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
static FbleExecStatus RefValueInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
  FbleThunkValueTc* rv = FbleNewValue(heap, FbleThunkValueTc);
  rv->_base.tag = FBLE_THUNK_VALUE_TC;
  rv->value = NULL;
  rv->tail = NULL;
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
static FbleExecStatus RefDefInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
  FbleThunkValueTc* rv = (FbleThunkValueTc*)thread->stack->locals.xs[ref_def_instr->ref];
  assert(rv->_base.tag == FBLE_THUNK_VALUE_TC);
  assert(rv->value == NULL);
  assert(rv->tail == NULL && rv->func == NULL && rv->pc == 0 && rv->locals.size == 0);

  FbleValue* value = FrameGet(thread, ref_def_instr->value);
  assert(value != NULL);

  // Unwrap any accumulated layers of thunks on the returned value, and, more
  // importantly, make sure we aren't forming a vacuous value.
  FbleThunkValueTc* thunk = (FbleThunkValueTc*)value;
  while (value->tag == FBLE_THUNK_VALUE_TC && thunk->value != NULL) {
    value = thunk->value;
    thunk = (FbleThunkValueTc*)value;
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
static FbleExecStatus ReturnInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
  FbleValue* result = FrameGet(thread, return_instr->result);

  // Unwrap any layers of thunks on the result to avoid long chains of thunks.
  // TODO: Is this redundant with the thunk unwrapping we do in RefDefInstr?
  FbleThunkValueTc* thunk_result = (FbleThunkValueTc*)result;
  while (result->tag == FBLE_THUNK_VALUE_TC && thunk_result->value != NULL) {
    result = thunk_result->value;
    thunk_result = (FbleThunkValueTc*)result;
  }

  // Pop the top frame from the stack.
  FbleThunkValueTc* thunk = thread->stack;
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
static FbleExecStatus TypeInstr(FbleValueHeap* heap, FbleThread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
  FbleTypeValueTc* value = FbleNewValue(heap, FbleTypeValueTc);
  value->_base.tag = FBLE_TYPE_VALUE_TC;
  FrameSetAndRelease(heap, thread, type_instr->dest, &value->_base);
  thread->stack->pc++;
  return FBLE_EXEC_RUNNING;
}

// RunFunction --
//   Invokes a compiled function's FbleRunFunction repeatedly as necessary 
//   process all the tail calls.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to run.
//   io_activity - set to true if the thread does any i/o activity that could
//                 unblock another thread.
//
// Results:
//   FBLE_EXEC_FINISHED - if we have just returned from the current stack frame.
//   FBLE_EXEC_BLOCKED - if the thread is blocked on I/O.
//   FBLE_EXEC_YIELDED - if our time slice for executing instructions is over.
//   FBLE_EXEC_RUNNING - not used.
//   FBLE_EXEC_CONTINUE - not used.
//   FBLE_EXEC_ABORTED - if the thread should be aborted.
//
// Side effects:
// * The function is executed, updating its stack.
// * io_activity is set to true if the thread does any i/o activity that could
//   unblock another thread.
static FbleExecStatus RunFunction(FbleValueHeap* heap, FbleThread* thread, bool* io_activity)
{
  FbleExecStatus status = FBLE_EXEC_CONTINUE;
  while (status == FBLE_EXEC_CONTINUE) {
    status = thread->stack->func->run(heap, thread, io_activity);
  }
  return status;
}

// RunFbleThread --
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
//   FBLE_EXEC_CONTINUE - not used.
//   FBLE_EXEC_ABORTED - not used.
//
// Side effects:
// * The thread is executed, updating its stack.
// * io_activity is set to true if the thread does any i/o activity that could
//   unblock another thread.
// * aborted is set to true if the thread aborts, then FBLE_EXEC_FINISHED is returned.
static FbleExecStatus RunFbleThread(FbleValueHeap* heap, FbleThread* thread, bool* io_activity, bool* aborted)
{
  if (*aborted) {
    return AbortFbleThread(heap, thread, aborted);
  }

  while (thread->stack != NULL) {
    FbleExecStatus status = RunFunction(heap, thread, io_activity);

    if (status == FBLE_EXEC_FINISHED) {
      continue;
    }

    if (status == FBLE_EXEC_ABORTED) {
      return AbortFbleThread(heap, thread, aborted);
    }

    return status;
  }
  return FBLE_EXEC_FINISHED;
}

// AbortFbleThread --
//   Unwind the given thread, cleaning the stack and children threads as
//   appropriate.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to abort.
//   aborted - set to true.
//
// Results:
//   FBLE_EXEC_FINISHED
//
// Side effects:
//   Cleans up the thread state. The state of the thread after this function
//   is the same as if it had completed normally, except that it will not have
//   produced a return value. Sets aborted to true.
static FbleExecStatus AbortFbleThread(FbleValueHeap* heap, FbleThread* thread, bool* aborted)
{
  *aborted = true;
  if (thread->stack != NULL) {
    FbleReleaseValue(heap, &thread->stack->_base);
    thread->stack = NULL;
  }
  return FBLE_EXEC_FINISHED;
}

// RunFbleThreads --
//   Run the given thread and its children to completion or until it can no
//   longer make progress.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to run.
//   aborted - aborts threads if set to true.
//             set to true if the thread is aborted.
//
// Results:
//   FBLE_EXEC_FINISHED - if we have finished running the threads.
//   FBLE_EXEC_BLOCKED - if we are blocked on I/O.
//   FBLE_EXEC_YIELDED - if our time slice for executing instructions is over.
//   FBLE_EXEC_RUNNING - not used.
//   FBLE_EXEC_CONTINUE - not used.
//   FBLE_EXEC_ABORTED - not used.
//
// Side effects:
//   The thread and its children are executed and updated.
//   Updates the profile based on the execution.
//   Sets aborted to true in case of abort.
static FbleExecStatus RunFbleThreads(FbleValueHeap* heap, FbleThread* thread, bool* aborted)
{
  FbleArena* arena = heap->arena;

  bool unblocked = false;
  while (thread != NULL) {
    if (thread->children.size > 0) {
      if (thread->next_child == thread->children.size) {
        thread->next_child = 0;
        thread = thread->parent;
      } else {
        // RunFbleThreads on the next child thread.
        thread = thread->children.xs[thread->next_child++];
      }
    } else {
      // Run the leaf thread, then go back up to the parent for the rest of
      // the threads to process.
      FbleExecStatus status = RunFbleThread(heap, thread, &unblocked, aborted);
      switch (status) {
        case FBLE_EXEC_FINISHED: {
          unblocked = true;
          if (thread->parent == NULL) {
            return FBLE_EXEC_FINISHED;
          }

          FbleThread* parent = thread->parent;
          parent->next_child--;
          parent->children.size--;
          assert(parent->children.xs[parent->next_child] == thread);
          parent->children.xs[parent->next_child] = parent->children.xs[parent->children.size];

          if (parent->children.size == 0) {
            FbleFree(arena, parent->children.xs);
            parent->children.xs = NULL;
          }

          assert(thread->stack == NULL);
          assert(thread->children.size == 0);
          assert(thread->children.xs == NULL);
          FbleFreeProfileThread(arena, thread->profile);
          FbleFree(arena, thread);
          thread = parent;
          break;
        }

        case FBLE_EXEC_BLOCKED: {
          thread = thread->parent;
          break;
        }

        case FBLE_EXEC_YIELDED: {
          unblocked = true;
          thread = thread->parent;
          break;
        }

        case FBLE_EXEC_RUNNING: UNREACHABLE("unexpected status"); break;
        case FBLE_EXEC_ABORTED: UNREACHABLE("unexpected status"); break;
        case FBLE_EXEC_CONTINUE: UNREACHABLE("unexpected status"); break;
      }
    }
  }
  return unblocked ? FBLE_EXEC_YIELDED : FBLE_EXEC_BLOCKED;
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
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleCompiledFuncValueTc* func, FbleValue** args, FbleProfile* profile)
{
  FbleArena* arena = heap->arena;
  FbleThread thread = {
    .stack = NULL,
    .profile = profile == NULL ? NULL : FbleNewProfileThread(arena, profile),
    .children = {0, NULL},
    .next_child = 0,
    .parent = NULL,
  };
  FbleThunkValueTc* final_result = (FbleThunkValueTc*)PushFrame(heap, func, args, &thread);
  assert(final_result->_base.tag == FBLE_THUNK_VALUE_TC);
  FbleRetainValue(heap, &final_result->_base);

  bool aborted = false;
  while (true) {
    FbleExecStatus status = RunFbleThreads(heap, &thread, &aborted);
    switch (status) {
      case FBLE_EXEC_FINISHED: {
        assert(aborted || final_result->value != NULL);
        assert(thread.stack == NULL);
        assert(thread.children.size == 0);
        assert(thread.children.xs == NULL);

        // Give a chance to process any remaining io before exiting.
        io->io(io, heap, false);

        FbleFreeProfileThread(arena, thread.profile);

        FbleValue* result = final_result->value;
        if (result != NULL) {
          FbleRetainValue(heap, result);
        }
        FbleReleaseValue(heap, &final_result->_base);
        return result;
      }

      case FBLE_EXEC_BLOCKED: {
        // The thread is not making forward progress anymore. Block for I/O.
        if (!io->io(io, heap, true)) {
          FbleString* source = FbleNewString(arena, __FILE__);
          FbleLoc loc = { .source = source, .line = 0, .col = 0 };
          FbleReportError("deadlock\n", loc);
          FbleFreeString(arena, source);
          aborted = true;
        }
        break;
      }

      case FBLE_EXEC_YIELDED: {
        // Give a chance to do some I/O, but don't block, because the thread
        // is making progress anyway.
        io->io(io, heap, false);
        break;
      }

      case FBLE_EXEC_RUNNING: UNREACHABLE("unexpected status"); break;
      case FBLE_EXEC_ABORTED: UNREACHABLE("unexpected status"); break;
      case FBLE_EXEC_CONTINUE: UNREACHABLE("unexpected status"); break;
    }
  }

  UNREACHABLE("should never get here");
  return NULL;
}

// FbleStandardRunFunction -- see documentation in eval.h
FbleExecStatus FbleStandardRunFunction(FbleValueHeap* heap, FbleThread* thread, bool* io_activity)
{
  FbleArena* arena = heap->arena;
  FbleProfileThread* profile = thread->profile;

  FbleInstr** code = thread->stack->func->code->instrs.xs;

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

    status = sInstrImpls[instr->tag](heap, thread, instr, io_activity);
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
  assert(func->tag == FBLE_COMPILED_FUNC_VALUE_TC);
  FbleIO io = { .io = &FbleNoIO, };
  FbleValue* result = Eval(heap, &io, (FbleCompiledFuncValueTc*)func, args, profile);
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
  assert(proc->tag == FBLE_COMPILED_PROC_VALUE_TC);
  return Eval(heap, io, (FbleCompiledFuncValueTc*)proc, NULL, profile);
}
