/**
 * @file compile.c
 *  Compile FbleTc* abstract syntax into FbleCode* bytecode.
 */

#include <fble/fble-codegen.h>

#include <assert.h>   // for assert
#include <string.h>   // for strlen, strcat
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>     // for FbleAlloc, etc.
#include <fble/fble-link.h>      // for FbleExecutableModule
#include <fble/fble-vector.h>    // for FbleInitVector, etc.

#include "code.h"
#include "tc.h"
#include "typecheck.h"
#include "unreachable.h"

typedef struct Local Local;

/** Vector of pointers to locals. */
typedef struct {
  size_t size;      /**< Number of elements. */
  Local** xs;       /**< The elements. */
} LocalV;

/** Info about a value available in the stack frame. */
struct Local {
  FbleVar var;        /**< The variable. */
  size_t refcount;    /**< The number of references to the local. */

  /**
   * NULL if this local is retained. Non-NULL if this local is kept alive by
   * some other owner, where owner is that other local keeping this alive.
   */
  struct Local* owner;

  /**
   * List of other locals that have this local as the owner.
   */
  LocalV owned;
};

/** Scope of variables visible during compilation. */
typedef struct Scope {
  /** Variables captured from the parent scope. Owned by the scope. */
  LocalV statics;

  /** Arguments to the function. Owns the Locals. */
  LocalV args;

  /**
   * Stack of local variables in scope order.
   * Entries may be NULL.
   * Owns the Locals.
   */
  LocalV vars;

  /**
   * Local values.
   * Entries may be NULL to indicate a free slot.
   * Owns the Locals.
   */
  LocalV locals;

  /** The instruction block for this scope. */
  FbleCode* code;

  /**
   * Debug info to apply before the next instruction to be added.
   */
  FbleDebugInfo* pending_debug_info;

  /** Profiling ops associated with the next instruction to be added. */
  FbleProfileOp* pending_profile_ops;

  /**
   * The currently active set of profiling ops.
   *
   * New ops should be added to this list to be coalesced together where
   * possible. This should be reset to point to pending_profile_ops at the
   * start of any new basic blocks.
   */
  FbleProfileOp** active_profile_ops;

  /** The parent of this scope. May be NULL. */
  struct Scope* parent;
} Scope;

static void FreeLocal(Local* local);
static Local* NewLocal(Scope* scope, Local* owner);
static void ReleaseLocal(Scope* scope, Local* local, bool exit);
static void PushVar(Scope* scope, FbleName name, Local* local);
static void PopVar(Scope* scope, bool exit);
static Local* GetVar(Scope* scope, FbleVar index);
static void SetVar(Scope* scope, size_t index, FbleName name, Local* local);

static FbleVar RewriteVar(FbleVarV statics, size_t arg_offset, FbleVar var);
static FbleTc* RewriteVars(FbleVarV statics, size_t arg_offset, FbleTc* tc);

static void InitScope(Scope* scope, FbleCode** code, FbleNameV args, FbleNameV statics, FbleBlockId block, Scope* parent);
static void FreeScope(Scope* scope);
static void AppendInstr(Scope* scope, FbleInstr* instr);
static void AppendRetainInstr(Scope* scope, FbleVar var);
static void AppendReleaseInstr(Scope* scope, FbleLocalIndex index);
static void AppendDebugInfo(Scope* scope, FbleDebugInfo* info);
static void AppendProfileOp(Scope* scope, FbleProfileOpTag tag, size_t arg);

/**
 *   Stack of block frames tracking the current block for profiling.
 */
typedef struct {
  /** The stack of black frames representing the current location. */
  FbleBlockIdV stack;

  /** The names of profile blocks to append to. */
  FbleNameV profile;
} Blocks;

static FbleBlockId PushBlock(Blocks* blocks, FbleName name, FbleLoc loc);
static FbleBlockId PushBodyBlock(Blocks* blocks, FbleLoc loc);
static void EnterBlock(Blocks* blocks, FbleName name, FbleLoc loc, Scope* scope, bool replace);
static void PopBlock(Blocks* blocks);
static void ExitBlock(Blocks* blocks, Scope* scope, bool exit);

static void CompileExit(bool exit, Scope* scope, Local* result);
static Local* CompileExpr(Blocks* blocks, bool stmt, bool exit, Scope* scope, FbleTc* tc);
static FbleCode* Compile(FbleNameV args, FbleTc* tc, FbleName name, FbleNameV* profile_blocks);
static FbleCompiledModule* CompileModule(FbleLoadedModule* module, FbleTc* tc);

/**
 * Frees memory assicoated with a local.
 *
 * @param local  The local to free.
 * @sideeffects
 *  Frees memory associated with local.
 */
static void FreeLocal(Local* local)
{
  FbleFreeVector(local->owned);
  FbleFree(local);
}

/**
 * Allocates space for an anonymous local variable on the stack frame.
 *
 * @param scope  The scope to allocate the local on.
 * @param owner  The owner of this local, or NULL.
 *
 * @returns
 *   A newly allocated local.
 *
 * @sideeffects
 *   Allocates a space on the scope's locals for the local. The local should
 *   be freed with ReleaseLocal when no longer in use.
 */
static Local* NewLocal(Scope* scope, Local* owner)
{
  size_t index = scope->locals.size;
  for (size_t i = 0; i < scope->locals.size; ++i) {
    if (scope->locals.xs[i] == NULL) {
      index = i;
      break;
    }
  }

  if (index == scope->locals.size) {
    FbleAppendToVector(scope->locals, NULL);
    scope->code->num_locals = scope->locals.size;
  }

  Local* local = FbleAlloc(Local);
  local->var.tag = FBLE_LOCAL_VAR;
  local->var.index = index;
  local->refcount = 1;
  local->owner = owner;
  FbleInitVector(local->owned);

  if (owner != NULL) {
    FbleAppendToVector(owner->owned, local);
  }

  scope->locals.xs[index] = local;
  return local;
}

/**
 * Decrements the reference count on a local and free it if appropriate.
 *
 * @param scope  The scope that the local belongs to
 * @param local  The local to release. May be NULL.
 * @param exit  Whether the stack frame has already been exited or not.
 *
 * @sideeffects
 *   Decrements the reference count on the local and frees it if the refcount
 *   drops to 0. Emits a RELEASE instruction if this is the last reference the
 *   local and the stack frame hasn't already been exited.
 */
static void ReleaseLocal(Scope* scope, Local* local, bool exit)
{
  if (local == NULL) {
    return;
  }

  local->refcount--;
  if (local->refcount == 0) {
    // refcount should never drop to 0 on static or arg vars.
    assert(local->var.tag == FBLE_LOCAL_VAR);
    assert(scope->locals.xs[local->var.index] == local);

    if (local->owner == NULL) {
      // Explicitly retain any owned by this local, because they can no
      // longer rely on this local to keep them alive.
      for (size_t i = 0; i < local->owned.size; ++i) {
        local->owned.xs[i]->owner = NULL;
        if (!exit) {
          AppendRetainInstr(scope, local->owned.xs[i]->var);
        }
      }

      if (!exit) {
        // Release this var.
        AppendReleaseInstr(scope, local->var.index);
      }
    } else {
      // Remove ownership of this from the owner
      for (size_t i = 0; i < local->owner->owned.size; ++i) {
        if (local->owner->owned.xs[i] == local) {
          local->owner->owned.size--;
          local->owner->owned.xs[i] = local->owner->owned.xs[local->owner->owned.size];
          break;
        }
      }

      // Transfer ownership of owned to the owner.
      for (size_t i = 0; i < local->owned.size; ++i) {
        local->owned.xs[i]->owner = local->owner;
        FbleAppendToVector(local->owner->owned, local->owned.xs[i]);
      }
    }

    scope->locals.xs[local->var.index] = NULL;
    FreeLocal(local);
  }
}

