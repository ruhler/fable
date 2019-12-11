// eval.c --
//   This file implements the fble evaluation routines.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for NULL, abort

#include "internal.h"

#define UNREACHABLE(x) assert(false && x)

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

// ScopeStack --
//   A stack of instruction blocks and their variable scopes.
//
// Fields:
//   vars - Values of variables in scope.
//   instrs - The currently executing instruction block.
//   pc - The location of the next instruction in the current block to execute.
typedef struct ScopeStack {
  FbleValueV vars;
  FbleInstrBlock* block;
  size_t pc;
  struct ScopeStack* tail;
} ScopeStack;

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
//   data_stack - used to store anonymous intermediate values.
//   scope_stack - stores variable scopes.
//   iquota - the number of instructions this thread is allowed to execute.
//   children - child threads that this thread is potentially blocked on.
//      This vector is {0, NULL} when there are no child threads.
//   aborted - true if the thread has been aborted due to undefined union
//             field access.
//   profile - the profile thread associated with this thread.
struct Thread {
  DataStack* data_stack;
  ScopeStack* scope_stack;
  size_t iquota;
  ThreadV children;
  bool aborted;
  FbleProfileThread* profile;
};

static FbleProcInstr g_proc_instr = {
  ._base = { .tag = FBLE_PROC_INSTR },
  .loc = { .source = "(internal)", .line = 0, .col = 0}
};
static FbleProfileEnterBlockInstr g_enter_instr = {
  ._base = { .tag = FBLE_PROFILE_ENTER_BLOCK_INSTR },
  .block = 0,
  .time = 1,
};
static FbleInstr* g_proc_block_instrs[] = { &g_enter_instr._base, &g_proc_instr._base };
static FbleInstrBlock g_proc_block = {
  .refcount = 1,
  .instrs = { .size = 2, .xs = g_proc_block_instrs }
};

static void Add(FbleRefArena* arena, FbleValue* src, FbleValue* dst);

static void PushVar(FbleArena* arena, FbleValue* value, ScopeStack* scopes);
static void PopVar(FbleArena* arena, ScopeStack* scopes);
static FbleValue* GetVar(ScopeStack* scopes, size_t position);
static void SetVar(ScopeStack* scopes, size_t position, FbleValue* value);

static void InitDataStack(FbleArena* arena, Thread* thread);
static void FreeDataStack(FbleArena* arena, Thread* thread);
static bool DataStackIsEmpty(Thread* thread);
static void PushData(FbleArena* arena, FbleValue* value, Thread* thread);
static FbleValue* PopData(FbleArena* arena, Thread* thread);
static FbleValue* PopTaggedData(FbleValueArena* arena, FbleValueTag tag, Thread* thread);

static ScopeStack* EnterScope(FbleArena* arena, FbleInstrBlock* block, ScopeStack* tail);
static ScopeStack* ExitScope(FbleValueArena* arena, ScopeStack* stack);
static ScopeStack* ChangeScope(FbleValueArena* arena, FbleInstrBlock* block, ScopeStack* stack);

static void CaptureScope(FbleValueArena* arena, Thread* thread, size_t scopec, FbleValue* value, FbleValueV* dst);
static void RestoreScope(FbleValueArena* arena, FbleValueV scope, Thread* thread);

static void RunThread(FbleValueArena* arena, FbleIO* io, FbleCallGraph* graph, Thread* thread);
static void AbortThread(FbleValueArena* arena, Thread* thread);
static void RunThreads(FbleValueArena* arena, FbleIO* io, FbleCallGraph* graph, Thread* thread);
static FbleValue* Eval(FbleValueArena* arena, FbleIO* io, FbleInstrBlock* prgm, FbleValueV args, FbleCallGraph* graph);

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

// PushVar --
//   Push a value onto the top scope of the given scope stack.
//
// Inputs:
//   arena - the arena to use for allocations
//   value - the value to push
//   scope_stack - the stack of scopes to push to the value onto.
//
// Result:
//   None.
//
// Side effects:
//   Pushes the value onto the top scope of the given scope stack.
static void PushVar(FbleArena* arena, FbleValue* value, ScopeStack* scope_stack)
{
  assert(scope_stack != NULL);
  FbleVectorAppend(arena, scope_stack->vars, value);
}

// PopVar --
//   Pop a value off the top scope of the given scope stack.
//
// Inputs:
//   arena - the arena to use for deallocation
//   scope_stack - the stack of scopes to pop the variable from
//
// Results:
//   none.
//
// Side effects:
//   Pops the variable off the top scope of the given stack. It is the users
//   job to release the value if necessary before popping the variable.
static void PopVar(FbleArena* arena, ScopeStack* scope_stack)
{
  assert(scope_stack != NULL);
  assert(scope_stack->vars.size > 0);
  scope_stack->vars.size--;
}

