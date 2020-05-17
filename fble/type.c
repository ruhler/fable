
#include <assert.h>   // for assert

#include "fble-vector.h"
#include "type.h"

#define UNREACHABLE(x) assert(false && x)

// Design notes on types:
// * Instances of Type represent both unevaluated and evaluated versions of
//   the type. We use the unevaluated versions of the type when printing error
//   messages and as a stable reference to a type before and after evaluation.
// * Cycles are allowed in the Type data structure, to represent recursive
//   types. Every cycle is guaranteed to go through a Var type.
// * Types are evaluated as they are constructed.
// * FBLE_TYPE_TYPE is handled specially: we propagate FBLE_TYPE_TYPE up to the top of
//   the type during construction rather than save the unevaluated version of
//   a typeof.

// TypeList --
//   A linked list of types.
typedef struct TypeList {
  FbleType* type;
  struct TypeList* next;
} TypeList;

// TypeList --
//   A linked list of type ids.
typedef struct TypeIdList {
  uintptr_t id;
  struct TypeIdList* next;
} TypeIdList;

// TypePairs --
//   A set of pairs of types.
typedef struct TypePairs {
  FbleType* a;
  FbleType* b;
  struct TypePairs* next;
} TypePairs;

// TypeIdPairs --
//   A set of pairs of type ids.
typedef struct TypeIdPairs {
  uintptr_t a;
  uintptr_t b;
  struct TypeIdPairs* next;
} TypeIdPairs;

static FbleKind* LevelAdjustedKind(FbleArena* arena, FbleKind* kind, int increment);

static void Ref(FbleHeapCallback* callback, FbleType* type);
static void Refs(FbleHeapCallback* callback, FbleType* type);
static void OnFree(FbleTypeHeap* heap, FbleType* type);

static FbleType* Normal(FbleTypeHeap* heap, FbleType* type, TypeIdList* normalizing);
static bool HasParam(FbleType* type, FbleType* param, TypeList* visited);
static FbleType* Subst(FbleTypeHeap* heap, FbleType* src, FbleType* param, FbleType* arg, TypePairs* tps);
static bool TypesEqual(FbleTypeHeap* heap, FbleType* a, FbleType* b, TypeIdPairs* eq);

// LevelAdjustedKind --
//   Construct a kind that is a level adjusted version of the given kind.
//
// Inputs:
//   arena - arena to use for allocations.
//   kind - the kind to use as the basis.
//   increment - the kind level increment amount. May be negative to decrease
//               the kind level.
//
// Results:
//   A new kind that is the same as the given kind except with level
//   incremented by the given increment.
//
// Side effects:
//   The caller is responsible for calling FbleReleaseKind on the returned
//   kind when it is no longer needed. This function does not take ownership
//   of the given kind.
//
//   Behavior is undefined if the increment causes the resulting kind level
//   to be less than 0.
static FbleKind* LevelAdjustedKind(FbleArena* arena, FbleKind* kind, int increment)
{
  switch (kind->tag) {
    case FBLE_BASIC_KIND: {
      FbleBasicKind* basic = (FbleBasicKind*)kind;
      assert((int)basic->level + increment >= 0);

      FbleBasicKind* adjusted = FbleAlloc(arena, FbleBasicKind);
      adjusted->_base.tag = FBLE_BASIC_KIND;
      adjusted->_base.loc = FbleCopyLoc(kind->loc);
      adjusted->_base.refcount = 1;
      adjusted->level = basic->level + increment;
      return &adjusted->_base;
    }

    case FBLE_POLY_KIND: {
      FblePolyKind* poly = (FblePolyKind*)kind;
      FblePolyKind* adjusted = FbleAlloc(arena, FblePolyKind);
      adjusted->_base.tag = FBLE_POLY_KIND;
      adjusted->_base.loc = FbleCopyLoc(kind->loc);
      adjusted->_base.refcount = 1;
      adjusted->arg = FbleRetainKind(arena, poly->arg);
      adjusted->rkind = LevelAdjustedKind(arena, poly->rkind, increment);
      return &adjusted->_base;
    }
  }
  UNREACHABLE("Should never get here");
  return NULL;
}



// Ref --
//   Helper function for traversing references.
//
// Inputs:
//   callback - the refs callback.
//   type - the type to traverse.
//
// Results:
//   none.
//
// Side effects:
//   If type is not null, the callback is called on it.
static void Ref(FbleHeapCallback* callback, FbleType* type)
{
  if (type != NULL) {
    callback->callback(callback, type);
  }
}

