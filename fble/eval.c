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

#define DATA_STACK_CHUNK_SIZE 64

// DataStack --
//   A stack of data values.
//
// Fields:
//   values - a chunk of values at the top of the stack.
//   pos - the index of the next place to put a value in the values array.
//   tail - the bottom chunks of the stack.
//   next - an optional cached, currently unused chunk of the stack whose tail
//          points to this chunk of the stack.
typedef struct DataStack {
  FbleValue* values[DATA_STACK_CHUNK_SIZE];
  size_t pos;
  struct DataStack* tail;
  struct DataStack* next;
} DataStack;

// Frame --
//   An execution frame.
//
// Fields:
//   scope - A value that owns the statics array. May be NULL.
//   statics - Variables captured from the parent scope.
//   locals - Space allocated for local variables.
//      Has length code->locals values. Takes ownership of all non-NULL
//      entries.
//   data - Stack of temporary values.
//   code - The currently executing instruction block.
//   pc - The location of the next instruction in the code to execute.
//   result - Where to store the result when exiting the scope.
typedef struct {
  FbleValue* scope;
  FbleValue** statics;
  FbleValue** locals;
  DataStack* data;
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

static void InitDataStack(FbleArena* arena, Frame* frame);
static void FreeDataStack(FbleArena* arena, Frame* frame);
static bool DataStackIsEmpty(Frame* frame);
static void PushData(FbleArena* arena, FbleValue* value, Frame* frame);
static FbleValue** AllocData(FbleArena* arena, Frame* frame);
static FbleValue* PopData(FbleArena* arena, Frame* frame);
static FbleValue* PopTaggedData(FbleValueArena* arena, FbleValueTag tag, Frame* frame);

static FbleValue* FrameGet(Frame* frame, FbleFrameIndex index);
static FbleValue* FrameTaggedGet(FbleValueTag tag, Frame* frame, FbleFrameIndex index);

static Stack* PushFrame(FbleArena* arena, FbleValue* scope, FbleValue** statics, FbleInstrBlock* code, FbleValue** result, Stack* tail);
static Stack* PopFrame(FbleValueArena* arena, Stack* stack);
static Stack* ReplaceFrame(FbleValueArena* arena, FbleValue* scope, FbleValue** statics, FbleInstrBlock* code, Stack* stack);

static bool RunThread(FbleValueArena* arena, FbleIO* io, FbleProfile* profile, Thread* thread);
static void AbortThread(FbleValueArena* arena, Thread* thread);
static bool RunThreads(FbleValueArena* arena, FbleIO* io, FbleProfile* profile, Thread* thread);
static FbleValue* Eval(FbleValueArena* arena, FbleIO* io, FbleValue** statics, FbleInstrBlock* code, FbleProfile* profile);

static void DumpInstrBlock(FbleInstrBlock* code);

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

// InitDataStack --
//   Initialize an empty data stack for the given scope.
//
// Inputs:
//   arena - the arena to use for allocations.
//   frame - the stack to initialize the data stack for.
//
// Results:
//   none.
//
// Side effects:
//   Initializes the data stack. The stack must be freed with FreeDataStack
//   when no longer in use.
static void InitDataStack(FbleArena* arena, Frame* frame)
{
  frame->data = FbleAlloc(arena, DataStack);
  frame->data->pos = 0;
  frame->data->tail = NULL;
  frame->data->next = NULL;
}

// FreeDataStack --
//   Free resources associated with the data stack for the given scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   frame - the scope whose data stack to free.
//
// Results:
//   none.
//
// Side effects:
//   Frees resources associated with the data stack for the given scope.
static void FreeDataStack(FbleArena* arena, Frame* frame)
{
  assert(frame->data != NULL);
  assert(DataStackIsEmpty(frame));

  DataStack* next = frame->data->next;
  while (next != NULL) {
    DataStack* new_next = next->next;
    FbleFree(arena, next);
    next = new_next;
  }

  DataStack* data = frame->data;
  while (data != NULL) {
    DataStack* tail = data->tail;
    FbleFree(arena, data);
    data = tail;
  }
  frame->data = NULL;
}

// DataStackIsEmpty --
//   Returns true if the data stack for the given scope is empty.
//
// Inputs:
//   frame - the stack to check.
//
// Results:
//   true if the scope's data stack is empty.
//
// Side effects:
//   None.
static bool DataStackIsEmpty(Frame* frame)
{
  return (frame->data->tail == NULL && frame->data->pos == 0);
}

// PushData --
//   Push a value onto the data stack of a scope.
//
// Inputs:
//   arena - the arena to use for allocations
//   value - the value to push
//   frame - the scope whose data stack to push the value on.
//
// Result:
//   None.
//
// Side effects:
//   Pushes a value onto the data stack of the scope that should be freed
//   with PopData when done.
static void PushData(FbleArena* arena, FbleValue* value, Frame* frame)
{
  *AllocData(arena, frame) = value;
}

// AllocData --
//   Allocate an empty slot on the data stack.
//
// Inputs:
//   arena - arena to use for allocations
//   frame - the scope whose data stack to allocate a slot on.
//
// Returns:
//   The address of the newly allocated slot.
//
// Side effects
//   Allocates a slot on the data stack that should be freed with PopData when
//   done.
static FbleValue** AllocData(FbleArena* arena, Frame* frame)
{
  DataStack* data = frame->data;
  assert(data->pos < DATA_STACK_CHUNK_SIZE);
  FbleValue** result = data->values + data->pos++;
  *result = NULL;
  if (data->pos == DATA_STACK_CHUNK_SIZE) {
    if (data->next == NULL) {
      data->next = FbleAlloc(arena, DataStack);
      data->next->pos = 0;
      data->next->tail = data;
      data->next->next = NULL;
    }
    frame->data = data->next;
  }
  return result;
}

// PopData --
//   Pop and retrieve the top value from the data stack for a scope.
//
// Inputs:
//   arena - the arena to use for deallocation.
//   frame - the scope to pop and get the data from.
//
// Results:
//   The popped value.
//
// Side effects:
//   Pops the top data element off the scope's data stack. It is the user's
//   job to release the returned value if necessary.
//   The behavior is undefined if there are no elements on the stack.
static FbleValue* PopData(FbleArena* arena, Frame* frame)
{
  DataStack* data = frame->data;
  if (data->pos == 0) {
    if (data->next != NULL) {
      FbleFree(arena, data->next);
      data->next = NULL;
    }
    frame->data = data->tail;
  }
  return frame->data->values[--frame->data->pos];
}

// PopTaggedData --
//   Pop and retrieve the top value from the data stack for a scope.
//   Dereferences the data value, removing all layers of reference values
//   until a non-reference value is encountered and returns the non-reference
//   value.
//
//   A tag for the type of dereferenced value should be provided. This
//   function will assert that the correct kind of value is encountered.
//
// Inputs:
//   arena - the arena to use for deallocation.
//   tag - the expected tag of the value.
//   frame - the scope to pop and get the data from.
//
// Results:
//   The popped and dereferenced value. Returns null in case of abstract value
//   dereference.
//
// Side effects:
//   Pops the top data element off the scope's data stack. It is the user's
//   job to release the returned value if necessary.
//   The behavior is undefined if there are no elements on the stack.
static FbleValue* PopTaggedData(FbleValueArena* arena, FbleValueTag tag, Frame* frame)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleValue* original = PopData(arena_, frame);
  FbleValue* value = original;
  while (value->tag == FBLE_REF_VALUE) {
    FbleRefValue* rv = (FbleRefValue*)value;

    if (rv->value == NULL) {
      // We are trying to dereference an abstract value. This is undefined
      // behavior according to the spec. Return NULL to indicate to the
      // caller.
      FbleValueRelease(arena, original);
      return NULL;
    }

    value = rv->value;
  }
  assert(value->tag == tag);

  // TODO: Only do retain/release if value != original to improve performance?
  FbleValueRetain(arena, value);
  FbleValueRelease(arena, original);
  return value;
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

  InitDataStack(arena, &stack->frame);
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
//   Behavior is undefined if there are any remaining values on the data
//   stack.
static Stack* PopFrame(FbleValueArena* arena, Stack* stack)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  FreeDataStack(arena_, &stack->frame);
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
  // It's the callers responsibility to ensure the data stack is empty when
  // changing scopes.
  assert(DataStackIsEmpty(&stack->frame));

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

      case FBLE_VAR_INSTR: {
        FbleVarInstr* var_instr = (FbleVarInstr*)instr;
        assert(thread->stack != NULL);
        FbleValue* value = FrameGet(&thread->stack->frame, var_instr->index);
        PushData(arena_, FbleValueRetain(arena, value), &thread->stack->frame);
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
        FbleProcValue* proc = (FbleProcValue*)PopTaggedData(arena, FBLE_PROC_VALUE, &thread->stack->frame);

        // You cannot execute a proc in a let binding, so it should be
        // impossible to ever have an undefined proc value.
        assert(proc != NULL && "undefined proc value");

        if (proc_instr->exit) {
          thread->stack = ReplaceFrame(arena, &proc->_base, proc->scope.xs, proc->code, thread->stack);
          FbleProfileAutoExitBlock(arena_, thread->profile);
        } else {
          FbleValue** result = AllocData(arena_, &thread->stack->frame);
          thread->stack = PushFrame(arena_, &proc->_base, proc->scope.xs, proc->code, result, thread->stack);
        }
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

      case FBLE_VPUSH_INSTR: {
        FbleVPushInstr* vpush_instr = (FbleVPushInstr*)instr;
        thread->stack->frame.locals[vpush_instr->dest] = PopData(arena_, &thread->stack->frame);
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
//   Unwind the given thread, cleaning up thread, variable, and data stacks
//   and children threads as appropriate.
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
//   as aborted.
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
    if (thread->stack->frame.data != NULL) {
      while (!DataStackIsEmpty(&thread->stack->frame)) {
        FbleValueRelease(arena, PopData(arena_, &thread->stack->frame));
      }
    }

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

// DumpInstrBlock -- 
//   For debugging purposes, dump the given code block in human readable
//   format to stderr.
//
// Inputs:
//   code - the code block to dump
//
// Results:
//   none
//
// Side effects:
//   Prints the code block in human readable format to stderr.
static void DumpInstrBlock(FbleInstrBlock* code)
{
  // Map from FbleFrameSection to short descriptor of the section.
  static const char* sections[] = {"s", "l"};

  FbleArena* arena = FbleNewArena();
  struct { size_t size; FbleInstrBlock** xs; } blocks;
  FbleVectorInit(arena, blocks);
  FbleVectorAppend(arena, blocks, code);

  while (blocks.size > 0) {
    FbleInstrBlock* block = blocks.xs[--blocks.size];
    fprintf(stderr, "%p statics[%zi] locals[%zi]:\n",
        (void*)block, block->statics, block->locals);
    for (size_t i = 0; i < block->instrs.size; ++i) {
      FbleInstr* instr = block->instrs.xs[i];
      fprintf(stderr, "%4zi.  ", i);
      switch (instr->tag) {
        case FBLE_STRUCT_VALUE_INSTR: {
          FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
          fprintf(stderr, "l[%zi] = struct(", struct_value_instr->dest);
          const char* comma = "";
          for (size_t j = 0; j < struct_value_instr->args.size; ++j) {
            FbleFrameIndex arg = struct_value_instr->args.xs[j];
            fprintf(stderr, "%s%s[%zi]", comma, sections[arg.section], arg.index);
            comma = ", ";
          }
          fprintf(stderr, ");\n");
          break;
        }

        case FBLE_UNION_VALUE_INSTR: {
          FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
          fprintf(stderr, "l[%zi] = union(%s[%zi]);\n",
              union_value_instr->dest,
              sections[union_value_instr->arg.section],
              union_value_instr->arg.index);
          break;
        }

        case FBLE_STRUCT_ACCESS_INSTR:
        case FBLE_UNION_ACCESS_INSTR: {
          FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
          fprintf(stderr, "l[%zi] = %s[%zi].%zi; // %s:%i:%i\n",
              access_instr->dest, sections[access_instr->obj.section],
              access_instr->obj.index,
              access_instr->tag, access_instr->loc.source,
              access_instr->loc.line, access_instr->loc.col);
          break;
        }

        case FBLE_UNION_SELECT_INSTR: {
          FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
          fprintf(stderr, "pc += %s[%zi]?;         // %s:%i:%i\n",
              sections[select_instr->condition.section],
              select_instr->condition.index,
              select_instr->loc.source, select_instr->loc.line,
              select_instr->loc.col);
          break;
        }

        case FBLE_GOTO_INSTR: {
          FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
          fprintf(stderr, "pc = %zi;", goto_instr->pc);
          break;
        }

        case FBLE_FUNC_VALUE_INSTR: {
          FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
          fprintf(stderr, "l[%zi] = func %p [",
              func_value_instr->dest,
              (void*)func_value_instr->code);
          const char* comma = "";
          for (size_t j = 0; j < func_value_instr->scope.size; ++j) {
            fprintf(stderr, "%s%s[%zi]",
                comma, sections[func_value_instr->scope.xs[j].section],
                func_value_instr->scope.xs[j].index);
            comma = ", ";
          }
          fprintf(stderr, "] %zi;\n", func_value_instr->argc);
          FbleVectorAppend(arena, blocks, func_value_instr->code);
          break;
        }

        case FBLE_RELEASE_INSTR: {
          FbleReleaseInstr* release = (FbleReleaseInstr*)instr;
          fprintf(stderr, "release l[%zi];\n", release->value);
          break;
        }

        case FBLE_FUNC_APPLY_INSTR: {
          FbleFuncApplyInstr* func_apply_instr = (FbleFuncApplyInstr*)instr;
          fprintf(stderr, "l[%zi] = %s[%zi](%s[%zi]); // (exit=%s) %s:%i:%i\n",
              func_apply_instr->dest,
              sections[func_apply_instr->func.section],
              func_apply_instr->func.index,
              sections[func_apply_instr->arg.section],
              func_apply_instr->arg.section,
              func_apply_instr->exit ? "true" : "false",
              func_apply_instr->loc.source, func_apply_instr->loc.line,
              func_apply_instr->loc.col);
          break;
        }

        case FBLE_PROC_VALUE_INSTR: {
          FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
          fprintf(stderr, "l[%zi] = proc %p [",
              proc_value_instr->dest,
              (void*)proc_value_instr->code);
          const char* comma = "";
          for (size_t j = 0; j < proc_value_instr->scope.size; ++j) {
            fprintf(stderr, "%s%s[%zi]",
                comma, sections[proc_value_instr->scope.xs[j].section],
                proc_value_instr->scope.xs[j].index);
            comma = ", ";
          }
          fprintf(stderr, "];\n");
          FbleVectorAppend(arena, blocks, proc_value_instr->code);
          break;
        }

        case FBLE_VAR_INSTR: {
          FbleVarInstr* var_instr = (FbleVarInstr*)instr;
          fprintf(stderr, "$ = %s[%zi];\n",
              sections[var_instr->index.section], var_instr->index.index);
          break;
        }

        case FBLE_COPY_INSTR: {
          FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
          fprintf(stderr, "l[%zi] = %s[%zi];\n",
              copy_instr->dest,
              sections[copy_instr->source.section],
              copy_instr->source.index);
          break;
        }

        case FBLE_GET_INSTR: {
          fprintf(stderr, "return get(s[0]);\n");
          break;
        }

        case FBLE_PUT_INSTR: {
          fprintf(stderr, "return put(s[0], s[1]);\n");
          break;
        }

        case FBLE_LINK_INSTR: {
          FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
          fprintf(stderr, "l[%zi], l[%zi] = link;\n", link_instr->get, link_instr->put);
          break;
        }

        case FBLE_FORK_INSTR: {
          FbleForkInstr* fork_instr = (FbleForkInstr*)instr;
          fprintf(stderr, "fork [");
          const char* comma = "";
          for (size_t j = 0; j < fork_instr->dests.size; ++j) {
            fprintf(stderr, "%s%zi = %s[%zi]",
                comma, fork_instr->dests.xs[j],
                sections[fork_instr->args.xs[j].section],
                fork_instr->args.xs[j].index);
            comma = ", ";
          }
          fprintf(stderr, "];\n");
          break;
        }

        case FBLE_JOIN_INSTR: {
          fprintf(stderr, "join;\n");
          break;
        }

        case FBLE_PROC_INSTR: {
          FbleProcInstr* proc_instr = (FbleProcInstr*)instr;
          fprintf(stderr, "proc (exit=%s);\n", proc_instr->exit ? "true" : "false");
          break;
        }

        case FBLE_REF_VALUE_INSTR: {
          FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
          fprintf(stderr, "l[%zi] = ref;\n", ref_instr->dest);
          break;
        }

        case FBLE_REF_DEF_INSTR: {
          FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
          fprintf(stderr, "l[%zi] ~= %s[%zi];\n",
              ref_def_instr->ref,
              sections[ref_def_instr->value.section],
              ref_def_instr->value.index);
          break;
        }

        case FBLE_STRUCT_IMPORT_INSTR: {
          FbleStructImportInstr* import_instr = (FbleStructImportInstr*)instr;
          fprintf(stderr, "%s[%zi].import(",
              sections[import_instr->obj.section],
              import_instr->obj.index);
          const char* comma = "";
          for (size_t j = 0; j < import_instr->fields.size; ++j) {
            fprintf(stderr, "%s[%zi]", comma, import_instr->fields.xs[j]);
            comma = ", ";
          }
          fprintf(stderr, ");    // %s:%i:%i\n",
              import_instr->loc.source, import_instr->loc.line,
              import_instr->loc.col);
          break;
        }

        case FBLE_RETURN_INSTR: {
          FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
          fprintf(stderr, "return %s[%zi];\n",
              sections[return_instr->result.section],
              return_instr->result.index);
          break;
        }

        case FBLE_TYPE_INSTR: {
          FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
          fprintf(stderr, "l[%zi] = type;\n", type_instr->dest);
          break;
        }

        case FBLE_VPUSH_INSTR: {
          FbleVPushInstr* vpush_instr = (FbleVPushInstr*)instr;
          fprintf(stderr, "l[%zi] = $;\n", vpush_instr->dest);
          break;
        }

        case FBLE_PROFILE_ENTER_BLOCK_INSTR: {
          FbleProfileEnterBlockInstr* enter = (FbleProfileEnterBlockInstr*)instr;
          fprintf(stderr, "enter block %zi for %zi;\n", enter->block, enter->time);
          break;
        }

        case FBLE_PROFILE_EXIT_BLOCK_INSTR: {
          fprintf(stderr, "exit block;\n");
          break;
        }

        case FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR: {
          fprintf(stderr, "auto exit block;\n");
          break;
        }
      }
    }
    fprintf(stderr, "\n\n");
  }

  FbleFree(arena, blocks.xs);
  FbleAssertEmptyArena(arena);
  FbleDeleteArena(arena);
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
FbleValue* FbleEval(FbleValueArena* arena, FbleProgram* program, FbleNameV* blocks, FbleProfile** profile, bool dump_compiled)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  FbleInstrBlock* code = FbleCompile(arena_, blocks, program);
  *profile = FbleNewProfile(arena_, blocks->size);
  if (code == NULL) {
    return NULL;
  }
  if (dump_compiled) {
    DumpInstrBlock(code);
  }

  FbleIO io = { .io = &NoIO, .ports = { .size = 0, .xs = NULL} };
  FbleValue* result = Eval(arena, &io, NULL, code, *profile);
  FbleFreeInstrBlock(arena_, code);
  return result;
}

// FbleApply -- see documentation in fble.h
FbleValue* FbleApply(FbleValueArena* arena, FbleValue* func, FbleValue* arg, FbleProfile* profile)
{
  assert(func->tag == FBLE_FUNC_VALUE);

  FbleFuncApplyInstr apply = {
    ._base = { .tag = FBLE_FUNC_APPLY_INSTR },
    .loc = { .source = "(internal)", .line = 0, .col = 0 },
    .exit = true,
    .dest = 0,      // arbitrary, because exit = true.
    .func = { .section = FBLE_STATICS_FRAME_SECTION, .index = 0 },
    .arg = { .section = FBLE_STATICS_FRAME_SECTION, .index = 1 },
  };
  FbleInstr* instrs[] = { &g_enter_instr._base, &apply._base };
  FbleInstrBlock code = { .refcount = 2, .statics = 2, .locals = 0, .instrs = { .size = 2, .xs = instrs } };
  FbleIO io = { .io = &NoIO, .ports = { .size = 0, .xs = NULL} };

  FbleValue* xs[] = { func, arg };
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