/**
 * Pushes a variable onto the current scope.
 *
 * @param scope  The scope to push the variable on to.
 * @param name  The name of the variable. Borrowed.
 * @param local  The local address of the variable. Consumed.
 *
 * @sideeffects
 *   Pushes a new variable onto the scope. Takes ownership of the given local,
 *   which will be released when the variable is freed by a call to PopVar or
 *   FreeScope.
 */
static void PushVar(Scope* scope, FbleName name, Local* local)
{
  if (local != NULL) {
    FbleVarDebugInfo* info = FbleAlloc(FbleVarDebugInfo);
    info->_base.tag = FBLE_VAR_DEBUG_INFO;
    info->_base.next = NULL;
    info->name = FbleCopyName(name);
    info->var = local->var;
    AppendDebugInfo(scope, &info->_base);
  }

  FbleAppendToVector(scope->vars, local);
}

/**
 * Pops a var off the given scope.
 *
 * @param scope  The scope to pop from.
 * @param exit  Whether the stack frame has already been exited or not.
 *
 * @sideeffects
 *   Pops the top var off the scope.
 */
static void PopVar(Scope* scope, bool exit)
{
  scope->vars.size--;
  Local* var = scope->vars.xs[scope->vars.size];
  ReleaseLocal(scope, var, exit);
}

/**
 * Lookup a var in the given scope.
 *
 * @param scope  The scope to look in.
 * @param var  The variable to look up.
 *
 * @returns
 *   The variable from the scope. The variable is owned by the scope and
 *   remains valid until either PopVar is called or the scope is finished.
 *
 * @sideeffects
 *   Behavior is undefined if there is no such variable.
 */
static Local* GetVar(Scope* scope, FbleVar var)
{
  switch (var.tag) {
    case FBLE_STATIC_VAR: {
      assert(var.index < scope->statics.size && "invalid static var index");
      return scope->statics.xs[var.index];
    }

    case FBLE_ARG_VAR: {
      assert(var.index < scope->args.size && "invalid arg var index");
      return scope->args.xs[var.index];
    }

    case FBLE_LOCAL_VAR: {
      assert(var.index < scope->vars.size && "invalid local var index");
      return scope->vars.xs[var.index];
    }
  }

  FbleUnreachable("should never get here");
  return NULL;
}

/**
 * Changes the value of a variable in scope.
 *
 * @param scope  Scope of variables
 * @param index  The index of the local variable to change
 * @param name  The name of the variable.
 * @param local  The new value for the local variable
 *
 * @sideeffects
 * * Frees the existing local value of the variable.
 * * Sets the value of the variable to the given local.
 * * Takes ownership of the given local.
 */
static void SetVar(Scope* scope, size_t index, FbleName name, Local* local)
{
  assert(index < scope->vars.size);
  ReleaseLocal(scope, scope->vars.xs[index], false);
  scope->vars.xs[index] = local;

  FbleVarDebugInfo* info = FbleAlloc(FbleVarDebugInfo);
  info->_base.tag = FBLE_VAR_DEBUG_INFO;
  info->_base.next = NULL;
  info->name = FbleCopyName(name);
  info->var = local->var;
  AppendDebugInfo(scope, &info->_base);
}

/**
 * @func[RewriteVar] Rewrites a variable.
 *  Replaces static variable references their corresponding values in the
 *  given scope, and increments arg variable indices by the given offset.
 *
 *  This is a helper function for merging nested function values into a multi
 *  argument function.
 *
 *  @arg[FbleVarV][statics] Replacements to use for static variable references.
 *  @arg[size_t][arg_offset] Offset to apply to arg variable references.
 *  @arg[FbleVar][var] The var to rewrite.
 *
 *  @returns[FbleVar] The rewriten var.
 *
 *  @sideeffects None
 */
static FbleVar RewriteVar(FbleVarV statics, size_t arg_offset, FbleVar var)
{
  switch (var.tag) {
    case FBLE_STATIC_VAR: {
      assert(var.index < statics.size);
      return statics.xs[var.index];
    }

    case FBLE_ARG_VAR: {
      FbleVar nvar = { .tag = FBLE_ARG_VAR, .index = var.index + arg_offset };
      return nvar;
    }

    case FBLE_LOCAL_VAR: {
      return var;
    }
  }

  FbleUnreachable("should never get here");
  return var;
}

/**
 * @func[RewriteVars] Adjust vars in expr.
 *  Replaces static variable references in expr with their corresponding
 *  values in the given scope, and increments arg variable indices by the
 *  given offset.
 *
 *  This is a helper function for merging nested function values into a multi
 *  argument function.
 *
 *  @arg[FbleVarV][statics] Replacements to use for static variable references.
 *  @arg[size_t][arg_offset] Offset to apply to arg variable references.
 *  @arg[FbleTc*][tc] The expression to rewrite. Borrowed.
 *
 *  @returns[FbleTc*] The rewriten expression.
 *
 *  @sideeffects
 *   Allocates a new FbleTc* that should be freed with FbleFreeTc when no
 *   longer needed.
 */
