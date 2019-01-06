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
  TypeTag tag;
  FbleLoc loc;
  int strong_ref_count;
  int break_cycle_ref_count;
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
  bool broke_cycle;
  struct RefType* siblings;
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

static Type* TypeTakeStrongRef(Type* type);
static Type* TypeBreakCycleRef(FbleArena* arena, Type* type);
static void TypeDropStrongRef(FbleArena* arena, Type* type);
static void DropBreakCycleRef(Type* type);
static void BreakCycle(FbleArena* arena, Type* type);
static void UnBreakCycle(Type* type);
static void FreeType(FbleArena* arena, Type* type);

static Kind* GetKind(FbleArena* arena, Type* type);
static bool HasParams(Type* type, TypeV params, TypeList* visited);
static Type* Subst(FbleArena* arena, Type* src, TypeV params, TypeV args);
static Type* SubstInternal(FbleArena* arena, Type* src, TypeV params, TypeV args, TypePairs* tps, RefTypeV* refs);
static void Eval(FbleArena* arena, Type* type, TypeList* evaled, PolyApplyList* applied);
static Type* Normal(Type* type);
static bool TypesEqual(Type* a, Type* b, TypePairs* eq);
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
//   type - the type to take the reference for.
//
// Results:
//   The type with incremented strong reference count.
//
// Side effects:
//   The returned type must be freed using TypeDropStrongRef when no longer in use.
static Type* TypeTakeStrongRef(Type* type)
{
  if (type != NULL) {
    assert(type->strong_ref_count > 0);
    if (type->strong_ref_count++ == type->break_cycle_ref_count) {
      UnBreakCycle(type);
    }
  }
  return type;
}

// TypeBreakCycleRef --
//   Increment the break cycle reference count of a compiled type.
//
// Inputs:
//   arena - for allocations.
//   type - the type to take the reference for.
//
// Results:
//   The type with incremented break cycle reference count.
//
// Side effects:
//   Increments the break cycle reference count on the type. Breaks the cycle
//   if necessary, which could cause types to be freed.
static Type* TypeBreakCycleRef(FbleArena* arena, Type* type)
{
  if (type != NULL) {
    type->break_cycle_ref_count++;
    if (type->break_cycle_ref_count == type->strong_ref_count) {
      BreakCycle(arena, type);
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

      case PROC_TYPE: {
        ProcType* pt = (ProcType*)type;
        TypeDropStrongRef(arena, pt->rtype);
        break;
      }

      case INPUT_TYPE: {
        InputType* t = (InputType*)type;
        TypeDropStrongRef(arena, t->rtype);
        break;
      }

      case OUTPUT_TYPE: {
        OutputType* t = (OutputType*)type;
        TypeDropStrongRef(arena, t->rtype);
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
        TypeDropStrongRef(arena, pat->result);
        break;
      }

      case REF_TYPE: {
        RefType* ref = (RefType*)type;
        if (ref->broke_cycle) {
          DropBreakCycleRef(ref->value);
        }
        TypeDropStrongRef(arena, ref->value);
        break;
      }
    }

    FreeType(arena, type);
  } else {
    type->strong_ref_count--;
    if (type->strong_ref_count == type->break_cycle_ref_count) {
      BreakCycle(arena, type);
    }
  }
}

// DropBreakCycleRef --
//   Decrement the break cycle reference count of a type.
//
// Inputs:
//   type - the type to decrement the break cycle reference count of.
//          May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Decrements the break cycle reference count of the type.
static void DropBreakCycleRef(Type* type)
{
  if (type == NULL) {
    return;
  }

  assert(type->break_cycle_ref_count > 0);
  if (type->break_cycle_ref_count-- == type->strong_ref_count) {
    UnBreakCycle(type);
  }
}

