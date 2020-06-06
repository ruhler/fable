// compile.c --
//   This file implements the fble compilation routines.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_arg, va_end
#include <stdio.h>    // for fprintf, stderr
#include <string.h>   // for strlen, strcat
#include <stdlib.h>   // for NULL

#include "fble-vector.h"
#include "fble.h"
#include "type.h"
#include "typecheck.h"

#define UNREACHABLE(x) assert(false && x)

// Var --
//   Information about a variable visible during type checking.
//
// name - the name of the variable.
// type - the type of the variable.
//   A reference to the type is owned by this Var.
// used  - true if the variable is used anywhere at runtime, false otherwise.
// accessed - true if the variable is referenced anywhere, false otherwise.
typedef struct {
  FbleName name;
  FbleType* type;
  bool used;
  bool accessed;
} Var;

// VarV --
//   A vector of pointers to vars.
typedef struct {
  size_t size;
  Var** xs;
} VarV;

// Scope --
//   Scope of variables visible during type checking.
//
// Fields:
//   statics - variables captured from the parent scope.
//     Takes ownership of the Vars.
//   vars - stack of local variables in scope order.
//     Takes ownership of the Vars.
//   capture - false if operations on this scope should not have any side
//             effects on the parent scope.
//   parent - the parent of this scope. May be NULL.
typedef struct Scope {
  VarV statics;
  VarV vars;
  bool capture; // TODO: Turn into a vector of FbleVarIndex.
  struct Scope* parent;
} Scope;

static Var* PushVar(FbleArena* arena, Scope* scope, FbleName name, FbleType* type);
static void PopVar(FbleTypeHeap* heap, Scope* scope);
static Var* GetVar(FbleTypeHeap* heap, Scope* scope, FbleName name, bool phantom);

static void InitScope(FbleArena* arena, Scope* scope, bool capture, Scope* parent);
static void FreeScope(FbleTypeHeap* heap, Scope* scope);

static void ReportError(FbleArena* arena, FbleLoc* loc, const char* fmt, ...);
static bool CheckNameSpace(FbleArena* arena, FbleName* name, FbleType* type);

// Tc --
//   A pair of returned type and type checked expression.
typedef struct {
  FbleType* type;
  FbleTc* tc;
} Tc;

// TC_FAILED --
//   Tc returned to indicate that type check has failed.
static Tc TC_FAILED = { .type = NULL, .tc = NULL };

static Tc MkTc(FbleType* type, FbleTc* tc);
static void FreeTc(FbleTypeHeap* heap, Tc tc);
static Tc ProfileBlock(FbleArena* arena, FbleName label, Tc tc);

static Tc TypeCheckExpr(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr);
static Tc TypeCheckList(FbleTypeHeap* heap, Scope* scope, FbleLoc loc, FbleExprV args);
static Tc TypeCheckExec(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr);
static FbleType* TypeCheckType(FbleTypeHeap* heap, Scope* scope, FbleTypeExpr* type);
static FbleType* TypeCheckExprForType(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr);
static FbleTc* TypeCheckProgram(FbleTypeHeap* heap, Scope* scope, FbleProgram* prgm);

// PushVar --
//   Push a variable onto the current scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to push the variable on to.
//   name - the name of the variable.
//   type - the type of the variable.
//
// Results:
//   A pointer to the newly pushed variable. The pointer is owned by the
//   scope. It remains valid until a corresponding PopVar or FreeScope
//   occurs.
//
// Side effects:
//   Pushes a new variable with given name and type onto the scope. Takes
//   ownership of the given type, which will be released when the variable is
//   freed. Does not take owner ship of name. It is the callers responsibility
//   to ensure that 'name' outlives the returned Var.
static Var* PushVar(FbleArena* arena, Scope* scope, FbleName name, FbleType* type)
{
  Var* var = FbleAlloc(arena, Var);
  var->name = name;
  var->type = type;
  var->used = false;
  var->accessed = false;
  FbleVectorAppend(arena, scope->vars, var);
  return var;
}

// PopVar --
//   Pops a var off the given scope.
//
// Inputs:
//   heap - heap to use for allocations.
//   scope - the scope to pop from.
//
// Results:
//   none.
//
// Side effects:
//   Pops the top var off the scope. Invalidates the pointer to the variable
//   originally returned in PushVar.
static void PopVar(FbleTypeHeap* heap, Scope* scope)
{
  FbleArena* arena = heap->arena;

  scope->vars.size--;
  Var* var = scope->vars.xs[scope->vars.size];
  FbleReleaseType(heap, var->type);
  FbleFree(arena, var);
}

// GetVar --
//   Lookup a var in the given scope.
//
// Inputs:
//   heap - heap to use for allocations.
//   scope - the scope to look in.
//   name - the name of the variable.
//   phantom - if true, do not consider the variable to be accessed.
//
// Result:
//   The variable from the scope, or NULL if no such variable was found. The
//   variable is owned by the scope and remains valid until either PopVar is
//   called or the scope is finished.
//
// Side effects:
//   Marks variable as used and for capture if necessary and not phantom.
static Var* GetVar(FbleTypeHeap* heap, Scope* scope, FbleName name, bool phantom)
{
  for (size_t i = 0; i < scope->vars.size; ++i) {
    size_t j = scope->vars.size - i - 1;
    Var* var = scope->vars.xs[j];
    if (FbleNamesEqual(name, var->name)) {
      var->accessed = true;
      if (!phantom) {
        var->used = true;
      }
      return var;
    }
  }

  for (size_t i = 0; i < scope->statics.size; ++i) {
    Var* var = scope->statics.xs[i];
    if (FbleNamesEqual(name, var->name)) {
      var->accessed = true;
      if (!phantom) {
        var->used = true;
      }
      return var;
    }
  }

  if (scope->parent != NULL) {
    Var* var = GetVar(heap, scope->parent, name, !scope->capture || phantom);
    if (var != NULL) {
      if (phantom) {
        // It doesn't matter that we are returning a variable for the wrong
        // scope here. phantom means we won't actually use it ever.
        return var;
      }

      FbleArena* arena = heap->arena;
      Var* captured = FbleAlloc(arena, Var);
      captured->name = var->name;
      captured->type = FbleRetainType(heap, var->type);
      captured->used = !phantom;
      captured->accessed = true;
      FbleVectorAppend(arena, scope->statics, captured);
      return captured;
    }
  }

  return NULL;
}

// InitScope --
//   Initialize a new scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to initialize.
//   capture - true to record variables captured from the parent scope as used.
//   parent - the parent of the scope to initialize. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Initializes scope based on parent. FreeScope should be
//   called to free the allocations for scope. The lifetime of the parent
//   scope must exceed the lifetime of this scope.
static void InitScope(FbleArena* arena, Scope* scope, bool capture, Scope* parent)
{
  FbleVectorInit(arena, scope->statics);
  FbleVectorInit(arena, scope->vars);
  scope->capture = capture;
  scope->parent = parent;
}

// FreeScope --
//   Free memory associated with a Scope.
//
// Inputs:
//   heap - heap to use for allocations
//   scope - the scope to finish.
//
// Results:
//   None.
//
// Side effects:
//   * Frees memory associated with scope.
static void FreeScope(FbleTypeHeap* heap, Scope* scope)
{
  FbleArena* arena = heap->arena;
  for (size_t i = 0; i < scope->statics.size; ++i) {
    FbleReleaseType(heap, scope->statics.xs[i]->type);
    FbleFree(arena, scope->statics.xs[i]);
  }
  FbleFree(arena, scope->statics.xs);

  while (scope->vars.size > 0) {
    PopVar(heap, scope);
  }
  FbleFree(arena, scope->vars.xs);
}