// Refs --
//   The refs function for types. See documentation in heap.h
static void Refs(FbleHeapCallback* callback, FbleType* type)
{
  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* st = (FbleStructType*)type;
      for (size_t i = 0; i < st->fields.size; ++i) {
        Ref(callback, st->fields.xs[i].type);
      }
      break;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* ut = (FbleUnionType*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        Ref(callback, ut->fields.xs[i].type);
      }
      break;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;
      for (size_t i = 0; i < ft->args.size; ++i) {
        Ref(callback, ft->args.xs[i]);
      }
      Ref(callback, ft->rtype);
      break;
    }

    case FBLE_PROC_TYPE: {
      FbleProcType* pt = (FbleProcType*)type;
      Ref(callback, pt->type);
      break;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pt = (FblePolyType*)type;
      Ref(callback, pt->arg);
      Ref(callback, pt->body);
      break;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      Ref(callback, pat->poly);
      Ref(callback, pat->arg);
      break;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      Ref(callback, var->value);
      break;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* t = (FbleTypeType*)type;
      Ref(callback, t->type);
      break;
    }
  }
}

//   The on_free function for types. See documentation in heap.h
static void OnFree(FbleTypeHeap* heap, FbleType* type)
{
  FbleArena* arena = heap->arena;
  FbleFreeLoc(arena, type->loc);
  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* st = (FbleStructType*)type;
      FbleFree(arena, st->fields.xs);
      return;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* ut = (FbleUnionType*)type;
      FbleFree(arena, ut->fields.xs);
      return;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;
      FbleFree(arena, ft->args.xs);
      return;
    }

    case FBLE_PROC_TYPE: return;
    case FBLE_POLY_TYPE: return;
    case FBLE_POLY_APPLY_TYPE: return;

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      FbleReleaseKind(arena, var->kind);
      return;
    }

    case FBLE_TYPE_TYPE: return;
  }

  UNREACHABLE("should never get here");
}

// Normal --
//   Compute the normal form of a type.
//
// Inputs: 
//   heap - heap to use for allocations.
//   type - the type to reduce.
//   normalizing - the set of types currently being normalized.
//
// Results:
//   The type reduced to normal form, or NULL if the type cannot be reduced to
//   normal form.
//
// Side effects:
//   The caller is responsible for calling FbleTypeRelease on the returned type
//   when it is no longer needed.
static FbleType* Normal(FbleTypeHeap* heap, FbleType* type, TypeIdList* normalizing)
{
  for (TypeIdList* n = normalizing; n != NULL; n = n->next) {
    if (type->id == n->id) {
      return NULL;
    }
  }

  TypeIdList nn = {
    .id = type->id,
    .next = normalizing
  };

  switch (type->tag) {
    case FBLE_STRUCT_TYPE: return FbleTypeRetain(heap, type);
    case FBLE_UNION_TYPE: return FbleTypeRetain(heap, type);
    case FBLE_FUNC_TYPE: return FbleTypeRetain(heap, type);
    case FBLE_PROC_TYPE: return FbleTypeRetain(heap, type);

    case FBLE_POLY_TYPE: {
      FblePolyType* poly = (FblePolyType*)type;

      // eta-reduce (\x -> f x) ==> f
      FblePolyApplyType* pat = (FblePolyApplyType*)Normal(heap, poly->body, &nn);
      if (pat == NULL) {
        return NULL;
      }

      if (pat->_base.tag == FBLE_POLY_APPLY_TYPE && pat->arg == poly->arg) {
        FbleType* normal = FbleTypeRetain(heap, pat->poly);
        FbleTypeRelease(heap, &pat->_base);
        return normal;
      }

      FbleTypeRelease(heap, &pat->_base);
      return FbleTypeRetain(heap, type);
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      FblePolyType* poly = (FblePolyType*)Normal(heap, pat->poly, &nn);
      if (poly == NULL) {
        return NULL;
      }

      if (poly->_base.tag == FBLE_POLY_TYPE) {
        FbleType* subst = Subst(heap, poly->body, poly->arg, pat->arg, NULL);
        FbleType* result = Normal(heap, subst, &nn);
        FbleTypeRelease(heap, &poly->_base);
        FbleTypeRelease(heap, subst);
        return result;
      }

      // Don't bother simplifying at all if we can't do a substituion.
      FbleTypeRelease(heap, &poly->_base);
      return FbleTypeRetain(heap, type);
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      if (var->value == NULL) {
        return FbleTypeRetain(heap, type);
      }
      return Normal(heap, var->value, &nn);
    }

    case FBLE_TYPE_TYPE: return FbleTypeRetain(heap, type);
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// HasParam --
//   Check whether a type has the given param as a free type variable.
//
// Inputs:
//   type - the type to check.
//   param - the abstract type to check for.
//   visited - a list of types already visited to end the recursion.
//
// Results:
//   True if the param occur in type, false otherwise.
//
// Side effects:
//   None.
static bool HasParam(FbleType* type, FbleType* param, TypeList* visited)
{
  for (TypeList* v = visited; v != NULL; v = v->next) {
    if (type == v->type) {
      return false;
    }
  }

  TypeList nv = {
    .type = type,
    .next = visited
  };

  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* st = (FbleStructType*)type;
      for (size_t i = 0; i < st->fields.size; ++i) {
        if (HasParam(st->fields.xs[i].type, param, &nv)) {
          return true;
        }
      }
      return false;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* ut = (FbleUnionType*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        if (HasParam(ut->fields.xs[i].type, param, &nv)) {
          return true;
        }
      }
      return false;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;
      for (size_t i = 0; i < ft->args.size; ++i) {
        if (HasParam(ft->args.xs[i], param, &nv)) {
          return true;
        }
      }
      return HasParam(ft->rtype, param, &nv);
    }

    case FBLE_PROC_TYPE: {
      FbleProcType* ut = (FbleProcType*)type;
      return HasParam(ut->type, param, &nv);
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pt = (FblePolyType*)type;
      return pt->arg != param && HasParam(pt->body, param, &nv);
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      return HasParam(pat->arg, param, &nv)
          || HasParam(pat->poly, param, &nv);
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      return (type == param)
          || (var->value != NULL && HasParam(var->value, param, &nv));
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* type_type = (FbleTypeType*)type;
      return HasParam(type_type->type, param, &nv);
    }
  }

  UNREACHABLE("Should never get here");
  return false;
}

