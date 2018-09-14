// compile.c --
//   This file implements the fble compilation routines.

#include <stdio.h>    // for fprintf, stderr

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

// TypeTag --
//   A tag used to dinstinguish among different kinds of compiled types.
typedef enum {
  STRUCT_TYPE,
  UNION_TYPE,
  FUNC_TYPE,
} TypeTag;

// Type --
//   A common structure used to describe compiled types.
typedef struct Type {
  TypeTag tag;
  FbleLoc loc;
  int refcount;
} Type;

// Field --
//   A pair of (Type, Name) used to describe type and function arguments.
typedef struct {
  Type* type;
  FbleName name;
} Field;

// FieldV --
//   A vector of Field.
typedef struct {
  size_t size;
  Field* xs;
} FieldV;

// BasicType --
//   A common structure used to describe the basic types: union, struct,
//   function, etc.
typedef struct {
  Type _base;
  FieldV fields;
  struct Type* rtype;
} BasicType;

// Vars --
//   Scope of variables visible during type checking.
typedef struct Vars {
  FbleName name;
  Type* type;
  struct Vars* next;
} Vars;

static Type* CopyType(FbleArena* arena, Type* type);
static void FreeType(FbleArena* arena, Type* type);
static bool TypesEqual(Type* a, Type* b);
static void PrintType(Type* type);

static Type* Compile(FbleArena* arena, Vars* vars, Vars* type_vars, FbleExpr* expr, FbleInstr** instrs);
static Type* CompileType(FbleArena* arena, Vars* vars, FbleType* type);

// CopyType --
//   Makes a (refcount) copy of a compiled type.
//
// Inputs:
//   arena - for allocations.
//   type - the type to copy.
//
// Results:
//   The copied type.
//
// Side effects:
//   The returned type must be freed using FreeType when no longer in use.
static Type* CopyType(FbleArena* arena, Type* type)
{
  assert(type != NULL);
  type->refcount++;
  return type;
}

// FreeType --
//   Frees a (refcount) copy of a compiled type.
//
// Inputs:
//   arena - for deallocations.
//   type - the type to free. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Decrements the refcount for the type and frees it if there are no
//   more references to it.
static void FreeType(FbleArena* arena, Type* type)
{
  if (type != NULL) {
    assert(type->refcount > 0);
    type->refcount--;
    if (type->refcount == 0) {
      switch (type->tag) {
        case STRUCT_TYPE:
        case UNION_TYPE:
        case FUNC_TYPE: {
          BasicType* basic = (BasicType*)type;
          for (size_t i = 0; i < basic->fields.size; ++i) {
            FreeType(arena, basic->fields.xs[i].type);
          }
          FbleFree(arena, basic->fields.xs);
          FreeType(arena, basic->rtype);
          FbleFree(arena, basic);
          break;
        }
      }
    }
  }
}

// TypesEqual --
//   Test whether the two given compiled types are equal.
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
static bool TypesEqual(Type* a, Type* b)
{
  if (a == b) {
    return true;
  }

  if (a->tag != b->tag) {
    return false;
  }

  switch (a->tag) {
    case STRUCT_TYPE:
    case UNION_TYPE:
    case FUNC_TYPE: {
      BasicType* aa = (BasicType*)a;
      BasicType* bb = (BasicType*)b;
      if (aa->fields.size != bb->fields.size) {
        return false;
      }

      for (size_t i = 0; i < aa->fields.size; ++i) {
        if (!FbleNamesEqual(aa->fields.xs[i].name.name, bb->fields.xs[i].name.name)) {
          return false;
        }

        if (!TypesEqual(aa->fields.xs[i].type, bb->fields.xs[i].type)) {
          return false;
        }
      }

      return TypesEqual(aa->rtype, bb->rtype);
    }
  }

  UNREACHABLE("should never get here");
  return false;
}

