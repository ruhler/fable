// eval.c --
//   This file implements the fble evaluation routines.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for NULL, abort, rand
#include <string.h>   // for memset

#include "fble.h"
#include "heap.h"
#include "instr.h"
#include "syntax.h"   // for FbleReportError
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// TIME_SLICE --
//   The number of instructions a thread is allowed to run before switching to
//   another thread. Also used as the profiling sample period in number of
//   instructions executed.
#define TIME_SLICE 1024

// Stack --
//   An execution stack.
//
// Fields:
//   tail - The next frame down in the stack.
//   result - Where to store the result when exiting the stack frame.
//   owner - if true, we own the function and args. Otherwise we don't.
//   func - The function being executed.
//   pc - The location of the next instruction in the code to execute.
//   locals - Space allocated for local variables.
//      Has length at least func->code->locals values.
//      The initial func->argc locals are arguments that may or may not be
//      owned by the stack as specified by 'owner'. The rest of the locals are
//      owned by this Stack.
typedef struct Stack {
  struct Stack* tail;
  FbleValue** result;
  bool owner;
  FbleFuncValue* func;
  FbleInstr** pc;
  FbleValue* locals[];
} Stack;

typedef struct Thread Thread;

// ThreadV --
//   A vector of threads.
typedef struct {
  size_t size;
  Thread** xs;
} ThreadV;

// Thread --
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
struct Thread {
  Stack* stack;
  FbleProfileThread* profile;
  ThreadV children;
  size_t next_child;
  Thread* parent;
};

// Status -- 
//   The status after running a thread.
typedef enum {
  FINISHED,       // The thread has finished running.
  BLOCKED,        // The thread is blocked on I/O.
  YIELDED,        // The thread yielded, but is not blocked on I/O.

  // Relevant only to RunThread:
  RUNNING,        // The thread is actively running.
  ABORTED,        // The thread needs to be aborted.
} Status;

static FbleValue* FrameGet(Thread* thread, FbleFrameIndex index);
static FbleValue* FrameGetStrict(Thread* thread, FbleFrameIndex index);
static FbleValue* FrameMove(FbleHeap* heap, Thread* thread, FbleFrameIndex index, size_t argc, bool owner);
static void FrameRelease(FbleHeap* heap, Thread* thread, FbleFrameIndex index, size_t argc, bool owner);

static Stack* PushFrame(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleValue** result, Stack* tail);
static Stack* PopFrame(FbleValueHeap* heap, Stack* stack);
static Stack* ReplaceFrame(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, Stack* stack);

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
//   The status of the thread resulting from execution of the function.
//
// Side effects:
//   Executes the release instruction.
typedef Status (*InstrImpl)(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);

static Status StructValueInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status UnionValueInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status StructAccessInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status UnionAccessInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status UnionSelectInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status JumpInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status FuncValueInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status ReleaseInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status CallInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status GetInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status PutInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status CopyInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status LinkInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status ForkInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status RefValueInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status RefDefInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status ReturnInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status TypeInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status SymbolicValueInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);
static Status SymbolicCompileInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity);

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
  &ReleaseInstr,          // FBLE_RELEASE_INSTR
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
  &SymbolicValueInstr,    // FBLE_SYMBOLIC_VALUE_INSTR
  &SymbolicCompileInstr,  // FBLE_SYMBOLIC_COMPILE_INSTR
};

static Status RunThread(FbleValueHeap* heap, Thread* thread, bool* io_activity, bool* aborted);
static Status AbortThread(FbleValueHeap* heap, Thread* thread, bool* aborted);
static Status RunThreads(FbleValueHeap* heap, Thread* thread, bool* aborted);
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
static FbleValue* FrameGet(Thread* thread, FbleFrameIndex index)
{
  switch (index.section) {
    case FBLE_STATICS_FRAME_SECTION: return thread->stack->func->scope[index.index];
    case FBLE_LOCALS_FRAME_SECTION: return thread->stack->locals[index.index];
  }
  UNREACHABLE("should never get here");
}

