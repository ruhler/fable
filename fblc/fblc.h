// fblc.h --
//   This header file describes the externally visible interface to the
//   machine-level fblc facilities.

#ifndef FBLC_H_
#define FBLC_H_

#include <stdbool.h>    // for bool
#include <sys/types.h>  // for size_t

// FblcArena --
//   An interface for allocating and freeing memory.
typedef struct FblcArena {
  // alloc --
  //   Allocate size bytes of memory.
  //
  // Inputs:
  //   this - The arena to allocate memory from.
  //   size - The number of bytes to allocate.
  //
  // Result:
  //   A pointer to a newly allocated size bytes of memory in the arena.
  //
  // Side effects:
  //   Allocates size bytes from the arena.
  //   The behavior is undefined if the 'this' argument is not the same from
  //   which the alloc function is invoked.
  void* (*alloc)(struct FblcArena* this, size_t size);

  // free --
  //   Free a memory allocation.
  //
  // Inputs:
  //   this - The arena to free the memory from.
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
  //   Behavior is undefined if the 'this' argument is not the same from
  //   which the alloc function is invoked, or if ptr was not previously
  //   returned by a call to alloc on this arena.
  void (*free)(struct FblcArena* this, void* ptr);
} FblcArena;

// FBLC_ALLOC --
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
#define FBLC_ALLOC(arena, T) ((T*) (arena)->alloc((arena), sizeof(T)))

// FblcMallocArena --
//   A stateless FblcArena that uses malloc and free to implement its alloc
//   and free functions respectively.
//
//   Note that the FblcMallocArena does not keep track of current allocations,
//   which means it is not suited for use with functions that rely on the
//   allocator to track allocations, unless it is okay to leak the memory.
extern FblcArena FblcMallocArena;

// FblcVector --
//   A common data structure in fblc is an array of elements with a size. By
//   convention, fblc uses the same data structure layout and naming for all
//   of these vector data structures. The type of a vector of elements T is:
//   struct {
//     size_t size;
//     T* xs;
//   };
//
//   Often these vectors are constructed without knowing the size ahead of
//   time. The following macros are used to help construct these vectors,
//   regardless of the element type.

// FblcVectorInit --
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
#define FblcVectorInit(arena, vector) \
  (vector).size = 0; \
  (vector).xs = arena->alloc(arena, sizeof(*((vector).xs)))

// FblcVectorExtend --
//   Append an uninitialized element to the vector.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to a vector that was initialized using FblcVectorInit.
//
// Results:
//   A pointer to the newly appended uninitialized element.
//
// Side effects:
//   A new uninitialized element is appended to the array and the size is
//   incremented. If necessary, the array is re-allocated to make space for
//   the new element.
#define FblcVectorExtend(arena, vector) \
  (FblcVectorIncrSize(arena, sizeof(*((vector).xs)), &(vector).size, (void**)&(vector).xs), (vector).xs + (vector).size - 1)

// FblcVectorAppend --
//   Append an element to a vector.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to a vector that was initialized using FblcVectorInit.
//   elem - An element of type T to append to the array.
//
// Results:
//   None.
//
// Side effects:
//   The given element is appended to the array and the size is incremented.
//   If necessary, the array is re-allocated to make space for the new
//   element.
#define FblcVectorAppend(arena, vector, elem) \
  (*FblcVectorExtend(arena, vector) = elem)

// FblcVectorIncrSize --
//   Increase the size of an fblc vector by a single element.
//
//   This is an internal function used for implementing the fblc vector macros.
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
void FblcVectorIncrSize(FblcArena* arena, size_t elem_size, size_t* size, void** xs);

// FblcFieldId --
//   Fields are identified using the order in which they are defined in their
//   type declaration. The first field has id 0, the second field has id 1,
//   and so on.
typedef size_t FblcFieldId;

// FblcVarId --
//   Variables are identified using De Bruijn indices. The inner most bound
//   variable has id 0, the next inner most bound variable has id 1, and so
//   on. For variables introduced as function arguments or exec actions, the
//   right-most argument is considered inner most.
typedef size_t FblcVarId;

// FblcPortId --
//   Ports are identified using De Bruijn indices. The inner most bound port
//   has id 0, the next inner most bound variable has id 1, and so on. For
//   ports introduced as process arguments, the right-most port is considered
//   inner most. For ports declared as part of link actions, the put port is
//   considered inner most with respect to the get port. Both put and get
//   ports belong to the same namespace of indices.
typedef size_t FblcPortId;

// FblcExprId --
//   Each expression within a declaration is assigned a unique id starting
//   from 0, followed by 1, and so on. These ids can be used as indices into
//   maps of other data associated with expressions, such as symbol
//   information.
typedef size_t FblcExprId;

