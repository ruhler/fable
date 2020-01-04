// compile.c --
//   This file implements the fble compilation routines.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_arg, va_end
#include <stdio.h>    // for fprintf, stderr
#include <string.h>   // for strlen, strcat
#include <stdlib.h>   // for NULL

#include "internal.h"

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
//   BASIC_KIND (level :: size_t)
//
// levels
//   0: A normal, non-type value.
//   1: A normal type. A type of a level 0.
//   2: A type of a type of a value.
//   3: A type of a type of a type of a value.
//   etc.
typedef struct {
  Kind _base;
  size_t level;
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
//
//   The evaluating field is set to true if the type is currently being
//   evaluated. See the Eval function for more info.
//
// Design notes on types:
// * Instances of Type represent both unevaluated and evaluated versions of
//   the type. We use the unevaluated versions of the type when printing error
//   messages and as a stable reference to a type before and after evaluation.
// * Cycles are allowed in the Type data structure, to represent recursive
//   types. Every cycle is guaranteed to go through a Var type.
// * Types are evaluated as they are constructed.
// * TYPE_TYPE is handled specially: we propagate TYPE_TYPE up to the top of
//   the type during construction rather than save the unevaluated version of
//   a typeof.
typedef struct Type {
  FbleRef ref;
  TypeTag tag;
  FbleLoc loc;
  bool evaluating;
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

// ProcType -- PROC_TYPE
typedef struct {
  Type _base;
  Type* type;
} ProcType;

// PolyType -- POLY_TYPE
//
// We maintain an invariant when constructing PolyTypes that the body is not a
// TYPE_TYPE. For example: \a -> typeof(a) is constructed as typeof(\a -> a)
typedef struct {
  Type _base;
  Type* arg;
  Type* body;
} PolyType;

// PolyApplyType -- POLY_APPLY_TYPE
//   The 'result' field is the result of evaluating the poly apply type, or
//   NULL if it has not yet been evaluated.
//
// We maintain an invariant when constructing PolyApplyTypes that the poly is
// not a TYPE_TYPE. For example: (typeof(f) x) is constructed as typeof(f x).
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

// Var --
//   Information about a variable visible during type checking.
//
// name - the name of the variable.
// type - the type of the variable.
// instrs - a record of the var instructions used to access the variable.
typedef struct {
  FbleName name;
  Type* type;
  FbleVarInstrV instrs;
} Var;

// Var --
//   A vector of vars.
typedef struct {
  size_t size;
  Var* xs;
} VarV;

// Vars --
//   Scope of variables visible during type checking.
//
// Fields:
//   vars - The data for vars in scope.
//   nvars - the number of entries of vars that are valid.
typedef struct {
  VarV vars;
  size_t nvars;
} Vars;

static void ReportError(FbleArena* arena, FbleLoc* loc, const char* fmt, ...);

static Kind* CopyKind(FbleArena* arena, Kind* kind);
static void FreeKind(FbleArena* arena, Kind* kind);

static Type* TypeRetain(TypeArena* arena, Type* type);
static void TypeRelease(TypeArena* arena, Type* type);

static void TypeFree(TypeArena* arena, FbleRef* ref);
static void Add(FbleRefCallback* add, Type* type);
static void TypeAdded(FbleRefCallback* add, FbleRef* ref);

static Kind* TypeofKind(FbleArena* arena, Kind* kind);
static Kind* GetKind(FbleArena* arena, Type* type);
static size_t GetKindLevel(Kind* kind);
static bool HasParam(Type* type, Type* param, TypeList* visited);
static Type* Subst(TypeArena* arena, Type* src, Type* param, Type* arg, TypePairs* tps);
static void Eval(TypeArena* arena, Type* type, PolyApplyList* applied);
static Type* Normal(Type* type);
static bool TypesEqual(Type* a, Type* b, TypePairs* eq);
static bool KindsEqual(Kind* a, Kind* b);
static void PrintType(FbleArena* arena, Type* type);
static void PrintKind(Kind* type);

static Type* ValueOfType(TypeArena* arena, Type* typeof);
static Type* MakePolyType(TypeArena* arena, FbleLoc loc, Type* arg, Type* body);
static Type* MakePolyApplyType(TypeArena* arena, FbleLoc loc, Type* poly, Type* arg);

static void PushVar(FbleArena* arena, Vars* vars, FbleName name, Type* type);
static void PopVar(FbleArena* arena, Vars* vars);
static void FreeVars(FbleArena* arena, Vars* vars);

static void EnterThunk(FbleArena* arena, Vars* vars, Vars* thunk_vars);
static size_t ExitThunk(FbleArena* arena, Vars* vars, Vars* thunk_vars, FbleInstrV* instrs);

static char* MakeBlockName(FbleArena* arena, FbleNameV* name);
static FbleInstrBlock* NewInstrBlock(FbleArena* arena, FbleNameV* blocks, FbleNameV* name, FbleLoc block);
static void FreeInstr(FbleArena* arena, FbleInstr* instr);

static bool CheckNameSpace(TypeArena* arena, FbleName* name, Type* type);

static void CompileExit(FbleArena* arena, bool exit, FbleInstrV* instrs);
static Type* CompileExpr(TypeArena* arena, FbleNameV* blocks, FbleNameV* name, bool exit, Vars* vars, FbleExpr* expr, FbleInstrV* instrs, size_t* time);
static Type* CompileList(TypeArena* arena, FbleNameV* blocks, FbleNameV* name, bool exit, Vars* vars, FbleLoc loc, FbleType* type, FbleExprV args, FbleInstrV* instrs, size_t* time);
static Type* CompileExprNoInstrs(TypeArena* arena, Vars* vars, FbleExpr* expr);
static Type* CompileType(TypeArena* arena, Vars* vars, FbleType* type);
static Kind* CompileKind(FbleArena* arena, FbleKind* kind);
static Type* CompileProgram(TypeArena* arena, FbleNameV* blocks, FbleNameV* name, Vars* vars, FbleProgram* prgm, FbleInstrV* instrs, size_t* time);

// ReportError --
//   Report a compiler error.
//
//   This uses a printf-like format string. The following format specifiers
//   are supported:
//     %i - size_t
//     %k - Kind*
//     %n - FbleName*
//     %t - Type*
//   Please add additional format specifiers as needed.
//
// Inputs:
//   arena - arena to use for allocations.
//   loc - the location of the error.
//   fmt - the format string.
//   ... - The var-args for the associated conversion specifiers in fmt.
//
// Results:
//   none.
//
// Side effects:
//   Prints a message to stderr as described by fmt and provided arguments.
static void ReportError(FbleArena* arena, FbleLoc* loc, const char* fmt, ...)
{
  FbleReportError("", loc);

  va_list ap;
  va_start(ap, fmt);

  for (const char* p = strchr(fmt, '%'); p != NULL; p = strchr(fmt, '%')) {
    fprintf(stderr, "%.*s", p - fmt, fmt);

    switch (*(p + 1)) {
      case 'i': {
        size_t x = va_arg(ap, size_t);
        fprintf(stderr, "%d", x);
        break;
      }

      case 'k': {
        Kind* kind = va_arg(ap, Kind*);
        PrintKind(kind);
        break;
      }

      case 'n': {
        FbleName* name = va_arg(ap, FbleName*);
        FblePrintName(stderr, name);
        break;
      }

      case 't': {
        Type* type = va_arg(ap, Type*);
        PrintType(arena, type);
        break;
      }

      default: {
        UNREACHABLE("Unsupported format conversion.");
        break;
      }
    }
    fmt = p + 2;
  }
  fprintf(stderr, "%s", fmt);
  va_end(ap);
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
//   The free function for types. See documentation in ref.h
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

      Kind* arg_kind = GetKind(arena, type_type->type);
      Kind* kind = TypeofKind(arena, arg_kind);
      FreeKind(arena, arg_kind);
      return kind;
    }
  }

  UNREACHABLE("Should never get here");
  return NULL;
}

