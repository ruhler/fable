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
//   POLY_KIND (arg :: Kind) (return :: Kind)
typedef struct {
  Kind _base;
  Kind* arg;
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
  POLY_TYPE,
  POLY_APPLY_TYPE,
  VAR_TYPE,
  TYPE_TYPE,
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
  Type* arg;
  struct Type* rtype;
} FuncType;

// UnaryType -- PROC_TYPE, INPUT_TYPE, OUTPUT_TYPE
typedef struct {
  Type _base;
  Type* type;
} UnaryType;

// PolyType -- POLY_TYPE
typedef struct {
  Type _base;
  Type* arg;
  Type* body;
} PolyType;

// PolyApplyType -- POLY_APPLY_TYPE
//   The 'result' field is the result of evaluating the poly apply type, or
//   NULL if it has not yet been evaluated.
typedef struct {
  Type _base;
  Type* poly;
  Type* arg;
  Type* result;
} PolyApplyType;

// VarType --
//   VAR_TYPE
//
// A variable type whose value may or may not be known. Used for the value of
// type paramaters and recursive type values.
typedef struct VarType {
  Type _base;
  Kind* kind;
  FbleName name;
  Type* value;
} VarType;

// VarTypeV - a vector of var types.
typedef struct {
  size_t size;
  VarType** xs;
} VarTypeV;

// TypeType --
//   TYPE_TYPE
//
// The type of a type.
typedef struct TypeType {
  Type _base;
  Type* type;
} TypeType;

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
static bool HasParam(Type* type, Type* param, TypeList* visited);
static Type* Subst(TypeArena* arena, Type* src, Type* param, Type* arg, TypePairs* tps);
static void Eval(TypeArena* arena, Type* type, TypeList* evaled, PolyApplyList* applied);
static Type* Normal(Type* type);
static bool TypesEqual(Type* a, Type* b, TypePairs* eq);
static bool KindsEqual(Kind* a, Kind* b);
static void PrintType(FbleArena* arena, Type* type, TypeList* printed);
static void PrintKind(Kind* type);

static Type* ValueOfType(TypeArena* arena, Type* typeof);

static void FreeInstr(FbleArena* arena, FbleInstr* instr);