// Subst --
//   Substitute the given argument in place of the given parameter in the
//   given type. This function does not attempt to evaluate the results
//   of the substitution.
//
// Inputs:
//   heap - heap to use for allocations.
//   type - the type to substitute into.
//   param - the abstract type to substitute out.
//   arg - the concrete type to substitute in.
//   tps - a map of already substituted types, to avoid infinite recursion.
//         External callers should pass NULL.
//
// Results:
//   A type with all occurrences of param replaced with the arg type. The type
//   may not be fully evaluated.
//
// Side effects:
//   The caller is responsible for calling FbleTypeRelease on the returned
//   type when it is no longer needed. No new type ids are allocated by
//   substitution.
//
// Design notes:
//   The given type may have cycles. For example:
//     <@>@ F@ = <@ T@> {
//        @ X@ = +(T@ a, X@ b);
//     };
//     F@<Unit@>
//
//   To prevent infinite recursion, we use tps to record that we have already
//   substituted Unit@ for T@ in X@ when traversing into field 'b' of X@.
static FbleType* Subst(FbleTypeHeap* heap, FbleType* type, FbleType* param, FbleType* arg, TypePairs* tps)
{
  if (!HasParam(type, param, NULL)) {
    return FbleTypeRetain(heap, type);
  }

  FbleArena* arena = heap->arena;

  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* st = (FbleStructType*)type;
      FbleStructType* sst = FbleNewType(heap, FbleStructType, FBLE_STRUCT_TYPE, st->_base.loc);
      sst->_base.id = st->_base.id;

      FbleVectorInit(arena, sst->fields);
      for (size_t i = 0; i < st->fields.size; ++i) {
        FbleTaggedType field = {
          .name = st->fields.xs[i].name,
          .type = Subst(heap, st->fields.xs[i].type, param, arg, tps)
        };
        FbleVectorAppend(arena, sst->fields, field);
        FbleTypeAddRef(heap, &sst->_base, field.type);
        FbleTypeRelease(heap, field.type);
      }
      return &sst->_base;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* ut = (FbleUnionType*)type;
      FbleUnionType* sut = FbleNewType(heap, FbleUnionType, FBLE_UNION_TYPE, ut->_base.loc);
      sut->_base.id = ut->_base.id;
      FbleVectorInit(arena, sut->fields);
      for (size_t i = 0; i < ut->fields.size; ++i) {
        FbleTaggedType field = {
          .name = ut->fields.xs[i].name,
          .type = Subst(heap, ut->fields.xs[i].type, param, arg, tps)
        };
        FbleVectorAppend(arena, sut->fields, field);
        FbleTypeAddRef(heap, &sut->_base, field.type);
        FbleTypeRelease(heap, field.type);
      }
      return &sut->_base;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;

      FbleType* rtype = Subst(heap, ft->rtype, param, arg, tps);

      FbleFuncType* sft = FbleNewType(heap, FbleFuncType, FBLE_FUNC_TYPE, ft->_base.loc);
      sft->_base.id = ft->_base.id;
      FbleVectorInit(arena, sft->args);
      sft->rtype = rtype;
      FbleTypeAddRef(heap, &sft->_base, sft->rtype);
      FbleTypeRelease(heap, sft->rtype);

      for (size_t i = 0; i < ft->args.size; ++i) {
        FbleType* farg = Subst(heap, ft->args.xs[i], param, arg, tps);
        FbleVectorAppend(arena, sft->args, farg);
        FbleTypeAddRef(heap, &sft->_base, farg);
        FbleTypeRelease(heap, farg);
      }

      return &sft->_base;
    }

    case FBLE_PROC_TYPE: {
      FbleProcType* ut = (FbleProcType*)type;
      FbleType* body = Subst(heap, ut->type, param, arg, tps);

      FbleProcType* sut = FbleNewType(heap, FbleProcType, FBLE_PROC_TYPE, ut->_base.loc);
      sut->_base.id = ut->_base.id;
      sut->type = body;
      FbleTypeAddRef(heap, &sut->_base, sut->type);
      FbleTypeRelease(heap, sut->type);
      return &sut->_base;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pt = (FblePolyType*)type;
      FbleType* body = Subst(heap, pt->body, param, arg, tps); 

      FblePolyType* spt = FbleNewType(heap, FblePolyType, FBLE_POLY_TYPE, pt->_base.loc);
      spt->_base.id = pt->_base.id;
      spt->arg = pt->arg;
      FbleTypeAddRef(heap, &spt->_base, spt->arg);
      spt->body = body;
      FbleTypeAddRef(heap, &spt->_base, spt->body);
      assert(spt->body->tag != FBLE_TYPE_TYPE);

      FbleTypeRelease(heap, body);
      return &spt->_base;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      FbleType* poly = Subst(heap, pat->poly, param, arg, tps);
      FbleType* sarg = Subst(heap, pat->arg, param, arg, tps);

      FblePolyApplyType* spat = FbleNewType(heap, FblePolyApplyType, FBLE_POLY_APPLY_TYPE, pat->_base.loc);
      spat->_base.id = pat->_base.id;
      spat->poly = poly;
      FbleTypeAddRef(heap, &spat->_base, spat->poly);
      spat->arg = sarg;
      FbleTypeAddRef(heap, &spat->_base, spat->arg);
      assert(spat->poly->tag != FBLE_TYPE_TYPE);

      FbleTypeRelease(heap, poly);
      FbleTypeRelease(heap, sarg);
      return &spat->_base;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      if (var->value == NULL) {
        return FbleTypeRetain(heap, (type == param ? arg : type));
      }

      // Check to see if we've already done substitution on the value pointed
      // to by this ref.
      for (TypePairs* tp = tps; tp != NULL; tp = tp->next) {
        if (tp->a == var->value) {
          return FbleTypeRetain(heap, tp->b);
        }
      }

      FbleType* svar = FbleNewVarType(heap, type->loc, var->kind, var->name);

      TypePairs ntp = {
        .a = var->value,
        .b = svar,
        .next = tps
      };

      FbleType* value = Subst(heap, var->value, param, arg, &ntp);
      FbleAssignVarType(heap, svar, value);
      FbleTypeRelease(heap, svar);
      return value;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* tt = (FbleTypeType*)type;

      FbleType* body = Subst(heap, tt->type, param, arg, tps);

      FbleTypeType* stt = FbleNewType(heap, FbleTypeType, FBLE_TYPE_TYPE, tt->_base.loc);
      stt->_base.id = tt->_base.id;
      stt->type = body;
      FbleTypeAddRef(heap, &stt->_base, stt->type);
      FbleTypeRelease(heap, stt->type);
      return &stt->_base;
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// TypesEqual --
//   Test whether the two given evaluated types are equal.
//
// Inputs:
//   heap - heap to use for allocations
//   a - the first type
//   b - the second type
//   eq - A set of pairs of type ids that should be assumed to be equal
//
// Results:
//   True if the first type equals the second type, false otherwise.
//
// Side effects:
//   None.
static bool TypesEqual(FbleTypeHeap* heap, FbleType* a, FbleType* b, TypeIdPairs* eq)
{
  a = FbleNormalType(heap, a);
  b = FbleNormalType(heap, b);

  for (TypeIdPairs* pairs = eq; pairs != NULL; pairs = pairs->next) {
    if (a->id == pairs->a && b->id == pairs->b) {
      FbleTypeRelease(heap, a);
      FbleTypeRelease(heap, b);
      return true;
    }
  }

  TypeIdPairs neq = {
    .a = a->id,
    .b = b->id,
    .next = eq,
  };

  if (a->tag != b->tag) {
    FbleTypeRelease(heap, a);
    FbleTypeRelease(heap, b);
    return false;
  }

  switch (a->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* sta = (FbleStructType*)a;
      FbleStructType* stb = (FbleStructType*)b;

      if (sta->fields.size != stb->fields.size) {
        FbleTypeRelease(heap, a);
        FbleTypeRelease(heap, b);
        return false;
      }

      for (size_t i = 0; i < sta->fields.size; ++i) {
        if (!FbleNamesEqual(&sta->fields.xs[i].name, &stb->fields.xs[i].name)) {
          FbleTypeRelease(heap, a);
          FbleTypeRelease(heap, b);
          return false;
        }

        if (!TypesEqual(heap, sta->fields.xs[i].type, stb->fields.xs[i].type, &neq)) {
          FbleTypeRelease(heap, a);
          FbleTypeRelease(heap, b);
          return false;
        }
      }

      FbleTypeRelease(heap, a);
      FbleTypeRelease(heap, b);
      return true;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* uta = (FbleUnionType*)a;
      FbleUnionType* utb = (FbleUnionType*)b;
      if (uta->fields.size != utb->fields.size) {
        FbleTypeRelease(heap, a);
        FbleTypeRelease(heap, b);
        return false;
      }

      for (size_t i = 0; i < uta->fields.size; ++i) {
        if (!FbleNamesEqual(&uta->fields.xs[i].name, &utb->fields.xs[i].name)) {
          FbleTypeRelease(heap, a);
          FbleTypeRelease(heap, b);
          return false;
        }

        if (!TypesEqual(heap, uta->fields.xs[i].type, utb->fields.xs[i].type, &neq)) {
          FbleTypeRelease(heap, a);
          FbleTypeRelease(heap, b);
          return false;
        }
      }

      FbleTypeRelease(heap, a);
      FbleTypeRelease(heap, b);
      return true;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* fta = (FbleFuncType*)a;
      FbleFuncType* ftb = (FbleFuncType*)b;
      if (fta->args.size != ftb->args.size) {
        FbleTypeRelease(heap, a);
        FbleTypeRelease(heap, b);
        return false;
      }

      for (size_t i = 0; i < fta->args.size; ++i) {
        if (!TypesEqual(heap, fta->args.xs[i], ftb->args.xs[i], &neq)) {
          FbleTypeRelease(heap, a);
          FbleTypeRelease(heap, b);
          return false;
        }
      }

      bool result = TypesEqual(heap, fta->rtype, ftb->rtype, &neq);
      FbleTypeRelease(heap, a);
      FbleTypeRelease(heap, b);
      return result;
    }

    case FBLE_PROC_TYPE: {
      FbleProcType* uta = (FbleProcType*)a;
      FbleProcType* utb = (FbleProcType*)b;
      bool result = TypesEqual(heap, uta->type, utb->type, &neq);
      FbleTypeRelease(heap, a);
      FbleTypeRelease(heap, b);
      return result;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pta = (FblePolyType*)a;
      FblePolyType* ptb = (FblePolyType*)b;

      FbleArena* arena = heap->arena;
      FbleKind* ka = FbleGetKind(arena, pta->arg);
      FbleKind* kb = FbleGetKind(arena, ptb->arg);
      if (!FbleKindsEqual(ka, kb)) {
        FbleReleaseKind(arena, ka);
        FbleReleaseKind(arena, kb);
        FbleTypeRelease(heap, a);
        FbleTypeRelease(heap, b);
        return false;
      }
      FbleReleaseKind(arena, ka);
      FbleReleaseKind(arena, kb);
  
      TypeIdPairs pneq = {
        .a = pta->arg->id,
        .b = ptb->arg->id,
        .next = &neq
      };
      bool result = TypesEqual(heap, pta->body, ptb->body, &pneq);
      FbleTypeRelease(heap, a);
      FbleTypeRelease(heap, b);
      return result;
    }

    case FBLE_POLY_APPLY_TYPE: {
      UNREACHABLE("poly apply type is not Normal");
      FbleTypeRelease(heap, a);
      FbleTypeRelease(heap, b);
      return false;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* va = (FbleVarType*)a;
      FbleVarType* vb = (FbleVarType*)b;

      assert(va->value == NULL && vb->value == NULL);
      FbleTypeRelease(heap, a);
      FbleTypeRelease(heap, b);
      return a == b;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* tta = (FbleTypeType*)a;
      FbleTypeType* ttb = (FbleTypeType*)b;
      bool result = TypesEqual(heap, tta->type, ttb->type, &neq);
      FbleTypeRelease(heap, a);
      FbleTypeRelease(heap, b);
      return result;
    }
  }

  UNREACHABLE("Should never get here");
  FbleTypeRelease(heap, a);
  FbleTypeRelease(heap, b);
  return false;
}

