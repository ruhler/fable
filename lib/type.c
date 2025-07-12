/**
 * @file type.c
 *  FbleType routines.
 *
 *  Design notes on types:
 *
 *  @item
 *   Instances of Type represent both unevaluated and evaluated versions of
 *   the type. We use the unevaluated versions of the type when printing error
 *   messages and as a stable reference to a type before and after evaluation.
 *  @item
 *   Cycles are allowed in the Type data structure, to represent recursive
 *   types. Every cycle is guaranteed to go through a Var type.
 *  @item
 *   Types are evaluated as they are constructed.
 *  @item
 *   FBLE_TYPE_TYPE is handled specially: we propagate FBLE_TYPE_TYPE up to
 *   the top of the type during construction rather than save the unevaluated
 *   version of a typeof.
 */

#include "type.h"

#include <assert.h>   // for assert

#include <fble/fble-module-path.h>
#include <fble/fble-vector.h>

#include "type-heap.h"
#include "unreachable.h"

/**
 * @struct[TypeList] Linked list of types.
 *  @field[FbleType*][type] The current element.
 *  @field[TypeList*][next] The rest of the elements.
 */
typedef struct TypeList {
  FbleType* type;
  struct TypeList* next;
} TypeList;

/**
 * @struct[TypePairs] A set of pairs of types.
 *  @field[FbleType*][a] The first type of the current pair.
 *  @field[FbleType*][b] The second type of the current pair.
 *  @field[TypePairs*][next] The rest of the pairs in the set.
 */
typedef struct TypePairs {
  FbleType* a;
  FbleType* b;
  struct TypePairs* next;
} TypePairs;

static FbleKind* LevelAdjustedKind(FbleKind* kind, int increment);

static void Ref(FbleTypeHeap* heap, FbleType* src, FbleType* dst);

static FbleType* Normal(FbleTypeHeap* heap, FbleType* type, TypeList* normalizing);
static bool HasParam(FbleTypeAssignmentV vars, FbleType* type);
static bool HasParam_(FbleTypeAssignmentV vars, FbleType* param);
static FbleType* Subst(FbleTypeHeap* heap, FbleTypeAssignmentV vars, FbleType* src, TypePairs* tps);
static bool TypesEqual(FbleTypeHeap* heap, FbleTypeAssignmentV vars, FbleType* a, FbleType* b, TypePairs* eq);

/**
 * @func[LevelAdjustedKind]
 * @ Constructs a level adjusted version of the given kind.
 *  @arg[FbleKind*][kind] The kind to use as the basis.
 *  @arg[FbleKind*][increment]
 *   The kind level increment amount. May be negative to decrease the kind
 *   level.
 *
 *  @returns[FbleKind*]
 *   A new kind that is the same as the given kind except with level
 *   incremented by the given increment.
 *
 *  @sideeffects
 *   @item
 *    The caller is responsible for calling FbleFreeKind on the returned kind
 *    when it is no longer needed. This function does not take ownership of
 *    the given kind.
 *   @item
 *    Behavior is undefined if the increment causes the resulting kind level
 *    to be less than 0.
 */
static FbleKind* LevelAdjustedKind(FbleKind* kind, int increment)
{
  switch (kind->tag) {
    case FBLE_BASIC_KIND: {
      FbleBasicKind* basic = (FbleBasicKind*)kind;
      assert((int)basic->level + increment >= 0);

      FbleBasicKind* adjusted = FbleAlloc(FbleBasicKind);
      adjusted->_base.tag = FBLE_BASIC_KIND;
      adjusted->_base.loc = FbleCopyLoc(kind->loc);
      adjusted->_base.refcount = 1;
      adjusted->level = basic->level + increment;
      return &adjusted->_base;
    }

    case FBLE_POLY_KIND: {
      FblePolyKind* poly = (FblePolyKind*)kind;
      FblePolyKind* adjusted = FbleAlloc(FblePolyKind);
      adjusted->_base.tag = FBLE_POLY_KIND;
      adjusted->_base.loc = FbleCopyLoc(kind->loc);
      adjusted->_base.refcount = 1;
      adjusted->arg = FbleCopyKind(poly->arg);
      adjusted->rkind = LevelAdjustedKind(poly->rkind, increment);
      return &adjusted->_base;
    }
  }
  FbleUnreachable("Should never get here");
  return NULL;
}

/**
 * @func[Ref] Helper function for implementing Refs.
 *  Calls FbleTypeAddRef if the dst is not NULL.
 *
 *  @arg[FbleTypeHeap*][heap] The heap.
 *  @arg[FbleType*][src] The source of the reference.
 *  @arg[FbleType*][dst] The target of the reference.
 *
 *  @sideeffects
 *   If value is non-NULL, FbleTypeAddRef is called for it.
 */
static void Ref(FbleTypeHeap* heap, FbleType* src, FbleType* dst)
{
  if (dst != NULL) {
    FbleTypeAddRef(heap, src, dst);
  }
}

// See documenatation in type-heap.h
void FbleTypeRefs(FbleTypeHeap* heap, FbleType* type)
{
  switch (type->tag) {
    case FBLE_DATA_TYPE: {
      FbleDataType* dt = (FbleDataType*)type;
      for (size_t i = 0; i < dt->fields.size; ++i) {
        Ref(heap, type, dt->fields.xs[i].type);
      }
      break;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;
      Ref(heap, type, ft->arg);
      Ref(heap, type, ft->rtype);
      break;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pt = (FblePolyType*)type;
      Ref(heap, type, pt->arg);
      Ref(heap, type, pt->body);
      break;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      Ref(heap, type, pat->poly);
      Ref(heap, type, pat->arg);
      break;
    }

    case FBLE_PACKAGE_TYPE: break;

    case FBLE_PRIVATE_TYPE: {
      FblePrivateType* t = (FblePrivateType*)type;
      Ref(heap, type, t->arg);
      break;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      Ref(heap, type, var->value);
      break;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* t = (FbleTypeType*)type;
      Ref(heap, type, t->type);
      break;
    }
  }
}

