// fble.h --
//   This header file describes the externally visible interface to the
//   fble library.

#ifndef FBLE_H_
#define FBLE_H_

#include <assert.h>       // for assert
#include <stdbool.h>      // for bool
#include <stdlib.h>       // for NULL
#include <sys/types.h>    // for size_t

// FbleArena --
//   A handle used for allocating and freeing memory.
typedef struct FbleArena FbleArena;

// FbleArenaAlloc --
//   Allocate size bytes of memory.
//
// Inputs:
//   arena - The arena to allocate memory from.
//   size - The number of bytes to allocate.
//   msg - A message used to identify the allocation for debugging purposes.
//
// Result:
//   A pointer to a newly allocated size bytes of memory in the arena.
//
// Side effects:
//   Allocates size bytes from the arena.
void* FbleArenaAlloc(FbleArena* arena, size_t size, const char* msg);

// FbleAlloc --
//   A type safer way of allocating objects from an arena.
//
// Inputs:
//   arena - The arena to use for allocation.
//   T - The type of object to allocate.
//
// Results:
//   A pointer to a newly allocated object of the given type.
//
// Side effects:
//   Uses the arena to allocation the object.
#define FbleAllocLine(x) #x
#define FbleAllocMsg(file, line) file ":" FbleAllocLine(line)
#define FbleAlloc(arena, T) ((T*) FbleArenaAlloc(arena, sizeof(T), FbleAllocMsg(__FILE__, __LINE__)))

// FbleFree --
//   Free a memory allocation.
//
// Inputs:
//   arena - The arena to free the memory from.
//   ptr - The allocation to free, which must have been returned by a call
//         to alloc on this arena.
//
// Result:
//   None.
//
// Side effects:
//   Frees memory associated with the ptr from the arena.
//   After this call, any accesses to the freed memory result in undefined
//   behavior.
//   Behavior is undefined if ptr was not previously returned by a call to
//   alloc on the given arena.
void FbleFree(FbleArena* arena, void* ptr);

// FbleNewArena --
//   Create a new arena descended from the given arena.
//
// Inputs:
//   parent - the parent arena for the new arena. May be null.
//
// Result:
//   The newly allocated arena.
//
// Side effects:
//   Allocates a new arena which must be freed using FbleDeleteArena.
FbleArena* FbleNewArena(FbleArena* parent);

// FbleDeleteArena --
//   Delete an arena created with FbleNewArena.
//
// Inputs:
//   arena - the arena to delete.
//
// Result:
//   None.
//
// Side effects:
//   Frees memory associated with the arena, including the arena itself, all
//   outstanding allocations made by the arena, and all descendant arenas and
//   their allocations.
void FbleDeleteArena(FbleArena* arena);

// FbleAssertEmptyArena
//   Check that there are no outstanding allocations in the given arena. Used
//   to aid in testing and debugging memory leaks.
//
// Inputs:
//   arena - A pointer to the arena to check.
//
// Results:
//   none.
//
// Side effects:
//   Prints debug message to stderr and aborts the process if there are any
//   outstanding allocations in the arena.
void FbleAssertEmptyArena(FbleArena* arena);

// FbleVector --
//   A common data structure in fble is an array of elements with a size. By
//   convention, fble uses the same data structure layout and naming for all
//   of these vector data structures. The type of a vector of elements T is:
//   struct {
//     size_t size;
//     T* xs;
//   };
//
//   Often these vectors are constructed without knowing the size ahead of
//   time. The following macros are used to help construct these vectors,
//   regardless of the element type.

// FbleVectorInit --
//   Initialize a vector for construction.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to an uninitialized vector.
//
// Results:
//   None.
//
// Side effects:
//   The vector is initialized to an array containing 0 elements.
//
// Implementation notes:
//   The array initially has size 0 and capacity 1.
#define FbleVectorInit(arena, vector) \
  (vector).size = 0; \
  (vector).xs = FbleArenaAlloc(arena, sizeof(*((vector).xs)), FbleAllocMsg(__FILE__, __LINE__))

// FbleVectorExtend --
//   Append an uninitialized element to the vector.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to a vector that was initialized using FbleVectorInit.
//
// Results:
//   A pointer to the newly appended uninitialized element.
//
// Side effects:
//   A new uninitialized element is appended to the array and the size is
//   incremented. If necessary, the array is re-allocated to make space for
//   the new element.
#define FbleVectorExtend(arena, vector) \
  (FbleVectorIncrSize(arena, sizeof(*((vector).xs)), &(vector).size, (void**)&(vector).xs), (vector).xs + (vector).size - 1)