// FrameGetStrict --
//   Get and dereference a value from the frame at the top of the given stack.
//   Dereferences the data value, removing all layers of reference values
//   until a non-reference value is encountered and returns the non-reference
//   value.
//
// Inputs:
//   thread - the current thread.
//   index - the location of the value in the frame.
//
// Results:
//   The dereferenced value. Returns null in case of abstract value
//   dereference.
//
// Side effects:
//   The returned value will only stay alive as long as the original value on
//   the stack frame.
static FbleValue* FrameGetStrict(Thread* thread, FbleFrameIndex index)
{
  FbleValue* value = FrameGet(thread, index);
  while (value->tag == FBLE_REF_VALUE) {
    FbleRefValue* rv = (FbleRefValue*)value;

    if (rv->value == NULL) {
      // We are trying to dereference an abstract value. This is undefined
      // behavior according to the spec. Return NULL to indicate to the
      // caller.
      return NULL;
    }

    value = rv->value;
  }
  return value;
}

// FrameMove --
//   Get a value from the frame on the top of the execution stack, transfering
//   ownership of the value if we have ownership of it.
//
// Inputs:
//   heap - heap used for allocations.
//   thread - the current thread.
//   index - the index of the value to access.
//   argc - the number of arguments to the currently executing function.
//   owner - whether we own the arguments to the currently executing function.
//
// Results:
//   The value in the top stack frame at the given index.
//
// Side effects:
//   Has the effect of taking a reference to the value and releasing the value
//   on the stack frame if we have ownership of it.
static FbleValue* FrameMove(FbleHeap* heap, Thread* thread, FbleFrameIndex index, size_t argc, bool owner)
{
  switch (index.section) {
    case FBLE_STATICS_FRAME_SECTION: {
      FbleValue* value = thread->stack->func->scope[index.index];
      FbleRetainValue(heap, value);
      return value;
    }

    case FBLE_LOCALS_FRAME_SECTION: {
      FbleValue* value = thread->stack->locals[index.index];
      if (index.index < argc && !owner) {
        FbleRetainValue(heap, value);
      }
      thread->stack->locals[index.index] = NULL;
      return value;
    }
  }

  UNREACHABLE("should never get here");
  return NULL;
}

// FrameRelease --
//   Release a value on the current stack frame if we have ownership of it.
//
// Inputs:
//   heap - heap used for allocations.
//   thread - the current thread.
//   index - the index of the value to access.
//   argc - the number of arguments to the currently executing function.
//   owner - whether we own the arguments to the currently executing function.
//
// Side effects:
//   Has the effect of releasing the value on the stack frame if we have
//   ownership of it.
static void FrameRelease(FbleHeap* heap, Thread* thread, FbleFrameIndex index, size_t argc, bool owner)
{
  if (index.section == FBLE_LOCALS_FRAME_SECTION) {
    if (index.index >= argc || owner) {
      FbleReleaseValue(heap, thread->stack->locals[index.index]);
    }
    thread->stack->locals[index.index] = NULL;
  }
}

// PushFrame --
//   Push a frame onto the execution stack.
//
// Inputs:
//   arena - the arena to use for allocations
//   func - the function to execute.
//   args - arguments to pass to the function. length == func->argc.
//   result - the return address.
//   tail - the stack to push to
//
// Result:
//   The stack with new pushed frame.
//
// Side effects:
//   Allocates new Stack instances that should be freed with PopFrame when done.
//   Does not take ownership of the function or the args.
//   It is the caller's responsibility to ensure the function and args remain
//   valid until the frame is popped.
static Stack* PushFrame(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleValue** result, Stack* tail)
{
  FbleArena* arena = heap->arena;

  size_t locals = func->code->locals;

  Stack* stack = FbleAllocExtra(arena, Stack, locals * sizeof(FbleValue*));
  stack->tail = tail;
  stack->result = result;
  stack->owner = false;
  stack->func = func;
  stack->pc = func->code->instrs.xs;
  memset(stack->locals, 0, locals * sizeof(FbleValue*));

  for (size_t i = 0; i < func->argc; ++i) {
    stack->locals[i] = args[i];
  }
  return stack;
}

