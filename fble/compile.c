// compile.c --
//   This file implements the fble compilation routines.

#include <stdlib.h>   // for NULL
#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

typedef FbleRefArena TypeArena;

// KindTag --
//   A tag used to distinguish between the two kinds of kinds.
typedef enum {
  BASIC_KIND,
  POLY_KIND
} KindTag;

// Kind --
//   A tagged union of kind types. All kinds have the same initial
//   layout as Kind. The tag can be used to determine what kind of
//   kind this is to get access to additional fields of the kind
//   by first casting to that specific type of kind.
typedef struct {
  KindTag tag;
  FbleLoc loc;
  int refcount;
} Kind;

// KindV --
//   A vector of Kind.
typedef struct {
  size_t size;
  Kind** xs;
} KindV;

// BasicKind --
//   BASIC_KIND
typedef struct {
  Kind _base;
} BasicKind;

// PolyKind --
//   POLY_KIND (args :: [Kind]) (return :: Kind)
typedef struct {
  Kind _base;
  KindV args;
  Kind* rkind;
} PolyKind;

// TypeTag --
//   A tag used to dinstinguish among different kinds of compiled types.
typedef enum {
  STRUCT_TYPE,
  UNION_TYPE,
  FUNC_TYPE,
  PROC_TYPE,
  INPUT_TYPE,
  OUTPUT_TYPE,
  VAR_TYPE,
  ABSTRACT_TYPE,
  POLY_TYPE,
  POLY_APPLY_TYPE,
  REF_TYPE,
} TypeTag;

// Type --
//   A tagged union of type types. All types have the same initial
//   layout as Type. The tag can be used to determine what kind of
//   type this is to get access to additional fields of the type
//   by first casting to that specific type of type.
typedef struct Type {
  FbleRef ref;
  TypeTag tag;
  FbleLoc loc;
} Type;

// TypeV --
//   A vector of Type.
typedef struct {
  size_t size;
  Type** xs;
} TypeV;

// Field --
//   A pair of (Type, Name) used to describe type and function arguments.
typedef struct {
  Type* type;
  FbleName name;
} Field;

// FieldV --
//   A vector of Field.
typedef struct {
  size_t size;
  Field* xs;
} FieldV;

// StructType -- STRUCT_TYPE
typedef struct {
  Type _base;
  FieldV type_fields;
  FieldV fields;
} StructType;

// UnionType -- UNION_TYPE
typedef struct {
  Type _base;
  FieldV fields;
} UnionType;

// FuncType -- FUNC_TYPE
typedef struct {
  Type _base;
  TypeV args;
  struct Type* rtype;
} FuncType;

// ProcType -- PROC_TYPE
typedef struct {
  Type _base;
  Type* rtype;
} ProcType;

// InputType -- INPUT_TYPE
typedef struct {
  Type _base;
  Type* rtype;
} InputType;

// OutputType -- OUTPUT_TYPE
typedef struct {
  Type _base;
  Type* rtype;
} OutputType;

// VarType -- VAR_TYPE
typedef struct {
  Type _base;
  FbleName var;
  Type* value;
} VarType;

// AbstractType -- ABSTRACT_TYPE
typedef struct {
  Type _base;
  Kind* kind;
} AbstractType;

// PolyType -- POLY_TYPE
typedef struct {
  Type _base;
  TypeV args;
  Type* body;
} PolyType;

// PolyApplyType -- POLY_APPLY_TYPE
//   The 'result' field is the result of evaluating the poly apply type, or
//   NULL if it has not yet been evaluated.
typedef struct {
  Type _base;
  Type* poly;
  TypeV args;
  Type* result;
} PolyApplyType;

// RefType --
//   REF_TYPE
//
// A implementation-specific type introduced to support recursive types. A
// ref type is simply a reference to another type.
typedef struct RefType {
  Type _base;
  Kind* kind;
  Type* value;
} RefType;

// RefTypeV - a vector of ref types.
typedef struct {
  size_t size;
  RefType** xs;
} RefTypeV;

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
  TypeV args;
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

// Vars --
//   Scope of variables visible during type checking.
typedef struct Vars {
  FbleName name;
  Type* type;
  struct Vars* next;
} Vars;

static Kind* CopyKind(FbleArena* arena, Kind* kind);
static void FreeKind(FbleArena* arena, Kind* kind);

static Type* TypeRetain(TypeArena* arena, Type* type);
static void TypeRelease(TypeArena* arena, Type* type);

static void TypeFree(TypeArena* arena, FbleRef* ref);
static void Add(FbleRefCallback* add, Type* type);
static void TypeAdded(FbleRefCallback* add, FbleRef* ref);

static Kind* GetKind(FbleArena* arena, Type* type);
static bool HasParams(Type* type, TypeV params, TypeList* visited);
static Type* Subst(TypeArena* arena, Type* src, TypeV params, TypeV args, TypePairs* tps);
static void Eval(TypeArena* arena, Type* type, TypeList* evaled, PolyApplyList* applied);
static Type* Normal(Type* type);
static bool TypesEqual(Type* a, Type* b, TypePairs* eq);
static bool KindsEqual(Kind* a, Kind* b);
static void PrintType(FbleArena* arena, Type* type);
static void PrintKind(Kind* type);

static void FreeInstr(FbleArena* arena, FbleInstr* instr);

static Type* Compile(TypeArena* arena, Vars* vars, Vars* type_vars, FbleExpr* expr, FbleInstrV* instrs);
static Type* CompileType(TypeArena* arena, Vars* vars, Vars* type_vars, FbleType* type);
static Kind* CompileKind(FbleArena* arena, FbleKind* kind);

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

// FreeKind --
//   Frees a (refcount) copy of a compiled kind.
//
// Inputs:
//   arena - for deallocations.
//   kind - the kind to free. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Decrements the refcount for the kind and frees it if there are no
//   more references to it.
static void FreeKind(FbleArena* arena, Kind* kind)
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
          for (size_t i = 0; i < poly->args.size; ++i) {
            FreeKind(arena, poly->args.xs[i]);
          }
          FbleFree(arena, poly->args.xs);
          FreeKind(arena, poly->rkind);
          FbleFree(arena, poly);
          break;
        }
      }
    }
  }
}

// TypeRetain --
//   Takes a strong reference to a compiled type.
//
// Inputs:
//   arena - the arena the type was allocated with.
//   type - the type to take the reference for.
//
// Results:
//   The type with incremented strong reference count.
//
// Side effects:
//   The returned type must be freed using TypeRelease when no longer in use.
static Type* TypeRetain(TypeArena* arena, Type* type)
{
  if (type != NULL) {
    FbleRefRetain(arena, &type->ref);
  }
  return type;
}

// TypeRelease --
//   Drop a strong reference to a compiled type.
//
// Inputs:
//   arena - for deallocations.
//   type - the type to drop the refcount for. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Decrements the strong refcount for the type and frees it if there are no
//   more references to it.
static void TypeRelease(TypeArena* arena, Type* type)
{
  if (type != NULL) {
    FbleRefRelease(arena, &type->ref);
  }
}

// TypeFree --
//   The free function for types. See documentation in fble-ref.h
static void TypeFree(TypeArena* arena, FbleRef* ref)
{
  Type* type = (Type*)ref;
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      FbleFree(arena_, st->type_fields.xs);
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

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      FbleFree(arena_, ft->args.xs);
      FbleFree(arena_, ft);
      break;
    }

    case PROC_TYPE:
    case INPUT_TYPE:
    case OUTPUT_TYPE:
    case VAR_TYPE: {
      FbleFree(arena_, type);
      break;
    }

    case ABSTRACT_TYPE: {
      AbstractType* abstract = (AbstractType*)type;
      FreeKind(arena_, abstract->kind);
      FbleFree(arena_, type);
      break;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      FbleFree(arena_, pt->args.xs);
      FbleFree(arena_, pt);
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      FbleFree(arena_, pat->args.xs);
      FbleFree(arena_, pat);
      break;
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;
      FreeKind(arena_, ref->kind);
      FbleFree(arena_, ref);
      break;
    }
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
//   The added function for types. See documentation in fble-ref.h
static void TypeAdded(FbleRefCallback* add, FbleRef* ref)
{
  Type* type = (Type*)ref;
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      for (size_t i = 0; i < st->type_fields.size; ++i) {
        Add(add, st->type_fields.xs[i].type);
      }
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
      for (size_t i = 0; i < ft->args.size; ++i) {
        Add(add, ft->args.xs[i]);
      }
      Add(add, ft->rtype);
      break;
    }

    case PROC_TYPE: {
      ProcType* pt = (ProcType*)type;
      Add(add, pt->rtype);
      break;
    }

    case INPUT_TYPE: {
      InputType* t = (InputType*)type;
      Add(add, t->rtype);
      break;
    }

    case OUTPUT_TYPE: {
      OutputType* t = (OutputType*)type;
      Add(add, t->rtype);
      break;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      Add(add, var->value);
      break;
    }

    case ABSTRACT_TYPE: {
      break;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      for (size_t i = 0; i < pt->args.size; ++i) {
        Add(add, pt->args.xs[i]);
      }
      Add(add, pt->body);
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      Add(add, pat->poly);
      for (size_t i = 0; i < pat->args.size; ++i) {
        Add(add, pat->args.xs[i]);
      }
      Add(add, pat->result);
      break;
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;
      Add(add, ref->value);
      break;
    }
  }
}

// GetKind --
//   Get the kind of the given type.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the type to get the kind of.
//
// Results:
//   The kind of the type.
//
// Side effects:
//   The caller is responsible for calling FreeKind on the returned type when
//   it is no longer needed.
static Kind* GetKind(FbleArena* arena, Type* type)
{
  switch (type->tag) {
    case STRUCT_TYPE:
    case UNION_TYPE:
    case FUNC_TYPE:
    case PROC_TYPE: 
    case INPUT_TYPE:
    case OUTPUT_TYPE: {
      BasicKind* kind = FbleAlloc(arena, BasicKind);
      kind->_base.tag = BASIC_KIND;
      kind->_base.loc = type->loc;
      kind->_base.refcount = 1;
      return &kind->_base;
    }

    case VAR_TYPE: {
      VarType* vt = (VarType*)type;
      return GetKind(arena, vt->value);
    }

    case ABSTRACT_TYPE: {
      AbstractType* abstract = (AbstractType*)type;
      return CopyKind(arena, abstract->kind);
    }

    case POLY_TYPE: {
      PolyType* poly = (PolyType*)type;
      PolyKind* kind = FbleAlloc(arena, PolyKind);
      kind->_base.tag = POLY_KIND;
      kind->_base.loc = type->loc;
      kind->_base.refcount = 1;
      FbleVectorInit(arena, kind->args);

      for (size_t i = 0; i < poly->args.size; ++i) {
        FbleVectorAppend(arena, kind->args, GetKind(arena, poly->args.xs[i]));
      }
      kind->rkind = GetKind(arena, poly->body);
      return &kind->_base;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PolyKind* kind = (PolyKind*)GetKind(arena, pat->poly);
      assert(kind->_base.tag == POLY_KIND);
      assert(kind->args.size >= pat->args.size);

      if (kind->args.size == pat->args.size) {
        Kind* rkind = CopyKind(arena, kind->rkind);
        FreeKind(arena, &kind->_base);
        return rkind;
      }

      PolyKind* pkind = FbleAlloc(arena, PolyKind);
      pkind->_base.tag = POLY_KIND;
      pkind->_base.loc = type->loc;
      pkind->_base.refcount = 1;
      FbleVectorInit(arena, pkind->args);
      for (size_t i = pat->args.size; i < kind->args.size; ++i) {
        FbleVectorAppend(arena, pkind->args, CopyKind(arena, kind->args.xs[i]));
      }
      pkind->rkind = CopyKind(arena, kind->rkind);
      FreeKind(arena, &kind->_base);
      return &pkind->_base;
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;
      return CopyKind(arena, ref->kind);
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// HasParams --
//   Check whether a type has any of the given params as free type variables.
//
// Inputs:
//   type - the type to check.
//   params - the abstract types to check for.
//   visited - a list of types already visited to end the recursion.
//
// Results:
//   True if any of the params occur in type, false otherwise.
//
// Side effects:
//   None.
static bool HasParams(Type* type, TypeV params, TypeList* visited)
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
      for (size_t i = 0; i < st->type_fields.size; ++i) {
        if (HasParams(st->type_fields.xs[i].type, params, &nv)) {
          return true;
        }
      }
      for (size_t i = 0; i < st->fields.size; ++i) {
        if (HasParams(st->fields.xs[i].type, params, &nv)) {
          return true;
        }
      }
      return false;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        if (HasParams(ut->fields.xs[i].type, params, &nv)) {
          return true;
        }
      }
      return false;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      for (size_t i = 0; i < ft->args.size; ++i) {
        if (HasParams(ft->args.xs[i], params, &nv)) {
          return true;
        }
      }
      return HasParams(ft->rtype, params, &nv);
    }

    case PROC_TYPE: {
      ProcType* pt = (ProcType*)type;
      return HasParams(pt->rtype, params, &nv);
    }

    case INPUT_TYPE: {
      InputType* t = (InputType*)type;
      return HasParams(t->rtype, params, &nv);
    }

    case OUTPUT_TYPE: {
      OutputType* t = (OutputType*)type;
      return HasParams(t->rtype, params, &nv);
    }

    case VAR_TYPE: {
      VarType* vt = (VarType*)type;
      return HasParams(vt->value, params, &nv);
    }

    case ABSTRACT_TYPE: {
      for (size_t i = 0; i < params.size; ++i) {
        if (type == params.xs[i]) {
          return true;
        }
      }
      return false;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;

      if (pt->args.xs == params.xs) {
        return false;
      }
      return HasParams(pt->body, params, &nv);
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      for (size_t i = 0; i < pat->args.size; ++i) {
        if (HasParams(pat->args.xs[i], params, &nv)) {
          return true;
        }
      }
      return HasParams(pat->poly, params, &nv);
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;

      if (ref->value == NULL) {
        return false;
      }
      return HasParams(ref->value, params, &nv);
    }
  }

  UNREACHABLE("Should never get here");
  return false;
}

