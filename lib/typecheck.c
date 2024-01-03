/**
 * @file typecheck.c
 *  Converts FbleExpr untyped abstract syntax to FbleTc typed abstract syntax.
 */

#include "typecheck.h"

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_arg, va_end
#include <stdio.h>    // for fprintf, stderr
#include <string.h>   // for strlen, strcat
#include <stdlib.h>   // for NULL

#include <fble/fble-vector.h>

#include "kind.h"
#include "expr.h"
#include "tc.h"
#include "type.h"
#include "unreachable.h"
#include "unused.h"

/**
 * Name of a variable.
 *
 * Variables can refer to normal values or module values.
 *
 * module == NULL means this is a normal value with name in 'normal'.
 * module != NULL means this is a module value with path in 'module'.
 */
typedef struct {
  FbleName normal;            /**< Name for normal variables. */
  FbleModulePath* module;     /**< Module for module variables. */
} VarName;

/** Info about an argument. */
typedef struct {
  VarName name;       /**< The name of the argument. */
  FbleType* type;     /**< The type of the argument. */
} Arg;

/** Vector of Args. */
typedef struct {
  size_t size;      /**< Number of elements. */
  Arg* xs;          /**< The elements. */
} ArgV;

/**
 * Info about a variable visible during type checking.
 *
 * A variable that is captured from one scope to another will have a separate
 * instance of Var for each scope that it is captured in.
 */
typedef struct {
  VarName name;     /**< The name of the variable. */

  /**
   * The type of the variable.
   * A reference to the type is owned by this Var.
   */
  FbleType* type;         

  bool used;        /**< True if the variable is used anywhere at runtime. */
  FbleVar var;      /**< The index of the variable. */
} Var;

/** Vector of Var. */
typedef struct {
  size_t size;    /**< Number of elements. */
  Var** xs;       /**< The elements. */
} VarV;

// Special value for FbleVarTag used during typechecking to indicate a new
// type value should be used instead of reading the variable.
#define TYPE_VAR 4

/**
 * Scope of variables visible during type checking.
 */
typedef struct Scope {
  /** Variables captured from the parent scope. Scope owns the Vars. */
  VarV statics;

  /** List of args to the current scope. Scope owns the Vars. */
  VarV args;

  /**
   * Stack of local variables in scope order.
   * Variables may be NULL to indicate they are anonymous.
   * The locals->var.tag may be TYPE_VAR to indicate the caller should create
   * a type value instead of trying to read the var value from locals.
   * Scope owns the Vars.
   */
  VarV locals;

  /**
   * The number of allocated locals.
   * This may be different from locals.size because some of the locals.xs can
   * be TYPE_VAR which are unallocated.
   */
  size_t allocated_locals;

  /**
   * Collects the source of variables captured from the parent scope.
   * May be NULL to indicate that operations on this scope should not have any
   * side effects on the parent scope.
   */
  FbleVarV* captured;

  FbleModulePath* module;   /**< The current module being compiled. */
  struct Scope* parent;     /**< The parent of this scope. May be NULL. */
} Scope;

static bool VarNamesEqual(VarName a, VarName b);
static Var* PushLocalVar(Scope* scope, VarName name, FbleType* type);
static Var* PushLocalTypeVar(Scope* scope, VarName name, FbleType* type);
static void PopLocalVar(FbleTypeHeap* heap, Scope* scope);
static Var* GetVar(FbleTypeHeap* heap, Scope* scope, VarName name, bool phantom);

static void InitScope(Scope* scope, FbleVarV* captured, ArgV args, FbleModulePath* module, Scope* parent);
static void FreeScope(FbleTypeHeap* heap, Scope* scope);

static void ReportError(FbleLoc loc, const char* fmt, ...);
static bool CheckNameSpace(FbleName name, FbleType* type);

/** Pair of returned type and type checked expression. */
typedef struct {
  FbleType* type;   /**< The type of the expression. */
  FbleTc* tc;       /**< The type checked expression. */
} Tc;

/** Vector of Tc. */
typedef struct {
  size_t size;    /**< Number of elements. */
  Tc* xs;         /**< The elements. */
} TcV;

/** Tc returned to indicate that type check has failed. */
static Tc TC_FAILED = { .type = NULL, .tc = NULL };

static Tc MkTc(FbleType* type, FbleTc* tc);
static void FreeTc(FbleTypeHeap* th, Tc tc);

/**
 * Tracks values for automatic cleanup.
 */
typedef struct {
  FbleTypeV types;      /**< List of types to clean up. */
  FbleTcV tcs;          /**< List of tcs to clean up. */
  struct {
    size_t size;                /**< Number of elements. */
    FbleTypeAssignmentV** xs;   /**< The elements. */
  } tyvars;             /**< List of type variables to clean up. */
  FbleTcBindingV bindings;      /**< List of bindings to clean up. */
} Cleaner;

static FbleTcBinding CopyTcBinding(FbleTcBinding binding);

static Cleaner* NewCleaner();
static void CleanType(Cleaner* cleaner, FbleType* type);
static void CleanTc(Cleaner* cleaner, Tc tc);
static void CleanTypeAssignmentV(Cleaner* cleaner, FbleTypeAssignmentV* vars);
static void CleanTcBinding(Cleaner* cleaner, FbleTcBinding vars);
static void Cleanup(FbleTypeHeap* th, Cleaner* cleaner);

static FbleType* DepolyType(FbleTypeHeap* th, FbleType* type, FbleTypeAssignmentV* vars);
static Tc PolyApply(FbleTypeHeap* th, Tc poly, FbleType* arg_type, FbleLoc expr_loc, FbleLoc arg_loc);
static Tc TypeInferArgs(FbleTypeHeap* th, FbleTypeAssignmentV vars, FbleTypeV expected, TcV actual, Tc poly);

static Tc TypeCheckExpr(FbleTypeHeap* th, Scope* scope, FbleExpr* expr);
static Tc TypeCheckExprWithCleaner(FbleTypeHeap* th, Scope* scope, FbleExpr* expr, Cleaner* cleaner);
static FbleType* TypeCheckType(FbleTypeHeap* th, Scope* scope, FbleTypeExpr* type);
static FbleType* TypeCheckTypeWithCleaner(FbleTypeHeap* th, Scope* scope, FbleTypeExpr* type, Cleaner* cleaner);
static FbleType* TypeCheckExprForType(FbleTypeHeap* th, Scope* scope, FbleExpr* expr);
static Tc TypeCheckModule(FbleTypeHeap* th, FbleLoadedModule* module, FbleType** deps);


/**
 * Tests whether two variable names are equal.
 *
 * @param a  The first variable name.
 * @param b  The second variable name.
 *
 * @returns
 *   true if the names are equal, false otherwise.
 *
 * @sideeffects
 *   None.
 */
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

/**
 * Pushes a local variable onto the current scope.
 *
 * @param scope  The scope to push the variable on to.
 * @param name  The name of the variable.
 * @param type  The type of the variable.
 *
 * @returns
 *   A pointer to the newly pushed variable. The pointer is owned by the
 *   scope. It remains valid until a corresponding PopLocalVar or FreeScope
 *   occurs.
 *
 * @sideeffects
 * * Pushes a new variable with given name and type onto the scope.
 * * Takes ownership of the given type, which will be released when the
 *   variable is freed.
 * * Does not take ownership of name. It is the callers responsibility
 *   to ensure that 'name' outlives the returned Var.
 */
