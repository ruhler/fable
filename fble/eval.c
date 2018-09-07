// eval.c --
//   This file implements the fble evaluation routines.

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

// VStack --
//   A stack of values.
typedef struct VStack {
  FbleValue* value;
  struct VStack* tail;
} VStack;

// FbleFuncValue -- FBLE_FUNC_VALUE
//
// Fields:
//   context - The value stack at the time the function was created,
//             representing the lexical context available to the function.
//             Stored in reverse order of the standard value stack.
//   body - The instr representing the body of the function.
//          Note: fbleFuncValue does not take ownership of body. The
//          FuncValueInstr that allocates the FbleFuncValue has ownership of
//          the body.
//   pop - An instruction that can be used to pop the arguments, the
//         context, and the function value itself after a function is done
//         executing.
struct FbleFuncValue {
  FbleValue _base;
  VStack* context;
  FbleInstr* body;
  FblePopInstr pop;
};

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
static VStack* VPush(FbleArena* arena, FbleValue* value, VStack* tail);
static VStack* VPop(FbleArena* arena, VStack* vstack);
static ThreadStack* TPush(FbleArena* arena, FbleValue** presult, FbleInstr* instr, ThreadStack* tail); 

static FbleValue* Eval(FbleArena* arena, FbleInstr* instrs, VStack* stack);


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
//   Allocates a new VStack instance that should be freed when done.
static VStack* VPush(FbleArena* arena, FbleValue* value, VStack* tail)
{
  VStack* vstack = FbleAlloc(arena, VStack);
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
static VStack* VPop(FbleArena* arena, VStack* vstack)
{
  VStack* tail = vstack->tail;
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
//   vstack - the stack of values in scope.
//
// Results:
//   The computed value, or NULL on error.
//
// Side effects:
//   Prints a message to stderr in case of error.
static FbleValue* Eval(FbleArena* arena, FbleInstr* prgm, VStack* vstack_in)
{
  VStack* vstack = vstack_in;
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
        VStack* v = vstack;
        for (size_t i = 0; i < var_instr->position; ++i) {
          assert(v->tail != NULL);
          v = v->tail;
        }
        *presult = FbleCopy(arena, v->value);
        break;
      }

      case FBLE_LET_INSTR: {
        FbleLetInstr* let_instr = (FbleLetInstr*)instr;

        tstack = TPush(arena, NULL, &let_instr->pop._base, tstack);
        tstack = TPush(arena, presult, let_instr->body, tstack);

        for (size_t i = 0; i < let_instr->bindings.size; ++i) {
          FbleRefValue* rv = FbleAlloc(arena, FbleRefValue);
          rv->_base.tag = FBLE_REF_VALUE;
          rv->_base.refcount = 1;
          rv->value = NULL;
          vstack = VPush(arena, &rv->_base, vstack);
          tstack = TPush(arena, &rv->value, let_instr->bindings.xs[i], tstack);
        }
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        FbleFuncValue* value = FbleAlloc(arena, FbleFuncValue);
        value->_base.tag = FBLE_FUNC_VALUE;
        value->_base.refcount = 1;
        value->context = NULL;
        value->body = func_value_instr->body;
        value->pop._base.tag = FBLE_POP_INSTR;
        value->pop.count = 1 + func_value_instr->argc;
        *presult = &value->_base;

        // TODO: This copies the entire context, but really we should only
        // need to copy those variables that are used in the body of the
        // function. This has implications for performance and memory that
        // should be considered.
        for (VStack* vs = vstack; vs != NULL;  vs = vs->tail) {
          value->context = VPush(arena, FbleCopy(arena, vs->value), value->context);
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
        for (VStack* vs = func->context; vs != NULL; vs = vs->tail) {
          vstack = VPush(arena, FbleCopy(arena, vs->value), vstack);
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
        value->_base.refcount = 1;
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
        *presult = FbleCopy(arena, sv->fields.xs[access_instr->tag]);
        FbleRelease(arena, vstack->value);
        vstack = VPop(arena, vstack);
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        FbleUnionValue* union_value = FbleAlloc(arena, FbleUnionValue);
        union_value->_base.tag = FBLE_UNION_VALUE;
        union_value->_base.refcount = 1;
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
          while (vstack != vstack_in) {
            FbleRelease(arena, vstack->value);
            vstack = VPop(arena, vstack);
          }

          while (tstack != NULL) {
            ThreadStack* otstack = tstack;
            tstack = tstack->tail;
            FbleFree(arena, otstack);
          }

          return NULL;
        }
        *presult = FbleCopy(arena, uv->arg);

        FbleRelease(arena, vstack->value);
        vstack = VPop(arena, vstack);
        break;
      }

      case FBLE_COND_INSTR: {
        FbleCondInstr* cond_instr = (FbleCondInstr*)instr;
        assert(vstack != NULL);
        FbleUnionValue* uv = (FbleUnionValue*)Deref(vstack->value, FBLE_UNION_VALUE);
        assert(uv->tag < cond_instr->choices.size);
        tstack = TPush(arena, presult, cond_instr->choices.xs[uv->tag], tstack);
        FbleRelease(arena, vstack->value);
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
          FbleRelease(arena, vstack->value);
          vstack = VPop(arena, vstack);
        }
        break;
      }
    }
  }
  assert(vstack == vstack_in);
  return final_result;
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleArena* arena, FbleExpr* expr)
{
  FbleInstr* instrs = FbleCompile(arena, expr);
  if (instrs == NULL) {
    return NULL;
  }

  VStack* vstack = NULL;
  FbleValue* result = Eval(arena, instrs, vstack);
  FbleFreeInstrs(arena, instrs);
  return result;
}

// FbleFreeFuncValue -- see documentation in fble-internal.h
void FbleFreeFuncValue(FbleArena* arena, FbleFuncValue* value)
{
  VStack* vs = value->context;
  while (vs != NULL) {
    FbleRelease(arena, vs->value);
    vs = VPop(arena, vs);
  }

  // Note: The FbleFuncValue does not take ownership of value->body, so we
  // should not free it here.
  FbleFree(arena, value);
}
