
#include <assert.h>   // for assert

#include "fble-type.h"

#define UNREACHABLE(x) assert(false && x)

// TypeList --
//   A linked list of types.
typedef struct TypeList {
  Type* type;
  struct TypeList* next;
} TypeList;

// PolyApplyList --
//   A linked list of cached poly apply results.
typedef struct PolyApplyList {
  Type* poly;
  Type* arg;
  Type* result;
  struct PolyApplyList* next;
} PolyApplyList;

// TypePairs --
//   A set of pairs of types.
typedef struct TypePairs {
  Type* a;
  Type* b;
  struct TypePairs* next;
} TypePairs;

static Kind* CopyKind(FbleArena* arena, Kind* kind);
static void Add(FbleRefCallback* add, Type* type);

static void TypeFree(FbleTypeArena* arena, FbleRef* ref);
static void TypeAdded(FbleRefCallback* add, FbleRef* ref);
static bool TypesEqual(Type* a, Type* b, TypePairs* eq);

static Kind* TypeofKind(FbleArena* arena, Kind* kind);
static bool HasParam(Type* type, Type* param, TypeList* visited);
static Type* Subst(FbleTypeArena* arena, Type* src, Type* param, Type* arg, TypePairs* tps);
static void Eval(FbleTypeArena* arena, Type* type, PolyApplyList* applied);

// FreeKind -- see documentation in fble-type.h
void FreeKind(FbleArena* arena, Kind* kind)
{
  if (kind != NULL) {
    assert(kind->refcount > 0);
    kind->refcount--;
    if (kind->refcount == 0) {
      switch (kind->tag) {
        case BASIC_KIND: {
          FbleFree(arena, kind);
          break;
        }

        case POLY_KIND: {
          PolyKind* poly = (PolyKind*)kind;
          FreeKind(arena, poly->arg);
          FreeKind(arena, poly->rkind);
          FbleFree(arena, poly);
          break;
        }
      }
    }
  }
}

//   The free function for types. See documentation in ref.h
static void TypeFree(FbleTypeArena* arena, FbleRef* ref)
{
  Type* type = (Type*)ref;
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      FbleFree(arena_, st->fields.xs);
      FbleFree(arena_, st);
      break;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      FbleFree(arena_, ut->fields.xs);
      FbleFree(arena_, ut);
      break;
    }

    case FUNC_TYPE:
    case PROC_TYPE:
    case POLY_TYPE:
    case POLY_APPLY_TYPE: {
      FbleFree(arena_, type);
      break;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      FreeKind(arena_, var->kind);
      FbleFree(arena_, var);
      break;
    }

    case TYPE_TYPE: {
      FbleFree(arena_, type);
      break;
    }
  }
}

// CopyKind --
//   Makes a (refcount) copy of a compiled kind.
//
// Inputs:
//   arena - for allocations.
//   kind - the kind to copy.
//
// Results:
//   The copied kind.
//
// Side effects:
//   The returned kind must be freed using FreeKind when no longer in use.
static Kind* CopyKind(FbleArena* arena, Kind* kind)
{
  assert(kind != NULL);
  kind->refcount++;
  return kind;
}

// FbleTypeRetain -- see documentation in fble-type.h
Type* FbleTypeRetain(FbleTypeArena* arena, Type* type)
{
  if (type != NULL) {
    FbleRefRetain(arena, &type->ref);
  }
  return type;
}

// FbleTypeRelease -- see documentation in fble-type.h
void FbleTypeRelease(FbleTypeArena* arena, Type* type)
{
  if (type != NULL) {
    FbleRefRelease(arena, &type->ref);
  }
}

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
static void Add(FbleRefCallback* add, Type* type)
{
  if (type != NULL) {
    add->callback(add, &type->ref);
  }
}

