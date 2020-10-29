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
#include "value.h"

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

static void ReportError(FbleArena* arena, FbleLoc loc, const char* fmt, ...);
static bool CheckNameSpace(FbleArena* arena, FbleName name, FbleType* type);

// Tc --
//   A pair of returned type and type checked expression.
typedef struct {
  FbleType* type;
  FbleValue* tc;
} Tc;

// TC_FAILED --
//   Tc returned to indicate that type check has failed.
static Tc TC_FAILED = { .type = NULL, .tc = NULL };

static Tc MkTc(FbleType* type, FbleValue* tc);
static void FreeTc(FbleTypeHeap* th, FbleValueHeap* vh, Tc tc);
static Tc ProfileBlock(FbleValueHeap* vh, FbleName label, FbleLoc loc, Tc tc);

static FbleValue* NewListTc(FbleValueHeap* vh, FbleLoc loc, FbleValueV args);

static Tc TypeCheckExpr(FbleTypeHeap* th, FbleValueHeap* vh, Scope* scope, FbleExpr* expr);
static Tc TypeCheckExec(FbleTypeHeap* th, FbleValueHeap* vh, Scope* scope, FbleExpr* expr);
static FbleType* TypeCheckType(FbleTypeHeap* th, FbleValueHeap* vh, Scope* scope, FbleTypeExpr* type);
static FbleType* TypeCheckExprForType(FbleTypeHeap* th, FbleValueHeap* vh, Scope* scope, FbleExpr* expr);
static FbleValue* TypeCheckProgram(FbleTypeHeap* th, FbleValueHeap* vh, Scope* scope, FbleModule* modules, size_t modulec, FbleExpr* body);

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
//     %n - FbleName
//     %s - const char*
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
static void ReportError(FbleArena* arena, FbleLoc loc, const char* fmt, ...)
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
        FbleName name = va_arg(ap, FbleName);
        FblePrintName(stderr, name);
        break;
      }

      case 's': {
        const char* str = va_arg(ap, const char*);
        fprintf(stderr, "%s", str);
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
static bool CheckNameSpace(FbleArena* arena, FbleName name, FbleType* type)
{
  FbleKind* kind = FbleGetKind(arena, type);
  size_t kind_level = FbleGetKindLevel(kind);
  FbleFreeKind(arena, kind);

  bool match = (kind_level == 0 && name.space == FBLE_NORMAL_NAME_SPACE)
            || (kind_level == 1 && name.space == FBLE_TYPE_NAME_SPACE);

  if (!match) {
    ReportError(arena, name.loc,
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
static Tc MkTc(FbleType* type, FbleValue* tc)
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
static void FreeTc(FbleTypeHeap* th, FbleValueHeap* vh, Tc tc)
{
  FbleReleaseType(th, tc.type);
  FbleReleaseValue(vh, tc.tc);
}

// ProfileBlock --
//   Wrap the given tc in a profile block.
//
// Inputs:
//   arena - arena to use for allocations.
//   label - the label of the profiling block. Borrowed.
//   loc - the location of the body of the profiling block. Borrowed.
//   tc - the tc to wrap in a profile block. May be TC_FAILED.
//
// Results:
//   The given tc wrapped in a profile block.
//
// Side effects:
//   Forwards ownership of the type and tc in 'tc' to the returned tc.
//   Does not take ownership of label, makes a copy instead.
static Tc ProfileBlock(FbleValueHeap* vh, FbleName label, FbleLoc loc, Tc tc)
{
  if (tc.type == NULL) {
    assert(tc.tc == NULL);
    return TC_FAILED;
  }

  FbleProfileTc* profile_tc = FbleAlloc(vh->arena, FbleProfileTc);
  profile_tc->_base.tag = FBLE_PROFILE_TC;
  profile_tc->loc = FbleCopyLoc(loc);
  profile_tc->name = FbleCopyName(vh->arena, label);
  profile_tc->body = tc.tc;
  tc.tc = FbleNewTcValue(vh, &profile_tc->_base);
  return tc;
}

// NewListTc --
//   Create an FbleTc representing an fble list expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   loc - the location of the list expression. Borrowed.
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
static FbleValue* NewListTc(FbleValueHeap* vh, FbleLoc loc, FbleValueV args)
{
  FbleArena* arena = vh->arena;

  // The goal is to desugar a list expression [a, b, c, d] into the
  // following expression:
  // <@ T@>(T@ x, T@ x1, T@ x2, T@ x3)<@ L@>((T@, L@){L@;} cons, L@ nil) {
  //   cons(x, cons(x1, cons(x2, cons(x3, nil))));
  // }<t@>(a, b, c, d)
  //
  // Note: we can erase the poly values and applications when generating TC
  // code. In that case this becomes something like:
  // (x0, x1, x2, x3)(cons, nil) {
  //   cons(x, cons(x1, cons(x2, cons(x3, nil))));
  // }(a, b, c, d)
  //   
  assert(args.size > 0 && "empty lists not allowed");

  FbleVarValue* nil = FbleNewValue(vh, FbleVarValue);
  nil->_base.tag = FBLE_VAR_VALUE;
  nil->index.source = FBLE_LOCAL_VAR;
  nil->index.index = 1;

  FbleValue* applys = &nil->_base;
  for (size_t i = 0; i < args.size; ++i) {
    FbleVarValue* cons = FbleNewValue(vh, FbleVarValue);
    cons->_base.tag = FBLE_VAR_VALUE;
    cons->index.source = FBLE_LOCAL_VAR;
    cons->index.index = 0;

    FbleVarValue* x = FbleNewValue(vh, FbleVarValue);
    x->_base.tag = FBLE_VAR_VALUE;
    x->index.source = FBLE_STATIC_VAR;
    x->index.index = args.size - i - 1;

    FbleFuncApplyTc* apply = FbleAlloc(arena, FbleFuncApplyTc);
    apply->_base.tag = FBLE_FUNC_APPLY_TC;
    apply->loc = FbleCopyLoc(loc);
    apply->func = &cons->_base;
    FbleVectorInit(arena, apply->args);
    FbleVectorAppend(arena, apply->args, &x->_base);
    FbleVectorAppend(arena, apply->args, applys);

    applys = FbleNewTcValue(vh, &apply->_base);
  }

  FbleFuncValueTc* inner_func = FbleAlloc(arena, FbleFuncValueTc);
  inner_func->_base.tag = FBLE_FUNC_VALUE_TC;
  inner_func->body_loc = FbleCopyLoc(loc);

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

  FbleFuncValueTc* outer_func = FbleAlloc(arena, FbleFuncValueTc);
  outer_func->_base.tag = FBLE_FUNC_VALUE_TC;
  outer_func->body_loc = FbleCopyLoc(loc);
  FbleVectorInit(arena, outer_func->scope);
  outer_func->argc = args.size;
  outer_func->body = FbleNewTcValue(vh, &inner_func->_base);

  FbleFuncApplyTc* apply_elems = FbleAlloc(arena, FbleFuncApplyTc);
  apply_elems->_base.tag = FBLE_FUNC_APPLY_TC;
  apply_elems->loc = FbleCopyLoc(loc);
  apply_elems->func = FbleNewTcValue(vh, &outer_func->_base);
  FbleVectorInit(arena, apply_elems->args);
  for (size_t i = 0; i < args.size; ++i) {
    FbleVectorAppend(arena, apply_elems->args, args.xs[i]);
  }

  return FbleNewTcValue(vh, &apply_elems->_base);
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
static Tc TypeCheckExpr(FbleTypeHeap* th, FbleValueHeap* vh, Scope* scope, FbleExpr* expr)
{
  FbleArena* arena = vh->arena;
  switch (expr->tag) {
    case FBLE_DATA_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR:
    {
      FbleType* type = TypeCheckType(th, vh, scope, expr);
      if (type == NULL) {
        return TC_FAILED;
      }

      FbleTypeType* type_type = FbleNewType(th, FbleTypeType, FBLE_TYPE_TYPE, expr->loc);
      type_type->type = type;
      FbleTypeAddRef(th, &type_type->_base, type_type->type);
      FbleReleaseType(th, type);

      FbleTypeValue* type_v = FbleNewValue(vh, FbleTypeValue);
      type_v->_base.tag = FBLE_TYPE_VALUE;
      return MkTc(&type_type->_base, &type_v->_base);
    }

    case FBLE_VAR_EXPR: {
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;
      Var* var = GetVar(th, scope, var_expr->var, false);
      if (var == NULL) {
        ReportError(arena, var_expr->var.loc, "variable '%n' not defined\n", var_expr->var);
        return TC_FAILED;
      }

      FbleVarValue* var_v = FbleNewValue(vh, FbleVarValue);
      var_v->_base.tag = FBLE_VAR_VALUE;
      var_v->index = var->index;
      return MkTc(FbleRetainType(th, var->type), &var_v->_base);
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
          // represent the type. If it's an abstract type, such as
          //   @ Unit@ = ...
          // Then we'll use the type name Unit@ as is.
          //
          // If it's an abstract value, such as
          //   % True = ...
          //
          // Then we'll use the slightly different name __True@, because it is
          // very confusing to show the type of True as True@.
          char renamed[strlen(binding->name.name->str) + 3];
          renamed[0] = '\0';
          if (FbleGetKindLevel(binding->kind) == 0) {
            strcat(renamed, "__");
          }
          strcat(renamed, binding->name.name->str);

          FbleName type_name = {
            .name = FbleNewString(arena, renamed),
            .space = FBLE_TYPE_NAME_SPACE,
            .loc = binding->name.loc,
          };

          types[i] = FbleNewVarType(th, binding->name.loc, binding->kind, type_name);
          FbleFreeString(arena, type_name.name);
        } else {
          assert(binding->kind == NULL);
          types[i] = TypeCheckType(th, vh, scope, binding->type);
          error = error || (types[i] == NULL);
        }
        
        if (types[i] != NULL && !CheckNameSpace(arena, binding->name, types[i])) {
          error = true;
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(let_expr->bindings.xs[i].name, let_expr->bindings.xs[j].name)) {
            ReportError(arena, let_expr->bindings.xs[i].name.loc,
                "duplicate variable name '%n'\n",
                let_expr->bindings.xs[i].name);
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
          defs[i] = TypeCheckExpr(th, vh, scope, binding->expr);
          defs[i] = ProfileBlock(vh, binding->name, binding->expr->loc, defs[i]);
        }
        error = error || (defs[i].type == NULL);

        if (!error && binding->type != NULL && !FbleTypesEqual(th, types[i], defs[i].type)) {
          error = true;
          ReportError(arena, binding->expr->loc,
              "expected type %t, but found something of type %t\n",
              types[i], defs[i].type);
        } else if (!error && binding->type == NULL) {
          FbleKind* expected_kind = FbleGetKind(arena, types[i]);
          FbleKind* actual_kind = FbleGetKind(arena, defs[i].type);
          if (!FbleKindsEqual(expected_kind, actual_kind)) {
            ReportError(arena, binding->expr->loc,
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
          FbleAssignVarType(th, types[i], defs[i].type);

          // Here we pick the name for the type to use in error messages.
          // For normal type definitions, such as
          //   @ Foo@ = ...
          // We want to use the simple name 'Foo@'.
          //
          // For value definitions, such as
          //   % Foo = ...
          // We want to use the inferred type, not the made up abstract type
          // name '__Foo@'.
          if (FbleGetKindLevel(let_expr->bindings.xs[i].kind) == 0) {
            vars[i]->type = defs[i].type;
            defs[i].type = types[i];
            types[i] = vars[i]->type;
          }
        }
        FbleReleaseType(th, defs[i].type);
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (defs[i].type != NULL) {
          if (FbleTypeIsVacuous(th, types[i])) {
            ReportError(arena, let_expr->bindings.xs[i].name.loc,
                "%n is vacuous\n", let_expr->bindings.xs[i].name);
            error = true;
          }
        }
      }

      Tc body = TC_FAILED;
      if (!error) {
        body = TypeCheckExpr(th, vh, scope, let_expr->body);
        error = (body.type == NULL);
      }

      if (body.type != NULL) {
        for (size_t i = 0; i < let_expr->bindings.size; ++i) {
          if (!vars[i]->accessed && vars[i]->name.name->str[0] != '_') {
            FbleReportWarning("variable '", vars[i]->name.loc);
            FblePrintName(stderr, vars[i]->name);
            fprintf(stderr, "' defined but not used\n");
          }
        }
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        PopVar(th, scope);
      }

      if (error) {
        for (size_t i = 0; i < let_expr->bindings.size; ++i) {
          FbleReleaseValue(vh, defs[i].tc);
        }
        FreeTc(th, vh, body);
        return TC_FAILED;
      }

      FbleLetTc* let_tc = FbleAlloc(arena, FbleLetTc);
      let_tc->_base.tag = FBLE_LET_TC;
      let_tc->recursive = recursive;
      FbleVectorInit(arena, let_tc->bindings);
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleVectorAppend(arena, let_tc->bindings, defs[i].tc);
      }
      let_tc->body = body.tc;
        
      return MkTc(body.type, FbleNewTcValue(vh, &let_tc->_base));
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      FbleStructValueImplicitTypeExpr* struct_expr = (FbleStructValueImplicitTypeExpr*)expr;
      FbleDataType* struct_type = FbleNewType(th, FbleDataType, FBLE_DATA_TYPE, expr->loc);
      struct_type->datatype = FBLE_STRUCT_DATATYPE;
      FbleVectorInit(arena, struct_type->fields);

      size_t argc = struct_expr->args.size;
      Tc args[argc];
      bool error = false;
      for (size_t i = 0; i < argc; ++i) {
        size_t j = argc - i - 1;
        FbleTaggedExpr* arg = struct_expr->args.xs + j;
        args[j] = TypeCheckExpr(th, vh, scope, arg->expr);
        args[j] = ProfileBlock(vh, arg->name, arg->expr->loc, args[j]);
        error = error || (args[j].type == NULL);
      }

      for (size_t i = 0; i < argc; ++i) {
        FbleTaggedExpr* arg = struct_expr->args.xs + i;
        if (args[i].type != NULL) {
          if (!CheckNameSpace(arena, arg->name, args[i].type)) {
            error = true;
          }

          FbleTaggedType cfield = {
            .name = FbleCopyName(arena, arg->name),
            .type = args[i].type
          };
          FbleVectorAppend(arena, struct_type->fields, cfield);
          FbleTypeAddRef(th, &struct_type->_base, cfield.type);
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(arg->name, struct_expr->args.xs[j].name)) {
            error = true;
            ReportError(arena, arg->name.loc,
                "duplicate field name '%n'\n",
                struct_expr->args.xs[j].name);
          }
        }
      }

      if (error) {
        FbleReleaseType(th, &struct_type->_base);
        for (size_t i = 0; i < argc; ++i) {
          FreeTc(th, vh, args[i]);
        }
        return TC_FAILED;
      }

      FbleValueV argv;
      FbleVectorInit(arena, argv);
      for (size_t i = 0; i < argc; ++i) {
        FbleReleaseType(th, args[i].type);
        FbleVectorAppend(arena, argv, args[i].tc);
      }

      FbleValue* struct_v = FbleNewStructValue(vh, argv);

      for (size_t i = 0; i < argc; ++i) {
        FbleReleaseValue(vh, argv.xs[i]);
      }
      FbleFree(arena, argv.xs);
      return MkTc(&struct_type->_base, struct_v);
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      FbleType* type = TypeCheckType(th, vh, scope, union_value_expr->type);
      if (type == NULL) {
        return TC_FAILED;
      }

      FbleDataType* union_type = (FbleDataType*)FbleNormalType(th, type);
      if (union_type->datatype != FBLE_UNION_DATATYPE) {
        ReportError(arena, union_value_expr->type->loc,
            "expected a union type, but found %t\n", type);
        FbleReleaseType(th, &union_type->_base);
        FbleReleaseType(th, type);
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
        ReportError(arena, union_value_expr->field.loc,
            "'%n' is not a field of type %t\n",
            union_value_expr->field, type);
        FbleReleaseType(th, &union_type->_base);
        FbleReleaseType(th, type);
        return TC_FAILED;
      }

      Tc arg = TypeCheckExpr(th, vh, scope, union_value_expr->arg);
      if (arg.type == NULL) {
        FbleReleaseType(th, &union_type->_base);
        FbleReleaseType(th, type);
        return TC_FAILED;
      }

      if (!FbleTypesEqual(th, field_type, arg.type)) {
        ReportError(arena, union_value_expr->arg->loc,
            "expected type %t, but found type %t\n",
            field_type, arg.type);
        FbleReleaseType(th, type);
        FbleReleaseType(th, &union_type->_base);
        FreeTc(th, vh, arg);
        return TC_FAILED;
      }
      FbleReleaseType(th, arg.type);
      FbleReleaseType(th, &union_type->_base);

      FbleValue* union_v = FbleNewUnionValue(vh, tag, arg.tc);
      FbleReleaseValue(vh, arg.tc);
      return MkTc(type, union_v);
    }

    case FBLE_UNION_SELECT_EXPR: {
      FbleUnionSelectExpr* select_expr = (FbleUnionSelectExpr*)expr;

      Tc condition = TypeCheckExpr(th, vh, scope, select_expr->condition);
      if (condition.type == NULL) {
        return TC_FAILED;
      }

      FbleDataType* union_type = (FbleDataType*)FbleNormalType(th, condition.type);
      if (union_type->_base.tag != FBLE_DATA_TYPE
          || union_type->datatype != FBLE_UNION_DATATYPE) {
        ReportError(arena, select_expr->condition->loc,
            "expected value of union type, but found value of type %t\n",
            condition.type);
        FbleReleaseType(th, &union_type->_base);
        FreeTc(th, vh, condition);
        return TC_FAILED;
      }
      FbleReleaseType(th, condition.type);

      FbleUnionSelectTc* select_tc = FbleAlloc(arena, FbleUnionSelectTc);
      select_tc->_base.tag = FBLE_UNION_SELECT_TC;
      select_tc->condition = condition.tc;
      select_tc->loc = FbleCopyLoc(expr->loc);
      FbleVectorInit(arena, select_tc->choices);
      FbleVectorInit(arena, select_tc->branches);

      bool error = false;
      FbleType* target = NULL;

      size_t branch = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        if (branch < select_expr->choices.size && FbleNamesEqual(select_expr->choices.xs[branch].name, union_type->fields.xs[i].name)) {
          FbleVectorAppend(arena, select_tc->choices, branch);

          Tc result = TypeCheckExpr(th, vh, scope, select_expr->choices.xs[branch].expr);
          result = ProfileBlock(vh, select_expr->choices.xs[branch].name, select_expr->choices.xs[branch].expr->loc, result);
          error = error || (result.type == NULL);
          FbleVectorAppend(arena, select_tc->branches, result.tc);

          if (target == NULL) {
            target = result.type;
          } else if (result.type != NULL) {
            if (!FbleTypesEqual(th, target, result.type)) {
              ReportError(arena, select_expr->choices.xs[branch].expr->loc,
                  "expected type %t, but found %t\n",
                  target, result.type);
              error = true;
            }
            FbleReleaseType(th, result.type);
          }

          branch++;
        } else if (select_expr->default_ == NULL) {
          error = true;

          if (branch < select_expr->choices.size) {
            ReportError(arena, select_expr->choices.xs[branch].name.loc,
                "expected tag '%n', but found '%n'\n",
                union_type->fields.xs[i].name, select_expr->choices.xs[branch].name);
          } else {
            ReportError(arena, expr->loc,
                "missing tag '%n'\n",
                union_type->fields.xs[i].name);
          }
        } else {
          // Use the default branch for this field.
          FbleVectorAppend(arena, select_tc->choices, select_expr->choices.size);
        }
      }

      if (select_expr->default_ != NULL) {
        FbleName label = {
          .name = FbleNewString(arena, ":"),
          .space = FBLE_NORMAL_NAME_SPACE,
          .loc = FbleCopyLoc(select_expr->default_->loc),
        };

        Tc result = TypeCheckExpr(th, vh, scope, select_expr->default_);
        result = ProfileBlock(vh, label, select_expr->default_->loc, result);
        FbleFreeName(arena, label);
        error = error || (result.type == NULL);
        FbleVectorAppend(arena, select_tc->branches, result.tc);
        if (target == NULL) {
          target = result.type;
        } else if (result.type != NULL) {
          if (!FbleTypesEqual(th, target, result.type)) {
            ReportError(arena, select_expr->default_->loc,
                "expected type %t, but found %t\n",
                target, result.type);
            error = true;
          }
          FbleReleaseType(th, result.type);
        }
      }

      if (branch < select_expr->choices.size) {
        ReportError(arena, select_expr->choices.xs[branch].name.loc,
            "unexpected tag '%n'\n",
            select_expr->choices.xs[branch]);
        error = true;
      }

      FbleReleaseType(th, &union_type->_base);
      Tc tc = MkTc(target, FbleNewTcValue(vh, &select_tc->_base));
      if (error) {
        FreeTc(th, vh, tc);
        return TC_FAILED;
      }
      return tc;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      size_t argc = func_value_expr->args.size;

      bool error = false;
      FbleTypeV arg_types;
      FbleVectorInit(arena, arg_types);
      for (size_t i = 0; i < argc; ++i) {
        FbleType* arg_type = TypeCheckType(th, vh, scope, func_value_expr->args.xs[i].type);
        FbleVectorAppend(arena, arg_types, arg_type);
        error = error || arg_type == NULL;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(func_value_expr->args.xs[i].name, func_value_expr->args.xs[j].name)) {
            error = true;
            ReportError(arena, func_value_expr->args.xs[i].name.loc,
                "duplicate arg name '%n'\n",
                func_value_expr->args.xs[i].name);
          }
        }
      }

      if (error) {
        for (size_t i = 0; i < argc; ++i) {
          FbleReleaseType(th, arg_types.xs[i]);
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

      Tc func_result = TypeCheckExpr(th, vh, &func_scope, func_value_expr->body);
      if (func_result.type == NULL) {
        FreeScope(th, &func_scope);
        FbleFree(arena, arg_types.xs);
        FbleFree(arena, captured.xs);
        return TC_FAILED;
      }

      FbleFuncType* ft = FbleNewType(th, FbleFuncType, FBLE_FUNC_TYPE, expr->loc);
      ft->args = arg_types;
      ft->rtype = func_result.type;
      FbleTypeAddRef(th, &ft->_base, ft->rtype);
      FbleReleaseType(th, ft->rtype);

      for (size_t i = 0; i < argc; ++i) {
        FbleTypeAddRef(th, &ft->_base, arg_types.xs[i]);
      }

      FbleFuncValueTc* func_tc = FbleAlloc(arena, FbleFuncValueTc);
      func_tc->_base.tag = FBLE_FUNC_VALUE_TC;
      func_tc->body_loc = FbleCopyLoc(func_value_expr->body->loc);
      func_tc->scope = captured;
      func_tc->argc = argc;
      func_tc->body = func_result.tc;

      FreeScope(th, &func_scope);
      return MkTc(&ft->_base, FbleNewTcValue(vh, &func_tc->_base));
    }

    case FBLE_EVAL_EXPR:
    case FBLE_LINK_EXPR:
    case FBLE_EXEC_EXPR: {
      FbleVarIndexV captured;
      FbleVectorInit(arena, captured);
      Scope body_scope;
      InitScope(arena, &body_scope, &captured, scope);

      Tc body = TypeCheckExec(th, vh, &body_scope, expr);
      if (body.type == NULL) {
        FreeScope(th, &body_scope);
        FbleFree(arena, captured.xs);
        return TC_FAILED;
      }

      FbleProcType* proc_type = FbleNewType(th, FbleProcType, FBLE_PROC_TYPE, expr->loc);
      proc_type->type = body.type;
      FbleTypeAddRef(th, &proc_type->_base, proc_type->type);
      FbleReleaseType(th, body.type);

      FbleFuncValueTc* proc_tc = FbleAlloc(arena, FbleFuncValueTc);
      proc_tc->_base.tag = FBLE_FUNC_VALUE_TC;
      proc_tc->body_loc = FbleCopyLoc(expr->loc);
      proc_tc->scope = captured;
      proc_tc->argc = 0;
      proc_tc->body = body.tc;

      FreeScope(th, &body_scope);
      return MkTc(&proc_type->_base, FbleNewTcValue(vh, &proc_tc->_base));
    }

    case FBLE_POLY_VALUE_EXPR: {
      FblePolyValueExpr* poly = (FblePolyValueExpr*)expr;

      if (FbleGetKindLevel(poly->arg.kind) != 1) {
        ReportError(arena, poly->arg.kind->loc,
            "expected a type kind, but found %k\n",
            poly->arg.kind);
        return TC_FAILED;
      }

      if (poly->arg.name.space != FBLE_TYPE_NAME_SPACE) {
        ReportError(arena, poly->arg.name.loc,
            "the namespace of '%n' is not appropriate for kind %k\n",
            poly->arg.name, poly->arg.kind);
        return TC_FAILED;
      }

      FbleType* arg_type = FbleNewVarType(th, poly->arg.name.loc, poly->arg.kind, poly->arg.name);
      FbleType* arg = FbleValueOfType(th, arg_type);
      assert(arg != NULL);

      PushVar(arena, scope, poly->arg.name, arg_type);
      Tc body = TypeCheckExpr(th, vh, scope, poly->body);
      PopVar(th, scope);

      if (body.type == NULL) {
        FbleReleaseType(th, arg);
        return TC_FAILED;
      }

      FbleType* pt = FbleNewPolyType(th, expr->loc, arg, body.type);
      FbleReleaseType(th, arg);
      FbleReleaseType(th, body.type);

      // A poly value expression gets rewritten as a let when we erase types:
      // <@ T@> ...
      // turns into:
      //   let T@ = type
      //   in ...

      // TODO: Do we really have to allocate a type value here? Can we
      // optimize this away?
      FbleTypeValue* type_v = FbleNewValue(vh, FbleTypeValue);
      type_v->_base.tag = FBLE_TYPE_VALUE;

      FbleLetTc* let_tc = FbleAlloc(arena, FbleLetTc);
      let_tc->_base.tag = FBLE_LET_TC;
      let_tc->recursive = false;
      FbleVectorInit(arena, let_tc->bindings);
      FbleVectorAppend(arena, let_tc->bindings, &type_v->_base);
      let_tc->body = body.tc;

      return MkTc(pt, FbleNewTcValue(vh, &let_tc->_base));
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;

      // Note: typeof(poly<arg>) = typeof(poly)<arg>
      // TypeCheckExpr gives us typeof(poly)
      Tc poly = TypeCheckExpr(th, vh, scope, apply->poly);
      if (poly.type == NULL) {
        return TC_FAILED;
      }

      FblePolyKind* poly_kind = (FblePolyKind*)FbleGetKind(arena, poly.type);
      if (poly_kind->_base.tag != FBLE_POLY_KIND) {
        ReportError(arena, expr->loc,
            "cannot apply poly args to a basic kinded entity\n");
        FbleFreeKind(arena, &poly_kind->_base);
        FreeTc(th, vh, poly);
        return TC_FAILED;
      }

      // Note: arg_type is typeof(arg)
      FbleType* arg_type = TypeCheckExprForType(th, vh, scope, apply->arg);
      if (arg_type == NULL) {
        FbleFreeKind(arena, &poly_kind->_base);
        FreeTc(th, vh, poly);
        return TC_FAILED;
      }

      FbleKind* expected_kind = poly_kind->arg;
      FbleKind* actual_kind = FbleGetKind(arena, arg_type);
      if (!FbleKindsEqual(expected_kind, actual_kind)) {
        ReportError(arena, apply->arg->loc,
            "expected kind %k, but found something of kind %k\n",
            expected_kind, actual_kind);
        FbleFreeKind(arena, &poly_kind->_base);
        FbleFreeKind(arena, actual_kind);
        FbleReleaseType(th, arg_type);
        FreeTc(th, vh, poly);
        return TC_FAILED;
      }
      FbleFreeKind(arena, actual_kind);
      FbleFreeKind(arena, &poly_kind->_base);

      FbleType* arg = FbleValueOfType(th, arg_type);
      assert(arg != NULL && "TODO: poly apply arg is a value?");
      FbleReleaseType(th, arg_type);

      FbleType* pat = FbleNewPolyApplyType(th, expr->loc, poly.type, arg);
      FbleReleaseType(th, arg);
      FbleReleaseType(th, poly.type);

      // When we erase types, poly application dissappears, because we already
      // supplied the generic type when creating the poly value.
      return MkTc(pat, poly.tc);
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* list_expr = (FbleListExpr*)expr;

      bool error = false;
      FbleType* type = NULL;
      FbleValue* args[list_expr->args.size];
      for (size_t i = 0; i < list_expr->args.size; ++i) {
        Tc tc = TypeCheckExpr(th, vh, scope, list_expr->args.xs[i]);
        error = error || (tc.type == NULL);

        if (type == NULL) {
          type = tc.type;
        } else if (tc.type != NULL) {
          if (!FbleTypesEqual(th, type, tc.type)) {
            error = true;
            ReportError(arena, list_expr->args.xs[i]->loc,
                "expected type %t, but found something of type %t\n",
                type, tc.type);
          }
          FbleReleaseType(th, tc.type);
        }
        args[i] = tc.tc;
      }

      if (error) {
        FbleReleaseType(th, type);
        for (size_t i = 0; i < list_expr->args.size; ++i) {
          FbleReleaseValue(vh, args[i]);
        }
        return TC_FAILED;
      }

      FbleValueV argv = { .size = list_expr->args.size, .xs = args };
      Tc tc = MkTc(FbleNewListType(th, type), NewListTc(vh, expr->loc, argv));
      FbleReleaseType(th, type);
      return tc;
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* literal = (FbleLiteralExpr*)expr;

      Tc spec = TypeCheckExpr(th, vh, scope, literal->spec);
      if (spec.type == NULL) {
        return TC_FAILED;
      }

      FbleDataType* normal = (FbleDataType*)FbleNormalType(th, spec.type);
      if (normal->_base.tag != FBLE_DATA_TYPE || normal->datatype != FBLE_STRUCT_DATATYPE) {
        ReportError(arena, literal->spec->loc,
            "expected a struct value, but literal spec has type %t\n",
            spec);
        FreeTc(th, vh, spec);
        FbleReleaseType(th, &normal->_base);
        return TC_FAILED;
      }

      size_t n = strlen(literal->word);
      if (n == 0) {
        ReportError(arena, literal->word_loc,
            "literals must not be empty\n");
        FreeTc(th, vh, spec);
        FbleReleaseType(th, &normal->_base);
        return TC_FAILED;
      }

      bool error = false;
      FbleType* type = NULL;    // Borrowed from normal.
      FbleValue* args[n];
      FbleLoc loc = literal->word_loc;
      for (size_t i = 0; i < n; ++i) {
        char field_str[2] = { literal->word[i], '\0' };
        args[i] = NULL;
        bool found = false;
        for (size_t j = 0; j < normal->fields.size; ++j) {
          if (strcmp(field_str, normal->fields.xs[j].name.name->str) == 0) {
            found = true;
            if (type == NULL) {
              type = normal->fields.xs[j].type;
            } else if (!FbleTypesEqual(th, type, normal->fields.xs[j].type)) {
              ReportError(arena, loc, "expected type %t, but found something of type %t\n",
                  type, normal->fields.xs[j].type);
              break;
            }

            FbleVarValue* var_v = FbleNewValue(vh, FbleVarValue);
            var_v->_base.tag = FBLE_VAR_VALUE;
            var_v->index.source = FBLE_LOCAL_VAR;
            var_v->index.index = scope->vars.size;

            FbleDataAccessValue* access_v = FbleNewValue(vh, FbleDataAccessValue);
            access_v->_base.tag = FBLE_DATA_ACCESS_VALUE;
            access_v->datatype = FBLE_STRUCT_DATATYPE;
            access_v->obj = &var_v->_base;
            FbleValueAddRef(vh, &access_v->_base, access_v->obj);
            FbleReleaseValue(vh, access_v->obj);
            access_v->tag = j;
            access_v->loc = FbleCopyLoc(loc);
            args[i] = &access_v->_base;
            break;
          }
        }
        error = error || (args[i] == NULL);

        if (!found) {
          ReportError(arena, loc, "'%s' is not a field of type %t\n",
              field_str, spec.type);
        }


        if (literal->word[i] == '\n') {
          loc.line++;
          loc.col = 0;
        }
        loc.col++;
      }

      if (error) {
        for (size_t i = 0; i < n; ++i) {
          FbleReleaseValue(vh, args[i]);
        }
        FreeTc(th, vh, spec);
        FbleReleaseType(th, &normal->_base);
        return TC_FAILED;
      }

      FbleLetTc* let_tc = FbleAlloc(arena, FbleLetTc);
      let_tc->_base.tag = FBLE_LET_TC;
      let_tc->recursive = false;
      FbleVectorInit(arena, let_tc->bindings);
      FbleVectorAppend(arena, let_tc->bindings, spec.tc);
      FbleReleaseType(th, spec.type);

      FbleValueV argv = { .size = n, .xs = args, };
      let_tc->body = NewListTc(vh, literal->word_loc, argv);
      FbleType* list_type = FbleNewListType(th, type);
      FbleReleaseType(th, &normal->_base);
      return MkTc(list_type, FbleNewTcValue(vh, &let_tc->_base));
    }

    case FBLE_ELABORATE_EXPR: {
      FbleElaborateExpr* elaborate = (FbleElaborateExpr*)expr;

      Tc body = TypeCheckExpr(th, vh, scope, elaborate->body);
      if (body.type == NULL) {
        return TC_FAILED;
      }
      
      FbleFuncType* normal = (FbleFuncType*)FbleNormalType(th, body.type);
      if (normal->_base.tag == FBLE_PROC_TYPE) {
        ReportError(arena, expr->loc,
            "support for elaboration of proc types not yet implemented");
        FreeTc(th, vh, body);
        return TC_FAILED;
      }

      if (normal->_base.tag != FBLE_FUNC_TYPE) {
        // There's nothing symbolic involved in elaborating this kind of
        // expression, so we can just evaluate it directly.
        return body;
      }

      size_t argc = normal->args.size;
      FbleReleaseType(th, &normal->_base);

      FbleLetTc* let_tc = FbleAlloc(arena, FbleLetTc);
      let_tc->_base.tag = FBLE_LET_TC;
      let_tc->recursive = false;
      FbleVectorInit(arena, let_tc->bindings);
      size_t arg_ids = scope->vars.size;
      for (size_t i = 0; i < argc; ++i) {
        FbleSymbolicValueTc* arg = FbleAlloc(arena, FbleSymbolicValueTc);
        arg->_base.tag = FBLE_SYMBOLIC_VALUE_TC;
        FbleValue* arg_v = FbleNewTcValue(vh, &arg->_base);
        FbleVectorAppend(arena, let_tc->bindings, arg_v);
      }

      FbleFuncApplyTc* apply_tc = FbleAlloc(arena, FbleFuncApplyTc);
      apply_tc->_base.tag = FBLE_FUNC_APPLY_TC;
      apply_tc->loc = FbleCopyLoc(expr->loc);
      apply_tc->func = body.tc;
      FbleVectorInit(arena, apply_tc->args);
      for (size_t i = 0; i < argc; ++i) {
        FbleVarValue* var_v = FbleNewValue(vh, FbleVarValue);
        var_v->_base.tag = FBLE_VAR_VALUE;
        var_v->index.source = FBLE_LOCAL_VAR;
        var_v->index.index = arg_ids + i;
        FbleVectorAppend(arena, apply_tc->args, &var_v->_base);
      }

      FbleSymbolicCompileTc* compile_tc = FbleAlloc(arena, FbleSymbolicCompileTc);
      compile_tc->_base.tag = FBLE_SYMBOLIC_COMPILE_TC;
      compile_tc->loc = FbleCopyLoc(expr->loc);
      FbleVectorInit(arena, compile_tc->args);
      for (size_t i = 0; i < argc; ++i) {
        FbleVarIndex arg = { .source = FBLE_LOCAL_VAR, .index = arg_ids + i };
        FbleVectorAppend(arena, compile_tc->args, arg);
      }
      compile_tc->body = FbleNewTcValue(vh, &apply_tc->_base);
      let_tc->body = FbleNewTcValue(vh, &compile_tc->_base);

      return MkTc(body.type, FbleNewTcValue(vh, &let_tc->_base));
    }

    case FBLE_MODULE_REF_EXPR: {
      FbleModuleRefExpr* module_ref_expr = (FbleModuleRefExpr*)expr;

      Var* var = GetVar(th, scope, module_ref_expr->ref.resolved, false);

      // We should have resolved all modules at program load time.
      assert(var != NULL && "module not in scope");
      assert(var->type != NULL && "recursive module reference");

      FbleVarValue* var_v = FbleNewValue(vh, FbleVarValue);
      var_v->_base.tag = FBLE_VAR_VALUE;
      var_v->index = var->index;
      return MkTc(FbleRetainType(th, var->type), &var_v->_base);
    }

    case FBLE_DATA_ACCESS_EXPR: {
      FbleDataAccessExpr* access_expr = (FbleDataAccessExpr*)expr;

      Tc obj = TypeCheckExpr(th, vh, scope, access_expr->object);
      if (obj.type == NULL) {
        return TC_FAILED;
      }

      FbleDataType* normal = (FbleDataType*)FbleNormalType(th, obj.type);
      if (normal->_base.tag != FBLE_DATA_TYPE) {
        ReportError(arena, access_expr->object->loc,
            "expected value of type struct or union, but found value of type %t\n",
            obj.type);

        FreeTc(th, vh, obj);
        FbleReleaseType(th, &normal->_base);
        return TC_FAILED;
      }

      FbleTaggedTypeV* fields = &normal->fields;
      for (size_t i = 0; i < fields->size; ++i) {
        if (FbleNamesEqual(access_expr->field, fields->xs[i].name)) {
          FbleType* rtype = FbleRetainType(th, fields->xs[i].type);
          FbleReleaseType(th, &normal->_base);

          FbleDataAccessValue* access_v = FbleNewValue(vh, FbleDataAccessValue);
          access_v->_base.tag = FBLE_DATA_ACCESS_VALUE;
          access_v->datatype = normal->datatype;
          access_v->obj = obj.tc;
          FbleValueAddRef(vh, &access_v->_base, access_v->obj);
          FbleReleaseValue(vh, access_v->obj);
          access_v->tag = i;
          access_v->loc = FbleCopyLoc(access_expr->field.loc);
          FbleReleaseType(th, obj.type);
          return MkTc(rtype, &access_v->_base);
        }
      }

      ReportError(arena, access_expr->field.loc,
          "'%n' is not a field of type %t\n",
          access_expr->field, obj.type);
      FreeTc(th, vh, obj);
      FbleReleaseType(th, &normal->_base);
      return TC_FAILED;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleApplyExpr* apply_expr = (FbleApplyExpr*)expr;

      Tc misc = TypeCheckExpr(th, vh, scope, apply_expr->misc);
      bool error = (misc.type == NULL);

      size_t argc = apply_expr->args.size;
      Tc args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = TypeCheckExpr(th, vh, scope, apply_expr->args.xs[i]);
        error = error || (args[i].type == NULL);
      }

      if (error) {
        FreeTc(th, vh, misc);
        for (size_t i = 0; i < argc; ++i) {
          FreeTc(th, vh, args[i]);
        }
        return TC_FAILED;
      }

      FbleType* normal = FbleNormalType(th, misc.type);
      if (normal->tag == FBLE_FUNC_TYPE) {
        // FUNC_APPLY
        FbleFuncType* func_type = (FbleFuncType*)normal;
        if (func_type->args.size != argc) {
          ReportError(arena, expr->loc,
              "expected %i args, but found %i\n",
              func_type->args.size, argc);
          FbleReleaseType(th, normal);
          FreeTc(th, vh, misc);
          for (size_t i = 0; i < argc; ++i) {
            FreeTc(th, vh, args[i]);
          }
          return TC_FAILED;
        }

        for (size_t i = 0; i < argc; ++i) {
          if (!FbleTypesEqual(th, func_type->args.xs[i], args[i].type)) {
            ReportError(arena, apply_expr->args.xs[i]->loc,
                "expected type %t, but found %t\n",
                func_type->args.xs[i], args[i].type);
            FbleReleaseType(th, normal);
            FreeTc(th, vh, misc);
            // TODO: This double for loop thing is pretty ugly. Anything we
            // can do to clean up?
            for (size_t j = 0; j < i; ++j) {
              FbleReleaseValue(vh, args[j].tc);
            }
            for (size_t j = i; j < argc; ++j) {
              FreeTc(th, vh, args[j]);
            }
            return TC_FAILED;
          }
          FbleReleaseType(th, args[i].type);
        }

        FbleType* rtype = FbleRetainType(th, func_type->rtype);
        FbleReleaseType(th, normal);
        FbleReleaseType(th, misc.type);

        FbleFuncApplyTc* apply_tc = FbleAlloc(arena, FbleFuncApplyTc);
        apply_tc->_base.tag = FBLE_FUNC_APPLY_TC;
        apply_tc->loc = FbleCopyLoc(expr->loc);
        apply_tc->func = misc.tc;
        FbleVectorInit(arena, apply_tc->args);
        for (size_t i = 0; i < argc; ++i) {
          FbleVectorAppend(arena, apply_tc->args, args[i].tc);
        }

        return MkTc(rtype, FbleNewTcValue(vh, &apply_tc->_base));
      }

      if (normal->tag == FBLE_TYPE_TYPE) {
        // FBLE_STRUCT_VALUE_EXPR
        FbleTypeType* type_type = (FbleTypeType*)normal;
        FbleType* vtype = FbleRetainType(th, type_type->type);
        FbleReleaseType(th, normal);
        FreeTc(th, vh, misc);

        FbleDataType* struct_type = (FbleDataType*)FbleNormalType(th, vtype);
        if (struct_type->datatype != FBLE_STRUCT_DATATYPE) {
          ReportError(arena, apply_expr->misc->loc,
              "expected a struct type, but found %t\n",
              vtype);
          FbleReleaseType(th, &struct_type->_base);
          FbleReleaseType(th, vtype);
          for (size_t i = 0; i < argc; ++i) {
            FreeTc(th, vh, args[i]);
          }
          return TC_FAILED;
        }

        if (struct_type->fields.size != argc) {
          // TODO: Where should the error message go?
          ReportError(arena, expr->loc,
              "expected %i args, but %i provided\n",
               struct_type->fields.size, argc);
          FbleReleaseType(th, &struct_type->_base);
          FbleReleaseType(th, vtype);
          for (size_t i = 0; i < argc; ++i) {
            FreeTc(th, vh, args[i]);
          }
          return TC_FAILED;
        }

        bool error = false;
        for (size_t i = 0; i < argc; ++i) {
          FbleTaggedType* field = struct_type->fields.xs + i;

          if (!FbleTypesEqual(th, field->type, args[i].type)) {
            ReportError(arena, apply_expr->args.xs[i]->loc,
                "expected type %t, but found %t\n",
                field->type, args[i]);
            error = true;
          }
        }

        FbleReleaseType(th, &struct_type->_base);

        if (error) {
          FbleReleaseType(th, vtype);
          for (size_t i = 0; i < argc; ++i) {
            FreeTc(th, vh, args[i]);
          }
          return TC_FAILED;
        }

        FbleValueV argv;
        FbleVectorInit(arena, argv);
        for (size_t i = 0; i < argc; ++i) {
          FbleReleaseType(th, args[i].type);
          FbleVectorAppend(arena, argv, args[i].tc);
        }

        FbleValue* struct_v = FbleNewStructValue(vh, argv);

        for (size_t i = 0; i < argc; ++i) {
          FbleReleaseValue(vh, argv.xs[i]);
        }
        FbleFree(arena, argv.xs);
        return MkTc(vtype, struct_v);
      }

      ReportError(arena, expr->loc,
          "expecting a function or struct type, but found something of type %t\n",
          misc.type);
      FreeTc(th, vh, misc);
      FbleReleaseType(th, normal);
      for (size_t i = 0; i < argc; ++i) {
        FreeTc(th, vh, args[i]);
      }
      return TC_FAILED;
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
static Tc TypeCheckExec(FbleTypeHeap* th, FbleValueHeap* vh, Scope* scope, FbleExpr* expr)
{
  FbleArena* arena = vh->arena;
  switch (expr->tag) {
    case FBLE_TYPEOF_EXPR:
    case FBLE_VAR_EXPR:
    case FBLE_LET_EXPR:
    case FBLE_DATA_TYPE_EXPR:
    case FBLE_DATA_ACCESS_EXPR:
    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR:
    case FBLE_UNION_VALUE_EXPR:
    case FBLE_UNION_SELECT_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_FUNC_VALUE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_POLY_VALUE_EXPR:
    case FBLE_POLY_APPLY_EXPR:
    case FBLE_LIST_EXPR:
    case FBLE_LITERAL_EXPR:
    case FBLE_ELABORATE_EXPR:
    case FBLE_MODULE_REF_EXPR:
    case FBLE_MISC_APPLY_EXPR:
    {
      Tc proc = TypeCheckExpr(th, vh, scope, expr);
      if (proc.type == NULL) {
        return TC_FAILED;
      }

      FbleProcType* normal = (FbleProcType*)FbleNormalType(th, proc.type);
      if (normal->_base.tag != FBLE_PROC_TYPE) {
        ReportError(arena, expr->loc,
            "expected process, but found expression of type %t\n",
            proc);
        FbleReleaseType(th, &normal->_base);
        FreeTc(th, vh, proc);
        return TC_FAILED;
      }

      FbleType* rtype = FbleRetainType(th, normal->type);
      FbleReleaseType(th, &normal->_base);
      FbleReleaseType(th, proc.type);

      FbleFuncApplyTc* apply_tc = FbleAlloc(arena, FbleFuncApplyTc);
      apply_tc->_base.tag = FBLE_FUNC_APPLY_TC;
      apply_tc->loc = FbleCopyLoc(expr->loc);
      apply_tc->func = proc.tc;
      FbleVectorInit(arena, apply_tc->args);

      return MkTc(rtype, FbleNewTcValue(vh, &apply_tc->_base));
    }

    case FBLE_EVAL_EXPR: {
      FbleEvalExpr* eval_expr = (FbleEvalExpr*)expr;
      return TypeCheckExpr(th, vh, scope, eval_expr->body);
    }

    case FBLE_LINK_EXPR: {
      FbleLinkExpr* link_expr = (FbleLinkExpr*)expr;
      if (FbleNamesEqual(link_expr->get, link_expr->put)) {
        ReportError(arena, link_expr->put.loc,
            "duplicate port name '%n'\n",
            link_expr->put);
        return TC_FAILED;
      }

      FbleType* port_type = TypeCheckType(th, vh, scope, link_expr->type);
      if (port_type == NULL) {
        return TC_FAILED;
      }

      FbleProcType* get_type = FbleNewType(th, FbleProcType, FBLE_PROC_TYPE, port_type->loc);
      get_type->type = port_type;
      FbleTypeAddRef(th, &get_type->_base, get_type->type);

      FbleDataType* unit_type = FbleNewType(th, FbleDataType, FBLE_DATA_TYPE, expr->loc);
      unit_type->datatype = FBLE_STRUCT_DATATYPE;
      FbleVectorInit(arena, unit_type->fields);

      FbleProcType* unit_proc_type = FbleNewType(th, FbleProcType, FBLE_PROC_TYPE, expr->loc);
      unit_proc_type->type = &unit_type->_base;
      FbleTypeAddRef(th, &unit_proc_type->_base, unit_proc_type->type);
      FbleReleaseType(th, &unit_type->_base);

      FbleFuncType* put_type = FbleNewType(th, FbleFuncType, FBLE_FUNC_TYPE, expr->loc);
      FbleVectorInit(arena, put_type->args);
      FbleVectorAppend(arena, put_type->args, port_type);
      FbleTypeAddRef(th, &put_type->_base, port_type);
      FbleReleaseType(th, port_type);
      put_type->rtype = &unit_proc_type->_base;
      FbleTypeAddRef(th, &put_type->_base, put_type->rtype);
      FbleReleaseType(th, &unit_proc_type->_base);

      PushVar(arena, scope, link_expr->get, &get_type->_base);
      PushVar(arena, scope, link_expr->put, &put_type->_base);

      Tc body = TypeCheckExec(th, vh, scope, link_expr->body);

      PopVar(th, scope);
      PopVar(th, scope);

      if (body.type == NULL) {
        return TC_FAILED;
      }

      FbleLinkTc* link_tc = FbleAlloc(arena, FbleLinkTc);
      link_tc->_base.tag = FBLE_LINK_TC;
      link_tc->body = body.tc;
      return MkTc(body.type, FbleNewTcValue(vh, &link_tc->_base));
    }

    case FBLE_EXEC_EXPR: {
      FbleExecExpr* exec_expr = (FbleExecExpr*)expr;
      bool error = false;

      // Evaluate the types of the bindings and set up the new vars.
      FbleType* types[exec_expr->bindings.size];
      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        types[i] = TypeCheckType(th, vh, scope, exec_expr->bindings.xs[i].type);
        error = error || (types[i] == NULL);
      }

      FbleExecTc* exec_tc = FbleAlloc(arena, FbleExecTc);
      exec_tc->_base.tag = FBLE_EXEC_TC;
      FbleVectorInit(arena, exec_tc->bindings);
      exec_tc->body = NULL;

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        Tc binding = TypeCheckExpr(th, vh, scope, exec_expr->bindings.xs[i].expr);
        if (binding.type != NULL) {
          FbleProcType* proc_type = (FbleProcType*)FbleNormalType(th, binding.type);
          if (proc_type->_base.tag == FBLE_PROC_TYPE) {
            if (types[i] != NULL && !FbleTypesEqual(th, types[i], proc_type->type)) {
              error = true;
              ReportError(arena, exec_expr->bindings.xs[i].expr->loc,
                  "expected type %t!, but found %t\n",
                  types[i], binding.type);
            }
          } else {
            error = true;
            ReportError(arena, exec_expr->bindings.xs[i].expr->loc,
                "expected process, but found expression of type %t\n",
                binding.type);
          }
          FbleReleaseType(th, &proc_type->_base);
        } else {
          error = true;
        }
        FbleVectorAppend(arena, exec_tc->bindings, binding.tc);
        FbleReleaseType(th, binding.type);
      }

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        PushVar(arena, scope, exec_expr->bindings.xs[i].name, types[i]);
      }

      Tc body = TC_FAILED;
      if (!error) {
        body = TypeCheckExec(th, vh, scope, exec_expr->body);
      }

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        PopVar(th, scope);
      }

      if (body.type == NULL) {
        FbleFreeTc(vh, &exec_tc->_base);
        return TC_FAILED;
      }

      exec_tc->body = body.tc;
      return MkTc(body.type, FbleNewTcValue(vh, &exec_tc->_base));
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
static FbleType* TypeCheckExprForType(FbleTypeHeap* th, FbleValueHeap* vh, Scope* scope, FbleExpr* expr)
{
  FbleArena* arena = vh->arena;
  Scope nscope;
  InitScope(arena, &nscope, NULL, scope);

  Tc result = TypeCheckExpr(th, vh, &nscope, expr);
  FreeScope(th, &nscope);
  FbleReleaseValue(vh, result.tc);
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
static FbleType* TypeCheckType(FbleTypeHeap* th, FbleValueHeap* vh, Scope* scope, FbleTypeExpr* type)
{
  FbleArena* arena = vh->arena;
  switch (type->tag) {
    case FBLE_TYPEOF_EXPR: {
      FbleTypeofExpr* typeof = (FbleTypeofExpr*)type;
      return TypeCheckExprForType(th, vh, scope, typeof->expr);
    }

    case FBLE_DATA_TYPE_EXPR: {
      FbleDataTypeExpr* data_type = (FbleDataTypeExpr*)type;

      FbleDataType* dt = FbleNewType(th, FbleDataType, FBLE_DATA_TYPE, type->loc);
      dt->datatype = data_type->datatype;
      FbleVectorInit(arena, dt->fields);

      for (size_t i = 0; i < data_type->fields.size; ++i) {
        FbleTaggedTypeExpr* field = data_type->fields.xs + i;
        FbleType* compiled = TypeCheckType(th, vh, scope, field->type);
        if (compiled == NULL) {
          FbleReleaseType(th, &dt->_base);
          return NULL;
        }

        if (!CheckNameSpace(arena, field->name, compiled)) {
          FbleReleaseType(th, compiled);
          FbleReleaseType(th, &dt->_base);
          return NULL;
        }

        FbleTaggedType cfield = {
          .name = FbleCopyName(arena, field->name),
          .type = compiled
        };
        FbleVectorAppend(arena, dt->fields, cfield);

        FbleTypeAddRef(th, &dt->_base, cfield.type);
        FbleReleaseType(th, cfield.type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name, data_type->fields.xs[j].name)) {
            ReportError(arena, field->name.loc,
                "duplicate field name '%n'\n",
                field->name);
            FbleReleaseType(th, &dt->_base);
            return NULL;
          }
        }
      }
      return &dt->_base;
    }

    case FBLE_FUNC_TYPE_EXPR: {
      FbleFuncType* ft = FbleNewType(th, FbleFuncType, FBLE_FUNC_TYPE, type->loc);
      FbleFuncTypeExpr* func_type = (FbleFuncTypeExpr*)type;

      FbleVectorInit(arena, ft->args);
      ft->rtype = NULL;

      bool error = false;
      for (size_t i = 0; i < func_type->args.size; ++i) {
        FbleType* arg = TypeCheckType(th, vh, scope, func_type->args.xs[i]);
        if (arg == NULL) {
          error = true;
        } else {
          FbleVectorAppend(arena, ft->args, arg);
          FbleTypeAddRef(th, &ft->_base, arg);
        }
        FbleReleaseType(th, arg);
      }

      if (error) {
        FbleReleaseType(th, &ft->_base);
        return NULL;
      }

      FbleType* rtype = TypeCheckType(th, vh, scope, func_type->rtype);
      if (rtype == NULL) {
        FbleReleaseType(th, &ft->_base);
        return NULL;
      }
      ft->rtype = rtype;
      FbleTypeAddRef(th, &ft->_base, ft->rtype);
      FbleReleaseType(th, ft->rtype);
      return &ft->_base;
    }

    case FBLE_PROC_TYPE_EXPR: {
      FbleProcType* ut = FbleNewType(th, FbleProcType, FBLE_PROC_TYPE, type->loc);
      ut->type = NULL;

      FbleProcTypeExpr* unary_type = (FbleProcTypeExpr*)type;
      ut->type = TypeCheckType(th, vh, scope, unary_type->type);
      if (ut->type == NULL) {
        FbleReleaseType(th, &ut->_base);
        return NULL;
      }
      FbleTypeAddRef(th, &ut->_base, ut->type);
      FbleReleaseType(th, ut->type);
      return &ut->_base;
    }

    case FBLE_VAR_EXPR:
    case FBLE_LET_EXPR:
    case FBLE_DATA_ACCESS_EXPR:
    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR:
    case FBLE_UNION_VALUE_EXPR:
    case FBLE_UNION_SELECT_EXPR:
    case FBLE_FUNC_VALUE_EXPR:
    case FBLE_EVAL_EXPR:
    case FBLE_LINK_EXPR:
    case FBLE_EXEC_EXPR:
    case FBLE_POLY_VALUE_EXPR:
    case FBLE_POLY_APPLY_EXPR:
    case FBLE_LIST_EXPR:
    case FBLE_LITERAL_EXPR:
    case FBLE_ELABORATE_EXPR:
    case FBLE_MODULE_REF_EXPR:
    case FBLE_MISC_APPLY_EXPR:
    {
      FbleExpr* expr = type;
      FbleType* type = TypeCheckExprForType(th, vh, scope, expr);
      if (type == NULL) {
        return NULL;
      }

      FbleType* type_value = FbleValueOfType(th, type);
      if (type_value == NULL) {
        ReportError(arena, expr->loc,
            "expected a type, but found value of type %t\n",
            type);
        FbleReleaseType(th, type);
        return NULL;
      }
      FbleReleaseType(th, type);
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
static FbleValue* TypeCheckProgram(FbleTypeHeap* th, FbleValueHeap* vh, Scope* scope, FbleModule* modules, size_t modulec, FbleExpr* body)
{
  FbleArena* arena = vh->arena;

  if (modulec == 0) {
    Tc result = TypeCheckExpr(th, vh, scope, body);
    FbleReleaseType(th, result.type);
    return result.tc;
  }

  // Push a dummy variable representing the value of the computed module,
  // because we'll be turning this into a LET_TC, which assumes a variable
  // index is consumed by the thing being defined. The module loading process
  // is responsible for ensuring we will never try to access the variable in
  // the definition of the module.
  PushVar(arena, scope, modules->name, NULL);
  Tc module = TypeCheckExpr(th, vh, scope, modules->value);
  module = ProfileBlock(vh, modules->name, modules->value->loc, module);
  PopVar(th, scope);

  if (module.type == NULL) {
    return NULL;
  }

  PushVar(arena, scope, modules->name, module.type);
  FbleValue* body_tc = TypeCheckProgram(th, vh, scope, modules + 1, modulec - 1, body);
  PopVar(th, scope);

  if (body_tc == NULL) {
    FbleReleaseValue(vh, module.tc);
    return NULL;
  }

  // TODO: Using modules->name.loc here is not terribly useful for users. The
  // plan eventually is to remove FbleLoc from FbleTc, so hopefully this
  // weirdness will solve itself by going away sometime in the near future.
  FbleLetTc* let_tc = FbleAlloc(arena, FbleLetTc);
  let_tc->_base.tag = FBLE_LET_TC;
  let_tc->recursive = false;
  FbleVectorInit(arena, let_tc->bindings);
  FbleVectorAppend(arena, let_tc->bindings, module.tc);
  let_tc->body = body_tc;
  return FbleNewTcValue(vh, &let_tc->_base);
}

// FbleTypeCheck -- see documentation in typecheck.h
FbleValue* FbleTypeCheck(FbleValueHeap* heap, struct FbleProgram* program)
{
  Scope scope;
  InitScope(heap->arena, &scope, NULL, NULL);

  FbleTypeHeap* th = FbleNewTypeHeap(heap->arena);
  FbleValue* result = TypeCheckProgram(th, heap, &scope, program->modules.xs, program->modules.size, program->main);
  FreeScope(th, &scope);
  FbleFreeTypeHeap(th);
  return result;
}

// FbleFreeTc -- see documentation in typecheck.h
void FbleFreeTc(FbleValueHeap* heap, FbleTc* tc)
{
  FbleArena* arena = heap->arena;
  if (tc == NULL) {
    return;
  }

  switch (tc->tag) {
    case FBLE_LET_TC: {
      FbleLetTc* let_tc = (FbleLetTc*)tc;
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        FbleReleaseValue(heap, let_tc->bindings.xs[i]);
      }
      FbleFree(arena, let_tc->bindings.xs);
      FbleReleaseValue(heap, let_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_UNION_SELECT_TC: {
      FbleUnionSelectTc* select_tc = (FbleUnionSelectTc*)tc;
      FbleFreeLoc(arena, select_tc->loc);
      FbleReleaseValue(heap, select_tc->condition);
      FbleFree(arena, select_tc->choices.xs);
      for (size_t i = 0; i < select_tc->branches.size; ++i) {
        FbleReleaseValue(heap, select_tc->branches.xs[i]);
      }
      FbleFree(arena, select_tc->branches.xs);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)tc;
      FbleFreeLoc(arena, func_tc->body_loc);
      FbleFree(arena, func_tc->scope.xs);
      FbleReleaseValue(heap, func_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)tc;
      FbleFreeLoc(arena, apply_tc->loc);
      FbleReleaseValue(heap, apply_tc->func);
      for (size_t i = 0; i < apply_tc->args.size; ++i) {
        FbleReleaseValue(heap, apply_tc->args.xs[i]);
      }
      FbleFree(arena, apply_tc->args.xs);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_LINK_TC: {
      FbleLinkTc* link_tc = (FbleLinkTc*)tc;
      FbleReleaseValue(heap, link_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_EXEC_TC: {
      FbleExecTc* exec_tc = (FbleExecTc*)tc;
      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        FbleReleaseValue(heap, exec_tc->bindings.xs[i]);
      }
      FbleFree(arena, exec_tc->bindings.xs);
      FbleReleaseValue(heap, exec_tc->body);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_SYMBOLIC_VALUE_TC: {
      FbleFree(arena, tc);
      return;
    }

    case FBLE_SYMBOLIC_COMPILE_TC: {
      FbleSymbolicCompileTc* compile_tc = (FbleSymbolicCompileTc*)tc;
      FbleFreeLoc(arena, compile_tc->loc);
      FbleReleaseValue(heap, compile_tc->body);
      FbleFree(arena, compile_tc->args.xs);
      FbleFree(arena, tc);
      return;
    }

    case FBLE_PROFILE_TC: {
      FbleProfileTc* profile_tc = (FbleProfileTc*)tc;
      FbleFreeLoc(arena, profile_tc->loc);
      FbleFreeName(arena, profile_tc->name);
      FbleReleaseValue(heap, profile_tc->body);
      FbleFree(arena, tc);
      return;
    }
  }

  UNREACHABLE("should never get here");
}

// FbleNewTcValue -- see documentation in typecheck.h
FbleValue* FbleNewTcValue(FbleValueHeap* heap, FbleTc* tc)
{
  FbleTcValue* v = FbleNewValue(heap, FbleTcValue);
  v->_base.tag = FBLE_TC_VALUE;
  v->tc = tc;
  return &v->_base;
}
