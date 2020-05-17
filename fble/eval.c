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
  UNBLOCKED,      // The thread is not blocked on I/O.
} Status;

static FbleValue* FrameGet(FbleValue** statics, FbleValue** locals, FbleFrameIndex index);
static FbleValue* FrameTaggedGet(FbleValueTag tag, FbleValue** statics, FbleValue** locals, FbleFrameIndex index);

static Stack* PushFrame(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleValue** result, Stack* tail);
static Stack* PopFrame(FbleValueHeap* heap, Stack* stack);
static Stack* ReplaceFrame(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, Stack* stack);

static Status RunThread(FbleValueHeap* heap, Thread* thread, bool* io_activity, bool* aborted);
static Status AbortThread(FbleValueHeap* heap, Thread* thread, bool* aborted);
static Status RunThreads(FbleValueHeap* heap, Thread* thread, bool* aborted);
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleFuncValue* func, FbleValue** args, FbleProfile* profile);

// FrameGet --
//   Get a value from the frame on the top of the execution stack.
//
// Inputs:
//   statics - the static variables at the top of the stack.
//   locals - the local variables at the top of the stack.
//   index - the index of the value to access.
//
// Results:
//   The value in the top stack frame at the given index.
//
// Side effects:
//   None.
static FbleValue* FrameGet(FbleValue** statics, FbleValue** locals, FbleFrameIndex index)
{
  switch (index.section) {
    case FBLE_STATICS_FRAME_SECTION: return statics[index.index];
    case FBLE_LOCALS_FRAME_SECTION: return locals[index.index];
  }
  UNREACHABLE("should never get here");
}

