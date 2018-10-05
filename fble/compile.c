// compile.c --
//   This file implements the fble compilation routines.

#include <stdio.h>    // for fprintf, stderr

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

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
  TypeTag tag;
  FbleLoc loc;
  int strong_ref_count;
  int weak_ref_count;
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
  FieldV args;
  struct Type* rtype;
} FuncType;

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
typedef struct {
  Type _base;
  Type* poly;
  TypeV args;
} PolyApplyType;

// RefType --
//   REF_TYPE
//
// A implementation-specific type introduced to support recursive types. A
// ref type is simply a reference to another type.
typedef struct {
  Type _base;
  Kind* kind;
  Type* value;
} RefType;

// Vars --
//   Scope of variables visible during type checking.
typedef struct Vars {
  FbleName name;
  Type* type;
  struct Vars* next;
} Vars;

static Kind* CopyKind(FbleArena* arena, Kind* kind);
static void FreeKind(FbleArena* arena, Kind* kind);
static Type* TypeTakeStrongRef(FbleArena* arena, Type* type);
static void TypeDropStrongRef(FbleArena* arena, Type* type);
static Type* TypeTakeWeakRef(FbleArena* arena, Type* type);
static void TypeDropWeakRef(FbleArena* arena, Type* type);
static void FreeType(FbleArena* arena, Type* type);
static Kind* GetKind(FbleArena* arena, Type* type);
static Type* Subst(FbleArena* arena, Type* src, TypeV params, TypeV args);
static Type* Normal(FbleArena* arena, Type* type);
static bool TypesEqual(FbleArena* arena, Type* a, Type* b);
static bool KindsEqual(Kind* a, Kind* b);
static void PrintType(Type* type);
static void PrintKind(Kind* type);

static Type* Compile(FbleArena* arena, Vars* vars, Vars* type_vars, FbleExpr* expr, FbleInstr** instrs);
static Type* CompileType(FbleArena* arena, Vars* vars, FbleType* type);
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

// TypeTakeStrongRef --
//   Takes a strong reference to a compiled type.
//
// Inputs:
//   arena - for allocations.
//   type - the type to take the reference for.
//
// Results:
//   The type with incremented strong reference count.
//
// Side effects:
//   The returned type must be freed using TypeDropStrongRef when no longer in use.
static Type* TypeTakeStrongRef(FbleArena* arena, Type* type)
{
  if (type != NULL) {
    if (type->strong_ref_count++ == 0) {
      switch (type->tag) {
        case STRUCT_TYPE: {
          StructType* st = (StructType*)type;
          for (size_t i = 0; i < st->fields.size; ++i) {
            TypeTakeStrongRef(arena, st->fields.xs[i].type);
          }
          break;
        }

        case UNION_TYPE: {
          UnionType* ut = (UnionType*)type;
          for (size_t i = 0; i < ut->fields.size; ++i) {
            TypeTakeStrongRef(arena, ut->fields.xs[i].type);
          }
          break;
        }

        case FUNC_TYPE: {
          FuncType* ft = (FuncType*)type;
          for (size_t i = 0; i < ft->args.size; ++i) {
            TypeTakeStrongRef(arena, ft->args.xs[i].type);
          }
          TypeTakeStrongRef(arena, ft->rtype);
          break;
        }

        case VAR_TYPE: {
          VarType* var = (VarType*)type;
          TypeTakeStrongRef(arena, var->value);
          break;
        }

        case ABSTRACT_TYPE: {
          break;
        }

        case POLY_TYPE: {
          PolyType* pt = (PolyType*)type;
          for (size_t i = 0; i < pt->args.size; ++i) {
            TypeTakeStrongRef(arena, pt->args.xs[i]);
          }
          TypeTakeStrongRef(arena, pt->body);
          break;
        }

        case POLY_APPLY_TYPE: {
          PolyApplyType* pat = (PolyApplyType*)type;
          TypeTakeStrongRef(arena, pat->poly);
          for (size_t i = 0; i < pat->args.size; ++i) {
            TypeTakeStrongRef(arena, pat->args.xs[i]);
          }
          break;
        }

        case REF_TYPE: {
          RefType* ref = (RefType*)type;
          TypeTakeWeakRef(arena, ref->value);
          break;
        }
      }
    }
  }

  return type;
}

// TypeDropStrongRef --
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
static void TypeDropStrongRef(FbleArena* arena, Type* type)
{
  if (type == NULL) {
    return;
  }

  assert(type->strong_ref_count > 0);
  if (type->strong_ref_count == 1) {
    switch (type->tag) {
      case STRUCT_TYPE: {
        StructType* st = (StructType*)type;
        for (size_t i = 0; i < st->fields.size; ++i) {
          TypeDropStrongRef(arena, st->fields.xs[i].type);
        }
        break;
      }

      case UNION_TYPE: {
        UnionType* ut = (UnionType*)type;
        for (size_t i = 0; i < ut->fields.size; ++i) {
          TypeDropStrongRef(arena, ut->fields.xs[i].type);
        }
        break;
      }

      case FUNC_TYPE: {
        FuncType* ft = (FuncType*)type;
        for (size_t i = 0; i < ft->args.size; ++i) {
          TypeDropStrongRef(arena, ft->args.xs[i].type);
        }
        TypeDropStrongRef(arena, ft->rtype);
        break;
      }

      case VAR_TYPE: {
        VarType* var = (VarType*)type;
        TypeDropStrongRef(arena, var->value);
        break;
      }

      case ABSTRACT_TYPE: {
        break;
      }

      case POLY_TYPE: {
        PolyType* pt = (PolyType*)type;
        for (size_t i = 0; i < pt->args.size; ++i) {
          TypeDropStrongRef(arena, pt->args.xs[i]);
        }
        TypeDropStrongRef(arena, pt->body);
        break;
      }

      case POLY_APPLY_TYPE: {
        PolyApplyType* pat = (PolyApplyType*)type;
        TypeDropStrongRef(arena, pat->poly);
        for (size_t i = 0; i < pat->args.size; ++i) {
          TypeDropStrongRef(arena, pat->args.xs[i]);
        }
        break;
      }

      case REF_TYPE: {
        RefType* ref = (RefType*)type;
        TypeDropWeakRef(arena, ref->value);
        break;
      }
    }
  }

  type->strong_ref_count--;
  if (type->strong_ref_count == 0 && type->weak_ref_count == 0) {
    FreeType(arena, type);
  }
}

// FreeType --
//   Free a compiled type that has no remaining references to it.
//
// Inputs:
//   arena - the arena to use for deallocations
//   type - the type to free
//
// Returns:
//   None.
//
// Side effects:
//   Frees memory associated with the type, and removes any references that
//   type has to other typesremaining references to it.
//
// Inputs:
//   arena - the arena to use for deallocations
//   type - the type to free
//
// Returns:
//   None.
//
// Side effects:
//   Frees memory associated with the type, and removes any references that
//   type has to other types. The type should not be accessed again after this
//   call.
static void FreeType(FbleArena* arena, Type* type)
{
  assert(type->strong_ref_count == 0 && type->weak_ref_count == 0);
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      FbleFree(arena, st->fields.xs);
      FbleFree(arena, st);
      break;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      FbleFree(arena, ut->fields.xs);
      FbleFree(arena, ut);
      break;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      FbleFree(arena, ft->args.xs);
      FbleFree(arena, ft);
      break;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      FbleFree(arena, var);
      break;
    }

    case ABSTRACT_TYPE: {
      AbstractType* abstract = (AbstractType*)type;
      FreeKind(arena, abstract->kind);
      FbleFree(arena, type);
      break;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      FbleFree(arena, pt->args.xs);
      FbleFree(arena, pt);
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      FbleFree(arena, pat->args.xs);
      FbleFree(arena, pat);
      break;
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;
      FreeKind(arena, ref->kind);
      FbleFree(arena, ref);
      break;
    }
  }
}