static Var* PushLocalVar(Scope* scope, VarName name, FbleType* type)
{
  Var* var = FbleAlloc(Var);
  var->name = name;
  var->type = type;
  var->used = false;
  var->var.tag = FBLE_LOCAL_VAR;
  var->var.index = scope->allocated_locals++;
  FbleAppendToVector(scope->locals, var);
  return var;
}

/**
 * Pushes a local type variable onto the current scope.
 * 
 * Type variables do not have a corresponding definition. Anyone who
 * references a type variable will allocate a new type value instead.
 *
 * @param scope  The scope to push the variable on to.
 * @param name  The name of the variable.
 * @param type  The type of the variable.
 *
 * @returns
 *   A pointer to the newly pushed variable. The pointer is owned by the
 *   scope. It remains valid until a corresponding PopLocalVar or FreeScope
 *   occurs.
 *
 * @sideeffects
 * * Pushes a new variable with given name and type onto the scope.
 * * Takes ownership of the given type, which will be released when the
 *   variable is freed.
 * * Does not take ownership of name. It is the callers responsibility
 *   to ensure that 'name' outlives the returned Var.
 */
static Var* PushLocalTypeVar(Scope* scope, VarName name, FbleType* type)
{
  Var* var = FbleAlloc(Var);
  var->name = name;
  var->type = type;
  var->used = false;
  var->var.tag = TYPE_VAR;
  var->var.index = -1;
  FbleAppendToVector(scope->locals, var);
  return var;
}

/**
 * Pops a local var off the given scope.
 *
 * @param heap  Heap to use for allocations.
 * @param scope  The scope to pop from.
 *
 * @sideeffects
 *   Pops the top var off the scope. Invalidates the pointer to the variable
 *   originally returned in PushLocalVar.
 */
static void PopLocalVar(FbleTypeHeap* heap, Scope* scope)
{
  scope->locals.size--;
  Var* var = scope->locals.xs[scope->locals.size];
  if (var != NULL) {
    if (var->var.tag != TYPE_VAR) {
      scope->allocated_locals--;
    }

    FbleReleaseType(heap, var->type);
    FbleFree(var);
  }
}

/**
 * Looks up a var in the given scope.
 *
 * @param heap  Heap to use for allocations.
 * @param scope  The scope to look in.
 * @param name  The name of the variable.
 * @param phantom  If true, do not consider the variable to be accessed.
 *
 * @returns
 *   The variable from the scope, or NULL if no such variable was found. The
 *   variable is owned by the scope and remains valid until either PopLocalVar
 *   is called or the scope is finished.
 *
 * @sideeffects
 *   Marks variable as used and for capture if necessary and not phantom.
 */
static Var* GetVar(FbleTypeHeap* heap, Scope* scope, VarName name, bool phantom)
{
  for (size_t i = 0; i < scope->locals.size; ++i) {
    size_t j = scope->locals.size - i - 1;
    Var* var = scope->locals.xs[j];
    if (var != NULL && VarNamesEqual(name, var->name)) {
      if (!phantom && var->var.tag != TYPE_VAR) {
        var->used = true;
      }
      return var;
    }
  }

  for (size_t i = 0; i < scope->args.size; ++i) {
    size_t j = scope->args.size - i - 1;
    Var* var = scope->args.xs[j];
    if (var != NULL && VarNamesEqual(name, var->name)) {
      if (!phantom) {
        var->used = true;
      }
      return var;
    }
  }

  for (size_t i = 0; i < scope->statics.size; ++i) {
    Var* var = scope->statics.xs[i];
    if (var != NULL && VarNamesEqual(name, var->name)) {
      if (!phantom) {
        var->used = true;
      }
      return var;
    }
  }

  if (scope->parent != NULL) {
    Var* var = GetVar(heap, scope->parent, name, scope->captured == NULL || phantom);
    if (var != NULL) {
      if (phantom || var->var.tag == TYPE_VAR) {
        // It doesn't matter that we are returning a variable for the wrong
        // scope here. Phantom/TYPE_VAR means we won't actually use it ever.
        return var;
      }

      Var* captured_var = FbleAlloc(Var);
      captured_var->name = var->name;
      captured_var->type = FbleRetainType(heap, var->type);
      captured_var->used = !phantom;
      captured_var->var.tag = FBLE_STATIC_VAR;
      captured_var->var.index = scope->statics.size;
      FbleAppendToVector(scope->statics, captured_var);
      if (scope->captured != NULL) {
        FbleAppendToVector(*scope->captured, var->var);
      }
      return captured_var;
    }
  }

  return NULL;
}

/**
 * Initialize a new scope.
 *
 * @param scope  The scope to initialize.
 * @param captured  Collects the source of variables captured from the parent
 *   scope. May be NULL to indicate that operations on this scope should not
 *   have any side effects on the parent scope.
 * @param args  Args to the scope.
 * @param module  The current module. Borrowed.
 * @param parent  The parent of the scope to initialize. May be NULL.
 *
 * @sideeffects
 * * Initializes scope based on parent.
 * * FreeScope should be called to free the allocations for scope.
 * * The lifetime of the parent scope must exceed the lifetime of this scope.
 * * Takes ownership of the args types, which will be released when the
 *   scope is freed.
 * * Does not take ownership of arg names. It is the callers responsibility to
 *   ensure that arg names outlive the scope.
 */
static void InitScope(Scope* scope, FbleVarV* captured, ArgV args, FbleModulePath* module, Scope* parent)
{
  FbleInitVector(scope->statics);
  FbleInitVector(scope->args);
  FbleInitVector(scope->locals);
  scope->allocated_locals = 0;
  scope->captured = captured;
  scope->module = FbleCopyModulePath(module);
  scope->parent = parent;

  for (size_t i = 0; i < args.size; ++i) {
    Var* var = FbleAlloc(Var);
    var->name = args.xs[i].name;
    var->type = args.xs[i].type;
    var->used = false;
    var->var.tag = FBLE_ARG_VAR;
    var->var.index = scope->args.size;
    FbleAppendToVector(scope->args, var);
  }
}

/**
 * Frees memory associated with a Scope.
 *
 * @param heap  Heap to use for allocations
 * @param scope  The scope to finish.
 *
 * @sideeffects
 * * Frees memory associated with scope.
 */
static void FreeScope(FbleTypeHeap* heap, Scope* scope)
{
  for (size_t i = 0; i < scope->statics.size; ++i) {
    FbleReleaseType(heap, scope->statics.xs[i]->type);
    FbleFree(scope->statics.xs[i]);
  }
  FbleFreeVector(scope->statics);

  for (size_t i = 0; i < scope->args.size; ++i) {
    FbleReleaseType(heap, scope->args.xs[i]->type);
    FbleFree(scope->args.xs[i]);
  }
  FbleFreeVector(scope->args);

  while (scope->locals.size > 0) {
    PopLocalVar(heap, scope);
  }
  FbleFreeVector(scope->locals);
  FbleFreeModulePath(scope->module);
}

