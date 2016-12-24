
#ifndef FBLC_H_
#define FBLC_H_

#include <stdbool.h>    // for bool
#include <stdint.h>     // for uint32_t
#include <sys/types.h>  // for size_t and ssize_t

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

// FblcVectorInit --
//   Initialize a vector for construction. A vector is used to construct an
//   array of elements the size of which is not known ahead of time. A vector
//   consists of a reference to an array of elements and a reference to the
//   size of that array. The array of elements and size are updated and
//   resized as necessary as elements are appended to the vector.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to an uninitialized pointer T* to an array of
//            elements of type T.
//   size - A reference to a size_t to hold the size of the array.
//
// Results:
//   None.
//
// Side effects:
//   vector and size are initialized to an array containing 0 elements.
//
// Implementation notes:
//   This array initially has size 0 and capacity 1.
#define FblcVectorInit(arena, vector, size) \
  size = 0; \
  vector = arena->alloc(arena, sizeof(*vector))

// FblcVectorExtend --
//   Extend a vector's capacity if necessary to ensure it has space for more
//   than size elements.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to a pointer T* to an array of elements of type T
//            that was initialized using FblcVectorInit.
//   size - A reference to a size_t that holds the size of the array.
//
// Results:
//   None.
//
// Side effects:
//   Extends the vector's capacity if necessary to ensure it has space for
//   more than size elements. If necessary, the array is re-allocated to make
//   space for the new element.
//
// Implementation notes:
//   We assume the capacity of the array is the smallest power of 2 that holds
//   size elements. If size is equal to the capacity of the array, we double
//   the capacity of the array, which preserves the invariant after the size is
//   incremented.
#define FblcVectorExtend(arena, vector, size) \
  if ((size) > 0 && (((size) & ((size)-1)) == 0)) { \
    void* resized = arena->alloc(arena, 2 * (size) * sizeof(*vector)); \
    memcpy(resized, vector, (size) * sizeof(*vector)); \
    arena->free(arena, vector); \
    vector = resized; \
  }

// FblcVectorAppend --
//   Append an element to a vector.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to a pointer T* to an array of elements of type T
//            that was initialized using FblcVectorInit.
//   size - A reference to a size_t that holds the size of the array.
//   elem - An element of type T to append to the array.
//
// Results:
//   None.
//
// Side effects:
//   The given element is append to the array and the size is incremented. If
//   necessary, the array is re-allocated to make space for the new element.
#define FblcVectorAppend(arena, vector, size, elem) \
  FblcVectorExtend(arena, vector, size); \
  vector[(size)++] = elem

// FblcDeclId --
//   Declarations are identified using the order in which they are defined in
//   the program. The first declaration has id 0, the second declaration id 1,
//   and so on.
typedef size_t FblcDeclId;

// FblcTypeId --
//   An FblcDeclId that ought to be referring to a type declaration.
typedef FblcDeclId FblcTypeId;

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

