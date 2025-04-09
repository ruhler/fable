/**
 * @file compile.c
 *  Compile FbleTc* abstract syntax into FbleCode* bytecode.
 */

#include <fble/fble-generate.h>

#include <assert.h>   // for assert
#include <string.h>   // for strlen, strcat
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>     // for FbleAlloc, etc.
#include <fble/fble-vector.h>    // for FbleInitVector, etc.

#include "code.h"
#include "tc.h"
#include "typecheck.h"
#include "unreachable.h"

typedef struct Local Local;

/**
 * @struct[LocalV] Vector of pointers to locals.
 *  @field[size_t][size] Number of elements.
 *  @field[Local**][xs] The elements.
 */
typedef struct {
  size_t size;
  Local** xs;
} LocalV;

/**
 * @struct[Local] Info about a value available in the stack frame.
 *  @field[FbleVar][var] The variable.
 *  @field[size_t][refcount] The number of references to the local.
 */
struct Local {
  FbleVar var;
  size_t refcount;
};

/**
 * @struct[Scope] Scope of variables visible during compilation.
 *  @field[LocalV][statics]
 *   Variables captured from the parent scope. Owned by the scope.
 *  @field[LocalV][args]
 *   Arguments to the function. Owns the Locals.
 *  @field[LocalV][vars]
 *   Stack of local variables in scope order. Entries may be NULL. Owns the
 *   Locals.
 *  @field[LocalV][locals]
 *   Local values. Entries may be NULL to indicate a free slot. Owns the
 *   Locals.
 *  @field[FbleCode*][code] The instruction block for this scope.
 *  @field[FbleDebugInfo*][pending_debug_info]
 *   Debug info to apply before the next instruction to be added.
 *  @field[FbleProfileOp*][pending_profile_ops]
 *   Profiling ops associated with the next instruction to be added.
 *  @field[FbleProfileOp**][active_profile_ops]
 *   The currently active set of profiling ops.
 *  
 *   New ops should be added to this list to be coalesced together where
 *   possible. This should be reset to point to pending_profile_ops at the
 *   start of any new basic blocks.
 *  @field[Scope*][parent] The parent of this scope. May be NULL.
 */
typedef struct Scope {
  LocalV statics;
  LocalV args;
  LocalV vars;
  LocalV locals;
  FbleCode* code;
  FbleDebugInfo* pending_debug_info;
  FbleProfileOp* pending_profile_ops;
  FbleProfileOp** active_profile_ops;
  struct Scope* parent;
} Scope;

static void FreeLocal(Local* local);
static Local* NewLocal(Scope* scope);
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
static void AppendDebugInfo(Scope* scope, FbleDebugInfo* info);
static void AppendProfileOp(Scope* scope, FbleProfileOpTag tag, size_t arg);

/**
 * @struct[Blocks] Stack of block frames for the current profiling block.
 *  @field[FbleBlockIdV][stack]
 *   The stack of black frames representing the current location.
 *  @field[FbleNameV][profile] The names of profile blocks to append to.
 */
typedef struct {
  FbleBlockIdV stack;
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
static void CompileModule(FbleModule* module, FbleTc* tc);

/**
 * @func[FreeLocal] Frees memory assicoated with a local.
 *  @arg[Local*][local] The local to free.
 *
 *  @sideeffects
 *   Frees memory associated with local.
 */
static void FreeLocal(Local* local)
{
  FbleFree(local);
}

/**
 * @func[NewLocal]
 * @ Allocates space for an anonymous local variable on the stack frame.
 *  @arg[Scope*][scope] The scope to allocate the local on.
 *
 *  @returns[Local*]
 *   A newly allocated local.
 *
 *  @sideeffects
 *   Allocates a space on the scope's locals for the local. The local should
 *   be freed with ReleaseLocal when no longer in use.
 */
static Local* NewLocal(Scope* scope)
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

  scope->locals.xs[index] = local;
  return local;
}

