// eval.c --
//   This file implements the fble evaluation routines.

#include <stdio.h>    // for fprintf, stderr
#include <string.h>   // for memcpy

#include "fble.h"

#define UNREACHABLE(x) assert(false && x)

static FbleValue gTypeTypeValue = {
  .tag = FBLE_TYPE_TYPE_VALUE,
  .refcount = 1,
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
typedef struct VStack {
  FbleValue* value;
  struct VStack* tail;
} VStack;

// InstrTag --
//   Enum used to distinguish among different kinds of instructions.
typedef enum {
  TYPE_TYPE_INSTR,
  VAR_INSTR,
  LET_INSTR,
  STRUCT_TYPE_INSTR,
  STRUCT_VALUE_INSTR,
  STRUCT_ACCESS_INSTR,
  UNION_TYPE_INSTR,
  UNION_VALUE_INSTR,
  UNION_ACCESS_INSTR,

  ACCESS_INSTR,
  POP_INSTR,
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

// PopInstr -- POP_INSTR
//   Pop count values from the value stack.
typedef struct {
  Instr _base;
  size_t count;
} PopInstr;

// LetInstr -- LET_INSTR
//   Evaluate each of the bindings, add the results to the scope, then execute
//   the body.
typedef struct {
  Instr _base;
  InstrV bindings;
  Instr* body;
  PopInstr pop;
} LetInstr;

// StructTypeInstr -- STRUCT_TYPE_INSTR
//   Allocate a struct type, then execute each of the field types.
typedef struct {
  Instr _base;
  FInstrV fields;
} StructTypeInstr;

// StructValueInstr -- STRUCT_VALUE_INSTR
//   Allocate a struct value, then execute each of its arguments.
typedef struct {
  Instr _base;
  InstrV fields;
} StructValueInstr;

// DataAccessInstr -- STRUCT_ACCESS_INSTR or UNION_ACCESS_INSTR
//   Access the tagged field from the object on top of the vstack.
//   This is the second stage of an ACCESS_INSTR
typedef struct {
  Instr _base;
  FbleLoc loc;
  size_t tag;
} DataAccessInstr;

// AccessInstr -- ACCESS_INSTR
//   Access the tagged field from the given object.
typedef struct {
  Instr _base;
  Instr* object;
  DataAccessInstr access;
} AccessInstr;

// UnionTypeInstr -- UNION_TYPE_INSTR
//   Allocate a union type, then execute each of the field types.
typedef struct {
  Instr _base;
  FInstrV fields;
} UnionTypeInstr;

// UnionValueInstr -- UNION_VALUE_INSTR
//   Allocate a union value, then execute its argument.
typedef struct {
  Instr _base;
  size_t tag;
  Instr* mkarg;
} UnionValueInstr;

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

static bool TypesEqual(FbleValue* a, FbleValue* b);
static void PrintType(FbleValue* type);
static bool IsKinded(FbleValue* type);

static ThreadStack* TPush(FbleArena* arena, FbleValue** presult, Instr* instr, ThreadStack* tail); 

static FbleValue* Compile(FbleArena* arena, Vars* vars, VStack* vstack, FbleExpr* expr, Instr** instrs);
static FbleValue* CompileType(FbleArena* arena, Vars* vars, VStack* vstack, FbleType* type);
static FbleValue* Eval(FbleArena* arena, Instr* instrs, VStack* stack);
static void FreeInstrs(FbleArena* arena, Instr* instrs);

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

    case FBLE_STRUCT_TYPE_VALUE: {
      FbleStructTypeValue* sta = (FbleStructTypeValue*)a;
      FbleStructTypeValue* stb = (FbleStructTypeValue*)b;
      if (sta->fields.size != stb->fields.size) {
        return false;
      }

      for (size_t i = 0; i < sta->fields.size; ++i) {
        if (!FbleNamesEqual(sta->fields.xs[i].name.name, stb->fields.xs[i].name.name)) {
          return false;
        }
        
        if (!TypesEqual(sta->fields.xs[i].type, stb->fields.xs[i].type)) {
          return false;
        }
      }
      return true;
    }

    case FBLE_STRUCT_VALUE: UNREACHABLE("not a type"); return false;

    case FBLE_UNION_TYPE_VALUE: {
      FbleUnionTypeValue* uta = (FbleUnionTypeValue*)a;
      FbleUnionTypeValue* utb = (FbleUnionTypeValue*)b;
      if (uta->fields.size != utb->fields.size) {
        return false;
      }

      for (size_t i = 0; i < uta->fields.size; ++i) {
        if (!FbleNamesEqual(uta->fields.xs[i].name.name, utb->fields.xs[i].name.name)) {
          return false;
        }
        
        if (!TypesEqual(uta->fields.xs[i].type, utb->fields.xs[i].type)) {
          return false;
        }
      }
      return true;
    }

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

    case FBLE_STRUCT_TYPE_VALUE: {
      FbleStructTypeValue* st = (FbleStructTypeValue*)type;
      for (size_t i = 0; i < st->fields.size; ++i) {
        if (IsKinded(st->fields.xs[i].type)) {
          return true;
        }
      }
      return false;
    }

    case FBLE_STRUCT_VALUE: UNREACHABLE("not a type"); return false;

    case FBLE_UNION_TYPE_VALUE: {
      FbleUnionTypeValue* ut = (FbleUnionTypeValue*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        if (IsKinded(ut->fields.xs[i].type)) {
          return true;
        }
      }
      return false;
    }

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
static ThreadStack* TPush(FbleArena* arena, FbleValue** presult, Instr* instr, ThreadStack* tail)
{
  ThreadStack* tstack = FbleAlloc(arena, ThreadStack);
  tstack->result = presult;
  tstack->instr = instr;
  tstack->tail = tail;
  return tstack;
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
      return FbleCopy(arena, vars->type);
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;

      // Step 1: Compile, check, and evaluate the types of all the variables.
      FbleValue* types[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        types[i] = CompileType(arena, vars, vstack, let_expr->bindings.xs[i].type);
        if (types[i] == NULL) {
          return NULL;
        }
      }

      // Step 2: Add the new variables to the scope.
      assert(let_expr->bindings.size > 0);
      Vars nvars[let_expr->bindings.size];
      VStack vstack_data[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        nvars[i].name = let_expr->bindings.xs[i].name;
        nvars[i].type = types[i];
        nvars[i].next = vars;
        vars = nvars + i;

        // TODO: Push an abstract value here instead of NULL?
        vstack_data[i].value = NULL;
        vstack_data[i].tail = vstack;
        vstack = vstack_data + i;
      }

      // Step 3: Compile, check, and if appropriate, evaluate the values of
      // the variables.
      LetInstr* instr = FbleAlloc(arena, LetInstr);
      instr->_base.tag = LET_INSTR;
      instr->pop._base.tag = POP_INSTR;
      instr->pop.count = instr->bindings.size;
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
          FbleRelease(arena, type);
          return NULL;
        }
        FbleRelease(arena, type);

        if (IsKinded(types[i])) {
          FbleValue* v = Eval(arena, *prgm, vstack);
          if (v == NULL) {
            return NULL;
          }
          vstack_data[i].value = v;
        }
      }

      *instrs = &instr->_base;
      FbleValue* result = Compile(arena, vars, vstack, let_expr->body, &(instr->body));
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleRelease(arena, nvars[i].type);
        FbleRelease(arena, vstack_data[i].value);
      }
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

        FbleRelease(arena, type);
      }

      *instrs = &instr->_base;
      return FbleCopy(arena, &gTypeTypeValue);
    }

    case FBLE_STRUCT_VALUE_EXPR: {
      FbleStructValueExpr* struct_value_expr = (FbleStructValueExpr*)expr;
      FbleValue* type = CompileType(arena, vars, vstack, struct_value_expr->type);
      if (type == NULL) {
        return NULL;
      }

      if (type->tag != FBLE_STRUCT_TYPE_VALUE) {
        FbleReportError("expected a struct type, but found ", &struct_value_expr->type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        FbleRelease(arena, type);
        return NULL;
      }

      FbleStructTypeValue* struct_type = (FbleStructTypeValue*)type;

      if (struct_type->fields.size != struct_value_expr->args.size) {
        // TODO: Where should the error message go?
        FbleReportError("expected %i args, but %i were provided\n",
            &expr->loc, struct_type->fields.size, struct_value_expr->args.size);
        FbleRelease(arena, type);
        return NULL;
      }

      StructValueInstr* instr = FbleAlloc(arena, StructValueInstr);
      instr->_base.tag = STRUCT_VALUE_INSTR;
      FbleVectorInit(arena, instr->fields);
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleFieldValue* field = struct_type->fields.xs + i;

        Instr* mkarg = NULL;
        FbleValue* arg_type = Compile(arena, vars, vstack, struct_value_expr->args.xs[i], &mkarg);
        if (arg_type == NULL) {
          FbleRelease(arena, type);
          return NULL;
        }

        if (!TypesEqual(field->type, arg_type)) {
          FbleReportError("expected type ", &struct_value_expr->args.xs[i]->loc);
          PrintType(field->type);
          fprintf(stderr, ", but found ");
          PrintType(arg_type);
          fprintf(stderr, "\n");
          FbleRelease(arena, arg_type);
          FbleRelease(arena, type);
          return NULL;
        }

        FbleVectorAppend(arena, instr->fields, mkarg);
        FbleRelease(arena, arg_type);
      }

      *instrs = &instr->_base;
      return type;
    }

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

        FbleRelease(arena, type);
      }

      *instrs = &instr->_base;
      return FbleCopy(arena, &gTypeTypeValue);
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      FbleValue* type = CompileType(arena, vars, vstack, union_value_expr->type);
      if (type == NULL) {
        return NULL;
      }

      if (type->tag != FBLE_UNION_TYPE_VALUE) {
        FbleReportError("expected a union type, but found ", &union_value_expr->type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        FbleRelease(arena, type);
        return NULL;
      }

      FbleUnionTypeValue* union_type = (FbleUnionTypeValue*)type;
      FbleValue* field_type = NULL;
      size_t tag = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleFieldValue* field = union_type->fields.xs + i;
        if (FbleNamesEqual(field->name.name, union_value_expr->field.name)) {
          tag = i;
          field_type = field->type;
          break;
        }
      }

      if (field_type == NULL) {
        FbleReportError("'%s' is not a field of type ", &union_value_expr->field.loc, union_value_expr->field.name);
        PrintType(type);
        fprintf(stderr, "\n");
        FbleRelease(arena, type);
        return NULL;
      }

      Instr* mkarg = NULL;
      FbleValue* arg_type = Compile(arena, vars, vstack, union_value_expr->arg, &mkarg);
      if (arg_type == NULL) {
        FbleRelease(arena, type);
        return NULL;
      }

      if (!TypesEqual(field_type, arg_type)) {
        FbleReportError("expected type ", &union_value_expr->arg->loc);
        PrintType(field_type);
        fprintf(stderr, ", but found type ");
        PrintType(arg_type);
        fprintf(stderr, "\n");
        FbleRelease(arena, arg_type);
        FbleRelease(arena, type);
        return NULL;
      }

      UnionValueInstr* instr = FbleAlloc(arena, UnionValueInstr);
      instr->_base.tag = UNION_VALUE_INSTR;
      instr->tag = tag;
      instr->mkarg = mkarg;
      *instrs = &instr->_base;

      FbleRelease(arena, arg_type);
      return type;
    }

    case FBLE_COND_EXPR: assert(false && "TODO: FBLE_COND_EXPR"); return NULL;

    case FBLE_PROC_TYPE_EXPR: assert(false && "TODO: FBLE_PROC_TYPE_EXPR"); return NULL;
    case FBLE_INPUT_TYPE_EXPR: assert(false && "TODO: FBLE_INPUT_TYPE_EXPR"); return NULL;
    case FBLE_OUTPUT_TYPE_EXPR: assert(false && "TODO: FBLE_OUTPUT_TYPE_EXPR"); return NULL;
    case FBLE_EVAL_EXPR: assert(false && "TODO: FBLE_EVAL_EXPR"); return NULL;
    case FBLE_GET_EXPR: assert(false && "TODO: FBLE_GET_EXPR"); return NULL;
    case FBLE_PUT_EXPR: assert(false && "TODO: FBLE_PUT_EXPR"); return NULL;
    case FBLE_LINK_EXPR: assert(false && "TODO: FBLE_LINK_EXPR"); return NULL;
    case FBLE_EXEC_EXPR: assert(false && "TODO: FBLE_EXEC_EXPR"); return NULL;

    case FBLE_ACCESS_EXPR: {
      FbleAccessExpr* access_expr = (FbleAccessExpr*)expr;

      // Allocate a slot on the variable stack for the intermediate value
      Vars nvars = {
        .name = { .name = "", .loc = expr->loc },
        .type = NULL,
        .next = vars,
      };

      // TODO: Push an abstract value here instead of NULL?
      // Or should we try to compute the actual value?
      VStack nvstack = { .value = NULL, .tail = vstack };

      AccessInstr* instr = FbleAlloc(arena, AccessInstr);
      instr->_base.tag = ACCESS_INSTR;
      instr->access.loc = access_expr->field.loc;
      FbleValue* type = Compile(arena, &nvars, &nvstack, access_expr->object, &instr->object);
      if (type == NULL) {
        return NULL;
      }

      FbleFieldValueV* fields = NULL;
      if (type->tag == FBLE_STRUCT_TYPE_VALUE) {
        instr->access._base.tag = STRUCT_ACCESS_INSTR;
        fields = &((FbleStructTypeValue*)type)->fields;
      } else if (type->tag == FBLE_UNION_TYPE_VALUE) {
        instr->access._base.tag = UNION_ACCESS_INSTR;
        fields = &((FbleUnionTypeValue*)type)->fields;
      } else {
        FbleReportError("expected value of type struct or union, but found value of type ", &access_expr->object->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        FbleRelease(arena, type);
        return NULL;
      }

      for (size_t i = 0; i < fields->size; ++i) {
        if (FbleNamesEqual(access_expr->field.name, fields->xs[i].name.name)) {
          instr->access.tag = i;
          *instrs = &instr->_base;
          FbleRelease(arena, type);
          return FbleCopy(arena, fields->xs[i].type);
        }
      }

      FbleReportError("%s is not a field of type ", &access_expr->field.loc, access_expr->field.name);
      PrintType(type);
      fprintf(stderr, "\n");
      FbleRelease(arena, type);
      return NULL;
    }

    case FBLE_APPLY_EXPR: assert(false && "TODO: FBLE_APPLY_EXPR"); return NULL;

    default:
      UNREACHABLE("invalid expression tag");
      return NULL;
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// CompileType --
//   Convenience function for compiling and evaluating a type.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the list of variables in scope.
//   vstack - the value stack.
//   expr - the type to compile.
//
// Results:
//   The value of the compiled and evaluated type, or NULL in case of error.
//
// Side effects:
//   Prints a message to stderr if the type fails to compile or evalute.
static FbleValue* CompileType(FbleArena* arena, Vars* vars, VStack* vstack, FbleType* type)
{
  Instr* mktype = NULL;
  FbleValue* type_type = Compile(arena, vars, vstack, type, &mktype);
  if (type_type == NULL) {
    return NULL;
  }

  if (type_type->tag != FBLE_TYPE_TYPE_VALUE) {
    FbleRelease(arena, type_type);
    FbleReportError("expected a type, found something else\n", &type->loc);
    return NULL;
  }
  FbleRelease(arena, type_type);

  FbleValue* type_value = Eval(arena, mktype, vstack);
  FreeInstrs(arena, mktype);
  if (type_value == NULL) {
    FbleReportError("failed to evaluate type\n", &type->loc);
    return NULL;
  }
  return type_value;
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
static FbleValue* Eval(FbleArena* arena, Instr* prgm, VStack* vstack_in)
{
  VStack* vstack = vstack_in;
  FbleValue* final_result = NULL;
  ThreadStack* tstack = TPush(arena, &final_result, prgm, NULL);

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
        VStack* v = vstack;
        for (size_t i = 0; i < var_instr->position; ++i) {
          assert(v->tail != NULL);
          v = v->tail;
        }
        *presult = FbleCopy(arena, v->value);
        break;
      }

      case LET_INSTR: {
        LetInstr* let_instr = (LetInstr*)instr;

        tstack = TPush(arena, NULL, &let_instr->pop._base, tstack);
        tstack = TPush(arena, presult, let_instr->body, tstack);

        for (size_t i = 0; i < let_instr->bindings.size; ++i) {
          VStack* nvstack = FbleAlloc(arena, VStack);
          nvstack->value = NULL;
          nvstack->tail = vstack;
          vstack = nvstack;

          tstack = TPush(arena, &vstack->value, let_instr->bindings.xs[i], tstack);
        }
        break;
      }

      case STRUCT_TYPE_INSTR: {
        StructTypeInstr* struct_type_instr = (StructTypeInstr*)instr;
        FbleStructTypeValue* value = FbleAlloc(arena, FbleStructTypeValue);
        value->_base.tag = FBLE_STRUCT_TYPE_VALUE;
        value->_base.refcount = 1;
        value->fields.size = struct_type_instr->fields.size;
        value->fields.xs = FbleArenaAlloc(arena, value->fields.size * sizeof(FbleFieldValue), FbleAllocMsg(__FILE__, __LINE__));
        *presult = &value->_base;

        for (size_t i = 0; i < struct_type_instr->fields.size; ++i) {
          FbleFieldValue* fv = value->fields.xs + i;
          fv->type = NULL;
          fv->name = struct_type_instr->fields.xs[i].name;

          tstack = TPush(arena, &fv->type, struct_type_instr->fields.xs[i].instr, tstack);
        }
        break;
      }

      case STRUCT_VALUE_INSTR: {
        StructValueInstr* struct_value_instr = (StructValueInstr*)instr;
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

      case STRUCT_ACCESS_INSTR: assert(false && "TODO: STRUCT_ACCESS_INSTR"); return NULL;

      case UNION_TYPE_INSTR: {
        UnionTypeInstr* union_type_instr = (UnionTypeInstr*)instr;
        FbleUnionTypeValue* value = FbleAlloc(arena, FbleUnionTypeValue);
        value->_base.tag = FBLE_UNION_TYPE_VALUE;
        value->_base.refcount = 1;
        value->fields.size = union_type_instr->fields.size;
        value->fields.xs = FbleArenaAlloc(arena, value->fields.size * sizeof(FbleFieldValue), FbleAllocMsg(__FILE__, __LINE__));
        *presult = &value->_base;

        for (size_t i = 0; i < union_type_instr->fields.size; ++i) {
          FbleFieldValue* fv = value->fields.xs + i;
          fv->type = NULL;
          fv->name = union_type_instr->fields.xs[i].name;

          tstack = TPush(arena, &fv->type, union_type_instr->fields.xs[i].instr, tstack);
        }
        break;
      }

      case UNION_VALUE_INSTR: {
        UnionValueInstr* union_value_instr = (UnionValueInstr*)instr;
        FbleUnionValue* union_value = FbleAlloc(arena, FbleUnionValue);
        union_value->_base.tag = FBLE_UNION_VALUE;
        union_value->_base.refcount = 1;
        union_value->tag = union_value_instr->tag;
        union_value->arg = NULL;

        *presult = &union_value->_base;

        tstack = TPush(arena, &union_value->arg, union_value_instr->mkarg, tstack);
        break;
      }

      case UNION_ACCESS_INSTR: {
        DataAccessInstr* access_instr = (DataAccessInstr*)instr;

        assert(vstack != NULL);
        FbleUnionValue* value = (FbleUnionValue*)vstack->value;
        assert(value->_base.tag == FBLE_UNION_VALUE);

        if (value->tag != access_instr->tag) {
          FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);

          // Clean up the stacks.
          while (vstack != vstack_in) {
            VStack* ovstack = vstack;
            vstack = vstack->tail;
            FbleRelease(arena, ovstack->value);
            FbleFree(arena, ovstack);
          }

          while (tstack != NULL) {
            ThreadStack* otstack = tstack;
            tstack = tstack->tail;
            FbleFree(arena, otstack);
          }

          return NULL;
        }
        *presult = FbleCopy(arena, value->arg);

        VStack* ovstack = vstack;
        vstack = vstack->tail;
        FbleRelease(arena, ovstack->value);
        FbleFree(arena, ovstack);
      }

      case ACCESS_INSTR: {
        AccessInstr* access_instr = (AccessInstr*)instr;

        VStack* nvstack = FbleAlloc(arena, VStack);
        nvstack->value = NULL;
        nvstack->tail = vstack;
        vstack = nvstack;

        tstack = TPush(arena, presult, &access_instr->access._base, tstack);
        tstack = TPush(arena, &vstack->value, access_instr->object, tstack);
        break;
      }

      case POP_INSTR: {
        PopInstr* pop_instr = (PopInstr*)instr;
        for (size_t i = 0; i < pop_instr->count; ++i) {
          assert(vstack != NULL);
          VStack* ovstack = vstack;
          vstack = vstack->tail;
          FbleRelease(arena, ovstack->value);
          FbleFree(arena, ovstack);
        }
        break;
      }
    }
  }
  assert(vstack == vstack_in);
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
    case VAR_INSTR:
    case POP_INSTR: {
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

    case STRUCT_VALUE_INSTR: {
      StructValueInstr* instr = (StructValueInstr*)instrs;
      for (size_t i = 0; i < instr->fields.size; ++i) {
        FreeInstrs(arena, instr->fields.xs[i]);
      }
      FbleFree(arena, instr->fields.xs);
      FbleFree(arena, instr);
      return;
    }

    case STRUCT_ACCESS_INSTR: assert(false && "TODO: STRUCT_ACCESS_INSTR"); return;

    case UNION_TYPE_INSTR: {
      UnionTypeInstr* union_type_instr = (UnionTypeInstr*)instrs;
      for (size_t i = 0; i < union_type_instr->fields.size; ++i) {
        FreeInstrs(arena, union_type_instr->fields.xs[i].instr);
      }
      FbleFree(arena, union_type_instr->fields.xs);
      FbleFree(arena, union_type_instr);
      return;
    }

    case UNION_VALUE_INSTR: {
      UnionValueInstr* instr = (UnionValueInstr*)instrs;
      FreeInstrs(arena, instr->mkarg);
      FbleFree(arena, instrs);
      return;
    }

    case UNION_ACCESS_INSTR: assert(false && "TODO: UNION_ACCESS_INSTR"); return;

    case ACCESS_INSTR: {
      AccessInstr* access_instr = (AccessInstr*)instrs;
      FreeInstrs(arena, access_instr->object);
      FbleFree(arena, instrs);
      return;
    }
  }
  UNREACHABLE("invalid instruction");
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleArena* arena, FbleExpr* expr)
{
  VStack* vstack = NULL;
  Instr* instrs = NULL;
  FbleValue* type = Compile(arena, NULL, vstack, expr, &instrs);
  if (type == NULL) {
    return NULL;
  }

  FbleRelease(arena, type);

  FbleValue* result = Eval(arena, instrs, vstack);
  FreeInstrs(arena, instrs);
  return result;
}