// PopFrame --
//   Pop a frame off the execution stack.
//
// Inputs:
//   heap - the value heap
//   stack - the stack to pop from
//
// Results:
//   The popped stack.
//
// Side effects:
//   Releases the current thunk and any remaining variables on the frame and
//   frees the frame.
static Stack* PopFrame(FbleValueHeap* heap, Stack* stack)
{
  FbleArena* arena = heap->arena;
  if (stack->owner) {
    FbleReleaseValue(heap, &stack->func->_base);
  }

  Stack* tail = stack->tail;
  FbleFree(arena, stack);
  return tail;
}

// ReplaceFrame --
//   Replace the current frame with a new one.
//
// Inputs:
//   heap - the value heap.
//   func - the function to execute. Consumed..
//   args - args to the function. length == func->argc. args[i] is Consumed.
//   tail - the stack to change.
//
// Result:
//   The stack with new frame.
//
// Side effects:
//   Allocates new Stack instances that should be freed with PopFrame when done.
//   Takes ownership of func and args.
//   Exits the current frame, which potentially frees any instructions
//   belonging to that frame.
static Stack* ReplaceFrame(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, Stack* stack)
{
  FbleArena* arena = heap->arena;

  size_t old_locals = stack->func->code->locals;
  if (stack->owner) {
    FbleReleaseValue(heap, &stack->func->_base);
  }

  size_t locals = func->code->locals;
  if (locals > old_locals) {
    Stack* nstack = FbleAllocExtra(arena, Stack, locals * sizeof(FbleValue*));
    nstack->tail = stack->tail;
    nstack->result = stack->result;

    FbleFree(arena, stack);
    stack = nstack;
    memset(stack->locals + old_locals, 0, (locals - old_locals) * sizeof(FbleValue*));
  }

  stack->owner = true;
  stack->func = func;
  stack->pc = func->code->instrs.xs;

  for (size_t i = 0; i < func->argc; ++i) {
    stack->locals[i] = args[i];
  }

  return stack;
}

// StructValueInstr -- see documentation of InstrImpl.
//   Execute a STRUCT_VALUE_INSTR.
static Status StructValueInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
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

  thread->stack->locals[struct_value_instr->dest] = &value->_base;
  return RUNNING;
}

// UnionValueInstr -- see documentation of InstrImpl
//   Execute a UNION_VALUE_INSTR.
static Status UnionValueInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;

  FbleUnionValue* value = FbleNewValue(heap, FbleUnionValue);
  value->_base.tag = FBLE_UNION_VALUE;
  value->tag = union_value_instr->tag;
  value->arg = FrameGet(thread, union_value_instr->arg);
  FbleValueAddRef(heap, &value->_base, value->arg);

  thread->stack->locals[union_value_instr->dest] = &value->_base;
  return RUNNING;
}

// StructAccessInstr -- see documentation of InstrImpl
//   Execute a STRUCT_ACCESS_INSTR.
static Status StructAccessInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

  FbleStructValue* sv = (FbleStructValue*)FrameGetStrict(thread, access_instr->obj);
  if (sv == NULL) {
    FbleReportError("undefined struct value access\n", access_instr->loc);
    return ABORTED;
  }

  if (sv->_base.tag == FBLE_STRUCT_VALUE) {
    assert(access_instr->tag < sv->fieldc);
    FbleValue* value = sv->fields[access_instr->tag];
    FbleRetainValue(heap, value);
    thread->stack->locals[access_instr->dest] = value;
    return RUNNING;
  }

  // The argument must be symbolic. Create a symbolic struct access value.
  FbleDataAccessValue* value = FbleNewValue(heap, FbleDataAccessValue);
  value->_base.tag = FBLE_DATA_ACCESS_VALUE;
  value->datatype = FBLE_STRUCT_DATATYPE;
  value->obj = &sv->_base;
  value->tag = access_instr->tag;
  value->loc = FbleCopyLoc(access_instr->loc);
  FbleValueAddRef(heap, &value->_base, &sv->_base);
  thread->stack->locals[access_instr->dest] = &value->_base;
  return RUNNING;
}

