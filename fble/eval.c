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

// Frame --
//   An execution frame.
//
// Fields:
//   scope - A value that owns the statics array. May be NULL.
//   statics - Variables captured from the parent scope.
//   locals - Space allocated for local variables.
//      Has length code->locals values. Takes ownership of all non-NULL
//      entries.
//   code - The currently executing instruction block.
//   pc - The location of the next instruction in the code to execute.
//   result - Where to store the result when exiting the scope.
typedef struct {
  FbleValue* scope;
  FbleValue** statics;
  FbleValue** locals;
  FbleInstrBlock* code;
  size_t pc;
  FbleValue** result;
} Frame;

// Stack --
//   An execution stack.
//
typedef struct Stack {
  Frame frame;
  struct Stack* tail;
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
//   children - child threads that this thread is potentially blocked on.
//      This vector is {0, NULL} when there are no child threads.
//   aborted - true if the thread has been aborted due to undefined union
//             field access.
//   profile - the profile thread associated with this thread.
struct Thread {
  Stack* stack;
  ThreadV children;
  bool aborted;
  FbleProfileThread* profile;
};

static FbleInstr g_put_instr = { .tag = FBLE_PUT_INSTR };
static FbleInstr* g_put_block_instrs[] = { &g_put_instr };
static FbleInstrBlock g_put_block = {
  .refcount = 1,
  .statics = 2,  // port, arg
  .locals = 0,
  .instrs = { .size = 1, .xs = g_put_block_instrs }
};

static void Add(FbleHeap* heap, FbleValue* src, FbleValue* dst);

static FbleValue* FrameGet(Frame* frame, FbleFrameIndex index);
static FbleValue* FrameTaggedGet(FbleValueTag tag, Frame* frame, FbleFrameIndex index);

static Stack* PushFrame(FbleArena* arena, FbleValue* scope, FbleValue** statics, FbleInstrBlock* code, FbleValue** result, Stack* tail);
static Stack* PopFrame(FbleValueHeap* heap, Stack* stack);
static Stack* ReplaceFrame(FbleValueHeap* heap, FbleValue* scope, FbleValue** statics, FbleInstrBlock* code, Stack* stack);

static bool RunThread(FbleValueHeap* heap, FbleIO* io, FbleProfile* profile, Thread* thread);
static void AbortThread(FbleValueHeap* heap, Thread* thread);
static bool RunThreads(FbleValueHeap* heap, FbleIO* io, FbleProfile* profile, Thread* thread);
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleValue** statics, FbleInstrBlock* code, FbleProfile* profile);

// Add --
//   Helper function for tracking ref value assignments.
//
// Inputs:
//   heap - the value heap
//   src - a source value
//   dst - a destination value
//
// Results:
//   none.
//
// Side effects:
//   Notifies the reference system that there is now a reference from src to
//   dst.
static void Add(FbleHeap* heap, FbleValue* src, FbleValue* dst)
{
  if (dst != NULL) {
    FbleValueAddRef(heap, src, dst);
  }
}

// FrameGet --
//   Get a value from the given frame.
//
// Inputs:
//   frame - the frame to access the value from.
//   index - the index of the value to access.
//
// Results:
//   The value in the frame at the given index.
//
// Side effects:
//   None.
static FbleValue* FrameGet(Frame* frame, FbleFrameIndex index)
{
  switch (index.section) {
    case FBLE_STATICS_FRAME_SECTION: return frame->statics[index.index];
    case FBLE_LOCALS_FRAME_SECTION: return frame->locals[index.index];
  }
  UNREACHABLE("should never get here");
}