// ReportError --
//   Report a compiler error.
//
//   This uses a printf-like format string. The following format specifiers
//   are supported:
//     %i - size_t
//     %k - FbleKind*
//     %n - FbleName*
//     %t - FbleType*
//   Please add additional format specifiers as needed.
//
// Inputs:
//   arena - arena to use for allocations.
//   loc - the location of the error.
//   fmt - the format string.
//   ... - The var-args for the associated conversion specifiers in fmt.
//
// Results:
//   none.
//
// Side effects:
//   Prints a message to stderr as described by fmt and provided arguments.
static void ReportError(FbleArena* arena, FbleLoc* loc, const char* fmt, ...)
{
  FbleReportError("", loc);

  va_list ap;
  va_start(ap, fmt);

  for (const char* p = strchr(fmt, '%'); p != NULL; p = strchr(fmt, '%')) {
    fprintf(stderr, "%.*s", p - fmt, fmt);

    switch (*(p + 1)) {
      case 'i': {
        size_t x = va_arg(ap, size_t);
        fprintf(stderr, "%d", x);
        break;
      }

      case 'k': {
        FbleKind* kind = va_arg(ap, FbleKind*);
        FblePrintKind(kind);
        break;
      }

      case 'n': {
        FbleName* name = va_arg(ap, FbleName*);
        FblePrintName(stderr, *name);
        break;
      }

      case 't': {
        FbleType* type = va_arg(ap, FbleType*);
        FblePrintType(arena, type);
        break;
      }

      default: {
        UNREACHABLE("Unsupported format conversion.");
        break;
      }
    }
    fmt = p + 2;
  }
  fprintf(stderr, "%s", fmt);
  va_end(ap);
}

// CheckNameSpace --
//   Verify that the namespace of the given name is appropriate for the type
//   of value the name refers to.
//
// Inputs:
//   arena - arena to use for allocations.
//   name - the name in question
//   type - the type of the value refered to by the name.
//
// Results:
//   true if the namespace of the name is consistent with the type. false
//   otherwise.
//
// Side effects:
//   Prints a message to stderr if the namespace and type don't match.
static bool CheckNameSpace(FbleArena* arena, FbleName* name, FbleType* type)
{
  FbleKind* kind = FbleGetKind(arena, type);
  size_t kind_level = FbleGetKindLevel(kind);
  FbleFreeKind(arena, kind);

  bool match = (kind_level == 0 && name->space == FBLE_NORMAL_NAME_SPACE)
            || (kind_level == 1 && name->space == FBLE_TYPE_NAME_SPACE);

  if (!match) {
    ReportError(arena, &name->loc,
        "the namespace of '%n' is not appropriate for something of type %t\n", name, type);
  }
  return match;
}

// MkTc --
//   Convenience function for constructing a tc pair.
//
// Inputs:
//   type - the type to put in the tc.
//   tc - the tc to put in the tc.
//
// Results:
//   A Tc with type and tc as fields.
//
// Side effects:
//   None.
static Tc MkTc(FbleType* type, FbleTc* tc)
{
  Tc x = { .type = type, .tc = tc };
  return x;
}

// FreeTc --
//   Convenience function to free the type and tc fields of a Tc.
//
// Inputs:
//   heap - heap to use for allocations
//   tc - tc to free the fields of. May be TC_FAILED.
//
// Side effects:
//   Frees resources associated with the type and tc fields of tc.
static void FreeTc(FbleTypeHeap* heap, Tc tc)
{
  FbleReleaseType(heap, tc.type);
  FbleFreeTc(heap->arena, tc.tc);
}

// ProfileBlock --
//   Wrap the given tc in a profile block.
//
// Inputs:
//   arena - arena to use for allocations.
//   label - the label of the profiling block.
//   tc - the tc to wrap in a profile block. May be TC_FAILED.
//
// Results:
//   The given tc wrapped in a profile block.
//
// Side effects:
//   Forwards ownership of the type and tc in 'tc' to the returned tc.
//   Does not take ownership of label, makes a copy instead.
static Tc ProfileBlock(FbleArena* arena, FbleName label, Tc tc)
{
  if (tc.type == NULL) {
    assert(tc.tc == NULL);
    return TC_FAILED;
  }

  FbleProfileTc* profile_tc = FbleAlloc(arena, FbleProfileTc);
  profile_tc->_base.tag = FBLE_PROFILE_TC;
  profile_tc->_base.loc = FbleCopyLoc(tc.tc->loc);
  profile_tc->name = FbleCopyName(arena, label);
  profile_tc->body = tc.tc;
  tc.tc = &profile_tc->_base;
  return tc;
}