/**
 * @func[ReleaseLocal]
 * @ Decrements the reference count on a local and free it if appropriate.
 *  @arg[Scope*][scope] The scope that the local belongs to
 *  @arg[Local*][local] The local to release. May be NULL.
 *  @arg[bool][exit] Whether the stack frame has already been exited or not.
 *
 *  @sideeffects
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
    scope->locals.xs[local->var.index] = NULL;
    FreeLocal(local);
  }
}

/**
 * @func[PushVar] Pushes a variable onto the current scope.
 *  @arg[Scope*][scope] The scope to push the variable on to.
 *  @arg[FbleName][name] The name of the variable. Borrowed.
 *  @arg[Local*][local] The local address of the variable. Consumed.
 *
 *  @sideeffects
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
 * @func[PopVar] Pops a var off the given scope.
 *  @arg[Scope*][scope] The scope to pop from.
 *  @arg[bool][exit] Whether the stack frame has already been exited or not.
 *
 *  @sideeffects
 *   Pops the top var off the scope.
 */
static void PopVar(Scope* scope, bool exit)
{
  scope->vars.size--;
  Local* var = scope->vars.xs[scope->vars.size];
  ReleaseLocal(scope, var, exit);
}

/**
 * @func[GetVar] Lookup a var in the given scope.
 *  @arg[Scope*][scope] The scope to look in.
 *  @arg[FbleVar][var] The variable to look up.
 *
 *  @returns[Local*]
 *   The variable from the scope. The variable is owned by the scope and
 *   remains valid until either PopVar is called or the scope is finished.
 *
 *  @sideeffects
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
 * @func[SetVar] Changes the value of a variable in scope.
 *  @arg[Scope*][scope] Scope of variables
 *  @arg[size_t][index] The index of the local variable to change
 *  @arg[FbleName][name] The name of the variable.
 *  @arg[Local*][local] The new value for the local variable
 *
 *  @sideeffects
 *   @i Frees the existing local value of the variable.
 *   @i Sets the value of the variable to the given local.
 *   @i Takes ownership of the given local.
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

    case FBLE_UNDEF_TC: {
      FbleUndefTc* undef_tc = (FbleUndefTc*)tc;
      FbleUndefTc* ntc = FbleNewTc(FbleUndefTc, FBLE_UNDEF_TC, tc->loc);
      ntc->name = FbleCopyName(undef_tc->name);
      ntc->body = RewriteVars(statics, arg_offset, undef_tc->body);
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
      ntc->tagwidth = uv->tagwidth;
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
      ntc->fieldc = v->fieldc;
      ntc->tagwidth = v->tagwidth;
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
 * @func[InitScope] Initializes a new scope.
 *  @arg[Scope*][scope] The scope to initialize.
 *  @arg[FbleCode**][code]
 *   A pointer to store the allocated code block for this scope.
 *  @arg[FbleNameV][args]
 *   The arguments to the function the scope is for. Borrowed.
 *  @arg[FbleNameV][statics]
 *   Static variables captured by the function. Borrowed.
 *  @arg[FbleBlockId][block]
 *   The profile block id to enter when executing this scope.
 *  @arg[Scope*][parent]
 *   The parent of the scope to initialize. May be NULL.
 *
 *  @sideeffects
 *   @item
 *    Initializes scope based on parent. FreeScope should be called to free the
 *    allocations for scope. The lifetimes of the code block and the parent
 *    scope must exceed the lifetime of this scope.
 *   @item
 *    The caller is responsible for calling FbleFreeCode on *code when it is no
 *    longer needed.
 */
