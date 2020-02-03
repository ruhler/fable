
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

// PolyApplyList --
//   A linked list of cached poly apply results.
typedef struct PolyApplyList {
  FbleType* poly;
  FbleType* arg;
  FbleType* result;
  struct PolyApplyList* next;
} PolyApplyList;

// TypePairs --
//   A set of pairs of types.
typedef struct TypePairs {
  FbleType* a;
  FbleType* b;
  struct TypePairs* next;
} TypePairs;

static void Add(FbleRefCallback* add, FbleType* type);
static void TypeAdded(FbleRefCallback* add, FbleRef* ref);
static void TypeFree(FbleTypeArena* arena, FbleRef* ref);

static FbleKind* TypeofKind(FbleArena* arena, FbleKind* kind);

static bool HasParam(FbleType* type, FbleType* param, TypeList* visited);
static FbleType* Subst(FbleTypeArena* arena, FbleType* src, FbleType* param, FbleType* arg, TypePairs* tps);
static void Eval(FbleTypeArena* arena, FbleType* type, PolyApplyList* applied);
static bool TypesEqual(FbleTypeArena* arena, FbleType* a, FbleType* b, TypePairs* eq);

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
      Add(add, pt->arg);
      Add(add, pt->body);
      break;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      Add(add, pat->poly);
      Add(add, pat->arg);
      Add(add, pat->result);
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
//   type when it is no longer needed.
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
static FbleType* Subst(FbleTypeArena* arena, FbleType* type, FbleType* param, FbleType* arg, TypePairs* tps)
{
  if (!HasParam(type, param, NULL)) {
    return FbleTypeRetain(arena, type);
  }

  FbleArena* arena_ = FbleRefArenaArena(arena);

  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* st = (FbleStructType*)type;
      FbleStructType* sst = FbleAlloc(arena_, FbleStructType);
      FbleRefInit(arena, &sst->_base.ref);
      sst->_base.tag = FBLE_STRUCT_TYPE;
      sst->_base.loc = st->_base.loc;
      sst->_base.evaluating = false;

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
      FbleRefInit(arena, &sut->_base.ref);
      sut->_base.tag = FBLE_UNION_TYPE;
      sut->_base.loc = ut->_base.loc;
      sut->_base.evaluating = false;
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
      FbleRefInit(arena, &sft->_base.ref);
      sft->_base.tag = FBLE_FUNC_TYPE;
      sft->_base.loc = ft->_base.loc;
      sft->_base.evaluating = false;

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
      FbleRefInit(arena, &sut->_base.ref);
      sut->_base.tag = ut->_base.tag;
      sut->_base.loc = ut->_base.loc;
      sut->_base.evaluating = false;
      sut->type = Subst(arena, ut->type, param, arg, tps);
      FbleRefAdd(arena, &sut->_base.ref, &sut->type->ref);
      FbleTypeRelease(arena, sut->type);
      return &sut->_base;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pt = (FblePolyType*)type;
      FbleType* body = Subst(arena, pt->body, param, arg, tps); 
      FbleType* spt = FbleNewPolyType(arena, pt->_base.loc, pt->arg, body);
      FbleTypeRelease(arena, body);
      return spt;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;

      FbleType* poly = Subst(arena, pat->poly, param, arg, tps);
      FbleType* sarg = Subst(arena, pat->arg, param, arg, tps);

      FbleType* spat = FbleNewPolyApplyType(arena, pat->_base.loc, poly, sarg);
      FbleTypeRelease(arena, poly);
      FbleTypeRelease(arena, sarg);
      return spat;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      if (var->value == NULL) {
        return FbleTypeRetain(arena, (type == param ? arg : type));
      }

      // Check to see if we've already done substitution on the value pointed
      // to by this ref.
      for (TypePairs* tp = tps; tp != NULL; tp = tp->next) {
        if (tp->a == var->value) {
          return FbleTypeRetain(arena, tp->b);
        }
      }

      FbleVarType* svar = FbleAlloc(arena_, FbleVarType);
      FbleRefInit(arena, &svar->_base.ref);
      svar->_base.tag = FBLE_VAR_TYPE;
      svar->_base.loc = type->loc;
      svar->_base.evaluating = false;
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
      FbleRefInit(arena, &stt->_base.ref);
      stt->_base.tag = FBLE_TYPE_TYPE;
      stt->_base.loc = tt->_base.loc;
      stt->_base.evaluating = false;
      stt->type = Subst(arena, tt->type, param, arg, tps);
      FbleRefAdd(arena, &stt->_base.ref, &stt->type->ref);
      FbleTypeRelease(arena, stt->type);
      return &stt->_base;
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// Eval --
//   Evaluate the given type in place. After evaluation there are no more
//   unevaluated poly apply types that can be applied.
//
//   Does nothing if the type is currently being evaluated, to prevent
//   infinite recursion.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the type to evaluate. May be NULL.
//   applied - cache of poly applications already evaluated, to avoid infinite
//             recursion. External callers should pass NULL for this argument.
//
// Results:
//   None.
//
// Side effects:
//   The type is evaluated in place. The type is marked as being evaluated for
//   the duration of the evaluation.
//
// Design notes:
// * Example of infinite recursion through evaluation:
//     @ X@ = +(Unit@ a, X@ b);
//     Eval(X@)
// * Example of infinite recursion through poly application:
//     <@>@ F@ = <@ T@> { +(T@ a, F@<T@> b); };
//     Eval(F@<Unit@>)
//   If we don't cache poly applications, we would end up with the infinite
//   type:
//     +(Unit@ a, +(Unit@ a, +(Unit@ a, ...)) b)
//   Which would take an infinite amount of time to construct.
static void Eval(FbleTypeArena* arena, FbleType* type, PolyApplyList* applied)
{
  if (type == NULL || type->evaluating) {
    return;
  }

  type->evaluating = true;

  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* st = (FbleStructType*)type;
      for (size_t i = 0; i < st->fields.size; ++i) {
        Eval(arena, st->fields.xs[i].type, applied);
      }
      break;
    }

    case FBLE_UNION_TYPE: {
      FbleUnionType* ut = (FbleUnionType*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        Eval(arena, ut->fields.xs[i].type, applied);
      }
      break;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;
      Eval(arena, ft->arg, applied);
      Eval(arena, ft->rtype, applied);
      break;
    }

    case FBLE_PROC_TYPE: {
      FbleProcType* ut = (FbleProcType*)type;
      Eval(arena, ut->type, applied);
      break;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pt = (FblePolyType*)type;
      Eval(arena, pt->body, applied);
      break;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;

      Eval(arena, pat->poly, applied);
      Eval(arena, pat->arg, applied);

      if (pat->result != NULL) {
        Eval(arena, pat->result, applied);
        break;
      }

      // Check whether we have already applied the arg to this poly.
      bool have_applied = false;
      for (PolyApplyList* pal = applied; pal != NULL; pal = pal->next) {
        if (FbleTypesEqual(arena, pal->poly, pat->poly)) {
          if (FbleTypesEqual(arena, pal->arg, pat->arg)) {
            // TODO: This feels like a hacky workaround to a bug that may not
            // catch all cases. Double check that this is the best way to
            // test that pat and pal refer to entities that are the same or
            // subst derived from one another.
            if (&pat->_base != pal->result) {
              pat->result = pal->result;
              assert(pat->result != NULL);
              assert(&pat->_base != pat->result);
              FbleRefAdd(arena, &pat->_base.ref, &pat->result->ref);
              have_applied = true;
              break;
            }
          }
        }
      }

      if (have_applied) {
        break;
      }

      FblePolyType* poly = (FblePolyType*)FbleNormalType(arena, pat->poly);
      if (poly->_base.tag == FBLE_POLY_TYPE) {
        pat->result = Subst(arena, poly->body, poly->arg, pat->arg, NULL);
        assert(pat->result != NULL);
        assert(&pat->_base != pat->result);
        FbleRefAdd(arena, &pat->_base.ref, &pat->result->ref);
        FbleTypeRelease(arena, pat->result);

        PolyApplyList napplied = {
          .poly = &poly->_base,
          .arg = pat->arg,
          .result = pat->result,
          .next = applied
        };

        Eval(arena, pat->result, &napplied);
      }
      FbleTypeRelease(arena, &poly->_base);
      break;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;

      if (var->value != NULL) {
        Eval(arena, var->value, applied);
      }
      break;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* tt = (FbleTypeType*)type;
      Eval(arena, tt->type, applied);
      break;
    }
  }

  type->evaluating = false;
}

