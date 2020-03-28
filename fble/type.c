
#include <assert.h>   // for assert

#include "fble-type.h"

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

static void Add(FbleRefCallback* add, FbleType* type);
static void TypeAdded(FbleRefCallback* add, FbleRef* ref);
static void TypeFree(FbleTypeArena* arena, FbleRef* ref);

static FbleKind* TypeofKind(FbleArena* arena, FbleKind* kind);

static FbleType* Normal(FbleTypeArena* arena, FbleType* type, TypeIdList* normalizing);
static bool HasParam(FbleType* type, FbleVarType* param, TypeList* visited);
static FbleType* Subst(FbleTypeArena* arena, FbleType* src, FbleVarType* param, FbleType* arg, TypePairs* tps);
static bool TypesEqual(FbleTypeArena* arena, FbleType* a, FbleType* b, TypeIdPairs* eq);

// Add --
//   Helper function for adding types to a vector of refs.
//
// Inputs:
//   add - the add callback.
//   type - the type to add.
//
// Results:
//   none.
//
// Side effects:
//   If type is not null, the add callback is called on it.
static void Add(FbleRefCallback* add, FbleType* type)
{
  if (type != NULL) {
    add->callback(add, &type->ref);
  }
}

// TypeAdded --
//   The added function for types. See documentation in ref.h
static void TypeAdded(FbleRefCallback* add, FbleRef* ref)
{
  FbleType* type = (FbleType*)ref;
  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* st = (FbleStructType*)type;
      for (size_t i = 0; i < st->fields.size; ++i) {
        Add(add, st->fields.xs[i].type);
      }
      break;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* ut = (FbleUnionType*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        Add(add, ut->fields.xs[i].type);
      }
      break;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;
      Add(add, ft->arg);
      Add(add, ft->rtype);
      break;
    }

    case FBLE_PROC_TYPE: {
      FbleProcType* pt = (FbleProcType*)type;
      Add(add, pt->type);
      break;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pt = (FblePolyType*)type;
      Add(add, &pt->arg->_base);
      Add(add, pt->body);
      break;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      Add(add, pat->poly);
      Add(add, pat->arg);
      break;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      Add(add, var->value);
      break;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* t = (FbleTypeType*)type;
      Add(add, t->type);
      break;
    }
  }
}

//   The free function for types. See documentation in ref.h
static void TypeFree(FbleTypeArena* arena, FbleRef* ref)
{
  FbleType* type = (FbleType*)ref;
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* st = (FbleStructType*)type;
      FbleFree(arena_, st->fields.xs);
      FbleFree(arena_, st);
      break;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* ut = (FbleUnionType*)type;
      FbleFree(arena_, ut->fields.xs);
      FbleFree(arena_, ut);
      break;
    }

    case FBLE_FUNC_TYPE:
    case FBLE_PROC_TYPE:
    case FBLE_POLY_TYPE:
    case FBLE_POLY_APPLY_TYPE: {
      FbleFree(arena_, type);
      break;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      FbleKindRelease(arena_, var->kind);
      FbleFree(arena_, var);
      break;
    }

    case FBLE_TYPE_TYPE: {
      FbleFree(arena_, type);
      break;
    }
  }
}

// TypeofKind --
//   Return the kind of a typeof expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   kind - the kind of the argument to a typeof expression.
//
// Results:
//   The kind of the typeof expression.
//
// Side effects:
//   The caller is responsible for calling FbleKindRelease on the returned type when
//   it is no longer needed.
static FbleKind* TypeofKind(FbleArena* arena, FbleKind* kind)
{
  switch (kind->tag) {
    case FBLE_BASIC_KIND: {
      FbleBasicKind* basic = (FbleBasicKind*)kind;
      FbleBasicKind* typeof = FbleAlloc(arena, FbleBasicKind);
      typeof->_base.tag = FBLE_BASIC_KIND;
      typeof->_base.loc = kind->loc;
      typeof->_base.refcount = 1;
      typeof->level = basic->level + 1;
      return &typeof->_base;
    }

    case FBLE_POLY_KIND: {
      FblePolyKind* poly = (FblePolyKind*)kind;
      FblePolyKind* typeof = FbleAlloc(arena, FblePolyKind);
      typeof->_base.tag = FBLE_POLY_KIND;
      typeof->_base.loc = kind->loc;
      typeof->_base.refcount = 1;
      typeof->arg = FbleKindRetain(arena, poly->arg);
      typeof->rkind = TypeofKind(arena, poly->rkind);
      return &typeof->_base;
    }
  }
  UNREACHABLE("Should never get here");
  return NULL;
}