// FbleVectorAppend --
//   Append an element to a vector.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to a vector that was initialized using FbleVectorInit.
//   elem - An element of type T to append to the array.
//
// Results:
//   None.
//
// Side effects:
//   The given element is appended to the array and the size is incremented.
//   If necessary, the array is re-allocated to make space for the new
//   element.
#define FbleVectorAppend(arena, vector, elem) \
  (*FbleVectorExtend(arena, vector) = elem)

// FbleVectorIncrSize --
//   Increase the size of an fble vector by a single element.
//
//   This is an internal function used for implementing the fble vector macros.
//   This function should not be called directly because because it does not
//   provide the same level of type safety the macros provide.
//
// Inputs:
//   arena - The arena used for allocations.
//   elem_size - The sizeof the element type in bytes.
//   size - A pointer to the size field of the vector.
//   xs - A pointer to the xs field of the vector.
//
// Results:
//   None.
//
// Side effects:
//   A new uninitialized element is appended to the vector and the size is
//   incremented. If necessary, the array is re-allocated to make space for
//   the new element.
void FbleVectorIncrSize(FbleArena* arena, size_t elem_size, size_t* size, void** xs);

// FbleLoc --
//   Represents a location in a source file.
//
// Fields:
//   source - The name of the source file or other description of the source
//            of the program text.
//   line - The line within the file for the location.
//   col - The column within the line for the location.
typedef struct {
  const char* source;
  int line;
  int col;
} FbleLoc;

// FbleReportError --
//   Report an error message associated with a location in a source file.
//
// Inputs:
//   format - A printf format string for the error message.
//   loc - The location of the error message to report.
//   ... - printf arguments as specified by the format string.
//
// Results:
//   None.
//
// Side effects:
//   Prints an error message to stderr with error location.
void FbleReportError(const char* format, FbleLoc* loc, ...);

// FbleName -- 
//   A name along with its associated location in a source file. The location
//   is typically used for error reporting purposes.
typedef struct {
  const char* name;
  FbleLoc loc;
} FbleName;

// FbleNamesEqual --
//   Test whether two names are equal.
//
// Inputs:
//   a - The first name.
//   b - The second name.
//
// Results:
//   true if the first name equals the second, false otherwise.
//
// Side effects:
//   None.
bool FbleNamesEqual(const char* a, const char* b);

// FbleKindTag --
//   A tag used to distinguish between the two kinds of kinds.
typedef enum {
  FBLE_BASIC_KIND,
  FBLE_POLY_KIND
} FbleKindTag;

// FbleKind --
//   A tagged union of kind types. All kinds have the same initial
//   layout as FbleKind. The tag can be used to determine what kind of
//   kind this is to get access to additional fields of the kind
//   by first casting to that specific type of kind.
typedef struct {
  FbleKindTag tag;
  FbleLoc loc;
} FbleKind;

// FbleKindV --
//   A vector of FbleKind.
typedef struct {
  size_t size;
  FbleKind** xs;
} FbleKindV;

// FbleBasicKind --
//   FBLE_BASIC_KIND
typedef struct {
  FbleKind _base;
} FbleBasicKind;

// FblePolyKind --
//   FBLE_POLY_KIND (args :: [Kind]) (return :: Kind)
typedef struct {
  FbleKind _base;
  FbleKindV args;
  FbleKind* rkind;
} FblePolyKind;

// FbleTypeField --
//   A pair of (Kind, Name) used to describe poly arguments.
typedef struct {
  FbleKind* kind;
  FbleName name;
} FbleTypeField;

// FbleTypeFieldV --
//   A vector of FbleTypeField.
typedef struct {
  size_t size;
  FbleTypeField* xs;
} FbleTypeFieldV;

// FbleTypeTag --
//   A tag used to dinstinguish among different kinds of types.
typedef enum {
  FBLE_STRUCT_TYPE,
  FBLE_UNION_TYPE,
  FBLE_FUNC_TYPE,
  FBLE_PROC_TYPE,
  FBLE_INPUT_TYPE,
  FBLE_OUTPUT_TYPE,
  FBLE_VAR_TYPE,
  FBLE_LET_TYPE,
  FBLE_POLY_TYPE,
  FBLE_POLY_APPLY_TYPE
} FbleTypeTag;

// FbleType --
//   A tagged union of type types. All types have the same initial
//   layout as FbleType. The tag can be used to determine what kind of
//   type this is to get access to additional fields of the type
//   by first casting to that specific type of type.
typedef struct {
  FbleTypeTag tag;
  FbleLoc loc;
} FbleType;