// Subst --
//   Substitute the given arguments in place of the given parameters in the
//   given type.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the type to substitute into.
//   params - the abstract types to substitute out.
//   args - the concrete types to substitute in.
//   tps - a map of already substituted types, to avoid infinite recursion.
//
// Results:
//   A type with all occurrences of params replaced with the corresponding
//   args types.
//
// Side effects:
//   The caller is responsible for calling TypeRelease on the returned
//   type when it is no longer needed.
static Type* Subst(TypeArena* arena, Type* type, TypeV params, TypeV args, TypePairs* tps)
{
  if (!HasParams(type, params, NULL)) {
    return TypeRetain(arena, type);
  }

  FbleArena* arena_ = FbleRefArenaArena(arena);

  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      StructType* sst = FbleAlloc(arena_, StructType);
      FbleRefInit(arena, &sst->_base.ref);
      sst->_base.tag = STRUCT_TYPE;
      sst->_base.loc = st->_base.loc;

      FbleVectorInit(arena_, sst->type_fields);
      for (size_t i = 0; i < st->type_fields.size; ++i) {
        Field* field = FbleVectorExtend(arena_, sst->type_fields);
        field->name = st->type_fields.xs[i].name;
        field->type = Subst(arena, st->type_fields.xs[i].type, params, args, tps);
        FbleRefAdd(arena, &sst->_base.ref, &field->type->ref);
        TypeRelease(arena, field->type);
      }

      FbleVectorInit(arena_, sst->fields);
      for (size_t i = 0; i < st->fields.size; ++i) {
        Field* field = FbleVectorExtend(arena_, sst->fields);
        field->name = st->fields.xs[i].name;
        field->type = Subst(arena, st->fields.xs[i].type, params, args, tps);
        FbleRefAdd(arena, &sst->_base.ref, &field->type->ref);
        TypeRelease(arena, field->type);
      }
      return &sst->_base;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      UnionType* sut = FbleAlloc(arena_, UnionType);
      FbleRefInit(arena, &sut->_base.ref);
      sut->_base.tag = UNION_TYPE;
      sut->_base.loc = ut->_base.loc;
      FbleVectorInit(arena_, sut->fields);
      for (size_t i = 0; i < ut->fields.size; ++i) {
        Field* field = FbleVectorExtend(arena_, sut->fields);
        field->name = ut->fields.xs[i].name;
        field->type = Subst(arena, ut->fields.xs[i].type, params, args, tps);
        FbleRefAdd(arena, &sut->_base.ref, &field->type->ref);
        TypeRelease(arena, field->type);
      }
      return &sut->_base;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      FuncType* sft = FbleAlloc(arena_, FuncType);
      FbleRefInit(arena, &sft->_base.ref);
      sft->_base.tag = FUNC_TYPE;
      sft->_base.loc = ft->_base.loc;
      FbleVectorInit(arena_, sft->args);
      for (size_t i = 0; i < ft->args.size; ++i) {
        Type* arg_type = Subst(arena, ft->args.xs[i], params, args, tps);
        FbleVectorAppend(arena_, sft->args, arg_type);
        FbleRefAdd(arena, &sft->_base.ref, &arg_type->ref);
        TypeRelease(arena, arg_type);
      }
      sft->rtype = Subst(arena, ft->rtype, params, args, tps);
      FbleRefAdd(arena, &sft->_base.ref, &sft->rtype->ref);
      TypeRelease(arena, sft->rtype);
      return &sft->_base;
    }

    case PROC_TYPE: {
      ProcType* pt = (ProcType*)type;
      ProcType* spt = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &spt->_base.ref);
      spt->_base.tag = PROC_TYPE;
      spt->_base.loc = pt->_base.loc;
      spt->rtype = Subst(arena, pt->rtype, params, args, tps);
      FbleRefAdd(arena, &spt->_base.ref, &spt->rtype->ref);
      TypeRelease(arena, spt->rtype);
      return &spt->_base;
    }

    case INPUT_TYPE: {
      InputType* pt = (InputType*)type;
      InputType* spt = FbleAlloc(arena_, InputType);
      FbleRefInit(arena, &spt->_base.ref);
      spt->_base.tag = INPUT_TYPE;
      spt->_base.loc = pt->_base.loc;
      spt->rtype = Subst(arena, pt->rtype, params, args, tps);
      FbleRefAdd(arena, &spt->_base.ref, &spt->rtype->ref);
      TypeRelease(arena, spt->rtype);
      return &spt->_base;
    }

    case OUTPUT_TYPE: {
      OutputType* pt = (OutputType*)type;
      OutputType* spt = FbleAlloc(arena_, OutputType);
      FbleRefInit(arena, &spt->_base.ref);
      spt->_base.tag = OUTPUT_TYPE;
      spt->_base.loc = pt->_base.loc;
      spt->rtype = Subst(arena, pt->rtype, params, args, tps);
      FbleRefAdd(arena, &spt->_base.ref, &spt->rtype->ref);
      TypeRelease(arena, spt->rtype);
      return &spt->_base;
    }

    case VAR_TYPE: {
      VarType* vt = (VarType*)type;
      VarType* svt = FbleAlloc(arena_, VarType);
      FbleRefInit(arena, &svt->_base.ref);
      svt->_base.tag = VAR_TYPE;
      svt->_base.loc = vt->_base.loc;
      svt->var = vt->var;
      svt->value = Subst(arena, vt->value, params, args, tps);
      FbleRefAdd(arena, &svt->_base.ref, &svt->value->ref);
      TypeRelease(arena, svt->value);
      return &svt->_base;
    }

    case ABSTRACT_TYPE: {
      for (size_t i = 0; i < params.size; ++i) {
        if (type == params.xs[i]) {
          return TypeRetain(arena, args.xs[i]);
        }
      }
      assert(false && "HasParams result was wrong.");
      return TypeRetain(arena, type);
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      PolyType* spt = FbleAlloc(arena_, PolyType);
      spt->_base.tag = POLY_TYPE;
      spt->_base.loc = pt->_base.loc;
      FbleRefInit(arena, &spt->_base.ref);
      FbleVectorInit(arena_, spt->args);
      for (size_t i = 0; i < pt->args.size; ++i) {
        FbleVectorAppend(arena_, spt->args, pt->args.xs[i]);
        FbleRefAdd(arena, &spt->_base.ref, &pt->args.xs[i]->ref);
      }
      spt->body = Subst(arena, pt->body, params, args, tps);
      FbleRefAdd(arena, &spt->_base.ref, &spt->body->ref);
      TypeRelease(arena, spt->body);
      return &spt->_base;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PolyApplyType* spat = FbleAlloc(arena_, PolyApplyType);
      FbleRefInit(arena, &spat->_base.ref);
      spat->_base.tag = POLY_APPLY_TYPE;
      spat->_base.loc = pat->_base.loc;
      spat->poly = Subst(arena, pat->poly, params, args, tps);
      FbleRefAdd(arena, &spat->_base.ref, &spat->poly->ref);
      TypeRelease(arena, spat->poly);
      spat->result = NULL;
      FbleVectorInit(arena_, spat->args);
      for (size_t i = 0; i < pat->args.size; ++i) {
        Type* arg = Subst(arena, pat->args.xs[i], params, args, tps);
        FbleVectorAppend(arena_, spat->args, arg);
        FbleRefAdd(arena, &spat->_base.ref, &arg->ref);
        TypeRelease(arena, arg);
      }
      return &spat->_base;
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;

      // Treat null refs as abstract types.
      if (ref->value == NULL) {
        return TypeRetain(arena, type);
      }

      // Check to see if we've already done substitution on the value pointed
      // to by this ref.
      for (TypePairs* tp = tps; tp != NULL; tp = tp->next) {
        if (tp->a == ref->value) {
          return TypeRetain(arena, tp->b);
        }
      }

      RefType* sref = FbleAlloc(arena_, RefType);
      FbleRefInit(arena, &sref->_base.ref);
      sref->_base.tag = REF_TYPE;
      sref->_base.loc = type->loc;
      sref->kind = CopyKind(arena_, ref->kind);
      sref->value = NULL;

      TypePairs ntp = {
        .a = ref->value,
        .b = &sref->_base,
        .next = tps
      };

      sref->value = Subst(arena, ref->value, params, args, &ntp);
      FbleRefAdd(arena, &sref->_base.ref, &sref->value->ref);
      return sref->value;
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// Eval --
//   Evaluate the given type in place. After evaluation there are no more
//   unevaluated poly apply types that can be applied.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the type to evaluate. May be NULL.
//   evaled - list of types to assume have been evaluated already, to avoid
//            infinite recursion.
//   applied - cache of poly applications already evaluated, to avoid infinite
//             recursion.
//
// Results:
//   None.
//
// Side effects:
//   The type is evaluated in place.
static void Eval(TypeArena* arena, Type* type, TypeList* evaled, PolyApplyList* applied)
{
  if (type == NULL) {
    return;
  }

  for (TypeList* e = evaled; e != NULL; e = e->next) {
    if (type == e->type) {
      return;
    }
  }

  TypeList nevaled = {
    .type = type,
    .next = evaled
  };

  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      for (size_t i = 0; i < st->type_fields.size; ++i) {
        Eval(arena, st->type_fields.xs[i].type, &nevaled, applied);
      }
      for (size_t i = 0; i < st->fields.size; ++i) {
        Eval(arena, st->fields.xs[i].type, &nevaled, applied);
      }
      return;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        Eval(arena, ut->fields.xs[i].type, &nevaled, applied);
      }
      return;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      for (size_t i = 0; i < ft->args.size; ++i) {
        Eval(arena, ft->args.xs[i], &nevaled, applied);
      }
      Eval(arena, ft->rtype, &nevaled, applied);
      return;
    }

    case PROC_TYPE: {
      ProcType* pt = (ProcType*)type;
      Eval(arena, pt->rtype, &nevaled, applied);
      return;
    }

    case INPUT_TYPE: {
      InputType* pt = (InputType*)type;
      Eval(arena, pt->rtype, &nevaled, applied);
      return;
    }

    case OUTPUT_TYPE: {
      OutputType* pt = (OutputType*)type;
      Eval(arena, pt->rtype, &nevaled, applied);
      return;
    }

    case VAR_TYPE: {
      VarType* vt = (VarType*)type;
      Eval(arena, vt->value, &nevaled, applied);
      return;
    }

    case ABSTRACT_TYPE: {
      return;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      Eval(arena, pt->body, &nevaled, applied);
      return;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      if (pat->result != NULL) {
        // We have already evaluated this poly apply.
        return;
      }

      Eval(arena, pat->poly, &nevaled, applied);
      for (size_t i = 0; i < pat->args.size; ++i) {
        Eval(arena, pat->args.xs[i], &nevaled, applied);
      }

      // Check whether we have already applied these args to this poly.
      for (PolyApplyList* pal = applied; pal != NULL; pal = pal->next) {
        if (pal->args.size == pat->args.size && TypesEqual(pal->poly, pat->poly, NULL)) {
          bool allmatch = true;
          for (size_t i = 0; i < pat->args.size; ++i) {
            allmatch = allmatch && TypesEqual(pal->args.xs[i], pat->args.xs[i], NULL);
          }

          if (allmatch) {
            pat->result = pal->result;
            assert(&pat->_base != pat->result);
            FbleRefAdd(arena, &pat->_base.ref, &pat->result->ref);
            return;
          }
        }
      }

      PolyType* poly = (PolyType*)Normal(pat->poly);
      if (poly->_base.tag == POLY_TYPE) {
        pat->result = Subst(arena, poly->body, poly->args, pat->args, NULL);
        assert(&pat->_base != pat->result);
        FbleRefAdd(arena, &pat->_base.ref, &pat->result->ref);
        TypeRelease(arena, pat->result);

        PolyApplyList napplied = {
          .poly = &poly->_base,
          .args = pat->args,
          .result = pat->result,
          .next = applied
        };

        Eval(arena, pat->result, &nevaled, &napplied);
      }
      return;
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;

      if (ref->value != NULL) {
        Eval(arena, ref->value, &nevaled, applied);
      }
      return;
    }
  }

  UNREACHABLE("Should never get here");
}

