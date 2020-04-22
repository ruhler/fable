// value.h --
//   Header file describing the internal interface for working with fble
//   values.
//   This is an internal library interface.

#ifndef FBLE_INTERNAL_VALUE_H_
#define FBLE_INTERNAL_VALUE_H_

#include "fble.h"
#include "instr.h"

// FbleValueTag --
//   A tag used to distinguish among different kinds of values.
typedef enum {
  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_FUNC_VALUE,
  FBLE_PROC_VALUE,
  FBLE_LINK_VALUE,
  FBLE_PORT_VALUE,
  FBLE_REF_VALUE,
  FBLE_TYPE_VALUE,
} FbleValueTag;

// FbleValue --
//   A tagged union of value types. All values have the same initial
//   layout as FbleValue. The tag can be used to determine what kind of
//   value this is to get access to additional fields of the value
//   by first casting to that specific type of value.
struct FbleValue {
  FbleValueTag tag;
};

// FbleStructValue --
//   FBLE_STRUCT_VALUE
typedef struct {
  FbleValue _base;
  size_t fieldc;
  FbleValue* fields[];
} FbleStructValue;

// FbleUnionValue --
//   FBLE_UNION_VALUE
typedef struct {
  FbleValue _base;
  size_t tag;
  FbleValue* arg;
} FbleUnionValue;

typedef enum {
  FBLE_BASIC_FUNC_VALUE,
  FBLE_THUNK_FUNC_VALUE,
  FBLE_PUT_FUNC_VALUE,
} FbleFuncValueTag;

// FbleFuncValue -- FBLE_FUNC_VALUE
//   A tagged union of func value types. All values have the same initial
//   layout as FbleFuncValue. The tag can be used to determine what kind of
//   func value this is to get access to additional fields of the func value
//   by first casting to that specific type of func value.
//
// Fields:
//   argc - The number of arguments to be applied to this function before the
//          function body is executed.
typedef struct {
  FbleValue _base;
  FbleFuncValueTag tag;
  size_t argc;
} FbleFuncValue;

// FbleBasicFuncValue -- FBLE_BASIC_FUNC_VALUE
//
// Fields:
//   code - The block of instructions representing the body of the function,
//          which should pop the arguments and context.
//   scopec - The number of values on the function scope.
//   scope - The scope at the time the function was created, representing the
//           lexical context available to the function.
typedef struct {
  FbleFuncValue _base;
  FbleInstrBlock* code;
  size_t scopec;
  FbleValue* scope[];
} FbleBasicFuncValue;

// FbleThunkFuncValue -- FBLE_THUNK_FUNC_VALUE
//   A function value that is the partial application of another function to
//   an argument.
//
// The value of this function value is: func[arg]
typedef struct {
  FbleFuncValue _base;
  FbleFuncValue* func;
  FbleValue* arg;
} FbleThunkFuncValue;

// FblePutFuncValue -- FBLE_PUT_FUNC_VALUE
//   A process put function. Given an argument, it returns a process to put
//   that argument onto the associated put port.
typedef struct {
  FbleFuncValue _base;
  FbleValue* port;
} FblePutFuncValue;

// FbleProcValue -- FBLE_PROC_VALUE
typedef struct {
  FbleValue _base;
  FbleValueV scope;
  FbleInstrBlock* code;
} FbleProcValue;

// FbleValues --
//   A non-circular singly linked list of values.
typedef struct FbleValues {
  FbleValue* value;
  struct FbleValues* next;
} FbleValues;

// FbleLinkValue -- FBLE_LINK_VALUE
//   Holds the list of values on a link. Values are added to the tail and taken
//   from the head. If there are no values on the list, both head and tail are
//   set to NULL.
typedef struct {
  FbleValue _base;
  FbleValues* head;
  FbleValues* tail;
} FbleLinkValue;

// FblePortValue --
//   FBLE_PORT_VALUE
//
// Use for input and output values linked to external IO.
typedef struct {
  FbleValue _base;
  size_t id;
} FblePortValue;

// FbleRefValue --
//   FBLE_REF_VALUE
//
// A implementation-specific value introduced to support recursive values. A
// ref value is simply a reference to another value. All values must be
// dereferenced before being otherwise accessed in case they are reference
// values.
//
// Fields:
//   value - the value being referenced, or NULL if no value is referenced.
typedef struct FbleRefValue {
  FbleValue _base;
  FbleValue* value;
} FbleRefValue;

// FbleTypeValue --
//   FBLE_TYPE_VALUE
//
// A value representing a type. Because types are compile-time concepts, not
// runtime concepts, the type value contains no information.
typedef struct FbleTypeValue {
  FbleValue _base;
} FbleTypeValue;

// FbleNewGetProcValue --
//   Create a new get proc value for the given link.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   port - the port value to get from.
//
// Results:
//   A newly allocated get value.
//
// Side effects:
//   The returned get value must be freed using FbleValueRelease when no
//   longer in use. This function does not take ownership of the port value.
//   argument.
FbleValue* FbleNewGetProcValue(FbleValueHeap* heap, FbleValue* port);

#endif // FBLE_INTERNAL_VALUE_H_
