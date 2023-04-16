/**
 * @file tc.h
 * Header for FbleTc, typed abstract syntax for fble.
 */

#ifndef FBLE_INTERNAL_TC_H_
#define FBLE_INTERNAL_TC_H_

#include <fble/fble-loc.h>
#include <fble/fble-name.h>

#include "kind.h"       // for FbleDataTypeTag
#include "var.h"        // for FbleVar

/**
 * Abstract syntax for already type-checked fble expressions.//
 *
 * FbleTc is like FbleExpr, except that:
 * * Field and variable names are replaced with integer indicies.
 * * Types are eliminated.
 */
typedef struct FbleTc FbleTc;

/** Vector of FbleTc. */
typedef struct {
  size_t size;  /**< Number of elements. */
  FbleTc** xs;  /**< The elements. */
} FbleTcV;

/**
 * Different kinds of FbleTc.
 */
typedef enum {
  FBLE_TYPE_VALUE_TC,
  FBLE_VAR_TC,
  FBLE_LET_TC,
  FBLE_STRUCT_VALUE_TC,
  FBLE_UNION_VALUE_TC,
  FBLE_UNION_SELECT_TC,
  FBLE_DATA_ACCESS_TC,
  FBLE_FUNC_VALUE_TC,
  FBLE_FUNC_APPLY_TC,
  FBLE_LIST_TC,
  FBLE_LITERAL_TC,
} FbleTcTag;

/**
 * Magic number used to detect double free in FbleTc.
 */
#define FBLE_TC_MAGIC 0x5443

/**
 * Base class for FbleTc types.
 *
 * A tagged union of tc types. All tcs have the same initial layout as FbleTc.
 * The tag can be used to determine what kind of tc this is to get access to
 * additional fields of the value by first casting to that specific type of
 * tc.
 *
 * FbleTc is reference counted. Pass by pointer. Explicit copy and free
 * required. The magic field is set to FBLE_TC_MAGIC and is used to detect
 * double frees of FbleTc.
 */
struct FbleTc {
  size_t refcount;    /**< Reference count. */
  size_t magic;       /**< FBLE_TC_MAGIC. */
  FbleTcTag tag;      /**< Kind of FbleTc. */

  /**
   * The location of the start of the expression in source code. Used for
   * general purpose debug information.
   */
  FbleLoc loc;
};

/**
 * FBLE_TYPE_VALUE_TC: Computes the type value.
 */
typedef struct {
  FbleTc _base;     /**< FbleTc base class */
} FbleTypeValueTc;

/**
 * FBLE_VAR_TC: A variable expression.
 *
 * Used to represent local variables, static variables, and arguments to
 * functions.
 *
 * For args, index starts at 0 and increases by one for each argument going
 * from left to right.
 *
 * For local variables, index starts at 0 and increases by one for each new
 * variable introduced, going from left to right, outer most to inner most
 * binding.
 */
typedef struct {
  FbleTc _base;   /**< FbleTc base class. */
  FbleVar var;    /**< Identifies the variable. */
} FbleVarTc;

/**
 * Information for a binding.
 *
 * Used for let bindings, exec bindings, and case branches.
 */
typedef struct {
  FbleName name;  /**< The name of the variable or branch. */
  FbleLoc loc;    /**< The location of the value. */
  FbleTc* tc;     /**< The value of the binding. */
} FbleTcBinding;

/** Vector of FbleTcBinding. */
typedef struct {
  size_t size;        /**< Number of elements. */
  FbleTcBinding* xs;  /**< The elements. */
} FbleTcBindingV;

/**
 * FBLE_LET_TC: A let expression.
 *
 * The bindings are bound to variables implicitly based on the position of the
 * binding in the let expression and the position of the let expression in its
 * parent expression as specified for FbleVar.
 */
typedef struct {
  FbleTc _base;             /**< FbleTc base class. */

  /** false if the let is a non-recursive let expression. */
  bool recursive;           
  FbleTcBindingV bindings;  /**< The variables being defined. */
  FbleTc* body;             /**< The body of the let. */
} FbleLetTc;

/**
 * FBLE_STRUCT_VALUE_TC: A struct value expression.
 */
typedef struct {
  FbleTc _base;       /**< FbleTc base class. */
  size_t fieldc;      /**< Number of arguments to the struct value. */
  FbleTc* fields[];   /**< Arguments to the struct value. */
} FbleStructValueTc;

/**
 * FBLE_UNION_VALUE_TC: A union value expression.
 */
typedef struct {
  FbleTc _base;       /**< FbleTc base class. */
  size_t tag;         /**< Tag of the union value to create. */
  FbleTc* arg;        /**< Argument to the union value to create. */
} FbleUnionValueTc;

/** Target of a union select branch. */
typedef struct {
  size_t tag;
  FbleTcBinding target;
} FbleTcBranchTarget;

