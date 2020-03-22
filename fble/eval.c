// eval.c --
//   This file implements the fble evaluation routines.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for NULL, abort
#include <string.h>   // for memset

#include "internal.h"

#define UNREACHABLE(x) assert(false && x)

// TIME_SLICE --
//   The number of instructions a thread is allowed to run before switching to
//   another thread.
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

static FbleProfileEnterBlockInstr g_enter_instr = {
  ._base = { .tag = FBLE_PROFILE_ENTER_BLOCK_INSTR },
  .block = 0,
  .time = 1,
};

static FbleInstr g_put_instr = { .tag = FBLE_PUT_INSTR };
static FbleInstr* g_put_block_instrs[] = { &g_put_instr };
static FbleInstrBlock g_put_block = {
  .refcount = 1,
  .statics = 2,  // port, arg
  .locals = 0,
  .instrs = { .size = 1, .xs = g_put_block_instrs }
};

static void Add(FbleRefArena* arena, FbleValue* src, FbleValue* dst);

static FbleValue* FrameGet(Frame* frame, FbleFrameIndex index);
static FbleValue* FrameTaggedGet(FbleValueTag tag, Frame* frame, FbleFrameIndex index);

static Stack* PushFrame(FbleArena* arena, FbleValue* scope, FbleValue** statics, FbleInstrBlock* code, FbleValue** result, Stack* tail);
static Stack* PopFrame(FbleValueArena* arena, Stack* stack);
static Stack* ReplaceFrame(FbleValueArena* arena, FbleValue* scope, FbleValue** statics, FbleInstrBlock* code, Stack* stack);

static bool RunThread(FbleValueArena* arena, FbleIO* io, FbleProfile* profile, Thread* thread);
static void AbortThread(FbleValueArena* arena, Thread* thread);
static bool RunThreads(FbleValueArena* arena, FbleIO* io, FbleProfile* profile, Thread* thread);
static FbleValue* Eval(FbleValueArena* arena, FbleIO* io, FbleValue** statics, FbleInstrBlock* code, FbleProfile* profile);

static bool NoIO(FbleIO* io, FbleValueArena* arena, bool block);

