// value.c
//   This file implements routines associated with fble values.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fble.h"
#include "heap.h"
#include "tc.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

static void OnFree(FbleValueHeap* heap, FbleValue* value);
static void Ref(FbleHeapCallback* callback, FbleValue* value);
static void Refs(FbleHeapCallback* callback, FbleValue* value);


// FbleNewValueHeap -- see documentation in fble-.h
FbleValueHeap* FbleNewValueHeap(FbleArena* arena)
{
  return FbleNewMarkSweepHeap(arena,
      (void (*)(FbleHeapCallback*, void*))&Refs,
      (void (*)(FbleHeap*, void*))&OnFree);
}

// FbleFreeValueHeap -- see documentation in fble.h
void FbleFreeValueHeap(FbleValueHeap* heap)
{
  FbleFreeMarkSweepHeap(heap);
}

// FbleRetainValue -- see documentation in fble-value.h
void FbleRetainValue(FbleValueHeap* heap, FbleValue* value)
{
  heap->retain(heap, value);
}

// FbleReleaseValue -- see documentation in fble-value.h
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value)
{
  if (value != NULL) {
    heap->release(heap, value);
  }
}

// FbleValueAddRef -- see documentation in fble-value.h
void FbleValueAddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst)
{
  heap->add_ref(heap, src, dst);
}

// FbleValueDelRef -- see documentation in fble-value.h
void FbleValueDelRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst)
{
  heap->del_ref(heap, src, dst);
}

// FbleValueFullGc -- see documentation in fble-value.h
void FbleValueFullGc(FbleValueHeap* heap)
{
  heap->full_gc(heap);
}

// OnFree --
//   The 'on_free' function for values. See documentation in heap.h
static void OnFree(FbleValueHeap* heap, FbleValue* value)
{
  FbleArena* arena = heap->arena;
  switch (value->tag) {
    case FBLE_TYPE_VALUE_TC: return;
    case FBLE_VAR_TC: return;

    case FBLE_LET_TC: {
      FbleLetTc* let_tc = (FbleLetTc*)value;
      FbleFree(arena, let_tc->bindings.xs);
      return;
    }

    case FBLE_STRUCT_VALUE_TC: return;
    case FBLE_UNION_VALUE_TC: return;

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* v = (FbleFuncValue*)value;
      FbleFreeInstrBlock(arena, v->code);
      return;
    }

    case FBLE_LINK_VALUE: {
      FbleLinkValue* v = (FbleLinkValue*)value;
      FbleValues* curr = v->head;
      while (curr != NULL) {
        FbleValues* tmp = curr;
        curr = curr->next;
        FbleFree(arena, tmp);
      }
      return;
    }

    case FBLE_PORT_VALUE: return;
    case FBLE_REF_VALUE: return;

    case FBLE_DATA_ACCESS_VALUE: {
      FbleDataAccessValue* v = (FbleDataAccessValue*)value;
      FbleFreeLoc(arena, v->loc);
      return;
    }

    case FBLE_UNION_SELECT_VALUE: {
      FbleUnionSelectValue* v = (FbleUnionSelectValue*)value;
      FbleFreeLoc(arena, v->loc);
      return;
    }

    case FBLE_PROFILE_TC: {
      FbleProfileTc* profile_tc = (FbleProfileTc*)value;
      FbleFreeLoc(arena, profile_tc->loc);
      FbleFreeName(arena, profile_tc->name);
      return;
    }

    case FBLE_LINK_TC: return;

    case FBLE_EXEC_TC: {
      FbleExecTc* exec_tc = (FbleExecTc*)value;
      FbleFree(arena, exec_tc->bindings.xs);
      return;
    }

    case FBLE_SYMBOLIC_VALUE_TC: return;

    case FBLE_SYMBOLIC_COMPILE_TC: {
      FbleSymbolicCompileTc* compile_tc = (FbleSymbolicCompileTc*)value;
      FbleFreeLoc(arena, compile_tc->loc);
      FbleFree(arena, compile_tc->args.xs);
      return;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)value;
      FbleFreeLoc(arena, func_tc->body_loc);
      FbleFree(arena, func_tc->scope.xs);
      return;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)value;
      FbleFreeLoc(arena, apply_tc->loc);
      FbleFree(arena, apply_tc->args.xs);
      return;
    }
  }

  UNREACHABLE("Should not get here");
}