static FbleTc* RewriteVars(FbleVarV statics, size_t arg_offset, FbleTc* tc)
{
  switch (tc->tag) {
    case FBLE_TYPE_VALUE_TC: return FbleCopyTc(tc);

    case FBLE_VAR_TC: {
      FbleVarTc* var_tc = (FbleVarTc*)tc;
      FbleVarTc* ntc = FbleNewTc(FbleVarTc, FBLE_VAR_TC, tc->loc);
      ntc->var = RewriteVar(statics, arg_offset, var_tc->var);
      return &ntc->_base;
    }

    case FBLE_LET_TC: {
      FbleLetTc* let_tc = (FbleLetTc*)tc;

      FbleLetTc* ntc = FbleNewTc(FbleLetTc, FBLE_LET_TC, tc->loc);
      ntc->recursive = let_tc->recursive;

      FbleInitVector(ntc->bindings);
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        FbleTcBinding* binding = FbleExtendVector(ntc->bindings);
        binding->name = FbleCopyName(let_tc->bindings.xs[i].name);
        binding->loc = FbleCopyLoc(let_tc->bindings.xs[i].loc);
        binding->tc = RewriteVars(statics, arg_offset, let_tc->bindings.xs[i].tc);
      }

      ntc->body = RewriteVars(statics, arg_offset, let_tc->body);
      return &ntc->_base;
    }

    case FBLE_STRUCT_VALUE_TC: {
      FbleStructValueTc* sv = (FbleStructValueTc*)tc;
      FbleStructValueTc* ntc = FbleNewTc(FbleStructValueTc, FBLE_STRUCT_VALUE_TC, tc->loc);
      FbleInitVector(ntc->fields);
      for (size_t i = 0; i < sv->fields.size; ++i) {
        FbleAppendToVector(ntc->fields, RewriteVars(statics, arg_offset, sv->fields.xs[i]));
      }
      return &ntc->_base;
    }

    case FBLE_STRUCT_COPY_TC: {
      FbleStructCopyTc* sv = (FbleStructCopyTc*)tc;
      FbleStructCopyTc* ntc = FbleNewTc(FbleStructCopyTc, FBLE_STRUCT_COPY_TC, tc->loc);
      ntc->source = RewriteVars(statics, arg_offset, sv->source);
      FbleInitVector(ntc->fields);
      for (size_t i = 0; i < sv->fields.size; ++i) {
        FbleTc* field = NULL;
        if (sv->fields.xs[i] != NULL) {
          field = RewriteVars(statics, arg_offset, sv->fields.xs[i]);
        }
        FbleAppendToVector(ntc->fields, field);
      }
      return &ntc->_base;
    }

    case FBLE_UNION_VALUE_TC: {
      FbleUnionValueTc* uv = (FbleUnionValueTc*)tc;
      FbleUnionValueTc* ntc = FbleNewTc(FbleUnionValueTc, FBLE_UNION_VALUE_TC, tc->loc);
      ntc->tag = uv->tag;
      ntc->arg = RewriteVars(statics, arg_offset, uv->arg);
      return &ntc->_base;
    }

    case FBLE_UNION_SELECT_TC: {
      FbleUnionSelectTc* v = (FbleUnionSelectTc*)tc;
      FbleUnionSelectTc* ntc = FbleNewTc(FbleUnionSelectTc, FBLE_UNION_SELECT_TC, tc->loc);
      ntc->condition = RewriteVars(statics, arg_offset, v->condition);
      ntc->num_tags = v->num_tags;
      FbleInitVector(ntc->targets);
      for (size_t i = 0; i < v->targets.size; ++i) {
        FbleTcBranchTarget* tgt = FbleExtendVector(ntc->targets);
        tgt->tag = v->targets.xs[i].tag;
        tgt->target.name = FbleCopyName(v->targets.xs[i].target.name);
        tgt->target.loc = FbleCopyLoc(v->targets.xs[i].target.loc);
        tgt->target.tc = RewriteVars(statics, arg_offset, v->targets.xs[i].target.tc);
      }

      ntc->default_.name = FbleCopyName(v->default_.name);
      ntc->default_.loc = FbleCopyLoc(v->default_.loc);
      ntc->default_.tc = RewriteVars(statics, arg_offset, v->default_.tc);
      return &ntc->_base;
    }

    case FBLE_DATA_ACCESS_TC: {
      FbleDataAccessTc* v = (FbleDataAccessTc*)tc;
      FbleDataAccessTc* ntc = FbleNewTc(FbleDataAccessTc, FBLE_DATA_ACCESS_TC, tc->loc);
      ntc->datatype = v->datatype;
      ntc->obj = RewriteVars(statics, arg_offset, v->obj);
      ntc->tag = v->tag;
      ntc->loc = FbleCopyLoc(v->loc);
      return &ntc->_base;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)tc;
      FbleFuncValueTc* ntc = FbleNewTc(FbleFuncValueTc, FBLE_FUNC_VALUE_TC, tc->loc);
      ntc->body_loc = FbleCopyLoc(func_tc->body_loc);

      FbleInitVector(ntc->scope);
      for (size_t i = 0; i < func_tc->scope.size; ++i) {
        FbleVar nvar = RewriteVar(statics, arg_offset, func_tc->scope.xs[i]);
        FbleAppendToVector(ntc->scope, nvar);
      }

      FbleInitVector(ntc->statics);
      for (size_t i = 0; i < func_tc->statics.size; ++i) {
        FbleAppendToVector(ntc->statics, FbleCopyName(func_tc->statics.xs[i]));
      }

      FbleInitVector(ntc->args);
      for (size_t i = 0; i < func_tc->args.size; ++i) {
        FbleAppendToVector(ntc->args, FbleCopyName(func_tc->args.xs[i]));
      }

      ntc->body = FbleCopyTc(func_tc->body);
      return &ntc->_base;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)tc;
      FbleFuncApplyTc* ntc = FbleNewTc(FbleFuncApplyTc, FBLE_FUNC_APPLY_TC, tc->loc);
      ntc->func = RewriteVars(statics, arg_offset, apply_tc->func);
      ntc->arg = RewriteVars(statics, arg_offset, apply_tc->arg);
      return &ntc->_base;
    }

    case FBLE_LIST_TC: {
      FbleListTc* v = (FbleListTc*)tc;
      FbleListTc* ntc = FbleNewTc(FbleListTc, FBLE_LIST_TC, tc->loc);
      FbleInitVector(ntc->fields);
      for (size_t i = 0; i < v->fields.size; ++i) {
        FbleAppendToVector(ntc->fields, RewriteVars(statics, arg_offset, v->fields.xs[i]));
      }
      return &ntc->_base;
    }

    case FBLE_LITERAL_TC: {
      return FbleCopyTc(tc);
    }
  }

  FbleUnreachable("should never get here");
  return NULL;
}

/**
 * Initializes a new scope.
 *
 * @param scope  The scope to initialize.
 * @param code  A pointer to store the allocated code block for this scope.
 * @param args  The arguments to the function the scope is for. Borrowed.
 * @param statics  Static variables captured by the function. Borrowed.
 * @param block  The profile block id to enter when executing this scope.
 * @param parent  The parent of the scope to initialize. May be NULL.
 *
 * @sideeffects
 * * Initializes scope based on parent. FreeScope should be
 *   called to free the allocations for scope. The lifetimes of the code block
 *   and the parent scope must exceed the lifetime of this scope.
 * * The caller is responsible for calling FbleFreeCode on *code when it
 *   is no longer needed.
 */
static void InitScope(Scope* scope, FbleCode** code, FbleNameV args, FbleNameV statics, FbleBlockId block, Scope* parent)
{
  FbleInitVector(scope->statics);
  for (size_t i = 0; i < statics.size; ++i) {
    Local* local = FbleAlloc(Local);
    local->var.tag = FBLE_STATIC_VAR;
    local->var.index = i;
    local->refcount = 1;
    local->owner = NULL;
    FbleInitVector(local->owned);
    FbleAppendToVector(scope->statics, local);
  }

  FbleInitVector(scope->args);
  for (size_t i = 0; i < args.size; ++i) {
    Local* local = FbleAlloc(Local);
    local->var.tag = FBLE_ARG_VAR;
    local->var.index = i;
    local->refcount = 1;
    local->owner = NULL;
    FbleInitVector(local->owned);
    FbleAppendToVector(scope->args, local);
  }

  FbleInitVector(scope->vars);
  FbleInitVector(scope->locals);

  scope->code = FbleNewCode(args.size, statics.size, 0, block);
  scope->pending_debug_info = NULL;
  scope->pending_profile_ops = NULL;
  scope->active_profile_ops = &scope->pending_profile_ops;
  scope->parent = parent;

  *code = scope->code;

  for (size_t i = 0; i < statics.size; ++i) {
    FbleVarDebugInfo* info = FbleAlloc(FbleVarDebugInfo);
    info->_base.tag = FBLE_VAR_DEBUG_INFO;
    info->_base.next = NULL;
    info->name = FbleCopyName(statics.xs[i]);
    info->var.tag = FBLE_STATIC_VAR;
    info->var.index = i;
    AppendDebugInfo(scope, &info->_base);
  }

  for (size_t i = 0; i < args.size; ++i) {
    FbleVarDebugInfo* info = FbleAlloc(FbleVarDebugInfo);
    info->_base.tag = FBLE_VAR_DEBUG_INFO;
    info->_base.next = NULL;
    info->name = FbleCopyName(args.xs[i]);
    info->var.tag = FBLE_ARG_VAR;
    info->var.index = i;
    AppendDebugInfo(scope, &info->_base);
  }
}