// FbleTypeV --
//   A vector of FbleType.
typedef struct {
  size_t size;
  FbleType** xs;
} FbleTypeV;

// FbleField --
//   A pair of (Type, Name) used to describe type and function arguments.
typedef struct {
  FbleType* type;
  FbleName name;
} FbleField;

// FbleFieldV --
//   A vector of FbleField.
typedef struct {
  size_t size;
  FbleField* xs;
} FbleFieldV;

// FbleStructType --
//   FBLE_STRUCT_TYPE (fields :: [(Type, Name)])
typedef struct {
  FbleType _base;
  FbleFieldV fields;
} FbleStructType;

// FbleUnionType --
//   FBLE_UNION_TYPE (fields :: [(Type, Name)])
typedef struct {
  FbleType _base;
  FbleFieldV fields;
} FbleUnionType;

// FbleFuncType --
//   FBLE_FUNC_TYPE (args :: [(Type, Name)]) (return :: Type)
typedef struct {
  FbleType _base;
  FbleFieldV args;
  FbleType* rtype;
} FbleFuncType;

// FbleProcType --
//   FBLE_PROC_TYPE (return :: Type)
typedef struct {
  FbleType _base;
  FbleType* rtype;
} FbleProcType;

// FbleInputType --
//   FBLE_INPUT_TYPE (type :: Type)
typedef struct {
  FbleType _base;
  FbleType* type;
} FbleInputType;

// FbleOutputType --
//   FBLE_OUTPUT_TYPE (type :: Type)
typedef struct {
  FbleType _base;
  FbleType* type;
} FbleOutputType;

// FbleVarType --
//   FBLE_VAR_TYPE (name :: Name)
typedef struct {
  FbleType _base;
  FbleName var;
} FbleVarType;

// FbleTypeBinding --
//   A triple of (Kind, Name, Type) used in type let types and expressions.
typedef struct {
  FbleKind* kind;
  FbleName name;
  FbleType* type;
} FbleTypeBinding;

// FbleTypeBindingV --
//   A vector of FbleTypeBinding
typedef struct {
  size_t size;
  FbleTypeBinding* xs;
} FbleTypeBindingV;

// FbleLetType --
//   FBLE_LET_TYPE (bindings :: [(Kind, Name, Type)]) (body :: Type)
typedef struct {
  FbleType _base;
  FbleTypeBindingV bindings;
  FbleType* body;
} FbleLetType;

// FblePolyType --
//   FBLE_POLY_TYPE (args :: [(Kind, Name)]) (body :: Type)
typedef struct {
  FbleType _base;
  FbleTypeFieldV args;
  FbleType* body;
} FblePolyType;

// FblePolyApplyType --
//   FBLE_POLY_APPLY_TYPE (poly :: Type) (args :: [Type])
typedef struct {
  FbleType _base;
  FbleType* poly;
  FbleTypeV args;
} FblePolyApplyType;

// FbleExprTag --
//   A tag used to distinguish among different kinds of expressions.
typedef enum {
  FBLE_STRUCT_VALUE_EXPR,
  FBLE_UNION_VALUE_EXPR,
  FBLE_ACCESS_EXPR,       // Used for STRUCT_ACCESS, UNION_ACCESS
  FBLE_COND_EXPR,
  FBLE_FUNC_VALUE_EXPR,
  FBLE_APPLY_EXPR,        // Used for APPLY, GET, PUT
  FBLE_EVAL_EXPR,
  FBLE_LINK_EXPR,
  FBLE_EXEC_EXPR,
  FBLE_VAR_EXPR,
  FBLE_LET_EXPR,
  FBLE_TYPE_LET_EXPR,
  FBLE_POLY_EXPR,
  FBLE_POLY_APPLY_EXPR
} FbleExprTag;

// FbleExpr --
//   A tagged union of expression types. All expressions have the same initial
//   layout as FbleExpr. The tag can be used to determine what kind of
//   expression this is to get access to additional fields of the expression
//   by first casting to that specific type of expression.
typedef struct {
  FbleExprTag tag;
  FbleLoc loc;
} FbleExpr;

// FbleExprV --
//   A vector of FbleExpr.
typedef struct {
  size_t size;
  FbleExpr** xs;
} FbleExprV;

// FbleStructValueExpr --
//   FBLE_STRUCT_VALUE_EXPR (type :: Type) (args :: [Expr])
typedef struct {
  FbleExpr _base;
  FbleType* type;
  FbleExprV args;
} FbleStructValueExpr;

