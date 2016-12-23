
#ifndef FBLC_H_
#define FBLC_H_

#include <stdbool.h>  // for bool
#include <stdint.h>   // for uint32_t
#include <stdlib.h>   // for size_t

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
//   size elements. If size is equal to the capacity of the array coming in,
//   we double the capacity of the array, which preserves the invariant.
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

// Program
typedef size_t DeclId;
typedef size_t FieldId;
typedef size_t PortId;
typedef size_t TypeId;
typedef size_t VarId;

typedef enum {
  VAR_EXPR,
  APP_EXPR,
  UNION_EXPR,
  ACCESS_EXPR,
  COND_EXPR,
  LET_EXPR
} ExprTag;

typedef struct {
  ExprTag tag;
} Expr;

typedef struct {
  ExprTag tag;
  VarId var;
} VarExpr;

typedef struct {
  ExprTag tag;
  DeclId func;
  size_t argc;
  Expr** argv;
} AppExpr;

typedef struct {
  ExprTag tag;
  TypeId type;
  FieldId field;
  Expr* body;
} UnionExpr;

typedef struct {
  ExprTag tag;
  Expr* object;
  FieldId field;
} AccessExpr;

typedef struct {
  ExprTag tag;
  Expr* select;
  size_t argc;
  Expr** argv;
} CondExpr;

typedef struct {
  ExprTag tag;
  Expr* def;
  Expr* body;
} LetExpr;

typedef enum {
  STRUCT_DECL,    // TypeDecl
  UNION_DECL,     // TypeDecl
  FUNC_DECL,      // FuncDecl
  PROC_DECL       // ProcDecl
} DeclTag;

typedef struct {
  DeclTag tag;
} Decl;

typedef struct {
  DeclTag tag;
  size_t fieldc;
  TypeId* fieldv;
} TypeDecl;

typedef struct {
  DeclTag tag;
  size_t argc;
  TypeId* argv;
  TypeId return_type; 
  Expr* body;
} FuncDecl;

typedef enum {
  GET_POLARITY,
  PUT_POLARITY
} Polarity;

typedef struct {
  TypeId type;
  Polarity polarity;
} Port;

typedef enum {
  EVAL_ACTN,
  GET_ACTN,
  PUT_ACTN,
  COND_ACTN,
  CALL_ACTN,
  LINK_ACTN,
  EXEC_ACTN
} ActnTag;

typedef struct {
  ActnTag tag;
} Actn;

typedef struct {
  ActnTag tag;
  Expr* expr;
} EvalActn;

typedef struct {
  ActnTag tag;
  PortId port;
} GetActn;

typedef struct {
  ActnTag tag;
  PortId port;
  Expr* arg;
} PutActn;

typedef struct {
  ActnTag tag;
  Expr* select;
  size_t argc;
  Actn** argv;
} CondActn;

typedef struct {
  ActnTag tag;
  DeclId proc;
  size_t portc;
  PortId* portv;
  size_t argc;
  Expr** argv;
} CallActn;

typedef struct {
  ActnTag tag;
  TypeId type;
  Actn* body;
} LinkActn;

typedef struct {
  ActnTag tag;
  size_t execc;
  Actn** execv;
  Actn* body;
} ExecActn;

typedef struct {
  DeclTag tag;
  size_t portc;
  Port* portv;
  size_t argc;
  TypeId* argv; 
  TypeId return_type;
  Actn* body;
} ProcDecl;

typedef struct {
  size_t declc;
  Decl** declv;
} Program;

// Value

#define UNION_KIND (-1)
typedef struct {
  size_t refcount;
  size_t kind;    // UNION_KIND for union, number of fields for struct.
} Value;

typedef struct {
  size_t refcount;
  size_t kind;
  Value* fields[];
} StructValue;

typedef struct {
  size_t refcount;
  size_t kind;
  size_t tag;
  Value* field;
} UnionValue;

StructValue* NewStructValue(FblcArena* arena, size_t fieldc);
UnionValue* NewUnionValue(FblcArena* arena);
Value* Copy(FblcArena* arena, Value* src);
void Release(FblcArena* arena, Value* value);

// BitStream
// Bit streams are represented as sequences of ascii digits '0' and '1'.
// TODO: Support more efficient encodings of bit streams when desired.
// If the string field is non-null, digits are read from the string.
// If the fd field is non-negative, digits are read from the file with that
// descriptor.
typedef struct {
  const char* string;
  int fd;
} InputBitStream;

void OpenBinaryStringInputBitStream(InputBitStream* stream, const char* bits);
void OpenBinaryFdInputBitStream(InputBitStream* stream, int fd);
uint32_t ReadBits(InputBitStream* stream, size_t num_bits);

typedef struct {
  int fd;
  bool flushed;
} OutputBitStream;

void OpenBinaryOutputBitStream(OutputBitStream* stream, int fd);
void WriteBits(OutputBitStream* stream, size_t num_bits, uint32_t bits);
void FlushWriteBits(OutputBitStream* stream);

// Encoder
Value* DecodeValue(FblcArena* arena, InputBitStream* bits, Program* prg, TypeId type);
void EncodeValue(OutputBitStream* bits, Program* prg, TypeId type, Value* value);
Program* DecodeProgram(FblcArena* arena, InputBitStream* bits);

// Evaluator
Value* Execute(FblcArena* arena, Program* program, ProcDecl* proc, Value** args);

#endif // FBLC_H_

