/**
 * @file tc.h
 *  Header for FbleTc, typed abstract syntax for fble.
 */

#ifndef FBLE_INTERNAL_TC_H_
#define FBLE_INTERNAL_TC_H_

#include <fble/fble-loc.h>
#include <fble/fble-name.h>

#include "tag.h"        // for FbleTagV
#include "var.h"        // for FbleVar

/**
 * Abstract syntax for already type-checked fble expressions.
 *
 * FbleTc is like FbleExpr, except that:
 * * Field and variable names are replaced with integer indicies.
 * * Types are eliminated.
 */
typedef struct FbleTc FbleTc;

/**
 * @struct[FbleTcV] Vector of FbleTc.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleTc**][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleTc** xs;
} FbleTcV;

/**
 * Different kinds of FbleTc.
 */
typedef enum {
  FBLE_TYPE_VALUE_TC,
  FBLE_VAR_TC,
  FBLE_LET_TC,
  FBLE_UNDEF_TC,
  FBLE_STRUCT_VALUE_TC,
  FBLE_STRUCT_ACCESS_TC,
  FBLE_STRUCT_COPY_TC,
  FBLE_UNION_VALUE_TC,
  FBLE_UNION_ACCESS_TC,
  FBLE_UNION_SELECT_TC,
  FBLE_FUNC_VALUE_TC,
  FBLE_FUNC_APPLY_TC,
  FBLE_LIST_TC,
  FBLE_LITERAL_TC,
} FbleTcTag;

/**
 * Magic number used to detect double free in FbleTc.
 */
typedef enum {
  FBLE_TC_MAGIC = 0x5443
} FbleTcMagic;

/**
 * @struct[FbleTc] Base class for FbleTc types.
 *  A tagged union of tc types. All tcs have the same initial layout as
 *  FbleTc.  The tag can be used to determine what kind of tc this is to get
 *  access to additional fields of the value by first casting to that specific
 *  type of tc.
 *
 *  FbleTc is reference counted. Pass by pointer. Explicit copy and free
 *  required. The magic field is set to FBLE_TC_MAGIC and is used to detect
 *  double frees of FbleTc.
 *
 *  @field[size_t][refcount] Reference count.
 *  @field[FbleTcMagic][magic] FBLE_TC_MAGIC.
 *  @field[FbleTcTag][tag] Kind of FbleTc.
 *  @field[FbleLoc][loc]
 *   The location of the start of the expression in source code. Used for
 *   general purpose debug information.
 */
struct FbleTc {
  size_t refcount;
  FbleTcMagic magic;
  FbleTcTag tag;
  FbleLoc loc;
};

/**
 * @struct[FbleTypeValueTc] FBLE_TYPE_VALUE_TC
 *  Computes the type value.
 *
 *  @field[FbleTc][_base] FbleTc base class
 */
typedef struct {
  FbleTc _base;
} FbleTypeValueTc;

/**
 * @struct[FbleVarTc] FBLE_VAR_TC
 *  A variable expression. Used to represent local variables, static
 *  variables, and arguments to functions.
 *
 *  For args, index starts at 0 and increases by one for each argument going
 *  from left to right.
 *
 *  For local variables, index starts at 0 and increases by one for each new
 *  variable introduced, going from left to right, outer most to inner most
 *  binding.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[FbleVar][var] Identifies the variable.
 */
typedef struct {
  FbleTc _base;
  FbleVar var;
} FbleVarTc;

/**
 * @struct[FbleTcBinding] Information for a binding.
 *  Used for let bindings, exec bindings, and case branches.
 *
 *  @field[FbleName][name] The name of the variable or branch.
 *  @field[FbleLoc][loc] The location of the value.
 *  @field[FbleTc*][tc] The value of the binding.
 */
typedef struct {
  FbleName name;
  FbleLoc loc;
  FbleTc* tc;
} FbleTcBinding;

