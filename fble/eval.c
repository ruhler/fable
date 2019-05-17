// eval.c --
//   This file implements the fble evaluation routines.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for NULL, abort

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

// DataStack --
//   A stack of data values.
typedef struct DataStack {
  FbleValue* value;
  struct DataStack* tail;
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
struct Thread {
  DataStack* data_stack;
  ScopeStack* scope_stack;
  size_t iquota;
  ThreadV children;
  bool aborted;
};

static FbleInstr g_proc_instr = { .tag = FBLE_PROC_INSTR };
static FbleInstr* g_proc_instr_p = &g_proc_instr;
static FbleInstrBlock g_proc_block = {
  .refcount = 1,
  .instrs = { .size = 1, .xs = &g_proc_instr_p }
};

static void Add(FbleRefArena* arena, FbleValue* src, FbleValue* dst);

static FbleValue* Deref(FbleValue* value, FbleValueTag tag);
static void PushVar(FbleArena* arena, FbleValue* value, ScopeStack* scopes);
static void PopVar(FbleArena* arena, ScopeStack* scopes);
static FbleValue* GetVar(ScopeStack* scopes, size_t position);
static void SetVar(ScopeStack* scopes, size_t position, FbleValue* value);
static DataStack* PushData(FbleArena* arena, FbleValue* value, DataStack* tail);
static DataStack* PopData(FbleArena* arena, DataStack* stack);
static ScopeStack* EnterScope(FbleArena* arena, FbleInstrBlock* block, ScopeStack* tail);
static ScopeStack* ExitScope(FbleValueArena* arena, ScopeStack* stack);
static ScopeStack* ChangeScope(FbleValueArena* arena, FbleInstrBlock* block, ScopeStack* stack);

static DataStack* CaptureScope(FbleValueArena* arena, DataStack* data_stack, size_t scopec, FbleValue* value, FbleValueV* dst);
static DataStack* RestoreScope(FbleValueArena* arena, FbleValueV scope, DataStack* stack);

static void RunThread(FbleValueArena* arena, FbleIO* io, Thread* thread);
static void AbortThread(FbleValueArena* arena, Thread* thread);
static void RunThreads(FbleValueArena* arena, FbleIO* io, Thread* thread);
static FbleValue* Eval(FbleValueArena* arena, FbleIO* io, FbleInstrBlock* prgm, FbleValueV args);

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

// Deref --
//   Dereference a value. Removes all layers of reference values until a
//   non-reference value is encountered and returns the non-reference value.
//
//   A tag for the type of dereferenced value should be provided. This
//   function will assert that the correct kind of value is encountered.
//
// Inputs:
//   value - the value to dereference.
//   tag - the expected tag of the dereferenced value.
static FbleValue* Deref(FbleValue* value, FbleValueTag tag)
{
  while (value->tag == FBLE_REF_VALUE) {
    FbleRefValue* rv = (FbleRefValue*)value;

    // In theory, if static analysis was done properly, the code should never
    // try to dereference an abstract reference value.
    assert(rv->value != NULL && "dereference of abstract value");
    value = rv->value;
  }
  assert(value->tag == tag);
  return value;
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

// PushData --
//   Push a value onto a data value stack.
//
// Inputs:
//   arena - the arena to use for allocations
//   value - the value to push
//   tail - the stack to push to
//
// Result:
//   The new stack with pushed value.
//
// Side effects:
//   Allocates a new DataStack instance that should be freed when done.
static DataStack* PushData(FbleArena* arena, FbleValue* value, DataStack* tail)
{
  DataStack* stack = FbleAlloc(arena, DataStack);
  stack->value = value;
  stack->tail = tail;
  return stack;
}

// PopData --
//   Pop a value off the value stack.
//
// Inputs:
//   arena - the arena to use for deallocation
//   stack - the stack to pop from
//
// Results:
//   The popped stack.
//
// Side effects:
//   Frees the top stack frame. It is the user's job to release the value if
//   necessary before popping the top of the stack.
static DataStack* PopData(FbleArena* arena, DataStack* stack)
{
  DataStack* tail = stack->tail;
  FbleFree(arena, stack);
  return tail;
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
//   Capture the variables from the top of the data stack and save it in a value.
//
// Inputs:
//   data_stack - the data stack with the variables to save.
//   scopec - the number of variables to save.
//   value - the value to save the scope to.
//   dst - a pointer to where the scope should be saved to. This is assumed to
//         be a field of the value.
//
// Results:
//   The data stack with scopec values popped off.
//
// Side effects:
//   Stores a copy of the variables from the data stack in dst, and adds
//   references from the value to everything on the scope.
static DataStack* CaptureScope(FbleValueArena* arena, DataStack* data_stack, size_t scopec, FbleValue* value, FbleValueV* dst)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  for (size_t i = 0; i < scopec; ++i) {
    FbleValue* var = data_stack->value;
    data_stack = PopData(arena_, data_stack);
    FbleVectorAppend(arena_, *dst, var);
    Add(arena, value, var);
    FbleValueRelease(arena, var);
  }
  return data_stack;
}

// RestoreScope --
//   Copy the given values to the data stack.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to copy.
//   stack - the stack to copy to.
//
// Result:
//   Updated stack with the scope values pushed.
//
// Side effects:
//   Allocates new DataStack that should be freed when no longer needed.
static DataStack* RestoreScope(FbleValueArena* arena, FbleValueV scope, DataStack* stack)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  for (size_t i = 0; i < scope.size; ++i) {
    stack = PushData(arena_, FbleValueRetain(arena, scope.xs[i]), stack);
  }
  return stack;
}

// RunThread --
//   Run the given thread to completion or until it can no longer make
//   progress.
//
// Inputs:
//   arena - the arena to use for allocations.
//   io - io to use for external ports.
//   thread - the thread to run.
//
// Results:
//   None.
//
// Side effects:
//   The thread is executed, updating its data_stack, and scope_stack.
static void RunThread(FbleValueArena* arena, FbleIO* io, Thread* thread)
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

        FbleValue* argv[argc];
        for (size_t i = 0; i < argc; ++i) {
          argv[argc - i - 1] = thread->data_stack->value;
          thread->data_stack = PopData(arena_, thread->data_stack);
        }

        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);

        FbleValueV args = { .size = argc, .xs = argv, };
        thread->data_stack = PushData(arena_, FbleNewStructValue(arena, args), thread->data_stack);
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        FbleValue* arg = thread->data_stack->value;
        thread->data_stack = PopData(arena_, thread->data_stack);
        thread->data_stack = PushData(arena_, FbleNewUnionValue(arena, union_value_instr->tag, arg), thread->data_stack);
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        assert(thread->data_stack != NULL);
        FbleStructValue* sv = (FbleStructValue*)Deref(thread->data_stack->value, FBLE_STRUCT_VALUE);
        assert(access_instr->tag < sv->fields.size);
        FbleValueRetain(arena, &sv->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);
        thread->data_stack = PushData(arena_, FbleValueRetain(arena, sv->fields.xs[access_instr->tag]), thread->data_stack);
        FbleValueRelease(arena, &sv->_base);
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        assert(thread->data_stack != NULL);
        FbleUnionValue* uv = (FbleUnionValue*)Deref(thread->data_stack->value, FBLE_UNION_VALUE);
        if (uv->tag != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);
          AbortThread(arena, thread);
          return;
        }

        FbleValueRetain(arena, &uv->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);
        thread->data_stack = PushData(arena_, FbleValueRetain(arena, uv->arg), thread->data_stack);

        FbleValueRelease(arena, &uv->_base);
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        assert(thread->data_stack != NULL);
        FbleUnionValue* uv = (FbleUnionValue*)Deref(thread->data_stack->value, FBLE_UNION_VALUE);
        thread->scope_stack->pc += uv->tag;
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);
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
        thread->data_stack = CaptureScope(arena, thread->data_stack, func_value_instr->scopec, &value->_base._base, &value->scope);
        thread->data_stack = PushData(arena_, &value->_base._base, thread->data_stack);
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
        FbleFuncValue* func = (FbleFuncValue*)Deref(thread->data_stack->value, FBLE_FUNC_VALUE);
        FbleValueRetain(arena, &func->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);

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
          value->arg = thread->data_stack->value;
          Add(arena, &value->_base._base, value->arg);
          FbleValueRelease(arena, thread->data_stack->value);
          thread->data_stack = PopData(arena_, thread->data_stack);
          thread->data_stack = PushData(arena_, &value->_base._base, thread->data_stack);

          if (func_apply_instr->exit) {
            thread->scope_stack = ExitScope(arena, thread->scope_stack);
          }
        } else {
          while (func->tag == FBLE_THUNK_FUNC_VALUE) {
            FbleThunkFuncValue* thunk = (FbleThunkFuncValue*)func;
            thread->data_stack = PushData(arena_, FbleValueRetain(arena, thunk->arg), thread->data_stack);
            func = thunk->func;
            FbleValueRetain(arena, &func->_base);
            FbleValueRelease(arena, &thunk->_base._base);
          }

          assert(func->tag == FBLE_BASIC_FUNC_VALUE);
          FbleBasicFuncValue* basic = (FbleBasicFuncValue*)func;
          thread->data_stack = RestoreScope(arena, basic->scope, thread->data_stack);
          if (func_apply_instr->exit) {
            thread->scope_stack = ChangeScope(arena, basic->body, thread->scope_stack);
          } else {
            thread->scope_stack = EnterScope(arena_, basic->body, thread->scope_stack);
          }
        }
        FbleValueRelease(arena, &func->_base);
        break;
      }

      case FBLE_GET_INSTR: {
        FbleGetProcValue* value = FbleAlloc(arena_, FbleGetProcValue);
        FbleRefInit(arena, &value->_base._base.ref);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base.tag = FBLE_GET_PROC_VALUE;
        value->port = thread->data_stack->value;
        Add(arena, &value->_base._base, value->port);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);
        thread->data_stack = PushData(arena_, &value->_base._base, thread->data_stack);
        break;
      }

      case FBLE_PUT_INSTR: {
        FblePutProcValue* value = FbleAlloc(arena_, FblePutProcValue);
        FbleRefInit(arena, &value->_base._base.ref);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base.tag = FBLE_PUT_PROC_VALUE;
        value->arg = thread->data_stack->value;
        // TODO: do we need to set value->port to NULL here first?
        Add(arena, &value->_base._base, value->arg);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);
        value->port = thread->data_stack->value;
        Add(arena, &value->_base._base, value->port);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);
        thread->data_stack = PushData(arena_, &value->_base._base, thread->data_stack);
        break;
      }

      case FBLE_EVAL_INSTR: {
        FbleEvalProcValue* proc_value = FbleAlloc(arena_, FbleEvalProcValue);
        FbleRefInit(arena, &proc_value->_base._base.ref);
        proc_value->_base._base.tag = FBLE_PROC_VALUE;
        proc_value->_base.tag = FBLE_EVAL_PROC_VALUE;
        proc_value->result = thread->data_stack->value;
        thread->data_stack = PopData(arena_, thread->data_stack);
        thread->data_stack = PushData(arena_, &proc_value->_base._base, thread->data_stack);
        break;
      }

      case FBLE_VAR_INSTR: {
        FbleVarInstr* var_instr = (FbleVarInstr*)instr;
        assert(thread->scope_stack != NULL);
        assert(var_instr->position < thread->scope_stack->vars.size);
        FbleValue* value = thread->scope_stack->vars.xs[var_instr->position];
        thread->data_stack = PushData(arena_, FbleValueRetain(arena, value), thread->data_stack);
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
        thread->data_stack = CaptureScope(arena, thread->data_stack, link_instr->scopec, &value->_base._base, &value->scope);
        thread->data_stack = PushData(arena_, &value->_base._base, thread->data_stack);
        break;
      }

      case FBLE_EXEC_INSTR: {
        FbleExecInstr* exec_instr = (FbleExecInstr*)instr;
        FbleExecProcValue* value = FbleAlloc(arena_, FbleExecProcValue);
        FbleRefInit(arena, &value->_base._base.ref);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base.tag = FBLE_EXEC_PROC_VALUE;
        value->bindings.size = exec_instr->argc;
        value->bindings.xs = FbleArenaAlloc(arena_, value->bindings.size * sizeof(FbleValue*), FbleAllocMsg(__FILE__, __LINE__));
        FbleVectorInit(arena_, value->scope);
        value->body = exec_instr->body;
        value->body->refcount++;
        thread->data_stack = CaptureScope(arena, thread->data_stack, exec_instr->scopec, &value->_base._base, &value->scope);

        for (size_t i = 0; i < exec_instr->argc; ++i) {
          size_t j = exec_instr->argc - 1 - i;
          value->bindings.xs[j] = thread->data_stack->value;
          Add(arena, &value->_base._base, thread->data_stack->value);
          FbleValueRelease(arena, thread->data_stack->value);
          thread->data_stack = PopData(arena_, thread->data_stack);
        }

        thread->data_stack = PushData(arena_, &value->_base._base, thread->data_stack);
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
          assert(child->data_stack != NULL);
          FbleValue* result = child->data_stack->value;
          child->data_stack = PopData(arena_, child->data_stack);
          assert(child->data_stack == NULL);
          assert(child->scope_stack == NULL);
          assert(child->iquota == 0);
          FbleFree(arena_, child);

          PushVar(arena_, result, thread->scope_stack);
        }

        thread->children.size = 0;
        FbleFree(arena_, thread->children.xs);
        thread->children.xs = NULL;
        break;
      }

      case FBLE_PROC_INSTR: {
        FbleProcValue* proc = (FbleProcValue*)Deref(thread->data_stack->value, FBLE_PROC_VALUE);
        FbleValueRetain(arena, &proc->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);

        switch (proc->tag) {
          case FBLE_GET_PROC_VALUE: {
            FbleGetProcValue* get = (FbleGetProcValue*)proc;

            if (get->port->tag == FBLE_INPUT_VALUE) {
              FbleInputValue* port = (FbleInputValue*)get->port;

              if (port->head == NULL) {
                // Blocked on get. Restore the thread state and return before
                // iquota has been decremented.
                thread->data_stack = PushData(arena_, &proc->_base, thread->data_stack);
                thread->scope_stack->pc--;
                return;
              }

              FbleValues* head = port->head;
              port->head = port->head->next;
              if (port->head == NULL) {
                port->tail = NULL;
              }
              thread->data_stack = PushData(arena_, head->value, thread->data_stack);
              FbleFree(arena_, head);

              thread->scope_stack = ExitScope(arena, thread->scope_stack);
              break;
            }

            if (get->port->tag == FBLE_PORT_VALUE) {
              FblePortValue* port = (FblePortValue*)get->port;
              assert(port->id < io->ports.size);
              if (io->ports.xs[port->id] == NULL) {
                // Blocked on get. Restore the thread state and return before
                // iquota has been decremented.
                thread->data_stack = PushData(arena_, &proc->_base, thread->data_stack);
                thread->scope_stack->pc--;
                return;
              }

              thread->data_stack = PushData(arena_, io->ports.xs[port->id], thread->data_stack);
              io->ports.xs[port->id] = NULL;

              thread->scope_stack = ExitScope(arena, thread->scope_stack);
              break;
            }

            UNREACHABLE("get port must be an input or port value");
            break;
          }

          case FBLE_PUT_PROC_VALUE: {
            FblePutProcValue* put = (FblePutProcValue*)proc;

            if (put->port->tag == FBLE_OUTPUT_VALUE) {
              FbleOutputValue* port = (FbleOutputValue*)put->port;

              FbleValues* tail = FbleAlloc(arena_, FbleValues);
              tail->value = FbleValueRetain(arena, put->arg);
              tail->next = NULL;

              FbleInputValue* link = port->dest;
              if (link->head == NULL) {
                link->head = tail;
                link->tail = tail;
              } else {
                assert(link->tail != NULL);
                link->tail->next = tail;
                link->tail = tail;
              }

              thread->data_stack = PushData(arena_, FbleValueRetain(arena, put->arg), thread->data_stack);
              thread->scope_stack = ExitScope(arena, thread->scope_stack);
              break;
            }

            if (put->port->tag == FBLE_PORT_VALUE) {
              FblePortValue* port = (FblePortValue*)put->port;
              assert(port->id < io->ports.size);

              if (io->ports.xs[port->id] != NULL) {
                // Blocked on put. Restore the thread state and return before
                // iquota has been decremented.
                thread->data_stack = PushData(arena_, &proc->_base, thread->data_stack);
                thread->scope_stack->pc--;
                return;
              }

              io->ports.xs[port->id] = FbleValueRetain(arena, put->arg);
              thread->data_stack = PushData(arena_, FbleValueRetain(arena, put->arg), thread->data_stack);
              thread->scope_stack = ExitScope(arena, thread->scope_stack);
              break;
            }

            UNREACHABLE("put port must be an output or port value");
          }

          case FBLE_EVAL_PROC_VALUE: {
            FbleEvalProcValue* eval = (FbleEvalProcValue*)proc;
            thread->data_stack = PushData(arena_, FbleValueRetain(arena, eval->result), thread->data_stack);
            thread->scope_stack = ExitScope(arena, thread->scope_stack);
            break;
          }

          case FBLE_LINK_PROC_VALUE: {
            FbleLinkProcValue* link = (FbleLinkProcValue*)proc;

            // Allocate the link and push the ports on top of the data stack.
            FbleInputValue* get = FbleAlloc(arena_, FbleInputValue);
            FbleRefInit(arena, &get->_base.ref);
            get->_base.tag = FBLE_INPUT_VALUE;
            get->head = NULL;
            get->tail = NULL;

            FbleOutputValue* put = FbleAlloc(arena_, FbleOutputValue);
            FbleRefInit(arena, &put->_base.ref);
            put->_base.tag = FBLE_OUTPUT_VALUE;
            put->dest = get;
            Add(arena, &put->_base, &get->_base);

            thread->data_stack = PushData(arena_, &put->_base, thread->data_stack);
            thread->data_stack = PushData(arena_, &get->_base, thread->data_stack);
            thread->data_stack = RestoreScope(arena, link->scope, thread->data_stack);
            thread->scope_stack = ChangeScope(arena, link->body, thread->scope_stack);
            break;
          }

          case FBLE_EXEC_PROC_VALUE: {
            FbleExecProcValue* exec = (FbleExecProcValue*)proc;

            assert(thread->children.size == 0);
            assert(thread->children.xs == NULL);
            FbleVectorInit(arena_, thread->children);
            for (size_t i = 0; i < exec->bindings.size; ++i) {
              Thread* child = FbleAlloc(arena_, Thread);
              child->data_stack = PushData(arena_, FbleValueRetain(arena, exec->bindings.xs[i]), NULL);
              child->scope_stack = EnterScope(arena_, &g_proc_block, NULL);
              child->iquota = 0;
              child->aborted = false;
              child->children.size = 0;
              child->children.xs = NULL;
              FbleVectorAppend(arena_, thread->children, child);
            }

            thread->data_stack = RestoreScope(arena, exec->scope, thread->data_stack);
            thread->scope_stack = ChangeScope(arena, exec->body, thread->scope_stack);
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

          rv->value = thread->data_stack->value;
          Add(arena, &rv->_base, thread->data_stack->value);
          FbleValueRelease(arena, thread->data_stack->value);
          thread->data_stack = PopData(arena_, thread->data_stack);

          assert(rv->value != NULL);
          SetVar(thread->scope_stack, i, FbleValueRetain(arena, rv->value));
          FbleValueRelease(arena, &rv->_base);
        }
        break;
      }

      case FBLE_STRUCT_IMPORT_INSTR: {
        FbleStructValue* sv = (FbleStructValue*)Deref(thread->data_stack->value, FBLE_STRUCT_VALUE);
        FbleValueRetain(arena, &sv->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);
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
        break;
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeValue* value = FbleAlloc(arena_, FbleTypeValue);
        FbleRefInit(arena, &value->_base.ref);
        value->_base.tag = FBLE_TYPE_VALUE;
        thread->data_stack = PushData(arena_, &value->_base, thread->data_stack);
        break;
      }

      case FBLE_VPUSH_INSTR: {
        FbleVPushInstr* vpush = (FbleVPushInstr*)instr;
        for (size_t i = 0; i < vpush->count; ++i) {
          assert(thread->data_stack != NULL);
          FbleValue* value = thread->data_stack->value;
          thread->data_stack = PopData(arena_, thread->data_stack);
          PushVar(arena_, value, thread->scope_stack);
        }
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

  while (thread->data_stack != NULL) {
    FbleValueRelease(arena, thread->data_stack->value);
    thread->data_stack = PopData(arena_, thread->data_stack);
  }

  while (thread->scope_stack != NULL) {
    thread->scope_stack = ExitScope(arena, thread->scope_stack);
  }
}

// RunThreads --
//   Run the given thread and its children to completion or until it can no
//   longer make progress.
//
// Inputs:
//   arena - the arena to use for allocations.
//   io - io to use for external ports.
//   thread - the thread to run.
//
// Results:
//   None.
//
// Side effects:
//   The thread and its children are executed and updated.
static void RunThreads(FbleValueArena* arena, FbleIO* io, Thread* thread)
{
  // TODO: Make this iterative instead of recursive to avoid smashing the
  // stack.

  // Spend some time running children threads first.
  for (size_t i = 0; i < thread->children.size; ++i) {
    size_t iquota = thread->iquota / (thread->children.size - i);
    Thread* child = thread->children.xs[i];
    thread->iquota -= iquota;
    child->iquota += iquota;
    RunThreads(arena, io, child);
    thread->iquota += child->iquota;
    child->iquota = 0;
  }

  // Spend the remaining time running this thread.
  RunThread(arena, io, thread);
}

// Eval --
//   Execute the given sequence of instructions to completion.
//
// Inputs:
//   arena - the arena to use for allocations.
//   io - io to use for external ports.
//   instrs - the instructions to evaluate.
//   args - an optional initial argument to place on the data stack.
//
// Results:
//   The computed value, or NULL on error.
//
// Side effects:
//   The returned value must be freed with FbleValueRelease when no longer in
//   use. Prints a message to stderr in case of error.
static FbleValue* Eval(FbleValueArena* arena, FbleIO* io, FbleInstrBlock* prgm, FbleValueV args)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  Thread thread = {
    .data_stack = NULL,
    .scope_stack = EnterScope(arena_, prgm, NULL),
    .iquota = 0,
    .children = {0, NULL},
    .aborted = false,
  };

  for (size_t i = 0; i < args.size; ++i) {
    thread.data_stack = PushData(arena_, FbleValueRetain(arena, args.xs[i]), thread.data_stack);
  }

  // Run the main thread repeatedly until it no longer makes any forward
  // progress.
  size_t QUOTA = 1024;
  bool did_io = false;
  do {
    thread.iquota = QUOTA;
    RunThreads(arena, io, &thread);
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
    fprintf(stderr, "Deadlock detected\n");
    abort();
    return NULL;
  }

  // The thread should now be finished with a single value on the data stack
  // which is the value to return.
  assert(thread.data_stack != NULL);
  FbleValue* final_result = thread.data_stack->value;
  thread.data_stack = PopData(arena_, thread.data_stack);
  assert(thread.data_stack == NULL);
  assert(thread.scope_stack == NULL);
  assert(thread.children.size == 0);
  assert(thread.children.xs == NULL);
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
FbleValue* FbleEval(FbleValueArena* arena, FbleExpr* expr)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleInstrBlock* instrs = FbleCompile(arena_, expr);
  if (instrs == NULL) {
    return NULL;
  }

  FbleIO io = { .io = &NoIO, .ports = { .size = 0, .xs = NULL} };
  FbleValueV args = { .size = 0, .xs = NULL };
  FbleValue* result = Eval(arena, &io, instrs, args);
  FbleFreeInstrBlock(arena_, instrs);
  return result;
}

