
#include <assert.h>  // for assert

#include "typecheck.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

static FbleTc* Compile(FbleArena* arena, FbleValueV args, FbleValue* value);

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
  switch (value->tag) {
    case FBLE_STRUCT_VALUE: assert(false && "TODO: Compile FBLE_STRUCT_VALUE"); return NULL;
    case FBLE_UNION_VALUE: assert(false && "TODO: Compile FBLE_UNION_VALUE"); return NULL;
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
      var_tc->_base.loc.source = FbleNewString(arena, __FILE__);
      var_tc->_base.loc.line = __LINE__;
      var_tc->_base.loc.col = 1;
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
