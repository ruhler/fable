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
// A variable that is captured from one scope to another will have a separate
// instance of Var for each scope that it is captured in.
//
// name - the name of the variable.
// type - the type of the variable.
//   A reference to the type is owned by this Var.
// used  - true if the variable is used anywhere at runtime, false otherwise.
// accessed - true if the variable is referenced anywhere, false otherwise.
// index - the index of the variable.
typedef struct {
  FbleName name;
  FbleType* type;
  bool used;
  bool accessed;
  FbleVarIndex index;
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
//   captured - Collects the source of variables captured from the parent
//              scope. May be NULL to indicate that operations on this scope
//              should not have any side effects on the parent scope.
//   parent - the parent of this scope. May be NULL.
typedef struct Scope {
  VarV statics;
  VarV vars;
  FbleVarIndexV* captured;
  struct Scope* parent;
} Scope;

static Var* PushVar(FbleArena* arena, Scope* scope, FbleName name, FbleType* type);
static void PopVar(FbleTypeHeap* heap, Scope* scope);
static Var* GetVar(FbleTypeHeap* heap, Scope* scope, FbleName name, bool phantom);

static void InitScope(FbleArena* arena, Scope* scope, FbleVarIndexV* captured, Scope* parent);
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

static FbleTc* NewListTc(FbleArena* arena, FbleLoc loc, FbleTcV args);

static Tc TypeCheckExpr(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr);
static Tc TypeCheckExec(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr);
static FbleType* TypeCheckType(FbleTypeHeap* heap, Scope* scope, FbleTypeExpr* type);
static FbleType* TypeCheckExprForType(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr);
static FbleTc* TypeCheckProgram(FbleTypeHeap* heap, Scope* scope, FbleModule* modules, size_t modulec, FbleExpr* body);

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
  var->index.source = FBLE_LOCAL_VAR;
  var->index.index = scope->vars.size;
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
    Var* var = GetVar(heap, scope->parent, name, scope->captured == NULL || phantom);
    if (var != NULL) {
      if (phantom) {
        // It doesn't matter that we are returning a variable for the wrong
        // scope here. phantom means we won't actually use it ever.
        return var;
      }

      FbleArena* arena = heap->arena;
      Var* captured_var = FbleAlloc(arena, Var);
      captured_var->name = var->name;
      captured_var->type = FbleRetainType(heap, var->type);
      captured_var->used = !phantom;
      captured_var->accessed = true;
      captured_var->index.source = FBLE_STATIC_VAR;
      captured_var->index.index = scope->statics.size;
      FbleVectorAppend(arena, scope->statics, captured_var);
      if (scope->captured != NULL) {
        FbleVectorAppend(arena, *scope->captured, var->index);
      }
      return captured_var;
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
//   captured - Collects the source of variables captured from the parent
//              scope. May be NULL to indicate that operations on this scope
//              should not have any side effects on the parent scope.
//   parent - the parent of the scope to initialize. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Initializes scope based on parent. FreeScope should be
//   called to free the allocations for scope. The lifetime of the parent
//   scope must exceed the lifetime of this scope.
static void InitScope(FbleArena* arena, Scope* scope, FbleVarIndexV* captured, Scope* parent)
{
  FbleVectorInit(arena, scope->statics);
  FbleVectorInit(arena, scope->vars);
  scope->captured = captured;
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

// NewListTc --
//   Create an FbleTc representing an fble list expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   loc - the location of the list expression.
//   args - the elements of the list. Must be non-empty.
//
// Results:
//   The newly created FbleTc representing the list expression.
//
// Side effects:
// * Allocates an FbleTc that should be freed with FbleFreeTc when no longer
//   needed.
// * Transfers ownership of the args to the returned FbleTc. Does not take
//   ownership of the args.xs array.
// * Does not take ownership of loc, makes a copy instead.
// * Behavior is undefined if there is not at least one list argument.
static FbleTc* NewListTc(FbleArena* arena, FbleLoc loc, FbleTcV args)
{
  // The goal is to desugar a list expression [a, b, c, d] into the
  // following expression:
  // <@ T@>(T@ x, T@ x1, T@ x2, T@ x3)<@ L@>((T@, L@){L@;} cons, L@ nil) {
  //   cons(x, cons(x1, cons(x2, cons(x3, nil))));
  // }<t@>(a, b, c, d)
  assert(args.size > 0 && "empty lists not allowed");

  FbleVarTc* nil = FbleAlloc(arena, FbleVarTc);
  nil->_base.tag = FBLE_VAR_TC;
  nil->_base.loc = FbleCopyLoc(loc);
  nil->index.source = FBLE_LOCAL_VAR;
  nil->index.index = 1;

  FbleTc* applys = &nil->_base;
  for (size_t i = 0; i < args.size; ++i) {
    FbleVarTc* cons = FbleAlloc(arena, FbleVarTc);
    cons->_base.tag = FBLE_VAR_TC;
    cons->_base.loc = FbleCopyLoc(loc);
    cons->index.source = FBLE_LOCAL_VAR;
    cons->index.index = 0;

    FbleVarTc* x = FbleAlloc(arena, FbleVarTc);
    x->_base.tag = FBLE_VAR_TC;
    x->_base.loc = FbleCopyLoc(loc);
    x->index.source = FBLE_STATIC_VAR;
    x->index.index = i;

    FbleFuncApplyTc* apply = FbleAlloc(arena, FbleFuncApplyTc);
    apply->_base.tag = FBLE_FUNC_APPLY_TC;
    apply->_base.loc = FbleCopyLoc(loc);
    apply->func = &cons->_base;
    FbleVectorInit(arena, apply->args);
    FbleVectorAppend(arena, apply->args, &x->_base);
    FbleVectorAppend(arena, apply->args, applys);

    applys = &apply->_base;
  }

  FbleFuncValueTc* inner_func = FbleAlloc(arena, FbleFuncValueTc);
  inner_func->_base.tag = FBLE_FUNC_VALUE_TC;
  inner_func->_base.loc = FbleCopyLoc(loc);

  FbleVectorInit(arena, inner_func->scope);
  for (size_t i = 0; i < args.size; ++i) {
    FbleVarIndex index = {
      .source = FBLE_LOCAL_VAR,
      .index = i
    };
    FbleVectorAppend(arena, inner_func->scope, index);
  }

  inner_func->argc = 2;
  inner_func->body = applys;

  FblePolyValueTc* inner_poly = FbleAlloc(arena, FblePolyValueTc);
  inner_poly->_base.tag = FBLE_POLY_VALUE_TC;
  inner_poly->_base.loc = FbleCopyLoc(loc);
  inner_poly->body = &inner_func->_base;

  FbleFuncValueTc* outer_func = FbleAlloc(arena, FbleFuncValueTc);
  outer_func->_base.tag = FBLE_FUNC_VALUE_TC;
  outer_func->_base.loc = FbleCopyLoc(loc);
  FbleVectorInit(arena, outer_func->scope);
  outer_func->argc = args.size;
  outer_func->body = &inner_poly->_base;

  FblePolyValueTc* outer_poly = FbleAlloc(arena, FblePolyValueTc);
  outer_poly->_base.tag = FBLE_POLY_VALUE_TC;
  outer_poly->_base.loc = FbleCopyLoc(loc);
  outer_poly->body = &outer_func->_base;

  FblePolyApplyTc* apply_type = FbleAlloc(arena, FblePolyApplyTc);
  apply_type->_base.tag = FBLE_POLY_APPLY_TC;
  apply_type->_base.loc = FbleCopyLoc(loc);
  apply_type->poly = &outer_poly->_base;

  FbleFuncApplyTc* apply_elems = FbleAlloc(arena, FbleFuncApplyTc);
  apply_elems->_base.tag = FBLE_FUNC_APPLY_TC;
  apply_elems->_base.loc = FbleCopyLoc(loc);
  apply_elems->func = &apply_type->_base;
  FbleVectorInit(arena, apply_elems->args);
  for (size_t i = 0; i < args.size; ++i) {
    FbleVectorAppend(arena, apply_elems->args, args.xs[i]);
  }

  return &apply_elems->_base;
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
        if (FbleNamesEqual(field->name, union_value_expr->field)) {
          field_type = field->type;
          tag = i;
          break;
        }
      }

      if (field_type == NULL) {
        ReportError(arena, &union_value_expr->field.loc,
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
        if (FbleNamesEqual(access_expr->field, fields->xs[i].name)) {
          FbleType* rtype = FbleRetainType(heap, fields->xs[i].type);
          FbleReleaseType(heap, normal);

          FbleAccessTc* access_tc = FbleAlloc(arena, FbleAccessTc);
          access_tc->_base.tag = (normal->tag == FBLE_STRUCT_TYPE)
            ? FBLE_STRUCT_ACCESS_TC
            : FBLE_UNION_ACCESS_TC;
          access_tc->_base.loc = FbleCopyLoc(expr->loc);
          access_tc->obj = obj.tc;
          access_tc->tag = i;
          access_tc->loc = FbleCopyLoc(access_expr->field.loc);
          FbleReleaseType(heap, obj.type);
          return MkTc(rtype, &access_tc->_base);
        }
      }

      ReportError(arena, &access_expr->field.loc,
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
        return TC_FAILED;
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
      FbleReleaseType(heap, condition.type);

      FbleUnionSelectTc* select_tc = FbleAlloc(arena, FbleUnionSelectTc);
      select_tc->_base.tag = FBLE_UNION_SELECT_TC;
      select_tc->_base.loc = FbleCopyLoc(expr->loc);
      select_tc->condition = condition.tc;
      FbleVectorInit(arena, select_tc->choices);
      FbleVectorInit(arena, select_tc->branches);

      bool error = false;
      FbleType* target = NULL;

      size_t branch = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        if (branch < select_expr->choices.size && FbleNamesEqual(select_expr->choices.xs[branch].name, union_type->fields.xs[i].name)) {
          FbleVectorAppend(arena, select_tc->choices, branch);

          Tc result = TypeCheckExpr(heap, scope, select_expr->choices.xs[branch].expr);
          result = ProfileBlock(arena, select_expr->choices.xs[branch].name, result);
          error = error || (result.type == NULL);
          FbleVectorAppend(arena, select_tc->branches, result.tc);

          if (target == NULL) {
            target = result.type;
          } else if (result.type != NULL) {
            if (!FbleTypesEqual(heap, target, result.type)) {
              ReportError(arena, &select_expr->choices.xs[branch].expr->loc,
                  "expected type %t, but found %t\n",
                  target, result.type);
              error = true;
            }
            FbleReleaseType(heap, result.type);
          }

          branch++;
        } else if (select_expr->default_ == NULL) {
          error = true;

          if (branch < select_expr->choices.size) {
            ReportError(arena, &select_expr->choices.xs[branch].name.loc,
                "expected tag '%n', but found '%n'\n",
                &union_type->fields.xs[i].name, &select_expr->choices.xs[branch].name);
          } else {
            ReportError(arena, &expr->loc,
                "missing tag '%n'\n",
                &union_type->fields.xs[i].name);
          }
        } else {
          // Use the default branch for this field.
          FbleVectorAppend(arena, select_tc->choices, select_expr->choices.size);
        }
      }

      if (select_expr->default_ != NULL) {
        // TODO: Label with profile block ":"?
        Tc result = TypeCheckExpr(heap, scope, select_expr->default_);
        error = error || (result.type == NULL);
        FbleVectorAppend(arena, select_tc->branches, result.tc);
        if (target == NULL) {
          target = result.type;
        } else if (result.type != NULL) {
          if (!FbleTypesEqual(heap, target, result.type)) {
            ReportError(arena, &select_expr->default_->loc,
                "expected type %t, but found %t\n",
                target, result.type);
            error = true;
          }
          FbleReleaseType(heap, result.type);
        }
      }

      if (branch < select_expr->choices.size) {
        ReportError(arena, &select_expr->choices.xs[branch].name.loc,
            "unexpected tag '%n'\n",
            &select_expr->choices.xs[branch]);
        error = true;
      }

      FbleReleaseType(heap, &union_type->_base);
      Tc tc = MkTc(target, &select_tc->_base);
      if (error) {
        FreeTc(heap, tc);
        return TC_FAILED;
      }
      return tc;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;

      // Note: typeof(poly<arg>) = typeof(poly)<arg>
      // TypeCheckExpr gives us typeof(poly)
      Tc poly = TypeCheckExpr(heap, scope, apply->poly);
      if (poly.type == NULL) {
        return TC_FAILED;
      }

      FblePolyKind* poly_kind = (FblePolyKind*)FbleGetKind(arena, poly.type);
      if (poly_kind->_base.tag != FBLE_POLY_KIND) {
        ReportError(arena, &expr->loc,
            "cannot apply poly args to a basic kinded entity\n");
        FbleFreeKind(arena, &poly_kind->_base);
        FreeTc(heap, poly);
        return TC_FAILED;
      }

      // Note: arg_type is typeof(arg)
      FbleType* arg_type = TypeCheckExprForType(heap, scope, apply->arg);
      if (arg_type == NULL) {
        FbleFreeKind(arena, &poly_kind->_base);
        FreeTc(heap, poly);
        return TC_FAILED;
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
        FreeTc(heap, poly);
        return TC_FAILED;
      }
      FbleFreeKind(arena, actual_kind);
      FbleFreeKind(arena, &poly_kind->_base);

      FbleType* arg = FbleValueOfType(heap, arg_type);
      assert(arg != NULL && "TODO: poly apply arg is a value?");
      FbleReleaseType(heap, arg_type);

      FbleType* pat = FbleNewPolyApplyType(heap, expr->loc, poly.type, arg);
      FbleReleaseType(heap, arg);
      FbleReleaseType(heap, poly.type);

      FblePolyApplyTc* apply_tc = FbleAlloc(arena, FblePolyApplyTc);
      apply_tc->_base.tag = FBLE_POLY_APPLY_TC;
      apply_tc->_base.loc = FbleCopyLoc(expr->loc);
      apply_tc->poly = poly.tc;

      return MkTc(pat, &apply_tc->_base);
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* list_expr = (FbleListExpr*)expr;

      bool error = false;
      FbleType* type = NULL;
      FbleTc* args[list_expr->args.size];
      for (size_t i = 0; i < list_expr->args.size; ++i) {
        Tc tc = TypeCheckExpr(heap, scope, list_expr->args.xs[i]);
        error = error || (tc.type == NULL);

        if (type == NULL) {
          type = tc.type;
        } else if (tc.type != NULL) {
          if (!FbleTypesEqual(heap, type, tc.type)) {
            error = true;
            ReportError(arena, &list_expr->args.xs[i]->loc,
                "expected type %t, but found something of type %t\n",
                type, tc.type);
          }
          FbleReleaseType(heap, tc.type);
        }
        args[i] = tc.tc;
      }

      if (error) {
        FbleReleaseType(heap, type);
        for (size_t i = 0; i < list_expr->args.size; ++i) {
          FbleFreeTc(arena, args[i]);
        }
        return TC_FAILED;
      }

      FbleTcV argv = { .size = list_expr->args.size, .xs = args };
      Tc tc = MkTc(FbleNewListType(heap, type), NewListTc(arena, expr->loc, argv));
      FbleReleaseType(heap, type);
      return tc;
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* literal = (FbleLiteralExpr*)expr;

      Tc spec = TypeCheckExpr(heap, scope, literal->spec);
      if (spec.type == NULL) {
        return TC_FAILED;
      }

      FbleStructType* normal = (FbleStructType*)FbleNormalType(heap, spec.type);
      if (normal->_base.tag != FBLE_STRUCT_TYPE) {
        ReportError(arena, &literal->spec->loc,
            "expected a struct value, but literal spec has type %t\n",
            spec);
        FreeTc(heap, spec);
        FbleReleaseType(heap, &normal->_base);
        return TC_FAILED;
      }

      size_t n = strlen(literal->word);
      if (n == 0) {
        ReportError(arena, &literal->word_loc,
            "literals must not be empty\n");
        FreeTc(heap, spec);
        FbleReleaseType(heap, &normal->_base);
        return TC_FAILED;
      }

      assert(false && "TODO: FBLE_LITERAL_EXPRESSION");
      return TC_FAILED;
//      for (size_t i = 0; i < n; ++i) {
//        char field_str[2] = { literal->word[i], '\0' };
//        fields[i] = FbleNewString(arena, field_str);
//
//        letters[i]._base.tag = FBLE_MISC_ACCESS_EXPR;
//        letters[i]._base.loc = loc;
//        letters[i].object = &spec_var._base;
//        letters[i].field.name.name = fields[i];
//        letters[i].field.name.space = FBLE_NORMAL_NAME_SPACE;
//        letters[i].field.name.loc = loc;
//        letters[i].field.tag = FBLE_UNRESOLVED_FIELD_TAG;
//
//        xs[i] = &letters[i]._base;
//
//        if (literal->word[i] == '\n') {
//          loc.line++;
//          loc.col = 0;
//        }
//        loc.col++;
//      }
//
//      FbleExprV args = { .size = n, .xs = xs, };
//      FbleType* result = TypeCheckList(heap, scope, literal->word_loc, args);
//      PopVar(heap, scope);
//
//      for (size_t i = 0; i < n; ++i) {
//        literal->tags[i] = letters[i].field.tag;
//        FbleFreeString(arena, fields[i]);
//      }
//      FbleFreeString(arena, spec_name.name);
//
//      return result;
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
        return TC_FAILED;
      }


      FbleVarIndexV captured;
      FbleVectorInit(arena, captured);
      Scope func_scope;
      InitScope(arena, &func_scope, &captured, scope);

      for (size_t i = 0; i < argc; ++i) {
        PushVar(arena, &func_scope, func_value_expr->args.xs[i].name, arg_types.xs[i]);
      }

      Tc func_result = TypeCheckExpr(heap, &func_scope, func_value_expr->body);
      if (func_result.type == NULL) {
        FreeScope(heap, &func_scope);
        FbleFree(arena, arg_types.xs);
        FbleFree(arena, captured.xs);
        return TC_FAILED;
      }

      FbleFuncType* ft = FbleNewType(heap, FbleFuncType, FBLE_FUNC_TYPE, expr->loc);
      ft->args = arg_types;
      ft->rtype = func_result.type;
      FbleTypeAddRef(heap, &ft->_base, ft->rtype);
      FbleReleaseType(heap, ft->rtype);

      for (size_t i = 0; i < argc; ++i) {
        FbleTypeAddRef(heap, &ft->_base, arg_types.xs[i]);
      }

      FbleFuncValueTc* func_tc = FbleAlloc(arena, FbleFuncValueTc);
      func_tc->_base.tag = FBLE_FUNC_VALUE_TC;
      func_tc->_base.loc = FbleCopyLoc(expr->loc);
      func_tc->scope = captured;
      func_tc->argc = argc;
      func_tc->body = func_result.tc;

      FreeScope(heap, &func_scope);
      return MkTc(&ft->_base, &func_tc->_base);
    }

    case FBLE_EVAL_EXPR:
    case FBLE_LINK_EXPR:
    case FBLE_EXEC_EXPR: {
      FbleVarIndexV captured;
      FbleVectorInit(arena, captured);
      Scope body_scope;
      InitScope(arena, &body_scope, &captured, scope);

      Tc body = TypeCheckExec(heap, &body_scope, expr);
      if (body.type == NULL) {
        FreeScope(heap, &body_scope);
        FbleFree(arena, captured.xs);
        return TC_FAILED;
      }

      FbleProcType* proc_type = FbleNewType(heap, FbleProcType, FBLE_PROC_TYPE, expr->loc);
      proc_type->type = body.type;
      FbleTypeAddRef(heap, &proc_type->_base, proc_type->type);
      FbleReleaseType(heap, body.type);

      FbleFuncValueTc* proc_tc = FbleAlloc(arena, FbleFuncValueTc);
      proc_tc->_base.tag = FBLE_FUNC_VALUE_TC;
      proc_tc->_base.loc = FbleCopyLoc(expr->loc);
      proc_tc->scope = captured;
      proc_tc->argc = 0;
      proc_tc->body = body.tc;

      FreeScope(heap, &body_scope);
      return MkTc(&proc_type->_base, &proc_tc->_base);
    }

    case FBLE_VAR_EXPR: {
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;
      Var* var = GetVar(heap, scope, var_expr->var, false);
      if (var == NULL) {
        ReportError(arena, &var_expr->var.loc, "variable '%n' not defined\n",
            &var_expr->var);
        return TC_FAILED;
      }

      FbleVarTc* var_tc = FbleAlloc(arena, FbleVarTc);
      var_tc->_base.tag = FBLE_VAR_TC;
      var_tc->_base.loc = FbleCopyLoc(expr->loc);
      var_tc->index = var->index;
      return MkTc(FbleRetainType(heap, var->type), &var_tc->_base);
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
      Tc defs[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleBinding* binding = let_expr->bindings.xs + i;

        defs[i] = TC_FAILED;
        if (!error) {
          defs[i] = TypeCheckExpr(heap, scope, binding->expr);
          defs[i] = ProfileBlock(arena, binding->name, defs[i]);
        }
        error = error || (defs[i].type == NULL);

        if (!error && binding->type != NULL && !FbleTypesEqual(heap, types[i], defs[i].type)) {
          error = true;
          ReportError(arena, &binding->expr->loc,
              "expected type %t, but found something of type %t\n",
              types[i], defs[i].type);
        } else if (!error && binding->type == NULL) {
          FbleKind* expected_kind = FbleGetKind(arena, types[i]);
          FbleKind* actual_kind = FbleGetKind(arena, defs[i].type);
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
      bool recursive = false;
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        recursive = recursive || vars[i]->used;
      }

      // Apply the newly computed type values for variables whose types were
      // previously unknown.
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (!error && let_expr->bindings.xs[i].type == NULL) {
          FbleAssignVarType(heap, types[i], defs[i].type);
        }
        FbleReleaseType(heap, defs[i].type);
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (defs[i].type != NULL) {
          if (FbleTypeIsVacuous(heap, types[i])) {
            ReportError(arena, &let_expr->bindings.xs[i].name.loc,
                "%n is vacuous\n", &let_expr->bindings.xs[i].name);
            error = true;
          }
        }
      }

      Tc body = TC_FAILED;
      if (!error) {
        body = TypeCheckExpr(heap, scope, let_expr->body);
        error = (body.type == NULL);
      }

      if (body.type != NULL) {
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

      if (error) {
        for (size_t i = 0; i < let_expr->bindings.size; ++i) {
          FbleFreeTc(arena, defs[i].tc);
        }
        FreeTc(heap, body);
        return TC_FAILED;
      }

      FbleLetTc* let_tc = FbleAlloc(arena, FbleLetTc);
      let_tc->_base.tag = FBLE_LET_TC;
      let_tc->_base.loc = FbleCopyLoc(expr->loc);
      let_tc->recursive = recursive;
      FbleVectorInit(arena, let_tc->bindings);
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleVectorAppend(arena, let_tc->bindings, defs[i].tc);
      }
      let_tc->body = body.tc;
        
      return MkTc(body.type, &let_tc->_base);
    }

    case FBLE_MODULE_REF_EXPR: {
      FbleModuleRefExpr* module_ref_expr = (FbleModuleRefExpr*)expr;

      Var* var = GetVar(heap, scope, module_ref_expr->ref.resolved, false);

      // We should have resolved all modules at program load time.
      assert(var != NULL && "module not in scope");

      if (var->type == NULL) {
        // TODO: The spec isn't very clear on whether module self reference is
        // allowed or not. Or is it? No recursive modules, right? Isn't this
        // possibility precluded in load.c?
        ReportError(arena, &expr->loc, "illegal module self reference\n");
        return TC_FAILED;
      }

      FbleVarTc* var_tc = FbleAlloc(arena, FbleVarTc);
      var_tc->_base.tag = FBLE_VAR_TC;
      var_tc->_base.loc = FbleCopyLoc(expr->loc);
      var_tc->index = var->index;
      return MkTc(FbleRetainType(heap, var->type), &var_tc->_base);
    }

    case FBLE_POLY_EXPR: {
      FblePolyExpr* poly = (FblePolyExpr*)expr;

      if (FbleGetKindLevel(poly->arg.kind) != 1) {
        ReportError(arena, &poly->arg.kind->loc,
            "expected a type kind, but found %k\n",
            poly->arg.kind);
        return TC_FAILED;
      }

      if (poly->arg.name.space != FBLE_TYPE_NAME_SPACE) {
        ReportError(arena, &poly->arg.name.loc,
            "the namespace of '%n' is not appropriate for kind %k\n",
            &poly->arg.name, poly->arg.kind);
        return TC_FAILED;
      }

      FbleType* arg_type = FbleNewVarType(heap, poly->arg.name.loc, poly->arg.kind, poly->arg.name);
      FbleType* arg = FbleValueOfType(heap, arg_type);
      assert(arg != NULL);

      PushVar(arena, scope, poly->arg.name, arg_type);
      Tc body = TypeCheckExpr(heap, scope, poly->body);
      PopVar(heap, scope);

      if (body.type == NULL) {
        FbleReleaseType(heap, arg);
        return TC_FAILED;
      }

      FbleType* pt = FbleNewPolyType(heap, expr->loc, arg, body.type);
      FbleReleaseType(heap, arg);
      FbleReleaseType(heap, body.type);

      FblePolyValueTc* poly_tc = FbleAlloc(arena, FblePolyValueTc);
      poly_tc->_base.tag = FBLE_POLY_VALUE_TC;
      poly_tc->_base.loc = FbleCopyLoc(expr->loc);
      poly_tc->body = body.tc;

      return MkTc(pt, &poly_tc->_base);
    }
  }

  UNREACHABLE("should already have returned");
  return TC_FAILED;
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
//   A type checked expression that computes the result of executing the
//   process expression, or TC_FAILED if the expression is not well typed or
//   is not a process expression. If the type of the process expression is T!,
//   the returned type is T.
//
// Side effects:
// * Prints a message to stderr if the expression fails to compile.
// * The caller should call FbleFreeTc when the returned result is no
//   longer needed and FbleReleaseType when the returned FbleType is no longer
//   needed.
static Tc TypeCheckExec(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr)
{
  FbleArena* arena = heap->arena;
  switch (expr->tag) {
    case FBLE_STRUCT_TYPE_EXPR:
    case FBLE_UNION_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR:
    case FBLE_MISC_APPLY_EXPR:
    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR:
    case FBLE_UNION_VALUE_EXPR:
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
      Tc proc = TypeCheckExpr(heap, scope, expr);
      if (proc.type == NULL) {
        return TC_FAILED;
      }

      FbleProcType* normal = (FbleProcType*)FbleNormalType(heap, proc.type);
      if (normal->_base.tag != FBLE_PROC_TYPE) {
        ReportError(arena, &expr->loc,
            "expected process, but found expression of type %t\n",
            proc);
        FbleReleaseType(heap, &normal->_base);
        FreeTc(heap, proc);
        return TC_FAILED;
      }

      FbleType* rtype = FbleRetainType(heap, normal->type);
      FbleReleaseType(heap, &normal->_base);
      FbleReleaseType(heap, proc.type);

      FbleFuncApplyTc* apply_tc = FbleAlloc(arena, FbleFuncApplyTc);
      apply_tc->_base.tag = FBLE_FUNC_APPLY_TC;
      apply_tc->_base.loc = FbleCopyLoc(expr->loc);
      apply_tc->func = proc.tc;
      FbleVectorInit(arena, apply_tc->args);

      return MkTc(rtype, &apply_tc->_base);
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
        return TC_FAILED;
      }

      FbleType* port_type = TypeCheckType(heap, scope, link_expr->type);
      if (port_type == NULL) {
        return TC_FAILED;
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

      Tc body = TypeCheckExec(heap, scope, link_expr->body);

      PopVar(heap, scope);
      PopVar(heap, scope);

      if (body.type == NULL) {
        return TC_FAILED;
      }

      FbleLinkTc* link_tc = FbleAlloc(arena, FbleLinkTc);
      link_tc->_base.tag = FBLE_LINK_TC;
      link_tc->_base.loc = FbleCopyLoc(expr->loc);
      link_tc->body = body.tc;
      return MkTc(body.type, &link_tc->_base);
    }

    case FBLE_EXEC_EXPR: {
      assert(false && "TODO: EXEC EXEC");
      return TC_FAILED;
//      FbleExecExpr* exec_expr = (FbleExecExpr*)expr;
//      bool error = false;
//
//      // Evaluate the types of the bindings and set up the new vars.
//      FbleType* types[exec_expr->bindings.size];
//      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
//        types[i] = TypeCheckType(heap, scope, exec_expr->bindings.xs[i].type);
//        error = error || (types[i] == NULL);
//      }
//
//      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
//        FbleType* binding = TypeCheckExpr(heap, scope, exec_expr->bindings.xs[i].expr);
//        if (binding != NULL) {
//          FbleProcType* proc_type = (FbleProcType*)FbleNormalType(heap, binding);
//          if (proc_type->_base.tag == FBLE_PROC_TYPE) {
//            if (types[i] != NULL && !FbleTypesEqual(heap, types[i], proc_type->type)) {
//              error = true;
//              ReportError(arena, &exec_expr->bindings.xs[i].expr->loc,
//                  "expected type %t!, but found %t\n",
//                  types[i], binding);
//            }
//          } else {
//            error = true;
//            ReportError(arena, &exec_expr->bindings.xs[i].expr->loc,
//                "expected process, but found expression of type %t\n",
//                binding);
//          }
//          FbleReleaseType(heap, &proc_type->_base);
//        } else {
//          error = true;
//        }
//        FbleReleaseType(heap, binding);
//      }
//
//      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
//        PushVar(arena, scope, exec_expr->bindings.xs[i].name, types[i]);
//      }
//
//      FbleType* body = NULL;
//      if (!error) {
//        body = TypeCheckExec(heap, scope, exec_expr->body);
//      }
//
//      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
//        PopVar(heap, scope);
//      }
//
//      return body;
    }
  }

  UNREACHABLE("should never get here");
  return TC_FAILED;
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
  InitScope(arena, &nscope, NULL, scope);

  Tc result = TypeCheckExpr(heap, &nscope, expr);
  FreeScope(heap, &nscope);
  FbleFreeTc(arena, result.tc);
  return result.type;
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

    case FBLE_MISC_APPLY_EXPR:
    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR:
    case FBLE_UNION_VALUE_EXPR:
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
//   Type check a program.
//
// Inputs:
//   heap - heap to use for allocations.
//   scope - the list of variables in scope.
//   modules - the modules of the program.
//   modulec - the number of modules to check.
//   body - the main body of the program.
//
// Results:
//   The type checked program, or NULL if the program failed to type check.
//
// Side effects:
// * Prints warning messages to stderr.
// * Prints a message to stderr if the program fails to type check.
// * The user is responsible for calling FbleFreeTc on the returned program
//   when it is no longer needed.
static FbleTc* TypeCheckProgram(FbleTypeHeap* heap, Scope* scope, FbleModule* modules, size_t modulec, FbleExpr* body)
{
  FbleArena* arena = heap->arena;

  if (modulec == 0) {
    Tc result = TypeCheckExpr(heap, scope, body);
    FbleReleaseType(heap, result.type);
    return result.tc;
  }

  // Push a dummy variable representing the value of the computed module,
  // because we'll be turning this into a LET_TC, which assumes a variable
  // index is consumed by the thing being defined.
  // TODO: The spec isn't very clear on whether module self reference is
  // allowed or not. Or is it? No recursive modules, right?
  PushVar(arena, scope, modules->name, NULL);
  Tc module = TypeCheckExpr(heap, scope, modules->value);
  module = ProfileBlock(arena, modules->name, module);
  PopVar(heap, scope);

  if (module.type == NULL) {
    return NULL;
  }

  PushVar(arena, scope, modules->name, module.type);
  FbleTc* body_tc = TypeCheckProgram(heap, scope, modules + 1, modulec - 1, body);
  PopVar(heap, scope);

  if (body_tc == NULL) {
    FbleFreeTc(arena, module.tc);
    return NULL;
  }

  FbleLetTc* let_tc = FbleAlloc(arena, FbleLetTc);
  let_tc->_base.tag = FBLE_LET_TC;
  let_tc->_base.loc = FbleCopyLoc(modules->name.loc);
  let_tc->recursive = false;
  FbleVectorInit(arena, let_tc->bindings);
  FbleVectorAppend(arena, let_tc->bindings, module.tc);
  let_tc->body = body_tc;
  return &let_tc->_base;
}

// FbleTypeCheck -- see documentation in typecheck.h
FbleTc* FbleTypeCheck(FbleArena* arena, FbleProgram* program)
{
  Scope scope;
  InitScope(arena, &scope, NULL, NULL);

  FbleTypeHeap* heap = FbleNewTypeHeap(arena);
  FbleTc* result = TypeCheckProgram(heap, &scope, program->modules.xs, program->modules.size, program->main);
  FreeScope(heap, &scope);
  FbleFreeTypeHeap(heap);
  return result;
}

// FbleFreeTc -- see documentation in typecheck.h
void FbleFreeTc(FbleArena* arena, FbleTc* tc)
{
  if (tc == NULL) {
    return;
  }

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
      FbleAccessTc* access_tc = (FbleAccessTc*)tc;
      FbleFreeTc(arena, access_tc->obj);
      FbleFreeLoc(arena, access_tc->loc);
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
      FbleFree(arena, select_tc->choices.xs);
      for (size_t i = 0; i < select_tc->branches.size; ++i) {
        FbleFreeTc(arena, select_tc->branches.xs[i]);
      }
      FbleFree(arena, select_tc->branches.xs);
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
        FbleFreeTc(arena, apply_tc->args.xs[i]);
      }
      FbleFree(arena, apply_tc->args.xs);
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
      FbleFreeName(arena, profile_tc->name);
      FbleFreeTc(arena, profile_tc->body);
      FbleFree(arena, tc);
      return;
    }
  }

  UNREACHABLE("should never get here");
}
