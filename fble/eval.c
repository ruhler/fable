// eval.c --
//   This file implements the fble evaluation routines.

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

// ThreadStack --
//   The computation context for a thread.
// 
// Fields:
//   result - Pointer to where the result of evaluating an expression should
//            be stored.
//   instr -  The instructions for executing the expression.
typedef struct ThreadStack {
  FbleValue** result;
  FbleInstr* instr;
  struct ThreadStack* tail;
} ThreadStack;

static FbleValue* Deref(FbleValue* value, FbleValueTag tag);
static FbleVStack* VPush(FbleArena* arena, FbleValue* value, FbleVStack* tail);
static FbleVStack* VPop(FbleArena* arena, FbleVStack* vstack);
static ThreadStack* TPush(FbleArena* arena, FbleValue** presult, FbleInstr* instr, ThreadStack* tail); 

static FbleValue* Eval(FbleArena* arena, FbleInstr* prgm, FbleValue* arg);


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

// TPush --
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

// TPush --
//   Push an element onto a thread stack.
//
// Inputs:
//   arena - the arena to use for allocations
//   presult - the result pointer to push
//   instr - the instr to push
//   tail - the stack to push to
//
// Result:
//   The new stack with pushed value.
//
// Side effects:
//   Allocates a new ThreadStack instance that should be freed when done.
static ThreadStack* TPush(FbleArena* arena, FbleValue** presult, FbleInstr* instr, ThreadStack* tail)
{
  assert(instr != NULL && "TPush NULL FbleInstr");
  ThreadStack* tstack = FbleAlloc(arena, ThreadStack);
  tstack->result = presult;
  tstack->instr = instr;
  tstack->tail = tail;
  return tstack;
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
//   The returned value must be freed with FbleDropStrongRef when no longer in
//   use. Prints a message to stderr in case of error.
static FbleValue* Eval(FbleArena* arena, FbleInstr* prgm, FbleValue* arg)
{
  FbleVStack* vstack = NULL;
  if (arg != NULL) {
    vstack = VPush(arena, FbleTakeStrongRef(arg), vstack);
  }
    
  FbleValue* final_result = NULL;
  ThreadStack* tstack = TPush(arena, &final_result, prgm, NULL);

  while (tstack != NULL) {
    FbleValue** presult = tstack->result;
    FbleInstr* instr = tstack->instr;
    ThreadStack* tstack_done = tstack;
    tstack = tstack->tail;
    FbleFree(arena, tstack_done);

    switch (instr->tag) {
      case FBLE_VAR_INSTR: {
        FbleVarInstr* var_instr = (FbleVarInstr*)instr;
        FbleVStack* v = vstack;
        for (size_t i = 0; i < var_instr->position; ++i) {
          assert(v->tail != NULL);
          v = v->tail;
        }
        *presult = FbleTakeStrongRef(v->value);
        break;
      }

      case FBLE_LET_INSTR: {
        FbleLetInstr* let_instr = (FbleLetInstr*)instr;

        tstack = TPush(arena, NULL, &let_instr->pop._base, tstack);
        tstack = TPush(arena, presult, let_instr->body, tstack);
        tstack = TPush(arena, NULL, &let_instr->break_cycle._base, tstack);

        FbleRefValue* first = NULL;
        FbleRefValue* curr = NULL;
        for (size_t i = 0; i < let_instr->bindings.size; ++i) {
          FbleRefValue* rv = FbleAlloc(arena, FbleRefValue);
          rv->_base.tag = FBLE_REF_VALUE;
          rv->_base.strong_ref_count = 1;
          rv->_base.break_cycle_ref_count = 0;
          rv->value = NULL;
          rv->broke_cycle = false;
          rv->siblings = curr;
          vstack = VPush(arena, &rv->_base, vstack);
          tstack = TPush(arena, &rv->value, let_instr->bindings.xs[i], tstack);

          if (first == NULL) {
            first = rv;
          }
          curr = rv;
        }
        assert(first != NULL);
        first->siblings = curr;
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        FbleFuncValue* value = FbleAlloc(arena, FbleFuncValue);
        value->_base.tag = FBLE_FUNC_VALUE;
        value->_base.strong_ref_count = 1;
        value->_base.break_cycle_ref_count = 0;
        value->context = NULL;
        value->body = func_value_instr->body;
        value->body->refcount++;
        value->pop._base.tag = FBLE_POP_INSTR;
        value->pop._base.refcount = 1;
        value->pop.count = 1 + func_value_instr->argc;
        *presult = &value->_base;

        // TODO: This copies the entire context, but really we should only
        // need to copy those variables that are used in the body of the
        // function. This has implications for performance and memory that
        // should be considered.
        for (FbleVStack* vs = vstack; vs != NULL;  vs = vs->tail) {
          value->context = VPush(arena, FbleTakeStrongRef(vs->value), value->context);
          value->pop.count++;
        }
        break;
      }

      case FBLE_FUNC_APPLY_INSTR: {
        FbleFuncApplyInstr* apply_instr = (FbleFuncApplyInstr*)instr;
        FbleValue* args[apply_instr->argc];
        for (size_t i = 0; i < apply_instr->argc; ++i) {
          args[i] = vstack->value;
          vstack = VPop(arena, vstack);
        }

        FbleFuncValue* func = (FbleFuncValue*)Deref(vstack->value, FBLE_FUNC_VALUE);

        // Push the function's context on top of the value stack.
        for (FbleVStack* vs = func->context; vs != NULL; vs = vs->tail) {
          vstack = VPush(arena, FbleTakeStrongRef(vs->value), vstack);
        }

        // Push the function args on top of the value stack.
        for (size_t i = 0; i < apply_instr->argc; ++i) {
          size_t j = apply_instr->argc - 1 - i;
          vstack = VPush(arena, args[j], vstack);
        }

        tstack = TPush(arena, NULL, &func->pop._base, tstack);
        tstack = TPush(arena, presult, func->body, tstack);
        break;
      }

      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;

        FbleStructValue* value = FbleAlloc(arena, FbleStructValue);
        value->_base.tag = FBLE_STRUCT_VALUE;
        value->_base.strong_ref_count = 1;
        value->_base.break_cycle_ref_count = 0;
        value->fields.size = struct_value_instr->argc;
        value->fields.xs = FbleArenaAlloc(arena, value->fields.size * sizeof(FbleValue*), FbleAllocMsg(__FILE__, __LINE__));
        *presult = &value->_base;

        for (size_t i = 0; i < struct_value_instr->argc; ++i) {
          value->fields.xs[value->fields.size - i - 1] = vstack->value;
          vstack = VPop(arena, vstack);
        }
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        assert(vstack != NULL);
        FbleStructValue* sv = (FbleStructValue*)Deref(vstack->value, FBLE_STRUCT_VALUE);
        assert(access_instr->tag < sv->fields.size);
        *presult = FbleTakeStrongRef(sv->fields.xs[access_instr->tag]);
        FbleDropStrongRef(arena, vstack->value);
        vstack = VPop(arena, vstack);
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        FbleUnionValue* union_value = FbleAlloc(arena, FbleUnionValue);
        union_value->_base.tag = FBLE_UNION_VALUE;
        union_value->_base.strong_ref_count = 1;
        union_value->_base.break_cycle_ref_count = 0;
        union_value->tag = union_value_instr->tag;
        union_value->arg = NULL;

        *presult = &union_value->_base;

        tstack = TPush(arena, &union_value->arg, union_value_instr->mkarg, tstack);
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        assert(vstack != NULL);
        FbleUnionValue* uv = (FbleUnionValue*)Deref(vstack->value, FBLE_UNION_VALUE);
        if (uv->tag != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);

          // Clean up the stacks.
          while (vstack != NULL) {
            FbleDropStrongRef(arena, vstack->value);
            vstack = VPop(arena, vstack);
          }

          while (tstack != NULL) {
            ThreadStack* otstack = tstack;
            tstack = tstack->tail;
            FbleFree(arena, otstack);
          }

          FbleDropStrongRef(arena, final_result);
          return NULL;
        }
        *presult = FbleTakeStrongRef(uv->arg);

        FbleDropStrongRef(arena, vstack->value);
        vstack = VPop(arena, vstack);
        break;
      }

      case FBLE_GET_INSTR: {
        FbleGetProcValue* value = FbleAlloc(arena, FbleGetProcValue);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base._base.strong_ref_count = 1;
        value->_base._base.break_cycle_ref_count = 0;
        value->_base.tag = FBLE_GET_PROC_VALUE;

        value->port = vstack->value;
        vstack = VPop(arena, vstack);

        *presult = &value->_base._base;
        break;
      }

      case FBLE_PUT_INSTR: {
        FblePutProcValue* value = FbleAlloc(arena, FblePutProcValue);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base._base.strong_ref_count = 1;
        value->_base._base.break_cycle_ref_count = 0;
        value->_base.tag = FBLE_PUT_PROC_VALUE;

        value->arg = vstack->value;
        vstack = VPop(arena, vstack);

        value->port = vstack->value;
        vstack = VPop(arena, vstack);

        *presult = &value->_base._base;
        break;
      }

      case FBLE_EVAL_INSTR: {
        FbleEvalInstr* proc_eval_instr = (FbleEvalInstr*)instr;
        FbleEvalProcValue* proc_value = FbleAlloc(arena, FbleEvalProcValue);
        proc_value->_base._base.tag = FBLE_PROC_VALUE;
        proc_value->_base._base.strong_ref_count = 1;
        proc_value->_base._base.break_cycle_ref_count = 0;
        proc_value->_base.tag = FBLE_EVAL_PROC_VALUE;
        proc_value->result = NULL;

        *presult = &proc_value->_base._base;

        tstack = TPush(arena, &proc_value->result, proc_eval_instr->body, tstack);
        break;
      }

      case FBLE_LINK_INSTR: {
        FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;
        FbleLinkProcValue* value = FbleAlloc(arena, FbleLinkProcValue);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base._base.strong_ref_count = 1;
        value->_base._base.break_cycle_ref_count = 0;
        value->_base.tag = FBLE_LINK_PROC_VALUE;
        value->context = NULL;
        value->body = link_instr->body;
        value->body->refcount++;
        value->pop._base.tag = FBLE_POP_INSTR;
        value->pop._base.refcount = 1;
        value->pop.count = 2;
        value->proc._base.tag = FBLE_PROC_INSTR;
        value->proc._base.refcount = 1;
        value->proc.pop._base.tag = FBLE_POP_INSTR;
        value->proc.pop._base.refcount = 1;
        value->proc.pop.count = 1;
        *presult = &value->_base._base;

        // TODO: This copies the entire context, but really we should only
        // need to copy those variables that are used in the body of the
        // link process. This has implications for performance and memory that
        // should be considered.
        for (FbleVStack* vs = vstack; vs != NULL;  vs = vs->tail) {
          value->context = VPush(arena, FbleTakeStrongRef(vs->value), value->context);
          value->pop.count++;
        }
        break;
      }

      case FBLE_EXEC_INSTR: {
        FbleExecInstr* exec_instr = (FbleExecInstr*)instr;
        FbleExecProcValue* value = FbleAlloc(arena, FbleExecProcValue);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base._base.strong_ref_count = 1;
        value->_base._base.break_cycle_ref_count = 0;
        value->_base.tag = FBLE_EXEC_PROC_VALUE;
        value->bindings.size = exec_instr->bindings.size;
        value->bindings.xs = FbleArenaAlloc(arena, value->bindings.size * sizeof(FbleValue*), FbleAllocMsg(__FILE__, __LINE__));
        value->context = NULL;
        value->body = exec_instr->body;
        value->body->refcount++;
        value->pop._base.tag = FBLE_POP_INSTR;
        value->pop._base.refcount = 1;
        value->pop.count = exec_instr->bindings.size;
        value->proc._base.tag = FBLE_PROC_INSTR;
        value->proc._base.refcount = 1;
        value->proc.pop._base.tag = FBLE_POP_INSTR;
        value->proc.pop._base.refcount = 1;
        value->proc.pop.count = 1;

        // TODO: This copies the entire context, but really we should only
        // need to copy those variables that are used in the body of the
        // exec process. This has implications for performance and memory that
        // should be considered.
        for (FbleVStack* vs = vstack; vs != NULL;  vs = vs->tail) {
          value->context = VPush(arena, FbleTakeStrongRef(vs->value), value->context);
          value->pop.count++;
        }

        for (size_t i = 0; i < exec_instr->bindings.size; ++i) {
          tstack = TPush(arena, value->bindings.xs + i, exec_instr->bindings.xs[i], tstack);
        }

        // Update the result after capturing the context so that we don't grab
        // a reference to ourself when we copy the context.
        // TODO: Shouldn't reference counting take care of this?
        *presult = &value->_base._base;
        break;
      }

      case FBLE_PROC_INSTR: {
        FbleProcInstr* proc_instr = (FbleProcInstr*)instr;
        FbleProcValue* proc = (FbleProcValue*)Deref(vstack->value, FBLE_PROC_VALUE);

        // Some proc values own the instructions they use to execute, which
        // means we need to ensure the proc values are retained until they are
        // done executing. To do so, simply keep the proc value on the value
        // stack until after it's done executing.
        tstack = TPush(arena, NULL, &proc_instr->pop._base, tstack);

        switch (proc->tag) {
          case FBLE_GET_PROC_VALUE: {
            FbleGetProcValue* get = (FbleGetProcValue*)proc;
            FbleInputValue* port = (FbleInputValue*)get->port;
            assert(port->_base.tag == FBLE_INPUT_VALUE);
            assert(port->head != NULL && "TODO: Block on empty link port");
            FbleValues* head = port->head;
            port->head = port->head->next;
            if (port->head == NULL) {
              port->tail = NULL;
            }
            *presult = head->value;
            FbleFree(arena, head);
            break;
          }

          case FBLE_PUT_PROC_VALUE: {
            FblePutProcValue* put = (FblePutProcValue*)proc;
            FbleOutputValue* port = (FbleOutputValue*)put->port;
            assert(port->_base.tag == FBLE_OUTPUT_VALUE);

            FbleValues* tail = FbleAlloc(arena, FbleValues);
            tail->value = FbleTakeStrongRef(put->arg);
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

            *presult = FbleTakeStrongRef(put->arg);
            break;
          }

          case FBLE_EVAL_PROC_VALUE: {
            FbleEvalProcValue* eval = (FbleEvalProcValue*)proc;
            *presult = FbleTakeStrongRef(eval->result);
            break;
          }

          case FBLE_LINK_PROC_VALUE: {
            FbleLinkProcValue* link = (FbleLinkProcValue*)proc;

            // Reserve a slot on the value stack for the link body ProcValue.
            vstack = VPush(arena, NULL, vstack);
            FbleValue** body = &vstack->value;

            // Push the body's context on top of the value stack.
            for (FbleVStack* vs = link->context; vs != NULL; vs = vs->tail) {
              vstack = VPush(arena, FbleTakeStrongRef(vs->value), vstack);
            }

            // Allocate the link and push the ports on top of the value stack.
            FbleInputValue* get = FbleAlloc(arena, FbleInputValue);
            get->_base.tag = FBLE_INPUT_VALUE;
            get->_base.strong_ref_count = 2;
            get->_base.break_cycle_ref_count = 0;
            get->head = NULL;
            get->tail = NULL;
            vstack = VPush(arena, &get->_base, vstack);

            FbleOutputValue* put = FbleAlloc(arena, FbleOutputValue);
            put->_base.tag = FBLE_OUTPUT_VALUE;
            put->_base.strong_ref_count = 1;
            put->_base.break_cycle_ref_count = 0;
            put->dest = get;
            vstack = VPush(arena, &put->_base, vstack);

            // Set up the thread stack to finish execution.
            tstack = TPush(arena, presult, &link->proc._base, tstack);
            tstack = TPush(arena, NULL, &link->pop._base, tstack);
            tstack = TPush(arena, body, link->body, tstack);
            break;
          }

          case FBLE_EXEC_PROC_VALUE: {
            FbleExecProcValue* exec = (FbleExecProcValue*)proc;

            // Reserve a slot on the value stack for the exec body ProcValue.
            vstack = VPush(arena, NULL, vstack);
            FbleValue** body = &vstack->value;

            // Push the body's context on top of the value stack.
            for (FbleVStack* vs = exec->context; vs != NULL; vs = vs->tail) {
              vstack = VPush(arena, FbleTakeStrongRef(vs->value), vstack);
            }

            assert(exec->bindings.size == 1 && "TODO: Support multi-binding exec");

            // Reserve a slot on the value stack for the binding value.
            vstack = VPush(arena, NULL, vstack);
            FbleValue** arg = &vstack->value;

            // Push the argument proc value on the stack in preparation for
            // execution.
            vstack = VPush(arena, FbleTakeStrongRef(exec->bindings.xs[0]), vstack);

            // Set up the thread stack to finish execution.
            tstack = TPush(arena, presult, &exec->proc._base, tstack);
            tstack = TPush(arena, NULL, &exec->pop._base, tstack);
            tstack = TPush(arena, body, exec->body, tstack);
            tstack = TPush(arena, arg, &exec->proc._base, tstack);
            break;
          }
        }
        break;
      }

      case FBLE_COND_INSTR: {
        FbleCondInstr* cond_instr = (FbleCondInstr*)instr;
        assert(vstack != NULL);
        FbleUnionValue* uv = (FbleUnionValue*)Deref(vstack->value, FBLE_UNION_VALUE);
        assert(uv->tag < cond_instr->choices.size);
        tstack = TPush(arena, presult, cond_instr->choices.xs[uv->tag], tstack);
        FbleDropStrongRef(arena, vstack->value);
        vstack = VPop(arena, vstack);
        break;
      }

      case FBLE_PUSH_INSTR: {
        FblePushInstr* push_instr = (FblePushInstr*)instr;

        tstack = TPush(arena, presult, push_instr->next, tstack);
        for (size_t i = 0; i < push_instr->values.size; ++i) {
          vstack = VPush(arena, NULL, vstack);
          tstack = TPush(arena, &vstack->value, push_instr->values.xs[i], tstack);
        }
        break;
      }

      case FBLE_POP_INSTR: {
        FblePopInstr* pop_instr = (FblePopInstr*)instr;
        for (size_t i = 0; i < pop_instr->count; ++i) {
          assert(vstack != NULL);
          FbleDropStrongRef(arena, vstack->value);
          vstack = VPop(arena, vstack);
        }
        break;
      }

      case FBLE_BREAK_CYCLE_INSTR: {
        FbleBreakCycleInstr* break_cycle_instr = (FbleBreakCycleInstr*)instr;
        FbleVStack* vs = vstack;
        for (size_t i = 0; i < break_cycle_instr->count; ++i) {
          assert(vs != NULL);
          FbleRefValue* rv = (FbleRefValue*) vs->value;
          assert(rv->_base.tag == FBLE_REF_VALUE);

          assert(rv->value != NULL);
          vs->value = FbleTakeStrongRef(rv->value);
          FbleBreakCycleRef(arena, rv->value);
          rv->broke_cycle = true;
          FbleDropStrongRef(arena, &rv->_base);

          vs = vs->tail;
        }
        break;
      }
    }
  }
  assert(vstack == NULL);
  return final_result;
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleArena* arena, FbleExpr* expr)
{
  FbleInstr* instrs = FbleCompile(arena, expr);
  if (instrs == NULL) {
    return NULL;
  }

  FbleValue* result = Eval(arena, instrs, NULL);
  FbleFreeInstrs(arena, instrs);
  return result;
}
// FbleExec -- see documentation in fble.h
FbleValue* FbleExec(FbleArena* arena, FbleProcValue* proc)
{
  FbleProcInstr instr = {
    ._base = {
      .tag = FBLE_PROC_INSTR,
      .refcount = 1
    },
    .pop = {
      ._base = {
        .tag = FBLE_POP_INSTR,
        .refcount = 1
      },
      .count = 1
    }
  };

  FbleValue* result = Eval(arena, &instr._base, &proc->_base);
  return result;
}