// FblcActnId --
//   Each action within a declaration is assigned a unique id starting
//   from 0, followed by 1, and so on. These ids can be used as indices into
//   maps of other data associated with actions, such as symbol
//   information.
typedef size_t FblcActnId;

// FBLC_NULL_ID -- 
//   A sentinel value that can be used for ids to indicate an otherwise
//   invalid id. Note that FBLC_NULL_ID does not have the same value as NULL.
#define FBLC_NULL_ID (-1)

// Forward declarations. See below for descriptions of these types.
typedef struct FblcType FblcType;
typedef struct FblcFunc FblcFunc;
typedef struct FblcProc FblcProc;

// FblcExprTag --
//   A tag used to distinguish among different kinds of expressions.
typedef enum {
  FBLC_VAR_EXPR,
  FBLC_APP_EXPR,
  FBLC_STRUCT_EXPR,
  FBLC_UNION_EXPR,
  FBLC_ACCESS_EXPR,
  FBLC_COND_EXPR,
  FBLC_LET_EXPR
} FblcExprTag;

// FblcExpr --
//   A tagged union of expression types. All expressions have the same initial
//   layout as FblcExpr. The tag can be used to determine what kind of
//   expression this is to get access to additional fields of the expression
//   by first casting to that specific type of expression.
typedef struct {
  FblcExprTag tag;
} FblcExpr;

// FblcExprV --
//   A vector of fblc expressions.
typedef struct {
  size_t size;
  FblcExpr** xs;
} FblcExprV;

// FblcVarExpr --
//   A variable expression of the form 'var' whose value is the value of the
//   corresponding variable in scope.
typedef struct {
  FblcExpr _base;
  FblcVarId var;
} FblcVarExpr;

// FblcAppExpr --
//   An application expression of the form 'func(arg0, arg1, ...)'.
typedef struct {
  FblcExpr _base;
  FblcFunc* func;
  FblcExprV argv;
} FblcAppExpr;

// FblcStructExpr --
//   A struct expression of the form 'type(arg0, arg1, ...)'.
typedef struct {
  FblcExpr _base;
  FblcType* type;
  FblcExprV argv;
} FblcStructExpr;

// FblcUnionExpr --
//   A union expression of the form 'type:field(arg)', used to construct a
//   union value.
typedef struct {
  FblcExpr _base;
  FblcType* type;
  FblcFieldId field;
  FblcExpr* arg;
} FblcUnionExpr;

// FblcAccessExpr --
//   An access expression of the form 'obj.field' used to access a field of
//   a struct or union value.
typedef struct {
  FblcExpr _base;
  FblcExpr* obj;
  FblcFieldId field;
} FblcAccessExpr;

// FblcCondExpr --
//   A conditional expression of the form '?(select; arg0, arg1, ...)', which
//   conditionally selects an argument based on the tag of the select value.
typedef struct {
  FblcExpr _base;
  FblcExpr* select;
  FblcExprV argv;
} FblcCondExpr;

// FblcLetExpr --
//   A let expression of the form '{ type var = def; body }', where the name
//   of the variable is a De Bruijn index based on the context where the
//   variable is accessed.
typedef struct {
  FblcExpr _base;
  FblcType* type;
  FblcExpr* def;
  FblcExpr* body;
} FblcLetExpr;

// FblcActnTag --
//   A tag used to distinguish among different kinds of actions.
typedef enum {
  FBLC_EVAL_ACTN,
  FBLC_GET_ACTN,
  FBLC_PUT_ACTN,
  FBLC_COND_ACTN,
  FBLC_CALL_ACTN,
  FBLC_LINK_ACTN,
  FBLC_EXEC_ACTN
} FblcActnTag;

// FblcActn --
//   A tagged union of action types. All actions have the same initial
//   layout as FblcExpr. The tag can be used to determine what kind of
//   action this is to get access to additional fields of the action
//   by first casting to that specific type of action.
typedef struct {
  FblcActnTag tag;
} FblcActn;

// FblcActnV --
//   A vector of fblc actions.
typedef struct {
  size_t size;
  FblcActn** xs;
} FblcActnV;


// FblcEvalActn --
//   An evaluation action of the form '$(arg)' which evaluates the given
//   expression without side effects.
typedef struct {
  FblcActn _base;
  FblcExpr* arg;
} FblcEvalActn;

// FblcGetActn --
//   A get action of the form '~port()' used to get a value from a port.
typedef struct {
  FblcActn _base;
  FblcPortId port;
} FblcGetActn;

