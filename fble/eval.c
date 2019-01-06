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
//   Prints a message to stderr in case of error.
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
        value->fields.size = struct_value_instr->fields.size;
        value->fields.xs = FbleArenaAlloc(arena, value->fields.size * sizeof(FbleValue*), FbleAllocMsg(__FILE__, __LINE__));
        *presult = &value->_base;

        for (size_t i = 0; i < struct_value_instr->fields.size; ++i) {
          tstack = TPush(arena, value->fields.xs + i, struct_value_instr->fields.xs[i], tstack);
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

      case FBLE_PROC_EVAL_INSTR: {
        FbleProcEvalInstr* proc_eval_instr = (FbleProcEvalInstr*)instr;
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

      case FBLE_PROC_LINK_INSTR: {
        assert(false && "TODO: FBLE_PROC_LINK_INSTR");
        break;
      }

      case FBLE_PROC_EXEC_INSTR: {
        assert(false && "TODO: FBLE_PROC_EXEC_INSTR");
        break;
      }

      case FBLE_PROC_INSTR: {
        FbleProcValue* proc = (FbleProcValue*)Deref(vstack->value, FBLE_PROC_VALUE);
        switch (proc->tag) {
          case FBLE_GET_PROC_VALUE: assert(false && "TODO: FBLE_GET_PROC_VALUE"); break;
          case FBLE_PUT_PROC_VALUE: assert(false && "TODO: FBLE_PUT_PROC_VALUE"); break;

          case FBLE_EVAL_PROC_VALUE: {
            FbleEvalProcValue* eval = (FbleEvalProcValue*)proc;
            *presult = FbleTakeStrongRef(eval->result);
            break;
          }

          case FBLE_LINK_PROC_VALUE: assert(false && "TODO: FBLE_LINK_PROC_VALUE"); break;
          case FBLE_EXEC_PROC_VALUE: assert(false && "TODO: FBLE_EXEC_PROC_VALUE"); break;
        }

        FbleDropStrongRef(arena, vstack->value);
        vstack = VPop(arena, vstack);
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
    ._base = { .tag = FBLE_PROC_INSTR, .refcount = 1 },
  };

  FbleValue* result = Eval(arena, &instr._base, &proc->_base);
  return result;
}