// See documentation in type-heap.h
void FbleTypeOnFree(FbleType* type)
{
  FbleFreeLoc(type->loc);
  switch (type->tag) {
    case FBLE_DATA_TYPE: {
      FbleDataType* dt = (FbleDataType*)type;
      for (size_t i = 0; i < dt->fields.size; ++i) {
        FbleFreeName(dt->fields.xs[i].name);
      }
      FbleFreeVector(dt->fields);
      return;
    }

    case FBLE_FUNC_TYPE: return;
    case FBLE_POLY_TYPE: return;
    case FBLE_POLY_APPLY_TYPE: return;

    case FBLE_PACKAGE_TYPE: {
      FblePackageType* package = (FblePackageType*)type;
      FbleFreeModulePath(package->path);
      return;
    }

    case FBLE_PRIVATE_TYPE: {
      FblePrivateType* private = (FblePrivateType*)type;
      FbleFreeModulePath(private->package);
      return;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      FbleFreeKind(var->kind);
      FbleFreeName(var->name);
      return;
    }

    case FBLE_TYPE_TYPE: return;
  }

  FbleUnreachable("should never get here");
}

/**
 * @func[Normal] Computes the normal form of a type.
 *  @arg[FbleTypeHeap*][heap] Heap to use for allocations.
 *  @arg[FbleType*][type] The type to reduce.
 *  @arg[TypeList*][normalizing] The set of types currently being normalized.
 *
 *  @returns[FbleType*]
 *   The type reduced to normal form, or NULL if the type cannot be reduced to
 *   normal form.
 *
 *  @sideeffects
 *   The caller is responsible for calling FbleReleaseType on the returned type
 *   when it is no longer needed.
 */
static FbleType* Normal(FbleTypeHeap* heap, FbleType* type, TypeList* normalizing)
{
  for (TypeList* n = normalizing; n != NULL; n = n->next) {
    if (type == n->type) {
      return NULL;
    }
  }

  TypeList nn = {
    .type = type,
    .next = normalizing
  };

  switch (type->tag) {
    case FBLE_DATA_TYPE: return FbleRetainType(heap, type);
    case FBLE_FUNC_TYPE: return FbleRetainType(heap, type);

    case FBLE_POLY_TYPE: {
      FblePolyType* poly = (FblePolyType*)type;

      // eta-reduce (\x -> f x) ==> f
      FblePolyApplyType* pat = (FblePolyApplyType*)Normal(heap, poly->body, &nn);
      if (pat == NULL) {
        return NULL;
      }

      if (pat->_base.tag == FBLE_POLY_APPLY_TYPE && pat->arg == poly->arg) {
        FbleType* normal = FbleRetainType(heap, pat->poly);
        FbleReleaseType(heap, &pat->_base);
        return normal;
      }

      FbleReleaseType(heap, &pat->_base);
      return FbleRetainType(heap, type);
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      FblePolyType* poly = (FblePolyType*)Normal(heap, pat->poly, &nn);
      if (poly == NULL) {
        return NULL;
      }

      if (poly->_base.tag == FBLE_POLY_TYPE) {
        FbleTypeAssignment assign = { .var = poly->arg, .value = pat->arg };
        FbleTypeAssignmentV vars = { .size = 1, .xs = &assign };
        FbleType* subst = Subst(heap, vars, poly->body, NULL);
        FbleType* result = Normal(heap, subst, &nn);
        FbleReleaseType(heap, &poly->_base);
        FbleReleaseType(heap, subst);
        return result;
      }

      // Don't bother simplifying at all if we can't do a substituion.
      FbleReleaseType(heap, &poly->_base);
      return FbleRetainType(heap, type);
    }

    case FBLE_PACKAGE_TYPE: return FbleRetainType(heap, type);

    case FBLE_PRIVATE_TYPE: {
      FblePrivateType* private = (FblePrivateType*)type;

      if (FbleModuleBelongsToPackage(FbleTypeHeapGetContext(heap), private->package)) {
        return Normal(heap, private->arg, &nn);
      }
      return FbleRetainType(heap, type);
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      if (var->value == NULL) {
        return FbleRetainType(heap, type);
      }
      return Normal(heap, var->value, &nn);
    }

    case FBLE_TYPE_TYPE: return FbleRetainType(heap, type);
  }

  FbleUnreachable("Should never get here");
  return NULL;
}

/**
 * @func[HasParam] Check if type has any type variables in a type assignment.
 *  @arg[FbleTypeAssignmentV][vars] The type assignment.
 *  @arg[FbleType*][type] The type to check.
 *
 *  @returns[bool]
 *   True if any of the type variables in the assignment occur in type, false
 *   otherwise.
 *
 *  @sideeffects
 *   None.
 */
static bool HasParam(FbleTypeAssignmentV vars, FbleType* type)
{
  if (vars.size == 0) {
    return false;
  }

  // To break recursion, avoid visiting the same type twice.
  if (type->visiting) {
    return false;
  }
  type->visiting = true;
  bool result = HasParam_(vars, type);
  type->visiting = false;
  return result;
}