// UnionAccessInstr -- see documentation of InstrImpl
//   Execute a UNION_ACCESS_INSTR.
static Status UnionAccessInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

  FbleUnionValue* uv = (FbleUnionValue*)FrameGetStrict(thread, access_instr->obj);
  if (uv == NULL) {
    FbleReportError("undefined union value access\n", access_instr->loc);
    return ABORTED;
  }

  if (uv->_base.tag == FBLE_UNION_VALUE) {
    if (uv->tag != access_instr->tag) {
      FbleReportError("union field access undefined: wrong tag\n", access_instr->loc);
      return ABORTED;
    }

    FbleRetainValue(heap, uv->arg);
    thread->stack->locals[access_instr->dest] = uv->arg;
    return RUNNING;
  }

  // The argument must be symbolic. Create a symbolic union access value.
  FbleDataAccessValue* value = FbleNewValue(heap, FbleDataAccessValue);
  value->_base.tag = FBLE_DATA_ACCESS_VALUE;
  value->datatype = FBLE_UNION_DATATYPE;
  value->obj = &uv->_base;
  value->tag = access_instr->tag;
  value->loc = FbleCopyLoc(access_instr->loc);
  FbleValueAddRef(heap, &value->_base, &uv->_base);
  thread->stack->locals[access_instr->dest] = &value->_base;
  return RUNNING;
}

// UnionSelectInstr -- see documentation of InstrImpl
//   Execute a UNION_SELECT_INSTR.
static Status UnionSelectInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
  FbleUnionValue* uv = (FbleUnionValue*)FrameGetStrict(thread, select_instr->condition);
  if (uv == NULL) {
    FbleReportError("undefined union value select\n", select_instr->loc);
    return ABORTED;
  }
  assert(uv->_base.tag == FBLE_UNION_VALUE);
  thread->stack->pc += select_instr->jumps.xs[uv->tag];
  return RUNNING;
}

// JumpInstr -- see documentation of InstrImpl
//   Execute a JUMP_INSTR.
static Status JumpInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
  thread->stack->pc += jump_instr->count;
  return RUNNING;
}

// FuncValueInstr -- see documentation of InstrImpl
//   Execute a FUNC_VALUE_INSTR.
static Status FuncValueInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
  size_t scopec = func_value_instr->code->statics;

  FbleFuncValue* value = FbleNewValueExtra(heap, FbleFuncValue, sizeof(FbleValue*) * scopec);
  value->_base.tag = FBLE_FUNC_VALUE;
  value->argc = func_value_instr->argc;
  value->code = func_value_instr->code;
  value->code->refcount++;
  for (size_t i = 0; i < scopec; ++i) {
    FbleValue* arg = FrameGet(thread, func_value_instr->scope.xs[i]);
    value->scope[i] = arg;
    FbleValueAddRef(heap, &value->_base, arg);
  }
  thread->stack->locals[func_value_instr->dest] = &value->_base;
  return RUNNING;
}

// ReleaseInstr -- see documentation of InstrImpl
//   Execute a RELEASE_INSTR.
static Status ReleaseInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleReleaseInstr* release = (FbleReleaseInstr*)instr;
  assert(thread->stack->locals[release->value] != NULL);
  if (release->value >= thread->stack->func->argc || thread->stack->owner) {
    FbleReleaseValue(heap, thread->stack->locals[release->value]);
  }
  thread->stack->locals[release->value] = NULL;
  return RUNNING;
}

// CallInstr -- see documentation of InstrImpl
//   Execute a CALL_INSTR.
static Status CallInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleCallInstr* call_instr = (FbleCallInstr*)instr;
  FbleFuncValue* func = (FbleFuncValue*)FrameGetStrict(thread, call_instr->func);
  if (func == NULL) {
    FbleReportError("called undefined function\n", call_instr->loc);
    return ABORTED;
  };
  assert(func->_base.tag == FBLE_FUNC_VALUE);


  if (call_instr->exit) {
    FbleRetainValue(heap, &func->_base);
    FrameRelease(heap, thread, call_instr->func, thread->stack->func->argc, thread->stack->owner);

    FbleValue* args[func->argc];
    for (size_t i = 0; i < func->argc; ++i) {
      args[i] = FrameMove(heap, thread, call_instr->args.xs[i], thread->stack->func->argc, thread->stack->owner);
    }

    thread->stack = ReplaceFrame(heap, func, args, thread->stack);
  } else {
    FbleValue* args[func->argc];
    for (size_t i = 0; i < func->argc; ++i) {
      args[i] = FrameGet(thread, call_instr->args.xs[i]);
    }

    FbleValue** result = thread->stack->locals + call_instr->dest;
    thread->stack = PushFrame(heap, func, args, result, thread->stack);
  }
  return RUNNING;
}

