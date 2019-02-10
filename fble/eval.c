// eval.c --
//   This file implements the fble evaluation routines.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for NULL, abort

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

// IStack --
//   A stack of instructions to execute. Describes the computation context for
//   a thread.
// 
// Fields:
//   instr -  The instructions for executing the expression.
typedef struct IStack {
  FbleInstr* instr;
  struct IStack* tail;
} IStack;

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
//   var_stack - used to store named values (aka variables)
//   data_stack - used to store anonymous intermediate values
//   istack - stores the instructions still to execute.
//   iquota - the number of instructions this thread is allowed to execute.
//   children - child threads that this thread is potentially blocked on.
struct Thread {
  FbleVStack* var_stack;
  FbleVStack* data_stack;
  IStack* istack;
  size_t iquota;
  ThreadV children;
};

static void Add(FbleRefArena* arena, FbleValue* src, FbleValue* dst);

static FbleValue* Deref(FbleValue* value, FbleValueTag tag);
static FbleVStack* VPush(FbleArena* arena, FbleValue* value, FbleVStack* tail);
static FbleVStack* VPop(FbleArena* arena, FbleVStack* vstack);
static IStack* IPush(FbleArena* arena, FbleInstr* instr, IStack* tail); 
static IStack* IPop(FbleArena* arena, IStack* istack);

static void RunThread(FbleValueArena* arena, Thread* thread);
static void RunThreads(FbleValueArena* arena, Thread* thread);
static FbleValue* Eval(FbleValueArena* arena, FbleInstr* prgm, FbleValue* arg);


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

// VPush --
//   Push a value onto a value stack.
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
//   Allocates a new FbleVStack instance that should be freed when done.
static FbleVStack* VPush(FbleArena* arena, FbleValue* value, FbleVStack* tail)
{
  FbleVStack* vstack = FbleAlloc(arena, FbleVStack);
  vstack->value = value;
  vstack->tail = tail;
  return vstack;
}

// VPop --
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
//   Frees the top stack frame. It is the users job to release the value if
//   necessary before popping the top of the stack.
static FbleVStack* VPop(FbleArena* arena, FbleVStack* vstack)
{
  FbleVStack* tail = vstack->tail;
  FbleFree(arena, vstack);
  return tail;
}

// IPush --
//   Push an instruction onto an instruction stack.
//
// Inputs:
//   arena - the arena to use for allocations
//   instr - the instr to push
//   tail - the stack to push to
//
// Result:
//   The new stack with pushed value.
//
// Side effects:
//   Allocates a new IStack instance that should be freed when done.
static IStack* IPush(FbleArena* arena, FbleInstr* instr, IStack* tail)
{
  assert(instr != NULL && "IPush NULL FbleInstr");
  IStack* istack = FbleAlloc(arena, IStack);
  istack->instr = instr;
  istack->tail = tail;
  return istack;
}

// IPop --
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
static IStack* IPop(FbleArena* arena, IStack* istack)
{
  IStack* tail = istack->tail;
  FbleFree(arena, istack);
  return tail;
}