// TypesEqual --
//   Test whether the two given evaluated types are equal.
//
// Inputs:
//   arena - arena to use for allocations
//   a - the first type
//   b - the second type
//   eq - A set of pairs of types that should be assumed to be equal
//
// Results:
//   True if the first type equals the second type, false otherwise.
//
// Side effects:
//   None.
static bool TypesEqual(FbleTypeArena* arena, FbleType* a, FbleType* b, TypePairs* eq)
{
  if (a == b) {
    return true;
  }

  for (TypePairs* pairs = eq; pairs != NULL; pairs = pairs->next) {
    if (a == pairs->a && b == pairs->b) {
      return true;
    }
  }

  TypePairs neq = {
    .a = a,
    .b = b,
    .next = eq,
  };

  if (a->tag == FBLE_VAR_TYPE) {
    FbleVarType* va = (FbleVarType*)a;
    if (va->value != NULL) {
      return TypesEqual(arena, va->value, b, &neq);
    }
  }

  if (b->tag == FBLE_VAR_TYPE) {
    FbleVarType* vb = (FbleVarType*)b;
    if (vb->value != NULL) {
      return TypesEqual(arena, a, vb->value, &neq);
    }
  }

  switch (a->tag) {
    case FBLE_STRUCT_TYPE: {
      switch (b->tag) {
        case FBLE_STRUCT_TYPE: {
          FbleStructType* sta = (FbleStructType*)a;
          FbleStructType* stb = (FbleStructType*)b;

          if (sta->fields.size != stb->fields.size) {
            return false;
          }

          for (size_t i = 0; i < sta->fields.size; ++i) {
            if (!FbleNamesEqual(&sta->fields.xs[i].name, &stb->fields.xs[i].name)) {
              return false;
            }

            if (!TypesEqual(arena, sta->fields.xs[i].type, stb->fields.xs[i].type, &neq)) {
              return false;
            }
          }
          return true;
        }

        case FBLE_UNION_TYPE: return false;
        case FBLE_FUNC_TYPE: return false;
        case FBLE_PROC_TYPE: return false;
        case FBLE_POLY_TYPE: return false;
        case FBLE_POLY_APPLY_TYPE: assert(false && "TODO"); return false;
        case FBLE_VAR_TYPE: assert(false && "TODO"); return false;
        case FBLE_TYPE_TYPE: return false;
      }
      UNREACHABLE("should never get here");
      return false;
    }

    case FBLE_UNION_TYPE: {
      switch (b->tag) {
        case FBLE_STRUCT_TYPE: return false;
        case FBLE_UNION_TYPE: {
          FbleUnionType* uta = (FbleUnionType*)a;
          FbleUnionType* utb = (FbleUnionType*)b;
          if (uta->fields.size != utb->fields.size) {
            return false;
          }

          for (size_t i = 0; i < uta->fields.size; ++i) {
            if (!FbleNamesEqual(&uta->fields.xs[i].name, &utb->fields.xs[i].name)) {
              return false;
            }

            if (!TypesEqual(arena, uta->fields.xs[i].type, utb->fields.xs[i].type, &neq)) {
              return false;
            }
          }

          return true;
        }

        case FBLE_FUNC_TYPE: return false;
        case FBLE_PROC_TYPE: return false;
        case FBLE_POLY_TYPE: return false;
        case FBLE_POLY_APPLY_TYPE: assert(false && "TODO"); return false;
        case FBLE_VAR_TYPE: assert(false && "TODO"); return false;
        case FBLE_TYPE_TYPE: return false;
      }
      UNREACHABLE("should never get here");
      return false;
    }

    case FBLE_FUNC_TYPE: {
      switch (b->tag) {
        case FBLE_STRUCT_TYPE: return false;
        case FBLE_UNION_TYPE: return false;
        case FBLE_FUNC_TYPE: {
          FbleFuncType* fta = (FbleFuncType*)a;
          FbleFuncType* ftb = (FbleFuncType*)b;
          return TypesEqual(arena, fta->arg, ftb->arg, &neq)
              && TypesEqual(arena, fta->rtype, ftb->rtype, &neq);
        }
        case FBLE_PROC_TYPE: return false;
        case FBLE_POLY_TYPE: return false;
        case FBLE_POLY_APPLY_TYPE: assert(false && "TODO"); return false;
        case FBLE_VAR_TYPE: assert(false && "TODO"); return false;
        case FBLE_TYPE_TYPE: return false;
      }
      UNREACHABLE("should never get here");
      return false;
    }

    case FBLE_PROC_TYPE: {
      switch (b->tag) {
        case FBLE_STRUCT_TYPE: return false;
        case FBLE_UNION_TYPE: return false;
        case FBLE_FUNC_TYPE: return false;
        case FBLE_PROC_TYPE: {
          FbleProcType* uta = (FbleProcType*)a;
          FbleProcType* utb = (FbleProcType*)b;
          return TypesEqual(arena, uta->type, utb->type, &neq);
        }
        case FBLE_POLY_TYPE: return false;
        case FBLE_POLY_APPLY_TYPE: assert(false && "TODO"); return false;
        case FBLE_VAR_TYPE: assert(false && "TODO"); return false;
        case FBLE_TYPE_TYPE: return false;
      }
      UNREACHABLE("should never get here");
      return false;
    }

    case FBLE_POLY_TYPE: {
      switch (b->tag) {
        case FBLE_STRUCT_TYPE: return false;
        case FBLE_UNION_TYPE: return false;
        case FBLE_FUNC_TYPE: return false;
        case FBLE_PROC_TYPE: return false;
        case FBLE_POLY_TYPE: {
          FblePolyType* pta = (FblePolyType*)a;
          FblePolyType* ptb = (FblePolyType*)b;

          // TODO: Verify the args have matching kinds.

          TypePairs pneq = {
            .a = pta->arg,
            .b = ptb->arg,
            .next = &neq
          };
          return TypesEqual(arena, pta->body, ptb->body, &pneq);
        }
        case FBLE_POLY_APPLY_TYPE: assert(false && "TODO"); return false;
        case FBLE_VAR_TYPE: assert(false && "TODO"); return false;
        case FBLE_TYPE_TYPE: return false;
      }
      UNREACHABLE("should never get here");
      return false;
    }

    case FBLE_POLY_APPLY_TYPE: {
      assert(false && "TODO");
      return false;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* va = (FbleVarType*)a;
      assert(va->value == NULL);
      assert(false && "TODO: TypeEquals for abstract var.");
      return false;
    }

    case FBLE_TYPE_TYPE: {
      if (b->tag != FBLE_TYPE_TYPE) {
        return false;
      }

      FbleTypeType* tta = (FbleTypeType*)a;
      FbleTypeType* ttb = (FbleTypeType*)b;
      return TypesEqual(arena, tta->type, ttb->type, &neq);
    }
  }

  UNREACHABLE("Should never get here");
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
      kind->arg = FbleGetKind(arena, poly->arg);
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
FbleType* FbleNewPolyType(FbleTypeArena* arena, FbleLoc loc, FbleType* arg, FbleType* body)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  if (body->tag == FBLE_TYPE_TYPE) {
    // \arg -> typeof(body) = typeof(\arg -> body)
    FbleTypeType* ttbody = (FbleTypeType*)body;
    FbleTypeType* tt = FbleAlloc(arena_, FbleTypeType);
    FbleRefInit(arena, &tt->_base.ref);
    tt->_base.tag = FBLE_TYPE_TYPE;
    tt->_base.loc = loc;
    tt->_base.evaluating = false;
    tt->type = FbleNewPolyType(arena, loc, arg, ttbody->type);
    FbleRefAdd(arena, &tt->_base.ref, &tt->type->ref);
    FbleTypeRelease(arena, tt->type);
    return &tt->_base;
  }

  FblePolyType* pt = FbleAlloc(arena_, FblePolyType);
  pt->_base.tag = FBLE_POLY_TYPE;
  pt->_base.loc = loc;
  pt->_base.evaluating = false;
  FbleRefInit(arena, &pt->_base.ref);
  pt->arg = arg;
  FbleRefAdd(arena, &pt->_base.ref, &pt->arg->ref);
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
    FbleRefInit(arena, &tt->_base.ref);
    tt->_base.tag = FBLE_TYPE_TYPE;
    tt->_base.loc = loc;
    tt->_base.evaluating = false;
    tt->type = FbleNewPolyApplyType(arena, loc, ttpoly->type, arg);
    FbleRefAdd(arena, &tt->_base.ref, &tt->type->ref);
    FbleTypeRelease(arena, tt->type);
    return &tt->_base;
  }

  FblePolyApplyType* pat = FbleAlloc(arena_, FblePolyApplyType);
  FbleRefInit(arena, &pat->_base.ref);
  pat->_base.tag = FBLE_POLY_APPLY_TYPE;
  pat->_base.loc = loc;
  pat->_base.evaluating = false;
  pat->poly = poly;
  FbleRefAdd(arena, &pat->_base.ref, &pat->poly->ref);
  pat->arg = arg;
  FbleRefAdd(arena, &pat->_base.ref, &pat->arg->ref);
  pat->result = NULL;

  assert(pat->poly->tag != FBLE_TYPE_TYPE);
  return &pat->_base;
}