/** Vector of FbleTcBranchTarget. */
typedef struct {
  size_t size;
  FbleTcBranchTarget* xs;
} FbleTcBranchTargetV;

/**
 * FBLE_UNION_SELECT_TC: A union select expression.
 *
 * * Targets must be listed in tag order.
 * * A default target is required.
 * * Not all tags need be present in the list of non-default targets.
 */
typedef struct {
  FbleTc _base;             /**< FbleTc base class. */
  FbleTc* condition;        /**< The condition to the union select. */
  size_t num_tags;          /**< Number of possible tags for condition. */
  FbleTcBranchTargetV targets;
  FbleTcBinding default_;
} FbleUnionSelectTc;

/**
 * FBLE_DATA_ACCESS_TC: Struct and union access expressions.
 */
typedef struct {
  FbleTc _base;               /**< FbleTc base class. */
  FbleDataTypeTag datatype;   /**< Whether this is struct or union access. */
  FbleTc* obj;                /**< The object to access a field of. */
  size_t tag;                 /**< The field to access. */
  FbleLoc loc;                /**< Location to use for error reporting. */
} FbleDataAccessTc;

/**
 * FBLE_FUNC_VALUE_TC: A function value.
 */
typedef struct {
  FbleTc _base;         /**< FbleTc base class. */
  FbleLoc body_loc;     /**< Location of the body. */
  FbleVarV scope;       /**< Sources of static variables. */
  FbleNameV statics;    /**< Names of static variables. */
  FbleNameV args;       /**< Names of arguments. */
  FbleTc* body;         /**< The body of the function. */
} FbleFuncValueTc;

/**
 * FBLE_FUNC_APPLY_TC: Function application.
 */
typedef struct {
  FbleTc _base;       /**< FbleTc base class. */
  FbleTc* func;       /**< The function to apply. */
  FbleTcV args;       /**< Arguments to function to apply. */
} FbleFuncApplyTc;

/**
 * FBLE_LIST_TC: List part of a list expression.
 *
 * An expression to construct the list value that will be passed to the
 * function as part of a list expression.
 */
typedef struct {
  FbleTc _base;       /**< FbleTc base class. */
  size_t fieldc;      /**< Number of list elements. */
  FbleTc* fields[];   /**< List elements. */
} FbleListTc;

/**
 * FBLE_LITERAL_TC: Literal part of a literal expression.
 *
 * An expression to construct the list value that will be passed to the
 * function as part of a literal expression.
 *
 * letters[i] is the tag value to use for the ith letter in the literal.
 */
typedef struct {
  FbleTc _base;       /**< FbleTc base class. */
  size_t letterc;     /**< Number of letters in the literal. */
  size_t letters[];   /**< Tag values for letters in the literal. */
} FbleLiteralTc;

/**
 * Allocates a new tc. This function is not type safe.
 *
 * @param T  The type of the tc to allocate.
 * @param tag  The tag of the tc.
 * @param loc  The source loc of the tc. Borrowed.
 *
 * @returns The newly allocated tc.
 *
 * @sideeffects
 *   Allocates a new tc that should be freed using FbleFreeTc when no longer
 *   needed.
 */
#define FbleNewTc(T, tag, loc) ((T*) FbleNewTcRaw(sizeof(T), tag, loc))

/**
 * Allocates a new tc. This function is not type safe.
 *
 * @param size The number of bytes to allocate.
 * @param tag  The tag of the tc.
 * @param loc  The source loc of the tc. Borrowed.
 *
 * @returns The newly allocated tc.
 *
 * @sideeffects
 *   Allocates a new tc that should be freed using FbleFreeTc when no longer
 *   needed.
 */
FbleTc* FbleNewTcRaw(size_t size, FbleTcTag tag, FbleLoc loc);

/**
 * Allocates a new tc with additional extra space.
 *
 * @param T  The type of the tc to allocate.
 * @param size  The size in bytes of additional extra space to include.
 * @param tag  The tag of the tc.
 * @param loc  The source loc of the tc. Borrowed.
 *
 * @returns The newly allocated tc.
 *
 * @sideeffects
 *   Allocates a new tc that should be freed using FbleFreeTc when no longer
 *   needed.
 */
#define FbleNewTcExtra(T, tag, size, loc) ((T*) FbleNewTcRaw(sizeof(T) + size, tag, loc))

/**
 * Makes a reference counted copy of the given tc.
 *
 * @param tc  The tc to copy.
 *
 * @returns
 *   The copy of the tc.
 *
 * @sideeffects
 *   The user should arrange for FbleFreeTc to be called on this tc when it is
 *   no longer needed.
 */
FbleTc* FbleCopyTc(FbleTc* tc);

/**
 * Frees resources associated with an FbleTc.
 *
 * @param tc  The tc to free. May be NULL.
 *
 * @sideeffects
 *   Frees all resources associated with the given tc.
 */
void FbleFreeTc(FbleTc* tc);

#endif // FBLE_INTERNAL_TC_H_
