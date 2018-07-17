// eval.c --
//   This file implements the fble evaluation routines.

#include <stdio.h>    // for fprintf, stderr
#include <string.h>   // for memcpy

#include "fble.h"

#define UNREACHABLE(x) assert(false && x)

static FbleValue gTypeTypeValue = {
  .tag = FBLE_TYPE_TYPE_VALUE,
  .refcount = 1,
  .type = &gTypeTypeValue
};

// Vars --
//   Scope of variables visible during type checking.
typedef struct Vars {
  FbleName name;
  FbleValue* type;
  struct Vars* next;
} Vars;

// VStack --
//   A stack of values.
typedef struct {
  size_t size;
  size_t capacity;
  FbleValue** xs;
} VStack;

// InstrTag --
//   Enum used to distinguish among different kinds of instructions.
typedef enum {
  TYPE_TYPE_INSTR,
  VAR_INSTR,
  LET_INSTR,
  STRUCT_TYPE_INSTR,
  UNION_TYPE_INSTR,
} InstrTag;

// Instr --
//   Common base type for all instructions.
typedef struct {
  InstrTag tag;
} Instr;

// InstrV --
//   A vector of Instr.
typedef struct {
  size_t size;
  Instr** xs;
} InstrV;

// FInstr --
//   An instruction associated with a field name.
typedef struct {
  Instr* instr;
  FbleName name;
} FInstr;

// FInstrV --
//   A vector of FInstr
typedef struct {
  size_t size;
  FInstr* xs;
} FInstrV;

// TypeTypeInstr -- TYPE_TYPE_INSTR
//   Outputs a TYPE_TYPE_VALUE.
typedef struct {
  Instr _base;
} TypeTypeInstr;

// VarInstr -- VAR_INSTR
//   Reads the variable at the given position in the stack. Position 0 is the
//   top of the stack.
typedef struct {
  Instr _base;
  size_t position;
} VarInstr;

// LetInstr -- LET_INSTR
//   Evaluate each of the bindings, add the results to the scope, then execute
//   the body.
typedef struct {
  Instr _base;
  InstrV bindings;
  Instr* body;
} LetInstr;

// StructTypeInstr -- STRUCT_TYPE_INSTR
//   Allocate a struct type, then execute each of the field types.
typedef struct {
  Instr _base;
  FInstrV fields;
} StructTypeInstr;

// UnionTypeInstr -- UNION_TYPE_INSTR
//   Allocate a union type, then execute each of the field types.
typedef struct {
  Instr _base;
  FInstrV fields;
} UnionTypeInstr;

// ThreadStack --
//   The computation context for a thread.
// 
// Fields:
//   result - Pointer to where the result of evaluating an expression should
//            be stored.
//   instr -  The instructions for executing the expression.
typedef struct ThreadStack {
  FbleValue** result;
  Instr* instr;
  struct ThreadStack* tail;
} ThreadStack;

static void Push(FbleArena* arena, VStack* stack, FbleValue* value);
static void Pop(FbleArena* arena, VStack* stack, size_t count);
static FbleValue* Get(VStack* stack, size_t position);

static bool TypesEqual(FbleValue* a, FbleValue* b);
static void PrintType(FbleValue* type);
static bool IsKinded(FbleValue* type);

static FbleValue* Compile(FbleArena* arena, Vars* vars, VStack* vstack, FbleExpr* expr, Instr** instrs);
static FbleValue* Eval(FbleArena* arena, Instr* instrs, VStack* stack);
static void FreeInstrs(FbleArena* arena, Instr* instrs);


// Push --
//   Push a value onto a value stack.
//
// Inputs:
//   arena - the arena to use for allocations.
//   stack - the stack to push the value onto.
//   value - the value to push.
//
// Result:
//   None.
//
// Side effects:
//   Pushes the value on top of the stack. Does not take a refcount copy of
//   the value that will be released when the value is popped from the stack.
//   It is the callers job to ensure a refcount is taken for the value before
//   it is put onto the stack.
static void Push(FbleArena* arena, VStack* stack, FbleValue* value)
{
  if (stack->size == stack->capacity) {
    stack->capacity = 2 * stack->size;
    FbleValue** nxs = FbleArenaAlloc(arena, sizeof(FbleValue*) * stack->capacity, FbleAllocMsg(__FILE__, __LINE__));
    memcpy(nxs, stack->xs, sizeof(FbleValue*) * stack->size);
    FbleFree(arena, stack->xs);
    stack->xs = nxs;
  }

  stack->xs[stack->size++] = value;
}