/**
 * Reports a compiler error.
 *
 * This uses a printf-like format string. The following format specifiers
 * are supported:
 *   %i - size_t
 *   %k - FbleKind*
 *   %n - FbleName
 *   %s - const char*
 *   %t - FbleType*
 *   %m - FbleModulePath*
 *   %% - literal '%'
 * Please add additional format specifiers as needed.
 *
 * @param loc  The location of the error.
 * @param fmt  The format string.
 * @param ...  The var-args for the associated conversion specifiers in fmt.
 *
 * @sideeffects
 *   Prints a message to stderr as described by fmt and provided arguments.
 */
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
        FbleUnreachable("Unsupported format conversion.");
        break;
      }
    }
    fmt = p + 2;
  }
  fprintf(stderr, "%s", fmt);
  va_end(ap);
}

/**
 * Checks that the right namespace is used for a variable.
 *
 * Normal variables should use the normal name space. Type variables should
 * use the type namespace.
 *
 * @param name  The name in question
 * @param type  The type of the value refered to by the name.
 *
 * @returns
 *   true if the namespace of the name is consistent with the type. false
 *   otherwise.
 *
 * @sideeffects
 *   Prints a message to stderr if the namespace and type don't match.
 */
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

/**
 * Constructs a tc pair.
 *
 * @param type  The type to put in the tc.
 * @param tc  The tc to put in the tc.
 *
 * @returns
 *   A Tc with type and tc as fields.
 *
 * @sideeffects
 *   None.
 */
static Tc MkTc(FbleType* type, FbleTc* tc)
{
  Tc x = { .type = type, .tc = tc };
  return x;
}

/**
 * Frees type and tc fields of a Tc.
 *
 * @param th  Heap to use for allocations
 * @param tc  Tc to free the fields of. May be TC_FAILED.
 *
 * @sideeffects
 *   Frees resources associated with the type and tc fields of tc.
 */
static void FreeTc(FbleTypeHeap* th, Tc tc)
{
  FbleReleaseType(th, tc.type);
  FbleFreeTc(tc.tc);
}

/**
 * Copies a tc binding.
 *
 * @param binding  The binding to copy.
 * @returns
 *  The copied binding.
 * @sideeffects
 *  The user should free name, loc, and tc of the copied binding when no
 *  longer needed.
 */
static FbleTcBinding CopyTcBinding(FbleTcBinding binding)
{
  FbleCopyName(binding.name);
  FbleCopyLoc(binding.loc);
  FbleCopyTc(binding.tc);
  return binding;
}

/**
 * Allocates and returns a new cleaner object.
 *
 * @returns
 *   A newly allocated cleaner object.
 *
 * @sideeffects
 *   The user should call a WithCleanup function to free resources associated
 *   with the cleaner object when no longer needed.
 */
static Cleaner* NewCleaner()
{
  Cleaner* cleaner = FbleAlloc(Cleaner);
  FbleInitVector(cleaner->types);
  FbleInitVector(cleaner->tcs);
  FbleInitVector(cleaner->tyvars);
  FbleInitVector(cleaner->bindings);
  return cleaner;
}

/**
 * Adds an FbleType for automatic cleanup.
 *
 * @param cleaner  The cleaner to add the type to.
 * @param type  The type to track.
 *
 * @sideeffects
 *   The type will be automatically released when the cleaner is cleaned up.
 */
static void CleanType(Cleaner* cleaner, FbleType* type)
{
  FbleAppendToVector(cleaner->types, type);
}

/**
 * Adds a Tc for automatic cleanup.
 *
 * @param cleaner  The cleaner to add the type to.
 * @param tc  The tc to track.
 *
 * @sideeffects
 *   The tc will be automatically released when the cleaner is cleaned up.
 */
static void CleanTc(Cleaner* cleaner, Tc tc)
{
  FbleAppendToVector(cleaner->types, tc.type);
  FbleAppendToVector(cleaner->tcs, tc.tc);
}

/**
 * Adds an FbleTypeAssignmentV for automatic cleanup.
 *
 * @param cleaner  The cleaner to add the assignment to.
 * @param vars  The vars to track. Should be allocated with FbleAlloc.
 *
 * @sideeffects
 *   The vars will be automatically released when the cleaner is cleaned up.
 */
static void CleanTypeAssignmentV(Cleaner* cleaner, FbleTypeAssignmentV* vars)
{
  FbleAppendToVector(cleaner->tyvars, vars);
}

/**
 * Adds an FbleTcBinding for automatic cleanup.
 *
 * @param cleaner  The cleaner to add binding to.
 * @param binding  The binding to track.
 *
 * @sideeffects
 *  The binding will be automatically freed when the cleaner is cleaned up.
 */
static void CleanTcBinding(Cleaner* cleaner, FbleTcBinding binding)
{
  FbleAppendToVector(cleaner->bindings, binding);
}

/**
 * Cleans up objects marked for automatic cleanup.
 *
 * @param th  The type heap used for cleanup.
 * @param cleaner  The cleaner object to trigger cleanup on.
 *
 * @sideeffects
 *   Cleans up all objects tracked by the cleaner along with the cleaner
 *   object itself.
 */
static void Cleanup(FbleTypeHeap* th, Cleaner* cleaner)
{
  for (size_t i = 0; i < cleaner->types.size; ++i) {
    FbleReleaseType(th, cleaner->types.xs[i]);
  }
  FbleFreeVector(cleaner->types);

  for (size_t i = 0; i < cleaner->tcs.size; ++i) {
    FbleFreeTc(cleaner->tcs.xs[i]);
  }
  FbleFreeVector(cleaner->tcs);

  for (size_t i = 0; i < cleaner->tyvars.size; ++i) {
    FbleTypeAssignmentV* vars = cleaner->tyvars.xs[i];
    for (size_t j = 0; j < vars->size; ++j) {
      FbleReleaseType(th, vars->xs[j].value);
    }
    FbleFreeVector(*vars);
    FbleFree(vars);
  }
  FbleFreeVector(cleaner->tyvars);

  for (size_t i = 0; i < cleaner->bindings.size; ++i) {
    FbleTcBinding binding = cleaner->bindings.xs[i];
    FbleFreeName(binding.name);
    FbleFreeLoc(binding.loc);
    FbleFreeTc(binding.tc);
  }
  FbleFreeVector(cleaner->bindings);
  FbleFree(cleaner);
}

/**
 * Normalizes the given type.
 *
 * Normalizes and removes any layers of polymorphic type variables from the
 * given type.
 *
 * @param th  The type heap
 * @param type  The type to normalize and unwrap poly type vars from.
 * @param vars  Output variable to populate unwrapped type variables to.
 *
 * @returns
 *   The normalized and depolyed type.
 *
 * @sideeffects
 * * Adds unwrapped type variables to vars.
 * * The caller should call FbleReleaseType on the returned type when no
 *   longer needed.
 */