// FblcPutActn --
//   A put action of the form '~port(arg)' used to put a value onto a port.
typedef struct {
  FblcActn _base;
  FblcPortId port;
  FblcExpr* arg;
} FblcPutActn;

// FblcCondActn --
//   A conditional action of the form '?(select; arg0, arg1, ...)', which
//   conditionally selects an argument based on the tag of the select value.
typedef struct {
  FblcActn _base;
  FblcExpr* select;
  FblcActnV argv;
} FblcCondActn;

// FblcPortIdV --
//   A vector of fblc port ids.
typedef struct {
  size_t size;
  FblcPortId* xs;
} FblcPortIdV;

// FblcCallActn --
//   A call action of the form 'proc(port0, port1, ... ; arg0, arg1, ...)',
//   which calls a process with the given port and value arguments.
typedef struct {
  FblcActn _base;
  FblcProc* proc;
  FblcPortIdV portv;
  FblcExprV argv;
} FblcCallActn;

// FblcLinkActn --
//   A link action of the form 'type <~> get, put; body'. The names of the get
//   and put ports are De Bruijn indices based on the context where the ports
//   are accessed.
typedef struct {
  FblcActn _base;
  FblcType* type;
  FblcActn* body;
} FblcLinkActn;

// FblcExec --
//   Pair of type and action used in the FblcExecActn.
typedef struct {
  FblcType* type;
  FblcActn* actn;
} FblcExec;

// FblcExecV --
//   A vector of fblc execs.
typedef struct {
  size_t size;
  FblcExec* xs;
} FblcExecV;

// FblcExecActn --
//   An exec action of the form 'type0 var0 = exec0, type1 var1 = exec1, ...; body',
//   which executes processes in parallel. The names of the variables are De
//   Bruijn indices based ont he context where they are accessed.
typedef struct {
  FblcActn _base;
  FblcExecV execv;
  FblcActn* body;
} FblcExecActn;

// FblcKind --
//   An enum used to distinguish between struct and union types or values.
typedef enum {
  FBLC_STRUCT_KIND,
  FBLC_UNION_KIND
} FblcKind;

// FblcTypeV -- 
//   A vector of FblcTypes
typedef struct {
  size_t size;
  FblcType** xs;
} FblcTypeV;

// FblcType --
//   A type declaration of the form 'name(field0 name0, field1 name1, ...)'.
//   This is a common structure used for both struct and union declarations.
struct FblcType {
  FblcKind kind;
  FblcTypeV fieldv;
};

// FblcFunc --
//   Declaration of a function of the form:
//     'name(arg0 name0, arg1 name1, ...; return_type) body'
struct FblcFunc {
  FblcTypeV argv;
  FblcType* return_type;
  FblcExpr* body;
};

// FblcFuncV --
//   A vector of fblc functions.
typedef struct {
  size_t size;
  FblcFunc** xs;
} FblcFuncV;

// FblcPolarity --
//   The polarity of a port.
typedef enum {
  FBLC_GET_POLARITY,
  FBLC_PUT_POLARITY
} FblcPolarity;

// FblcPort --
//   The type and polarity of a port.
typedef struct {
  FblcType* type;
  FblcPolarity polarity;
} FblcPort;

// FblcPortV --
//   A vector of fblc ports.
typedef struct {
  size_t size;
  FblcPort* xs;
} FblcPortV;

// FblcProc --
//   Declaration of a process of the form:
//     'name(p0type p0polarity p0name, p1type p1polarity p1name, ... ;
//           arg0 name0, arg1, name1, ... ; return_type) body'
struct FblcProc {
  FblcPortV portv;
  FblcTypeV argv;
  FblcType* return_type;
  FblcActn* body;
};

// FblcProcV --
//   A vector of fblc procs.
typedef struct {
  size_t size;
  FblcProc** xs;
} FblcProcV;

// FblcProgram --
//   A collection of declarations that make up a program.
typedef struct {
  FblcTypeV typev;
  FblcFuncV funcv;
  FblcProcV procv;
} FblcProgram;

// FblcValue --
//   An fblc struct or union value.
//
// Fields:
//   refcount - The number of references to this value.
//   kind - Whether this is a struct or union value.
//   fieldc - The number of fields in the value's corresponding struct or
//            union type.
//   tag - The tag of a union value. The tag field is unused for struct
//         values.
//   fields - The fields of this value. For struct values, there will be
//            fieldc fields corresponding to the fields in the struct
//            declarations. For union values, there will be a single field
//            containing the value associated with the union tag.
typedef struct FblcValue {
  size_t refcount;
  FblcKind kind;
  size_t fieldc;
  FblcFieldId tag;
  struct FblcValue* fields[];
} FblcValue;