// Normal --
//   Compute the normal form of a type.
//
// Inputs: 
//   arena - arena to use for allocations.
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
static FbleType* Normal(FbleTypeArena* arena, FbleType* type, TypeIdList* normalizing)
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
    case FBLE_STRUCT_TYPE: return FbleTypeRetain(arena, type);
    case FBLE_UNION_TYPE: return FbleTypeRetain(arena, type);
    case FBLE_FUNC_TYPE: return FbleTypeRetain(arena, type);
    case FBLE_PROC_TYPE: return FbleTypeRetain(arena, type);

    case FBLE_POLY_TYPE: {
      FblePolyType* poly = (FblePolyType*)type;

      // eta-reduce (\x -> f x) ==> f
      FblePolyApplyType* pat = (FblePolyApplyType*)Normal(arena, poly->body, &nn);
      if (pat == NULL) {
        return NULL;
      }

      if (pat->_base.tag == FBLE_POLY_APPLY_TYPE && pat->arg == &poly->arg->_base) {
        FbleType* normal = FbleTypeRetain(arena, pat->poly);
        FbleTypeRelease(arena, &pat->_base);
        return normal;
      }

      FbleTypeRelease(arena, &pat->_base);
      return FbleTypeRetain(arena, type);
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      FblePolyType* poly = (FblePolyType*)Normal(arena, pat->poly, &nn);
      if (poly == NULL) {
        return NULL;
      }

      if (poly->_base.tag == FBLE_POLY_TYPE) {
        FbleType* subst = Subst(arena, poly->body, poly->arg, pat->arg, NULL);
        FbleType* result = Normal(arena, subst, &nn);
        FbleTypeRelease(arena, &poly->_base);
        FbleTypeRelease(arena, subst);
        return result;
      }

      // Don't bother simplifying at all if we can't do a substituion.
      FbleTypeRelease(arena, &poly->_base);
      return FbleTypeRetain(arena, type);
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      if (var->value == NULL) {
        return FbleTypeRetain(arena, type);
      }
      return Normal(arena, var->value, &nn);
    }

    case FBLE_TYPE_TYPE: return FbleTypeRetain(arena, type);
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
static bool HasParam(FbleType* type, FbleVarType* param, TypeList* visited)
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
      return HasParam(ft->arg, param, &nv)
          || HasParam(ft->rtype, param, &nv);
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
      return (var == param)
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
//   arena - arena to use for allocations.
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
static FbleType* Subst(FbleTypeArena* arena, FbleType* type, FbleVarType* param, FbleType* arg, TypePairs* tps)
{
  if (!HasParam(type, param, NULL)) {
    return FbleTypeRetain(arena, type);
  }

  FbleArena* arena_ = FbleRefArenaArena(arena);

  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* st = (FbleStructType*)type;
      FbleStructType* sst = FbleAlloc(arena_, FbleStructType);
      FbleTypeInit(arena, &sst->_base, FBLE_STRUCT_TYPE, st->_base.loc);
      sst->_base.id = st->_base.id;

      FbleVectorInit(arena_, sst->fields);
      for (size_t i = 0; i < st->fields.size; ++i) {
        FbleTaggedType* field = FbleVectorExtend(arena_, sst->fields);
        field->name = st->fields.xs[i].name;
        field->type = Subst(arena, st->fields.xs[i].type, param, arg, tps);
        FbleRefAdd(arena, &sst->_base.ref, &field->type->ref);
        FbleTypeRelease(arena, field->type);
      }
      return &sst->_base;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* ut = (FbleUnionType*)type;
      FbleUnionType* sut = FbleAlloc(arena_, FbleUnionType);
      FbleTypeInit(arena, &sut->_base, FBLE_UNION_TYPE, ut->_base.loc);
      sut->_base.id = ut->_base.id;
      FbleVectorInit(arena_, sut->fields);
      for (size_t i = 0; i < ut->fields.size; ++i) {
        FbleTaggedType* field = FbleVectorExtend(arena_, sut->fields);
        field->name = ut->fields.xs[i].name;
        field->type = Subst(arena, ut->fields.xs[i].type, param, arg, tps);
        FbleRefAdd(arena, &sut->_base.ref, &field->type->ref);
        FbleTypeRelease(arena, field->type);
      }
      return &sut->_base;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;
      FbleFuncType* sft = FbleAlloc(arena_, FbleFuncType);
      FbleTypeInit(arena, &sft->_base, FBLE_FUNC_TYPE, ft->_base.loc);
      sft->_base.id = ft->_base.id;

      sft->arg = Subst(arena, ft->arg, param, arg, tps);
      FbleRefAdd(arena, &sft->_base.ref, &sft->arg->ref);
      FbleTypeRelease(arena, sft->arg);

      sft->rtype = Subst(arena, ft->rtype, param, arg, tps);
      FbleRefAdd(arena, &sft->_base.ref, &sft->rtype->ref);
      FbleTypeRelease(arena, sft->rtype);
      return &sft->_base;
    }

    case FBLE_PROC_TYPE: {
      FbleProcType* ut = (FbleProcType*)type;
      FbleProcType* sut = FbleAlloc(arena_, FbleProcType);
      FbleTypeInit(arena, &sut->_base, FBLE_PROC_TYPE, ut->_base.loc);
      sut->_base.id = ut->_base.id;
      sut->type = Subst(arena, ut->type, param, arg, tps);
      FbleRefAdd(arena, &sut->_base.ref, &sut->type->ref);
      FbleTypeRelease(arena, sut->type);
      return &sut->_base;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pt = (FblePolyType*)type;
      FbleType* body = Subst(arena, pt->body, param, arg, tps); 

      FblePolyType* spt = FbleAlloc(arena_, FblePolyType);
      FbleTypeInit(arena, &spt->_base, FBLE_POLY_TYPE, pt->_base.loc);
      spt->_base.id = pt->_base.id;
      spt->arg = pt->arg;
      FbleRefAdd(arena, &spt->_base.ref, &spt->arg->_base.ref);
      spt->body = body;
      FbleRefAdd(arena, &spt->_base.ref, &spt->body->ref);
      assert(spt->body->tag != FBLE_TYPE_TYPE);

      FbleTypeRelease(arena, body);
      return &spt->_base;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      FbleType* poly = Subst(arena, pat->poly, param, arg, tps);
      FbleType* sarg = Subst(arena, pat->arg, param, arg, tps);

      FblePolyApplyType* spat = FbleAlloc(arena_, FblePolyApplyType);
      FbleTypeInit(arena, &spat->_base, FBLE_POLY_APPLY_TYPE, pat->_base.loc);
      spat->_base.id = pat->_base.id;
      spat->poly = poly;
      FbleRefAdd(arena, &spat->_base.ref, &spat->poly->ref);
      spat->arg = sarg;
      FbleRefAdd(arena, &spat->_base.ref, &spat->arg->ref);
      assert(spat->poly->tag != FBLE_TYPE_TYPE);

      FbleTypeRelease(arena, poly);
      FbleTypeRelease(arena, sarg);
      return &spat->_base;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      if (var->value == NULL) {
        return FbleTypeRetain(arena, (var == param ? arg : type));
      }

      // Check to see if we've already done substitution on the value pointed
      // to by this ref.
      for (TypePairs* tp = tps; tp != NULL; tp = tp->next) {
        if (tp->a == var->value) {
          return FbleTypeRetain(arena, tp->b);
        }
      }

      FbleVarType* svar = FbleAlloc(arena_, FbleVarType);
      FbleTypeInit(arena, &svar->_base, FBLE_VAR_TYPE, type->loc);
      svar->_base.id = type->id;
      svar->name = var->name;
      svar->kind = FbleKindRetain(arena_, var->kind);
      svar->value = NULL;

      TypePairs ntp = {
        .a = var->value,
        .b = &svar->_base,
        .next = tps
      };

      FbleType* value = Subst(arena, var->value, param, arg, &ntp);
      svar->value = value;
      FbleRefAdd(arena, &svar->_base.ref, &svar->value->ref);
      FbleTypeRelease(arena, &svar->_base);
      return value;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* tt = (FbleTypeType*)type;
      FbleTypeType* stt = FbleAlloc(arena_, FbleTypeType);
      FbleTypeInit(arena, &stt->_base, FBLE_TYPE_TYPE, tt->_base.loc);
      stt->_base.id = tt->_base.id;
      stt->type = Subst(arena, tt->type, param, arg, tps);
      FbleRefAdd(arena, &stt->_base.ref, &stt->type->ref);
      FbleTypeRelease(arena, stt->type);
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
//   arena - arena to use for allocations
//   a - the first type
//   b - the second type
//   eq - A set of pairs of type ids that should be assumed to be equal
//
// Results:
//   True if the first type equals the second type, false otherwise.
//
// Side effects:
//   None.
static bool TypesEqual(FbleTypeArena* arena, FbleType* a, FbleType* b, TypeIdPairs* eq)
{
  a = FbleNormalType(arena, a);
  b = FbleNormalType(arena, b);

  for (TypeIdPairs* pairs = eq; pairs != NULL; pairs = pairs->next) {
    if (a->id == pairs->a && b->id == pairs->b) {
      FbleTypeRelease(arena, a);
      FbleTypeRelease(arena, b);
      return true;
    }
  }

  TypeIdPairs neq = {
    .a = a->id,
    .b = b->id,
    .next = eq,
  };

  if (a->tag != b->tag) {
    FbleTypeRelease(arena, a);
    FbleTypeRelease(arena, b);
    return false;
  }

  switch (a->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* sta = (FbleStructType*)a;
      FbleStructType* stb = (FbleStructType*)b;

      if (sta->fields.size != stb->fields.size) {
        FbleTypeRelease(arena, a);
        FbleTypeRelease(arena, b);
        return false;
      }

      for (size_t i = 0; i < sta->fields.size; ++i) {
        if (!FbleNamesEqual(&sta->fields.xs[i].name, &stb->fields.xs[i].name)) {
          FbleTypeRelease(arena, a);
          FbleTypeRelease(arena, b);
          return false;
        }

        if (!TypesEqual(arena, sta->fields.xs[i].type, stb->fields.xs[i].type, &neq)) {
          FbleTypeRelease(arena, a);
          FbleTypeRelease(arena, b);
          return false;
        }
      }

      FbleTypeRelease(arena, a);
      FbleTypeRelease(arena, b);
      return true;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* uta = (FbleUnionType*)a;
      FbleUnionType* utb = (FbleUnionType*)b;
      if (uta->fields.size != utb->fields.size) {
        FbleTypeRelease(arena, a);
        FbleTypeRelease(arena, b);
        return false;
      }

      for (size_t i = 0; i < uta->fields.size; ++i) {
        if (!FbleNamesEqual(&uta->fields.xs[i].name, &utb->fields.xs[i].name)) {
          FbleTypeRelease(arena, a);
          FbleTypeRelease(arena, b);
          return false;
        }

        if (!TypesEqual(arena, uta->fields.xs[i].type, utb->fields.xs[i].type, &neq)) {
          FbleTypeRelease(arena, a);
          FbleTypeRelease(arena, b);
          return false;
        }
      }

      FbleTypeRelease(arena, a);
      FbleTypeRelease(arena, b);
      return true;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* fta = (FbleFuncType*)a;
      FbleFuncType* ftb = (FbleFuncType*)b;
      bool result = TypesEqual(arena, fta->arg, ftb->arg, &neq)
                 && TypesEqual(arena, fta->rtype, ftb->rtype, &neq);
      FbleTypeRelease(arena, a);
      FbleTypeRelease(arena, b);
      return result;
    }

    case FBLE_PROC_TYPE: {
      FbleProcType* uta = (FbleProcType*)a;
      FbleProcType* utb = (FbleProcType*)b;
      bool result = TypesEqual(arena, uta->type, utb->type, &neq);
      FbleTypeRelease(arena, a);
      FbleTypeRelease(arena, b);
      return result;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pta = (FblePolyType*)a;
      FblePolyType* ptb = (FblePolyType*)b;

      // TODO: Verify the args have matching kinds.
      if (!FbleKindsEqual(pta->arg->kind, ptb->arg->kind)) {
        FbleTypeRelease(arena, a);
        FbleTypeRelease(arena, b);
        return false;
      }
  
      TypeIdPairs pneq = {
        .a = pta->arg->_base.id,
        .b = ptb->arg->_base.id,
        .next = &neq
      };
      bool result = TypesEqual(arena, pta->body, ptb->body, &pneq);
      FbleTypeRelease(arena, a);
      FbleTypeRelease(arena, b);
      return result;
    }

    case FBLE_POLY_APPLY_TYPE: {
      UNREACHABLE("poly apply type is not Normal");
      FbleTypeRelease(arena, a);
      FbleTypeRelease(arena, b);
      return false;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* va = (FbleVarType*)a;
      FbleVarType* vb = (FbleVarType*)b;

      assert(va->value == NULL && vb->value == NULL);
      FbleTypeRelease(arena, a);
      FbleTypeRelease(arena, b);
      return a == b;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* tta = (FbleTypeType*)a;
      FbleTypeType* ttb = (FbleTypeType*)b;
      bool result = TypesEqual(arena, tta->type, ttb->type, &neq);
      FbleTypeRelease(arena, a);
      FbleTypeRelease(arena, b);
      return result;
    }
  }

  UNREACHABLE("Should never get here");
  FbleTypeRelease(arena, a);
  FbleTypeRelease(arena, b);
  return false;
}