// Pop --
//   Remove values from the top of the stack.
//
// Inputs:
//   arena - the arena to use for allocation.
//   stack - the stack to pop the value from.
//   count - the number of values to remove from the stack.
//
// Result:
//   none.
//
// Side effects:
//   Removes the top 'count' values on the stack.
static void Pop(FbleArena* arena, VStack* stack, size_t count)
{
  assert(count <= stack->size);
  stack->size -= count;
  for (size_t i = 0; i < count; ++i) {
    FbleRelease(arena, stack->xs[stack->size + i]);
  }

  if (2 * stack->size < stack->capacity) {
    stack->capacity = 2 * stack->size;
    FbleValue** nxs = FbleArenaAlloc(arena, sizeof(FbleValue*) * stack->capacity, FbleAllocMsg(__FILE__, __LINE__));
    memcpy(nxs, stack->xs, sizeof(FbleValue*) * stack->size);
    FbleFree(arena, stack->xs);
    stack->xs = nxs;
  }
}

// Get --
//   Get the value at the given position relative to the top of the stack.
//
// Inputs: 
//   stack - the stack to get the value from.
//   position - how deep into the stack to get the value from. 0 is the top of
//              the stack.
//
// Results:
//   The value at the given position in the stack.
//
// Side effects:
//   Does not increment the refcount of the value. It is the callers job to do
//   so if so required.
static FbleValue* Get(VStack* stack, size_t position)
{
  assert(position < stack->size);
  return stack->xs[stack->size - 1 - position];
}

// TypesEqual --
//   Test whether the two given types are equal.
//
// Inputs:
//   a - the first type
//   b - the second type
//
// Results:
//   True if the first type equals the second type, false otherwise.
//
// Side effects:
//   None.
static bool TypesEqual(FbleValue* a, FbleValue* b)
{
  if (a->tag != b->tag) {
    return false;
  }

  switch (a->tag) {
    case FBLE_TYPE_TYPE_VALUE: return true;
    case FBLE_FUNC_TYPE_VALUE: assert(false && "TODO FUNC_TYPE"); return false;
    case FBLE_FUNC_VALUE: UNREACHABLE("not a type"); return false;
    case FBLE_STRUCT_TYPE_VALUE: assert(false && "TODO STRUCT_TYPE"); return false;
    case FBLE_STRUCT_VALUE: UNREACHABLE("not a type"); return false;
    case FBLE_UNION_TYPE_VALUE: assert(false && "TODO UNION_TYPE"); return false;
    case FBLE_UNION_VALUE: UNREACHABLE("not a type"); return false;
    case FBLE_PROC_TYPE_VALUE: assert(false && "TODO PROC_TYPE"); return false;
    case FBLE_INPUT_TYPE_VALUE: assert(false && "TODO INPUT_TYPE"); return false;
    case FBLE_OUTPUT_TYPE_VALUE: assert(false && "TODO OUTPUT_TYPE"); return false;
    case FBLE_PROC_VALUE: UNREACHABLE("not a type"); return false;
    case FBLE_INPUT_VALUE: UNREACHABLE("not a type"); return false;
    case FBLE_OUTPUT_VALUE: UNREACHABLE("not a type"); return false;
  }

  UNREACHABLE("should never get here");
}