static void InitScope(Scope* scope, FbleCode** code, FbleNameV args, FbleNameV statics, FbleBlockId block, Scope* parent)
{
  FbleInitVector(scope->statics);
  for (size_t i = 0; i < statics.size; ++i) {
    Local* local = FbleAlloc(Local);
    local->var.tag = FBLE_STATIC_VAR;
    local->var.index = i;
    local->refcount = 1;
    FbleAppendToVector(scope->statics, local);
  }

  FbleInitVector(scope->args);
  for (size_t i = 0; i < args.size; ++i) {
    Local* local = FbleAlloc(Local);
    local->var.tag = FBLE_ARG_VAR;
    local->var.index = i;
    local->refcount = 1;
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
 * @func[FreeScope] Free memory associated with a Scope.
 *  @arg[Scope*][scope] The scope to finish.
 *
 *  @sideeffects
 *   Frees memory associated with scope.
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
 * @func[AppendInstr]
 * @ Appends an instruction to the code block for the given scope.
 *  @arg[Scope*][scope] The scope to append the instruction to.
 *  @arg[FbleInstr*][instr] The instruction to append.
 *
 *  @sideeffects
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
 * @func[AppendDebugInfo]
 * @ Appends a single debug info entry to the code block for the given scope.
 *  @arg[Scope*][scope] The scope to append the instruction to.
 *  @arg[FbleDebugInfo*][info]
 *   The debug info entry whose next field must be NULL. Consumed.
 *
 *  @sideeffects
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
 * @func[AppendProfileOp]
 * @ Appends a profile op to the code block for the given scope.
 *  @arg[Scope*][scope] The scope to append the instruction to.
 *  @arg[FbleProfileOpTag][tag] The tag of the profile op to insert.
 *  @arg[size_t][arg] The argument to the profiling op if relevant.
 *
 *  @sideeffects
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
 * @func[PushBlock] Pushes a new profiling block onto the block stack.
 *  @arg[Blocks*][blocks] The blocks stack.
 *  @arg[FbleName][name]
 *   Name to add to the current block path for naming the new block.
 *  @arg[FbleLoc][loc] The location of the block.
 *
 *  @returns[FbleBlockId]
 *   The id of the newly pushed profiling block.
 *
 *  @sideeffects
 *   @i Pushes a new block to the blocks stack.
 *   @item
 *    The block should be popped from the stack using ExitBlock or one of the
 *    other functions that exit a block when no longer needed.
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
 * @func[PushBodyBlock] Adds a new body profiling block to the block stack.
 *  This is used for the body of functions and processes that are executed
 *  when they are called, not when they are defined.
 *
 *  @arg[Blocks*][blocks] The blocks stack.
 *  @arg[FbleLoc][loc] The location of the new block.
 *
 *  @returns[FbleBlockId]
 *   The id of the newly pushed block.
 *
 *  @sideeffects
 *   @i Adds a new block to the blocks stack.
 *   @item
 *    The block should be popped from the stack using ExitBlock or one of the
 *    other functions that exit a block when no longer needed.
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
 * @func[EnterBlock] Enters a new profiling block.
 *  @arg[Blocks*][blocks] The blocks stack.
 *  @arg[FbleName][name]
 *   Name to add to the current block path for naming the new block.
 *  @arg[FbleLoc][loc] The location of the block.
 *  @arg[Scope*][scope] Where to add the ENTER_BLOCK instruction to.
 *  @arg[bool][replace]
 *   If true, emit a REPLACE_BLOCK instruction instead of ENTER_BLOCK.
 *
 *  @sideeffects
 *   Adds a new block to the blocks stack. Change the current block to the new
 *   block. Outputs an ENTER_BLOCK instruction to instrs. The block should be
 *   exited when no longer in scope using ExitBlock.
 */
static void EnterBlock(Blocks* blocks, FbleName name, FbleLoc loc, Scope* scope, bool replace)
{
  FbleBlockId id = PushBlock(blocks, name, loc);
  AppendProfileOp(scope,
      replace ? FBLE_PROFILE_REPLACE_OP : FBLE_PROFILE_ENTER_OP,
      id - scope->code->profile_block_id);
}

/**
 * @func[PopBlock] Pops the current profiling block frame.
 *  @arg[Blocks*][blocks] The blocks stack.
 *
 *  @sideeffects
 *   Pops the top block frame off the blocks stack.
 */
static void PopBlock(Blocks* blocks)
{
  assert(blocks->stack.size > 0);
  blocks->stack.size--;
}

/**
 * @func[ExitBlock] Exits the current profiling block frame.
 *  @arg[Blocks*][blocks] The blocks stack.
 *  @arg[Scope*][scope] Where to append the profile exit op.
 *  @arg[bool][exit] Whether the frame has already been exited.
 *
 *  @sideeffects
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
 * @func[CompileExit] Appends a return instruction to instrs if exit is true.
 *  @arg[bool][exit] Whether we actually want to exit.
 *  @arg[Scope*][scope] The scope to append the instructions to.
 *  @arg[Local*][result] The result to return when exiting. May be NULL.
 *
 *  @sideeffects
 *   If exit is true, appends a return instruction to instrs
 */
static void CompileExit(bool exit, Scope* scope, Local* result)
{
  if (exit && result != NULL) {
    FbleReturnInstr* return_instr = FbleAllocInstr(FbleReturnInstr, FBLE_RETURN_INSTR);
    return_instr->result = result->var;
    AppendInstr(scope, &return_instr->_base);
  }
}

/**
 * @func[CompileExpr] Compiles the given expression.
 *  Returns the local variable that will hold the result of the expression and
 *  generates instructions to compute the value of that expression at runtime.
 *
 *  @arg[Blocks*][blocks] The blocks stack.
 *  @arg[bool][stmt]
 *   True if this marks the beginning of a statement, for debug purposes.
 *  @arg[bool][exit] If true, generate instructions to exit the current scope.
 *  @arg[Scope*][scope] The list of variables in scope.
 *  @arg[FbleTc*][v] The type checked expression to compile.
 *
 *  @returns[Local*]
 *   The local of the compiled expression.
 *
 *  @sideeffects
 *   @i Updates the blocks stack with with compiled block information.
 *   @item
 *    Appends instructions to the scope for executing the given expression.
 *    There is no gaurentee about what instructions have been appended to the
 *    scope if the expression fails to compile.
 *   @item
 *    The caller should call ReleaseLocal when the returned results are no
 *    longer needed. Note that FreeScope calls ReleaseLocal for all locals
 *    allocated to the scope, so that can also be used to clean up the local.
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
      Local* local = NewLocal(scope);
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

      Local* decl = NULL;
      if (let_tc->recursive) {
        decl = NewLocal(scope);
        FbleRecDeclInstr* decl_instr = FbleAllocInstr(FbleRecDeclInstr, FBLE_REC_DECL_INSTR);
        decl_instr->dest = decl->var.index;
        decl_instr->n = let_tc->bindings.size;
        AppendInstr(scope, &decl_instr->_base);
      }

      Local* vars[let_tc->bindings.size];
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        vars[i] = NULL;
        if (let_tc->recursive) {
          vars[i] = NewLocal(scope);

          FbleAccessInstr* access = FbleAllocInstr(FbleAccessInstr, FBLE_STRUCT_ACCESS_INSTR);
          access->obj = decl->var;
          access->fieldc = let_tc->bindings.size;
          access->tag = i;
          access->loc = FbleCopyLoc(let_tc->bindings.xs[i].loc);
          access->dest = vars[i]->var.index;
          AppendInstr(scope, &access->_base);
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

      if (let_tc->recursive) {
        // Assemble all the definitions into a single struct value.
        Local* defn = NewLocal(scope);
        FbleStructValueInstr* struct_instr = FbleAllocInstr(FbleStructValueInstr, FBLE_STRUCT_VALUE_INSTR);
        struct_instr->dest = defn->var.index;
        FbleInitVector(struct_instr->args);
        AppendInstr(scope, &struct_instr->_base);

        for (size_t i = 0; i < let_tc->bindings.size; ++i) {
          FbleAppendToVector(struct_instr->args, defs[i]->var);
        }

        // Call FbleDefineRecursiveValues.
        FbleRecDefnInstr* defn_instr = FbleAllocInstr(FbleRecDefnInstr, FBLE_REC_DEFN_INSTR);
        defn_instr->decl = decl->var.index;
        defn_instr->defn = defn->var.index;
        FbleInitVector(defn_instr->locs);
        for (size_t i = 0; i < let_tc->bindings.size; ++i) {
          FbleAppendToVector(defn_instr->locs, FbleCopyLoc(FbleCopyLoc(let_tc->bindings.xs[i].name.loc)));
        }
        AppendInstr(scope, &defn_instr->_base);
        ReleaseLocal(scope, defn, exit);

        // Write back the final results.
        for (size_t i = 0; i < let_tc->bindings.size; ++i) {
          FbleAccessInstr* access = FbleAllocInstr(FbleAccessInstr, FBLE_STRUCT_ACCESS_INSTR);
          access->obj = decl->var;
          access->fieldc = let_tc->bindings.size;
          access->tag = i;
          access->loc = FbleCopyLoc(let_tc->bindings.xs[i].loc);
          access->dest = vars[i]->var.index;
          AppendInstr(scope, &access->_base);
        }
        ReleaseLocal(scope, decl, exit);
      } else {
        for (size_t i = 0; i < let_tc->bindings.size; ++i) {
          SetVar(scope, base_index + i, let_tc->bindings.xs[i].name, defs[i]);
        }
      }

      Local* body = CompileExpr(blocks, true, exit, scope, let_tc->body);

      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        PopVar(scope, exit);
      }

      return body;
    }

    case FBLE_UNDEF_TC: {
      FbleUndefTc* undef_tc = (FbleUndefTc*)v;

      Local* var = NewLocal(scope);
      FbleUndefInstr* undef_instr = FbleAllocInstr(FbleUndefInstr, FBLE_UNDEF_INSTR);
      undef_instr->dest = var->var.index;
      AppendInstr(scope, &undef_instr->_base);
      PushVar(scope, undef_tc->name, var);
      Local* body = CompileExpr(blocks, true, exit, scope, undef_tc->body);
      PopVar(scope, exit);
      return body;
    }

    case FBLE_STRUCT_VALUE_TC: {
      FbleStructValueTc* struct_tc = (FbleStructValueTc*)v;

      size_t argc = struct_tc->fields.size;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(blocks, false, false, scope, struct_tc->fields.xs[i]);
      }

      Local* local = NewLocal(scope);
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

      size_t tagwidth = 0;
      while ((1 << tagwidth) < argc) {
        tagwidth++;
      }

      for (size_t i = 0; i < argc; ++i) {
        if (struct_copy->fields.xs[i]) {
          args[i] = CompileExpr(blocks, false, false, scope, struct_copy->fields.xs[i]);
        } else {
          args[i] = NewLocal(scope);

          FbleAccessInstr* access = FbleAllocInstr(FbleAccessInstr, FBLE_STRUCT_ACCESS_INSTR);
          access->obj = source->var;
          access->fieldc = argc;
          access->tagwidth = tagwidth;
          access->tag = i;
          access->loc = FbleCopyLoc(struct_copy->_base.loc);
          access->dest = args[i]->var.index;
          AppendInstr(scope, &access->_base);
        }
      }

      Local* local = NewLocal(scope);
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

      Local* local = NewLocal(scope);
      FbleUnionValueInstr* union_instr = FbleAllocInstr(FbleUnionValueInstr, FBLE_UNION_VALUE_INSTR);
      union_instr->tagwidth = union_tc->tagwidth;
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

      size_t tagwidth = 0;
      while ((1 << tagwidth) < select_tc->num_tags) {
        tagwidth++;
      }

      FbleUnionSelectInstr* select_instr = FbleAllocInstr(FbleUnionSelectInstr, FBLE_UNION_SELECT_INSTR);
      select_instr->loc = FbleCopyLoc(select_tc->_base.loc);
      select_instr->condition = condition->var;
      select_instr->tagwidth = tagwidth;
      select_instr->num_tags = select_tc->num_tags;
      FbleInitVector(select_instr->targets);
      AppendInstr(scope, &select_instr->_base);

      // TODO: Could we arrange for the branches to put their value in the
      // target directly instead of in some cases allocating a new local and
      // then copying that to target?
      Local* select_result = exit ? NULL : NewLocal(scope);
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
      access->fieldc = access_tc->fieldc;
      access->tagwidth = access_tc->tagwidth;
      access->tag = access_tc->tag;
      access->loc = FbleCopyLoc(access_tc->loc);
      AppendInstr(scope, &access->_base);

      Local* local = NewLocal(scope);
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
      instr->profile_block_offset = scope_block - scope->code->profile_block_id;
      assert(func_tc->scope.size == func_tc->statics.size);
      InitScope(&func_scope, &instr->code, args, func_tc->statics, scope_block, scope);
      FbleFreeVector(args);

      Local* func_result = CompileExpr(blocks, true, true, &func_scope, body);
      ExitBlock(blocks, &func_scope, true);
      ReleaseLocal(&func_scope, func_result, true);
      FreeScope(&func_scope);

      FbleFreeTc(body);

      Local* local = NewLocal(scope);
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

      Local* dest = exit ? NULL : NewLocal(scope);

      if (argc > scope->code->executable.max_call_args) {
        scope->code->executable.max_call_args = argc;
      }

      if (exit) {
        FbleTailCallInstr* call_instr = FbleAllocInstr(FbleTailCallInstr, FBLE_TAIL_CALL_INSTR);
        call_instr->loc = FbleCopyLoc(apply_tc->_base.loc);
        call_instr->func = func->var;
        FbleInitVector(call_instr->args);
        for (size_t i = 0; i < argc; ++i) {
          FbleAppendToVector(call_instr->args, args[i]->var);
        }

        AppendInstr(scope, &call_instr->_base);
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

      Local* local = NewLocal(scope);
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

      Local* local = NewLocal(scope);
      FbleLiteralInstr* literal_instr = FbleAllocInstr(FbleLiteralInstr, FBLE_LITERAL_INSTR);
      literal_instr->dest = local->var.index;
      literal_instr->tagwidth = literal_tc->tagwidth;
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
 * @func[Compile] Compiles a type-checked expression.
 *  @arg[FbleNameV][args] Local variables to reserve for arguments.
 *  @arg[FbleTc*][tc] The type-checked expression to compile.
 *  @arg[FbleName][name]
 *   The name of the expression to use in profiling. Borrowed.
 *  @arg[FbleNameV*][profile_blocks]
 *   Uninitialized vector to store profile blocks to.
 *
 *  @returns[FbleCode*] The compiled program.
 *
 *  @sideeffects
 *   @item
 *    Initializes and fills profile_blocks vector. The caller should free the
 *    elements and vector itself when no longer needed.
 *   @i Adds blocks to the given profile.
 *   @item
 *    The caller should call FbleFreeCode to release resources associated with
 *    the returned program when it is no longer needed.
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
 * @func[CompileModule] Compiles a single module.
 *  @arg[FbleModule*][module]
 *   Info about the module and its dependencies.
 *  @arg[FbleTc*][tc] The typechecked value of the module.
 *
 *  @sideeffects
 *   @i Updates the code for the module with the compiled code.
 */
static void CompileModule(FbleModule* module, FbleTc* tc)
{
  if (module->value == NULL) {
    // There's nothing to compile. This can happen for builtin modules, for
    // example.
    return;
  }

  assert(module->code == NULL && "module already compiled?");
  assert(module->profile_blocks.size == 0 && "module already compiled?");

  FbleNameV args;
  FbleInitVector(args);
  for (size_t d = 0; d < module->link_deps.size; ++d) {
    FbleAppendToVector(args, FbleModulePathName(module->link_deps.xs[d]));
  }

  FbleName label = FbleModulePathName(module->path);
  module->code = Compile(args, tc, label, &module->profile_blocks);
  for (size_t i = 0; i < args.size; ++i) {
    FbleFreeName(args.xs[i]);
  }
  FbleFreeVector(args);
  FbleFreeName(label);
}

// See documentation in fble-compile.h.
bool FbleCompileModule(FbleProgram* program)
{
  FbleTc* tc = FbleTypeCheckModule(program);
  if (!tc) {
    return false;
  }

  assert(program->modules.size > 0);
  FbleModule* module = program->modules.xs + program->modules.size - 1;
  CompileModule(module, tc);

  FbleFreeTc(tc);
  return true;
}

// See documentation in fble-compile.h.
bool FbleCompileProgram(FbleProgram* program)
{
  FbleTc** typechecked = FbleTypeCheckProgram(program);
  if (!typechecked) {
    return false;
  }

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleModule* module = program->modules.xs + i;
    CompileModule(module, typechecked[i]);
    FbleFreeTc(typechecked[i]);
  }
  FbleFree(typechecked);
  return true;
}