// FbleGetKind -- see documentation in fble-type.h
FbleKind* FbleGetKind(FbleArena* arena, FbleType* type)
{
  switch (type->tag) {
    case FBLE_STRUCT_TYPE:
    case FBLE_UNION_TYPE:
    case FBLE_FUNC_TYPE:
    case FBLE_PROC_TYPE: {
      FbleBasicKind* kind = FbleAlloc(arena, FbleBasicKind);
      kind->_base.tag = FBLE_BASIC_KIND;
      kind->_base.loc = type->loc;
      kind->_base.refcount = 1;
      kind->level = 1;
      return &kind->_base;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* poly = (FblePolyType*)type;
      FblePolyKind* kind = FbleAlloc(arena, FblePolyKind);
      kind->_base.tag = FBLE_POLY_KIND;
      kind->_base.loc = type->loc;
      kind->_base.refcount = 1;
      kind->arg = FbleGetKind(arena, &poly->arg->_base);
      kind->rkind = FbleGetKind(arena, poly->body);
      return &kind->_base;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      FblePolyKind* kind = (FblePolyKind*)FbleGetKind(arena, pat->poly);
      assert(kind->_base.tag == FBLE_POLY_KIND);

      FbleKind* rkind = FbleKindRetain(arena, kind->rkind);
      FbleKindRelease(arena, &kind->_base);
      return rkind;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      return FbleKindRetain(arena, var->kind);
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* type_type = (FbleTypeType*)type;

      FbleKind* arg_kind = FbleGetKind(arena, type_type->type);
      FbleKind* kind = TypeofKind(arena, arg_kind);
      FbleKindRelease(arena, arg_kind);
      return kind;
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// FbleGetKindLevel -- see documentation in fble-type.h
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

// FbleKindsEqual -- see documentation in fble-type.h
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

// FblePrintKind -- see documentation in fble-type.h
void FblePrintKind(FbleKind* kind)
{
  switch (kind->tag) {
    case FBLE_BASIC_KIND: {
      FbleBasicKind* basic = (FbleBasicKind*)kind;
      if (basic->level == 1) {
        fprintf(stderr, "@");
      } else {
        // TODO: Will an end user ever see this?
        fprintf(stderr, "@%i", basic->level);
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

// FbleNewTypeArena -- see documentation in fble-types.h
FbleTypeArena* FbleNewTypeArena(FbleArena* arena)
{
  return FbleNewRefArena(arena, &TypeFree, &TypeAdded);
}

// FbleFreeTypeArena -- see documentation in fble-types.h
void FbleFreeTypeArena(FbleTypeArena* arena)
{
  FbleDeleteRefArena(arena);
}

// FbleTypeInit -- see documentation in fble-type.h
void FbleTypeInit(FbleTypeArena* arena, FbleType* type, FbleTypeTag tag, FbleLoc loc)
{
  FbleRefInit(arena, &type->ref);
  type->tag = tag;
  type->loc = loc;
  type->id = type->ref.id;
}

// FbleTypeRetain -- see documentation in fble-type.h
FbleType* FbleTypeRetain(FbleTypeArena* arena, FbleType* type)
{
  if (type != NULL) {
    FbleRefRetain(arena, &type->ref);
  }
  return type;
}

// FbleTypeRelease -- see documentation in fble-type.h
void FbleTypeRelease(FbleTypeArena* arena, FbleType* type)
{
  if (type != NULL) {
    FbleRefRelease(arena, &type->ref);
  }
}

// FbleNewPolyType -- see documentation in fble-type.h
FbleType* FbleNewPolyType(FbleTypeArena* arena, FbleLoc loc, FbleVarType* arg, FbleType* body)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  if (body->tag == FBLE_TYPE_TYPE) {
    // \arg -> typeof(body) = typeof(\arg -> body)
    FbleTypeType* ttbody = (FbleTypeType*)body;
    FbleTypeType* tt = FbleAlloc(arena_, FbleTypeType);
    FbleTypeInit(arena, &tt->_base, FBLE_TYPE_TYPE, loc);
    tt->type = FbleNewPolyType(arena, loc, arg, ttbody->type);
    FbleRefAdd(arena, &tt->_base.ref, &tt->type->ref);
    FbleTypeRelease(arena, tt->type);
    return &tt->_base;
  }

  FblePolyType* pt = FbleAlloc(arena_, FblePolyType);
  FbleTypeInit(arena, &pt->_base, FBLE_POLY_TYPE, loc);
  pt->arg = arg;
  FbleRefAdd(arena, &pt->_base.ref, &pt->arg->_base.ref);
  pt->body = body;
  FbleRefAdd(arena, &pt->_base.ref, &pt->body->ref);

  assert(pt->body->tag != FBLE_TYPE_TYPE);
  return &pt->_base;
}

// FbleNewPolyApplyType -- see documentation in fble-type.h
FbleType* FbleNewPolyApplyType(FbleTypeArena* arena, FbleLoc loc, FbleType* poly, FbleType* arg)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  if (poly->tag == FBLE_TYPE_TYPE) {
    // typeof(poly)<arg> == typeof(poly<arg>)
    FbleTypeType* ttpoly = (FbleTypeType*)poly;
    FbleTypeType* tt = FbleAlloc(arena_, FbleTypeType);
    FbleTypeInit(arena, &tt->_base, FBLE_TYPE_TYPE, loc);
    tt->type = FbleNewPolyApplyType(arena, loc, ttpoly->type, arg);
    FbleRefAdd(arena, &tt->_base.ref, &tt->type->ref);
    FbleTypeRelease(arena, tt->type);
    return &tt->_base;
  }

  FblePolyApplyType* pat = FbleAlloc(arena_, FblePolyApplyType);
  FbleTypeInit(arena, &pat->_base, FBLE_POLY_APPLY_TYPE, loc);
  pat->poly = poly;
  FbleRefAdd(arena, &pat->_base.ref, &pat->poly->ref);
  pat->arg = arg;
  FbleRefAdd(arena, &pat->_base.ref, &pat->arg->ref);

  assert(pat->poly->tag != FBLE_TYPE_TYPE);
  return &pat->_base;
}

// FbleTypeIsVacuous -- see documentation in fble-type.h
bool FbleTypeIsVacuous(FbleTypeArena* arena, FbleType* type)
{
  FbleType* normal = Normal(arena, type, NULL);
  while (normal != NULL && normal->tag == FBLE_POLY_TYPE) {
    FblePolyType* poly = (FblePolyType*)normal;
    FbleType* tmp = normal;
    normal = Normal(arena, poly->body, NULL);
    FbleTypeRelease(arena, tmp);
  }
  FbleTypeRelease(arena, normal);
  return normal == NULL;
}

// FbleNormalType -- see documentation in fble-type.h
FbleType* FbleNormalType(FbleTypeArena* arena, FbleType* type)
{
  FbleType* normal = Normal(arena, type, NULL);
  assert(normal != NULL && "vacuous type does not have a normal form");
  return normal;
}

// FbleTypesEqual -- see documentation in fble-types.h
bool FbleTypesEqual(FbleTypeArena* arena, FbleType* a, FbleType* b)
{
  return TypesEqual(arena, a, b, NULL);
}

// FblePrintType -- see documentation in fble-type.h
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
      FblePrintType(arena, ft->arg);
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
        FbleKind* kind = FbleGetKind(arena, &pt->arg->_base);
        FblePrintKind(kind);
        FbleKindRelease(arena, kind);
        fprintf(stderr, " ");
        FblePrintType(arena, &pt->arg->_base);
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