// GetInstr -- see documentation of InstrImpl
//   Execute a GET_INSTR.
static Status GetInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleGetInstr* get_instr = (FbleGetInstr*)instr;
  FbleValue* get_port = FrameGet(thread, get_instr->port);
  if (get_port->tag == FBLE_LINK_VALUE) {
    FbleLinkValue* link = (FbleLinkValue*)get_port;

    if (link->head == NULL) {
      // Blocked on get. Restore the thread state and return before
      // logging progress.
      assert(instr->profile_ops == NULL);
      return BLOCKED;
    }

    FbleValues* head = link->head;
    link->head = link->head->next;
    if (link->head == NULL) {
      link->tail = NULL;
    }

    FbleRetainValue(heap, head->value);
    thread->stack->locals[get_instr->dest] = head->value;
    FbleValueDelRef(heap, &link->_base, head->value);
    FbleFree(heap->arena, head);
    return RUNNING;
  }

  if (get_port->tag == FBLE_PORT_VALUE) {
    FblePortValue* port = (FblePortValue*)get_port;
    if (*port->data == NULL) {
      // Blocked on get. Restore the thread state and return before
      // logging progress.
      assert(instr->profile_ops == NULL);
      return BLOCKED;
    }

    thread->stack->locals[get_instr->dest] = *port->data;
    *port->data = NULL;
    *io_activity = true;
    return RUNNING;
  }

  UNREACHABLE("get port must be an input or port value");
  return ABORTED;
}

// PutInstr -- see documentation of InstrImpl
//   Execute a PUT_INSTR.
static Status PutInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
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
    thread->stack->locals[put_instr->dest] = unit;
    *io_activity = true;
    return RUNNING;
  }

  if (put_port->tag == FBLE_PORT_VALUE) {
    FblePortValue* port = (FblePortValue*)put_port;

    if (*port->data != NULL) {
      // Blocked on put. Restore the thread state and return before
      // logging progress.
      assert(instr->profile_ops == NULL);
      return BLOCKED;
    }

    FbleRetainValue(heap, arg);
    *port->data = arg;
    thread->stack->locals[put_instr->dest] = unit;
    *io_activity = true;
    return RUNNING;
  }

  UNREACHABLE("put port must be an output or port value");
  return ABORTED;
}

// ForkInstr -- see documentation of InstrImpl
//   Execute a FORK_INSTR.
static Status ForkInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleForkInstr* fork_instr = (FbleForkInstr*)instr;

  assert(thread->children.size == 0);
  assert(thread->children.xs == NULL);
  FbleVectorInit(heap->arena, thread->children);

  for (size_t i = 0; i < fork_instr->args.size; ++i) {
    FbleProcValue* arg = (FbleProcValue*)FrameGetStrict(thread, fork_instr->args.xs[i]);

    // You cannot execute a proc in a let binding, so it should be
    // impossible to ever have an undefined proc value.
    assert(arg != NULL && "undefined proc value");
    assert(arg->_base.tag == FBLE_PROC_VALUE);

    FbleValue** result = thread->stack->locals + fork_instr->dests.xs[i];

    Thread* child = FbleAlloc(heap->arena, Thread);
    child->stack = PushFrame(heap, arg, NULL, result, NULL);
    child->profile = thread->profile == NULL ? NULL : FbleForkProfileThread(heap->arena, thread->profile);
    child->children.size = 0;
    child->children.xs = NULL;
    child->next_child = 0;
    child->parent = thread;
    FbleVectorAppend(heap->arena, thread->children, child);
  }
  thread->next_child = 0;
  return YIELDED;
}

// CopyInstr -- see documentation of InstrImpl
//   Execute a COPY_INSTR.
static Status CopyInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
  FbleValue* value = FrameGet(thread, copy_instr->source);
  FbleRetainValue(heap, value);
  thread->stack->locals[copy_instr->dest] = value;
  return RUNNING;
}

