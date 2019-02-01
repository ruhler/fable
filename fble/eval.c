// eval.c --
//   This file implements the fble evaluation routines.

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

static FbleValue* Deref(FbleValue* value, FbleValueTag tag);
static FbleVStack* VPush(FbleArena* arena, FbleValue* value, FbleVStack* tail);
static FbleVStack* VPop(FbleArena* arena, FbleVStack* vstack);
static IStack* IPush(FbleArena* arena, FbleInstr* instr, IStack* tail); 

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
static FbleValue* Eval(FbleArena* arena, FbleInstr* prgm, FbleValue* arg)
{
  // The var_stack is used to store named values (aka variables).
  FbleVStack* var_stack = NULL;

  // The data_stack is used to store anonymous intermediate values.
  FbleVStack* data_stack = NULL;
  if (arg != NULL) {
    data_stack = VPush(arena, FbleTakeStrongRef(arg), data_stack);
  }
    
  IStack* istack = IPush(arena, prgm, NULL);
  while (istack != NULL) {
    FbleInstr* instr = istack->instr;
    IStack* tstack_done = istack;
    istack = istack->tail;
    FbleFree(arena, tstack_done);

    switch (instr->tag) {
      case FBLE_COMPOUND_INSTR: {
        FbleCompoundInstr* compound_instr = (FbleCompoundInstr*)instr;
        for (size_t i = 0; i < compound_instr->instrs.size; ++i) {
          size_t j = compound_instr->instrs.size - 1 - i;
          istack = IPush(arena, compound_instr->instrs.xs[j], istack);
        }
        break;
      }

      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_value_instr->argc;

        FbleValue* argv[argc];
        for (size_t i = 0; i < argc; ++i) {
          argv[argc - i - 1] = data_stack->value;
          data_stack = VPop(arena, data_stack);
        }

        FbleValueV args = { .size = argc, .xs = argv, };
        data_stack = VPush(arena, FbleNewStructValue(arena, &args), data_stack);

        for (size_t i = 0; i < args.size; ++i) {
          FbleValueRelease(arena, argv[i]);
        }
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        FbleValue* arg = data_stack->value;
        data_stack = VPop(arena, data_stack);
        data_stack = VPush(arena, FbleNewUnionValue(arena, union_value_instr->tag, arg), data_stack);
        FbleValueRelease(arena, arg);
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
        assert(data_stack != NULL);
        FbleStructValue* sv = (FbleStructValue*)Deref(data_stack->value, FBLE_STRUCT_VALUE);
        assert(access_instr->tag < sv->fields.size);
        data_stack = VPop(arena, data_stack);
        data_stack = VPush(arena, FbleTakeStrongRef(sv->fields.xs[access_instr->tag]), data_stack);
        FbleValueRelease(arena, &sv->_base);
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
            FbleValueRelease(arena, var_stack->value);
            var_stack = VPop(arena, var_stack);
          }

          while (data_stack != NULL) {
            FbleValueRelease(arena, data_stack->value);
            data_stack = VPop(arena, data_stack);
          }

          while (istack != NULL) {
            IStack* otstack = istack;
            istack = istack->tail;
            FbleFree(arena, otstack);
          }
          return NULL;
        }

        data_stack = VPop(arena, data_stack);
        data_stack = VPush(arena, FbleTakeStrongRef(uv->arg), data_stack);

        FbleValueRelease(arena, &uv->_base);
        break;
      }

      case FBLE_COND_INSTR: {
        FbleCondInstr* cond_instr = (FbleCondInstr*)instr;
        assert(data_stack != NULL);
        FbleUnionValue* uv = (FbleUnionValue*)Deref(data_stack->value, FBLE_UNION_VALUE);
        data_stack = VPop(arena, data_stack);
        assert(uv->tag < cond_instr->choices.size);
        istack = IPush(arena, cond_instr->choices.xs[uv->tag], istack);
        FbleValueRelease(arena, &uv->_base);
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

        // TODO: This copies the entire lexical context, but really we should
        // only need to copy those variables that are used in the body of the
        // function. This has implications for performance and memory that
        // should be considered.
        FbleVStack* vs = var_stack;
        for (size_t i = 0; i < func_value_instr->contextc; ++i) {
          assert(vs != NULL);
          value->context = VPush(arena, FbleTakeStrongRef(vs->value), value->context);
          vs = vs->tail;
        }

        data_stack = VPush(arena, &value->_base, data_stack);
        break;
      }

      case FBLE_DESCOPE_INSTR: {
        FbleDescopeInstr* descope_instr = (FbleDescopeInstr*)instr;
        for (size_t i = 0; i < descope_instr->count; ++i) {
          assert(var_stack != NULL);
          FbleValueRelease(arena, var_stack->value);
          var_stack = VPop(arena, var_stack);
        }
        break;
      }

      case FBLE_RELEASE_INSTR: {
        FbleValue* v = data_stack->value;
        data_stack = VPop(arena, data_stack);

        FbleValueRelease(arena, data_stack->value);
        data_stack = VPop(arena, data_stack);
        data_stack = VPush(arena, v, data_stack);
        break;
      }

      case FBLE_FUNC_APPLY_INSTR: {
        FbleFuncApplyInstr* apply_instr = (FbleFuncApplyInstr*)instr;

        FbleValue* args[apply_instr->argc];
        for (size_t i = 0; i < apply_instr->argc; ++i) {
          args[apply_instr->argc - i - 1] = data_stack->value;
          data_stack = VPop(arena, data_stack);
        }

        FbleFuncValue* func = (FbleFuncValue*)Deref(data_stack->value, FBLE_FUNC_VALUE);
        data_stack = VPop(arena, data_stack);

        // Push the function's context on top of the variable stack.
        for (FbleVStack* vs = func->context; vs != NULL; vs = vs->tail) {
          var_stack = VPush(arena, FbleTakeStrongRef(vs->value), var_stack);
        }

        // Push the function args onto the variable stack.
        for (size_t i = 0; i < apply_instr->argc; ++i) {
          var_stack = VPush(arena, args[i], var_stack);
        }

        // The func value owns the instructions we use to execute the body,
        // which means we need to ensure the func value is retained until
        // the body is done executing. To do so, keep the func value on the
        // data stack until after it's done executing. The body of the
        // function will take care of releasing this.
        data_stack = VPush(arena, &func->_base, data_stack);

        istack = IPush(arena, func->body, istack);
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
        data_stack = VPush(arena, &value->_base._base, data_stack);
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
        data_stack = VPush(arena, &value->_base._base, data_stack);
        break;
      }

      case FBLE_EVAL_INSTR: {
        FbleEvalProcValue* proc_value = FbleAlloc(arena, FbleEvalProcValue);
        proc_value->_base._base.tag = FBLE_PROC_VALUE;
        proc_value->_base._base.strong_ref_count = 1;
        proc_value->_base._base.break_cycle_ref_count = 0;
        proc_value->_base.tag = FBLE_EVAL_PROC_VALUE;
        proc_value->result = data_stack->value;
        data_stack = VPop(arena, data_stack);
        data_stack = VPush(arena, &proc_value->_base._base, data_stack);
        break;
      }

      case FBLE_VAR_INSTR: {
        FbleVarInstr* var_instr = (FbleVarInstr*)instr;
        FbleVStack* v = var_stack;
        for (size_t i = 0; i < var_instr->position; ++i) {
          assert(v->tail != NULL);
          v = v->tail;
        }
        data_stack = VPush(arena, FbleTakeStrongRef(v->value), data_stack);
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

        // TODO: This copies the entire lexical context, but really we should only
        // need to copy those variables that are used in the body of the
        // link process. This has implications for performance and memory that
        // should be considered.
        FbleVStack* vs = var_stack;
        for (size_t i = 0; i < link_instr->contextc; ++i) {
          assert(vs != NULL);
          value->context = VPush(arena, FbleTakeStrongRef(vs->value), value->context);
          vs = vs->tail;
        }

        data_stack = VPush(arena, &value->_base._base, data_stack);
        break;
      }

      case FBLE_EXEC_INSTR: {
        FbleExecInstr* exec_instr = (FbleExecInstr*)instr;
        FbleExecProcValue* value = FbleAlloc(arena, FbleExecProcValue);
        value->_base._base.tag = FBLE_PROC_VALUE;
        value->_base._base.strong_ref_count = 1;
        value->_base._base.break_cycle_ref_count = 0;
        value->_base.tag = FBLE_EXEC_PROC_VALUE;
        value->bindings.size = exec_instr->argc;
        value->bindings.xs = FbleArenaAlloc(arena, value->bindings.size * sizeof(FbleValue*), FbleAllocMsg(__FILE__, __LINE__));
        value->context = NULL;
        value->body = exec_instr->body;
        value->body->refcount++;

        // TODO: This copies the entire lexical context, but really we should
        // only need to copy those variables that are used in the body of the
        // exec process. This has implications for performance and memory that
        // should be considered.
        FbleVStack* vs = var_stack;
        for (size_t i = 0; i < exec_instr->contextc; ++i) {
          assert(vs != NULL);
          value->context = VPush(arena, FbleTakeStrongRef(vs->value), value->context);
          vs = vs->tail;
        }

        for (size_t i = 0; i < exec_instr->argc; ++i) {
          size_t j = exec_instr->argc - 1 - i;
          value->bindings.xs[j] = data_stack->value;
          data_stack = VPop(arena, data_stack);
        }

        data_stack = VPush(arena, &value->_base._base, data_stack);
        break;
      }

      case FBLE_JOIN_INSTR: {
        var_stack = VPush(arena, data_stack->value, var_stack);
        data_stack = VPop(arena, data_stack);
        break;
      }

      case FBLE_PROC_INSTR: {
        FbleProcValue* proc = (FbleProcValue*)Deref(data_stack->value, FBLE_PROC_VALUE);
        data_stack = VPop(arena, data_stack);

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
            data_stack = VPush(arena, head->value, data_stack);
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

            data_stack = VPush(arena, FbleTakeStrongRef(put->arg), data_stack);
            break;
          }

          case FBLE_EVAL_PROC_VALUE: {
            FbleEvalProcValue* eval = (FbleEvalProcValue*)proc;
            data_stack = VPush(arena, FbleTakeStrongRef(eval->result), data_stack);
            break;
          }

          case FBLE_LINK_PROC_VALUE: {
            FbleLinkProcValue* link = (FbleLinkProcValue*)proc;

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

            // The link proc value owns the instruction it uses to execute,
            // which means we need to ensure the proc value is retained
            // until we are done executing. To do so, keep the proc value on
            // the data stack until after it's done executing.
            data_stack = VPush(arena, FbleTakeStrongRef(&proc->_base), data_stack);

            istack = IPush(arena, link->body, istack);
            break;
          }

          case FBLE_EXEC_PROC_VALUE: {
            FbleExecProcValue* exec = (FbleExecProcValue*)proc;

            // Push the body's context on top of the variable stack.
            for (FbleVStack* vs = exec->context; vs != NULL; vs = vs->tail) {
              var_stack = VPush(arena, FbleTakeStrongRef(vs->value), var_stack);
            }

            assert(exec->bindings.size == 1 && "TODO: Support multi-binding exec");

            // The exec proc value owns the instruction it uses to execute,
            // which means we need to ensure the proc value is retained
            // until we are done executing. To do so, keep the proc value on
            // the data stack until after it's done executing.
            data_stack = VPush(arena, FbleTakeStrongRef(&proc->_base), data_stack);

            // Push the argument proc value on the stack in preparation for
            // execution.
            data_stack = VPush(arena, FbleTakeStrongRef(exec->bindings.xs[0]), data_stack);

            istack = IPush(arena, exec->body, istack);
            break;
          }
        }

        FbleValueRelease(arena, &proc->_base);
        break;
      }

      case FBLE_LET_PREP_INSTR: {
        FbleLetPrepInstr* let_instr = (FbleLetPrepInstr*)instr;

        FbleRefValue* first = NULL;
        FbleRefValue* curr = NULL;
        for (size_t i = 0; i < let_instr->count; ++i) {
          FbleRefValue* rv = FbleAlloc(arena, FbleRefValue);
          rv->_base.tag = FBLE_REF_VALUE;
          rv->_base.strong_ref_count = 1;
          rv->_base.break_cycle_ref_count = 0;
          rv->value = NULL;
          rv->broke_cycle = false;
          rv->siblings = curr;
          var_stack = VPush(arena, &rv->_base, var_stack);

          if (first == NULL) {
            first = rv;
          }
          curr = rv;
        }
        assert(first != NULL);
        first->siblings = curr;
        break;
      }

      case FBLE_LET_DEF_INSTR: {
        FbleLetDefInstr* let_def_instr = (FbleLetDefInstr*)instr;
        FbleVStack* vs = var_stack;
        for (size_t i = 0; i < let_def_instr->count; ++i) {
          assert(vs != NULL);
          FbleRefValue* rv = (FbleRefValue*) vs->value;
          assert(rv->_base.tag == FBLE_REF_VALUE);

          rv->value = data_stack->value;
          data_stack = VPop(arena, data_stack);

          assert(rv->value != NULL);
          vs->value = FbleTakeStrongRef(rv->value);
          FbleBreakCycleRef(arena, rv->value);
          rv->broke_cycle = true;
          FbleValueRelease(arena, &rv->_base);

          vs = vs->tail;
        }
        break;
      }
    }
  }
  assert(var_stack == NULL);

  assert(data_stack != NULL);
  FbleValue* final_result = data_stack->value;
  data_stack = VPop(arena, data_stack);
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
  };

  FbleValue* result = Eval(arena, &instr._base, &proc->_base);
  return result;
}
