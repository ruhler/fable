
#include <assert.h>  // for assert

#include "typecheck.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

static FbleLoc DummyLoc(FbleArena* arena);
static FbleTc* Compile(FbleArena* arena, FbleValueV args, FbleValue* value);


// DummyLoc -- 
//   Create a dummy location instance, for use until we figure out a proper
//   way to track locations for symbolic compilation.
//
// Inputs:
//   arena - arean to use for allocations.
//
// Returns:
//   A location.
//
// Side effects:
//   The caller must call FbleFreeLoc when the location is no longer needed.
static FbleLoc DummyLoc(FbleArena* arena)
{
  FbleLoc loc = {
    .source = FbleNewString(arena, __FILE__),
    .line = __LINE__ - 4,
    .col = 1,
  };
  return loc;
}

// Compile -- 
//   Compile an FbleValue to a FbleTc representing an expression to compute
//   that value.
//
// Input:
//   arena - arena to use for allocations.
//   args - values that should be treated as arguments to the function.
//   value - the value to compile.
//
// Result:
//   A type checked expression to compute the desired value.
//
// Side effects:
//   The returned FbleTc must be freed with FbleFreeTc when no longer needed.
static FbleTc* Compile(FbleArena* arena, FbleValueV args, FbleValue* value)
{
  // TODO: Avoid rebuilding values from scratch if they don't depend on any of
  // the function arguments. Store them as part of the function scope instead.
  switch (value->tag) {
    case FBLE_STRUCT_VALUE: {
      FbleStructValue* struct_value = (FbleStructValue*)value;
      FbleStructValueTc* struct_tc = FbleAlloc(arena, FbleStructValueTc);
      struct_tc->_base.tag = FBLE_STRUCT_VALUE_TC;
      struct_tc->_base.loc = DummyLoc(arena);
      FbleVectorInit(arena, struct_tc->args);
      for (size_t i = 0; i < struct_value->fieldc; ++i) {
        FbleTc* arg = Compile(arena, args, struct_value->fields[i]);
        FbleVectorAppend(arena, struct_tc->args, arg);
      }
      return &struct_tc->_base;
    }

    case FBLE_UNION_VALUE: {
      FbleUnionValue* union_value = (FbleUnionValue*)value;
      FbleUnionValueTc* union_tc = FbleAlloc(arena, FbleUnionValueTc);
      union_tc->_base.tag = FBLE_UNION_VALUE_TC;
      union_tc->_base.loc = DummyLoc(arena);
      union_tc->tag = union_value->tag;
      union_tc->arg = Compile(arena, args, union_value->arg);
      return &union_tc->_base;
    }

    case FBLE_FUNC_VALUE: assert(false && "TODO: Compile FBLE_FUNC_VALUE"); return NULL;
    case FBLE_LINK_VALUE: assert(false && "TODO: Compile FBLE_LINK_VALUE"); return NULL;
    case FBLE_PORT_VALUE: assert(false && "TODO: Compile FBLE_PORT_VALUE"); return NULL;
    case FBLE_REF_VALUE: assert(false && "TODO: Compile FBLE_REF_VALUE"); return NULL;
    case FBLE_TYPE_VALUE: assert(false && "TODO: Compile FBLE_TYPE_VALUE"); return NULL;

    case FBLE_SYMBOLIC_VALUE: {
      size_t i = 0;
      while (i < args.size && args.xs[i] != value) {
        ++i;
      }

      assert(i < args.size && "TODO: handle case of unbound symbolic value");

      FbleVarTc* var_tc = FbleAlloc(arena, FbleVarTc);
      var_tc->_base.tag = FBLE_VAR_TC;
      var_tc->_base.loc = DummyLoc(arena);
      var_tc->index.source = FBLE_LOCAL_VAR;
      var_tc->index.index = i;
      return &var_tc->_base;
    }

    case FBLE_STRUCT_ACCESS_VALUE: assert(false && "TODO: Compile FBLE_STRUCT_ACCESS_VALUE"); return NULL;
    case FBLE_UNION_ACCESS_VALUE: assert(false && "TODO: Compile FBLE_UNION_ACCESS_VALUE"); return NULL;
    case FBLE_UNION_SELECT_VALUE: assert(false && "TODO: Compile FBLE_UNION_SELECT_VALUE"); return NULL;
  }

  UNREACHABLE("should never get here");
}

// FbleSymbolicCompile -- see documentation in value.h
FbleValue* FbleSymbolicCompile(FbleValueHeap* heap, FbleValueV args, FbleValue* body)
{
  FbleArena* arena = heap->arena;
  FbleTc* tc = Compile(arena, args, body);
  FbleInstrBlock* code = FbleCompileTc(arena, args.size, tc, NULL);
  FbleFreeTc(arena, tc);

  FbleFuncValue* func = FbleNewValue(heap, FbleFuncValue);
  func->_base.tag = FBLE_FUNC_VALUE;
  func->argc = args.size;
  func->code = code;
  assert(func->code->statics == 0);
  return &func->_base;
}
