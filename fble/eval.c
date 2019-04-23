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

// CodeStack --
//   A stack of instructions to execute. Describes the computation context for
//   a thread.
// 
// Fields:
//   retain - A value to retain for the duration of the stack frame. May be NULL.
//   instrs - A vector of instructions in the currently executing block.
//   pc - The location of the next instruction in the current block to execute.
//   scope - used to store locally visible variables.
typedef struct CodeStack {
  FbleValue* retain;
  FbleInstrV instrs;
  size_t pc;
  FbleScope* scope;
  struct CodeStack* tail;
} CodeStack;

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
//   data_stack - used to store anonymous intermediate values
//   code_stack - stores the instructions still to execute.
//   iquota - the number of instructions this thread is allowed to execute.
//   children - child threads that this thread is potentially blocked on.
//      This vector is {0, NULL} when there are no child threads.
//   aborted - true if the thread has been aborted due to undefined union
//             field access.
struct Thread {
  DataStack* data_stack;
  CodeStack* code_stack;
  size_t iquota;
  ThreadV children;
  bool aborted;
};

static FbleInstr g_proc_instr = { .tag = FBLE_PROC_INSTR };
static FbleInstr g_ipop_instr = { .tag = FBLE_IPOP_INSTR };
static FbleInstr* g_proc_block_instrs[] = { &g_proc_instr, &g_ipop_instr };
static FbleInstrBlock g_proc_block = {
  .refcount = 1,
  .instrs = { .size = 2, .xs = g_proc_block_instrs }
};

static void Add(FbleRefArena* arena, FbleValue* src, FbleValue* dst);

static FbleValue* Deref(FbleValue* value, FbleValueTag tag);
static FbleScope* PushVar(FbleArena* arena, FbleValue* value, FbleScope* tail);
static FbleScope* PopVar(FbleArena* arena, FbleScope* vstack);
static DataStack* PushData(FbleArena* arena, FbleValue* value, DataStack* tail);
static DataStack* PopData(FbleArena* arena, DataStack* stack);
static CodeStack* PushCode(FbleValueArena* arena, FbleValue* retain, FbleScope* scope, FbleInstrBlock* block, CodeStack* tail);
static CodeStack* PopCode(FbleValueArena* arena, CodeStack* stack);

static void CaptureScope(FbleValueArena* arena, FbleScope* src, size_t scopec, FbleValue* value, FbleScope** dst);
static FbleScope* CopyScope(FbleValueArena* arena, FbleScope* scope);

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
//   Push a value onto a scope.
//
// Inputs:
//   arena - the arena to use for allocations
//   value - the value to push
//   tail - the scope to push to
//
// Result:
//   The new scope with pushed value.
//
// Side effects:
//   Allocates a new FbleScope instance that should be freed when done.
static FbleScope* PushVar(FbleArena* arena, FbleValue* value, FbleScope* tail)
{
  FbleScope* scope = FbleAlloc(arena, FbleScope);
  scope->value = value;
  scope->tail = tail;
  return scope;
}

// PopVar --
//   Pop a value off the scope.
//
// Inputs:
//   arena - the arena to use for deallocation
//   scope - the stack to pop from
//
// Results:
//   The new scope.
//
// Side effects:
//   Frees the top stack frame. It is the users job to release the value if
//   necessary before popping from the scope.
static FbleScope* PopVar(FbleArena* arena, FbleScope* scope)
{
  FbleScope* tail = scope->tail;
  FbleFree(arena, scope);
  return tail;
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

// PushCode --
//   Push a block of instructions onto an instruction stack.
//
// Inputs:
//   arena - the arena to use for allocations
//   retain - an optional value to retain for the duration of the stack frame.
//   scope - the initial scope, assumed to be shared with other contexts.
//   block - the block of instructions to push
//   tail - the stack to push to
//
// Result:
//   The new stack with pushed instructions.
//
// Side effects:
//   Allocates new CodeStack instances that should be freed when done. Takes over
//   ownership of the passed 'retain' value.
static CodeStack* PushCode(FbleValueArena* arena, FbleValue* retain, FbleScope* scope, FbleInstrBlock* block, CodeStack* tail)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  // For debugging purposes, double check that all blocks will pop themselves
  // when done.
  assert(block->instrs.size > 0);
  assert(block->instrs.xs[block->instrs.size - 1]->tag == FBLE_IPOP_INSTR);

  CodeStack* stack = FbleAlloc(arena_, CodeStack);
  stack->retain = retain;
  stack->instrs = block->instrs;
  stack->pc = 0;
  stack->scope = CopyScope(arena, scope);
  stack->tail = tail;
  return stack;
}