// TypeCheckExpr --
//   Type check the given expression.
//
// Inputs:
//   heap - heap to use for type allocations.
//   scope - the list of variables in scope.
//   expr - the expression to type check.
//
// Results:
//   The type checked expression, or TC_FAILED if the expression is not well
//   typed.
//
// Side effects:
// * Prints a message to stderr if the expression fails to compile.
// * The caller should call FbleFreeTc when the returned FbleTc is no longer
//   needed and FbleReleaseType when the returned FbleType is no longer
//   needed.
static Tc TypeCheckExpr(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr)
{
  FbleArena* arena = heap->arena;
  switch (expr->tag) {
    case FBLE_STRUCT_TYPE_EXPR:
    case FBLE_UNION_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR: {
      FbleType* type = TypeCheckType(heap, scope, expr);
      if (type == NULL) {
        return TC_FAILED;
      }

      FbleTypeType* type_type = FbleNewType(heap, FbleTypeType, FBLE_TYPE_TYPE, expr->loc);
      type_type->type = type;
      FbleTypeAddRef(heap, &type_type->_base, type_type->type);
      FbleReleaseType(heap, type);

      FbleTypeTc* type_tc = FbleAlloc(arena, FbleTypeTc);
      type_tc->_base.tag = FBLE_TYPE_TC;
      type_tc->_base.loc = FbleCopyLoc(expr->loc);

      return MkTc(&type_type->_base, &type_tc->_base);
    }

    case FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR: {
      UNREACHABLE("TODO: remove this enum option. Use FBLE_MISC_APPLY_EXPR instead");
      return TC_FAILED;
    }

    case FBLE_FUNC_APPLY_EXPR: {
      UNREACHABLE("TODO: remove this enum option. Use FBLE_MISC_APPLY_EXPR instead");
      return TC_FAILED;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleApplyExpr* apply_expr = (FbleApplyExpr*)expr;

      Tc misc = TypeCheckExpr(heap, scope, apply_expr->misc);
      bool error = (misc.type == NULL);

      size_t argc = apply_expr->args.size;
      Tc args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = TypeCheckExpr(heap, scope, apply_expr->args.xs[i]);
        error = error || (args[i].type == NULL);
      }

      if (error) {
        FreeTc(heap, misc);
        for (size_t i = 0; i < argc; ++i) {
          FreeTc(heap, args[i]);
        }
        return TC_FAILED;
      }

      FbleType* normal = FbleNormalType(heap, misc.type);
      if (normal->tag == FBLE_FUNC_TYPE) {
        // FUNC_APPLY
        FbleFuncType* func_type = (FbleFuncType*)normal;
        if (func_type->args.size != argc) {
          ReportError(arena, &expr->loc,
              "expected %i args, but found %i\n",
              func_type->args.size, argc);
          FbleReleaseType(heap, normal);
          FreeTc(heap, misc);
          for (size_t i = 0; i < argc; ++i) {
            FreeTc(heap, args[i]);
          }
          return TC_FAILED;
        }

        for (size_t i = 0; i < argc; ++i) {
          if (!FbleTypesEqual(heap, func_type->args.xs[i], args[i].type)) {
            ReportError(arena, &apply_expr->args.xs[i]->loc,
                "expected type %t, but found %t\n",
                func_type->args.xs[i], args[i].type);
            FbleReleaseType(heap, normal);
            FreeTc(heap, misc);
            for (size_t j = i; j < argc; ++j) {
              FreeTc(heap, args[j]);
            }
            return TC_FAILED;
          }
          FbleReleaseType(heap, args[i].type);
        }

        FbleType* rtype = FbleRetainType(heap, func_type->rtype);
        FbleReleaseType(heap, normal);
        FbleReleaseType(heap, misc.type);

        FbleFuncApplyTc* apply_tc = FbleAlloc(arena, FbleFuncApplyTc);
        apply_tc->_base.tag = FBLE_FUNC_APPLY_TC;
        apply_tc->_base.loc = FbleCopyLoc(expr->loc);
        apply_tc->func = misc.tc;
        FbleVectorInit(arena, apply_tc->args);
        for (size_t i = 0; i < argc; ++i) {
          FbleVectorAppend(arena, apply_tc->args, args[i].tc);
        }

        return MkTc(rtype, &apply_tc->_base);
      }

      if (normal->tag == FBLE_TYPE_TYPE) {
        // FBLE_STRUCT_VALUE_EXPR
        FbleTypeType* type_type = (FbleTypeType*)normal;
        FbleType* vtype = FbleRetainType(heap, type_type->type);
        FbleReleaseType(heap, normal);
        FreeTc(heap, misc);

        FbleStructType* struct_type = (FbleStructType*)FbleNormalType(heap, vtype);
        if (struct_type->_base.tag != FBLE_STRUCT_TYPE) {
          ReportError(arena, &apply_expr->misc->loc,
              "expected a struct type, but found %t\n",
              vtype);
          FbleReleaseType(heap, &struct_type->_base);
          FbleReleaseType(heap, vtype);
          for (size_t i = 0; i < argc; ++i) {
            FreeTc(heap, args[i]);
          }
          return TC_FAILED;
        }

        if (struct_type->fields.size != argc) {
          // TODO: Where should the error message go?
          ReportError(arena, &expr->loc,
              "expected %i args, but %i provided\n",
               struct_type->fields.size, argc);
          FbleReleaseType(heap, &struct_type->_base);
          FbleReleaseType(heap, vtype);
          for (size_t i = 0; i < argc; ++i) {
            FreeTc(heap, args[i]);
          }
          return TC_FAILED;
        }

        bool error = false;
        for (size_t i = 0; i < argc; ++i) {
          FbleTaggedType* field = struct_type->fields.xs + i;

          if (!FbleTypesEqual(heap, field->type, args[i].type)) {
            ReportError(arena, &apply_expr->args.xs[i]->loc,
                "expected type %t, but found %t\n",
                field->type, args[i]);
            error = true;
          }
        }

        FbleReleaseType(heap, &struct_type->_base);

        if (error) {
          FbleReleaseType(heap, vtype);
          for (size_t i = 0; i < argc; ++i) {
            FreeTc(heap, args[i]);
          }
          return TC_FAILED;
        }

        FbleStructValueTc* struct_tc = FbleAlloc(arena, FbleStructValueTc);
        struct_tc->_base.tag = FBLE_STRUCT_VALUE_TC;
        struct_tc->_base.loc = FbleCopyLoc(expr->loc);
        FbleVectorInit(arena, struct_tc->args);
        for (size_t i = 0; i < argc; ++i) {
          FbleReleaseType(heap, args[i].type);
          FbleVectorAppend(arena, struct_tc->args, args[i].tc);
        }

        return MkTc(vtype, &struct_tc->_base);
      }

      ReportError(arena, &expr->loc,
          "expecting a function or struct type, but found something of type %t\n",
          misc.type);
      FreeTc(heap, misc);
      FbleReleaseType(heap, normal);
      for (size_t i = 0; i < argc; ++i) {
        FreeTc(heap, args[i]);
      }
      return TC_FAILED;
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      FbleStructValueImplicitTypeExpr* struct_expr = (FbleStructValueImplicitTypeExpr*)expr;
      FbleStructType* struct_type = FbleNewType(heap, FbleStructType, FBLE_STRUCT_TYPE, expr->loc);
      FbleVectorInit(arena, struct_type->fields);

      size_t argc = struct_expr->args.size;
      Tc args[argc];
      bool error = false;
      for (size_t i = 0; i < argc; ++i) {
        size_t j = argc - i - 1;
        FbleTaggedExpr* arg = struct_expr->args.xs + j;
        args[j] = TypeCheckExpr(heap, scope, arg->expr);
        args[j] = ProfileBlock(arena, arg->name, args[j]);
        error = error || (args[j].type == NULL);
      }

      for (size_t i = 0; i < argc; ++i) {
        FbleTaggedExpr* arg = struct_expr->args.xs + i;
        if (args[i].type != NULL) {
          if (!CheckNameSpace(arena, &arg->name, args[i].type)) {
            error = true;
          }

          FbleTaggedType cfield = {
            .name = arg->name,
            .type = args[i].type
          };
          FbleVectorAppend(arena, struct_type->fields, cfield);
          FbleTypeAddRef(heap, &struct_type->_base, cfield.type);
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(arg->name, struct_expr->args.xs[j].name)) {
            error = true;
            ReportError(arena, &arg->name.loc,
                "duplicate field name '%n'\n",
                &struct_expr->args.xs[j].name);
          }
        }
      }

      if (error) {
        FbleReleaseType(heap, &struct_type->_base);
        for (size_t i = 0; i < argc; ++i) {
          FreeTc(heap, args[i]);
        }
        return TC_FAILED;
      }

      FbleStructValueTc* struct_tc = FbleAlloc(arena, FbleStructValueTc);
      struct_tc->_base.tag = FBLE_STRUCT_VALUE_TC;
      struct_tc->_base.loc = FbleCopyLoc(expr->loc);
      FbleVectorInit(arena, struct_tc->args);
      for (size_t i = 0; i < argc; ++i) {
        FbleReleaseType(heap, args[i].type);
        FbleVectorAppend(arena, struct_tc->args, args[i].tc);
      }

      return MkTc(&struct_type->_base, &struct_tc->_base);
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      FbleType* type = TypeCheckType(heap, scope, union_value_expr->type);
      if (type == NULL) {
        return TC_FAILED;
      }

      FbleUnionType* union_type = (FbleUnionType*)FbleNormalType(heap, type);
      if (union_type->_base.tag != FBLE_UNION_TYPE) {
        ReportError(arena, &union_value_expr->type->loc,
            "expected a union type, but found %t\n", type);
        FbleReleaseType(heap, &union_type->_base);
        FbleReleaseType(heap, type);
        return TC_FAILED;
      }

      FbleType* field_type = NULL;
      size_t tag = -1;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleTaggedType* field = union_type->fields.xs + i;
        if (FbleNamesEqual(field->name, union_value_expr->field.name)) {
          field_type = field->type;
          tag = i;
          break;
        }
      }

      if (field_type == NULL) {
        ReportError(arena, &union_value_expr->field.name.loc,
            "'%n' is not a field of type %t\n",
            &union_value_expr->field, type);
        FbleReleaseType(heap, &union_type->_base);
        FbleReleaseType(heap, type);
        return TC_FAILED;
      }

      Tc arg = TypeCheckExpr(heap, scope, union_value_expr->arg);
      if (arg.type == NULL) {
        FbleReleaseType(heap, &union_type->_base);
        FbleReleaseType(heap, type);
        return TC_FAILED;
      }

      if (!FbleTypesEqual(heap, field_type, arg.type)) {
        ReportError(arena, &union_value_expr->arg->loc,
            "expected type %t, but found type %t\n",
            field_type, arg.type);
        FbleReleaseType(heap, type);
        FbleReleaseType(heap, &union_type->_base);
        FreeTc(heap, arg);
        return TC_FAILED;
      }
      FbleReleaseType(heap, arg.type);
      FbleReleaseType(heap, &union_type->_base);

      FbleUnionValueTc* union_tc = FbleAlloc(arena, FbleUnionValueTc);
      union_tc->_base.tag = FBLE_UNION_VALUE_TC;
      union_tc->_base.loc = FbleCopyLoc(expr->loc);
      union_tc->arg = arg.tc;
      union_tc->tag = tag;

      return MkTc(type, &union_tc->_base);
    }

    case FBLE_STRUCT_ACCESS_EXPR: {
      UNREACHABLE("TODO: remove this enum option. Use FBLE_MISC_ACCESS_EXPR instead");
      return TC_FAILED;
    }

    case FBLE_UNION_ACCESS_EXPR: {
      UNREACHABLE("TODO: remove this enum option. Use FBLE_MISC_ACCESS_EXPR instead");
      return TC_FAILED;
    }

    case FBLE_MISC_ACCESS_EXPR: {
      FbleAccessExpr* access_expr = (FbleAccessExpr*)expr;

      Tc obj = TypeCheckExpr(heap, scope, access_expr->object);
      if (obj.type == NULL) {
        return TC_FAILED;
      }

      FbleType* normal = FbleNormalType(heap, obj.type);
      FbleTaggedTypeV* fields = NULL;
      if (normal->tag == FBLE_STRUCT_TYPE) {
        fields = &((FbleStructType*)normal)->fields;
      } else if (normal->tag == FBLE_UNION_TYPE) {
        fields = &((FbleUnionType*)normal)->fields;
      } else {
        ReportError(arena, &access_expr->object->loc,
            "expected value of type struct or union, but found value of type %t\n",
            obj.type);

        FreeTc(heap, obj);
        FbleReleaseType(heap, normal);
        return TC_FAILED;
      }

      for (size_t i = 0; i < fields->size; ++i) {
        if (FbleNamesEqual(access_expr->field.name, fields->xs[i].name)) {
          FbleType* rtype = FbleRetainType(heap, fields->xs[i].type);
          FbleReleaseType(heap, normal);

          FbleAccessTc* access_tc = FbleAlloc(arena, FbleAccessTc);
          access_tc->_base.tag = (normal->tag == FBLE_STRUCT_TYPE)
            ? FBLE_STRUCT_ACCESS_EXPR
            : FBLE_UNION_ACCESS_EXPR;
          access_tc->_base.loc = FbleCopyLoc(expr->loc);
          access_tc->obj = obj.tc;
          access_tc->tag = i;
          return MkTc(rtype, &access_tc->_base);
        }
      }

      ReportError(arena, &access_expr->field.name.loc,
          "'%n' is not a field of type %t\n",
          &access_expr->field, obj.type);
      FreeTc(heap, obj);
      FbleReleaseType(heap, normal);
      return TC_FAILED;
    }

    case FBLE_UNION_SELECT_EXPR: {
      FbleUnionSelectExpr* select_expr = (FbleUnionSelectExpr*)expr;

      Tc condition = TypeCheckExpr(heap, scope, select_expr->condition);
      if (condition.type == NULL) {
        return NULL;
      }

      FbleUnionType* union_type = (FbleUnionType*)FbleNormalType(heap, condition.type);
      if (union_type->_base.tag != FBLE_UNION_TYPE) {
        ReportError(arena, &select_expr->condition->loc,
            "expected value of union type, but found value of type %t\n",
            condition.type);
        FbleReleaseType(heap, &union_type->_base);
        FreeTc(heap, condition);
        return TC_FAILED;
      }

      FbleType* target = NULL;
      if (select_expr->default_ != NULL) {
        // TODO: Label with profile block ":"?
        FbleType* result = TypeCheckExpr(heap, scope, select_expr->default_);

        if (result == NULL) {
          FbleReleaseType(heap, &union_type->_base);
          return NULL;
        }

        target = result;
      }

      for (size_t i = 0; i < union_type->fields.size; ++i) {
        if (i < select_expr->choices.size && FbleNamesEqual(select_expr->choices.xs[i].name, union_type->fields.xs[i].name)) {
          // TODO: Label with profile block.
          FbleType* result = TypeCheckExpr(heap, scope, select_expr->choices.xs[i].expr);

          if (result == NULL) {
            FbleReleaseType(heap, &union_type->_base);
            FbleReleaseType(heap, target);
            return NULL;
          }

          if (target == NULL) {
            target = result;
          } else {
            if (!FbleTypesEqual(heap, target, result)) {
              ReportError(arena, &select_expr->choices.xs[i].expr->loc,
                  "expected type %t, but found %t\n",
                  target, result);

              FbleReleaseType(heap, result);
              FbleReleaseType(heap, target);
              FbleReleaseType(heap, &union_type->_base);
              return NULL;
            }
            FbleReleaseType(heap, result);
          }
        } else if (select_expr->default_ == NULL) {
          if (i < select_expr->choices.size) {
            ReportError(arena, &select_expr->choices.xs[i].name.loc,
                "expected tag '%n', but found '%n'\n",
                &union_type->fields.xs[i].name, &select_expr->choices.xs[i].name);
          } else {
            ReportError(arena, &expr->loc,
                "missing tag '%n'\n",
                &union_type->fields.xs[i].name);
          }
          FbleReleaseType(heap, &union_type->_base);
          FbleReleaseType(heap, target);
          return NULL;
        } else {
          // Insert a default expr for the current choice, so we can keep
          // track of which fields refer to what choices without having to
          // hold on to the union type.
          FbleTaggedExpr x = { .expr = NULL };
          for (int j = i; j < select_expr->choices.size; ++j) {
            FbleTaggedExpr tmp = select_expr->choices.xs[j];
            select_expr->choices.xs[j] = x;
            x = tmp;
          }
          FbleVectorAppend(arena, select_expr->choices, x);
        }
      }

      if (union_type->fields.size < select_expr->choices.size) {
        ReportError(arena, &select_expr->choices.xs[union_type->fields.size].name.loc,
            "unexpected tag '%n'\n",
            &select_expr->choices.xs[union_type->fields.size]);
        FbleReleaseType(heap, &union_type->_base);
        FbleReleaseType(heap, target);
        return NULL;
      }

      FbleReleaseType(heap, &union_type->_base);
      return target;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      size_t argc = func_value_expr->args.size;

      bool error = false;
      FbleTypeV arg_types;
      FbleVectorInit(arena, arg_types);
      for (size_t i = 0; i < argc; ++i) {
        FbleType* arg_type = TypeCheckType(heap, scope, func_value_expr->args.xs[i].type);
        FbleVectorAppend(arena, arg_types, arg_type);
        error = error || arg_type == NULL;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(func_value_expr->args.xs[i].name, func_value_expr->args.xs[j].name)) {
            error = true;
            ReportError(arena, &func_value_expr->args.xs[i].name.loc,
                "duplicate arg name '%n'\n",
                &func_value_expr->args.xs[i].name);
          }
        }
      }

      if (error) {
        for (size_t i = 0; i < argc; ++i) {
          FbleReleaseType(heap, arg_types.xs[i]);
        }
        FbleFree(arena, arg_types.xs);
        return NULL;
      }

      Scope func_scope;
      InitScope(arena, &func_scope, true, scope);

      for (size_t i = 0; i < argc; ++i) {
        PushVar(arena, &func_scope, func_value_expr->args.xs[i].name, arg_types.xs[i]);
      }

      FbleType* func_result = TypeCheckExpr(heap, &func_scope, func_value_expr->body);
      if (func_result == NULL) {
        FreeScope(heap, &func_scope);
        FbleFree(arena, arg_types.xs);
        return NULL;
      }
      FbleType* type = func_result;

      FbleFuncType* ft = FbleNewType(heap, FbleFuncType, FBLE_FUNC_TYPE, expr->loc);
      ft->args = arg_types;
      ft->rtype = type;
      FbleTypeAddRef(heap, &ft->_base, ft->rtype);
      FbleReleaseType(heap, ft->rtype);

      for (size_t i = 0; i < argc; ++i) {
        FbleTypeAddRef(heap, &ft->_base, arg_types.xs[i]);
      }

      FreeScope(heap, &func_scope);
      return &ft->_base;
    }

    case FBLE_EVAL_EXPR:
    case FBLE_LINK_EXPR:
    case FBLE_EXEC_EXPR: {
      Scope body_scope;
      InitScope(arena, &body_scope, true, scope);

      FbleType* body = TypeCheckExec(heap, &body_scope, expr);
      if (body == NULL) {
        FreeScope(heap, &body_scope);
        return NULL;
      }

      FbleProcType* proc_type = FbleNewType(heap, FbleProcType, FBLE_PROC_TYPE, expr->loc);
      proc_type->type = body;
      FbleTypeAddRef(heap, &proc_type->_base, proc_type->type);
      FbleReleaseType(heap, body);
      FreeScope(heap, &body_scope);
      return &proc_type->_base;
    }

    case FBLE_VAR_EXPR: {
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;
      Var* var = GetVar(heap, scope, var_expr->var, false);
      if (var == NULL) {
        ReportError(arena, &var_expr->var.loc, "variable '%n' not defined\n",
            &var_expr->var);
        return NULL;
      }
      return FbleRetainType(heap, var->type);
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;
      bool error = false;

      // Evaluate the types of the bindings and set up the new vars.
      FbleType* types[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleBinding* binding = let_expr->bindings.xs + i;

        if (binding->type == NULL) {
          assert(binding->kind != NULL);

          // We don't know the type, so create an abstract type variable to
          // represent the type.
          // TODO: It would be nice to pick a more descriptive type for kind
          // level 0 variables. Perhaps: __name@?
          FbleName type_name = binding->name;
          type_name.space = FBLE_TYPE_NAME_SPACE;
          types[i] = FbleNewVarType(heap, binding->name.loc, binding->kind, type_name);
        } else {
          assert(binding->kind == NULL);
          types[i] = TypeCheckType(heap, scope, binding->type);
          error = error || (types[i] == NULL);
        }
        
        if (types[i] != NULL && !CheckNameSpace(arena, &binding->name, types[i])) {
          error = true;
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(let_expr->bindings.xs[i].name, let_expr->bindings.xs[j].name)) {
            ReportError(arena, &let_expr->bindings.xs[i].name.loc,
                "duplicate variable name '%n'\n",
                &let_expr->bindings.xs[i].name);
            error = true;
          }
        }
      }

      Var* vars[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        vars[i] = PushVar(arena, scope, let_expr->bindings.xs[i].name, types[i]);
      }

      // Compile the values of the variables.
      FbleType* defs[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        // TODO: Wrap binding defs in profile blocks by variable name.
        FbleBinding* binding = let_expr->bindings.xs + i;

        defs[i] = NULL;
        if (!error) {
          defs[i] = TypeCheckExpr(heap, scope, binding->expr);
        }
        error = error || (defs[i] == NULL);

        if (!error && binding->type != NULL && !FbleTypesEqual(heap, types[i], defs[i])) {
          error = true;
          ReportError(arena, &binding->expr->loc,
              "expected type %t, but found something of type %t\n",
              types[i], defs[i]);
        } else if (!error && binding->type == NULL) {
          FbleKind* expected_kind = FbleGetKind(arena, types[i]);
          FbleKind* actual_kind = FbleGetKind(arena, defs[i]);
          if (!FbleKindsEqual(expected_kind, actual_kind)) {
            ReportError(arena, &binding->expr->loc,
                "expected kind %k, but found something of kind %k\n",
                expected_kind, actual_kind);
            error = true;
          }
          FbleFreeKind(arena, expected_kind);
          FbleFreeKind(arena, actual_kind);
        }
      }

      // Check to see if this is a recursive let block.
      let_expr->recursive = false;
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        let_expr->recursive = let_expr->recursive || vars[i]->used;
      }

      // Apply the newly computed type values for variables whose types were
      // previously unknown.
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (!error && let_expr->bindings.xs[i].type == NULL) {
          FbleAssignVarType(heap, types[i], defs[i]);
        }
        FbleReleaseType(heap, defs[i]);
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (defs[i] != NULL) {
          if (FbleTypeIsVacuous(heap, types[i])) {
            ReportError(arena, &let_expr->bindings.xs[i].name.loc,
                "%n is vacuous\n", &let_expr->bindings.xs[i].name);
            error = true;
          }
        }
      }

      FbleType* body = NULL;
      if (!error) {
        body = TypeCheckExpr(heap, scope, let_expr->body);
      }

      if (body != NULL) {
        for (size_t i = 0; i < let_expr->bindings.size; ++i) {
          if (!vars[i]->accessed && vars[i]->name.name->str[0] != '_') {
            FbleReportWarning("variable '", &vars[i]->name.loc);
            FblePrintName(stderr, vars[i]->name);
            fprintf(stderr, "' defined but not used\n");
          }
        }
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        PopVar(heap, scope);
      }

      return body;
    }

    case FBLE_MODULE_REF_EXPR: {
      FbleModuleRefExpr* module_ref_expr = (FbleModuleRefExpr*)expr;

      Var* var = GetVar(heap, scope, module_ref_expr->ref.resolved, false);

      // We should have resolved all modules at program load time.
      assert(var != NULL && "module not in scope");
      return FbleRetainType(heap, var->type);
    }

    case FBLE_POLY_EXPR: {
      FblePolyExpr* poly = (FblePolyExpr*)expr;

      if (FbleGetKindLevel(poly->arg.kind) != 1) {
        ReportError(arena, &poly->arg.kind->loc,
            "expected a type kind, but found %k\n",
            poly->arg.kind);
        return NULL;
      }

      if (poly->arg.name.space != FBLE_TYPE_NAME_SPACE) {
        ReportError(arena, &poly->arg.name.loc,
            "the namespace of '%n' is not appropriate for kind %k\n",
            &poly->arg.name, poly->arg.kind);
        return NULL;
      }

      FbleType* arg_type = FbleNewVarType(heap, poly->arg.name.loc, poly->arg.kind, poly->arg.name);
      FbleType* arg = FbleValueOfType(heap, arg_type);
      assert(arg != NULL);

      PushVar(arena, scope, poly->arg.name, arg_type);
      FbleType* body = TypeCheckExpr(heap, scope, poly->body);
      PopVar(heap, scope);

      if (body == NULL) {
        FbleReleaseType(heap, arg);
        return NULL;
      }

      FbleType* pt = FbleNewPolyType(heap, expr->loc, arg, body);
      FbleReleaseType(heap, arg);
      FbleReleaseType(heap, body);
      return pt;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;

      // Note: typeof(poly<arg>) = typeof(poly)<arg>
      // TypeCheckExpr gives us typeof(poly)
      FbleType* poly = TypeCheckExpr(heap, scope, apply->poly);
      if (poly == NULL) {
        return NULL;
      }

      FblePolyKind* poly_kind = (FblePolyKind*)FbleGetKind(arena, poly);
      if (poly_kind->_base.tag != FBLE_POLY_KIND) {
        ReportError(arena, &expr->loc,
            "cannot apply poly args to a basic kinded entity\n");
        FbleFreeKind(arena, &poly_kind->_base);
        FbleReleaseType(heap, poly);
        return NULL;
      }

      // Note: arg_type is typeof(arg)
      FbleType* arg_type = TypeCheckExprForType(heap, scope, apply->arg);
      if (arg_type == NULL) {
        FbleFreeKind(arena, &poly_kind->_base);
        FbleReleaseType(heap, poly);
        return NULL;
      }

      FbleKind* expected_kind = poly_kind->arg;
      FbleKind* actual_kind = FbleGetKind(arena, arg_type);
      if (!FbleKindsEqual(expected_kind, actual_kind)) {
        ReportError(arena, &apply->arg->loc,
            "expected kind %k, but found something of kind %k\n",
            expected_kind, actual_kind);
        FbleFreeKind(arena, &poly_kind->_base);
        FbleFreeKind(arena, actual_kind);
        FbleReleaseType(heap, arg_type);
        FbleReleaseType(heap, poly);
        return NULL;
      }
      FbleFreeKind(arena, actual_kind);
      FbleFreeKind(arena, &poly_kind->_base);

      FbleType* arg = FbleValueOfType(heap, arg_type);
      assert(arg != NULL && "TODO: poly apply arg is a value?");
      FbleReleaseType(heap, arg_type);

      FbleType* pat = FbleNewPolyApplyType(heap, expr->loc, poly, arg);
      FbleReleaseType(heap, arg);
      FbleReleaseType(heap, poly);
      return pat;
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* list_expr = (FbleListExpr*)expr;
      return TypeCheckList(heap, scope, expr->loc, list_expr->args);
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* literal = (FbleLiteralExpr*)expr;

      FbleType* spec = TypeCheckExpr(heap, scope, literal->spec);
      if (spec == NULL) {
        return NULL;
      }

      FbleStructType* normal = (FbleStructType*)FbleNormalType(heap, spec);
      if (normal->_base.tag != FBLE_STRUCT_TYPE) {
        ReportError(arena, &literal->spec->loc,
            "expected a struct value, but literal spec has type %t\n",
            spec);
        FbleReleaseType(heap, spec);
        FbleReleaseType(heap, &normal->_base);
        return NULL;
      }
      FbleReleaseType(heap, &normal->_base);

      size_t n = strlen(literal->word);
      if (n == 0) {
        ReportError(arena, &literal->word_loc,
            "literals must not be empty\n");
        FbleReleaseType(heap, spec);
        return NULL;
      }

      FbleName spec_name = {
        .name = FbleNewString(arena, "__literal_spec"),
        .space = FBLE_NORMAL_NAME_SPACE,
        .loc = literal->spec->loc,
      };
      PushVar(arena, scope, spec_name, spec);

      FbleVarExpr spec_var = {
        ._base = { .tag = FBLE_VAR_EXPR, .loc = literal->spec->loc },
        .var = spec_name
      };

      FbleAccessExpr letters[n];
      FbleExpr* xs[n];
      FbleString* fields[n];
      FbleLoc loc = literal->word_loc;
      for (size_t i = 0; i < n; ++i) {
        char field_str[2] = { literal->word[i], '\0' };
        fields[i] = FbleNewString(arena, field_str);

        letters[i]._base.tag = FBLE_MISC_ACCESS_EXPR;
        letters[i]._base.loc = loc;
        letters[i].object = &spec_var._base;
        letters[i].field.name.name = fields[i];
        letters[i].field.name.space = FBLE_NORMAL_NAME_SPACE;
        letters[i].field.name.loc = loc;
        letters[i].field.tag = FBLE_UNRESOLVED_FIELD_TAG;

        xs[i] = &letters[i]._base;

        if (literal->word[i] == '\n') {
          loc.line++;
          loc.col = 0;
        }
        loc.col++;
      }

      FbleExprV args = { .size = n, .xs = xs, };
      FbleType* result = TypeCheckList(heap, scope, literal->word_loc, args);
      PopVar(heap, scope);

      for (size_t i = 0; i < n; ++i) {
        literal->tags[i] = letters[i].field.tag;
        FbleFreeString(arena, fields[i]);
      }
      FbleFreeString(arena, spec_name.name);

      return result;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// TypeCheckList --
//   Type check a list expression.
//
// Inputs:
//   heap - heap to use for allocations.
//   scope - the list of variables in scope.
//   loc - the location of the list expression.
//   args - the elements of the list expression to compile.
//
// Results:
//   The type of the compiled expression, or NULL if the expression is not
//   well typed.
//
// Side effects:
//   * Resolves field tags and MISC_*_EXPRs.
//   * Prints a message to stderr if the expression fails to type check.
//   * Allocates a reference-counted type that must be freed using
//     FbleReleaseType when it is no longer needed.
//   * Behavior is undefined if there is not at least one list argument.
static FbleType* TypeCheckList(FbleTypeHeap* heap, Scope* scope, FbleLoc loc, FbleExprV args)
{
  // The goal is to desugar a list expression [a, b, c, d] into the
  // following expression:
  // <@ T@>(T@ x, T@ x1, T@ x2, T@ x3)<@ L@>((T@, L@){L@;} cons, L@ nil) {
  //   cons(x, cons(x1, cons(x2, cons(x3, nil))));
  // }<t@>(a, b, c, d)
  assert(args.size > 0 && "empty lists not allowed");
  FbleTypeofExpr typeof_elem = {
    ._base = { .tag = FBLE_TYPEOF_EXPR, .loc = loc },
    .expr = args.xs[0],
  };
  FbleTypeExpr* type = &typeof_elem._base;

  FbleArena* arena = heap->arena;
  FbleBasicKind* basic_kind = FbleAlloc(arena, FbleBasicKind);
  basic_kind->_base.tag = FBLE_BASIC_KIND;
  basic_kind->_base.loc = FbleCopyLoc(loc);
  basic_kind->_base.refcount = 1;
  basic_kind->level = 1;

  FbleName elem_type_name = {
    .name = FbleNewString(arena, "T"),
    .space = FBLE_TYPE_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr elem_type = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = elem_type_name,
  };

  // Generate unique names for the variables x, x0, x1, ...
  size_t num_digits = 0;
  for (size_t x = args.size; x > 0; x /= 10) {
    num_digits++;
  }

  FbleName arg_names[args.size];
  FbleVarExpr arg_values[args.size];
  for (size_t i = 0; i < args.size; ++i) {
    FbleString* name = FbleAllocExtra(arena, FbleString, num_digits + 2);
    name->refcount = 1;
    name->magic = FBLE_STRING_MAGIC;
    name->str[0] = 'x';
    name->str[num_digits+1] = '\0';
    for (size_t j = 0, x = i; j < num_digits; j++, x /= 10) {
      name->str[num_digits - j] = (x % 10) + '0';
    }
    arg_names[i].name = name;
    arg_names[i].space = FBLE_NORMAL_NAME_SPACE;
    arg_names[i].loc = loc;

    arg_values[i]._base.tag = FBLE_VAR_EXPR;
    arg_values[i]._base.loc = loc;
    arg_values[i].var = arg_names[i];
  }

  FbleName list_type_name = {
    .name = FbleNewString(arena, "L"),
    .space = FBLE_TYPE_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr list_type = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = list_type_name,
  };

  FbleTaggedTypeExpr inner_args[2];
  FbleName cons_name = {
    .name = FbleNewString(arena, "cons"),
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr cons = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = cons_name,
  };

  // T@, L@ -> L@
  FbleTypeExpr* cons_arg_types[] = {
    &elem_type._base,
    &list_type._base
  };
  FbleFuncTypeExpr cons_type = {
    ._base = { .tag = FBLE_FUNC_TYPE_EXPR, .loc = loc, },
    .args = { .size = 2, .xs = cons_arg_types },
    .rtype = &list_type._base,
  };

  inner_args[0].type = &cons_type._base;
  inner_args[0].name = cons_name;

  FbleName nil_name = {
    .name = FbleNewString(arena, "nil"),
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr nil = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = nil_name,
  };

  inner_args[1].type = &list_type._base;
  inner_args[1].name = nil_name;

  FbleApplyExpr applys[args.size];
  FbleExpr* all_args[args.size * 2];
  for (size_t i = 0; i < args.size; ++i) {
    applys[i]._base.tag = FBLE_MISC_APPLY_EXPR;
    applys[i]._base.loc = loc;
    applys[i].misc = &cons._base;
    applys[i].args.size = 2;
    applys[i].args.xs = all_args + 2 * i;

    applys[i].args.xs[0] = &arg_values[i]._base;
    applys[i].args.xs[1] = (i + 1 < args.size) ? &applys[i+1]._base : &nil._base;
  }

  FbleFuncValueExpr inner_func = {
    ._base = { .tag = FBLE_FUNC_VALUE_EXPR, .loc = loc },
    .args = { .size = 2, .xs = inner_args },
    .body = (args.size == 0) ? &nil._base : &applys[0]._base,
  };

  FblePolyExpr inner_poly = {
    ._base = { .tag = FBLE_POLY_EXPR, .loc = loc },
    .arg = {
      .kind = &basic_kind->_base,
      .name = list_type_name,
    },
    .body = &inner_func._base,
  };

  FbleTaggedTypeExpr outer_args[args.size];
  for (size_t i = 0; i < args.size; ++i) {
    outer_args[i].type = &elem_type._base;
    outer_args[i].name = arg_names[i];
  }

  FbleFuncValueExpr outer_func = {
    ._base = { .tag = FBLE_FUNC_VALUE_EXPR, .loc = loc },
    .args = { .size = args.size, .xs = outer_args },
    .body = &inner_poly._base,
  };

  FblePolyExpr outer_poly = {
    ._base = { .tag = FBLE_POLY_EXPR, .loc = loc },
    .arg = {
      .kind = &basic_kind->_base,
      .name = elem_type_name,
    },
    .body = &outer_func._base,
  };

  FblePolyApplyExpr apply_type = {
    ._base = { .tag = FBLE_POLY_APPLY_EXPR, .loc = loc },
    .poly = &outer_poly._base,
    .arg = type,
  };

  FbleApplyExpr apply_elems = {
    ._base = { .tag = FBLE_MISC_APPLY_EXPR, .loc = loc },
    .misc = &apply_type._base,
    .args = args,
  };

  FbleExpr* expr = &apply_elems._base;

  FbleType* result = TypeCheckExpr(heap, scope, expr);

  FbleFreeKind(arena, &basic_kind->_base);
  for (size_t i = 0; i < args.size; i++) {
    FbleFreeString(arena, arg_names[i].name);
  }
  FbleFreeString(arena, elem_type_name.name);
  FbleFreeString(arena, list_type_name.name);
  FbleFreeString(arena, cons_name.name);
  FbleFreeString(arena, nil_name.name);
  return result;
}

// TypeCheckExec --
//   Type check the given process expression.
//
// Inputs:
//   heap - heap to use for type allocations.
//   scope - the list of variables in scope.
//   expr - the expression to compile.
//
// Results:
//   The type of the result of executing the process expression.
//   NULL if the expression is not well typed or is not a process expression.
//
// Side effects:
// * Resolves field tags and MISC_*_EXPRs.
// * Prints warning messages to stderr.
// * Prints a message to stderr if the expression fails to compile.
// * The caller should call FbleReleaseType when the returned result is no
//   longer needed.
static FbleType* TypeCheckExec(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr)
{
  FbleArena* arena = heap->arena;
  switch (expr->tag) {
    case FBLE_STRUCT_TYPE_EXPR:
    case FBLE_UNION_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR:
    case FBLE_MISC_APPLY_EXPR:
    case FBLE_FUNC_APPLY_EXPR:
    case FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR:
    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR:
    case FBLE_UNION_VALUE_EXPR:
    case FBLE_STRUCT_ACCESS_EXPR:
    case FBLE_UNION_ACCESS_EXPR:
    case FBLE_MISC_ACCESS_EXPR:
    case FBLE_UNION_SELECT_EXPR:
    case FBLE_FUNC_VALUE_EXPR:
    case FBLE_VAR_EXPR:
    case FBLE_LET_EXPR:
    case FBLE_MODULE_REF_EXPR:
    case FBLE_POLY_EXPR:
    case FBLE_POLY_APPLY_EXPR:
    case FBLE_LIST_EXPR:
    case FBLE_LITERAL_EXPR: {
      FbleType* proc = TypeCheckExpr(heap, scope, expr);
      if (proc == NULL) {
        return NULL;
      }

      FbleProcType* normal = (FbleProcType*)FbleNormalType(heap, proc);
      if (normal->_base.tag != FBLE_PROC_TYPE) {
        ReportError(arena, &expr->loc,
            "expected process, but found expression of type %t\n",
            proc);
        FbleReleaseType(heap, &normal->_base);
        FbleReleaseType(heap, proc);
        return NULL;
      }

      FbleType* rtype = FbleRetainType(heap, normal->type);
      FbleReleaseType(heap, &normal->_base);
      FbleReleaseType(heap, proc);
      return rtype;
    }

    case FBLE_EVAL_EXPR: {
      FbleEvalExpr* eval_expr = (FbleEvalExpr*)expr;
      return TypeCheckExpr(heap, scope, eval_expr->body);
    }

    case FBLE_LINK_EXPR: {
      FbleLinkExpr* link_expr = (FbleLinkExpr*)expr;
      if (FbleNamesEqual(link_expr->get, link_expr->put)) {
        ReportError(arena, &link_expr->put.loc,
            "duplicate port name '%n'\n",
            &link_expr->put);
        return NULL;
      }

      FbleType* port_type = TypeCheckType(heap, scope, link_expr->type);
      if (port_type == NULL) {
        return NULL;
      }

      FbleProcType* get_type = FbleNewType(heap, FbleProcType, FBLE_PROC_TYPE, port_type->loc);
      get_type->type = port_type;
      FbleTypeAddRef(heap, &get_type->_base, get_type->type);

      FbleStructType* unit_type = FbleNewType(heap, FbleStructType, FBLE_STRUCT_TYPE, expr->loc);
      FbleVectorInit(arena, unit_type->fields);

      FbleProcType* unit_proc_type = FbleNewType(heap, FbleProcType, FBLE_PROC_TYPE, expr->loc);
      unit_proc_type->type = &unit_type->_base;
      FbleTypeAddRef(heap, &unit_proc_type->_base, unit_proc_type->type);
      FbleReleaseType(heap, &unit_type->_base);

      FbleFuncType* put_type = FbleNewType(heap, FbleFuncType, FBLE_FUNC_TYPE, expr->loc);
      FbleVectorInit(arena, put_type->args);
      FbleVectorAppend(arena, put_type->args, port_type);
      FbleTypeAddRef(heap, &put_type->_base, port_type);
      FbleReleaseType(heap, port_type);
      put_type->rtype = &unit_proc_type->_base;
      FbleTypeAddRef(heap, &put_type->_base, put_type->rtype);
      FbleReleaseType(heap, &unit_proc_type->_base);

      PushVar(arena, scope, link_expr->get, &get_type->_base);
      PushVar(arena, scope, link_expr->put, &put_type->_base);
      return TypeCheckExec(heap, scope, link_expr->body);
    }

    case FBLE_EXEC_EXPR: {
      FbleExecExpr* exec_expr = (FbleExecExpr*)expr;
      bool error = false;

      // Evaluate the types of the bindings and set up the new vars.
      FbleType* types[exec_expr->bindings.size];
      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        types[i] = TypeCheckType(heap, scope, exec_expr->bindings.xs[i].type);
        error = error || (types[i] == NULL);
      }

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        FbleType* binding = TypeCheckExpr(heap, scope, exec_expr->bindings.xs[i].expr);
        if (binding != NULL) {
          FbleProcType* proc_type = (FbleProcType*)FbleNormalType(heap, binding);
          if (proc_type->_base.tag == FBLE_PROC_TYPE) {
            if (types[i] != NULL && !FbleTypesEqual(heap, types[i], proc_type->type)) {
              error = true;
              ReportError(arena, &exec_expr->bindings.xs[i].expr->loc,
                  "expected type %t!, but found %t\n",
                  types[i], binding);
            }
          } else {
            error = true;
            ReportError(arena, &exec_expr->bindings.xs[i].expr->loc,
                "expected process, but found expression of type %t\n",
                binding);
          }
          FbleReleaseType(heap, &proc_type->_base);
        } else {
          error = true;
        }
        FbleReleaseType(heap, binding);
      }

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        PushVar(arena, scope, exec_expr->bindings.xs[i].name, types[i]);
      }

      FbleType* body = NULL;
      if (!error) {
        body = TypeCheckExec(heap, scope, exec_expr->body);
      }

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        PopVar(heap, scope);
      }

      return body;
    }
  }

  UNREACHABLE("should never get here");
  return NULL;
}