/**
 * @func[HasParam_] Check if type has any type variables in a type assignment.
 *  @arg[FbleTypeAssignmentV][vars] The type assignment.
 *  @arg[FbleType*][type] The type to check.
 *
 *  @returns[bool]
 *   True if the param occur in type, false otherwise.
 *
 *  @sideeffects
 *   None.
 */
static bool HasParam_(FbleTypeAssignmentV vars, FbleType* type)
{
  switch (type->tag) {
    case FBLE_DATA_TYPE: {
      FbleDataType* dt = (FbleDataType*)type;
      for (size_t i = 0; i < dt->fields.size; ++i) {
        if (HasParam(vars, dt->fields.xs[i].type)) {
          return true;
        }
      }
      return false;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;
      return HasParam(vars, ft->arg) || HasParam(vars, ft->rtype);
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pt = (FblePolyType*)type;

      // Remove shadowed type variables from the assignment.
      FbleTypeAssignment xs[vars.size];
      FbleTypeAssignmentV nvars = { .size = 0, .xs = xs };
      for (size_t i = 0; i < vars.size; ++i) {
        if (pt->arg != vars.xs[i].var) {
          nvars.xs[nvars.size++] = vars.xs[i];
        }
      }

      return HasParam(nvars, pt->body);
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      return HasParam(vars, pat->arg) || HasParam(vars, pat->poly);
    }

    case FBLE_PACKAGE_TYPE: {
      return false;
    }

    case FBLE_PRIVATE_TYPE: {
      FblePrivateType* pt = (FblePrivateType*)type;
      return HasParam(vars, pt->arg);
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;

      for (size_t i = 0; i < vars.size; ++i) {
        if (type == vars.xs[i].var) {
          return true;
        }
      }

      return var->value != NULL && HasParam(vars, var->value);
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* type_type = (FbleTypeType*)type;
      return HasParam(vars, type_type->type);
    }
  }

  FbleUnreachable("Should never get here");
  return false;
}

/**
 * @func[Subst] Subsititues a value for a type variable.
 *  Substitute the type arguments for the given values in the given type.
 *  This function does not attempt to evaluate the results of the
 *  substitution.
 *
 *  @arg[FbleTypeHeap*][heap] Heap to use for allocations.
 *  @arg[FbleTypeAssignmentV][vars] The types to assign.
 *  @arg[FbleType*][type] The type to substitute into.
 *  @arg[TypePairs*][tps]
 *   A map of already substituted types, to avoid infinite recursion.
 *   External callers should pass NULL.
 *
 *  @returns[FbleType*]
 *   A type with all occurrences of a type assignment var with corresponding
 *   value from the type assignments. The type may not be fully evaluated.
 *
 *  @sideeffects
 *   The caller is responsible for calling FbleReleaseType on the returned
 *   type when it is no longer needed. No new type ids are allocated by
 *   substitution.
 *
 *  Design note: The given type may have cycles. For example:
 *
 *  @code[fble] @
 *   <@>@ F@ = <@ T@> {
 *      @ X@ = +(T@ a, X@ b);
 *   };
 *   F@<Unit@>
 *
 *  To prevent infinite recursion, we use tps to record that we have already
 *  substituted @l{Unit@} for @l{T@} in @l{X@} when traversing into field 'b'
 *  of @l{X@}.
 */