/**
 * Free memory associated with a Scope.
 *
 * @param scope  The scope to finish.
 *
 * @sideeffects
 *   * Frees memory associated with scope.
 */
static void FreeScope(Scope* scope)
{
  for (size_t i = 0; i < scope->statics.size; ++i) {
    FreeLocal(scope->statics.xs[i]);
  }
  FbleFreeVector(scope->statics);

  for (size_t i = 0; i < scope->args.size; ++i) {
    FreeLocal(scope->args.xs[i]);
  }
  FbleFreeVector(scope->args);

  while (scope->vars.size > 0) {
    PopVar(scope, true);
  }
  FbleFreeVector(scope->vars);

  for (size_t i = 0; i < scope->locals.size; ++i) {
    if (scope->locals.xs[i] != NULL) {
      FreeLocal(scope->locals.xs[i]);
    }
  }
  FbleFreeVector(scope->locals);
  FbleFreeDebugInfo(scope->pending_debug_info);

  while (scope->pending_profile_ops != NULL) {
    FbleProfileOp* op = scope->pending_profile_ops;
    scope->pending_profile_ops = op->next;
    FbleFree(op);
  }
}

/**
 * Appends an instruction to the code block for the given scope.
 *
 * @param scope  The scope to append the instruction to.
 * @param instr  The instruction to append.
 *
 * @sideeffects
 *   Appends instr to the code block for the given scope, thus taking
 *   ownership of the instr.
 */
static void AppendInstr(Scope* scope, FbleInstr* instr)
{
  AppendProfileOp(scope, FBLE_PROFILE_SAMPLE_OP, 1);

  assert(instr->debug_info == NULL);
  instr->debug_info = scope->pending_debug_info;
  scope->pending_debug_info = NULL;

  assert(instr->profile_ops == NULL);
  if (scope->pending_profile_ops != NULL) {
    instr->profile_ops = scope->pending_profile_ops;
    scope->pending_profile_ops = NULL;
    scope->active_profile_ops = &instr->profile_ops;
  }

  FbleAppendToVector(scope->code->instrs, instr);
}

/**
 * Outputs an FbleRetainInstr.
 *
 * @param scope  The scope to append the instruction to.
 * @var var  The variable to retain.
 *
 * @sideeffects:
 *   Appends an instruction to the code block for the given scope.
 */
static void AppendRetainInstr(Scope* scope, FbleVar var)
{
  FbleRetainInstr* retain = FbleAllocInstr(FbleRetainInstr, FBLE_RETAIN_INSTR);
  retain->target = var;
  assert((var.tag == FBLE_STATIC_VAR
      || var.tag == FBLE_ARG_VAR
      || var.tag == FBLE_LOCAL_VAR)
      && "Invalid var tag");
  AppendInstr(scope, &retain->_base);
}

/**
 * Outputs an FbleReleaseInstr.
 *
 * Use this for release instructions to facilitate coallescing of sequential
 * releases into a single instruction.
 *
 * @param scope  The scope to append the instruction to.
 * @param index  The index of the variable to release.
 *
 * @sideeffects:
 * * Appends an instruction to the code block for the given scope.
 */
static void AppendReleaseInstr(Scope* scope, FbleLocalIndex index)
{
  if (scope->pending_debug_info == NULL
      && scope->pending_profile_ops == NULL
      && scope->code->instrs.size > 0) {
    FbleReleaseInstr* instr = (FbleReleaseInstr*)scope->code->instrs.xs[scope->code->instrs.size - 1];
    if (instr->_base.tag == FBLE_RELEASE_INSTR) {
      FbleAppendToVector(instr->targets, index);
      return;
    }
  }

  FbleReleaseInstr* release_instr = FbleAllocInstr(FbleReleaseInstr, FBLE_RELEASE_INSTR);
  FbleInitVector(release_instr->targets);
  FbleAppendToVector(release_instr->targets, index);
  AppendInstr(scope, &release_instr->_base);
}

/**
 * Appends a single debug info entry to the code block for the given scope.
 *
 * @param scope  The scope to append the instruction to.
 * @param info  The debug info entry whose next field must be NULL. Consumed.
 *
 * @sideeffects
 *   Appends the debug info to the code block for the given scope, taking
 *   ownership of the allocated debug info object.
 */
static void AppendDebugInfo(Scope* scope, FbleDebugInfo* info)
{
  assert(info->next == NULL);
  if (scope->pending_debug_info == NULL) {
    scope->pending_debug_info = info;
  } else {
    FbleDebugInfo* curr = scope->pending_debug_info;
    while (curr->next != NULL) {
      curr = curr->next;
    }
    curr->next = info;
  } 
}

/**
 * Appends a profile op to the code block for the given scope.
 *
 * @param scope  The scope to append the instruction to.
 * @param tag  The tag of the profile op to insert.
 * @param arg  The argument to the profiling op if relevant.
 *
 * @sideeffects
 *   Appends the profile op to the code block for the given scope.
 */
static void AppendProfileOp(Scope* scope, FbleProfileOpTag tag, size_t arg)
{
  FbleProfileOp* op = FbleAlloc(FbleProfileOp);
  op->tag = tag;
  op->arg = arg;
  op->next = NULL;

  if (*scope->active_profile_ops == NULL) {
    *scope->active_profile_ops = op;
  } else {
    FbleProfileOp* curr = *scope->active_profile_ops;
    while (curr->next != NULL) {
      curr = curr->next;
    }

    if (tag == FBLE_PROFILE_SAMPLE_OP && curr->tag == FBLE_PROFILE_SAMPLE_OP) {
      // No need to add a new sample op, we can merge this with the existing
      // profile sample op.
      curr->arg += op->arg;
      FbleFree(op);
    } else {
      curr->next = op;
    }
  } 
}

/**
 * Pushes a new profiling block onto the block stack.
 *
 * @param blocks  The blocks stack.
 * @param name  Name to add to the current block path for naming the new block.
 * @param loc  The location of the block.
 *
 * @returns
 *   The id of the newly pushed profiling block.
 *
 * @sideeffects
 * * Pushes a new block to the blocks stack.
 * * The block should be popped from the stack using ExitBlock or one of the
 *   other functions that exit a block when no longer needed.
 */