// RunThread --
//   Run the given thread to completion or until it can no longer make
//   progress.
//
// Inputs:
//   arena - the arena to use for allocations.
//   thread - the thread to run.
//
// Results:
//   None.
//
// Side effects:
//   The thread is executed, updating its var_stack, data_stack, and istack.
static void RunThread(FbleValueArena* arena, Thread* thread)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  while (thread->iquota > 0 && thread->istack != NULL) {
    FbleInstr* instr = thread->istack->instr;
    thread->istack = IPop(arena_, thread->istack);

    switch (instr->tag) {
      case FBLE_COMPOUND_INSTR: {
        FbleCompoundInstr* compound_instr = (FbleCompoundInstr*)instr;
        for (size_t i = 0; i < compound_instr->instrs.size; ++i) {
          size_t j = compound_instr->instrs.size - 1 - i;
          thread->istack = IPush(arena_, compound_instr->instrs.xs[j], thread->istack);
        }
        break;
      }

      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_value_instr->argc;

        FbleValue* argv[argc];
        for (size_t i = 0; i < argc; ++i) {
          argv[argc - i - 1] = thread->data_stack->value;
          thread->data_stack = VPop(arena_, thread->data_stack);
        }

        FbleValueV args = { .size = argc, .xs = argv, };
        thread->data_stack = VPush(arena_, FbleNewStructValue(arena, &args), thread->data_stack);

        for (size_t i = 0; i < args.size; ++i) {
          FbleValueRelease(arena, argv[i]);
        }
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        FbleValue* arg = thread->data_stack->value;
        thread->data_stack = VPop(arena_, thread->data_stack);
        thread->data_stack = VPush(arena_, FbleNewUnionValue(arena, union_value_instr->tag, arg), thread->data_stack);
        FbleValueRelease(arena, arg);
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        assert(thread->data_stack != NULL);
        FbleStructValue* sv = (FbleStructValue*)Deref(thread->data_stack->value, FBLE_STRUCT_VALUE);
        assert(access_instr->tag < sv->fields.size);
        FbleValueRetain(arena, &sv->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = VPop(arena_, thread->data_stack);
        thread->data_stack = VPush(arena_, FbleValueRetain(arena, sv->fields.xs[access_instr->tag]), thread->data_stack);
        FbleValueRelease(arena, &sv->_base);
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        assert(thread->data_stack != NULL);
        FbleUnionValue* uv = (FbleUnionValue*)Deref(thread->data_stack->value, FBLE_UNION_VALUE);
        if (uv->tag != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);

          // Clean up the stacks.
          while (thread->var_stack != NULL) {
            FbleValueRelease(arena, thread->var_stack->value);
            thread->var_stack = VPop(arena_, thread->var_stack);
          }

          while (thread->data_stack != NULL) {
            FbleValueRelease(arena, thread->data_stack->value);
            thread->data_stack = VPop(arena_, thread->data_stack);
          }

          while (thread->istack != NULL) {
            thread->istack = IPop(arena_, thread->istack);
          }

          // Push a NULL value on top of the data_stack in place of the return
          // value.
          thread->data_stack = VPush(arena_, NULL, thread->data_stack);
          break;
        }

        FbleValueRetain(arena, &uv->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = VPop(arena_, thread->data_stack);
        thread->data_stack = VPush(arena_, FbleValueRetain(arena, uv->arg), thread->data_stack);

        FbleValueRelease(arena, &uv->_base);
        break;
      }

      case FBLE_COND_INSTR: {
        FbleCondInstr* cond_instr = (FbleCondInstr*)instr;
        assert(thread->data_stack != NULL);
        FbleUnionValue* uv = (FbleUnionValue*)Deref(thread->data_stack->value, FBLE_UNION_VALUE);
        FbleValueRetain(arena, &uv->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = VPop(arena_, thread->data_stack);
        assert(uv->tag < cond_instr->choices.size);
        thread->istack = IPush(arena_, cond_instr->choices.xs[uv->tag], thread->istack);
        FbleValueRelease(arena, &uv->_base);
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        FbleFuncValue* value = FbleAlloc(arena_, FbleFuncValue);
        FbleRefInit(arena, &value->_base.ref);
        value->_base.tag = FBLE_FUNC_VALUE;
        value->context = NULL;
        value->body = func_value_instr->body;
        value->body->refcount++;

        // TODO: This copies the entire lexical context, but really we should
        // only need to copy those variables that are used in the body of the
        // function. This has implications for performance and memory that
        // should be considered.
        FbleVStack* vs = thread->var_stack;
        for (size_t i = 0; i < func_value_instr->contextc; ++i) {
          assert(vs != NULL);
          value->context = VPush(arena_, vs->value, value->context);
          Add(arena, &value->_base, vs->value);
          vs = vs->tail;
        }

        thread->data_stack = VPush(arena_, &value->_base, thread->data_stack);
        break;
      }

      case FBLE_DESCOPE_INSTR: {
        FbleDescopeInstr* descope_instr = (FbleDescopeInstr*)instr;
        for (size_t i = 0; i < descope_instr->count; ++i) {
          assert(thread->var_stack != NULL);
          FbleValueRelease(arena, thread->var_stack->value);
          thread->var_stack = VPop(arena_, thread->var_stack);
        }
        break;
      }

      case FBLE_RELEASE_INSTR: {
        FbleValue* v = thread->data_stack->value;
        thread->data_stack = VPop(arena_, thread->data_stack);

        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = VPop(arena_, thread->data_stack);
        thread->data_stack = VPush(arena_, v, thread->data_stack);
        break;
      }

      case FBLE_FUNC_APPLY_INSTR: {
        FbleFuncApplyInstr* apply_instr = (FbleFuncApplyInstr*)instr;

        FbleValue* args[apply_instr->argc];
        for (size_t i = 0; i < apply_instr->argc; ++i) {
          args[apply_instr->argc - i - 1] = thread->data_stack->value;
          thread->data_stack = VPop(arena_, thread->data_stack);
        }

        FbleFuncValue* func = (FbleFuncValue*)Deref(thread->data_stack->value, FBLE_FUNC_VALUE);
        FbleValueRetain(arena, &func->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = VPop(arena_, thread->data_stack);

        // Push the function's context on top of the variable stack.
        for (FbleVStack* vs = func->context; vs != NULL; vs = vs->tail) {
          thread->var_stack = VPush(arena_, FbleValueRetain(arena, vs->value), thread->var_stack);
        }

        // Push the function args onto the variable stack.
        for (size_t i = 0; i < apply_instr->argc; ++i) {
          thread->var_stack = VPush(arena_, args[i], thread->var_stack);
        }

        // The func value owns the instructions we use to execute the body,
        // which means we need to ensure the func value is retained until
        // the body is done executing. To do so, keep the func value on the
        // data stack until after it's done executing. The body of the
        // function will take care of releasing this.
        thread->data_stack = VPush(arena_, &func->_base, thread->data_stack);

        thread->istack = IPush(arena_, func->body, thread->istack);
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
        thread->data_stack = VPop(arena_, thread->data_stack);
        thread->data_stack = VPush(arena_, &value->_base._base, thread->data_stack);
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
        thread->data_stack = VPop(arena_, thread->data_stack);
        value->port = thread->data_stack->value;
        Add(arena, &value->_base._base, value->port);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = VPop(arena_, thread->data_stack);
        thread->data_stack = VPush(arena_, &value->_base._base, thread->data_stack);
        break;
      }

      case FBLE_EVAL_INSTR: {
        FbleEvalProcValue* proc_value = FbleAlloc(arena_, FbleEvalProcValue);
        FbleRefInit(arena, &proc_value->_base._base.ref);
        proc_value->_base._base.tag = FBLE_PROC_VALUE;
        proc_value->_base.tag = FBLE_EVAL_PROC_VALUE;
        proc_value->result = thread->data_stack->value;
        thread->data_stack = VPop(arena_, thread->data_stack);
        thread->data_stack = VPush(arena_, &proc_value->_base._base, thread->data_stack);
        break;
      }

      case FBLE_VAR_INSTR: {
        FbleVarInstr* var_instr = (FbleVarInstr*)instr;
        FbleVStack* v = thread->var_stack;
        for (size_t i = 0; i < var_instr->position; ++i) {
          assert(v->tail != NULL);
          v = v->tail;
        }
        thread->data_stack = VPush(arena_, FbleValueRetain(arena, v->value), thread->data_stack);
        break;
      }

      case FBLE_LINK_INSTR: {
        FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
        FbleLinkProcValue* value = FbleAlloc(arena_, FbleLinkProcValue);
        FbleRefInit(arena, &value->_base._base.ref);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base.tag = FBLE_LINK_PROC_VALUE;
        value->context = NULL;
        value->body = link_instr->body;
        value->body->refcount++;

        // TODO: This copies the entire lexical context, but really we should only
        // need to copy those variables that are used in the body of the
        // link process. This has implications for performance and memory that
        // should be considered.
        FbleVStack* vs = thread->var_stack;
        for (size_t i = 0; i < link_instr->contextc; ++i) {
          assert(vs != NULL);
          value->context = VPush(arena_, vs->value, value->context);
          Add(arena, &value->_base._base, vs->value);
          vs = vs->tail;
        }

        thread->data_stack = VPush(arena_, &value->_base._base, thread->data_stack);
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
        value->context = NULL;
        value->body = exec_instr->body;
        value->body->refcount++;

        // TODO: This copies the entire lexical context, but really we should
        // only need to copy those variables that are used in the body of the
        // exec process. This has implications for performance and memory that
        // should be considered.
        FbleVStack* vs = thread->var_stack;
        for (size_t i = 0; i < exec_instr->contextc; ++i) {
          assert(vs != NULL);
          value->context = VPush(arena_, vs->value, value->context);
          Add(arena, &value->_base._base, vs->value);
          vs = vs->tail;
        }

        for (size_t i = 0; i < exec_instr->argc; ++i) {
          size_t j = exec_instr->argc - 1 - i;
          value->bindings.xs[j] = thread->data_stack->value;
          Add(arena, &value->_base._base, thread->data_stack->value);
          FbleValueRelease(arena, thread->data_stack->value);
          thread->data_stack = VPop(arena_, thread->data_stack);
        }

        thread->data_stack = VPush(arena_, &value->_base._base, thread->data_stack);
        break;
      }

      case FBLE_JOIN_INSTR: {
        thread->var_stack = VPush(arena_, thread->data_stack->value, thread->var_stack);
        thread->data_stack = VPop(arena_, thread->data_stack);
        break;
      }

      case FBLE_PROC_INSTR: {
        FbleProcValue* proc = (FbleProcValue*)Deref(thread->data_stack->value, FBLE_PROC_VALUE);
        FbleValueRetain(arena, &proc->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = VPop(arena_, thread->data_stack);

        switch (proc->tag) {
          case FBLE_GET_PROC_VALUE: {
            FbleGetProcValue* get = (FbleGetProcValue*)proc;
            FbleInputValue* port = (FbleInputValue*)get->port;
            assert(port->_base.tag == FBLE_INPUT_VALUE);

            if (port->head == NULL) {
              // Blocked on get.
              return;
            }

            FbleValues* head = port->head;
            port->head = port->head->next;
            if (port->head == NULL) {
              port->tail = NULL;
            }
            thread->data_stack = VPush(arena_, head->value, thread->data_stack);
            FbleFree(arena_, head);
            break;
          }

          case FBLE_PUT_PROC_VALUE: {
            FblePutProcValue* put = (FblePutProcValue*)proc;
            FbleOutputValue* port = (FbleOutputValue*)put->port;
            assert(port->_base.tag == FBLE_OUTPUT_VALUE);

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

            thread->data_stack = VPush(arena_, FbleValueRetain(arena, put->arg), thread->data_stack);
            break;
          }

          case FBLE_EVAL_PROC_VALUE: {
            FbleEvalProcValue* eval = (FbleEvalProcValue*)proc;
            thread->data_stack = VPush(arena_, FbleValueRetain(arena, eval->result), thread->data_stack);
            break;
          }

          case FBLE_LINK_PROC_VALUE: {
            FbleLinkProcValue* link = (FbleLinkProcValue*)proc;

            // Push the body's context on top of the variable stack.
            for (FbleVStack* vs = link->context; vs != NULL; vs = vs->tail) {
              thread->var_stack = VPush(arena_, FbleValueRetain(arena, vs->value), thread->var_stack);
            }

            // Allocate the link and push the ports on top of the variable stack.
            FbleInputValue* get = FbleAlloc(arena_, FbleInputValue);
            FbleRefInit(arena, &get->_base.ref);
            get->_base.tag = FBLE_INPUT_VALUE;
            get->head = NULL;
            get->tail = NULL;
            thread->var_stack = VPush(arena_, &get->_base, thread->var_stack);

            FbleOutputValue* put = FbleAlloc(arena_, FbleOutputValue);
            FbleRefInit(arena, &put->_base.ref);
            put->_base.tag = FBLE_OUTPUT_VALUE;
            put->dest = get;
            Add(arena, &put->_base, &get->_base);
            thread->var_stack = VPush(arena_, &put->_base, thread->var_stack);

            // The link proc value owns the instruction it uses to execute,
            // which means we need to ensure the proc value is retained
            // until we are done executing. To do so, keep the proc value on
            // the data stack until after it's done executing.
            thread->data_stack = VPush(arena_, FbleValueRetain(arena, &proc->_base), thread->data_stack);

            thread->istack = IPush(arena_, link->body, thread->istack);
            break;
          }

          case FBLE_EXEC_PROC_VALUE: {
            FbleExecProcValue* exec = (FbleExecProcValue*)proc;

            // Push the body's context on top of the variable stack.
            for (FbleVStack* vs = exec->context; vs != NULL; vs = vs->tail) {
              thread->var_stack = VPush(arena_, FbleValueRetain(arena, vs->value), thread->var_stack);
            }

            assert(exec->bindings.size == 1 && "TODO: Support multi-binding exec");

            // The exec proc value owns the instruction it uses to execute,
            // which means we need to ensure the proc value is retained
            // until we are done executing. To do so, keep the proc value on
            // the data stack until after it's done executing.
            thread->data_stack = VPush(arena_, FbleValueRetain(arena, &proc->_base), thread->data_stack);

            // Push the argument proc value on the stack in preparation for
            // execution.
            thread->data_stack = VPush(arena_, FbleValueRetain(arena, exec->bindings.xs[0]), thread->data_stack);

            thread->istack = IPush(arena_, exec->body, thread->istack);
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
          thread->var_stack = VPush(arena_, &rv->_base, thread->var_stack);
        }
        break;
      }

      case FBLE_LET_DEF_INSTR: {
        FbleLetDefInstr* let_def_instr = (FbleLetDefInstr*)instr;
        FbleVStack* vs = thread->var_stack;
        for (size_t i = 0; i < let_def_instr->count; ++i) {
          assert(vs != NULL);
          FbleRefValue* rv = (FbleRefValue*) vs->value;
          assert(rv->_base.tag == FBLE_REF_VALUE);

          rv->value = thread->data_stack->value;
          Add(arena, &rv->_base, thread->data_stack->value);
          FbleValueRelease(arena, thread->data_stack->value);
          thread->data_stack = VPop(arena_, thread->data_stack);

          assert(rv->value != NULL);
          vs->value = FbleValueRetain(arena, rv->value);
          FbleValueRelease(arena, &rv->_base);

          vs = vs->tail;
        }
        break;
      }
    }
    thread->iquota--;
  }
}

// RunThreads --
//   Run the given thread and its children to completion or until it can no
//   longer make progress.
//
// Inputs:
//   arena - the arena to use for allocations.
//   thread - the thread to run.
//
// Results:
//   None.
//
// Side effects:
//   The thread and its children are executed and updated.
static void RunThreads(FbleValueArena* arena, Thread* thread)
{
  // TODO: Make this iterative instead of recursive to avoid smashing the
  // stack.

  // Spend some time running children threads first.
  for (size_t i = 0; i < thread->children.size; ++i) {
    size_t iquota = thread->iquota / (thread->children.size - i);
    Thread* child = thread->children.xs[i];
    thread->iquota -= iquota;
    child->iquota += iquota;
    RunThread(arena, child);
    thread->iquota += child->iquota;
    child->iquota = 0;
  }

  // Spend the remaining time running this thread.
  RunThread(arena, thread);
}

// Eval --
//   Execute the given sequence of instructions to completion.
//
// Inputs:
//   arena - the arena to use for allocations.
//   instrs - the instructions to evaluate.
//   arg - an optional initial argument to place on the stack.
//         Note: ideally this is an arbitrary list of arguments to prepopulate
//         the value stack with, but currently it is only used to implement
//         FbleExec, which passes a single ProcValue argument, and it doesn't
//         seem worth the hassle to pass in a list.
//
// Results:
//   The computed value, or NULL on error.
//
// Side effects:
//   The returned value must be freed with FbleValueRelease when no longer in
//   use. Prints a message to stderr in case of error.
static FbleValue* Eval(FbleValueArena* arena, FbleInstr* prgm, FbleValue* arg)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  Thread thread = {
    .var_stack = NULL,
    .data_stack = NULL,
    .istack = IPush(arena_, prgm, NULL),
    .iquota = 0,
  };
  FbleVectorInit(arena_, thread.children);

  if (arg != NULL) {
    thread.data_stack = VPush(arena_, FbleValueRetain(arena, arg), NULL);
  }

  // Run the main thread repeatedly until it no longer makes any forward
  // progress.
  do {
    thread.iquota = 1024;
    RunThreads(arena, &thread);
  } while (thread.iquota < 1024);

  if (thread.istack != NULL) {
    // We have instructions to run still, but we stopped making forward
    // progress. This must be a deadlock.
    // TODO: Handle this more gracefully.
    fprintf(stderr, "Deadlock detected\n");
    abort();
    return NULL;
  }

  // The thread should now be finished with a single value on the data stack
  // which is the value to return.
  assert(thread.var_stack == NULL);
  assert(thread.data_stack != NULL);
  FbleValue* final_result = thread.data_stack->value;
  thread.data_stack = VPop(arena_, thread.data_stack);
  assert(thread.data_stack == NULL);
  assert(thread.istack == NULL);
  FbleFree(arena_, thread.children.xs);
  return final_result;
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleValueArena* arena, FbleExpr* expr)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleInstr* instrs = FbleCompile(arena_, expr);
  if (instrs == NULL) {
    return NULL;
  }

  FbleValue* result = Eval(arena, instrs, NULL);
  FbleFreeInstrs(arena_, instrs);
  return result;
}
// FbleExec -- see documentation in fble.h
FbleValue* FbleExec(FbleValueArena* arena, FbleProcValue* proc)
{
  FbleProcInstr instr = {
    ._base = {
      .tag = FBLE_PROC_INSTR,
      .refcount = 1
    },
  };

  FbleValue* result = Eval(arena, &instr._base, &proc->_base);
  return result;
}