// PrintType --
//   Print the given type in human readable form to stderr.
//
// Inputs:
//   type - the type to print.
//
// Result:
//   None.
//
// Side effect:
//   Prints the given type in human readable form to stderr.
static void PrintType(FbleValue* type)
{
  switch (type->tag) {
    case FBLE_TYPE_TYPE_VALUE: fprintf(stderr, "@"); return;
    case FBLE_FUNC_TYPE_VALUE: assert(false && "TODO FUNC_TYPE"); return;
    case FBLE_FUNC_VALUE: UNREACHABLE("not a type"); return;
    case FBLE_STRUCT_TYPE_VALUE: assert(false && "TODO STRUCT_TYPE"); return;
    case FBLE_STRUCT_VALUE: UNREACHABLE("not a type"); return;
    case FBLE_UNION_TYPE_VALUE: assert(false && "TODO UNION_TYPE"); return;
    case FBLE_UNION_VALUE: UNREACHABLE("not a type"); return;
    case FBLE_PROC_TYPE_VALUE: assert(false && "TODO PROC_TYPE"); return;
    case FBLE_INPUT_TYPE_VALUE: assert(false && "TODO INPUT_TYPE"); return;
    case FBLE_OUTPUT_TYPE_VALUE: assert(false && "TODO OUTPUT_TYPE"); return;
    case FBLE_PROC_VALUE: UNREACHABLE("not a type"); return;
    case FBLE_INPUT_VALUE: UNREACHABLE("not a type"); return;
    case FBLE_OUTPUT_VALUE: UNREACHABLE("not a type"); return;
  }
}

// IsKinded --
//   Returns true if the given type contains @ somewhere within it.
//
// Inputs:
//   type - the type to check kindedness of
//
// Results:
//   true if the type is kinded, false otherwise.
//
// Side effects:
//   none.
static bool IsKinded(FbleValue* type)
{
  switch (type->tag) {
    case FBLE_TYPE_TYPE_VALUE: return true;
    case FBLE_FUNC_TYPE_VALUE: assert(false && "TODO FUNC_TYPE"); return false;
    case FBLE_FUNC_VALUE: UNREACHABLE("not a type"); return false;
    case FBLE_STRUCT_TYPE_VALUE: assert(false && "TODO STRUCT_TYPE"); return false;
    case FBLE_STRUCT_VALUE: UNREACHABLE("not a type"); return false;
    case FBLE_UNION_TYPE_VALUE: assert(false && "TODO UNION_TYPE"); return false;
    case FBLE_UNION_VALUE: UNREACHABLE("not a type"); return false;
    case FBLE_PROC_TYPE_VALUE: assert(false && "TODO PROC_TYPE"); return false;
    case FBLE_INPUT_TYPE_VALUE: assert(false && "TODO INPUT_TYPE"); return false;
    case FBLE_OUTPUT_TYPE_VALUE: assert(false && "TODO OUTPUT_TYPE"); return false;
    case FBLE_PROC_VALUE: UNREACHABLE("not a type"); return false;
    case FBLE_INPUT_VALUE: UNREACHABLE("not a type"); return false;
    case FBLE_OUTPUT_VALUE: UNREACHABLE("not a type"); return false;
  }
  UNREACHABLE("Should not get here");
  return false;
}