// TypeCheckExprForType --
//   Type check the given expression, ignoring accesses to variables.
//
//   Some times an expression is only use only for its type. We don't want to
//   mark variables referenced by the expression as used, because we don't
//   need to know the value of the variable at runtime. This function type
//   checks an expression without marking variables as used. The variables are
//   marked as 'accessed' though, to avoid emitting warnings about unused
//   variables that are actually used to get their type.
//
// Inputs:
//   heap - heap to use for allocations.
//   scope - the list of variables in scope.
//   expr - the expression to compile.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Resolves field tags and MISC_*_EXPRs.
//   Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using
//   FbleReleaseType when it is no longer needed.
static FbleType* TypeCheckExprForType(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr)
{
  FbleArena* arena = heap->arena;
  Scope nscope;
  InitScope(arena, &nscope, false, scope);

  FbleType* result = TypeCheckExpr(heap, &nscope, expr);
  FreeScope(heap, &nscope);
  return result;
}

// TypeCheckType --
//   Type check a type, returning its value.
//
// Inputs:
//   heap - heap to use for allocations.
//   scope - the value of variables in scope.
//   type - the type to compile.
//
// Results:
//   The type checked and evaluated type, or NULL in case of error.
//
// Side effects:
//   Prints a message to stderr if the type fails to compile or evalute.
//   Allocates a reference-counted type that must be freed using
//   FbleReleaseType when it is no longer needed.
static FbleType* TypeCheckType(FbleTypeHeap* heap, Scope* scope, FbleTypeExpr* type)
{
  FbleArena* arena = heap->arena;
  switch (type->tag) {
    case FBLE_STRUCT_TYPE_EXPR: {
      FbleStructTypeExpr* struct_type = (FbleStructTypeExpr*)type;
      FbleStructType* st = FbleNewType(heap, FbleStructType, FBLE_STRUCT_TYPE, type->loc);
      FbleVectorInit(arena, st->fields);

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleTaggedTypeExpr* field = struct_type->fields.xs + i;
        FbleType* compiled = TypeCheckType(heap, scope, field->type);
        if (compiled == NULL) {
          FbleReleaseType(heap, &st->_base);
          return NULL;
        }

        if (!CheckNameSpace(arena, &field->name, compiled)) {
          FbleReleaseType(heap, compiled);
          FbleReleaseType(heap, &st->_base);
          return NULL;
        }

        FbleTaggedType cfield = {
          .name = field->name,
          .type = compiled
        };
        FbleVectorAppend(arena, st->fields, cfield);

        FbleTypeAddRef(heap, &st->_base, cfield.type);
        FbleReleaseType(heap, cfield.type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name, struct_type->fields.xs[j].name)) {
            ReportError(arena, &field->name.loc,
                "duplicate field name '%n'\n",
                &field->name);
            FbleReleaseType(heap, &st->_base);
            return NULL;
          }
        }
      }
      return &st->_base;
    }

    case FBLE_UNION_TYPE_EXPR: {
      FbleUnionType* ut = FbleNewType(heap, FbleUnionType, FBLE_UNION_TYPE, type->loc);
      FbleVectorInit(arena, ut->fields);

      FbleUnionTypeExpr* union_type = (FbleUnionTypeExpr*)type;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleTaggedTypeExpr* field = union_type->fields.xs + i;
        FbleType* compiled = TypeCheckType(heap, scope, field->type);
        if (compiled == NULL) {
          FbleReleaseType(heap, &ut->_base);
          return NULL;
        }
        FbleTaggedType cfield = {
          .name = field->name,
          .type = compiled
        };
        FbleVectorAppend(arena, ut->fields, cfield);
        FbleTypeAddRef(heap, &ut->_base, cfield.type);
        FbleReleaseType(heap, cfield.type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name, union_type->fields.xs[j].name)) {
            ReportError(arena, &field->name.loc,
                "duplicate field name '%n'\n",
                &field->name);
            FbleReleaseType(heap, &ut->_base);
            return NULL;
          }
        }
      }
      return &ut->_base;
    }

    case FBLE_FUNC_TYPE_EXPR: {
      FbleFuncType* ft = FbleNewType(heap, FbleFuncType, FBLE_FUNC_TYPE, type->loc);
      FbleFuncTypeExpr* func_type = (FbleFuncTypeExpr*)type;

      FbleVectorInit(arena, ft->args);
      ft->rtype = NULL;

      bool error = false;
      for (size_t i = 0; i < func_type->args.size; ++i) {
        FbleType* arg = TypeCheckType(heap, scope, func_type->args.xs[i]);
        if (arg == NULL) {
          error = true;
        } else {
          FbleVectorAppend(arena, ft->args, arg);
          FbleTypeAddRef(heap, &ft->_base, arg);
        }
        FbleReleaseType(heap, arg);
      }

      if (error) {
        FbleReleaseType(heap, &ft->_base);
        return NULL;
      }

      FbleType* rtype = TypeCheckType(heap, scope, func_type->rtype);
      if (rtype == NULL) {
        FbleReleaseType(heap, &ft->_base);
        return NULL;
      }
      ft->rtype = rtype;
      FbleTypeAddRef(heap, &ft->_base, ft->rtype);
      FbleReleaseType(heap, ft->rtype);
      return &ft->_base;
    }

    case FBLE_PROC_TYPE_EXPR: {
      FbleProcType* ut = FbleNewType(heap, FbleProcType, FBLE_PROC_TYPE, type->loc);
      ut->type = NULL;

      FbleProcTypeExpr* unary_type = (FbleProcTypeExpr*)type;
      ut->type = TypeCheckType(heap, scope, unary_type->type);
      if (ut->type == NULL) {
        FbleReleaseType(heap, &ut->_base);
        return NULL;
      }
      FbleTypeAddRef(heap, &ut->_base, ut->type);
      FbleReleaseType(heap, ut->type);
      return &ut->_base;
    }

    case FBLE_TYPEOF_EXPR: {
      FbleTypeofExpr* typeof = (FbleTypeofExpr*)type;
      return TypeCheckExprForType(heap, scope, typeof->expr);
    }

    case FBLE_FUNC_APPLY_EXPR:
    case FBLE_MISC_APPLY_EXPR:
    case FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR:
    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR:
    case FBLE_UNION_VALUE_EXPR:
    case FBLE_STRUCT_ACCESS_EXPR:
    case FBLE_UNION_ACCESS_EXPR:
    case FBLE_MISC_ACCESS_EXPR:
    case FBLE_UNION_SELECT_EXPR:
    case FBLE_FUNC_VALUE_EXPR:
    case FBLE_EVAL_EXPR:
    case FBLE_LINK_EXPR:
    case FBLE_EXEC_EXPR:
    case FBLE_VAR_EXPR:
    case FBLE_MODULE_REF_EXPR:
    case FBLE_LET_EXPR:
    case FBLE_POLY_EXPR:
    case FBLE_POLY_APPLY_EXPR:
    case FBLE_LIST_EXPR:
    case FBLE_LITERAL_EXPR: {
      FbleExpr* expr = type;
      FbleType* type = TypeCheckExprForType(heap, scope, expr);
      if (type == NULL) {
        return NULL;
      }

      FbleType* type_value = FbleValueOfType(heap, type);
      if (type_value == NULL) {
        ReportError(arena, &expr->loc,
            "expected a type, but found value of type %t\n",
            type);
        FbleReleaseType(heap, type);
        return NULL;
      }
      FbleReleaseType(heap, type);
      return type_value;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// TypeCheckProgram --
//   Type check the given program.
//
// Inputs:
//   heap - heap to use for allocations.
//   scope - the list of variables in scope.
//   prgm - the program to type check.
//
// Results:
//   true if the program type checked successfully, false otherwise.
//
// Side effects:
//   Prints warning messages to stderr.
//   Prints a message to stderr if the program fails to compile.
static FbleTc* TypeCheckProgram(FbleTypeHeap* heap, Scope* scope, FbleProgram* prgm)
{
  FbleArena* arena = heap->arena;
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    // TODO: Put each module into its own profiling block.
    FbleType* module = TypeCheckExpr(heap, scope, prgm->modules.xs[i].value);

    if (module == NULL) {
      return false;
    }

    PushVar(arena, scope, prgm->modules.xs[i].name, module);
  }

  FbleType* result = TypeCheckExpr(heap, scope, prgm->main);
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    PopVar(heap, scope);
  }

  FbleReleaseType(heap, result);
  return result != NULL;
}
// FbleTypeCheck -- see documentation in typecheck.h
bool FbleTypeCheck(FbleArena* arena, FbleProgram* program)
{
  Scope scope;
  InitScope(arena, &scope, false, NULL);

  FbleTypeHeap* heap = FbleNewTypeHeap(arena);
  bool ok = TypeCheckProgram(heap, &scope, program);
  FreeScope(heap, &scope);
  FbleFreeTypeHeap(heap);
  return ok;
}

