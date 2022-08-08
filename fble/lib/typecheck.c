// compile.c --
//   This file implements the fble compilation routines.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_arg, va_end
#include <stdio.h>    // for fprintf, stderr
#include <string.h>   // for strlen, strcat
#include <stdlib.h>   // for NULL

#include "fble-vector.h"

#include "expr.h"
#include "tc.h"
#include "type.h"
#include "typecheck.h"

#define UNREACHABLE(x) assert(false && x)

// VarName --
//   Variables can refer to normal values or module values.
//
// module == NULL means this is a normal value with name in 'normal'.
// module != NULL means this is a module value with path in 'module'.
typedef struct {
  FbleName normal;
  FbleModulePath* module;
} VarName;

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
  VarName name;
  FbleModulePath* path;
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
//     Variables may be NULL to indicate they are anonymous.
//     Takes ownership of the Vars.
//   captured - Collects the source of variables captured from the parent
//              scope. May be NULL to indicate that operations on this scope
//              should not have any side effects on the parent scope.
//   module - the current module being compiled.
//   parent - the parent of this scope. May be NULL.
typedef struct Scope {
  VarV statics;
  VarV vars;
  FbleVarIndexV* captured;
  FbleModulePath* module;
  struct Scope* parent;
} Scope;

static bool VarNamesEqual(VarName a, VarName b);
static Var* PushVar(Scope* scope, VarName name, FbleType* type);
static void PopVar(FbleTypeHeap* heap, Scope* scope);
static Var* GetVar(FbleTypeHeap* heap, Scope* scope, VarName name, bool phantom);

static void InitScope(Scope* scope, FbleVarIndexV* captured, FbleModulePath* module, Scope* parent);
static void FreeScope(FbleTypeHeap* heap, Scope* scope);

static void ReportError(FbleLoc loc, const char* fmt, ...);
static bool CheckNameSpace(FbleName name, FbleType* type);

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
static void FreeTc(FbleTypeHeap* th, Tc tc);

// Cleaner --
//   Tracks values to be automatically cleaned up when exiting a typecheck
//   function.
typedef struct {
  FbleTypeV types;
  FbleTcV tcs;
  struct {
    size_t size;
    FbleTypeAssignmentV** xs;
  } tyvars;
} Cleaner;

static Cleaner* NewCleaner();
static void CleanType(Cleaner* cleaner, FbleType* type);
static void CleanTc(Cleaner* cleaner, Tc tc);
static void CleanTypeAssignmentV(Cleaner* cleaner, FbleTypeAssignmentV* vars);
static void Cleanup(FbleTypeHeap* th, Cleaner* cleaner);

static FbleType* DepolyType(FbleTypeHeap* th, FbleType* type, FbleTypeAssignmentV* vars);
static Tc PolyApply(FbleTypeHeap* th, Scope* scope, Tc poly, FbleType* arg_type, FbleLoc expr_loc, FbleLoc arg_loc);

static Tc TypeCheckExpr(FbleTypeHeap* th, Scope* scope, FbleExpr* expr);
static Tc TypeCheckExprWithCleaner(FbleTypeHeap* th, Scope* scope, FbleExpr* expr, Cleaner* cleaner);
static FbleType* TypeCheckType(FbleTypeHeap* th, Scope* scope, FbleTypeExpr* type);
static FbleType* TypeCheckTypeWithCleaner(FbleTypeHeap* th, Scope* scope, FbleTypeExpr* type, Cleaner* cleaner);
static FbleType* TypeCheckExprForType(FbleTypeHeap* th, Scope* scope, FbleExpr* expr);
static Tc TypeCheckModule(FbleTypeHeap* th, FbleLoadedModule* module, FbleType** deps);


// VarNamesEqual --
//   Test whether two variable names are equal.
//
// Inputs:
//   a - the first variable name.
//   b - the second variable name.
//
// Results:
//   true if the names are equal, false otherwise.
//
// Side effects:
//   None.
static bool VarNamesEqual(VarName a, VarName b)
{
  if (a.module == NULL && b.module == NULL) {
    return FbleNamesEqual(a.normal, b.normal);
  }

  if (a.module != NULL && b.module != NULL) {
    return FbleModulePathsEqual(a.module, b.module);
  }

  return false;
}

