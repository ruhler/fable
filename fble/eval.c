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
//   retain - A value to retain for the duration of the stack frame. May be NULL.
//   instrs - A vector of instructions in the currently executing block.
//   pc - The location of the next instruction in the current block to execute.
typedef struct IStack {
  FbleValue* retain;
  FbleInstrV instrs;
  size_t pc;
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

static FbleInstr g_proc_instr = { .tag = FBLE_PROC_INSTR };
static FbleInstr g_ipop_instr = { .tag = FBLE_IPOP_INSTR };
static FbleInstr* g_proc_block_instrs[] = { &g_proc_instr, &g_ipop_instr };
static FbleInstrBlock g_proc_block = {
  .refcount = 1,
  .instrs = { .size = 2, .xs = g_proc_block_instrs }
};

static void Add(FbleRefArena* arena, FbleValue* src, FbleValue* dst);

static FbleValue* Deref(FbleValue* value, FbleValueTag tag);
static FbleVStack* VPush(FbleArena* arena, FbleValue* value, FbleVStack* tail);
static FbleVStack* VPop(FbleArena* arena, FbleVStack* vstack);
static IStack* IPush(FbleArena* arena, FbleValue* retain, FbleInstrBlock* block, IStack* tail); 
static IStack* IPop(FbleValueArena* arena, IStack* istack);

static void RunThread(FbleValueArena* arena, FbleIO* io, Thread* thread);
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
//   Push a block of instructions onto an instruction stack.
//
// Inputs:
//   arena - the arena to use for allocations
//   retain - an optional value to retain for the duration of the stack frame.
//   block - the block of instructions to push
//   tail - the stack to push to
//
// Result:
//   The new stack with pushed instructions.
//
// Side effects:
//   Allocates new IStack instances that should be freed when done. Takes over
//   ownership of the passed 'retain' value.
static IStack* IPush(FbleArena* arena, FbleValue* retain, FbleInstrBlock* block, IStack* tail)
{
  // For debugging purposes, double check that all blocks will pop themselves
  // when done.
  assert(block->instrs.size > 0);
  assert(block->instrs.xs[block->instrs.size - 1]->tag == FBLE_IPOP_INSTR);

  IStack* istack = FbleAlloc(arena, IStack);
  istack->retain = retain;
  istack->instrs = block->instrs;
  istack->pc = 0;
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
static IStack* IPop(FbleValueArena* arena, IStack* istack)
{
  FbleValueRelease(arena, istack->retain);
  IStack* tail = istack->tail;
  FbleFree(FbleRefArenaArena(arena), istack);
  return tail;
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
//   The thread is executed, updating its var_stack, data_stack, and istack.
static void RunThread(FbleValueArena* arena, FbleIO* io, Thread* thread)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  while (thread->iquota > 0 && thread->istack != NULL) {
    assert(thread->istack->pc < thread->istack->instrs.size);
    FbleInstr* instr = thread->istack->instrs.xs[thread->istack->pc++];
    switch (instr->tag) {
      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_value_instr->argc;

        FbleValue* argv[argc];
        for (size_t i = 0; i < argc; ++i) {
          argv[argc - i - 1] = thread->data_stack->value;
          thread->data_stack = VPop(arena_, thread->data_stack);
        }

        FbleValueV args = { .size = argc, .xs = argv, };
        thread->data_stack = VPush(arena_, FbleNewStructValue(arena, args), thread->data_stack);
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        FbleValue* arg = thread->data_stack->value;
        thread->data_stack = VPop(arena_, thread->data_stack);
        thread->data_stack = VPush(arena_, FbleNewUnionValue(arena, union_value_instr->tag, arg), thread->data_stack);
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
            thread->istack = IPop(arena, thread->istack);
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
        thread->istack = IPush(arena_, NULL, cond_instr->choices.xs[uv->tag], thread->istack);
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

      case FBLE_FUNC_APPLY_INSTR: {
        FbleValue* arg = thread->data_stack->value;
        thread->data_stack = VPop(arena_, thread->data_stack);

        FbleFuncValue* func = (FbleFuncValue*)Deref(thread->data_stack->value, FBLE_FUNC_VALUE);
        FbleValueRetain(arena, &func->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = VPop(arena_, thread->data_stack);

        // Push the function's context on top of the variable stack.
        for (FbleVStack* vs = func->context; vs != NULL; vs = vs->tail) {
          thread->var_stack = VPush(arena_, FbleValueRetain(arena, vs->value), thread->var_stack);
        }

        // Push the function arg onto the variable stack.
        thread->var_stack = VPush(arena_, arg, thread->var_stack);

        thread->istack = IPush(arena_, &func->_base, func->body, thread->istack);
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
        assert(thread->children.size > 0);
        for (size_t i = 0; i < thread->children.size; ++i) {
          if (thread->children.xs[i]->istack != NULL) {
            // Blocked on child. Restore the thread state and return before
            // iquota has been decremented.
            thread->istack->pc--;
            return;
          }
        }
        
        for (size_t i = 0; i < thread->children.size; ++i) {
          Thread* child = thread->children.xs[i];
          assert(child->data_stack != NULL);
          FbleValue* result = child->data_stack->value;
          child->data_stack = VPop(arena_, child->data_stack);
          assert(child->data_stack == NULL);
          assert(result != NULL && "TODO: Propagate error from child?");
          thread->var_stack = VPush(arena_, result, thread->var_stack);
          assert(child->istack == NULL);
          assert(child->iquota == 0);
          FbleFree(arena_, child->children.xs);
          FbleFree(arena_, child);
        }

        thread->children.size = 0;
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

            if (get->port->tag == FBLE_INPUT_VALUE) {
              FbleInputValue* port = (FbleInputValue*)get->port;

              if (port->head == NULL) {
                // Blocked on get. Restore the thread state and return before
                // iquota has been decremented.
                thread->data_stack = VPush(arena_, &proc->_base, thread->data_stack);
                thread->istack->pc--;
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

            if (get->port->tag == FBLE_PORT_VALUE) {
              FblePortValue* port = (FblePortValue*)get->port;
              assert(port->id < io->ports.size);
              if (io->ports.xs[port->id] == NULL) {
                // Blocked on get. Restore the thread state and return before
                // iquota has been decremented.
                thread->data_stack = VPush(arena_, &proc->_base, thread->data_stack);
                thread->istack->pc--;
                return;
              }

              thread->data_stack = VPush(arena_, io->ports.xs[port->id], thread->data_stack);
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

              thread->data_stack = VPush(arena_, FbleValueRetain(arena, put->arg), thread->data_stack);
              break;
            }

            if (put->port->tag == FBLE_PORT_VALUE) {
              FblePortValue* port = (FblePortValue*)put->port;
              assert(port->id < io->ports.size);

              if (io->ports.xs[port->id] != NULL) {
                // Blocked on put. Restore the thread state and return before
                // iquota has been decremented.
                thread->data_stack = VPush(arena_, &proc->_base, thread->data_stack);
                thread->istack->pc--;
                return;
              }

              io->ports.xs[port->id] = FbleValueRetain(arena, put->arg);
              thread->data_stack = VPush(arena_, FbleValueRetain(arena, put->arg), thread->data_stack);
              break;
            }

            UNREACHABLE("put port must be an output or port value");
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
            thread->istack = IPush(arena_, FbleValueRetain(arena, &link->_base._base), link->body, thread->istack);
            break;
          }

          case FBLE_EXEC_PROC_VALUE: {
            FbleExecProcValue* exec = (FbleExecProcValue*)proc;

            // Push the body's context on top of the variable stack.
            for (FbleVStack* vs = exec->context; vs != NULL; vs = vs->tail) {
              thread->var_stack = VPush(arena_, FbleValueRetain(arena, vs->value), thread->var_stack);
            }

            // Set up child threads.
            for (size_t i = 0; i < exec->bindings.size; ++i) {
              Thread* child = FbleAlloc(arena_, Thread);
              child->var_stack = thread->var_stack;
              child->data_stack = VPush(arena_, FbleValueRetain(arena, exec->bindings.xs[i]), NULL);
              child->istack = IPush(arena_, NULL, &g_proc_block, NULL);
              child->iquota = 0;
              FbleVectorInit(arena_, child->children);
              FbleVectorAppend(arena_, thread->children, child);
            }

            thread->istack = IPush(arena_, FbleValueRetain(arena, &proc->_base), exec->body, thread->istack);
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

      case FBLE_NAMESPACE_INSTR: {
        FbleStructValue* sv = (FbleStructValue*)Deref(thread->data_stack->value, FBLE_STRUCT_VALUE);
        FbleValueRetain(arena, &sv->_base);
        FbleValueRelease(arena, thread->data_stack->value);
        thread->data_stack = VPop(arena_, thread->data_stack);
        for (size_t i = 0; i < sv->fields.size; ++i) {
          thread->var_stack = VPush(arena_, FbleValueRetain(arena, sv->fields.xs[i]), thread->var_stack);
        }
        FbleValueRelease(arena, &sv->_base);
        break;
      }

      case FBLE_IPOP_INSTR: {
        thread->istack = IPop(arena, thread->istack);
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
    .var_stack = NULL,
    .data_stack = NULL,
    .istack = IPush(arena_, NULL, prgm, NULL),
    .iquota = 0,
  };
  FbleVectorInit(arena_, thread.children);

  for (size_t i = 0; i < args.size; ++i) {
    thread.data_stack = VPush(arena_, FbleValueRetain(arena, args.xs[i]), thread.data_stack);
  }

  // Run the main thread repeatedly until it no longer makes any forward
  // progress.
  size_t QUOTA = 1024;
  bool did_io = false;
  do {
    thread.iquota = QUOTA;
    RunThreads(arena, io, &thread);

    bool block = (thread.iquota == QUOTA) && (thread.istack != NULL);
    did_io = io->io(io, arena, block);
  } while (did_io || thread.iquota < QUOTA);

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