// GetVar --
//   Gets the variable at the given position from the top of the top scope.
//
// Inputs:
//   scopes - the stack of scopes to get the var from.
//   position - the position of the variable in the top scope of the stack.
//              Position 0 is the last variable in the scope, 1 is second to
//              last, and so on.
//
// Results:
//   The variable at the given position of the top scope from the scopes
//   stack.
//
// Side effects:
//   None.
static FbleValue* GetVar(ScopeStack* scopes, size_t position)
{
  assert(scopes != NULL);
  assert(position < scopes->vars.size);
  return scopes->vars.xs[scopes->vars.size - 1 - position];
}

// SetVar --
//   Sets the variable at the given position from the top of the top scope.
//
// Inputs:
//   scopes - the stack of scopes to get the var from.
//   position - the position of the variable in the top scope of the stack.
//              Position 0 is the last variable in the scope, 1 is second to
//              last, and so on.
//   value - the value to set the variable to.
//
// Results:
//   None.
//
// Side effects:
//   Sets the variable at the given position of the top scope from the scopes
//   stack.
static void SetVar(ScopeStack* scopes, size_t position, FbleValue* value)
{
  assert(scopes != NULL);
  assert(position < scopes->vars.size);
  scopes->vars.xs[scopes->vars.size - 1 - position] = value;
}

// InitDataStack --
//   Initialize an empty data stack for the given thread.
//
// Inputs:
//   arena - the arena to use for allocations.
//   thread - the thread to initialize the data stack for.
//
// Results:
//   none.
//
// Side effects:
//   Initializes the data stack. The stack must be freed with FreeDataStack
//   when no longer in use.
static void InitDataStack(FbleArena* arena, Thread* thread)
{
  thread->data_stack = FbleAlloc(arena, DataStack);
  thread->data_stack->pos = 0;
  thread->data_stack->tail = NULL;
  thread->data_stack->next = NULL;
}

// FreeDataStack --
//   Free resources associated with the data stack for the given thread.
//
// Inputs:
//   arena - arena to use for allocations.
//   thread - the thread whose data stack to free.
//
// Results:
//   none.
//
// Side effects:
//   Frees resources associated with the data stack for the given thread.
static void FreeDataStack(FbleArena* arena, Thread* thread)
{
  assert(thread->data_stack != NULL);
  assert(DataStackIsEmpty(thread));

  DataStack* next = thread->data_stack->next;
  while (next != NULL) {
    DataStack* new_next = next->next;
    FbleFree(arena, next);
    next = new_next;
  }

  DataStack* stack = thread->data_stack;
  while (stack != NULL) {
    DataStack* tail = stack->tail;
    FbleFree(arena, stack);
    stack = tail;
  }
  thread->data_stack = NULL;
}

// DataStackIsEmpty --
//   Returns true if the data stack for the given thread is empty.
//
// Inputs:
//   thread - the thread to check.
//
// Results:
//   true if the thread's data stack is empty.
//
// Side effects:
//   None.
static bool DataStackIsEmpty(Thread* thread)
{
  return (thread->data_stack->tail == NULL && thread->data_stack->pos == 0);
}

// PushData --
//   Push a value onto the data stack of a thread.
//
// Inputs:
//   arena - the arena to use for allocations
//   value - the value to push
//   thread - the thread whose data stack to push the value on.
//
// Result:
//   None.
//
// Side effects:
//   Pushes a value onto the data stack of the thread that should be freed
//   with PopData when done.
static void PushData(FbleArena* arena, FbleValue* value, Thread* thread)
{
  DataStack* stack = thread->data_stack;
  assert(stack->pos < DATA_STACK_CHUNK_SIZE);
  stack->values[stack->pos++] = value;
  if (stack->pos == DATA_STACK_CHUNK_SIZE) {
    if (stack->next == NULL) {
      stack->next = FbleAlloc(arena, DataStack);
      stack->next->pos = 0;
      stack->next->tail = stack;
      stack->next->next = NULL;
    }
    thread->data_stack = stack->next;
  }
}

// PopData --
//   Pop and retrieve the top value from the data stack for a thread.
//
// Inputs:
//   arena - the arena to use for deallocation.
//   thread - the thread to pop and get the data from.
//
// Results:
//   The popped value.
//
// Side effects:
//   Pops the top data element off the thread's data stack. It is the user's
//   job to release the returned value if necessary.
//   The behavior is undefined if there are no elements on the stack.
static FbleValue* PopData(FbleArena* arena, Thread* thread)
{
  DataStack* stack = thread->data_stack;
  if (stack->pos == 0) {
    if (stack->next != NULL) {
      FbleFree(arena, stack->next);
      stack->next = NULL;
    }
    thread->data_stack = stack->tail;
  }
  return thread->data_stack->values[--thread->data_stack->pos];
}

// PopTaggedData --
//   Pop and retrieve the top value from the data stack for a thread.
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
//   thread - the thread to pop and get the data from.
//
// Results:
//   The popped and dereferenced value. Returns null in case of abstract value
//   dereference.
//
// Side effects:
//   Pops the top data element off the thread's data stack. It is the user's
//   job to release the returned value if necessary.
//   The behavior is undefined if there are no elements on the stack.
static FbleValue* PopTaggedData(FbleValueArena* arena, FbleValueTag tag, Thread* thread)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleValue* original = PopData(arena_, thread);
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