// FbleGetKind -- see documentation in type.h
FbleKind* FbleGetKind(FbleArena* arena, FbleType* type)
{
  switch (type->tag) {
    case FBLE_STRUCT_TYPE:
    case FBLE_UNION_TYPE:
    case FBLE_FUNC_TYPE:
    case FBLE_PROC_TYPE: {
      FbleBasicKind* kind = FbleAlloc(arena, FbleBasicKind);
      kind->_base.tag = FBLE_BASIC_KIND;
      kind->_base.loc = FbleCopyLoc(type->loc);
      kind->_base.refcount = 1;
      kind->level = 0;
      return &kind->_base;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* poly = (FblePolyType*)type;
      FblePolyKind* kind = FbleAlloc(arena, FblePolyKind);
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
      FbleKind* arg_kind = FbleGetKind(arena, poly->arg);
      kind->arg = LevelAdjustedKind(arena, arg_kind, 1);
      FbleReleaseKind(arena, arg_kind);

      kind->rkind = FbleGetKind(arena, poly->body);
      return &kind->_base;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      FblePolyKind* kind = (FblePolyKind*)FbleGetKind(arena, pat->poly);
      assert(kind->_base.tag == FBLE_POLY_KIND);

      FbleKind* rkind = FbleRetainKind(arena, kind->rkind);
      FbleReleaseKind(arena, &kind->_base);
      return rkind;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      return FbleRetainKind(arena, var->kind);
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* type_type = (FbleTypeType*)type;

      FbleKind* arg_kind = FbleGetKind(arena, type_type->type);
      FbleKind* kind = LevelAdjustedKind(arena, arg_kind, 1);
      FbleReleaseKind(arena, arg_kind);
      return kind;
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// FbleGetKindLevel -- see documentation in type.h
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
  UNREACHABLE("Should never get here");
  return -1;
}

// FbleKindsEqual -- see documentation in type.h
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

  UNREACHABLE("Should never get here");
  return false;
}

// FblePrintKind -- see documentation in type.h
void FblePrintKind(FbleKind* kind)
{
  switch (kind->tag) {
    case FBLE_BASIC_KIND: {
      FbleBasicKind* basic = (FbleBasicKind*)kind;
      switch (basic->level) {
        case 0: fprintf(stderr, "%%"); break;
        case 1: fprintf(stderr, "@"); break;
        default: fprintf(stderr, "@%i", basic->level); break;
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

// FbleNewTypeHeap -- see documentation in fble-types.h
FbleTypeHeap* FbleNewTypeHeap(FbleArena* arena)
{
  return FbleNewMarkSweepHeap(arena,
      (void (*)(FbleHeapCallback*, void*))&Refs,
      (void (*)(FbleHeap*, void*))&OnFree);
}

// FbleFreeTypeHeap -- see documentation in fble-types.h
void FbleFreeTypeHeap(FbleTypeHeap* heap)
{
  FbleFreeMarkSweepHeap(heap);
}

// FbleNewType -- see documentation in type.h
FbleType* FbleNewTypeRaw(FbleTypeHeap* heap, size_t size, FbleTypeTag tag, FbleLoc loc)
{
  FbleType* type = (FbleType*)heap->new(heap, size);
  type->tag = tag;
  type->loc = FbleCopyLoc(loc);

  // TODO: Don't use a static variable for this. Static variables are bad.
  // What am I thinking? Surely there is a better way to do this.
  static uintptr_t s_id = 1;
  type->id = s_id++;
  return type;
}

// FbleTypeRetain -- see documentation in type.h
FbleType* FbleTypeRetain(FbleTypeHeap* heap, FbleType* type)
{
  if (type != NULL) {
    heap->retain(heap, type);
  }
  return type;
}

// FbleTypeRelease -- see documentation in type.h
void FbleTypeRelease(FbleTypeHeap* heap, FbleType* type)
{
  if (type != NULL) {
    heap->release(heap, type);
  }
}
// FbleTypeAddRef -- see documentation in type.h
void FbleTypeAddRef(FbleTypeHeap* heap, FbleType* src, FbleType* dst)
{
  heap->add_ref(heap, src, dst);
}

// FbleNewVarType -- see documentation in type.h
FbleType* FbleNewVarType(FbleTypeHeap* heap, FbleLoc loc, FbleKind* kind, FbleName name)
{
  assert(name.space == FBLE_TYPE_NAME_SPACE && "bad namespace for var type");

  FbleArena* arena = heap->arena;

  size_t level = FbleGetKindLevel(kind);

  FbleVarType* var = FbleNewType(heap, FbleVarType, FBLE_VAR_TYPE, loc);
  var->name = name;
  var->kind = LevelAdjustedKind(arena, kind, -(int)level);
  var->value = NULL;

  FbleType* type = &var->_base;
  for (size_t i = 0; i < level; ++i) {
    FbleTypeType* type_type = FbleNewType(heap, FbleTypeType, FBLE_TYPE_TYPE, loc);
    type_type->type = type;
    FbleTypeAddRef(heap, &type_type->_base, type);
    FbleTypeRelease(heap, type);
    type = &type_type->_base;
  }

  return type;
}

// FbleAssignVarType -- see documentation in type.h
void FbleAssignVarType(FbleTypeHeap* heap, FbleType* var, FbleType* value)
{
  while (var->tag == FBLE_TYPE_TYPE) {
    assert(value->tag == FBLE_TYPE_TYPE && "Kind mismatch");
    var = ((FbleTypeType*)var)->type;
    value = ((FbleTypeType*)value)->type;
  }

  assert(var->tag == FBLE_VAR_TYPE && "non-var type passed to FbleAssignVarType");
  FbleVarType* var_type = (FbleVarType*)var;
  var_type->value = value;
  FbleTypeAddRef(heap, &var_type->_base, var_type->value);
}

// FbleNewPolyType -- see documentation in type.h
FbleType* FbleNewPolyType(FbleTypeHeap* heap, FbleLoc loc, FbleType* arg, FbleType* body)
{
  if (body->tag == FBLE_TYPE_TYPE) {
    // \arg -> typeof(body) = typeof(\arg -> body)
    FbleTypeType* ttbody = (FbleTypeType*)body;
    FbleType* body_type = FbleNewPolyType(heap, loc, arg, ttbody->type);

    FbleTypeType* tt = FbleNewType(heap, FbleTypeType, FBLE_TYPE_TYPE, loc);
    tt->type = body_type;
    FbleTypeAddRef(heap, &tt->_base, tt->type);
    FbleTypeRelease(heap, tt->type);
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

// FbleNewPolyApplyType -- see documentation in type.h
FbleType* FbleNewPolyApplyType(FbleTypeHeap* heap, FbleLoc loc, FbleType* poly, FbleType* arg)
{
  if (poly->tag == FBLE_TYPE_TYPE) {
    // typeof(poly)<arg> == typeof(poly<arg>)
    FbleTypeType* ttpoly = (FbleTypeType*)poly;
    FbleType* body_type = FbleNewPolyApplyType(heap, loc, ttpoly->type, arg);
    FbleTypeType* tt = FbleNewType(heap, FbleTypeType, FBLE_TYPE_TYPE, loc);
    tt->type = body_type;
    FbleTypeAddRef(heap, &tt->_base, tt->type);
    FbleTypeRelease(heap, tt->type);
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

// FbleTypeIsVacuous -- see documentation in type.h
bool FbleTypeIsVacuous(FbleTypeHeap* heap, FbleType* type)
{
  FbleType* normal = Normal(heap, type, NULL);
  while (normal != NULL && normal->tag == FBLE_TYPE_TYPE) {
    FbleTypeType* type_type = (FbleTypeType*)normal;
    FbleType* tmp = normal;
    normal = Normal(heap, type_type->type, NULL);
    FbleTypeRelease(heap, tmp);
  }

  while (normal != NULL && normal->tag == FBLE_POLY_TYPE) {
    FblePolyType* poly = (FblePolyType*)normal;
    FbleType* tmp = normal;
    normal = Normal(heap, poly->body, NULL);
    FbleTypeRelease(heap, tmp);
  }
  FbleTypeRelease(heap, normal);
  return normal == NULL;
}

// FbleNormalType -- see documentation in type.h
FbleType* FbleNormalType(FbleTypeHeap* heap, FbleType* type)
{
  FbleType* normal = Normal(heap, type, NULL);
  assert(normal != NULL && "vacuous type does not have a normal form");
  return normal;
}

// FbleValueOfType -- see documentation in type.h
FbleType* FbleValueOfType(FbleTypeHeap* heap, FbleType* typeof)
{
  if (typeof->tag == FBLE_TYPE_TYPE) {
    FbleTypeType* tt = (FbleTypeType*)typeof;
    FbleTypeRetain(heap, tt->type);
    return tt->type;
  }
  return NULL;
}

// FbleTypesEqual -- see documentation in fble-types.h
bool FbleTypesEqual(FbleTypeHeap* heap, FbleType* a, FbleType* b)
{
  return TypesEqual(heap, a, b, NULL);
}

// FblePrintType -- see documentation in type.h
//
// Notes:
//   Human readable means we print var types using their name, without the
//   value associated with the variable. Because of this, we don't have to
//   worry about infinite recursion when trying to print a type: all recursion
//   must happen through a var type, and we don't ever go through a var type
//   when printing.
void FblePrintType(FbleArena* arena, FbleType* type)
{
  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* st = (FbleStructType*)type;
      fprintf(stderr, "*(");
      const char* comma = "";
      for (size_t i = 0; i < st->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        FblePrintType(arena, st->fields.xs[i].type);
        fprintf(stderr, " ");
        FblePrintName(stderr, &st->fields.xs[i].name);
        comma = ", ";
      }
      fprintf(stderr, ")");
      return;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* ut = (FbleUnionType*)type;
      fprintf(stderr, "+(");
      const char* comma = "";
      for (size_t i = 0; i < ut->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        FblePrintType(arena, ut->fields.xs[i].type);
        fprintf(stderr, " ");
        FblePrintName(stderr, &ut->fields.xs[i].name);
        comma = ", ";
      }
      return;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;
      fprintf(stderr, "(");
      const char* comma = "";
      for (size_t i = 0; i < ft->args.size; ++i) {
        fprintf(stderr, "%s", comma);
        FblePrintType(arena, ft->args.xs[i]);
        comma = ", ";
      }
      fprintf(stderr, "){");
      FblePrintType(arena, ft->rtype);
      fprintf(stderr, ";}");
      return;
    }

    case FBLE_PROC_TYPE: {
      FbleProcType* ut = (FbleProcType*)type;
      FblePrintType(arena, ut->type);
      fprintf(stderr, "!");
      return;
    }

    case FBLE_POLY_TYPE: {
      const char* prefix = "<";
      while (type->tag == FBLE_POLY_TYPE) {
        FblePolyType* pt = (FblePolyType*)type;
        fprintf(stderr, "%s", prefix);
        FbleKind* kind = FbleGetKind(arena, pt->arg);
        FblePrintKind(kind);
        FbleReleaseKind(arena, kind);
        fprintf(stderr, " ");
        FblePrintType(arena, pt->arg);
        prefix = ", ";
        type = pt->body;
      }
      fprintf(stderr, "> { ");
      FblePrintType(arena, type);
      fprintf(stderr, "; }");
      return;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FbleTypeV args;
      FbleVectorInit(arena, args);

      while (type->tag == FBLE_POLY_APPLY_TYPE) {
        FblePolyApplyType* pat = (FblePolyApplyType*)type;
        FbleVectorAppend(arena, args, pat->arg);
        type = pat->poly;
      }

      FblePrintType(arena, type);
      const char* prefix = "<";
      for (size_t i = 0; i < args.size; ++i) {
        size_t j = args.size - i - 1;
        fprintf(stderr, "%s", prefix);
        FblePrintType(arena, args.xs[j]);
        prefix = ", ";
      }
      fprintf(stderr, ">");
      FbleFree(arena, args.xs);
      return;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      FblePrintName(stderr, &var->name);
      return;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* tt = (FbleTypeType*)type;
      fprintf(stderr, "@<");
      FblePrintType(arena, tt->type);
      fprintf(stderr, ">");
      return;
    }
  }

  UNREACHABLE("should never get here");
}