// LinkInstr -- see documentation of InstrImpl
//   Execute a LINK_INSTR.
static Status LinkInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;

  FbleLinkValue* link = FbleNewValue(heap, FbleLinkValue);
  link->_base.tag = FBLE_LINK_VALUE;
  link->head = NULL;
  link->tail = NULL;

  FbleValue* get = FbleNewGetValue(heap, &link->_base);
  FbleValue* put = FbleNewPutValue(heap, &link->_base);
  FbleReleaseValue(heap, &link->_base);

  thread->stack->locals[link_instr->get] = get;
  thread->stack->locals[link_instr->put] = put;
  return RUNNING;
}

// RefValueInstr -- see documentation of InstrImpl
//   Execute a REF_VALUE_INSTR.
static Status RefValueInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
  FbleRefValue* rv = FbleNewValue(heap, FbleRefValue);
  rv->_base.tag = FBLE_REF_VALUE;
  rv->value = NULL;

  thread->stack->locals[ref_instr->dest] = &rv->_base;
  return RUNNING;
}

// RefDefInstr -- see documentation of InstrImpl
//   Execute a REF_DEF_INSTR.
static Status RefDefInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
  FbleRefValue* rv = (FbleRefValue*)thread->stack->locals[ref_def_instr->ref];
  assert(rv->_base.tag == FBLE_REF_VALUE);

  FbleValue* value = FrameGet(thread, ref_def_instr->value);
  assert(value != NULL);
  rv->value = value;
  FbleValueAddRef(heap, &rv->_base, rv->value);
  return RUNNING;
}

// ReturnInstr -- see documentation of InstrImpl
//   Execute a RETURN_INSTR.
static Status ReturnInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
  *thread->stack->result = FrameMove(heap, thread, return_instr->result, thread->stack->func->argc, thread->stack->owner);
  thread->stack = PopFrame(heap, thread->stack);
  if (thread->stack == NULL) {
    return FINISHED;
  }
  return RUNNING;
}

// TypeInstr -- see documentation of InstrImpl
//   Execute a FBLE_TYPE_INSTR.
static Status TypeInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
  FbleTypeValue* value = FbleNewValue(heap, FbleTypeValue);
  value->_base.tag = FBLE_TYPE_VALUE;
  thread->stack->locals[type_instr->dest] = &value->_base;
  return RUNNING;
}

// SymbolicValueInstr -- see documentation of InstrImpl
//   Execute a FBLE_SYMBOLIC_VALUE_INSTR.
static Status SymbolicValueInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleSymbolicValueInstr* symbolic_value_instr = (FbleSymbolicValueInstr*)instr;
  FbleSymbolicValue* value = FbleNewValue(heap, FbleSymbolicValue);
  value->_base.tag = FBLE_SYMBOLIC_VALUE;
  thread->stack->locals[symbolic_value_instr->dest] = &value->_base;
  return RUNNING;
}