// EnterScope --
//   Push a scope onto the scope stack.
//
// Inputs:
//   arena - the arena to use for allocations
//   block - the block of instructions to push
//   tail - the stack to push to
//
// Result:
//   The stack with new pushed scope.
//
// Side effects:
//   Allocates new ScopeStack instances that should be freed with ExitScope when done.
static ScopeStack* EnterScope(FbleArena* arena, FbleInstrBlock* block, ScopeStack* tail)
{
  block->refcount++;

  ScopeStack* stack = FbleAlloc(arena, ScopeStack);
  FbleVectorInit(arena, stack->vars);
  stack->block = block;
  stack->pc = 0;
  stack->tail = tail;
  return stack;
}

// ExitScope --
//   Pop a scope off the scope stack.
//
// Inputs:
//   arena - the arena to use for deallocation
//   stack - the stack to pop from
//
// Results:
//   The popped stack.
//
// Side effects:
//   Releases any remaining variables on the top scope and frees the top scope.
static ScopeStack* ExitScope(FbleValueArena* arena, ScopeStack* stack)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  for (size_t i = 0; i < stack->vars.size; ++i) {
    FbleValueRelease(arena, stack->vars.xs[i]);
  }

  FbleFree(arena_, stack->vars.xs);
  FbleFreeInstrBlock(arena_, stack->block);

  ScopeStack* tail = stack->tail;
  FbleFree(arena_, stack);
  return tail;
}

// ChangeScope --
//   Exit the current scope and enter a new one.
//
// Inputs:
//   arena - the arena to use for allocations
//   block - the block of instructions for the new scope.
//   tail - the stack to change.
//
// Result:
//   The stack with new scope.
//
// Side effects:
//   Allocates new ScopeStack instances that should be freed with ExitScope when done.
//   Exits the current scope, which potentially frees any instructions
//   belonging to that scope.
static ScopeStack* ChangeScope(FbleValueArena* arena, FbleInstrBlock* block, ScopeStack* stack)
{
  block->refcount++;

  FbleArena* arena_ = FbleRefArenaArena(arena);
  for (size_t i = 0; i < stack->vars.size; ++i) {
    FbleValueRelease(arena, stack->vars.xs[i]);
  }
  stack->vars.size = 0;

  FbleFreeInstrBlock(arena_, stack->block);
  stack->block = block;
  stack->pc = 0;
  return stack;
}

// CaptureScope --
//   Capture the variables from the top of the thread's data stack and save it
//   in a value.
//
// Inputs:
//   arena - arena to use for allocations.
//   thread - the thread to capture the scope for.
//   scopec - the number of variables to save.
//   value - the value to save the scope to.
//   dst - a pointer to where the scope should be saved to. This is assumed to
//         be a field of the value.
//
// Results:
//   None.
//
// Side effects:
//   Pops scopec values from the thread's data stack.
//   Stores a copy of the variables from the data stack in dst, and adds
//   references from the value to everything on the scope.
static void CaptureScope(FbleValueArena* arena, Thread* thread, size_t scopec, FbleValue* value, FbleValueV* dst)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  for (size_t i = 0; i < scopec; ++i) {
    FbleValue* var = PopData(arena_, thread);
    FbleVectorAppend(arena_, *dst, var);
    Add(arena, value, var);
    FbleValueRelease(arena, var);
  }
}

// RestoreScope --
//   Copy the given values to the thread's data stack.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to copy.
//   thread - the thread whose data stack to copy to.
//
// Result:
//   None.
//
// Side effects:
//   Pushes all values from the scope onto the thread's data stack.
static void RestoreScope(FbleValueArena* arena, FbleValueV scope, Thread* thread)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  for (size_t i = 0; i < scope.size; ++i) {
    PushData(arena_, FbleValueRetain(arena, scope.xs[i]), thread);
  }
}