// Ref --
//   Helper function for implementing Refs. Call the callback if the value is
//   not null.
//
// Inputs:
//   callback - the refs callback.
//   value - the value to add.
//
// Results:
//   none.
//
// Side effects:
//   If value is non-null, the callback is called for it.
static void Ref(FbleHeapCallback* callback, FbleValue* value)
{
  if (value != NULL) {
    callback->callback(callback, value);
  }
}

// Refs --
//   The 'refs' function for values. See documentation in heap.h
static void Refs(FbleHeapCallback* callback, FbleValue* value)
{
  switch (value->tag) {
    case FBLE_TYPE_VALUE_TC: break;
    case FBLE_VAR_TC: break;

    case FBLE_LET_TC: {
      FbleLetTc* let_tc = (FbleLetTc*)value;
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        Ref(callback, let_tc->bindings.xs[i]);
      }
      Ref(callback, let_tc->body);
      return;
    }

    case FBLE_STRUCT_VALUE_TC: {
      FbleStructValueTc* sv = (FbleStructValueTc*)value;
      for (size_t i = 0; i < sv->fieldc; ++i) {
        Ref(callback, sv->fields[i]);
      }
      break;
    }

    case FBLE_UNION_VALUE_TC: {
      FbleUnionValueTc* uv = (FbleUnionValueTc*)value;
      Ref(callback, uv->arg);
      break;
    }

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* v = (FbleFuncValue*)value;
      for (size_t i = 0; i < v->code->statics; ++i) {
        Ref(callback, v->scope[i]);
      }
      break;
    }

    case FBLE_LINK_VALUE: {
      FbleLinkValue* v = (FbleLinkValue*)value;
      for (FbleValues* elem = v->head; elem != NULL; elem = elem->next) {
        Ref(callback, elem->value);
      }
      break;
    }

    case FBLE_PORT_VALUE: {
      break;
    }

    case FBLE_REF_VALUE: {
      FbleRefValue* rv = (FbleRefValue*)value;
      Ref(callback, rv->value);
      break;
    }

    case FBLE_DATA_ACCESS_VALUE: {
      FbleDataAccessValue* v = (FbleDataAccessValue*)value;
      Ref(callback, v->obj);
      break;
    }

    case FBLE_UNION_SELECT_VALUE: {
      FbleUnionSelectValue* v = (FbleUnionSelectValue*)value;
      Ref(callback, v->condition);
      for (size_t i = 0; i < v->choicec; ++i) {
        Ref(callback, v->choices[i]);
      }
      break;
    }

    case FBLE_PROFILE_TC: {
      FbleProfileTc* v = (FbleProfileTc*)value;
      Ref(callback, v->body);
      break;
    }

    case FBLE_LINK_TC: {
      FbleLinkTc* link_tc = (FbleLinkTc*)value;
      Ref(callback, link_tc->body);
      break;
    }

    case FBLE_EXEC_TC: {
      FbleExecTc* exec_tc = (FbleExecTc*)value;
      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        Ref(callback, exec_tc->bindings.xs[i]);
      }
      Ref(callback, exec_tc->body);
      break;
    }

    case FBLE_SYMBOLIC_VALUE_TC: break;

    case FBLE_SYMBOLIC_COMPILE_TC: {
      FbleSymbolicCompileTc* compile_tc = (FbleSymbolicCompileTc*)value;
      Ref(callback, compile_tc->body);
      return;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)value;
      Ref(callback, func_tc->body);
      return;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)value;
      Ref(callback, apply_tc->func);
      for (size_t i = 0; i < apply_tc->args.size; ++i) {
        Ref(callback, apply_tc->args.xs[i]);
      }
      return;
    }
  }
}