static FbleBlockId PushBlock(Blocks* blocks, FbleName name, FbleLoc loc)
{
  const char* curr = NULL;
  size_t currlen = 0;
  if (blocks->stack.size > 0) {
    size_t curr_id = blocks->stack.xs[blocks->stack.size-1];
    curr = blocks->profile.xs[curr_id].name->str;
    currlen = strlen(curr);
  }

  // Append ".name" to the current block name to figure out the new block
  // name.
  FbleString* str = FbleAllocExtra(FbleString, currlen + strlen(name.name->str) + 3);
  str->refcount = 1;
  str->magic = FBLE_STRING_MAGIC;
  str->str[0] = '\0';
  if (currlen > 0) {
    strcat(str->str, curr);
    strcat(str->str, ".");
  }
  strcat(str->str, name.name->str);
  switch (name.space) {
    case FBLE_NORMAL_NAME_SPACE: break;
    case FBLE_TYPE_NAME_SPACE: strcat(str->str, "@"); break;
  }

  FbleName nm = { .name = str, .loc = FbleCopyLoc(loc) };
  size_t id = blocks->profile.size;
  FbleAppendToVector(blocks->profile, nm);
  FbleAppendToVector(blocks->stack, id);
  return id;
}

/**
 * Adds a new body profiling block to the block stack.
 *
 * This is used for the body of functions and processes that are executed when
 * they are called, not when they are defined.
 *
 * @param blocks  The blocks stack.
 * @param loc  The location of the new block.
 *
 * @returns
 *   The id of the newly pushed block.
 *
 * @sideeffects
 * * Adds a new block to the blocks stack.
 * * The block should be popped from the stack using ExitBlock or one of the
 *   other functions that exit a block when no longer needed.
 */
static FbleBlockId PushBodyBlock(Blocks* blocks, FbleLoc loc)
{
  const char* curr = "";
  if (blocks->stack.size > 0) {
    size_t curr_id = blocks->stack.xs[blocks->stack.size-1];
    curr = blocks->profile.xs[curr_id].name->str;
  }

  // Append "!" to the current block name to figure out the new block name.
  FbleString* str = FbleAllocExtra(FbleString, strlen(curr) + 2);
  str->refcount = 1;
  str->magic = FBLE_STRING_MAGIC;
  str->str[0] = '\0';
  strcat(str->str, curr);
  strcat(str->str, "!");

  FbleName nm = { .name = str, .loc = FbleCopyLoc(loc) };
  size_t id = blocks->profile.size;
  FbleAppendToVector(blocks->profile, nm);
  FbleAppendToVector(blocks->stack, id);
  return id;
}

/**
 * Enters a new profiling block.
 *
 * @param blocks  The blocks stack.
 * @param name  Name to add to the current block path for naming the new block.
 * @param loc  The location of the block.
 * @param scope  Where to add the ENTER_BLOCK instruction to.
 * @param replace  If true, emit a REPLACE_BLOCK instruction instead of ENTER_BLOCK.
 *
 * @sideeffects
 *   Adds a new block to the blocks stack. Change the current block to the new
 *   block. Outputs an ENTER_BLOCK instruction to instrs. The block should be
 *   exited when no longer in scope using ExitBlock.
 */
static void EnterBlock(Blocks* blocks, FbleName name, FbleLoc loc, Scope* scope, bool replace)
{
  FbleBlockId id = PushBlock(blocks, name, loc);
  AppendProfileOp(scope, replace ? FBLE_PROFILE_REPLACE_OP : FBLE_PROFILE_ENTER_OP, id);
}

/**
 * Pops the current profiling block frame.
 *
 * @param blocks  The blocks stack.
 *
 * @sideeffects
 *   Pops the top block frame off the blocks stack.
 */
static void PopBlock(Blocks* blocks)
{
  assert(blocks->stack.size > 0);
  blocks->stack.size--;
}

/**
 * Exits the current profiling block frame.
 *
 * @param blocks  The blocks stack.
 * @param scope  Where to append the profile exit op.
 * @param exit  Whether the frame has already been exited.
 *
 * @sideeffects
 *   Pops the top block frame off the blocks stack.
 *   profile exit op to the scope if exit is false.
 */
static void ExitBlock(Blocks* blocks, Scope* scope, bool exit)
{
  PopBlock(blocks);
  if (!exit) {
    AppendProfileOp(scope, FBLE_PROFILE_EXIT_OP, 0);
  }
}

/**
 * Appends a return instruction to instrs if exit is true.
 *
 * @param exit  Whether we actually want to exit.
 * @param scope  The scope to append the instructions to.
 * @param result  The result to return when exiting. May be NULL.
 *
 * @sideeffects
 *   If exit is true, appends a return instruction to instrs
 */
static void CompileExit(bool exit, Scope* scope, Local* result)
{
  if (exit && result != NULL) {
    // Release any remaining local variables before returning.
    for (size_t i = 0; i < scope->locals.size; ++i) {
      Local* local = scope->locals.xs[i];
      if (local != NULL && local != result && local->owner == NULL) {
        AppendReleaseInstr(scope, local->var.index);
      }
    }

    if (result->var.tag != FBLE_LOCAL_VAR || result->owner != NULL) {
      AppendRetainInstr(scope, result->var);
    }

    FbleReturnInstr* return_instr = FbleAllocInstr(FbleReturnInstr, FBLE_RETURN_INSTR);
    return_instr->result = result->var;
    AppendInstr(scope, &return_instr->_base);
  }
}

/**
 * Compiles the given expression.
 *
 * Returns the local variable that will hold the result of the expression and
 * generates instructions to compute the value of that expression at runtime.
 *
 * @param blocks  The blocks stack.
 * @param stmt  True if this marks the beginning of a statement, for debug
 *   purposes.
 * @param exit  If true, generate instructions to exit the current scope.
 * @param scope  The list of variables in scope.
 * @param v  The type checked expression to compile.
 *
 * @returns
 *   The local of the compiled expression.
 *
 * @sideeffects
 * * Updates the blocks stack with with compiled block information.
 * * Appends instructions to the scope for executing the given expression.
 *   There is no gaurentee about what instructions have been appended to
 *   the scope if the expression fails to compile.
 * * The caller should call ReleaseLocal when the returned results are no
 *   longer needed. Note that FreeScope calls ReleaseLocal for all locals
 *   allocated to the scope, so that can also be used to clean up the local.
 */