static FbleType* Subst(FbleTypeHeap* heap, FbleTypeAssignmentV vars, FbleType* type, TypePairs* tps)
{
  if (!HasParam(vars, type)) {
    return FbleRetainType(heap, type);
  }

  switch (type->tag) {
    case FBLE_DATA_TYPE: {
      FbleDataType* dt = (FbleDataType*)type;
      FbleDataType* sdt = FbleNewType(heap, FbleDataType, type->tag, dt->_base.loc);
      sdt->datatype = dt->datatype;

      FbleInitVector(sdt->fields);
      for (size_t i = 0; i < dt->fields.size; ++i) {
        FbleTaggedType field = {
          .name = FbleCopyName(dt->fields.xs[i].name),
          .type = Subst(heap, vars, dt->fields.xs[i].type, tps)
        };
        FbleAppendToVector(sdt->fields, field);
        FbleTypeAddRef(heap, &sdt->_base, field.type);
        FbleReleaseType(heap, field.type);
      }
      return &sdt->_base;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;

      FbleType* sarg = Subst(heap, vars, ft->arg, tps);
      FbleType* rtype = Subst(heap, vars, ft->rtype, tps);

      FbleFuncType* sft = FbleNewType(heap, FbleFuncType, FBLE_FUNC_TYPE, ft->_base.loc);
      sft->arg = sarg;
      sft->rtype = rtype;
      FbleTypeAddRef(heap, &sft->_base, sft->arg);
      FbleTypeAddRef(heap, &sft->_base, sft->rtype);

      FbleReleaseType(heap, sft->arg);
      FbleReleaseType(heap, sft->rtype);
      return &sft->_base;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pt = (FblePolyType*)type;

      // Remove shadowed type variables from the assignment.
      FbleTypeAssignment xs[vars.size];
      FbleTypeAssignmentV nvars = { .size = 0, .xs = xs };
      for (size_t i = 0; i < vars.size; ++i) {
        if (pt->arg != vars.xs[i].var) {
          nvars.xs[nvars.size++] = vars.xs[i];
        }
      }

      FbleType* body = Subst(heap, nvars, pt->body, tps); 

      FblePolyType* spt = FbleNewType(heap, FblePolyType, FBLE_POLY_TYPE, pt->_base.loc);
      spt->arg = pt->arg;
      FbleTypeAddRef(heap, &spt->_base, spt->arg);
      spt->body = body;
      FbleTypeAddRef(heap, &spt->_base, spt->body);
      assert(spt->body->tag != FBLE_TYPE_TYPE);

      FbleReleaseType(heap, body);
      return &spt->_base;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      FbleType* poly = Subst(heap, vars, pat->poly, tps);
      FbleType* sarg = Subst(heap, vars, pat->arg, tps);

      FblePolyApplyType* spat = FbleNewType(heap, FblePolyApplyType, FBLE_POLY_APPLY_TYPE, pat->_base.loc);
      spat->poly = poly;
      FbleTypeAddRef(heap, &spat->_base, spat->poly);
      spat->arg = sarg;
      FbleTypeAddRef(heap, &spat->_base, spat->arg);
      assert(spat->poly->tag != FBLE_TYPE_TYPE);

      FbleReleaseType(heap, poly);
      FbleReleaseType(heap, sarg);
      return &spat->_base;
    }

    case FBLE_PACKAGE_TYPE: {
      FbleUnreachable("package type does not have params");
      return NULL;
    }

    case FBLE_PRIVATE_TYPE: {
      FblePrivateType* pt = (FblePrivateType*)type;
      FbleType* sarg = Subst(heap, vars, pt->arg, tps);

      FblePrivateType* spt = FbleNewType(heap, FblePrivateType, FBLE_PRIVATE_TYPE, pt->_base.loc);
      spt->package = FbleCopyModulePath(pt->package);
      spt->arg = sarg;
      FbleTypeAddRef(heap, &spt->_base, spt->arg);
      FbleReleaseType(heap, spt->arg);
      return &spt->_base;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      if (var->value == NULL) {
        for (size_t i = 0; i < vars.size; ++i) {
          if (type == vars.xs[i].var) {
            return FbleRetainType(heap, vars.xs[i].value);
          }
        }
        FbleUnreachable("unmatched var type does not have params");
        return NULL;
      }

      // Check to see if we've already done substitution on the value pointed
      // to by this ref.
      for (TypePairs* tp = tps; tp != NULL; tp = tp->next) {
        if (tp->a == var->value) {
          return FbleRetainType(heap, tp->b);
        }
      }

      FbleType* svar = FbleNewVarType(heap, type->loc, var->kind, var->name);

      TypePairs ntp = {
        .a = var->value,
        .b = svar,
        .next = tps
      };

      FbleType* value = Subst(heap, vars, var->value, &ntp);
      FbleAssignVarType(heap, svar, value);
      FbleReleaseType(heap, svar);
      return value;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* tt = (FbleTypeType*)type;

      FbleType* body = Subst(heap, vars, tt->type, tps);

      FbleTypeType* stt = FbleNewType(heap, FbleTypeType, FBLE_TYPE_TYPE, tt->_base.loc);
      stt->type = body;
      FbleTypeAddRef(heap, &stt->_base, stt->type);
      FbleReleaseType(heap, stt->type);
      return &stt->_base;
    }
  }

  FbleUnreachable("Should never get here");
  return NULL;
}

/**
 * @func[TypesEqual] Infers types and checks for type equality.
 *  @arg[FbleTypeHeap*][heap] Heap to use for allocations
 *  @arg[FbleTypeAssignmentV][vars] Variables for type inference.
 *  @arg[FbleType*][a] The first type, abstract in the type variables. Borrowed.
 *  @arg[FbleType*][b] The second type, which should be concrete. Borrowed.
 *  @arg[TypePairs*][eq]
 *   A set of pairs of types that should be assumed to be equal
 *
 *  @returns[bool]
 *   True if the first type equals the second type, false otherwise.
 *
 *  @sideeffects
 *   Sets value of assignments to type variables to make the types equal.
 */