static Type* CompileExpr(TypeArena* arena, Vars* vars, FbleExpr* expr, FbleInstrV* instrs);
static Type* CompileExprNoInstrs(TypeArena* arena, Vars* vars, FbleExpr* expr);
static Type* CompileType(TypeArena* arena, Vars* vars, FbleType* type);
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
          FreeKind(arena, poly->arg);
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
    case INPUT_TYPE:
    case OUTPUT_TYPE:
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

    case PROC_TYPE:
    case INPUT_TYPE:
    case OUTPUT_TYPE: {
      UnaryType* ut = (UnaryType*)type;
      Add(add, ut->type);
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

    case POLY_TYPE: {
      PolyType* poly = (PolyType*)type;
      PolyKind* kind = FbleAlloc(arena, PolyKind);
      kind->_base.tag = POLY_KIND;
      kind->_base.loc = type->loc;
      kind->_base.refcount = 1;
      kind->arg = GetKind(arena, poly->arg);
      kind->rkind = GetKind(arena, poly->body);
      return &kind->_base;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PolyKind* kind = (PolyKind*)GetKind(arena, pat->poly);
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

      // TODO: This isn't right once we add non-type basic kinds internally.
      // We need to adjust the kind in that case.
      // TODO: Maybe we should arrange for TYPE_TYPE to only ever be applied
      // to basic types?
      return GetKind(arena, type_type->type);
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

    case PROC_TYPE:
    case INPUT_TYPE:
    case OUTPUT_TYPE: {
      UnaryType* ut = (UnaryType*)type;
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
//   given type.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the type to substitute into.
//   param - the abstract type to substitute out.
//   arg - the concrete type to substitute in.
//   tps - a map of already substituted types, to avoid infinite recursion.
//
// Results:
//   A type with all occurrences of param replaced with the arg type.
//
// Side effects:
//   The caller is responsible for calling TypeRelease on the returned
//   type when it is no longer needed.
static Type* Subst(TypeArena* arena, Type* type, Type* param, Type* arg, TypePairs* tps)
{
  if (!HasParam(type, param, NULL)) {
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

      FbleVectorInit(arena_, sst->fields);
      for (size_t i = 0; i < st->fields.size; ++i) {
        Field* field = FbleVectorExtend(arena_, sst->fields);
        field->name = st->fields.xs[i].name;
        field->type = Subst(arena, st->fields.xs[i].type, param, arg, tps);
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
        field->type = Subst(arena, ut->fields.xs[i].type, param, arg, tps);
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

      sft->arg = Subst(arena, ft->arg, param, arg, tps);
      FbleRefAdd(arena, &sft->_base.ref, &sft->arg->ref);
      TypeRelease(arena, sft->arg);

      sft->rtype = Subst(arena, ft->rtype, param, arg, tps);
      FbleRefAdd(arena, &sft->_base.ref, &sft->rtype->ref);
      TypeRelease(arena, sft->rtype);
      return &sft->_base;
    }

    case PROC_TYPE:
    case INPUT_TYPE:
    case OUTPUT_TYPE: {
      UnaryType* ut = (UnaryType*)type;
      UnaryType* sut = FbleAlloc(arena_, UnaryType);
      FbleRefInit(arena, &sut->_base.ref);
      sut->_base.tag = ut->_base.tag;
      sut->_base.loc = ut->_base.loc;
      sut->type = Subst(arena, ut->type, param, arg, tps);
      FbleRefAdd(arena, &sut->_base.ref, &sut->type->ref);
      TypeRelease(arena, sut->type);
      return &sut->_base;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      PolyType* spt = FbleAlloc(arena_, PolyType);
      spt->_base.tag = POLY_TYPE;
      spt->_base.loc = pt->_base.loc;
      FbleRefInit(arena, &spt->_base.ref);
      spt->arg = pt->arg;
      FbleRefAdd(arena, &spt->_base.ref, &pt->arg->ref);
      spt->body = Subst(arena, pt->body, param, arg, tps);
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
      spat->poly = Subst(arena, pat->poly, param, arg, tps);
      FbleRefAdd(arena, &spat->_base.ref, &spat->poly->ref);
      TypeRelease(arena, spat->poly);
      spat->result = NULL;
      spat->arg = Subst(arena, pat->arg, param, arg, tps);
      FbleRefAdd(arena, &spat->_base.ref, &spat->arg->ref);
      TypeRelease(arena, spat->arg);
      return &spat->_base;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      if (var->value == NULL) {
        return TypeRetain(arena, (type == param ? arg : type));
      }

      // Check to see if we've already done substitution on the value pointed
      // to by this ref.
      for (TypePairs* tp = tps; tp != NULL; tp = tp->next) {
        if (tp->a == var->value) {
          return TypeRetain(arena, tp->b);
        }
      }

      VarType* svar = FbleAlloc(arena_, VarType);
      FbleRefInit(arena, &svar->_base.ref);
      svar->_base.tag = VAR_TYPE;
      svar->_base.loc = type->loc;
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
      TypeRelease(arena, &svar->_base);
      return value;
    }

    case TYPE_TYPE: {
      TypeType* tt = (TypeType*)type;
      TypeType* stt = FbleAlloc(arena_, TypeType);
      FbleRefInit(arena, &stt->_base.ref);
      stt->_base.tag = TYPE_TYPE;
      stt->_base.loc = tt->_base.loc;
      stt->type = Subst(arena, tt->type, param, arg, tps);
      FbleRefAdd(arena, &stt->_base.ref, &stt->type->ref);
      TypeRelease(arena, stt->type);
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
      Eval(arena, ft->arg, &nevaled, applied);
      Eval(arena, ft->rtype, &nevaled, applied);
      return;
    }

    case PROC_TYPE:
    case INPUT_TYPE:
    case OUTPUT_TYPE: {
      UnaryType* ut = (UnaryType*)type;
      Eval(arena, ut->type, &nevaled, applied);
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
      Eval(arena, pat->arg, &nevaled, applied);

      // Check whether we have already applied the arg to this poly.
      for (PolyApplyList* pal = applied; pal != NULL; pal = pal->next) {
        if (TypesEqual(pal->poly, pat->poly, NULL)) {
          if (TypesEqual(pal->arg, pat->arg, NULL)) {
            pat->result = pal->result;
            assert(&pat->_base != Normal(pat->result));
            FbleRefAdd(arena, &pat->_base.ref, &pat->result->ref);
            return;
          }
        }
      }

      PolyType* poly = (PolyType*)Normal(pat->poly);
      if (poly->_base.tag == POLY_TYPE) {
        pat->result = Subst(arena, poly->body, poly->arg, pat->arg, NULL);
        assert(&pat->_base != Normal(pat->result));
        FbleRefAdd(arena, &pat->_base.ref, &pat->result->ref);
        TypeRelease(arena, pat->result);

        PolyApplyList napplied = {
          .poly = &poly->_base,
          .arg = pat->arg,
          .result = pat->result,
          .next = applied
        };

        Eval(arena, pat->result, &nevaled, &napplied);
      }
      return;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;

      if (var->value != NULL) {
        Eval(arena, var->value, &nevaled, applied);
      }
      return;
    }

    case TYPE_TYPE: {
      TypeType* tt = (TypeType*)type;
      Eval(arena, tt->type, &nevaled, applied);
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
    case POLY_TYPE: return type;

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      if (pat->result == NULL) {
        return type;
      }
      return Normal(pat->result);
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      if (var->value == NULL) {
        return type;
      }
      return Normal(var->value);
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
      return TypesEqual(fta->arg, ftb->arg, &neq)
          && TypesEqual(fta->rtype, ftb->rtype, &neq);
    }

    case PROC_TYPE:
    case INPUT_TYPE:
    case OUTPUT_TYPE: {
      UnaryType* uta = (UnaryType*)a;
      UnaryType* utb = (UnaryType*)b;
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
      return KindsEqual(pa->arg, pb->arg)
          && KindsEqual(pa->rkind, pb->rkind);
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
//   printed - list of types in the process of printing, to prevent infinite
//             recursion.
//
// Result:
//   None.
//
// Side effect:
//   Prints the given type in human readable form to stderr.
static void PrintType(FbleArena* arena, Type* type, TypeList* printed)
{
  for (TypeList* p = printed; p != NULL; p = p->next) {
    if (type == p->type) {
      fprintf(stderr, "...");
      return;
    }
  }

  TypeList nprinted = {
    .type = type,
    .next = printed
  };

  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      fprintf(stderr, "*(");
      const char* comma = "";
      for (size_t i = 0; i < st->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(arena, st->fields.xs[i].type, &nprinted);
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
        PrintType(arena, ut->fields.xs[i].type, &nprinted);
        fprintf(stderr, " %s", ut->fields.xs[i].name.name);
        comma = ", ";
      }
      fprintf(stderr, ")");
      break;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      fprintf(stderr, "[");
      PrintType(arena, ft->arg, &nprinted);
      fprintf(stderr, "]{");
      PrintType(arena, ft->rtype, &nprinted);
      fprintf(stderr, ";}");
      break;
    }

    case PROC_TYPE: {
      UnaryType* ut = (UnaryType*)type;
      PrintType(arena, ut->type, &nprinted);
      fprintf(stderr, "!");
      break;
    }

    case INPUT_TYPE: {
      UnaryType* ut = (UnaryType*)type;
      PrintType(arena, ut->type, &nprinted);
      fprintf(stderr, "-");
      break;
    }

    case OUTPUT_TYPE: {
      UnaryType* ut = (UnaryType*)type;
      PrintType(arena, ut->type, &nprinted);
      fprintf(stderr, "+");
      break;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      fprintf(stderr, "<");
      Kind* kind = GetKind(arena, pt->arg);
      PrintKind(kind);
      FreeKind(arena, kind);
      fprintf(stderr, " ");
      PrintType(arena, pt->arg, &nprinted);
      fprintf(stderr, "> { ");
      PrintType(arena, pt->body, &nprinted);
      fprintf(stderr, "; }");
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PrintType(arena, pat->poly, &nprinted);
      fprintf(stderr, "<");
      PrintType(arena, pat->arg, &nprinted);
      fprintf(stderr, ">");
      break;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      if (var->value == NULL) {
        fprintf(stderr, "%s_%lx", var->name.name, (unsigned long)type);
      } else {
        PrintType(arena, var->value, &nprinted);
      }
      break;
    }

    case TYPE_TYPE: {
      TypeType* tt = (TypeType*)type;
      fprintf(stderr, "@<");
      PrintType(arena, tt->type, &nprinted);
      fprintf(stderr, ">");
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
      PrintKind(poly->arg);
      fprintf(stderr, "; ");
      PrintKind(poly->rkind);
      fprintf(stderr, ">");
      break;
    }
  }
}

// ValueOfType --
//   Returns the value of a type given the type of the type.
//
// Inputs:
//   arena - type arena for allocations.
//   typeof - the type of the type to get the value of.
//
// Results:
//   The value of the type to get the value of. Or NULL if the value is not a
//   type.
//
// Side effects:
//   The returned type must be released using TypeRelease when no longer
//   needed.
static Type* ValueOfType(TypeArena* arena, Type* typeof)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (typeof->tag) {
    case STRUCT_TYPE:
    case UNION_TYPE:
    case FUNC_TYPE:
    case PROC_TYPE:
    case INPUT_TYPE:
    case OUTPUT_TYPE: {
      return NULL;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)typeof;
      Type* valueof = ValueOfType(arena, pt->body);
      if (valueof == NULL) {
        return NULL;
      }

      PolyType* spt = FbleAlloc(arena_, PolyType);
      spt->_base.tag = POLY_TYPE;
      spt->_base.loc = pt->_base.loc;
      FbleRefInit(arena, &spt->_base.ref);
      spt->arg = pt->arg;
      FbleRefAdd(arena, &spt->_base.ref, &pt->arg->ref);
      spt->body = valueof;
      FbleRefAdd(arena, &spt->_base.ref, &spt->body->ref);
      TypeRelease(arena, spt->body);
      return &spt->_base;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)typeof;
      Type* valueof = ValueOfType(arena, pat->poly);
      if (valueof == NULL) {
        return NULL;
      }

      PolyApplyType* spat = FbleAlloc(arena_, PolyApplyType);
      FbleRefInit(arena, &spat->_base.ref);
      spat->_base.tag = POLY_APPLY_TYPE;
      spat->_base.loc = pat->_base.loc;
      spat->poly = valueof;
      FbleRefAdd(arena, &spat->_base.ref, &spat->poly->ref);
      TypeRelease(arena, spat->poly);
      spat->result = NULL;
      spat->arg = pat->arg;
      FbleRefAdd(arena, &spat->_base.ref, &spat->arg->ref);
      return &spat->_base;
    }

    case VAR_TYPE: {
      assert(false && "TODO: value of var type");
      return NULL;
    }

    case TYPE_TYPE: {
      TypeType* type_type = (TypeType*)Normal(typeof);
      return TypeRetain(arena, type_type->type);
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
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
    case FBLE_IPOP_INSTR:
    case FBLE_TYPE_INSTR:
    case FBLE_VPUSH_INSTR: {
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

// CompileExpr --
//   Type check and compile the given expression. Returns the type of the
//   expression and generates instructions to compute the value of that
//   expression at runtime.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the list of variables in scope.
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
static Type* CompileExpr(TypeArena* arena, Vars* vars, FbleExpr* expr, FbleInstrV* instrs)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (expr->tag) {
    case FBLE_STRUCT_TYPE_EXPR:
    case FBLE_UNION_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_INPUT_TYPE_EXPR:
    case FBLE_OUTPUT_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR: {
      Type* type = CompileType(arena, vars, expr);
      if (type == NULL) {
        return NULL;
      }

      assert(type->tag != POLY_TYPE && "TODO: typeof poly type");

      TypeType* type_type = FbleAlloc(arena_, TypeType);
      FbleRefInit(arena, &type_type->_base.ref);
      type_type->_base.tag = TYPE_TYPE;
      type_type->_base.loc = expr->loc;
      type_type->type = type;
      FbleRefAdd(arena, &type_type->_base.ref, &type_type->type->ref);
      TypeRelease(arena, type);

      FbleTypeInstr* instr = FbleAlloc(arena_, FbleTypeInstr);
      instr->_base.tag = FBLE_TYPE_INSTR;
      FbleVectorAppend(arena_, *instrs, &instr->_base);
      return &type_type->_base;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleMiscApplyExpr* misc_apply_expr = (FbleMiscApplyExpr*)expr;
      Type* type = CompileExpr(arena, vars, misc_apply_expr->misc, instrs);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      Type* normal = Normal(type);
      if (normal->tag == INPUT_TYPE) {
        // FBLE_GET_EXPR
        if (misc_apply_expr->args.size != 0) {
          FbleReportError("too many arguments to get. expected 0.\n", &expr->loc);
          TypeRelease(arena, type);
          return NULL;
        }

        UnaryType* input_type = (UnaryType*)normal;
        FbleGetInstr* get_instr = FbleAlloc(arena_, FbleGetInstr);
        get_instr->_base.tag = FBLE_GET_INSTR;
        FbleVectorAppend(arena_, *instrs, &get_instr->_base);

        UnaryType* proc_type = FbleAlloc(arena_, UnaryType);
        FbleRefInit(arena, &proc_type->_base.ref);
        proc_type->_base.tag = PROC_TYPE;
        proc_type->_base.loc = expr->loc;
        proc_type->type = input_type->type;
        FbleRefAdd(arena, &proc_type->_base.ref, &proc_type->type->ref);
        TypeRelease(arena, type);
        return &proc_type->_base;
      }

      if (normal->tag == OUTPUT_TYPE) {
        // FBLE_PUT_EXPR
        if (misc_apply_expr->args.size != 1) {
          FbleReportError("wrong number of arguments to put. expected 1.\n", &expr->loc);
          TypeRelease(arena, type);
          return NULL;
        }

        UnaryType* output_type = (UnaryType*)normal;

        // Compile the argument.
        FbleExpr* arg = misc_apply_expr->args.xs[0];
        Type* arg_type = CompileExpr(arena, vars, arg, instrs);
        Eval(arena, arg_type, NULL, NULL);
        bool error = (arg_type == NULL);

        if (arg_type != NULL && !TypesEqual(output_type->type, arg_type, NULL)) {
          FbleReportError("expected type ", &arg->loc);
          PrintType(arena_, output_type->type, NULL);
          fprintf(stderr, ", but found ");
          PrintType(arena_, arg_type, NULL);
          fprintf(stderr, "\n");
          error = true;
        }

        TypeRelease(arena, arg_type);

        if (error) {
          TypeRelease(arena, type);
          return NULL;
        }

        FblePutInstr* put_instr = FbleAlloc(arena_, FblePutInstr);
        put_instr->_base.tag = FBLE_PUT_INSTR;
        FbleVectorAppend(arena_, *instrs, &put_instr->_base);

        UnaryType* proc_type = FbleAlloc(arena_, UnaryType);
        FbleRefInit(arena, &proc_type->_base.ref);
        proc_type->_base.tag = PROC_TYPE;
        proc_type->_base.loc = expr->loc;
        proc_type->type = output_type->type;
        FbleRefAdd(arena, &proc_type->_base.ref, &proc_type->type->ref);
        TypeRelease(arena, type);
        return &proc_type->_base;
      }

      // FBLE_STRUCT_VALUE_EXPR
      Type* vtype = ValueOfType(arena, normal);
      Eval(arena, vtype, NULL, NULL);
      TypeRelease(arena, type);
      if (vtype == NULL) {
        FbleReportError("expected a type.\n", &misc_apply_expr->misc->loc);
        return NULL;
      }

      StructType* struct_type = (StructType*)Normal(vtype);
      if (struct_type->_base.tag != STRUCT_TYPE) {
        FbleReportError("expected a struct type, but found ", &misc_apply_expr->misc->loc);
        PrintType(arena_, vtype, NULL);
        fprintf(stderr, "\n");
        TypeRelease(arena, vtype);
        return NULL;
      }

      if (struct_type->fields.size != misc_apply_expr->args.size) {
        // TODO: Where should the error message go?
        FbleReportError("expected %i args, but %i were provided\n",
            &expr->loc, struct_type->fields.size, misc_apply_expr->args.size);
        TypeRelease(arena, vtype);
        return NULL;
      }

      bool error = false;
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        Field* field = struct_type->fields.xs + i;

        Type* arg_type = CompileExpr(arena, vars, misc_apply_expr->args.xs[i], instrs);
        Eval(arena, arg_type, NULL, NULL);
        error = error || (arg_type == NULL);

        if (arg_type != NULL && !TypesEqual(field->type, arg_type, NULL)) {
          FbleReportError("expected type ", &misc_apply_expr->args.xs[i]->loc);
          PrintType(arena_, field->type, NULL);
          fprintf(stderr, ", but found ");
          PrintType(arena_, arg_type, NULL);
          fprintf(stderr, "\n");
          error = true;
        }
        TypeRelease(arena, arg_type);
      }

      if (error) {
        TypeRelease(arena, vtype);
        return NULL;
      }

      FbleStructValueInstr* struct_instr = FbleAlloc(arena_, FbleStructValueInstr);
      struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      struct_instr->argc = struct_type->fields.size;
      FbleVectorAppend(arena_, *instrs, &struct_instr->_base);
      return vtype;
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      FbleStructValueImplicitTypeExpr* struct_expr = (FbleStructValueImplicitTypeExpr*)expr;
      StructType* struct_type = FbleAlloc(arena_, StructType);
      FbleRefInit(arena, &struct_type->_base.ref);
      struct_type->_base.loc = expr->loc;
      struct_type->_base.tag = STRUCT_TYPE;
      FbleVectorInit(arena_, struct_type->fields);

      FbleTypeInstr* instr = FbleAlloc(arena_, FbleTypeInstr);
      instr->_base.tag = FBLE_TYPE_INSTR;
      FbleVectorAppend(arena_, *instrs, &instr->_base);

      bool error = false;
      for (size_t i = 0; i < struct_expr->args.size; ++i) {
        FbleChoice* choice = struct_expr->args.xs + i;
        Type* arg_type = CompileExpr(arena, vars, choice->expr, instrs);
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
      Type* type = CompileType(arena, vars, union_value_expr->type);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      UnionType* union_type = (UnionType*)Normal(type);
      if (union_type->_base.tag != UNION_TYPE) {
        FbleReportError("expected a union type, but found ", &union_value_expr->type->loc);
        PrintType(arena_, type, NULL);
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
        PrintType(arena_, type, NULL);
        fprintf(stderr, "\n");
        TypeRelease(arena, type);
        return NULL;
      }

      Type* arg_type = CompileExpr(arena, vars, union_value_expr->arg, instrs);
      Eval(arena, arg_type, NULL, NULL);
      if (arg_type == NULL) {
        TypeRelease(arena, type);
        return NULL;
      }

      if (!TypesEqual(field_type, arg_type, NULL)) {
        FbleReportError("expected type ", &union_value_expr->arg->loc);
        PrintType(arena_, field_type, NULL);
        fprintf(stderr, ", but found type ");
        PrintType(arena_, arg_type, NULL);
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

    case FBLE_MISC_ACCESS_EXPR: {
      FbleMiscAccessExpr* access_expr = (FbleMiscAccessExpr*)expr;

      Type* type = CompileExpr(arena, vars, access_expr->object, instrs);
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
        PrintType(arena_, type, NULL);
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
      PrintType(arena_, type, NULL);
      fprintf(stderr, "\n");
      TypeRelease(arena, type);
      return NULL;
    }

    case FBLE_COND_EXPR: {
      FbleCondExpr* cond_expr = (FbleCondExpr*)expr;

      Type* type = CompileExpr(arena, vars, cond_expr->condition, instrs);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      UnionType* union_type = (UnionType*)Normal(type);
      if (union_type->_base.tag != UNION_TYPE) {
        FbleReportError("expected value of union type, but found value of type ", &cond_expr->condition->loc);
        PrintType(arena_, type, NULL);
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

        Type* arg_type = CompileExpr(arena, vars, cond_expr->choices.xs[i].expr, &choice->instrs);

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
            PrintType(arena_, return_type, NULL);
            fprintf(stderr, ", but found ");
            PrintType(arena_, arg_type, NULL);
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
      type->arg = NULL;
      type->rtype = NULL;

      Vars nvars = {
        .name = func_value_expr->arg.name,
        .type = NULL,
        .next = vars,
      };
      
      type->arg = CompileType(arena, vars, func_value_expr->arg.type);
      nvars.type = type->arg;
      if (type->arg != NULL) {
        FbleRefAdd(arena, &type->_base.ref, &type->arg->ref);
        TypeRelease(arena, type->arg);
      }
      bool error = (type->arg == NULL);

      FbleFuncValueInstr* instr = FbleAlloc(arena_, FbleFuncValueInstr);
      instr->_base.tag = FBLE_FUNC_VALUE_INSTR;
      FbleVectorAppend(arena_, *instrs, &instr->_base);

      instr->contextc = 0;
      for (Vars* v = vars; v != NULL; v = v->next) {
        instr->contextc++;
      }

      instr->body = FbleAlloc(arena_, FbleInstrBlock);
      instr->body->refcount = 1;
      FbleVectorInit(arena_, instr->body->instrs);

      if (!error) {
        type->rtype = CompileExpr(arena, &nvars, func_value_expr->body, &instr->body->instrs);
        if (type->rtype != NULL) {
          FbleRefAdd(arena, &type->_base.ref, &type->rtype->ref);
          TypeRelease(arena, type->rtype);
        }
        error = error || (type->rtype == NULL);

        if (!error) {
          FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
          descope->_base.tag = FBLE_DESCOPE_INSTR;
          descope->count = 1;
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

    case FBLE_FUNC_APPLY_EXPR: {
      FbleFuncApplyExpr* apply_expr = (FbleFuncApplyExpr*)expr;

      // Compile the function value.
      Type* type = CompileExpr(arena, vars, apply_expr->func, instrs);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      FuncType* func_type = (FuncType*)Normal(type);
      if (func_type->_base.tag != FUNC_TYPE) {
        FbleReportError("expected function, but found something of type ", &expr->loc);
        PrintType(arena_, type, NULL);
        fprintf(stderr, "\n");
        TypeRelease(arena, type);
        return NULL;
      }

      // Compile the argument.
      Type* arg_type = CompileExpr(arena, vars, apply_expr->arg, instrs);
      Eval(arena, arg_type, NULL, NULL);
      bool error = (arg_type == NULL);

      if (arg_type != NULL && !TypesEqual(func_type->arg, arg_type, NULL)) {
        FbleReportError("expected type ", &apply_expr->arg->loc);
        PrintType(arena_, func_type->arg, NULL);
        fprintf(stderr, ", but found ");
        PrintType(arena_, arg_type, NULL);
        fprintf(stderr, "\n");
        error = true;
      }

      TypeRelease(arena, arg_type);

      if (error) {
        TypeRelease(arena, type);
        return NULL;
      }

      FbleFuncApplyInstr* apply_instr = FbleAlloc(arena_, FbleFuncApplyInstr);
      apply_instr->_base.tag = FBLE_FUNC_APPLY_INSTR;

      FbleVectorAppend(arena_, *instrs, &apply_instr->_base);
      Type* rtype = TypeRetain(arena, func_type->rtype);
      TypeRelease(arena, type);
      return rtype;
    }

    case FBLE_EVAL_EXPR: {
      FbleEvalExpr* eval_expr = (FbleEvalExpr*)expr;

      Type* type = CompileExpr(arena, vars, eval_expr->expr, instrs);
      if (type == NULL) {
        return NULL;
      }

      UnaryType* proc_type = FbleAlloc(arena_, UnaryType);
      FbleRefInit(arena, &proc_type->_base.ref);
      proc_type->_base.tag = PROC_TYPE;
      proc_type->_base.loc = expr->loc;
      proc_type->type = type;
      FbleRefAdd(arena, &proc_type->_base.ref, &proc_type->type->ref);
      TypeRelease(arena, proc_type->type);

      FbleEvalInstr* eval_instr = FbleAlloc(arena_, FbleEvalInstr);
      eval_instr->_base.tag = FBLE_EVAL_INSTR;
      FbleVectorAppend(arena_, *instrs, &eval_instr->_base);
      return &proc_type->_base;
    }

    case FBLE_LINK_EXPR: {
      FbleLinkExpr* link_expr = (FbleLinkExpr*)expr;
      Type* port_type = CompileType(arena, vars, link_expr->type);
      if (port_type == NULL) {
        return NULL;
      }

      UnaryType* get_type = FbleAlloc(arena_, UnaryType);
      FbleRefInit(arena, &get_type->_base.ref);
      get_type->_base.tag = INPUT_TYPE;
      get_type->_base.loc = port_type->loc;
      get_type->type = port_type;
      FbleRefAdd(arena, &get_type->_base.ref, &get_type->type->ref);
      Vars get_var = {
        .name = link_expr->get,
        .type = &get_type->_base,
        .next = vars
      };

      UnaryType* put_type = FbleAlloc(arena_, UnaryType);
      FbleRefInit(arena, &put_type->_base.ref);
      put_type->_base.tag = OUTPUT_TYPE;
      put_type->_base.loc = port_type->loc;
      put_type->type = port_type;
      FbleRefAdd(arena, &put_type->_base.ref, &put_type->type->ref);
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

      Type* type = CompileExpr(arena, &put_var, link_expr->body, &instr->body->instrs);
      Eval(arena, type, NULL, NULL);

      FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
      descope->_base.tag = FBLE_DESCOPE_INSTR;
      descope->count = 2;
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
        FbleReportError("expected a value of type proc, but found ", &link_expr->body->loc);
        PrintType(arena_, type, NULL);
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
        nvd[i].type = CompileType(arena, vars, exec_expr->bindings.xs[i].type);
        Eval(arena, nvd[i].type, NULL, NULL);
        error = error || (nvd[i].type == NULL);
        nvd[i].next = nvars;
        nvars = nvd + i;
      }

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        Type* type = CompileExpr(arena, vars, exec_expr->bindings.xs[i].expr, instrs);
        Eval(arena, type, NULL, NULL);
        error = error || (type == NULL);

        if (type != NULL) {
          UnaryType* proc_type = (UnaryType*)Normal(type);
          if (proc_type->_base.tag == PROC_TYPE) {
            if (nvd[i].type != NULL && !TypesEqual(nvd[i].type, proc_type->type, NULL)) {
              error = true;
              FbleReportError("expected type ", &exec_expr->bindings.xs[i].expr->loc);
              PrintType(arena_, nvd[i].type, NULL);
              fprintf(stderr, "!, but found ");
              PrintType(arena_, type, NULL);
              fprintf(stderr, "\n");
            }
          } else {
            error = true;
            FbleReportError("expected process, but found expression of type",
                &exec_expr->bindings.xs[i].expr->loc);
            PrintType(arena_, type, NULL);
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
        rtype = CompileExpr(arena, nvars, exec_expr->body, &exec_instr->body->instrs);
        Eval(arena, rtype, NULL, NULL);
        error = (rtype == NULL);
      }

      if (rtype != NULL && Normal(rtype)->tag != PROC_TYPE) {
        error = true;
        FbleReportError("expected a value of type proc, but found ", &exec_expr->body->loc);
        PrintType(arena_, rtype, NULL);
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
      descope->count = exec_instr->argc;
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
      VarType* var_types[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleBinding* binding = let_expr->bindings.xs + i;

        nvd[i].name = let_expr->bindings.xs[i].name;
        if (binding->type == NULL) {
          assert(binding->kind != NULL);
          VarType* var = FbleAlloc(arena_, VarType);
          FbleRefInit(arena, &var->_base.ref);
          var->_base.tag = VAR_TYPE;
          var->_base.loc = binding->name.loc;
          var->name = let_expr->bindings.xs[i].name;
          var->kind = CompileKind(arena_, binding->kind);
          var->value = NULL;

          TypeType* type_type = FbleAlloc(arena_, TypeType);
          FbleRefInit(arena, &type_type->_base.ref);
          type_type->_base.tag = TYPE_TYPE;
          type_type->_base.loc = binding->name.loc;
          type_type->type = &var->_base;
          FbleRefAdd(arena, &type_type->_base.ref, &type_type->type->ref);
          TypeRelease(arena, &var->_base);

          nvd[i].type = &type_type->_base;
          var_types[i] = var;
        } else {
          assert(binding->kind == NULL);
          nvd[i].type = CompileType(arena, vars, binding->type);
          Eval(arena, nvd[i].type, NULL, NULL);
          error = error || (nvd[i].type == NULL);
        }

        nvd[i].next = nvars;
        nvars = nvd + i;
      }

      FbleLetPrepInstr* let_prep_instr = FbleAlloc(arena_, FbleLetPrepInstr);
      let_prep_instr->_base.tag = FBLE_LET_PREP_INSTR;
      let_prep_instr->count = let_expr->bindings.size;
      FbleVectorAppend(arena_, *instrs, &let_prep_instr->_base);

      // Compile the values of the variables.
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleBinding* binding = let_expr->bindings.xs + i;

        Type* type = NULL;
        if (!error) {
          type = CompileExpr(arena, nvars, binding->expr, instrs);
          Eval(arena, type, NULL, NULL);
        }
        error = error || (type == NULL);

        if (!error && binding->type != NULL && !TypesEqual(nvd[i].type, type, NULL)) {
          error = true;
          FbleReportError("expected type ", &let_expr->bindings.xs[i].expr->loc);
          PrintType(arena_, nvd[i].type, NULL);
          fprintf(stderr, ", but found ");
          PrintType(arena_, type, NULL);
          fprintf(stderr, "\n");
        } else if (!error && binding->type == NULL) {
          VarType* var = var_types[i];

          Kind* expected_kind = var->kind;
          Kind* actual_kind = GetKind(arena_, type);
          if (!KindsEqual(expected_kind, actual_kind)) {
            FbleReportError("expected kind ", &type->loc);
            PrintKind(expected_kind);
            fprintf(stderr, ", but found ");
            PrintKind(actual_kind);
            fprintf(stderr, "\n");
            error = true;
          }
          FreeKind(arena_, actual_kind);

          var->value = ValueOfType(arena, type);
          if (var->value != NULL) {
            FbleRefAdd(arena, &var->_base.ref, &var->value->ref);
            TypeRelease(arena, var->value);
          }
        }

        TypeRelease(arena, type);
      }

      FbleLetDefInstr* let_def_instr = FbleAlloc(arena_, FbleLetDefInstr);
      let_def_instr->_base.tag = FBLE_LET_DEF_INSTR;
      let_def_instr->count = let_expr->bindings.size;
      FbleVectorAppend(arena_, *instrs, &let_def_instr->_base);

      Type* rtype = NULL;
      if (!error) {
        rtype = CompileExpr(arena, nvars, let_expr->body, instrs);
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

    case FBLE_POLY_EXPR: {
      FblePolyExpr* poly = (FblePolyExpr*)expr;

      PolyType* pt = FbleAlloc(arena_, PolyType);
      FbleRefInit(arena, &pt->_base.ref);
      pt->_base.tag = POLY_TYPE;
      pt->_base.loc = expr->loc;

      VarType* arg = FbleAlloc(arena_, VarType);
      FbleRefInit(arena, &arg->_base.ref);
      arg->_base.tag = VAR_TYPE;
      arg->_base.loc = poly->arg.name.loc;
      arg->name = poly->arg.name;
      arg->kind = CompileKind(arena_, poly->arg.kind);
      arg->value = NULL;
      pt->arg = &arg->_base;
      FbleRefAdd(arena, &pt->_base.ref, &arg->_base.ref);

      TypeType* type_type = FbleAlloc(arena_, TypeType);
      FbleRefInit(arena, &type_type->_base.ref);
      type_type->_base.tag = TYPE_TYPE;
      type_type->_base.loc = poly->arg.name.loc;
      type_type->type = &arg->_base;
      FbleRefAdd(arena, &type_type->_base.ref, &arg->_base.ref);

      TypeRelease(arena, &arg->_base);

      Vars nvars = {
        .name = poly->arg.name,
        .type = &type_type->_base,
        .next = vars
      };

      // TODO: It's a little silly that we are pushing an empty type value
      // here. Oh well. Maybe in the future we'll optimize those away or
      // add support for non-type poly args too.
      FbleTypeInstr* type_instr = FbleAlloc(arena_, FbleTypeInstr);
      type_instr->_base.tag = FBLE_TYPE_INSTR;
      FbleVectorAppend(arena_, *instrs, &type_instr->_base);

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      FbleVectorAppend(arena_, *instrs, &vpush->_base);

      pt->body = CompileExpr(arena, &nvars, poly->body, instrs);
      TypeRelease(arena, &type_type->_base);

      FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
      descope->_base.tag = FBLE_DESCOPE_INSTR;
      descope->count = 1;
      FbleVectorAppend(arena_, *instrs, &descope->_base);

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
      pat->poly = NULL;
      pat->arg = NULL;
      pat->result = NULL;

      pat->poly = CompileExpr(arena, vars, apply->poly, instrs);
      if (pat->poly == NULL) {
        TypeRelease(arena, &pat->_base);
        return NULL;
      }
      FbleRefAdd(arena, &pat->_base.ref, &pat->poly->ref);
      TypeRelease(arena, pat->poly);

      pat->arg = CompileType(arena, vars, apply->arg);
      if (pat->arg == NULL) {
        TypeRelease(arena, &pat->_base);
        return NULL;
      }
      FbleRefAdd(arena, &pat->_base.ref, &pat->arg->ref);
      TypeRelease(arena, pat->arg);

      PolyKind* poly_kind = (PolyKind*)GetKind(arena_, pat->poly);
      if (poly_kind->_base.tag != POLY_KIND) {
        FbleReportError("cannot apply poly args to a basic kinded entity", &expr->loc);
        FreeKind(arena_, &poly_kind->_base);
        TypeRelease(arena, &pat->_base);
        return NULL;
      }

      bool error = (pat->arg == NULL);
      if (pat->arg != NULL) {
        Kind* expected_kind = poly_kind->arg;
        Kind* actual_kind = GetKind(arena_, pat->arg);
        if (!KindsEqual(expected_kind, actual_kind)) {
          FbleReportError("expected kind ", &pat->arg->loc);
          PrintKind(expected_kind);
          fprintf(stderr, ", but found ");
          PrintKind(actual_kind);
          fprintf(stderr, "\n");
          error = true;
        }
        FreeKind(arena_, actual_kind);
      }

      FreeKind(arena_, &poly_kind->_base);
      if (error) {
        TypeRelease(arena, &pat->_base);
        return NULL;
      }

      return &pat->_base;
    }

    case FBLE_STRUCT_EVAL_EXPR:
    case FBLE_STRUCT_IMPORT_EXPR: {
      FbleNamespaceExpr* namespace_expr = (FbleNamespaceExpr*)expr;

      Type* type = CompileExpr(arena, vars, namespace_expr->nspace, instrs);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      StructType* struct_type = (StructType*)Normal(type);
      if (struct_type->_base.tag != STRUCT_TYPE) {
        FbleReportError("expected value of type struct, but found value of type ", &namespace_expr->nspace->loc);
        PrintType(arena_, type, NULL);
        fprintf(stderr, "\n");

        TypeRelease(arena, type);
        return NULL;
      }

      {
        FbleNamespaceInstr* nspace = FbleAlloc(arena_, FbleNamespaceInstr);
        nspace->_base.tag = FBLE_NAMESPACE_INSTR;
        nspace->fieldc = struct_type->fields.size;
        FbleVectorAppend(arena_, *instrs, &nspace->_base);
      }

      Vars nvd[struct_type->fields.size];
      Vars* nvars = (expr->tag == FBLE_STRUCT_EVAL_EXPR) ? NULL : vars;
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        nvd[i].name = struct_type->fields.xs[i].name;
        nvd[i].type = struct_type->fields.xs[i].type;
        nvd[i].next = nvars;
        nvars = nvd + i;
      }

      Type* rtype = CompileExpr(arena, nvars, namespace_expr->body, instrs);
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

// CompileExprNoInstrs --
//   Type check and compile the given expression. Returns the type of the
//   expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the list of variables in scope.
//   expr - the expression to compile.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using
//   TypeRelease when it is no longer needed.
static Type* CompileExprNoInstrs(TypeArena* arena, Vars* vars, FbleExpr* expr)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleInstrV instrs;
  FbleVectorInit(arena_, instrs);
  Type* type = CompileExpr(arena, vars, expr, &instrs);
  Eval(arena, type, NULL, NULL);
  for (size_t i = 0; i < instrs.size; ++i) {
    FreeInstr(arena_, instrs.xs[i]);
  }
  FbleFree(arena_, instrs.xs);
  return type;
}

// CompileType --
//   Compile a type, returning its value.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the value of variables in scope.
//   type - the type to compile.
//
// Results:
//   The compiled and evaluated type, or NULL in case of error.
//
// Side effects:
//   Prints a message to stderr if the type fails to compile or evalute.
//   Allocates a reference-counted type that must be freed using
//   TypeRelease when it is no longer needed.
static Type* CompileType(TypeArena* arena, Vars* vars, FbleType* type)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (type->tag) {
    case FBLE_STRUCT_TYPE_EXPR: {
      FbleStructTypeExpr* struct_type = (FbleStructTypeExpr*)type;
      StructType* st = FbleAlloc(arena_, StructType);
      FbleRefInit(arena, &st->_base.ref);
      st->_base.loc = type->loc;
      st->_base.tag = STRUCT_TYPE;
      FbleVectorInit(arena_, st->fields);

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleField* field = struct_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, field->type);
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

    case FBLE_UNION_TYPE_EXPR: {
      UnionType* ut = FbleAlloc(arena_, UnionType);
      FbleRefInit(arena, &ut->_base.ref);
      ut->_base.loc = type->loc;
      ut->_base.tag = UNION_TYPE;
      FbleVectorInit(arena_, ut->fields);

      FbleUnionTypeExpr* union_type = (FbleUnionTypeExpr*)type;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleField* field = union_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, field->type);
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

    case FBLE_FUNC_TYPE_EXPR: {
      FuncType* ft = FbleAlloc(arena_, FuncType);
      FbleRefInit(arena, &ft->_base.ref);
      ft->_base.loc = type->loc;
      ft->_base.tag = FUNC_TYPE;
      ft->arg = NULL;
      ft->rtype = NULL;

      FbleFuncTypeExpr* func_type = (FbleFuncTypeExpr*)type;
      ft->arg = CompileType(arena, vars, func_type->arg);
      if (ft->arg == NULL) {
        TypeRelease(arena, &ft->_base);
        return NULL;
      }
      FbleRefAdd(arena, &ft->_base.ref, &ft->arg->ref);
      TypeRelease(arena, ft->arg);

      Type* rtype = CompileType(arena, vars, func_type->rtype);
      if (rtype == NULL) {
        TypeRelease(arena, &ft->_base);
        return NULL;
      }
      ft->rtype = rtype;
      FbleRefAdd(arena, &ft->_base.ref, &ft->rtype->ref);
      TypeRelease(arena, ft->rtype);
      return &ft->_base;
    }

    case FBLE_PROC_TYPE_EXPR:
    case FBLE_INPUT_TYPE_EXPR:
    case FBLE_OUTPUT_TYPE_EXPR: {
      UnaryType* ut = FbleAlloc(arena_, UnaryType);
      FbleRefInit(arena, &ut->_base.ref);
      ut->_base.loc = type->loc;

      // TODO: This logic assumes that PROC_TYPE, INPUT_TYPE, and OUTPUT_TYPE
      // are defined consecutively and in the same order as
      // FBLE_PROC_TYPE_EXPR, FBLE_INPUT_TYPE_EXPR, and FBLE_OUTPUT_TYPE_EXPR.
      // That seems like a bad idea.
      ut->_base.tag = PROC_TYPE + type->tag - FBLE_PROC_TYPE_EXPR;

      ut->type = NULL;

      FbleUnaryTypeExpr* unary_type = (FbleUnaryTypeExpr*)type;
      ut->type = CompileType(arena, vars, unary_type->type);
      if (ut->type == NULL) {
        TypeRelease(arena, &ut->_base);
        return NULL;
      }
      FbleRefAdd(arena, &ut->_base.ref, &ut->type->ref);
      TypeRelease(arena, ut->type);
      return &ut->_base;
    }

    case FBLE_TYPEOF_EXPR: {
      FbleTypeofExpr* typeof = (FbleTypeofExpr*)type;
      return CompileExprNoInstrs(arena, vars, typeof->expr);
    }

    case FBLE_MISC_APPLY_EXPR:
    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR:
    case FBLE_UNION_VALUE_EXPR:
    case FBLE_MISC_ACCESS_EXPR:
    case FBLE_COND_EXPR:
    case FBLE_FUNC_VALUE_EXPR:
    case FBLE_FUNC_APPLY_EXPR:
    case FBLE_EVAL_EXPR:
    case FBLE_LINK_EXPR:
    case FBLE_EXEC_EXPR:
    case FBLE_VAR_EXPR:
    case FBLE_LET_EXPR:
    case FBLE_POLY_EXPR:
    case FBLE_POLY_APPLY_EXPR:
    case FBLE_STRUCT_EVAL_EXPR:
    case FBLE_STRUCT_IMPORT_EXPR: {
      FbleExpr* expr = type;
      Type* type = CompileExprNoInstrs(arena, vars, expr);
      if (type == NULL) {
        return NULL;
      }

      Type* type_value = ValueOfType(arena, type);
      TypeRelease(arena, type);
      if (type_value == NULL) {
        FbleReportError("expected a type, but found value of type ", &expr->loc);
        PrintType(arena_, type, NULL);
        return NULL;
      }
      return type_value;
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
      k->arg = CompileKind(arena, poly->arg);
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
  Type* type = CompileExpr(type_arena, NULL, expr, &block->instrs);
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