/**
 * @struct[FbleTcBindingV] Vector of FbleTcBinding.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleTcBinding*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleTcBinding* xs;
} FbleTcBindingV;

/**
 * @struct[FbleLetTc] FBLE_LET_TC
 *  A let expression.
 *
 *  The bindings are bound to variables implicitly based on the position of
 *  the binding in the let expression and the position of the let expression
 *  in its parent expression as specified for FbleVar.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[bool][recursive] false if the let is a non-recursive let expression.
 *  @field[FbleTcBindingV][bindings] The variables being defined.
 *  @field[FbleTc*][body] The body of the let.
 */
typedef struct {
  FbleTc _base;
  bool recursive;           
  FbleTcBindingV bindings;
  FbleTc* body;
} FbleLetTc;

/**
 * @struct[FbleUndefTc] FBLE_UNDEF_TC
 *  An undef expression.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[FbleName][name] Name of the undefined variable.
 *  @field[FbleTc*][body] The body of the let
 */
typedef struct {
  FbleTc _base;
  FbleName name;
  FbleTc* body;
} FbleUndefTc;

/**
 * @struct[FbleStructValueTc] FBLE_STRUCT_VALUE_TC
 *  A struct value expression.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[FbleTcV][fields] Arguments to the struct value.
 */
typedef struct {
  FbleTc _base;
  FbleTcV fields;
} FbleStructValueTc;

/**
 * @struct[FbleStructAccessTc] FBLE_STRUCT_ACCESS_TC
 *  Struct access expression.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[FbleTc*][obj] The object to access a field of.
 *  @field[size_t][fieldc] The number of fields in the type.
 *  @field[size_t][field] The field to access.
 *  @field[FbleLoc][loc] Location to use for error reporting.
 */
typedef struct {
  FbleTc _base;
  FbleTc* obj;
  size_t fieldc;
  size_t field;
  FbleLoc loc;
} FbleStructAccessTc;

/**
 * @struct[FbleStructCopyTc] FBLE_STRUCT_COPY_TC
 *  A struct copy expression.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[FbleTc*][source] The source object.
 *  @field[FbleTcV][fields]
 *   Arguments to the struct value, or NULL to take from source.
 */
typedef struct {
  FbleTc _base;
  FbleTc* source;
  FbleTcV fields;
} FbleStructCopyTc;

/**
 * @struct[FbleUnionValueTc] FBLE_UNION_VALUE_TC
 *  A union value expression.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[size_t][tagwidth] Number of bits needed for the tag.
 *  @field[size_t][tag] Tag of the union value to create.
 *  @field[FbleTc*][arg] Argument to the union valu to create.
 */
typedef struct {
  FbleTc _base;
  size_t tagwidth;
  size_t tag;
  FbleTc* arg;
} FbleUnionValueTc;

/**
 * @struct[FbleUnionAccessTc] FBLE_UNION_ACCESS_TC
 *  Union access expression.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[FbleTc*][obj] The object to access a field of.
 *  @field[size_t][tagwidth] The number of bits needed for the tag.
 *  @field[size_t][tag] The field to access.
 *  @field[FbleLoc][loc] Location to use for error reporting.
 */
typedef struct {
  FbleTc _base;
  FbleTc* obj;
  size_t tagwidth;
  size_t tag;
  FbleLoc loc;
} FbleUnionAccessTc;


/**
 * @struct[FbleTcBranchTarget] Target of a union select branch.
 *  @field[size_t][tag] Tag for the branch target.
 *  @field[FbleTcBinding][target] The branch target.
 */
typedef struct {
  size_t tag;
  FbleTcBinding target;
} FbleTcBranchTarget;

/**
 * @struct[FbleTcBranchTargetV] Vector of FbleTcBranchTarget.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleTcBranchTarget*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleTcBranchTarget* xs;
} FbleTcBranchTargetV;

/**
 * @struct[FbleUnionSelectTc] FBLE_UNION_SELECT_TC
 *  A union select expression.
 *
 *  @i Targets must be listed in tag order.
 *  @i A default target is required.
 *  @i Not all tags need be present in the list of non-default targets.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[FbleTc*][condition] The condition to the union select.
 *  @field[size_t][num_tags] Number of possible tags for condition.
 *  @field[FbleTcBranchTargetV][targets] Non-default targets.
 *  @field[FbleTcBinding][default_] The default target.
 */