// TypeAdded --
//   The added function for types. See documentation in ref.h
static void TypeAdded(FbleRefCallback* add, FbleRef* ref)
{
  Type* type = (Type*)ref;
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      for (size_t i = 0; i < st->fields.size; ++i) {
        Add(add, st->fields.xs[i].type);
      }
      break;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        Add(add, ut->fields.xs[i].type);
      }
      break;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      Add(add, ft->arg);
      Add(add, ft->rtype);
      break;
    }

    case PROC_TYPE: {
      ProcType* pt = (ProcType*)type;
      Add(add, pt->type);
      break;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      Add(add, pt->arg);
      Add(add, pt->body);
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      Add(add, pat->poly);
      Add(add, pat->arg);
      Add(add, pat->result);
      break;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      Add(add, var->value);
      break;
    }

    case TYPE_TYPE: {
      TypeType* t = (TypeType*)type;
      Add(add, t->type);
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
//   The caller is responsible for calling FreeKind on the returned type when
//   it is no longer needed.
static Kind* TypeofKind(FbleArena* arena, Kind* kind)
{
  switch (kind->tag) {
    case BASIC_KIND: {
      BasicKind* basic = (BasicKind*)kind;
      BasicKind* typeof = FbleAlloc(arena, BasicKind);
      typeof->_base.tag = BASIC_KIND;
      typeof->_base.loc = kind->loc;
      typeof->_base.refcount = 1;
      typeof->level = basic->level + 1;
      return &typeof->_base;
    }

    case POLY_KIND: {
      PolyKind* poly = (PolyKind*)kind;
      PolyKind* typeof = FbleAlloc(arena, PolyKind);
      typeof->_base.tag = POLY_KIND;
      typeof->_base.loc = kind->loc;
      typeof->_base.refcount = 1;
      typeof->arg = CopyKind(arena, poly->arg);
      typeof->rkind = TypeofKind(arena, poly->rkind);
      return &typeof->_base;
    }
  }
  UNREACHABLE("Should never get here");
  return NULL;
}

// FbleGetKind -- see documentation in fble-type.h
Kind* FbleGetKind(FbleArena* arena, Type* type)
{
  switch (type->tag) {
    case STRUCT_TYPE:
    case UNION_TYPE:
    case FUNC_TYPE:
    case PROC_TYPE: {
      BasicKind* kind = FbleAlloc(arena, BasicKind);
      kind->_base.tag = BASIC_KIND;
      kind->_base.loc = type->loc;
      kind->_base.refcount = 1;
      kind->level = 1;
      return &kind->_base;
    }

    case POLY_TYPE: {
      PolyType* poly = (PolyType*)type;
      PolyKind* kind = FbleAlloc(arena, PolyKind);
      kind->_base.tag = POLY_KIND;
      kind->_base.loc = type->loc;
      kind->_base.refcount = 1;
      kind->arg = FbleGetKind(arena, poly->arg);
      kind->rkind = FbleGetKind(arena, poly->body);
      return &kind->_base;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PolyKind* kind = (PolyKind*)FbleGetKind(arena, pat->poly);
      assert(kind->_base.tag == POLY_KIND);

      Kind* rkind = CopyKind(arena, kind->rkind);
      FreeKind(arena, &kind->_base);
      return rkind;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      return CopyKind(arena, var->kind);
    }

    case TYPE_TYPE: {
      TypeType* type_type = (TypeType*)type;

      Kind* arg_kind = FbleGetKind(arena, type_type->type);
      Kind* kind = TypeofKind(arena, arg_kind);
      FreeKind(arena, arg_kind);
      return kind;
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// FbleGetKindLevel -- see documentation in fble-type.h
size_t FbleGetKindLevel(Kind* kind)
{
  switch (kind->tag) {
    case BASIC_KIND: {
      BasicKind* basic = (BasicKind*)kind;
      return basic->level;
    }

    case POLY_KIND: {
      PolyKind* poly = (PolyKind*)kind;
      return FbleGetKindLevel(poly->rkind);
    }
  }
  UNREACHABLE("Should never get here");
  return -1;
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
static bool HasParam(Type* type, Type* param, TypeList* visited)
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
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      for (size_t i = 0; i < st->fields.size; ++i) {
        if (HasParam(st->fields.xs[i].type, param, &nv)) {
          return true;
        }
      }
      return false;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        if (HasParam(ut->fields.xs[i].type, param, &nv)) {
          return true;
        }
      }
      return false;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      return HasParam(ft->arg, param, &nv)
          || HasParam(ft->rtype, param, &nv);
    }

    case PROC_TYPE: {
      ProcType* ut = (ProcType*)type;
      return HasParam(ut->type, param, &nv);
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      return pt->arg != param && HasParam(pt->body, param, &nv);
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      return HasParam(pat->arg, param, &nv)
          || HasParam(pat->poly, param, &nv);
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      return (type == param)
          || (var->value != NULL && HasParam(var->value, param, &nv));
    }

    case TYPE_TYPE: {
      TypeType* type_type = (TypeType*)type;
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
static Type* Subst(FbleTypeArena* arena, Type* type, Type* param, Type* arg, TypePairs* tps)
{
  if (!HasParam(type, param, NULL)) {
    return FbleTypeRetain(arena, type);
  }

  FbleArena* arena_ = FbleRefArenaArena(arena);

  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      StructType* sst = FbleAlloc(arena_, StructType);
      FbleRefInit(arena, &sst->_base.ref);
      sst->_base.tag = STRUCT_TYPE;
      sst->_base.loc = st->_base.loc;
      sst->_base.evaluating = false;

      FbleVectorInit(arena_, sst->fields);
      for (size_t i = 0; i < st->fields.size; ++i) {
        Field* field = FbleVectorExtend(arena_, sst->fields);
        field->name = st->fields.xs[i].name;
        field->type = Subst(arena, st->fields.xs[i].type, param, arg, tps);
        FbleRefAdd(arena, &sst->_base.ref, &field->type->ref);
        FbleTypeRelease(arena, field->type);
      }
      return &sst->_base;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      UnionType* sut = FbleAlloc(arena_, UnionType);
      FbleRefInit(arena, &sut->_base.ref);
      sut->_base.tag = UNION_TYPE;
      sut->_base.loc = ut->_base.loc;
      sut->_base.evaluating = false;
      FbleVectorInit(arena_, sut->fields);
      for (size_t i = 0; i < ut->fields.size; ++i) {
        Field* field = FbleVectorExtend(arena_, sut->fields);
        field->name = ut->fields.xs[i].name;
        field->type = Subst(arena, ut->fields.xs[i].type, param, arg, tps);
        FbleRefAdd(arena, &sut->_base.ref, &field->type->ref);
        FbleTypeRelease(arena, field->type);
      }
      return &sut->_base;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      FuncType* sft = FbleAlloc(arena_, FuncType);
      FbleRefInit(arena, &sft->_base.ref);
      sft->_base.tag = FUNC_TYPE;
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

    case PROC_TYPE: {
      ProcType* ut = (ProcType*)type;
      ProcType* sut = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &sut->_base.ref);
      sut->_base.tag = ut->_base.tag;
      sut->_base.loc = ut->_base.loc;
      sut->_base.evaluating = false;
      sut->type = Subst(arena, ut->type, param, arg, tps);
      FbleRefAdd(arena, &sut->_base.ref, &sut->type->ref);
      FbleTypeRelease(arena, sut->type);
      return &sut->_base;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      Type* body = Subst(arena, pt->body, param, arg, tps); 
      Type* spt = FbleNewPolyType(arena, pt->_base.loc, pt->arg, body);
      FbleTypeRelease(arena, body);
      return spt;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;

      Type* poly = Subst(arena, pat->poly, param, arg, tps);
      Type* sarg = Subst(arena, pat->arg, param, arg, tps);

      Type* spat = FbleNewPolyApplyType(arena, pat->_base.loc, poly, sarg);
      FbleTypeRelease(arena, poly);
      FbleTypeRelease(arena, sarg);
      return spat;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
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

      VarType* svar = FbleAlloc(arena_, VarType);
      FbleRefInit(arena, &svar->_base.ref);
      svar->_base.tag = VAR_TYPE;
      svar->_base.loc = type->loc;
      svar->_base.evaluating = false;
      svar->name = var->name;
      svar->kind = CopyKind(arena_, var->kind);
      svar->value = NULL;

      TypePairs ntp = {
        .a = var->value,
        .b = &svar->_base,
        .next = tps
      };

      Type* value = Subst(arena, var->value, param, arg, &ntp);
      svar->value = value;
      FbleRefAdd(arena, &svar->_base.ref, &svar->value->ref);
      FbleTypeRelease(arena, &svar->_base);
      return value;
    }

    case TYPE_TYPE: {
      TypeType* tt = (TypeType*)type;
      TypeType* stt = FbleAlloc(arena_, TypeType);
      FbleRefInit(arena, &stt->_base.ref);
      stt->_base.tag = TYPE_TYPE;
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
static void Eval(FbleTypeArena* arena, Type* type, PolyApplyList* applied)
{
  if (type == NULL || type->evaluating) {
    return;
  }

  type->evaluating = true;

  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      for (size_t i = 0; i < st->fields.size; ++i) {
        Eval(arena, st->fields.xs[i].type, applied);
      }
      break;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        Eval(arena, ut->fields.xs[i].type, applied);
      }
      break;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      Eval(arena, ft->arg, applied);
      Eval(arena, ft->rtype, applied);
      break;
    }

    case PROC_TYPE: {
      ProcType* ut = (ProcType*)type;
      Eval(arena, ut->type, applied);
      break;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      Eval(arena, pt->body, applied);
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;

      Eval(arena, pat->poly, applied);
      Eval(arena, pat->arg, applied);

      if (pat->result != NULL) {
        Eval(arena, pat->result, applied);
        break;
      }

      // Check whether we have already applied the arg to this poly.
      bool have_applied = false;
      for (PolyApplyList* pal = applied; pal != NULL; pal = pal->next) {
        if (FbleTypesEqual(pal->poly, pat->poly)) {
          if (FbleTypesEqual(pal->arg, pat->arg)) {
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

      PolyType* poly = (PolyType*)FbleNormalType(pat->poly);
      if (poly->_base.tag == POLY_TYPE) {
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
      break;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;

      if (var->value != NULL) {
        Eval(arena, var->value, applied);
      }
      break;
    }

    case TYPE_TYPE: {
      TypeType* tt = (TypeType*)type;
      Eval(arena, tt->type, applied);
      break;
    }
  }

  type->evaluating = false;
}

// FbleNormalType -- see documentation in fble-type.h
Type* FbleNormalType(Type* type)
{
  switch (type->tag) {
    case STRUCT_TYPE: return type;
    case UNION_TYPE: return type;
    case FUNC_TYPE: return type;
    case PROC_TYPE: return type;

    case POLY_TYPE: {
      // Normalize: (\x -> f x) to f
      // TODO: Does this cover all the cases? It seems like overly specific
      // pattern matching.
      PolyType* poly = (PolyType*)type;
      PolyApplyType* pat = (PolyApplyType*)FbleNormalType(poly->body);
      if (pat->_base.tag == POLY_APPLY_TYPE) {
        if (poly->arg == FbleNormalType(pat->arg)) {
          return FbleNormalType(pat->poly);
        }
      }
      return type;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      if (pat->result == NULL) {
        return type;
      }
      return FbleNormalType(pat->result);
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      if (var->value == NULL) {
        return type;
      }
      return FbleNormalType(var->value);
    }

    case TYPE_TYPE: return type;
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// TypesEqual --
//   Test whether the two given evaluated types are equal.
//
// Inputs:
//   a - the first type
//   b - the second type
//   eq - A set of pairs of types that should be assumed to be equal
//
// Results:
//   True if the first type equals the second type, false otherwise.
//
// Side effects:
//   None.
static bool TypesEqual(Type* a, Type* b, TypePairs* eq)
{
  a = FbleNormalType(a);
  b = FbleNormalType(b);
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

  if (a->tag != b->tag) {
    return false;
  }

  switch (a->tag) {
    case STRUCT_TYPE: {
      StructType* sta = (StructType*)a;
      StructType* stb = (StructType*)b;

      if (sta->fields.size != stb->fields.size) {
        return false;
      }

      for (size_t i = 0; i < sta->fields.size; ++i) {
        if (!FbleNamesEqual(&sta->fields.xs[i].name, &stb->fields.xs[i].name)) {
          return false;
        }

        if (!TypesEqual(sta->fields.xs[i].type, stb->fields.xs[i].type, &neq)) {
          return false;
        }
      }

      return true;
    }

    case UNION_TYPE: {
      UnionType* uta = (UnionType*)a;
      UnionType* utb = (UnionType*)b;
      if (uta->fields.size != utb->fields.size) {
        return false;
      }

      for (size_t i = 0; i < uta->fields.size; ++i) {
        if (!FbleNamesEqual(&uta->fields.xs[i].name, &utb->fields.xs[i].name)) {
          return false;
        }

        if (!TypesEqual(uta->fields.xs[i].type, utb->fields.xs[i].type, &neq)) {
          return false;
        }
      }

      return true;
    }

    case FUNC_TYPE: {
      FuncType* fta = (FuncType*)a;
      FuncType* ftb = (FuncType*)b;
      return TypesEqual(fta->arg, ftb->arg, &neq)
          && TypesEqual(fta->rtype, ftb->rtype, &neq);
    }

    case PROC_TYPE: {
      ProcType* uta = (ProcType*)a;
      ProcType* utb = (ProcType*)b;
      return TypesEqual(uta->type, utb->type, &neq);
    }

    case POLY_TYPE: {
      PolyType* pta = (PolyType*)a;
      PolyType* ptb = (PolyType*)b;

      // TODO: Verify the args have matching kinds.
  
      TypePairs pneq = {
        .a = pta->arg,
        .b = ptb->arg,
        .next = &neq
      };
      return TypesEqual(pta->body, ptb->body, &pneq);
    }

    case POLY_APPLY_TYPE: {
      UNREACHABLE("poly apply type is not Normal");
      return false;
    }

    case VAR_TYPE: {
      VarType* va = (VarType*)a;
      VarType* vb = (VarType*)b;

      assert(va->value == NULL && vb->value == NULL);
      assert(a != b);
      return false;
    }

    case TYPE_TYPE: {
      TypeType* tta = (TypeType*)a;
      TypeType* ttb = (TypeType*)b;
      return TypesEqual(tta->type, ttb->type, &neq);
    }
  }

  UNREACHABLE("Should never get here");
  return false;
}

// FbleKindsEqual -- see documentation in fble-type.h
bool FbleKindsEqual(Kind* a, Kind* b)
{
  if (a->tag != b->tag) {
    return false;
  }

  switch (a->tag) {
    case BASIC_KIND: {
      BasicKind* ba = (BasicKind*)a;
      BasicKind* bp = (BasicKind*)b;
      return ba->level == bp->level;
    }

    case POLY_KIND: {
      PolyKind* pa = (PolyKind*)a;
      PolyKind* pb = (PolyKind*)b;
      return FbleKindsEqual(pa->arg, pb->arg)
          && FbleKindsEqual(pa->rkind, pb->rkind);
    }
  }

  UNREACHABLE("Should never get here");
  return false;
}

// FblePrintType -- see documentation in fble-type.h
//
// Notes:
//   Human readable means we print var types using their name, without the
//   value associated with the variable. Because of this, we don't have to
//   worry about infinite recursion when trying to print a type: all recursion
//   must happen through a var type, and we don't ever go through a var type
//   when printing.
void FblePrintType(FbleArena* arena, Type* type)
{
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
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

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
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

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      fprintf(stderr, "(");
      FblePrintType(arena, ft->arg);
      fprintf(stderr, "){");
      FblePrintType(arena, ft->rtype);
      fprintf(stderr, ";}");
      break;
    }

    case PROC_TYPE: {
      ProcType* ut = (ProcType*)type;
      FblePrintType(arena, ut->type);
      fprintf(stderr, "!");
      break;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      fprintf(stderr, "<");
      Kind* kind = FbleGetKind(arena, pt->arg);
      FblePrintKind(kind);
      FreeKind(arena, kind);
      fprintf(stderr, " ");
      FblePrintType(arena, pt->arg);
      fprintf(stderr, "> { ");
      FblePrintType(arena, pt->body);
      fprintf(stderr, "; }");
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      FblePrintType(arena, pat->poly);
      fprintf(stderr, "<");
      FblePrintType(arena, pat->arg);
      fprintf(stderr, ">");
      break;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      FblePrintName(stderr, &var->name);
      break;
    }

    case TYPE_TYPE: {
      TypeType* tt = (TypeType*)type;
      fprintf(stderr, "@<");
      FblePrintType(arena, tt->type);
      fprintf(stderr, ">");
      break;
    }
  }
}

// FblePrintKind -- see documentation in fble-type.h
void FblePrintKind(Kind* kind)
{
  switch (kind->tag) {
    case BASIC_KIND: {
      BasicKind* basic = (BasicKind*)kind;
      if (basic->level == 1) {
        fprintf(stderr, "@");
      } else {
        // TODO: Will an end user ever see this?
        fprintf(stderr, "@%i", basic->level);
      }
      break;
    }

    case POLY_KIND: {
      const char* prefix = "<";
      while (kind->tag == POLY_KIND) {
        PolyKind* poly = (PolyKind*)kind;
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

// FbleNewPolyType -- see documentation in fble-type.h
Type* FbleNewPolyType(FbleTypeArena* arena, FbleLoc loc, Type* arg, Type* body)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  if (body->tag == TYPE_TYPE) {
    // \arg -> typeof(body) = typeof(\arg -> body)
    TypeType* ttbody = (TypeType*)body;
    TypeType* tt = FbleAlloc(arena_, TypeType);
    FbleRefInit(arena, &tt->_base.ref);
    tt->_base.tag = TYPE_TYPE;
    tt->_base.loc = loc;
    tt->_base.evaluating = false;
    tt->type = FbleNewPolyType(arena, loc, arg, ttbody->type);
    FbleRefAdd(arena, &tt->_base.ref, &tt->type->ref);
    FbleTypeRelease(arena, tt->type);
    return &tt->_base;
  }

  PolyType* pt = FbleAlloc(arena_, PolyType);
  pt->_base.tag = POLY_TYPE;
  pt->_base.loc = loc;
  pt->_base.evaluating = false;
  FbleRefInit(arena, &pt->_base.ref);
  pt->arg = arg;
  FbleRefAdd(arena, &pt->_base.ref, &pt->arg->ref);
  pt->body = body;
  FbleRefAdd(arena, &pt->_base.ref, &pt->body->ref);

  assert(pt->body->tag != TYPE_TYPE);
  return &pt->_base;
}

// FbleNewPolyApplyType -- see documentation in fble-type.h
Type* FbleNewPolyApplyType(FbleTypeArena* arena, FbleLoc loc, Type* poly, Type* arg)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  if (poly->tag == TYPE_TYPE) {
    // typeof(poly)<arg> == typeof(poly<arg>)
    TypeType* ttpoly = (TypeType*)poly;
    TypeType* tt = FbleAlloc(arena_, TypeType);
    FbleRefInit(arena, &tt->_base.ref);
    tt->_base.tag = TYPE_TYPE;
    tt->_base.loc = loc;
    tt->_base.evaluating = false;
    tt->type = FbleNewPolyApplyType(arena, loc, ttpoly->type, arg);
    FbleRefAdd(arena, &tt->_base.ref, &tt->type->ref);
    FbleTypeRelease(arena, tt->type);
    return &tt->_base;
  }

  PolyApplyType* pat = FbleAlloc(arena_, PolyApplyType);
  FbleRefInit(arena, &pat->_base.ref);
  pat->_base.tag = POLY_APPLY_TYPE;
  pat->_base.loc = loc;
  pat->_base.evaluating = false;
  pat->poly = poly;
  FbleRefAdd(arena, &pat->_base.ref, &pat->poly->ref);
  pat->arg = arg;
  FbleRefAdd(arena, &pat->_base.ref, &pat->arg->ref);
  pat->result = NULL;

  assert(pat->poly->tag != TYPE_TYPE);
  return &pat->_base;
}

// FbleTypesEqual -- see documentation in fble-types.h
bool FbleTypesEqual(Type* a, Type* b)
{
  return TypesEqual(a, b, NULL);
}

// FbleEvalType -- see documentation in fble-types.h
void FbleEvalType(FbleTypeArena* arena, Type* type)
{
  Eval(arena, type, NULL);
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