// PopCode --
//   Pop a value off the instruction stack.
//
// Inputs:
//   arena - the arena to use for deallocation
//   stack - the stack to pop from
//
// Results:
//   The popped stack.
//
// Side effects:
//   Frees the top instruction stack.
static CodeStack* PopCode(FbleValueArena* arena, CodeStack* stack)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  while (stack->scope != NULL) {
    FbleValueRelease(arena, stack->scope->value);
    stack->scope = PopVar(arena_, stack->scope);
  }

  FbleValueRelease(arena, stack->retain);
  CodeStack* tail = stack->tail;
  FbleFree(FbleRefArenaArena(arena), stack);
  return tail;
}

// CaptureScope --
//   Capture the current scope and save it in a value.
//
// Inputs:
//   src - the scope to save.
//   scopec - the size of the scope to save.
//   value - the value to save the scope to.
//   dst - a pointer to where the scope should be saved to. This is assumed to
//         be a field of the value.
//
// Results:
//   none.
//
// Side effects:
//   Makes a copy of the src scope, stores the copy in dst, and adds
//   references from the value to everything on the scope.
static void CaptureScope(FbleValueArena* arena, FbleScope* src, size_t scopec, FbleValue* value, FbleScope** dst)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleValue* scope[scopec];
  FbleScope* vs = src;
  for (size_t i = 0; i < scopec; ++i) {
    assert(vs != NULL);
    scope[scopec - 1 - i] = vs->value;
    vs = vs->tail;
  }

  for (size_t i = 0; i < scopec; ++i) {
    *dst = PushVar(arena_, scope[i], *dst);
    Add(arena, value, scope[i]);
  }
}