// FrameTaggedGet --
//   Get and dereference a value from the frame at the top of the given stack.
//   Dereferences the data value, removing all layers of reference values
//   until a non-reference value is encountered and returns the non-reference
//   value.
//
//   A tag for the type of dereferenced value should be provided. This
//   function will assert that the correct kind of value is encountered.
//
// Inputs:
//   tag - the expected tag of the value.
//   statics - the static variables at the top of the stack.
//   locals - the local variables at the top of the stack.
//   index - the location of the value in the frame.
//
// Results:
//   The dereferenced value. Returns null in case of abstract value
//   dereference.
//
// Side effects:
//   The returned value will only stay alive as long as the original value on
//   the stack frame.
static FbleValue* FrameTaggedGet(FbleValueTag tag, FbleValue** statics, FbleValue** locals, FbleFrameIndex index)
{
  FbleValue* value = FrameGet(statics, locals, index);
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
  assert(value->tag == tag);
  return value;
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

  size_t start = stack->owner ? 0 : stack->func->argc;
  for (size_t i = start; i < stack->func->code->locals; ++i) {
    FbleReleaseValue(heap, stack->locals[i]);
  }

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
//   func - the function to execute.
//   args - args to the function. length == func->argc
//   tail - the stack to change.
//
// Result:
//   The stack with new frame.
//
// Side effects:
//   Allocates new Stack instances that should be freed with PopFrame when done.
//   Does not take ownership of func nor args. The caller need not guarantee
//   the func and args remain valid after this call.
//   Exits the current frame, which potentially frees any instructions
//   belonging to that frame.
static Stack* ReplaceFrame(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, Stack* stack)
{
  FbleArena* arena = heap->arena;

  FbleRetainValue(heap, &func->_base);
  for (size_t i = 0; i < func->argc; ++i) {
    FbleRetainValue(heap, args[i]);
  }

  size_t old_locals = stack->func->code->locals;
  size_t start = stack->owner ? 0 : stack->func->argc;
  for (size_t i = start; i < old_locals; ++i) {
    FbleReleaseValue(heap, stack->locals[i]);
  }
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
  }

  stack->owner = true;
  stack->func = func;
  stack->pc = func->code->instrs.xs;
  memset(stack->locals, 0, locals * sizeof(FbleValue*));

  for (size_t i = 0; i < func->argc; ++i) {
    stack->locals[i] = args[i];
  }

  return stack;
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
  FbleInstr** pc = thread->stack->pc;
  FbleValue** statics = thread->stack->func->scope;
  FbleValue** locals = thread->stack->locals;

  while (true) {
    FbleInstr* instr = *pc++;
    if (rand() % TIME_SLICE == 0) {
      if (profile != NULL) {
        FbleProfileSample(arena, profile, 1);
      }

      thread->stack->pc = pc - 1;
      return UNBLOCKED;
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

    switch (instr->tag) {
      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_value_instr->args.size;

        FbleStructValue* value = FbleNewValueExtra(heap, FbleStructValue, sizeof(FbleValue*) * argc);
        value->_base.tag = FBLE_STRUCT_VALUE;
        value->fieldc = argc;
        for (size_t i = 0; i < argc; ++i) {
          value->fields[i] = FrameGet(statics, locals, struct_value_instr->args.xs[i]);
          FbleValueAddRef(heap, &value->_base, value->fields[i]);
        }

        locals[struct_value_instr->dest] = &value->_base;
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;

        FbleUnionValue* value = FbleNewValue(heap, FbleUnionValue);
        value->_base.tag = FBLE_UNION_VALUE;
        value->tag = union_value_instr->tag;
        value->arg = FrameGet(statics, locals, union_value_instr->arg);
        FbleValueAddRef(heap, &value->_base, value->arg);

        locals[union_value_instr->dest] = &value->_base;
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleStructValue* sv = (FbleStructValue*)FrameTaggedGet(FBLE_STRUCT_VALUE, statics, locals, access_instr->obj);
        if (sv == NULL) {
          FbleReportError("undefined struct value access\n", &access_instr->loc);
          thread->stack->pc = pc - 1;
          return AbortThread(heap, thread, aborted);
        }

        assert(access_instr->tag < sv->fieldc);
        FbleValue* value = sv->fields[access_instr->tag];
        FbleRetainValue(heap, value);
        locals[access_instr->dest] = value;
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleUnionValue* uv = (FbleUnionValue*)FrameTaggedGet(FBLE_UNION_VALUE, statics, locals, access_instr->obj);
        if (uv == NULL) {
          FbleReportError("undefined union value access\n", &access_instr->loc);
          thread->stack->pc = pc - 1;
          return AbortThread(heap, thread, aborted);
        }

        if (uv->tag != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);
          thread->stack->pc = pc - 1;
          return AbortThread(heap, thread, aborted);
        }

        FbleRetainValue(heap, uv->arg);
        locals[access_instr->dest] = uv->arg;
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
        FbleUnionValue* uv = (FbleUnionValue*)FrameTaggedGet(FBLE_UNION_VALUE, statics, locals, select_instr->condition);
        if (uv == NULL) {
          FbleReportError("undefined union value select\n", &select_instr->loc);
          thread->stack->pc = pc - 1;
          return AbortThread(heap, thread, aborted);
        }
        pc += select_instr->jumps.xs[uv->tag];
        break;
      }

      case FBLE_JUMP_INSTR: {
        FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
        pc += jump_instr->count;
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        size_t scopec = func_value_instr->code->statics;

        FbleFuncValue* value = FbleNewValueExtra(heap, FbleFuncValue, sizeof(FbleValue*) * scopec);
        value->_base.tag = FBLE_FUNC_VALUE;
        value->argc = func_value_instr->argc;
        value->code = func_value_instr->code;
        value->code->refcount++;
        for (size_t i = 0; i < scopec; ++i) {
          FbleValue* arg = FrameGet(statics, locals, func_value_instr->scope.xs[i]);
          value->scope[i] = arg;
          FbleValueAddRef(heap, &value->_base, arg);
        }
        locals[func_value_instr->dest] = &value->_base;
        break;
      }

      case FBLE_RELEASE_INSTR: {
        FbleReleaseInstr* release = (FbleReleaseInstr*)instr;
        assert(locals[release->value] != NULL);
        FbleReleaseValue(heap, locals[release->value]);
        locals[release->value] = NULL;
        break;
      }

      case FBLE_CALL_INSTR: {
        FbleCallInstr* call_instr = (FbleCallInstr*)instr;
        FbleFuncValue* func = (FbleFuncValue*)FrameTaggedGet(FBLE_FUNC_VALUE, statics, locals, call_instr->func);
        if (func == NULL) {
          FbleReportError("called undefined function\n", &call_instr->loc);
          thread->stack->pc = pc - 1;
          return AbortThread(heap, thread, aborted);
        };

        FbleValue* args[func->argc];
        for (size_t i = 0; i < func->argc; ++i) {
          args[i] = FrameGet(statics, locals, call_instr->args.xs[i]);
        }

        if (call_instr->exit) {
          thread->stack = ReplaceFrame(heap, func, args, thread->stack);
          pc = thread->stack->pc;
          statics = thread->stack->func->scope;
          locals = thread->stack->locals;
        } else {
          thread->stack->pc = pc;
          FbleValue** result = locals + call_instr->dest;
          thread->stack = PushFrame(heap, func, args, result, thread->stack);
          pc = thread->stack->pc;
          statics = thread->stack->func->scope;
          locals = thread->stack->locals;
        }
        break;
      }

      case FBLE_PROC_VALUE_INSTR: {
        FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
        size_t scopec = proc_value_instr->code->statics;

        FbleProcValue* value = FbleNewValueExtra(heap, FbleProcValue, sizeof(FbleValue*) * scopec);
        value->_base.tag = FBLE_PROC_VALUE;
        value->argc = 0;
        value->code = proc_value_instr->code;
        value->code->refcount++;
        for (size_t i = 0; i < scopec; ++i) {
          FbleValue* arg = FrameGet(statics, locals, proc_value_instr->scope.xs[i]);
          value->scope[i] = arg;
          FbleValueAddRef(heap, &value->_base, arg);
        }
        locals[proc_value_instr->dest] = &value->_base;
        break;
      }

      case FBLE_COPY_INSTR: {
        FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
        FbleValue* value = FrameGet(statics, locals, copy_instr->source);
        FbleRetainValue(heap, value);
        locals[copy_instr->dest] = value;
        break;
      }

      case FBLE_GET_INSTR: {
        FbleGetInstr* get_instr = (FbleGetInstr*)instr;
        FbleValue* get_port = FrameGet(statics, locals, get_instr->port);
        if (get_port->tag == FBLE_LINK_VALUE) {
          FbleLinkValue* link = (FbleLinkValue*)get_port;

          if (link->head == NULL) {
            // Blocked on get. Restore the thread state and return before
            // logging progress.
            assert(instr->profile_ops == NULL);
            thread->stack->pc = pc - 1;
            return BLOCKED;
          }

          FbleValues* head = link->head;
          link->head = link->head->next;
          if (link->head == NULL) {
            link->tail = NULL;
          }

          FbleRetainValue(heap, head->value);
          locals[get_instr->dest] = head->value;
          FbleValueDelRef(heap, &link->_base, head->value);
          FbleFree(arena, head);
          break;
        }

        if (get_port->tag == FBLE_PORT_VALUE) {
          FblePortValue* port = (FblePortValue*)get_port;
          if (*port->data == NULL) {
            // Blocked on get. Restore the thread state and return before
            // logging progress.
            assert(instr->profile_ops == NULL);
            thread->stack->pc = pc - 1;
            return BLOCKED;
          }

          locals[get_instr->dest] = *port->data;
          *port->data = NULL;
          *io_activity = true;
          break;
        }

        UNREACHABLE("get port must be an input or port value");
        break;
      }

      case FBLE_PUT_INSTR: {
        FblePutInstr* put_instr = (FblePutInstr*)instr;
        FbleValue* put_port = FrameGet(statics, locals, put_instr->port);
        FbleValue* arg = FrameGet(statics, locals, put_instr->arg);

        FbleValueV args = { .size = 0, .xs = NULL, };
        FbleValue* unit = FbleNewStructValue(heap, args);

        if (put_port->tag == FBLE_LINK_VALUE) {
          FbleLinkValue* link = (FbleLinkValue*)put_port;

          FbleValues* tail = FbleAlloc(arena, FbleValues);
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
          locals[put_instr->dest] = unit;
          *io_activity = true;
          break;
        }

        if (put_port->tag == FBLE_PORT_VALUE) {
          FblePortValue* port = (FblePortValue*)put_port;

          if (*port->data != NULL) {
            // Blocked on put. Restore the thread state and return before
            // logging progress.
            assert(instr->profile_ops == NULL);
            thread->stack->pc = pc - 1;
            return BLOCKED;
          }

          FbleRetainValue(heap, arg);
          *port->data = arg;
          locals[put_instr->dest] = unit;
          *io_activity = true;
          break;
        }

        UNREACHABLE("put port must be an output or port value");
        break;
      }

      case FBLE_LINK_INSTR: {
        FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;

        FbleLinkValue* link = FbleNewValue(heap, FbleLinkValue);
        link->_base.tag = FBLE_LINK_VALUE;
        link->head = NULL;
        link->tail = NULL;

        FbleValue* get = FbleNewGetValue(heap, &link->_base);
        FbleValue* put = FbleNewPutValue(heap, &link->_base);
        FbleReleaseValue(heap, &link->_base);

        locals[link_instr->get] = get;
        locals[link_instr->put] = put;
        break;
      }

      case FBLE_FORK_INSTR: {
        FbleForkInstr* fork_instr = (FbleForkInstr*)instr;

        assert(thread->children.size == 0);
        assert(thread->children.xs == NULL);
        FbleVectorInit(arena, thread->children);

        for (size_t i = 0; i < fork_instr->args.size; ++i) {
          FbleProcValue* arg = (FbleProcValue*)FrameTaggedGet(FBLE_PROC_VALUE, statics, locals, fork_instr->args.xs[i]);

          // You cannot execute a proc in a let binding, so it should be
          // impossible to ever have an undefined proc value.
          assert(arg != NULL && "undefined proc value");

          FbleValue** result = locals + fork_instr->dests.xs[i];

          Thread* child = FbleAlloc(arena, Thread);
          child->stack = PushFrame(heap, arg, NULL, result, NULL);
          child->profile = profile == NULL ? NULL : FbleForkProfileThread(arena, profile);
          child->children.size = 0;
          child->children.xs = NULL;
          child->next_child = 0;
          child->parent = thread;
          FbleVectorAppend(arena, thread->children, child);
        }
        thread->next_child = 0;
        thread->stack->pc = pc;
        return UNBLOCKED;
      }

      case FBLE_REF_VALUE_INSTR: {
        FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
        FbleRefValue* rv = FbleNewValue(heap, FbleRefValue);
        rv->_base.tag = FBLE_REF_VALUE;
        rv->value = NULL;

        locals[ref_instr->dest] = &rv->_base;
        break;
      }

      case FBLE_REF_DEF_INSTR: {
        FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
        FbleRefValue* rv = (FbleRefValue*)locals[ref_def_instr->ref];
        assert(rv->_base.tag == FBLE_REF_VALUE);

        FbleValue* value = FrameGet(statics, locals, ref_def_instr->value);
        assert(value != NULL);
        rv->value = value;
        FbleValueAddRef(heap, &rv->_base, rv->value);
        break;
      }

      case FBLE_RETURN_INSTR: {
        FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
        FbleValue* result = FrameGet(statics, locals, return_instr->result);
        FbleRetainValue(heap, result);
        *thread->stack->result = result;
        thread->stack = PopFrame(heap, thread->stack);
        if (thread->stack == NULL) {
          return FINISHED;
        }
        pc = thread->stack->pc;
        statics = thread->stack->func->scope;
        locals = thread->stack->locals;
        break;
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
        FbleTypeValue* value = FbleNewValue(heap, FbleTypeValue);
        value->_base.tag = FBLE_TYPE_VALUE;
        locals[type_instr->dest] = &value->_base;
        break;
      }
    }
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

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        locals[access_instr->dest] = NULL;
        break;
      }

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
        FbleReleaseValue(heap, locals[release->value]);
        locals[release->value] = NULL;
        break;
      }

      case FBLE_CALL_INSTR: {
        FbleCallInstr* call_instr = (FbleCallInstr*)instr;
        if (call_instr->exit) {
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

      case FBLE_PROC_VALUE_INSTR: {
        FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
        locals[proc_value_instr->dest] = NULL;
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

        case UNBLOCKED: {
          unblocked = true;
          thread = thread->parent;
          break;
        }
      }
    }
  }
  return unblocked ? UNBLOCKED : BLOCKED;
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
          FbleLoc loc = { .source = source, .line = __LINE__, .col = 0 };
          FbleReportError("deadlock\n", &loc);
          FbleReleaseString(arena, source);
          aborted = true;
        }
        break;
      }

      case UNBLOCKED: {
        // Give a chance to do some I/O, but don't block, because the thread
        // is making progress anyway.
        io->io(io, heap, false);
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