static FbleType* DepolyType(FbleTypeHeap* th, FbleType* type, FbleTypeAssignmentV* vars)
{
  FbleType* pbody = FbleNormalType(th, type);
  while (pbody->tag == FBLE_POLY_TYPE) {
    FblePolyType* poly = (FblePolyType*)pbody;
    FbleTypeAssignment var = { .var = poly->arg, .value = NULL };
    FbleAppendToVector(*vars, var);

    FbleType* next = FbleNormalType(th, poly->body);
    FbleReleaseType(th, pbody);
    pbody = next;
  }
  return pbody;
}

/**
 * Typechecks poly application.
 *
 * @param th  The type heap
 * @param poly  The type and value of the poly. Borrowed.
 * @param arg_type  The type of the arg type to apply the poly to. May be NULL. Borrowed.
 * @param expr_loc  The location of the application expression.
 * @param arg_loc  The location of the argument type.
 *
 * @returns
 *   The Tc for the application of the poly to the argument, or TC_FAILED in
 *   case of error.
 *
 * @sideeffects
 * * Reports an error in case of error.
 * * The caller should call FbleFreeTc when the returned FbleTc is no longer
 *   needed and FbleReleaseType when the returned FbleType is no longer
 *   needed.
 */
static Tc PolyApply(FbleTypeHeap* th, Tc poly, FbleType* arg_type, FbleLoc expr_loc, FbleLoc arg_loc)
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

/**
 * Infers and checks argument types.
 *
 * Common code to infer type variables of and check types of arguments to a
 * potential polymorphic typed object.
 *
 * @param th  The type heap
 * @param vars  The type variables. Borrowed.
 * @param expected  List of expected types to be passed to the poly. Borrowed.
 * @param actual  List of actual arguments to the poly. Borrowed.
 * @param poly  The poly. Borrowed.
 *
 * @returns
 *   The result of applying the poly to the inferred type variables, or
 *   TC_FAILED in case of type error.
 *
 * @sideeffects
 * * Prints error message in case of failure to type check.
 * * Populates values of vars based on type inference.
 * * Allocates a Tc that should be freed with FreeTc when no longer needed.
 */
static Tc TypeInferArgs(FbleTypeHeap* th, FbleTypeAssignmentV vars, FbleTypeV expected, TcV actual, Tc poly)
{
  if (poly.type == NULL) {
    return TC_FAILED;
  }

  FbleLoc loc = poly.tc->loc;

  if (expected.size != actual.size) {
    ReportError(loc, "expected %i args, but found %i\n", expected.size, actual.size);
    return TC_FAILED;
  }
  
  // Check function arg types match what is expected.
  // Infer values for poly type variables as we go.
  bool error = false;
  for (size_t i = 0; i < expected.size; ++i) {
    if (!FbleTypeInfer(th, vars, expected.xs[i], actual.xs[i].type)) {
      ReportError(actual.xs[i].tc->loc, "expected type %t, but found %t\n",
          expected.xs[i], actual.xs[i].type);
      error = true;
    }
  }

  // Apply type variables to the poly.
  // This checks that the inferred type variables have the correct kind.
  Tc result = { .type = FbleRetainType(th, poly.type), .tc = FbleCopyTc(poly.tc) };
  for (size_t i = 0; !error && i < vars.size; ++i) {
    if (vars.xs[i].value == NULL) {
      ReportError(loc, "unable to infer types for poly.\n");
      error = true;
      break;
    }

    FbleTypeType* arg_type = FbleNewType(th, FbleTypeType, FBLE_TYPE_TYPE, loc);
    arg_type->type = vars.xs[i].value;
    FbleTypeAddRef(th, &arg_type->_base, arg_type->type);
    FbleReleaseType(th, &arg_type->_base);

    Tc prev = result;
    result = PolyApply(th, result, &arg_type->_base, loc, loc);
    FreeTc(th, prev);
    if (result.type == NULL) {
      error = true;
    }
  }

  if (error) {
    // An error message should already have been output for the error.
    // Add info about the inferred type variables.
    if (vars.size > 0) {
      fprintf(stderr, "Inferred types:\n");
      for (size_t i = 0; i < vars.size; ++i) {
        fprintf(stderr, "  ");
        FblePrintType(vars.xs[i].var);
        fprintf(stderr, ": ");
        if (vars.xs[i].value == NULL) {
          fprintf(stderr, "???");
        } else {
          FblePrintType(vars.xs[i].value);
        }
        fprintf(stderr, "\n");
      }
    }
    FreeTc(th, result);
    return TC_FAILED;
  }

  return result;
}

/**
 * Typechecks an expression.
 *
 * @param th  Heap to use for type allocations.
 * @param scope  The list of variables in scope.
 * @param expr  The expression to type check.
 *
 * @returns
 *   The type checked expression, or TC_FAILED if the expression is not well
 *   typed.
 *
 * @sideeffects
 * * Prints a message to stderr if the expression fails to compile.
 * * The caller should call FbleFreeTc when the returned FbleTc is no longer
 *   needed and FbleReleaseType when the returned FbleType is no longer
 *   needed.
 */
static Tc TypeCheckExpr(FbleTypeHeap* th, Scope* scope, FbleExpr* expr)
{
  Cleaner* cleaner = NewCleaner();
  Tc result = TypeCheckExprWithCleaner(th, scope, expr, cleaner);
  Cleanup(th, cleaner);
  return result;
}