// GetKindLevel --
//   Returns the level of the fully applied version of this kind.
//
// Inputs:
//   kind - the kind to get the fully applied level of.
//
// Results:
//   The level of the kind after it has been fully applied.
//
// Side effects:
//   None.
size_t GetKindLevel(Kind* kind)
{
  switch (kind->tag) {
    case BASIC_KIND: {
      BasicKind* basic = (BasicKind*)kind;
      return basic->level;
    }

    case POLY_KIND: {
      PolyKind* poly = (PolyKind*)kind;
      return GetKindLevel(poly->rkind);
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
//   The caller is responsible for calling TypeRelease on the returned
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
      sst->_base.evaluating = false;

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
      sut->_base.evaluating = false;
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
      sft->_base.evaluating = false;

      sft->arg = Subst(arena, ft->arg, param, arg, tps);
      FbleRefAdd(arena, &sft->_base.ref, &sft->arg->ref);
      TypeRelease(arena, sft->arg);

      sft->rtype = Subst(arena, ft->rtype, param, arg, tps);
      FbleRefAdd(arena, &sft->_base.ref, &sft->rtype->ref);
      TypeRelease(arena, sft->rtype);
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
      TypeRelease(arena, sut->type);
      return &sut->_base;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      Type* body = Subst(arena, pt->body, param, arg, tps); 
      Type* spt = MakePolyType(arena, pt->_base.loc, pt->arg, body);
      TypeRelease(arena, body);
      return spt;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;

      Type* poly = Subst(arena, pat->poly, param, arg, tps);
      Type* sarg = Subst(arena, pat->arg, param, arg, tps);

      Type* spat = MakePolyApplyType(arena, pat->_base.loc, poly, sarg);
      TypeRelease(arena, poly);
      TypeRelease(arena, sarg);
      return spat;
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
      TypeRelease(arena, &svar->_base);
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
static void Eval(TypeArena* arena, Type* type, PolyApplyList* applied)
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
        if (TypesEqual(pal->poly, pat->poly, NULL)) {
          if (TypesEqual(pal->arg, pat->arg, NULL)) {
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

      PolyType* poly = (PolyType*)Normal(pat->poly);
      if (poly->_base.tag == POLY_TYPE) {
        pat->result = Subst(arena, poly->body, poly->arg, pat->arg, NULL);
        assert(pat->result != NULL);
        assert(&pat->_base != pat->result);
        FbleRefAdd(arena, &pat->_base.ref, &pat->result->ref);
        TypeRelease(arena, pat->result);

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

    case POLY_TYPE: {
      // Normalize: (\x -> f x) to f
      // TODO: Does this cover all the cases? It seems like overly specific
      // pattern matching.
      PolyType* poly = (PolyType*)type;
      PolyApplyType* pat = (PolyApplyType*)Normal(poly->body);
      if (pat->_base.tag == POLY_APPLY_TYPE) {
        if (poly->arg == Normal(pat->arg)) {
          return Normal(pat->poly);
        }
      }
      return type;
    }

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
      BasicKind* ba = (BasicKind*)a;
      BasicKind* bp = (BasicKind*)b;
      return ba->level == bp->level;
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
//
// Result:
//   None.
//
// Side effect:
//   Prints the given type in human readable form to stderr.
//
// Notes:
//   Human readable means we print var types using their name, without the
//   value associated with the variable. Because of this, we don't have to
//   worry about infinite recursion when trying to print a type: all recursion
//   must happen through a var type, and we don't ever go through a var type
//   when printing.
static void PrintType(FbleArena* arena, Type* type)
{
  switch (type->tag) {
    case STRUCT_TYPE: {
      StructType* st = (StructType*)type;
      fprintf(stderr, "*(");
      const char* comma = "";
      for (size_t i = 0; i < st->fields.size; ++i) {
        fprintf(stderr, "%s", comma);
        PrintType(arena, st->fields.xs[i].type);
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
        PrintType(arena, ut->fields.xs[i].type);
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
      PrintType(arena, ft->arg);
      fprintf(stderr, "){");
      PrintType(arena, ft->rtype);
      fprintf(stderr, ";}");
      break;
    }

    case PROC_TYPE: {
      ProcType* ut = (ProcType*)type;
      PrintType(arena, ut->type);
      fprintf(stderr, "!");
      break;
    }

    case POLY_TYPE: {
      PolyType* pt = (PolyType*)type;
      fprintf(stderr, "<");
      Kind* kind = GetKind(arena, pt->arg);
      PrintKind(kind);
      FreeKind(arena, kind);
      fprintf(stderr, " ");
      PrintType(arena, pt->arg);
      fprintf(stderr, "> { ");
      PrintType(arena, pt->body);
      fprintf(stderr, "; }");
      break;
    }

    case POLY_APPLY_TYPE: {
      PolyApplyType* pat = (PolyApplyType*)type;
      PrintType(arena, pat->poly);
      fprintf(stderr, "<");
      PrintType(arena, pat->arg);
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
      PrintType(arena, tt->type);
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
        PrintKind(poly->arg);
        prefix = ", ";
        kind = poly->rkind;
      }
      fprintf(stderr, ">");
      PrintKind(kind);
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
  if (typeof->tag == TYPE_TYPE) {
    TypeType* tt = (TypeType*)typeof;
    TypeRetain(arena, tt->type);
    return tt->type;
  }
  return NULL;
}

// MakePolyType --
//   Construct a PolyType. Maintains the invariant that poly of a typeof
//   should be constructed as a typeof a poly.
//
// Inputs:
//   arena - the arena to use for allocations.
//   loc - the location for the type.
//   arg - the poly arg.
//   body - the poly body.
//
// Results:
//   A type representing the poly type: \arg -> body.
//
// Side effects:
//   The caller is responsible for calling TypeRelease on the returned type
//   when it is no longer needed. This function does not take ownership of the
//   passed arg or body types.
static Type* MakePolyType(TypeArena* arena, FbleLoc loc, Type* arg, Type* body)
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
    tt->type = MakePolyType(arena, loc, arg, ttbody->type);
    FbleRefAdd(arena, &tt->_base.ref, &tt->type->ref);
    TypeRelease(arena, tt->type);
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

// MakePolyApplyType --
//   Construct a PolyApplyType. Maintains the invariant that poly apply of a
//   typeof should be constructed as a typeof a poly apply.
//
// Inputs:
//   arena - the arena to use for allocations.
//   loc - the location for the type.
//   poly - the poly apply poly.
//   arg - the poly apply arg.
//
// Results:
//   An unevaluated type representing the poly apply type: poly<arg>
//
// Side effects:
//   The caller is responsible for calling TypeRelease on the returned type
//   when it is no longer needed. This function does not take ownership of the
//   passed poly or arg types.
static Type* MakePolyApplyType(TypeArena* arena, FbleLoc loc, Type* poly, Type* arg)
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
    tt->type = MakePolyApplyType(arena, loc, ttpoly->type, arg);
    FbleRefAdd(arena, &tt->_base.ref, &tt->type->ref);
    TypeRelease(arena, tt->type);
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

// PushVar --
//   Push a variable onto the current scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the scope to push the variable on to.
//   name - the name of the variable.
//   type - the type of the variable.
//
// Results:
//   none
//
// Side effects:
//   Pushes a new variable with given name and type onto the scope. It is the
//   callers responsibility to ensure that the type stays alive as long as is
//   needed.
static void PushVar(FbleArena* arena, Vars* vars, FbleName name, Type* type)
{
  Var* var = vars->vars.xs + vars->nvars;
  if (vars->nvars == vars->vars.size) {
    var = FbleVectorExtend(arena, vars->vars);
    FbleVectorInit(arena, var->instrs);
  }
  var->name = name;
  var->type = type;
  vars->nvars++;
}

// PopVar --
//   Pops a var off the given scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the scope to pop from.
//
// Results:
//   none.
//
// Side effects:
//   Pops the top var off the scope.
static void PopVar(FbleArena* arena, Vars* vars)
{
  assert(vars->nvars > 0);
  vars->nvars--;
}

// FreeVars --
//   Free memory associated with vars.
//
// Inputs:
//   arena - arena for allocations.
//   vars - the vars to free memory for.
//
// Results:
//   none.
//
// Side effects:
//   Memory allocated for vars is freed.
static void FreeVars(FbleArena* arena, Vars* vars)
{
  for (size_t i = 0; i < vars->vars.size; ++i) {
    FbleFree(arena, vars->vars.xs[i].instrs.xs);
  }
  FbleFree(arena, vars->vars.xs);
}

// EnterThunk --
//   Prepare to capture variables for a thunk.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the current variable scope.
//   thunk_vars - a variable scope to use in the thunk.
//
// Results:
//   None.
//
// Side effects:
//   Initializes thunk_vars based on vars. ExitThunk or FreeVars should be
//   called to free the allocations for thunk_vars.
static void EnterThunk(FbleArena* arena, Vars* vars, Vars* thunk_vars)
{
  FbleVectorInit(arena, thunk_vars->vars);
  thunk_vars->nvars = 0;
  for (size_t i = 0; i < vars->nvars; ++i) {
    PushVar(arena, thunk_vars, vars->vars.xs[i].name, vars->vars.xs[i].type);
  }
}

// ExitThunk --
//   Generate instructions to capture just the variables accessed for a thunk.
//
// Inputs:
//   arena - arena to use for allocations
//   vars - the current variable scope.
//   thunk_vars - the thunk variable scope, created with EnterThunk.
//   instrs - vector of instructions to append new instructions to.
//
// Results:
//   The number of variables accessed in the thunk.
//
// Side effects:
//   * Frees memory associated with thunk_vars.
//   * Appends instructions to instrs to push captured variables to the data
//     stack.
//   * Updates thunk instructions to point to the captured variable indicies
//     instead of the original variable indicies.
static size_t ExitThunk(FbleArena* arena, Vars* vars, Vars* thunk_vars, FbleInstrV* instrs)
{
  size_t scopec = 0;
  for (size_t i = 0; i < vars->nvars; ++i) {
    if (thunk_vars->vars.xs[i].instrs.size > 0) {
      // Copy the accessed var to the data stack for capturing.
      FbleVarInstr* get_var = FbleAlloc(arena, FbleVarInstr);
      get_var->_base.tag = FBLE_VAR_INSTR;
      get_var->position = i;
      FbleVectorAppend(arena, vars->vars.xs[i].instrs, get_var);
      FbleVectorAppend(arena, *instrs, &get_var->_base);

      // Update references to this var.
      for (size_t j = 0; j < thunk_vars->vars.xs[i].instrs.size; ++j) {
        FbleVarInstr* var_instr = thunk_vars->vars.xs[i].instrs.xs[j];
        var_instr->position = scopec;
      }
      scopec++;
    }
  }

  // Update references to all vars first introduced in the thunk scope.
  for (size_t i = vars->nvars; i < thunk_vars->vars.size; ++i) {
      for (size_t j = 0; j < thunk_vars->vars.xs[i].instrs.size; ++j) {
        FbleVarInstr* var_instr = thunk_vars->vars.xs[i].instrs.xs[j];
        var_instr->position = i - vars->nvars + scopec;
      }
  }

  FreeVars(arena, thunk_vars);
  return scopec;
}

// MakeBlockName --
//   Compute the name for a profiling block, given a sequence of names
//   representing the current location.
//
// Inputs:
//   arena - arena to use for allocations.
//   name - sequence of names representing the current location
//
// Results:
//   A newly allocated name for the block.
//
// Side effects:
//   Allocates a string that should be freed with FbleFree when no longer
//   needed.
static char* MakeBlockName(FbleArena* arena, FbleNameV* name)
{
  size_t size = 1;
  for (size_t i = 0; i < name->size; ++i) {
    // + 1 for '.'
    // + 1 for namespace indicator
    size += strlen(name->xs[i].name) + 2;
  }

  char* str = FbleArrayAlloc(arena, char, size);
  str[0] = '\0';
  for (size_t i = 0; i < name->size; ++i) {
    if (i > 0) {
      strcat(str, ".");
    }
    strcat(str, name->xs[i].name);
    switch (name->xs[i].space) {
      case FBLE_NORMAL_NAME_SPACE: break;
      case FBLE_TYPE_NAME_SPACE: strcat(str, "@"); break;
      case FBLE_MODULE_NAME_SPACE: strcat(str, "%"); break;
    }
  }
  return str;
}

// NewInstrBlock --
//   Allocate and initialize a new instruction block.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the list of blocks in the program to populate.
//   name - a sequence of human readable names identifying the current block.
//   block - the source location of the block, used for profiling purposes.
//
// Results:
//   A newly allocated and initialized instruction block.
//
// Side effects:
//   Records the new block in the 'blocks' data structure.
static FbleInstrBlock* NewInstrBlock(FbleArena* arena, FbleNameV* blocks, FbleNameV* name, FbleLoc block)
{
  FbleInstrBlock* instr_block = FbleAlloc(arena, FbleInstrBlock);
  instr_block->refcount = 1;
  FbleVectorInit(arena, instr_block->instrs);

  FbleProfileEnterBlockInstr* enter = FbleAlloc(arena, FbleProfileEnterBlockInstr);
  enter->_base.tag = FBLE_PROFILE_ENTER_BLOCK_INSTR;
  enter->block = blocks->size;
  enter->time = 1;
  FbleVectorAppend(arena, instr_block->instrs, &enter->_base);

  FbleName* nm = FbleVectorExtend(arena, *blocks);
  nm->name = MakeBlockName(arena, name);
  nm->loc = block;
  return instr_block;
}

// FreeInstr --
//   Free the given instruction.
//
// Inputs:
//   arena - the arena used to allocation the instructions.
//   instr - the instruction to free.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the given instruction.
static void FreeInstr(FbleArena* arena, FbleInstr* instr)
{
  assert(instr != NULL);
  switch (instr->tag) {
    case FBLE_STRUCT_VALUE_INSTR:
    case FBLE_UNION_VALUE_INSTR:
    case FBLE_STRUCT_ACCESS_INSTR:
    case FBLE_UNION_ACCESS_INSTR:
    case FBLE_UNION_SELECT_INSTR:
    case FBLE_GOTO_INSTR:
    case FBLE_DESCOPE_INSTR:
    case FBLE_FUNC_APPLY_INSTR:
    case FBLE_VAR_INSTR:
    case FBLE_GET_INSTR:
    case FBLE_PUT_INSTR:
    case FBLE_LINK_INSTR:
    case FBLE_FORK_INSTR:
    case FBLE_PROC_INSTR:
    case FBLE_JOIN_INSTR:
    case FBLE_LET_PREP_INSTR:
    case FBLE_LET_DEF_INSTR:
    case FBLE_STRUCT_IMPORT_INSTR:
    case FBLE_EXIT_SCOPE_INSTR:
    case FBLE_TYPE_INSTR:
    case FBLE_VPUSH_INSTR:
    case FBLE_PROFILE_ENTER_BLOCK_INSTR:
    case FBLE_PROFILE_EXIT_BLOCK_INSTR:
    case FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR:
    {
      FbleFree(arena, instr);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
      FbleFreeInstrBlock(arena, func_value_instr->body);
      FbleFree(arena, func_value_instr);
      return;
    }

    case FBLE_PROC_VALUE_INSTR: {
      FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
      FbleFreeInstrBlock(arena, proc_value_instr->body);
      FbleFree(arena, proc_value_instr);
      return;
    }
  }

  UNREACHABLE("invalid instruction");
}

// CheckNameSpace --
//   Verify that the namespace of the given name is appropriate for the type
//   of value the name refers to.
//
// Inputs:
//   name - the name in question
//   type - the type of the value refered to be the name.
//
// Results:
//   true if the namespace of the name is consistent with the type. false
//   otherwise.
//
// Side effects:
//   Prints a message to stderr if the namespace and type don't match.
static bool CheckNameSpace(TypeArena* arena, FbleName* name, Type* type)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  Kind* kind = GetKind(arena_, type);
  size_t kind_level = GetKindLevel(kind);
  FreeKind(arena_, kind);

  if (name->space == FBLE_TYPE_NAME_SPACE && kind_level != 2) {
    ReportError(FbleRefArenaArena(arena), &name->loc,
        "expected a type type for field named '%n', but found type %t\n",
        name, type);
    return false;
  }

  if (name->space == FBLE_NORMAL_NAME_SPACE && kind_level != 1) {
    ReportError(FbleRefArenaArena(arena), &name->loc,
        "expected a normal type for field named '%n', but found type %t\n",
        name, type);
    return false;
  }

  return true;
}

// CompileExit --
//   If exit is true, append an exit scope instruction to instrs.
//
// Inputs:
//   arena - arena for allocations.
//   exit - whether we actually want to exit.
//   instrs - vector of instructions to append new instructions to.
//
// Results:
//   none.
//
// Side effects:
//   If exit is true, append an exit scope instruction to instrs
static void CompileExit(FbleArena* arena, bool exit, FbleInstrV* instrs)
{
  if (exit) {
    FbleExitScopeInstr* exit_scope = FbleAlloc(arena, FbleExitScopeInstr);
    exit_scope->_base.tag = FBLE_EXIT_SCOPE_INSTR;
    FbleVectorAppend(arena, *instrs, &exit_scope->_base);
  }
}

// CompileExpr --
//   Type check and compile the given expression. Returns the type of the
//   expression and generates instructions to compute the value of that
//   expression at runtime.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the vector of block locations to populate.
//   name - a sequence of names describing the current location in the code.
//          Used for naming profiling blocks.
//   exit - if true, generate instructions to exit the current scope when done.
//   vars - the list of variables in scope.
//   expr - the expression to compile.
//   instrs - vector of instructions to append new instructions to.
//   time - output var tracking the time required to execute the compiled block.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Appends blocks to 'blocks' with compiled block information.
//   Appends instructions to 'instrs' for executing the given expression.
//   There is no gaurentee about what instructions have been appended to
//   'instrs' if the expression fails to compile.
//   Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using
//   TypeRelease when it is no longer needed.
//   Increments 'time' by the amount of time required to execute this
//   expression.
static Type* CompileExpr(TypeArena* arena, FbleNameV* blocks, FbleNameV* name, bool exit, Vars* vars, FbleExpr* expr, FbleInstrV* instrs, size_t* time)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (expr->tag) {
    case FBLE_STRUCT_TYPE_EXPR:
    case FBLE_UNION_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR: {
      *time += 1;

      Type* type = CompileType(arena, vars, expr);
      if (type == NULL) {
        return NULL;
      }

      TypeType* type_type = FbleAlloc(arena_, TypeType);
      FbleRefInit(arena, &type_type->_base.ref);
      type_type->_base.tag = TYPE_TYPE;
      type_type->_base.loc = expr->loc;
      type_type->_base.evaluating = false;
      type_type->type = type;
      FbleRefAdd(arena, &type_type->_base.ref, &type_type->type->ref);
      TypeRelease(arena, type);

      FbleTypeInstr* instr = FbleAlloc(arena_, FbleTypeInstr);
      instr->_base.tag = FBLE_TYPE_INSTR;
      FbleVectorAppend(arena_, *instrs, &instr->_base);

      CompileExit(arena_, exit, instrs);
      return &type_type->_base;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleMiscApplyExpr* misc_apply_expr = (FbleMiscApplyExpr*)expr;
      bool error = false;

      size_t argc = misc_apply_expr->args.size;
      *time += 1 + argc;
      Type* arg_types[argc];
      for (size_t i = 0; i < argc; ++i) {
        size_t j = argc - 1 - i;
        arg_types[j] = CompileExpr(arena, blocks, name, false, vars, misc_apply_expr->args.xs[j], instrs, time);
        error = error || (arg_types[j] == NULL);
      }

      Type* type = CompileExpr(arena, blocks, name, false, vars, misc_apply_expr->misc, instrs, time);
      error = error || (type == NULL);

      if (error) {
        for (size_t i = 0; i < argc; ++i) {
          TypeRelease(arena, arg_types[i]);
        }
        TypeRelease(arena, type);
        return NULL;
      }

      Type* normal = Normal(type);
      switch (normal->tag) {
        case FUNC_TYPE: {
          // FUNC_APPLY
          for (size_t i = 0; i < argc; ++i) {
            if (normal->tag != FUNC_TYPE) {
              ReportError(arena_, &expr->loc, "too many arguments to function\n");
              for (size_t i = 0; i < argc; ++i) {
                TypeRelease(arena, arg_types[i]);
              }
              TypeRelease(arena, type);
              return NULL;
            }

            FuncType* func_type = (FuncType*)normal;
            if (!TypesEqual(func_type->arg, arg_types[i], NULL)) {
              ReportError(arena_, &misc_apply_expr->args.xs[i]->loc,
                  "expected type %t, but found %t\n",
                  func_type->arg, arg_types[i]);
              for (size_t j = 0; j < argc; ++j) {
                TypeRelease(arena, arg_types[j]);
              }
              TypeRelease(arena, type);
              return NULL;
            }

            FbleFuncApplyInstr* apply_instr = FbleAlloc(arena_, FbleFuncApplyInstr);
            apply_instr->_base.tag = FBLE_FUNC_APPLY_INSTR;
            apply_instr->loc = misc_apply_expr->misc->loc;
            apply_instr->exit = exit && (i+1 == argc);
            FbleVectorAppend(arena_, *instrs, &apply_instr->_base);

            normal = Normal(func_type->rtype);
          }

          for (size_t i = 0; i < argc; ++i) {
            TypeRelease(arena, arg_types[i]);
          }

          TypeRetain(arena, normal);
          TypeRelease(arena, type);
          return normal;
        }

        case TYPE_TYPE: {
          // FBLE_STRUCT_VALUE_EXPR
          TypeType* type_type = (TypeType*)normal;
          Type* vtype = TypeRetain(arena, type_type->type);
          TypeRelease(arena, type);

          StructType* struct_type = (StructType*)Normal(vtype);
          if (struct_type->_base.tag != STRUCT_TYPE) {
            ReportError(arena_, &misc_apply_expr->misc->loc,
                "expected a struct type, but found %t\n",
                vtype);
            for (size_t i = 0; i < argc; ++i) {
              TypeRelease(arena, arg_types[i]);
            }
            TypeRelease(arena, vtype);
            return NULL;
          }

          if (struct_type->fields.size != argc) {
            // TODO: Where should the error message go?
            ReportError(arena_, &expr->loc,
                "expected %i args, but %i were provided\n",
                 struct_type->fields.size, argc);
            for (size_t i = 0; i < argc; ++i) {
              TypeRelease(arena, arg_types[i]);
            }
            TypeRelease(arena, vtype);
            return NULL;
          }

          bool error = false;
          for (size_t i = 0; i < argc; ++i) {
            Field* field = struct_type->fields.xs + i;

            if (!TypesEqual(field->type, arg_types[i], NULL)) {
              ReportError(arena_, &misc_apply_expr->args.xs[i]->loc,
                  "expected type %t, but found %t\n",
                  field->type, arg_types[i]);
              error = true;
            }
            TypeRelease(arena, arg_types[i]);
          }

          if (error) {
            TypeRelease(arena, vtype);
            return NULL;
          }

          FbleStructValueInstr* struct_instr = FbleAlloc(arena_, FbleStructValueInstr);
          struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
          struct_instr->argc = struct_type->fields.size;
          FbleVectorAppend(arena_, *instrs, &struct_instr->_base);
          CompileExit(arena_, exit, instrs);
          return vtype;
        }

        default: {
          ReportError(arena_, &expr->loc,
              "expecting a function or struct type, but found something of type %t\n",
              type);
          for (size_t i = 0; i < argc; ++i) {
            TypeRelease(arena, arg_types[i]);
          }
          TypeRelease(arena, type);
          return NULL;
        }
      }

      UNREACHABLE("Should never get here");
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      *time += 1;

      FbleStructValueImplicitTypeExpr* struct_expr = (FbleStructValueImplicitTypeExpr*)expr;
      StructType* struct_type = FbleAlloc(arena_, StructType);
      FbleRefInit(arena, &struct_type->_base.ref);
      struct_type->_base.tag = STRUCT_TYPE;
      struct_type->_base.loc = expr->loc;
      struct_type->_base.evaluating = false;
      FbleVectorInit(arena_, struct_type->fields);

      size_t argc = struct_expr->args.size;
      Type* arg_types[argc];
      bool error = false;
      for (size_t i = 0; i < argc; ++i) {
        size_t j = argc - i - 1;
        FbleTaggedExpr* arg = struct_expr->args.xs + j;
        FbleVectorAppend(arena_, *name, arg->name);
        arg_types[j] = CompileExpr(arena, blocks, name, false, vars, arg->expr, instrs, time);
        name->size--;
        error = error || (arg_types[j] == NULL);
      }

      for (size_t i = 0; i < argc; ++i) {
        FbleTaggedExpr* arg = struct_expr->args.xs + i;
        if (arg_types[i] != NULL) {
          if (!CheckNameSpace(arena, &arg->name, arg_types[i])) {
            error = true;
          }

          Field* cfield = FbleVectorExtend(arena_, struct_type->fields);
          cfield->name = arg->name;
          cfield->type = arg_types[i];
          FbleRefAdd(arena, &struct_type->_base.ref, &cfield->type->ref);
          TypeRelease(arena, arg_types[i]);
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&arg->name, &struct_expr->args.xs[j].name)) {
            error = true;
            ReportError(arena_, &arg->name.loc,
                "duplicate field name '%n'\n",
                &struct_expr->args.xs[j].name);
          }
        }
      }

      if (error) {
        TypeRelease(arena, &struct_type->_base);
        return NULL;
      }

      FbleTypeInstr* instr = FbleAlloc(arena_, FbleTypeInstr);
      instr->_base.tag = FBLE_TYPE_INSTR;
      FbleVectorAppend(arena_, *instrs, &instr->_base);

      FbleStructValueInstr* struct_instr = FbleAlloc(arena_, FbleStructValueInstr);
      struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      struct_instr->argc = struct_expr->args.size;
      FbleVectorAppend(arena_, *instrs, &struct_instr->_base);
      CompileExit(arena_, exit, instrs);
      return &struct_type->_base;
    }

    case FBLE_UNION_VALUE_EXPR: {
      *time += 1;
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      Type* type = CompileType(arena, vars, union_value_expr->type);
      if (type == NULL) {
        return NULL;
      }

      UnionType* union_type = (UnionType*)Normal(type);
      if (union_type->_base.tag != UNION_TYPE) {
        ReportError(arena_, &union_value_expr->type->loc,
            "expected a union type, but found %t\n", type);
        TypeRelease(arena, type);
        return NULL;
      }

      Type* field_type = NULL;
      size_t tag = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        Field* field = union_type->fields.xs + i;
        if (FbleNamesEqual(&field->name, &union_value_expr->field)) {
          tag = i;
          field_type = field->type;
          break;
        }
      }

      if (field_type == NULL) {
        ReportError(arena_, &union_value_expr->field.loc,
            "'%n' is not a field of type %t\n",
            &union_value_expr->field, type);
        TypeRelease(arena, type);
        return NULL;
      }

      Type* arg_type = CompileExpr(arena, blocks, name, false, vars, union_value_expr->arg, instrs, time);
      if (arg_type == NULL) {
        TypeRelease(arena, type);
        return NULL;
      }

      if (!TypesEqual(field_type, arg_type, NULL)) {
        ReportError(arena_, &union_value_expr->arg->loc,
            "expected type %t, but found type %t\n",
            field_type, arg_type);
        TypeRelease(arena, type);
        TypeRelease(arena, arg_type);
        return NULL;
      }
      TypeRelease(arena, arg_type);

      FbleUnionValueInstr* union_instr = FbleAlloc(arena_, FbleUnionValueInstr);
      union_instr->_base.tag = FBLE_UNION_VALUE_INSTR;
      union_instr->tag = tag;
      FbleVectorAppend(arena_, *instrs, &union_instr->_base);
      CompileExit(arena_, exit, instrs);
      return type;
    }

    case FBLE_MISC_ACCESS_EXPR: {
      // TODO: Should time be O(lg(N)) instead of O(1), where N is the number
      // of fields in the union/struct?
      *time += 1;

      FbleMiscAccessExpr* access_expr = (FbleMiscAccessExpr*)expr;

      Type* type = CompileExpr(arena, blocks, name, false, vars, access_expr->object, instrs, time);
      if (type == NULL) {
        return NULL;
      }

      FbleAccessInstr* access = FbleAlloc(arena_, FbleAccessInstr);
      access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      access->loc = access_expr->field.loc;
      FbleVectorAppend(arena_, *instrs, &access->_base);

      CompileExit(arena_, exit, instrs);

      Type* normal = Normal(type);
      FieldV* fields = NULL;
      if (normal->tag == STRUCT_TYPE) {
        access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
        fields = &((StructType*)normal)->fields;
      } else if (normal->tag == UNION_TYPE) {
        access->_base.tag = FBLE_UNION_ACCESS_INSTR;
        fields = &((UnionType*)normal)->fields;
      } else {
        ReportError(arena_, &access_expr->object->loc,
            "expected value of type struct or union, but found value of type %t\n",
            type);

        TypeRelease(arena, type);
        return NULL;
      }

      for (size_t i = 0; i < fields->size; ++i) {
        if (FbleNamesEqual(&access_expr->field, &fields->xs[i].name)) {
          access->tag = i;
          Type* rtype = TypeRetain(arena, fields->xs[i].type);
          TypeRelease(arena, type);
          return rtype;
        }
      }

      ReportError(arena_, &access_expr->field.loc,
          "'%n' is not a field of type %t\n",
          &access_expr->field, type);
      TypeRelease(arena, type);
      return NULL;
    }

    case FBLE_UNION_SELECT_EXPR: {
      // TODO: Should time be O(lg(N)) instead of O(1), where N is the number
      // of fields in the union/struct?
      *time += 1;

      FbleUnionSelectExpr* select_expr = (FbleUnionSelectExpr*)expr;

      Type* type = CompileExpr(arena, blocks, name, false, vars, select_expr->condition, instrs, time);
      if (type == NULL) {
        return NULL;
      }

      UnionType* union_type = (UnionType*)Normal(type);
      if (union_type->_base.tag != UNION_TYPE) {
        ReportError(arena_, &select_expr->condition->loc,
            "expected value of union type, but found value of type %t\n",
            type);
        TypeRelease(arena, type);
        return NULL;
      }

      if (exit) {
        FbleProfileAutoExitBlockInstr* exit_instr = FbleAlloc(arena_, FbleProfileAutoExitBlockInstr);
        exit_instr->_base.tag = FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR;
        FbleVectorAppend(arena_, *instrs, &exit_instr->_base);
      }

      FbleUnionSelectInstr* select_instr = FbleAlloc(arena_, FbleUnionSelectInstr);
      select_instr->_base.tag = FBLE_UNION_SELECT_INSTR;
      select_instr->loc = select_expr->condition->loc;
      FbleVectorAppend(arena_, *instrs, &select_instr->_base);

      Type* return_type = NULL;
      FbleGotoInstr* enter_gotos[union_type->fields.size];
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        enter_gotos[i] = FbleAlloc(arena_, FbleGotoInstr);
        enter_gotos[i]->_base.tag = FBLE_GOTO_INSTR;
        FbleVectorAppend(arena_, *instrs, &enter_gotos[i]->_base);
      }

      size_t default_pc = instrs->size;
      FbleGotoInstr* exit_goto_default = NULL;
      if (select_expr->default_ != NULL) {
        return_type = CompileExpr(arena, blocks, name, exit, vars, select_expr->default_, instrs, time);
        if (return_type == NULL) {
          TypeRelease(arena, type);
          return NULL;
        }

        if (!exit) {
          exit_goto_default = FbleAlloc(arena_, FbleGotoInstr);
          exit_goto_default->_base.tag = FBLE_GOTO_INSTR;
          FbleVectorAppend(arena_, *instrs, &exit_goto_default->_base);
        }
      }

      FbleGotoInstr* exit_gotos[select_expr->choices.size];
      size_t choice = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        if (choice < select_expr->choices.size && FbleNamesEqual(&select_expr->choices.xs[choice].name, &union_type->fields.xs[i].name)) {
          enter_gotos[i]->pc = instrs->size;

          FbleVectorAppend(arena_, *name, select_expr->choices.xs[choice].name);

          FbleProfileEnterBlockInstr* enter = FbleAlloc(arena_, FbleProfileEnterBlockInstr);
          enter->_base.tag = FBLE_PROFILE_ENTER_BLOCK_INSTR;
          enter->block = blocks->size;
          enter->time = 1;
          FbleVectorAppend(arena_, *instrs, &enter->_base);
          FbleName* nm = FbleVectorExtend(arena_, *blocks);
          nm->name = MakeBlockName(arena_, name);
          nm->loc = select_expr->choices.xs[choice].expr->loc;

          Type* arg_type = CompileExpr(arena, blocks, name, exit, vars, select_expr->choices.xs[choice].expr, instrs, time);
          name->size--;

          if (!exit) {
            FbleProfileExitBlockInstr* exit_instr = FbleAlloc(arena_, FbleProfileExitBlockInstr);
            exit_instr->_base.tag = FBLE_PROFILE_EXIT_BLOCK_INSTR;
            FbleVectorAppend(arena_, *instrs, &exit_instr->_base);

            exit_gotos[choice] = FbleAlloc(arena_, FbleGotoInstr);
            exit_gotos[choice]->_base.tag = FBLE_GOTO_INSTR;
            FbleVectorAppend(arena_, *instrs, &exit_gotos[choice]->_base);
          }

          if (arg_type == NULL) {
            TypeRelease(arena, return_type);
            TypeRelease(arena, type);
            return NULL;
          }

          if (return_type == NULL) {
            return_type = arg_type;
          } else {
            if (!TypesEqual(return_type, arg_type, NULL)) {
              ReportError(arena_, &select_expr->choices.xs[choice].expr->loc,
                  "expected type %t, but found %t\n",
                  return_type, arg_type);

              TypeRelease(arena, type);
              TypeRelease(arena, return_type);
              TypeRelease(arena, arg_type);
              return NULL;
            }
            TypeRelease(arena, arg_type);
          }
          choice++;
        } else if (select_expr->default_ == NULL) {
          if (choice < select_expr->choices.size) {
            ReportError(arena_, &select_expr->choices.xs[choice].name.loc,
                "expected tag '%n', but found '%n'\n",
                &union_type->fields.xs[i].name, &select_expr->choices.xs[choice].name);
          } else {
            ReportError(arena_, &expr->loc,
                "missing tag '%n'\n",
                &union_type->fields.xs[i].name);
          }
          TypeRelease(arena, return_type);
          TypeRelease(arena, type);
          return NULL;
        } else {
          enter_gotos[i]->pc = default_pc;
        }
      }
      TypeRelease(arena, type);

      if (choice < select_expr->choices.size) {
        ReportError(arena_, &select_expr->choices.xs[choice].name.loc,
            "unexpected tag '%n'\n",
            &select_expr->choices.xs[choice]);
        TypeRelease(arena, return_type);
        return NULL;
      }

      if (!exit) {
        if (exit_goto_default != NULL) {
          exit_goto_default->pc = instrs->size;
        }
        for (size_t i = 0; i < select_expr->choices.size; ++i) {
          exit_gotos[i]->pc = instrs->size;
        }
      }
      return return_type;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      size_t argc = func_value_expr->args.size;

      bool error = false;
      Type* arg_types[argc];
      for (size_t i = 0; i < argc; ++i) {
        arg_types[i] = CompileType(arena, vars, func_value_expr->args.xs[i].type);
        error = error || arg_types[i] == NULL;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&func_value_expr->args.xs[i].name, &func_value_expr->args.xs[j].name)) {
            error = true;
            ReportError(arena_, &func_value_expr->args.xs[i].name.loc,
                "duplicate arg name '%n'\n",
                &func_value_expr->args.xs[i].name);
          }
        }
      }

      if (error) {
        for (size_t i = 0; i < argc; ++i) {
          TypeRelease(arena, arg_types[i]);
        }
        return NULL;
      }

      FbleFuncValueInstr* instr = FbleAlloc(arena_, FbleFuncValueInstr);
      instr->_base.tag = FBLE_FUNC_VALUE_INSTR;
      instr->body = NewInstrBlock(arena_, blocks, name, func_value_expr->body->loc);
      instr->argc = argc;

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &vpush->_base);

      Vars thunk_vars;
      EnterThunk(arena_, vars, &thunk_vars);

      for (size_t i = 0; i < argc; ++i) {
        PushVar(arena_, &thunk_vars, func_value_expr->args.xs[i].name, arg_types[i]);
      }

      assert(instr->body->instrs.xs[0]->tag == FBLE_PROFILE_ENTER_BLOCK_INSTR);
      size_t* body_time = &((FbleProfileEnterBlockInstr*)instr->body->instrs.xs[0])->time;

      Type* type = CompileExpr(arena, blocks, name, true, &thunk_vars, func_value_expr->body, &instr->body->instrs, body_time);
      if (type == NULL) {
        FreeVars(arena_, &thunk_vars);
        FreeInstr(arena_, &instr->_base);
        for (size_t i = 0; i < argc; ++i) {
          TypeRelease(arena, arg_types[i]);
        }
        return NULL;
      }

      instr->scopec = ExitThunk(arena_, vars, &thunk_vars, instrs);

      // TODO: Is it right for time to be proportional to number of captured
      // variables?
      *time += instr->scopec;
      vpush->count = instr->scopec + argc;
      FbleVectorAppend(arena_, *instrs, &instr->_base);

      CompileExit(arena_, exit, instrs);

      for (size_t i = 0; i < argc; ++i) {
        Type* arg_type = arg_types[argc - 1 - i];
        FuncType* ft = FbleAlloc(arena_, FuncType);
        FbleRefInit(arena, &ft->_base.ref);
        ft->_base.tag = FUNC_TYPE;
        ft->_base.loc = expr->loc;
        ft->_base.evaluating = false;
        ft->arg = arg_type;
        ft->rtype = type;

        FbleRefAdd(arena, &ft->_base.ref, &ft->arg->ref);
        TypeRelease(arena, ft->arg);

        FbleRefAdd(arena, &ft->_base.ref, &ft->rtype->ref);
        TypeRelease(arena, ft->rtype);
        type = &ft->_base;
      }

      return type;
    }

    case FBLE_EVAL_EXPR: {
      *time += 1;
      FbleEvalExpr* eval_expr = (FbleEvalExpr*)expr;

      Vars thunk_vars;
      EnterThunk(arena_, vars, &thunk_vars);

      FbleProcValueInstr* instr = FbleAlloc(arena_, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->body = NewInstrBlock(arena_, blocks, name, expr->loc);

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &vpush->_base);

      assert(instr->body->instrs.xs[0]->tag == FBLE_PROFILE_ENTER_BLOCK_INSTR);
      size_t* body_time = &((FbleProfileEnterBlockInstr*)instr->body->instrs.xs[0])->time;
      Type* type = CompileExpr(arena, blocks, name, false, &thunk_vars, eval_expr->body, &instr->body->instrs, body_time);

      CompileExit(arena_, true, &instr->body->instrs);

      instr->scopec = ExitThunk(arena_, vars, &thunk_vars, instrs);
      vpush->count = instr->scopec;
      FbleVectorAppend(arena_, *instrs, &instr->_base);
      CompileExit(arena_, exit, instrs);

      if (type == NULL) {
        return NULL;
      }

      ProcType* proc_type = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &proc_type->_base.ref);
      proc_type->_base.tag = PROC_TYPE;
      proc_type->_base.loc = expr->loc;
      proc_type->_base.evaluating = false;
      proc_type->type = type;
      FbleRefAdd(arena, &proc_type->_base.ref, &proc_type->type->ref);
      TypeRelease(arena, proc_type->type);
      return &proc_type->_base;
    }

    case FBLE_LINK_EXPR: {
      *time += 1;
      FbleLinkExpr* link_expr = (FbleLinkExpr*)expr;
      if (FbleNamesEqual(&link_expr->get, &link_expr->put)) {
        ReportError(arena_, &link_expr->put.loc,
            "duplicate port name '%n'\n",
            &link_expr->put);
        return NULL;
      }

      Type* port_type = CompileType(arena, vars, link_expr->type);
      if (port_type == NULL) {
        return NULL;
      }

      ProcType* get_type = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &get_type->_base.ref);
      get_type->_base.tag = PROC_TYPE;
      get_type->_base.loc = port_type->loc;
      get_type->_base.evaluating = false;
      get_type->type = port_type;
      FbleRefAdd(arena, &get_type->_base.ref, &get_type->type->ref);

      StructType* unit_type = FbleAlloc(arena_, StructType);
      FbleRefInit(arena, &unit_type->_base.ref);
      unit_type->_base.tag = STRUCT_TYPE;
      unit_type->_base.loc = expr->loc;
      unit_type->_base.evaluating = false;
      FbleVectorInit(arena_, unit_type->fields);

      ProcType* unit_proc_type = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &unit_proc_type->_base.ref);
      unit_proc_type->_base.tag = PROC_TYPE;
      unit_proc_type->_base.loc = expr->loc;
      unit_proc_type->_base.evaluating = false;
      unit_proc_type->type = &unit_type->_base;
      FbleRefAdd(arena, &unit_proc_type->_base.ref, &unit_proc_type->type->ref);
      TypeRelease(arena, &unit_type->_base);

      FuncType* put_type = FbleAlloc(arena_, FuncType);
      FbleRefInit(arena, &put_type->_base.ref);
      put_type->_base.tag = FUNC_TYPE;
      put_type->_base.loc = expr->loc;
      put_type->_base.evaluating = false;
      put_type->arg = port_type;
      FbleRefAdd(arena, &put_type->_base.ref, &put_type->arg->ref);
      put_type->rtype = &unit_proc_type->_base;
      FbleRefAdd(arena, &put_type->_base.ref, &put_type->rtype->ref);
      TypeRelease(arena, &unit_proc_type->_base);

      Vars thunk_vars;
      EnterThunk(arena_, vars, &thunk_vars);

      FbleProcValueInstr* instr = FbleAlloc(arena_, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->body = NewInstrBlock(arena_, blocks, name, link_expr->body->loc);

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &vpush->_base);

      FbleLinkInstr* link = FbleAlloc(arena_, FbleLinkInstr);
      link->_base.tag = FBLE_LINK_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &link->_base);

      FbleVPushInstr* vpush_ports = FbleAlloc(arena_, FbleVPushInstr);
      vpush_ports->_base.tag = FBLE_VPUSH_INSTR;
      vpush_ports->count = 2;
      FbleVectorAppend(arena_, instr->body->instrs, &vpush_ports->_base);
      PushVar(arena_, &thunk_vars, link_expr->get, &get_type->_base);
      PushVar(arena_, &thunk_vars, link_expr->put, &put_type->_base);

      assert(instr->body->instrs.xs[0]->tag == FBLE_PROFILE_ENTER_BLOCK_INSTR);
      size_t* body_time = &((FbleProfileEnterBlockInstr*)instr->body->instrs.xs[0])->time;

      Type* type = CompileExpr(arena, blocks, name, false, &thunk_vars, link_expr->body, &instr->body->instrs, body_time);

      FbleProcInstr* proc = FbleAlloc(arena_, FbleProcInstr);
      proc->_base.tag = FBLE_PROC_INSTR;
      proc->exit = true;
      FbleVectorAppend(arena_, instr->body->instrs, &proc->_base);

      instr->scopec = ExitThunk(arena_, vars, &thunk_vars, instrs);
      vpush->count = instr->scopec;
      FbleVectorAppend(arena_, *instrs, &instr->_base);
      CompileExit(arena_, exit, instrs);

      TypeRelease(arena, port_type);
      TypeRelease(arena, &get_type->_base);
      TypeRelease(arena, &put_type->_base);

      if (type == NULL) {
        TypeRelease(arena, type);
        return NULL;
      }

      if (Normal(type)->tag != PROC_TYPE) {
        ReportError(arena_, &link_expr->body->loc,
            "expected a value of type proc, but found %t\n",
            type);
        TypeRelease(arena, type);
        return NULL;
      }
      return type;
    }

    case FBLE_EXEC_EXPR: {
      FbleExecExpr* exec_expr = (FbleExecExpr*)expr;
      bool error = false;

      *time += 1 + exec_expr->bindings.size;

      // Evaluate the types of the bindings and set up the new vars.
      Var nvd[exec_expr->bindings.size];
      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        nvd[i].name = exec_expr->bindings.xs[i].name;
        nvd[i].type = CompileType(arena, vars, exec_expr->bindings.xs[i].type);
        error = error || (nvd[i].type == NULL);
      }

      Vars thunk_vars;
      EnterThunk(arena_, vars, &thunk_vars);

      FbleProcValueInstr* instr = FbleAlloc(arena_, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->body = NewInstrBlock(arena_, blocks, name, exec_expr->body->loc);

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &vpush->_base);

      FbleForkInstr* fork = FbleAlloc(arena_, FbleForkInstr);
      fork->_base.tag = FBLE_FORK_INSTR;
      fork->argc = exec_expr->bindings.size;
      FbleVectorAppend(arena_, instr->body->instrs, &fork->_base);

      FbleJoinInstr* join = FbleAlloc(arena_, FbleJoinInstr);
      join->_base.tag = FBLE_JOIN_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &join->_base);

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        PushVar(arena_, &thunk_vars, nvd[i].name, nvd[i].type);
      }

      Type* rtype = NULL;
      if (!error) {
        assert(instr->body->instrs.xs[0]->tag == FBLE_PROFILE_ENTER_BLOCK_INSTR);
        size_t* body_time = &((FbleProfileEnterBlockInstr*)instr->body->instrs.xs[0])->time;

        rtype = CompileExpr(arena, blocks, name, false, &thunk_vars, exec_expr->body, &instr->body->instrs, body_time);
        error = (rtype == NULL);
      }

      if (rtype != NULL && Normal(rtype)->tag != PROC_TYPE) {
        error = true;
        ReportError(arena_, &exec_expr->body->loc,
            "expected a value of type proc, but found %t\n",
            rtype);
      }

      FbleProcInstr* proc = FbleAlloc(arena_, FbleProcInstr);
      proc->_base.tag = FBLE_PROC_INSTR;
      proc->exit = true;
      FbleVectorAppend(arena_, instr->body->instrs, &proc->_base);

      size_t captured = ExitThunk(arena_, vars, &thunk_vars, instrs);
      instr->scopec = captured + exec_expr->bindings.size;
      vpush->count = captured;

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {

        Vars bthunk_vars;
        EnterThunk(arena_, vars, &bthunk_vars);

        FbleProcValueInstr* binstr = FbleAlloc(arena_, FbleProcValueInstr);
        binstr->_base.tag = FBLE_PROC_VALUE_INSTR;
        binstr->body = NewInstrBlock(arena_, blocks, name, exec_expr->bindings.xs[i].expr->loc);

        FbleVPushInstr* bvpush = FbleAlloc(arena_, FbleVPushInstr);
        bvpush->_base.tag = FBLE_VPUSH_INSTR;
        FbleVectorAppend(arena_, binstr->body->instrs, &bvpush->_base);

        assert(binstr->body->instrs.xs[0]->tag == FBLE_PROFILE_ENTER_BLOCK_INSTR);
        size_t* bbody_time = &((FbleProfileEnterBlockInstr*)binstr->body->instrs.xs[0])->time;
        Type* type = CompileExpr(arena, blocks, name, false, &bthunk_vars, exec_expr->bindings.xs[i].expr, &binstr->body->instrs, bbody_time);

        FbleProcInstr* bproc = FbleAlloc(arena_, FbleProcInstr);
        bproc->_base.tag = FBLE_PROC_INSTR;
        bproc->exit = true;
        FbleVectorAppend(arena_, binstr->body->instrs, &bproc->_base);

        binstr->scopec = ExitThunk(arena_, vars, &bthunk_vars, instrs);
        bvpush->count = binstr->scopec;
        FbleVectorAppend(arena_, *instrs, &binstr->_base);

        error = error || (type == NULL);

        if (type != NULL) {
          ProcType* proc_type = (ProcType*)Normal(type);
          if (proc_type->_base.tag == PROC_TYPE) {
            if (nvd[i].type != NULL && !TypesEqual(nvd[i].type, proc_type->type, NULL)) {
              error = true;
              ReportError(arena_, &exec_expr->bindings.xs[i].expr->loc,
                  "expected type %t!, but found %t\n",
                  nvd[i].type, type);
            }
          } else {
            error = true;
            ReportError(arena_, &exec_expr->bindings.xs[i].expr->loc,
                "expected process, but found expression of type %t\n",
                type);
          }
          TypeRelease(arena, type);
        }
        TypeRelease(arena, nvd[i].type);
      }

      FbleVectorAppend(arena_, *instrs, &instr->_base);
      CompileExit(arena_, exit, instrs);

      if (error) {
        TypeRelease(arena, rtype);
        return NULL;
      }

      return rtype;
    }

    case FBLE_VAR_EXPR: {
      *time += 1;
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;

      for (size_t i = 0; i < vars->nvars; ++i) {
        size_t j = vars->nvars - i - 1;
        Var* var = vars->vars.xs + j;
        if (FbleNamesEqual(&var_expr->var, &var->name)) {
          FbleVarInstr* instr = FbleAlloc(arena_, FbleVarInstr);
          instr->_base.tag = FBLE_VAR_INSTR;
          instr->position = j;
          FbleVectorAppend(arena_, vars->vars.xs[j].instrs, instr);
          FbleVectorAppend(arena_, *instrs, &instr->_base);
          CompileExit(arena_, exit, instrs);
          return TypeRetain(arena, var->type);
        }
      }

      ReportError(arena_, &var_expr->var.loc,
          "variable '%n' not defined\n",
          &var_expr->var);
      return NULL;
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;
      bool error = false;
      *time += 1 + let_expr->bindings.size;

      // Evaluate the types of the bindings and set up the new vars.
      Var nvd[let_expr->bindings.size];
      VarType* var_types[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleBinding* binding = let_expr->bindings.xs + i;
        var_types[i] = NULL;

        nvd[i].name = let_expr->bindings.xs[i].name;
        if (binding->type == NULL) {
          assert(binding->kind != NULL);
          VarType* var = FbleAlloc(arena_, VarType);
          FbleRefInit(arena, &var->_base.ref);
          var->_base.tag = VAR_TYPE;
          var->_base.loc = binding->name.loc;
          var->_base.evaluating = false;
          var->name = let_expr->bindings.xs[i].name;
          var->kind = CompileKind(arena_, binding->kind);
          var->value = NULL;

          TypeType* type_type = FbleAlloc(arena_, TypeType);
          FbleRefInit(arena, &type_type->_base.ref);
          type_type->_base.tag = TYPE_TYPE;
          type_type->_base.loc = binding->name.loc;
          type_type->_base.evaluating = false;
          type_type->type = &var->_base;
          FbleRefAdd(arena, &type_type->_base.ref, &type_type->type->ref);
          TypeRelease(arena, &var->_base);

          nvd[i].type = &type_type->_base;
          var_types[i] = var;
        } else {
          assert(binding->kind == NULL);
          nvd[i].type = CompileType(arena, vars, binding->type);
          error = error || (nvd[i].type == NULL);
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&let_expr->bindings.xs[i].name, &let_expr->bindings.xs[j].name)) {
            ReportError(arena_, &let_expr->bindings.xs[i].name.loc,
                "duplicate variable name '%n'\n",
                &let_expr->bindings.xs[i].name);
            error = true;
          }
        }
      }

      FbleLetPrepInstr* let_prep_instr = FbleAlloc(arena_, FbleLetPrepInstr);
      let_prep_instr->_base.tag = FBLE_LET_PREP_INSTR;
      let_prep_instr->count = let_expr->bindings.size;
      FbleVectorAppend(arena_, *instrs, &let_prep_instr->_base);

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        PushVar(arena_, vars, nvd[i].name, nvd[i].type);
      }

      // Compile the values of the variables.
      Type* var_type_values[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        var_type_values[i] = NULL;
        FbleBinding* binding = let_expr->bindings.xs + i;

        Type* type = NULL;
        if (!error) {
          FbleVectorAppend(arena_, *name, binding->name);
          type = CompileExpr(arena, blocks, name, false, vars, binding->expr, instrs, time);
          name->size--;
        }
        error = error || (type == NULL);

        if (!error && binding->type != NULL && !TypesEqual(nvd[i].type, type, NULL)) {
          error = true;
          ReportError(arena_, &binding->expr->loc,
              "expected type %t, but found %t\n",
              nvd[i].type, type);
        } else if (!error && binding->type == NULL) {
          VarType* var = var_types[i];
          var_type_values[i] = ValueOfType(arena, type);
          if (var_type_values[i] == NULL) {
            ReportError(arena_, &binding->expr->loc,
                "expected type, but found something of type %t\n",
                type);
            error = true;
          } else {
            Kind* expected_kind = var->kind;
            Kind* actual_kind = GetKind(arena_, var_type_values[i]);
            if (!KindsEqual(expected_kind, actual_kind)) {
              ReportError(arena_, &binding->expr->loc,
                  "expected kind %k, but found %k\n",
                  expected_kind, actual_kind);
              error = true;
            }
            FreeKind(arena_, actual_kind);
          }
        }

        TypeRelease(arena, type);
      }

      // Apply the newly computed type values for variables whose types were
      // previously unknown.
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (var_type_values[i] != NULL) {
          VarType* var = var_types[i];
          assert(var != NULL);
          var->value = var_type_values[i];
          FbleRefAdd(arena, &var->_base.ref, &var->value->ref);
          TypeRelease(arena, var->value);
        }
      }

      // Re-evaluate the variable types now that we have updated information
      // about the values of some of the types in question.
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        Eval(arena, nvd[i].type, NULL);
      }

      FbleLetDefInstr* let_def_instr = FbleAlloc(arena_, FbleLetDefInstr);
      let_def_instr->_base.tag = FBLE_LET_DEF_INSTR;
      let_def_instr->count = let_expr->bindings.size;
      FbleVectorAppend(arena_, *instrs, &let_def_instr->_base);

      Type* rtype = NULL;
      if (!error) {
        rtype = CompileExpr(arena, blocks, name, exit, vars, let_expr->body, instrs, time);
        error = (rtype == NULL);
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        PopVar(arena_, vars);
        TypeRelease(arena, nvd[i].type);
      }

      if (error) {
        TypeRelease(arena, rtype);
        return NULL;
      }

      if (!exit) {
        FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
        descope->_base.tag = FBLE_DESCOPE_INSTR;
        descope->count = let_expr->bindings.size;
        FbleVectorAppend(arena_, *instrs, &descope->_base);
      }
      return rtype;
    }

    case FBLE_MODULE_REF_EXPR: {
      *time += 1;
      FbleModuleRefExpr* module_ref_expr = (FbleModuleRefExpr*)expr;

      for (size_t i = 0; i < vars->nvars; ++i) {
        size_t j = vars->nvars - i - 1;
        Var* var = vars->vars.xs + j;
        if (FbleNamesEqual(&module_ref_expr->ref.resolved, &var->name)) {
          FbleVarInstr* instr = FbleAlloc(arena_, FbleVarInstr);
          instr->_base.tag = FBLE_VAR_INSTR;
          instr->position = j;
          FbleVectorAppend(arena_, vars->vars.xs[j].instrs, instr);
          FbleVectorAppend(arena_, *instrs, &instr->_base);
          CompileExit(arena_, exit, instrs);
          return TypeRetain(arena, var->type);
        }
      }

      // We should have resolved all modules at program load time, so we
      // should never get here.
      UNREACHABLE("module not in scope");
      return NULL;
    }

    case FBLE_POLY_EXPR: {
      FblePolyExpr* poly = (FblePolyExpr*)expr;

      VarType* arg = FbleAlloc(arena_, VarType);
      FbleRefInit(arena, &arg->_base.ref);
      arg->_base.tag = VAR_TYPE;
      arg->_base.loc = poly->arg.name.loc;
      arg->_base.evaluating = false;
      arg->name = poly->arg.name;
      arg->kind = CompileKind(arena_, poly->arg.kind);
      arg->value = NULL;

      TypeType* type_type = FbleAlloc(arena_, TypeType);
      FbleRefInit(arena, &type_type->_base.ref);
      type_type->_base.tag = TYPE_TYPE;
      type_type->_base.loc = poly->arg.name.loc;
      type_type->_base.evaluating = false;
      type_type->type = &arg->_base;
      FbleRefAdd(arena, &type_type->_base.ref, &arg->_base.ref);

      // TODO: It's a little silly that we are pushing an empty type value
      // here. Oh well. Maybe in the future we'll optimize those away or
      // add support for non-type poly args too.
      *time += 1;
      FbleTypeInstr* type_instr = FbleAlloc(arena_, FbleTypeInstr);
      type_instr->_base.tag = FBLE_TYPE_INSTR;
      FbleVectorAppend(arena_, *instrs, &type_instr->_base);

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      vpush->count = 1;
      FbleVectorAppend(arena_, *instrs, &vpush->_base);
      PushVar(arena_, vars, poly->arg.name, &type_type->_base);

      Type* body = CompileExpr(arena, blocks, name, exit, vars, poly->body, instrs, time);

      PopVar(arena_, vars);
      TypeRelease(arena, &type_type->_base);

      if (!exit) {
        FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
        descope->_base.tag = FBLE_DESCOPE_INSTR;
        descope->count = 1;
        FbleVectorAppend(arena_, *instrs, &descope->_base);
      }

      if (body == NULL) {
        TypeRelease(arena, &arg->_base);
        return NULL;
      }

      Type* pt = MakePolyType(arena, expr->loc, &arg->_base, body);
      TypeRelease(arena, &arg->_base);
      TypeRelease(arena, body);
      return pt;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;

      // Note: typeof(poly<arg>) = typeof(poly)<arg>
      // CompileExpr gives us typeof(poly)
      Type* poly = CompileExpr(arena, blocks, name, exit, vars, apply->poly, instrs, time);
      if (poly == NULL) {
        return NULL;
      }

      PolyKind* poly_kind = (PolyKind*)GetKind(arena_, poly);
      if (poly_kind->_base.tag != POLY_KIND) {
        ReportError(arena_, &expr->loc,
            "cannot apply poly args to a basic kinded entity");
        FreeKind(arena_, &poly_kind->_base);
        TypeRelease(arena, poly);
        return NULL;
      }

      // Note: CompileType gives us the value of arg
      Type* arg = CompileType(arena, vars, apply->arg);
      if (arg == NULL) {
        FreeKind(arena_, &poly_kind->_base);
        TypeRelease(arena, poly);
        return NULL;
      }

      Kind* expected_kind = poly_kind->arg;
      Kind* actual_kind = GetKind(arena_, arg);
      if (!KindsEqual(expected_kind, actual_kind)) {
        ReportError(arena_, &apply->arg->loc,
            "expected kind %k, but found %k\n",
            expected_kind, actual_kind);
        FreeKind(arena_, &poly_kind->_base);
        FreeKind(arena_, actual_kind);
        TypeRelease(arena, poly);
        TypeRelease(arena, arg);
        return NULL;
      }
      FreeKind(arena_, actual_kind);
      FreeKind(arena_, &poly_kind->_base);

      Type* pat = MakePolyApplyType(arena, expr->loc, poly, arg);
      TypeRelease(arena, poly);
      TypeRelease(arena, arg);
      Eval(arena, pat, NULL);
      return pat;
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* list_expr = (FbleListExpr*)expr;
      assert(list_expr->args.size > 0);

      FbleTypeofExpr typeof_elem = {
        ._base = { .tag = FBLE_TYPEOF_EXPR, .loc = expr->loc },
        .expr = list_expr->args.xs[0],
      };
      return CompileList(arena, blocks, name, exit, vars, expr->loc, &typeof_elem._base, list_expr->args, instrs, time);
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* literal = (FbleLiteralExpr*)expr;

      // Check that the type compiles before desugaring the literal
      // expression. Otherwise we could end up giving a lot of duplicate error
      // messages.
      Type* type = CompileType(arena, vars, literal->type);
      if (type == NULL) {
        return NULL;
      }
      TypeRelease(arena, type);

      FbleStructValueImplicitTypeExpr unit = {
        ._base = {
          .tag = FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR,
          .loc = literal->word_loc,
        },
        .args = { .size = 0, .xs = NULL, },
      };

      size_t n = strlen(literal->word);

      FbleUnionValueExpr letters[n];
      FbleExpr* xs[n];
      char word_letters[2*n];
      FbleLoc loc = literal->word_loc;
      for (size_t i = 0; i < n; ++i) {
        letters[i]._base.tag = FBLE_UNION_VALUE_EXPR;
        letters[i]._base.loc = loc;
        letters[i].type = literal->type;
        word_letters[2*i] = literal->word[i];
        word_letters[2*i + 1] = '\0';
        letters[i].field.name = word_letters + 2*i;
        letters[i].field.space = FBLE_NORMAL_NAME_SPACE;
        letters[i].field.loc = loc;
        letters[i].arg = &unit._base;

        xs[i] = &letters[i]._base;

        if (literal->word[i] == '\n') {
          loc.line++;
          loc.col = 0;
        }
        loc.col++;
      }

      FbleExprV args = { .size = n, .xs = xs, };
      return CompileList(arena, blocks, name, exit, vars, literal->word_loc, literal->type, args, instrs, time);
    }

    case FBLE_STRUCT_IMPORT_EXPR: {
      *time += 1;
      FbleStructImportExpr* struct_import_expr = (FbleStructImportExpr*)expr;

      Type* type = CompileExpr(arena, blocks, name, false, vars, struct_import_expr->nspace, instrs, time);
      if (type == NULL) {
        return NULL;
      }

      StructType* struct_type = (StructType*)Normal(type);
      if (struct_type->_base.tag != STRUCT_TYPE) {
        ReportError(arena_, &struct_import_expr->nspace->loc,
            "expected value of type struct, but found value of type %t\n",
            type);

        TypeRelease(arena, type);
        return NULL;
      }

      *time += 1 + struct_type->fields.size;

      FbleStructImportInstr* struct_import = FbleAlloc(arena_, FbleStructImportInstr);
      struct_import->_base.tag = FBLE_STRUCT_IMPORT_INSTR;
      struct_import->loc = struct_import_expr->nspace->loc;
      FbleVectorAppend(arena_, *instrs, &struct_import->_base);

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        PushVar(arena_, vars, struct_type->fields.xs[i].name, struct_type->fields.xs[i].type);
      }

      Type* rtype = CompileExpr(arena, blocks, name, exit, vars, struct_import_expr->body, instrs, time);

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        PopVar(arena_, vars);
      }

      if (!exit) {
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

// CompileList --
//   Type check and compile a list expression. Returns the type of the
//   expression and generates instructions to compute the value of that
//   expression at runtime.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the vector of block locations to populate.
//   name - a sequence of names describing the current location in the code.
//          Used for naming profiling blocks.
//   exit - if true, generate instructions to exit the current scope when done.
//   vars - the list of variables in scope.
//   loc - the location of the list expression.
//   type - the expected type of the list elements.
//   args - the elements of the list expression to compile.
//   instrs - vector of instructions to append new instructions to.
//   time - output var tracking the time required to execute the compiled block.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Appends blocks to 'blocks' with compiled block information.
//   Appends instructions to 'instrs' for executing the given expression.
//   There is no gaurentee about what instructions have been appended to
//   'instrs' if the expression fails to compile.
//   Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using
//   TypeRelease when it is no longer needed.
//   Increments 'time' by the amount of time required to execute this
//   expression.
static Type* CompileList(TypeArena* arena, FbleNameV* blocks, FbleNameV* name, bool exit, Vars* vars, FbleLoc loc, FbleType* type, FbleExprV args, FbleInstrV* instrs, size_t* time)
{
  // The goal is to desugar a list expression [a, b, c, d] into the
  // following expression:
  // <@ T@>(T@ x, T@ x1, T@ x2, T@ x3)<@ L@>((T@, L@){L@;} cons, L@ nil) {
  //   cons(x, cons(x1, cons(x2, cons(x3, nil))));
  // }<t@>(a, b, c, d)
  //
  // But if there are zero args, we'll instead generate:
  // <@ T@><@ L@>((T@, L@){L@;} cons, L@ nil) {
  //   nil;
  // }<t@>
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleKind basic_kind = { .tag = FBLE_BASIC_KIND, .loc = loc, };

  FbleName elem_type_name = {
    .name = "T",
    .space = FBLE_TYPE_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr elem_type = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = elem_type_name,
  };

  // Generate unique names for the variables x, x0, x1, ...
  size_t num_digits = 0;
  for (size_t x = args.size; x > 0; x /= 10) {
    num_digits++;
  }

  FbleName arg_names[args.size];
  FbleVarExpr arg_values[args.size];
  for (size_t i = 0; i < args.size; ++i) {
    char* name = FbleArrayAlloc(arena_, char, num_digits + 2);
    name[0] = 'x';
    name[num_digits+1] = '\0';
    for (size_t j = 0, x = i; j < num_digits; j++, x /= 10) {
      name[num_digits - j] = (x % 10) + '0';
    }
    arg_names[i].name = name;
    arg_names[i].space = FBLE_NORMAL_NAME_SPACE;
    arg_names[i].loc = loc;

    arg_values[i]._base.tag = FBLE_VAR_EXPR;
    arg_values[i]._base.loc = loc;
    arg_values[i].var = arg_names[i];
  }

  FbleName list_type_name = {
    .name = "L",
    .space = FBLE_TYPE_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr list_type = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = list_type_name,
  };

  FbleField inner_args[2];
  FbleName cons_name = {
    .name = "cons",
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr cons = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = cons_name,
  };

  // L@ -> L@
  FbleFuncTypeExpr cons_type_inner = {
    ._base = { .tag = FBLE_FUNC_TYPE_EXPR, .loc = loc, },
    .arg = &list_type._base,
    .rtype = &list_type._base,
  };

  // T@ -> (L@ -> L@)
  FbleFuncTypeExpr cons_type = {
    ._base = { .tag = FBLE_FUNC_TYPE_EXPR, .loc = loc, },
    .arg = &elem_type._base,
    .rtype = &cons_type_inner._base,
  };

  inner_args[0].type = &cons_type._base;
  inner_args[0].name = cons_name;

  FbleName nil_name = {
    .name = "nil",
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr nil = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = nil_name,
  };

  inner_args[1].type = &list_type._base;
  inner_args[1].name = nil_name;

  FbleMiscApplyExpr applys[args.size];
  FbleExpr* all_args[args.size * 2];
  for (size_t i = 0; i < args.size; ++i) {
    applys[i]._base.tag = FBLE_MISC_APPLY_EXPR;
    applys[i]._base.loc = loc;
    applys[i].misc = &cons._base;
    applys[i].args.size = 2;
    applys[i].args.xs = all_args + 2 * i;

    applys[i].args.xs[0] = &arg_values[i]._base;
    applys[i].args.xs[1] = (i + 1 < args.size) ? &applys[i+1]._base : &nil._base;
  }

  FbleFuncValueExpr inner_func = {
    ._base = { .tag = FBLE_FUNC_VALUE_EXPR, .loc = loc },
    .args = { .size = 2, .xs = inner_args },
    .body = (args.size == 0) ? &nil._base : &applys[0]._base,
  };

  FblePolyExpr inner_poly = {
    ._base = { .tag = FBLE_POLY_EXPR, .loc = loc },
    .arg = {
      .kind = &basic_kind,
      .name = list_type_name,
    },
    .body = &inner_func._base,
  };

  FbleField outer_args[args.size];
  for (size_t i = 0; i < args.size; ++i) {
    outer_args[i].type = &elem_type._base;
    outer_args[i].name = arg_names[i];
  }

  FbleFuncValueExpr outer_func = {
    ._base = { .tag = FBLE_FUNC_VALUE_EXPR, .loc = loc },
    .args = { .size = args.size, .xs = outer_args },
    .body = &inner_poly._base,
  };

  FblePolyExpr outer_poly = {
    ._base = { .tag = FBLE_POLY_EXPR, .loc = loc },
    .arg = {
      .kind = &basic_kind,
      .name = elem_type_name,
    },
    .body = (args.size == 0) ? &inner_poly._base : &outer_func._base,
  };

  FblePolyApplyExpr apply_type = {
    ._base = { .tag = FBLE_POLY_APPLY_EXPR, .loc = loc },
    .poly = &outer_poly._base,
    .arg = type,
  };

  FbleMiscApplyExpr apply_elems = {
    ._base = { .tag = FBLE_MISC_APPLY_EXPR, .loc = loc },
    .misc = &apply_type._base,
    .args = args,
  };

  FbleExpr* expr = (args.size == 0) ? &apply_type._base : &apply_elems._base;

  Type* result = CompileExpr(arena, blocks, name, exit, vars, expr, instrs, time);

  for (size_t i = 0; i < args.size; i++) {
    FbleFree(arena_, (void*)arg_names[i].name);
  }
  return result;
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

  // Make a copy of vars to ensure we don't save references to any
  // instructions that we may free.
  Vars nvars;
  FbleVectorInit(arena_, nvars.vars);
  nvars.nvars = 0;
  for (size_t i = 0; i < vars->nvars; ++i) {
    PushVar(arena_, &nvars, vars->vars.xs[i].name, vars->vars.xs[i].type);
  }

  FbleInstrV instrs;
  FbleVectorInit(arena_, instrs);
  FbleNameV blocks;
  FbleVectorInit(arena_, blocks);
  FbleNameV name;
  FbleVectorInit(arena_, name);
  size_t time;
  Type* type = CompileExpr(arena, &blocks, &name, true, &nvars, expr, &instrs, &time);
  FbleFree(arena_, name.xs);

  FbleFreeBlockNames(arena_, &blocks);
  for (size_t i = 0; i < instrs.size; ++i) {
    FreeInstr(arena_, instrs.xs[i]);
  }
  FbleFree(arena_, instrs.xs);
  FreeVars(arena_, &nvars);
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
      st->_base.tag = STRUCT_TYPE;
      st->_base.loc = type->loc;
      st->_base.evaluating = false;
      FbleVectorInit(arena_, st->fields);

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleField* field = struct_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          TypeRelease(arena, &st->_base);
          return NULL;
        }

        if (!CheckNameSpace(arena, &field->name, compiled)) {
          TypeRelease(arena, compiled);
          TypeRelease(arena, &st->_base);
          return NULL;
        }

        Field* cfield = FbleVectorExtend(arena_, st->fields);
        cfield->name = field->name;
        cfield->type = compiled;

        FbleRefAdd(arena, &st->_base.ref, &cfield->type->ref);
        TypeRelease(arena, cfield->type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&field->name, &struct_type->fields.xs[j].name)) {
            ReportError(arena_, &field->name.loc,
                "duplicate field name '%n'\n",
                &field->name);
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
      ut->_base.tag = UNION_TYPE;
      ut->_base.loc = type->loc;
      ut->_base.evaluating = false;
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
          if (FbleNamesEqual(&field->name, &union_type->fields.xs[j].name)) {
            ReportError(arena_, &field->name.loc,
                "duplicate field name '%n'\n",
                &field->name);
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
      ft->_base.tag = FUNC_TYPE;
      ft->_base.loc = type->loc;
      ft->_base.evaluating = false;
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

    case FBLE_PROC_TYPE_EXPR: {
      ProcType* ut = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &ut->_base.ref);
      ut->_base.loc = type->loc;
      ut->_base.evaluating = false;

      ut->_base.tag = PROC_TYPE;

      ut->type = NULL;

      FbleProcTypeExpr* unary_type = (FbleProcTypeExpr*)type;
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
    case FBLE_UNION_SELECT_EXPR:
    case FBLE_FUNC_VALUE_EXPR:
    case FBLE_EVAL_EXPR:
    case FBLE_LINK_EXPR:
    case FBLE_EXEC_EXPR:
    case FBLE_VAR_EXPR:
    case FBLE_MODULE_REF_EXPR:
    case FBLE_LET_EXPR:
    case FBLE_POLY_EXPR:
    case FBLE_POLY_APPLY_EXPR:
    case FBLE_LIST_EXPR:
    case FBLE_LITERAL_EXPR:
    case FBLE_STRUCT_IMPORT_EXPR: {
      FbleExpr* expr = type;
      Type* type = CompileExprNoInstrs(arena, vars, expr);
      if (type == NULL) {
        return NULL;
      }

      Type* type_value = ValueOfType(arena, type);
      TypeRelease(arena, type);
      if (type_value == NULL) {
        ReportError(arena_, &expr->loc,
            "expected a type, but found value of type %t\n",
            type);
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
      k->level = 1;
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

// CompileProgram --
//   Type check and compile the given program. Returns the type of the
//   program and generates instructions to compute the value of that
//   program at runtime.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the vector of block locations to populate.
//   name - a sequence of names describing the current location in the code.
//          Used for naming profiling blocks.
//   vars - the list of variables in scope.
//   prgm - the program to compile.
//   instrs - vector of instructions to append new instructions to.
//   time - output var tracking the time required to execute the compiled block.
//
// Results:
//   The type of the program, or NULL if the program is not well typed.
//
// Side effects:
//   Appends blocks to 'blocks' with compiled block information.
//   Appends instructions to 'instrs' for executing the given program.
//   There is no gaurentee about what instructions have been appended to
//   'instrs' if the program fails to compile.
//   Prints a message to stderr if the program fails to compile.
//   Allocates a reference-counted type that must be freed using
//   TypeRelease when it is no longer needed.
//   Increments 'time' by the amount of time required to execute this
//   program.
static Type* CompileProgram(TypeArena* arena, FbleNameV* blocks, FbleNameV* name, Vars* vars, FbleProgram* prgm, FbleInstrV* instrs, size_t* time)
{
  *time += 1 + prgm->modules.size;

  FbleArena* arena_ = FbleRefArenaArena(arena);
  Type* types[prgm->modules.size];
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    FbleVectorAppend(arena_, *name, prgm->modules.xs[i].name);
    types[i] = CompileExpr(arena, blocks, name, false, vars, prgm->modules.xs[i].value, instrs, time);
    name->size--;

    if (types[i] == NULL) {
      for (size_t j = 0; j < i; ++j) {
        TypeRelease(arena, types[j]);
      }
      return NULL;
    }

    FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
    vpush->_base.tag = FBLE_VPUSH_INSTR;
    vpush->count = 1;
    FbleVectorAppend(arena_, *instrs, &vpush->_base);
    PushVar(arena_, vars, prgm->modules.xs[i].name, types[i]);
  }

  Type* rtype = CompileExpr(arena, blocks, name, true, vars, prgm->main, instrs, time);
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    PopVar(arena_, vars);
    TypeRelease(arena, types[i]);
  }

  if (rtype == NULL) {
    return NULL;
  }

  return rtype;
}

// FbleFreeInstrBlock -- see documentation in internal.h
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

// FbleCompile -- see documentation in internal.h
FbleInstrBlock* FbleCompile(FbleArena* arena, FbleNameV* blocks, FbleProgram* program)
{
  FbleVectorInit(arena, *blocks);
  FbleName* nmain = FbleVectorExtend(arena, *blocks);

  nmain->name = FbleArrayAlloc(arena, char, 7);
  strcpy((char*)nmain->name, "__main");
  nmain->loc.source = __FILE__;
  nmain->loc.line = __LINE__;
  nmain->loc.col = 0;

  FbleNameV name;
  FbleVectorInit(arena, name);

  // TODO: What location to use for the instruction block?
  FbleInstrBlock* block = NewInstrBlock(arena, blocks, &name, program->main->loc);

  assert(block->instrs.xs[0]->tag == FBLE_PROFILE_ENTER_BLOCK_INSTR);
  size_t* body_time = &((FbleProfileEnterBlockInstr*)block->instrs.xs[0])->time;

  Vars vars;
  FbleVectorInit(arena, vars.vars);
  vars.nvars = 0;

  TypeArena* type_arena = FbleNewRefArena(arena, &TypeFree, &TypeAdded);
  Type* type = CompileProgram(type_arena, blocks, &name, &vars, program, &block->instrs, body_time);
  TypeRelease(type_arena, type);
  FbleDeleteRefArena(type_arena);

  FbleFree(arena, name.xs);

  FreeVars(arena, &vars);
  if (type == NULL) {
    FbleFreeInstrBlock(arena, block);
    return NULL;
  }
  return block;
}