static Local* CompileExpr(Blocks* blocks, bool stmt, bool exit, Scope* scope, FbleTc* v)
{
  if (stmt) {
    FbleStatementDebugInfo* info = FbleAlloc(FbleStatementDebugInfo);
    info->_base.tag = FBLE_STATEMENT_DEBUG_INFO;
    info->_base.next = NULL;
    info->loc = FbleCopyLoc(v->loc);
    AppendDebugInfo(scope, &info->_base);
  }

  switch (v->tag) {
    case FBLE_TYPE_VALUE_TC: {
      Local* local = NewLocal(scope, NULL);
      FbleTypeInstr* instr = FbleAllocInstr(FbleTypeInstr, FBLE_TYPE_INSTR);
      instr->dest = local->var.index;
      AppendInstr(scope, &instr->_base);
      CompileExit(exit, scope, local);
      return local;
    }

    case FBLE_VAR_TC: {
      FbleVarTc* var_tc = (FbleVarTc*)v;
      Local* local = GetVar(scope, var_tc->var);
      local->refcount++;
      CompileExit(exit, scope, local);
      return local;
    }

    case FBLE_LET_TC: {
      FbleLetTc* let_tc = (FbleLetTc*)v;

      size_t base_index = scope->vars.size;

      Local* vars[let_tc->bindings.size];
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        vars[i] = NULL;
        if (let_tc->recursive) {
          vars[i] = NewLocal(scope, NULL);
          FbleRefValueInstr* ref_instr = FbleAllocInstr(FbleRefValueInstr, FBLE_REF_VALUE_INSTR);
          ref_instr->dest = vars[i]->var.index;
          AppendInstr(scope, &ref_instr->_base);
        }
        PushVar(scope, let_tc->bindings.xs[i].name, vars[i]);
      }

      // Compile the values of the variables.
      Local* defs[let_tc->bindings.size];
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        EnterBlock(blocks, let_tc->bindings.xs[i].name, let_tc->bindings.xs[i].loc, scope, false);
        defs[i] = CompileExpr(blocks, false, false, scope, let_tc->bindings.xs[i].tc);
        ExitBlock(blocks, scope, false);
      }

      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        if (let_tc->recursive) {
          FbleRefDefInstr* ref_def_instr = FbleAllocInstr(FbleRefDefInstr, FBLE_REF_DEF_INSTR);
          ref_def_instr->loc = FbleCopyLoc(let_tc->bindings.xs[i].name.loc);
          ref_def_instr->ref = vars[i]->var.index;
          ref_def_instr->value = defs[i]->var;
          AppendInstr(scope, &ref_def_instr->_base);
        }
        SetVar(scope, base_index + i, let_tc->bindings.xs[i].name, defs[i]);
      }

      Local* body = CompileExpr(blocks, true, exit, scope, let_tc->body);

      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        PopVar(scope, exit);
      }

      return body;
    }

    case FBLE_STRUCT_VALUE_TC: {
      FbleStructValueTc* struct_tc = (FbleStructValueTc*)v;

      size_t argc = struct_tc->fields.size;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(blocks, false, false, scope, struct_tc->fields.xs[i]);
      }

      Local* local = NewLocal(scope, NULL);
      FbleStructValueInstr* struct_instr = FbleAllocInstr(FbleStructValueInstr, FBLE_STRUCT_VALUE_INSTR);
      struct_instr->dest = local->var.index;
      FbleInitVector(struct_instr->args);
      AppendInstr(scope, &struct_instr->_base);
      CompileExit(exit, scope, local);

      for (size_t i = 0; i < argc; ++i) {
        FbleAppendToVector(struct_instr->args, args[i]->var);
        ReleaseLocal(scope, args[i], exit);
      }

      return local;
    }

    case FBLE_STRUCT_COPY_TC: {
      FbleStructCopyTc* struct_copy = (FbleStructCopyTc*)v;

      Local* source = CompileExpr(blocks, false, false, scope, struct_copy->source);

      size_t argc = struct_copy->fields.size;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        if (struct_copy->fields.xs[i]) {
          args[i] = CompileExpr(blocks, false, false, scope, struct_copy->fields.xs[i]);
        } else {
          args[i] = NewLocal(scope, source);
          FbleAccessInstr* access = FbleAllocInstr(FbleAccessInstr, FBLE_STRUCT_ACCESS_INSTR);
          access->obj = source->var;
          access->tag = i;
          access->loc = FbleCopyLoc(struct_copy->_base.loc);
          access->dest = args[i]->var.index;
          AppendInstr(scope, &access->_base);
        }
      }

      Local* local = NewLocal(scope, NULL);
      FbleStructValueInstr* struct_instr = FbleAllocInstr(FbleStructValueInstr, FBLE_STRUCT_VALUE_INSTR);
      struct_instr->dest = local->var.index;
      FbleInitVector(struct_instr->args);
      AppendInstr(scope, &struct_instr->_base);
      CompileExit(exit, scope, local);

      for (size_t i = 0; i < argc; ++i) {
        FbleAppendToVector(struct_instr->args, args[i]->var);
        ReleaseLocal(scope, args[i], exit);
      }
      ReleaseLocal(scope, source, exit);

      return local;
    }

    case FBLE_UNION_VALUE_TC: {
      FbleUnionValueTc* union_tc = (FbleUnionValueTc*)v;
      Local* arg = CompileExpr(blocks, false, false, scope, union_tc->arg);

      Local* local = NewLocal(scope, NULL);
      FbleUnionValueInstr* union_instr = FbleAllocInstr(FbleUnionValueInstr, FBLE_UNION_VALUE_INSTR);
      union_instr->tag = union_tc->tag;
      union_instr->arg = arg->var;
      union_instr->dest = local->var.index;
      AppendInstr(scope, &union_instr->_base);
      CompileExit(exit, scope, local);
      ReleaseLocal(scope, arg, exit);
      return local;
    }

    case FBLE_UNION_SELECT_TC: {
      FbleUnionSelectTc* select_tc = (FbleUnionSelectTc*)v;
      Local* condition = CompileExpr(blocks, false, false, scope, select_tc->condition);

      FbleUnionSelectInstr* select_instr = FbleAllocInstr(FbleUnionSelectInstr, FBLE_UNION_SELECT_INSTR);
      select_instr->loc = FbleCopyLoc(select_tc->_base.loc);
      select_instr->condition = condition->var;
      select_instr->num_tags = select_tc->num_tags;
      FbleInitVector(select_instr->targets);
      AppendInstr(scope, &select_instr->_base);

      // TODO: Could we arrange for the branches to put their value in the
      // target directly instead of in some cases allocating a new local and
      // then copying that to target?
      Local* select_result = exit ? NULL : NewLocal(scope, NULL);
      FbleGotoInstr* exit_gotos[select_tc->targets.size];

      // Non-default branches.
      for (size_t i = 0; i < select_tc->targets.size; ++i) {
        scope->active_profile_ops = &scope->pending_profile_ops;

        FbleBranchTarget* tgt = FbleExtendVector(select_instr->targets);
        tgt->tag = select_tc->targets.xs[i].tag;
        tgt->target = scope->code->instrs.size;

        EnterBlock(blocks, select_tc->targets.xs[i].target.name, select_tc->targets.xs[i].target.loc, scope, exit);
        Local* result = CompileExpr(blocks, true, exit, scope, select_tc->targets.xs[i].target.tc);

        if (!exit) {
          AppendRetainInstr(scope, result->var);

          FbleCopyInstr* copy = FbleAllocInstr(FbleCopyInstr, FBLE_COPY_INSTR);
          copy->source = result->var;
          copy->dest = select_result->var.index;
          AppendInstr(scope, &copy->_base);
        }
        ExitBlock(blocks, scope, exit);

        ReleaseLocal(scope, result, exit);

        if (!exit) {
          exit_gotos[i] = FbleAllocInstr(FbleGotoInstr, FBLE_GOTO_INSTR);
          exit_gotos[i]->target = -1; // Will be fixed up later.
          AppendInstr(scope, &exit_gotos[i]->_base);
        }
      }

      // Default branch
      {
        scope->active_profile_ops = &scope->pending_profile_ops;
        select_instr->default_ = scope->code->instrs.size;

        EnterBlock(blocks, select_tc->default_.name, select_tc->default_.loc, scope, exit);
        Local* result = CompileExpr(blocks, true, exit, scope, select_tc->default_.tc);

        if (!exit) {
          AppendRetainInstr(scope, result->var);

          FbleCopyInstr* copy = FbleAllocInstr(FbleCopyInstr, FBLE_COPY_INSTR);
          copy->source = result->var;
          copy->dest = select_result->var.index;
          AppendInstr(scope, &copy->_base);
        }
        ExitBlock(blocks, scope, exit);
        ReleaseLocal(scope, result, exit);

        if (!exit) {
          // Emit a nop instruction to force any profile ops to be done as
          // part of this default branch instead of for all branches.
          FbleNopInstr* nop = FbleAllocInstr(FbleNopInstr, FBLE_NOP_INSTR);
          AppendInstr(scope, &nop->_base);
        }
      }

      scope->active_profile_ops = &scope->pending_profile_ops;

      // Fix up exit gotos now that all the branch code is generated.
      if (!exit) {
        for (size_t i = 0; i < select_tc->targets.size; ++i) {
          exit_gotos[i]->target = scope->code->instrs.size;
        }
      }

      // We release the condition after the entire case block as finished
      // executing because the ReleaseLocal infra doesn't have an easy way
      // right now to allow us to release the condition at the start of each
      // branch.
      ReleaseLocal(scope, condition, exit);
      return select_result;
    }

    case FBLE_DATA_ACCESS_TC: {
      FbleDataAccessTc* access_tc = (FbleDataAccessTc*)v;
      Local* obj = CompileExpr(blocks, false, false, scope, access_tc->obj);

      FbleInstrTag tag;
      if (access_tc->datatype == FBLE_STRUCT_DATATYPE) {
        tag = FBLE_STRUCT_ACCESS_INSTR;
      } else {
        assert(access_tc->datatype == FBLE_UNION_DATATYPE);
        tag = FBLE_UNION_ACCESS_INSTR;
      }

      FbleAccessInstr* access = FbleAllocInstr(FbleAccessInstr, tag);
      access->obj = obj->var;
      access->tag = access_tc->tag;
      access->loc = FbleCopyLoc(access_tc->loc);
      AppendInstr(scope, &access->_base);

      Local* local = NewLocal(scope, obj);
      access->dest = local->var.index;

      CompileExit(exit, scope, local);
      ReleaseLocal(scope, obj, exit);
      return local;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)v;

      // We merge multiple func values into one to reduce the overhead of
      // function calls.
      //  e.g. \a -> \b -> ... ==> \a b -> ...
      // This requires rewriting references to statics and args in the bodies
      // of the functions.
      FbleVarV statics;
      FbleInitVector(statics);
      for (size_t i = 0; i < func_tc->scope.size; ++i) {
        FbleVar var = { .tag = FBLE_STATIC_VAR, .index = i };
        FbleAppendToVector(statics, var);
      }

      FbleNameV args;
      FbleInitVector(args);
      for (size_t i = 0; i < func_tc->args.size; ++i) {
        FbleAppendToVector(args, func_tc->args.xs[i]);
      }
      FbleTc* body = func_tc->body;
      FbleLoc body_loc = func_tc->body_loc;
      size_t arg_offset = 0;
      while (body->tag == FBLE_FUNC_VALUE_TC) {
        FbleFuncValueTc* ftc = (FbleFuncValueTc*)body;

        FbleVarV nstatics;
        FbleInitVector(nstatics);
        for (size_t i = 0; i < ftc->scope.size; ++i) {
          FbleVar nvar = RewriteVar(statics, arg_offset, ftc->scope.xs[i]);
          FbleAppendToVector(nstatics, nvar);
        }
        FbleFreeVector(statics);
        statics = nstatics;
        
        arg_offset = args.size;
        for (size_t i = 0; i < ftc->args.size; ++i) {
          FbleAppendToVector(args, ftc->args.xs[i]);
        }

        body = ftc->body;
        body_loc = ftc->body_loc;
      }

      body = RewriteVars(statics, arg_offset, body);
      FbleFreeVector(statics);

      FbleFuncValueInstr* instr = FbleAllocInstr(FbleFuncValueInstr, FBLE_FUNC_VALUE_INSTR);
      FbleInitVector(instr->scope);
      for (size_t i = 0; i < func_tc->scope.size; ++i) {
        Local* local = GetVar(scope, func_tc->scope.xs[i]);
        FbleAppendToVector(instr->scope, local->var);
      }

      Scope func_scope;
      FbleBlockId scope_block = PushBodyBlock(blocks, body_loc);
      assert(func_tc->scope.size == func_tc->statics.size);
      InitScope(&func_scope, &instr->code, args, func_tc->statics, scope_block, scope);
      FbleFreeVector(args);

      Local* func_result = CompileExpr(blocks, true, true, &func_scope, body);
      ExitBlock(blocks, &func_scope, true);
      ReleaseLocal(&func_scope, func_result, true);
      FreeScope(&func_scope);

      FbleFreeTc(body);

      Local* local = NewLocal(scope, NULL);
      instr->dest = local->var.index;
      AppendInstr(scope, &instr->_base);
      CompileExit(exit, scope, local);
      return local;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)v;

      // We merge multiple func applies into one to reduce the overhead of
      // function calls.
      //  e.g. f(a)(b) ==> f(a, b).

      // Find the underlying function and count total number of args.
      FbleFuncApplyTc* atc = apply_tc;
      size_t argc = 0;
      while (atc->_base.tag == FBLE_FUNC_APPLY_TC) {
        argc++;
        atc = (FbleFuncApplyTc*)atc->func;
      }
      FbleTc* func_tc = &atc->_base;

      // Compile the function.
      Local* func = CompileExpr(blocks, false, false, scope, func_tc);

      // Compile the args.
      Local* args[argc];
      size_t argi = argc;
      atc = apply_tc;
      while (atc->_base.tag == FBLE_FUNC_APPLY_TC) {
        args[--argi] = CompileExpr(blocks, false, false, scope, atc->arg);
        atc = (FbleFuncApplyTc*)atc->func;
      }

      if (exit) {
        // Take ownership of func for transfer to the tail call.
        if (func->var.tag != FBLE_LOCAL_VAR || func->owner != NULL ) {
          AppendRetainInstr(scope, func->var);
        }

        // Take ownership of args for transfer to the tail call.
        for (size_t i = 0; i < argc; ++i) {
          // We can trasfer ownership instead of take ownership if it's a
          // local variable that we haven't already transferred ownership for.
          bool transfer = args[i]->var.tag == FBLE_LOCAL_VAR
            && args[i]->owner == NULL;

          if (func->var.tag == FBLE_LOCAL_VAR
              && args[i]->var.index == func->var.index) {
            transfer = false;
          }

          for (size_t j = 0; j < i; ++j) {
            if (args[j]->var.tag == FBLE_LOCAL_VAR
                && args[i]->var.index == args[j]->var.index) {
              transfer = false;
              break;
            }
          }

          if (!transfer) {
            AppendRetainInstr(scope, args[i]->var);
          }
        }

        // Release any remaining unused locals before tail calling.
        for (size_t i = 0; i < scope->locals.size; ++i) {
          Local* local = scope->locals.xs[i];
          if (local != NULL) {
            bool used = (local == func);
            for (size_t j = 0; !used && j < argc; ++j) {
              used = (local == args[j]);
            }

            if (!used && local->owner == NULL) {
              AppendReleaseInstr(scope, local->var.index);
            }
          }
        }
      }

      Local* dest = exit ? NULL : NewLocal(scope, NULL);

      if (exit) {
        FbleTailCallInstr* call_instr = FbleAllocInstr(FbleTailCallInstr, FBLE_TAIL_CALL_INSTR);
        call_instr->loc = FbleCopyLoc(apply_tc->_base.loc);
        call_instr->func = func->var;
        FbleInitVector(call_instr->args);
        for (size_t i = 0; i < argc; ++i) {
          FbleAppendToVector(call_instr->args, args[i]->var);
        }
        AppendInstr(scope, &call_instr->_base);

        size_t tail_call_buffer_size = 2 + argc;
        if (tail_call_buffer_size > scope->code->_base.tail_call_buffer_size) {
          scope->code->_base.tail_call_buffer_size = tail_call_buffer_size;
        }
      } else {
        FbleCallInstr* call_instr = FbleAllocInstr(FbleCallInstr, FBLE_CALL_INSTR);
        call_instr->loc = FbleCopyLoc(apply_tc->_base.loc);
        call_instr->func = func->var;
        FbleInitVector(call_instr->args);
        call_instr->dest = dest->var.index;
        for (size_t i = 0; i < argc; ++i) {
          FbleAppendToVector(call_instr->args, args[i]->var);
        }
        AppendInstr(scope, &call_instr->_base);
      }

      scope->active_profile_ops = &scope->pending_profile_ops;
      ReleaseLocal(scope, func, exit);
      for (size_t i = 0; i < argc; ++i) {
        ReleaseLocal(scope, args[i], exit);
      }

      return dest;
    }

    case FBLE_LIST_TC: {
      FbleListTc* list_tc = (FbleListTc*)v;

      size_t argc = list_tc->fields.size;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(blocks, false, false, scope, list_tc->fields.xs[i]);
      }

      Local* local = NewLocal(scope, NULL);
      FbleListInstr* list_instr = FbleAllocInstr(FbleListInstr, FBLE_LIST_INSTR);
      list_instr->dest = local->var.index;
      FbleInitVector(list_instr->args);
      AppendInstr(scope, &list_instr->_base);
      CompileExit(exit, scope, local);

      for (size_t i = 0; i < argc; ++i) {
        FbleAppendToVector(list_instr->args, args[i]->var);
        ReleaseLocal(scope, args[i], exit);
      }

      return local;
    }

    case FBLE_LITERAL_TC: {
      FbleLiteralTc* literal_tc = (FbleLiteralTc*)v;

      Local* local = NewLocal(scope, NULL);
      FbleLiteralInstr* literal_instr = FbleAllocInstr(FbleLiteralInstr, FBLE_LITERAL_INSTR);
      literal_instr->dest = local->var.index;
      FbleInitVector(literal_instr->letters);
      AppendInstr(scope, &literal_instr->_base);
      CompileExit(exit, scope, local);

      for (size_t i = 0; i < literal_tc->letters.size; ++i) {
        FbleAppendToVector(literal_instr->letters, literal_tc->letters.xs[i]);
      }

      return local;
    }
  }

  FbleUnreachable("should already have returned");
  return NULL;
}