// RunThread --
//   Run the given thread to completion or until it can no longer make
//   progress.
//
// Inputs:
//   arena - the arena to use for allocations.
//   io - io to use for external ports.
//   graph - the call graph to save execution stats to.
//   thread - the thread to run.
//
// Results:
//   None.
//
// Side effects:
//   The thread is executed, updating its data_stack, and scope_stack.
static void RunThread(FbleValueArena* arena, FbleIO* io, FbleCallGraph* graph, Thread* thread)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  assert(!thread->aborted);
  while (thread->iquota > 0 && thread->scope_stack != NULL) {
    assert(thread->scope_stack->pc < thread->scope_stack->block->instrs.size);
    FbleInstr* instr = thread->scope_stack->block->instrs.xs[thread->scope_stack->pc++];
    switch (instr->tag) {
      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_value_instr->argc;

        FbleValueRelease(arena, PopData(arena_, thread));

        FbleValue* argv[argc];
        for (size_t i = 0; i < argc; ++i) {
          argv[i] = PopData(arena_, thread);
        }

        FbleValueV args = { .size = argc, .xs = argv, };
        PushData(arena_, FbleNewStructValue(arena, args), thread);
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        FbleValue* arg = PopData(arena_, thread);
        PushData(arena_, FbleNewUnionValue(arena, union_value_instr->tag, arg), thread);
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        FbleStructValue* sv = (FbleStructValue*)PopTaggedData(arena, FBLE_STRUCT_VALUE, thread);
        if (sv == NULL) {
          FbleReportError("undefined struct value access\n", &access_instr->loc);
          AbortThread(arena, thread);
          return;
        }
        assert(access_instr->tag < sv->fields.size);
        PushData(arena_, FbleValueRetain(arena, sv->fields.xs[access_instr->tag]), thread);
        FbleValueRelease(arena, &sv->_base);
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        FbleUnionValue* uv = (FbleUnionValue*)PopTaggedData(arena, FBLE_UNION_VALUE, thread);
        if (uv == NULL) {
          FbleReportError("undefined union value access\n", &access_instr->loc);
          AbortThread(arena, thread);
          return;
        }

        if (uv->tag != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);
          FbleValueRelease(arena, &uv->_base);
          AbortThread(arena, thread);
          return;
        }

        PushData(arena_, FbleValueRetain(arena, uv->arg), thread);
        FbleValueRelease(arena, &uv->_base);
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
        FbleUnionValue* uv = (FbleUnionValue*)PopTaggedData(arena, FBLE_UNION_VALUE, thread);
        if (uv == NULL) {
          FbleReportError("undefined union value select\n", &select_instr->loc);
          AbortThread(arena, thread);
          return;
        }
        thread->scope_stack->pc += uv->tag;
        FbleValueRelease(arena, &uv->_base);
        break;
      }

      case FBLE_GOTO_INSTR: {
        FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
        thread->scope_stack->pc = goto_instr->pc;
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        FbleBasicFuncValue* value = FbleAlloc(arena_, FbleBasicFuncValue);
        FbleRefInit(arena, &value->_base._base.ref);
        value->_base._base.tag = FBLE_FUNC_VALUE;
        value->_base.tag = FBLE_BASIC_FUNC_VALUE;
        value->_base.argc = func_value_instr->argc;
        FbleVectorInit(arena_, value->scope);
        value->body = func_value_instr->body;
        value->body->refcount++;
        CaptureScope(arena, thread, func_value_instr->scopec, &value->_base._base, &value->scope);
        PushData(arena_, &value->_base._base, thread);
        break;
      }

      case FBLE_DESCOPE_INSTR: {
        FbleDescopeInstr* descope_instr = (FbleDescopeInstr*)instr;
        assert(descope_instr->count <= thread->scope_stack->vars.size); 
        for (size_t i = 0; i < descope_instr->count; ++i) {
          FbleValueRelease(arena, GetVar(thread->scope_stack, 0));
          PopVar(arena_, thread->scope_stack);
        }
        break;
      }

      case FBLE_FUNC_APPLY_INSTR: {
        FbleFuncApplyInstr* func_apply_instr = (FbleFuncApplyInstr*)instr;
        FbleFuncValue* func = (FbleFuncValue*)PopTaggedData(arena, FBLE_FUNC_VALUE, thread);
        if (func == NULL) {
          FbleReportError("undefined function value apply\n", &func_apply_instr->loc);
          AbortThread(arena, thread);
          return;
        };

        if (func->argc > 1) {
          FbleThunkFuncValue* value = FbleAlloc(arena_, FbleThunkFuncValue);
          FbleRefInit(arena, &value->_base._base.ref);
          value->_base._base.tag = FBLE_FUNC_VALUE;
          value->_base.tag = FBLE_THUNK_FUNC_VALUE;
          value->_base.argc = func->argc - 1;
          value->func = NULL;
          value->arg = NULL;

          value->func = func;
          Add(arena, &value->_base._base, &value->func->_base);
          value->arg = PopData(arena_, thread);
          Add(arena, &value->_base._base, value->arg);
          FbleValueRelease(arena, value->arg);
          PushData(arena_, &value->_base._base, thread);

          if (func_apply_instr->exit) {
            thread->scope_stack = ExitScope(arena, thread->scope_stack);
            FbleProfileExitBlock(arena_, thread->profile);
          }
        } else if (func->tag == FBLE_PUT_FUNC_VALUE) {
          FblePutFuncValue* f = (FblePutFuncValue*)func;

          FblePutProcValue* value = FbleAlloc(arena_, FblePutProcValue);
          FbleRefInit(arena, &value->_base._base.ref);
          value->_base._base.tag = FBLE_PROC_VALUE;
          value->_base.tag = FBLE_PUT_PROC_VALUE;

          value->port = f->port;
          Add(arena, &value->_base._base, value->port);

          value->arg = PopData(arena_, thread);
          Add(arena, &value->_base._base, value->arg);
          FbleValueRelease(arena, value->arg);

          PushData(arena_, &value->_base._base, thread);
          if (func_apply_instr->exit) {
            thread->scope_stack = ExitScope(arena, thread->scope_stack);
            FbleProfileExitBlock(arena_, thread->profile);
          }
        } else {
          FbleFuncValue* f = func;
          while (f->tag == FBLE_THUNK_FUNC_VALUE) {
            FbleThunkFuncValue* thunk = (FbleThunkFuncValue*)f;
            PushData(arena_, FbleValueRetain(arena, thunk->arg), thread);
            f = thunk->func;
          }

          assert(f->tag == FBLE_BASIC_FUNC_VALUE);
          FbleBasicFuncValue* basic = (FbleBasicFuncValue*)f;
          RestoreScope(arena, basic->scope, thread);
          if (func_apply_instr->exit) {
            thread->scope_stack = ChangeScope(arena, basic->body, thread->scope_stack);
            FbleProfileAutoExitBlock(arena_, thread->profile);
          } else {
            thread->scope_stack = EnterScope(arena_, basic->body, thread->scope_stack);
          }
        }
        FbleValueRelease(arena, &func->_base);
        break;
      }

      case FBLE_EVAL_INSTR: {
        FbleEvalProcValue* proc_value = FbleAlloc(arena_, FbleEvalProcValue);
        FbleRefInit(arena, &proc_value->_base._base.ref);
        proc_value->_base._base.tag = FBLE_PROC_VALUE;
        proc_value->_base.tag = FBLE_EVAL_PROC_VALUE;
        proc_value->result = PopData(arena_, thread);
        PushData(arena_, &proc_value->_base._base, thread);
        break;
      }

      case FBLE_VAR_INSTR: {
        FbleVarInstr* var_instr = (FbleVarInstr*)instr;
        assert(thread->scope_stack != NULL);
        assert(var_instr->position < thread->scope_stack->vars.size);
        FbleValue* value = thread->scope_stack->vars.xs[var_instr->position];
        PushData(arena_, FbleValueRetain(arena, value), thread);
        break;
      }

      case FBLE_LINK_INSTR: {
        FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
        FbleLinkProcValue* value = FbleAlloc(arena_, FbleLinkProcValue);
        FbleRefInit(arena, &value->_base._base.ref);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base.tag = FBLE_LINK_PROC_VALUE;
        FbleVectorInit(arena_, value->scope);
        value->body = link_instr->body;
        value->body->refcount++;
        CaptureScope(arena, thread, link_instr->scopec, &value->_base._base, &value->scope);
        PushData(arena_, &value->_base._base, thread);
        break;
      }

      case FBLE_EXEC_INSTR: {
        FbleExecInstr* exec_instr = (FbleExecInstr*)instr;
        FbleExecProcValue* value = FbleAlloc(arena_, FbleExecProcValue);
        FbleRefInit(arena, &value->_base._base.ref);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base.tag = FBLE_EXEC_PROC_VALUE;
        value->bindings.size = exec_instr->argc;
        value->bindings.xs = FbleArrayAlloc(arena_, FbleValue*, value->bindings.size);
        FbleVectorInit(arena_, value->scope);
        value->body = exec_instr->body;
        value->body->refcount++;
        CaptureScope(arena, thread, exec_instr->scopec, &value->_base._base, &value->scope);

        for (size_t i = 0; i < exec_instr->argc; ++i) {
          size_t j = exec_instr->argc - 1 - i;
          FbleValue* v = PopData(arena_, thread);
          value->bindings.xs[j] = v;
          Add(arena, &value->_base._base, v);
          FbleValueRelease(arena, v);
        }

        PushData(arena_, &value->_base._base, thread);
        break;
      }

      case FBLE_JOIN_INSTR: {
        assert(thread->children.size > 0);
        for (size_t i = 0; i < thread->children.size; ++i) {
          if (thread->children.xs[i]->aborted) {
            AbortThread(arena, thread);
            return;
          }

          if (thread->children.xs[i]->scope_stack != NULL) {
            // Blocked on child. Restore the thread state and return before
            // iquota has been decremented.
            thread->scope_stack->pc--;
            return;
          }
        }
        
        for (size_t i = 0; i < thread->children.size; ++i) {
          Thread* child = thread->children.xs[i];
          FbleValue* result = PopData(arena_, child);
          FreeDataStack(arena_, child);
          assert(child->scope_stack == NULL);
          assert(child->iquota == 0);
          FbleFreeProfileThread(arena_, child->profile);
          FbleFree(arena_, child);

          PushVar(arena_, result, thread->scope_stack);
        }

        thread->children.size = 0;
        FbleFree(arena_, thread->children.xs);
        thread->children.xs = NULL;
        break;
      }

      case FBLE_PROC_INSTR: {
        FbleProcInstr* proc_instr = (FbleProcInstr*)instr;
        FbleProcValue* proc = (FbleProcValue*)PopTaggedData(arena, FBLE_PROC_VALUE, thread);
        if (proc == NULL) {
          FbleReportError("undefined proc value\n", &proc_instr->loc);
          AbortThread(arena, thread);
          return;
        }
        switch (proc->tag) {
          case FBLE_GET_PROC_VALUE: {
            FbleGetProcValue* get = (FbleGetProcValue*)proc;

            if (get->port->tag == FBLE_LINK_VALUE) {
              FbleLinkValue* link = (FbleLinkValue*)get->port;

              if (link->head == NULL) {
                // Blocked on get. Restore the thread state and return before
                // iquota has been decremented.
                PushData(arena_, &proc->_base, thread);
                thread->scope_stack->pc--;
                return;
              }

              FbleValues* head = link->head;
              link->head = link->head->next;
              if (link->head == NULL) {
                link->tail = NULL;
              }
              PushData(arena_, head->value, thread);
              FbleFree(arena_, head);

              thread->scope_stack = ExitScope(arena, thread->scope_stack);
              FbleProfileExitBlock(arena_, thread->profile);
              break;
            }

            if (get->port->tag == FBLE_PORT_VALUE) {
              FblePortValue* port = (FblePortValue*)get->port;
              assert(port->id < io->ports.size);
              if (io->ports.xs[port->id] == NULL) {
                // Blocked on get. Restore the thread state and return before
                // iquota has been decremented.
                PushData(arena_, &proc->_base, thread);
                thread->scope_stack->pc--;
                return;
              }

              PushData(arena_, io->ports.xs[port->id], thread);
              io->ports.xs[port->id] = NULL;

              thread->scope_stack = ExitScope(arena, thread->scope_stack);
              FbleProfileExitBlock(arena_, thread->profile);
              break;
            }

            UNREACHABLE("get port must be an input or port value");
            break;
          }

          case FBLE_PUT_PROC_VALUE: {
            FblePutProcValue* put = (FblePutProcValue*)proc;

            FbleValueV args = { .size = 0, .xs = NULL, };
            FbleValue* unit = FbleNewStructValue(arena, args);

            if (put->port->tag == FBLE_LINK_VALUE) {
              FbleLinkValue* link = (FbleLinkValue*)put->port;

              FbleValues* tail = FbleAlloc(arena_, FbleValues);
              tail->value = FbleValueRetain(arena, put->arg);
              tail->next = NULL;

              if (link->head == NULL) {
                link->head = tail;
                link->tail = tail;
              } else {
                assert(link->tail != NULL);
                link->tail->next = tail;
                link->tail = tail;
              }

              PushData(arena_, unit, thread);
              thread->scope_stack = ExitScope(arena, thread->scope_stack);
              FbleProfileExitBlock(arena_, thread->profile);
              break;
            }

            if (put->port->tag == FBLE_PORT_VALUE) {
              FblePortValue* port = (FblePortValue*)put->port;
              assert(port->id < io->ports.size);

              if (io->ports.xs[port->id] != NULL) {
                // Blocked on put. Restore the thread state and return before
                // iquota has been decremented.
                PushData(arena_, &proc->_base, thread);
                thread->scope_stack->pc--;
                return;
              }

              io->ports.xs[port->id] = FbleValueRetain(arena, put->arg);
              PushData(arena_, unit, thread);
              thread->scope_stack = ExitScope(arena, thread->scope_stack);
              FbleProfileExitBlock(arena_, thread->profile);
              break;
            }

            UNREACHABLE("put port must be an output or port value");
          }

          case FBLE_EVAL_PROC_VALUE: {
            FbleEvalProcValue* eval = (FbleEvalProcValue*)proc;
            PushData(arena_, FbleValueRetain(arena, eval->result), thread);
            thread->scope_stack = ExitScope(arena, thread->scope_stack);
            FbleProfileExitBlock(arena_, thread->profile);
            break;
          }

          case FBLE_LINK_PROC_VALUE: {
            FbleLinkProcValue* link = (FbleLinkProcValue*)proc;

            // Allocate the link and push the ports on top of the data stack.
            FbleLinkValue* port = FbleAlloc(arena_, FbleLinkValue);
            FbleRefInit(arena, &port->_base.ref);
            port->_base.tag = FBLE_LINK_VALUE;
            port->head = NULL;
            port->tail = NULL;

            FbleGetProcValue* get = FbleAlloc(arena_, FbleGetProcValue);
            FbleRefInit(arena, &get->_base._base.ref);
            get->_base._base.tag = FBLE_PROC_VALUE;
            get->_base.tag = FBLE_GET_PROC_VALUE;
            get->port = &port->_base;
            Add(arena, &get->_base._base, get->port);
            FbleValueRelease(arena, get->port);

            FblePutFuncValue* put = FbleAlloc(arena_, FblePutFuncValue);
            FbleRefInit(arena, &put->_base._base.ref);
            put->_base._base.tag = FBLE_FUNC_VALUE;
            put->_base.tag = FBLE_PUT_FUNC_VALUE;
            put->_base.argc = 1;
            put->port = &port->_base;
            Add(arena, &put->_base._base, put->port);

            PushData(arena_, &put->_base._base, thread);
            PushData(arena_, &get->_base._base, thread);
            RestoreScope(arena, link->scope, thread);
            thread->scope_stack = ChangeScope(arena, link->body, thread->scope_stack);
            FbleProfileAutoExitBlock(arena_, thread->profile);
            break;
          }

          case FBLE_EXEC_PROC_VALUE: {
            FbleExecProcValue* exec = (FbleExecProcValue*)proc;

            assert(thread->children.size == 0);
            assert(thread->children.xs == NULL);
            FbleVectorInit(arena_, thread->children);
            for (size_t i = 0; i < exec->bindings.size; ++i) {
              Thread* child = FbleAlloc(arena_, Thread);
              InitDataStack(arena_, child);
              PushData(arena_, FbleValueRetain(arena, exec->bindings.xs[i]), child);
              child->scope_stack = EnterScope(arena_, &g_proc_block, NULL);
              child->iquota = 0;
              child->aborted = false;
              child->children.size = 0;
              child->children.xs = NULL;
              child->profile = FbleNewProfileThread(arena_, graph);
              FbleVectorAppend(arena_, thread->children, child);
            }

            RestoreScope(arena, exec->scope, thread);
            thread->scope_stack = ChangeScope(arena, exec->body, thread->scope_stack);
            FbleProfileAutoExitBlock(arena_, thread->profile);
            break;
          }
        }

        FbleValueRelease(arena, &proc->_base);
        break;
      }

      case FBLE_LET_PREP_INSTR: {
        FbleLetPrepInstr* let_instr = (FbleLetPrepInstr*)instr;

        for (size_t i = 0; i < let_instr->count; ++i) {
          FbleRefValue* rv = FbleAlloc(arena_, FbleRefValue);
          FbleRefInit(arena, &rv->_base.ref);
          rv->_base.tag = FBLE_REF_VALUE;
          rv->value = NULL;
          PushVar(arena_, &rv->_base, thread->scope_stack);
        }
        break;
      }

      case FBLE_LET_DEF_INSTR: {
        FbleLetDefInstr* let_def_instr = (FbleLetDefInstr*)instr;
        for (size_t i = 0; i < let_def_instr->count; ++i) {
          FbleRefValue* rv = (FbleRefValue*)GetVar(thread->scope_stack, i);
          assert(rv->_base.tag == FBLE_REF_VALUE);

          rv->value = PopData(arena_, thread);
          Add(arena, &rv->_base, rv->value);
          FbleValueRelease(arena, rv->value);

          assert(rv->value != NULL);
          SetVar(thread->scope_stack, i, FbleValueRetain(arena, rv->value));
          FbleValueRelease(arena, &rv->_base);
        }
        break;
      }

      case FBLE_STRUCT_IMPORT_INSTR: {
        FbleStructImportInstr* import_instr = (FbleStructImportInstr*)instr;
        FbleStructValue* sv = (FbleStructValue*)PopTaggedData(arena, FBLE_STRUCT_VALUE, thread);
        if (sv == NULL) {
          FbleReportError("undefined struct value import\n", &import_instr->loc);
          AbortThread(arena, thread);
          return;
        }
        for (size_t i = 0; i < sv->fields.size; ++i) {
          PushVar(arena_, FbleValueRetain(arena, sv->fields.xs[i]), thread->scope_stack);
        }
        FbleValueRelease(arena, &sv->_base);
        break;
      }

      case FBLE_ENTER_SCOPE_INSTR: {
        FbleEnterScopeInstr* enter_scope_instr = (FbleEnterScopeInstr*)instr;
        thread->scope_stack = EnterScope(arena_, enter_scope_instr->block, thread->scope_stack);
        break;
      }

      case FBLE_EXIT_SCOPE_INSTR: {
        thread->scope_stack = ExitScope(arena, thread->scope_stack);
        FbleProfileExitBlock(arena_, thread->profile);
        break;
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeValue* value = FbleAlloc(arena_, FbleTypeValue);
        FbleRefInit(arena, &value->_base.ref);
        value->_base.tag = FBLE_TYPE_VALUE;
        PushData(arena_, &value->_base, thread);
        break;
      }

      case FBLE_VPUSH_INSTR: {
        FbleVPushInstr* vpush = (FbleVPushInstr*)instr;
        for (size_t i = 0; i < vpush->count; ++i) {
          FbleValue* value = PopData(arena_, thread);
          PushVar(arena_, value, thread->scope_stack);
        }
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

    thread->iquota--;
  }
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

  if (thread->data_stack != NULL) {
    while (!DataStackIsEmpty(thread)) {
      FbleValueRelease(arena, PopData(arena_, thread));
    }
    FreeDataStack(arena_, thread);
    thread->data_stack = NULL;
  }

  while (thread->scope_stack != NULL) {
    thread->scope_stack = ExitScope(arena, thread->scope_stack);
    FbleProfileExitBlock(arena_, thread->profile);
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
//   graph - call graph to save execution stats to.
//   thread - the thread to run.
//
// Results:
//   None.
//
// Side effects:
//   The thread and its children are executed and updated.
//   Updates the call graph based on the execution.
static void RunThreads(FbleValueArena* arena, FbleIO* io, FbleCallGraph* graph, Thread* thread)
{
  // TODO: Make this iterative instead of recursive to avoid smashing the
  // stack.

  // Spend some time running children threads first.
  for (size_t i = 0; i < thread->children.size; ++i) {
    size_t iquota = thread->iquota / (thread->children.size - i);
    Thread* child = thread->children.xs[i];
    thread->iquota -= iquota;
    child->iquota += iquota;
    RunThreads(arena, io, graph, child);
    thread->iquota += child->iquota;
    child->iquota = 0;
  }

  // Spend the remaining time running this thread.
  RunThread(arena, io, graph, thread);
}

// Eval --
//   Execute the given sequence of instructions to completion.
//
// Inputs:
//   arena - the arena to use for allocations.
//   io - io to use for external ports.
//   instrs - the instructions to evaluate.
//   args - an optional initial argument to place on the data stack.
//   graph - call graph to update with execution stats.
//
// Results:
//   The computed value, or NULL on error.
//
// Side effects:
//   The returned value must be freed with FbleValueRelease when no longer in
//   use. Prints a message to stderr in case of error.
//   Updates call graph based on the execution.
static FbleValue* Eval(FbleValueArena* arena, FbleIO* io, FbleInstrBlock* prgm, FbleValueV args, FbleCallGraph* graph)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  Thread thread = {
    .data_stack = NULL,
    .scope_stack = EnterScope(arena_, prgm, NULL),
    .iquota = 0,
    .children = {0, NULL},
    .aborted = false,
    .profile = FbleNewProfileThread(arena_, graph),
  };
  InitDataStack(arena_, &thread);

  for (size_t i = 0; i < args.size; ++i) {
    PushData(arena_, FbleValueRetain(arena, args.xs[i]), &thread);
  }

  // Run the main thread repeatedly until it no longer makes any forward
  // progress.
  size_t QUOTA = 1024;
  bool did_io = false;
  do {
    thread.iquota = QUOTA;
    RunThreads(arena, io, graph, &thread);
    if (thread.aborted) {
      return NULL;
    }

    bool block = (thread.iquota == QUOTA) && (thread.scope_stack != NULL);
    did_io = io->io(io, arena, block);
  } while (did_io || thread.iquota < QUOTA);

  if (thread.scope_stack != NULL) {
    // We have instructions to run still, but we stopped making forward
    // progress. This must be a deadlock.
    // TODO: Handle this more gracefully.
    FbleFreeProfileThread(arena_, thread.profile);
    fprintf(stderr, "Deadlock detected\n");
    abort();
    return NULL;
  }

  // The thread should now be finished with a single value on the data stack
  // which is the value to return.
  FbleValue* final_result = PopData(arena_, &thread);
  FreeDataStack(arena_, &thread);
  assert(thread.scope_stack == NULL);
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
FbleValue* FbleEval(FbleValueArena* arena, FbleProgram* program, FbleNameV* blocks, FbleCallGraph** graph)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  FbleInstrBlock* instrs = FbleCompile(arena_, blocks, program);
  *graph = FbleNewCallGraph(arena_, blocks->size);
  if (instrs == NULL) {
    return NULL;
  }

  FbleIO io = { .io = &NoIO, .ports = { .size = 0, .xs = NULL} };
  FbleValueV args = { .size = 0, .xs = NULL };
  FbleValue* result = Eval(arena, &io, instrs, args, *graph);
  FbleFreeInstrBlock(arena_, instrs);
  return result;
}