// FbleFreeTc -- see documentation in typecheck.h
void FbleFreeTc(FbleArena* arena, FbleTc* tc)
{
  FbleFreeLoc(arena, tc->loc);
  switch (tc->tag) {
    case FBLE_TYPE_TC: {
      FbleFree(arena, tc);
      return;
    }

    case FBLE_VAR_TC: {
      FbleFree(arena, tc);
      return;
    }

    case FBLE_LET_TC: {
      FbleLetTc* let_tc = (FbleLetTc*)tc;
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        FbleFreeTc(arena, let_tc->bindings.xs[i]);
      }
      FbleFree(arena, let_tc->bindings.xs);
      FbleFreeTc(arena, let_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_STRUCT_VALUE_TC: {
      FbleStructValueTc* struct_tc = (FbleStructValueTc*)tc;
      for (size_t i = 0; i < struct_tc->args.size; ++i) {
        FbleFreeTc(arena, struct_tc->args.xs[i]);
      }
      FbleFree(arena, struct_tc->args.xs);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_STRUCT_ACCESS_TC:
    case FBLE_UNION_ACCESS_TC: {
      FbleAccessTc access_tc = (FbleAccessTc*)tc;
      FbleFreeTc(arena, access_tc->obj);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_UNION_VALUE_TC: {
      FbleUnionValueTc* union_tc = (FbleUnionValueTc*)tc;
      FbleFreeTc(arena, union_tc->arg);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_UNION_SELECT_TC: {
      FbleUnionSelectTc* select_tc = (FbleUnionSelectTc*)tc;
      FbleFreeTc(arena, select_tc->condition);
      for (size_t i = 0; i < select_tc->choices.size; ++i) {
        FbleFreeTc(arena, select_tc->choices.xs[i]);
      }
      FbleFree(arena, select_tc->choices.xs);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)tc;
      FbleFree(arena, func_tc->scope.xs);
      FbleFreeTc(arena, func_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)tc;
      FbleFreeTc(arena, apply_tc->func);
      for (size_t i = 0; i < apply_tc->args.size; ++i) {
        FbleFreeTc(arena, apply_tc->func.xs[i]);
      }
      FbleFree(arena, apply_tc->func.xs);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_LINK_TC: {
      FbleLinkTc* link_tc = (FbleLinkTc*)tc;
      FbleFreeTc(arena, link_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_EXEC_TC: {
      FbleExecTc* exec_tc = (FbleExecTc*)tc;
      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        FbleFreeTc(arena, exec_tc->bindings.xs[i]);
      }
      FbleFree(arena, exec_tc->bindings.xs);
      FbleFreeTc(arena, exec_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_POLY_VALUE_TC: {
      FblePolyValueTc* poly_tc = (FblePolyValueTc*)tc;
      FbleFreeTc(arena, poly_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_POLY_APPLY_TC: {
      FblePolyApplyTc* apply_tc = (FblePolyApplyTc*)tc;
      FbleFreeTc(arena, apply_tc->poly);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_PROFILE_TC: {
      FbleProfileTc* profile_tc = (FbleProfileTc*)tc;
      FbleFreeName(profile_tc->name);
      FbleFreeTc(arena, profile_tc->body);
      FbleFree(arena, tc);
      return;
    }
  }

  UNREACHABLE("should never get here");
}