// Normal --
//   Reduce an evaluated type to normal form. Normal form types are struct,
//   union, and func types, but not var types, for example.
//
// Inputs:
//   type - the type to reduce.
//
// Results:
//   The type reduced to normal form.
//
// Side effects:
//   The result is only valid for as long as the input type is retained. It is
//   the callers responsibility to take a references to the return typed if
//   they want it to live longer than the given input type.
static Type* Normal(Type* type)
{
  switch (type->tag) {
    case STRUCT_TYPE: return type;
    case UNION_TYPE: return type;
    case FUNC_TYPE: return type;
    case PROC_TYPE: return type;
    case INPUT_TYPE: return type;
    case OUTPUT_TYPE: return type;

    case VAR_TYPE: {
      VarType* var_type = (VarType*)type;
      return Normal(var_type->value);
    }

    case ABSTRACT_TYPE: return type;
    case POLY_TYPE: return type;

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      if (pat->result == NULL) {
        return type;
      }
      return Normal(pat->result);
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;
      if (ref->value == NULL) {
        return type;
      }
      return Normal(ref->value);
    }
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
  a = Normal(a);
  b = Normal(b);
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
      if (sta->type_fields.size != stb->type_fields.size) {
        return false;
      }

      for (size_t i = 0; i < sta->type_fields.size; ++i) {
        if (!FbleNamesEqual(sta->type_fields.xs[i].name.name, stb->type_fields.xs[i].name.name)) {
          return false;
        }

        if (!TypesEqual(sta->type_fields.xs[i].type, stb->type_fields.xs[i].type, &neq)) {
          return false;
        }
      }

      if (sta->fields.size != stb->fields.size) {
        return false;
      }

      for (size_t i = 0; i < sta->fields.size; ++i) {
        if (!FbleNamesEqual(sta->fields.xs[i].name.name, stb->fields.xs[i].name.name)) {
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
        if (!FbleNamesEqual(uta->fields.xs[i].name.name, utb->fields.xs[i].name.name)) {
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
      if (fta->args.size != ftb->args.size) {
        return false;
      }

      for (size_t i = 0; i < fta->args.size; ++i) {
        if (!TypesEqual(fta->args.xs[i], ftb->args.xs[i], &neq)) {
          return false;
        }
      }

      return TypesEqual(fta->rtype, ftb->rtype, &neq);
    }

    case PROC_TYPE: {
      ProcType* pta = (ProcType*)a;
      ProcType* ptb = (ProcType*)b;
      return TypesEqual(pta->rtype, ptb->rtype, &neq);
    }

    case INPUT_TYPE: {
      InputType* pta = (InputType*)a;
      InputType* ptb = (InputType*)b;
      return TypesEqual(pta->rtype, ptb->rtype, &neq);
    }

    case OUTPUT_TYPE: {
      OutputType* pta = (OutputType*)a;
      OutputType* ptb = (OutputType*)b;
      return TypesEqual(pta->rtype, ptb->rtype, &neq);
    }

    case VAR_TYPE: {
      UNREACHABLE("var type is not Normal");
      return false;
    }

    case ABSTRACT_TYPE: {
      assert(a != b);
      return false;
    }

    case POLY_TYPE: {
      PolyType* pta = (PolyType*)a;
      PolyType* ptb = (PolyType*)b;
      
      if (pta->args.size != ptb->args.size) {
        return false;
      }

      // TODO: Verify all the args have matching kinds.
   
      TypePairs nneq[pta->args.size]; 
      TypePairs* pneq = &neq;
      for (size_t i = 0; i < pta->args.size; ++i) {
        nneq[i].a = pta->args.xs[i];
        nneq[i].b = ptb->args.xs[i];
        nneq[i].next = pneq;
        pneq = nneq + i;
      }
      
      return TypesEqual(pta->body, ptb->body, pneq);
    }

    case POLY_APPLY_TYPE: {
      UNREACHABLE("poly apply type is not Normal");
      return false;
    }

    case REF_TYPE: {
      RefType* ra = (RefType*)a;
      RefType* rb = (RefType*)b;

      assert(ra->value == NULL && rb->value == NULL);
      assert(a != b);
      return false;
    }
  }

  UNREACHABLE("Should never get here");
  return false;
}

// KindsEqual --
//   Test whether the two given compiled kinds are equal.
//
// Inputs:
//   a - the first kind
//   b - the second kind
//
// Results:
//   True if the first kind equals the second kind, false otherwise.
//
// Side effects:
//   None.
static bool KindsEqual(Kind* a, Kind* b)
{
  if (a->tag != b->tag) {
    return false;
  }

  switch (a->tag) {
    case BASIC_KIND: {
      assert(b->tag == BASIC_KIND);
      return true;
    }

    case POLY_KIND: {
      PolyKind* pa = (PolyKind*)a;
      PolyKind* pb = (PolyKind*)b;

      if (pa->args.size != pb->args.size) {
        return false;
      }

      for (size_t i = 0; i < pa->args.size; ++i) {
        if (!KindsEqual(pa->args.xs[i], pb->args.xs[i])) {
          return false;
        }
      }

      // TODO: Is "(a, b) -> c" equal to "a -> b -> c"?
      return KindsEqual(pa->rkind, pb->rkind);
    }
  }

  UNREACHABLE("Should never get here");
  return false;
}

// PrintType --
//   Print the given compiled type in human readable form to stderr.
//
// Inputs:
//   arena - arena to use for internal allocations.
//   type - the type to print.
//
// Result:
//   None.
//
// Side effect:
//   Prints the given type in human readable form to stderr.
static void PrintType(FbleArena* arena, Type* type)
{
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      fprintf(stderr, "*(");
      const char* comma = "";
      for (size_t i = 0; i < st->type_fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        Kind* kind = GetKind(arena, st->type_fields.xs[i].type);
        PrintKind(kind);
        FreeKind(arena, kind);
        fprintf(stderr, " %s@ = ", st->type_fields.xs[i].name.name);
        PrintType(arena, st->type_fields.xs[i].type);
        comma = ", ";
      }

      for (size_t i = 0; i < st->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(arena, st->fields.xs[i].type);
        fprintf(stderr, " %s", st->fields.xs[i].name.name);
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
        PrintType(arena, ut->fields.xs[i].type);
        fprintf(stderr, " %s", ut->fields.xs[i].name.name);
        comma = ", ";
      }
      fprintf(stderr, ")");
      break;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      fprintf(stderr, "\\(");
      const char* comma = "";
      for (size_t i = 0; i < ft->args.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(arena, ft->args.xs[i]);
        comma = ", ";
      }
      fprintf(stderr, "; ");
      PrintType(arena, ft->rtype);
      fprintf(stderr, ")");
      break;
    }

    case PROC_TYPE: {
      ProcType* pt = (ProcType*)type;
      PrintType(arena, pt->rtype);
      fprintf(stderr, "!");
      break;
    }

    case INPUT_TYPE: {
      InputType* pt = (InputType*)type;
      PrintType(arena, pt->rtype);
      fprintf(stderr, "-");
      break;
    }

    case OUTPUT_TYPE: {
      OutputType* pt = (OutputType*)type;
      PrintType(arena, pt->rtype);
      fprintf(stderr, "+");
      break;
    }

    case VAR_TYPE: {
      VarType* var_type = (VarType*)type;
      fprintf(stderr, "%s@", var_type->var.name);
      break;
    }

    case ABSTRACT_TYPE: {
      fprintf(stderr, "??%p", (void*)type);
      break;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      fprintf(stderr, "\\<");
      const char* comma = "";
      for (size_t i = 0; i < pt->args.size; ++i) {
        fprintf(stderr, "%s@ ??%p@", comma, (void*)pt->args.xs[i]);
        comma = ", ";
      }
      fprintf(stderr, " { ");
      PrintType(arena, pt->body);
      fprintf(stderr, "; }");
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PrintType(arena, pat->poly);
      fprintf(stderr, "<");
      const char* comma = "";
      for (size_t i = 0; i < pat->args.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(arena, pat->args.xs[i]);
        comma = ", ";
      }
      fprintf(stderr, ">");
      break;
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)ref;
      if (ref->value == NULL) {
        fprintf(stderr, "??%p", (void*)type);
      } else {
        PrintType(arena, ref->value);
      }
      break;
    }
  }
}

// PrintKind --
//   Print the given compiled kind in human readable form to stderr.
//
// Inputs:
//   kind - the kind to print.
//
// Result:
//   None.
//
// Side effect:
//   Prints the given kind in human readable form to stderr.
static void PrintKind(Kind* kind)
{
  switch (kind->tag) {
    case BASIC_KIND: {
      fprintf(stderr, "@");
      break;
    }

    case POLY_KIND: {
      PolyKind* poly = (PolyKind*)kind;
      fprintf(stderr, "\\<");
      const char* comma = "";
      for (size_t i = 0; i < poly->args.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintKind(poly->args.xs[i]);
        comma = ", ";
      }
      fprintf(stderr, "; ");
      PrintKind(poly->rkind);
      fprintf(stderr, ">");
      break;
    }
  }
}

