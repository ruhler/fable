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
  VAR_TYPE,
  ABSTRACT_TYPE,
  POLY_TYPE,
  POLY_APPLY_TYPE,
} TypeTag;

// Type --
//   A tagged union of type types. All types have the same initial
//   layout as Type. The tag can be used to determine what kind of
//   type this is to get access to additional fields of the type
//   by first casting to that specific type of type.
typedef struct Type {
  TypeTag tag;
  FbleLoc loc;
  int refcount;
} Type;

// TypeV --
//   A vector of Type.
typedef struct {
  size_t size;
  Type** xs;
} TypeV;

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

// StructType -- STRUCT_TYPE
typedef struct {
  Type _base;
  FieldV fields;
} StructType;

// UnionType -- UNION_TYPE
typedef struct {
  Type _base;
  FieldV fields;
} UnionType;

// FuncType -- FUNC_TYPE
typedef struct {
  Type _base;
  FieldV args;
  struct Type* rtype;
} FuncType;

// VarType -- VAR_TYPE
typedef struct {
  Type _base;
  FbleName var;
  Type* value;
} VarType;

// AbstractType -- ABSTRAC_TYPE
typedef Type AbstractType;

// A vector of AbstractType
typedef TypeV AbstractTypeV;

// PolyType -- POLY_TYPE
typedef struct {
  Type _base;
  AbstractTypeV args;
  Type* body;
} PolyType;

// PolyApplyType -- POLY_APPLY_TYPE
typedef struct {
  Type _base;
  Type* poly;
  TypeV args;
} PolyApplyType;

// Vars --
//   Scope of variables visible during type checking.
typedef struct Vars {
  FbleName name;
  Type* type;
  struct Vars* next;
} Vars;

static Type* CopyType(FbleArena* arena, Type* type);
static void FreeType(FbleArena* arena, Type* type);
static Type* Subst(FbleArena* arena, Type* src, AbstractTypeV params, TypeV args);
static Type* Normal(FbleArena* arena, Type* type);
static bool TypesEqual(FbleArena* arena, Type* a, Type* b);
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
        case STRUCT_TYPE: {
          StructType* st = (StructType*)type;
          for (size_t i = 0; i < st->fields.size; ++i) {
            FreeType(arena, st->fields.xs[i].type);
          }
          FbleFree(arena, st->fields.xs);
          FbleFree(arena, st);
          break;
        }

        case UNION_TYPE: {
          UnionType* ut = (UnionType*)type;
          for (size_t i = 0; i < ut->fields.size; ++i) {
            FreeType(arena, ut->fields.xs[i].type);
          }
          FbleFree(arena, ut->fields.xs);
          FbleFree(arena, ut);
          break;
        }

        case FUNC_TYPE: {
          FuncType* ft = (FuncType*)type;
          for (size_t i = 0; i < ft->args.size; ++i) {
            FreeType(arena, ft->args.xs[i].type);
          }
          FbleFree(arena, ft->args.xs);
          FreeType(arena, ft->rtype);
          FbleFree(arena, ft);
          break;
        }

        case VAR_TYPE: {
          VarType* var = (VarType*)type;
          FreeType(arena, var->value);
          FbleFree(arena, var);
          break;
        }

        case ABSTRACT_TYPE: {
          FbleFree(arena, type);
          break;
        }

        case POLY_TYPE: {
          PolyType* pt = (PolyType*)type;
          for (size_t i = 0; i < pt->args.size; ++i) {
            FreeType(arena, pt->args.xs[i]);
          }
          FbleFree(arena, pt->args.xs);
          FreeType(arena, pt->body);
          FbleFree(arena, pt);
          break;
        }

        case POLY_APPLY_TYPE: {
          PolyApplyType* pat = (PolyApplyType*)type;
          FreeType(arena, pat->poly);
          for (size_t i = 0; i < pat->args.size; ++i) {
            FreeType(arena, pat->args.xs[i]);
          }
          FbleFree(arena, pat->args.xs);
          FbleFree(arena, pat);
          break;
        }
      }
    }
  }
}