// FbleNewStructValue -- see documentation in fble-value.h
FbleValue* FbleNewStructValue(FbleValueHeap* heap, FbleValueV args)
{
  FbleStructValueTc* value = FbleNewValueExtra(heap, FbleStructValueTc, sizeof(FbleValue*) * args.size);
  value->_base.tag = FBLE_STRUCT_VALUE_TC;
  value->fieldc = args.size;

  for (size_t i = 0; i < args.size; ++i) {
    value->fields[i] = args.xs[i];
    FbleValueAddRef(heap, &value->_base, args.xs[i]);
  }
  return &value->_base;
}

// FbleStructValueAccess -- see documentation in fble-value.h
FbleValue* FbleStructValueAccess(FbleValue* object, size_t field)
{
  assert(object->tag == FBLE_STRUCT_VALUE_TC);
  FbleStructValueTc* value = (FbleStructValueTc*)object;
  assert(field < value->fieldc);
  return value->fields[field];
}

// FbleNewUnionValue -- see documentation in fble-value.h
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue* arg)
{
  FbleUnionValueTc* union_value = FbleNewValue(heap, FbleUnionValueTc);
  union_value->_base.tag = FBLE_UNION_VALUE_TC;
  union_value->tag = tag;
  union_value->arg = arg;
  FbleValueAddRef(heap, &union_value->_base, arg);
  return &union_value->_base;
}
// FbleNewEnumValue -- see documentation in fble-value.h
FbleValue* FbleNewEnumValue(FbleValueHeap* heap, size_t tag)
{
  FbleValueV args = { .size = 0, .xs = NULL, };
  FbleValue* unit = FbleNewStructValue(heap, args);
  FbleValue* result = FbleNewUnionValue(heap, tag, unit);
  FbleReleaseValue(heap, unit);
  return result;
}

// FbleUnionValueTag -- see documentation in fble-value.h
size_t FbleUnionValueTag(FbleValue* object)
{
  assert(object->tag == FBLE_UNION_VALUE_TC);
  FbleUnionValueTc* value = (FbleUnionValueTc*)object;
  return value->tag;
}

// FbleUnionValueAccess -- see documentation in fble-value.h
FbleValue* FbleUnionValueAccess(FbleValue* object)
{
  assert(object->tag == FBLE_UNION_VALUE_TC);
  FbleUnionValueTc* value = (FbleUnionValueTc*)object;
  return value->arg;
}

// FbleIsProcValue -- see documentation in fble-value.h
bool FbleIsProcValue(FbleValue* value)
{
  FbleProcValue* proc = (FbleProcValue*)value;
  return value->tag == FBLE_PROC_VALUE && proc->argc == 0;
}

// FbleNewGetValue -- see documentation in value.h
FbleValue* FbleNewGetValue(FbleValueHeap* heap, FbleValue* port)
{
  static FbleGetInstr iget = {
    ._base = { .tag = FBLE_GET_INSTR, .profile_ops = NULL },
    .port = { .section = FBLE_STATICS_FRAME_SECTION, .index = 0},
    .dest = 0,
  };

  static FbleReturnInstr irtn = {
    ._base = { .tag = FBLE_RETURN_INSTR, .profile_ops = NULL },
    .result = { .section = FBLE_LOCALS_FRAME_SECTION, .index = 0}
  };

  static FbleInstr* instrs[] = {
    &iget._base,
    &irtn._base,
  };

  static FbleInstrBlock code = {
    .refcount = 1,
    .magic = FBLE_INSTR_BLOCK_MAGIC,
    .statics = 1,  // port
    .locals = 1,   // result
    .instrs = { .size = 2, .xs = instrs }
  };

  assert(port->tag == FBLE_LINK_VALUE || port->tag == FBLE_PORT_VALUE);

  FbleProcValue* get = FbleNewValueExtra(heap, FbleProcValue, sizeof(FbleValue*));
  get->_base.tag = FBLE_PROC_VALUE;
  get->argc = 0;
  get->code = &code;
  get->code->refcount++;
  get->scope[0] = port;
  FbleValueAddRef(heap, &get->_base, port);
  return &get->_base;
}

// FbleNewInputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewInputPortValue(FbleValueHeap* heap, FbleValue** data)
{
  FblePortValue* get_port = FbleNewValue(heap, FblePortValue);
  get_port->_base.tag = FBLE_PORT_VALUE;
  get_port->data = data;

  FbleValue* get = FbleNewGetValue(heap, &get_port->_base);
  FbleReleaseValue(heap, &get_port->_base);
  return get;
}

// FbleNewPutValue -- see documentation in value.h
FbleValue* FbleNewPutValue(FbleValueHeap* heap, FbleValue* link)
{
  static FblePutInstr iput = {
    ._base = { .tag = FBLE_PUT_INSTR, .profile_ops = NULL },
    .port = { .section = FBLE_STATICS_FRAME_SECTION, .index = 0},
    .arg = { .section = FBLE_STATICS_FRAME_SECTION, .index = 1},
    .dest = 0,
  };

  static FbleReturnInstr irtn0 = {
    ._base = { .tag = FBLE_RETURN_INSTR, .profile_ops = NULL },
    .result = { .section = FBLE_LOCALS_FRAME_SECTION, .index = 0}
  };

  static FbleInstr* proc_instrs[] = {
    &iput._base,
    &irtn0._base,
  };

  static FbleInstrBlock proc_code = {
    .refcount = 1,
    .magic = FBLE_INSTR_BLOCK_MAGIC,
    .statics = 2,  // port, arg
    .locals = 1,   // result
    .instrs = { .size = 2, .xs = proc_instrs }
  };

  static FbleFrameIndex proc_scope[] = {
    { .section = FBLE_STATICS_FRAME_SECTION, .index = 0 },  // port
    { .section = FBLE_LOCALS_FRAME_SECTION, .index = 0 },   // arg
  };

  static FbleProcValueInstr iproc = {
    ._base = { .tag = FBLE_PROC_VALUE_INSTR, .profile_ops = NULL },
    .argc = 0,
    .code = &proc_code,
    .scope = { .size = 2, .xs = proc_scope },
    .dest = 1,
  };
  iproc.code->refcount++;

  static FbleReleaseInstr irls1 = {
    ._base = { .tag = FBLE_RELEASE_INSTR, .profile_ops = NULL },
    .value = 0
  };

  static FbleReturnInstr irtn1 = {
    ._base = { .tag = FBLE_RETURN_INSTR, .profile_ops = NULL },
    .result = { .section = FBLE_LOCALS_FRAME_SECTION, .index = 1 }
  };

  static FbleInstr* func_instrs[] = {
    &iproc._base,
    &irls1._base,
    &irtn1._base,
  };

  static FbleInstrBlock func_code = {
    .refcount = 1,
    .magic = FBLE_INSTR_BLOCK_MAGIC,
    .statics = 1,  // port
    .locals = 2,   // arg, result
    .instrs = { .size = 3, .xs = func_instrs }
  };
  func_code.refcount++;

  FbleFuncValue* put = FbleNewValueExtra(heap, FbleFuncValue, sizeof(FbleValue*));
  put->_base.tag = FBLE_FUNC_VALUE;
  put->argc = 1;
  put->code = &func_code;
  put->scope[0] = link;
  FbleValueAddRef(heap, &put->_base, link);
  return &put->_base;
}

// FbleNewOutputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewOutputPortValue(FbleValueHeap* heap, FbleValue** data)
{
  FblePortValue* port_value = FbleNewValue(heap, FblePortValue);
  port_value->_base.tag = FBLE_PORT_VALUE;
  port_value->data = data;
  FbleValue* put = FbleNewPutValue(heap, &port_value->_base);
  FbleReleaseValue(heap, &port_value->_base);
  return put;
}