/**
 * Type checks an expression with automatic cleanup.
 *
 * @param th  Heap to use for type allocations.
 * @param scope  The list of variables in scope.
 * @param expr  The expression to type check.
 * @param cleaner  The cleaner object.
 *
 * @returns
 *   The type checked expression, or TC_FAILED if the expression is not well
 *   typed.
 *
 * @sideeffects
 * * Prints a message to stderr if the expression fails to compile.
 * * The caller should call FbleFreeTc when the returned FbleTc is no longer
 *   needed and FbleReleaseType when the returned FbleType is no longer
 *   needed.
 * * Adds objects to the cleaner that the caller is responsible for
 *   automatically cleaning up when this function returns.
 */
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

      FbleTc* tc = NULL;
      if (var->var.tag == TYPE_VAR) {
        FbleTypeValueTc* type_tc = FbleNewTc(FbleTypeValueTc, FBLE_TYPE_VALUE_TC, expr->loc);
        tc = &type_tc->_base;
      } else {
        FbleVarTc* var_tc = FbleNewTc(FbleVarTc, FBLE_VAR_TC, expr->loc);
        var_tc->var = var->var;
        tc = &var_tc->_base;
      }
      return MkTc(FbleRetainType(th, var->type), tc);
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
          size_t kind_level = FbleGetKindLevel(binding->kind);
          if (kind_level == 0) {
            strcat(renamed, "__");
          } 
          strcat(renamed, binding->name.name->str);

          FbleName type_name = {
            .name = FbleNewString(renamed),
            .space = FBLE_TYPE_NAME_SPACE,
            .loc = binding->name.loc,
          };

          FbleKind* kind = FbleNewBasicKind(binding->kind->loc, kind_level);
          types[i] = FbleNewVarType(th, binding->name.loc, kind, type_name);
          FbleFreeKind(kind);
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
        vars[i] = PushLocalVar(scope, name, types[i]);
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
          ReportError(types[i]->loc, "(%t from here)\n", types[i]);
          ReportError(defs[i].type->loc, "(%t from here)\n", defs[i].type);
        } else if (!error && binding->type == NULL) {
          FbleKind* expected_kind = FbleCopyKind(binding->kind);
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

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        PopLocalVar(th, scope);
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
      FbleInitVector(let_tc->bindings);
      let_tc->body = body.tc;
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleTcBinding ltc = {
          .name = FbleCopyName(let_expr->bindings.xs[i].name),
          .loc = FbleCopyLoc(let_expr->bindings.xs[i].expr->loc),
          .tc = defs[i].tc
        };
        FbleAppendToVector(let_tc->bindings, ltc);
      }
        
      return MkTc(body.type, &let_tc->_base);
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      FbleStructValueImplicitTypeExpr* struct_expr = (FbleStructValueImplicitTypeExpr*)expr;

      FbleDataType* struct_type = FbleNewType(th, FbleDataType, FBLE_DATA_TYPE, expr->loc);
      struct_type->datatype = FBLE_STRUCT_DATATYPE;
      FbleInitVector(struct_type->fields);
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
          FbleAppendToVector(struct_type->fields, cfield);
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

      FbleStructValueTc* struct_tc = FbleNewTc(FbleStructValueTc, FBLE_STRUCT_VALUE_TC, expr->loc);
      FbleInitVector(struct_tc->fields);
      for (size_t i = 0; i < argc; ++i) {
        FbleAppendToVector(struct_tc->fields, FbleCopyTc(args[i].tc));
      }

      return MkTc(FbleRetainType(th, &struct_type->_base), &struct_tc->_base);
    }

    case FBLE_STRUCT_COPY_EXPR: {
      FbleStructCopyExpr* struct_expr = (FbleStructCopyExpr*)expr;

      Tc src = TypeCheckExpr(th, scope, struct_expr->src);
      CleanTc(cleaner, src);
      if (src.type == NULL) {
        return TC_FAILED;
      }

      FbleDataType* struct_type = (FbleDataType*)FbleNormalType(th, src.type);
      CleanType(cleaner, &struct_type->_base);
      if (struct_type->_base.tag != FBLE_DATA_TYPE
          || struct_type->datatype != FBLE_STRUCT_DATATYPE) {
        ReportError(struct_expr->src->loc,
            "expected value of struct type, but found value of type %t\n",
            src.type);
        return TC_FAILED;
      }

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

      size_t fieldc = struct_type->fields.size;
      FbleStructCopyTc* struct_copy = FbleNewTc(FbleStructCopyTc, FBLE_STRUCT_COPY_TC, expr->loc);
      struct_copy->source = FbleCopyTc(src.tc);
      FbleInitVector(struct_copy->fields);

      size_t a = 0;
      for (size_t i = 0; i < fieldc; ++i) {
        FbleTaggedExpr* arg = struct_expr->args.xs + a;
        if (a < struct_expr->args.size && FbleNamesEqual(arg->name, struct_type->fields.xs[i].name)) {
          // Take the field value from the provided argument.
          FbleAppendToVector(struct_copy->fields, FbleCopyTc(args[a].tc));
          if (!FbleTypesEqual(th, struct_type->fields.xs[i].type, args[a].type)) {
            ReportError(args[a].tc->loc,
                "expected type %t, but found %t\n",
                struct_type->fields.xs[i].type, args[a].type);
            error = true;
          }
          a++;
        } else {
          // Take the field value from the source struct.
          FbleAppendToVector(struct_copy->fields, NULL);
        }
      }

      if (a < struct_expr->args.size) {
        ReportError(struct_expr->args.xs[a].name.loc,
          "expected next field in struct, but found '%n'\n",
          struct_expr->args.xs[a].name);
        error = true;
      }

      if (error) {
        FbleFreeTc(&struct_copy->_base);
        return TC_FAILED;
      }

      return MkTc(FbleRetainType(th, &struct_type->_base), &struct_copy->_base);
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      FbleType* type = TypeCheckType(th, scope, union_value_expr->type);
      CleanType(cleaner, type);
      if (type == NULL) {
        return TC_FAILED;
      }

      FbleTypeAssignmentV* vars = FbleAlloc(FbleTypeAssignmentV);
      FbleInitVector(*vars);
      CleanTypeAssignmentV(cleaner, vars);

      FbleDataType* union_type = (FbleDataType*)DepolyType(th, type, vars);
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

      FbleTypeV expected = { .size = 1, .xs = &field_type };
      TcV argv = { .size = 1, .xs = &arg };

      // Create a dummy tc value to pass location information to
      // TypeInferArgs.
      // TODO: Any cleaner way we can do this?
      FbleStructValueTc* unit_tc = FbleNewTc(FbleStructValueTc, FBLE_STRUCT_VALUE_TC, expr->loc);
      FbleInitVector(unit_tc->fields);
      Tc vtc = { .type = FbleRetainType(th, type), .tc = &unit_tc->_base };
      CleanTc(cleaner, vtc);

      Tc poly = TypeInferArgs(th, *vars, expected, argv, vtc);
      CleanTc(cleaner, poly);

      if (poly.type == NULL) {
        return TC_FAILED;
      }

      FbleUnionValueTc* union_tc = FbleNewTc(FbleUnionValueTc, FBLE_UNION_VALUE_TC, expr->loc);
      union_tc->tag = tag;
      union_tc->arg = FbleCopyTc(arg.tc);
      return MkTc(FbleRetainType(th, poly.type), &union_tc->_base);
    }

    case FBLE_UNION_SELECT_EXPR: {
      FbleUnionSelectExpr* select_expr = (FbleUnionSelectExpr*)expr;

      Tc condition = TypeCheckExpr(th, scope, select_expr->condition);
      CleanTc(cleaner, condition);
      if (condition.type == NULL) {
        return TC_FAILED;
      }

      FbleDataType* union_type = (FbleDataType*)FbleNormalType(th, condition.type);
      CleanType(cleaner, &union_type->_base);
      if (union_type->_base.tag != FBLE_DATA_TYPE
          || union_type->datatype != FBLE_UNION_DATATYPE) {
        ReportError(select_expr->condition->loc,
            "expected value of union type, but found value of type %t\n",
            condition.type);
        return TC_FAILED;
      }

      bool error = false;
      FbleType* result_type = NULL;
      FbleTcBinding default_ = { .tc = NULL };
      bool default_used = false;
      if (select_expr->default_ != NULL) {
        Tc result = TypeCheckExpr(th, scope, select_expr->default_);
        CleanTc(cleaner, result);
        error = error || (result.type == NULL);
        if (result.type != NULL) {
          FbleName label = {
            .name = FbleNewString(":"),
            .space = FBLE_NORMAL_NAME_SPACE,
            .loc = FbleCopyLoc(select_expr->default_->loc),
          };
          default_.name = label;
          default_.loc = FbleCopyLoc(select_expr->default_->loc);
          default_.tc = FbleCopyTc(result.tc);
          CleanTcBinding(cleaner, default_);
        }
        result_type = result.type;
      }

      size_t branch = 0;
      FbleTcBranchTarget branches[select_expr->choices.size];
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        if (branch < select_expr->choices.size && FbleNamesEqual(select_expr->choices.xs[branch].name, union_type->fields.xs[i].name)) {
          Tc result = TypeCheckExpr(th, scope, select_expr->choices.xs[branch].expr);
          CleanTc(cleaner, result);
          error = error || (result.type == NULL);

          if (result.type != NULL) {
            branches[branch].tag = i;
            branches[branch].target.name = FbleCopyName(select_expr->choices.xs[branch].name);
            branches[branch].target.loc = FbleCopyLoc(select_expr->choices.xs[branch].expr->loc);
            branches[branch].target.tc = FbleCopyTc(result.tc);
            CleanTcBinding(cleaner, branches[branch].target);
          }

          if (result_type == NULL) {
            result_type = result.type;
          } else if (result.type != NULL) {
            if (!FbleTypesEqual(th, result_type, result.type)) {
              ReportError(select_expr->choices.xs[branch].expr->loc,
                  "expected type %t, but found %t\n",
                  result_type, result.type);
              error = true;
            }
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
                "tag '%n' missing from union select\n",
                union_type->fields.xs[i].name);
          }
        } else {
          // Use the default branch for this field.
          default_used = true;
        }
      }

      if (branch < select_expr->choices.size) {
        error = true;
        ReportError(select_expr->choices.xs[branch].name.loc,
            "illegal use of tag '%n' in union select\n",
            select_expr->choices.xs[branch]);
      }

      if (error) {
        return TC_FAILED;
      }

      if (!default_used) {
        // No default branch was used, but select_tc requires a default. Pick
        // the last tag value to use for the default.
        branch--;
        default_ = branches[branch].target;
      }

      FbleUnionSelectTc* select_tc = FbleNewTc(FbleUnionSelectTc, FBLE_UNION_SELECT_TC, expr->loc);
      select_tc->condition = FbleCopyTc(condition.tc);
      select_tc->num_tags = union_type->fields.size;
      FbleInitVector(select_tc->targets);
      for (size_t i = 0; i < branch; ++i) {
        FbleTcBranchTarget* tgt = FbleExtendVector(select_tc->targets);
        tgt->tag = branches[i].tag;
        tgt->target = CopyTcBinding(branches[i].target);
      }
      select_tc->default_ = CopyTcBinding(default_);

      return MkTc(FbleRetainType(th, result_type), &select_tc->_base);
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* fv = (FbleFuncValueExpr*)expr;

      FbleType* arg_type = TypeCheckType(th, scope, fv->arg.type);
      if (arg_type == NULL) {
        return TC_FAILED;
      }

      FbleVarV captured;
      FbleInitVector(captured);

      Arg arg;
      arg.name.normal = fv->arg.name;
      arg.name.module = NULL;
      arg.type = arg_type;
      ArgV args = { .size = 1, .xs = &arg };

      Scope func_scope;
      InitScope(&func_scope, &captured, args, scope->module, scope);

      Tc func_result = TypeCheckExpr(th, &func_scope, fv->body);
      if (func_result.type == NULL) {
        FreeScope(th, &func_scope);
        FbleFreeVector(captured);
        return TC_FAILED;
      }

      FbleFuncType* ft = FbleNewType(th, FbleFuncType, FBLE_FUNC_TYPE, expr->loc);
      ft->arg = arg_type;
      ft->rtype = func_result.type;
      FbleTypeAddRef(th, &ft->_base, ft->arg);
      FbleTypeAddRef(th, &ft->_base, ft->rtype);
      FbleReleaseType(th, ft->rtype);

      FbleFuncValueTc* func_tc = FbleNewTc(FbleFuncValueTc, FBLE_FUNC_VALUE_TC, expr->loc);
      func_tc->body_loc = FbleCopyLoc(fv->body->loc);
      func_tc->scope = captured;

      FbleInitVector(func_tc->statics);
      for (size_t i = 0; i < func_scope.statics.size; ++i) {
        Var* var = func_scope.statics.xs[i];
        FbleName name;
        if (var->name.module == NULL) {
          name = FbleCopyName(var->name.normal);
        } else {
          name = FbleModulePathName(var->name.module);
        }
        FbleAppendToVector(func_tc->statics, name);
      }

      FbleInitVector(func_tc->args);
      FbleAppendToVector(func_tc->args, FbleCopyName(fv->arg.name));
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
      PushLocalTypeVar(scope, name, arg_type);
      Tc body = TypeCheckExpr(th, scope, poly->body);
      CleanTc(cleaner, body);
      PopLocalVar(th, scope);

      if (body.type == NULL) {
        return TC_FAILED;
      }

      FbleType* pt = FbleNewPolyType(th, expr->loc, arg, body.type);
      return MkTc(pt, FbleCopyTc(body.tc));
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;
      Tc poly = TypeCheckExpr(th, scope, apply->poly);
      CleanTc(cleaner, poly);
      FbleType* arg_type = TypeCheckExprForType(th, scope, apply->arg);
      CleanType(cleaner, arg_type);
      return PolyApply(th, poly, arg_type, expr->loc, apply->arg->loc);
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* list_expr = (FbleListExpr*)expr;

      Tc func = TypeCheckExpr(th, scope, list_expr->func);
      CleanTc(cleaner, func);
      if (func.type == NULL) {
        return TC_FAILED;
      }

      FbleTypeAssignmentV* vars = FbleAlloc(FbleTypeAssignmentV);
      FbleInitVector(*vars);
      CleanTypeAssignmentV(cleaner, vars);

      FbleFuncType* func_type = (FbleFuncType*)DepolyType(th, func.type, vars);
      CleanType(cleaner, &func_type->_base);
      if (func_type->_base.tag != FBLE_FUNC_TYPE) {
        ReportError(list_expr->func->loc, "expected a function, but found something of type %t\n", func.type);
        return TC_FAILED;
      }

      FbleType* elem_type = FbleListElementType(th, func_type->arg);
      CleanType(cleaner, elem_type);
      if (elem_type == NULL) {
        ReportError(list_expr->func->loc, "expected a list type, but the input to the function has type %t\n", func_type->arg);
        return TC_FAILED;
      }

      bool error = false;

      size_t argc = list_expr->args.size;
      FbleType* expected_arg_types[argc];
      Tc args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = TypeCheckExpr(th, scope, list_expr->args.xs[i]);
        CleanTc(cleaner, args[i]);
        error = error || (args[i].type == NULL);
        expected_arg_types[i] = elem_type;
      }

      if (error) {
        return TC_FAILED;
      }

      TcV argv = { .size = argc, .xs = args };
      FbleTypeV expected = { .size = argc, .xs = expected_arg_types };
      Tc poly = TypeInferArgs(th, *vars, expected, argv, func);
      CleanTc(cleaner, poly);

      if (poly.type == NULL) {
        return TC_FAILED;
      }

      FbleFuncType* inferred_func_type = (FbleFuncType*)FbleNormalType(th, poly.type);
      CleanType(cleaner, &inferred_func_type->_base);
      assert(inferred_func_type->_base.tag == FBLE_FUNC_TYPE);

      FbleType* result_type = FbleRetainType(th, inferred_func_type->rtype);

      FbleListTc* list_tc = FbleNewTc(FbleListTc, FBLE_LIST_TC, expr->loc);
      FbleInitVector(list_tc->fields);
      for (size_t i = 0; i < argc; ++i) {
        FbleAppendToVector(list_tc->fields, FbleCopyTc(args[i].tc));
      }

      FbleFuncApplyTc* apply = FbleNewTc(FbleFuncApplyTc, FBLE_FUNC_APPLY_TC, expr->loc);
      apply->func = FbleCopyTc(poly.tc);
      apply->arg = &list_tc->_base;
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
      if (func_type->_base.tag != FBLE_FUNC_TYPE) {
        ReportError(literal_expr->func->loc, "expected a function, but found something of type %t\n", func.type);
        return TC_FAILED;
      }

      FbleType* elem_type = FbleListElementType(th, func_type->arg);
      CleanType(cleaner, elem_type);
      if (elem_type == NULL) {
        ReportError(literal_expr->func->loc, "expected a list type, but the input to the function has type %t\n", func_type->arg);
        return TC_FAILED;
      }

      FbleDataType* elem_data_type = (FbleDataType*)FbleNormalType(th, elem_type);
      CleanType(cleaner, &elem_data_type->_base);
      if (elem_data_type->_base.tag != FBLE_DATA_TYPE || elem_data_type->datatype != FBLE_UNION_DATATYPE) {
        ReportError(literal_expr->func->loc, "expected union type, but element type of literal expression is %t\n", elem_type);
        return TC_FAILED;
      }

      FbleDataType* unit_type = FbleNewType(th, FbleDataType, FBLE_DATA_TYPE, expr->loc);
      unit_type->datatype = FBLE_STRUCT_DATATYPE;
      FbleInitVector(unit_type->fields);
      CleanType(cleaner, &unit_type->_base);

      bool error = false;
      FbleLoc loc = literal_expr->word_loc;
      size_t maxargc = strlen(literal_expr->word);
      size_t args[maxargc];
      size_t argc = 0;
      const char* word = literal_expr->word;
      size_t wordlen = maxargc;
      while (wordlen > 0) {
        size_t maxlen = 0;
        for (size_t j = 0; j < elem_data_type->fields.size; ++j) {
          const char* fieldname = elem_data_type->fields.xs[j].name.name->str;
          size_t fieldnamelen = strlen(fieldname);
          if (fieldnamelen > maxlen && fieldnamelen <= wordlen &&
              strncmp(word, fieldname, fieldnamelen) == 0) {
            maxlen = fieldnamelen;
            args[argc] = j;
          }
        }

        if (maxlen == 0) {
          ReportError(loc, "next letter of literal '%s' not found in type %t\n", word, elem_type);
          error = true;
          break;
        } else if (!FbleTypesEqual(th, &unit_type->_base, elem_data_type->fields.xs[args[argc]].type)) {
          ReportError(loc, "expected field type %t, but '%s' has field type %t\n",
              unit_type, elem_data_type->fields.xs[args[argc]].name.name->str, elem_data_type->fields.xs[args[argc]].type);
          error = true;
          break;
        }

        argc++;
        for (size_t i = 0; i < maxlen; ++i) {
          if (word[i] == '\n') {
            loc.line++;
            loc.col = 0;
          }
          loc.col++;
        }
        word += maxlen;
        wordlen -= maxlen;
      }

      if (error) {
        return TC_FAILED;
      }

      FbleType* result_type = FbleRetainType(th, func_type->rtype);

      FbleLiteralTc* literal_tc = FbleNewTc(FbleLiteralTc, FBLE_LITERAL_TC, expr->loc);
      FbleInitVector(literal_tc->letters);
      for (size_t i = 0; i < argc; ++i) {
        FbleAppendToVector(literal_tc->letters, args[i]);
      }

      FbleFuncApplyTc* apply = FbleNewTc(FbleFuncApplyTc, FBLE_FUNC_APPLY_TC, expr->loc);
      apply->func = FbleCopyTc(func.tc);
      apply->arg = &literal_tc->_base;
      return MkTc(result_type, &apply->_base);
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

    case FBLE_MODULE_PATH_EXPR: {
      FbleModulePathExpr* path_expr = (FbleModulePathExpr*)expr;

      VarName name = { .module = path_expr->path };
      Var* var = GetVar(th, scope, name, false);

      // We should have resolved all modules at program load time.
      assert(var != NULL && "module not in scope");
      assert(var->type != NULL && "recursive module reference");

      FbleVarTc* var_tc = FbleNewTc(FbleVarTc, FBLE_VAR_TC, expr->loc);
      var_tc->var = var->var;
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

      FbleType* nmisc = FbleNormalType(th, misc.type);
      CleanType(cleaner, nmisc);

      if (!apply_expr->bind && nmisc->tag == FBLE_TYPE_TYPE) {
        FbleTypeType* type_type = (FbleTypeType*)nmisc;
        FbleType* vtype = FbleRetainType(th, type_type->type);
        CleanType(cleaner, vtype);

        FbleType* vnorm = FbleNormalType(th, vtype);
        CleanType(cleaner, vnorm);

        // Typecheck for abstract type expression.
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

        // Typecheck for possibly polymorphic struct value expression.
        FbleTypeAssignmentV* vars = FbleAlloc(FbleTypeAssignmentV);
        FbleInitVector(*vars);
        CleanTypeAssignmentV(cleaner, vars);

        FbleDataType* struct_type = (FbleDataType*)DepolyType(th, vtype, vars);
        CleanType(cleaner, &struct_type->_base);

        if (struct_type->_base.tag == FBLE_DATA_TYPE
            && struct_type->datatype == FBLE_STRUCT_DATATYPE) {
          FbleTypeV expected;
          FbleInitVector(expected);
          for (size_t i = 0; i < struct_type->fields.size; ++i) {
            FbleAppendToVector(expected, struct_type->fields.xs[i].type);
          }

          TcV argv = { .size = argc, .xs = args };
          Tc vtc = { .type = vtype, .tc = misc.tc };
          Tc poly = TypeInferArgs(th, *vars, expected, argv, vtc);
          CleanTc(cleaner, poly);
          FbleFreeVector(expected);

          if (poly.type == NULL) {
            return TC_FAILED;
          }

          FbleStructValueTc* struct_tc = FbleNewTc(FbleStructValueTc, FBLE_STRUCT_VALUE_TC, expr->loc);
          FbleInitVector(struct_tc->fields);
          for (size_t i = 0; i < argc; ++i) {
            FbleAppendToVector(struct_tc->fields, FbleCopyTc(args[i].tc));
          }
          return MkTc(FbleRetainType(th, poly.type), &struct_tc->_base);
        }
      }

      // Typecheck for possibly polymorphic function application.
      // We do type inference and application one argument at a time.
      Tc result = misc;
      for (size_t i = 0; i < argc; ++i) {
        FbleTypeAssignmentV* vars = FbleAlloc(FbleTypeAssignmentV);
        FbleInitVector(*vars);
        CleanTypeAssignmentV(cleaner, vars);

        FbleType* pbody = DepolyType(th, result.type, vars);
        CleanType(cleaner, pbody);

        if (pbody->tag == FBLE_FUNC_TYPE) {
          FbleFuncType* func_type = (FbleFuncType*)pbody;

          TcV argv = { .size = 1, .xs = args + i };
          FbleTypeV expected = { .size = 1, .xs = &func_type->arg };
          Tc poly = TypeInferArgs(th, *vars, expected, argv, result);
          CleanTc(cleaner, poly);
          if (poly.type == NULL) {
            return TC_FAILED;
          }

          // Do the func apply.
          func_type = (FbleFuncType*)FbleNormalType(th, poly.type);
          assert(func_type->_base.tag == FBLE_FUNC_TYPE);

          FbleFuncApplyTc* apply_tc = FbleNewTc(FbleFuncApplyTc, FBLE_FUNC_APPLY_TC, expr->loc);
          apply_tc->func = FbleCopyTc(poly.tc);
          apply_tc->arg = FbleCopyTc(args[i].tc);

          CleanTc(cleaner, MkTc(&func_type->_base, &apply_tc->_base));
          result.tc = &apply_tc->_base;
          result.type = func_type->rtype;
        } else {
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

      return MkTc(FbleRetainType(th, result.type), FbleCopyTc(result.tc));
    }
  }

  FbleUnreachable("should already have returned");
  return TC_FAILED;
}