// SymbolicCompileInstr -- see documentation of InstrImpl
//   Execute a FBLE_SYMBOLIC_COMPILE_INSTR.
static Status SymbolicCompileInstr(FbleValueHeap* heap, Thread* thread, FbleInstr* instr, bool* io_activity)
{
  FbleSymbolicCompileInstr* compile_instr = (FbleSymbolicCompileInstr*)instr;
  FbleValue* args[compile_instr->args.size];
  for (size_t i = 0; i < compile_instr->args.size; ++i) {
    args[i] = FrameGet(thread, compile_instr->args.xs[i]);
  }
  FbleValue* body = FrameGet(thread, compile_instr->body);
  FbleValueV argv = { .size = compile_instr->args.size, .xs = args };
  FbleValue* value = FbleSymbolicCompile(heap, argv, body);
  thread->stack->locals[compile_instr->dest] = value;
  return RUNNING;
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
//   The status of the thread.
//
// Side effects:
//   The thread is executed, updating its stack.
//   io_activity is set to true if the thread does any i/o activity that could
//   unblock another thread.
//   aborted is set to true if the thread aborts.
static Status RunThread(FbleValueHeap* heap, Thread* thread, bool* io_activity, bool* aborted)
{
  if (*aborted) {
    return AbortThread(heap, thread, aborted);
  }

  FbleArena* arena = heap->arena;
  FbleProfileThread* profile = thread->profile;

  while (true) {
    FbleInstr* instr = *thread->stack->pc++;
    if (rand() % TIME_SLICE == 0) {
      if (profile != NULL) {
        FbleProfileSample(arena, profile, 1);
      }

      thread->stack->pc--;
      return YIELDED;
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

    Status status = sInstrImpls[instr->tag](heap, thread, instr, io_activity);

    if (status == RUNNING) {
      continue;
    }

    if (status == BLOCKED) {
      thread->stack->pc--;
      return BLOCKED;
    }

    if (status == ABORTED) {
      thread->stack->pc--;
      return AbortThread(heap, thread, aborted);
    }

    return status;
  }

  UNREACHABLE("should never get here");
  return FINISHED;
}

// AbortThread --
//   Unwind the given thread, cleaning the stack and children threads as
//   appropriate.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to abort.
//   aborted - set to true.
//
// Results:
//   FINISHED
//
// Side effects:
//   Cleans up the thread state by unwinding the thread. The state of the
//   thread after this function is the same as if it had completed normally,
//   except that it will not have produced a return value.
//   Sets aborted to true.
static Status AbortThread(FbleValueHeap* heap, Thread* thread, bool* aborted)
{
  *aborted = true;

  FbleInstr** pc = thread->stack->pc;
  FbleValue** locals = thread->stack->locals;

  while (true) {
    FbleInstr* instr = *pc++;

    switch (instr->tag) {
      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        locals[struct_value_instr->dest] = NULL;
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        locals[union_value_instr->dest] = NULL;
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR:
      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        locals[access_instr->dest] = NULL;
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
        pc += select_instr->jumps.xs[0];
        break;
      }

      case FBLE_JUMP_INSTR: {
        FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
        pc += jump_instr->count;
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        locals[func_value_instr->dest] = NULL;
        break;
      }

      case FBLE_RELEASE_INSTR: {
        FbleReleaseInstr* release = (FbleReleaseInstr*)instr;
        if (release->value >= thread->stack->func->argc || thread->stack->owner) {
          FbleReleaseValue(heap, locals[release->value]);
        }
        locals[release->value] = NULL;
        break;
      }

      case FBLE_CALL_INSTR: {
        FbleCallInstr* call_instr = (FbleCallInstr*)instr;
        if (call_instr->exit) {
          FrameRelease(heap, thread, call_instr->func, thread->stack->func->argc, thread->stack->owner);
          for (size_t i = 0; i < call_instr->args.size; ++i) {
            FrameRelease(heap, thread, call_instr->args.xs[i], thread->stack->func->argc, thread->stack->owner);
          }

          *thread->stack->result = NULL;
          thread->stack = PopFrame(heap, thread->stack);
          if (thread->stack == NULL) {
            return FINISHED;
          }
          pc = thread->stack->pc;
          locals = thread->stack->locals;
        } else {
          locals[call_instr->dest] = NULL;
        }
        break;
      }

      case FBLE_COPY_INSTR: {
        FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
        locals[copy_instr->dest] = NULL;
        break;
      }

      case FBLE_GET_INSTR: {
        FbleGetInstr* get_instr = (FbleGetInstr*)instr;
        locals[get_instr->dest] = NULL;
        break;
      }

      case FBLE_PUT_INSTR: {
        // A put instruction is always the first instruction of a put
        // function, and it never aborts. That makes this code unreachable for
        // the time being.
        UNREACHABLE("put functions can't abort");

        FblePutInstr* put_instr = (FblePutInstr*)instr;
        locals[put_instr->dest] = NULL;
        break;
      }

      case FBLE_LINK_INSTR: {
        FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
        locals[link_instr->get] = NULL;
        locals[link_instr->put] = NULL;
        break;
      }

      case FBLE_FORK_INSTR: {
        FbleForkInstr* fork_instr = (FbleForkInstr*)instr;
        for (size_t i = 0; i < fork_instr->args.size; ++i) {
          locals[fork_instr->dests.xs[i]] = NULL;
        }
        break;
      }

      case FBLE_REF_VALUE_INSTR: {
        FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
        locals[ref_instr->dest] = NULL;
        break;
      }

      case FBLE_REF_DEF_INSTR: {
        break;
      }

      case FBLE_RETURN_INSTR: {
        FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
        FrameRelease(heap, thread, return_instr->result, thread->stack->func->argc, thread->stack->owner);
        *thread->stack->result = NULL;
        thread->stack = PopFrame(heap, thread->stack);
        if (thread->stack == NULL) {
          return FINISHED;
        }
        pc = thread->stack->pc;
        locals = thread->stack->locals;
        break;
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
        locals[type_instr->dest] = NULL;
        break;
      }

      case FBLE_SYMBOLIC_VALUE_INSTR: {
        FbleSymbolicValueInstr* symbolic_value_instr = (FbleSymbolicValueInstr*)instr;
        locals[symbolic_value_instr->dest] = NULL;
        break;
      }

      case FBLE_SYMBOLIC_COMPILE_INSTR: {
        FbleSymbolicCompileInstr* compile_instr = (FbleSymbolicCompileInstr*)instr;
        locals[compile_instr->dest] = NULL;
        break;
      }
    }
  }

  UNREACHABLE("should never get here");
  return FINISHED;
}

// RunThreads --
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
//   The status of running the threads.
//
// Side effects:
//   The thread and its children are executed and updated.
//   Updates the profile based on the execution.
//   Sets aborted to true in case of abort.
static Status RunThreads(FbleValueHeap* heap, Thread* thread, bool* aborted)
{
  FbleArena* arena = heap->arena;

  bool unblocked = false;
  while (thread != NULL) {
    if (thread->children.size > 0) {
      if (thread->next_child == thread->children.size) {
        thread->next_child = 0;
        thread = thread->parent;
      } else {
        // RunThreads on the next child thread.
        thread = thread->children.xs[thread->next_child++];
      }
    } else {
      // Run the leaf thread, then go back up to the parent for the rest of
      // the threads to process.
      Status status = RunThread(heap, thread, &unblocked, aborted);
      switch (status) {
        case FINISHED: {
          unblocked = true;
          if (thread->parent == NULL) {
            return status;
          }

          Thread* parent = thread->parent;
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

        case BLOCKED: {
          thread = thread->parent;
          break;
        }

        case YIELDED: {
          unblocked = true;
          thread = thread->parent;
          break;
        }

        case RUNNING: {
          // If the thread is actively running, it should not have returned
          // control to this point.
          UNREACHABLE("should never get here");
          break;
        }

        case ABORTED: {
          // RunThread should have converted this into FINISHED after
          // unwinding the thread.
          UNREACHABLE("should never get here");
          break;
        }
      }
    }
  }
  return unblocked ? YIELDED : BLOCKED;
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
  FbleValue* final_result = NULL;
  Thread thread = {
    .stack = PushFrame(heap, func, args, &final_result, NULL),
    .profile = profile == NULL ? NULL : FbleNewProfileThread(arena, profile),
    .children = {0, NULL},
    .next_child = 0,
    .parent = NULL,
  };

  bool aborted = false;
  while (true) {
    Status status = RunThreads(heap, &thread, &aborted);
    switch (status) {
      case FINISHED: {
        assert(aborted || final_result != NULL);
        assert(thread.stack == NULL);
        assert(thread.children.size == 0);
        assert(thread.children.xs == NULL);
        FbleFreeProfileThread(arena, thread.profile);
        return final_result;
      }

      case BLOCKED: {
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

      case YIELDED: {
        // Give a chance to do some I/O, but don't block, because the thread
        // is making progress anyway.
        io->io(io, heap, false);
        break;
      }

      case RUNNING: {
        // If the thread is actively running, it should not have returned
        // control to this point.
        UNREACHABLE("should never get here");
        break;
      }

      case ABORTED: {
        // RunThread should have converted this into FINISHED after
        // unwinding the thread.
        UNREACHABLE("should never get here");
        break;
      }
    }
  }

  UNREACHABLE("should never get here");
  return NULL;
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleValueHeap* heap, FbleCompiledProgram* program, FbleProfile* profile)
{
  FbleFuncValue* func = FbleNewValue(heap, FbleFuncValue);
  func->_base.tag = FBLE_FUNC_VALUE;
  func->argc = 0;
  func->code = program->code;
  func->code->refcount++;
  assert(program->code->statics == 0);

  FbleIO io = { .io = &FbleNoIO };
  FbleValue* result = Eval(heap, &io, func, NULL, profile);
  FbleReleaseValue(heap, &func->_base);
  return result;
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