// Compile --
//   Type check and compile the given expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the list of variables in scope.
//   vstack - the value stack.
//   expr - the expression to compile.
//   instrs - output pointer to store generated in structions
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression fails to compile.
static FbleValue* Compile(FbleArena* arena, Vars* vars, VStack* vstack, FbleExpr* expr, Instr** instrs)
{
  switch (expr->tag) {
    case FBLE_VAR_EXPR: {
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;

      int position = 0;
      while (vars != NULL && !FbleNamesEqual(var_expr->var.name, vars->name.name)) {
        vars = vars->next;
        position++;
      }

      if (vars == NULL) {
        FbleReportError("variable '%s' not defined\n", &var_expr->var.loc, var_expr->var.name);
        return NULL;
      }

      VarInstr* instr = FbleAlloc(arena, VarInstr);
      instr->_base.tag = VAR_INSTR;
      instr->position = position;
      *instrs = &instr->_base;
      return vars->type;
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;

      // Step 1: Compile, check, and evaluate the types of all the variables.
      FbleValue* types[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        Instr* prgm;
        FbleType* type_expr = let_expr->bindings.xs[i].type;
        FbleValue* type_type = Compile(arena, vars, vstack, type_expr, &prgm);
        if (type_type == NULL) {
          return NULL;
        }
        
        if (type_type->tag != FBLE_TYPE_TYPE_VALUE) {
          FbleReportError("expected a type, found something else\n", &type_expr->loc);
          return NULL;
        }

        types[i] = Eval(arena, prgm, vstack);
        if (types[i] == NULL) {
          FbleReportError("failed to evaluate type\n", &type_expr->loc);
          return NULL;
        }
      }

      // Step 2: Add the new variables to the scope.
      assert(let_expr->bindings.size > 0);
      Vars nvars[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        nvars[i].name = let_expr->bindings.xs[i].name;
        nvars[i].type = types[i];
        nvars[i].next = vars;
        vars = nvars + i;

        // TODO: Push an abstract value here instead of NULL?
        Push(arena, vstack, NULL);
      }

      // Step 3: Compile, check, and if appropriate, evaluate the values of
      // the variables.
      LetInstr* instr = FbleAlloc(arena, LetInstr);
      instr->_base.tag = LET_INSTR;
      FbleVectorInit(arena, instr->bindings);
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        Instr** prgm = FbleVectorExtend(arena, instr->bindings);
        FbleValue* type = Compile(arena, vars, vstack, let_expr->bindings.xs[i].expr, prgm);
        if (type == NULL) {
          return NULL;
        }

        if (!TypesEqual(types[i], type)) {
          FbleReportError("expected type ", &let_expr->bindings.xs[i].expr->loc);
          PrintType(types[i]);
          fprintf(stderr, ", but found ");
          PrintType(type);
          fprintf(stderr, "\n");
          return NULL;
        }

        if (IsKinded(types[i])) {
          FbleValue* v = Eval(arena, *prgm, vstack);
          if (v == NULL) {
            return NULL;
          }
          vstack->xs[vstack->size - let_expr->bindings.size + i] = v;
        }
      }

      *instrs = &instr->_base;
      FbleValue* result = Compile(arena, vars, vstack, let_expr->body, &(instr->body));
      Pop(arena, vstack, let_expr->bindings.size);
      return result;
    }

    case FBLE_TYPE_TYPE_EXPR: {
      TypeTypeInstr* instr = FbleAlloc(arena, TypeTypeInstr);
      instr->_base.tag = TYPE_TYPE_INSTR;
      *instrs = &instr->_base;

      return FbleCopy(arena, &gTypeTypeValue);
    }

    case FBLE_FUNC_TYPE_EXPR: assert(false && "TODO: FBLE_FUNC_TYPE_EXPR"); return NULL;
    case FBLE_FUNC_VALUE_EXPR: assert(false && "TODO: FBLE_FUNC_VALUE_EXPR"); return NULL;
    case FBLE_FUNC_APPLY_EXPR: assert(false && "TODO: FBLE_FUNC_APPLY_EXPR"); return NULL;

    case FBLE_STRUCT_TYPE_EXPR: {
      FbleStructTypeExpr* struct_type_expr = (FbleStructTypeExpr*)expr;
      StructTypeInstr* instr = FbleAlloc(arena, StructTypeInstr);
      instr->_base.tag = STRUCT_TYPE_INSTR;
      FbleVectorInit(arena, instr->fields);
      for (size_t i = 0; i < struct_type_expr->fields.size; ++i) {
        FInstr* finstr = FbleVectorExtend(arena, instr->fields);
        FbleField* field = struct_type_expr->fields.xs + i;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, struct_type_expr->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, &field->name.name);
            return NULL;
          }
        }

        finstr->name = field->name;
        FbleValue* type = Compile(arena, vars, vstack, field->type, &finstr->instr);
        if (type == NULL) {
          return NULL;
        }

        if (!TypesEqual(type, &gTypeTypeValue)) {
          FbleReportError("expected a type, but found something of type ", &field->type->loc);
          PrintType(type);
          fprintf(stderr, "\n");
          return NULL;
        }
      }

      *instrs = &instr->_base;
      return FbleCopy(arena, &gTypeTypeValue);
    }

    case FBLE_STRUCT_VALUE_EXPR: assert(false && "TODO: FBLE_STRUCT_VALUE_EXPR"); return NULL;
    case FBLE_STRUCT_ACCESS_EXPR: assert(false && "TODO: FBLE_STRUCT_ACCESS_EXPR"); return NULL;

    case FBLE_UNION_TYPE_EXPR: {
      FbleUnionTypeExpr* union_type_expr = (FbleUnionTypeExpr*)expr;
      UnionTypeInstr* instr = FbleAlloc(arena, UnionTypeInstr);
      instr->_base.tag = UNION_TYPE_INSTR;
      FbleVectorInit(arena, instr->fields);
      assert(union_type_expr->fields.size > 0);
      for (size_t i = 0; i < union_type_expr->fields.size; ++i) {
        FInstr* finstr = FbleVectorExtend(arena, instr->fields);
        FbleField* field = union_type_expr->fields.xs + i;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, union_type_expr->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, &field->name.name);
            return NULL;
          }
        }

        finstr->name = field->name;
        FbleValue* type = Compile(arena, vars, vstack, field->type, &finstr->instr);
        if (type == NULL) {
          return NULL;
        }

        if (!TypesEqual(type, &gTypeTypeValue)) {
          FbleReportError("expected a type, but found something of type ", &field->type->loc);
          PrintType(type);
          fprintf(stderr, "\n");
          return NULL;
        }
      }

      *instrs = &instr->_base;
      return FbleCopy(arena, &gTypeTypeValue);
    }

    case FBLE_UNION_VALUE_EXPR: assert(false && "TODO: FBLE_UNION_VALUE_EXPR"); return NULL;
    case FBLE_UNION_ACCESS_EXPR: assert(false && "TODO: FBLE_UNION_ACCESS_EXPR"); return NULL;
    case FBLE_COND_EXPR: assert(false && "TODO: FBLE_COND_EXPR"); return NULL;

    case FBLE_PROC_TYPE_EXPR: assert(false && "TODO: FBLE_PROC_TYPE_EXPR"); return NULL;
    case FBLE_INPUT_TYPE_EXPR: assert(false && "TODO: FBLE_INPUT_TYPE_EXPR"); return NULL;
    case FBLE_OUTPUT_TYPE_EXPR: assert(false && "TODO: FBLE_OUTPUT_TYPE_EXPR"); return NULL;
    case FBLE_EVAL_EXPR: assert(false && "TODO: FBLE_EVAL_EXPR"); return NULL;
    case FBLE_GET_EXPR: assert(false && "TODO: FBLE_GET_EXPR"); return NULL;
    case FBLE_PUT_EXPR: assert(false && "TODO: FBLE_PUT_EXPR"); return NULL;
    case FBLE_LINK_EXPR: assert(false && "TODO: FBLE_LINK_EXPR"); return NULL;
    case FBLE_EXEC_EXPR: assert(false && "TODO: FBLE_EXEC_EXPR"); return NULL;

    case FBLE_ACCESS_EXPR: assert(false && "TODO: FBLE_ACCESS_EXPR"); return NULL;
    case FBLE_APPLY_EXPR: assert(false && "TODO: FBLE_APPLY_EXPR"); return NULL;

    default:
      UNREACHABLE("invalid expression tag");
      return NULL;
  }

  UNREACHABLE("should already have returned");
  return NULL;
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
static FbleValue* Eval(FbleArena* arena, Instr* prgm, VStack* vstack)
{
  FbleValue* final_result = NULL;
  ThreadStack* tstack = FbleAlloc(arena, ThreadStack);
  tstack->result = &final_result;
  tstack->instr = prgm;
  tstack->tail = NULL;

  while (tstack != NULL) {
    FbleValue** presult = tstack->result;
    Instr* instr = tstack->instr;
    ThreadStack* tstack_done = tstack;
    tstack = tstack->tail;
    FbleFree(arena, tstack_done);

    switch (instr->tag) {
      case TYPE_TYPE_INSTR: {
        *presult = FbleCopy(arena, &gTypeTypeValue);
        break;
      }

      case VAR_INSTR: {
        VarInstr* var_instr = (VarInstr*)instr;
        *presult = FbleCopy(arena, Get(vstack, var_instr->position));
        break;
      }

      case LET_INSTR: assert(false && "TODO LET_INSTR"); return NULL;

      case STRUCT_TYPE_INSTR: {
        StructTypeInstr* struct_type_instr = (StructTypeInstr*)instr;
        FbleStructTypeValue* value = FbleAlloc(arena, FbleStructTypeValue);
        value->_base.tag = FBLE_STRUCT_TYPE_VALUE;
        value->_base.refcount = 1;
        value->_base.type = FbleCopy(arena, &gTypeTypeValue);
        *presult = &value->_base;

        FbleVectorInit(arena, value->fields);
        for (size_t i = 0; i < struct_type_instr->fields.size; ++i) {
          FbleFieldValue* fv = FbleVectorExtend(arena, value->fields);
          fv->type = NULL;
          fv->name = struct_type_instr->fields.xs[i].name;

          ThreadStack* ntstack = FbleAlloc(arena, ThreadStack);
          ntstack->result = &fv->type;
          ntstack->instr = struct_type_instr->fields.xs[i].instr;
          ntstack->tail = tstack;
          tstack = ntstack;
        }
        break;
      }

      case UNION_TYPE_INSTR: {
        UnionTypeInstr* union_type_instr = (UnionTypeInstr*)instr;
        FbleUnionTypeValue* value = FbleAlloc(arena, FbleUnionTypeValue);
        value->_base.tag = FBLE_UNION_TYPE_VALUE;
        value->_base.refcount = 1;
        value->_base.type = FbleCopy(arena, &gTypeTypeValue);
        *presult = &value->_base;

        FbleVectorInit(arena, value->fields);
        for (size_t i = 0; i < union_type_instr->fields.size; ++i) {
          FbleFieldValue* fv = FbleVectorExtend(arena, value->fields);
          fv->type = NULL;
          fv->name = union_type_instr->fields.xs[i].name;

          ThreadStack* ntstack = FbleAlloc(arena, ThreadStack);
          ntstack->result = &fv->type;
          ntstack->instr = union_type_instr->fields.xs[i].instr;
          ntstack->tail = tstack;
          tstack = ntstack;
        }
        break;
      }
    }
  }
  return final_result;
}