// Add --
//   Helper function for tracking ref value assignments.
//
// Inputs:
//   arena - the value arena
//   src - a source value
//   dst - a destination value
//
// Results:
//   none.
//
// Side effects:
//   Notifies the reference system that there is now a reference from src to
//   dst.
static void Add(FbleRefArena* arena, FbleValue* src, FbleValue* dst)
{
  if (dst != NULL) {
    FbleRefAdd(arena, &src->ref, &dst->ref);
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
//   arena - the arena to use for deallocation
//   stack - the stack to pop from
//
// Results:
//   The popped stack.
//
// Side effects:
//   Releases any remaining variables on the frame and frees the frame.
static Stack* PopFrame(FbleValueArena* arena, Stack* stack)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  FbleValueRelease(arena, stack->frame.scope);

  for (size_t i = 0; i < stack->frame.code->locals; ++i) {
    FbleValueRelease(arena, stack->frame.locals[i]);
  }
  FbleFree(arena_, stack->frame.locals);

  FbleFreeInstrBlock(arena_, stack->frame.code);

  Stack* tail = stack->tail;
  FbleFree(arena_, stack);
  return tail;
}

// ReplaceFrame --
//   Replace the current frame with a new one.
//
// Inputs:
//   arena - the arena to use for allocations.
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
static Stack* ReplaceFrame(FbleValueArena* arena, FbleValue* scope, FbleValue** statics, FbleInstrBlock* code, Stack* stack)
{
  code->refcount++;

  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleValueRelease(arena, stack->frame.scope);
  stack->frame.scope = scope;
  stack->frame.statics = statics;

  for (size_t i = 0; i < stack->frame.code->locals; ++i) {
    FbleValueRelease(arena, stack->frame.locals[i]);
  }

  if (code->locals > stack->frame.code->locals) {
    FbleFree(arena_, stack->frame.locals);
    stack->frame.locals = FbleArrayAlloc(arena_, FbleValue*, code->locals);
  }
  memset(stack->frame.locals, 0, code->locals * sizeof(FbleValue*));

  FbleFreeInstrBlock(arena_, stack->frame.code);
  stack->frame.code = code;
  stack->frame.pc = 0;
  return stack;
}

// RunThread --
//   Run the given thread to completion or until it can no longer make
//   progress.
//
// Inputs:
//   arena - the arena to use for allocations.
//   io - io to use for external ports.
//   profile - the profile to save execution stats to.
//   thread - the thread to run.
//
// Results:
//   true if the thread made some progress, false otherwise.
//
// Side effects:
//   The thread is executed, updating its stack.
static bool RunThread(FbleValueArena* arena, FbleIO* io, FbleProfile* profile, Thread* thread)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  bool progress = false;
  for (size_t i = 0; i < TIME_SLICE && thread->stack != NULL; ++i) {
    assert(thread->stack->frame.pc < thread->stack->frame.code->instrs.size);
    FbleInstr* instr = thread->stack->frame.code->instrs.xs[thread->stack->frame.pc++];
    switch (instr->tag) {
      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_value_instr->args.size;
        FbleValue* argv[argc];
        for (size_t i = 0; i < argc; ++i) {
          FbleValue* arg = FrameGet(&thread->stack->frame, struct_value_instr->args.xs[i]);
          argv[i] = FbleValueRetain(arena, arg);
        }

        FbleValueV args = { .size = argc, .xs = argv, };
        thread->stack->frame.locals[struct_value_instr->dest] = FbleNewStructValue(arena, args);
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        FbleValue* arg = FrameGet(&thread->stack->frame, union_value_instr->arg);
        thread->stack->frame.locals[union_value_instr->dest] = FbleNewUnionValue(arena, union_value_instr->tag, FbleValueRetain(arena, arg));
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleStructValue* sv = (FbleStructValue*)FrameTaggedGet(FBLE_STRUCT_VALUE, &thread->stack->frame, access_instr->obj);
        if (sv == NULL) {
          FbleReportError("undefined struct value access\n", &access_instr->loc);
          AbortThread(arena, thread);
          return progress;
        }
        assert(access_instr->tag < sv->fields.size);
        thread->stack->frame.locals[access_instr->dest] = FbleValueRetain(arena, sv->fields.xs[access_instr->tag]);
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleUnionValue* uv = (FbleUnionValue*)FrameTaggedGet(FBLE_UNION_VALUE, &thread->stack->frame, access_instr->obj);
        if (uv == NULL) {
          FbleReportError("undefined union value access\n", &access_instr->loc);
          AbortThread(arena, thread);
          return progress;
        }

        if (uv->tag != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);
          AbortThread(arena, thread);
          return progress;
        }

        thread->stack->frame.locals[access_instr->dest] = FbleValueRetain(arena, uv->arg);
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
        FbleUnionValue* uv = (FbleUnionValue*)FrameTaggedGet(FBLE_UNION_VALUE, &thread->stack->frame, select_instr->condition);
        if (uv == NULL) {
          FbleReportError("undefined union value select\n", &select_instr->loc);
          AbortThread(arena, thread);
          return progress;
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
        FbleBasicFuncValue* value = FbleAlloc(arena_, FbleBasicFuncValue);
        FbleRefInit(arena, &value->_base._base.ref);
        value->_base._base.tag = FBLE_FUNC_VALUE;
        value->_base.tag = FBLE_BASIC_FUNC_VALUE;
        value->_base.argc = func_value_instr->argc;
        value->code = func_value_instr->code;
        value->code->refcount++;
        FbleVectorInit(arena_, value->scope);
        for (size_t i = 0; i < func_value_instr->scope.size; ++i) {
          FbleValue* arg = FrameGet(&thread->stack->frame, func_value_instr->scope.xs[i]);
          FbleValueRetain(arena, arg);
          FbleVectorAppend(arena_, value->scope, arg);
        }
        thread->stack->frame.locals[func_value_instr->dest] = &value->_base._base;
        break;
      }

      case FBLE_RELEASE_INSTR: {
        FbleReleaseInstr* release = (FbleReleaseInstr*)instr;
        assert(thread->stack->frame.locals[release->value] != NULL);
        FbleValueRelease(arena, thread->stack->frame.locals[release->value]);
        thread->stack->frame.locals[release->value] = NULL;
        break;
      }

      case FBLE_FUNC_APPLY_INSTR: {
        FbleFuncApplyInstr* func_apply_instr = (FbleFuncApplyInstr*)instr;
        FbleFuncValue* func = (FbleFuncValue*)FrameTaggedGet(FBLE_FUNC_VALUE, &thread->stack->frame, func_apply_instr->func);
        if (func == NULL) {
          FbleReportError("undefined function value apply\n", &func_apply_instr->loc);
          AbortThread(arena, thread);
          return progress;
        };
        FbleValue* arg = FrameGet(&thread->stack->frame, func_apply_instr->arg);

        if (func->argc > 1) {
          FbleThunkFuncValue* value = FbleAlloc(arena_, FbleThunkFuncValue);
          FbleRefInit(arena, &value->_base._base.ref);
          value->_base._base.tag = FBLE_FUNC_VALUE;
          value->_base.tag = FBLE_THUNK_FUNC_VALUE;
          value->_base.argc = func->argc - 1;
          value->func = func;
          Add(arena, &value->_base._base, &value->func->_base);
          value->arg = arg;
          Add(arena, &value->_base._base, value->arg);

          if (func_apply_instr->exit) {
            *thread->stack->frame.result = &value->_base._base;
            thread->stack = PopFrame(arena, thread->stack);
            FbleProfileExitBlock(arena_, thread->profile);
          } else {
            thread->stack->frame.locals[func_apply_instr->dest] = &value->_base._base;
          }
        } else if (func->tag == FBLE_PUT_FUNC_VALUE) {
          FblePutFuncValue* f = (FblePutFuncValue*)func;

          FbleProcValue* value = FbleAlloc(arena_, FbleProcValue);
          FbleRefInit(arena, &value->_base.ref);
          value->_base.tag = FBLE_PROC_VALUE;
          FbleVectorInit(arena_, value->scope);

          FbleVectorAppend(arena_, value->scope, f->port);
          Add(arena, &value->_base, f->port);

          FbleVectorAppend(arena_, value->scope, arg);
          Add(arena, &value->_base, arg);

          value->code = &g_put_block;
          value->code->refcount++;

          if (func_apply_instr->exit) {
            *thread->stack->frame.result = &value->_base;
            thread->stack = PopFrame(arena, thread->stack);
            FbleProfileExitBlock(arena_, thread->profile);
          } else {
            thread->stack->frame.locals[func_apply_instr->dest] = &value->_base;
          }
        } else {
          FbleFuncValue* f = func;

          FbleValueV args;
          FbleVectorInit(arena_, args);
          FbleValueRetain(arena, arg);
          FbleVectorAppend(arena_, args, arg);
          while (f->tag == FBLE_THUNK_FUNC_VALUE) {
            FbleThunkFuncValue* thunk = (FbleThunkFuncValue*)f;
            FbleValueRetain(arena, thunk->arg);
            FbleVectorAppend(arena_, args, thunk->arg);
            f = thunk->func;
          }

          assert(f->tag == FBLE_BASIC_FUNC_VALUE);
          FbleBasicFuncValue* basic = (FbleBasicFuncValue*)f;
          FbleValueRetain(arena, &basic->_base._base);
          if (func_apply_instr->exit) {
            thread->stack = ReplaceFrame(arena, &basic->_base._base, basic->scope.xs, basic->code, thread->stack);
            FbleProfileAutoExitBlock(arena_, thread->profile);
          } else {
            FbleValue** result = thread->stack->frame.locals + func_apply_instr->dest;
            thread->stack = PushFrame(arena_, &basic->_base._base, basic->scope.xs, basic->code, result, thread->stack);
          }

          for (size_t i = 0; i < args.size; ++i) {
            size_t j = args.size - i - 1;
            thread->stack->frame.locals[i] = args.xs[j];
          }
          FbleFree(arena_, args.xs);
        }
        break;
      }

      case FBLE_PROC_VALUE_INSTR: {
        FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
        FbleProcValue* value = FbleAlloc(arena_, FbleProcValue);
        FbleRefInit(arena, &value->_base.ref);
        value->_base.tag = FBLE_PROC_VALUE;
        value->code = proc_value_instr->code;
        value->code->refcount++;
        FbleVectorInit(arena_, value->scope);
        for (size_t i = 0; i < proc_value_instr->scope.size; ++i) {
          FbleValue* arg = FrameGet(&thread->stack->frame, proc_value_instr->scope.xs[i]);
          FbleValueRetain(arena, arg);
          FbleVectorAppend(arena_, value->scope, arg);
        }
        thread->stack->frame.locals[proc_value_instr->dest] = &value->_base;
        break;
      }

      case FBLE_COPY_INSTR: {
        FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
        FbleValue* value = FrameGet(&thread->stack->frame, copy_instr->source);
        thread->stack->frame.locals[copy_instr->dest] = FbleValueRetain(arena, value);
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


          *thread->stack->frame.result = head->value;
          thread->stack = PopFrame(arena, thread->stack);
          FbleFree(arena_, head);
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
          thread->stack = PopFrame(arena, thread->stack);
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
        FbleValue* unit = FbleNewStructValue(arena, args);

        if (put_port->tag == FBLE_LINK_VALUE) {
          FbleLinkValue* link = (FbleLinkValue*)put_port;

          FbleValues* tail = FbleAlloc(arena_, FbleValues);
          tail->value = FbleValueRetain(arena, arg);
          tail->next = NULL;

          if (link->head == NULL) {
            link->head = tail;
            link->tail = tail;
          } else {
            assert(link->tail != NULL);
            link->tail->next = tail;
            link->tail = tail;
          }

          *thread->stack->frame.result = unit;
          thread->stack = PopFrame(arena, thread->stack);
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

          io->ports.xs[port->id] = FbleValueRetain(arena, arg);
          *thread->stack->frame.result = unit;
          thread->stack = PopFrame(arena, thread->stack);
          break;
        }

        UNREACHABLE("put port must be an output or port value");
        break;
      }

      case FBLE_LINK_INSTR: {
        FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;

        FbleLinkValue* port = FbleAlloc(arena_, FbleLinkValue);
        FbleRefInit(arena, &port->_base.ref);
        port->_base.tag = FBLE_LINK_VALUE;
        port->head = NULL;
        port->tail = NULL;

        FbleValue* get = FbleNewGetProcValue(arena, &port->_base);

        FblePutFuncValue* put = FbleAlloc(arena_, FblePutFuncValue);
        FbleRefInit(arena, &put->_base._base.ref);
        put->_base._base.tag = FBLE_FUNC_VALUE;
        put->_base.tag = FBLE_PUT_FUNC_VALUE;
        put->_base.argc = 1;
        put->port = &port->_base;
        Add(arena, &put->_base._base, put->port);

        FbleValueRelease(arena, &port->_base);

        thread->stack->frame.locals[link_instr->get] = get;
        thread->stack->frame.locals[link_instr->put] = &put->_base._base;
        break;
      }

      case FBLE_FORK_INSTR: {
        FbleForkInstr* fork_instr = (FbleForkInstr*)instr;

        assert(thread->children.size == 0);
        assert(thread->children.xs == NULL);
        FbleVectorInit(arena_, thread->children);

        for (size_t i = 0; i < fork_instr->args.size; ++i) {
          FbleProcValue* arg = (FbleProcValue*)FrameTaggedGet(FBLE_PROC_VALUE, &thread->stack->frame, fork_instr->args.xs[i]);
          FbleValueRetain(arena, &arg->_base);

          // You cannot execute a proc in a let binding, so it should be
          // impossible to ever have an undefined proc value.
          assert(arg != NULL && "undefined proc value");

          FbleValue** result = thread->stack->frame.locals + fork_instr->dests.xs[i];

          Thread* child = FbleAlloc(arena_, Thread);
          child->stack = PushFrame(arena_, &arg->_base, arg->scope.xs, arg->code, result, NULL);
          child->aborted = false;
          child->children.size = 0;
          child->children.xs = NULL;
          child->profile = FbleNewProfileThread(arena_, profile);
          FbleVectorAppend(arena_, thread->children, child);
        }
        break;
      }

      case FBLE_JOIN_INSTR: {
        assert(thread->children.size > 0);
        for (size_t i = 0; i < thread->children.size; ++i) {
          if (thread->children.xs[i]->aborted) {
            AbortThread(arena, thread);
            return progress;
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
          FbleFreeProfileThread(arena_, child->profile);
          FbleFree(arena_, child);
        }

        thread->children.size = 0;
        FbleFree(arena_, thread->children.xs);
        thread->children.xs = NULL;
        break;
      }

      case FBLE_PROC_INSTR: {
        FbleProcInstr* proc_instr = (FbleProcInstr*)instr;
        FbleProcValue* proc = (FbleProcValue*)FrameTaggedGet(FBLE_PROC_VALUE, &thread->stack->frame, proc_instr->proc);

        // You cannot execute a proc in a let binding, so it should be
        // impossible to ever have an undefined proc value.
        assert(proc != NULL && "undefined proc value");

        FbleValueRetain(arena, &proc->_base);

        thread->stack = ReplaceFrame(arena, &proc->_base, proc->scope.xs, proc->code, thread->stack);
        FbleProfileAutoExitBlock(arena_, thread->profile);
        break;
      }

      case FBLE_REF_VALUE_INSTR: {
        FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
        FbleRefValue* rv = FbleAlloc(arena_, FbleRefValue);
        FbleRefInit(arena, &rv->_base.ref);
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
        Add(arena, &rv->_base, rv->value);
        break;
      }

      case FBLE_STRUCT_IMPORT_INSTR: {
        FbleStructImportInstr* import_instr = (FbleStructImportInstr*)instr;
        FbleStructValue* sv = (FbleStructValue*)FrameTaggedGet(FBLE_STRUCT_VALUE, &thread->stack->frame, import_instr->obj);
        if (sv == NULL) {
          FbleReportError("undefined struct value import\n", &import_instr->loc);
          AbortThread(arena, thread);
          return progress;
        }
        for (size_t i = 0; i < sv->fields.size; ++i) {
          thread->stack->frame.locals[import_instr->fields.xs[i]] = FbleValueRetain(arena, sv->fields.xs[i]);
        }
        break;
      }

      case FBLE_RETURN_INSTR: {
        FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
        FbleValue* result = FrameGet(&thread->stack->frame, return_instr->result);
        *thread->stack->frame.result = FbleValueRetain(arena, result);
        thread->stack = PopFrame(arena, thread->stack);
        FbleProfileExitBlock(arena_, thread->profile);
        break;
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
        FbleTypeValue* value = FbleAlloc(arena_, FbleTypeValue);
        FbleRefInit(arena, &value->_base.ref);
        value->_base.tag = FBLE_TYPE_VALUE;
        thread->stack->frame.locals[type_instr->dest] = &value->_base;
        break;
      }

      case FBLE_PROFILE_ENTER_BLOCK_INSTR: {
        FbleProfileEnterBlockInstr* enter = (FbleProfileEnterBlockInstr*)instr;
        FbleProfileEnterBlock(arena_, thread->profile, enter->block);
        FbleProfileTime(arena_, thread->profile, enter->time);
        break;
      }

      case FBLE_PROFILE_EXIT_BLOCK_INSTR: {
        FbleProfileExitBlock(arena_, thread->profile);
        break;
      }

      case FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR: {
        FbleProfileAutoExitBlock(arena_, thread->profile);
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
//   arena - the arena to use for allocations.
//   thread - the thread to abort.
//
// Results:
//   None.
//
// Side effects:
//   Cleans up the thread state by unwinding the thread. Sets the thread state

static void AbortThread(FbleValueArena* arena, Thread* thread)
{
  thread->aborted = true;

  FbleArena* arena_ = FbleRefArenaArena(arena);
  for (size_t i = 0; i < thread->children.size; ++i) {
    AbortThread(arena, thread->children.xs[i]);
    FbleFree(arena_, thread->children.xs[i]);
  }
  thread->children.size = 0;
  FbleFree(arena_, thread->children.xs);
  thread->children.xs = NULL;

  while (thread->stack != NULL) {
    thread->stack = PopFrame(arena, thread->stack);
  }

  if (thread->profile != NULL) {
    FbleFreeProfileThread(arena_, thread->profile);
    thread->profile = NULL;
  }
}

// RunThreads --
//   Run the given thread and its children to completion or until it can no
//   longer make progress.
//
// Inputs:
//   arena - the arena to use for allocations.
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
static bool RunThreads(FbleValueArena* arena, FbleIO* io, FbleProfile* profile, Thread* thread)
{
  // Note: It's possible we smash the stack with this recursive implementation
  // of RunThreads, but the amount of stack space we need for each frame is so
  // small it doesn't seem worth worrying about for the time being.

  bool progress = false;
  for (size_t i = 0; i < thread->children.size; ++i) {
    Thread* child = thread->children.xs[i];
    progress = RunThreads(arena, io, profile, child) || progress;
  }

  // If we have child threads and they made some progress, don't bother
  // running the parent thread, because it's probably blocked on a child
  // thread anyway.
  if (!progress) {
    FbleResumeProfileThread(thread->profile);
    progress = RunThread(arena, io, profile, thread);
    FbleSuspendProfileThread(thread->profile);
  }
  return progress;
}

// Eval --
//   Execute the given sequence of instructions to completion.
//
// Inputs:
//   arena - the arena to use for allocations.
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
static FbleValue* Eval(FbleValueArena* arena, FbleIO* io, FbleValue** statics, FbleInstrBlock* code, FbleProfile* profile)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleValue* final_result = NULL;
  Thread thread = {
    .stack = PushFrame(arena_, NULL, statics, code, &final_result, NULL),
    .children = {0, NULL},
    .aborted = false,
    .profile = FbleNewProfileThread(arena_, profile),
  };

  // Run the main thread repeatedly until it no longer makes any forward
  // progress.
  bool progress = false;
  do {
    progress = RunThreads(arena, io, profile, &thread);
    if (thread.aborted) {
      return NULL;
    }

    bool block = (!progress) && (thread.stack != NULL);
    progress = io->io(io, arena, block) || progress;
  } while (progress);

  if (thread.stack != NULL) {
    // We have instructions to run still, but we stopped making forward
    // progress. This must be a deadlock.
    // TODO: Handle this more gracefully.
    FbleFreeProfileThread(arena_, thread.profile);
    fprintf(stderr, "Deadlock detected\n");
    abort();
    return NULL;
  }

  // The thread should now be finished
  assert(final_result != NULL);
  assert(thread.stack == NULL);
  assert(thread.children.size == 0);
  assert(thread.children.xs == NULL);
  FbleFreeProfileThread(arena_, thread.profile);
  return final_result;
}

// NoIO --
//   An IO function that does no IO.
//   See documentation in fble.h
static bool NoIO(FbleIO* io, FbleValueArena* arena, bool block)
{
  assert(!block && "blocked indefinately on no IO");
  return false;
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleValueArena* arena, FbleProgram* program, FbleNameV* blocks, FbleProfile** profile)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  FbleInstrBlock* code = FbleCompile(arena_, blocks, program);
  *profile = FbleNewProfile(arena_, blocks->size);
  if (code == NULL) {
    return NULL;
  }

  FbleIO io = { .io = &NoIO, .ports = { .size = 0, .xs = NULL} };
  FbleValue* result = Eval(arena, &io, NULL, code, *profile);
  FbleFreeInstrBlock(arena_, code);
  return result;
}

// FbleApply -- see documentation in fble.h
FbleValue* FbleApply(FbleValueArena* arena, FbleValue* func, FbleValueV args, FbleProfile* profile)
{
  assert(args.size > 0);
  assert(func->tag == FBLE_FUNC_VALUE);

  FbleInstr* instrs[1 + args.size];
  instrs[0] = &g_enter_instr._base;

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
  FbleIO io = { .io = &NoIO, .ports = { .size = 0, .xs = NULL} };
  FbleValue* result = Eval(arena, &io, xs, &code, profile);
  return result;
}

// FbleExec -- see documentation in fble.h
FbleValue* FbleExec(FbleValueArena* arena, FbleIO* io, FbleValue* proc, FbleProfile* profile)
{
  assert(proc->tag == FBLE_PROC_VALUE);
  FbleProcValue* p = (FbleProcValue*)proc;
  return Eval(arena, io, p->scope.xs, p->code, profile);
}