// TypeTakeWeakRef --
//   Takes a weak reference to a compiled type.
//
// Inputs:
//   arena - for allocations.
//   type - the type to take the reference for.
//
// Results:
//   The type with incremented weak reference count.
//
// Side effects:
//   The returned type must be freed using TypeDropWeakRef when no longer in use.
static Type* TypeTakeWeakRef(FbleArena* arena, Type* type)
{
  if (type != NULL) {
    if (type->weak_ref_count++ == 0) {
      switch (type->tag) {
        case STRUCT_TYPE: {
          StructType* st = (StructType*)type;
          for (size_t i = 0; i < st->fields.size; ++i) {
            TypeTakeWeakRef(arena, st->fields.xs[i].type);
          }
          break;
        }

        case UNION_TYPE: {
          UnionType* ut = (UnionType*)type;
          for (size_t i = 0; i < ut->fields.size; ++i) {
            TypeTakeWeakRef(arena, ut->fields.xs[i].type);
          }
          break;
        }

        case FUNC_TYPE: {
          FuncType* ft = (FuncType*)type;
          for (size_t i = 0; i < ft->args.size; ++i) {
            TypeTakeWeakRef(arena, ft->args.xs[i].type);
          }
          TypeTakeWeakRef(arena, ft->rtype);
          break;
        }

        case VAR_TYPE: {
          VarType* var = (VarType*)type;
          TypeTakeWeakRef(arena, var->value);
          break;
        }

        case ABSTRACT_TYPE: {
          break;
        }

        case POLY_TYPE: {
          PolyType* pt = (PolyType*)type;
          for (size_t i = 0; i < pt->args.size; ++i) {
            TypeTakeWeakRef(arena, pt->args.xs[i]);
          }
          TypeTakeWeakRef(arena, pt->body);
          break;
        }

        case POLY_APPLY_TYPE: {
          PolyApplyType* pat = (PolyApplyType*)type;
          TypeTakeWeakRef(arena, pat->poly);
          for (size_t i = 0; i < pat->args.size; ++i) {
            TypeTakeWeakRef(arena, pat->args.xs[i]);
          }
          break;
        }

        case REF_TYPE: {
          break;
        }
      }
    }
  }

  return type;
}