/**
 * Typechecks the given expression, ignoring accesses to variables.
 *
 * Sometimes an expression is used only for its type. We don't want to
 * mark variables referenced by the expression as used, because we don't need
 * to know the value of the variable at runtime. This function typechecks an
 * expression without marking variables as used.
 *
 * @param th  Heap to use for allocations.
 * @param scope  The list of variables in scope.
 * @param expr  The expression to compile.
 *
 * @returns
 *   The type of the expression, or NULL if the expression is not well typed.
 *
 * @sideeffects
 * * Prints a message to stderr if the expression fails to compile.
 * * Allocates an FbleType that must be freed using FbleReleaseType when it is
 *   no longer needed.
 */
static FbleType* TypeCheckExprForType(FbleTypeHeap* th, Scope* scope, FbleExpr* expr)
{
  ArgV args = { .size = 0, .xs = NULL };

  Scope nscope;
  InitScope(&nscope, NULL, args, scope->module, scope);

  Tc result = TypeCheckExpr(th, &nscope, expr);
  FreeScope(th, &nscope);
  FbleFreeTc(result.tc);
  return result.type;
}

/**
 * Typechecks a type, returning its value.
 *
 * @param th  Heap to use for allocations.
 * @param scope  The value of variables in scope.
 * @param type  The type to compile.
 *
 * @returns
 *   The type checked and evaluated type, or NULL in case of error.
 *
 * @sideeffects
 * * Prints a message to stderr if the type fails to compile or evalute.
 * * Allocates an FbleType that must be freed using FbleReleaseType when it is
 *   no longer needed.
 */