// FbleApply -- see documentation in fble.h
FbleValue* FbleApply(FbleValueArena* arena, FbleValue* func, FbleValue* arg, FbleCallGraph* graph)
{
  FbleValueRetain(arena, func);
  assert(func->tag == FBLE_FUNC_VALUE);

  FbleFuncApplyInstr apply = {
    ._base = { .tag = FBLE_FUNC_APPLY_INSTR },
    .loc = { .source = "(internal)", .line = 0, .col = 0 },
    .exit = true
  };
  FbleInstr* instrs[] = { &g_enter_instr._base, &apply._base };
  FbleInstrBlock block = { .refcount = 2, .instrs = { .size = 2, .xs = instrs } };
  FbleIO io = { .io = &NoIO, .ports = { .size = 0, .xs = NULL} };

  FbleValue* xs[2];
  xs[0] = arg;
  xs[1] = func;
  FbleValueV eval_args = { .size = 2, .xs = xs };
  FbleValue* result = Eval(arena, &io, &block, eval_args, graph);
  FbleValueRelease(arena, func);
  return result;
}

// FbleExec -- see documentation in fble.h
FbleValue* FbleExec(FbleValueArena* arena, FbleIO* io, FbleValue* proc, FbleCallGraph* graph)
{
  assert(proc->tag == FBLE_PROC_VALUE);
  FbleValue* xs[1] = { proc };
  FbleValueV args = { .size = 1, .xs = xs };
  FbleValue* result = Eval(arena, io, &g_proc_block, args, graph);
  return result;
}