// FblcNewStruct --
//   Construct a new struct value for a struct type with the given number of
//   fields.
//
// Inputs:
//   arena - The arena to use for allocations.
//   fieldc - The number of fields in the struct type.
//
// Results:
//   A newly constructed struct value. The refcount, kind, and fieldc of the
//   returned value are initialized. The fields are uninitialized.
//
// Side effects:
//   Performs arena allocations.
FblcValue* FblcNewStruct(FblcArena* arena, size_t fieldc);

// FblcNewUnion --
//   Construct a new union value.
//
// Inputs:
//   arena - The arena to use for allocations.
//   fieldc - The number of fields in the union type.
//   tag - The tag of the union value.
//   value - The value associated with the tag of the union value.
//
// Results:
//   A newly constructed, fully initialized union value.
//
// Side effects:
//   Performs arena allocations.
FblcValue* FblcNewUnion(FblcArena* arena, size_t fieldc, FblcFieldId tag, FblcValue* value);

// FblcCopy --
//
//   Make a (likely shared) copy of the given value.
//
// Inputs:
//   arena - The arena to use for allocations.
//   src - The value to make a copy of.
//
// Results:
//   A copy of the value, which may be the same as the original value.
//
// Side effects:
//   May perform arena allocations.
FblcValue* FblcCopy(FblcArena* arena, FblcValue* src);

// FblcRelease --
//
//   Free the resources associated with a value.
//
// Inputs:
//   arena - The arena the value was allocated with.
//   value - The value to free the resources of.
//
// Results:
//   None.
//
// Side effect:
//   The resources for the value are freed. The value may be NULL, in which
//   case no action is taken.
void FblcRelease(FblcArena* arena, FblcValue* value);

// FblcIO --
//   An interface for reading or writing values over external ports.
typedef struct FblcIO {
  // io --
  //   Read or write values over the external ports. An FblcValue* is supplied
  //   for each port argument to the main entry point, in the order they are
  //   declared. The behavior of the function depends on the port type and
  //   whether the provided FblcValue* is NULL or non-NULL as follows:
  //
  //   Get Ports:
  //     NULL: the io function may, at its option, read the next input
  //       value and replace NULL with the newly read value.
  //     non-NULL: the io function should do nothing for this port.
  //
  //   Put Ports:
  //     NULL: the io function should do nothing for this port.
  //     non-NULL: the io function may, at its option, output the value.
  //       If the io function chooses to output the value, it should
  //       FblcRelease the value and replace it with NULL. Otherwise the io
  //       function should leave the existing value as is.
  //
  //   io may blocking or non-blocking depending on the 'block' input. For
  //   non-blocking io, the io function should return immediately without
  //   blocking for inputs to be ready. For blocking io, the io function
  //   should block until there is an input available on one of the NULL get
  //   ports.
  //
  // Inputs:
  //   user - The user data associated with this io.
  //   arena - The arena to use for allocating and freeing values.
  //   block - true if io should be blocking, false if it should be
  //           non-blocking.
  //   ports - Array of values indicating which of the external ports to read
  //           to and write from. The length of the array is the number of
  //           ports to the main entry function being executed.
  //
  // Result:
  //   None.
  //
  // Side effects:
  //   Reads or writes values to external ports depending on the provided
  //   arguments and may block.
  //   
  void (*io)(void* user, FblcArena* arena, bool block, FblcValue** ports);
  void* user;
} FblcIO;

// FblcInstr --
//   An interface for instrumenting execution of fblc programs.
//   These fields may be NULL to use the default action.
typedef struct FblcInstr {
  // on_undefined_access --
  //   Report an undefined member access error.
  //
  // Inputs:
  //   this - This instrumentation object.
  //   source - The expression that led to the undefined member access.
  //
  // Result:
  //   None.
  //
  // Side effects:
  //   Implementation specific.
  void (*on_undefined_access)(struct FblcInstr* this, FblcExpr* source);
} FblcInstr;

// FblcExecute --
//   Execute a process with the given args and ports.
//
// Inputs:
//   arena - The arena to use for allocations.
//   instr - Instrumentation to use when executing the program.
//   proc - The process to execute.
//   args - Arguments to the process to execute.
//   io - Interface for getting and putting values on external ports.
//
// Returns:
//   The result of executing the given procedure with the given arguments and
//   external port io.
//
// Side effects:
//   Releases the args values.
//   Calls the corresponding io function to read and write values from
//   external ports.
//   Calls the instrumentation functions as appropriate during the course of
//   execution.
FblcValue* FblcExecute(FblcArena* arena, FblcInstr* instr, FblcProc* proc, FblcValue** args, FblcIO* io);
#endif // FBLC_H_