// FbleAccessExpr --
//   FBLE_ACCESS_EXPR (object :: Expr) (field :: Name)
// Common form used for both struct and union access.
typedef struct {
  FbleExpr _base;
  FbleExpr* object;
  FbleName field;
} FbleAccessExpr;

// FbleUnionValueExpr --
//   FBLE_UNION_VALUE_EXPR (type :: Type) (field :: Name) (arg :: Expr)
typedef struct {
  FbleExpr _base;
  FbleType* type;
  FbleName field;
  FbleExpr* arg;
} FbleUnionValueExpr;

// FbleChoice --
//   A pair of (Name, Expr) used in conditional expressions.
typedef struct {
  FbleName name;
  FbleExpr* expr;
} FbleChoice;

// FbleChoiceV --
//   A vector of FbleChoice.
typedef struct {
  size_t size;
  FbleChoice* xs;
} FbleChoiceV;

// FbleCondExpr --
//   FBLE_COND_EXPR (condition :: Expr) (choices :: [(Name, Expr)])
typedef struct {
  FbleExpr _base;
  FbleExpr* condition;
  FbleChoiceV choices;
} FbleCondExpr;

// FbleFuncValueExpr --
//   FBLE_FUNC_VALUE_EXPR (args :: [(Type, Name)]) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleFieldV args;
  FbleExpr* body;
} FbleFuncValueExpr;

// FbleApplyExpr --
//   FBLE_APPLY_EXPR (func :: Expr) (args :: [Expr])
// Common form used for apply, get, and put expressions.
typedef struct {
  FbleExpr _base;
  FbleExpr* func;
  FbleExprV args;
} FbleApplyExpr;

// FbleEvalExpr --
//   FBLE_EVAL_EXPR (expr :: Expr)
typedef struct {
  FbleExpr _base;
  FbleExpr* expr;
} FbleEvalExpr;

// FbleLinkExpr --
//   FBLE_LINK_EXPR (type :: Type) (get :: Name) (put :: Name) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleType* type;
  FbleName get;
  FbleName put;
  FbleExpr* body;
} FbleLinkExpr;

// FbleBinding --
//   A triple of (Type, Name, Expr) used in let and exec expressions.
typedef struct {
  FbleType* type;
  FbleName name;
  FbleExpr* expr;
} FbleBinding;

// FbleBindingV --
//   A vector of FbleBinding
typedef struct {
  size_t size;
  FbleBinding* xs;
} FbleBindingV;

// FbleExecExpr --
//   FBLE_EXEC_EXPR (bindings :: [(Type, Name, Expr)]) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleBindingV bindings;
  FbleExpr* body;
} FbleExecExpr;

// FbleVarExpr --
//   FBLE_VAR_EXPR (name :: Name)
typedef struct {
  FbleExpr _base;
  FbleName var;
} FbleVarExpr;

// FbleLetExpr --
//   FBLE_LET_EXPR (bindings :: [(Type, Name, Expr)]) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleBindingV bindings;
  FbleExpr* body;
} FbleLetExpr;

// FbleTypeLetExpr --
//   FBLE_TYPE_LET_EXPR (bindings :: [(Kind, Name, Type)]) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleTypeBindingV bindings;
  FbleExpr* body;
} FbleTypeLetExpr;

// FblePolyExpr --
//   FBLE_POLY_EXPR (args :: [(Kind, Name)]) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleTypeFieldV args;
  FbleExpr* body;
} FblePolyExpr;

// FblePolyApplyExpr --
//   FBLE_POLY_APPLY_EXPR (poly :: Expr) (args :: [Type])
typedef struct {
  FbleExpr _base;
  FbleExpr* poly;
  FbleTypeV args;
} FblePolyApplyExpr;

// FbleParse --
//   Parse an expression from a file.
//
// Inputs:
//   arena - The arena to use for allocating the parsed program.
//   filename - The name of the file to parse the program from.
//
// Results:
//   The parsed program, or NULL in case of error.
//
// Side effects:
//   Prints an error message to stderr if the program cannot be parsed.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the returned program if there is no error.
//
// Note:
//   A copy of the filename will be made for use in locations. The user need
//   not ensure that filename remains valid for the duration of the lifetime
//   of the program.
FbleExpr* FbleParse(FbleArena* arena, const char* filename);

// FbleValueTag --
//   A tag used to distinguish among different kinds of values.
typedef enum {
  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_FUNC_VALUE,
  FBLE_PROC_VALUE,
  FBLE_INPUT_VALUE,
  FBLE_OUTPUT_VALUE,
  FBLE_REF_VALUE,
} FbleValueTag;