// PushVar --
//   Push a variable onto the current scope.
//
// Inputs:
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
//   freed. Does not take ownership of name. It is the callers responsibility
//   to ensure that 'name' outlives the returned Var.
static Var* PushVar(Scope* scope, VarName name, FbleType* type)
{
  Var* var = FbleAlloc(Var);
  var->name = name;
  var->type = type;
  var->used = false;
  var->accessed = false;
  var->index.source = FBLE_LOCAL_VAR;
  var->index.index = scope->vars.size;
  FbleVectorAppend(scope->vars, var);
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
  scope->vars.size--;
  Var* var = scope->vars.xs[scope->vars.size];
  if (var != NULL) {
    FbleReleaseType(heap, var->type);
    FbleFree(var);
  }
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
static Var* GetVar(FbleTypeHeap* heap, Scope* scope, VarName name, bool phantom)
{
  for (size_t i = 0; i < scope->vars.size; ++i) {
    size_t j = scope->vars.size - i - 1;
    Var* var = scope->vars.xs[j];
    if (var != NULL && VarNamesEqual(name, var->name)) {
      var->accessed = true;
      if (!phantom) {
        var->used = true;
      }
      return var;
    }
  }

  for (size_t i = 0; i < scope->statics.size; ++i) {
    Var* var = scope->statics.xs[i];
    if (var != NULL && VarNamesEqual(name, var->name)) {
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

      Var* captured_var = FbleAlloc(Var);
      captured_var->name = var->name;
      captured_var->type = FbleRetainType(heap, var->type);
      captured_var->used = !phantom;
      captured_var->accessed = true;
      captured_var->index.source = FBLE_STATIC_VAR;
      captured_var->index.index = scope->statics.size;
      FbleVectorAppend(scope->statics, captured_var);
      if (scope->captured != NULL) {
        FbleVectorAppend(*scope->captured, var->index);
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
//   scope - the scope to initialize.
//   captured - Collects the source of variables captured from the parent
//              scope. May be NULL to indicate that operations on this scope
//              should not have any side effects on the parent scope.
//   module - the current module. Borrowed.
//   parent - the parent of the scope to initialize. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Initializes scope based on parent. FreeScope should be
//   called to free the allocations for scope. The lifetime of the parent
//   scope must exceed the lifetime of this scope.
static void InitScope(Scope* scope, FbleVarIndexV* captured, FbleModulePath* module, Scope* parent)
{
  FbleVectorInit(scope->statics);
  FbleVectorInit(scope->vars);
  scope->captured = captured;
  scope->module = FbleCopyModulePath(module);
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
  for (size_t i = 0; i < scope->statics.size; ++i) {
    FbleReleaseType(heap, scope->statics.xs[i]->type);
    FbleFree(scope->statics.xs[i]);
  }
  FbleFree(scope->statics.xs);

  while (scope->vars.size > 0) {
    PopVar(heap, scope);
  }
  FbleFree(scope->vars.xs);
  FbleFreeModulePath(scope->module);
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
//     %m - FbleModulePath*
//     %% - literal '%'
//   Please add additional format specifiers as needed.
//
// Inputs:
//   loc - the location of the error.
//   fmt - the format string.
//   ... - The var-args for the associated conversion specifiers in fmt.
//
// Results:
//   none.
//
// Side effects:
//   Prints a message to stderr as described by fmt and provided arguments.
static void ReportError(FbleLoc loc, const char* fmt, ...)
{
  FbleReportError("", loc);

  va_list ap;
  va_start(ap, fmt);

  for (const char* p = strchr(fmt, '%'); p != NULL; p = strchr(fmt, '%')) {
    fprintf(stderr, "%.*s", (int)(p - fmt), fmt);

    switch (*(p + 1)) {
      case '%': {
        fprintf(stderr, "%%");
        break;
      }

      case 'i': {
        size_t x = va_arg(ap, size_t);
        fprintf(stderr, "%zd", x);
        break;
      }

      case 'k': {
        FbleKind* kind = va_arg(ap, FbleKind*);
        FblePrintKind(kind);
        break;
      }

      case 'm': {
        FbleModulePath* path = va_arg(ap, FbleModulePath*);
        FblePrintModulePath(stderr, path);
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
        FblePrintType(type);
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
//   name - the name in question
//   type - the type of the value refered to by the name.
//
// Results:
//   true if the namespace of the name is consistent with the type. false
//   otherwise.
//
// Side effects:
//   Prints a message to stderr if the namespace and type don't match.
static bool CheckNameSpace(FbleName name, FbleType* type)
{
  FbleKind* kind = FbleGetKind(type);
  size_t kind_level = FbleGetKindLevel(kind);
  FbleFreeKind(kind);

  bool match = (kind_level == 0 && name.space == FBLE_NORMAL_NAME_SPACE)
            || (kind_level == 1 && name.space == FBLE_TYPE_NAME_SPACE);

  if (!match) {
    ReportError(name.loc,
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
//   th - heap to use for allocations
//   tc - tc to free the fields of. May be TC_FAILED.
//
// Side effects:
//   Frees resources associated with the type and tc fields of tc.
static void FreeTc(FbleTypeHeap* th, Tc tc)
{
  FbleReleaseType(th, tc.type);
  FbleFreeTc(tc.tc);
}

// NewCleaner --
//   Allocate and return a new cleaner object.
//
// Results:
//   A newly allocated cleaner object.
//
// Side effects:
//   The user should call a WithCleanup function to free resources associated
//   with the cleaner object when no longer needed.
static Cleaner* NewCleaner()
{
  Cleaner* cleaner = FbleAlloc(Cleaner);
  FbleVectorInit(cleaner->types);
  FbleVectorInit(cleaner->tcs);
  FbleVectorInit(cleaner->tyvars);
  return cleaner;
}

// CleanType --
//   Add a type to the cleaner to be released when cleanup occurs.
//
// Inputs:
//   cleaner - the cleaner to add the type to.
//   type - the type to track.
//
// Side effects:
//   The type will be automatically released when the cleaner is cleaned up.
static void CleanType(Cleaner* cleaner, FbleType* type)
{
  FbleVectorAppend(cleaner->types, type);
}

// CleanTc --
//   Add a tc to the cleaner to be released when cleanup occurs.
//
// Inputs:
//   cleaner - the cleaner to add the type to.
//   tc - the tc to track.
//
// Side effects:
//   The tc will be automatically released when the cleaner is cleaned up.
static void CleanTc(Cleaner* cleaner, Tc tc)
{
  FbleVectorAppend(cleaner->types, tc.type);
  FbleVectorAppend(cleaner->tcs, tc.tc);
}

// CleanTc --
//   Add an FbleTypeAssignmentV to the cleaner to be released when cleanup
//   occurs.
//
// Inputs:
//   cleaner - the cleaner to add the type to.
//   vars - the vars to track. Should be allocated with FbleAlloc.
//
// Side effects:
//   The vars will be automatically released when the cleaner is cleaned up.
static void CleanTypeAssignmentV(Cleaner* cleaner, FbleTypeAssignmentV* vars)
{
  FbleVectorAppend(cleaner->tyvars, vars);
}

// Cleanup -- 
//   Invoke cleanup.
//
// Inputs:
//   th - the type heap used for cleanup.
//   cleaner - the cleaner object to trigger cleanup on.
//
// Side effects:
//   Cleans up all objects tracked by the cleaner along with the cleaner
//   object itself.
static void Cleanup(FbleTypeHeap* th, Cleaner* cleaner)
{
  for (size_t i = 0; i < cleaner->types.size; ++i) {
    FbleReleaseType(th, cleaner->types.xs[i]);
  }
  FbleFree(cleaner->types.xs);

  for (size_t i = 0; i < cleaner->tcs.size; ++i) {
    FbleFreeTc(cleaner->tcs.xs[i]);
  }
  FbleFree(cleaner->tcs.xs);

  for (size_t i = 0; i < cleaner->tyvars.size; ++i) {
    FbleTypeAssignmentV* vars = cleaner->tyvars.xs[i];
    for (size_t j = 0; j < vars->size; ++j) {
      FbleReleaseType(th, vars->xs[j].value);
    }
    FbleFree(vars->xs);
    FbleFree(vars);
  }
  FbleFree(cleaner->tyvars.xs);

  FbleFree(cleaner);
}

// DepolyType --
//   Normalize and remove any layers of polymorphic type variables from the
//   given type.
//
// Inputs:
//   th - the type heap
//   type - the type to normalize and unwrap poly type vars from.
//   vars - output variable to populate unwrapped type variables to.
//
// Results:
//   The normalized and depolyed type.
//
// Side effects:
// * Adds unwrapped type variables to vars.
// * The caller should call FbleReleaseType on the returned type when no
//   longer needed.
static FbleType* DepolyType(FbleTypeHeap* th, FbleType* type, FbleTypeAssignmentV* vars)
{
  FbleType* pbody = FbleNormalType(th, type);
  while (pbody->tag == FBLE_POLY_TYPE) {
    FblePolyType* poly = (FblePolyType*)pbody;
    FbleTypeAssignment var = { .var = poly->arg, .value = NULL };
    FbleVectorAppend(*vars, var);

    FbleType* next = FbleNormalType(th, poly->body);
    FbleReleaseType(th, pbody);
    pbody = next;
  }
  return pbody;
}

// PolyApply --
//   Helper function for type checking poly application.
//
// Inputs:
//   th - the type heap
//   scope - the current scope
//   poly - The type and value of the poly. Borrowed.
//   arg_type - The type of the arg type to apply the poly to. May be NULL. Borrowed.
//   expr_loc - The location of the application expression.
//   arg_loc - The location of the argument type.
//
// Returns:
//   The Tc for the application of the poly to the argument, or TC_FAILED in
//   case of error.
//
// Side effects:
// * Reports an error in case of error.
// * The caller should call FbleFreeTc when the returned FbleTc is no longer
//   needed and FbleReleaseType when the returned FbleType is no longer
//   needed.
static Tc PolyApply(FbleTypeHeap* th, Scope* scope, Tc poly, FbleType* arg_type, FbleLoc expr_loc, FbleLoc arg_loc)
{
  // Note: typeof(poly<arg>) = typeof(poly)<arg>
  // poly.type is typeof(poly)
  if (poly.type == NULL) {
    return TC_FAILED;
  }

  // Note: arg_type is typeof(arg)
  if (arg_type == NULL) {
    return TC_FAILED;
  }

  FblePolyKind* poly_kind = (FblePolyKind*)FbleGetKind(poly.type);
  if (poly_kind->_base.tag == FBLE_POLY_KIND) {
    // poly_apply
    FbleKind* expected_kind = poly_kind->arg;
    FbleKind* actual_kind = FbleGetKind(arg_type);
    if (!FbleKindsEqual(expected_kind, actual_kind)) {
      ReportError(arg_loc,
          "expected kind %k, but found something of kind %k\n",
          expected_kind, actual_kind);
      FbleFreeKind(&poly_kind->_base);
      FbleFreeKind(actual_kind);
      return TC_FAILED;
    }
    FbleFreeKind(actual_kind);
    FbleFreeKind(&poly_kind->_base);

    FbleType* arg = FbleValueOfType(th, arg_type);
    assert(arg != NULL && "TODO: poly apply arg is a value?");

    FbleType* pat = FbleNewPolyApplyType(th, expr_loc, poly.type, arg);
    FbleReleaseType(th, arg);

    // When we erase types, poly application dissappears, because we already
    // supplied the generic type when creating the poly value.
    return MkTc(pat, FbleCopyTc(poly.tc));
  }
  FbleFreeKind(&poly_kind->_base);

  FbleType* poly_value = FbleValueOfType(th, poly.type);
  if (poly_value != NULL) {
    FblePackageType* package = (FblePackageType*)FbleNormalType(th, poly_value);
    FbleReleaseType(th, poly_value);
    if (package->_base.tag == FBLE_PACKAGE_TYPE) {
      // abstract_type
      FbleType* arg = FbleValueOfType(th, arg_type);
      if (arg == NULL) {
        ReportError(arg_loc,
            "expected type, but found something of kind %%\n");
        FbleReleaseType(th, &package->_base);
        return TC_FAILED;
      }

      FbleAbstractType* abs_type = FbleNewType(th, FbleAbstractType, FBLE_ABSTRACT_TYPE, expr_loc);
      abs_type->package = package;
      abs_type->type = arg;
      FbleTypeAddRef(th, &abs_type->_base, &abs_type->package->_base);
      FbleTypeAddRef(th, &abs_type->_base, abs_type->type);
      FbleReleaseType(th, &abs_type->package->_base);
      FbleReleaseType(th, abs_type->type);

      FbleTypeType* type_type = FbleNewType(th, FbleTypeType, FBLE_TYPE_TYPE, expr_loc);
      type_type->type = &abs_type->_base;
      FbleTypeAddRef(th, &type_type->_base, type_type->type);
      FbleReleaseType(th, &abs_type->_base);

      FbleTypeValueTc* type_tc = FbleNewTc(FbleTypeValueTc, FBLE_TYPE_VALUE_TC, expr_loc);
      return MkTc(&type_type->_base, &type_tc->_base);
    }
    FbleReleaseType(th, &package->_base);
  }

  ReportError(expr_loc,
      "type application requires a poly or package type\n");
  return TC_FAILED;
}

// TypeCheckExpr --
//   Type check the given expression.
//
// Inputs:
//   th - heap to use for type allocations.
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
static Tc TypeCheckExpr(FbleTypeHeap* th, Scope* scope, FbleExpr* expr)
{
  Cleaner* cleaner = NewCleaner();
  Tc result = TypeCheckExprWithCleaner(th, scope, expr, cleaner);
  Cleanup(th, cleaner);
  return result;
}

// TypeCheckExprWithCleaner
//   Same as TypeCheckExpr, except with a cleaner provided for convenience.
//
// See documentation of TypeCheckExpr.
//
// Objects added to the cleaner will be automatically cleaned up by the caller
// when this function returns.
static Tc TypeCheckExprWithCleaner(FbleTypeHeap* th, Scope* scope, FbleExpr* expr, Cleaner* cleaner)
{
  switch (expr->tag) {
    case FBLE_DATA_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PACKAGE_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR:
    {
      FbleType* type = TypeCheckType(th, scope, expr);
      CleanType(cleaner, type);
      if (type == NULL) {
        return TC_FAILED;
      }

      FbleTypeType* type_type = FbleNewType(th, FbleTypeType, FBLE_TYPE_TYPE, expr->loc);
      type_type->type = type;
      FbleTypeAddRef(th, &type_type->_base, type_type->type);

      FbleTypeValueTc* type_tc = FbleNewTc(FbleTypeValueTc, FBLE_TYPE_VALUE_TC, expr->loc);
      return MkTc(&type_type->_base, &type_tc->_base);
    }

    case FBLE_VAR_EXPR: {
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;
      VarName name = { .normal = var_expr->var, .module = NULL };
      Var* var = GetVar(th, scope, name, false);
      if (var == NULL) {
        ReportError(var_expr->var.loc, "variable '%n' not defined\n", var_expr->var);
        return TC_FAILED;
      }

      FbleVarTc* var_tc = FbleNewTc(FbleVarTc, FBLE_VAR_TC, expr->loc);
      var_tc->index = var->index;
      return MkTc(FbleRetainType(th, var->type), &var_tc->_base);
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
            .name = FbleNewString(renamed),
            .space = FBLE_TYPE_NAME_SPACE,
            .loc = binding->name.loc,
          };

          types[i] = FbleNewVarType(th, binding->name.loc, binding->kind, type_name);
          FbleFreeString(type_name.name);
        } else {
          assert(binding->kind == NULL);
          types[i] = TypeCheckType(th, scope, binding->type);
          error = error || (types[i] == NULL);
        }
        
        if (types[i] != NULL && !CheckNameSpace(binding->name, types[i])) {
          error = true;
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(let_expr->bindings.xs[i].name, let_expr->bindings.xs[j].name)) {
            ReportError(let_expr->bindings.xs[i].name.loc,
                "duplicate variable name '%n'\n",
                let_expr->bindings.xs[i].name);
            error = true;
          }
        }
      }

      Var* vars[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        VarName name = { .normal = let_expr->bindings.xs[i].name, .module = NULL };
        vars[i] = PushVar(scope, name, types[i]);
      }

      // Compile the values of the variables.
      Tc defs[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleBinding* binding = let_expr->bindings.xs + i;

        defs[i] = TC_FAILED;
        if (!error) {
          defs[i] = TypeCheckExpr(th, scope, binding->expr);
        }
        error = error || (defs[i].type == NULL);

        if (!error && binding->type != NULL && !FbleTypesEqual(th, types[i], defs[i].type)) {
          error = true;
          ReportError(binding->expr->loc,
              "expected type %t, but found something of type %t\n",
              types[i], defs[i].type);
        } else if (!error && binding->type == NULL) {
          FbleKind* expected_kind = FbleGetKind(types[i]);
          FbleKind* actual_kind = FbleGetKind(defs[i].type);
          if (!FbleKindsEqual(expected_kind, actual_kind)) {
            ReportError(binding->expr->loc,
                "expected kind %k, but found something of kind %k\n",
                expected_kind, actual_kind);
            error = true;
          }
          FbleFreeKind(expected_kind);
          FbleFreeKind(actual_kind);
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
            ReportError(let_expr->bindings.xs[i].name.loc,
                "%n is vacuous\n", let_expr->bindings.xs[i].name);
            error = true;
          }
        }
      }

      Tc body = TC_FAILED;
      if (!error) {
        body = TypeCheckExpr(th, scope, let_expr->body);
        error = (body.type == NULL);
      }

      if (body.type != NULL) {
        for (size_t i = 0; i < let_expr->bindings.size; ++i) {
          if (!vars[i]->accessed
              && vars[i]->name.module == NULL
              && vars[i]->name.normal.name->str[0] != '_') {
            FbleReportWarning("variable '", vars[i]->name.normal.loc);
            FblePrintName(stderr, vars[i]->name.normal);
            fprintf(stderr, "' defined but not used\n");
          }
        }
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        PopVar(th, scope);
      }

      if (error) {
        for (size_t i = 0; i < let_expr->bindings.size; ++i) {
          FbleFreeTc(defs[i].tc);
        }
        FreeTc(th, body);
        return TC_FAILED;
      }

      FbleLetTc* let_tc = FbleNewTc(FbleLetTc, FBLE_LET_TC, expr->loc);
      let_tc->recursive = recursive;
      FbleVectorInit(let_tc->bindings);
      let_tc->body = body.tc;
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleTcBinding ltc = {
          .name = FbleCopyName(let_expr->bindings.xs[i].name),
          .loc = FbleCopyLoc(let_expr->bindings.xs[i].expr->loc),
          .tc = defs[i].tc
        };
        FbleVectorAppend(let_tc->bindings, ltc);
      }
        
      return MkTc(body.type, &let_tc->_base);
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      FbleStructValueImplicitTypeExpr* struct_expr = (FbleStructValueImplicitTypeExpr*)expr;

      FbleDataType* struct_type = FbleNewType(th, FbleDataType, FBLE_DATA_TYPE, expr->loc);
      struct_type->datatype = FBLE_STRUCT_DATATYPE;
      FbleVectorInit(struct_type->fields);
      CleanType(cleaner, &struct_type->_base);

      size_t argc = struct_expr->args.size;
      Tc args[argc];
      bool error = false;
      for (size_t i = 0; i < argc; ++i) {
        size_t j = argc - i - 1;
        FbleTaggedExpr* arg = struct_expr->args.xs + j;
        args[j] = TypeCheckExpr(th, scope, arg->expr);
        CleanTc(cleaner, args[j]);
        error = error || (args[j].type == NULL);
      }

      for (size_t i = 0; i < argc; ++i) {
        FbleTaggedExpr* arg = struct_expr->args.xs + i;
        if (args[i].type != NULL) {
          if (!CheckNameSpace(arg->name, args[i].type)) {
            error = true;
          }

          FbleTaggedType cfield = {
            .name = FbleCopyName(arg->name),
            .type = args[i].type
          };
          FbleVectorAppend(struct_type->fields, cfield);
          FbleTypeAddRef(th, &struct_type->_base, cfield.type);
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(arg->name, struct_expr->args.xs[j].name)) {
            error = true;
            ReportError(arg->name.loc,
                "duplicate field name '%n'\n",
                struct_expr->args.xs[j].name);
          }
        }
      }

      if (error) {
        return TC_FAILED;
      }

      FbleStructValueTc* struct_tc = FbleNewTcExtra(FbleStructValueTc, FBLE_STRUCT_VALUE_TC, argc * sizeof(FbleTc*), expr->loc);
      struct_tc->fieldc = argc;
      for (size_t i = 0; i < argc; ++i) {
        struct_tc->fields[i] = FbleCopyTc(args[i].tc);
      }

      return MkTc(FbleRetainType(th, &struct_type->_base), &struct_tc->_base);
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      FbleType* type = TypeCheckType(th, scope, union_value_expr->type);
      CleanType(cleaner, type);
      if (type == NULL) {
        return TC_FAILED;
      }

      FbleDataType* union_type = (FbleDataType*)FbleNormalType(th, type);
      CleanType(cleaner, &union_type->_base);
      if (union_type->datatype != FBLE_UNION_DATATYPE) {
        ReportError(union_value_expr->type->loc,
            "expected a union type, but found %t\n", type);
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
        ReportError(union_value_expr->field.loc,
            "'%n' is not a field of type %t\n",
            union_value_expr->field, type);
        return TC_FAILED;
      }

      Tc arg = TypeCheckExpr(th, scope, union_value_expr->arg);
      CleanTc(cleaner, arg);
      if (arg.type == NULL) {
        return TC_FAILED;
      }

      if (!FbleTypesEqual(th, field_type, arg.type)) {
        ReportError(union_value_expr->arg->loc,
            "expected type %t, but found type %t\n",
            field_type, arg.type);
        return TC_FAILED;
      }

      FbleUnionValueTc* union_tc = FbleNewTc(FbleUnionValueTc, FBLE_UNION_VALUE_TC, expr->loc);
      union_tc->tag = tag;
      union_tc->arg = FbleCopyTc(arg.tc);
      return MkTc(FbleRetainType(th, type), &union_tc->_base);
    }

    case FBLE_UNION_SELECT_EXPR: {
      FbleUnionSelectExpr* select_expr = (FbleUnionSelectExpr*)expr;

      Tc condition = TypeCheckExpr(th, scope, select_expr->condition);
      if (condition.type == NULL) {
        return TC_FAILED;
      }

      FbleDataType* union_type = (FbleDataType*)FbleNormalType(th, condition.type);
      if (union_type->_base.tag != FBLE_DATA_TYPE
          || union_type->datatype != FBLE_UNION_DATATYPE) {
        ReportError(select_expr->condition->loc,
            "expected value of union type, but found value of type %t\n",
            condition.type);
        FbleReleaseType(th, &union_type->_base);
        FreeTc(th, condition);
        return TC_FAILED;
      }
      FbleReleaseType(th, condition.type);

      FbleUnionSelectTc* select_tc = FbleNewTcExtra(FbleUnionSelectTc, FBLE_UNION_SELECT_TC, union_type->fields.size * sizeof(FbleTc*), expr->loc);
      select_tc->condition = condition.tc;
      FbleVectorInit(select_tc->choices);

      bool error = false;
      FbleType* target = NULL;

      FbleTcBinding default_ = { .tc = NULL };
      bool default_used = false;
      if (select_expr->default_ != NULL) {
        Tc result = TypeCheckExpr(th, scope, select_expr->default_);
        error = error || (result.type == NULL);
        if (result.type != NULL) {
          FbleName label = {
            .name = FbleNewString(":"),
            .space = FBLE_NORMAL_NAME_SPACE,
            .loc = FbleCopyLoc(select_expr->default_->loc),
          };
          default_.name = label;
          default_.loc = FbleCopyLoc(select_expr->default_->loc);
          default_.tc = result.tc;
        }
        target = result.type;
      }

      size_t branch = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        if (branch < select_expr->choices.size && FbleNamesEqual(select_expr->choices.xs[branch].name, union_type->fields.xs[i].name)) {
          Tc result = TypeCheckExpr(th, scope, select_expr->choices.xs[branch].expr);
          error = error || (result.type == NULL);

          if (result.type != NULL) {
            FbleTcBinding choice = {
              .name = FbleCopyName(select_expr->choices.xs[branch].name),
              .loc = FbleCopyLoc(select_expr->choices.xs[branch].expr->loc),
              .tc = result.tc,
            };
            FbleVectorAppend(select_tc->choices, choice);
          }

          if (target == NULL) {
            target = result.type;
          } else if (result.type != NULL) {
            if (!FbleTypesEqual(th, target, result.type)) {
              ReportError(select_expr->choices.xs[branch].expr->loc,
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
            ReportError(select_expr->choices.xs[branch].name.loc,
                "expected tag '%n', but found '%n'\n",
                union_type->fields.xs[i].name, select_expr->choices.xs[branch].name);
          } else {
            ReportError(expr->loc,
                "missing tag '%n'\n",
                union_type->fields.xs[i].name);
          }
        } else {
          // Use the default branch for this field.
          if (default_.tc != NULL) {
            default_used = true;
            FbleVectorAppend(select_tc->choices, default_);
          }
        }
      }

      if (default_.tc != NULL && !default_used) {
        FbleFreeName(default_.name);
        FbleFreeLoc(default_.loc);
        FbleFreeTc(default_.tc);
      }

      if (branch < select_expr->choices.size) {
        ReportError(select_expr->choices.xs[branch].name.loc,
            "unexpected tag '%n'\n",
            select_expr->choices.xs[branch]);
        error = true;
      }

      FbleReleaseType(th, &union_type->_base);
      Tc tc = MkTc(target, &select_tc->_base);
      if (error) {
        FreeTc(th, tc);
        return TC_FAILED;
      }
      return tc;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      size_t argc = func_value_expr->args.size;

      bool error = false;
      FbleTypeV arg_types;
      FbleVectorInit(arg_types);
      for (size_t i = 0; i < argc; ++i) {
        FbleType* arg_type = TypeCheckType(th, scope, func_value_expr->args.xs[i].type);
        FbleVectorAppend(arg_types, arg_type);
        error = error || arg_type == NULL;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(func_value_expr->args.xs[i].name, func_value_expr->args.xs[j].name)) {
            error = true;
            ReportError(func_value_expr->args.xs[i].name.loc,
                "duplicate arg name '%n'\n",
                func_value_expr->args.xs[i].name);
          }
        }
      }

      if (error) {
        for (size_t i = 0; i < argc; ++i) {
          FbleReleaseType(th, arg_types.xs[i]);
        }
        FbleFree(arg_types.xs);
        return TC_FAILED;
      }


      FbleVarIndexV captured;
      FbleVectorInit(captured);
      Scope func_scope;
      InitScope(&func_scope, &captured, scope->module, scope);

      for (size_t i = 0; i < argc; ++i) {
        VarName name = { .normal = func_value_expr->args.xs[i].name, .module = NULL };
        PushVar(&func_scope, name, arg_types.xs[i]);
      }

      Tc func_result = TypeCheckExpr(th, &func_scope, func_value_expr->body);
      if (func_result.type == NULL) {
        FreeScope(th, &func_scope);
        FbleFree(arg_types.xs);
        FbleFree(captured.xs);
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

      FbleFuncValueTc* func_tc = FbleNewTc(FbleFuncValueTc, FBLE_FUNC_VALUE_TC, expr->loc);
      func_tc->body_loc = FbleCopyLoc(func_value_expr->body->loc);
      func_tc->scope = captured;
      FbleVectorInit(func_tc->args);
      for (size_t i = 0; i < argc; ++i) {
        FbleVectorAppend(func_tc->args, FbleCopyName(func_value_expr->args.xs[i].name));
      }
      func_tc->body = func_result.tc;

      FreeScope(th, &func_scope);
      return MkTc(&ft->_base, &func_tc->_base);
    }

    case FBLE_POLY_VALUE_EXPR: {
      FblePolyValueExpr* poly = (FblePolyValueExpr*)expr;

      if (FbleGetKindLevel(poly->arg.kind) != 1) {
        ReportError(poly->arg.kind->loc,
            "expected a type kind, but found %k\n",
            poly->arg.kind);
        return TC_FAILED;
      }

      if (poly->arg.name.space != FBLE_TYPE_NAME_SPACE) {
        ReportError(poly->arg.name.loc,
            "the namespace of '%n' is not appropriate for kind %k\n",
            poly->arg.name, poly->arg.kind);
        return TC_FAILED;
      }

      FbleType* arg_type = FbleNewVarType(th, poly->arg.name.loc, poly->arg.kind, poly->arg.name);
      FbleType* arg = FbleValueOfType(th, arg_type);
      CleanType(cleaner, arg);
      assert(arg != NULL);

      VarName name = { .normal = poly->arg.name, .module = NULL };
      PushVar(scope, name, arg_type);
      Tc body = TypeCheckExpr(th, scope, poly->body);
      CleanTc(cleaner, body);
      PopVar(th, scope);

      if (body.type == NULL) {
        return TC_FAILED;
      }

      FbleType* pt = FbleNewPolyType(th, expr->loc, arg, body.type);

      // A poly value expression gets rewritten as a let when we erase types:
      // <@ T@> ...
      // turns into:
      //   let T@ = type
      //   in ...

      // TODO: Do we really have to allocate a type value here? Can we
      // optimize this away?
      FbleTypeValueTc* type_tc = FbleNewTc(FbleTypeValueTc, FBLE_TYPE_VALUE_TC, expr->loc);

      FbleLetTc* let_tc = FbleNewTc(FbleLetTc, FBLE_LET_TC, expr->loc);
      let_tc->recursive = false;
      let_tc->body = FbleCopyTc(body.tc);
      FbleVectorInit(let_tc->bindings);
      FbleTcBinding ltc = {
        .name = FbleCopyName(poly->arg.name),
        .loc = FbleCopyLoc(poly->arg.name.loc),
        .tc = &type_tc->_base
      };
      FbleVectorAppend(let_tc->bindings, ltc);

      return MkTc(pt, &let_tc->_base);
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;
      Tc poly = TypeCheckExpr(th, scope, apply->poly);
      CleanTc(cleaner, poly);
      FbleType* arg_type = TypeCheckExprForType(th, scope, apply->arg);
      CleanType(cleaner, arg_type);
      return PolyApply(th, scope, poly, arg_type, expr->loc, apply->arg->loc);
    }

    case FBLE_ABSTRACT_CAST_EXPR: {
      FbleAbstractCastExpr* cast_expr = (FbleAbstractCastExpr*)expr;

      FbleType* package_type = TypeCheckType(th, scope, cast_expr->package);
      CleanType(cleaner, package_type);
      if (package_type == NULL) {
        return TC_FAILED;
      }

      FblePackageType* package = (FblePackageType*)FbleNormalType(th, package_type);
      CleanType(cleaner, &package->_base);
      if (package->_base.tag != FBLE_PACKAGE_TYPE) {
        ReportError(cast_expr->package->loc,
          "expected package type, but found %t\n", package_type);
        return TC_FAILED;
      }

      FbleType* target = TypeCheckType(th, scope, cast_expr->target);
      CleanType(cleaner, target);
      if (target == NULL) {
        return TC_FAILED;
      }

      Tc value = TypeCheckExpr(th, scope, cast_expr->value);
      CleanTc(cleaner, value);
      if (value.type == NULL) {
        return TC_FAILED;
      }

      if (!FbleModuleBelongsToPackage(scope->module, package->path)) {
        ReportError(expr->loc, "Module %m is not allowed access to package %m\n",
            scope->module, package->path);
        return TC_FAILED;
      }

      assert(package->opaque);
      package->opaque = false;
      bool legal = FbleTypesEqual(th, target, value.type);
      package->opaque = true;

      if (!legal) {
        ReportError(expr->loc, "cannot cast value of type %t to %t\n", value.type, target);
        return TC_FAILED;
      }

      return MkTc(FbleRetainType(th, target), FbleCopyTc(value.tc));
    }

    case FBLE_ABSTRACT_ACCESS_EXPR: {
      FbleAbstractAccessExpr* access_expr = (FbleAbstractAccessExpr*)expr;

      Tc value = TypeCheckExpr(th, scope, access_expr->value);
      CleanTc(cleaner, value);
      if (value.type == NULL) {
        return TC_FAILED;
      }

      FbleAbstractType* abstract_type = (FbleAbstractType*)FbleNormalType(th, value.type);
      CleanType(cleaner, &abstract_type->_base);
      if (abstract_type->_base.tag != FBLE_ABSTRACT_TYPE) {
        ReportError(expr->loc, "expected value of abstract type, but found some of type %t\n",
            value.type);
        return TC_FAILED;
      }

      if (!FbleModuleBelongsToPackage(scope->module, abstract_type->package->path)) {
        ReportError(expr->loc, "Module %m is not allowed to access package %m\n",
            scope->module, abstract_type->package->path);
        return TC_FAILED;
      }

      FbleType* type = FbleRetainType(th, abstract_type->type);
      return MkTc(type, FbleCopyTc(value.tc));
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* list_expr = (FbleListExpr*)expr;

      Tc func = TypeCheckExpr(th, scope, list_expr->func);
      CleanTc(cleaner, func);
      if (func.type == NULL) {
        return TC_FAILED;
      }

      FbleFuncType* func_type = (FbleFuncType*)FbleNormalType(th, func.type);
      CleanType(cleaner, &func_type->_base);
      if (func_type->_base.tag != FBLE_FUNC_TYPE || func_type->args.size != 1) {
        ReportError(list_expr->func->loc, "expected a function of one argument, but found something of type %t\n", func.type);
        return TC_FAILED;
      }

      FbleType* elem_type = FbleListElementType(th, func_type->args.xs[0]);
      CleanType(cleaner, elem_type);
      if (elem_type == NULL) {
        ReportError(list_expr->func->loc, "expected a list type, but the input to the function has type %t\n", func_type->args.xs[0]);
        return TC_FAILED;
      }

      bool error = false;

      size_t argc = list_expr->args.size;
      FbleTc* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        Tc tc = TypeCheckExpr(th, scope, list_expr->args.xs[i]);
        CleanTc(cleaner, tc);
        error = error || (tc.type == NULL);

        if (tc.type != NULL) {
          if (!FbleTypesEqual(th, elem_type, tc.type)) {
            error = true;
            ReportError(list_expr->args.xs[i]->loc,
                "expected type %t, but found something of type %t\n",
                elem_type, tc.type);
          }
        }
        args[i] = tc.tc;
      }

      if (error) {
        return TC_FAILED;
      }

      FbleType* result_type = FbleRetainType(th, func_type->rtype);

      FbleListTc* list_tc = FbleNewTcExtra(FbleListTc, FBLE_LIST_TC, argc * sizeof(FbleTc*), expr->loc);
      list_tc->fieldc = argc;
      for (size_t i = 0; i < argc; ++i) {
        list_tc->fields[i] = FbleCopyTc(args[i]);
      }

      FbleFuncApplyTc* apply = FbleNewTc(FbleFuncApplyTc, FBLE_FUNC_APPLY_TC, expr->loc);
      apply->func = FbleCopyTc(func.tc);
      FbleVectorInit(apply->args);
      FbleVectorAppend(apply->args, &list_tc->_base);
      return MkTc(result_type, &apply->_base);
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* literal_expr = (FbleLiteralExpr*)expr;

      Tc func = TypeCheckExpr(th, scope, literal_expr->func);
      CleanTc(cleaner, func);
      if (func.type == NULL) {
        return TC_FAILED;
      }

      FbleFuncType* func_type = (FbleFuncType*)FbleNormalType(th, func.type);
      CleanType(cleaner, &func_type->_base);
      if (func_type->_base.tag != FBLE_FUNC_TYPE || func_type->args.size != 1) {
        ReportError(literal_expr->func->loc, "expected a function of one argument, but found something of type %t\n", func.type);
        return TC_FAILED;
      }

      FbleType* elem_type = FbleListElementType(th, func_type->args.xs[0]);
      CleanType(cleaner, elem_type);
      if (elem_type == NULL) {
        ReportError(literal_expr->func->loc, "expected a list type, but the input to the function has type %t\n", func_type->args.xs[0]);
        return TC_FAILED;
      }

      FbleDataType* elem_data_type = (FbleDataType*)FbleNormalType(th, elem_type);
      CleanType(cleaner, &elem_data_type->_base);
      if (elem_data_type->_base.tag != FBLE_DATA_TYPE || elem_data_type->datatype != FBLE_UNION_DATATYPE) {
        ReportError(literal_expr->func->loc, "expected union type, but element type of literal expression is %t\n", elem_type);
        return TC_FAILED;
      }

      size_t argc = strlen(literal_expr->word);
      size_t args[argc];

      FbleDataType* unit_type = FbleNewType(th, FbleDataType, FBLE_DATA_TYPE, expr->loc);
      unit_type->datatype = FBLE_STRUCT_DATATYPE;
      FbleVectorInit(unit_type->fields);
      CleanType(cleaner, &unit_type->_base);

      bool error = false;
      FbleLoc loc = literal_expr->word_loc;
      for (size_t i = 0; i < argc; ++i) {
        char field_str[2] = { literal_expr->word[i], '\0' };
        bool found = false;
        for (size_t j = 0; j < elem_data_type->fields.size; ++j) {
          if (strcmp(field_str, elem_data_type->fields.xs[j].name.name->str) == 0) {
            found = true;
            if (!FbleTypesEqual(th, &unit_type->_base, elem_data_type->fields.xs[j].type)) {
              ReportError(loc, "expected field type %t, but letter '%s' has field type %t\n",
                  unit_type, field_str, elem_data_type->fields.xs[j].type);
              error = true;
              break;
            }

            args[i] = j;
            break;
          }
        }

        if (!found) {
          ReportError(loc, "'%s' is not a field of type %t\n", field_str, elem_type);
          error = true;
        }

        if (literal_expr->word[i] == '\n') {
          loc.line++;
          loc.col = 0;
        }
        loc.col++;
      }

      if (error) {
        return TC_FAILED;
      }

      FbleType* result_type = FbleRetainType(th, func_type->rtype);

      FbleLiteralTc* literal_tc = FbleNewTcExtra(FbleLiteralTc, FBLE_LITERAL_TC, argc * sizeof(size_t), expr->loc);
      literal_tc->letterc = argc;
      for (size_t i = 0; i < argc; ++i) {
        literal_tc->letters[i] = args[i];
      }

      FbleFuncApplyTc* apply = FbleNewTc(FbleFuncApplyTc, FBLE_FUNC_APPLY_TC, expr->loc);
      apply->func = FbleCopyTc(func.tc);
      FbleVectorInit(apply->args);
      FbleVectorAppend(apply->args, &literal_tc->_base);
      return MkTc(result_type, &apply->_base);
    }

    case FBLE_MODULE_PATH_EXPR: {
      FbleModulePathExpr* path_expr = (FbleModulePathExpr*)expr;

      VarName name = { .module = path_expr->path };
      Var* var = GetVar(th, scope, name, false);

      // We should have resolved all modules at program load time.
      assert(var != NULL && "module not in scope");
      assert(var->type != NULL && "recursive module reference");

      FbleVarTc* var_tc = FbleNewTc(FbleVarTc, FBLE_VAR_TC, expr->loc);
      var_tc->index = var->index;
      return MkTc(FbleRetainType(th, var->type), &var_tc->_base);
    }

    case FBLE_DATA_ACCESS_EXPR: {
      FbleDataAccessExpr* access_expr = (FbleDataAccessExpr*)expr;

      Tc obj = TypeCheckExpr(th, scope, access_expr->object);
      CleanTc(cleaner, obj);
      if (obj.type == NULL) {
        return TC_FAILED;
      }

      FbleDataType* normal = (FbleDataType*)FbleNormalType(th, obj.type);
      CleanType(cleaner, &normal->_base);
      if (normal->_base.tag != FBLE_DATA_TYPE) {
        ReportError(access_expr->object->loc,
            "expected value of type struct or union, but found value of type %t\n",
            obj.type);
        return TC_FAILED;
      }

      FbleTaggedTypeV* fields = &normal->fields;
      for (size_t i = 0; i < fields->size; ++i) {
        if (FbleNamesEqual(access_expr->field, fields->xs[i].name)) {
          FbleType* rtype = FbleRetainType(th, fields->xs[i].type);

          FbleDataAccessTc* access_tc = FbleNewTc(FbleDataAccessTc, FBLE_DATA_ACCESS_TC, expr->loc);
          access_tc->datatype = normal->datatype;
          access_tc->obj = FbleCopyTc(obj.tc);
          access_tc->tag = i;
          access_tc->loc = FbleCopyLoc(access_expr->field.loc);
          return MkTc(rtype, &access_tc->_base);
        }
      }

      ReportError(access_expr->field.loc,
          "'%n' is not a field of type %t\n",
          access_expr->field, obj.type);
      return TC_FAILED;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleApplyExpr* apply_expr = (FbleApplyExpr*)expr;

      // Type check the function.
      Tc misc = TypeCheckExpr(th, scope, apply_expr->misc);
      CleanTc(cleaner, misc);
      bool error = (misc.type == NULL);

      // Type check the args.
      size_t argc = apply_expr->args.size;
      Tc args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = TypeCheckExpr(th, scope, apply_expr->args.xs[i]);
        CleanTc(cleaner, args[i]);
        error = error || (args[i].type == NULL);
      }

      if (error) {
        return TC_FAILED;
      }

      // Unwrap any layers of polymorphism from the function in preparation
      // for type inference.
      FbleTypeAssignmentV* vars = FbleAlloc(FbleTypeAssignmentV);
      FbleVectorInit(*vars);
      CleanTypeAssignmentV(cleaner, vars);

      FbleType* pbody = DepolyType(th, misc.type, vars);
      CleanType(cleaner, pbody);

      if (pbody->tag == FBLE_FUNC_TYPE) {
        FbleFuncType* func_type = (FbleFuncType*)pbody;
        if (func_type->args.size != argc) {
          ReportError(expr->loc, "expected %i args, but found %i\n", func_type->args.size, argc);
          return TC_FAILED;
        }

        // Check function arg types match what is expected.
        // Infer values for poly type variables as we go.
        for (size_t i = 0; i < argc; ++i) {
          if (!FbleTypeInfer(th, *vars, func_type->args.xs[i], args[i].type)) {
            ReportError(args[i].tc->loc,
                "expected type %t, but found %t\n",
                func_type->args.xs[i], args[i].type);
            error = true;
          }
        }

        // Apply type variables to the poly.
        // This checks that the inferred type variables have the correct kind.
        Tc poly = misc;
        for (size_t i = 0; !error && i < vars->size; ++i) {
          if (vars->xs[i].value == NULL) {
            ReportError(expr->loc, "unable to infer types for poly.\n");
            error = true;
            break;
          }

          FbleTypeType* arg_type = FbleNewType(th, FbleTypeType, FBLE_TYPE_TYPE, expr->loc);
          arg_type->type = vars->xs[i].value;
          FbleTypeAddRef(th, &arg_type->_base, arg_type->type);
          CleanType(cleaner, &arg_type->_base);

          poly = PolyApply(th, scope, poly, &arg_type->_base, expr->loc, expr->loc);
          CleanTc(cleaner, poly);
          if (poly.type == NULL) {
            error = true;
          }
        }

        if (error) {
          // An error message should already have been output for the error.
          // Add info about the inferred type variables.
          if (vars->size > 0) {
            fprintf(stderr, "Inferred types:\n");
            for (size_t i = 0; i < vars->size; ++i) {
              fprintf(stderr, "  ");
              FblePrintType(vars->xs[i].var);
              fprintf(stderr, ": ");
              if (vars->xs[i].value == NULL) {
                fprintf(stderr, "???");
              } else {
                FblePrintType(vars->xs[i].value);
              }
              fprintf(stderr, "\n");
            }
          }
          return TC_FAILED;
        }

        // Do the func apply.
        func_type = (FbleFuncType*)FbleNormalType(th, poly.type);
        CleanType(cleaner, &func_type->_base);
        assert(func_type->_base.tag == FBLE_FUNC_TYPE);

        FbleType* rtype = FbleRetainType(th, func_type->rtype);

        FbleFuncApplyTc* apply_tc = FbleNewTc(FbleFuncApplyTc, FBLE_FUNC_APPLY_TC, expr->loc);
        apply_tc->func = FbleCopyTc(poly.tc);
        FbleVectorInit(apply_tc->args);
        for (size_t i = 0; i < argc; ++i) {
          FbleVectorAppend(apply_tc->args, FbleCopyTc(args[i].tc));
        }
        return MkTc(rtype, &apply_tc->_base);
      }

      // TODO: support type inference on struct value.
      if (vars->size == 0 && !apply_expr->bind && pbody->tag == FBLE_TYPE_TYPE) {
        FbleTypeType* type_type = (FbleTypeType*)pbody;
        FbleType* vtype = FbleRetainType(th, type_type->type);
        CleanType(cleaner, vtype);

        FbleType* vnorm = FbleNormalType(th, vtype);
        CleanType(cleaner, vnorm);
        FbleDataType* struct_type = (FbleDataType*)vnorm;
        if (struct_type->_base.tag == FBLE_DATA_TYPE
            && struct_type->datatype == FBLE_STRUCT_DATATYPE) {
          // struct_value
          if (struct_type->fields.size != argc) {
            // TODO: Where should the error message go?
            ReportError(expr->loc, "expected %i args, but %i provided\n", struct_type->fields.size, argc);
            return TC_FAILED;
          }

          bool error = false;
          for (size_t i = 0; i < argc; ++i) {
            FbleTaggedType* field = struct_type->fields.xs + i;

            if (!FbleTypesEqual(th, field->type, args[i].type)) {
              ReportError(apply_expr->args.xs[i]->loc, "expected type %t, but found %t\n", field->type, args[i]);
              error = true;
            }
          }

          if (error) {
            return TC_FAILED;
          }

          FbleStructValueTc* struct_tc = FbleNewTcExtra(FbleStructValueTc, FBLE_STRUCT_VALUE_TC, argc * sizeof(FbleTc*), expr->loc);
          struct_tc->fieldc = argc;
          for (size_t i = 0; i < argc; ++i) {
            struct_tc->fields[i] = FbleCopyTc(args[i].tc);
          }
          return MkTc(FbleRetainType(th, vtype), &struct_tc->_base);
        }

        FblePackageType* pkg_type = (FblePackageType*)vnorm;
        if (pkg_type->_base.tag == FBLE_PACKAGE_TYPE) {
          // abstract_value
          if (argc != 1) {
            ReportError(expr->loc, "expected 1 argument, but %i provided\n", argc);
            return TC_FAILED;
          }

          if (!FbleModuleBelongsToPackage(scope->module, pkg_type->path)) {
            ReportError(expr->loc, "Module %m is not allowed access to package %m\n",
                scope->module, pkg_type->path);
            return TC_FAILED;
          }

          FbleAbstractType* abs_type = FbleNewType(th, FbleAbstractType, FBLE_ABSTRACT_TYPE, expr->loc);
          abs_type->package = pkg_type;
          abs_type->type = args[0].type;
          FbleTypeAddRef(th, &abs_type->_base, &abs_type->package->_base);
          FbleTypeAddRef(th, &abs_type->_base, abs_type->type);

          return MkTc(&abs_type->_base, FbleCopyTc(args[0].tc));
        }
      }

      if (apply_expr->bind) {
        ReportError(apply_expr->misc->loc,
            "invalid type for bind function: %t\n", misc.type);
      } else {
        ReportError(expr->loc,
            "cannot apply arguments to something of type %t\n", misc.type);
      }
      return TC_FAILED;
    }
  }

  UNREACHABLE("should already have returned");
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
//   th - heap to use for allocations.
//   scope - the list of variables in scope.
//   expr - the expression to compile.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
// * Prints a message to stderr if the expression fails to compile.
// * Allocates an FbleType that must be freed using FbleReleaseType when it is
//   no longer needed.
static FbleType* TypeCheckExprForType(FbleTypeHeap* th, Scope* scope, FbleExpr* expr)
{
  Scope nscope;
  InitScope(&nscope, NULL, scope->module, scope);

  Tc result = TypeCheckExpr(th, &nscope, expr);
  FreeScope(th, &nscope);
  FbleFreeTc(result.tc);
  return result.type;
}

// TypeCheckType --
//   Type check a type, returning its value.
//
// Inputs:
//   th - heap to use for allocations.
//   scope - the value of variables in scope.
//   type - the type to compile.
//
// Results:
//   The type checked and evaluated type, or NULL in case of error.
//
// Side effects:
// * Prints a message to stderr if the type fails to compile or evalute.
// * Allocates an FbleType that must be freed using FbleReleaseType when it is
//   no longer needed.
static FbleType* TypeCheckType(FbleTypeHeap* th, Scope* scope, FbleTypeExpr* type)
{
  Cleaner* cleaner = NewCleaner();
  FbleType* result = TypeCheckTypeWithCleaner(th, scope, type, cleaner);
  Cleanup(th, cleaner);
  return result;
}
 
// TypeCheckTypeWithCleaner --
//   Same as TypeCheckType, except with a cleaner provided for convenience.
//
// See documentation of TypeCheckType.
//
// Objects added to the cleaner will be automatically cleaned up by the caller
// when this function returns.
static FbleType* TypeCheckTypeWithCleaner(FbleTypeHeap* th, Scope* scope, FbleTypeExpr* type, Cleaner* cleaner)
{
  switch (type->tag) {
    case FBLE_TYPEOF_EXPR: {
      FbleTypeofExpr* typeof = (FbleTypeofExpr*)type;
      FbleType* result = TypeCheckExprForType(th, scope, typeof->expr);
      return result;
    }

    case FBLE_DATA_TYPE_EXPR: {
      FbleDataTypeExpr* data_type = (FbleDataTypeExpr*)type;

      FbleDataType* dt = FbleNewType(th, FbleDataType, FBLE_DATA_TYPE, type->loc);
      dt->datatype = data_type->datatype;
      FbleVectorInit(dt->fields);
      CleanType(cleaner, &dt->_base);

      for (size_t i = 0; i < data_type->fields.size; ++i) {
        FbleTaggedTypeExpr* field = data_type->fields.xs + i;
        FbleType* compiled = TypeCheckType(th, scope, field->type);
        CleanType(cleaner, compiled);
        if (compiled == NULL) {
          return NULL;
        }

        if (!CheckNameSpace(field->name, compiled)) {
          return NULL;
        }

        FbleTaggedType cfield = {
          .name = FbleCopyName(field->name),
          .type = compiled
        };
        FbleVectorAppend(dt->fields, cfield);
        FbleTypeAddRef(th, &dt->_base, cfield.type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name, data_type->fields.xs[j].name)) {
            ReportError(field->name.loc,
                "duplicate field name '%n'\n",
                field->name);
            return NULL;
          }
        }
      }
      return FbleRetainType(th, &dt->_base);
    }

    case FBLE_FUNC_TYPE_EXPR: {
      FbleFuncType* ft = FbleNewType(th, FbleFuncType, FBLE_FUNC_TYPE, type->loc);
      CleanType(cleaner, &ft->_base);

      FbleFuncTypeExpr* func_type = (FbleFuncTypeExpr*)type;

      FbleVectorInit(ft->args);
      ft->rtype = NULL;

      bool error = false;
      for (size_t i = 0; i < func_type->args.size; ++i) {
        FbleType* arg = TypeCheckType(th, scope, func_type->args.xs[i]);
        if (arg == NULL) {
          error = true;
        } else {
          FbleVectorAppend(ft->args, arg);
          FbleTypeAddRef(th, &ft->_base, arg);
        }
        FbleReleaseType(th, arg);
      }

      FbleType* rtype = TypeCheckType(th, scope, func_type->rtype);
      CleanType(cleaner, rtype);
      if (error || rtype == NULL) {
        return NULL;
      }
      ft->rtype = rtype;
      FbleTypeAddRef(th, &ft->_base, ft->rtype);
      return FbleRetainType(th, &ft->_base);
    }

    case FBLE_PACKAGE_TYPE_EXPR: {
      FblePackageTypeExpr* e = (FblePackageTypeExpr*)type;
      FblePackageType* t = FbleNewType(th, FblePackageType, FBLE_PACKAGE_TYPE, type->loc);
      t->path = FbleCopyModulePath(e->path);
      t->opaque = true;
      return &t->_base;
    }

    case FBLE_VAR_EXPR:
    case FBLE_LET_EXPR:
    case FBLE_DATA_ACCESS_EXPR:
    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR:
    case FBLE_UNION_VALUE_EXPR:
    case FBLE_UNION_SELECT_EXPR:
    case FBLE_FUNC_VALUE_EXPR:
    case FBLE_POLY_VALUE_EXPR:
    case FBLE_POLY_APPLY_EXPR:
    case FBLE_LIST_EXPR:
    case FBLE_LITERAL_EXPR:
    case FBLE_MODULE_PATH_EXPR:
    case FBLE_ABSTRACT_CAST_EXPR:
    case FBLE_ABSTRACT_ACCESS_EXPR:
    case FBLE_MISC_APPLY_EXPR:
    {
      FbleExpr* expr = type;
      FbleType* type = TypeCheckExprForType(th, scope, expr);
      CleanType(cleaner, type);
      if (type == NULL) {
        return NULL;
      }

      FbleType* type_value = FbleValueOfType(th, type);
      if (type_value == NULL) {
        ReportError(expr->loc, "expected a type, but found value of type %t\n", type);
        return NULL;
      }
      return type_value;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// TypeCheckModule --
//   Type check a module.
//
// Inputs:
//   th - heap to use for allocations.
//   module - the module to check.
//   deps - the type of each module this module depends on, in the same order
//          as module->deps. size is module->deps.size.
//
// Results:
//   Returns the type, and optionally value of the module as the body of a
//   function that takes module dependencies as arguments and computes the
//   value of the module. TC_FAILED if the module failed to type check.
//
//   If module->value is not provided but module->type still type checks, this
//   will return a Tc with non-NULL type but NULL tc.
//
// Side effects:
// * Prints warning messages to stderr.
// * Prints a message to stderr if the module fails to type check.
// * The caller should call FbleFreeTc when the returned result is no longer
//   needed and FbleReleaseType when the returned FbleType is no longer
//   needed.
static Tc TypeCheckModule(FbleTypeHeap* th, FbleLoadedModule* module, FbleType** deps)
{
  Scope scope;
  InitScope(&scope, NULL, module->path, NULL);

  for (int i = 0; i < module->deps.size; ++i) {
    VarName name = { .module = module->deps.xs[i] };
    PushVar(&scope, name, FbleRetainType(th, deps[i]));
  }

  assert(module->type || module->value);

  FbleType* type = NULL;
  if (module->type) {
    type = TypeCheckType(th, &scope, module->value);
    if (type == NULL) {
      FreeScope(th, &scope);
      return TC_FAILED;
    }
  }

  Tc tc = TC_FAILED;
  if (module->value) {
    tc = TypeCheckExpr(th, &scope, module->value);
    if (tc.type == NULL) {
      FreeScope(th, &scope);
      FbleReleaseType(th, type);
      return TC_FAILED;
    }
  }

  FreeScope(th, &scope);

  if (type && tc.type) {
    if (!FbleTypesEqual(th, type, tc.type)) {
      ReportError(tc.type->loc, "the type %t does not match interface type %t for module ",
          tc.type, type);
      FblePrintModulePath(stderr, module->path);
      fprintf(stderr, "\n");
      FbleReleaseType(th, type);
      FreeTc(th, tc);
      return TC_FAILED;
    }
    FbleReleaseType(th, type);
    return tc;
  }

  if (type) {
    tc.type = type;
    return tc;
  }

  return tc;
}

// FbleTypeCheckModule -- see documentation in typecheck.h
FbleTc* FbleTypeCheckModule(FbleLoadedProgram* program)
{
  FbleTc** tcs = FbleTypeCheckProgram(program);
  if (tcs == NULL) {
    return NULL;
  }

  FbleTc* tc = tcs[program->modules.size - 1];
  for (size_t i = 0; i < program->modules.size-1; ++i) {
    FbleFreeTc(tcs[i]);
  }
  FbleFree(tcs);
  return tc;
}

// FbleTypeCheckProgram -- see documentation in typecheck.h
FbleTc** FbleTypeCheckProgram(FbleLoadedProgram* program)
{
  FbleTc** tcs = FbleArrayAlloc(FbleTc*, program->modules.size);

  bool error = false;
  FbleTypeHeap* th = FbleNewTypeHeap();
  FbleType* types[program->modules.size];

  for (int i = 0; i < program->modules.size; ++i) {
    FbleLoadedModule* module = program->modules.xs + i;
    FbleType* deps[module->deps.size];

    bool skip = false;
    for (int d = 0; d < module->deps.size; ++d) {
      deps[d] = NULL;
      for (int t = 0; t < i; ++t) {
        if (FbleModulePathsEqual(module->deps.xs[d], program->modules.xs[t].path)) {
          deps[d] = types[t];
          break;
        }
      }

      if (deps[d] == NULL) {
        skip = true;
        break;
      }
    }

    Tc tc = skip ? TC_FAILED : TypeCheckModule(th, module, deps);
    if (tc.type == NULL) {
      error = true;
      types[i] = NULL;
      tcs[i] = NULL;
    } else {
      types[i] = tc.type;
      tcs[i] = tc.tc;
    }
  }

  for (int i = 0; i < program->modules.size; ++i) {
    FbleReleaseType(th, types[i]);
  }
  FbleFreeTypeHeap(th);

  if (error) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleFreeTc(tcs[i]);
    }
    FbleFree(tcs);
    return NULL;
  }

  return tcs;
}