static bool TypesEqual(FbleTypeHeap* heap, FbleTypeAssignmentV vars, FbleType* a, FbleType* b, TypePairs* eq)
{
  a = FbleNormalType(heap, a);

  // Check for type inference.
  for (size_t i = 0; i < vars.size; ++i) {
    if (a == vars.xs[i].var) {
      FbleReleaseType(heap, a);

      // We can infer the value of type a is b.
      if (vars.xs[i].value == NULL) {
        vars.xs[i].value = FbleRetainType(heap, b);
        return true;
      }

      // We should use the previously inferred value for a.
      a = FbleNormalType(heap, vars.xs[i].value);
      break;
    }
  }

  b = FbleNormalType(heap, b);

  for (TypePairs* pairs = eq; pairs != NULL; pairs = pairs->next) {
    if (a == pairs->a && b == pairs->b) {
      FbleReleaseType(heap, a);
      FbleReleaseType(heap, b);
      return true;
    }
  }

  TypePairs neq = {
    .a = a,
    .b = b,
    .next = eq,
  };

  if (a->tag != b->tag) {
    FbleReleaseType(heap, a);
    FbleReleaseType(heap, b);
    return false;
  }

  switch (a->tag) {
    case FBLE_DATA_TYPE: {
      FbleDataType* dta = (FbleDataType*)a;
      FbleDataType* dtb = (FbleDataType*)b;

      if (dta->datatype != dtb->datatype || dta->fields.size != dtb->fields.size) {
        FbleReleaseType(heap, a);
        FbleReleaseType(heap, b);
        return false;
      }

      for (size_t i = 0; i < dta->fields.size; ++i) {
        if (!FbleNamesEqual(dta->fields.xs[i].name, dtb->fields.xs[i].name)) {
          FbleReleaseType(heap, a);
          FbleReleaseType(heap, b);
          return false;
        }

        if (!TypesEqual(heap, vars, dta->fields.xs[i].type, dtb->fields.xs[i].type, &neq)) {
          FbleReleaseType(heap, a);
          FbleReleaseType(heap, b);
          return false;
        }
      }

      FbleReleaseType(heap, a);
      FbleReleaseType(heap, b);
      return true;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* fta = (FbleFuncType*)a;
      FbleFuncType* ftb = (FbleFuncType*)b;

      bool result = TypesEqual(heap, vars, fta->arg, ftb->arg, &neq)
        && TypesEqual(heap, vars, fta->rtype, ftb->rtype, &neq);

      FbleReleaseType(heap, a);
      FbleReleaseType(heap, b);
      return result;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pta = (FblePolyType*)a;
      FblePolyType* ptb = (FblePolyType*)b;

      FbleKind* ka = FbleGetKind(FbleTypeHeapGetContext(heap), pta->arg);
      FbleKind* kb = FbleGetKind(FbleTypeHeapGetContext(heap), ptb->arg);
      if (!FbleKindsEqual(ka, kb)) {
        FbleFreeKind(ka);
        FbleFreeKind(kb);
        FbleReleaseType(heap, a);
        FbleReleaseType(heap, b);
        return false;
      }
      FbleFreeKind(ka);
      FbleFreeKind(kb);
  
      TypePairs pneq = {
        .a = pta->arg,
        .b = ptb->arg,
        .next = &neq
      };
      bool result = TypesEqual(heap, vars, pta->body, ptb->body, &pneq);
      FbleReleaseType(heap, a);
      FbleReleaseType(heap, b);
      return result;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pa = (FblePolyApplyType*)a;
      FblePolyApplyType* pb = (FblePolyApplyType*)b;
      bool result = TypesEqual(heap, vars, pa->poly, pb->poly, &neq)
                 && TypesEqual(heap, vars, pa->arg, pb->arg, &neq);
      FbleReleaseType(heap, a);
      FbleReleaseType(heap, b);
      return result;
    }

    case FBLE_PACKAGE_TYPE: {
      FblePackageType* pa = (FblePackageType*)a;
      FblePackageType* pb = (FblePackageType*)b;
      bool result = FbleModulePathsEqual(pa->path, pb->path);
      FbleReleaseType(heap, a);
      FbleReleaseType(heap, b);
      return result;
    }

    case FBLE_PRIVATE_TYPE: {
      FblePrivateType* pa = (FblePrivateType*)a;
      FblePrivateType* pb = (FblePrivateType*)b;

      bool result = FbleModulePathsEqual(pa->package, pb->package)
        && TypesEqual(heap, vars, pa->arg, pb->arg, &neq);

      FbleReleaseType(heap, a);
      FbleReleaseType(heap, b);
      return result;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* va = (FbleVarType*)a;
      FbleVarType* vb = (FbleVarType*)b;

      assert(va->value == NULL && vb->value == NULL);
      FbleReleaseType(heap, a);
      FbleReleaseType(heap, b);
      return a == b;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* tta = (FbleTypeType*)a;
      FbleTypeType* ttb = (FbleTypeType*)b;
      bool result = TypesEqual(heap, vars, tta->type, ttb->type, &neq);
      FbleReleaseType(heap, a);
      FbleReleaseType(heap, b);
      return result;
    }
  }

  FbleUnreachable("Should never get here");
  FbleReleaseType(heap, a);
  FbleReleaseType(heap, b);
  return false;
}

// See documentation in type.h.
FbleKind* FbleGetKind(FbleModulePath* context, FbleType* type)
{
  switch (type->tag) {
    case FBLE_DATA_TYPE:
    case FBLE_FUNC_TYPE:
    case FBLE_PACKAGE_TYPE: {
      return FbleNewBasicKind(type->loc, 0);
    }

    case FBLE_PRIVATE_TYPE: {
      FblePrivateType* private = (FblePrivateType*)type;

      if (context == NULL || FbleModuleBelongsToPackage(context, private->package)) {
        return FbleGetKind(context, private->arg);
      }
      return FbleNewBasicKind(type->loc, 0);
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* poly = (FblePolyType*)type;
      FblePolyKind* kind = FbleAlloc(FblePolyKind);
      kind->_base.tag = FBLE_POLY_KIND;
      kind->_base.loc = FbleCopyLoc(type->loc);
      kind->_base.refcount = 1;

      // This is tricky. Consider: <@ A@> { ... }
      // poly->arg is the type A@. A@ has kind level 0, because it is the type
      // of a normal value (e.g. Unit) whose type is A@ (e.g. Unit@).
      //
      // The kind of the poly captures what kind of type values can be
      // substituted for A@. So now we are talking about values of type @<A@>,
      // which has kind level 1.
      //
      // In short, we have to increment the level of the argument kind to get
      // the proper kind for the poly.
      FbleKind* arg_kind = FbleGetKind(context, poly->arg);
      kind->arg = LevelAdjustedKind(arg_kind, 1);
      FbleFreeKind(arg_kind);

      kind->rkind = FbleGetKind(context, poly->body);
      return &kind->_base;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      FblePolyKind* kind = (FblePolyKind*)FbleGetKind(context, pat->poly);
      assert(kind->_base.tag == FBLE_POLY_KIND);

      FbleKind* rkind = FbleCopyKind(kind->rkind);
      FbleFreeKind(&kind->_base);
      return rkind;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      if (var->value == NULL) {
        return FbleCopyKind(var->kind);
      }
      return FbleGetKind(context, var->value);
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* type_type = (FbleTypeType*)type;

      FbleKind* arg_kind = FbleGetKind(context, type_type->type);
      FbleKind* kind = LevelAdjustedKind(arg_kind, 1);
      FbleFreeKind(arg_kind);
      return kind;
    }
  }

  FbleUnreachable("Should never get here");
  return NULL;
}