// FbleValue --
//   A tagged union of value types. All values have the same initial
//   layout as FbleValue. The tag can be used to determine what kind of
//   value this is to get access to additional fields of the value
//   by first casting to that specific type of value.
typedef struct FbleValue {
  FbleValueTag tag;
  size_t strong_ref_count;
  size_t break_cycle_ref_count;
} FbleValue;

// FbleValueV --
//   A vector of FbleValue*
typedef struct {
  size_t size;
  FbleValue** xs;
} FbleValueV;

// FbleStructValue --
//   FBLE_STRUCT_VALUE
typedef struct {
  FbleValue _base;
  FbleValueV fields;
} FbleStructValue;

// FbleUnionValue --
//   FBLE_UNION_VALUE
typedef struct {
  FbleValue _base;
  size_t tag;
  FbleValue* arg;
} FbleUnionValue;

// FbleFuncValue --
//   FBLE_FUNC_VALUE
//
// Defined internally, because representing the body of the function depends
// on the internals of the evaluator.
typedef struct FbleFuncValue FbleFuncValue;

// FbleProcValue --
//   FBLE_PROC_VALUE
//
// Defined internally, because representing a process depends on the internals
// of the evaluator.
typedef struct FbleProcValue FbleProcValue;

// FbleInputValue --
//   FBLE_INPUT_VALUE
//
// Defined internally.
typedef struct FbleInputValue FbleInputValue;

// FbleOutputValue --
//   FBLE_OUTPUT_VALUE
//
// Defined internally.
typedef struct FbleOutputValue FbleOutputValue;

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
//   broke_cycle - true if the FbleBreakCycleRef count has been called on the
//                 value for this reference, false otherwise.
//   siblings - A circular singly linked list of the ref values for the other
//              variables defined in the same let binding as this ref value.
//              Used for breaking reference cycles.
//              The siblings list does not count as a reference to the ref
//              values. RefValues are responsible for removing themselves from
//              the list when they are freed.
typedef struct FbleRefValue {
  FbleValue _base;
  FbleValue* value;
  bool broke_cycle;
  struct FbleRefValue* siblings;
} FbleRefValue;

// FbleTakeStrongRef --
//
//   Increment the strong reference count of a value.
//
// Inputs:
//   value - The value to increment the strong reference count of. The value
//           may be NULL, in which case nothing is done.
//
// Results:
//   The given value, for the caller's convenience when incrementing the
//   reference count and assigning it at the same time.
//
// Side effects:
//   Increments the strong reference count on the value. The reference count
//   must be decrement using FbleDropStrongRef when the value is no longer
//   needed.
FbleValue* FbleTakeStrongRef(FbleValue* src);

// FbleBreakCycleRef --
//   Increment the break cycle reference count of a value.
//
// Inputs:
//   arena - Arena to use for deallocation.
//   value - The value to increment the break cycle reference count of. The
//           value may be NULL, in which case nothing is done.
//
// Results:
//   The given value, for the caller's convenience when incrementing the
//   reference count and assigning it at the same time.
//
// Side effects:
//   Increments the break cycle reference count on the value. Breaks the cycle
//   if necessary, which could cause values to be freed.
FbleValue* FbleBreakCycleRef(FbleArena* arena, FbleValue* src);

// FbleDropStrongRef --
//
//   Decrement the strong reference count of a value and free the resources
//   associated with that value if it has no more references.
//
// Inputs:
//   arena - The arena the value was allocated with.
//   value - The value to decrement the strong reference count of. The value
//           may be NULL, in which case no action is performed.
//
// Results:
//   None.
//
// Side effect:
//   Decrements the strong reference count of the value and frees resources
//   associated with the value if there are no more references to it.
void FbleDropStrongRef(FbleArena* arena, FbleValue* value);

// FbleEval --
//   Type check and evaluate an expression.
//
// Inputs:
//   arena - The arena to use for allocating values.
//   expr - The expression to evaluate.
//
// Results:
//   The value of the evaluated expression, or NULL in case of error. The
//   error could be a type error or an undefined union field access.
//
// Side effects:
//   Prints an error message to stderr in case of error.
FbleValue* FbleEval(FbleArena* arena, FbleExpr* expr);

// FbleExec --
//   Execute a process.
//
// Inputs:
//   arena - The arena to use for allocating values.
//   proc - The process to execute.
//
// Results:
//   The result of executing the process, or NULL in case of error. The
//   error could be an undefined union field access.
//
// Side effects:
//   Prints an error message to stderr in case of error.
FbleValue* FbleExec(FbleArena* arena, FbleProcValue* proc);

#endif // FBLE_H_