// FrameTaggedGet --
//   Get and dereference a value from the given frame.
//   Dereferences the data value, removing all layers of reference values
//   until a non-reference value is encountered and returns the non-reference
//   value.
//
//   A tag for the type of dereferenced value should be provided. This
//   function will assert that the correct kind of value is encountered.
//
// Inputs:
//   tag - the expected tag of the value.
//   frame - the frame to get the value from.
//   index - the location of the value in the frame.
//
// Results:
//   The dereferenced value. Returns null in case of abstract value
//   dereference.
//
// Side effects:
//   The returned value will only stay alive as long as the original value on
//   the stack frame.
static FbleValue* FrameTaggedGet(FbleValueTag tag, Frame* frame, FbleFrameIndex index)
{
  FbleValue* original = FrameGet(frame, index);
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

  Stack* stack = FbleAlloc(arena, Stack);
  stack->frame.scope = scope;
  stack->frame.statics = statics;

  stack->frame.locals = FbleArrayAlloc(arena, FbleValue*, code->locals);
  memset(stack->frame.locals, 0, code->locals * sizeof(FbleValue*));

  stack->frame.code = code;
  stack->frame.pc = 0;
  stack->frame.result = result;
  stack->tail = tail;
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

  FbleValueRelease(heap, stack->frame.scope);

  for (size_t i = 0; i < stack->frame.code->locals; ++i) {
    FbleValueRelease(heap, stack->frame.locals[i]);
  }
  FbleFree(arena, stack->frame.locals);

  FbleFreeInstrBlock(arena, stack->frame.code);

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
  code->refcount++;

  FbleArena* arena = heap->arena;
  FbleValueRelease(heap, stack->frame.scope);
  stack->frame.scope = scope;
  stack->frame.statics = statics;

  for (size_t i = 0; i < stack->frame.code->locals; ++i) {
    FbleValueRelease(heap, stack->frame.locals[i]);
  }

  if (code->locals > stack->frame.code->locals) {
    FbleFree(arena, stack->frame.locals);
    stack->frame.locals = FbleArrayAlloc(arena, FbleValue*, code->locals);
  }
  memset(stack->frame.locals, 0, code->locals * sizeof(FbleValue*));

  FbleFreeInstrBlock(arena, stack->frame.code);
  stack->frame.code = code;
  stack->frame.pc = 0;
  return stack;
}