// See documentation in type.h.
size_t FbleGetKindLevel(FbleKind* kind)
{
  switch (kind->tag) {
    case FBLE_BASIC_KIND: {
      FbleBasicKind* basic = (FbleBasicKind*)kind;
      return basic->level;
    }

    case FBLE_POLY_KIND: {
      FblePolyKind* poly = (FblePolyKind*)kind;
      return FbleGetKindLevel(poly->rkind);
    }
  }
  FbleUnreachable("Should never get here");
  return -1;
}

// See documentation in type.h.
bool FbleKindsEqual(FbleKind* a, FbleKind* b)
{
  if (a->tag != b->tag) {
    return false;
  }

  switch (a->tag) {
    case FBLE_BASIC_KIND: {
      FbleBasicKind* ba = (FbleBasicKind*)a;
      FbleBasicKind* bp = (FbleBasicKind*)b;
      return ba->level == bp->level;
    }

    case FBLE_POLY_KIND: {
      FblePolyKind* pa = (FblePolyKind*)a;
      FblePolyKind* pb = (FblePolyKind*)b;
      return FbleKindsEqual(pa->arg, pb->arg)
          && FbleKindsEqual(pa->rkind, pb->rkind);
    }
  }

  FbleUnreachable("Should never get here");
  return false;
}

// See documentation in type.h
bool FbleKindCompatible(FbleKind* expected, FbleKind* actual)
{
  switch (expected->tag) {
    case FBLE_BASIC_KIND: {
      return FbleGetKindLevel(expected) == FbleGetKindLevel(actual);
    }

    case FBLE_POLY_KIND: {
      if (actual->tag != FBLE_POLY_KIND) {
        return false;
      }

      FblePolyKind* pe = (FblePolyKind*)expected;
      FblePolyKind* pa = (FblePolyKind*)actual;
      return FbleKindCompatible(pe->arg, pa->arg)
          && FbleKindCompatible(pe->rkind, pa->rkind);
    }
  }

  FbleUnreachable("Should never get here");
  return false;
}

// See documentation in type.h
void FblePrintKind(FbleKind* kind)
{
  switch (kind->tag) {
    case FBLE_BASIC_KIND: {
      FbleBasicKind* basic = (FbleBasicKind*)kind;
      switch (basic->level) {
        case 0: fprintf(stderr, "%%"); break;
        case 1: fprintf(stderr, "@"); break;
        default: fprintf(stderr, "@%zi", basic->level); break;
      }
      break;
    }

    case FBLE_POLY_KIND: {
      const char* prefix = "<";
      while (kind->tag == FBLE_POLY_KIND) {
        FblePolyKind* poly = (FblePolyKind*)kind;
        fprintf(stderr, "%s", prefix);
        FblePrintKind(poly->arg);
        prefix = ", ";
        kind = poly->rkind;
      }
      fprintf(stderr, ">");
      FblePrintKind(kind);
      break;
    }
  }
}

// See documentation in type.h.
FbleType* FbleNewTypeRaw(FbleTypeHeap* heap, size_t size, FbleTypeTag tag, FbleLoc loc)
{
  FbleType* type = (FbleType*)FbleAllocType(heap, size);
  type->tag = tag;
  type->loc = FbleCopyLoc(loc);
  type->visiting = false;
  return type;
}

// See documentation in type.h.
FbleType* FbleNewVarType(FbleTypeHeap* heap, FbleLoc loc, FbleKind* kind, FbleName name)
{
  assert(name.space == FBLE_TYPE_NAME_SPACE && "bad namespace for var type");

  size_t level = FbleGetKindLevel(kind);

  FbleVarType* var = FbleNewType(heap, FbleVarType, FBLE_VAR_TYPE, loc);
  var->name = FbleCopyName(name);
  var->kind = LevelAdjustedKind(kind, -(int)level);
  var->value = NULL;

  FbleType* type = &var->_base;
  for (size_t i = 0; i < level; ++i) {
    FbleTypeType* type_type = FbleNewType(heap, FbleTypeType, FBLE_TYPE_TYPE, loc);
    type_type->type = type;
    FbleTypeAddRef(heap, &type_type->_base, type);
    FbleReleaseType(heap, type);
    type = &type_type->_base;
  }

  return type;
}

// See documentation in type.h.
void FbleAssignVarType(FbleTypeHeap* heap, FbleType* var, FbleType* value)
{
  while (var->tag == FBLE_TYPE_TYPE) {
    assert(value->tag == FBLE_TYPE_TYPE && "Kind level mismatch");
    var = ((FbleTypeType*)var)->type;
    value = ((FbleTypeType*)value)->type;
  }

  assert(var->tag == FBLE_VAR_TYPE && "non-var type passed to FbleAssignVarType");
  FbleVarType* var_type = (FbleVarType*)var;
  FbleKind* kind = FbleGetKind(FbleTypeHeapGetContext(heap), value);
  FbleFreeKind(var_type->kind);
  var_type->kind = kind;
  var_type->value = value;
  FbleTypeAddRef(heap, &var_type->_base, var_type->value);
}