// Subst --
//   Substitute the given arguments in place of the given parameters in the
//   given type.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the type to substitute into.
//   params - the abstract types to substitute out.
//   args - the concrete types to substitute in.
//
// Results:
//   A type with all occurrences of params replaced with the corresponding
//   args types.
//
// Side effects:
//   The caller is responsible for calling FreeType on the returned type when
//   it is no longer needed.
static Type* Subst(FbleArena* arena, Type* type, AbstractTypeV params, TypeV args)
{
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      StructType* sst = FbleAlloc(arena, StructType);
      sst->_base.tag = STRUCT_TYPE;
      sst->_base.loc = st->_base.loc;
      sst->_base.refcount = 1;
      FbleVectorInit(arena, sst->fields);
      for (size_t i = 0; i < st->fields.size; ++i) {
        Field* field = FbleVectorExtend(arena, sst->fields);
        field->name = st->fields.xs[i].name;
        field->type = Subst(arena, st->fields.xs[i].type, params, args);
      }
      return &sst->_base;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      UnionType* sut = FbleAlloc(arena, UnionType);
      sut->_base.tag = UNION_TYPE;
      sut->_base.loc = ut->_base.loc;
      sut->_base.refcount = 1;
      FbleVectorInit(arena, sut->fields);
      for (size_t i = 0; i < ut->fields.size; ++i) {
        Field* field = FbleVectorExtend(arena, sut->fields);
        field->name = ut->fields.xs[i].name;
        field->type = Subst(arena, ut->fields.xs[i].type, params, args);
      }
      return &sut->_base;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      FuncType* sft = FbleAlloc(arena, FuncType);
      sft->_base.tag = FUNC_TYPE;
      sft->_base.loc = ft->_base.loc;
      sft->_base.refcount = 1;
      FbleVectorInit(arena, sft->args);
      for (size_t i = 0; i < ft->args.size; ++i) {
        Field* arg = FbleVectorExtend(arena, sft->args);
        arg->name = ft->args.xs[i].name;
        arg->type = Subst(arena, ft->args.xs[i].type, params, args);
      }
      sft->rtype = Subst(arena, ft->rtype, params, args);
      return &sft->_base;
    }

    case VAR_TYPE: {
      VarType* vt = (VarType*)type;
      VarType* svt = FbleAlloc(arena, VarType);
      svt->_base.tag = VAR_TYPE;
      svt->_base.loc = vt->_base.loc;
      svt->_base.refcount = 1;
      svt->var = vt->var;
      svt->value = Subst(arena, vt->value, params, args);
      return &svt->_base;
    }

    case ABSTRACT_TYPE: {
      for (size_t i = 0; i < params.size; ++i) {
        if (type == params.xs[i]) {
          return CopyType(arena, args.xs[i]);
        }
      }
      return CopyType(arena, type);
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      PolyType* spt = FbleAlloc(arena, PolyType);
      spt->_base.tag = POLY_TYPE;
      spt->_base.loc = pt->_base.loc;
      spt->_base.refcount = 1;
      FbleVectorInit(arena, spt->args);
      for (size_t i = 0; i < pt->args.size; ++i) {
        FbleVectorAppend(arena, spt->args, CopyType(arena, pt->args.xs[i]));
      }
      spt->body = Subst(arena, pt->body, params, args);
      return &spt->_base;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PolyApplyType* spat = FbleAlloc(arena, PolyApplyType);
      spat->_base.tag = POLY_APPLY_TYPE;
      spat->_base.loc = pat->_base.loc;
      spat->_base.refcount = 1;
      spat->poly = Subst(arena, pat->poly, params, args);
      FbleVectorInit(arena, spat->args);
      for (size_t i = 0; i < pat->args.size; ++i) {
        Type* arg = Subst(arena, pat->args.xs[i], params, args);
        FbleVectorAppend(arena, spat->args, arg);
      }
      return &spat->_base;
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// Normal --
//   Reduce a Type to normal form. Normal form types are struct, union, and
//   func types, but not var types, for example.
//
// Inputs:
//   arena - arena to use for allocations, if any.
//   type - the type to reduce.
//
// Results:
//   The type reduced to normal form.
//
// Side effects:
//   The caller is responsible for calling FreeType on the returned type when
//   it is no longer needed.
static Type* Normal(FbleArena* arena, Type* type)
{
  switch (type->tag) {
    case STRUCT_TYPE: return CopyType(arena, type);
    case UNION_TYPE: return CopyType(arena, type);
    case FUNC_TYPE: return CopyType(arena, type);

    case VAR_TYPE: {
      VarType* var_type = (VarType*)type;
      return Normal(arena, var_type->value);
    }

    case ABSTRACT_TYPE: return CopyType(arena, type);
    case POLY_TYPE: return CopyType(arena, type);

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PolyType* poly = (PolyType*)Normal(arena, pat->poly);
      assert(poly->_base.tag == POLY_TYPE);
      Type* subst = Subst(arena, poly->body, poly->args, pat->args);
      FreeType(arena, &poly->_base);
      return subst;
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
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
static bool TypesEqual(FbleArena* arena, Type* a, Type* b)
{
  if (a == b) {
    return true;
  }

  a = Normal(arena, a);
  b = Normal(arena, b);
  if (a == b) {
    FreeType(arena, a);
    FreeType(arena, b);
    return true;
  }

  if (a->tag != b->tag) {
    FreeType(arena, a);
    FreeType(arena, b);
    return false;
  }

  switch (a->tag) {
    case STRUCT_TYPE: {
      StructType* sta = (StructType*)a;
      StructType* stb = (StructType*)b;
      if (sta->fields.size != stb->fields.size) {
        FreeType(arena, a);
        FreeType(arena, b);
        return false;
      }

      for (size_t i = 0; i < sta->fields.size; ++i) {
        if (!FbleNamesEqual(sta->fields.xs[i].name.name, stb->fields.xs[i].name.name)) {
          FreeType(arena, a);
          FreeType(arena, b);
          return false;
        }

        if (!TypesEqual(arena, sta->fields.xs[i].type, stb->fields.xs[i].type)) {
          FreeType(arena, a);
          FreeType(arena, b);
          return false;
        }
      }

      FreeType(arena, a);
      FreeType(arena, b);
      return true;
    }

    case UNION_TYPE: {
      UnionType* uta = (UnionType*)a;
      UnionType* utb = (UnionType*)b;
      if (uta->fields.size != utb->fields.size) {
        FreeType(arena, a);
        FreeType(arena, b);
        return false;
      }

      for (size_t i = 0; i < uta->fields.size; ++i) {
        if (!FbleNamesEqual(uta->fields.xs[i].name.name, utb->fields.xs[i].name.name)) {
          FreeType(arena, a);
          FreeType(arena, b);
          return false;
        }

        if (!TypesEqual(arena, uta->fields.xs[i].type, utb->fields.xs[i].type)) {
          FreeType(arena, a);
          FreeType(arena, b);
          return false;
        }
      }

      FreeType(arena, a);
      FreeType(arena, b);
      return true;
    }

    case FUNC_TYPE: {
      FuncType* fta = (FuncType*)a;
      FuncType* ftb = (FuncType*)b;
      if (fta->args.size != ftb->args.size) {
        FreeType(arena, a);
        FreeType(arena, b);
        return false;
      }

      for (size_t i = 0; i < fta->args.size; ++i) {
        if (!TypesEqual(arena, fta->args.xs[i].type, ftb->args.xs[i].type)) {
          FreeType(arena, a);
          FreeType(arena, b);
          return false;
        }
      }

      bool rtype_equal = TypesEqual(arena, fta->rtype, ftb->rtype);
      FreeType(arena, a);
      FreeType(arena, b);
      return rtype_equal;
    }

    case VAR_TYPE: {
      UNREACHABLE("var type is not Normal");
      return false;
    }

    case ABSTRACT_TYPE: {
      assert(a != b);
      return false;
    }

    case POLY_TYPE: {
      assert(false && "TODO: TYPE_EQ for POLY_TYPE");
      return false;
    }

    case POLY_APPLY_TYPE: {
      UNREACHABLE("poly apply type is not Normal");
      return false;
    }
  }

  UNREACHABLE("Should never get here");
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
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      fprintf(stderr, "*(");
      const char* comma = "";
      for (size_t i = 0; i < st->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(st->fields.xs[i].type);
        fprintf(stderr, " %s", st->fields.xs[i].name.name);
        comma = ", ";
      }
      fprintf(stderr, ")");
      break;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      fprintf(stderr, "+(");
      const char* comma = "";
      for (size_t i = 0; i < ut->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(ut->fields.xs[i].type);
        fprintf(stderr, " %s", ut->fields.xs[i].name.name);
        comma = ", ";
      }
      fprintf(stderr, ")");
      break;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      fprintf(stderr, "\\(");
      const char* comma = "";
      for (size_t i = 0; i < ft->args.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(ft->args.xs[i].type);
        fprintf(stderr, " %s", ft->args.xs[i].name.name);
        comma = ", ";
      }
      fprintf(stderr, "; ");
      PrintType(ft->rtype);
      fprintf(stderr, ")");
      break;
    }

    case VAR_TYPE: {
      VarType* var_type = (VarType*)type;
      fprintf(stderr, "%s", var_type->var.name);
      break;
    }

    case ABSTRACT_TYPE: {
      fprintf(stderr, "??%p", (void*)type);
      break;
    }

    case POLY_TYPE: {
      assert(false && "TODO: PrintType POLY_TYPE");
      break;
    }

    case POLY_APPLY_TYPE: {
      assert(false && "TODO: PrintType POLY_APPLY_TYPE");
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

      StructType* struct_type = (StructType*)Normal(arena, type);
      if (struct_type->_base.tag != STRUCT_TYPE) {
        FbleReportError("expected a struct type, but found ", &struct_value_expr->type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        FreeType(arena, type);
        FreeType(arena, &struct_type->_base);
        return NULL;
      }

      if (struct_type->fields.size != struct_value_expr->args.size) {
        // TODO: Where should the error message go?
        FbleReportError("expected %i args, but %i were provided\n",
            &expr->loc, struct_type->fields.size, struct_value_expr->args.size);
        FreeType(arena, type);
        FreeType(arena, &struct_type->_base);
        return NULL;
      }

      bool error = false;
      FbleStructValueInstr* instr = FbleAlloc(arena, FbleStructValueInstr);
      instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      FbleVectorInit(arena, instr->fields);
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        Field* field = struct_type->fields.xs + i;

        FbleInstr* mkarg = NULL;
        Type* arg_type = Compile(arena, vars, type_vars, struct_value_expr->args.xs[i], &mkarg);
        error = error || (arg_type == NULL);

        if (arg_type != NULL && !TypesEqual(arena, field->type, arg_type)) {
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

      FreeType(arena, &struct_type->_base);

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

      UnionType* union_type = (UnionType*)Normal(arena, type);
      if (union_type->_base.tag != UNION_TYPE) {
        FbleReportError("expected a union type, but found ", &union_value_expr->type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        FreeType(arena, type);
        FreeType(arena, &union_type->_base);
        return NULL;
      }

      Type* field_type = NULL;
      size_t tag = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        Field* field = union_type->fields.xs + i;
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
        FreeType(arena, &union_type->_base);
        return NULL;
      }

      FbleInstr* mkarg = NULL;
      Type* arg_type = Compile(arena, vars, type_vars, union_value_expr->arg, &mkarg);
      if (arg_type == NULL) {
        FreeType(arena, type);
        FreeType(arena, &union_type->_base);
        return NULL;
      }

      if (!TypesEqual(arena, field_type, arg_type)) {
        FbleReportError("expected type ", &union_value_expr->arg->loc);
        PrintType(field_type);
        fprintf(stderr, ", but found type ");
        PrintType(arg_type);
        fprintf(stderr, "\n");
        FbleFreeInstrs(arena, mkarg);
        FreeType(arena, type);
        FreeType(arena, &union_type->_base);
        FreeType(arena, arg_type);
        return NULL;
      }
      FreeType(arena, arg_type);
      FreeType(arena, &union_type->_base);

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
      if (type == NULL) {
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      // Note: We can safely pretend the type is always a struct, because
      // StructType and UnionType have the same structure.
      StructType* data_type = (StructType*)Normal(arena, type);
      if (data_type->_base.tag == STRUCT_TYPE) {
        access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      } else if (data_type->_base.tag == UNION_TYPE) {
        access->_base.tag = FBLE_UNION_ACCESS_INSTR;
      } else {
        FbleReportError("expected value of type struct or union, but found value of type ", &access_expr->object->loc);
        PrintType(type);
        fprintf(stderr, "\n");

        FreeType(arena, type);
        FreeType(arena, &data_type->_base);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      for (size_t i = 0; i < data_type->fields.size; ++i) {
        if (FbleNamesEqual(access_expr->field.name, data_type->fields.xs[i].name.name)) {
          access->tag = i;
          *instrs = &instr->_base;
          Type* rtype = CopyType(arena, data_type->fields.xs[i].type);
          FreeType(arena, type);
          FreeType(arena, &data_type->_base);
          return rtype;
        }
      }

      FbleReportError("%s is not a field of type ", &access_expr->field.loc, access_expr->field.name);
      PrintType(type);
      fprintf(stderr, "\n");
      FreeType(arena, type);
      FreeType(arena, &data_type->_base);
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

      UnionType* union_type = (UnionType*)Normal(arena, type);
      if (union_type->_base.tag != UNION_TYPE) {
        FbleReportError("expected value of union type, but found value of type ", &cond_expr->condition->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        FbleFreeInstrs(arena, &push->_base);
        FreeType(arena, type);
        FreeType(arena, &union_type->_base);
        return NULL;
      }

      if (union_type->fields.size != cond_expr->choices.size) {
        FbleReportError("expected %d arguments, but %d were provided.\n", &cond_expr->_base.loc, union_type->fields.size, cond_expr->choices.size);
        FbleFreeInstrs(arena, &push->_base);
        FreeType(arena, type);
        FreeType(arena, &union_type->_base);
        return NULL;
      }

      FbleCondInstr* cond_instr = FbleAlloc(arena, FbleCondInstr);
      cond_instr->_base.tag = FBLE_COND_INSTR;
      push->next = &cond_instr->_base;
      FbleVectorInit(arena, cond_instr->choices);

      Type* return_type = NULL;
      for (size_t i = 0; i < cond_expr->choices.size; ++i) {
        if (!FbleNamesEqual(cond_expr->choices.xs[i].name.name, union_type->fields.xs[i].name.name)) {
          FbleReportError("expected tag '%s', but found '%s'.\n",
              &cond_expr->choices.xs[i].name.loc,
              union_type->fields.xs[i].name.name,
              cond_expr->choices.xs[i].name.name);
          FbleFreeInstrs(arena, &push->_base);
          FreeType(arena, return_type);
          FreeType(arena, type);
          FreeType(arena, &union_type->_base);
          return NULL;
        }

        FbleInstr* mkarg = NULL;
        Type* arg_type = Compile(arena, vars, type_vars, cond_expr->choices.xs[i].expr, &mkarg);
        if (arg_type == NULL) {
          FreeType(arena, return_type);
          FreeType(arena, type);
          FreeType(arena, &union_type->_base);
          FbleFreeInstrs(arena, &push->_base);
          return NULL;
        }
        FbleVectorAppend(arena, cond_instr->choices, mkarg);

        if (return_type == NULL) {
          return_type = arg_type;
        } else {
          if (!TypesEqual(arena, return_type, arg_type)) {
            FbleReportError("expected type ", &cond_expr->choices.xs[i].expr->loc);
            PrintType(return_type);
            fprintf(stderr, ", but found ");
            PrintType(arg_type);
            fprintf(stderr, "\n");

            FreeType(arena, type);
            FreeType(arena, &union_type->_base);
            FreeType(arena, return_type);
            FreeType(arena, arg_type);
            FbleFreeInstrs(arena, &push->_base);
            return NULL;
          }
          FreeType(arena, arg_type);
        }
      }
      FreeType(arena, type);
      FreeType(arena, &union_type->_base);

      *instrs = &push->_base;
      return return_type;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      FuncType* type = FbleAlloc(arena, FuncType);
      type->_base.tag = FUNC_TYPE;
      type->_base.loc = expr->loc;
      type->_base.refcount = 1;
      FbleVectorInit(arena, type->args);
      type->rtype = NULL;
      
      Vars nvds[func_value_expr->args.size];
      Vars* nvars = vars;
      bool error = false;
      for (size_t i = 0; i < func_value_expr->args.size; ++i) {
        Field* field = FbleVectorExtend(arena, type->args);
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

      FuncType* func_type = (FuncType*)Normal(arena, type);
      if (func_type->_base.tag != FUNC_TYPE) {
        FbleReportError("cannot perform application on an object of type ", &expr->loc);
        PrintType(type);
        FreeType(arena, type);
        FreeType(arena, &func_type->_base);
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      bool error = false;
      if (func_type->args.size != apply_expr->args.size) {
        FbleReportError("expected %i args, but %i provided\n",
            &expr->loc, func_type->args.size, apply_expr->args.size);
        error = true;
      }

      for (size_t i = 0; i < func_type->args.size && i < apply_expr->args.size; ++i) {
        FbleInstr** mkarg = FbleVectorExtend(arena, push->values);
        *mkarg = NULL;
        Type* arg_type = Compile(arena, nvars, type_vars, apply_expr->args.xs[i], mkarg);
        error = error || (arg_type == NULL);
        if (arg_type != NULL && !TypesEqual(arena, func_type->args.xs[i].type, arg_type)) {
          FbleReportError("expected type ", &apply_expr->args.xs[i]->loc);
          PrintType(func_type->args.xs[i].type);
          fprintf(stderr, ", but found ");
          PrintType(arg_type);
          fprintf(stderr, "\n");
          error = true;
        }
        FreeType(arena, arg_type);
      }

      if (error) {
        FreeType(arena, type);
        FreeType(arena, &func_type->_base);
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      FbleFuncApplyInstr* apply_instr = FbleAlloc(arena, FbleFuncApplyInstr);
      apply_instr->_base.tag = FBLE_FUNC_APPLY_INSTR;
      apply_instr->argc = func_type->args.size;
      push->next = &apply_instr->_base;
      *instrs = &push->_base;
      Type* rtype = CopyType(arena, func_type->rtype);
      FreeType(arena, type);
      FreeType(arena, &func_type->_base);
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

        if (!error && !TypesEqual(arena, nvd[i].type, type)) {
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
  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      StructType* st = FbleAlloc(arena, StructType);
      st->_base.loc = type->loc;
      st->_base.refcount = 1;
      st->_base.tag = STRUCT_TYPE;
      FbleVectorInit(arena, st->fields);
      FbleStructType* struct_type = (FbleStructType*)type;
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleField* field = struct_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          FreeType(arena, &st->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena, st->fields);
        cfield->name = field->name;
        cfield->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, struct_type->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, field->name.name);
            FreeType(arena, &st->_base);
            return NULL;
          }
        }
      }
      return &st->_base;
    }

    case FBLE_UNION_TYPE: {
      UnionType* ut = FbleAlloc(arena, UnionType);
      ut->_base.loc = type->loc;
      ut->_base.refcount = 1;
      ut->_base.tag = UNION_TYPE;
      FbleVectorInit(arena, ut->fields);

      FbleUnionType* union_type = (FbleUnionType*)type;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleField* field = union_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          FreeType(arena, &ut->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena, ut->fields);
        cfield->name = field->name;
        cfield->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, union_type->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, field->name.name);
            FreeType(arena, &ut->_base);
            return NULL;
          }
        }
      }
      return &ut->_base;
    }

    case FBLE_FUNC_TYPE: {
      FuncType* ft = FbleAlloc(arena, FuncType);
      ft->_base.loc = type->loc;
      ft->_base.refcount = 1;
      ft->_base.tag = FUNC_TYPE;
      FbleVectorInit(arena, ft->args);
      ft->rtype = NULL;

      FbleFuncType* func_type = (FbleFuncType*)type;
      for (size_t i = 0; i < func_type->args.size; ++i) {
        FbleField* field = func_type->args.xs + i;

        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          FreeType(arena, &ft->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena, ft->args);
        cfield->name = field->name;
        cfield->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, func_type->args.xs[j].name.name)) {
            FbleReportError("duplicate arg name '%s'\n", &field->name.loc, field->name.name);
            FreeType(arena, &ft->_base);
            return NULL;
          }
        }
      }

      Type* compiled = CompileType(arena, vars, func_type->rtype);
      if (compiled == NULL) {
        FreeType(arena, &ft->_base);
        return NULL;
      }
      ft->rtype = compiled;
      return &ft->_base;
    }

    case FBLE_PROC_TYPE: assert(false && "TODO: FBLE_PROC_TYPE"); return NULL;
    case FBLE_INPUT_TYPE: assert(false && "TODO: FBLE_INPUT_TYPE"); return NULL;
    case FBLE_OUTPUT_TYPE: assert(false && "TODO: FBLE_OUTPUT_TYPE"); return NULL;

    case FBLE_VAR_TYPE: {
      FbleVarType* var_type = (FbleVarType*)type;

      while (vars != NULL && !FbleNamesEqual(var_type->var.name, vars->name.name)) {
        vars = vars->next;
      }

      if (vars == NULL) {
        FbleReportError("variable '%s' not defined\n", &var_type->var.loc, var_type->var.name);
        return NULL;
      }

      VarType* vt = FbleAlloc(arena, VarType);
      vt->_base.loc = type->loc;
      vt->_base.refcount = 1;
      vt->_base.tag = VAR_TYPE;
      vt->var = var_type->var;
      vt->value = CopyType(arena, vars->type);
      return &vt->_base;
    }

    case FBLE_LET_TYPE: {
      FbleLetType* let = (FbleLetType*)type;

      Vars ntvd[let->bindings.size];
      Vars* ntvs = vars;
      bool error = false;
      for (size_t i = 0; i < let->bindings.size; ++i) {
        // TODO: Confirm the kind of the type matches what is specified in the
        // let binding.
        ntvd[i].name = let->bindings.xs[i].name;
        ntvd[i].type = CompileType(arena, vars, let->bindings.xs[i].type);
        error = error || (ntvd[i].type == NULL);
        ntvd[i].next = ntvs;
        ntvs = ntvd + i;
      }

      Type* rtype = NULL;
      if (!error) {
        rtype = CompileType(arena, ntvs, let->body);
      }

      for (size_t i = 0; i < let->bindings.size; ++i) {
        FreeType(arena, ntvd[i].type);
      }
      return rtype;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* poly = (FblePolyType*)type;
      PolyType* pt = FbleAlloc(arena, PolyType);
      pt->_base.tag = POLY_TYPE;
      pt->_base.loc = type->loc;
      pt->_base.refcount = 1;
      FbleVectorInit(arena, pt->args);

      Vars ntvd[poly->args.size];
      Vars* ntvs = vars;
      for (size_t i = 0; i < poly->args.size; ++i) {
        AbstractType* arg = FbleAlloc(arena, AbstractType);
        arg->tag = ABSTRACT_TYPE;
        arg->loc = poly->args.xs[i].name.loc;
        arg->refcount = 1;
        FbleVectorAppend(arena, pt->args, arg);

        ntvd[i].name = poly->args.xs[i].name;
        ntvd[i].type = arg;
        ntvd[i].next = ntvs;
        ntvs = ntvd + i;
      }

      pt->body = CompileType(arena, ntvs, poly->body);
      if (pt->body == NULL) {
        FreeType(arena, &pt->_base);
        return NULL;
      }

      return &pt->_base;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* apply = (FblePolyApplyType*)type;
      PolyApplyType* pat = FbleAlloc(arena, PolyApplyType);
      pat->_base.tag = POLY_APPLY_TYPE;
      pat->_base.loc = type->loc;
      pat->_base.refcount = 1;
      FbleVectorInit(arena, pat->args);

      // TODO: Verify the poly has the right kind and the arg kinds match.
      pat->poly = CompileType(arena, vars, apply->poly);
      bool error = pat->poly == NULL;
      for (size_t i = 0; i < apply->args.size; ++i) {
        Type* arg = CompileType(arena, vars, apply->args.xs[i]);
        FbleVectorAppend(arena, pat->args, arg);
        error = (error || arg == NULL);
      }

      if (error) {
        FreeType(arena, &pat->_base);
        return NULL;
      }

      return &pat->_base;
    }
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