// FbleNormalType -- see documentation in fble-type.h
FbleType* FbleNormalType(FbleTypeArena* arena, FbleType* type)
{
  switch (type->tag) {
    case FBLE_STRUCT_TYPE: return FbleTypeRetain(arena, type);
    case FBLE_UNION_TYPE: return FbleTypeRetain(arena, type);
    case FBLE_FUNC_TYPE: return FbleTypeRetain(arena, type);
    case FBLE_PROC_TYPE: return FbleTypeRetain(arena, type);

    case FBLE_POLY_TYPE: {
      // Normalize: (\x -> f x) to f
      // TODO: Does this cover all the cases? It seems like overly specific
      // pattern matching.
      FbleType* result = FbleTypeRetain(arena, type);
      FblePolyType* poly = (FblePolyType*)type;
      FblePolyApplyType* pat = (FblePolyApplyType*)FbleNormalType(arena, poly->body);
      if (pat->_base.tag == FBLE_POLY_APPLY_TYPE) {
        FbleType* arg = FbleNormalType(arena, pat->arg);
        if (poly->arg == arg) {
          FbleTypeRelease(arena, result);
          result = FbleNormalType(arena, pat->poly);
        }
        FbleTypeRelease(arena, arg);
      }
      FbleTypeRelease(arena, &pat->_base);
      return result;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      if (pat->result == NULL) {
        return FbleTypeRetain(arena, type);
      }
      return FbleNormalType(arena, pat->result);
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      if (var->value == NULL) {
        return FbleTypeRetain(arena, type);
      }
      return FbleNormalType(arena, var->value);
    }

    case FBLE_TYPE_TYPE: return FbleTypeRetain(arena, type);
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// FbleEvalType -- see documentation in fble-types.h
void FbleEvalType(FbleTypeArena* arena, FbleType* type)
{
  Eval(arena, type, NULL);
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
      break;
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
      fprintf(stderr, ")");
      break;
    }

    case FBLE_FUNC_TYPE: {
      FbleFuncType* ft = (FbleFuncType*)type;
      fprintf(stderr, "(");
      FblePrintType(arena, ft->arg);
      fprintf(stderr, "){");
      FblePrintType(arena, ft->rtype);
      fprintf(stderr, ";}");
      break;
    }

    case FBLE_PROC_TYPE: {
      FbleProcType* ut = (FbleProcType*)type;
      FblePrintType(arena, ut->type);
      fprintf(stderr, "!");
      break;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* pt = (FblePolyType*)type;
      fprintf(stderr, "<");
      FbleKind* kind = FbleGetKind(arena, pt->arg);
      FblePrintKind(kind);
      FbleKindRelease(arena, kind);
      fprintf(stderr, " ");
      FblePrintType(arena, pt->arg);
      fprintf(stderr, "> { ");
      FblePrintType(arena, pt->body);
      fprintf(stderr, "; }");
      break;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* pat = (FblePolyApplyType*)type;
      FblePrintType(arena, pat->poly);
      fprintf(stderr, "<");
      FblePrintType(arena, pat->arg);
      fprintf(stderr, ">");
      break;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var = (FbleVarType*)type;
      FblePrintName(stderr, &var->name);
      break;
    }

    case FBLE_TYPE_TYPE: {
      FbleTypeType* tt = (FbleTypeType*)type;
      fprintf(stderr, "@<");
      FblePrintType(arena, tt->type);
      fprintf(stderr, ">");
      break;
    }
  }
}