// See documentation in type.h.
FbleType* FbleNewPolyType(FbleTypeHeap* heap, FbleLoc loc, FbleType* arg, FbleType* body)
{
  if (body->tag == FBLE_TYPE_TYPE) {
    // \arg -> typeof(body) = typeof(\arg -> body)
    FbleTypeType* ttbody = (FbleTypeType*)body;
    FbleType* body_type = FbleNewPolyType(heap, loc, arg, ttbody->type);

    FbleTypeType* tt = FbleNewType(heap, FbleTypeType, FBLE_TYPE_TYPE, loc);
    tt->type = body_type;
    FbleTypeAddRef(heap, &tt->_base, tt->type);
    FbleReleaseType(heap, tt->type);
    return &tt->_base;
  }

  FblePolyType* pt = FbleNewType(heap, FblePolyType, FBLE_POLY_TYPE, loc);
  pt->arg = arg;
  FbleTypeAddRef(heap, &pt->_base, pt->arg);
  pt->body = body;
  FbleTypeAddRef(heap, &pt->_base, pt->body);

  assert(pt->body->tag != FBLE_TYPE_TYPE);
  return &pt->_base;
}

// See documentation in type.h.
FbleType* FbleNewPolyApplyType(FbleTypeHeap* heap, FbleLoc loc, FbleType* poly, FbleType* arg)
{
  if (poly->tag == FBLE_TYPE_TYPE) {
    // typeof(poly)<arg> == typeof(poly<arg>)
    FbleTypeType* ttpoly = (FbleTypeType*)poly;
    FbleType* body_type = FbleNewPolyApplyType(heap, loc, ttpoly->type, arg);
    FbleTypeType* tt = FbleNewType(heap, FbleTypeType, FBLE_TYPE_TYPE, loc);
    tt->type = body_type;
    FbleTypeAddRef(heap, &tt->_base, tt->type);
    FbleReleaseType(heap, tt->type);
    return &tt->_base;
  }

  FblePolyApplyType* pat = FbleNewType(heap, FblePolyApplyType, FBLE_POLY_APPLY_TYPE, loc);
  pat->poly = poly;
  FbleTypeAddRef(heap, &pat->_base, pat->poly);
  pat->arg = arg;
  FbleTypeAddRef(heap, &pat->_base, pat->arg);

  assert(pat->poly->tag != FBLE_TYPE_TYPE);
  return &pat->_base;
}

// See documentation in type.h.
FbleType* FbleNewPrivateType(FbleTypeHeap* heap, FbleLoc loc, FbleType* arg, FbleModulePath* package)
{
  if (arg->tag == FBLE_TYPE_TYPE) {
    FbleTypeType* ttarg = (FbleTypeType*)arg;
    FbleType* body_type = FbleNewPrivateType(heap, loc, ttarg->type, package);
    FbleTypeType* tt = FbleNewType(heap, FbleTypeType, FBLE_TYPE_TYPE, loc);
    tt->type = body_type;
    FbleTypeAddRef(heap, &tt->_base, tt->type);
    FbleReleaseType(heap, tt->type);
    return &tt->_base;
  }

  FblePrivateType* private = FbleNewType(heap, FblePrivateType, FBLE_PRIVATE_TYPE, loc);
  private->package = FbleCopyModulePath(package);
  private->arg = arg;
  FbleTypeAddRef(heap, &private->_base, private->arg);
  return &private->_base;
}

// See documentation in type.h.
bool FbleTypeIsVacuous(FbleTypeHeap* heap, FbleType* type)
{
  FbleType* normal = Normal(heap, type, NULL);
  while (normal != NULL && normal->tag == FBLE_TYPE_TYPE) {
    FbleTypeType* type_type = (FbleTypeType*)normal;
    FbleType* tmp = normal;
    normal = Normal(heap, type_type->type, NULL);
    FbleReleaseType(heap, tmp);
  }

  while (normal != NULL && normal->tag == FBLE_POLY_TYPE) {
    FblePolyType* poly = (FblePolyType*)normal;
    FbleType* tmp = normal;
    normal = Normal(heap, poly->body, NULL);
    FbleReleaseType(heap, tmp);
  }
  FbleReleaseType(heap, normal);
  return normal == NULL;
}

// See documentation in type.h.
FbleType* FbleNormalType(FbleTypeHeap* heap, FbleType* type)
{
  FbleType* normal = Normal(heap, type, NULL);
  assert(normal != NULL && "vacuous type does not have a normal form");
  return normal;
}

// See documentation in type.h.
FbleType* FbleValueOfType(FbleTypeHeap* heap, FbleType* typeof)
{
  if (typeof->tag == FBLE_TYPE_TYPE) {
    FbleTypeType* tt = (FbleTypeType*)typeof;
    FbleRetainType(heap, tt->type);
    return tt->type;
  }
  return NULL;
}

// See documentation in type.h.
FbleType* FbleListElementType(FbleTypeHeap* heap, FbleType* type)
{
  FbleDataType* data_type = (FbleDataType*)FbleNormalType(heap, type);
  if (data_type->_base.tag != FBLE_DATA_TYPE
      || data_type->datatype != FBLE_UNION_DATATYPE
      || data_type->fields.size != 2) {
    FbleReleaseType(heap, &data_type->_base);
    return NULL;
  }

  FbleDataType* nil_data_type = (FbleDataType*)FbleNormalType(heap, data_type->fields.xs[1].type);
  if (nil_data_type->_base.tag != FBLE_DATA_TYPE
      || nil_data_type->datatype != FBLE_STRUCT_DATATYPE
      || nil_data_type->fields.size != 0) {
    FbleReleaseType(heap, &nil_data_type->_base);
    FbleReleaseType(heap, &data_type->_base);
    return NULL;
  }
  FbleReleaseType(heap, &nil_data_type->_base);

  FbleDataType* cons_data_type = (FbleDataType*)FbleNormalType(heap, data_type->fields.xs[0].type);
  if (cons_data_type->_base.tag != FBLE_DATA_TYPE
      || cons_data_type->datatype != FBLE_STRUCT_DATATYPE
      || cons_data_type->fields.size != 2) {
    FbleReleaseType(heap, &cons_data_type->_base);
    FbleReleaseType(heap, &data_type->_base);
    return NULL;
  }

  if (!FbleTypesEqual(heap, type, cons_data_type->fields.xs[1].type)) {
    FbleReleaseType(heap, &cons_data_type->_base);
    FbleReleaseType(heap, &data_type->_base);
    return NULL;
  }

  FbleType* element_type = FbleRetainType(heap, cons_data_type->fields.xs[0].type);
  FbleReleaseType(heap, &cons_data_type->_base);
  FbleReleaseType(heap, &data_type->_base);
  return element_type;
}