// PrintType --
//   Print the given compiled type in human readable form to stderr.
//
// Inputs:
//   type - the type to print.
//
// Result:
//   None.
//
// Side effect:
//   Prints the given type in human readable form to stderr.
static void PrintType(Type* type)
{

  switch (type->tag) {
    case STRUCT_TYPE:
    case UNION_TYPE:
    case FUNC_TYPE: {
      char kind = '?';
      switch (type->tag) {
        case STRUCT_TYPE: kind = '*'; break;
        case UNION_TYPE: kind = '+'; break;
        case FUNC_TYPE: kind = '\\'; break;
      }

      BasicType* basic = (BasicType*)type;
      fprintf(stderr, "%c(", kind);
      const char* comma = "";
      for (size_t i = 0; i < basic->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(basic->fields.xs[i].type);
        fprintf(stderr, " %s", basic->fields.xs[i].name.name);
        comma = ", ";
      }

      if (type->tag == FUNC_TYPE) {
        fprintf(stderr, "; ");
        PrintType(basic->rtype);
      }
      fprintf(stderr, ")");
      break;
    }
  }
}

// Compile --
//   Type check and compile the given expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the list of variables in scope.
//   type_vars - the value of type variables in scope.
//   expr - the expression to compile.
//   instrs - output pointer to store generated in structions
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Sets instrs to point to the compiled instructions if the expression is
//   well typed. Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using FreeType
//   when it is no longer needed.
static Type* Compile(FbleArena* arena, Vars* vars, Vars* type_vars, FbleExpr* expr, FbleInstr** instrs)
{
  switch (expr->tag) {
    case FBLE_STRUCT_VALUE_EXPR: {
      FbleStructValueExpr* struct_value_expr = (FbleStructValueExpr*)expr;
      Type* type = CompileType(arena, type_vars, struct_value_expr->type);
      if (type == NULL) {
        return NULL;
      }

      if (type->tag != STRUCT_TYPE) {
        FbleReportError("expected a struct type, but found ", &struct_value_expr->type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        FreeType(arena, type);
        return NULL;
      }

      BasicType* basic = (BasicType*)type;

      if (basic->fields.size != struct_value_expr->args.size) {
        // TODO: Where should the error message go?
        FbleReportError("expected %i args, but %i were provided\n",
            &expr->loc, basic->fields.size, struct_value_expr->args.size);
        FreeType(arena, type);
        return NULL;
      }

      bool error = false;
      FbleStructValueInstr* instr = FbleAlloc(arena, FbleStructValueInstr);
      instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      FbleVectorInit(arena, instr->fields);
      for (size_t i = 0; i < basic->fields.size; ++i) {
        Field* field = basic->fields.xs + i;

        FbleInstr* mkarg = NULL;
        Type* arg_type = Compile(arena, vars, type_vars, struct_value_expr->args.xs[i], &mkarg);
        error = error || (arg_type == NULL);

        if (arg_type != NULL && !TypesEqual(field->type, arg_type)) {
          FbleReportError("expected type ", &struct_value_expr->args.xs[i]->loc);
          PrintType(field->type);
          fprintf(stderr, ", but found ");
          PrintType(arg_type);
          fprintf(stderr, "\n");
          error = true;
        }
        FreeType(arena, arg_type);

        FbleVectorAppend(arena, instr->fields, mkarg);
      }

      if (error) {
        FreeType(arena, type);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      *instrs = &instr->_base;
      return type;
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      Type* type = CompileType(arena, type_vars, union_value_expr->type);
      if (type == NULL) {
        return NULL;
      }

      if (type->tag != UNION_TYPE) {
        FbleReportError("expected a union type, but found ", &union_value_expr->type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        FreeType(arena, type);
        return NULL;
      }

      BasicType* basic = (BasicType*)type;
      Type* field_type = NULL;
      size_t tag = 0;
      for (size_t i = 0; i < basic->fields.size; ++i) {
        Field* field = basic->fields.xs + i;
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
        FreeType(arena, type);
        return NULL;
      }

      FbleInstr* mkarg = NULL;
      Type* arg_type = Compile(arena, vars, type_vars, union_value_expr->arg, &mkarg);
      if (arg_type == NULL) {
        FreeType(arena, type);
        return NULL;
      }

      if (!TypesEqual(field_type, arg_type)) {
        FbleReportError("expected type ", &union_value_expr->arg->loc);
        PrintType(field_type);
        fprintf(stderr, ", but found type ");
        PrintType(arg_type);
        fprintf(stderr, "\n");
        FbleFreeInstrs(arena, mkarg);
        FreeType(arena, type);
        FreeType(arena, arg_type);
        return NULL;
      }
      FreeType(arena, arg_type);

      FbleUnionValueInstr* instr = FbleAlloc(arena, FbleUnionValueInstr);
      instr->_base.tag = FBLE_UNION_VALUE_INSTR;
      instr->tag = tag;
      instr->mkarg = mkarg;
      *instrs = &instr->_base;
      return type;
    }

    case FBLE_ACCESS_EXPR: {
      FbleAccessExpr* access_expr = (FbleAccessExpr*)expr;

      // Allocate a slot on the variable stack for the intermediate value
      Vars nvars = {
        .name = { .name = "", .loc = expr->loc },
        .type = NULL,
        .next = vars,
      };

      FblePushInstr* instr = FbleAlloc(arena, FblePushInstr);
      instr->_base.tag = FBLE_PUSH_INSTR;
      FbleVectorInit(arena, instr->values);
      FbleInstr** mkobj = FbleVectorExtend(arena, instr->values);
      *mkobj = NULL;

      FbleAccessInstr* access = FbleAlloc(arena, FbleAccessInstr);
      access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      instr->next = &access->_base;
      access->loc = access_expr->field.loc;
      Type* type = Compile(arena, &nvars, type_vars, access_expr->object, mkobj);

      if (type != NULL && type->tag == STRUCT_TYPE) {
        access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      } else if (type != NULL && type->tag == UNION_TYPE) {
        access->_base.tag = FBLE_UNION_ACCESS_INSTR;
      } else {
        if (type != NULL) {
          FbleReportError("expected value of type struct or union, but found value of type ", &access_expr->object->loc);
          PrintType(type);
          fprintf(stderr, "\n");
        }

        FreeType(arena, type);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      BasicType* basic = (BasicType*)type;
      for (size_t i = 0; i < basic->fields.size; ++i) {
        if (FbleNamesEqual(access_expr->field.name, basic->fields.xs[i].name.name)) {
          access->tag = i;
          *instrs = &instr->_base;
          Type* rtype = CopyType(arena, basic->fields.xs[i].type);
          FreeType(arena, type);
          return rtype;
        }
      }

      FbleReportError("%s is not a field of type ", &access_expr->field.loc, access_expr->field.name);
      PrintType(type);
      fprintf(stderr, "\n");
      FreeType(arena, type);
      FbleFreeInstrs(arena, &instr->_base);
      return NULL;
    }

    case FBLE_COND_EXPR: {
      FbleCondExpr* cond_expr = (FbleCondExpr*)expr;

      // Allocate a slot on the variable stack for the intermediate value
      Vars nvars = {
        .name = { .name = "", .loc = expr->loc },
        .type = NULL,
        .next = vars,
      };

      FblePushInstr* push = FbleAlloc(arena, FblePushInstr);
      push->_base.tag = FBLE_PUSH_INSTR;
      FbleVectorInit(arena, push->values);
      push->next = NULL;

      FbleInstr** mkobj = FbleVectorExtend(arena, push->values);
      *mkobj = NULL;
      Type* type = Compile(arena, &nvars, type_vars, cond_expr->condition, mkobj);
      if (type == NULL) {
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      if (type->tag != UNION_TYPE) {
        FbleReportError("expected value of union type, but found value of type ", &cond_expr->condition->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        FbleFreeInstrs(arena, &push->_base);
        FreeType(arena, type);
        return NULL;
      }

      BasicType* basic = (BasicType*)type;
      if (basic->fields.size != cond_expr->choices.size) {
        FbleReportError("expected %d arguments, but %d were provided.\n", &cond_expr->_base.loc, basic->fields.size, cond_expr->choices.size);
        FbleFreeInstrs(arena, &push->_base);
        FreeType(arena, type);
        return NULL;
      }

      FbleCondInstr* cond_instr = FbleAlloc(arena, FbleCondInstr);
      cond_instr->_base.tag = FBLE_COND_INSTR;
      push->next = &cond_instr->_base;
      FbleVectorInit(arena, cond_instr->choices);

      Type* return_type = NULL;
      for (size_t i = 0; i < cond_expr->choices.size; ++i) {
        if (!FbleNamesEqual(cond_expr->choices.xs[i].name.name, basic->fields.xs[i].name.name)) {
          FbleReportError("expected tag '%s', but found '%s'.\n",
              &cond_expr->choices.xs[i].name.loc,
              basic->fields.xs[i].name.name,
              cond_expr->choices.xs[i].name.name);
          FbleFreeInstrs(arena, &push->_base);
          FreeType(arena, return_type);
          FreeType(arena, type);
          return NULL;
        }

        FbleInstr* mkarg = NULL;
        Type* arg_type = Compile(arena, vars, type_vars, cond_expr->choices.xs[i].expr, &mkarg);
        if (arg_type == NULL) {
          FreeType(arena, return_type);
          FreeType(arena, type);
          FbleFreeInstrs(arena, &push->_base);
          return NULL;
        }
        FbleVectorAppend(arena, cond_instr->choices, mkarg);

        if (return_type == NULL) {
          return_type = arg_type;
        } else {
          if (!TypesEqual(return_type, arg_type)) {
            FbleReportError("expected type ", &cond_expr->choices.xs[i].expr->loc);
            PrintType(return_type);
            fprintf(stderr, ", but found ");
            PrintType(arg_type);
            fprintf(stderr, "\n");

            FreeType(arena, type);
            FreeType(arena, return_type);
            FreeType(arena, arg_type);
            FbleFreeInstrs(arena, &push->_base);
            return NULL;
          }
          FreeType(arena, arg_type);
        }
      }
      FreeType(arena, type);

      *instrs = &push->_base;
      return return_type;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      BasicType* type = FbleAlloc(arena, BasicType);
      type->_base.tag = FUNC_TYPE;
      type->_base.loc = expr->loc;
      type->_base.refcount = 1;
      FbleVectorInit(arena, type->fields);
      type->rtype = NULL;
      
      Vars nvds[func_value_expr->args.size];
      Vars* nvars = vars;
      bool error = false;
      for (size_t i = 0; i < func_value_expr->args.size; ++i) {
        Field* field = FbleVectorExtend(arena, type->fields);
        field->name = func_value_expr->args.xs[i].name;
        field->type = CompileType(arena, type_vars, func_value_expr->args.xs[i].type);
        error = error || (field->type == NULL);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, func_value_expr->args.xs[j].name.name)) {
            error = true;
            FbleReportError("duplicate arg name '%s'\n", &field->name.loc, field->name.name);
            break;
          }
        }

        nvds[i].name = field->name;
        nvds[i].type = field->type;
        nvds[i].next = nvars;
        nvars = nvds + i;
      }

      FbleFuncValueInstr* instr = FbleAlloc(arena, FbleFuncValueInstr);
      instr->_base.tag = FBLE_FUNC_VALUE_INSTR;
      instr->argc = func_value_expr->args.size;
      instr->body = NULL;
      if (!error) {
        type->rtype = Compile(arena, nvars, type_vars, func_value_expr->body, &instr->body);
        error = error || (type->rtype == NULL);
      }

      if (error) {
        FreeType(arena, &type->_base);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      *instrs = &instr->_base;
      return &type->_base;
    }

    case FBLE_APPLY_EXPR: {
      FbleApplyExpr* apply_expr = (FbleApplyExpr*)expr;

      // Allocate space on the stack for the intermediate values.
      Vars nvd[1 + apply_expr->args.size];
      Vars* nvars = vars;
      for (size_t i = 0; i < 1 + apply_expr->args.size; ++i) {
        nvd[i].type = NULL;
        nvd[i].name.name = "";
        nvd[i].name.loc = expr->loc;
        nvd[i].next = nvars;
        nvars = nvd + i;
      };

      FblePushInstr* push = FbleAlloc(arena, FblePushInstr);
      push->_base.tag = FBLE_PUSH_INSTR;
      FbleVectorInit(arena, push->values);
      push->next = NULL;

      FbleInstr** mkfunc = FbleVectorExtend(arena, push->values);
      *mkfunc = NULL;
      Type* type = Compile(arena, nvars, type_vars, apply_expr->func, mkfunc);
      if (type == NULL) {
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      if (type->tag != FUNC_TYPE) {
        FbleReportError("cannot perform application on an object of type ", &expr->loc);
        PrintType(type);
        FreeType(arena, type);
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      BasicType* basic = (BasicType*)type;
      bool error = false;
      if (basic->fields.size != apply_expr->args.size) {
        FbleReportError("expected %i args, but %i provided\n",
            &expr->loc, basic->fields.size, apply_expr->args.size);
        error = true;
      }

      for (size_t i = 0; i < basic->fields.size && i < apply_expr->args.size; ++i) {
        FbleInstr** mkarg = FbleVectorExtend(arena, push->values);
        *mkarg = NULL;
        Type* arg_type = Compile(arena, nvars, type_vars, apply_expr->args.xs[i], mkarg);
        error = error || (arg_type == NULL);
        if (arg_type != NULL && !TypesEqual(basic->fields.xs[i].type, arg_type)) {
          FbleReportError("expected type ", &apply_expr->args.xs[i]->loc);
          PrintType(basic->fields.xs[i].type);
          fprintf(stderr, ", but found ");
          PrintType(arg_type);
          fprintf(stderr, "\n");
          error = true;
        }
        FreeType(arena, arg_type);
      }

      if (error) {
        FreeType(arena, type);
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      FbleFuncApplyInstr* apply_instr = FbleAlloc(arena, FbleFuncApplyInstr);
      apply_instr->_base.tag = FBLE_FUNC_APPLY_INSTR;
      apply_instr->argc = basic->fields.size;
      push->next = &apply_instr->_base;
      *instrs = &push->_base;
      Type* rtype = CopyType(arena, basic->rtype);
      FreeType(arena, type);
      return rtype;
    }

    case FBLE_EVAL_EXPR: assert(false && "TODO: FBLE_EVAL_EXPR"); return NULL;
    case FBLE_LINK_EXPR: assert(false && "TODO: FBLE_LINK_EXPR"); return NULL;
    case FBLE_EXEC_EXPR: assert(false && "TODO: FBLE_EXEC_EXPR"); return NULL;

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

      FbleVarInstr* instr = FbleAlloc(arena, FbleVarInstr);
      instr->_base.tag = FBLE_VAR_INSTR;
      instr->position = position;
      *instrs = &instr->_base;
      return CopyType(arena, vars->type);
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;
      bool error = false;

      // Evaluate the types of the bindings and set up the new vars.
      Vars nvd[let_expr->bindings.size];
      Vars* nvars = vars;
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        nvd[i].name = let_expr->bindings.xs[i].name;
        nvd[i].type = CompileType(arena, type_vars, let_expr->bindings.xs[i].type);
        error = error || (nvd[i].type == NULL);
        nvd[i].next = nvars;
        nvars = nvd + i;
      }

      // Compile the values of the variables.
      FbleLetInstr* instr = FbleAlloc(arena, FbleLetInstr);
      instr->_base.tag = FBLE_LET_INSTR;
      FbleVectorInit(arena, instr->bindings);
      instr->body = NULL;
      instr->pop._base.tag = FBLE_POP_INSTR;
      instr->pop.count = let_expr->bindings.size;
      instr->weaken._base.tag = FBLE_WEAKEN_INSTR;
      instr->weaken.count = let_expr->bindings.size;

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleInstr* mkval = NULL;
        Type* type = NULL;
        if (!error) {
          type = Compile(arena, nvars, type_vars, let_expr->bindings.xs[i].expr, &mkval);
        }
        error = error || (type == NULL);
        FbleVectorAppend(arena, instr->bindings, mkval);

        if (!error && !TypesEqual(nvd[i].type, type)) {
          error = true;
          FbleReportError("expected type ", &let_expr->bindings.xs[i].expr->loc);
          PrintType(nvd[i].type);
          fprintf(stderr, ", but found ");
          PrintType(type);
          fprintf(stderr, "\n");
        }
        FreeType(arena, type);
      }

      Type* rtype = NULL;
      if (!error) {
        rtype = Compile(arena, nvars, type_vars, let_expr->body, &(instr->body));
        error = (rtype == NULL);
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FreeType(arena, nvd[i].type);
      }

      if (error) {
        FreeType(arena, rtype);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      *instrs = &instr->_base;
      return rtype;
    }

    case FBLE_TYPE_LET_EXPR: {
      FbleTypeLetExpr* let = (FbleTypeLetExpr*)expr;

      Vars ntvd[let->bindings.size];
      Vars* ntvs = type_vars;
      bool error = false;
      for (size_t i = 0; i < let->bindings.size; ++i) {
        // TODO: Confirm the kind of the type matches what is specified in the
        // let binding.
        ntvd[i].name = let->bindings.xs[i].name;
        ntvd[i].type = CompileType(arena, type_vars, let->bindings.xs[i].type);
        error = error || (ntvd[i].type == NULL);
        ntvd[i].next = ntvs;
        ntvs = ntvd + i;
      }

      Type* rtype = NULL;
      if (!error) {
        rtype = Compile(arena, vars, ntvs, let->body, instrs);
      }

      for (size_t i = 0; i < let->bindings.size; ++i) {
        FreeType(arena, ntvd[i].type);
      }
      return rtype;
    }

    case FBLE_POLY_EXPR: assert(false && "TODO: FBLE_POLY_EXPR"); return NULL;
    case FBLE_POLY_APPLY_EXPR: assert(false && "TODO: FBLE_POLY_APPLY_EXPR"); return NULL;

  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// CompileType --
//   Compile and evaluate a type.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the value of type variables in scope.
//   type - the type to compile.
//
// Results:
//   The compiled and evaluated type, or NULL in case of error.
//
// Side effects:
//   Prints a message to stderr if the type fails to compile or evalute.
//   Allocates a reference-counted type that must be freed using FreeType
//   when it is no longer needed.
static Type* CompileType(FbleArena* arena, Vars* vars, FbleType* type)
{
  BasicType* ctype = FbleAlloc(arena, BasicType);
  ctype->_base.loc = type->loc;
  ctype->_base.refcount = 1;
  ctype->_base.tag = STRUCT_TYPE;
  FbleVectorInit(arena, ctype->fields);
  ctype->rtype = NULL;

  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      ctype->_base.tag = STRUCT_TYPE;
      FbleStructType* struct_type = (FbleStructType*)type;
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleField* field = struct_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          FreeType(arena, &ctype->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena, ctype->fields);
        cfield->name = field->name;
        cfield->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, struct_type->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, field->name.name);
            FreeType(arena, &ctype->_base);
            return NULL;
          }
        }
      }
      return &ctype->_base;
    }

    case FBLE_UNION_TYPE: {
      ctype->_base.tag = UNION_TYPE;
      FbleUnionType* union_type = (FbleUnionType*)type;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleField* field = union_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          FreeType(arena, &ctype->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena, ctype->fields);
        cfield->name = field->name;
        cfield->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, union_type->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, field->name.name);
            FreeType(arena, &ctype->_base);
            return NULL;
          }
        }
      }
      return &ctype->_base;
    }

    case FBLE_FUNC_TYPE: {
      ctype->_base.tag = FUNC_TYPE;
      FbleFuncType* func_type = (FbleFuncType*)type;
      for (size_t i = 0; i < func_type->args.size; ++i) {
        FbleField* field = func_type->args.xs + i;

        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          FreeType(arena, &ctype->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena, ctype->fields);
        cfield->name = field->name;
        cfield->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, func_type->args.xs[j].name.name)) {
            FbleReportError("duplicate arg name '%s'\n", &field->name.loc, field->name.name);
            FreeType(arena, &ctype->_base);
            return NULL;
          }
        }
      }

      Type* compiled = CompileType(arena, vars, func_type->rtype);
      if (compiled == NULL) {
        FreeType(arena, &ctype->_base);
        return NULL;
      }
      ctype->rtype = compiled;
      return &ctype->_base;
    }

    case FBLE_PROC_TYPE: assert(false && "TODO: FBLE_PROC_TYPE"); return NULL;
    case FBLE_INPUT_TYPE: assert(false && "TODO: FBLE_INPUT_TYPE"); return NULL;
    case FBLE_OUTPUT_TYPE: assert(false && "TODO: FBLE_OUTPUT_TYPE"); return NULL;

    case FBLE_VAR_TYPE: {
      // TODO: Don't allocate ctype in the first case in this case.
      // TODO: Allocate a VarType instead of a BasicType.
      FreeType(arena, &ctype->_base);
      FbleVarType* var_type = (FbleVarType*)type;

      while (vars != NULL && !FbleNamesEqual(var_type->var.name, vars->name.name)) {
        vars = vars->next;
      }

      if (vars == NULL) {
        FbleReportError("variable '%s' not defined\n", &var_type->var.loc, var_type->var.name);
        return NULL;
      }
      return CopyType(arena, vars->type);
    }

    case FBLE_LET_TYPE: assert(false && "TODO: FBLE_LET_TYPE"); return NULL;
    case FBLE_POLY_TYPE: assert(false && "TODO: FBLE_POLY_TYPE"); return NULL;
    case FBLE_POLY_APPLY_TYPE: assert(false && "TODO: FBLE_POLY_APPLY_TYPE"); return NULL;
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// FbleFreeInstrs -- see documentation in fble-internal.h
void FbleFreeInstrs(FbleArena* arena, FbleInstr* instrs)
{
  if (instrs == NULL) {
    return;
  }

  switch (instrs->tag) {
    case FBLE_VAR_INSTR:
    case FBLE_FUNC_APPLY_INSTR:
    case FBLE_STRUCT_ACCESS_INSTR:
    case FBLE_UNION_ACCESS_INSTR:
    case FBLE_POP_INSTR:
    case FBLE_WEAKEN_INSTR: {
      FbleFree(arena, instrs);
      return;
    }

    case FBLE_LET_INSTR: {
      FbleLetInstr* let_instr = (FbleLetInstr*)instrs;
      for (size_t i = 0; i < let_instr->bindings.size; ++i) {
        FbleFreeInstrs(arena, let_instr->bindings.xs[i]);
      }
      FbleFree(arena, let_instr->bindings.xs);
      FbleFreeInstrs(arena, let_instr->body);
      FbleFree(arena, let_instr);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instrs;
      FbleFreeInstrs(arena, func_value_instr->body);
      FbleFree(arena, func_value_instr);
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* instr = (FbleStructValueInstr*)instrs;
      for (size_t i = 0; i < instr->fields.size; ++i) {
        FbleFreeInstrs(arena, instr->fields.xs[i]);
      }
      FbleFree(arena, instr->fields.xs);
      FbleFree(arena, instr);
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* instr = (FbleUnionValueInstr*)instrs;
      FbleFreeInstrs(arena, instr->mkarg);
      FbleFree(arena, instrs);
      return;
    }

    case FBLE_COND_INSTR: {
      FbleCondInstr* instr = (FbleCondInstr*)instrs;
      for (size_t i = 0; i < instr->choices.size; ++i) {
        FbleFreeInstrs(arena, instr->choices.xs[i]);
      }
      FbleFree(arena, instr->choices.xs);
      FbleFree(arena, instrs);
      return;
    }

    case FBLE_PUSH_INSTR: {
      FblePushInstr* push_instr = (FblePushInstr*)instrs;
      for (size_t i = 0; i < push_instr->values.size; ++i) {
        FbleFreeInstrs(arena, push_instr->values.xs[i]);
      }
      FbleFree(arena, push_instr->values.xs);
      FbleFreeInstrs(arena, push_instr->next);
      FbleFree(arena, instrs);
      return;
    }
  }
  UNREACHABLE("invalid instruction");
}

// FbleCompile -- see documentation in fble-internal.h
FbleInstr* FbleCompile(FbleArena* arena, FbleExpr* expr)
{
  FbleInstr* instrs = NULL;
  Type* type = Compile(arena, NULL, NULL, expr, &instrs);
  if (type == NULL) {
    return NULL;
  }
  FreeType(arena, type);
  return instrs;
}
