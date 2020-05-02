// eval.c --
//   This file implements the fble evaluation routines.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for NULL, abort, rand
#include <string.h>   // for memset

#include "fble.h"
#include "instr.h"
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
//   scope - A value that owns the statics array. May be NULL.
//   statics - Variables captured from the parent scope.
//   code - The currently executing instruction block.
//   pc - The location of the next instruction in the code to execute.
//   result - Where to store the result when exiting the stack frame.
//   tail - The next frame down in the stack.
//   locals - Space allocated for local variables.
//      Has length at least code->locals values. Takes ownership of all
//      non-NULL entries.
typedef struct Stack {
  FbleValue* scope;
  FbleValue** statics;
  FbleInstrBlock* code;
  size_t pc;
  FbleValue** result;
  struct Stack* tail;
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
//   profile - the profile thread associated with this thread.
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
  ABORTED,        // The thread was aborted.
  BLOCKED,        // The thread is blocked on I/O.
  UNBLOCKED,      // The thread is not blocked on I/O.
} Status;

static FbleValue* FrameGet(Stack* stack, FbleFrameIndex index);
static FbleValue* FrameTaggedGet(FbleValueTag tag, Stack* stack, FbleFrameIndex index);

static Stack* PushFrame(FbleArena* arena, FbleValue* scope, FbleValue** statics, FbleInstrBlock* code, FbleValue** result, Stack* tail);
static Stack* PopFrame(FbleValueHeap* heap, Stack* stack);
static Stack* ReplaceFrame(FbleValueHeap* heap, FbleValue* scope, FbleValue** statics, FbleInstrBlock* code, Stack* stack);

static void AbortThread(FbleValueHeap* heap, Thread* thread);
static Status RunThread(FbleValueHeap* heap, Thread* thread, bool* io_activity);
static Status RunThreads(FbleValueHeap* heap, Thread* thread);
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleValue** statics, FbleInstrBlock* code, FbleProfile* profile);