// FbleApply -- see documentation in fble.h
FbleValue* FbleApply(FbleValueArena* arena, FbleValue* func, FbleValueV args)
{
  FbleValue* result = func;
  FbleValueRetain(arena, result);
  assert(args.size > 0);
  for (size_t i = 0; i < args.size; ++i) {
    assert(result->tag == FBLE_FUNC_VALUE);
    func = result;

    FbleFuncApplyInstr apply = {
      ._base = { .tag = FBLE_FUNC_APPLY_INSTR },
      .exit = true
    };
    FbleInstr* instrs[] = { &apply._base };
    FbleInstrBlock block = { .refcount = 1, .instrs = { .size = 1, .xs = instrs } };
    FbleIO io = { .io = &NoIO, .ports = { .size = 0, .xs = NULL} };

    FbleValue* xs[2];
    xs[0] = args.xs[i];
    xs[1] = func;
    FbleValueV eval_args = { .size = 2, .xs = xs };
    result = Eval(arena, &io, &block, eval_args);
    FbleValueRelease(arena, func);
  }
  return result;
}

// FbleExec -- see documentation in fble.h
FbleValue* FbleExec(FbleValueArena* arena, FbleIO* io, FbleProcValue* proc)
{
  FbleValue* xs[1] = { &proc->_base };
  FbleValueV args = { .size = 1, .xs = xs };
  FbleValue* result = Eval(arena, io, &g_proc_block, args);
  return result;
}