typedef struct {
  FbleTc _base;
  FbleTc* condition;
  size_t num_tags;
  FbleTcBranchTargetV targets;
  FbleTcBinding default_;
} FbleUnionSelectTc;

/**
 * @struct[FbleFuncValueTc] FBLE_FUNC_VALUE_TC
 *  A function value. Supports multi-argument functions.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[FbleLoc][body_loc] Location of the body.
 *  @field[FbleVarV][scope] Sources of static variables.
 *  @field[FbleNameV][statics] Names of static variables.
 *  @field[FbleNameV][args] Names of arguments.
 *  @field[FbleTc*][body] The body of the function.
 */
typedef struct {
  FbleTc _base;
  FbleLoc body_loc;
  FbleVarV scope;
  FbleNameV statics;
  FbleNameV args;
  FbleTc* body;
} FbleFuncValueTc;

/**
 * @struct[FbleFuncApplyTc] FBLE_FUNC_APPLY_TC
 *  Function application.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[FbleTc*][func] The function to apply.
 *  @field[FbleTc*][arg] Argument to function to apply.
 */
typedef struct {
  FbleTc _base;
  FbleTc* func;
  FbleTc* arg;
} FbleFuncApplyTc;

/**
 * @struct[FbleListTc] FBLE_LIST_TC
 *  List part of a list expression.
 *
 *  An expression to construct the list value that will be passed to the
 *  function as part of a list expression.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[FbleTcV][fields] The elements of the list.
 */
typedef struct {
  FbleTc _base;
  FbleTcV fields;
} FbleListTc;

/**
 * @struct[FbleLiteralTc] FBLE_LITERAL_TC
 *  Literal part of a literal expression.
 *
 *  An expression to construct the list value that will be passed to the
 *  function as part of a literal expression.
 *
 *  @field[FbleTc][_base] FbleTc base class.
 *  @field[size_t][tagwidth] Number of bits in the tag of a letter.
 *  @field[FbleTagV][letters] Tag values for letters in the literal.
 */
typedef struct {
  FbleTc _base;
  size_t tagwidth;
  FbleTagV letters;
} FbleLiteralTc;

/**
 * @func[FbleNewTc] Allocates a new tc.
 *  This function is not type safe.
 *
 *  @arg[<type>][T] The type of the tc to allocate.
 *  @arg[FbleTcTag][tag] The tag of the tc.
 *  @arg[FbleLoc][loc] The source loc of the tc. Borrowed.
 *
 *  @returns[T*] The newly allocated tc.
 *
 *  @sideeffects
 *   Allocates a new tc that should be freed using FbleFreeTc when no longer
 *   needed.
 */
#define FbleNewTc(T, tag, loc) ((T*) FbleNewTcRaw(sizeof(T), tag, loc))

/**
 * @func[FbleNewTcRaw] Allocates a new tc.
 *  This function is not type safe.
 *
 *  @arg[size_t][size] The number of bytes to allocate.
 *  @arg[FbleTcTag][tag] The tag of the tc.
 *  @arg[FbleLoc][loc] The source loc of the tc. Borrowed.
 *
 *  @returns[FbleTc*] The newly allocated tc.
 *
 *  @sideeffects
 *   Allocates a new tc that should be freed using FbleFreeTc when no longer
 *   needed.
 */
FbleTc* FbleNewTcRaw(size_t size, FbleTcTag tag, FbleLoc loc);

/**
 * @func[FbleCopyTc] Makes a reference counted copy of the given tc.
 *  @arg[FbleTc*][tc] The tc to copy.
 *
 *  @returns[FbleTc*]
 *   The copy of the tc.
 *
 *  @sideeffects
 *   The user should arrange for FbleFreeTc to be called on this tc when it is
 *   no longer needed.
 */
FbleTc* FbleCopyTc(FbleTc* tc);

/**
 * @func[FbleFreeTc] Frees resources associated with an FbleTc.
 *  @arg[FbleTc*][tc] The tc to free. May be NULL.
 *
 *  @sideeffects
 *   Frees all resources associated with the given tc.
 */
void FbleFreeTc(FbleTc* tc);

#endif // FBLE_INTERNAL_TC_H_