// BreakCycle --
//   Called when the strong ref count and break cycle ref count of type
//   become equal.
//
// Inputs:
//   arena - arena for deallocation
//   type - the type to break the cycle of.
//
// Results: 
//   none.
//
// Side effects:
//   Propagates the broken cycle to references and, if appropriate, breaks the
//   cycle through ref types.
static void BreakCycle(FbleArena* arena, Type* type)
{
  assert(type->strong_ref_count > 0);
  assert(type->strong_ref_count == type->break_cycle_ref_count);
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      for (size_t i = 0; i < st->fields.size; ++i) {
        TypeBreakCycleRef(arena, st->fields.xs[i].type);
      }
      break;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        TypeBreakCycleRef(arena, ut->fields.xs[i].type);
      }
      break;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      for (size_t i = 0; i < ft->args.size; ++i) {
        TypeBreakCycleRef(arena, ft->args.xs[i].type);
      }
      TypeBreakCycleRef(arena, ft->rtype);
      break;
    }

    case PROC_TYPE: {
      ProcType* pt = (ProcType*)type;
      TypeBreakCycleRef(arena, pt->rtype);
      break;
    }

    case INPUT_TYPE: {
      InputType* t = (InputType*)type;
      TypeBreakCycleRef(arena, t->rtype);
      break;
    }

    case OUTPUT_TYPE: {
      OutputType* t = (OutputType*)type;
      TypeBreakCycleRef(arena, t->rtype);
      break;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      TypeBreakCycleRef(arena, var->value);
      break;
    }

    case ABSTRACT_TYPE: {
      break;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      for (size_t i = 0; i < pt->args.size; ++i) {
        TypeBreakCycleRef(arena, pt->args.xs[i]);
      }
      TypeBreakCycleRef(arena, pt->body);
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      TypeBreakCycleRef(arena, pat->poly);
      for (size_t i = 0; i < pat->args.size; ++i) {
        TypeBreakCycleRef(arena, pat->args.xs[i]);
      }
      TypeBreakCycleRef(arena, pat->result);
      break;
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;

      // Check if all siblings can be broken now.
      bool can_break = true;
      RefType* sibling = ref;
      do {
        if (sibling->_base.strong_ref_count != sibling->_base.break_cycle_ref_count) {
          can_break = false;
          break;
        }
        sibling = sibling->siblings;
      } while (sibling != ref);

      if (can_break) {
        // Break the references of all the siblings.
        // 1. Grab strong references to all the siblings to make sure they
        // don't get freed out from under us.
        sibling = ref;
        do {
          TypeTakeStrongRef(&sibling->_base);
          sibling = sibling->siblings;
        } while (sibling != ref);

        // 2. Break the references of all the siblings.
        sibling = ref;
        do {
          Type* value = sibling->value;
          sibling->value = NULL;
          if (sibling->broke_cycle) {
            DropBreakCycleRef(value);
          }
          TypeDropStrongRef(arena, value);
          sibling = sibling->siblings;
        } while (sibling != ref);

        // 3. Release our strong references to the siblings.
        while (ref->siblings != ref) {
          TypeDropStrongRef(arena, &(ref->siblings->_base));
        }
        TypeDropStrongRef(arena, &ref->_base);
      }

      break;
    }
  }
}