// TypeDropWeakRef --
//   Drop a weak reference to a compiled type.
//
// Inputs:
//   arena - for deallocations.
//   type - the type to drop the refcount for. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Decrements the weak refcount for the type and frees it if there are no
//   more references to it.
static void TypeDropWeakRef(FbleArena* arena, Type* type)
{
  if (type == NULL) {
    return;
  }

  assert(type->weak_ref_count > 0);
  if (type->weak_ref_count == 1) {
    switch (type->tag) {
      case STRUCT_TYPE: {
        StructType* st = (StructType*)type;
        for (size_t i = 0; i < st->fields.size; ++i) {
          TypeDropWeakRef(arena, st->fields.xs[i].type);
        }
        break;
      }

      case UNION_TYPE: {
        UnionType* ut = (UnionType*)type;
        for (size_t i = 0; i < ut->fields.size; ++i) {
          TypeDropWeakRef(arena, ut->fields.xs[i].type);
        }
        break;
      }

      case FUNC_TYPE: {
        FuncType* ft = (FuncType*)type;
        for (size_t i = 0; i < ft->args.size; ++i) {
          TypeDropWeakRef(arena, ft->args.xs[i].type);
        }
        TypeDropWeakRef(arena, ft->rtype);
        break;
      }

      case VAR_TYPE: {
        VarType* var = (VarType*)type;
        TypeDropWeakRef(arena, var->value);
        break;
      }

      case ABSTRACT_TYPE: {
        break;
      }

      case POLY_TYPE: {
        PolyType* pt = (PolyType*)type;
        for (size_t i = 0; i < pt->args.size; ++i) {
          TypeDropWeakRef(arena, pt->args.xs[i]);
        }
        TypeDropWeakRef(arena, pt->body);
        break;
      }

      case POLY_APPLY_TYPE: {
        PolyApplyType* pat = (PolyApplyType*)type;
        TypeDropWeakRef(arena, pat->poly);
        for (size_t i = 0; i < pat->args.size; ++i) {
          TypeDropWeakRef(arena, pat->args.xs[i]);
        }
        break;
      }

      case REF_TYPE: {
        break;
      }
    }
  }

  type->weak_ref_count--;
  if (type->strong_ref_count == 0 && type->weak_ref_count == 0) {
    FreeType(arena, type);
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
    case FUNC_TYPE: {
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

// Subst --
//   Substitute the given arguments in place of the given parameters in the
//   given type.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the type to substitute into.
//   params - the abstract types to substitute out.
//   args - the concrete types to substitute in.
//
// Results:
//   A type with all occurrences of params replaced with the corresponding
//   args types.
//
// Side effects:
//   The caller is responsible for calling TypeDropStrongRef on the returned
//   type when it is no longer needed.
static Type* Subst(FbleArena* arena, Type* type, TypeV params, TypeV args)
{
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      StructType* sst = FbleAlloc(arena, StructType);
      sst->_base.tag = STRUCT_TYPE;
      sst->_base.loc = st->_base.loc;
      sst->_base.strong_ref_count = 1;
      sst->_base.weak_ref_count = 0;
      FbleVectorInit(arena, sst->fields);
      for (size_t i = 0; i < st->fields.size; ++i) {
        Field* field = FbleVectorExtend(arena, sst->fields);
        field->name = st->fields.xs[i].name;
        field->type = Subst(arena, st->fields.xs[i].type, params, args);
      }
      return &sst->_base;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      UnionType* sut = FbleAlloc(arena, UnionType);
      sut->_base.tag = UNION_TYPE;
      sut->_base.loc = ut->_base.loc;
      sut->_base.strong_ref_count = 1;
      sut->_base.weak_ref_count = 0;
      FbleVectorInit(arena, sut->fields);
      for (size_t i = 0; i < ut->fields.size; ++i) {
        Field* field = FbleVectorExtend(arena, sut->fields);
        field->name = ut->fields.xs[i].name;
        field->type = Subst(arena, ut->fields.xs[i].type, params, args);
      }
      return &sut->_base;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      FuncType* sft = FbleAlloc(arena, FuncType);
      sft->_base.tag = FUNC_TYPE;
      sft->_base.loc = ft->_base.loc;
      sft->_base.strong_ref_count = 1;
      sft->_base.weak_ref_count = 0;
      FbleVectorInit(arena, sft->args);
      for (size_t i = 0; i < ft->args.size; ++i) {
        Field* arg = FbleVectorExtend(arena, sft->args);
        arg->name = ft->args.xs[i].name;
        arg->type = Subst(arena, ft->args.xs[i].type, params, args);
      }
      sft->rtype = Subst(arena, ft->rtype, params, args);
      return &sft->_base;
    }

    case VAR_TYPE: {
      VarType* vt = (VarType*)type;
      VarType* svt = FbleAlloc(arena, VarType);
      svt->_base.tag = VAR_TYPE;
      svt->_base.loc = vt->_base.loc;
      svt->_base.strong_ref_count = 1;
      svt->_base.weak_ref_count = 0;
      svt->var = vt->var;
      svt->value = Subst(arena, vt->value, params, args);
      return &svt->_base;
    }

    case ABSTRACT_TYPE: {
      for (size_t i = 0; i < params.size; ++i) {
        if (type == params.xs[i]) {
          return TypeTakeStrongRef(arena, args.xs[i]);
        }
      }
      return TypeTakeStrongRef(arena, type);
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      PolyType* spt = FbleAlloc(arena, PolyType);
      spt->_base.tag = POLY_TYPE;
      spt->_base.loc = pt->_base.loc;
      spt->_base.strong_ref_count = 1;
      spt->_base.weak_ref_count = 0;
      FbleVectorInit(arena, spt->args);
      for (size_t i = 0; i < pt->args.size; ++i) {
        FbleVectorAppend(arena, spt->args, TypeTakeStrongRef(arena, pt->args.xs[i]));
      }
      spt->body = Subst(arena, pt->body, params, args);
      return &spt->_base;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PolyApplyType* spat = FbleAlloc(arena, PolyApplyType);
      spat->_base.tag = POLY_APPLY_TYPE;
      spat->_base.loc = pat->_base.loc;
      spat->_base.strong_ref_count = 1;
      spat->_base.weak_ref_count = 0;
      spat->poly = Subst(arena, pat->poly, params, args);
      FbleVectorInit(arena, spat->args);
      for (size_t i = 0; i < pat->args.size; ++i) {
        Type* arg = Subst(arena, pat->args.xs[i], params, args);
        FbleVectorAppend(arena, spat->args, arg);
      }
      return &spat->_base;
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;

      // TODO: Avoid infinite recursion by caching the result of substitution
      // on this reference.
      if (ref->value == NULL) {
        return TypeTakeStrongRef(arena, type);
      }
      return Subst(arena, ref->value, params, args);
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// Normal --
//   Reduce a Type to normal form. Normal form types are struct, union, and
//   func types, but not var types, for example.
//
// Inputs:
//   arena - arena to use for allocations, if any.
//   type - the type to reduce.
//
// Results:
//   The type reduced to normal form.
//
// Side effects:
//   The caller is responsible for calling TypeDropStrongRef on the returned
//   type when it is no longer needed.
static Type* Normal(FbleArena* arena, Type* type)
{
  switch (type->tag) {
    case STRUCT_TYPE: return TypeTakeStrongRef(arena, type);
    case UNION_TYPE: return TypeTakeStrongRef(arena, type);
    case FUNC_TYPE: return TypeTakeStrongRef(arena, type);

    case VAR_TYPE: {
      VarType* var_type = (VarType*)type;
      return Normal(arena, var_type->value);
    }

    case ABSTRACT_TYPE: return TypeTakeStrongRef(arena, type);
    case POLY_TYPE: return TypeTakeStrongRef(arena, type);

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PolyType* poly = (PolyType*)Normal(arena, pat->poly);
      assert(poly->_base.tag == POLY_TYPE);
      Type* subst = Subst(arena, poly->body, poly->args, pat->args);
      TypeDropStrongRef(arena, &poly->_base);
      return subst;
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;
      if (ref->value == NULL) {
        return TypeTakeStrongRef(arena, type);
      }
      return Normal(arena, ref->value);
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// TypesEqual --
//   Test whether the two given compiled types are equal.
//
// Inputs:
//   a - the first type
//   b - the second type
//
// Results:
//   True if the first type equals the second type, false otherwise.
//
// Side effects:
//   None.
static bool TypesEqual(FbleArena* arena, Type* a, Type* b)
{
  if (a == b) {
    return true;
  }

  a = Normal(arena, a);
  b = Normal(arena, b);
  if (a == b) {
    TypeDropStrongRef(arena, a);
    TypeDropStrongRef(arena, b);
    return true;
  }

  if (a->tag != b->tag) {
    TypeDropStrongRef(arena, a);
    TypeDropStrongRef(arena, b);
    return false;
  }

  switch (a->tag) {
    case STRUCT_TYPE: {
      StructType* sta = (StructType*)a;
      StructType* stb = (StructType*)b;
      if (sta->fields.size != stb->fields.size) {
        TypeDropStrongRef(arena, a);
        TypeDropStrongRef(arena, b);
        return false;
      }

      for (size_t i = 0; i < sta->fields.size; ++i) {
        if (!FbleNamesEqual(sta->fields.xs[i].name.name, stb->fields.xs[i].name.name)) {
          TypeDropStrongRef(arena, a);
          TypeDropStrongRef(arena, b);
          return false;
        }

        if (!TypesEqual(arena, sta->fields.xs[i].type, stb->fields.xs[i].type)) {
          TypeDropStrongRef(arena, a);
          TypeDropStrongRef(arena, b);
          return false;
        }
      }

      TypeDropStrongRef(arena, a);
      TypeDropStrongRef(arena, b);
      return true;
    }

    case UNION_TYPE: {
      UnionType* uta = (UnionType*)a;
      UnionType* utb = (UnionType*)b;
      if (uta->fields.size != utb->fields.size) {
        TypeDropStrongRef(arena, a);
        TypeDropStrongRef(arena, b);
        return false;
      }

      for (size_t i = 0; i < uta->fields.size; ++i) {
        if (!FbleNamesEqual(uta->fields.xs[i].name.name, utb->fields.xs[i].name.name)) {
          TypeDropStrongRef(arena, a);
          TypeDropStrongRef(arena, b);
          return false;
        }

        if (!TypesEqual(arena, uta->fields.xs[i].type, utb->fields.xs[i].type)) {
          TypeDropStrongRef(arena, a);
          TypeDropStrongRef(arena, b);
          return false;
        }
      }

      TypeDropStrongRef(arena, a);
      TypeDropStrongRef(arena, b);
      return true;
    }

    case FUNC_TYPE: {
      FuncType* fta = (FuncType*)a;
      FuncType* ftb = (FuncType*)b;
      if (fta->args.size != ftb->args.size) {
        TypeDropStrongRef(arena, a);
        TypeDropStrongRef(arena, b);
        return false;
      }

      for (size_t i = 0; i < fta->args.size; ++i) {
        if (!TypesEqual(arena, fta->args.xs[i].type, ftb->args.xs[i].type)) {
          TypeDropStrongRef(arena, a);
          TypeDropStrongRef(arena, b);
          return false;
        }
      }

      bool rtype_equal = TypesEqual(arena, fta->rtype, ftb->rtype);
      TypeDropStrongRef(arena, a);
      TypeDropStrongRef(arena, b);
      return rtype_equal;
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
        TypeDropStrongRef(arena, a);
        TypeDropStrongRef(arena, b);
        return false;
      }

      // TODO: Verify all the args have matching kinds.

      // Substitute in the arguments of pta for the paramaters of b so we can
      // compare the bodies of the types with the same set of abstract types.
      Type* t = Subst(arena, ptb->body,  ptb->args, pta->args);
      bool eq = TypesEqual(arena, pta->body, t);
      TypeDropStrongRef(arena, a);
      TypeDropStrongRef(arena, b);
      TypeDropStrongRef(arena, t);
      return eq;
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
//   type - the type to print.
//
// Result:
//   None.
//
// Side effect:
//   Prints the given type in human readable form to stderr.
static void PrintType(Type* type)
{
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      fprintf(stderr, "*(");
      const char* comma = "";
      for (size_t i = 0; i < st->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(st->fields.xs[i].type);
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
        PrintType(ut->fields.xs[i].type);
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
        PrintType(ft->args.xs[i].type);
        fprintf(stderr, " %s", ft->args.xs[i].name.name);
        comma = ", ";
      }
      fprintf(stderr, "; ");
      PrintType(ft->rtype);
      fprintf(stderr, ")");
      break;
    }

    case VAR_TYPE: {
      VarType* var_type = (VarType*)type;
      fprintf(stderr, "%s", var_type->var.name);
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
      PrintType(pt->body);
      fprintf(stderr, "; }");
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PrintType(pat->poly);
      fprintf(stderr, "<");
      const char* comma = "";
      for (size_t i = 0; i < pat->args.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(pat->args.xs[i]);
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
        PrintType(ref->value);
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

// Compile --
//   Type check and compile the given expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the list of variables in scope.
//   type_vars - the value of type variables in scope.
//   expr - the expression to compile.
//   instrs - output pointer to store generated in structions
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Sets instrs to point to the compiled instructions if the expression is
//   well typed. Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using
//   TypeDropStrongRef when it is no longer needed.
static Type* Compile(FbleArena* arena, Vars* vars, Vars* type_vars, FbleExpr* expr, FbleInstr** instrs)
{
  switch (expr->tag) {
    case FBLE_STRUCT_VALUE_EXPR: {
      FbleStructValueExpr* struct_value_expr = (FbleStructValueExpr*)expr;
      Type* type = CompileType(arena, type_vars, struct_value_expr->type);
      if (type == NULL) {
        return NULL;
      }

      StructType* struct_type = (StructType*)Normal(arena, type);
      if (struct_type->_base.tag != STRUCT_TYPE) {
        FbleReportError("expected a struct type, but found ", &struct_value_expr->type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, &struct_type->_base);
        return NULL;
      }

      if (struct_type->fields.size != struct_value_expr->args.size) {
        // TODO: Where should the error message go?
        FbleReportError("expected %i args, but %i were provided\n",
            &expr->loc, struct_type->fields.size, struct_value_expr->args.size);
        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, &struct_type->_base);
        return NULL;
      }

      bool error = false;
      FbleStructValueInstr* instr = FbleAlloc(arena, FbleStructValueInstr);
      instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      FbleVectorInit(arena, instr->fields);
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        Field* field = struct_type->fields.xs + i;

        FbleInstr* mkarg = NULL;
        Type* arg_type = Compile(arena, vars, type_vars, struct_value_expr->args.xs[i], &mkarg);
        error = error || (arg_type == NULL);

        if (arg_type != NULL && !TypesEqual(arena, field->type, arg_type)) {
          FbleReportError("expected type ", &struct_value_expr->args.xs[i]->loc);
          PrintType(field->type);
          fprintf(stderr, ", but found ");
          PrintType(arg_type);
          fprintf(stderr, "\n");
          error = true;
        }
        TypeDropStrongRef(arena, arg_type);

        FbleVectorAppend(arena, instr->fields, mkarg);
      }

      TypeDropStrongRef(arena, &struct_type->_base);

      if (error) {
        TypeDropStrongRef(arena, type);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      *instrs = &instr->_base;
      return type;
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      Type* type = CompileType(arena, type_vars, union_value_expr->type);
      if (type == NULL) {
        return NULL;
      }

      UnionType* union_type = (UnionType*)Normal(arena, type);
      if (union_type->_base.tag != UNION_TYPE) {
        FbleReportError("expected a union type, but found ", &union_value_expr->type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, &union_type->_base);
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
        PrintType(type);
        fprintf(stderr, "\n");
        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, &union_type->_base);
        return NULL;
      }

      FbleInstr* mkarg = NULL;
      Type* arg_type = Compile(arena, vars, type_vars, union_value_expr->arg, &mkarg);
      if (arg_type == NULL) {
        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, &union_type->_base);
        return NULL;
      }

      if (!TypesEqual(arena, field_type, arg_type)) {
        FbleReportError("expected type ", &union_value_expr->arg->loc);
        PrintType(field_type);
        fprintf(stderr, ", but found type ");
        PrintType(arg_type);
        fprintf(stderr, "\n");
        FbleFreeInstrs(arena, mkarg);
        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, &union_type->_base);
        TypeDropStrongRef(arena, arg_type);
        return NULL;
      }
      TypeDropStrongRef(arena, arg_type);
      TypeDropStrongRef(arena, &union_type->_base);

      FbleUnionValueInstr* instr = FbleAlloc(arena, FbleUnionValueInstr);
      instr->_base.tag = FBLE_UNION_VALUE_INSTR;
      instr->tag = tag;
      instr->mkarg = mkarg;
      *instrs = &instr->_base;
      return type;
    }

    case FBLE_ACCESS_EXPR: {
      FbleAccessExpr* access_expr = (FbleAccessExpr*)expr;

      // Allocate a slot on the variable stack for the intermediate value
      Vars nvars = {
        .name = { .name = "", .loc = expr->loc },
        .type = NULL,
        .next = vars,
      };

      FblePushInstr* instr = FbleAlloc(arena, FblePushInstr);
      instr->_base.tag = FBLE_PUSH_INSTR;
      FbleVectorInit(arena, instr->values);
      FbleInstr** mkobj = FbleVectorExtend(arena, instr->values);
      *mkobj = NULL;

      FbleAccessInstr* access = FbleAlloc(arena, FbleAccessInstr);
      access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      instr->next = &access->_base;
      access->loc = access_expr->field.loc;
      Type* type = Compile(arena, &nvars, type_vars, access_expr->object, mkobj);
      if (type == NULL) {
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      // Note: We can safely pretend the type is always a struct, because
      // StructType and UnionType have the same structure.
      StructType* data_type = (StructType*)Normal(arena, type);
      if (data_type->_base.tag == STRUCT_TYPE) {
        access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      } else if (data_type->_base.tag == UNION_TYPE) {
        access->_base.tag = FBLE_UNION_ACCESS_INSTR;
      } else {
        FbleReportError("expected value of type struct or union, but found value of type ", &access_expr->object->loc);
        PrintType(type);
        fprintf(stderr, "\n");

        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, &data_type->_base);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      for (size_t i = 0; i < data_type->fields.size; ++i) {
        if (FbleNamesEqual(access_expr->field.name, data_type->fields.xs[i].name.name)) {
          access->tag = i;
          *instrs = &instr->_base;
          Type* rtype = TypeTakeStrongRef(arena, data_type->fields.xs[i].type);
          TypeDropStrongRef(arena, type);
          TypeDropStrongRef(arena, &data_type->_base);
          return rtype;
        }
      }

      FbleReportError("%s is not a field of type ", &access_expr->field.loc, access_expr->field.name);
      PrintType(type);
      fprintf(stderr, "\n");
      TypeDropStrongRef(arena, type);
      TypeDropStrongRef(arena, &data_type->_base);
      FbleFreeInstrs(arena, &instr->_base);
      return NULL;
    }

    case FBLE_COND_EXPR: {
      FbleCondExpr* cond_expr = (FbleCondExpr*)expr;

      // Allocate a slot on the variable stack for the intermediate value
      Vars nvars = {
        .name = { .name = "", .loc = expr->loc },
        .type = NULL,
        .next = vars,
      };

      FblePushInstr* push = FbleAlloc(arena, FblePushInstr);
      push->_base.tag = FBLE_PUSH_INSTR;
      FbleVectorInit(arena, push->values);
      push->next = NULL;

      FbleInstr** mkobj = FbleVectorExtend(arena, push->values);
      *mkobj = NULL;
      Type* type = Compile(arena, &nvars, type_vars, cond_expr->condition, mkobj);
      if (type == NULL) {
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      UnionType* union_type = (UnionType*)Normal(arena, type);
      if (union_type->_base.tag != UNION_TYPE) {
        FbleReportError("expected value of union type, but found value of type ", &cond_expr->condition->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        FbleFreeInstrs(arena, &push->_base);
        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, &union_type->_base);
        return NULL;
      }

      if (union_type->fields.size != cond_expr->choices.size) {
        FbleReportError("expected %d arguments, but %d were provided.\n", &cond_expr->_base.loc, union_type->fields.size, cond_expr->choices.size);
        FbleFreeInstrs(arena, &push->_base);
        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, &union_type->_base);
        return NULL;
      }

      FbleCondInstr* cond_instr = FbleAlloc(arena, FbleCondInstr);
      cond_instr->_base.tag = FBLE_COND_INSTR;
      push->next = &cond_instr->_base;
      FbleVectorInit(arena, cond_instr->choices);

      Type* return_type = NULL;
      for (size_t i = 0; i < cond_expr->choices.size; ++i) {
        if (!FbleNamesEqual(cond_expr->choices.xs[i].name.name, union_type->fields.xs[i].name.name)) {
          FbleReportError("expected tag '%s', but found '%s'.\n",
              &cond_expr->choices.xs[i].name.loc,
              union_type->fields.xs[i].name.name,
              cond_expr->choices.xs[i].name.name);
          FbleFreeInstrs(arena, &push->_base);
          TypeDropStrongRef(arena, return_type);
          TypeDropStrongRef(arena, type);
          TypeDropStrongRef(arena, &union_type->_base);
          return NULL;
        }

        FbleInstr* mkarg = NULL;
        Type* arg_type = Compile(arena, vars, type_vars, cond_expr->choices.xs[i].expr, &mkarg);
        if (arg_type == NULL) {
          TypeDropStrongRef(arena, return_type);
          TypeDropStrongRef(arena, type);
          TypeDropStrongRef(arena, &union_type->_base);
          FbleFreeInstrs(arena, &push->_base);
          return NULL;
        }
        FbleVectorAppend(arena, cond_instr->choices, mkarg);

        if (return_type == NULL) {
          return_type = arg_type;
        } else {
          if (!TypesEqual(arena, return_type, arg_type)) {
            FbleReportError("expected type ", &cond_expr->choices.xs[i].expr->loc);
            PrintType(return_type);
            fprintf(stderr, ", but found ");
            PrintType(arg_type);
            fprintf(stderr, "\n");

            TypeDropStrongRef(arena, type);
            TypeDropStrongRef(arena, &union_type->_base);
            TypeDropStrongRef(arena, return_type);
            TypeDropStrongRef(arena, arg_type);
            FbleFreeInstrs(arena, &push->_base);
            return NULL;
          }
          TypeDropStrongRef(arena, arg_type);
        }
      }
      TypeDropStrongRef(arena, type);
      TypeDropStrongRef(arena, &union_type->_base);

      *instrs = &push->_base;
      return return_type;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      FuncType* type = FbleAlloc(arena, FuncType);
      type->_base.tag = FUNC_TYPE;
      type->_base.loc = expr->loc;
      type->_base.strong_ref_count = 1;
      type->_base.weak_ref_count = 0;
      FbleVectorInit(arena, type->args);
      type->rtype = NULL;
      
      Vars nvds[func_value_expr->args.size];
      Vars* nvars = vars;
      bool error = false;
      for (size_t i = 0; i < func_value_expr->args.size; ++i) {
        Field* field = FbleVectorExtend(arena, type->args);
        field->name = func_value_expr->args.xs[i].name;
        field->type = CompileType(arena, type_vars, func_value_expr->args.xs[i].type);
        error = error || (field->type == NULL);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, func_value_expr->args.xs[j].name.name)) {
            error = true;
            FbleReportError("duplicate arg name '%s'\n", &field->name.loc, field->name.name);
            break;
          }
        }

        nvds[i].name = field->name;
        nvds[i].type = field->type;
        nvds[i].next = nvars;
        nvars = nvds + i;
      }

      FbleFuncValueInstr* instr = FbleAlloc(arena, FbleFuncValueInstr);
      instr->_base.tag = FBLE_FUNC_VALUE_INSTR;
      instr->argc = func_value_expr->args.size;
      instr->body = NULL;
      if (!error) {
        type->rtype = Compile(arena, nvars, type_vars, func_value_expr->body, &instr->body);
        error = error || (type->rtype == NULL);
      }

      if (error) {
        TypeDropStrongRef(arena, &type->_base);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      *instrs = &instr->_base;
      return &type->_base;
    }

    case FBLE_APPLY_EXPR: {
      FbleApplyExpr* apply_expr = (FbleApplyExpr*)expr;

      // Allocate space on the stack for the intermediate values.
      Vars nvd[1 + apply_expr->args.size];
      Vars* nvars = vars;
      for (size_t i = 0; i < 1 + apply_expr->args.size; ++i) {
        nvd[i].type = NULL;
        nvd[i].name.name = "";
        nvd[i].name.loc = expr->loc;
        nvd[i].next = nvars;
        nvars = nvd + i;
      };

      FblePushInstr* push = FbleAlloc(arena, FblePushInstr);
      push->_base.tag = FBLE_PUSH_INSTR;
      FbleVectorInit(arena, push->values);
      push->next = NULL;

      FbleInstr** mkfunc = FbleVectorExtend(arena, push->values);
      *mkfunc = NULL;
      Type* type = Compile(arena, nvars, type_vars, apply_expr->func, mkfunc);
      if (type == NULL) {
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      FuncType* func_type = (FuncType*)Normal(arena, type);
      if (func_type->_base.tag != FUNC_TYPE) {
        FbleReportError("cannot perform application on an object of type ", &expr->loc);
        PrintType(type);
        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, &func_type->_base);
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      bool error = false;
      if (func_type->args.size != apply_expr->args.size) {
        FbleReportError("expected %i args, but %i provided\n",
            &expr->loc, func_type->args.size, apply_expr->args.size);
        error = true;
      }

      for (size_t i = 0; i < func_type->args.size && i < apply_expr->args.size; ++i) {
        FbleInstr** mkarg = FbleVectorExtend(arena, push->values);
        *mkarg = NULL;
        Type* arg_type = Compile(arena, nvars, type_vars, apply_expr->args.xs[i], mkarg);
        error = error || (arg_type == NULL);
        if (arg_type != NULL && !TypesEqual(arena, func_type->args.xs[i].type, arg_type)) {
          FbleReportError("expected type ", &apply_expr->args.xs[i]->loc);
          PrintType(func_type->args.xs[i].type);
          fprintf(stderr, ", but found ");
          PrintType(arg_type);
          fprintf(stderr, "\n");
          error = true;
        }
        TypeDropStrongRef(arena, arg_type);
      }

      if (error) {
        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, &func_type->_base);
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      FbleFuncApplyInstr* apply_instr = FbleAlloc(arena, FbleFuncApplyInstr);
      apply_instr->_base.tag = FBLE_FUNC_APPLY_INSTR;
      apply_instr->argc = func_type->args.size;
      push->next = &apply_instr->_base;
      *instrs = &push->_base;
      Type* rtype = TypeTakeStrongRef(arena, func_type->rtype);
      TypeDropStrongRef(arena, type);
      TypeDropStrongRef(arena, &func_type->_base);
      return rtype;
    }

    case FBLE_EVAL_EXPR: assert(false && "TODO: FBLE_EVAL_EXPR"); return NULL;
    case FBLE_LINK_EXPR: assert(false && "TODO: FBLE_LINK_EXPR"); return NULL;
    case FBLE_EXEC_EXPR: assert(false && "TODO: FBLE_EXEC_EXPR"); return NULL;

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

      FbleVarInstr* instr = FbleAlloc(arena, FbleVarInstr);
      instr->_base.tag = FBLE_VAR_INSTR;
      instr->position = position;
      *instrs = &instr->_base;
      return TypeTakeStrongRef(arena, vars->type);
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;
      bool error = false;

      // Evaluate the types of the bindings and set up the new vars.
      Vars nvd[let_expr->bindings.size];
      Vars* nvars = vars;
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        nvd[i].name = let_expr->bindings.xs[i].name;
        nvd[i].type = CompileType(arena, type_vars, let_expr->bindings.xs[i].type);
        error = error || (nvd[i].type == NULL);
        nvd[i].next = nvars;
        nvars = nvd + i;
      }

      // Compile the values of the variables.
      FbleLetInstr* instr = FbleAlloc(arena, FbleLetInstr);
      instr->_base.tag = FBLE_LET_INSTR;
      FbleVectorInit(arena, instr->bindings);
      instr->body = NULL;
      instr->pop._base.tag = FBLE_POP_INSTR;
      instr->pop.count = let_expr->bindings.size;
      instr->weaken._base.tag = FBLE_WEAKEN_INSTR;
      instr->weaken.count = let_expr->bindings.size;

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleInstr* mkval = NULL;
        Type* type = NULL;
        if (!error) {
          type = Compile(arena, nvars, type_vars, let_expr->bindings.xs[i].expr, &mkval);
        }
        error = error || (type == NULL);
        FbleVectorAppend(arena, instr->bindings, mkval);

        if (!error && !TypesEqual(arena, nvd[i].type, type)) {
          error = true;
          FbleReportError("expected type ", &let_expr->bindings.xs[i].expr->loc);
          PrintType(nvd[i].type);
          fprintf(stderr, ", but found ");
          PrintType(type);
          fprintf(stderr, "\n");
        }
        TypeDropStrongRef(arena, type);
      }

      Type* rtype = NULL;
      if (!error) {
        rtype = Compile(arena, nvars, type_vars, let_expr->body, &(instr->body));
        error = (rtype == NULL);
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        TypeDropStrongRef(arena, nvd[i].type);
      }

      if (error) {
        TypeDropStrongRef(arena, rtype);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      *instrs = &instr->_base;
      return rtype;
    }

    case FBLE_TYPE_LET_EXPR: {
      FbleTypeLetExpr* let = (FbleTypeLetExpr*)expr;

      Vars ntvd[let->bindings.size];
      Vars* ntvs = type_vars;

      // Set up reference types for all the bindings, to support recursive
      // types.
      for (size_t i = 0; i < let->bindings.size; ++i) {
        RefType* ref = FbleAlloc(arena, RefType);
        ref->_base.tag = REF_TYPE;
        ref->_base.loc = let->bindings.xs[i].name.loc;
        ref->_base.strong_ref_count = 1;
        ref->_base.weak_ref_count = 0;
        ref->kind = CompileKind(arena, let->bindings.xs[i].kind);
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
        types[i] = CompileType(arena, ntvs, let->bindings.xs[i].type);
        error = error || (types[i] == NULL);

        if (types[i] != NULL) {
          // TODO: get expected_kind from ntvd[i]->kind rather than recompile?
          Kind* expected_kind = CompileKind(arena, let->bindings.xs[i].kind);
          Kind* actual_kind = GetKind(arena, types[i]);
          if (!KindsEqual(expected_kind, actual_kind)) {
            FbleReportError("expected kind ", &types[i]->loc);
            PrintKind(expected_kind);
            fprintf(stderr, ", but found ");
            PrintKind(actual_kind);
            fprintf(stderr, "\n");
            error = true;
          }

          FreeKind(arena, expected_kind);
          FreeKind(arena, actual_kind);
        }
      }

      for (size_t i = 0; i < let->bindings.size; ++i) {
        // At this point types[i] has a strong reference to it.
        // Take a weak reference to types[i] for ref->value, and reuse the
        // strong reference to it for ntvd.
        RefType* ref = (RefType*)ntvd[i].type;
        assert(ref->_base.tag == REF_TYPE);
        if (types[i] != NULL) {
          ref->value = TypeTakeWeakRef(arena, types[i]);
        }
        TypeDropStrongRef(arena, &ref->_base);
        ntvd[i].type = types[i];
      }

      Type* rtype = NULL;
      if (!error) {
        rtype = Compile(arena, vars, ntvs, let->body, instrs);
      }

      for (size_t i = 0; i < let->bindings.size; ++i) {
        TypeDropStrongRef(arena, ntvd[i].type);
      }
      return rtype;
    }

    case FBLE_POLY_EXPR: {
      FblePolyExpr* poly = (FblePolyExpr*)expr;

      PolyType* pt = FbleAlloc(arena, PolyType);
      pt->_base.tag = POLY_TYPE;
      pt->_base.loc = expr->loc;
      pt->_base.strong_ref_count = 1;
      FbleVectorInit(arena, pt->args);

      Vars ntvd[poly->args.size];
      Vars* ntvs = type_vars;
      for (size_t i = 0; i < poly->args.size; ++i) {
        AbstractType* arg = FbleAlloc(arena, AbstractType);
        arg->_base.tag = ABSTRACT_TYPE;
        arg->_base.loc = poly->args.xs[i].name.loc;
        arg->_base.strong_ref_count = 1;
        arg->_base.weak_ref_count = 0;
        arg->kind = CompileKind(arena, poly->args.xs[i].kind);
        FbleVectorAppend(arena, pt->args, &arg->_base);

        ntvd[i].name = poly->args.xs[i].name;
        ntvd[i].type = &arg->_base;
        ntvd[i].next = ntvs;
        ntvs = ntvd + i;
      }

      pt->body = Compile(arena, vars, ntvs, poly->body, instrs);
      if (pt->body == NULL) {
        TypeDropStrongRef(arena, &pt->_base);
        return NULL;
      }

      return &pt->_base;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;
      PolyApplyType* pat = FbleAlloc(arena, PolyApplyType);
      pat->_base.tag = POLY_APPLY_TYPE;
      pat->_base.loc = expr->loc;
      pat->_base.strong_ref_count = 1;
      pat->_base.weak_ref_count = 0;
      FbleVectorInit(arena, pat->args);

      pat->poly = Compile(arena, vars, type_vars, apply->poly, instrs);
      if (pat->poly == NULL) {
        TypeDropStrongRef(arena, &pat->_base);
        return NULL;
      }

      PolyKind* poly_kind = (PolyKind*)GetKind(arena, pat->poly);
      if (poly_kind->_base.tag != POLY_KIND) {
        FbleReportError("cannot apply poly args to a basic kinded entity", &expr->loc);
        FreeKind(arena, &poly_kind->_base);
        TypeDropStrongRef(arena, &pat->_base);
        return NULL;
      }

      bool error = false;
      if (apply->args.size > poly_kind->args.size) {
        FbleReportError("expected %i poly args, but %i provided",
            &expr->loc, poly_kind->args.size, apply->args.size);
        error = true;
      }

      for (size_t i = 0; i < apply->args.size; ++i) {
        Type* arg = CompileType(arena, type_vars, apply->args.xs[i]);
        FbleVectorAppend(arena, pat->args, arg);
        error = (error || arg == NULL);

        if (arg != NULL && i < poly_kind->args.size) {
          Kind* expected_kind = poly_kind->args.xs[i];
          Kind* actual_kind = GetKind(arena, arg);
          if (!KindsEqual(expected_kind, actual_kind)) {
            FbleReportError("expected kind ", &arg->loc);
            PrintKind(expected_kind);
            fprintf(stderr, ", but found ");
            PrintKind(actual_kind);
            fprintf(stderr, "\n");
            error = true;
          }
          FreeKind(arena, actual_kind);
        }
      }

      FreeKind(arena, &poly_kind->_base);
      if (error) {
        TypeDropStrongRef(arena, &pat->_base);
        FbleFreeInstrs(arena, *instrs);
        *instrs = NULL;
        return NULL;
      }

      return &pat->_base;
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
//   vars - the value of type variables in scope.
//   type - the type to compile.
//
// Results:
//   The compiled and evaluated type, or NULL in case of error.
//
// Side effects:
//   Prints a message to stderr if the type fails to compile or evalute.
//   Allocates a reference-counted type that must be freed using
//   TypeDropStrongRef when it is no longer needed.
static Type* CompileType(FbleArena* arena, Vars* vars, FbleType* type)
{
  switch (type->tag) {
    case FBLE_STRUCT_TYPE: {
      StructType* st = FbleAlloc(arena, StructType);
      st->_base.loc = type->loc;
      st->_base.strong_ref_count = 1;
      st->_base.weak_ref_count = 0;
      st->_base.tag = STRUCT_TYPE;
      FbleVectorInit(arena, st->fields);
      FbleStructType* struct_type = (FbleStructType*)type;
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleField* field = struct_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          TypeDropStrongRef(arena, &st->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena, st->fields);
        cfield->name = field->name;
        cfield->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, struct_type->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, field->name.name);
            TypeDropStrongRef(arena, &st->_base);
            return NULL;
          }
        }
      }
      return &st->_base;
    }

    case FBLE_UNION_TYPE: {
      UnionType* ut = FbleAlloc(arena, UnionType);
      ut->_base.loc = type->loc;
      ut->_base.strong_ref_count = 1;
      ut->_base.weak_ref_count = 0;
      ut->_base.tag = UNION_TYPE;
      FbleVectorInit(arena, ut->fields);

      FbleUnionType* union_type = (FbleUnionType*)type;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleField* field = union_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          TypeDropStrongRef(arena, &ut->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena, ut->fields);
        cfield->name = field->name;
        cfield->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, union_type->fields.xs[j].name.name)) {
            FbleReportError("duplicate field name '%s'\n", &field->name.loc, field->name.name);
            TypeDropStrongRef(arena, &ut->_base);
            return NULL;
          }
        }
      }
      return &ut->_base;
    }

    case FBLE_FUNC_TYPE: {
      FuncType* ft = FbleAlloc(arena, FuncType);
      ft->_base.loc = type->loc;
      ft->_base.strong_ref_count = 1;
      ft->_base.weak_ref_count = 0;
      ft->_base.tag = FUNC_TYPE;
      FbleVectorInit(arena, ft->args);
      ft->rtype = NULL;

      FbleFuncType* func_type = (FbleFuncType*)type;
      for (size_t i = 0; i < func_type->args.size; ++i) {
        FbleField* field = func_type->args.xs + i;

        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          TypeDropStrongRef(arena, &ft->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena, ft->args);
        cfield->name = field->name;
        cfield->type = compiled;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(field->name.name, func_type->args.xs[j].name.name)) {
            FbleReportError("duplicate arg name '%s'\n", &field->name.loc, field->name.name);
            TypeDropStrongRef(arena, &ft->_base);
            return NULL;
          }
        }
      }

      Type* compiled = CompileType(arena, vars, func_type->rtype);
      if (compiled == NULL) {
        TypeDropStrongRef(arena, &ft->_base);
        return NULL;
      }
      ft->rtype = compiled;
      return &ft->_base;
    }

    case FBLE_PROC_TYPE: assert(false && "TODO: FBLE_PROC_TYPE"); return NULL;
    case FBLE_INPUT_TYPE: assert(false && "TODO: FBLE_INPUT_TYPE"); return NULL;
    case FBLE_OUTPUT_TYPE: assert(false && "TODO: FBLE_OUTPUT_TYPE"); return NULL;

    case FBLE_VAR_TYPE: {
      FbleVarType* var_type = (FbleVarType*)type;

      while (vars != NULL && !FbleNamesEqual(var_type->var.name, vars->name.name)) {
        vars = vars->next;
      }

      if (vars == NULL) {
        FbleReportError("variable '%s' not defined\n", &var_type->var.loc, var_type->var.name);
        return NULL;
      }

      VarType* vt = FbleAlloc(arena, VarType);
      vt->_base.loc = type->loc;
      vt->_base.strong_ref_count = 1;
      vt->_base.weak_ref_count = 0;
      vt->_base.tag = VAR_TYPE;
      vt->var = var_type->var;
      vt->value = TypeTakeStrongRef(arena, vars->type);
      return &vt->_base;
    }

    case FBLE_LET_TYPE: {
      FbleLetType* let = (FbleLetType*)type;

      Vars ntvd[let->bindings.size];
      Vars* ntvs = vars;
      bool error = false;
      for (size_t i = 0; i < let->bindings.size; ++i) {
        ntvd[i].name = let->bindings.xs[i].name;
        ntvd[i].type = CompileType(arena, vars, let->bindings.xs[i].type);
        error = error || (ntvd[i].type == NULL);

        if (ntvd[i].type != NULL) {
          Kind* expected_kind = CompileKind(arena, let->bindings.xs[i].kind);
          Kind* actual_kind = GetKind(arena, ntvd[i].type);
          if (!KindsEqual(expected_kind, actual_kind)) {
            FbleReportError("expected kind ", &ntvd[i].type->loc);
            PrintKind(expected_kind);
            fprintf(stderr, ", but found ");
            PrintKind(actual_kind);
            fprintf(stderr, "\n");
            error = true;
          }

          FreeKind(arena, expected_kind);
          FreeKind(arena, actual_kind);
        }

        ntvd[i].next = ntvs;
        ntvs = ntvd + i;
      }

      Type* rtype = NULL;
      if (!error) {
        rtype = CompileType(arena, ntvs, let->body);
      }

      for (size_t i = 0; i < let->bindings.size; ++i) {
        TypeDropStrongRef(arena, ntvd[i].type);
      }
      return rtype;
    }

    case FBLE_POLY_TYPE: {
      FblePolyType* poly = (FblePolyType*)type;
      PolyType* pt = FbleAlloc(arena, PolyType);
      pt->_base.tag = POLY_TYPE;
      pt->_base.loc = type->loc;
      pt->_base.strong_ref_count = 1;
      pt->_base.weak_ref_count = 0;
      FbleVectorInit(arena, pt->args);

      Vars ntvd[poly->args.size];
      Vars* ntvs = vars;
      for (size_t i = 0; i < poly->args.size; ++i) {
        AbstractType* arg = FbleAlloc(arena, AbstractType);
        arg->_base.tag = ABSTRACT_TYPE;
        arg->_base.loc = poly->args.xs[i].name.loc;
        arg->_base.strong_ref_count = 1;
        arg->_base.weak_ref_count = 0;
        arg->kind = CompileKind(arena, poly->args.xs[i].kind);
        FbleVectorAppend(arena, pt->args, &arg->_base);

        ntvd[i].name = poly->args.xs[i].name;
        ntvd[i].type = &arg->_base;
        ntvd[i].next = ntvs;
        ntvs = ntvd + i;
      }

      pt->body = CompileType(arena, ntvs, poly->body);
      if (pt->body == NULL) {
        TypeDropStrongRef(arena, &pt->_base);
        return NULL;
      }

      return &pt->_base;
    }

    case FBLE_POLY_APPLY_TYPE: {
      FblePolyApplyType* apply = (FblePolyApplyType*)type;
      PolyApplyType* pat = FbleAlloc(arena, PolyApplyType);
      pat->_base.tag = POLY_APPLY_TYPE;
      pat->_base.loc = type->loc;
      pat->_base.strong_ref_count = 1;
      pat->_base.weak_ref_count = 0;
      FbleVectorInit(arena, pat->args);

      pat->poly = CompileType(arena, vars, apply->poly);
      if (pat->poly == NULL) {
        TypeDropStrongRef(arena, &pat->_base);
        return NULL;
      }

      PolyKind* poly_kind = (PolyKind*)GetKind(arena, pat->poly);
      if (poly_kind->_base.tag != POLY_KIND) {
        FbleReportError("cannot apply poly args to a basic kinded entity", &type->loc);
        FreeKind(arena, &poly_kind->_base);
        TypeDropStrongRef(arena, &pat->_base);
        return NULL;
      }

      bool error = false;
      if (apply->args.size > poly_kind->args.size) {
        FbleReportError("expected %i poly args, but %i provided",
            &type->loc, poly_kind->args.size, apply->args.size);
        error = true;
      }

      for (size_t i = 0; i < apply->args.size; ++i) {
        Type* arg = CompileType(arena, vars, apply->args.xs[i]);
        FbleVectorAppend(arena, pat->args, arg);
        error = (error || arg == NULL);

        if (arg != NULL && i < poly_kind->args.size) {
          Kind* expected_kind = poly_kind->args.xs[i];
          Kind* actual_kind = GetKind(arena, arg);
          if (!KindsEqual(expected_kind, actual_kind)) {
            FbleReportError("expected kind ", &arg->loc);
            PrintKind(expected_kind);
            fprintf(stderr, ", but found ");
            PrintKind(actual_kind);
            fprintf(stderr, "\n");
            error = true;
          }
          FreeKind(arena, actual_kind);
        }
      }

      FreeKind(arena, &poly_kind->_base);
      if (error) {
        TypeDropStrongRef(arena, &pat->_base);
        return NULL;
      }

      return &pat->_base;
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

// FbleFreeInstrs -- see documentation in fble-internal.h
void FbleFreeInstrs(FbleArena* arena, FbleInstr* instrs)
{
  if (instrs == NULL) {
    return;
  }

  switch (instrs->tag) {
    case FBLE_VAR_INSTR:
    case FBLE_FUNC_APPLY_INSTR:
    case FBLE_STRUCT_ACCESS_INSTR:
    case FBLE_UNION_ACCESS_INSTR:
    case FBLE_POP_INSTR:
    case FBLE_WEAKEN_INSTR: {
      FbleFree(arena, instrs);
      return;
    }

    case FBLE_LET_INSTR: {
      FbleLetInstr* let_instr = (FbleLetInstr*)instrs;
      for (size_t i = 0; i < let_instr->bindings.size; ++i) {
        FbleFreeInstrs(arena, let_instr->bindings.xs[i]);
      }
      FbleFree(arena, let_instr->bindings.xs);
      FbleFreeInstrs(arena, let_instr->body);
      FbleFree(arena, let_instr);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instrs;
      FbleFreeInstrs(arena, func_value_instr->body);
      FbleFree(arena, func_value_instr);
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* instr = (FbleStructValueInstr*)instrs;
      for (size_t i = 0; i < instr->fields.size; ++i) {
        FbleFreeInstrs(arena, instr->fields.xs[i]);
      }
      FbleFree(arena, instr->fields.xs);
      FbleFree(arena, instr);
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* instr = (FbleUnionValueInstr*)instrs;
      FbleFreeInstrs(arena, instr->mkarg);
      FbleFree(arena, instrs);
      return;
    }

    case FBLE_COND_INSTR: {
      FbleCondInstr* instr = (FbleCondInstr*)instrs;
      for (size_t i = 0; i < instr->choices.size; ++i) {
        FbleFreeInstrs(arena, instr->choices.xs[i]);
      }
      FbleFree(arena, instr->choices.xs);
      FbleFree(arena, instrs);
      return;
    }

    case FBLE_PUSH_INSTR: {
      FblePushInstr* push_instr = (FblePushInstr*)instrs;
      for (size_t i = 0; i < push_instr->values.size; ++i) {
        FbleFreeInstrs(arena, push_instr->values.xs[i]);
      }
      FbleFree(arena, push_instr->values.xs);
      FbleFreeInstrs(arena, push_instr->next);
      FbleFree(arena, instrs);
      return;
    }
  }
  UNREACHABLE("invalid instruction");
}

// FbleCompile -- see documentation in fble-internal.h
FbleInstr* FbleCompile(FbleArena* arena, FbleExpr* expr)
{
  FbleInstr* instrs = NULL;
  Type* type = Compile(arena, NULL, NULL, expr, &instrs);
  if (type == NULL) {
    return NULL;
  }
  TypeDropStrongRef(arena, type);
  return instrs;
}