// FreeInstr --
//   Free the given instruction.
//
// Inputs:
//   arena - the arena used to allocation the instructions.
//   instr - the instruction to free. May be NULL.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the given instruction.
static void FreeInstr(FbleArena* arena, FbleInstr* instr)
{
  if (instr == NULL) {
    return;
  }

  switch (instr->tag) {
    case FBLE_STRUCT_VALUE_INSTR:
    case FBLE_UNION_VALUE_INSTR:
    case FBLE_STRUCT_ACCESS_INSTR:
    case FBLE_UNION_ACCESS_INSTR:
    case FBLE_DESCOPE_INSTR:
    case FBLE_FUNC_APPLY_INSTR:
    case FBLE_VAR_INSTR:
    case FBLE_EVAL_INSTR:
    case FBLE_GET_INSTR:
    case FBLE_PUT_INSTR:
    case FBLE_PROC_INSTR:
    case FBLE_JOIN_INSTR:
    case FBLE_LET_PREP_INSTR:
    case FBLE_LET_DEF_INSTR:
    case FBLE_NAMESPACE_INSTR:
    case FBLE_IPOP_INSTR: {
      FbleFree(arena, instr);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
      FbleFreeInstrBlock(arena, func_value_instr->body);
      FbleFree(arena, func_value_instr);
      return;
    }

    case FBLE_LINK_INSTR: {
      FbleLinkInstr* proc_link_instr = (FbleLinkInstr*)instr;
      FbleFreeInstrBlock(arena, proc_link_instr->body);
      FbleFree(arena, proc_link_instr);
      return;
    }

    case FBLE_EXEC_INSTR: {
      FbleExecInstr* exec_instr = (FbleExecInstr*)instr;
      FbleFreeInstrBlock(arena, exec_instr->body);
      FbleFree(arena, exec_instr);
      return;
    }

    case FBLE_COND_INSTR: {
      FbleCondInstr* cond_instr = (FbleCondInstr*)instr;
      for (size_t i = 0; i < cond_instr->choices.size; ++i) {
        FbleFreeInstrBlock(arena, cond_instr->choices.xs[i]);
      }
      FbleFree(arena, cond_instr->choices.xs);
      FbleFree(arena, instr);
      return;
    }
  }

  UNREACHABLE("invalid instruction");
}