// FrameGet --
//   Get a value from the frame on the top of the execution stack.
//
// Inputs:
//   stack - the stack to access the value from.
//   index - the index of the value to access.
//
// Results:
//   The value in the top stack frame at the given index.
//
// Side effects:
//   None.
static FbleValue* FrameGet(Stack* stack, FbleFrameIndex index)
{
  switch (index.section) {
    case FBLE_STATICS_FRAME_SECTION: return stack->statics[index.index];
    case FBLE_LOCALS_FRAME_SECTION: return stack->locals[index.index];
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
//   stack - the stack to get the value from.
//   index - the location of the value in the frame.
//
// Results:
//   The dereferenced value. Returns null in case of abstract value
//   dereference.
//
// Side effects:
//   The returned value will only stay alive as long as the original value on
//   the stack frame.
static FbleValue* FrameTaggedGet(FbleValueTag tag, Stack* stack, FbleFrameIndex index)
{
  FbleValue* original = FrameGet(stack, index);
  FbleValue* value = original;
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
//   scope - a value that owns the statics array provided.
//   statics - array of statics variables, owned by scope.
//   code - the block of instructions to push
//   result - the return address.
//   tail - the stack to push to
//
// Result:
//   The stack with new pushed frame.
//
// Side effects:
//   Allocates new Stack instances that should be freed with PopFrame when done.
//   Takes ownership of scope variable.
static Stack* PushFrame(FbleArena* arena, FbleValue* scope, FbleValue** statics, FbleInstrBlock* code, FbleValue** result, Stack* tail)
{
  code->refcount++;

  Stack* stack = FbleAllocExtra(arena, Stack, code->locals * sizeof(FbleValue*));
  stack->scope = scope;
  stack->statics = statics;
  stack->code = code;
  stack->pc = 0;
  stack->result = result;
  stack->tail = tail;
  memset(stack->locals, 0, code->locals * sizeof(FbleValue*));
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
//   Releases any remaining variables on the frame and frees the frame.
static Stack* PopFrame(FbleValueHeap* heap, Stack* stack)
{
  FbleArena* arena = heap->arena;

  FbleValueRelease(heap, stack->scope);
  for (size_t i = 0; i < stack->code->locals; ++i) {
    FbleValueRelease(heap, stack->locals[i]);
  }
  FbleFreeInstrBlock(arena, stack->code);

  Stack* tail = stack->tail;
  FbleFree(arena, stack);
  return tail;
}

// ReplaceFrame --
//   Replace the current frame with a new one.
//
// Inputs:
//   heap - the value heap.
//   scope - the value that owns the statics array.
//   statics - array of captured variables.
//   code - the block of instructions for the new frame.
//   tail - the stack to change.
//
// Result:
//   The stack with new frame.
//
// Side effects:
//   Allocates new Stack instances that should be freed with PopFrame when done.
//   Takes ownership of scope.
//   Exits the current frame, which potentially frees any instructions
//   belonging to that frame.
static Stack* ReplaceFrame(FbleValueHeap* heap, FbleValue* scope, FbleValue** statics, FbleInstrBlock* code, Stack* stack)
{
  FbleArena* arena = heap->arena;

  FbleValueRelease(heap, stack->scope);
  for (size_t i = 0; i < stack->code->locals; ++i) {
    FbleValueRelease(heap, stack->locals[i]);
  }
  FbleFreeInstrBlock(arena, stack->code);

  if (code->locals > stack->code->locals) {
    Stack* nstack = FbleAllocExtra(arena, Stack, code->locals * sizeof(FbleValue*));
    nstack->tail = stack->tail;
    nstack->result = stack->result;

    FbleFree(arena, stack);
    stack = nstack;
  }

  code->refcount++;
  stack->scope = scope;
  stack->statics = statics;
  stack->code = code;
  stack->pc = 0;
  memset(stack->locals, 0, code->locals * sizeof(FbleValue*));
  return stack;
}

// AbortThread --
//   Unwind the given thread, cleaning the stack and children threads as
//   appropriate.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to abort.
//
// Results:
//   None.
//
// Side effects:
//   Cleans up the thread state by unwinding the thread.
static void AbortThread(FbleValueHeap* heap, Thread* thread)
{
  // TODO: Worry about this recursive function smashing the stack?
  FbleArena* arena = heap->arena;
  for (size_t i = 0; i < thread->children.size; ++i) {
    AbortThread(heap, thread->children.xs[i]);
    FbleFree(arena, thread->children.xs[i]);
  }
  thread->children.size = 0;
  FbleFree(arena, thread->children.xs);
  thread->children.xs = NULL;

  while (thread->stack != NULL) {
    thread->stack = PopFrame(heap, thread->stack);
  }

  if (thread->profile != NULL) {
    FbleFreeProfileThread(arena, thread->profile);
    thread->profile = NULL;
  }
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
//
// Results:
//   The status of the thread.
//
// Side effects:
//   The thread is executed, updating its stack.
//   io_activity is set to true if the thread does any i/o activity that could
//   unblock another thread.
static Status RunThread(FbleValueHeap* heap, Thread* thread, bool* io_activity)
{
  FbleArena* arena = heap->arena;
  while (thread->stack != NULL) {
    assert(thread->stack->pc < thread->stack->code->instrs.size);
    FbleInstr* instr = thread->stack->code->instrs.xs[thread->stack->pc++];
    if (rand() % TIME_SLICE == 0) {
      // Don't count profiling instructions against the profile time. The user
      // doesn't care about those.
      if (instr->tag != FBLE_PROFILE_INSTR) {
        FbleProfileSample(arena, thread->profile, 1);
      }

      thread->stack->pc--;
      return UNBLOCKED;
    }

    switch (instr->tag) {
      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_value_instr->args.size;
        FbleValue* argv[argc];
        for (size_t i = 0; i < argc; ++i) {
          FbleValue* arg = FrameGet(thread->stack, struct_value_instr->args.xs[i]);
          argv[i] = FbleValueRetain(heap, arg);
        }

        FbleValueV args = { .size = argc, .xs = argv, };
        thread->stack->locals[struct_value_instr->dest] = FbleNewStructValue(heap, args);
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        FbleValue* arg = FrameGet(thread->stack, union_value_instr->arg);
        thread->stack->locals[union_value_instr->dest] = FbleNewUnionValue(heap, union_value_instr->tag, FbleValueRetain(heap, arg));
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleStructValue* sv = (FbleStructValue*)FrameTaggedGet(FBLE_STRUCT_VALUE, thread->stack, access_instr->obj);
        if (sv == NULL) {
          FbleReportError("undefined struct value access\n", &access_instr->loc);
          return ABORTED;
        }
        assert(access_instr->tag < sv->fieldc);
        thread->stack->locals[access_instr->dest] = FbleValueRetain(heap, sv->fields[access_instr->tag]);
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleUnionValue* uv = (FbleUnionValue*)FrameTaggedGet(FBLE_UNION_VALUE, thread->stack, access_instr->obj);
        if (uv == NULL) {
          FbleReportError("undefined union value access\n", &access_instr->loc);
          return ABORTED;
        }

        if (uv->tag != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);
          return ABORTED;
        }

        thread->stack->locals[access_instr->dest] = FbleValueRetain(heap, uv->arg);
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
        FbleUnionValue* uv = (FbleUnionValue*)FrameTaggedGet(FBLE_UNION_VALUE, thread->stack, select_instr->condition);
        if (uv == NULL) {
          FbleReportError("undefined union value select\n", &select_instr->loc);
          return ABORTED;
        }
        thread->stack->pc += uv->tag;
        break;
      }

      case FBLE_GOTO_INSTR: {
        FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
        thread->stack->pc = goto_instr->pc;
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        FbleBasicFuncValue* value = FbleNewValueExtra(
            heap, FbleBasicFuncValue,
            sizeof(FbleValue*) * func_value_instr->scope.size);
        value->_base._base.tag = FBLE_FUNC_VALUE;
        value->_base.tag = FBLE_BASIC_FUNC_VALUE;
        value->_base.argc = func_value_instr->argc;
        value->code = func_value_instr->code;
        value->code->refcount++;
        value->scopec = func_value_instr->scope.size;
        for (size_t i = 0; i < value->scopec; ++i) {
          FbleValue* arg = FrameGet(thread->stack, func_value_instr->scope.xs[i]);
          value->scope[i] = arg;
          FbleValueAddRef(heap, &value->_base._base, arg);
        }
        thread->stack->locals[func_value_instr->dest] = &value->_base._base;
        break;
      }

      case FBLE_RELEASE_INSTR: {
        FbleReleaseInstr* release = (FbleReleaseInstr*)instr;
        assert(thread->stack->locals[release->value] != NULL);
        FbleValueRelease(heap, thread->stack->locals[release->value]);
        thread->stack->locals[release->value] = NULL;
        break;
      }

      case FBLE_FUNC_APPLY_INSTR: {
        FbleFuncApplyInstr* func_apply_instr = (FbleFuncApplyInstr*)instr;
        FbleFuncValue* func = (FbleFuncValue*)FrameTaggedGet(FBLE_FUNC_VALUE, thread->stack, func_apply_instr->func);
        if (func == NULL) {
          FbleReportError("undefined function value apply\n", &func_apply_instr->loc);
          return ABORTED;
        };

        FbleThunkFuncValue* value = FbleNewValue(heap, FbleThunkFuncValue);
        value->_base._base.tag = FBLE_FUNC_VALUE;
        value->_base.tag = FBLE_THUNK_FUNC_VALUE;
        value->_base.argc = func->argc - 1;
        value->func = func;
        FbleValueAddRef(heap, &value->_base._base, &value->func->_base);
        value->arg = FrameGet(thread->stack, func_apply_instr->arg);
        FbleValueAddRef(heap, &value->_base._base, value->arg);

        if (value->_base.argc > 0) {
          if (func_apply_instr->exit) {
            *thread->stack->result = &value->_base._base;
            thread->stack = PopFrame(heap, thread->stack);
          } else {
            thread->stack->locals[func_apply_instr->dest] = &value->_base._base;
          }
        } else {
          size_t argc = 0;
          FbleFuncValue* f = &value->_base;
          while (f->tag == FBLE_THUNK_FUNC_VALUE) {
            argc++;
            FbleThunkFuncValue* thunk = (FbleThunkFuncValue*)f;
            f = thunk->func;
          }

          assert(f->tag == FBLE_BASIC_FUNC_VALUE);
          FbleBasicFuncValue* basic = (FbleBasicFuncValue*)f;
          if (func_apply_instr->exit) {
            thread->stack = ReplaceFrame(heap, &value->_base._base, basic->scope, basic->code, thread->stack);
          } else {
            FbleValue** result = thread->stack->locals + func_apply_instr->dest;
            thread->stack = PushFrame(arena, &value->_base._base, basic->scope, basic->code, result, thread->stack);
          }

          f = &value->_base;
          while (f->tag == FBLE_THUNK_FUNC_VALUE) {
            FbleThunkFuncValue* thunk = (FbleThunkFuncValue*)f;
            thread->stack->locals[--argc] = FbleValueRetain(heap, thunk->arg);
            f = thunk->func;
          }
        }
        break;
      }

      case FBLE_PROC_VALUE_INSTR: {
        FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
        size_t scopec = proc_value_instr->code->statics;

        FbleProcValue* value = FbleNewValueExtra(heap, FbleProcValue, sizeof(FbleValue*) * scopec);
        value->_base.tag = FBLE_PROC_VALUE;
        value->code = proc_value_instr->code;
        value->code->refcount++;
        for (size_t i = 0; i < scopec; ++i) {
          FbleValue* arg = FrameGet(thread->stack, proc_value_instr->scope.xs[i]);
          value->scope[i] = arg;
          FbleValueAddRef(heap, &value->_base, arg);
        }
        thread->stack->locals[proc_value_instr->dest] = &value->_base;
        break;
      }

      case FBLE_COPY_INSTR: {
        FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
        FbleValue* value = FrameGet(thread->stack, copy_instr->source);
        thread->stack->locals[copy_instr->dest] = FbleValueRetain(heap, value);
        break;
      }

      case FBLE_GET_INSTR: {
        FbleGetInstr* get_instr = (FbleGetInstr*)instr;
        FbleValue* get_port = FrameGet(thread->stack, get_instr->port);
        if (get_port->tag == FBLE_LINK_VALUE) {
          FbleLinkValue* link = (FbleLinkValue*)get_port;

          if (link->head == NULL) {
            // Blocked on get. Restore the thread state and return before
            // logging progress.
            thread->stack->pc--;
            return BLOCKED;
          }

          FbleValues* head = link->head;
          link->head = link->head->next;
          if (link->head == NULL) {
            link->tail = NULL;
          }

          thread->stack->locals[get_instr->dest] = FbleValueRetain(heap, head->value);
          FbleValueDelRef(heap, &link->_base, head->value);
          FbleFree(arena, head);
          break;
        }

        if (get_port->tag == FBLE_PORT_VALUE) {
          FblePortValue* port = (FblePortValue*)get_port;
          if (*port->data == NULL) {
            // Blocked on get. Restore the thread state and return before
            // logging progress.
            thread->stack->pc--;
            return BLOCKED;
          }

          thread->stack->locals[get_instr->dest] = *port->data;
          *port->data = NULL;
          *io_activity = true;
          break;
        }

        UNREACHABLE("get port must be an input or port value");
        break;
      }

      case FBLE_PUT_INSTR: {
        FblePutInstr* put_instr = (FblePutInstr*)instr;
        FbleValue* put_port = FrameGet(thread->stack, put_instr->port);
        FbleValue* arg = FrameGet(thread->stack, put_instr->arg);

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
          thread->stack->locals[put_instr->dest] = unit;
          *io_activity = true;
          break;
        }

        if (put_port->tag == FBLE_PORT_VALUE) {
          FblePortValue* port = (FblePortValue*)put_port;

          if (*port->data != NULL) {
            // Blocked on put. Restore the thread state and return before
            // logging progress.
            thread->stack->pc--;
            return BLOCKED;
          }

          *port->data = FbleValueRetain(heap, arg);
          thread->stack->locals[put_instr->dest] = unit;
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
        FbleValueRelease(heap, &link->_base);

        thread->stack->locals[link_instr->get] = get;
        thread->stack->locals[link_instr->put] = put;
        break;
      }

      case FBLE_FORK_INSTR: {
        FbleForkInstr* fork_instr = (FbleForkInstr*)instr;

        assert(thread->children.size == 0);
        assert(thread->children.xs == NULL);
        FbleVectorInit(arena, thread->children);

        for (size_t i = 0; i < fork_instr->args.size; ++i) {
          FbleProcValue* arg = (FbleProcValue*)FrameTaggedGet(FBLE_PROC_VALUE, thread->stack, fork_instr->args.xs[i]);
          FbleValueRetain(heap, &arg->_base);

          // You cannot execute a proc in a let binding, so it should be
          // impossible to ever have an undefined proc value.
          assert(arg != NULL && "undefined proc value");

          FbleValue** result = thread->stack->locals + fork_instr->dests.xs[i];

          Thread* child = FbleAlloc(arena, Thread);
          child->stack = PushFrame(arena, &arg->_base, arg->scope, arg->code, result, NULL);
          child->profile = FbleForkProfileThread(arena, thread->profile);
          child->children.size = 0;
          child->children.xs = NULL;
          child->next_child = 0;
          child->parent = thread;
          FbleVectorAppend(arena, thread->children, child);
        }
        thread->next_child = 0;
        return UNBLOCKED;
      }

      case FBLE_PROC_INSTR: {
        FbleProcInstr* proc_instr = (FbleProcInstr*)instr;
        FbleProcValue* proc = (FbleProcValue*)FrameTaggedGet(FBLE_PROC_VALUE, thread->stack, proc_instr->proc);

        // You cannot execute a proc in a let binding, so it should be
        // impossible to ever have an undefined proc value.
        assert(proc != NULL && "undefined proc value");

        FbleValueRetain(heap, &proc->_base);
        if (proc_instr->exit) {
          thread->stack = ReplaceFrame(heap, &proc->_base, proc->scope, proc->code, thread->stack);
        } else {
          FbleValue** result = thread->stack->locals + proc_instr->dest;
          thread->stack = PushFrame(arena, &proc->_base, proc->scope, proc->code, result, thread->stack);
        }
        break;
      }

      case FBLE_REF_VALUE_INSTR: {
        FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
        FbleRefValue* rv = FbleNewValue(heap, FbleRefValue);
        rv->_base.tag = FBLE_REF_VALUE;
        rv->value = NULL;

        thread->stack->locals[ref_instr->dest] = &rv->_base;
        break;
      }

      case FBLE_REF_DEF_INSTR: {
        FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
        FbleRefValue* rv = (FbleRefValue*)thread->stack->locals[ref_def_instr->ref];
        assert(rv->_base.tag == FBLE_REF_VALUE);

        FbleValue* value = FrameGet(thread->stack, ref_def_instr->value);
        assert(value != NULL);
        rv->value = value;
        FbleValueAddRef(heap, &rv->_base, rv->value);
        break;
      }

      case FBLE_RETURN_INSTR: {
        FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
        FbleValue* result = FrameGet(thread->stack, return_instr->result);
        *thread->stack->result = FbleValueRetain(heap, result);
        thread->stack = PopFrame(heap, thread->stack);
        break;
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
        FbleTypeValue* value = FbleNewValue(heap, FbleTypeValue);
        value->_base.tag = FBLE_TYPE_VALUE;
        thread->stack->locals[type_instr->dest] = &value->_base;
        break;
      }

      case FBLE_PROFILE_INSTR: {
        FbleProfileInstr* profile_instr = (FbleProfileInstr*)instr;
        switch (profile_instr->op) {
          case FBLE_PROFILE_ENTER_OP:
            FbleProfileEnterBlock(arena, thread->profile, profile_instr->data.enter.block);
            break;

          case FBLE_PROFILE_EXIT_OP:
            FbleProfileExitBlock(arena, thread->profile);
            break;

          case FBLE_PROFILE_AUTO_EXIT_OP: {
            FbleProfileAutoExitBlock(arena, thread->profile);
            break;
          }

          case FBLE_PROFILE_FUNC_EXIT_OP: {
            FbleFuncValue* func = (FbleFuncValue*)FrameTaggedGet(FBLE_FUNC_VALUE, thread->stack, profile_instr->data.func_exit.func);
            if (func == NULL) {
              FbleReportError("undefined function value apply\n", &profile_instr->data.func_exit.loc);
              return ABORTED;
            };

            if (func->argc > 1) {
              FbleProfileExitBlock(arena, thread->profile);
            } else {
              FbleProfileAutoExitBlock(arena, thread->profile);
            }
            break;
          }
        }

        break;
      }
    }
  }
  return FINISHED;
}

// RunThreads --
//   Run the given thread and its children to completion or until it can no
//   longer make progress.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to run.
//
// Results:
//   The status of running the threads.
//
// Side effects:
//   The thread and its children are executed and updated.
//   Updates the profile based on the execution.
static Status RunThreads(FbleValueHeap* heap, Thread* thread)
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
      Status status = RunThread(heap, thread, &unblocked);
      switch (status) {
        case FINISHED: {
          unblocked = true;
          if (thread->parent == NULL) {
            return FINISHED;
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

        case ABORTED: {
          return ABORTED;
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
//   Execute the given sequence of instructions to completion.
//
// Inputs:
//   heap - the value heap.
//   io - io to use for external ports.
//   statics - statics to use for evaluation. May be NULL.
//   code - the instructions to evaluate.
//   profile - profile to update with execution stats.
//
// Results:
//   The computed value, or NULL on error.
//
// Side effects:
//   The returned value must be freed with FbleValueRelease when no longer in
//   use. Prints a message to stderr in case of error.
//   Updates profile based on the execution.
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleValue** statics, FbleInstrBlock* code, FbleProfile* profile)
{
  FbleArena* arena = heap->arena;
  FbleValue* final_result = NULL;
  Thread thread = {
    .stack = PushFrame(arena, NULL, statics, code, &final_result, NULL),
    .profile = FbleNewProfileThread(arena, profile),
    .children = {0, NULL},
    .next_child = 0,
    .parent = NULL,
  };

  while (true) {
    Status status = RunThreads(heap, &thread);
    switch (status) {
      case FINISHED: {
        assert(final_result != NULL);
        assert(thread.stack == NULL);
        assert(thread.children.size == 0);
        assert(thread.children.xs == NULL);
        FbleFreeProfileThread(arena, thread.profile);
        return final_result;
      }

      case ABORTED: {
        AbortThread(heap, &thread);
        return NULL;
      }

      case BLOCKED: {
        // The thread is not making forward progress anymore. Block for I/O.
        if (!io->io(io, heap, true)) {
          FbleLoc loc = { .source = __FILE__, .line = 0, .col = 0 };
          FbleReportError("deadlock\n", &loc);
          AbortThread(heap, &thread);
          return NULL;
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
FbleValue* FbleEval(FbleValueHeap* heap, FbleProgram* program, FbleProfile* profile)
{
  FbleArena* arena = heap->arena;
  FbleInstrBlock* code = FbleCompile(arena, profile, program);
  if (code == NULL) {
    return NULL;
  }

  FbleIO io = { .io = &FbleNoIO };
  FbleValue* result = Eval(heap, &io, NULL, code, profile);
  FbleFreeInstrBlock(arena, code);
  return result;
}

// FbleApply -- see documentation in fble.h
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValueV args, FbleProfile* profile)
{
  assert(args.size > 0);
  assert(func->tag == FBLE_FUNC_VALUE);

  FbleInstr* instrs[args.size];

  FbleValue* xs[1 + args.size];
  xs[0] = func;

  FbleFuncApplyInstr applies[args.size];
  for (size_t i = 0; i < args.size; ++i) {
    instrs[i] = &applies[i]._base;
    xs[i+1] = args.xs[i];

    applies[i]._base.tag = FBLE_FUNC_APPLY_INSTR;
    applies[i].loc.source = __FILE__;
    applies[i].loc.line = __LINE__;
    applies[i].loc.col = 0;
    applies[i].exit = (i + 1 == args.size);
    applies[i].dest = i;

    if (i == 0) {
      applies[i].func.section = FBLE_STATICS_FRAME_SECTION;
      applies[i].func.index = 0;
    } else {
      applies[i].func.section = FBLE_LOCALS_FRAME_SECTION;
      applies[i].func.index = i - 1;
    }

    applies[i].arg.section = FBLE_STATICS_FRAME_SECTION;
    applies[i].arg.index = i + 1;
  }

  FbleInstrBlock code = {
    .refcount = 2,
    .statics = 1 + args.size,
    .locals = args.size,
    .instrs = { .size = args.size, .xs = instrs }
  };
  FbleIO io = { .io = &FbleNoIO, };
  FbleValue* result = Eval(heap, &io, xs, &code, profile);
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
  FbleProcValue* p = (FbleProcValue*)proc;
  return Eval(heap, io, p->scope, p->code, profile);
}
