// eval.c --
//   This file implements the fble evaluation routines.

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

// IStack --
//   A stack of instructions to execute. Describes the computation context for
//   a thread.
// 
// Fields:
//   result - Pointer to where the result of evaluating an expression should
//            be stored.
//   instr -  The instructions for executing the expression.
typedef struct IStack {
  FbleValue** result;
  FbleInstr* instr;
  struct IStack* tail;
} IStack;

static FbleValue* Deref(FbleValue* value, FbleValueTag tag);
static FbleVStack* VPush(FbleArena* arena, FbleValue* value, FbleVStack* tail);
static FbleVStack* VPop(FbleArena* arena, FbleVStack* vstack);
static IStack* IPush(FbleArena* arena, FbleValue** presult, FbleInstr* instr, IStack* tail); 

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
//   Push an element onto an instruction stack.
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
//   Allocates a new IStack instance that should be freed when done.
static IStack* IPush(FbleArena* arena, FbleValue** presult, FbleInstr* instr, IStack* tail)
{
  assert(instr != NULL && "IPush NULL FbleInstr");
  IStack* istack = FbleAlloc(arena, IStack);
  istack->result = presult;
  istack->instr = instr;
  istack->tail = tail;
  return istack;
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
  // The var_stack is used to store named values (aka variables).
  FbleVStack* var_stack = NULL;

  // The data_stack is used to store anonymous intermediate values.
  FbleVStack* data_stack = NULL;
  if (arg != NULL) {
    data_stack = VPush(arena, FbleTakeStrongRef(arg), data_stack);
  }
    
  FbleValue* final_result = NULL;
  IStack* istack = IPush(arena, &final_result, prgm, NULL);

  while (istack != NULL) {
    FbleValue** presult = istack->result;
    FbleInstr* instr = istack->instr;
    IStack* tstack_done = istack;
    istack = istack->tail;
    FbleFree(arena, tstack_done);

    switch (instr->tag) {
      case FBLE_VAR_INSTR: {
        FbleVarInstr* var_instr = (FbleVarInstr*)instr;
        FbleVStack* v = var_stack;
        for (size_t i = 0; i < var_instr->position; ++i) {
          assert(v->tail != NULL);
          v = v->tail;
        }
        *presult = FbleTakeStrongRef(v->value);
        break;
      }

      case FBLE_LET_INSTR: {
        FbleLetInstr* let_instr = (FbleLetInstr*)instr;

        istack = IPush(arena, NULL, &let_instr->pop._base, istack);
        istack = IPush(arena, presult, let_instr->body, istack);
        istack = IPush(arena, NULL, &let_instr->break_cycle._base, istack);

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
          var_stack = VPush(arena, &rv->_base, var_stack);
          istack = IPush(arena, &rv->value, let_instr->bindings.xs[i], istack);

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
        for (FbleVStack* vs = var_stack; vs != NULL;  vs = vs->tail) {
          value->context = VPush(arena, FbleTakeStrongRef(vs->value), value->context);
          value->pop.count++;
        }
        break;
      }

      case FBLE_FUNC_APPLY_INSTR: {
        FbleFuncApplyInstr* apply_instr = (FbleFuncApplyInstr*)instr;
        FbleValue* args[apply_instr->argc];
        for (size_t i = 0; i < apply_instr->argc; ++i) {
          args[i] = data_stack->value;
          data_stack = VPop(arena, data_stack);
        }

        FbleFuncValue* func = (FbleFuncValue*)Deref(data_stack->value, FBLE_FUNC_VALUE);

        // Push the function's context on top of the variable stack.
        for (FbleVStack* vs = func->context; vs != NULL; vs = vs->tail) {
          var_stack = VPush(arena, FbleTakeStrongRef(vs->value), var_stack);
        }

        // Push the function args on top of the variable stack.
        for (size_t i = 0; i < apply_instr->argc; ++i) {
          size_t j = apply_instr->argc - 1 - i;
          var_stack = VPush(arena, args[j], var_stack);
        }

        istack = IPush(arena, NULL, &func->pop._base, istack);
        istack = IPush(arena, presult, func->body, istack);
        break;
      }

      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_value_instr->argc;

        FbleValue* argv[argc];
        for (size_t i = 0; i < struct_value_instr->argc; ++i) {
          argv[i] = data_stack->value;
          data_stack = VPop(arena, data_stack);
        }

        FbleValueV args = {
          .size = argc,
          .xs = argv,
        };

        *presult = FbleNewStructValue(arena, &args);

        for (size_t i = 0; i < args.size; ++i) {
          FbleDropStrongRef(arena, argv[i]);
        }
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        assert(data_stack != NULL);
        FbleStructValue* sv = (FbleStructValue*)Deref(data_stack->value, FBLE_STRUCT_VALUE);
        assert(access_instr->tag < sv->fields.size);
        *presult = FbleTakeStrongRef(sv->fields.xs[access_instr->tag]);
        FbleDropStrongRef(arena, data_stack->value);
        data_stack = VPop(arena, data_stack);
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        *presult = FbleNewUnionValue(arena, union_value_instr->tag, data_stack->value);
        FbleDropStrongRef(arena, data_stack->value);
        data_stack = VPop(arena, data_stack);
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

        assert(data_stack != NULL);
        FbleUnionValue* uv = (FbleUnionValue*)Deref(data_stack->value, FBLE_UNION_VALUE);
        if (uv->tag != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);

          // Clean up the stacks.
          while (var_stack != NULL) {
            FbleDropStrongRef(arena, var_stack->value);
            var_stack = VPop(arena, var_stack);
          }

          while (data_stack != NULL) {
            FbleDropStrongRef(arena, data_stack->value);
            data_stack = VPop(arena, data_stack);
          }

          while (istack != NULL) {
            IStack* otstack = istack;
            istack = istack->tail;
            FbleFree(arena, otstack);
          }

          FbleDropStrongRef(arena, final_result);
          return NULL;
        }
        *presult = FbleTakeStrongRef(uv->arg);

        FbleDropStrongRef(arena, data_stack->value);
        data_stack = VPop(arena, data_stack);
        break;
      }

      case FBLE_GET_INSTR: {
        FbleGetProcValue* value = FbleAlloc(arena, FbleGetProcValue);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base._base.strong_ref_count = 1;
        value->_base._base.break_cycle_ref_count = 0;
        value->_base.tag = FBLE_GET_PROC_VALUE;

        value->port = data_stack->value;
        data_stack = VPop(arena, data_stack);

        *presult = &value->_base._base;
        break;
      }

      case FBLE_PUT_INSTR: {
        FblePutProcValue* value = FbleAlloc(arena, FblePutProcValue);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base._base.strong_ref_count = 1;
        value->_base._base.break_cycle_ref_count = 0;
        value->_base.tag = FBLE_PUT_PROC_VALUE;

        value->arg = data_stack->value;
        data_stack = VPop(arena, data_stack);

        value->port = data_stack->value;
        data_stack = VPop(arena, data_stack);

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

        istack = IPush(arena, &proc_value->result, proc_eval_instr->body, istack);
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
        for (FbleVStack* vs = var_stack; vs != NULL;  vs = vs->tail) {
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
        for (FbleVStack* vs = var_stack; vs != NULL;  vs = vs->tail) {
          value->context = VPush(arena, FbleTakeStrongRef(vs->value), value->context);
          value->pop.count++;
        }

        for (size_t i = 0; i < exec_instr->bindings.size; ++i) {
          istack = IPush(arena, value->bindings.xs + i, exec_instr->bindings.xs[i], istack);
        }

        // Update the result after capturing the context so that we don't grab
        // a reference to ourself when we copy the context.
        // TODO: Shouldn't reference counting take care of this?
        *presult = &value->_base._base;
        break;
      }

      case FBLE_PROC_INSTR: {
        FbleProcInstr* proc_instr = (FbleProcInstr*)instr;
        FbleProcValue* proc = (FbleProcValue*)Deref(data_stack->value, FBLE_PROC_VALUE);

        // Some proc values own the instructions they use to execute, which
        // means we need to ensure the proc values are retained until they are
        // done executing. To do so, simply keep the proc value on the value
        // stack until after it's done executing.
        istack = IPush(arena, NULL, &proc_instr->pop._base, istack);

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

            // Reserve a slot on the data stack for the link body ProcValue.
            data_stack = VPush(arena, NULL, data_stack);
            FbleValue** body = &data_stack->value;

            // Push the body's context on top of the variable stack.
            for (FbleVStack* vs = link->context; vs != NULL; vs = vs->tail) {
              var_stack = VPush(arena, FbleTakeStrongRef(vs->value), var_stack);
            }

            // Allocate the link and push the ports on top of the variable stack.
            FbleInputValue* get = FbleAlloc(arena, FbleInputValue);
            get->_base.tag = FBLE_INPUT_VALUE;
            get->_base.strong_ref_count = 2;
            get->_base.break_cycle_ref_count = 0;
            get->head = NULL;
            get->tail = NULL;
            var_stack = VPush(arena, &get->_base, var_stack);

            FbleOutputValue* put = FbleAlloc(arena, FbleOutputValue);
            put->_base.tag = FBLE_OUTPUT_VALUE;
            put->_base.strong_ref_count = 1;
            put->_base.break_cycle_ref_count = 0;
            put->dest = get;
            var_stack = VPush(arena, &put->_base, var_stack);

            // Set up the instruction stack to finish execution.
            istack = IPush(arena, presult, &link->proc._base, istack);
            istack = IPush(arena, NULL, &link->pop._base, istack);
            istack = IPush(arena, body, link->body, istack);
            break;
          }

          case FBLE_EXEC_PROC_VALUE: {
            FbleExecProcValue* exec = (FbleExecProcValue*)proc;

            // Reserve a slot on the data stack for the exec body ProcValue.
            data_stack = VPush(arena, NULL, data_stack);
            FbleValue** body = &data_stack->value;

            // Push the body's context on top of the variable stack.
            for (FbleVStack* vs = exec->context; vs != NULL; vs = vs->tail) {
              var_stack = VPush(arena, FbleTakeStrongRef(vs->value), var_stack);
            }

            assert(exec->bindings.size == 1 && "TODO: Support multi-binding exec");

            // Reserve a slot on the data stack for the binding value.
            data_stack = VPush(arena, NULL, data_stack);
            FbleValue** arg = &data_stack->value;

            // Push the argument proc value on the stack in preparation for
            // execution.
            data_stack = VPush(arena, FbleTakeStrongRef(exec->bindings.xs[0]), data_stack);

            // Set up the instruction stack to finish execution.
            istack = IPush(arena, presult, &exec->proc._base, istack);
            istack = IPush(arena, NULL, &exec->pop._base, istack);
            istack = IPush(arena, body, exec->body, istack);
            // TODO: Add a Join instruction here to move the binding over to
            // the variable stack.
            istack = IPush(arena, arg, &exec->proc._base, istack);
            break;
          }
        }
        break;
      }

      case FBLE_COND_INSTR: {
        FbleCondInstr* cond_instr = (FbleCondInstr*)instr;
        assert(data_stack != NULL);
        FbleUnionValue* uv = (FbleUnionValue*)Deref(data_stack->value, FBLE_UNION_VALUE);
        assert(uv->tag < cond_instr->choices.size);
        istack = IPush(arena, presult, cond_instr->choices.xs[uv->tag], istack);
        FbleDropStrongRef(arena, data_stack->value);
        data_stack = VPop(arena, data_stack);
        break;
      }

      case FBLE_PUSH_INSTR: {
        FblePushInstr* push_instr = (FblePushInstr*)instr;

        istack = IPush(arena, presult, push_instr->next, istack);
        for (size_t i = 0; i < push_instr->values.size; ++i) {
          data_stack = VPush(arena, NULL, data_stack);
          istack = IPush(arena, &data_stack->value, push_instr->values.xs[i], istack);
        }
        break;
      }

      case FBLE_POP_INSTR: {
        FblePopInstr* pop_instr = (FblePopInstr*)instr;
        for (size_t i = 0; i < pop_instr->count; ++i) {
          assert(var_stack != NULL);
          FbleDropStrongRef(arena, var_stack->value);
          var_stack = VPop(arena, var_stack);
        }
        break;
      }

      case FBLE_BREAK_CYCLE_INSTR: {
        FbleBreakCycleInstr* break_cycle_instr = (FbleBreakCycleInstr*)instr;
        FbleVStack* vs = var_stack;
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
  assert(var_stack == NULL);
  assert(data_stack == NULL);
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