// Compile --
//   Type check and compile the given expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the list of variables in scope.
//   type_vars - the value of type variables in scope.
//   expr - the expression to compile.
//   instrs - vector of instructions to append new instructions to.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Appends instructions to 'instrs' for executing the given expression.
//   There is no gaurentee about what instructions have been appended to
//   'instrs' if the expression fails to compile.
//   Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using
//   TypeRelease when it is no longer needed.
static Type* Compile(TypeArena* arena, Vars* vars, Vars* type_vars, FbleExpr* expr, FbleInstrV* instrs)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (expr->tag) {
    case FBLE_STRUCT_VALUE_EXPR: {
      FbleStructValueExpr* struct_value_expr = (FbleStructValueExpr*)expr;
      Type* type = CompileType(arena, vars, type_vars, struct_value_expr->type);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      StructType* struct_type = (StructType*)Normal(type);
      if (struct_type->_base.tag != STRUCT_TYPE) {
        FbleReportError("expected a struct type, but found ", &struct_value_expr->type->loc);
        PrintType(arena_, type);
        fprintf(stderr, "\n");
        TypeRelease(arena, type);
        return NULL;
      }

      if (struct_type->fields.size != struct_value_expr->args.size) {
        // TODO: Where should the error message go?
        FbleReportError("expected %i args, but %i were provided\n",
            &expr->loc, struct_type->fields.size, struct_value_expr->args.size);
        TypeRelease(arena, type);
        return NULL;
      }

      bool error = false;
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        Field* field = struct_type->fields.xs + i;

        Type* arg_type = Compile(arena, vars, type_vars, struct_value_expr->args.xs[i], instrs);
        Eval(arena, arg_type, NULL, NULL);
        error = error || (arg_type == NULL);

        if (arg_type != NULL && !TypesEqual(field->type, arg_type, NULL)) {
          FbleReportError("expected type ", &struct_value_expr->args.xs[i]->loc);
          PrintType(arena_, field->type);
          fprintf(stderr, ", but found ");
          PrintType(arena_, arg_type);
          fprintf(stderr, "\n");
          error = true;
        }
        TypeRelease(arena, arg_type);
      }

      if (error) {
        TypeRelease(arena, type);
        return NULL;
      }

      FbleStructValueInstr* struct_instr = FbleAlloc(arena_, FbleStructValueInstr);
      struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      struct_instr->argc = struct_type->fields.size;
      FbleVectorAppend(arena_, *instrs, &struct_instr->_base);
      return type;
    }

    case FBLE_ANON_STRUCT_VALUE_EXPR: {
      FbleAnonStructValueExpr* struct_expr = (FbleAnonStructValueExpr*)expr;
      StructType* struct_type = FbleAlloc(arena_, StructType);
      FbleRefInit(arena, &struct_type->_base.ref);
      struct_type->_base.loc = expr->loc;
      struct_type->_base.tag = STRUCT_TYPE;
      FbleVectorInit(arena_, struct_type->type_fields);
      FbleVectorInit(arena_, struct_type->fields);

      bool error = false;
      for (size_t i = 0; i < struct_expr->type_args.size; ++i) {
        FbleField* field = struct_expr->type_args.xs + i;
        Type* type = CompileType(arena, vars, type_vars, field->type);
        Eval(arena, type, NULL, NULL);
        error = error || (type == NULL);
        if (type != NULL) {
          Field* cfield = FbleVectorExtend(arena_, struct_type->type_fields);
          cfield->name = field->name;
          cfield->type = type;
          FbleRefAdd(arena, &struct_type->_base.ref, &cfield->type->ref);
          TypeRelease(arena, type);
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, struct_expr->type_args.xs[j].name.name)) {
            error = true;
            FbleReportError("duplicate type field name '%s'\n", &field->name.loc, struct_expr->type_args.xs[j].name.name);
          }
        }
      }

      for (size_t i = 0; i < struct_expr->args.size; ++i) {
        FbleChoice* choice = struct_expr->args.xs + i;
        Type* arg_type = Compile(arena, vars, type_vars, choice->expr, instrs);
        Eval(arena, arg_type, NULL, NULL);
        error = error || (arg_type == NULL);

        if (arg_type != NULL) {
          Field* cfield = FbleVectorExtend(arena_, struct_type->fields);
          cfield->name = choice->name;
          cfield->type = arg_type;
          FbleRefAdd(arena, &struct_type->_base.ref, &cfield->type->ref);
          TypeRelease(arena, arg_type);
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(choice->name.name, struct_expr->args.xs[j].name.name)) {
            error = true;
            FbleReportError("duplicate field name '%s'\n", &choice->name.loc, struct_expr->args.xs[j].name.name);
          }
        }
      }

      if (error) {
        TypeRelease(arena, &struct_type->_base);
        return NULL;
      }

      FbleStructValueInstr* struct_instr = FbleAlloc(arena_, FbleStructValueInstr);
      struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      struct_instr->argc = struct_expr->args.size;
      FbleVectorAppend(arena_, *instrs, &struct_instr->_base);
      return &struct_type->_base;
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      Type* type = CompileType(arena, vars, type_vars, union_value_expr->type);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      UnionType* union_type = (UnionType*)Normal(type);
      if (union_type->_base.tag != UNION_TYPE) {
        FbleReportError("expected a union type, but found ", &union_value_expr->type->loc);
        PrintType(arena_, type);
        fprintf(stderr, "\n");
        TypeRelease(arena, type);
        return NULL;
      }

      Type* field_type = NULL;
      size_t tag = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        Field* field = union_type->fields.xs + i;
        if (FbleNamesEqual(field->name.name, union_value_expr->field.name)) {
          tag = i;
          field_type = field->type;
          break;
        }
      }

      if (field_type == NULL) {
        FbleReportError("'%s' is not a field of type ", &union_value_expr->field.loc, union_value_expr->field.name);
        PrintType(arena_, type);
        fprintf(stderr, "\n");
        TypeRelease(arena, type);
        return NULL;
      }

      Type* arg_type = Compile(arena, vars, type_vars, union_value_expr->arg, instrs);
      Eval(arena, arg_type, NULL, NULL);
      if (arg_type == NULL) {
        TypeRelease(arena, type);
        return NULL;
      }

      if (!TypesEqual(field_type, arg_type, NULL)) {
        FbleReportError("expected type ", &union_value_expr->arg->loc);
        PrintType(arena_, field_type);
        fprintf(stderr, ", but found type ");
        PrintType(arena_, arg_type);
        fprintf(stderr, "\n");
        TypeRelease(arena, type);
        TypeRelease(arena, arg_type);
        return NULL;
      }
      TypeRelease(arena, arg_type);

      FbleUnionValueInstr* union_instr = FbleAlloc(arena_, FbleUnionValueInstr);
      union_instr->_base.tag = FBLE_UNION_VALUE_INSTR;
      union_instr->tag = tag;
      FbleVectorAppend(arena_, *instrs, &union_instr->_base);
      return type;
    }

    case FBLE_ACCESS_EXPR: {
      FbleAccessExpr* access_expr = (FbleAccessExpr*)expr;

      Type* type = Compile(arena, vars, type_vars, access_expr->object, instrs);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      FbleAccessInstr* access = FbleAlloc(arena_, FbleAccessInstr);
      access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      access->loc = access_expr->field.loc;
      FbleVectorAppend(arena_, *instrs, &access->_base);

      Type* normal = Normal(type);
      FieldV* fields = NULL;
      if (normal->tag == STRUCT_TYPE) {
        access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
        fields = &((StructType*)normal)->fields;
      } else if (normal->tag == UNION_TYPE) {
        access->_base.tag = FBLE_UNION_ACCESS_INSTR;
        fields = &((UnionType*)normal)->fields;
      } else {
        FbleReportError("expected value of type struct or union, but found value of type ", &access_expr->object->loc);
        PrintType(arena_, type);
        fprintf(stderr, "\n");

        TypeRelease(arena, type);
        return NULL;
      }

      for (size_t i = 0; i < fields->size; ++i) {
        if (FbleNamesEqual(access_expr->field.name, fields->xs[i].name.name)) {
          access->tag = i;
          Type* rtype = TypeRetain(arena, fields->xs[i].type);
          TypeRelease(arena, type);
          return rtype;
        }
      }

      FbleReportError("%s is not a field of type ", &access_expr->field.loc, access_expr->field.name);
      PrintType(arena_, type);
      fprintf(stderr, "\n");
      TypeRelease(arena, type);
      return NULL;
    }

    case FBLE_COND_EXPR: {
      FbleCondExpr* cond_expr = (FbleCondExpr*)expr;

      Type* type = Compile(arena, vars, type_vars, cond_expr->condition, instrs);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      UnionType* union_type = (UnionType*)Normal(type);
      if (union_type->_base.tag != UNION_TYPE) {
        FbleReportError("expected value of union type, but found value of type ", &cond_expr->condition->loc);
        PrintType(arena_, type);
        fprintf(stderr, "\n");
        TypeRelease(arena, type);
        return NULL;
      }

      if (union_type->fields.size != cond_expr->choices.size) {
        FbleReportError("expected %d arguments, but %d were provided.\n", &cond_expr->_base.loc, union_type->fields.size, cond_expr->choices.size);
        TypeRelease(arena, type);
        return NULL;
      }

      FbleCondInstr* cond_instr = FbleAlloc(arena_, FbleCondInstr);
      cond_instr->_base.tag = FBLE_COND_INSTR;
      FbleVectorAppend(arena_, *instrs, &cond_instr->_base);
      FbleVectorInit(arena_, cond_instr->choices);

      Type* return_type = NULL;
      for (size_t i = 0; i < cond_expr->choices.size; ++i) {
        if (!FbleNamesEqual(cond_expr->choices.xs[i].name.name, union_type->fields.xs[i].name.name)) {
          FbleReportError("expected tag '%s', but found '%s'.\n",
              &cond_expr->choices.xs[i].name.loc,
              union_type->fields.xs[i].name.name,
              cond_expr->choices.xs[i].name.name);
          TypeRelease(arena, return_type);
          TypeRelease(arena, type);
          return NULL;
        }

        FbleInstrBlock* choice = FbleAlloc(arena_, FbleInstrBlock);
        choice->refcount = 1;
        FbleVectorInit(arena_, choice->instrs);
        FbleVectorAppend(arena_, cond_instr->choices, choice);

        Type* arg_type = Compile(arena, vars, type_vars, cond_expr->choices.xs[i].expr, &choice->instrs);

        {
          FbleIPopInstr* ipop = FbleAlloc(arena_, FbleIPopInstr);
          ipop->_base.tag = FBLE_IPOP_INSTR;
          FbleVectorAppend(arena_, choice->instrs, &ipop->_base);
        }

        Eval(arena, arg_type, NULL, NULL);
        if (arg_type == NULL) {
          TypeRelease(arena, return_type);
          TypeRelease(arena, type);
          return NULL;
        }

        if (return_type == NULL) {
          return_type = arg_type;
        } else {
          if (!TypesEqual(return_type, arg_type, NULL)) {
            FbleReportError("expected type ", &cond_expr->choices.xs[i].expr->loc);
            PrintType(arena_, return_type);
            fprintf(stderr, ", but found ");
            PrintType(arena_, arg_type);
            fprintf(stderr, "\n");

            TypeRelease(arena, type);
            TypeRelease(arena, return_type);
            TypeRelease(arena, arg_type);
            return NULL;
          }
          TypeRelease(arena, arg_type);
        }
      }
      TypeRelease(arena, type);
      return return_type;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      FuncType* type = FbleAlloc(arena_, FuncType);
      FbleRefInit(arena, &type->_base.ref);
      type->_base.tag = FUNC_TYPE;
      type->_base.loc = expr->loc;
      FbleVectorInit(arena_, type->args);
      type->rtype = NULL;
      
      Vars nvds[func_value_expr->args.size];
      Vars* nvars = vars;
      bool error = false;
      for (size_t i = 0; i < func_value_expr->args.size; ++i) {
        Type* arg_type = CompileType(arena, vars, type_vars, func_value_expr->args.xs[i].type);
        if (arg_type != NULL) {
          FbleVectorAppend(arena_, type->args, arg_type);
          FbleRefAdd(arena, &type->_base.ref, &arg_type->ref);
          TypeRelease(arena, arg_type);
        }
        error = error || (arg_type == NULL);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(func_value_expr->args.xs[i].name.name, func_value_expr->args.xs[j].name.name)) {
            error = true;
            FbleReportError("duplicate arg name '%s'\n", &func_value_expr->args.xs[i].name.loc, func_value_expr->args.xs[i].name.name);
            break;
          }
        }

        nvds[i].name = func_value_expr->args.xs[i].name;
        nvds[i].type = arg_type;
        nvds[i].next = nvars;
        nvars = nvds + i;
      }

      FbleFuncValueInstr* instr = FbleAlloc(arena_, FbleFuncValueInstr);
      instr->_base.tag = FBLE_FUNC_VALUE_INSTR;
      FbleVectorAppend(arena_, *instrs, &instr->_base);

      instr->contextc = 0;
      for (Vars* v = vars; v != NULL; v = v->next) {
        instr->contextc++;
      }

      instr->argc = func_value_expr->args.size;

      instr->body = FbleAlloc(arena_, FbleInstrBlock);
      instr->body->refcount = 1;
      FbleVectorInit(arena_, instr->body->instrs);

      if (!error) {
        type->rtype = Compile(arena, nvars, type_vars, func_value_expr->body, &instr->body->instrs);
        if (type->rtype != NULL) {
          FbleRefAdd(arena, &type->_base.ref, &type->rtype->ref);
          TypeRelease(arena, type->rtype);
        }
        error = error || (type->rtype == NULL);

        if (!error) {
          FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
          descope->_base.tag = FBLE_DESCOPE_INSTR;
          descope->count = instr->contextc + instr->argc;
          FbleVectorAppend(arena_, instr->body->instrs, &descope->_base);

          FbleIPopInstr* ipop = FbleAlloc(arena_, FbleIPopInstr);
          ipop->_base.tag = FBLE_IPOP_INSTR;
          FbleVectorAppend(arena_, instr->body->instrs, &ipop->_base);
        }
      }

      if (error) {
        TypeRelease(arena, &type->_base);
        return NULL;
      }
      return &type->_base;
    }

    case FBLE_APPLY_EXPR: {
      FbleApplyExpr* apply_expr = (FbleApplyExpr*)expr;

      // Compile the function value.
      Type* type = Compile(arena, vars, type_vars, apply_expr->func, instrs);
      Eval(arena, type, NULL, NULL);
      bool error = (type == NULL);

      // Compile the arguments.
      Type* arg_types[apply_expr->args.size];
      for (size_t i = 0; i < apply_expr->args.size; ++i) {
        arg_types[i] = Compile(arena, vars, type_vars, apply_expr->args.xs[i], instrs);
        Eval(arena, arg_types[i], NULL, NULL);
        error = error || (arg_types[i] == NULL);
      }

      // Check the argument types match what is expected.
      if (type != NULL) {
        Type* normal = Normal(type);
        switch (normal->tag) {
          case FUNC_TYPE: {
            // This is normal function application.
            FuncType* func_type = (FuncType*)normal;
            if (func_type->args.size != apply_expr->args.size) {
              FbleReportError("expected %i args, but %i provided\n",
                  &expr->loc, func_type->args.size, apply_expr->args.size);
              error = true;
            }

            for (size_t i = 0; i < func_type->args.size && i < apply_expr->args.size; ++i) {
              if (arg_types[i] != NULL && !TypesEqual(func_type->args.xs[i], arg_types[i], NULL)) {
                FbleReportError("expected type ", &apply_expr->args.xs[i]->loc);
                PrintType(arena_, func_type->args.xs[i]);
                fprintf(stderr, ", but found ");
                PrintType(arena_, arg_types[i]);
                fprintf(stderr, "\n");
                error = true;
              }
            }

            for (size_t i = 0; i < apply_expr->args.size; ++i) {
              TypeRelease(arena, arg_types[i]);
            }

            if (error) {
              TypeRelease(arena, type);
              return NULL;
            }

            FbleFuncApplyInstr* apply_instr = FbleAlloc(arena_, FbleFuncApplyInstr);
            apply_instr->_base.tag = FBLE_FUNC_APPLY_INSTR;
            apply_instr->argc = func_type->args.size;

            FbleVectorAppend(arena_, *instrs, &apply_instr->_base);
            Type* rtype = TypeRetain(arena, func_type->rtype);
            TypeRelease(arena, type);
            return rtype;
          }

          case INPUT_TYPE: {
            // This is a GET process.
            InputType* input_type = (InputType*)normal;
            if (apply_expr->args.size != 0) {
              FbleReportError("expected 0 args to get process, but %i provided\n",
                  &expr->loc, apply_expr->args.size);
              error = true;
            }

            for (size_t i = 0; i < apply_expr->args.size; ++i) {
              TypeRelease(arena, arg_types[i]);
            }

            if (error) {
              TypeRelease(arena, type);
              return NULL;
            }

            FbleGetInstr* get_instr = FbleAlloc(arena_, FbleGetInstr);
            get_instr->_base.tag = FBLE_GET_INSTR;
            FbleVectorAppend(arena_, *instrs, &get_instr->_base);

            ProcType* proc_type = FbleAlloc(arena_, ProcType);
            FbleRefInit(arena, &proc_type->_base.ref);
            proc_type->_base.tag = PROC_TYPE;
            proc_type->_base.loc = expr->loc;
            proc_type->rtype = input_type->rtype;
            FbleRefAdd(arena, &proc_type->_base.ref, &proc_type->rtype->ref);
            TypeRelease(arena, type);
            return &proc_type->_base;
          }

          case OUTPUT_TYPE: {
            // This is a PUT process.
            OutputType* output_type = (OutputType*)normal;
            if (apply_expr->args.size == 1) {
              if (arg_types[0] != NULL && !TypesEqual(output_type->rtype, arg_types[0], NULL)) {
                FbleReportError("expected type ", &apply_expr->args.xs[0]->loc);
                PrintType(arena_, output_type->rtype);
                fprintf(stderr, ", but found ");
                PrintType(arena_, arg_types[0]);
                fprintf(stderr, "\n");
                error = true;
              }
            } else {
              FbleReportError("expected 1 args to put process, but %i provided\n",
                  &expr->loc, apply_expr->args.size);
              error = true;
            }

            for (size_t i = 0; i < apply_expr->args.size; ++i) {
              TypeRelease(arena, arg_types[i]);
            }

            if (error) {
              TypeRelease(arena, type);
              return NULL;
            }

            FblePutInstr* put_instr = FbleAlloc(arena_, FblePutInstr);
            put_instr->_base.tag = FBLE_PUT_INSTR;
            FbleVectorAppend(arena_, *instrs, &put_instr->_base);

            ProcType* proc_type = FbleAlloc(arena_, ProcType);
            FbleRefInit(arena, &proc_type->_base.ref);
            proc_type->_base.tag = PROC_TYPE;
            proc_type->_base.loc = expr->loc;
            proc_type->rtype = output_type->rtype;
            FbleRefAdd(arena, &proc_type->_base.ref, &proc_type->rtype->ref);
            TypeRelease(arena, type);
            return &proc_type->_base;
          }

          default: {
            FbleReportError("cannot perform application on an object of type ", &expr->loc);
            PrintType(arena_, type);
            fprintf(stderr, "\n");
            break;
          }
        }
      }

      TypeRelease(arena, type);
      for (size_t i = 0; i < apply_expr->args.size; ++i) {
        TypeRelease(arena, arg_types[i]);
      }
      return NULL;
    }

    case FBLE_EVAL_EXPR: {
      FbleEvalExpr* eval_expr = (FbleEvalExpr*)expr;

      Type* type = Compile(arena, vars, type_vars, eval_expr->expr, instrs);
      if (type == NULL) {
        return NULL;
      }

      ProcType* proc_type = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &proc_type->_base.ref);
      proc_type->_base.tag = PROC_TYPE;
      proc_type->_base.loc = expr->loc;
      proc_type->rtype = type;
      FbleRefAdd(arena, &proc_type->_base.ref, &proc_type->rtype->ref);
      TypeRelease(arena, proc_type->rtype);

      FbleEvalInstr* eval_instr = FbleAlloc(arena_, FbleEvalInstr);
      eval_instr->_base.tag = FBLE_EVAL_INSTR;
      FbleVectorAppend(arena_, *instrs, &eval_instr->_base);
      return &proc_type->_base;
    }

    case FBLE_LINK_EXPR: {
      FbleLinkExpr* link_expr = (FbleLinkExpr*)expr;
      Type* port_type = CompileType(arena, vars, type_vars, link_expr->type);
      if (port_type == NULL) {
        return NULL;
      }

      InputType* get_type = FbleAlloc(arena_, InputType);
      FbleRefInit(arena, &get_type->_base.ref);
      get_type->_base.tag = INPUT_TYPE;
      get_type->_base.loc = port_type->loc;
      get_type->rtype = port_type;
      FbleRefAdd(arena, &get_type->_base.ref, &get_type->rtype->ref);
      Vars get_var = {
        .name = link_expr->get,
        .type = &get_type->_base,
        .next = vars
      };

      OutputType* put_type = FbleAlloc(arena_, OutputType);
      FbleRefInit(arena, &put_type->_base.ref);
      put_type->_base.tag = OUTPUT_TYPE;
      put_type->_base.loc = port_type->loc;
      put_type->rtype = port_type;
      FbleRefAdd(arena, &put_type->_base.ref, &put_type->rtype->ref);
      Vars put_var = {
        .name = link_expr->put,
        .type = &put_type->_base,
        .next = &get_var
      };

      FbleLinkInstr* instr = FbleAlloc(arena_, FbleLinkInstr);
      instr->_base.tag = FBLE_LINK_INSTR;
      FbleVectorAppend(arena_, *instrs, &instr->_base);

      instr->contextc = 0;
      for (Vars* v = vars; v != NULL; v = v->next) {
        instr->contextc++;
      }

      instr->body = FbleAlloc(arena_, FbleInstrBlock);
      instr->body->refcount = 1;
      FbleVectorInit(arena_, instr->body->instrs);

      Type* type = Compile(arena, &put_var, type_vars, link_expr->body, &instr->body->instrs);
      Eval(arena, type, NULL, NULL);

      FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
      descope->_base.tag = FBLE_DESCOPE_INSTR;
      descope->count = 2 + instr->contextc;
      FbleVectorAppend(arena_, instr->body->instrs, &descope->_base);

      FbleProcInstr* proc = FbleAlloc(arena_, FbleProcInstr);
      proc->_base.tag = FBLE_PROC_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &proc->_base);

      FbleIPopInstr* ipop = FbleAlloc(arena_, FbleIPopInstr);
      ipop->_base.tag = FBLE_IPOP_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &ipop->_base);

      TypeRelease(arena, port_type);
      TypeRelease(arena, &get_type->_base);
      TypeRelease(arena, &put_type->_base);

      if (type == NULL) {
        TypeRelease(arena, type);
        return NULL;
      }

      if (Normal(type)->tag != PROC_TYPE) {
        FbleReportError("expected a value of type proc, but found ", &type->loc);
        PrintType(arena_, type);
        fprintf(stderr, "\n");
        TypeRelease(arena, type);
        return NULL;
      }
      return type;
    }

    case FBLE_EXEC_EXPR: {
      FbleExecExpr* exec_expr = (FbleExecExpr*)expr;
      bool error = false;

      // Evaluate the types of the bindings and set up the new vars.
      Vars nvd[exec_expr->bindings.size];
      Vars* nvars = vars;
      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        nvd[i].name = exec_expr->bindings.xs[i].name;
        nvd[i].type = CompileType(arena, vars, type_vars, exec_expr->bindings.xs[i].type);
        Eval(arena, nvd[i].type, NULL, NULL);
        error = error || (nvd[i].type == NULL);
        nvd[i].next = nvars;
        nvars = nvd + i;
      }

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        Type* type = Compile(arena, vars, type_vars, exec_expr->bindings.xs[i].expr, instrs);
        Eval(arena, type, NULL, NULL);
        error = error || (type == NULL);

        if (type != NULL) {
          ProcType* proc_type = (ProcType*)Normal(type);
          if (proc_type->_base.tag == PROC_TYPE) {
            if (nvd[i].type != NULL && !TypesEqual(nvd[i].type, proc_type->rtype, NULL)) {
              error = true;
              FbleReportError("expected type ", &exec_expr->bindings.xs[i].expr->loc);
              PrintType(arena_, nvd[i].type);
              fprintf(stderr, "!, but found ");
              PrintType(arena_, type);
              fprintf(stderr, "\n");
            }
          } else {
            error = true;
            FbleReportError("expected process, but found expression of type",
                &exec_expr->bindings.xs[i].expr->loc);
            PrintType(arena_, type);
            fprintf(stderr, "\n");
          }
          TypeRelease(arena, type);
        }
      }

      FbleExecInstr* exec_instr = FbleAlloc(arena_, FbleExecInstr);
      exec_instr->_base.tag = FBLE_EXEC_INSTR;
      exec_instr->argc = exec_expr->bindings.size;
      exec_instr->contextc = 0;
      for (Vars* v = vars; v != NULL; v = v->next) {
        exec_instr->contextc++;
      }
      FbleVectorAppend(arena_, *instrs, &exec_instr->_base);

      exec_instr->body = FbleAlloc(arena_, FbleInstrBlock);
      exec_instr->body->refcount = 1;
      FbleVectorInit(arena_, exec_instr->body->instrs);

      {
        FbleJoinInstr* join = FbleAlloc(arena_, FbleJoinInstr);
        join->_base.tag = FBLE_JOIN_INSTR;
        FbleVectorAppend(arena_, exec_instr->body->instrs, &join->_base);
      }

      Type* rtype = NULL;
      if (!error) {
        rtype = Compile(arena, nvars, type_vars, exec_expr->body, &exec_instr->body->instrs);
        Eval(arena, rtype, NULL, NULL);
        error = (rtype == NULL);
      }

      if (rtype != NULL && Normal(rtype)->tag != PROC_TYPE) {
        error = true;
        FbleReportError("expected a value of type proc, but found ", &exec_expr->body->loc);
        PrintType(arena_, rtype);
        fprintf(stderr, "\n");
      }

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        TypeRelease(arena, nvd[i].type);
      }

      if (error) {
        TypeRelease(arena, rtype);
        return NULL;
      }

      FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
      descope->_base.tag = FBLE_DESCOPE_INSTR;
      descope->count = exec_instr->contextc + exec_instr->argc;
      FbleVectorAppend(arena_, exec_instr->body->instrs, &descope->_base);

      FbleProcInstr* proc = FbleAlloc(arena_, FbleProcInstr);
      proc->_base.tag = FBLE_PROC_INSTR;
      FbleVectorAppend(arena_, exec_instr->body->instrs, &proc->_base);

      FbleIPopInstr* ipop = FbleAlloc(arena_, FbleIPopInstr);
      ipop->_base.tag = FBLE_IPOP_INSTR;
      FbleVectorAppend(arena_, exec_instr->body->instrs, &ipop->_base);

      return rtype;
    }

    case FBLE_VAR_EXPR: {
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;

      int position = 0;
      while (vars != NULL && !FbleNamesEqual(var_expr->var.name, vars->name.name)) {
        vars = vars->next;
        position++;
      }

      if (vars == NULL) {
        FbleReportError("variable '%s' not defined\n", &var_expr->var.loc, var_expr->var.name);
        return NULL;
      }

      FbleVarInstr* instr = FbleAlloc(arena_, FbleVarInstr);
      instr->_base.tag = FBLE_VAR_INSTR;
      instr->position = position;
      FbleVectorAppend(arena_, *instrs, &instr->_base);
      return TypeRetain(arena, vars->type);
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;
      bool error = false;

      // Evaluate the types of the bindings and set up the new vars.
      Vars nvd[let_expr->bindings.size];
      Vars* nvars = vars;
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        nvd[i].name = let_expr->bindings.xs[i].name;
        nvd[i].type = CompileType(arena, vars, type_vars, let_expr->bindings.xs[i].type);
        Eval(arena, nvd[i].type, NULL, NULL);
        error = error || (nvd[i].type == NULL);
        nvd[i].next = nvars;
        nvars = nvd + i;
      }

      FbleLetPrepInstr* let_prep_instr = FbleAlloc(arena_, FbleLetPrepInstr);
      let_prep_instr->_base.tag = FBLE_LET_PREP_INSTR;
      let_prep_instr->count = let_expr->bindings.size;
      FbleVectorAppend(arena_, *instrs, &let_prep_instr->_base);

      // Compile the values of the variables.
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        Type* type = NULL;
        if (!error) {
          type = Compile(arena, nvars, type_vars, let_expr->bindings.xs[i].expr, instrs);
          Eval(arena, type, NULL, NULL);
        }
        error = error || (type == NULL);

        if (!error && !TypesEqual(nvd[i].type, type, NULL)) {
          error = true;
          FbleReportError("expected type ", &let_expr->bindings.xs[i].expr->loc);
          PrintType(arena_, nvd[i].type);
          fprintf(stderr, ", but found ");
          PrintType(arena_, type);
          fprintf(stderr, "\n");
        }
        TypeRelease(arena, type);
      }

      FbleLetDefInstr* let_def_instr = FbleAlloc(arena_, FbleLetDefInstr);
      let_def_instr->_base.tag = FBLE_LET_DEF_INSTR;
      let_def_instr->count = let_expr->bindings.size;
      FbleVectorAppend(arena_, *instrs, &let_def_instr->_base);

      Type* rtype = NULL;
      if (!error) {
        rtype = Compile(arena, nvars, type_vars, let_expr->body, instrs);
        error = (rtype == NULL);
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        TypeRelease(arena, nvd[i].type);
      }

      if (error) {
        TypeRelease(arena, rtype);
        return NULL;
      }

      FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
      descope->_base.tag = FBLE_DESCOPE_INSTR;
      descope->count = let_expr->bindings.size;
      FbleVectorAppend(arena_, *instrs, &descope->_base);
      return rtype;
    }

    case FBLE_TYPE_LET_EXPR: {
      FbleTypeLetExpr* let = (FbleTypeLetExpr*)expr;

      Vars ntvd[let->bindings.size];
      Vars* ntvs = type_vars;

      // Set up reference types for all the bindings, to support recursive
      // types.
      for (size_t i = 0; i < let->bindings.size; ++i) {
        RefType* ref = FbleAlloc(arena_, RefType);
        FbleRefInit(arena, &ref->_base.ref);
        ref->_base.tag = REF_TYPE;
        ref->_base.loc = let->bindings.xs[i].name.loc;
        ref->kind = CompileKind(arena_, let->bindings.xs[i].kind);
        ref->value = NULL;

        ntvd[i].name = let->bindings.xs[i].name;
        ntvd[i].type = &ref->_base;
        ntvd[i].next = ntvs;
        ntvs = ntvd + i;
      }

      // Evaluate the type bindings.
      Type* types[let->bindings.size];
      bool error = false;
      for (size_t i = 0; i < let->bindings.size; ++i) {
        types[i] = CompileType(arena, vars, ntvs, let->bindings.xs[i].type);
        error = error || (types[i] == NULL);

        if (types[i] != NULL) {
          // TODO: get expected_kind from ntvd[i]->kind rather than recompile?
          Kind* expected_kind = CompileKind(arena_, let->bindings.xs[i].kind);
          Kind* actual_kind = GetKind(arena_, types[i]);
          if (!KindsEqual(expected_kind, actual_kind)) {
            FbleReportError("expected kind ", &types[i]->loc);
            PrintKind(expected_kind);
            fprintf(stderr, ", but found ");
            PrintKind(actual_kind);
            fprintf(stderr, "\n");
            error = true;
          }

          FreeKind(arena_, expected_kind);
          FreeKind(arena_, actual_kind);
        }
      }

      for (size_t i = 0; i < let->bindings.size; ++i) {
        RefType* ref = (RefType*)ntvd[i].type;
        assert(ref->_base.tag == REF_TYPE);
        ref->value = types[i];
        if (ref->value != NULL) {
          FbleRefAdd(arena, &ref->_base.ref, &ref->value->ref);
          TypeRelease(arena, ref->value);
        }
        ntvd[i].type = TypeRetain(arena, ref->value);
        TypeRelease(arena, &ref->_base);
      }

      Type* rtype = NULL;
      if (!error) {
        rtype = Compile(arena, vars, ntvs, let->body, instrs);
      }

      for (size_t i = 0; i < let->bindings.size; ++i) {
        TypeRelease(arena, ntvd[i].type);
      }
      return rtype;
    }

    case FBLE_POLY_EXPR: {
      FblePolyExpr* poly = (FblePolyExpr*)expr;

      PolyType* pt = FbleAlloc(arena_, PolyType);
      FbleRefInit(arena, &pt->_base.ref);
      pt->_base.tag = POLY_TYPE;
      pt->_base.loc = expr->loc;
      FbleVectorInit(arena_, pt->args);

      Vars ntvd[poly->args.size];
      Vars* ntvs = type_vars;
      for (size_t i = 0; i < poly->args.size; ++i) {
        AbstractType* arg = FbleAlloc(arena_, AbstractType);
        FbleRefInit(arena, &arg->_base.ref);
        arg->_base.tag = ABSTRACT_TYPE;
        arg->_base.loc = poly->args.xs[i].name.loc;
        arg->kind = CompileKind(arena_, poly->args.xs[i].kind);
        FbleVectorAppend(arena_, pt->args, &arg->_base);
        FbleRefAdd(arena, &pt->_base.ref, &arg->_base.ref);
        TypeRelease(arena, &arg->_base);

        ntvd[i].name = poly->args.xs[i].name;
        ntvd[i].type = &arg->_base;
        ntvd[i].next = ntvs;
        ntvs = ntvd + i;
      }

      pt->body = Compile(arena, vars, ntvs, poly->body, instrs);
      if (pt->body == NULL) {
        TypeRelease(arena, &pt->_base);
        return NULL;
      }

      return &pt->_base;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;
      PolyApplyType* pat = FbleAlloc(arena_, PolyApplyType);
      FbleRefInit(arena, &pat->_base.ref);
      pat->_base.tag = POLY_APPLY_TYPE;
      pat->_base.loc = expr->loc;
      FbleVectorInit(arena_, pat->args);
      pat->result = NULL;

      pat->poly = Compile(arena, vars, type_vars, apply->poly, instrs);
      if (pat->poly == NULL) {
        TypeRelease(arena, &pat->_base);
        return NULL;
      }
      FbleRefAdd(arena, &pat->_base.ref, &pat->poly->ref);
      TypeRelease(arena, pat->poly);

      PolyKind* poly_kind = (PolyKind*)GetKind(arena_, pat->poly);
      if (poly_kind->_base.tag != POLY_KIND) {
        FbleReportError("cannot apply poly args to a basic kinded entity", &expr->loc);
        FreeKind(arena_, &poly_kind->_base);
        TypeRelease(arena, &pat->_base);
        return NULL;
      }

      bool error = false;
      if (apply->args.size > poly_kind->args.size) {
        FbleReportError("expected %i poly args, but %i provided",
            &expr->loc, poly_kind->args.size, apply->args.size);
        error = true;
      }

      for (size_t i = 0; i < apply->args.size; ++i) {
        Type* arg = CompileType(arena, vars, type_vars, apply->args.xs[i]);
        FbleVectorAppend(arena_, pat->args, arg);
        if (arg != NULL) {
          FbleRefAdd(arena, &pat->_base.ref, &arg->ref);
          TypeRelease(arena, arg);
        }
        error = (error || arg == NULL);

        if (arg != NULL && i < poly_kind->args.size) {
          Kind* expected_kind = poly_kind->args.xs[i];
          Kind* actual_kind = GetKind(arena_, arg);
          if (!KindsEqual(expected_kind, actual_kind)) {
            FbleReportError("expected kind ", &arg->loc);
            PrintKind(expected_kind);
            fprintf(stderr, ", but found ");
            PrintKind(actual_kind);
            fprintf(stderr, "\n");
            error = true;
          }
          FreeKind(arena_, actual_kind);
        }
      }

      FreeKind(arena_, &poly_kind->_base);
      if (error) {
        TypeRelease(arena, &pat->_base);
        return NULL;
      }

      return &pat->_base;
    }

    case FBLE_NAMESPACE_EVAL_EXPR:
    case FBLE_NAMESPACE_IMPORT_EXPR: {
      FbleNamespaceExpr* namespace_expr = (FbleNamespaceExpr*)expr;

      Type* type = Compile(arena, vars, type_vars, namespace_expr->nspace, instrs);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      StructType* struct_type = (StructType*)Normal(type);
      if (struct_type->_base.tag != STRUCT_TYPE) {
        FbleReportError("expected value of type struct, but found value of type ", &namespace_expr->nspace->loc);
        PrintType(arena_, type);
        fprintf(stderr, "\n");

        TypeRelease(arena, type);
        return NULL;
      }

      {
        FbleNamespaceInstr* nspace = FbleAlloc(arena_, FbleNamespaceInstr);
        nspace->_base.tag = FBLE_NAMESPACE_INSTR;
        FbleVectorAppend(arena_, *instrs, &nspace->_base);
      }

      Vars ntvd[struct_type->type_fields.size];
      Vars* ntype_vars = (expr->tag == FBLE_NAMESPACE_EVAL_EXPR) ? NULL : type_vars;
      for (size_t i = 0; i < struct_type->type_fields.size; ++i) {
        ntvd[i].name = struct_type->type_fields.xs[i].name;
        ntvd[i].type = struct_type->type_fields.xs[i].type;
        ntvd[i].next = ntype_vars;
        ntype_vars = ntvd + i;
      }

      Vars nvd[struct_type->fields.size];
      Vars* nvars = (expr->tag == FBLE_NAMESPACE_EVAL_EXPR) ? NULL : vars;
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        nvd[i].name = struct_type->fields.xs[i].name;
        nvd[i].type = struct_type->fields.xs[i].type;
        nvd[i].next = nvars;
        nvars = nvd + i;
      }

      Type* rtype = Compile(arena, nvars, ntype_vars, namespace_expr->body, instrs);
      if (rtype == NULL) {
        TypeRelease(arena, type);
        return NULL;
      }

      {
        FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
        descope->_base.tag = FBLE_DESCOPE_INSTR;
        descope->count = struct_type->fields.size;
        FbleVectorAppend(arena_, *instrs, &descope->_base);
      }

      TypeRelease(arena, type);
      return rtype;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// CompileType --