// RunThread --
//   Run the given thread to completion or until it can no longer make
//   progress.
//
// Inputs:
//   heap - the value heap.
//   io - io to use for external ports.
//   profile - the profile to save execution stats to.
//   thread - the thread to run.
//
// Results:
//   true if the thread made some progress, false otherwise.
//
// Side effects:
//   The thread is executed, updating its stack.
static bool RunThread(FbleValueHeap* heap, FbleIO* io, FbleProfile* profile, Thread* thread)
{
  FbleArena* arena = heap->arena;
  bool progress = false;
  while (thread->stack != NULL) {
    if (rand() % TIME_SLICE == 0) {
      FbleProfileSample(arena, thread->profile, 1);
      return true;
    }

    assert(thread->stack->frame.pc < thread->stack->frame.code->instrs.size);
    FbleInstr* instr = thread->stack->frame.code->instrs.xs[thread->stack->frame.pc++];
    switch (instr->tag) {
      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_value_instr->args.size;
        FbleValue* argv[argc];
        for (size_t i = 0; i < argc; ++i) {
          FbleValue* arg = FrameGet(&thread->stack->frame, struct_value_instr->args.xs[i]);
          argv[i] = FbleValueRetain(heap, arg);
        }

        FbleValueV args = { .size = argc, .xs = argv, };
        thread->stack->frame.locals[struct_value_instr->dest] = FbleNewStructValue(heap, args);
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        FbleValue* arg = FrameGet(&thread->stack->frame, union_value_instr->arg);
        thread->stack->frame.locals[union_value_instr->dest] = FbleNewUnionValue(heap, union_value_instr->tag, FbleValueRetain(heap, arg));
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleStructValue* sv = (FbleStructValue*)FrameTaggedGet(FBLE_STRUCT_VALUE, &thread->stack->frame, access_instr->obj);
        if (sv == NULL) {
          FbleReportError("undefined struct value access\n", &access_instr->loc);
          AbortThread(heap, thread);
          return true;
        }
        assert(access_instr->tag < sv->fields.size);
        thread->stack->frame.locals[access_instr->dest] = FbleValueRetain(heap, sv->fields.xs[access_instr->tag]);
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleUnionValue* uv = (FbleUnionValue*)FrameTaggedGet(FBLE_UNION_VALUE, &thread->stack->frame, access_instr->obj);
        if (uv == NULL) {
          FbleReportError("undefined union value access\n", &access_instr->loc);
          AbortThread(heap, thread);
          return true;
        }

        if (uv->tag != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);
          AbortThread(heap, thread);
          return true;
        }

        thread->stack->frame.locals[access_instr->dest] = FbleValueRetain(heap, uv->arg);
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
        FbleUnionValue* uv = (FbleUnionValue*)FrameTaggedGet(FBLE_UNION_VALUE, &thread->stack->frame, select_instr->condition);
        if (uv == NULL) {
          FbleReportError("undefined union value select\n", &select_instr->loc);
          AbortThread(heap, thread);
          return true;
        }
        thread->stack->frame.pc += uv->tag;
        break;
      }

      case FBLE_GOTO_INSTR: {
        FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
        thread->stack->frame.pc = goto_instr->pc;
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        FbleBasicFuncValue* value = FbleNewValue(heap, FbleBasicFuncValue);
        value->_base._base.tag = FBLE_FUNC_VALUE;
        value->_base.tag = FBLE_BASIC_FUNC_VALUE;
        value->_base.argc = func_value_instr->argc;
        value->code = func_value_instr->code;
        value->code->refcount++;
        FbleVectorInit(arena, value->scope);
        for (size_t i = 0; i < func_value_instr->scope.size; ++i) {
          FbleValue* arg = FrameGet(&thread->stack->frame, func_value_instr->scope.xs[i]);
          FbleValueRetain(heap, arg);
          FbleVectorAppend(arena, value->scope, arg);
        }
        thread->stack->frame.locals[func_value_instr->dest] = &value->_base._base;
        break;
      }

      case FBLE_RELEASE_INSTR: {
        FbleReleaseInstr* release = (FbleReleaseInstr*)instr;
        assert(thread->stack->frame.locals[release->value] != NULL);
        FbleValueRelease(heap, thread->stack->frame.locals[release->value]);
        thread->stack->frame.locals[release->value] = NULL;
        break;
      }

      case FBLE_FUNC_APPLY_INSTR: {
        FbleFuncApplyInstr* func_apply_instr = (FbleFuncApplyInstr*)instr;
        FbleFuncValue* func = (FbleFuncValue*)FrameTaggedGet(FBLE_FUNC_VALUE, &thread->stack->frame, func_apply_instr->func);
        if (func == NULL) {
          FbleReportError("undefined function value apply\n", &func_apply_instr->loc);
          AbortThread(heap, thread);
          return true;
        };
        FbleValue* arg = FrameGet(&thread->stack->frame, func_apply_instr->arg);

        if (func->argc > 1) {
          FbleThunkFuncValue* value = FbleNewValue(heap, FbleThunkFuncValue);
          value->_base._base.tag = FBLE_FUNC_VALUE;
          value->_base.tag = FBLE_THUNK_FUNC_VALUE;
          value->_base.argc = func->argc - 1;
          value->func = func;
          Add(heap, &value->_base._base, &value->func->_base);
          value->arg = arg;
          Add(heap, &value->_base._base, value->arg);

          if (func_apply_instr->exit) {
            *thread->stack->frame.result = &value->_base._base;
            thread->stack = PopFrame(heap, thread->stack);
            FbleProfileExitBlock(arena, thread->profile);
          } else {
            thread->stack->frame.locals[func_apply_instr->dest] = &value->_base._base;
          }
        } else if (func->tag == FBLE_PUT_FUNC_VALUE) {
          FblePutFuncValue* f = (FblePutFuncValue*)func;

          FbleProcValue* value = FbleNewValue(heap, FbleProcValue);
          value->_base.tag = FBLE_PROC_VALUE;
          FbleVectorInit(arena, value->scope);

          FbleVectorAppend(arena, value->scope, f->port);
          Add(heap, &value->_base, f->port);

          FbleVectorAppend(arena, value->scope, arg);
          Add(heap, &value->_base, arg);

          value->code = &g_put_block;
          value->code->refcount++;

          if (func_apply_instr->exit) {
            *thread->stack->frame.result = &value->_base;
            thread->stack = PopFrame(heap, thread->stack);
            FbleProfileExitBlock(arena, thread->profile);
          } else {
            thread->stack->frame.locals[func_apply_instr->dest] = &value->_base;
          }
        } else {
          FbleFuncValue* f = func;

          FbleValueV args;
          FbleVectorInit(arena, args);
          FbleValueRetain(heap, arg);
          FbleVectorAppend(arena, args, arg);
          while (f->tag == FBLE_THUNK_FUNC_VALUE) {
            FbleThunkFuncValue* thunk = (FbleThunkFuncValue*)f;
            FbleValueRetain(heap, thunk->arg);
            FbleVectorAppend(arena, args, thunk->arg);
            f = thunk->func;
          }

          assert(f->tag == FBLE_BASIC_FUNC_VALUE);
          FbleBasicFuncValue* basic = (FbleBasicFuncValue*)f;
          FbleValueRetain(heap, &basic->_base._base);
          if (func_apply_instr->exit) {
            thread->stack = ReplaceFrame(heap, &basic->_base._base, basic->scope.xs, basic->code, thread->stack);
            FbleProfileAutoExitBlock(arena, thread->profile);
          } else {
            FbleValue** result = thread->stack->frame.locals + func_apply_instr->dest;
            thread->stack = PushFrame(arena, &basic->_base._base, basic->scope.xs, basic->code, result, thread->stack);
          }

          for (size_t i = 0; i < args.size; ++i) {
            size_t j = args.size - i - 1;
            thread->stack->frame.locals[i] = args.xs[j];
          }
          FbleFree(arena, args.xs);
        }
        break;
      }

      case FBLE_PROC_VALUE_INSTR: {
        FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
        FbleProcValue* value = FbleNewValue(heap, FbleProcValue);
        value->_base.tag = FBLE_PROC_VALUE;
        value->code = proc_value_instr->code;
        value->code->refcount++;
        FbleVectorInit(arena, value->scope);
        for (size_t i = 0; i < proc_value_instr->scope.size; ++i) {
          FbleValue* arg = FrameGet(&thread->stack->frame, proc_value_instr->scope.xs[i]);
          FbleValueRetain(heap, arg);
          FbleVectorAppend(arena, value->scope, arg);
        }
        thread->stack->frame.locals[proc_value_instr->dest] = &value->_base;
        break;
      }

      case FBLE_COPY_INSTR: {
        FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
        FbleValue* value = FrameGet(&thread->stack->frame, copy_instr->source);
        thread->stack->frame.locals[copy_instr->dest] = FbleValueRetain(heap, value);
        break;
      }

      case FBLE_GET_INSTR: {
        FbleValue* get_port = thread->stack->frame.statics[0];
        if (get_port->tag == FBLE_LINK_VALUE) {
          FbleLinkValue* link = (FbleLinkValue*)get_port;

          if (link->head == NULL) {
            // Blocked on get. Restore the thread state and return before
            // logging progress.
            thread->stack->frame.pc--;
            return progress;
          }

          FbleValues* head = link->head;
          link->head = link->head->next;
          if (link->head == NULL) {
            link->tail = NULL;
          }

          *thread->stack->frame.result = FbleValueRetain(heap, head->value);
          FbleValueDelRef(heap, &link->_base, head->value);

          thread->stack = PopFrame(heap, thread->stack);
          FbleFree(arena, head);
          break;
        }

        if (get_port->tag == FBLE_PORT_VALUE) {
          FblePortValue* port = (FblePortValue*)get_port;
          assert(port->id < io->ports.size);
          if (io->ports.xs[port->id] == NULL) {
            // Blocked on get. Restore the thread state and return before
            // logging progress.
            thread->stack->frame.pc--;
            return progress;
          }

          *thread->stack->frame.result = io->ports.xs[port->id];
          thread->stack = PopFrame(heap, thread->stack);
          io->ports.xs[port->id] = NULL;
          break;
        }

        UNREACHABLE("get port must be an input or port value");
        break;
      }

      case FBLE_PUT_INSTR: {
        FbleValue* put_port = thread->stack->frame.statics[0];
        FbleValue* arg = thread->stack->frame.statics[1];

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

          Add(heap, &link->_base, tail->value);

          *thread->stack->frame.result = unit;
          thread->stack = PopFrame(heap, thread->stack);
          break;
        }

        if (put_port->tag == FBLE_PORT_VALUE) {
          FblePortValue* port = (FblePortValue*)put_port;
          assert(port->id < io->ports.size);

          if (io->ports.xs[port->id] != NULL) {
            // Blocked on put. Restore the thread state and return before
            // logging progress.
            thread->stack->frame.pc--;
            return progress;
          }

          io->ports.xs[port->id] = FbleValueRetain(heap, arg);
          *thread->stack->frame.result = unit;
          thread->stack = PopFrame(heap, thread->stack);
          break;
        }

        UNREACHABLE("put port must be an output or port value");
        break;
      }

      case FBLE_LINK_INSTR: {
        FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;

        FbleLinkValue* port = FbleNewValue(heap, FbleLinkValue);
        port->_base.tag = FBLE_LINK_VALUE;
        port->head = NULL;
        port->tail = NULL;

        FbleValue* get = FbleNewGetProcValue(heap, &port->_base);

        FblePutFuncValue* put = FbleNewValue(heap, FblePutFuncValue);
        put->_base._base.tag = FBLE_FUNC_VALUE;
        put->_base.tag = FBLE_PUT_FUNC_VALUE;
        put->_base.argc = 1;
        put->port = &port->_base;
        Add(heap, &put->_base._base, put->port);

        FbleValueRelease(heap, &port->_base);

        thread->stack->frame.locals[link_instr->get] = get;
        thread->stack->frame.locals[link_instr->put] = &put->_base._base;
        break;
      }

      case FBLE_FORK_INSTR: {
        FbleForkInstr* fork_instr = (FbleForkInstr*)instr;

        assert(thread->children.size == 0);
        assert(thread->children.xs == NULL);
        FbleVectorInit(arena, thread->children);

        for (size_t i = 0; i < fork_instr->args.size; ++i) {
          FbleProcValue* arg = (FbleProcValue*)FrameTaggedGet(FBLE_PROC_VALUE, &thread->stack->frame, fork_instr->args.xs[i]);
          FbleValueRetain(heap, &arg->_base);

          // You cannot execute a proc in a let binding, so it should be
          // impossible to ever have an undefined proc value.
          assert(arg != NULL && "undefined proc value");

          FbleValue** result = thread->stack->frame.locals + fork_instr->dests.xs[i];

          Thread* child = FbleAlloc(arena, Thread);
          child->stack = PushFrame(arena, &arg->_base, arg->scope.xs, arg->code, result, NULL);
          child->aborted = false;
          child->children.size = 0;
          child->children.xs = NULL;
          child->profile = FbleNewProfileThread(arena, thread->profile, profile);
          FbleVectorAppend(arena, thread->children, child);
        }
        break;
      }

      case FBLE_JOIN_INSTR: {
        assert(thread->children.size > 0);
        for (size_t i = 0; i < thread->children.size; ++i) {
          if (thread->children.xs[i]->aborted) {
            AbortThread(heap, thread);
            return true;
          }
        }

        for (size_t i = 0; i < thread->children.size; ++i) {
          if (thread->children.xs[i]->stack != NULL) {
            // Blocked on child. Restore the thread state and return before
            // logging progress
            thread->stack->frame.pc--;
            return progress;
          }
        }

        for (size_t i = 0; i < thread->children.size; ++i) {
          Thread* child = thread->children.xs[i];
          assert(child->stack == NULL);
          FbleFreeProfileThread(arena, child->profile);
          FbleFree(arena, child);
        }

        thread->children.size = 0;
        FbleFree(arena, thread->children.xs);
        thread->children.xs = NULL;
        break;
      }

      case FBLE_PROC_INSTR: {
        FbleProcInstr* proc_instr = (FbleProcInstr*)instr;
        FbleProcValue* proc = (FbleProcValue*)FrameTaggedGet(FBLE_PROC_VALUE, &thread->stack->frame, proc_instr->proc);

        // You cannot execute a proc in a let binding, so it should be
        // impossible to ever have an undefined proc value.
        assert(proc != NULL && "undefined proc value");

        FbleValueRetain(heap, &proc->_base);

        thread->stack = ReplaceFrame(heap, &proc->_base, proc->scope.xs, proc->code, thread->stack);
        FbleProfileAutoExitBlock(arena, thread->profile);
        break;
      }

      case FBLE_REF_VALUE_INSTR: {
        FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
        FbleRefValue* rv = FbleNewValue(heap, FbleRefValue);
        rv->_base.tag = FBLE_REF_VALUE;
        rv->value = NULL;

        thread->stack->frame.locals[ref_instr->dest] = &rv->_base;
        break;
      }

      case FBLE_REF_DEF_INSTR: {
        FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
        FbleRefValue* rv = (FbleRefValue*)thread->stack->frame.locals[ref_def_instr->ref];
        assert(rv->_base.tag == FBLE_REF_VALUE);

        FbleValue* value = FrameGet(&thread->stack->frame, ref_def_instr->value);
        assert(value != NULL);
        rv->value = value;
        Add(heap, &rv->_base, rv->value);
        break;
      }

      case FBLE_RETURN_INSTR: {
        FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
        FbleValue* result = FrameGet(&thread->stack->frame, return_instr->result);
        *thread->stack->frame.result = FbleValueRetain(heap, result);
        thread->stack = PopFrame(heap, thread->stack);
        FbleProfileExitBlock(arena, thread->profile);
        break;
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
        FbleTypeValue* value = FbleNewValue(heap, FbleTypeValue);
        value->_base.tag = FBLE_TYPE_VALUE;
        thread->stack->frame.locals[type_instr->dest] = &value->_base;
        break;
      }

      case FBLE_PROFILE_ENTER_BLOCK_INSTR: {
        FbleProfileEnterBlockInstr* enter = (FbleProfileEnterBlockInstr*)instr;
        FbleProfileEnterBlock(arena, thread->profile, enter->block);
        break;
      }

      case FBLE_PROFILE_EXIT_BLOCK_INSTR: {
        FbleProfileExitBlock(arena, thread->profile);
        break;
      }

      case FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR: {
        FbleProfileAutoExitBlock(arena, thread->profile);
        break;
      }
    }

    progress = true;
  }
  return progress;
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
//   Cleans up the thread state by unwinding the thread. Sets the thread state

