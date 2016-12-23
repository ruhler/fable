
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

// Vector is a helper for dynamically allocating an array of data whose
// size is not known ahead of time.
// 'size' is the number of bytes taken by a single element.
// 'capacity' is the maximum number of elements supported by the current
// allocation of data.
// 'count' is the number of elements currently in use.
// 'data' is where the data is stored.

typedef struct {
  FblcArena* arena;
  size_t size;
  size_t capacity;
  size_t count;
  void* data;
} Vector ;

void VectorInit(FblcArena* arena, Vector* vector, size_t size);
void* VectorAppend(Vector* vector);
void* VectorExtract(Vector* vector, size_t* count);

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