/**
 * Compiles a type-checked expression.
 *
 * @param args            Local variables to reserve for arguments.
 * @param tc              The type-checked expression to compile.
 * @param name            The name of the expression to use in profiling. Borrowed.
 * @param profile_blocks  Uninitialized vector to store profile blocks to.
 *
 * @returns The compiled program.
 *
 * @sideeffects
 * * Initializes and fills profile_blocks vector. The caller should free the
 *   elements and vector itself when no longer needed.
 * * Adds blocks to the given profile.
 * * The caller should call FbleFreeCode to release resources
 *   associated with the returned program when it is no longer needed.
 */
static FbleCode* Compile(FbleNameV args, FbleTc* tc, FbleName name, FbleNameV* profile_blocks)
{
  Blocks blocks;
  FbleInitVector(blocks.stack);
  FbleInitVector(blocks.profile);

  FbleCode* code;
  Scope scope;
  FbleBlockId scope_block = PushBlock(&blocks, name, name.loc);
  FbleNameV statics = { .size = 0, .xs = NULL };
  InitScope(&scope, &code, args, statics, scope_block, NULL);

  CompileExpr(&blocks, true, true, &scope, tc);
  ExitBlock(&blocks, &scope, true);

  FreeScope(&scope);
  assert(blocks.stack.size == 0);
  FbleFreeVector(blocks.stack);
  *profile_blocks = blocks.profile;
  return code;
}