static void AbortThread(FbleValueHeap* heap, Thread* thread)
{
  thread->aborted = true;

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

// RunThreads --
//   Run the given thread and its children to completion or until it can no
//   longer make progress.
//
// Inputs:
//   heap - the value heap.
//   io - io to use for external ports.
//   profile - profile to save execution stats to.
//   thread - the thread to run.
//
// Results:
//   True if progress was made, false otherwise.
//
// Side effects:
//   The thread and its children are executed and updated.
//   Updates the profile based on the execution.
static bool RunThreads(FbleValueHeap* heap, FbleIO* io, FbleProfile* profile, Thread* thread)
{
  // Note: It's possible we smash the stack with this recursive implementation
  // of RunThreads, but the amount of stack space we need for each frame is so
  // small it doesn't seem worth worrying about for the time being.

  bool progress = false;
  for (size_t i = 0; i < thread->children.size; ++i) {
    Thread* child = thread->children.xs[i];
    progress = RunThreads(heap, io, profile, child) || progress;
  }

  // If we have child threads and they made some progress, don't bother
  // running the parent thread, because it's probably blocked on a child
  // thread anyway.
  if (!progress) {
    progress = RunThread(heap, io, profile, thread);
  }
  return progress;
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
    .children = {0, NULL},
    .aborted = false,
    .profile = FbleNewProfileThread(arena, NULL, profile),
  };

  // Run the main thread repeatedly until it no longer makes any forward
  // progress.
  bool progress = false;
  do {
    progress = RunThreads(heap, io, profile, &thread);
    if (thread.aborted) {
      return NULL;
    }

    bool block = (!progress) && (thread.stack != NULL);
    progress = io->io(io, heap, block) || progress;
  } while (progress);

  if (thread.stack != NULL) {
    // We have instructions to run still, but we stopped making forward
    // progress. This must be a deadlock.
    FbleLoc loc = { .source = __FILE__, .line = 0, .col = 0 };
    FbleReportError("deadlock\n", &loc);
    AbortThread(heap, &thread);
    return NULL;
  }

  // The thread should now be finished
  assert(final_result != NULL);
  assert(thread.stack == NULL);
  assert(thread.children.size == 0);
  assert(thread.children.xs == NULL);
  FbleFreeProfileThread(arena, thread.profile);
  return final_result;
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleValueHeap* heap, FbleProgram* program, FbleNameV* blocks, FbleProfile** profile)
{
  FbleArena* arena = heap->arena;

  FbleInstrBlock* code = FbleCompile(arena, blocks, program);
  *profile = FbleNewProfile(arena, blocks->size);
  if (code == NULL) {
    return NULL;
  }

  FbleIO io = { .io = &FbleNoIO, .ports = { .size = 0, .xs = NULL} };
  FbleValue* result = Eval(heap, &io, NULL, code, *profile);
  FbleFreeInstrBlock(arena, code);
  return result;
}

// FbleApply -- see documentation in fble.h
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValueV args, FbleProfile* profile)
{
  assert(args.size > 0);
  assert(func->tag == FBLE_FUNC_VALUE);

  FbleProfileEnterBlockInstr enter = {
    ._base = { .tag = FBLE_PROFILE_ENTER_BLOCK_INSTR },
    .block = FBLE_ROOT_BLOCK_ID,
  };

  FbleInstr* instrs[1 + args.size];
  instrs[0] = &enter._base;

  FbleValue* xs[1 + args.size];
  xs[0] = func;

  FbleFuncApplyInstr applies[args.size];
  for (size_t i = 0; i < args.size; ++i) {
    instrs[i+1] = &applies[i]._base;
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
    .instrs = { .size = 1 + args.size, .xs = instrs }
  };
  FbleIO io = { .io = &FbleNoIO, .ports = { .size = 0, .xs = NULL} };
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
  return Eval(heap, io, p->scope.xs, p->code, profile);
}
