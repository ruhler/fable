
#include <assert.h>  // for assert

#include "typecheck.h"
#include "value.h"


// FbleSymbolicCompile -- see documentation in value.h
FbleValue* FbleSymbolicCompile(FbleValueHeap* heap, FbleValueV args, FbleValue* body, FbleName name)
{
  FbleArena* arena = heap->arena; 

  for (size_t i = 0; i < args.size; ++i) {
    FbleSymbolicValue* arg = (FbleSymbolicValue*)args.xs[i];
    assert(arg->_base.tag == FBLE_SYMBOLIC_VALUE);
    arg->index.source = FBLE_LOCAL_VAR;
    arg->index.index = i;
  }

  FbleInstrBlock* code = FbleCompileValue(arena, args.size, body, name, NULL);

  FbleFuncValue* func = FbleNewValue(heap, FbleFuncValue);
  func->_base.tag = FBLE_FUNC_VALUE;
  func->argc = args.size;
  func->code = code;
  assert(func->code->statics == 0);
  return &func->_base;
}
