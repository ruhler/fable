/**
 * @file value.h
 *  Header for FbleValue.
 */

#ifndef FBLE_INTERNAL_VALUE_H_
#define FBLE_INTERNAL_VALUE_H_

#include <fble/fble-value.h>

#include <fble/fble-execute.h>    // for FbleFunction

/** Different kinds of FbleValue. */
typedef enum {
  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_FUNC_VALUE,
  FBLE_REF_VALUE,
} FbleValueTag;

/**
 * Base class for fble values.
 *
 * A tagged union of value types. All values have the same initial layout as
 * FbleValue. The tag can be used to determine what kind of value this is to
 * get access to additional fields of the value by first casting to that
 * specific type of value.
 */
struct FbleValue {
  FbleValueTag tag;   /**< The kind of value. */
};

/**
 * FBLE_FUNC_VALUE: An fble function value.
 */
typedef struct {
  FbleValue _base;              /**< FbleValue base class. */
  FbleFunction function;        /**< Function information. */
  FbleValue* statics[];         /**< Storage location for static variables. */
} FbleFuncValue;

#endif // FBLE_INTERNAL_VALUE_H_
