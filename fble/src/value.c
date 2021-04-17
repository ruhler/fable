// value.c
//   This file implements routines associated with fble values.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "code.h"
#include "fble-interpret.h"
#include "heap.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

static void OnFree(FbleValueHeap* heap, FbleValue* value);
static void Ref(FbleHeapCallback* callback, FbleValue* value);
static void Refs(FbleHeapCallback* callback, FbleValue* value);


// FbleNewValueHeap -- see documentation in fble-.h
FbleValueHeap* FbleNewValueHeap()
{
  return FbleNewHeap(
      (void (*)(FbleHeapCallback*, void*))&Refs,
      (void (*)(FbleHeap*, void*))&OnFree);
}

// FbleFreeValueHeap -- see documentation in fble.h
void FbleFreeValueHeap(FbleValueHeap* heap)
{
  FbleFreeHeap(heap);
}

// FbleRetainValue -- see documentation in fble-value.h
void FbleRetainValue(FbleValueHeap* heap, FbleValue* value)
{
  FbleRetainHeapObject(heap, value);
}

// FbleReleaseValue -- see documentation in fble-value.h
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value)
{
  if (value != NULL) {
    FbleReleaseHeapObject(heap, value);
  }
}

// FbleValueAddRef -- see documentation in fble-value.h
void FbleValueAddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst)
{
  FbleHeapObjectAddRef(heap, src, dst);
}

// FbleValueFullGc -- see documentation in fble-value.h
void FbleValueFullGc(FbleValueHeap* heap)
{
  FbleHeapFullGc(heap);
}

// OnFree --
//   The 'on_free' function for values. See documentation in heap.h
static void OnFree(FbleValueHeap* heap, FbleValue* value)
{
  switch (value->tag) {
    case FBLE_TYPE_VALUE: return;
    case FBLE_STRUCT_VALUE: return;
    case FBLE_UNION_VALUE: return;

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* v = (FbleFuncValue*)value;
      FbleFreeExecutable(v->executable);
      return;
    }

    case FBLE_LINK_VALUE: {
      FbleLinkValue* v = (FbleLinkValue*)value;
      FbleValues* curr = v->head;
      while (curr != NULL) {
        FbleValues* tmp = curr;
        curr = curr->next;
        FbleFree(tmp);
      }
      return;
    }

    case FBLE_PORT_VALUE: return;
    case FBLE_REF_VALUE: return;
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
    case FBLE_TYPE_VALUE: break;

    case FBLE_STRUCT_VALUE: {
      FbleStructValue* sv = (FbleStructValue*)value;
      for (size_t i = 0; i < sv->fieldc; ++i) {
        Ref(callback, sv->fields[i]);
      }
      break;
    }

    case FBLE_UNION_VALUE: {
      FbleUnionValue* uv = (FbleUnionValue*)value;
      Ref(callback, uv->arg);
      break;
    }

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* v = (FbleFuncValue*)value;
      for (size_t i = 0; i < v->executable->statics; ++i) {
        Ref(callback, v->statics[i]);
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
      FbleRefValue* v = (FbleRefValue*)value;
      Ref(callback, v->value);
      break;
    }
  }
}

// FbleNewStructValue -- see documentation in fble-value.h
FbleValue* FbleNewStructValue(FbleValueHeap* heap, FbleValueV args)
{
  FbleStructValue* value = FbleNewValueExtra(heap, FbleStructValue, sizeof(FbleValue*) * args.size);
  value->_base.tag = FBLE_STRUCT_VALUE;
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
  object = FbleStrictValue(object);
  assert(object != NULL && object->tag == FBLE_STRUCT_VALUE);
  FbleStructValue* value = (FbleStructValue*)object;
  assert(field < value->fieldc);
  return value->fields[field];
}

// FbleNewUnionValue -- see documentation in fble-value.h
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue* arg)
{
  FbleUnionValue* union_value = FbleNewValue(heap, FbleUnionValue);
  union_value->_base.tag = FBLE_UNION_VALUE;
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
  object = FbleStrictValue(object);
  assert(object != NULL && object->tag == FBLE_UNION_VALUE);
  FbleUnionValue* value = (FbleUnionValue*)object;
  return value->tag;
}

// FbleUnionValueAccess -- see documentation in fble-value.h
FbleValue* FbleUnionValueAccess(FbleValue* object)
{
  object = FbleStrictValue(object);
  assert(object != NULL && object->tag == FBLE_UNION_VALUE);
  FbleUnionValue* value = (FbleUnionValue*)object;
  return value->arg;
}
// FbleFreeExecutable -- see documentation in value.h
void FbleFreeExecutable(FbleExecutable* executable)
{
  if (executable == NULL) {
    return;
  }

  // We've had trouble with double free in the past. Check to make sure the
  // magic in the block hasn't been corrupted. Otherwise we've probably
  // already freed this executable and decrementing the refcount could end up
  // corrupting whatever is now making use of the memory that was previously
  // used for the instruction block.
  assert(executable->magic == FBLE_EXECUTABLE_MAGIC && "corrupt FbleExecutable");

  assert(executable->refcount > 0);
  executable->refcount--;
  if (executable->refcount == 0) {
    executable->on_free(executable);
    FbleFree(executable);
  }
}