//   Compile and evaluate a type.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the value of variables in scope.
//   type_vars - the value of type variables in scope.
//   type - the type to compile.
//
// Results:
//   The compiled and evaluated type, or NULL in case of error.
//
// Side effects:
//   Prints a message to stderr if the type fails to compile or evalute.
//   Allocates a reference-counted type that must be freed using
//   TypeRelease when it is no longer needed.
static Type* CompileType(TypeArena* arena, Vars* vars, Vars* type_vars, FbleType* type)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      FbleStructType* struct_type = (FbleStructType*)type;
      StructType* st = FbleAlloc(arena_, StructType);
      FbleRefInit(arena, &st->_base.ref);
      st->_base.loc = type->loc;
      st->_base.tag = STRUCT_TYPE;
      FbleVectorInit(arena_, st->type_fields);
      FbleVectorInit(arena_, st->fields);

      for (size_t i = 0; i < struct_type->type_fields.size; ++i) {
        FbleTypeBinding* field = struct_type->type_fields.xs + i;

        Type* type = CompileType(arena, vars, type_vars, field->type);
        if (type == NULL) {
          TypeRelease(arena, &st->_base);
          return NULL;
        }

        Kind* expected_kind = CompileKind(arena_, field->kind);
        if (expected_kind == NULL) {
          TypeRelease(arena, type);
          TypeRelease(arena, &st->_base);
          return NULL;
        }

        Kind* actual_kind = GetKind(arena_, type);
        if (!KindsEqual(expected_kind, actual_kind)) {
          FbleReportError("expected kind ", &type->loc);
          PrintKind(expected_kind);
          fprintf(stderr, ", but found ");
          PrintKind(actual_kind);
          fprintf(stderr, "\n");

          FreeKind(arena_, expected_kind);
          FreeKind(arena_, actual_kind);
          TypeRelease(arena, type);
          TypeRelease(arena, &st->_base);
          return NULL;
        }

        FreeKind(arena_, expected_kind);
        FreeKind(arena_, actual_kind);

        Field* cfield = FbleVectorExtend(arena_, st->type_fields);
        cfield->name = field->name;
        cfield->type = type;
        FbleRefAdd(arena, &st->_base.ref, &cfield->type->ref);
        TypeRelease(arena, type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, struct_type->type_fields.xs[j].name.name)) {
            FbleReportError("duplicate type field name '%s'\n", &field->name.loc, field->name.name);
            TypeRelease(arena, &st->_base);
            return NULL;
          }
        }
      }

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleField* field = struct_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, type_vars, field->type);
        if (compiled == NULL) {
          TypeRelease(arena, &st->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena_, st->fields);
        cfield->name = field->name;
        cfield->type = compiled;
        FbleRefAdd(arena, &st->_base.ref, &cfield->type->ref);
        TypeRelease(arena, cfield->type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, struct_type->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, field->name.name);
            TypeRelease(arena, &st->_base);
            return NULL;
          }
        }
      }
      return &st->_base;
    }

    case FBLE_UNION_TYPE: {
      UnionType* ut = FbleAlloc(arena_, UnionType);
      FbleRefInit(arena, &ut->_base.ref);
      ut->_base.loc = type->loc;
      ut->_base.tag = UNION_TYPE;
      FbleVectorInit(arena_, ut->fields);

      FbleUnionType* union_type = (FbleUnionType*)type;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleField* field = union_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, type_vars, field->type);
        if (compiled == NULL) {
          TypeRelease(arena, &ut->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena_, ut->fields);
        cfield->name = field->name;
        cfield->type = compiled;
        FbleRefAdd(arena, &ut->_base.ref, &cfield->type->ref);
        TypeRelease(arena, cfield->type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, union_type->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, field->name.name);
            TypeRelease(arena, &ut->_base);
            return NULL;
          }
        }
      }
      return &ut->_base;
    }

    case FBLE_FUNC_TYPE: {
      FuncType* ft = FbleAlloc(arena_, FuncType);
      FbleRefInit(arena, &ft->_base.ref);
      ft->_base.loc = type->loc;
      ft->_base.tag = FUNC_TYPE;
      FbleVectorInit(arena_, ft->args);
      ft->rtype = NULL;

      FbleFuncType* func_type = (FbleFuncType*)type;
      for (size_t i = 0; i < func_type->args.size; ++i) {
        Type* arg_type = CompileType(arena, vars, type_vars, func_type->args.xs[i]);
        if (arg_type == NULL) {
          TypeRelease(arena, &ft->_base);
          return NULL;
        }
        FbleVectorAppend(arena_, ft->args, arg_type);
        FbleRefAdd(arena, &ft->_base.ref, &arg_type->ref);
        TypeRelease(arena, arg_type);
      }

      Type* rtype = CompileType(arena, vars, type_vars, func_type->rtype);
      if (rtype == NULL) {
        TypeRelease(arena, &ft->_base);
        return NULL;
      }
      ft->rtype = rtype;
      FbleRefAdd(arena, &ft->_base.ref, &ft->rtype->ref);
      TypeRelease(arena, ft->rtype);
      return &ft->_base;
    }

    case FBLE_PROC_TYPE: {
      ProcType* pt = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &pt->_base.ref);
      pt->_base.loc = type->loc;
      pt->_base.tag = PROC_TYPE;
      pt->rtype = NULL;

      FbleProcType* proc_type = (FbleProcType*)type;
      pt->rtype = CompileType(arena, vars, type_vars, proc_type->rtype);
      if (pt->rtype == NULL) {
        TypeRelease(arena, &pt->_base);
        return NULL;
      }
      FbleRefAdd(arena, &pt->_base.ref, &pt->rtype->ref);
      TypeRelease(arena, pt->rtype);
      return &pt->_base;
    }

    case FBLE_INPUT_TYPE: {
      InputType* pt = FbleAlloc(arena_, InputType);
      FbleRefInit(arena, &pt->_base.ref);
      pt->_base.loc = type->loc;
      pt->_base.tag = INPUT_TYPE;
      pt->rtype = NULL;

      FbleInputType* input_type = (FbleInputType*)type;
      pt->rtype = CompileType(arena, vars, type_vars, input_type->type);
      if (pt->rtype == NULL) {
        TypeRelease(arena, &pt->_base);
        return NULL;
      }
      FbleRefAdd(arena, &pt->_base.ref, &pt->rtype->ref);
      TypeRelease(arena, pt->rtype);
      return &pt->_base;
    }

    case FBLE_OUTPUT_TYPE: {
      OutputType* pt = FbleAlloc(arena_, OutputType);
      FbleRefInit(arena, &pt->_base.ref);
      pt->_base.loc = type->loc;
      pt->_base.tag = OUTPUT_TYPE;
      pt->rtype = NULL;

      FbleOutputType* output_type = (FbleOutputType*)type;
      pt->rtype = CompileType(arena, vars, type_vars, output_type->type);
      if (pt->rtype == NULL) {
        TypeRelease(arena, &pt->_base);
        return NULL;
      }
      FbleRefAdd(arena, &pt->_base.ref, &pt->rtype->ref);
      TypeRelease(arena, pt->rtype);
      return &pt->_base;
    }

    case FBLE_VAR_TYPE: {
      FbleVarType* var_type = (FbleVarType*)type;

      while (type_vars != NULL && !FbleNamesEqual(var_type->var.name, type_vars->name.name)) {
        type_vars = type_vars->next;
      }

      if (type_vars == NULL) {
        FbleReportError("type variable '%s' not defined\n", &var_type->var.loc, var_type->var.name);
        return NULL;
      }

      VarType* vt = FbleAlloc(arena_, VarType);
      FbleRefInit(arena, &vt->_base.ref);
      vt->_base.loc = type->loc;
      vt->_base.tag = VAR_TYPE;
      vt->var = var_type->var;
      vt->value = type_vars->type;
      FbleRefAdd(arena, &vt->_base.ref, &vt->value->ref);
      return &vt->_base;
    }

    case FBLE_LET_TYPE: {
      FbleLetType* let = (FbleLetType*)type;

      Vars ntvd[let->bindings.size];
      Vars* ntvs = type_vars;

      // Set up reference types for all the bindings, to support recursive
      // types.
      for (size_t i = 0; i < let->bindings.size; ++i) {
        RefType* ref = FbleAlloc(arena_, RefType);
        FbleRefInit(arena, &ref->_base.ref);
        ref->_base.tag = REF_TYPE;
        ref->_base.loc = let->bindings.xs[i].name.loc;
        ref->kind = CompileKind(arena_, let->bindings.xs[i].kind);
        ref->value = NULL;

        ntvd[i].name = let->bindings.xs[i].name;
        ntvd[i].type = &ref->_base;
        ntvd[i].next = ntvs;
        ntvs = ntvd + i;
      }

      // Evaluate the type bindings.
      Type* types[let->bindings.size];
      bool error = false;
      for (size_t i = 0; i < let->bindings.size; ++i) {
        types[i] = CompileType(arena, vars, ntvs, let->bindings.xs[i].type);
        error = error || (types[i] == NULL);

        if (types[i] != NULL) {
          // TODO: get expected_kind from ntvd[i]->kind rather than recompile?
          Kind* expected_kind = CompileKind(arena_, let->bindings.xs[i].kind);
          Kind* actual_kind = GetKind(arena_, ntvd[i].type);
          if (!KindsEqual(expected_kind, actual_kind)) {
            FbleReportError("expected kind ", &ntvd[i].type->loc);
            PrintKind(expected_kind);
            fprintf(stderr, ", but found ");
            PrintKind(actual_kind);
            fprintf(stderr, "\n");
            error = true;
          }

          FreeKind(arena_, expected_kind);
          FreeKind(arena_, actual_kind);
        }
      }

      for (size_t i = 0; i < let->bindings.size; ++i) {
        RefType* ref = (RefType*)ntvd[i].type;
        assert(ref->_base.tag == REF_TYPE);
        ref->value = types[i];
        if (ref->value != NULL) {
          FbleRefAdd(arena, &ref->_base.ref, &ref->value->ref);
          TypeRelease(arena, ref->value);
        }
        ntvd[i].type = TypeRetain(arena, ref->value);
        TypeRelease(arena, &ref->_base);
      }

      Type* rtype = NULL;
      if (!error) {
        rtype = CompileType(arena, vars, ntvs, let->body);
      }

      for (size_t i = 0; i < let->bindings.size; ++i) {
        TypeRelease(arena, ntvd[i].type);
      }
      return rtype;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* poly = (FblePolyType*)type;
      PolyType* pt = FbleAlloc(arena_, PolyType);
      FbleRefInit(arena, &pt->_base.ref);
      pt->_base.tag = POLY_TYPE;
      pt->_base.loc = type->loc;
      FbleVectorInit(arena_, pt->args);

      Vars ntvd[poly->args.size];
      Vars* ntvs = type_vars;
      for (size_t i = 0; i < poly->args.size; ++i) {
        AbstractType* arg = FbleAlloc(arena_, AbstractType);
        FbleRefInit(arena, &arg->_base.ref);
        arg->_base.tag = ABSTRACT_TYPE;
        arg->_base.loc = poly->args.xs[i].name.loc;
        arg->kind = CompileKind(arena_, poly->args.xs[i].kind);
        FbleVectorAppend(arena_, pt->args, &arg->_base);
        FbleRefAdd(arena, &pt->_base.ref, &arg->_base.ref);
        TypeRelease(arena, &arg->_base);

        ntvd[i].name = poly->args.xs[i].name;
        ntvd[i].type = &arg->_base;
        ntvd[i].next = ntvs;
        ntvs = ntvd + i;
      }

      pt->body = CompileType(arena, vars, ntvs, poly->body);
      if (pt->body == NULL) {
        TypeRelease(arena, &pt->_base);
        return NULL;
      }
      FbleRefAdd(arena, &pt->_base.ref, &pt->body->ref);
      TypeRelease(arena, pt->body);
      return &pt->_base;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* apply = (FblePolyApplyType*)type;
      PolyApplyType* pat = FbleAlloc(arena_, PolyApplyType);
      FbleRefInit(arena, &pat->_base.ref);
      pat->_base.tag = POLY_APPLY_TYPE;
      pat->_base.loc = type->loc;
      FbleVectorInit(arena_, pat->args);
      pat->result = NULL;

      pat->poly = CompileType(arena, vars, type_vars, apply->poly);
      if (pat->poly == NULL) {
        TypeRelease(arena, &pat->_base);
        return NULL;
      }
      FbleRefAdd(arena, &pat->_base.ref, &pat->poly->ref);
      TypeRelease(arena, pat->poly);

      PolyKind* poly_kind = (PolyKind*)GetKind(arena_, pat->poly);
      if (poly_kind->_base.tag != POLY_KIND) {
        FbleReportError("cannot apply poly args to a basic kinded entity", &type->loc);
        FreeKind(arena_, &poly_kind->_base);
        TypeRelease(arena, &pat->_base);
        return NULL;
      }

      bool error = false;
      if (apply->args.size > poly_kind->args.size) {
        FbleReportError("expected %i poly args, but %i provided",
            &type->loc, poly_kind->args.size, apply->args.size);
        error = true;
      }

      for (size_t i = 0; i < apply->args.size; ++i) {
        Type* arg = CompileType(arena, vars, type_vars, apply->args.xs[i]);
        FbleVectorAppend(arena_, pat->args, arg);
        if (arg != NULL) {
          FbleRefAdd(arena, &pat->_base.ref, &arg->ref);
          TypeRelease(arena, arg);
        }
        error = (error || arg == NULL);

        if (arg != NULL && i < poly_kind->args.size) {
          Kind* expected_kind = poly_kind->args.xs[i];
          Kind* actual_kind = GetKind(arena_, arg);
          if (!KindsEqual(expected_kind, actual_kind)) {
            FbleReportError("expected kind ", &arg->loc);
            PrintKind(expected_kind);
            fprintf(stderr, ", but found ");
            PrintKind(actual_kind);
            fprintf(stderr, "\n");
            error = true;
          }
          FreeKind(arena_, actual_kind);
        }
      }

      FreeKind(arena_, &poly_kind->_base);
      if (error) {
        TypeRelease(arena, &pat->_base);
        return NULL;
      }

      return &pat->_base;
    }

    case FBLE_TYPE_FIELD_ACCESS_TYPE: {
      FbleTypeFieldAccessType* access_type = (FbleTypeFieldAccessType*)type;
      FbleInstrV instrs;
      FbleVectorInit(arena_, instrs);
      Type* type = Compile(arena, vars, type_vars, access_type->expr, &instrs);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      for (size_t i = 0; i < instrs.size; ++i) {
        FreeInstr(arena_, instrs.xs[i]);
      }
      FbleFree(arena_, instrs.xs);

      StructType* struct_type = (StructType*)Normal(type);
      if (struct_type->_base.tag != STRUCT_TYPE) {
        FbleReportError("expected expression of type struct, but found expression of type ", &access_type->expr->loc);
        PrintType(arena_, type);
        fprintf(stderr, "\n");
        TypeRelease(arena, type);
        return NULL;
      }

      for (size_t i = 0; i < struct_type->type_fields.size; ++i) {
        if (FbleNamesEqual(access_type->field.name, struct_type->type_fields.xs[i].name.name)) {
          Type* rtype = TypeRetain(arena, struct_type->type_fields.xs[i].type);
          TypeRelease(arena, type);
          return rtype;
        }
      }

      FbleReportError("%s@ is not a type field of type ", &access_type->field.loc, access_type->field.name);
      PrintType(arena_, type);
      fprintf(stderr, "\n");
      TypeRelease(arena, type);
      return NULL;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// CompileKind --
//   Compile a kind.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the kind to compile.
//
// Results:
//   The compiled kind.
//
// Side effects:
//   Allocates a reference-counted kind that must be freed using FreeKind
//   when it is no longer needed.
static Kind* CompileKind(FbleArena* arena, FbleKind* kind)
{
  switch (kind->tag) {
    case FBLE_BASIC_KIND: {
      FbleBasicKind* basic = (FbleBasicKind*)kind;
      BasicKind* k = FbleAlloc(arena, BasicKind);
      k->_base.tag = BASIC_KIND;
      k->_base.loc = basic->_base.loc;
      k->_base.refcount = 1;
      return &k->_base;
    }

    case FBLE_POLY_KIND: {
      FblePolyKind* poly = (FblePolyKind*)kind;
      PolyKind* k = FbleAlloc(arena, PolyKind);
      k->_base.tag = POLY_KIND;
      k->_base.loc = poly->_base.loc;
      k->_base.refcount = 1;
      FbleVectorInit(arena, k->args);
      for (size_t i = 0; i < poly->args.size; ++i) {
        FbleVectorAppend(arena, k->args, CompileKind(arena, poly->args.xs[i]));
      }
      k->rkind = CompileKind(arena, poly->rkind);
      return &k->_base;
    }
  }

  UNREACHABLE("Should never get here");
}

// FbleFreeInstrBlock -- see documentation in fble-internal.h
void FbleFreeInstrBlock(FbleArena* arena, FbleInstrBlock* block)
{
  if (block == NULL) {
    return;
  }

  assert(block->refcount > 0);
  block->refcount--;
  if (block->refcount == 0) {
    for (size_t i = 0; i < block->instrs.size; ++i) {
      FreeInstr(arena, block->instrs.xs[i]);
    }
    FbleFree(arena, block->instrs.xs);
    FbleFree(arena, block);
  }
}

// FbleCompile -- see documentation in fble-internal.h
FbleInstrBlock* FbleCompile(FbleArena* arena, FbleExpr* expr)
{
  FbleInstrBlock* block = FbleAlloc(arena, FbleInstrBlock);
  block->refcount = 1;
  FbleVectorInit(arena, block->instrs);

  TypeArena* type_arena = FbleNewRefArena(arena, &TypeFree, &TypeAdded);
  Type* type = Compile(type_arena, NULL, NULL, expr, &block->instrs);
  TypeRelease(type_arena, type);
  FbleDeleteRefArena(type_arena);
  if (type == NULL) {
    FbleFreeInstrBlock(arena, block);
    return NULL;
  }

  FbleIPopInstr* ipop = FbleAlloc(arena, FbleIPopInstr);
  ipop->_base.tag = FBLE_IPOP_INSTR;
  FbleVectorAppend(arena, block->instrs, &ipop->_base);
  return block;
}