static FbleType* TypeCheckType(FbleTypeHeap* th, Scope* scope, FbleTypeExpr* type)
{
  Cleaner* cleaner = NewCleaner();
  FbleType* result = TypeCheckTypeWithCleaner(th, scope, type, cleaner);
  Cleanup(th, cleaner);
  return result;
}
 
/**
 * Typechecks a type, with automatic cleanup.
 *
 * @param th  Heap to use for allocations.
 * @param scope  The value of variables in scope.
 * @param type  The type to compile.
 * @param cleaner  The cleaner object.
 *
 * @returns
 *   The type checked and evaluated type, or NULL in case of error.
 *
 * @sideeffects
 * * Prints a message to stderr if the type fails to compile or evalute.
 * * Allocates an FbleType that must be freed using FbleReleaseType when it is
 *   no longer needed.
 * * Adds objects to the cleaner that the caller is responsible for cleaning
 *   up after the function returns.
 */
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
      FbleInitVector(dt->fields);
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
        FbleAppendToVector(dt->fields, cfield);
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
      FbleFuncTypeExpr* func_type = (FbleFuncTypeExpr*)type;

      FbleType* arg = TypeCheckType(th, scope, func_type->arg);
      CleanType(cleaner, arg);

      FbleType* rtype = TypeCheckType(th, scope, func_type->rtype);
      CleanType(cleaner, rtype);

      if (arg == NULL || rtype == NULL) {
        return NULL;
      }

      FbleFuncType* ft = FbleNewType(th, FbleFuncType, FBLE_FUNC_TYPE, type->loc);
      ft->arg = arg;
      ft->rtype = rtype;
      FbleTypeAddRef(th, &ft->_base, arg);
      FbleTypeAddRef(th, &ft->_base, rtype);
      return &ft->_base;
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
    case FBLE_STRUCT_COPY_EXPR:
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
      FbleType* type_type = TypeCheckExprForType(th, scope, expr);
      CleanType(cleaner, type_type);
      if (type_type == NULL) {
        return NULL;
      }

      FbleType* type_value = FbleValueOfType(th, type_type);
      if (type_value == NULL) {
        ReportError(expr->loc, "expected a type, but found value of type %t\n", type_type);
        return NULL;
      }
      return type_value;
    }
  }

  FbleUnreachable("should already have returned");
  return NULL;
}