// FbleNewFuncValue -- see documentation in value.h
FbleFuncValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable)
{
  FbleFuncValue* v = FbleNewValueExtra(heap, FbleFuncValue, sizeof(FbleValue*) * executable->statics);
  v->_base.tag = FBLE_FUNC_VALUE;
  v->executable = executable;
  v->executable->refcount++;
  return v;
}

// FbleIsProcValue -- see documentation in fble-value.h
bool FbleIsProcValue(FbleValue* value)
{
  FbleProcValue* proc = (FbleProcValue*)value;
  return value->tag == FBLE_PROC_VALUE && proc->executable->args == 0;
}

// FbleNewGetValue -- see documentation in value.h
FbleValue* FbleNewGetValue(FbleValueHeap* heap, FbleValue* port)
{
  assert(port->tag == FBLE_LINK_VALUE || port->tag == FBLE_PORT_VALUE);

  // statics[0] is the port.
  // locals[0] is the result.
  FbleCode* code = FbleNewCode(0, 1, 1);

  FbleGetInstr* iget = FbleAlloc(FbleGetInstr);
  iget->_base.tag = FBLE_GET_INSTR;
  iget->_base.profile_ops = NULL;
  iget->port.section = FBLE_STATICS_FRAME_SECTION;
  iget->port.index = 0;
  iget->dest = 0;
  FbleVectorAppend(code->instrs, &iget->_base);

  FbleReturnInstr* irtn = FbleAlloc(FbleReturnInstr);
  irtn->_base.tag = FBLE_RETURN_INSTR;
  irtn->_base.profile_ops = NULL;
  irtn->result.section = FBLE_LOCALS_FRAME_SECTION;
  irtn->result.index = 0;
  FbleVectorAppend(code->instrs, &irtn->_base);

  FbleProcValue* get = FbleNewFuncValue(heap, &code->_base);
  get->statics[0] = port;
  FbleValueAddRef(heap, &get->_base, port);

  FbleFreeCode(code);
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
  // statics[0] is port
  // statics[1] is arg
  // locals[0] is result.
  FbleCode* proc_code = FbleNewCode(0, 2, 1);

  FblePutInstr* iput = FbleAlloc(FblePutInstr);
  iput->_base.tag = FBLE_PUT_INSTR;
  iput->_base.profile_ops = NULL;
  iput->port.section = FBLE_STATICS_FRAME_SECTION;
  iput->port.index = 0;
  iput->arg.section = FBLE_STATICS_FRAME_SECTION;
  iput->arg.index = 1;
  iput->dest = 0;
  FbleVectorAppend(proc_code->instrs, &iput->_base);

  FbleReturnInstr* irtn0 = FbleAlloc(FbleReturnInstr);
  irtn0->_base.tag = FBLE_RETURN_INSTR;
  irtn0->_base.profile_ops = NULL;
  irtn0->result.section = FBLE_LOCALS_FRAME_SECTION;
  irtn0->result.index = 0;
  FbleVectorAppend(proc_code->instrs, &irtn0->_base);

  // statics[0] = port
  // locals[0] = arg
  // locals[1] = result
  FbleCode* func_code = FbleNewCode(1, 1, 2);

  FbleProcValueInstr* iproc = FbleAlloc(FbleProcValueInstr);
  iproc->_base.tag = FBLE_PROC_VALUE_INSTR;
  iproc->_base.profile_ops = NULL;
  iproc->code = proc_code;
  FbleVectorInit(iproc->scope);

  FbleFrameIndex port_index = { .section = FBLE_STATICS_FRAME_SECTION, .index = 0 };
  FbleVectorAppend(iproc->scope, port_index);

  FbleFrameIndex arg_index = { .section = FBLE_LOCALS_FRAME_SECTION, .index = 0 };
  FbleVectorAppend(iproc->scope, arg_index);
  iproc->dest = 1;

  FbleVectorAppend(func_code->instrs, &iproc->_base);

  FbleReturnInstr* irtn1 = FbleAlloc(FbleReturnInstr);
  irtn1->_base.tag = FBLE_RETURN_INSTR;
  irtn1->_base.profile_ops = NULL;
  irtn1->result.section = FBLE_LOCALS_FRAME_SECTION;
  irtn1->result.index = 1;
  FbleVectorAppend(func_code->instrs, &irtn1->_base);

  FbleFuncValue* put = FbleNewFuncValue(heap, &func_code->_base);
  put->statics[0] = link;
  FbleValueAddRef(heap, &put->_base, link);
  FbleFreeCode(func_code);
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

// FbleStrictValue -- see documentation in value.h
FbleValue* FbleStrictValue(FbleValue* value)
{
  while (value != NULL && value->tag == FBLE_REF_VALUE) {
    FbleRefValue* ref = (FbleRefValue*)value;
    value = ref->value;
  }
  return value;
}