// UnBreakCycle --
//   Called when the strong ref count and break cycle ref count of type
//   become unequal.
//
// Inputs:
//   type - the type to unbreak the cycle of.
//
// Results: 
//   none.
//
// Side effects:
//   Propagates the unbroken cycle to references.
static void UnBreakCycle(Type* type)
{
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      for (size_t i = 0; i < st->fields.size; ++i) {
        DropBreakCycleRef(st->fields.xs[i].type);
      }
      break;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      for (size_t i = 0; i < ut->fields.size; ++i) {
        DropBreakCycleRef(ut->fields.xs[i].type);
      }
      break;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      for (size_t i = 0; i < ft->args.size; ++i) {
        DropBreakCycleRef(ft->args.xs[i].type);
      }
      DropBreakCycleRef(ft->rtype);
      break;
    }

    case PROC_TYPE: {
      ProcType* pt = (ProcType*)type;
      DropBreakCycleRef(pt->rtype);
      break;
    }

    case INPUT_TYPE: {
      InputType* t = (InputType*)type;
      DropBreakCycleRef(t->rtype);
      break;
    }

    case OUTPUT_TYPE: {
      OutputType* t = (OutputType*)type;
      DropBreakCycleRef(t->rtype);
      break;
    }

    case VAR_TYPE: {
      VarType* var = (VarType*)type;
      DropBreakCycleRef(var->value);
      break;
    }

    case ABSTRACT_TYPE: {
      break;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      for (size_t i = 0; i < pt->args.size; ++i) {
        DropBreakCycleRef(pt->args.xs[i]);
      }
      DropBreakCycleRef(pt->body);
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      DropBreakCycleRef(pat->poly);
      for (size_t i = 0; i < pat->args.size; ++i) {
        DropBreakCycleRef(pat->args.xs[i]);
      }
      DropBreakCycleRef(pat->result);
      break;
    }

    case REF_TYPE: {
      // Nothing to do here. 
      break;
    }
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

    case PROC_TYPE:
    case INPUT_TYPE:
    case OUTPUT_TYPE:
    case VAR_TYPE: {
      FbleFree(arena, type);
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

      // Remove this ref from its list of siblings.
      RefType* sibling = ref->siblings;
      while (sibling->siblings != ref) {
        sibling = sibling->siblings;
      }
      sibling->siblings = ref->siblings;

      // TODO: If the rest of the siblings can be broken, do that now!

      FreeKind(arena, ref->kind);
      FbleFree(arena, ref);
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
        if (HasParams(ft->args.xs[i].type, params, &nv)) {
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
//   given type. The returned type may not be fully evaluated or normalized.
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
//   The caller is responsible for calling TypeDropStrongRef on the returned
//   type when it is no longer needed.
static Type* Subst(FbleArena* arena, Type* type, TypeV params, TypeV args)
{
  RefTypeV refs;
  FbleVectorInit(arena, refs);
  Type* result = SubstInternal(arena, type, params, args, NULL, &refs);

  if (refs.size > 0) {
    for (size_t i = 1; i < refs.size; ++i) {
      refs.xs[i]->siblings = refs.xs[i-1];
    }
    refs.xs[0]->siblings = refs.xs[refs.size-1];

    for (size_t i = 0; i < refs.size; ++i) {
      TypeBreakCycleRef(arena, refs.xs[i]->value);
      refs.xs[i]->broke_cycle = true;
      TypeDropStrongRef(arena, &refs.xs[i]->_base);
    }
  }
  FbleFree(arena, refs.xs);
  return result;
}

// SubstInternal --
//   Substitute the given arguments in place of the given parameters in the
//   given type.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the type to substitute into.
//   params - the abstract types to substitute out.
//   args - the concrete types to substitute in.
//   tps - a map of already substituted types, to avoid infinite recursion.
//   refs - a collection of new RefTypes being created for the substitution.
//
// Results:
//   A type with all occurrences of params replaced with the corresponding
//   args types.
//
// Side effects:
//   The caller is responsible for calling TypeDropStrongRef on the returned
//   type when it is no longer needed. The caller needs to hook up the refs.
static Type* SubstInternal(FbleArena* arena, Type* type, TypeV params, TypeV args, TypePairs* tps, RefTypeV* refs)
{
  if (!HasParams(type, params, NULL)) {
    return TypeTakeStrongRef(type);
  }

  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      StructType* sst = FbleAlloc(arena, StructType);
      sst->_base.tag = STRUCT_TYPE;
      sst->_base.loc = st->_base.loc;
      sst->_base.strong_ref_count = 1;
      sst->_base.break_cycle_ref_count = 0;
      FbleVectorInit(arena, sst->fields);
      for (size_t i = 0; i < st->fields.size; ++i) {
        Field* field = FbleVectorExtend(arena, sst->fields);
        field->name = st->fields.xs[i].name;
        field->type = SubstInternal(arena, st->fields.xs[i].type, params, args, tps, refs);
      }
      return &sst->_base;
    }

    case UNION_TYPE: {
      UnionType* ut = (UnionType*)type;
      UnionType* sut = FbleAlloc(arena, UnionType);
      sut->_base.tag = UNION_TYPE;
      sut->_base.loc = ut->_base.loc;
      sut->_base.strong_ref_count = 1;
      sut->_base.break_cycle_ref_count = 0;
      FbleVectorInit(arena, sut->fields);
      for (size_t i = 0; i < ut->fields.size; ++i) {
        Field* field = FbleVectorExtend(arena, sut->fields);
        field->name = ut->fields.xs[i].name;
        field->type = SubstInternal(arena, ut->fields.xs[i].type, params, args, tps, refs);
      }
      return &sut->_base;
    }

    case FUNC_TYPE: {
      FuncType* ft = (FuncType*)type;
      FuncType* sft = FbleAlloc(arena, FuncType);
      sft->_base.tag = FUNC_TYPE;
      sft->_base.loc = ft->_base.loc;
      sft->_base.strong_ref_count = 1;
      sft->_base.break_cycle_ref_count = 0;
      FbleVectorInit(arena, sft->args);
      for (size_t i = 0; i < ft->args.size; ++i) {
        Field* arg = FbleVectorExtend(arena, sft->args);
        arg->name = ft->args.xs[i].name;
        arg->type = SubstInternal(arena, ft->args.xs[i].type, params, args, tps, refs);
      }
      sft->rtype = SubstInternal(arena, ft->rtype, params, args, tps, refs);
      return &sft->_base;
    }

    case PROC_TYPE: {
      ProcType* pt = (ProcType*)type;
      ProcType* spt = FbleAlloc(arena, ProcType);
      spt->_base.tag = PROC_TYPE;
      spt->_base.loc = pt->_base.loc;
      spt->_base.strong_ref_count = 1;
      spt->_base.break_cycle_ref_count = 0;
      spt->rtype = SubstInternal(arena, pt->rtype, params, args, tps, refs);
      return &spt->_base;
    }

    case INPUT_TYPE: {
      InputType* pt = (InputType*)type;
      InputType* spt = FbleAlloc(arena, InputType);
      spt->_base.tag = INPUT_TYPE;
      spt->_base.loc = pt->_base.loc;
      spt->_base.strong_ref_count = 1;
      spt->_base.break_cycle_ref_count = 0;
      spt->rtype = SubstInternal(arena, pt->rtype, params, args, tps, refs);
      return &spt->_base;
    }

    case OUTPUT_TYPE: {
      OutputType* pt = (OutputType*)type;
      OutputType* spt = FbleAlloc(arena, OutputType);
      spt->_base.tag = OUTPUT_TYPE;
      spt->_base.loc = pt->_base.loc;
      spt->_base.strong_ref_count = 1;
      spt->_base.break_cycle_ref_count = 0;
      spt->rtype = SubstInternal(arena, pt->rtype, params, args, tps, refs);
      return &spt->_base;
    }

    case VAR_TYPE: {
      VarType* vt = (VarType*)type;
      VarType* svt = FbleAlloc(arena, VarType);
      svt->_base.tag = VAR_TYPE;
      svt->_base.loc = vt->_base.loc;
      svt->_base.strong_ref_count = 1;
      svt->_base.break_cycle_ref_count = 0;
      svt->var = vt->var;
      svt->value = SubstInternal(arena, vt->value, params, args, tps, refs);
      return &svt->_base;
    }

    case ABSTRACT_TYPE: {
      for (size_t i = 0; i < params.size; ++i) {
        if (type == params.xs[i]) {
          return TypeTakeStrongRef(args.xs[i]);
        }
      }
      assert(false && "HasParams result was wrong.");
      return TypeTakeStrongRef(type);
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      PolyType* spt = FbleAlloc(arena, PolyType);
      spt->_base.tag = POLY_TYPE;
      spt->_base.loc = pt->_base.loc;
      spt->_base.strong_ref_count = 1;
      spt->_base.break_cycle_ref_count = 0;
      FbleVectorInit(arena, spt->args);
      for (size_t i = 0; i < pt->args.size; ++i) {
        FbleVectorAppend(arena, spt->args, TypeTakeStrongRef(pt->args.xs[i]));
      }
      spt->body = SubstInternal(arena, pt->body, params, args, tps, refs);
      return &spt->_base;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PolyApplyType* spat = FbleAlloc(arena, PolyApplyType);
      spat->_base.tag = POLY_APPLY_TYPE;
      spat->_base.loc = pat->_base.loc;
      spat->_base.strong_ref_count = 1;
      spat->_base.break_cycle_ref_count = 0;
      spat->poly = SubstInternal(arena, pat->poly, params, args, tps, refs);
      spat->result = NULL;
      FbleVectorInit(arena, spat->args);
      for (size_t i = 0; i < pat->args.size; ++i) {
        Type* arg = SubstInternal(arena, pat->args.xs[i], params, args, tps, refs);
        FbleVectorAppend(arena, spat->args, arg);
      }
      return &spat->_base;
    }

    case REF_TYPE: {
      RefType* ref = (RefType*)type;

      // Treat null refs as abstract types.
      if (ref->value == NULL) {
        return TypeTakeStrongRef(type);
      }

      // Check to see if we've already done substitution on the value pointed
      // to by this ref.
      for (TypePairs* tp = tps; tp != NULL; tp = tp->next) {
        if (tp->a == ref->value) {
          return TypeTakeStrongRef(tp->b);
        }
      }

      RefType* sref = FbleAlloc(arena, RefType);
      sref->_base.tag = REF_TYPE;
      sref->_base.loc = type->loc;
      sref->_base.strong_ref_count = 1;
      sref->_base.break_cycle_ref_count = 0;
      sref->kind = CopyKind(arena, ref->kind);
      sref->value = NULL;
      sref->broke_cycle = false;

      TypePairs ntp = {
        .a = ref->value,
        .b = &sref->_base,
        .next = tps
      };

      FbleVectorAppend(arena, *refs, sref);
      sref->value = SubstInternal(arena, ref->value, params, args, &ntp, refs);
      return TypeTakeStrongRef(sref->value);
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
static void Eval(FbleArena* arena, Type* type, TypeList* evaled, PolyApplyList* applied)
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
      for (size_t i = 0; i < ft->args.size; ++i) {
        Eval(arena, ft->args.xs[i].type, &nevaled, applied);
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
            pat->result = TypeTakeStrongRef(pal->result);
            return;
          }
        }
      }

      PolyType* poly = (PolyType*)Normal(pat->poly);
      if (poly->_base.tag == POLY_TYPE) {
        pat->result = Subst(arena, poly->body, poly->args, pat->args);

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
        if (!TypesEqual(fta->args.xs[i].type, ftb->args.xs[i].type, &neq)) {
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

    case PROC_TYPE: {
      ProcType* pt = (ProcType*)type;
      PrintType(pt->rtype);
      fprintf(stderr, "!");
      break;
    }

    case INPUT_TYPE: {
      InputType* pt = (InputType*)type;
      PrintType(pt->rtype);
      fprintf(stderr, "-");
      break;
    }

    case OUTPUT_TYPE: {
      OutputType* pt = (OutputType*)type;
      PrintType(pt->rtype);
      fprintf(stderr, "+");
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
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      StructType* struct_type = (StructType*)Normal(type);
      if (struct_type->_base.tag != STRUCT_TYPE) {
        FbleReportError("expected a struct type, but found ", &struct_value_expr->type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        TypeDropStrongRef(arena, type);
        return NULL;
      }

      if (struct_type->fields.size != struct_value_expr->args.size) {
        // TODO: Where should the error message go?
        FbleReportError("expected %i args, but %i were provided\n",
            &expr->loc, struct_type->fields.size, struct_value_expr->args.size);
        TypeDropStrongRef(arena, type);
        return NULL;
      }

      bool error = false;
      FbleStructValueInstr* instr = FbleAlloc(arena, FbleStructValueInstr);
      instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      instr->_base.refcount = 1;
      FbleVectorInit(arena, instr->fields);
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        Field* field = struct_type->fields.xs + i;

        FbleInstr* mkarg = NULL;
        Type* arg_type = Compile(arena, vars, type_vars, struct_value_expr->args.xs[i], &mkarg);
        Eval(arena, arg_type, NULL, NULL);
        error = error || (arg_type == NULL);

        if (arg_type != NULL && !TypesEqual(field->type, arg_type, NULL)) {
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
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        return NULL;
      }

      UnionType* union_type = (UnionType*)Normal(type);
      if (union_type->_base.tag != UNION_TYPE) {
        FbleReportError("expected a union type, but found ", &union_value_expr->type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        TypeDropStrongRef(arena, type);
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
        return NULL;
      }

      FbleInstr* mkarg = NULL;
      Type* arg_type = Compile(arena, vars, type_vars, union_value_expr->arg, &mkarg);
      Eval(arena, arg_type, NULL, NULL);
      if (arg_type == NULL) {
        TypeDropStrongRef(arena, type);
        return NULL;
      }

      if (!TypesEqual(field_type, arg_type, NULL)) {
        FbleReportError("expected type ", &union_value_expr->arg->loc);
        PrintType(field_type);
        fprintf(stderr, ", but found type ");
        PrintType(arg_type);
        fprintf(stderr, "\n");
        FbleFreeInstrs(arena, mkarg);
        TypeDropStrongRef(arena, type);
        TypeDropStrongRef(arena, arg_type);
        return NULL;
      }
      TypeDropStrongRef(arena, arg_type);

      FbleUnionValueInstr* instr = FbleAlloc(arena, FbleUnionValueInstr);
      instr->_base.tag = FBLE_UNION_VALUE_INSTR;
      instr->_base.refcount = 1;
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
      instr->_base.refcount = 1;
      FbleVectorInit(arena, instr->values);
      FbleInstr** mkobj = FbleVectorExtend(arena, instr->values);
      *mkobj = NULL;

      FbleAccessInstr* access = FbleAlloc(arena, FbleAccessInstr);
      access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      access->_base.refcount = 1;
      instr->next = &access->_base;
      access->loc = access_expr->field.loc;
      Type* type = Compile(arena, &nvars, type_vars, access_expr->object, mkobj);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      // Note: We can safely pretend the type is always a struct, because
      // StructType and UnionType have the same structure.
      StructType* data_type = (StructType*)Normal(type);
      if (data_type->_base.tag == STRUCT_TYPE) {
        access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      } else if (data_type->_base.tag == UNION_TYPE) {
        access->_base.tag = FBLE_UNION_ACCESS_INSTR;
      } else {
        FbleReportError("expected value of type struct or union, but found value of type ", &access_expr->object->loc);
        PrintType(type);
        fprintf(stderr, "\n");

        TypeDropStrongRef(arena, type);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      for (size_t i = 0; i < data_type->fields.size; ++i) {
        if (FbleNamesEqual(access_expr->field.name, data_type->fields.xs[i].name.name)) {
          access->tag = i;
          *instrs = &instr->_base;
          Type* rtype = TypeTakeStrongRef(data_type->fields.xs[i].type);
          TypeDropStrongRef(arena, type);
          return rtype;
        }
      }

      FbleReportError("%s is not a field of type ", &access_expr->field.loc, access_expr->field.name);
      PrintType(type);
      fprintf(stderr, "\n");
      TypeDropStrongRef(arena, type);
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
      push->_base.refcount = 1;
      FbleVectorInit(arena, push->values);
      push->next = NULL;

      FbleInstr** mkobj = FbleVectorExtend(arena, push->values);
      *mkobj = NULL;
      Type* type = Compile(arena, &nvars, type_vars, cond_expr->condition, mkobj);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      UnionType* union_type = (UnionType*)Normal(type);
      if (union_type->_base.tag != UNION_TYPE) {
        FbleReportError("expected value of union type, but found value of type ", &cond_expr->condition->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        FbleFreeInstrs(arena, &push->_base);
        TypeDropStrongRef(arena, type);
        return NULL;
      }

      if (union_type->fields.size != cond_expr->choices.size) {
        FbleReportError("expected %d arguments, but %d were provided.\n", &cond_expr->_base.loc, union_type->fields.size, cond_expr->choices.size);
        FbleFreeInstrs(arena, &push->_base);
        TypeDropStrongRef(arena, type);
        return NULL;
      }

      FbleCondInstr* cond_instr = FbleAlloc(arena, FbleCondInstr);
      cond_instr->_base.tag = FBLE_COND_INSTR;
      cond_instr->_base.refcount = 1;
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
          return NULL;
        }

        FbleInstr* mkarg = NULL;
        Type* arg_type = Compile(arena, vars, type_vars, cond_expr->choices.xs[i].expr, &mkarg);
        Eval(arena, arg_type, NULL, NULL);
        if (arg_type == NULL) {
          TypeDropStrongRef(arena, return_type);
          TypeDropStrongRef(arena, type);
          FbleFreeInstrs(arena, &push->_base);
          return NULL;
        }
        FbleVectorAppend(arena, cond_instr->choices, mkarg);

        if (return_type == NULL) {
          return_type = arg_type;
        } else {
          if (!TypesEqual(return_type, arg_type, NULL)) {
            FbleReportError("expected type ", &cond_expr->choices.xs[i].expr->loc);
            PrintType(return_type);
            fprintf(stderr, ", but found ");
            PrintType(arg_type);
            fprintf(stderr, "\n");

            TypeDropStrongRef(arena, type);
            TypeDropStrongRef(arena, return_type);
            TypeDropStrongRef(arena, arg_type);
            FbleFreeInstrs(arena, &push->_base);
            return NULL;
          }
          TypeDropStrongRef(arena, arg_type);
        }
      }
      TypeDropStrongRef(arena, type);

      *instrs = &push->_base;
      return return_type;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      FuncType* type = FbleAlloc(arena, FuncType);
      type->_base.tag = FUNC_TYPE;
      type->_base.loc = expr->loc;
      type->_base.strong_ref_count = 1;
      type->_base.break_cycle_ref_count = 0;
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
      instr->_base.refcount = 1;
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
      push->_base.refcount = 1;
      FbleVectorInit(arena, push->values);
      push->next = NULL;

      FbleInstr** mkfunc = FbleVectorExtend(arena, push->values);
      *mkfunc = NULL;
      Type* type = Compile(arena, nvars, type_vars, apply_expr->func, mkfunc);
      Eval(arena, type, NULL, NULL);
      if (type == NULL) {
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      FuncType* func_type = (FuncType*)Normal(type);
      if (func_type->_base.tag != FUNC_TYPE) {
        FbleReportError("cannot perform application on an object of type ", &expr->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        TypeDropStrongRef(arena, type);
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
        Eval(arena, arg_type, NULL, NULL);
        error = error || (arg_type == NULL);
        if (arg_type != NULL && !TypesEqual(func_type->args.xs[i].type, arg_type, NULL)) {
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
        FbleFreeInstrs(arena, &push->_base);
        return NULL;
      }

      FbleFuncApplyInstr* apply_instr = FbleAlloc(arena, FbleFuncApplyInstr);
      apply_instr->_base.tag = FBLE_FUNC_APPLY_INSTR;
      apply_instr->_base.refcount = 1;
      apply_instr->argc = func_type->args.size;
      push->next = &apply_instr->_base;
      *instrs = &push->_base;
      Type* rtype = TypeTakeStrongRef(func_type->rtype);
      TypeDropStrongRef(arena, type);
      return rtype;
    }

    case FBLE_EVAL_EXPR: {
      FbleEvalExpr* eval_expr = (FbleEvalExpr*)expr;
      ProcType* type = FbleAlloc(arena, ProcType);
      type->_base.tag = PROC_TYPE;
      type->_base.loc = expr->loc;
      type->_base.strong_ref_count = 1;
      type->_base.break_cycle_ref_count = 0;

      FbleProcEvalInstr* instr = FbleAlloc(arena, FbleProcEvalInstr);
      instr->_base.tag = FBLE_PROC_EVAL_INSTR;
      instr->_base.refcount = 1;
      instr->body = NULL;

      type->rtype = Compile(arena, vars, type_vars, eval_expr->expr, &instr->body);
      if (type->rtype == NULL) {
        TypeDropStrongRef(arena, &type->_base);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }
      
      *instrs = &instr->_base;
      return &type->_base;
    }

    case FBLE_LINK_EXPR: {
      FbleLinkExpr* link_expr = (FbleLinkExpr*)expr;
      Type* port_type = CompileType(arena, type_vars, link_expr->type);
      if (port_type == NULL) {
        return NULL;
      }

      InputType* get_type = FbleAlloc(arena, InputType);
      get_type->_base.tag = INPUT_TYPE;
      get_type->_base.loc = port_type->loc;
      get_type->_base.strong_ref_count = 1;
      get_type->_base.break_cycle_ref_count = 0;
      get_type->rtype = TypeTakeStrongRef(port_type);
      Vars get_var = {
        .name = link_expr->get,
        .type = &get_type->_base,
        .next = vars
      };

      OutputType* put_type = FbleAlloc(arena, OutputType);
      put_type->_base.tag = OUTPUT_TYPE;
      put_type->_base.loc = port_type->loc;
      put_type->_base.strong_ref_count = 1;
      put_type->_base.break_cycle_ref_count = 0;
      put_type->rtype = TypeTakeStrongRef(port_type);
      Vars put_var = {
        .name = link_expr->put,
        .type = &put_type->_base,
        .next = &get_var
      };

      FbleProcLinkInstr* instr = FbleAlloc(arena, FbleProcLinkInstr);
      instr->_base.tag = FBLE_PROC_LINK_INSTR;
      instr->_base.refcount = 1;
      instr->body = NULL;

      Type* type = Compile(arena, &put_var, type_vars, link_expr->body, &instr->body);
      Eval(arena, type, NULL, NULL);

      TypeDropStrongRef(arena, port_type);
      TypeDropStrongRef(arena, &get_type->_base);
      TypeDropStrongRef(arena, &put_type->_base);

      if (type == NULL) {
        TypeDropStrongRef(arena, type);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      if (Normal(type)->tag != PROC_TYPE) {
        FbleReportError("expected a value of type proc, but found ", &type->loc);
        PrintType(type);
        fprintf(stderr, "\n");
        TypeDropStrongRef(arena, type);
        FbleFreeInstrs(arena, &instr->_base);
        return NULL;
      }

      *instrs = &instr->_base;
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
        nvd[i].type = CompileType(arena, type_vars, exec_expr->bindings.xs[i].type);
        Eval(arena, nvd[i].type, NULL, NULL);
        error = error || (nvd[i].type == NULL);
        nvd[i].next = nvars;
        nvars = nvd + i;
      }

      // Compile the proc values for computing the variables.
      FbleProcExecInstr* instr = FbleAlloc(arena, FbleProcExecInstr);
      instr->_base.tag = FBLE_PROC_EXEC_INSTR;
      instr->_base.refcount = 1;
      FbleVectorInit(arena, instr->bindings);
      instr->body = NULL;

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        FbleInstr* mkarg = NULL;
        Type* type = Compile(arena, vars, type_vars, exec_expr->bindings.xs[i].expr, &mkarg);
        Eval(arena, type, NULL, NULL);
        error = error || (type == NULL);
        FbleVectorAppend(arena, instr->bindings, mkarg);

        if (nvd[i].type != NULL && type != NULL && !TypesEqual(nvd[i].type, type, NULL)) {
          error = true;
          FbleReportError("expected type ", &exec_expr->bindings.xs[i].expr->loc);
          PrintType(nvd[i].type);
          fprintf(stderr, ", but found ");
          PrintType(type);
          fprintf(stderr, "\n");
        }
        TypeDropStrongRef(arena, type);
      }

      Type* rtype = NULL;
      if (!error) {
        rtype = Compile(arena, nvars, type_vars, exec_expr->body, &(instr->body));
        Eval(arena, rtype, NULL, NULL);
        error = (rtype == NULL);
      }

      if (rtype != NULL && Normal(rtype)->tag != PROC_TYPE) {
        error = true;
        FbleReportError("expected a value of type proc, but found ", &rtype->loc);
      }

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
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
      instr->_base.refcount = 1;
      instr->position = position;
      *instrs = &instr->_base;
      return TypeTakeStrongRef(vars->type);
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
        Eval(arena, nvd[i].type, NULL, NULL);
        error = error || (nvd[i].type == NULL);
        nvd[i].next = nvars;
        nvars = nvd + i;
      }

      // Compile the values of the variables.
      FbleLetInstr* instr = FbleAlloc(arena, FbleLetInstr);
      instr->_base.tag = FBLE_LET_INSTR;
      instr->_base.refcount = 1;
      FbleVectorInit(arena, instr->bindings);
      instr->body = NULL;
      instr->pop._base.tag = FBLE_POP_INSTR;
      instr->pop._base.refcount = 1;
      instr->pop.count = let_expr->bindings.size;
      instr->break_cycle._base.tag = FBLE_BREAK_CYCLE_INSTR;
      instr->break_cycle._base.refcount = 1;
      instr->break_cycle.count = let_expr->bindings.size;

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleInstr* mkval = NULL;
        Type* type = NULL;
        if (!error) {
          type = Compile(arena, nvars, type_vars, let_expr->bindings.xs[i].expr, &mkval);
          Eval(arena, type, NULL, NULL);
        }
        error = error || (type == NULL);
        FbleVectorAppend(arena, instr->bindings, mkval);

        if (!error && !TypesEqual(nvd[i].type, type, NULL)) {
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
      RefType* first = NULL;
      RefType* curr = NULL;
      for (size_t i = 0; i < let->bindings.size; ++i) {
        RefType* ref = FbleAlloc(arena, RefType);
        ref->_base.tag = REF_TYPE;
        ref->_base.loc = let->bindings.xs[i].name.loc;
        ref->_base.strong_ref_count = 1;
        ref->_base.break_cycle_ref_count = 0;
        ref->kind = CompileKind(arena, let->bindings.xs[i].kind);
        ref->value = NULL;
        ref->broke_cycle = false;
        ref->siblings = curr;

        ntvd[i].name = let->bindings.xs[i].name;
        ntvd[i].type = &ref->_base;
        ntvd[i].next = ntvs;
        ntvs = ntvd + i;

        if (first == NULL) {
          first = ref;
        }
        curr = ref;
      }
      assert(first != NULL);
      first->siblings = curr;

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
        RefType* ref = (RefType*)ntvd[i].type;
        assert(ref->_base.tag == REF_TYPE);
        ref->value = types[i];
        ntvd[i].type = TypeTakeStrongRef(ref->value);
        TypeBreakCycleRef(arena, ref->value);
        ref->broke_cycle = true;
        TypeDropStrongRef(arena, &ref->_base);
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
        arg->_base.break_cycle_ref_count = 0;
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
      pat->_base.break_cycle_ref_count = 0;
      FbleVectorInit(arena, pat->args);
      pat->result = NULL;

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
      st->_base.break_cycle_ref_count = 0;
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
      ut->_base.break_cycle_ref_count = 0;
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
      ft->_base.break_cycle_ref_count = 0;
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

    case FBLE_PROC_TYPE: {
      ProcType* pt = FbleAlloc(arena, ProcType);
      pt->_base.loc = type->loc;
      pt->_base.strong_ref_count = 1;
      pt->_base.break_cycle_ref_count = 0;
      pt->_base.tag = PROC_TYPE;
      pt->rtype = NULL;

      FbleProcType* proc_type = (FbleProcType*)type;
      pt->rtype = CompileType(arena, vars, proc_type->rtype);
      if (pt->rtype == NULL) {
        TypeDropStrongRef(arena, &pt->_base);
        return NULL;
      }
      return &pt->_base;
    }

    case FBLE_INPUT_TYPE: {
      InputType* pt = FbleAlloc(arena, InputType);
      pt->_base.loc = type->loc;
      pt->_base.strong_ref_count = 1;
      pt->_base.break_cycle_ref_count = 0;
      pt->_base.tag = INPUT_TYPE;
      pt->rtype = NULL;

      FbleInputType* input_type = (FbleInputType*)type;
      pt->rtype = CompileType(arena, vars, input_type->type);
      if (pt->rtype == NULL) {
        TypeDropStrongRef(arena, &pt->_base);
        return NULL;
      }
      return &pt->_base;
    }

    case FBLE_OUTPUT_TYPE: {
      OutputType* pt = FbleAlloc(arena, OutputType);
      pt->_base.loc = type->loc;
      pt->_base.strong_ref_count = 1;
      pt->_base.break_cycle_ref_count = 0;
      pt->_base.tag = OUTPUT_TYPE;
      pt->rtype = NULL;

      FbleOutputType* output_type = (FbleOutputType*)type;
      pt->rtype = CompileType(arena, vars, output_type->type);
      if (pt->rtype == NULL) {
        TypeDropStrongRef(arena, &pt->_base);
        return NULL;
      }
      return &pt->_base;
    }

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
      vt->_base.break_cycle_ref_count = 0;
      vt->_base.tag = VAR_TYPE;
      vt->var = var_type->var;
      vt->value = TypeTakeStrongRef(vars->type);
      return &vt->_base;
    }

    case FBLE_LET_TYPE: {
      FbleLetType* let = (FbleLetType*)type;

      Vars ntvd[let->bindings.size];
      Vars* ntvs = vars;

      // Set up reference types for all the bindings, to support recursive
      // types.
      RefType* first = NULL;
      RefType* curr = NULL;
      for (size_t i = 0; i < let->bindings.size; ++i) {
        RefType* ref = FbleAlloc(arena, RefType);
        ref->_base.tag = REF_TYPE;
        ref->_base.loc = let->bindings.xs[i].name.loc;
        ref->_base.strong_ref_count = 1;
        ref->_base.break_cycle_ref_count = 0;
        ref->kind = CompileKind(arena, let->bindings.xs[i].kind);
        ref->value = NULL;
        ref->broke_cycle = false;
        ref->siblings = curr;

        ntvd[i].name = let->bindings.xs[i].name;
        ntvd[i].type = &ref->_base;
        ntvd[i].next = ntvs;
        ntvs = ntvd + i;

        if (first == NULL) {
          first = ref;
        }
        curr = ref;
      }
      assert(first != NULL);
      first->siblings = curr;

      // Evaluate the type bindings.
      Type* types[let->bindings.size];
      bool error = false;
      for (size_t i = 0; i < let->bindings.size; ++i) {
        types[i] = CompileType(arena, ntvs, let->bindings.xs[i].type);
        error = error || (types[i] == NULL);

        if (types[i] != NULL) {
          // TODO: get expected_kind from ntvd[i]->kind rather than recompile?
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
      }

      for (size_t i = 0; i < let->bindings.size; ++i) {
        RefType* ref = (RefType*)ntvd[i].type;
        assert(ref->_base.tag == REF_TYPE);
        ref->value = types[i];
        ntvd[i].type = TypeTakeStrongRef(ref->value);
        TypeBreakCycleRef(arena, ref->value);
        ref->broke_cycle = true;
        TypeDropStrongRef(arena, &ref->_base);
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
      pt->_base.break_cycle_ref_count = 0;
      FbleVectorInit(arena, pt->args);

      Vars ntvd[poly->args.size];
      Vars* ntvs = vars;
      for (size_t i = 0; i < poly->args.size; ++i) {
        AbstractType* arg = FbleAlloc(arena, AbstractType);
        arg->_base.tag = ABSTRACT_TYPE;
        arg->_base.loc = poly->args.xs[i].name.loc;
        arg->_base.strong_ref_count = 1;
        arg->_base.break_cycle_ref_count = 0;
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
      pat->_base.break_cycle_ref_count = 0;
      FbleVectorInit(arena, pat->args);
      pat->result = NULL;

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

  assert(instrs->refcount > 0);
  instrs->refcount--;
  if (instrs->refcount == 0) {
    switch (instrs->tag) {
      case FBLE_VAR_INSTR:
      case FBLE_FUNC_APPLY_INSTR:
      case FBLE_STRUCT_ACCESS_INSTR:
      case FBLE_UNION_ACCESS_INSTR:
      case FBLE_PROC_INSTR:
      case FBLE_POP_INSTR:
      case FBLE_BREAK_CYCLE_INSTR: {
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

      case FBLE_PROC_EVAL_INSTR: {
        FbleProcEvalInstr* proc_eval_instr = (FbleProcEvalInstr*)instrs;
        FbleFreeInstrs(arena, proc_eval_instr->body);
        FbleFree(arena, proc_eval_instr);
        return;
      }

      case FBLE_PROC_LINK_INSTR: {
        FbleProcLinkInstr* proc_link_instr = (FbleProcLinkInstr*)instrs;
        FbleFreeInstrs(arena, proc_link_instr->body);
        FbleFree(arena, proc_link_instr);
        return;
      }

      case FBLE_PROC_EXEC_INSTR: {
        FbleProcExecInstr* exec_instr = (FbleProcExecInstr*)instrs;
        for (size_t i = 0; i < exec_instr->bindings.size; ++i) {
          FbleFreeInstrs(arena, exec_instr->bindings.xs[i]);
        }
        FbleFree(arena, exec_instr->bindings.xs);
        FbleFreeInstrs(arena, exec_instr->body);
        FbleFree(arena, exec_instr);
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