/**
 * Typechecks a module.
 *
 * @param th  Heap to use for allocations.
 * @param module  The module to check.
 * @param deps  The type of each module this module depends on, in the same order
 *   as module->deps. size is module->deps.size.
 *
 * @returns
 *   Returns the type, and optionally value of the module as the body of a
 *   function that takes module dependencies as arguments and computes the
 *   value of the module. TC_FAILED if the module failed to type check.
 *
 *   If module->value is not provided but module->type still type checks, this
 *   will return a Tc with non-NULL type but NULL tc.
 *
 * @sideeffects
 * * Prints warning messages to stderr.
 * * Prints a message to stderr if the module fails to type check.
 * * The caller should call FbleFreeTc when the returned result is no longer
 *   needed and FbleReleaseType when the returned FbleType is no longer
 *   needed.
 */
static Tc TypeCheckModule(FbleTypeHeap* th, FbleLoadedModule* module, FbleType** deps)
{
  Arg args_xs[module->deps.size];
  for (size_t i = 0; i < module->deps.size; ++i) {
    args_xs[i].name.module = module->deps.xs[i];
    args_xs[i].type = FbleRetainType(th, deps[i]);
  }
  ArgV args = { .size = module->deps.size, .xs = args_xs };

  Scope scope;
  InitScope(&scope, NULL, args, module->path, NULL);

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
    FbleWarnAboutUnusedVars(module->value);
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

// See documentation in typecheck.h.
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

// See documentation in typecheck.h.
FbleTc** FbleTypeCheckProgram(FbleLoadedProgram* program)
{
  FbleTc** tcs = FbleAllocArray(FbleTc*, program->modules.size);

  bool error = false;
  FbleTypeHeap* th = FbleNewTypeHeap();
  FbleType* types[program->modules.size];

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleLoadedModule* module = program->modules.xs + i;
    FbleType* deps[module->deps.size];

    bool skip = false;
    for (size_t d = 0; d < module->deps.size; ++d) {
      deps[d] = NULL;
      for (size_t t = 0; t < i; ++t) {
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

  for (size_t i = 0; i < program->modules.size; ++i) {
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