// FblcExprTag --
//   A tag used to distinguish among different kinds of expressions.
typedef enum {
  FBLC_VAR_EXPR,
  FBLC_APP_EXPR,
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

// FblcVarExpr --
//   A variable expression of the form 'var' whose value is the value of the
//   corresponding variable in scope.
typedef struct {
  FblcExprTag tag;
  FblcVarId var;
} FblcVarExpr;

// FblcAppExpr --
//   An application expression of the form 'func(arg0, arg1, ...)'. func may
//   refer to a function or a struct type.
typedef struct {
  FblcExprTag tag;
  FblcDeclId func;
  size_t argc;
  FblcExpr** argv;
} FblcAppExpr;

// FblcUnionExpr --
//   An union expression of the form 'type:field(body)', used to construct a
//   union value.
typedef struct {
  FblcExprTag tag;
  FblcTypeId type;
  FblcFieldId field;
  FblcExpr* body;
} FblcUnionExpr;

// FblcAccessExpr --
//   An access expression of the form 'object.field' used to access a field of
//   a struct or union value.
typedef struct {
  FblcExprTag tag;
  FblcExpr* object;
  FblcFieldId field;
} FblcAccessExpr;

// FblcCondExpr --
//   A conditional expression of the form '?(select; arg0, arg1, ...)', which
//   conditionally selects an argument based on the tag of the select value.
typedef struct {
  FblcExprTag tag;
  FblcExpr* select;
  size_t argc;
  FblcExpr** argv;
} FblcCondExpr;

// FblcLetExpr --
//   A let expression of the form '{ type var = def; body }', where the type
//   of the variable can be determined as the type of the def expression and
//   the name of the variable is a De Bruijn index based on the context where
//   the variable is accessed.
typedef struct {
  FblcExprTag tag;
  FblcExpr* def;
  FblcExpr* body;
} FblcLetExpr;

// FblcDeclTag --
//   A tag used to distinguish among different kinds of declarations.
typedef enum {
  FBLC_STRUCT_DECL,
  FBLC_UNION_DECL,
  FBLC_FUNC_DECL,
  FBLC_PROC_DECL
} FblcDeclTag;

// FblcDecl --
//   A tagged union of declaration types. All declarations have the same
//   initial layout as FblcDecl. The tag can be used to determine what kind of
//   declaration this is to get access to additional fields of the declaration
//   by first casting to that specific type of declaration.
typedef struct {
  FblcDeclTag tag;
} FblcDecl;

// FblcTypeDecl --
//   Common structure used for struct and union declarations.
typedef struct {
  FblcDeclTag tag;
  size_t fieldc;
  FblcTypeId* fieldv;
} FblcTypeDecl;

// FblcStructDecl --
//   Declaration of a struct type.
typedef FblcTypeDecl FblcStructDecl;

// FblcUnionDecl --
//   Declaration of a union type.
typedef FblcTypeDecl FblcUnionDecl;

// FblcFuncDecl --
//   Declaration of a function.
typedef struct {
  FblcDeclTag tag;
  size_t argc;
  FblcTypeId* argv;
  FblcTypeId return_type; 
  FblcExpr* body;
} FblcFuncDecl;

// FblcPolarity --
//   The polarity of a port.
typedef enum {
  FBLC_GET_POLARITY,
  FBLC_PUT_POLARITY
} FblcPolarity;

// FblcPort --
//   The type and polarity of a port.
typedef struct {
  FblcTypeId type;
  FblcPolarity polarity;
} FblcPort;

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

// FblcEvalActn --
//   An evaluation action of the form '$(expr)' which evaluates the given
//   expression without side effects.
typedef struct {
  FblcActnTag tag;
  FblcExpr* expr;
} FblcEvalActn;

// FblcGetActn --
//   A get action of the form 'port~()' used to get a value from a port.
typedef struct {
  FblcActnTag tag;
  FblcPortId port;
} FblcGetActn;

// FblcPutActn --
//   A put action of the form 'port(arg)' used to put a value onto a port.
typedef struct {
  FblcActnTag tag;
  FblcPortId port;
  FblcExpr* arg;
} FblcPutActn;

// FblcCondActn --
//   A conditional action of the form '?(select; arg0, arg1, ...)', which
//   conditionally selects an argument based on the tag of the select value.
typedef struct {
  FblcActnTag tag;
  FblcExpr* select;
  size_t argc;
  FblcActn** argv;
} FblcCondActn;

// FblcCallActn --
//   A call action of the form 'proc(port0, port1, ... ; arg0, arg1, ...)',
//   which calls a process with the given port and value arguments.
typedef struct {
  FblcActnTag tag;
  FblcDeclId proc;
  size_t portc;
  FblcPortId* portv;
  size_t argc;
  FblcExpr** argv;
} FblcCallActn;

// FblcLinkActn --
//   A link action of the form 'type <~> get, put; body'. The names of the get
//   and put ports are De Bruijn indices based on the context where the ports
//   are accessed.
typedef struct {
  FblcActnTag tag;
  FblcTypeId type;
  FblcActn* body;
} FblcLinkActn;

// FblcExecActn --
//   An exec action of the form 'type0 var0 = exec0, type1 var1 = exec1, ...; body',
//   which executes processes in parallel. The types of the variables can be
//   determined as the types of their exec processes, and the name of the
//   variables are De Bruijn indices based ont he context where they are
//   accessed.
typedef struct {
  FblcActnTag tag;
  size_t execc;
  FblcActn** execv;
  FblcActn* body;
} FblcExecActn;

// FblcProcDecl --
//   Declaration of a process.
typedef struct {
  FblcDeclTag tag;
  size_t portc;
  FblcPort* portv;
  size_t argc;
  FblcTypeId* argv; 
  FblcTypeId return_type;
  FblcActn* body;
} FblcProcDecl;

// FblcProgram --
//   A collection of declarations that make up a program.
typedef struct {
  size_t declc;
  FblcDecl** declv;
} FblcProgram;

// FblcKind --
//   An enum used to distinguish between struct and union values.
typedef enum {
  FBLC_STRUCT_KIND,
  FBLC_UNION_KIND
} FblcKind;

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

// BitStream
// Bit streams are represented as sequences of ascii digits '0' and '1'.
// TODO: Support more efficient encodings of bit streams when desired.
// If the string field is non-null, digits are read from the string.
// If the fd field is non-negative, digits are read from the file with that
// descriptor.
typedef struct BitSource BitSource;
BitSource* CreateStringBitSource(FblcArena* arena, const char* string);
BitSource* CreateFdBitSource(FblcArena* arena, int fd);
uint32_t ReadBits(BitSource* source, size_t num_bits);
void SyncBitSource(BitSource* source);
void FreeBitSource(FblcArena* arena, BitSource* source);

typedef struct {
  int fd;
  bool flushed;
} OutputBitStream;

void OpenBinaryOutputBitStream(OutputBitStream* stream, int fd);
void WriteBits(OutputBitStream* stream, size_t num_bits, uint32_t bits);
void FlushWriteBits(OutputBitStream* stream);

// Encoder
FblcValue* DecodeValue(FblcArena* arena, BitSource* source, FblcProgram* prg, FblcTypeId type);
void EncodeValue(OutputBitStream* bits, FblcValue* value);
FblcProgram* DecodeProgram(FblcArena* arena, BitSource* source);

// Evaluator
FblcValue* Execute(FblcArena* arena, FblcProgram* program, FblcProcDecl* proc, FblcValue** args);

#endif // FBLC_H_