// See documentation in type.h.
bool FbleTypesEqual(FbleTypeHeap* heap, FbleType* a, FbleType* b)
{
  FbleTypeAssignmentV vars = { .size = 0, .xs = NULL };
  return TypesEqual(heap, vars, a, b, NULL);
}

// See documentation in type.h.
void FbleInferTypes(FbleTypeHeap* heap, FbleTypeAssignmentV vars, FbleType* abstract, FbleType* concrete)
{
  // TODO: Separate this from TypesEqual.
  TypesEqual(heap, vars, abstract, concrete, NULL);
}

// See documentation in type.h
FbleType* FbleSpecializeType(FbleTypeHeap* heap, FbleTypeAssignmentV vars, FbleType* type)
{
  return Subst(heap, vars, type, NULL);
}

// See documentation in type.h
//
// Notes:
//   Human readable means we print var types using their name, without the
//   value associated with the variable. Because of this, we don't have to
//   worry about infinite recursion when trying to print a type: all recursion
//   must happen through a var type, and we don't ever go through a var type
//   when printing.
void FblePrintType(FbleType* type)
{
  switch (type->tag) {
    case FBLE_DATA_TYPE: {
      FbleDataType* dt = (FbleDataType*)type;
      fprintf(stderr, "%s(", dt->datatype == FBLE_STRUCT_DATATYPE ? "*" : "+");
      const char* comma = "";
      for (size_t i = 0; i < dt->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        FblePrintType(dt->fields.xs[i].type);
        fprintf(stderr, " ");
        FblePrintName(stderr, dt->fields.xs[i].name);
        comma = ", ";
      }
      fprintf(stderr, ")");
      return;
    }

    case FBLE_FUNC_TYPE: {
      const char* prefix = "(";
      while (type->tag == FBLE_FUNC_TYPE) {
        FbleFuncType* ft = (FbleFuncType*)type;
        fprintf(stderr, "%s", prefix);
        FblePrintType(ft->arg);
        prefix = ", ";
        type = ft->rtype;
      }
      fprintf(stderr, ") { ");
      FblePrintType(type);
      fprintf(stderr, "; }");
      return;
    }

    case FBLE_POLY_TYPE: {
      const char* prefix = "<";
      while (type->tag == FBLE_POLY_TYPE) {
        FblePolyType* pt = (FblePolyType*)type;
        fprintf(stderr, "%s", prefix);

        FbleKind* value_kind = FbleGetKind(NULL, pt->arg);
        FbleKind* type_kind = LevelAdjustedKind(value_kind, 1);
        FblePrintKind(type_kind);
        FbleFreeKind(type_kind);
        FbleFreeKind(value_kind);

        fprintf(stderr, " ");
        FblePrintType(pt->arg);
        prefix = ", ";
        type = pt->body;
      }
      fprintf(stderr, "> { ");
      FblePrintType(type);
      fprintf(stderr, "; }");
      return;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FbleTypeV args;
      FbleInitVector(args);

      while (type->tag == FBLE_POLY_APPLY_TYPE) {
        FblePolyApplyType* pat = (FblePolyApplyType*)type;
        FbleAppendToVector(args, pat->arg);
        type = pat->poly;
      }

      FblePrintType(type);
      const char* prefix = "<";
      for (size_t i = 0; i < args.size; ++i) {
        size_t j = args.size - i - 1;
        fprintf(stderr, "%s", prefix);
        FblePrintType(args.xs[j]);
        prefix = ", ";
      }
      fprintf(stderr, ">");
      FbleFreeVector(args);
      return;
    }

    case FBLE_PACKAGE_TYPE: {
      FblePackageType* package = (FblePackageType*)type;
      fprintf(stderr, "@");
      FblePrintModulePath(stderr, package->path);
      return;
    }

    case FBLE_PRIVATE_TYPE: {
      FblePrivateType* private = (FblePrivateType*)type;
      FblePrintType(private->arg);
      fprintf(stderr, ".%%(@");
      FblePrintModulePath(stderr, private->package);
      fprintf(stderr, ")");
      return;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      FblePrintName(stderr, var->name);

      // Special case to make error messages nicer for failed private type
      // access.
      if (var->value != NULL && var->value->tag == FBLE_PRIVATE_TYPE) {
        FblePrivateType* private = (FblePrivateType*)var->value;
        fprintf(stderr, ".%%(@");
        FblePrintModulePath(stderr, private->package);
        fprintf(stderr, ")");
      }
      return;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* tt = (FbleTypeType*)type;
      fprintf(stderr, "@<");
      FblePrintType(tt->type);
      fprintf(stderr, ">");
      return;
    }
  }

  FbleUnreachable("should never get here");
}