// CopyScope --
//   Make a copy of the given scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to copy.
//
// Result:
//   A copy of the scope, with its own references to the values on the scope.
//
// Side effects:
//   Allocates a new scope that should be freed when no longer needed.
static FbleScope* CopyScope(FbleValueArena* arena, FbleScope* scope)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleScope* reverse = NULL;
  for (FbleScope* vs = scope; vs != NULL; vs = vs->tail) {
    reverse = PushVar(arena_, vs->value, reverse);
  }

  FbleScope* copy = NULL;
  while (reverse != NULL) {
    copy = PushVar(arena_, FbleValueRetain(arena, reverse->value), copy);
    reverse = PopVar(arena_, reverse);
  }
  return copy;
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
//   The thread is executed, updating its data_stack, and code_stack.
static void RunThread(FbleValueArena* arena, FbleIO* io, Thread* thread)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  assert(!thread->aborted);
  while (thread->iquota > 0 && thread->code_stack != NULL) {
    assert(thread->code_stack->pc < thread->code_stack->instrs.size);
    FbleInstr* instr = thread->code_stack->instrs.xs[thread->code_stack->pc++];
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
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
        assert(thread->data_stack != NULL);
        FbleUnionValue* uv = (FbleUnionValue*)Deref(thread->data_stack->value, FBLE_UNION_VALUE);
        FbleValueRetain(arena, &uv->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);
        assert(uv->tag < select_instr->choices.size);
        thread->code_stack = PushCode(arena, NULL, thread->code_stack->scope, select_instr->choices.xs[uv->tag], thread->code_stack);
        FbleValueRelease(arena, &uv->_base);
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        FbleFuncValue* value = FbleAlloc(arena_, FbleFuncValue);
        FbleRefInit(arena, &value->_base.ref);
        value->_base.tag = FBLE_FUNC_VALUE;
        value->scope = NULL;
        value->body = func_value_instr->body;
        value->body->refcount++;
        CaptureScope(arena, thread->code_stack->scope, func_value_instr->scopec, &value->_base, &value->scope);
        thread->data_stack = PushData(arena_, &value->_base, thread->data_stack);
        break;
      }

      case FBLE_DESCOPE_INSTR: {
        FbleDescopeInstr* descope_instr = (FbleDescopeInstr*)instr;
        for (size_t i = 0; i < descope_instr->count; ++i) {
          assert(thread->code_stack->scope != NULL);
          FbleValueRelease(arena, thread->code_stack->scope->value);
          thread->code_stack->scope = PopVar(arena_, thread->code_stack->scope);
        }
        break;
      }

      case FBLE_FUNC_APPLY_INSTR: {
        FbleValue* arg = thread->data_stack->value;
        thread->data_stack = PopData(arena_, thread->data_stack);

        FbleFuncValue* func = (FbleFuncValue*)Deref(thread->data_stack->value, FBLE_FUNC_VALUE);
        FbleValueRetain(arena, &func->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);
        thread->code_stack = PushCode(arena, &func->_base, func->scope, func->body, thread->code_stack);
        thread->code_stack->scope = PushVar(arena_, arg, thread->code_stack->scope);
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
        FbleScope* v = thread->code_stack->scope;
        for (size_t i = 0; i < var_instr->position; ++i) {
          assert(v->tail != NULL);
          v = v->tail;
        }
        thread->data_stack = PushData(arena_, FbleValueRetain(arena, v->value), thread->data_stack);
        break;
      }

      case FBLE_LINK_INSTR: {
        FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
        FbleLinkProcValue* value = FbleAlloc(arena_, FbleLinkProcValue);
        FbleRefInit(arena, &value->_base._base.ref);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base.tag = FBLE_LINK_PROC_VALUE;
        value->scope = NULL;
        value->body = link_instr->body;
        value->body->refcount++;
        CaptureScope(arena, thread->code_stack->scope, link_instr->scopec, &value->_base._base, &value->scope);
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
        value->scope = NULL;
        value->body = exec_instr->body;
        value->body->refcount++;
        CaptureScope(arena, thread->code_stack->scope, exec_instr->scopec, &value->_base._base, &value->scope);

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

          if (thread->children.xs[i]->code_stack != NULL) {
            // Blocked on child. Restore the thread state and return before
            // iquota has been decremented.
            thread->code_stack->pc--;
            return;
          }
        }
        
        for (size_t i = 0; i < thread->children.size; ++i) {
          Thread* child = thread->children.xs[i];
          assert(child->data_stack != NULL);
          FbleValue* result = child->data_stack->value;
          child->data_stack = PopData(arena_, child->data_stack);
          assert(child->data_stack == NULL);
          assert(child->code_stack == NULL);
          assert(child->iquota == 0);
          FbleFree(arena_, child);

          thread->code_stack->scope = PushVar(arena_, result, thread->code_stack->scope);
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
                thread->code_stack->pc--;
                return;
              }

              FbleValues* head = port->head;
              port->head = port->head->next;
              if (port->head == NULL) {
                port->tail = NULL;
              }
              thread->data_stack = PushData(arena_, head->value, thread->data_stack);
              FbleFree(arena_, head);
              break;
            }

            if (get->port->tag == FBLE_PORT_VALUE) {
              FblePortValue* port = (FblePortValue*)get->port;
              assert(port->id < io->ports.size);
              if (io->ports.xs[port->id] == NULL) {
                // Blocked on get. Restore the thread state and return before
                // iquota has been decremented.
                thread->data_stack = PushData(arena_, &proc->_base, thread->data_stack);
                thread->code_stack->pc--;
                return;
              }

              thread->data_stack = PushData(arena_, io->ports.xs[port->id], thread->data_stack);
              io->ports.xs[port->id] = NULL;
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
              break;
            }

            if (put->port->tag == FBLE_PORT_VALUE) {
              FblePortValue* port = (FblePortValue*)put->port;
              assert(port->id < io->ports.size);

              if (io->ports.xs[port->id] != NULL) {
                // Blocked on put. Restore the thread state and return before
                // iquota has been decremented.
                thread->data_stack = PushData(arena_, &proc->_base, thread->data_stack);
                thread->code_stack->pc--;
                return;
              }

              io->ports.xs[port->id] = FbleValueRetain(arena, put->arg);
              thread->data_stack = PushData(arena_, FbleValueRetain(arena, put->arg), thread->data_stack);
              break;
            }

            UNREACHABLE("put port must be an output or port value");
          }

          case FBLE_EVAL_PROC_VALUE: {
            FbleEvalProcValue* eval = (FbleEvalProcValue*)proc;
            thread->data_stack = PushData(arena_, FbleValueRetain(arena, eval->result), thread->data_stack);
            break;
          }

          case FBLE_LINK_PROC_VALUE: {
            FbleLinkProcValue* link = (FbleLinkProcValue*)proc;
            thread->code_stack = PushCode(arena, FbleValueRetain(arena, &link->_base._base), link->scope, link->body, thread->code_stack);

            // Allocate the link and push the ports on top of the variable stack.
            FbleInputValue* get = FbleAlloc(arena_, FbleInputValue);
            FbleRefInit(arena, &get->_base.ref);
            get->_base.tag = FBLE_INPUT_VALUE;
            get->head = NULL;
            get->tail = NULL;
            thread->code_stack->scope = PushVar(arena_, &get->_base, thread->code_stack->scope);

            FbleOutputValue* put = FbleAlloc(arena_, FbleOutputValue);
            FbleRefInit(arena, &put->_base.ref);
            put->_base.tag = FBLE_OUTPUT_VALUE;
            put->dest = get;
            Add(arena, &put->_base, &get->_base);
            thread->code_stack->scope = PushVar(arena_, &put->_base, thread->code_stack->scope);
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
              child->code_stack = PushCode(arena, NULL, NULL, &g_proc_block, NULL);
              child->iquota = 0;
              child->aborted = false;
              child->children.size = 0;
              child->children.xs = NULL;
              FbleVectorAppend(arena_, thread->children, child);
            }

            thread->code_stack = PushCode(arena, FbleValueRetain(arena, &proc->_base), exec->scope, exec->body, thread->code_stack);
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
          thread->code_stack->scope = PushVar(arena_, &rv->_base, thread->code_stack->scope);
        }
        break;
      }

      case FBLE_LET_DEF_INSTR: {
        FbleLetDefInstr* let_def_instr = (FbleLetDefInstr*)instr;
        FbleScope* vs = thread->code_stack->scope;
        for (size_t i = 0; i < let_def_instr->count; ++i) {
          assert(vs != NULL);
          FbleRefValue* rv = (FbleRefValue*) vs->value;
          assert(rv->_base.tag == FBLE_REF_VALUE);

          rv->value = thread->data_stack->value;
          Add(arena, &rv->_base, thread->data_stack->value);
          FbleValueRelease(arena, thread->data_stack->value);
          thread->data_stack = PopData(arena_, thread->data_stack);

          assert(rv->value != NULL);
          vs->value = FbleValueRetain(arena, rv->value);
          FbleValueRelease(arena, &rv->_base);

          vs = vs->tail;
        }
        break;
      }

      case FBLE_NAMESPACE_INSTR: {
        FbleStructValue* sv = (FbleStructValue*)Deref(thread->data_stack->value, FBLE_STRUCT_VALUE);
        FbleValueRetain(arena, &sv->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = PopData(arena_, thread->data_stack);
        for (size_t i = 0; i < sv->fields.size; ++i) {
          thread->code_stack->scope = PushVar(arena_, FbleValueRetain(arena, sv->fields.xs[i]), thread->code_stack->scope);
        }
        FbleValueRelease(arena, &sv->_base);
        break;
      }

      case FBLE_IPOP_INSTR: {
        thread->code_stack = PopCode(arena, thread->code_stack);
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
        FbleValue* value = thread->data_stack->value;
        thread->data_stack = PopData(arena_, thread->data_stack);
        thread->code_stack->scope = PushVar(arena_, value, thread->code_stack->scope);
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

  while (thread->code_stack != NULL) {
    thread->code_stack = PopCode(arena, thread->code_stack);
  }

  while (thread->data_stack != NULL) {
    FbleValueRelease(arena, thread->data_stack->value);
    thread->data_stack = PopData(arena_, thread->data_stack);
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
    .code_stack = PushCode(arena, NULL, NULL, prgm, NULL),
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

    bool block = (thread.iquota == QUOTA) && (thread.code_stack != NULL);
    did_io = io->io(io, arena, block);
  } while (did_io || thread.iquota < QUOTA);

  if (thread.code_stack != NULL) {
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
  assert(thread.code_stack == NULL);
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
FbleValue* FbleApply(FbleValueArena* arena, FbleFuncValue* func, FbleValueV args)
{
  FbleValue* result = &func->_base;
  FbleValueRetain(arena, result);
  assert(args.size > 0);
  for (size_t i = 0; i < args.size; ++i) {
    assert(result->tag == FBLE_FUNC_VALUE);
    func = (FbleFuncValue*)result;

    FbleFuncApplyInstr apply = { ._base = { .tag = FBLE_FUNC_APPLY_INSTR }, };
    FbleIPopInstr ipop = { ._base = { .tag = FBLE_IPOP_INSTR }, };
    FbleInstr* instrs[] = { &apply._base, &ipop._base };
    FbleInstrBlock block = { .refcount = 1, .instrs = { .size = 2, .xs = instrs } };
    FbleIO io = { .io = &NoIO, .ports = { .size = 0, .xs = NULL} };

    FbleValue* xs[2];
    xs[0] = &func->_base;
    xs[1] = args.xs[i];
    FbleValueV eval_args = { .size = 1 + args.size, .xs = xs };
    result = Eval(arena, &io, &block, eval_args);
    FbleValueRelease(arena, &func->_base);
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