// FreeInstrs --
//   Free the given sequence of instructions.
//
// Inputs:
//   arena - the arena used to allocation the instructions.
//   instrs - the instructions to free.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the instrs.
static void FreeInstrs(FbleArena* arena, Instr* instrs)
{
  switch (instrs->tag) {
    case TYPE_TYPE_INSTR:
    case VAR_INSTR: {
      FbleFree(arena, instrs);
      return;
    }

    case LET_INSTR: {
      LetInstr* let_instr = (LetInstr*)instrs;
      for (size_t i = 0; i < let_instr->bindings.size; ++i) {
        FreeInstrs(arena, let_instr->bindings.xs[i]);
      }
      FbleFree(arena, let_instr->bindings.xs);
      FreeInstrs(arena, let_instr->body);
      FbleFree(arena, let_instr);
      return;
    }

    case STRUCT_TYPE_INSTR: {
      StructTypeInstr* struct_type_instr = (StructTypeInstr*)instrs;
      for (size_t i = 0; i < struct_type_instr->fields.size; ++i) {
        FreeInstrs(arena, struct_type_instr->fields.xs[i].instr);
      }
      FbleFree(arena, struct_type_instr->fields.xs);
      FbleFree(arena, struct_type_instr);
      return;
    }

    case UNION_TYPE_INSTR: {
      UnionTypeInstr* union_type_instr = (UnionTypeInstr*)instrs;
      for (size_t i = 0; i < union_type_instr->fields.size; ++i) {
        FreeInstrs(arena, union_type_instr->fields.xs[i].instr);
      }
      FbleFree(arena, union_type_instr->fields.xs);
      FbleFree(arena, union_type_instr);
      return;
    }
  }
  UNREACHABLE("invalid instruction");
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleArena* arena, FbleExpr* expr)
{
  VStack vstack = {
    .size = 0,
    .capacity = 1,
    .xs = FbleArenaAlloc(arena, sizeof(FbleValue*), FbleAllocMsg(__FILE__, __LINE__)),
  };

  Instr* instrs = NULL;
  FbleValue* type = Compile(arena, NULL, &vstack, expr, &instrs);
  assert(vstack.size == 0);

  if (type == NULL) {
    FbleFree(arena, vstack.xs);
    return NULL;
  }

  FbleRelease(arena, type);

  FbleValue* result = Eval(arena, instrs, &vstack);

  assert(vstack.size == 0);
  FbleFree(arena, vstack.xs);
  FreeInstrs(arena, instrs);
  return result;
}