/**
 * Compiles a single module.
 *
 * @param module  Meta info about the module and its dependencies.
 * @param tc  The typechecked value of the module.
 *
 * @returns
 *   The compiled module.
 *
 * @sideeffects
 *   The user should call FbleFreeCompiledModule on the resulting module when
 *   it is no longer needed.
 */
static FbleCompiledModule* CompileModule(FbleLoadedModule* module, FbleTc* tc)
{
  FbleCompiledModule* compiled = FbleAlloc(FbleCompiledModule);
  compiled->path = FbleCopyModulePath(module->path);
  FbleInitVector(compiled->deps);
  FbleNameV args;
  FbleInitVector(args);
  for (size_t d = 0; d < module->deps.size; ++d) {
    FbleAppendToVector(compiled->deps, FbleCopyModulePath(module->deps.xs[d]));
    FbleAppendToVector(args, FbleModulePathName(module->deps.xs[d]));
  }

  FbleName label = FbleModulePathName(module->path);
  compiled->code = Compile(args, tc, label, &compiled->profile_blocks);
  for (size_t i = 0; i < args.size; ++i) {
    FbleFreeName(args.xs[i]);
  }
  FbleFreeVector(args);
  FbleFreeName(label);
  return compiled;
}

// See documentation in fble-compile.h.
void FbleFreeCompiledModule(FbleCompiledModule* module)
{
  FbleFreeModulePath(module->path);
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleFreeModulePath(module->deps.xs[i]);
  }
  FbleFreeVector(module->deps);
  FbleFreeCode(module->code);
  for (size_t i = 0; i < module->profile_blocks.size; ++i) {
    FbleFreeName(module->profile_blocks.xs[i]);
  }
  FbleFreeVector(module->profile_blocks);

  FbleFree(module);
}

// See documentation in fble-compile.h.
void FbleFreeCompiledProgram(FbleCompiledProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleFreeCompiledModule(program->modules.xs[i]);
    }
    FbleFreeVector(program->modules);
    FbleFree(program);
  }
}

// See documentation in fble-compile.h.
FbleCompiledModule* FbleCompileModule(FbleLoadedProgram* program)
{
  FbleTc* tc = FbleTypeCheckModule(program);
  if (!tc) {
    return NULL;
  }

  assert(program->modules.size > 0);
  FbleLoadedModule* module = program->modules.xs + program->modules.size - 1;
  FbleCompiledModule* compiled = CompileModule(module, tc);

  FbleFreeTc(tc);
  return compiled;
}

// See documentation in fble-compile.h.
FbleCompiledProgram* FbleCompileProgram(FbleLoadedProgram* program)
{
  FbleTc** typechecked = FbleTypeCheckProgram(program);
  if (!typechecked) {
    return NULL;
  }

  FbleCompiledProgram* compiled = FbleAlloc(FbleCompiledProgram);
  FbleInitVector(compiled->modules);

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleLoadedModule* module = program->modules.xs + i;
    FbleAppendToVector(compiled->modules, CompileModule(module, typechecked[i]));
    FbleFreeTc(typechecked[i]);
  }
  FbleFree(typechecked);
  return compiled;
}
