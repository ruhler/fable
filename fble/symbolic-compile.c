
#include <assert.h>  // for assert

#include "typecheck.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

static FbleValue* Compile(FbleValueHeap* heap, FbleValueV args, FbleValue* value);


// Compile -- 
//   Compile an FbleValue to a FbleTc representing an expression to compute
//   that value.
//
// Input:
//   heap - heap to use for allocations.
//   args - values that should be treated as arguments to the function.
//   value - the value to compile.
//
// Result:
//   A type checked expression to compute the desired value.
//
// Side effects:
//   The returned FbleTc must be freed with FbleFreeTc when no longer needed.
static FbleValue* Compile(FbleValueHeap* heap, FbleValueV args, FbleValue* value)
{
  FbleArena* arena = heap->arena;

  // TODO: Avoid rebuilding values from scratch if they don't depend on any of
  // the function arguments. Store them as part of the function scope instead.
  switch (value->tag) {
    case FBLE_STRUCT_VALUE: {
      FbleStructValue* struct_value = (FbleStructValue*)value;
      FbleStructValueTc* struct_tc = FbleAlloc(arena, FbleStructValueTc);
      struct_tc->_base.tag = FBLE_STRUCT_VALUE_TC;
      FbleVectorInit(arena, struct_tc->args);
      for (size_t i = 0; i < struct_value->fieldc; ++i) {
        FbleValue* arg = Compile(heap, args, struct_value->fields[i]);
        FbleVectorAppend(arena, struct_tc->args, arg);
      }
      return FbleNewTcValue(heap, &struct_tc->_base);
    }

    case FBLE_UNION_VALUE: {
      FbleUnionValue* union_value = (FbleUnionValue*)value;
      FbleUnionValueTc* union_tc = FbleAlloc(arena, FbleUnionValueTc);
      union_tc->_base.tag = FBLE_UNION_VALUE_TC;
      union_tc->tag = union_value->tag;
      union_tc->arg = Compile(heap, args, union_value->arg);
      return FbleNewTcValue(heap, &union_tc->_base);
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
      var_tc->index.source = FBLE_LOCAL_VAR;
      var_tc->index.index = i;
      return FbleNewTcValue(heap, &var_tc->_base);
    }

    case FBLE_DATA_ACCESS_VALUE: {
      FbleDataAccessValue* access_value = (FbleDataAccessValue*)value;
      FbleDataAccessTc* access_tc = FbleAlloc(arena, FbleDataAccessTc);
      access_tc->_base.tag = FBLE_DATA_ACCESS_TC;
      access_tc->datatype = access_value->datatype;
      access_tc->obj = Compile(heap, args, access_value->obj);
      access_tc->tag = access_value->tag;
      access_tc->loc = FbleCopyLoc(access_value->loc);
      return FbleNewTcValue(heap, &access_tc->_base);
    }

    case FBLE_UNION_SELECT_VALUE: assert(false && "TODO: Compile FBLE_UNION_SELECT_VALUE"); return NULL;
    case FBLE_TC_VALUE: assert(false && "TODO: Compile FBLE_TC_VALUE"); return NULL;
  }

  UNREACHABLE("should never get here");
}

// FbleSymbolicCompile -- see documentation in value.h
FbleValue* FbleSymbolicCompile(FbleValueHeap* heap, FbleValueV args, FbleValue* body, FbleName name)
{
  FbleArena* arena = heap->arena;
  FbleValue* tc = Compile(heap, args, body);
  FbleInstrBlock* code = FbleCompileValue(arena, args.size, tc, name, NULL);
  FbleReleaseValue(heap, tc);

  FbleFuncValue* func = FbleNewValue(heap, FbleFuncValue);
  func->_base.tag = FBLE_FUNC_VALUE;
  func->argc = args.size;
  func->code = code;
  assert(func->code->statics == 0);
  return &func->_base;
}
